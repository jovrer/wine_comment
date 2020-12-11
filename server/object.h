/*
 * Wine server objects
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

#ifndef __WINE_SERVER_OBJECT_H
#define __WINE_SERVER_OBJECT_H

#ifdef HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#include <sys/time.h>
#include "wine/server_protocol.h"
#include "wine/list.h"

#define DEBUG_OBJECTS

/* kernel objects */

struct namespace;
struct object;
struct object_name;
struct thread;
struct process;
struct token;
struct file;
struct wait_queue_entry;
struct async;
struct async_queue;
struct winstation;

/* operations valid on all objects */
struct object_ops
{
    /* size of this object type */
    size_t size;
    /* dump the object (for debugging) */
    void (*dump)(struct object *,int);
    /* add a thread to the object wait queue */
    int  (*add_queue)(struct object *,struct wait_queue_entry *);
    /* remove a thread from the object wait queue */
    void (*remove_queue)(struct object *,struct wait_queue_entry *);
    /* is object signaled? */
    int  (*signaled)(struct object *,struct thread *);
    /* wait satisfied; return 1 if abandoned */
    int  (*satisfied)(struct object *,struct thread *);
    /* signal an object */
    int  (*signal)(struct object *, unsigned int);
    /* return an fd object that can be used to read/write from the object */
    struct fd *(*get_fd)(struct object *);
    /* close a handle to this object */
    int (*close_handle)(struct object *,struct process *,obj_handle_t);
    /* destroy on refcount == 0 */
    void (*destroy)(struct object *);
};

struct object
{
    unsigned int              refcount;    /* reference count */
    const struct object_ops  *ops;
    struct list               wait_queue;
    struct object_name       *name;
#ifdef DEBUG_OBJECTS
    struct list               obj_list;
#endif
};

struct wait_queue_entry
{
    struct list     entry;
    struct object  *obj;
    struct thread  *thread;
};

extern void *mem_alloc( size_t size );  /* malloc wrapper */
extern void *memdup( const void *data, size_t len );
extern void *alloc_object( const struct object_ops *ops );
extern const WCHAR *get_object_name( struct object *obj, size_t *len );
extern void dump_object_name( struct object *obj );
extern void *create_named_object( struct namespace *namespace, const struct object_ops *ops,
                                  const WCHAR *name, size_t len, unsigned int attributes );
extern struct namespace *create_namespace( unsigned int hash_size );
/* grab/release_object can take any pointer, but you better make sure */
/* that the thing pointed to starts with a struct object... */
extern struct object *grab_object( void *obj );
extern void release_object( void *obj );
extern struct object *find_object( const struct namespace *namespace, const WCHAR *name, size_t len,
                                   unsigned int attributes );
extern int no_add_queue( struct object *obj, struct wait_queue_entry *entry );
extern int no_satisfied( struct object *obj, struct thread *thread );
extern int no_signal( struct object *obj, unsigned int access );
extern struct fd *no_get_fd( struct object *obj );
extern int no_close_handle( struct object *obj, struct process *process, obj_handle_t handle );
extern void no_destroy( struct object *obj );
#ifdef DEBUG_OBJECTS
extern void dump_objects(void);
#endif

/* event functions */

struct event;

extern struct event *create_event( const WCHAR *name, size_t len, unsigned int attr,
                                   int manual_reset, int initial_state );
extern struct event *get_event_obj( struct process *process, obj_handle_t handle, unsigned int access );
extern void pulse_event( struct event *event );
extern void set_event( struct event *event );
extern void reset_event( struct event *event );

/* mutex functions */

extern void abandon_mutexes( struct thread *thread );

/* serial functions */

int get_serial_async_timeout(struct object *obj, int type, int count);

/* socket functions */

extern void sock_init(void);

/* debugger functions */

extern int set_process_debugger( struct process *process, struct thread *debugger );
extern void generate_debug_event( struct thread *thread, int code, void *arg );
extern void generate_startup_debug_events( struct process *process, void *entry );
extern void debug_exit_thread( struct thread *thread );

/* mapping functions */

extern int get_page_size(void);

/* registry functions */

extern void init_registry(void);
extern void flush_registry(void);
extern void close_registry(void);

/* signal functions */

extern void start_watchdog(void);
extern void stop_watchdog(void);
extern int watchdog_triggered(void);
extern void init_signals(void);
extern void close_signals(void);

/* atom functions */

extern atom_t add_global_atom( struct winstation *winstation, const WCHAR *str, size_t len );
extern atom_t find_global_atom( struct winstation *winstation, const WCHAR *str, size_t len );
extern int grab_global_atom( struct winstation *winstation, atom_t atom );
extern void release_global_atom( struct winstation *winstation, atom_t atom );

/* global variables */

  /* command-line options */
extern int debug_level;
extern int master_socket_timeout;
extern int foreground;
extern const char *server_argv0;

  /* server start time used for GetTickCount() */
extern time_t server_start_time;

/* name space for synchronization objects */
extern struct namespace *sync_namespace;

#endif  /* __WINE_SERVER_OBJECT_H */
