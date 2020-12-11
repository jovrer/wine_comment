/*
 * Server-side registry management
 *
 * Copyright (C) 1999 Alexandre Julliard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* To do:
 * - behavior with deleted keys
 * - values larger than request buffer
 * - symbolic links
 */

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "object.h"
#include "file.h"
#include "handle.h"
#include "request.h"
#include "unicode.h"
#include "security.h"

#include "winternl.h"
#include "wine/library.h"

struct notify
{
    struct list       entry;    /* entry in list of notifications */
    struct event     *event;    /* event to set when changing this key */
    int               subtree;  /* true if subtree notification */
    unsigned int      filter;   /* which events to notify on */
    obj_handle_t      hkey;     /* hkey associated with this notification */
    struct process   *process;  /* process in which the hkey is valid */
};

/* a registry key */
struct key
{
    struct object     obj;         /* object header */
    WCHAR            *name;        /* key name */
    WCHAR            *class;       /* key class */
    struct key       *parent;      /* parent key */
    int               last_subkey; /* last in use subkey */
    int               nb_subkeys;  /* count of allocated subkeys */
    struct key      **subkeys;     /* subkeys array */
    int               last_value;  /* last in use value */
    int               nb_values;   /* count of allocated values in array */
    struct key_value *values;      /* values array */
    unsigned int      flags;       /* flags */
    time_t            modif;       /* last modification time */
    struct list       notify_list; /* list of notifications */
};

/* key flags */
#define KEY_VOLATILE 0x0001  /* key is volatile (not saved to disk) */
#define KEY_DELETED  0x0002  /* key has been deleted */
#define KEY_DIRTY    0x0004  /* key has been modified */

/* a key value */
struct key_value
{
    WCHAR            *name;    /* value name */
    int               type;    /* value type */
    size_t            len;     /* value data length in bytes */
    void             *data;    /* pointer to value data */
};

#define MIN_SUBKEYS  8   /* min. number of allocated subkeys per key */
#define MIN_VALUES   8   /* min. number of allocated values per key */


/* the root of the registry tree */
static struct key *root_key;

static const int save_period = 30000;           /* delay between periodic saves (in ms) */
static struct timeout_user *save_timeout_user;  /* saving timer */

static void set_periodic_save_timer(void);

/* information about where to save a registry branch */
struct save_branch_info
{
    struct key  *key;
    char        *path;
};

#define MAX_SAVE_BRANCH_INFO 3
static int save_branch_count;
static struct save_branch_info save_branch_info[MAX_SAVE_BRANCH_INFO];


/* information about a file being loaded */
struct file_load_info
{
    FILE *file;    /* input file */
    char *buffer;  /* line buffer */
    int   len;     /* buffer length */
    int   line;    /* current input line */
    WCHAR *tmp;    /* temp buffer to use while parsing input */
    size_t tmplen; /* length of temp buffer */
};


static void key_dump( struct object *obj, int verbose );
static int key_close_handle( struct object *obj, struct process *process, obj_handle_t handle );
static void key_destroy( struct object *obj );

static const struct object_ops key_ops =
{
    sizeof(struct key),      /* size */
    key_dump,                /* dump */
    no_add_queue,            /* add_queue */
    NULL,                    /* remove_queue */
    NULL,                    /* signaled */
    NULL,                    /* satisfied */
    no_signal,               /* signal */
    no_get_fd,               /* get_fd */
    key_close_handle,        /* close_handle */
    key_destroy              /* destroy */
};


/*
 * The registry text file format v2 used by this code is similar to the one
 * used by REGEDIT import/export functionality, with the following differences:
 * - strings and key names can contain \x escapes for Unicode
 * - key names use escapes too in order to support Unicode
 * - the modification time optionally follows the key name
 * - REG_EXPAND_SZ and REG_MULTI_SZ are saved as strings instead of hex
 */

static inline char to_hex( char ch )
{
    if (isdigit(ch)) return ch - '0';
    return tolower(ch) - 'a' + 10;
}

/* dump the full path of a key */
static void dump_path( const struct key *key, const struct key *base, FILE *f )
{
    if (key->parent && key->parent != base)
    {
        dump_path( key->parent, base, f );
        fprintf( f, "\\\\" );
    }
    dump_strW( key->name, strlenW(key->name), f, "[]" );
}

/* dump a value to a text file */
static void dump_value( const struct key_value *value, FILE *f )
{
    unsigned int i;
    int count;

    if (value->name[0])
    {
        fputc( '\"', f );
        count = 1 + dump_strW( value->name, strlenW(value->name), f, "\"\"" );
        count += fprintf( f, "\"=" );
    }
    else count = fprintf( f, "@=" );

    switch(value->type)
    {
    case REG_SZ:
    case REG_EXPAND_SZ:
    case REG_MULTI_SZ:
        if (value->type != REG_SZ) fprintf( f, "str(%d):", value->type );
        fputc( '\"', f );
        if (value->data) dump_strW( (WCHAR *)value->data, value->len / sizeof(WCHAR), f, "\"\"" );
        fputc( '\"', f );
        break;
    case REG_DWORD:
        if (value->len == sizeof(DWORD))
        {
            DWORD dw;
            memcpy( &dw, value->data, sizeof(DWORD) );
            fprintf( f, "dword:%08lx", dw );
            break;
        }
        /* else fall through */
    default:
        if (value->type == REG_BINARY) count += fprintf( f, "hex:" );
        else count += fprintf( f, "hex(%x):", value->type );
        for (i = 0; i < value->len; i++)
        {
            count += fprintf( f, "%02x", *((unsigned char *)value->data + i) );
            if (i < value->len-1)
            {
                fputc( ',', f );
                if (++count > 76)
                {
                    fprintf( f, "\\\n  " );
                    count = 2;
                }
            }
        }
        break;
    }
    fputc( '\n', f );
}

/* save a registry and all its subkeys to a text file */
static void save_subkeys( const struct key *key, const struct key *base, FILE *f )
{
    int i;

    if (key->flags & KEY_VOLATILE) return;
    /* save key if it has either some values or no subkeys */
    /* keys with no values but subkeys are saved implicitly by saving the subkeys */
    if ((key->last_value >= 0) || (key->last_subkey == -1))
    {
        fprintf( f, "\n[" );
        if (key != base) dump_path( key, base, f );
        fprintf( f, "] %ld\n", (long)key->modif );
        for (i = 0; i <= key->last_value; i++) dump_value( &key->values[i], f );
    }
    for (i = 0; i <= key->last_subkey; i++) save_subkeys( key->subkeys[i], base, f );
}

static void dump_operation( const struct key *key, const struct key_value *value, const char *op )
{
    fprintf( stderr, "%s key ", op );
    if (key) dump_path( key, NULL, stderr );
    else fprintf( stderr, "ERROR" );
    if (value)
    {
        fprintf( stderr, " value ");
        dump_value( value, stderr );
    }
    else fprintf( stderr, "\n" );
}

static void key_dump( struct object *obj, int verbose )
{
    struct key *key = (struct key *)obj;
    assert( obj->ops == &key_ops );
    fprintf( stderr, "Key flags=%x ", key->flags );
    dump_path( key, NULL, stderr );
    fprintf( stderr, "\n" );
}

