/*
 * Server-side serial port communications management
 *
 * Copyright (C) 1998 Alexandre Julliard
 * Copyright (C) 2000,2001 Mike McCormack
 *
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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_UTIME_H
#include <utime.h>
#endif
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "windef.h"
#include "winternl.h"

#include "file.h"
#include "handle.h"
#include "thread.h"
#include "request.h"

static void serial_dump( struct object *obj, int verbose );
static struct fd *serial_get_fd( struct object *obj );
static void serial_destroy(struct object *obj);

static int serial_get_poll_events( struct fd *fd );
static void serial_poll_event( struct fd *fd, int event );
static int serial_get_info( struct fd *fd );
static int serial_flush( struct fd *fd, struct event **event );
static void serial_queue_async( struct fd *fd, void *apc, void *user, void *iosb, int type, int count );
static void serial_cancel_async( struct fd *fd );

struct serial
{
    struct object       obj;
    struct fd          *fd;
    unsigned int        options;

    /* timeout values */
    unsigned int        readinterval;
    unsigned int        readconst;
    unsigned int        readmult;
    unsigned int        writeconst;
    unsigned int        writemult;

    unsigned int        eventmask;
    unsigned int        commerror;

    struct termios      original;

    struct list         read_q;
    struct list         write_q;
    struct list         wait_q;

    /* FIXME: add dcb, comm status, handler module, sharing */
};

static const struct object_ops serial_ops =
{
    sizeof(struct serial),        /* size */
    serial_dump,                  /* dump */
    default_fd_add_queue,         /* add_queue */
    default_fd_remove_queue,      /* remove_queue */
    default_fd_signaled,          /* signaled */
    no_satisfied,                 /* satisfied */
    no_signal,                    /* signal */
    serial_get_fd,                /* get_fd */
    no_close_handle,              /* close_handle */
    serial_destroy                /* destroy */
};

static const struct fd_ops serial_fd_ops =
{
    serial_get_poll_events,       /* get_poll_events */
    serial_poll_event,            /* poll_event */
    serial_flush,                 /* flush */
    serial_get_info,              /* get_file_info */
    serial_queue_async,           /* queue_async */
    serial_cancel_async           /* cancel_async */
};

/* check if the given fd is a serial port */
int is_serial_fd( struct fd *fd )
{
    struct termios tios;

    return !tcgetattr( get_unix_fd(fd), &tios );
}

/* create a serial object for a given fd */
struct object *create_serial( struct fd *fd, unsigned int options )
{
    struct serial *serial;
    int unix_fd;

    if ((unix_fd = dup( get_unix_fd(fd) )) == -1) return NULL;

    /* set the fd back to blocking if necessary */
    if (options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT))
        fcntl( unix_fd, F_SETFL, 0 );

    if (!(serial = alloc_object( &serial_ops )))
    {
        close( unix_fd );
        return NULL;
    }
    serial->options      = options;
    serial->readinterval = 0;
    serial->readmult     = 0;
    serial->readconst    = 0;
    serial->writemult    = 0;
    serial->writeconst   = 0;
    serial->eventmask    = 0;
    serial->commerror    = 0;
    list_init( &serial->read_q );
    list_init( &serial->write_q );
    list_init( &serial->wait_q );
    if (!(serial->fd = create_anonymous_fd( &serial_fd_ops, unix_fd, &serial->obj )))
    {
        release_object( serial );
        return NULL;
    }
    return &serial->obj;
}

static struct fd *serial_get_fd( struct object *obj )
{
    struct serial *serial = (struct serial *)obj;
    return (struct fd *)grab_object( serial->fd );
}

static void serial_destroy( struct object *obj)
{
    struct serial *serial = (struct serial *)obj;

    async_terminate_queue( &serial->read_q, STATUS_CANCELLED );
    async_terminate_queue( &serial->write_q, STATUS_CANCELLED );
    async_terminate_queue( &serial->wait_q, STATUS_CANCELLED );
    if (serial->fd) release_object( serial->fd );
}

static void serial_dump( struct object *obj, int verbose )
{
    struct serial *serial = (struct serial *)obj;
    assert( obj->ops == &serial_ops );
    fprintf( stderr, "Port fd=%p mask=%x\n", serial->fd, serial->eventmask );
}

static struct serial *get_serial_obj( struct process *process, obj_handle_t handle, unsigned int access )
{
    return (struct serial *)get_handle_obj( process, handle, access, &serial_ops );
}

static int serial_get_poll_events( struct fd *fd )
{
    struct serial *serial = get_fd_user( fd );
    int events = 0;
    assert( serial->obj.ops == &serial_ops );

    if (!list_empty( &serial->read_q ))  events |= POLLIN;
    if (!list_empty( &serial->write_q )) events |= POLLOUT;
    if (!list_empty( &serial->wait_q ))  events |= POLLIN;

    /* fprintf(stderr,"poll events are %04x\n",events); */

    return events;
}

