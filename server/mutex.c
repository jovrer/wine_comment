/*
 * Server-side mutex management
 *
 * Copyright (C) 1998 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "windef.h"
#include "winternl.h"

#include "handle.h"
#include "thread.h"
#include "request.h"

struct mutex
{
    struct object  obj;             /* object header */
    struct thread *owner;           /* mutex owner */
    unsigned int   count;           /* recursion count */
    int            abandoned;       /* has it been abandoned? */
    struct list    entry;           /* entry in owner thread mutex list */
};

static void mutex_dump( struct object *obj, int verbose );
static int mutex_signaled( struct object *obj, struct thread *thread );
static int mutex_satisfied( struct object *obj, struct thread *thread );
static void mutex_destroy( struct object *obj );
static int mutex_signal( struct object *obj, unsigned int access );

static const struct object_ops mutex_ops =
{
    sizeof(struct mutex),      /* size */
    mutex_dump,                /* dump */
    add_queue,                 /* add_queue */
    remove_queue,              /* remove_queue */
    mutex_signaled,            /* signaled */
    mutex_satisfied,           /* satisfied */
    mutex_signal,              /* signal */
    no_get_fd,                 /* get_fd */
    no_close_handle,           /* close_handle */
    mutex_destroy              /* destroy */
};


static struct mutex *create_mutex( const WCHAR *name, size_t len, unsigned int attr,
                                   int owned )
{
    struct mutex *mutex;

    if ((mutex = create_named_object( sync_namespace, &mutex_ops, name, len, attr )))
    {
        if (get_error() != STATUS_OBJECT_NAME_COLLISION)
        {
            /* initialize it if it didn't already exist */
            mutex->count = 0;
            mutex->owner = NULL;
            mutex->abandoned = 0;
            if (owned) mutex_satisfied( &mutex->obj, current );
        }
    }
    return mutex;
}

/* release a mutex once the recursion count is 0 */
static void do_release( struct mutex *mutex )
{
    assert( !mutex->count );
    /* remove the mutex from the thread list of owned mutexes */
    list_remove( &mutex->entry );
    mutex->owner = NULL;
    wake_up( &mutex->obj, 0 );
}

void abandon_mutexes( struct thread *thread )
{
    struct list *ptr;

    while ((ptr = list_head( &thread->mutex_list )) != NULL)
    {
        struct mutex *mutex = LIST_ENTRY( ptr, struct mutex, entry );
        assert( mutex->owner == thread );
        mutex->count = 0;
        mutex->abandoned = 1;
        do_release( mutex );
    }
}

static void mutex_dump( struct object *obj, int verbose )
{
    struct mutex *mutex = (struct mutex *)obj;
    assert( obj->ops == &mutex_ops );
    fprintf( stderr, "Mutex count=%u owner=%p ", mutex->count, mutex->owner );
    dump_object_name( &mutex->obj );
    fputc( '\n', stderr );
}

static int mutex_signaled( struct object *obj, struct thread *thread )
{
    struct mutex *mutex = (struct mutex *)obj;
    assert( obj->ops == &mutex_ops );
    return (!mutex->count || (mutex->owner == thread));
}

static int mutex_satisfied( struct object *obj, struct thread *thread )
{
    struct mutex *mutex = (struct mutex *)obj;
    assert( obj->ops == &mutex_ops );
    assert( !mutex->count || (mutex->owner == thread) );

    if (!mutex->count++)  /* FIXME: avoid wrap-around */
    {
        assert( !mutex->owner );
        mutex->owner = thread;
        list_add_head( &thread->mutex_list, &mutex->entry );
    }
    if (!mutex->abandoned) return 0;
    mutex->abandoned = 0;
    return 1;
}

static int mutex_signal( struct object *obj, unsigned int access )
{
    struct mutex *mutex = (struct mutex *)obj;
    assert( obj->ops == &mutex_ops );

    if (!(access & SYNCHRONIZE))  /* FIXME: MUTEX_MODIFY_STATE? */
    {
        set_error( STATUS_ACCESS_DENIED );
        return 0;
    }
    if (!mutex->count || (mutex->owner != current))
    {
        set_error( STATUS_MUTANT_NOT_OWNED );
        return 0;
    }
    if (!--mutex->count) do_release( mutex );
    return 1;
}

static void mutex_destroy( struct object *obj )
{
    struct mutex *mutex = (struct mutex *)obj;
    assert( obj->ops == &mutex_ops );

    if (!mutex->count) return;
    mutex->count = 0;
    do_release( mutex );
}

/* create a mutex */
DECL_HANDLER(create_mutex)
{
    struct mutex *mutex;

    reply->handle = 0;
    if ((mutex = create_mutex( get_req_data(), get_req_data_size(), req->attributes,
                               req->owned )))
    {
        reply->handle = alloc_handle( current->process, mutex, req->access,
                                      req->attributes & OBJ_INHERIT );
        release_object( mutex );
    }
}

/* open a handle to a mutex */
DECL_HANDLER(open_mutex)
{
    reply->handle = open_object( sync_namespace, get_req_data(), get_req_data_size(),
                                 &mutex_ops, req->access, req->attributes );
}

/* release a mutex */
DECL_HANDLER(release_mutex)
{
    struct mutex *mutex;

    if ((mutex = (struct mutex *)get_handle_obj( current->process, req->handle,
                                                 MUTEX_MODIFY_STATE, &mutex_ops )))
    {
        if (!mutex->count || (mutex->owner != current)) set_error( STATUS_MUTANT_NOT_OWNED );
        else
        {
            reply->prev_count = mutex->count;
            if (!--mutex->count) do_release( mutex );
        }
        release_object( mutex );
    }
}