/* notify waiter and maybe delete the notification */
static void do_notification( struct key *key, struct notify *notify, int del )
{
    if (notify->event)
    {
        set_event( notify->event );
        release_object( notify->event );
        notify->event = NULL;
    }
    if (del)
    {
        list_remove( &notify->entry );
        free( notify );
    }
}

static inline struct notify *find_notify( struct key *key, struct process *process, obj_handle_t hkey )
{
    struct notify *notify;

    LIST_FOR_EACH_ENTRY( notify, &key->notify_list, struct notify, entry )
    {
        if (notify->process == process && notify->hkey == hkey) return notify;
    }
    return NULL;
}

/* close the notification associated with a handle */
static int key_close_handle( struct object *obj, struct process *process, obj_handle_t handle )
{
    struct key * key = (struct key *) obj;
    struct notify *notify = find_notify( key, process, handle );
    if (notify) do_notification( key, notify, 1 );
    return 1;  /* ok to close */
}

static void key_destroy( struct object *obj )
{
    int i;
    struct list *ptr;
    struct key *key = (struct key *)obj;
    assert( obj->ops == &key_ops );

    if (key->name) free( key->name );
    if (key->class) free( key->class );
    for (i = 0; i <= key->last_value; i++)
    {
        free( key->values[i].name );
        if (key->values[i].data) free( key->values[i].data );
    }
    if (key->values) free( key->values );
    for (i = 0; i <= key->last_subkey; i++)
    {
        key->subkeys[i]->parent = NULL;
        release_object( key->subkeys[i] );
    }
    if (key->subkeys) free( key->subkeys );
    /* unconditionally notify everything waiting on this key */
    while ((ptr = list_head( &key->notify_list )))
    {
        struct notify *notify = LIST_ENTRY( ptr, struct notify, entry );
        do_notification( key, notify, 1 );
    }
}

/* duplicate a key path */
/* returns a pointer to a static buffer, so only useable once per request */
static WCHAR *copy_path( const WCHAR *path, size_t len, int skip_root )
{
    static WCHAR buffer[MAX_PATH+1];
    static const WCHAR root_name[] = { '\\','R','e','g','i','s','t','r','y','\\',0 };

    if (len > sizeof(buffer)-sizeof(buffer[0]))
    {
        set_error( STATUS_BUFFER_OVERFLOW );
        return NULL;
    }
    memcpy( buffer, path, len );
    buffer[len / sizeof(WCHAR)] = 0;
    if (skip_root && !strncmpiW( buffer, root_name, 10 )) return buffer + 10;
    return buffer;
}

/* copy a path from the request buffer */
static WCHAR *copy_req_path( size_t len, int skip_root )
{
    const WCHAR *name_ptr = get_req_data();
    if (len > get_req_data_size())
    {
        fatal_protocol_error( current, "copy_req_path: invalid length %d/%d\n",
                              len, get_req_data_size() );
        return NULL;
    }
    return copy_path( name_ptr, len, skip_root );
}

/* return the next token in a given path */
/* returns a pointer to a static buffer, so only useable once per request */
static WCHAR *get_path_token( WCHAR *initpath )
{
    static WCHAR *path;
    WCHAR *ret;

    if (initpath)
    {
        /* path cannot start with a backslash */
        if (*initpath == '\\')
        {
            set_error( STATUS_OBJECT_PATH_INVALID );
            return NULL;
        }
        path = initpath;
    }
    else while (*path == '\\') path++;

    ret = path;
    while (*path && *path != '\\') path++;
    if (*path) *path++ = 0;
    return ret;
}

/* duplicate a Unicode string from the request buffer */
static WCHAR *req_strdupW( const void *req, const WCHAR *str, size_t len )
{
    WCHAR *name;
    if ((name = mem_alloc( len + sizeof(WCHAR) )) != NULL)
    {
        memcpy( name, str, len );
        name[len / sizeof(WCHAR)] = 0;
    }
    return name;
}

/* allocate a key object */
static struct key *alloc_key( const WCHAR *name, time_t modif )
{
    struct key *key;
    if ((key = alloc_object( &key_ops )))
    {
        key->class       = NULL;
        key->flags       = 0;
        key->last_subkey = -1;
        key->nb_subkeys  = 0;
        key->subkeys     = NULL;
        key->nb_values   = 0;
        key->last_value  = -1;
        key->values      = NULL;
        key->modif       = modif;
        key->parent      = NULL;
        list_init( &key->notify_list );
        if (!(key->name = strdupW( name )))
        {
            release_object( key );
            key = NULL;
        }
    }
    return key;
}

/* mark a key and all its parents as dirty (modified) */
static void make_dirty( struct key *key )
{
    while (key)
    {
        if (key->flags & (KEY_DIRTY|KEY_VOLATILE)) return;  /* nothing to do */
        key->flags |= KEY_DIRTY;
        key = key->parent;
    }
}

/* mark a key and all its subkeys as clean (not modified) */
static void make_clean( struct key *key )
{
    int i;

    if (key->flags & KEY_VOLATILE) return;
    if (!(key->flags & KEY_DIRTY)) return;
    key->flags &= ~KEY_DIRTY;
    for (i = 0; i <= key->last_subkey; i++) make_clean( key->subkeys[i] );
}

/* go through all the notifications and send them if necessary */
static void check_notify( struct key *key, unsigned int change, int not_subtree )
{
    struct list *ptr, *next;

    LIST_FOR_EACH_SAFE( ptr, next, &key->notify_list )
    {
        struct notify *n = LIST_ENTRY( ptr, struct notify, entry );
        if ( ( not_subtree || n->subtree ) && ( change & n->filter ) )
            do_notification( key, n, 0 );
    }
}

/* update key modification time */
static void touch_key( struct key *key, unsigned int change )
{
    struct key *k;

    key->modif = time(NULL);
    make_dirty( key );

    /* do notifications */
    check_notify( key, change, 1 );
    for ( k = key->parent; k; k = k->parent )
        check_notify( k, change & ~REG_NOTIFY_CHANGE_LAST_SET, 0 );
}

/* try to grow the array of subkeys; return 1 if OK, 0 on error */
static int grow_subkeys( struct key *key )
{
    struct key **new_subkeys;
    int nb_subkeys;

    if (key->nb_subkeys)
    {
        nb_subkeys = key->nb_subkeys + (key->nb_subkeys / 2);  /* grow by 50% */
        if (!(new_subkeys = realloc( key->subkeys, nb_subkeys * sizeof(*new_subkeys) )))
        {
            set_error( STATUS_NO_MEMORY );
            return 0;
        }
    }
    else
    {
        nb_subkeys = MIN_VALUES;
        if (!(new_subkeys = mem_alloc( nb_subkeys * sizeof(*new_subkeys) ))) return 0;
    }
    key->subkeys    = new_subkeys;
    key->nb_subkeys = nb_subkeys;
    return 1;
}

/* allocate a subkey for a given key, and return its index */
static struct key *alloc_subkey( struct key *parent, const WCHAR *name, int index, time_t modif )
{
    struct key *key;
    int i;