static int serial_get_info( struct fd *fd )
{
    int flags = 0;
    struct serial *serial = get_fd_user( fd );
    assert( serial->obj.ops == &serial_ops );

    if (!(serial->options & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)))
        flags |= FD_FLAG_OVERLAPPED;
    else if (!(serial->readinterval == MAXDWORD &&
               serial->readmult == 0 && serial->readconst == 0))
        flags |= FD_FLAG_TIMEOUT;
    if (serial->readinterval == MAXDWORD &&
        serial->readmult == 0 && serial->readconst == 0)
        flags |= FD_FLAG_AVAILABLE;

    return flags;
}

static void serial_poll_event(struct fd *fd, int event)
{
    struct serial *serial = get_fd_user( fd );

    /* fprintf(stderr,"Poll event %02x\n",event); */

    if (!list_empty( &serial->read_q ) && (POLLIN & event) )
        async_terminate_head( &serial->read_q, STATUS_ALERTED );

    if (!list_empty( &serial->write_q ) && (POLLOUT & event) )
        async_terminate_head( &serial->write_q, STATUS_ALERTED );

    if (!list_empty( &serial->wait_q ) && (POLLIN & event) )
        async_terminate_head( &serial->wait_q, STATUS_ALERTED );

    set_fd_events( fd, serial_get_poll_events(fd) );
}

static void serial_queue_async( struct fd *fd, void *apc, void *user, void *iosb,
                                int type, int count )
{
    struct serial *serial = get_fd_user( fd );
    struct list *queue;
    int timeout;
    int events;

    assert(serial->obj.ops == &serial_ops);

    switch (type)
    {
    case ASYNC_TYPE_READ:
        queue = &serial->read_q;
        timeout = serial->readconst + serial->readmult*count;
        break;
    case ASYNC_TYPE_WAIT:
        queue = &serial->wait_q;
        timeout = 0;
        break;
    case ASYNC_TYPE_WRITE:
        queue = &serial->write_q;
        timeout = serial->writeconst + serial->writemult*count;
        break;
    default:
        set_error(STATUS_INVALID_PARAMETER);
        return;
    }

    if (!create_async( current, &timeout, queue, apc, user, iosb ))
        return;

    /* Check if the new pending request can be served immediately */
    events = check_fd_events( fd, serial_get_poll_events( fd ) );
    if (events)
    {
        /* serial_poll_event() calls set_select_events() */
        serial_poll_event( fd, events );
        return;
    }

    set_fd_events( fd, serial_get_poll_events( fd ) );
}

static void serial_cancel_async( struct fd *fd )
{
    struct serial *serial = get_fd_user( fd );
    assert(serial->obj.ops == &serial_ops);

    async_terminate_queue( &serial->read_q, STATUS_CANCELLED );
    async_terminate_queue( &serial->write_q, STATUS_CANCELLED );
    async_terminate_queue( &serial->wait_q, STATUS_CANCELLED );
}

static int serial_flush( struct fd *fd, struct event **event )
{
    /* MSDN says: If hFile is a handle to a communications device,
     * the function only flushes the transmit buffer.
     */
    int ret = (tcflush( get_unix_fd(fd), TCOFLUSH ) != -1);
    if (!ret) file_set_error();
    return ret;
}

DECL_HANDLER(get_serial_info)
{
    struct serial *serial;

    if ((serial = get_serial_obj( current->process, req->handle, 0 )))
    {
        /* timeouts */
        reply->readinterval = serial->readinterval;
        reply->readconst    = serial->readconst;
        reply->readmult     = serial->readmult;
        reply->writeconst   = serial->writeconst;
        reply->writemult    = serial->writemult;

        /* event mask */
        reply->eventmask    = serial->eventmask;

        /* comm port error status */
        reply->commerror    = serial->commerror;

        release_object( serial );
    }
}

DECL_HANDLER(set_serial_info)
{
    struct serial *serial;

    if ((serial = get_serial_obj( current->process, req->handle, 0 )))
    {
        /* timeouts */
        if (req->flags & SERIALINFO_SET_TIMEOUTS)
        {
            serial->readinterval = req->readinterval;
            serial->readconst    = req->readconst;
            serial->readmult     = req->readmult;
            serial->writeconst   = req->writeconst;
            serial->writemult    = req->writemult;
        }

        /* event mask */
        if (req->flags & SERIALINFO_SET_MASK)
        {
            serial->eventmask = req->eventmask;
            if (!serial->eventmask)
            {
                async_terminate_queue( &serial->wait_q, STATUS_SUCCESS );
            }
        }

        /* comm port error status */
        if (req->flags & SERIALINFO_SET_ERROR)
        {
            serial->commerror = req->commerror;
        }

        release_object( serial );
    }
}
