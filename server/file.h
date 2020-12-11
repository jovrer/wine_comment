/*
 * Server-side file definitions
 *
 * Copyright (C) 2003 Alexandre Julliard
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

#ifndef __WINE_SERVER_FILE_H
#define __WINE_SERVER_FILE_H

#include "object.h"

struct fd;

typedef unsigned __int64 file_pos_t;

/* operations valid on file descriptor objects */
struct fd_ops
{
    /* get the events we want to poll() for on this object */
    int  (*get_poll_events)(struct fd *);
    /* a poll() event occurred */
    void (*poll_event)(struct fd *,int event);
    /* flush the object buffers */
    int  (*flush)(struct fd *, struct event **);
    /* get file information */
    int  (*get_file_info)(struct fd *);
    /* queue an async operation */
    void (*queue_async)(struct fd *, void* apc, void* user, void* io_sb, int type, int count);
    /* cancel an async operation */
    void (*cancel_async)(struct fd *);
};

/* file descriptor functions */

extern struct fd *alloc_fd( const struct fd_ops *fd_user_ops, struct object *user );
extern struct fd *open_fd( struct fd *fd, const char *name, int flags, mode_t *mode,
                           unsigned int access, unsigned int sharing, unsigned int options );
extern struct fd *create_anonymous_fd( const struct fd_ops *fd_user_ops,
                                       int unix_fd, struct object *user );
extern void *get_fd_user( struct fd *fd );
extern int get_unix_fd( struct fd *fd );
extern int is_same_file_fd( struct fd *fd1, struct fd *fd2 );
extern void fd_poll_event( struct fd *fd, int event );
extern int check_fd_events( struct fd *fd, int events );
extern void set_fd_events( struct fd *fd, int events );
extern obj_handle_t lock_fd( struct fd *fd, file_pos_t offset, file_pos_t count, int shared, int wait );
extern void unlock_fd( struct fd *fd, file_pos_t offset, file_pos_t count );

extern int default_fd_add_queue( struct object *obj, struct wait_queue_entry *entry );
extern void default_fd_remove_queue( struct object *obj, struct wait_queue_entry *entry );
extern int default_fd_signaled( struct object *obj, struct thread *thread );
extern int default_fd_get_poll_events( struct fd *fd );
extern void default_poll_event( struct fd *fd, int event );
extern void fd_queue_async_timeout( struct fd *fd, void *apc, void *user, void *io_sb, int type, int count, int *timeout );
extern void default_fd_queue_async( struct fd *fd, void *apc, void *user, void *io_sb, int type, int count );
extern void default_fd_cancel_async( struct fd *fd );
extern int no_flush( struct fd *fd, struct event **event );
extern int no_get_file_info( struct fd *fd );
extern void no_queue_async( struct fd *fd, void* apc, void* user, void* io_sb, int type, int count);
extern void no_cancel_async( struct fd *fd );
extern void main_loop(void);
extern void remove_process_locks( struct process *process );

inline static struct fd *get_obj_fd( struct object *obj ) { return obj->ops->get_fd( obj ); }

/* timeout functions */

struct timeout_user;

typedef void (*timeout_callback)( void *private );

extern struct timeout_user *add_timeout_user( const struct timeval *when,
                                              timeout_callback func, void *private );
extern void remove_timeout_user( struct timeout_user *user );
extern void add_timeout( struct timeval *when, int timeout );
/* return 1 if t1 is before t2 */
static inline int time_before( const struct timeval *t1, const struct timeval *t2 )
{
    return ((t1->tv_sec < t2->tv_sec) ||
            ((t1->tv_sec == t2->tv_sec) && (t1->tv_usec < t2->tv_usec)));
}

/* file functions */

extern struct file *get_file_obj( struct process *process, obj_handle_t handle,
                                  unsigned int access );
extern int get_file_unix_fd( struct file *file );
extern int is_same_file( struct file *file1, struct file *file2 );
extern int grow_file( struct file *file, file_pos_t size );
extern struct file *create_temp_file( int access );
extern void file_set_error(void);

/* change notification functions */

extern void do_change_notify( int unix_fd );
extern void sigio_callback(void);

/* serial port functions */

extern int is_serial_fd( struct fd *fd );
extern struct object *create_serial( struct fd *fd, unsigned int options );

/* async I/O functions */
extern struct async *create_async( struct thread *thread, int* timeout,
                                   struct list *queue, void *, void *, void *);
extern void async_terminate_head( struct list *queue, int status );

static inline void async_terminate_queue( struct list *queue, int status )
{
    while (!list_empty( queue )) async_terminate_head( queue, status );
}

#endif  /* __WINE_SERVER_FILE_H */