    if (parent->last_subkey + 1 == parent->nb_subkeys)
    {
        /* need to grow the array */
        if (!grow_subkeys( parent )) return NULL;
    }
    if ((key = alloc_key( name, modif )) != NULL)
    {
        key->parent = parent;
        for (i = ++parent->last_subkey; i > index; i--)
            parent->subkeys[i] = parent->subkeys[i-1];
        parent->subkeys[index] = key;
    }
    return key;
}

/* free a subkey of a given key */
static void free_subkey( struct key *parent, int index )
{
    struct key *key;
    int i, nb_subkeys;

    assert( index >= 0 );
    assert( index <= parent->last_subkey );

    key = parent->subkeys[index];
    for (i = index; i < parent->last_subkey; i++) parent->subkeys[i] = parent->subkeys[i + 1];
    parent->last_subkey--;
    key->flags |= KEY_DELETED;
    key->parent = NULL;
    release_object( key );

    /* try to shrink the array */
    nb_subkeys = parent->nb_subkeys;
    if (nb_subkeys > MIN_SUBKEYS && parent->last_subkey < nb_subkeys / 2)
    {
        struct key **new_subkeys;
        nb_subkeys -= nb_subkeys / 3;  /* shrink by 33% */
        if (nb_subkeys < MIN_SUBKEYS) nb_subkeys = MIN_SUBKEYS;
        if (!(new_subkeys = realloc( parent->subkeys, nb_subkeys * sizeof(*new_subkeys) ))) return;
        parent->subkeys = new_subkeys;
        parent->nb_subkeys = nb_subkeys;
    }
}

/* find the named child of a given key and return its index */
static struct key *find_subkey( const struct key *key, const WCHAR *name, int *index )
{
    int i, min, max, res;

    min = 0;
    max = key->last_subkey;
    while (min <= max)
    {
        i = (min + max) / 2;
        if (!(res = strcmpiW( key->subkeys[i]->name, name )))
        {
            *index = i;
            return key->subkeys[i];
        }
        if (res > 0) max = i - 1;
        else min = i + 1;
    }
    *index = min;  /* this is where we should insert it */
    return NULL;
}

/* open a subkey */
/* warning: the key name must be writeable (use copy_path) */
static struct key *open_key( struct key *key, WCHAR *name )
{
    int index;
    WCHAR *path;

    if (!(path = get_path_token( name ))) return NULL;
    while (*path)
    {
        if (!(key = find_subkey( key, path, &index )))
        {
            set_error( STATUS_OBJECT_NAME_NOT_FOUND );
            break;
        }
        path = get_path_token( NULL );
    }

    if (debug_level > 1) dump_operation( key, NULL, "Open" );
    if (key) grab_object( key );
    return key;
}

/* create a subkey */
/* warning: the key name must be writeable (use copy_path) */
static struct key *create_key( struct key *key, WCHAR *name, WCHAR *class,
                               int flags, time_t modif, int *created )
{
    struct key *base;
    int base_idx, index;
    WCHAR *path;

    if (key->flags & KEY_DELETED) /* we cannot create a subkey under a deleted key */
    {
        set_error( STATUS_KEY_DELETED );
        return NULL;
    }
    if (!(flags & KEY_VOLATILE) && (key->flags & KEY_VOLATILE))
    {
        set_error( STATUS_CHILD_MUST_BE_VOLATILE );
        return NULL;
    }
    if (!modif) modif = time(NULL);

    if (!(path = get_path_token( name ))) return NULL;
    *created = 0;
    while (*path)
    {
        struct key *subkey;
        if (!(subkey = find_subkey( key, path, &index ))) break;
        key = subkey;
        path = get_path_token( NULL );
    }

    /* create the remaining part */

    if (!*path) goto done;
    *created = 1;
    if (flags & KEY_DIRTY) make_dirty( key );
    base = key;
    base_idx = index;
    key = alloc_subkey( key, path, index, modif );
    while (key)
    {
        key->flags |= flags;
        path = get_path_token( NULL );
        if (!*path) goto done;
        /* we know the index is always 0 in a new key */
        key = alloc_subkey( key, path, 0, modif );
    }
    if (base_idx != -1) free_subkey( base, base_idx );
    return NULL;

 done:
    if (debug_level > 1) dump_operation( key, NULL, "Create" );
    if (class) key->class = strdupW(class);
    grab_object( key );
    return key;
}

/* query information about a key or a subkey */
static void enum_key( const struct key *key, int index, int info_class,
                      struct enum_key_reply *reply )
{
    int i;
    size_t len, namelen, classlen;
    int max_subkey = 0, max_class = 0;
    int max_value = 0, max_data = 0;
    WCHAR *data;

    if (index != -1)  /* -1 means use the specified key directly */
    {
        if ((index < 0) || (index > key->last_subkey))
        {
            set_error( STATUS_NO_MORE_ENTRIES );
            return;
        }
        key = key->subkeys[index];
    }

    namelen = strlenW(key->name) * sizeof(WCHAR);
    classlen = key->class ? strlenW(key->class) * sizeof(WCHAR) : 0;

    switch(info_class)
    {
    case KeyBasicInformation:
        classlen = 0; /* only return the name */
        /* fall through */
    case KeyNodeInformation:
        reply->max_subkey = 0;
        reply->max_class  = 0;
        reply->max_value  = 0;
        reply->max_data   = 0;
        break;
    case KeyFullInformation:
        for (i = 0; i <= key->last_subkey; i++)
        {
            struct key *subkey = key->subkeys[i];
            len = strlenW( subkey->name );
            if (len > max_subkey) max_subkey = len;
            if (!subkey->class) continue;
            len = strlenW( subkey->class );
            if (len > max_class) max_class = len;
        }
        for (i = 0; i <= key->last_value; i++)
        {
            len = strlenW( key->values[i].name );
            if (len > max_value) max_value = len;
            len = key->values[i].len;
            if (len > max_data) max_data = len;
        }
        reply->max_subkey = max_subkey;
        reply->max_class  = max_class;
        reply->max_value  = max_value;
        reply->max_data   = max_data;
        namelen = 0;  /* only return the class */
        break;
    default:
        set_error( STATUS_INVALID_PARAMETER );
        return;
    }
    reply->subkeys = key->last_subkey + 1;
    reply->values  = key->last_value + 1;
    reply->modif   = key->modif;
    reply->total   = namelen + classlen;

    len = min( reply->total, get_reply_max_size() );
    if (len && (data = set_reply_data_size( len )))
    {
        if (len > namelen)
        {
            reply->namelen = namelen;
            memcpy( data, key->name, namelen );
            memcpy( (char *)data + namelen, key->class, len - namelen );
        }
        else
        {
            reply->namelen = len;
            memcpy( data, key->name, len );
        }
    }
    if (debug_level > 1) dump_operation( key, NULL, "Enum" );
}

/* delete a key and its values */
static int delete_key( struct key *key, int recurse )
{
    int index;
    struct key *parent;

    /* must find parent and index */
    if (key == root_key)
    {
        set_error( STATUS_ACCESS_DENIED );
        return -1;
    }
    if (!(parent = key->parent) || (key->flags & KEY_DELETED))
    {
        set_error( STATUS_KEY_DELETED );
        return -1;
    }

    while (recurse && (key->last_subkey>=0))
        if (0 > delete_key(key->subkeys[key->last_subkey], 1))
            return -1;

    for (index = 0; index <= parent->last_subkey; index++)
        if (parent->subkeys[index] == key) break;
    assert( index <= parent->last_subkey );

    /* we can only delete a key that has no subkeys */
    if (key->last_subkey >= 0)
    {
        set_error( STATUS_ACCESS_DENIED );
        return -1;
    }

    if (debug_level > 1) dump_operation( key, NULL, "Delete" );
    free_subkey( parent, index );
    touch_key( parent, REG_NOTIFY_CHANGE_NAME );
    return 0;
}

/* try to grow the array of values; return 1 if OK, 0 on error */
static int grow_values( struct key *key )
{
    struct key_value *new_val;
    int nb_values;

    if (key->nb_values)
    {
        nb_values = key->nb_values + (key->nb_values / 2);  /* grow by 50% */
        if (!(new_val = realloc( key->values, nb_values * sizeof(*new_val) )))
        {
            set_error( STATUS_NO_MEMORY );
            return 0;
        }
    }
    else
    {
        nb_values = MIN_VALUES;
        if (!(new_val = mem_alloc( nb_values * sizeof(*new_val) ))) return 0;
    }
    key->values = new_val;
    key->nb_values = nb_values;
    return 1;
}

/* find the named value of a given key and return its index in the array */
static struct key_value *find_value( const struct key *key, const WCHAR *name, int *index )
{
    int i, min, max, res;

    min = 0;
    max = key->last_value;
    while (min <= max)
    {
        i = (min + max) / 2;
        if (!(res = strcmpiW( key->values[i].name, name )))
        {
            *index = i;
            return &key->values[i];
        }
        if (res > 0) max = i - 1;
        else min = i + 1;
    }
    *index = min;  /* this is where we should insert it */
    return NULL;
}

/* insert a new value; the index must have been returned by find_value */
static struct key_value *insert_value( struct key *key, const WCHAR *name, int index )
{
    struct key_value *value;
    WCHAR *new_name;
    int i;

    if (key->last_value + 1 == key->nb_values)
    {
        if (!grow_values( key )) return NULL;
    }
    if (!(new_name = strdupW(name))) return NULL;
    for (i = ++key->last_value; i > index; i--) key->values[i] = key->values[i - 1];
    value = &key->values[index];
    value->name = new_name;
    value->len  = 0;
    value->data = NULL;
    return value;
}

/* set a key value */
static void set_value( struct key *key, WCHAR *name, int type, const void *data, size_t len )
{
    struct key_value *value;
    void *ptr = NULL;
    int index;

    if ((value = find_value( key, name, &index )))
    {
        /* check if the new value is identical to the existing one */
        if (value->type == type && value->len == len &&
            value->data && !memcmp( value->data, data, len ))
        {
            if (debug_level > 1) dump_operation( key, value, "Skip setting" );
            return;
        }
    }

    if (len && !(ptr = memdup( data, len ))) return;

    if (!value)
    {
        if (!(value = insert_value( key, name, index )))
        {
            if (ptr) free( ptr );
            return;
        }
    }
    else if (value->data) free( value->data ); /* already existing, free previous data */

    value->type  = type;
    value->len   = len;
    value->data  = ptr;
    touch_key( key, REG_NOTIFY_CHANGE_LAST_SET );
    if (debug_level > 1) dump_operation( key, value, "Set" );
}

/* get a key value */
static void get_value( struct key *key, const WCHAR *name, int *type, unsigned int *len )
{
    struct key_value *value;
    int index;

    if ((value = find_value( key, name, &index )))
    {
        *type = value->type;
        *len  = value->len;
        if (value->data) set_reply_data( value->data, min( value->len, get_reply_max_size() ));
        if (debug_level > 1) dump_operation( key, value, "Get" );
    }
    else
    {
        *type = -1;
        set_error( STATUS_OBJECT_NAME_NOT_FOUND );
    }
}

/* enumerate a key value */
static void enum_value( struct key *key, int i, int info_class, struct enum_key_value_reply *reply )
{
    struct key_value *value;

    if (i < 0 || i > key->last_value) set_error( STATUS_NO_MORE_ENTRIES );
    else
    {
        void *data;
        size_t namelen, maxlen;

        value = &key->values[i];
        reply->type = value->type;
        namelen = strlenW( value->name ) * sizeof(WCHAR);

        switch(info_class)
        {
        case KeyValueBasicInformation:
            reply->total = namelen;
            break;
        case KeyValueFullInformation:
            reply->total = namelen + value->len;
            break;
        case KeyValuePartialInformation:
            reply->total = value->len;
            namelen = 0;
            break;
        default:
            set_error( STATUS_INVALID_PARAMETER );
            return;
        }

        maxlen = min( reply->total, get_reply_max_size() );
        if (maxlen && ((data = set_reply_data_size( maxlen ))))
        {
            if (maxlen > namelen)
            {
                reply->namelen = namelen;
                memcpy( data, value->name, namelen );
                memcpy( (char *)data + namelen, value->data, maxlen - namelen );
            }
            else
            {
                reply->namelen = maxlen;
                memcpy( data, value->name, maxlen );
            }
        }
        if (debug_level > 1) dump_operation( key, value, "Enum" );
    }
}

/* delete a value */
static void delete_value( struct key *key, const WCHAR *name )
{
    struct key_value *value;
    int i, index, nb_values;

    if (!(value = find_value( key, name, &index )))
    {
        set_error( STATUS_OBJECT_NAME_NOT_FOUND );
        return;
    }
    if (debug_level > 1) dump_operation( key, value, "Delete" );
    free( value->name );
    if (value->data) free( value->data );
    for (i = index; i < key->last_value; i++) key->values[i] = key->values[i + 1];
    key->last_value--;
    touch_key( key, REG_NOTIFY_CHANGE_LAST_SET );

    /* try to shrink the array */
    nb_values = key->nb_values;
    if (nb_values > MIN_VALUES && key->last_value < nb_values / 2)
    {
        struct key_value *new_val;
        nb_values -= nb_values / 3;  /* shrink by 33% */
        if (nb_values < MIN_VALUES) nb_values = MIN_VALUES;
        if (!(new_val = realloc( key->values, nb_values * sizeof(*new_val) ))) return;
        key->values = new_val;
        key->nb_values = nb_values;
    }
}

/* get the registry key corresponding to an hkey handle */
static struct key *get_hkey_obj( obj_handle_t hkey, unsigned int access )
{
    if (!hkey) return (struct key *)grab_object( root_key );
    return (struct key *)get_handle_obj( current->process, hkey, access, &key_ops );
}

/* read a line from the input file */
static int read_next_line( struct file_load_info *info )
{
    char *newbuf;
    int newlen, pos = 0;

    info->line++;
    for (;;)
    {
        if (!fgets( info->buffer + pos, info->len - pos, info->file ))
            return (pos != 0);  /* EOF */
        pos = strlen(info->buffer);
        if (info->buffer[pos-1] == '\n')
        {
            /* got a full line */
            info->buffer[--pos] = 0;
            if (pos > 0 && info->buffer[pos-1] == '\r') info->buffer[pos-1] = 0;
            return 1;
        }
        if (pos < info->len - 1) return 1;  /* EOF but something was read */

        /* need to enlarge the buffer */
        newlen = info->len + info->len / 2;
        if (!(newbuf = realloc( info->buffer, newlen )))
        {
            set_error( STATUS_NO_MEMORY );
            return -1;
        }
        info->buffer = newbuf;
        info->len = newlen;
    }
}

/* make sure the temp buffer holds enough space */
static int get_file_tmp_space( struct file_load_info *info, size_t size )
{
    WCHAR *tmp;
    if (info->tmplen >= size) return 1;
    if (!(tmp = realloc( info->tmp, size )))
    {
        set_error( STATUS_NO_MEMORY );
        return 0;
    }
    info->tmp = tmp;
    info->tmplen = size;
    return 1;
}

/* report an error while loading an input file */
static void file_read_error( const char *err, struct file_load_info *info )
{
    fprintf( stderr, "Line %d: %s '%s'\n", info->line, err, info->buffer );
}

/* parse an escaped string back into Unicode */
/* return the number of chars read from the input, or -1 on output overflow */
static int parse_strW( WCHAR *dest, int *len, const char *src, char endchar )
{
    int count = sizeof(WCHAR);  /* for terminating null */
    const char *p = src;
    while (*p && *p != endchar)
    {
        if (*p != '\\') *dest = (WCHAR)*p++;
        else
        {
            p++;
            switch(*p)
            {
            case 'a': *dest = '\a'; p++; break;
            case 'b': *dest = '\b'; p++; break;
            case 'e': *dest = '\e'; p++; break;
            case 'f': *dest = '\f'; p++; break;
            case 'n': *dest = '\n'; p++; break;
            case 'r': *dest = '\r'; p++; break;
            case 't': *dest = '\t'; p++; break;
            case 'v': *dest = '\v'; p++; break;
            case 'x':  /* hex escape */
                p++;
                if (!isxdigit(*p)) *dest = 'x';
                else
                {
                    *dest = to_hex(*p++);
                    if (isxdigit(*p)) *dest = (*dest * 16) + to_hex(*p++);
                    if (isxdigit(*p)) *dest = (*dest * 16) + to_hex(*p++);
                    if (isxdigit(*p)) *dest = (*dest * 16) + to_hex(*p++);
                }
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':  /* octal escape */
                *dest = *p++ - '0';
                if (*p >= '0' && *p <= '7') *dest = (*dest * 8) + (*p++ - '0');
                if (*p >= '0' && *p <= '7') *dest = (*dest * 8) + (*p++ - '0');
                break;
            default:
                *dest = (WCHAR)*p++;
                break;
            }
        }
        if ((count += sizeof(WCHAR)) > *len) return -1;  /* dest buffer overflow */
        dest++;
    }
    *dest = 0;
    if (!*p) return -1;  /* delimiter not found */
    *len = count;
    return p + 1 - src;
}

/* convert a data type tag to a value type */
static int get_data_type( const char *buffer, int *type, int *parse_type )
{
    struct data_type { const char *tag; int len; int type; int parse_type; };

    static const struct data_type data_types[] =
    {                   /* actual type */  /* type to assume for parsing */
        { "\"",        1,   REG_SZ,              REG_SZ },
        { "str:\"",    5,   REG_SZ,              REG_SZ },
        { "str(2):\"", 8,   REG_EXPAND_SZ,       REG_SZ },
        { "str(7):\"", 8,   REG_MULTI_SZ,        REG_SZ },
        { "hex:",      4,   REG_BINARY,          REG_BINARY },
        { "dword:",    6,   REG_DWORD,           REG_DWORD },
        { "hex(",      4,   -1,                  REG_BINARY },
        { NULL,        0,    0,                  0 }
    };

    const struct data_type *ptr;
    char *end;

    for (ptr = data_types; ptr->tag; ptr++)
    {
        if (memcmp( ptr->tag, buffer, ptr->len )) continue;
        *parse_type = ptr->parse_type;
        if ((*type = ptr->type) != -1) return ptr->len;
        /* "hex(xx):" is special */
        *type = (int)strtoul( buffer + 4, &end, 16 );
        if ((end <= buffer) || memcmp( end, "):", 2 )) return 0;
        return end + 2 - buffer;
    }
    return 0;
}

/* load and create a key from the input file */
static struct key *load_key( struct key *base, const char *buffer, int flags,
                             int prefix_len, struct file_load_info *info,
                             int default_modif )
{
    WCHAR *p, *name;
    int res, len, modif;

    len = strlen(buffer) * sizeof(WCHAR);
    if (!get_file_tmp_space( info, len )) return NULL;

    if ((res = parse_strW( info->tmp, &len, buffer, ']' )) == -1)
    {
        file_read_error( "Malformed key", info );
        return NULL;
    }
    if (sscanf( buffer + res, " %d", &modif ) != 1) modif = default_modif;

    p = info->tmp;
    while (prefix_len && *p) { if (*p++ == '\\') prefix_len--; }

    if (!*p)
    {
        if (prefix_len > 1)
        {
            file_read_error( "Malformed key", info );
            return NULL;
        }
        /* empty key name, return base key */
        return (struct key *)grab_object( base );
    }
    if (!(name = copy_path( p, len - (p - info->tmp) * sizeof(WCHAR), 0 )))
    {
        file_read_error( "Key is too long", info );
        return NULL;
    }
    return create_key( base, name, NULL, flags, modif, &res );
}

/* parse a comma-separated list of hex digits */
static int parse_hex( unsigned char *dest, int *len, const char *buffer )
{
    const char *p = buffer;
    int count = 0;
    while (isxdigit(*p))
    {
        int val;
        char buf[3];
        memcpy( buf, p, 2 );
        buf[2] = 0;
        sscanf( buf, "%x", &val );
        if (count++ >= *len) return -1;  /* dest buffer overflow */
        *dest++ = (unsigned char )val;
        p += 2;
        if (*p == ',') p++;
    }
    *len = count;
    return p - buffer;
}

/* parse a value name and create the corresponding value */
static struct key_value *parse_value_name( struct key *key, const char *buffer, int *len,
                                           struct file_load_info *info )
{
    struct key_value *value;
    int index, maxlen;

    maxlen = strlen(buffer) * sizeof(WCHAR);
    if (!get_file_tmp_space( info, maxlen )) return NULL;
    if (buffer[0] == '@')
    {
        info->tmp[0] = 0;
        *len = 1;
    }
    else
    {
        if ((*len = parse_strW( info->tmp, &maxlen, buffer + 1, '\"' )) == -1) goto error;
        (*len)++;  /* for initial quote */
    }
    while (isspace(buffer[*len])) (*len)++;
    if (buffer[*len] != '=') goto error;
    (*len)++;
    while (isspace(buffer[*len])) (*len)++;
    if (!(value = find_value( key, info->tmp, &index )))
        value = insert_value( key, info->tmp, index );
    return value;

 error:
    file_read_error( "Malformed value name", info );
    return NULL;
}

/* load a value from the input file */
static int load_value( struct key *key, const char *buffer, struct file_load_info *info )
{
    DWORD dw;
    void *ptr, *newptr;
    int maxlen, len, res;
    int type, parse_type;
    struct key_value *value;

    if (!(value = parse_value_name( key, buffer, &len, info ))) return 0;
    if (!(res = get_data_type( buffer + len, &type, &parse_type ))) goto error;
    buffer += len + res;

    switch(parse_type)
    {
    case REG_SZ:
        len = strlen(buffer) * sizeof(WCHAR);
        if (!get_file_tmp_space( info, len )) return 0;
        if ((res = parse_strW( info->tmp, &len, buffer, '\"' )) == -1) goto error;
        ptr = info->tmp;
        break;
    case REG_DWORD:
        dw = strtoul( buffer, NULL, 16 );
        ptr = &dw;
        len = sizeof(dw);
        break;
    case REG_BINARY:  /* hex digits */
        len = 0;
        for (;;)
        {
            maxlen = 1 + strlen(buffer)/3;  /* 3 chars for one hex byte */
            if (!get_file_tmp_space( info, len + maxlen )) return 0;
            if ((res = parse_hex( (unsigned char *)info->tmp + len, &maxlen, buffer )) == -1) goto error;
            len += maxlen;
            buffer += res;
            while (isspace(*buffer)) buffer++;
            if (!*buffer) break;
            if (*buffer != '\\') goto error;
            if (read_next_line( info) != 1) goto error;
            buffer = info->buffer;
            while (isspace(*buffer)) buffer++;
        }
        ptr = info->tmp;
        break;
    default:
        assert(0);
        ptr = NULL;  /* keep compiler quiet */
        break;
    }

    if (!len) newptr = NULL;
    else if (!(newptr = memdup( ptr, len ))) return 0;

    if (value->data) free( value->data );
    value->data = newptr;
    value->len  = len;
    value->type = type;
    make_dirty( key );
    return 1;

 error:
    file_read_error( "Malformed value", info );
    return 0;
}

/* return the length (in path elements) of name that is part of the key name */
/* for instance if key is USER\foo\bar and name is foo\bar\baz, return 2 */
static int get_prefix_len( struct key *key, const char *name, struct file_load_info *info )
{
    WCHAR *p;
    int res;
    int len = strlen(name) * sizeof(WCHAR);
    if (!get_file_tmp_space( info, len )) return 0;

    if ((res = parse_strW( info->tmp, &len, name, ']' )) == -1)
    {
        file_read_error( "Malformed key", info );
        return 0;
    }
    for (p = info->tmp; *p; p++) if (*p == '\\') break;
    *p = 0;
    for (res = 1; key != root_key; res++)
    {
        if (!strcmpiW( info->tmp, key->name )) break;
        key = key->parent;
    }
    if (key == root_key) res = 0;  /* no matching name */
    return res;
}

/* load all the keys from the input file */
/* prefix_len is the number of key name prefixes to skip, or -1 for autodetection */
static void load_keys( struct key *key, FILE *f, int prefix_len )
{
    struct key *subkey = NULL;
    struct file_load_info info;
    char *p;
    int default_modif = time(NULL);
    int flags = (key->flags & KEY_VOLATILE) ? KEY_VOLATILE : KEY_DIRTY;

    info.file   = f;
    info.len    = 4;
    info.tmplen = 4;
    info.line   = 0;
    if (!(info.buffer = mem_alloc( info.len ))) return;
    if (!(info.tmp = mem_alloc( info.tmplen )))
    {
        free( info.buffer );
        return;
    }

    if ((read_next_line( &info ) != 1) ||
        strcmp( info.buffer, "WINE REGISTRY Version 2" ))
    {
        set_error( STATUS_NOT_REGISTRY_FILE );
        goto done;
    }

    while (read_next_line( &info ) == 1)
    {
        p = info.buffer;
        while (*p && isspace(*p)) p++;
        switch(*p)
        {
        case '[':   /* new key */
            if (subkey) release_object( subkey );
            if (prefix_len == -1) prefix_len = get_prefix_len( key, p + 1, &info );
            if (!(subkey = load_key( key, p + 1, flags, prefix_len, &info, default_modif )))
                file_read_error( "Error creating key", &info );
            break;
        case '@':   /* default value */
        case '\"':  /* value */
            if (subkey) load_value( subkey, p, &info );
            else file_read_error( "Value without key", &info );
            break;
        case '#':   /* comment */
        case ';':   /* comment */
        case 0:     /* empty line */
            break;
        default:
            file_read_error( "Unrecognized input", &info );
            break;
        }
    }

 done:
    if (subkey) release_object( subkey );
    free( info.buffer );
    free( info.tmp );
}

/* load a part of the registry from a file */
static void load_registry( struct key *key, obj_handle_t handle )
{
    struct file *file;
    int fd;

    if (!(file = get_file_obj( current->process, handle, GENERIC_READ ))) return;
    fd = dup( get_file_unix_fd( file ) );
    release_object( file );
    if (fd != -1)
    {
        FILE *f = fdopen( fd, "r" );
        if (f)
        {
            load_keys( key, f, -1 );
            fclose( f );
        }
        else file_set_error();
    }
}

/* load one of the initial registry files */
static void load_init_registry_from_file( const char *filename, struct key *key )
{
    FILE *f;

    if ((f = fopen( filename, "r" )))
    {
        load_keys( key, f, 0 );
        fclose( f );
        if (get_error() == STATUS_NOT_REGISTRY_FILE)
            fatal_error( "%s is not a valid registry file\n", filename );
        if (get_error())
            fatal_error( "loading %s failed with error %x\n", filename, get_error() );
    }

    if (!(key->flags & KEY_VOLATILE))
    {
        assert( save_branch_count < MAX_SAVE_BRANCH_INFO );

        if ((save_branch_info[save_branch_count].path = strdup( filename )))
            save_branch_info[save_branch_count++].key = (struct key *)grab_object( key );
    }
}

static WCHAR *format_user_registry_path( const SID *sid )
{
    static const WCHAR prefixW[] = {'U','s','e','r','\\','S',0};
    static const WCHAR formatW[] = {'-','%','u',0};
    WCHAR buffer[7 + 10 + 10 + 10 * SID_MAX_SUB_AUTHORITIES];
    WCHAR *p = buffer;
    unsigned int i;

    strcpyW( p, prefixW );
    p += strlenW( prefixW );
    p += sprintfW( p, formatW, sid->Revision );
    p += sprintfW( p, formatW, MAKELONG( MAKEWORD( sid->IdentifierAuthority.Value[5],
                                                   sid->IdentifierAuthority.Value[4] ),
                                         MAKEWORD( sid->IdentifierAuthority.Value[3],
                                                   sid->IdentifierAuthority.Value[2] )));
    for (i = 0; i < sid->SubAuthorityCount; i++)
        p += sprintfW( p, formatW, sid->SubAuthority[i] );

    return memdup( buffer, (p + 1 - buffer) * sizeof(WCHAR) );
}

/* registry initialisation */
void init_registry(void)
{
    static const WCHAR root_name[] = { 0 };
    static const WCHAR HKLM[] = { 'M','a','c','h','i','n','e' };
    static const WCHAR HKU_default[] = { 'U','s','e','r','\\','.','D','e','f','a','u','l','t' };
    WCHAR *current_user_path;

    const char *config = wine_get_config_dir();
    char *p, *filename;
    struct key *key;
    int dummy;

    /* create the root key */
    root_key = alloc_key( root_name, time(NULL) );
    assert( root_key );

    if (!(filename = malloc( strlen(config) + 16 ))) fatal_error( "out of memory\n" );
    strcpy( filename, config );
    p = filename + strlen(filename);

    /* load system.reg into Registry\Machine */

    if (!(key = create_key( root_key, copy_path( HKLM, sizeof(HKLM), 0 ),
                            NULL, 0, time(NULL), &dummy )))
        fatal_error( "could not create Machine registry key\n" );

    strcpy( p, "/system.reg" );
    load_init_registry_from_file( filename, key );
    release_object( key );

    /* load userdef.reg into Registry\User\.Default */

    if (!(key = create_key( root_key, copy_path( HKU_default, sizeof(HKU_default), 0 ),
                            NULL, 0, time(NULL), &dummy )))
        fatal_error( "could not create User\\.Default registry key\n" );

    strcpy( p, "/userdef.reg" );
    load_init_registry_from_file( filename, key );
    release_object( key );

    /* load user.reg into HKEY_CURRENT_USER */

    /* FIXME: match default user in token.c. should get from process token instead */
    current_user_path = format_user_registry_path( security_interactive_sid );
    if (!current_user_path ||
        !(key = create_key( root_key, current_user_path, NULL, 0, time(NULL), &dummy )))
        fatal_error( "could not create HKEY_CURRENT_USER registry key\n" );
    free( current_user_path );
    strcpy( p, "/user.reg" );
    load_init_registry_from_file( filename, key );
    release_object( key );

    free( filename );

    /* start the periodic save timer */
    set_periodic_save_timer();
}

/* save a registry branch to a file */
static void save_all_subkeys( struct key *key, FILE *f )
{
    fprintf( f, "WINE REGISTRY Version 2\n" );
    fprintf( f, ";; All keys relative to " );
    dump_path( key, NULL, f );
    fprintf( f, "\n" );
    save_subkeys( key, key, f );
}

/* save a registry branch to a file handle */
static void save_registry( struct key *key, obj_handle_t handle )
{
    struct file *file;
    int fd;

    if (key->flags & KEY_DELETED)
    {
        set_error( STATUS_KEY_DELETED );
        return;
    }
    if (!(file = get_file_obj( current->process, handle, GENERIC_WRITE ))) return;
    fd = dup( get_file_unix_fd( file ) );
    release_object( file );
    if (fd != -1)
    {
        FILE *f = fdopen( fd, "w" );
        if (f)
        {
            save_all_subkeys( key, f );
            if (fclose( f )) file_set_error();
        }
        else
        {
            file_set_error();
            close( fd );
        }
    }
}

/* save a registry branch to a file */
static int save_branch( struct key *key, const char *path )
{
    struct stat st;
    char *p, *real, *tmp = NULL;
    int fd, count = 0, ret = 0, by_symlink;
    FILE *f;

    if (!(key->flags & KEY_DIRTY))
    {
        if (debug_level > 1) dump_operation( key, NULL, "Not saving clean" );
        return 1;
    }

    /* get the real path */

    by_symlink = (!lstat(path, &st) && S_ISLNK (st.st_mode));
    if (!(real = malloc( PATH_MAX ))) return 0;
    if (!realpath( path, real ))
    {
        free( real );
        real = NULL;
    }
    else path = real;

    /* test the file type */

    if ((fd = open( path, O_WRONLY )) != -1)
    {
        /* if file is not a regular file or has multiple links or is accessed
         * via symbolic links, write directly into it; otherwise use a temp file */
        if (by_symlink ||
            (!fstat( fd, &st ) && (!S_ISREG(st.st_mode) || st.st_nlink > 1)))
        {
            ftruncate( fd, 0 );
            goto save;
        }
        close( fd );
    }

    /* create a temp file in the same directory */

    if (!(tmp = malloc( strlen(path) + 20 ))) goto done;
    strcpy( tmp, path );
    if ((p = strrchr( tmp, '/' ))) p++;
    else p = tmp;
    for (;;)
    {
        sprintf( p, "reg%lx%04x.tmp", (long) getpid(), count++ );
        if ((fd = open( tmp, O_CREAT | O_EXCL | O_WRONLY, 0666 )) != -1) break;
        if (errno != EEXIST) goto done;
        close( fd );
    }

    /* now save to it */

 save:
    if (!(f = fdopen( fd, "w" )))
    {
        if (tmp) unlink( tmp );
        close( fd );
        goto done;
    }

    if (debug_level > 1)
    {
        fprintf( stderr, "%s: ", path );
        dump_operation( key, NULL, "saving" );
    }

    save_all_subkeys( key, f );
    ret = !fclose(f);

    if (tmp)
    {
        /* if successfully written, rename to final name */
        if (ret) ret = !rename( tmp, path );
        if (!ret) unlink( tmp );
    }

done:
    free( tmp );
    free( real );
    if (ret) make_clean( key );
    return ret;
}

/* periodic saving of the registry */
static void periodic_save( void *arg )
{
    int i;

    save_timeout_user = NULL;
    for (i = 0; i < save_branch_count; i++)
        save_branch( save_branch_info[i].key, save_branch_info[i].path );
    set_periodic_save_timer();
}

/* start the periodic save timer */
static void set_periodic_save_timer(void)
{
    struct timeval next;

    gettimeofday( &next, NULL );
    add_timeout( &next, save_period );
    if (save_timeout_user) remove_timeout_user( save_timeout_user );
    save_timeout_user = add_timeout_user( &next, periodic_save, NULL );
}

/* save the modified registry branches to disk */
void flush_registry(void)
{
    int i;

    for (i = 0; i < save_branch_count; i++)
    {
        if (!save_branch( save_branch_info[i].key, save_branch_info[i].path ))
        {
            fprintf( stderr, "wineserver: could not save registry branch to %s",
                     save_branch_info[i].path );
            perror( " " );
        }
    }
}

/* close the top-level keys; used on server exit */
void close_registry(void)
{
    int i;

    if (save_timeout_user) remove_timeout_user( save_timeout_user );
    save_timeout_user = NULL;
    for (i = 0; i < save_branch_count; i++)
    {
        release_object( save_branch_info[i].key );
        free( save_branch_info[i].path );
    }
    release_object( root_key );
}


/* create a registry key */
DECL_HANDLER(create_key)
{
    struct key *key = NULL, *parent;
    unsigned int access = req->access;
    WCHAR *name, *class;

    if (access & MAXIMUM_ALLOWED) access = KEY_ALL_ACCESS;  /* FIXME: needs general solution */
    reply->hkey = 0;
    if (!(name = copy_req_path( req->namelen, !req->parent ))) return;
    /* NOTE: no access rights are required from the parent handle to create a key */
    if ((parent = get_hkey_obj( req->parent, 0 )))
    {
        int flags = (req->options & REG_OPTION_VOLATILE) ? KEY_VOLATILE : KEY_DIRTY;

        if (req->namelen == get_req_data_size())  /* no class specified */
        {
            key = create_key( parent, name, NULL, flags, req->modif, &reply->created );
        }
        else
        {
            const WCHAR *class_ptr = (const WCHAR *)((const char *)get_req_data() + req->namelen);

            if ((class = req_strdupW( req, class_ptr, get_req_data_size() - req->namelen )))
            {
                key = create_key( parent, name, class, flags, req->modif, &reply->created );
                free( class );
            }
        }
        if (key)
        {
            reply->hkey = alloc_handle( current->process, key, access, 0 );
            release_object( key );
        }
        release_object( parent );
    }
}

/* open a registry key */
DECL_HANDLER(open_key)
{
    struct key *key, *parent;
    unsigned int access = req->access;

    if (access & MAXIMUM_ALLOWED) access = KEY_ALL_ACCESS;  /* FIXME: needs general solution */
    reply->hkey = 0;
    /* NOTE: no access rights are required to open the parent key, only the child key */
    if ((parent = get_hkey_obj( req->parent, 0 )))
    {
        WCHAR *name = copy_path( get_req_data(), get_req_data_size(), !req->parent );
        if (name && (key = open_key( parent, name )))
        {
            reply->hkey = alloc_handle( current->process, key, access, 0 );
            release_object( key );
        }
        release_object( parent );
    }
}

/* delete a registry key */
DECL_HANDLER(delete_key)
{
    struct key *key;

    if ((key = get_hkey_obj( req->hkey, DELETE )))
    {
        delete_key( key, 0);
        release_object( key );
    }
}

/* flush a registry key */
DECL_HANDLER(flush_key)
{
    struct key *key = get_hkey_obj( req->hkey, 0 );
    if (key)
    {
        /* we don't need to do anything here with the current implementation */
        release_object( key );
    }
}

/* enumerate registry subkeys */
DECL_HANDLER(enum_key)
{
    struct key *key;

    if ((key = get_hkey_obj( req->hkey,
                             req->index == -1 ? KEY_QUERY_VALUE : KEY_ENUMERATE_SUB_KEYS )))
    {
        enum_key( key, req->index, req->info_class, reply );
        release_object( key );
    }
}

/* set a value of a registry key */
DECL_HANDLER(set_key_value)
{
    struct key *key;
    WCHAR *name;

    if (!(name = copy_req_path( req->namelen, 0 ))) return;
    if ((key = get_hkey_obj( req->hkey, KEY_SET_VALUE )))
    {
        size_t datalen = get_req_data_size() - req->namelen;
        const char *data = (const char *)get_req_data() + req->namelen;

        set_value( key, name, req->type, data, datalen );
        release_object( key );
    }
}

/* retrieve the value of a registry key */
DECL_HANDLER(get_key_value)
{
    struct key *key;
    WCHAR *name;

    reply->total = 0;
    if (!(name = copy_path( get_req_data(), get_req_data_size(), 0 ))) return;
    if ((key = get_hkey_obj( req->hkey, KEY_QUERY_VALUE )))
    {
        get_value( key, name, &reply->type, &reply->total );
        release_object( key );
    }
}

/* enumerate the value of a registry key */
DECL_HANDLER(enum_key_value)
{
    struct key *key;

    if ((key = get_hkey_obj( req->hkey, KEY_QUERY_VALUE )))
    {
        enum_value( key, req->index, req->info_class, reply );
        release_object( key );
    }
}

/* delete a value of a registry key */
DECL_HANDLER(delete_key_value)
{
    WCHAR *name;
    struct key *key;

    if ((key = get_hkey_obj( req->hkey, KEY_SET_VALUE )))
    {
        if ((name = req_strdupW( req, get_req_data(), get_req_data_size() )))
        {
            delete_value( key, name );
            free( name );
        }
        release_object( key );
    }
}

/* load a registry branch from a file */
DECL_HANDLER(load_registry)
{
    struct key *key, *parent;
    struct token *token = thread_get_impersonation_token( current );

    const LUID_AND_ATTRIBUTES privs[] =
    {
        { SeBackupPrivilege,  0 },
        { SeRestorePrivilege, 0 },
    };

    if (!token || !token_check_privileges( token, TRUE, privs,
                                           sizeof(privs)/sizeof(privs[0]), NULL ))
    {
        set_error( STATUS_PRIVILEGE_NOT_HELD );
        return;
    }

    if ((parent = get_hkey_obj( req->hkey, 0 )))
    {
        int dummy;
        WCHAR *name = copy_path( get_req_data(), get_req_data_size(), !req->hkey );
        if (name && (key = create_key( parent, name, NULL, KEY_DIRTY, time(NULL), &dummy )))
        {
            load_registry( key, req->file );
            release_object( key );
        }
        release_object( parent );
    }
}

DECL_HANDLER(unload_registry)
{
    struct key *key;
    struct token *token = thread_get_impersonation_token( current );

    const LUID_AND_ATTRIBUTES privs[] =
    {
        { SeBackupPrivilege,  0 },
        { SeRestorePrivilege, 0 },
    };

    if (!token || !token_check_privileges( token, TRUE, privs,
                                           sizeof(privs)/sizeof(privs[0]), NULL ))
    {
        set_error( STATUS_PRIVILEGE_NOT_HELD );
        return;
    }

    if ((key = get_hkey_obj( req->hkey, 0 )))
    {
        delete_key( key, 1 );     /* FIXME */
        release_object( key );
    }
}

/* save a registry branch to a file */
DECL_HANDLER(save_registry)
{
    struct key *key;

    if (!thread_single_check_privilege( current, &SeBackupPrivilege ))
    {
        set_error( STATUS_PRIVILEGE_NOT_HELD );
        return;
    }

    if ((key = get_hkey_obj( req->hkey, 0 )))
    {
        save_registry( key, req->file );
        release_object( key );
    }
}

/* add a registry key change notification */
DECL_HANDLER(set_registry_notification)
{
    struct key *key;
    struct event *event;
    struct notify *notify;

    key = get_hkey_obj( req->hkey, KEY_NOTIFY );
    if (key)
    {
        event = get_event_obj( current->process, req->event, SYNCHRONIZE );
        if (event)
        {
            notify = find_notify( key, current->process, req->hkey );
            if (notify)
            {
                release_object( notify->event );
                grab_object( event );
                notify->event = event;
            }
            else
            {
                notify = mem_alloc( sizeof(*notify) );
                if (notify)
                {
                    grab_object( event );
                    notify->event   = event;
                    notify->subtree = req->subtree;
                    notify->filter  = req->filter;
                    notify->hkey    = req->hkey;
                    notify->process = current->process;
                    list_add_head( &key->notify_list, &notify->entry );
                }
            }
            release_object( event );
        }
        release_object( key );
    }
}
