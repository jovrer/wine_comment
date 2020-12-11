/*
 * Copyright 1999, 2000 Juergen Schmied
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <assert.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#ifdef HAVE_LINUX_MAJOR_H
# include <linux/major.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#ifdef HAVE_UTIME_H
# include <utime.h>
#endif
#ifdef STATFS_DEFINED_BY_SYS_VFS
# include <sys/vfs.h>
#else
# ifdef STATFS_DEFINED_BY_SYS_MOUNT
#  include <sys/mount.h>
# else
#  ifdef STATFS_DEFINED_BY_SYS_STATFS
#   include <sys/statfs.h>
#  endif
# endif
#endif

#ifdef HAVE_IOKIT_IOKITLIB_H
# include <IOKit/IOKitLib.h>
# include <CoreFoundation/CFNumber.h> /* for kCFBooleanTrue, kCFBooleanFalse */
# include <paths.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "wine/unicode.h"
#include "wine/debug.h"
#include "thread.h"
#include "wine/server.h"
#include "ntdll_misc.h"

#include "winternl.h"
#include "winioctl.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

mode_t FILE_umask = 0;

#define SECSPERDAY         86400
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)

/**************************************************************************
 *                 NtOpenFile				[NTDLL.@]
 *                 ZwOpenFile				[NTDLL.@]
 *
 * Open a file.
 *
 * PARAMS
 *  handle    [O] Variable that receives the file handle on return
 *  access    [I] Access desired by the caller to the file
 *  attr      [I] Structure describing the file to be opened
 *  io        [O] Receives details about the result of the operation
 *  sharing   [I] Type of shared access the caller requires
 *  options   [I] Options for the file open
 *
 * RETURNS
 *  Success: 0. FileHandle and IoStatusBlock are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtOpenFile( PHANDLE handle, ACCESS_MASK access,
                            POBJECT_ATTRIBUTES attr, PIO_STATUS_BLOCK io,
                            ULONG sharing, ULONG options )
{
    return NtCreateFile( handle, access, attr, io, NULL, 0,
                         sharing, FILE_OPEN, options, NULL, 0 );
}

/**************************************************************************
 *		NtCreateFile				[NTDLL.@]
 *		ZwCreateFile				[NTDLL.@]
 *
 * Either create a new file or directory, or open an existing file, device,
 * directory or volume.
 *
 * PARAMS
 *	handle       [O] Points to a variable which receives the file handle on return
 *	access       [I] Desired access to the file
 *	attr         [I] Structure describing the file
 *	io           [O] Receives information about the operation on return
 *	alloc_size   [I] Initial size of the file in bytes
 *	attributes   [I] Attributes to create the file with
 *	sharing      [I] Type of shared access the caller would like to the file
 *	disposition  [I] Specifies what to do, depending on whether the file already exists
 *	options      [I] Options for creating a new file
 *	ea_buffer    [I] Pointer to an extended attributes buffer
 *	ea_length    [I] Length of ea_buffer
 *
 * RETURNS
 *  Success: 0. handle and io are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtCreateFile( PHANDLE handle, ACCESS_MASK access, POBJECT_ATTRIBUTES attr,
                              PIO_STATUS_BLOCK io, PLARGE_INTEGER alloc_size,
                              ULONG attributes, ULONG sharing, ULONG disposition,
                              ULONG options, PVOID ea_buffer, ULONG ea_length )
{
    static const WCHAR pipeW[] = {'\\','?','?','\\','p','i','p','e','\\'};
    static const WCHAR mailslotW[] = {'\\','?','?','\\','M','A','I','L','S','L','O','T','\\'};
    ANSI_STRING unix_name;
    int created = FALSE;

    TRACE("handle=%p access=%08lx name=%s objattr=%08lx root=%p sec=%p io=%p alloc_size=%p\n"
          "attr=%08lx sharing=%08lx disp=%ld options=%08lx ea=%p.0x%08lx\n",
          handle, access, debugstr_us(attr->ObjectName), attr->Attributes,
          attr->RootDirectory, attr->SecurityDescriptor, io, alloc_size,
          attributes, sharing, disposition, options, ea_buffer, ea_length );

    if (!attr || !attr->ObjectName) return STATUS_INVALID_PARAMETER;

    if (attr->RootDirectory)
    {
        FIXME( "RootDirectory %p not supported\n", attr->RootDirectory );
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }
    if (alloc_size) FIXME( "alloc_size not supported\n" );

    /* check for named pipe */

    if (attr->ObjectName->Length > sizeof(pipeW) &&
        !memicmpW( attr->ObjectName->Buffer, pipeW, sizeof(pipeW)/sizeof(WCHAR) ))
    {
        SERVER_START_REQ( open_named_pipe )
        {
            req->access = access;
            req->attributes = (attr) ? attr->Attributes : 0;
            req->flags = options;
            wine_server_add_data( req, attr->ObjectName->Buffer + 4,
                                  attr->ObjectName->Length - 4*sizeof(WCHAR) );
            io->u.Status = wine_server_call( req );
            *handle = reply->handle;
        }
        SERVER_END_REQ;
        return io->u.Status;
    }

    /* check for mailslot */

    if (attr->ObjectName->Length > sizeof(mailslotW) &&
        !memicmpW( attr->ObjectName->Buffer, mailslotW, sizeof(mailslotW)/sizeof(WCHAR) ))
    {
        SERVER_START_REQ( open_mailslot )
        {
            req->access = access & GENERIC_WRITE;
            req->attributes = (attr) ? attr->Attributes : 0;
            req->sharing = sharing;
            wine_server_add_data( req, attr->ObjectName->Buffer + 4,
                                  attr->ObjectName->Length - 4*sizeof(WCHAR) );
            io->u.Status = wine_server_call( req );
            *handle = reply->handle;
        }
        SERVER_END_REQ;
        return io->u.Status;
    }

    io->u.Status = wine_nt_to_unix_file_name( attr->ObjectName, &unix_name, disposition,
                                              !(attr->Attributes & OBJ_CASE_INSENSITIVE) );

    if (io->u.Status == STATUS_NO_SUCH_FILE &&
        disposition != FILE_OPEN && disposition != FILE_OVERWRITE)
    {
        created = TRUE;
        io->u.Status = STATUS_SUCCESS;
    }

    if (io->u.Status == STATUS_SUCCESS)
    {
        SERVER_START_REQ( create_file )
        {
            req->access     = access;
            req->inherit    = (attr->Attributes & OBJ_INHERIT) != 0;
            req->sharing    = sharing;
            req->create     = disposition;
            req->options    = options;
            req->attrs      = attributes;
            wine_server_add_data( req, unix_name.Buffer, unix_name.Length );
            io->u.Status = wine_server_call( req );
            *handle = reply->handle;
        }
        SERVER_END_REQ;
        RtlFreeAnsiString( &unix_name );
    }
    else WARN("%s not found (%lx)\n", debugstr_us(attr->ObjectName), io->u.Status );

    if (io->u.Status == STATUS_SUCCESS)
    {
        if (created) io->Information = FILE_CREATED;
        else switch(disposition)
        {
        case FILE_SUPERSEDE:
            io->Information = FILE_SUPERSEDED;
            break;
        case FILE_CREATE:
            io->Information = FILE_CREATED;
            break;
        case FILE_OPEN:
        case FILE_OPEN_IF:
            io->Information = FILE_OPENED;
            break;
        case FILE_OVERWRITE:
        case FILE_OVERWRITE_IF:
            io->Information = FILE_OVERWRITTEN;
            break;
        }
    }

    return io->u.Status;
}

/***********************************************************************
 *                  Asynchronous file I/O                              *
 */
static void WINAPI FILE_AsyncReadService(void*, PIO_STATUS_BLOCK, ULONG);
static void WINAPI FILE_AsyncWriteService(void*, PIO_STATUS_BLOCK, ULONG);

typedef struct async_fileio
{
    HANDLE              handle;
    PIO_APC_ROUTINE     apc;
    void*               apc_user;
    char*               buffer;
    unsigned int        count;
    off_t               offset;
    int                 queue_apc_on_error;
    BOOL                avail_mode;
    int                 fd;
    HANDLE              event;
} async_fileio;

static void fileio_terminate(async_fileio *fileio, IO_STATUS_BLOCK* iosb)
{
    TRACE("data: %p\n", fileio);

    wine_server_release_fd( fileio->handle, fileio->fd );
    if ( fileio->event != INVALID_HANDLE_VALUE )
        NtSetEvent( fileio->event, NULL );

    if (fileio->apc && 
        (iosb->u.Status == STATUS_SUCCESS || fileio->queue_apc_on_error))
        fileio->apc( fileio->apc_user, iosb, iosb->Information );

    RtlFreeHeap( GetProcessHeap(), 0, fileio );
}


static ULONG fileio_queue_async(async_fileio* fileio, IO_STATUS_BLOCK* iosb, 
                                BOOL do_read)
{
    PIO_APC_ROUTINE     apc = do_read ? FILE_AsyncReadService : FILE_AsyncWriteService;
    NTSTATUS            status;

    SERVER_START_REQ( register_async )
    {
        req->handle = fileio->handle;
        req->io_apc = apc;
        req->io_sb = iosb;
        req->io_user = fileio;
        req->type = do_read ? ASYNC_TYPE_READ : ASYNC_TYPE_WRITE;
        req->count = (fileio->count < iosb->Information) ? 
            0 : fileio->count - iosb->Information;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;

    if ( status ) iosb->u.Status = status;
    if ( iosb->u.Status != STATUS_PENDING )
    {
        (apc)( fileio, iosb, iosb->u.Status );
        return iosb->u.Status;
    }
    NtCurrentTeb()->num_async_io++;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *           FILE_GetNtStatus(void)
 *
 * Retrieve the Nt Status code from errno.
 * Try to be consistent with FILE_SetDosError().
 */
NTSTATUS FILE_GetNtStatus(void)
{
    int err = errno;

    TRACE( "errno = %d\n", errno );
    switch (err)
    {
    case EAGAIN:    return STATUS_SHARING_VIOLATION;
    case EBADF:     return STATUS_INVALID_HANDLE;
    case EBUSY:     return STATUS_DEVICE_BUSY;
    case ENOSPC:    return STATUS_DISK_FULL;
    case EPERM:
    case EROFS:
    case EACCES:    return STATUS_ACCESS_DENIED;
    case ENOTDIR:   return STATUS_OBJECT_PATH_NOT_FOUND;
    case ENOENT:    return STATUS_OBJECT_NAME_NOT_FOUND;
    case EISDIR:    return STATUS_FILE_IS_A_DIRECTORY;
    case EMFILE:
    case ENFILE:    return STATUS_TOO_MANY_OPENED_FILES;
    case EINVAL:    return STATUS_INVALID_PARAMETER;
    case ENOTEMPTY: return STATUS_DIRECTORY_NOT_EMPTY;
    case EPIPE:     return STATUS_PIPE_BROKEN;
    case EIO:       return STATUS_DEVICE_NOT_READY;
#ifdef ENOMEDIUM
    case ENOMEDIUM: return STATUS_NO_MEDIA_IN_DEVICE;
#endif
    case ENOTTY:
    case EOPNOTSUPP:return STATUS_NOT_SUPPORTED;
    case ECONNRESET:return STATUS_PIPE_DISCONNECTED;
    case ENOEXEC:   /* ?? */
    case ESPIPE:    /* ?? */
    case EEXIST:    /* ?? */
    default:
        FIXME( "Converting errno %d to STATUS_UNSUCCESSFUL\n", err );
        return STATUS_UNSUCCESSFUL;
    }
}

/***********************************************************************
 *             FILE_AsyncReadService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void WINAPI FILE_AsyncReadService(void *user, PIO_STATUS_BLOCK iosb, ULONG status)
{
    async_fileio *fileio = (async_fileio*)user;
    int result;
    int already = iosb->Information;

    TRACE("%p %p 0x%lx\n", iosb, fileio->buffer, status);

    switch (status)
    {
    case STATUS_ALERTED: /* got some new data */
        if (iosb->u.Status != STATUS_PENDING) FIXME("unexpected status %08lx\n", iosb->u.Status);
        /* check to see if the data is ready (non-blocking) */
        if ( fileio->avail_mode )
            result = read(fileio->fd, &fileio->buffer[already], 
                          fileio->count - already);
        else
        {
            result = pread(fileio->fd, &fileio->buffer[already],
                           fileio->count - already,
                           fileio->offset + already);
            if ((result < 0) && (errno == ESPIPE))
                result = read(fileio->fd, &fileio->buffer[already], 
                              fileio->count - already);
        }

        if (result < 0)
        {
            if (errno == EAGAIN || errno == EINTR)
            {
                TRACE("Deferred read %d\n", errno);
                iosb->u.Status = STATUS_PENDING;
            }
            else /* check to see if the transfer is complete */
                iosb->u.Status = FILE_GetNtStatus();
        }
        else if (result == 0)
        {
            iosb->u.Status = iosb->Information ? STATUS_SUCCESS : STATUS_END_OF_FILE;
        }
        else
        {
            iosb->Information += result;
            if (iosb->Information >= fileio->count || fileio->avail_mode)
                iosb->u.Status = STATUS_SUCCESS;
            else
            {
                /* if we only have to read the available data, and none is available,
                 * simply cancel the request. If data was available, it has been read
                 * while in by previous call (NtDelayExecution)
                 */
                iosb->u.Status = (fileio->avail_mode) ? STATUS_SUCCESS : STATUS_PENDING;
            }

            TRACE("read %d more bytes %ld/%d so far (%s)\n",
                  result, iosb->Information, fileio->count, 
                  (iosb->u.Status == STATUS_SUCCESS) ? "success" : "pending");
        }
        /* queue another async operation ? */
        if (iosb->u.Status == STATUS_PENDING)
            fileio_queue_async(fileio, iosb, TRUE);
        else
            fileio_terminate(fileio, iosb);
        break;
    default:
        iosb->u.Status = status;
        fileio_terminate(fileio, iosb);
        break;
    }
}


/******************************************************************************
 *  NtReadFile					[NTDLL.@]
 *  ZwReadFile					[NTDLL.@]
 *
 * Read from an open file handle.
 *
 * PARAMS
 *  FileHandle    [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event         [I] Event to signal upon completion (or NULL)
 *  ApcRoutine    [I] Callback to call upon completion (or NULL)
 *  ApcContext    [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock [O] Receives information about the operation on return
 *  Buffer        [O] Destination for the data read
 *  Length        [I] Size of Buffer
 *  ByteOffset    [O] Destination for the new file pointer position (or NULL)
 *  Key           [O] Function unknown (may be NULL)
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated, and the Information member contains
 *           The number of bytes read.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtReadFile(HANDLE hFile, HANDLE hEvent,
                           PIO_APC_ROUTINE apc, void* apc_user,
                           PIO_STATUS_BLOCK io_status, void* buffer, ULONG length,
                           PLARGE_INTEGER offset, PULONG key)
{
    int unix_handle, flags;

    TRACE("(%p,%p,%p,%p,%p,%p,0x%08lx,%p,%p),partial stub!\n",
          hFile,hEvent,apc,apc_user,io_status,buffer,length,offset,key);

    if (!io_status) return STATUS_ACCESS_VIOLATION;

    io_status->Information = 0;
    io_status->u.Status = wine_server_handle_to_fd( hFile, GENERIC_READ, &unix_handle, &flags );
    if (io_status->u.Status) return io_status->u.Status;

    if (flags & FD_FLAG_RECV_SHUTDOWN)
    {
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_PIPE_DISCONNECTED;
    }

    if (flags & FD_FLAG_TIMEOUT)
    {
        if (hEvent)
        {
            /* this shouldn't happen, but check it */
            FIXME("NIY-hEvent\n");
            wine_server_release_fd( hFile, unix_handle );
            return STATUS_NOT_IMPLEMENTED;
        }
        io_status->u.Status = NtCreateEvent(&hEvent, EVENT_ALL_ACCESS, NULL, 0, 0);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }

    if (flags & (FD_FLAG_OVERLAPPED|FD_FLAG_TIMEOUT))
    {
        async_fileio*   fileio;
        NTSTATUS ret;

        if (!(fileio = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(async_fileio))))
        {
            wine_server_release_fd( hFile, unix_handle );
            if (flags & FD_FLAG_TIMEOUT) NtClose(hEvent);
            return STATUS_NO_MEMORY;
        }
        fileio->handle = hFile;
        fileio->count = length;
        if ( offset == NULL ) 
            fileio->offset = 0;
        else
        {
            fileio->offset = offset->QuadPart;
            if (offset->u.HighPart && fileio->offset == offset->u.LowPart)
                FIXME("High part of offset is lost\n");
        } 
        fileio->apc = apc;
        fileio->apc_user = apc_user;
        fileio->buffer = buffer;
        fileio->queue_apc_on_error = 0;
        fileio->avail_mode = (flags & FD_FLAG_AVAILABLE);
        fileio->fd = unix_handle;  /* FIXME */
        fileio->event = hEvent;
        NtResetEvent(hEvent, NULL);

        io_status->u.Status = STATUS_PENDING;
        ret = fileio_queue_async(fileio, io_status, TRUE);
        if (ret != STATUS_SUCCESS)
        {
            wine_server_release_fd( hFile, unix_handle );
            if (flags & FD_FLAG_TIMEOUT) NtClose(hEvent);
            return ret;
        }
        if (flags & FD_FLAG_TIMEOUT)
        {
            do
            {
                ret = NtWaitForSingleObject(hEvent, TRUE, NULL);
            }
            while (ret == STATUS_USER_APC && io_status->u.Status == STATUS_PENDING);
            NtClose(hEvent);
            if (ret != STATUS_USER_APC)
                fileio->queue_apc_on_error = 1;
        }
        else
        {
            LARGE_INTEGER   timeout;

            /* let some APC be run, this will read some already pending data */
            timeout.u.LowPart = timeout.u.HighPart = 0;
            ret = NtDelayExecution( TRUE, &timeout );
            /* the apc didn't run and therefore the completion routine now
             * needs to be sent errors.
             * Note that there is no race between setting this flag and
             * returning errors because apc's are run only during alertable
             * waits */
            if (ret != STATUS_USER_APC)
                fileio->queue_apc_on_error = 1;
        }
        TRACE("= 0x%08lx\n", io_status->u.Status);
        return io_status->u.Status;
    }

    if (offset)
    {
        FILE_POSITION_INFORMATION   fpi;

        fpi.CurrentByteOffset = *offset;
        io_status->u.Status = NtSetInformationFile(hFile, io_status, &fpi, sizeof(fpi), 
                                                   FilePositionInformation);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }
    /* code for synchronous reads */
    while ((io_status->Information = read( unix_handle, buffer, length )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if (errno == EFAULT)
        {
            io_status->Information = 0;
            io_status->u.Status = STATUS_ACCESS_VIOLATION;
        }
        else io_status->u.Status = FILE_GetNtStatus();
	break;
    }
    wine_server_release_fd( hFile, unix_handle );
    TRACE("= 0x%08lx\n", io_status->u.Status);
    return io_status->u.Status;
}

/***********************************************************************
 *             FILE_AsyncWriteService      (INTERNAL)
 *
 *  This function is called while the client is waiting on the
 *  server, so we can't make any server calls here.
 */
static void WINAPI FILE_AsyncWriteService(void *ovp, IO_STATUS_BLOCK *iosb, ULONG status)
{
    async_fileio *fileio = (async_fileio *) ovp;
    int result;
    int already = iosb->Information;

    TRACE("(%p %p 0x%lx)\n",iosb, fileio->buffer, status);

    switch (status)
    {
    case STATUS_ALERTED:
        /* write some data (non-blocking) */
        if ( fileio->avail_mode )
            result = write(fileio->fd, &fileio->buffer[already], 
                           fileio->count - already);
        else
        {
            result = pwrite(fileio->fd, &fileio->buffer[already], 
                            fileio->count - already, fileio->offset + already);
            if ((result < 0) && (errno == ESPIPE))
                result = write(fileio->fd, &fileio->buffer[already], 
                               fileio->count - already);
        }

        if (result < 0)
        {
            if (errno == EAGAIN || errno == EINTR) iosb->u.Status = STATUS_PENDING;
            else iosb->u.Status = FILE_GetNtStatus();
        }
        else
        {
            iosb->Information += result;
            iosb->u.Status = (iosb->Information < fileio->count) ? STATUS_PENDING : STATUS_SUCCESS;
            TRACE("wrote %d more bytes %ld/%d so far\n", 
                  result, iosb->Information, fileio->count);
        }
        if (iosb->u.Status == STATUS_PENDING)
            fileio_queue_async(fileio, iosb, FALSE);
        else
            fileio_terminate(fileio, iosb);
        break;
    default:
        iosb->u.Status = status;
        fileio_terminate(fileio, iosb);
        break;
    }
}

/******************************************************************************
 *  NtWriteFile					[NTDLL.@]
 *  ZwWriteFile					[NTDLL.@]
 *
 * Write to an open file handle.
 *
 * PARAMS
 *  FileHandle    [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event         [I] Event to signal upon completion (or NULL)
 *  ApcRoutine    [I] Callback to call upon completion (or NULL)
 *  ApcContext    [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock [O] Receives information about the operation on return
 *  Buffer        [I] Source for the data to write
 *  Length        [I] Size of Buffer
 *  ByteOffset    [O] Destination for the new file pointer position (or NULL)
 *  Key           [O] Function unknown (may be NULL)
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated, and the Information member contains
 *           The number of bytes written.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtWriteFile(HANDLE hFile, HANDLE hEvent,
                            PIO_APC_ROUTINE apc, void* apc_user,
                            PIO_STATUS_BLOCK io_status, 
                            const void* buffer, ULONG length,
                            PLARGE_INTEGER offset, PULONG key)
{
    int unix_handle, flags;

    TRACE("(%p,%p,%p,%p,%p,%p,0x%08lx,%p,%p)!\n",
          hFile,hEvent,apc,apc_user,io_status,buffer,length,offset,key);

    if (!io_status) return STATUS_ACCESS_VIOLATION;

    io_status->Information = 0;
    io_status->u.Status = wine_server_handle_to_fd( hFile, GENERIC_WRITE, &unix_handle, &flags );
    if (io_status->u.Status) return io_status->u.Status;

    if (flags & FD_FLAG_SEND_SHUTDOWN)
    {
        wine_server_release_fd( hFile, unix_handle );
        return STATUS_PIPE_DISCONNECTED;
    }

    if (flags & FD_FLAG_TIMEOUT)
    {
        if (hEvent)
        {
            /* this shouldn't happen, but check it */
            FIXME("NIY-hEvent\n");
            wine_server_release_fd( hFile, unix_handle );
            return STATUS_NOT_IMPLEMENTED;
        }
        io_status->u.Status = NtCreateEvent(&hEvent, EVENT_ALL_ACCESS, NULL, 0, 0);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }

    if (flags & (FD_FLAG_OVERLAPPED|FD_FLAG_TIMEOUT))
    {
        async_fileio*   fileio;
        NTSTATUS ret;

        if (!(fileio = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(async_fileio))))
        {
            wine_server_release_fd( hFile, unix_handle );
            if (flags & FD_FLAG_TIMEOUT) NtClose(hEvent);
            return STATUS_NO_MEMORY;
        }
        fileio->handle = hFile;
        fileio->count = length;
        if (offset)
        {
            fileio->offset = offset->QuadPart;
            if (offset->u.HighPart && fileio->offset == offset->u.LowPart)
                FIXME("High part of offset is lost\n");
        }
        else  
        {
            fileio->offset = 0;
        }
        fileio->apc = apc;
        fileio->apc_user = apc_user;
        fileio->buffer = (void*)buffer;
        fileio->queue_apc_on_error = 0;
        fileio->avail_mode = (flags & FD_FLAG_AVAILABLE);
        fileio->fd = unix_handle;  /* FIXME */
        fileio->event = hEvent;
        NtResetEvent(hEvent, NULL);

        io_status->Information = 0;
        io_status->u.Status = STATUS_PENDING;
        ret = fileio_queue_async(fileio, io_status, FALSE);
        if (ret != STATUS_SUCCESS)
        {
            wine_server_release_fd( hFile, unix_handle );
            if (flags & FD_FLAG_TIMEOUT) NtClose(hEvent);
            return ret;
        }
        if (flags & FD_FLAG_TIMEOUT)
        {
            do
            {
                ret = NtWaitForSingleObject(hEvent, TRUE, NULL);
            }
            while (ret == STATUS_USER_APC && io_status->u.Status == STATUS_PENDING);
            NtClose(hEvent);
            if (ret != STATUS_USER_APC)
                fileio->queue_apc_on_error = 1;
        }
        else
        {
            LARGE_INTEGER   timeout;

            /* let some APC be run, this will write as much data as possible */
            timeout.u.LowPart = timeout.u.HighPart = 0;
            ret = NtDelayExecution( TRUE, &timeout );
            /* the apc didn't run and therefore the completion routine now
             * needs to be sent errors.
             * Note that there is no race between setting this flag and
             * returning errors because apc's are run only during alertable
             * waits */
            if (ret != STATUS_USER_APC)
                fileio->queue_apc_on_error = 1;
        }
        return io_status->u.Status;
    }

    if (offset)
    {
        FILE_POSITION_INFORMATION   fpi;

        fpi.CurrentByteOffset = *offset;
        io_status->u.Status = NtSetInformationFile(hFile, io_status, &fpi, sizeof(fpi),
                                                   FilePositionInformation);
        if (io_status->u.Status)
        {
            wine_server_release_fd( hFile, unix_handle );
            return io_status->u.Status;
        }
    }

    /* synchronous file write */
    while ((io_status->Information = write( unix_handle, buffer, length )) == -1)
    {
        if ((errno == EAGAIN) || (errno == EINTR)) continue;
        if (errno == EFAULT)
        {
            io_status->Information = 0;
            io_status->u.Status = STATUS_INVALID_USER_BUFFER;
        }
        else if (errno == ENOSPC) io_status->u.Status = STATUS_DISK_FULL;
        else io_status->u.Status = FILE_GetNtStatus();
        break;
    }
    wine_server_release_fd( hFile, unix_handle );
    return io_status->u.Status;
}

/**************************************************************************
 *		NtDeviceIoControlFile			[NTDLL.@]
 *		ZwDeviceIoControlFile			[NTDLL.@]
 *
 * Perform an I/O control operation on an open file handle.
 *
 * PARAMS
 *  DeviceHandle     [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event            [I] Event to signal upon completion (or NULL)
 *  ApcRoutine       [I] Callback to call upon completion (or NULL)
 *  ApcContext       [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock    [O] Receives information about the operation on return
 *  IoControlCode    [I] Control code for the operation to perform
 *  InputBuffer      [I] Source for any input data required (or NULL)
 *  InputBufferSize  [I] Size of InputBuffer
 *  OutputBuffer     [O] Source for any output data returned (or NULL)
 *  OutputBufferSize [I] Size of OutputBuffer
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtDeviceIoControlFile(HANDLE DeviceHandle, HANDLE hEvent,
                                      PIO_APC_ROUTINE UserApcRoutine, 
                                      PVOID UserApcContext,
                                      PIO_STATUS_BLOCK IoStatusBlock,
                                      ULONG IoControlCode,
                                      PVOID InputBuffer,
                                      ULONG InputBufferSize,
                                      PVOID OutputBuffer,
                                      ULONG OutputBufferSize)
{
    TRACE("(%p,%p,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx)\n",
          DeviceHandle, hEvent, UserApcRoutine, UserApcContext,
          IoStatusBlock, IoControlCode, 
          InputBuffer, InputBufferSize, OutputBuffer, OutputBufferSize);

    if (CDROM_DeviceIoControl(DeviceHandle, hEvent,
                              UserApcRoutine, UserApcContext,
                              IoStatusBlock, IoControlCode,
                              InputBuffer, InputBufferSize,
                              OutputBuffer, OutputBufferSize) == STATUS_NO_SUCH_DEVICE)
    {
        /* it wasn't a CDROM */
        FIXME("Unimplemented dwIoControlCode=%08lx\n", IoControlCode);
        IoStatusBlock->u.Status = STATUS_NOT_IMPLEMENTED;
        IoStatusBlock->Information = 0;
        if (hEvent) NtSetEvent(hEvent, NULL);
    }
    return IoStatusBlock->u.Status;
}

/***********************************************************************
 *           pipe_completion_wait   (Internal)
 */
static void CALLBACK pipe_completion_wait(HANDLE event, PIO_STATUS_BLOCK iosb, ULONG status)
{
    TRACE("for %p/%p, status=%08lx\n", event, iosb, status);

    if (iosb)
        iosb->u.Status = status;
    NtSetEvent(event, NULL);
    TRACE("done\n");
}

/**************************************************************************
 *              NtFsControlFile                 [NTDLL.@]
 *              ZwFsControlFile                 [NTDLL.@]
 *
 * Perform a file system control operation on an open file handle.
 *
 * PARAMS
 *  DeviceHandle     [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  Event            [I] Event to signal upon completion (or NULL)
 *  ApcRoutine       [I] Callback to call upon completion (or NULL)
 *  ApcContext       [I] Context for ApcRoutine (or NULL)
 *  IoStatusBlock    [O] Receives information about the operation on return
 *  FsControlCode    [I] Control code for the operation to perform
 *  InputBuffer      [I] Source for any input data required (or NULL)
 *  InputBufferSize  [I] Size of InputBuffer
 *  OutputBuffer     [O] Source for any output data returned (or NULL)
 *  OutputBufferSize [I] Size of OutputBuffer
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtFsControlFile(HANDLE DeviceHandle, HANDLE Event OPTIONAL, PIO_APC_ROUTINE ApcRoutine,
                                PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock, ULONG FsControlCode,
                                PVOID InputBuffer, ULONG InputBufferSize, PVOID OutputBuffer, ULONG OutputBufferSize)
{
    NTSTATUS ret;

    TRACE("(%p,%p,%p,%p,%p,0x%08lx,%p,0x%08lx,%p,0x%08lx)\n",
    DeviceHandle,Event,ApcRoutine,ApcContext,IoStatusBlock,FsControlCode,
    InputBuffer,InputBufferSize,OutputBuffer,OutputBufferSize);

    if(!IoStatusBlock) return STATUS_INVALID_PARAMETER;

    switch(FsControlCode)
    {
        case FSCTL_DISMOUNT_VOLUME:
            ret = DIR_unmount_device( DeviceHandle );
            break;

        case FSCTL_PIPE_LISTEN :
        {
            HANDLE internal_event;

            if(!Event)
            {
                OBJECT_ATTRIBUTES obj;
                InitializeObjectAttributes(&obj, NULL, 0, 0, NULL);
                ret = NtCreateEvent(&internal_event, EVENT_ALL_ACCESS, &obj, FALSE, FALSE);
                if(ret != STATUS_SUCCESS) return ret;
            }

            SERVER_START_REQ(connect_named_pipe)
            {
                req->handle = DeviceHandle;
                req->event = Event ? Event : internal_event;
                req->func = pipe_completion_wait;
                ret = wine_server_call(req);
            }
            SERVER_END_REQ;

            if(ret == STATUS_SUCCESS)
            {
                if(Event)
                    ret = STATUS_PENDING;
                else
                {
                    do
                        ret = NtWaitForSingleObject(internal_event, TRUE, NULL);
                    while(ret == STATUS_USER_APC);
                    NtClose(internal_event);
                }
            }
            break;
        }
        case FSCTL_PIPE_DISCONNECT :
            SERVER_START_REQ(disconnect_named_pipe)
            {
                req->handle = DeviceHandle;
                ret = wine_server_call(req);
                if (!ret && reply->fd != -1) close(reply->fd);
            }
            SERVER_END_REQ;
            break;
        default :
            FIXME("Unsupported FsControlCode %lx\n", FsControlCode);
            ret = STATUS_NOT_SUPPORTED;
            break;
    }
    IoStatusBlock->u.Status = ret;
    return ret;
}

/******************************************************************************
 *  NtSetVolumeInformationFile		[NTDLL.@]
 *  ZwSetVolumeInformationFile		[NTDLL.@]
 *
 * Set volume information for an open file handle.
 *
 * PARAMS
 *  FileHandle         [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock      [O] Receives information about the operation on return
 *  FsInformation      [I] Source for volume information
 *  Length             [I] Size of FsInformation
 *  FsInformationClass [I] Type of volume information to set
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtSetVolumeInformationFile(
	IN HANDLE FileHandle,
	PIO_STATUS_BLOCK IoStatusBlock,
	PVOID FsInformation,
        ULONG Length,
	FS_INFORMATION_CLASS FsInformationClass)
{
	FIXME("(%p,%p,%p,0x%08lx,0x%08x) stub\n",
	FileHandle,IoStatusBlock,FsInformation,Length,FsInformationClass);
	return 0;
}

/******************************************************************************
 *  NtQueryInformationFile		[NTDLL.@]
 *  ZwQueryInformationFile		[NTDLL.@]
 *
 * Get information about an open file handle.
 *
 * PARAMS
 *  hFile    [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  io       [O] Receives information about the operation on return
 *  ptr      [O] Destination for file information
 *  len      [I] Size of FileInformation
 *  class    [I] Type of file information to get
 *
 * RETURNS
 *  Success: 0. IoStatusBlock and FileInformation are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtQueryInformationFile( HANDLE hFile, PIO_STATUS_BLOCK io,
                                        PVOID ptr, LONG len, FILE_INFORMATION_CLASS class )
{
    static const size_t info_sizes[] =
    {
        0,
        sizeof(FILE_DIRECTORY_INFORMATION),            /* FileDirectoryInformation */
        sizeof(FILE_FULL_DIRECTORY_INFORMATION),       /* FileFullDirectoryInformation */
        sizeof(FILE_BOTH_DIRECTORY_INFORMATION),       /* FileBothDirectoryInformation */
        sizeof(FILE_BASIC_INFORMATION),                /* FileBasicInformation */
        sizeof(FILE_STANDARD_INFORMATION),             /* FileStandardInformation */
        sizeof(FILE_INTERNAL_INFORMATION),             /* FileInternalInformation */
        sizeof(FILE_EA_INFORMATION),                   /* FileEaInformation */
        sizeof(FILE_ACCESS_INFORMATION),               /* FileAccessInformation */
        sizeof(FILE_NAME_INFORMATION)-sizeof(WCHAR),   /* FileNameInformation */
        sizeof(FILE_RENAME_INFORMATION)-sizeof(WCHAR), /* FileRenameInformation */
        0,                                             /* FileLinkInformation */
        sizeof(FILE_NAMES_INFORMATION)-sizeof(WCHAR),  /* FileNamesInformation */
        sizeof(FILE_DISPOSITION_INFORMATION),          /* FileDispositionInformation */
        sizeof(FILE_POSITION_INFORMATION),             /* FilePositionInformation */
        sizeof(FILE_FULL_EA_INFORMATION),              /* FileFullEaInformation */
        sizeof(FILE_MODE_INFORMATION),                 /* FileModeInformation */
        sizeof(FILE_ALIGNMENT_INFORMATION),            /* FileAlignmentInformation */
        sizeof(FILE_ALL_INFORMATION)-sizeof(WCHAR),    /* FileAllInformation */
        sizeof(FILE_ALLOCATION_INFORMATION),           /* FileAllocationInformation */
        sizeof(FILE_END_OF_FILE_INFORMATION),          /* FileEndOfFileInformation */
        0,                                             /* FileAlternateNameInformation */
        sizeof(FILE_STREAM_INFORMATION)-sizeof(WCHAR), /* FileStreamInformation */
        0,                                             /* FilePipeInformation */
        0,                                             /* FilePipeLocalInformation */
        0,                                             /* FilePipeRemoteInformation */
        sizeof(FILE_MAILSLOT_QUERY_INFORMATION),       /* FileMailslotQueryInformation */
        0,                                             /* FileMailslotSetInformation */
        0,                                             /* FileCompressionInformation */
        0,                                             /* FileObjectIdInformation */
        0,                                             /* FileCompletionInformation */
        0,                                             /* FileMoveClusterInformation */
        0,                                             /* FileQuotaInformation */
        0,                                             /* FileReparsePointInformation */
        0,                                             /* FileNetworkOpenInformation */
        0,                                             /* FileAttributeTagInformation */
        0                                              /* FileTrackingInformation */
    };

    struct stat st;
    int fd;

    TRACE("(%p,%p,%p,0x%08lx,0x%08x)\n", hFile, io, ptr, len, class);

    io->Information = 0;

    if (class <= 0 || class >= FileMaximumInformation)
        return io->u.Status = STATUS_INVALID_INFO_CLASS;
    if (!info_sizes[class])
    {
        FIXME("Unsupported class (%d)\n", class);
        return io->u.Status = STATUS_NOT_IMPLEMENTED;
    }
    if (len < info_sizes[class])
        return io->u.Status = STATUS_INFO_LENGTH_MISMATCH;

    if ((io->u.Status = wine_server_handle_to_fd( hFile, 0, &fd, NULL )))
        return io->u.Status;

    switch (class)
    {
    case FileBasicInformation:
        {
            FILE_BASIC_INFORMATION *info = ptr;

            if (fstat( fd, &st ) == -1)
                io->u.Status = FILE_GetNtStatus();
            else if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
                io->u.Status = STATUS_INVALID_INFO_CLASS;
            else
            {
                if (S_ISDIR(st.st_mode)) info->FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                else info->FileAttributes = FILE_ATTRIBUTE_ARCHIVE;
                if (!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
                    info->FileAttributes |= FILE_ATTRIBUTE_READONLY;
                RtlSecondsSince1970ToTime( st.st_mtime, &info->CreationTime);
                RtlSecondsSince1970ToTime( st.st_mtime, &info->LastWriteTime);
                RtlSecondsSince1970ToTime( st.st_ctime, &info->ChangeTime);
                RtlSecondsSince1970ToTime( st.st_atime, &info->LastAccessTime);
            }
        }
        break;
    case FileStandardInformation:
        {
            FILE_STANDARD_INFORMATION *info = ptr;

            if (fstat( fd, &st ) == -1) io->u.Status = FILE_GetNtStatus();
            else
            {
                if ((info->Directory = S_ISDIR(st.st_mode)))
                {
                    info->AllocationSize.QuadPart = 0;
                    info->EndOfFile.QuadPart      = 0;
                    info->NumberOfLinks           = 1;
                    info->DeletePending           = FALSE;
                }
                else
                {
                    info->AllocationSize.QuadPart = (ULONGLONG)st.st_blocks * 512;
                    info->EndOfFile.QuadPart      = st.st_size;
                    info->NumberOfLinks           = st.st_nlink;
                    info->DeletePending           = FALSE; /* FIXME */
                }
            }
        }
        break;
    case FilePositionInformation:
        {
            FILE_POSITION_INFORMATION *info = ptr;
            off_t res = lseek( fd, 0, SEEK_CUR );
            if (res == (off_t)-1) io->u.Status = FILE_GetNtStatus();
            else info->CurrentByteOffset.QuadPart = res;
        }
        break;
    case FileInternalInformation:
        {
            FILE_INTERNAL_INFORMATION *info = ptr;

            if (fstat( fd, &st ) == -1) io->u.Status = FILE_GetNtStatus();
            else info->IndexNumber.QuadPart = st.st_ino;
        }
        break;
    case FileEaInformation:
        {
            FILE_EA_INFORMATION *info = ptr;
            info->EaSize = 0;
        }
        break;
    case FileEndOfFileInformation:
        {
            FILE_END_OF_FILE_INFORMATION *info = ptr;

            if (fstat( fd, &st ) == -1) io->u.Status = FILE_GetNtStatus();
            else info->EndOfFile.QuadPart = S_ISDIR(st.st_mode) ? 0 : st.st_size;
        }
        break;
    case FileAllInformation:
        {
            FILE_ALL_INFORMATION *info = ptr;

            if (fstat( fd, &st ) == -1) io->u.Status = FILE_GetNtStatus();
            else if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
                io->u.Status = STATUS_INVALID_INFO_CLASS;
            else
            {
                if ((info->StandardInformation.Directory = S_ISDIR(st.st_mode)))
                {
                    info->BasicInformation.FileAttributes = FILE_ATTRIBUTE_DIRECTORY;
                    info->StandardInformation.AllocationSize.QuadPart = 0;
                    info->StandardInformation.EndOfFile.QuadPart      = 0;
                    info->StandardInformation.NumberOfLinks           = 1;
                    info->StandardInformation.DeletePending           = FALSE;
                }
                else
                {
                    info->BasicInformation.FileAttributes = FILE_ATTRIBUTE_ARCHIVE;
                    info->StandardInformation.AllocationSize.QuadPart = (ULONGLONG)st.st_blocks * 512;
                    info->StandardInformation.EndOfFile.QuadPart      = st.st_size;
                    info->StandardInformation.NumberOfLinks           = st.st_nlink;
                    info->StandardInformation.DeletePending           = FALSE; /* FIXME */
                }
                if (!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
                    info->BasicInformation.FileAttributes |= FILE_ATTRIBUTE_READONLY;
                RtlSecondsSince1970ToTime( st.st_mtime, &info->BasicInformation.CreationTime);
                RtlSecondsSince1970ToTime( st.st_mtime, &info->BasicInformation.LastWriteTime);
                RtlSecondsSince1970ToTime( st.st_ctime, &info->BasicInformation.ChangeTime);
                RtlSecondsSince1970ToTime( st.st_atime, &info->BasicInformation.LastAccessTime);
                info->InternalInformation.IndexNumber.QuadPart = st.st_ino;
                info->EaInformation.EaSize = 0;
                info->AccessInformation.AccessFlags = 0;  /* FIXME */
                info->PositionInformation.CurrentByteOffset.QuadPart = lseek( fd, 0, SEEK_CUR );
                info->ModeInformation.Mode = 0;  /* FIXME */
                info->AlignmentInformation.AlignmentRequirement = 1;  /* FIXME */
                info->NameInformation.FileNameLength = 0;
                io->Information = sizeof(*info) - sizeof(WCHAR);
            }
        }
        break;
    case FileMailslotQueryInformation:
        {
            FILE_MAILSLOT_QUERY_INFORMATION *info = ptr;

            SERVER_START_REQ( set_mailslot_info )
            {
                req->handle = hFile;
                req->flags = 0;
                io->u.Status = wine_server_call( req );
                if( io->u.Status == STATUS_SUCCESS )
                {
                    info->MaximumMessageSize = reply->max_msgsize;
                    info->MailslotQuota = 0;
                    info->NextMessageSize = reply->next_msgsize;
                    info->MessagesAvailable = reply->msg_count;
                    info->ReadTimeout.QuadPart = reply->read_timeout * -10000;
                }
            }
            SERVER_END_REQ;
        }
        break;
    default:
        FIXME("Unsupported class (%d)\n", class);
        io->u.Status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    wine_server_release_fd( hFile, fd );
    if (io->u.Status == STATUS_SUCCESS && !io->Information) io->Information = info_sizes[class];
    return io->u.Status;
}

/******************************************************************************
 *  NtSetInformationFile		[NTDLL.@]
 *  ZwSetInformationFile		[NTDLL.@]
 *
 * Set information about an open file handle.
 *
 * PARAMS
 *  handle  [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  io      [O] Receives information about the operation on return
 *  ptr     [I] Source for file information
 *  len     [I] Size of FileInformation
 *  class   [I] Type of file information to set
 *
 * RETURNS
 *  Success: 0. io is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtSetInformationFile(HANDLE handle, PIO_STATUS_BLOCK io,
                                     PVOID ptr, ULONG len, FILE_INFORMATION_CLASS class)
{
    int fd;

    TRACE("(%p,%p,%p,0x%08lx,0x%08x)\n", handle, io, ptr, len, class);

    if ((io->u.Status = wine_server_handle_to_fd( handle, 0, &fd, NULL )))
        return io->u.Status;

    io->u.Status = STATUS_SUCCESS;
    switch (class)
    {
    case FileBasicInformation:
        if (len >= sizeof(FILE_BASIC_INFORMATION))
        {
            struct stat st;
            const FILE_BASIC_INFORMATION *info = ptr;

            if (info->LastAccessTime.QuadPart || info->LastWriteTime.QuadPart)
            {
                ULONGLONG sec, nsec;
                struct timeval tv[2];

                if (!info->LastAccessTime.QuadPart || !info->LastWriteTime.QuadPart)
                {

                    tv[0].tv_sec = tv[0].tv_usec = 0;
                    tv[1].tv_sec = tv[1].tv_usec = 0;
                    if (!fstat( fd, &st ))
                    {
                        tv[0].tv_sec = st.st_atime;
                        tv[1].tv_sec = st.st_mtime;
                    }
                }
                if (info->LastAccessTime.QuadPart)
                {
                    sec = RtlLargeIntegerDivide( info->LastAccessTime.QuadPart, 10000000, &nsec );
                    tv[0].tv_sec = sec - SECS_1601_TO_1970;
                    tv[0].tv_usec = (UINT)nsec / 10;
                }
                if (info->LastWriteTime.QuadPart)
                {
                    sec = RtlLargeIntegerDivide( info->LastWriteTime.QuadPart, 10000000, &nsec );
                    tv[1].tv_sec = sec - SECS_1601_TO_1970;
                    tv[1].tv_usec = (UINT)nsec / 10;
                }
                if (futimes( fd, tv ) == -1) io->u.Status = FILE_GetNtStatus();
            }

            if (io->u.Status == STATUS_SUCCESS && info->FileAttributes)
            {
                if (fstat( fd, &st ) == -1) io->u.Status = FILE_GetNtStatus();
                else
                {
                    if (info->FileAttributes & FILE_ATTRIBUTE_READONLY)
                    {
                        if (S_ISDIR( st.st_mode))
                            WARN("FILE_ATTRIBUTE_READONLY ignored for directory.\n");
                        else
                            st.st_mode &= ~0222; /* clear write permission bits */
                    }
                    else
                    {
                        /* add write permission only where we already have read permission */
                        st.st_mode |= (0600 | ((st.st_mode & 044) >> 1)) & (~FILE_umask);
                    }
                    if (fchmod( fd, st.st_mode ) == -1) io->u.Status = FILE_GetNtStatus();
                }
            }
        }
        else io->u.Status = STATUS_INVALID_PARAMETER_3;
        break;

    case FilePositionInformation:
        if (len >= sizeof(FILE_POSITION_INFORMATION))
        {
            const FILE_POSITION_INFORMATION *info = ptr;

            if (lseek( fd, info->CurrentByteOffset.QuadPart, SEEK_SET ) == (off_t)-1)
                io->u.Status = FILE_GetNtStatus();
        }
        else io->u.Status = STATUS_INVALID_PARAMETER_3;
        break;

    case FileEndOfFileInformation:
        if (len >= sizeof(FILE_END_OF_FILE_INFORMATION))
        {
            struct stat st;
            const FILE_END_OF_FILE_INFORMATION *info = ptr;

            /* first try normal truncate */
            if (ftruncate( fd, (off_t)info->EndOfFile.QuadPart ) != -1) break;

            /* now check for the need to extend the file */
            if (fstat( fd, &st ) != -1 && (off_t)info->EndOfFile.QuadPart > st.st_size)
            {
                static const char zero;

                /* extend the file one byte beyond the requested size and then truncate it */
                /* this should work around ftruncate implementations that can't extend files */
                if (pwrite( fd, &zero, 1, (off_t)info->EndOfFile.QuadPart ) != -1 &&
                    ftruncate( fd, (off_t)info->EndOfFile.QuadPart ) != -1) break;
            }
            io->u.Status = FILE_GetNtStatus();
        }
        else io->u.Status = STATUS_INVALID_PARAMETER_3;
        break;

    case FileMailslotSetInformation:
        {
            FILE_MAILSLOT_SET_INFORMATION *info = ptr;

            SERVER_START_REQ( set_mailslot_info )
            {
                req->handle = handle;
                req->flags = MAILSLOT_SET_READ_TIMEOUT;
                req->read_timeout = info->ReadTimeout.QuadPart / -10000;
                io->u.Status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        break;

    default:
        FIXME("Unsupported class (%d)\n", class);
        io->u.Status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    wine_server_release_fd( handle, fd );
    io->Information = 0;
    return io->u.Status;
}


/******************************************************************************
 *              NtQueryFullAttributesFile   (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryFullAttributesFile( const OBJECT_ATTRIBUTES *attr,
                                           FILE_NETWORK_OPEN_INFORMATION *info )
{
    ANSI_STRING unix_name;
    NTSTATUS status;

    if (!(status = wine_nt_to_unix_file_name( attr->ObjectName, &unix_name, FILE_OPEN,
                                              !(attr->Attributes & OBJ_CASE_INSENSITIVE) )))
    {
        struct stat st;

        if (stat( unix_name.Buffer, &st ) == -1)
            status = FILE_GetNtStatus();
        else if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
            status = STATUS_INVALID_INFO_CLASS;
        else
        {
            if (S_ISDIR(st.st_mode))
            {
                info->FileAttributes          = FILE_ATTRIBUTE_DIRECTORY;
                info->AllocationSize.QuadPart = 0;
                info->EndOfFile.QuadPart      = 0;
            }
            else
            {
                info->FileAttributes          = FILE_ATTRIBUTE_ARCHIVE;
                info->AllocationSize.QuadPart = (ULONGLONG)st.st_blocks * 512;
                info->EndOfFile.QuadPart      = st.st_size;
            }
            if (!(st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)))
                info->FileAttributes |= FILE_ATTRIBUTE_READONLY;
            RtlSecondsSince1970ToTime( st.st_mtime, &info->CreationTime );
            RtlSecondsSince1970ToTime( st.st_mtime, &info->LastWriteTime );
            RtlSecondsSince1970ToTime( st.st_ctime, &info->ChangeTime );
            RtlSecondsSince1970ToTime( st.st_atime, &info->LastAccessTime );
            if (DIR_is_hidden_file( attr->ObjectName ))
                info->FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
        }
        RtlFreeAnsiString( &unix_name );
    }
    else WARN("%s not found (%lx)\n", debugstr_us(attr->ObjectName), status );
    return status;
}


/******************************************************************************
 *              NtQueryAttributesFile   (NTDLL.@)
 *              ZwQueryAttributesFile   (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryAttributesFile( const OBJECT_ATTRIBUTES *attr, FILE_BASIC_INFORMATION *info )
{
    FILE_NETWORK_OPEN_INFORMATION full_info;
    NTSTATUS status;

    if (!(status = NtQueryFullAttributesFile( attr, &full_info )))
    {
        info->CreationTime.QuadPart   = full_info.CreationTime.QuadPart;
        info->LastAccessTime.QuadPart = full_info.LastAccessTime.QuadPart;
        info->LastWriteTime.QuadPart  = full_info.LastWriteTime.QuadPart;
        info->ChangeTime.QuadPart     = full_info.ChangeTime.QuadPart;
        info->FileAttributes          = full_info.FileAttributes;
    }
    return status;
}


/******************************************************************************
 *              FILE_GetDeviceInfo
 *
 * Implementation of the FileFsDeviceInformation query for NtQueryVolumeInformationFile.
 */
NTSTATUS FILE_GetDeviceInfo( int fd, FILE_FS_DEVICE_INFORMATION *info )
{
    struct stat st;

    info->Characteristics = 0;
    if (fstat( fd, &st ) < 0) return FILE_GetNtStatus();
    if (S_ISCHR( st.st_mode ))
    {
        info->DeviceType = FILE_DEVICE_UNKNOWN;
#ifdef linux
        switch(major(st.st_rdev))
        {
        case MEM_MAJOR:
            info->DeviceType = FILE_DEVICE_NULL;
            break;
        case TTY_MAJOR:
            info->DeviceType = FILE_DEVICE_SERIAL_PORT;
            break;
        case LP_MAJOR:
            info->DeviceType = FILE_DEVICE_PARALLEL_PORT;
            break;
        }
#endif
    }
    else if (S_ISBLK( st.st_mode ))
    {
        info->DeviceType = FILE_DEVICE_DISK;
    }
    else if (S_ISFIFO( st.st_mode ) || S_ISSOCK( st.st_mode ))
    {
        info->DeviceType = FILE_DEVICE_NAMED_PIPE;
    }
    else  /* regular file or directory */
    {
#if defined(linux) && defined(HAVE_FSTATFS)
        struct statfs stfs;

        /* check for floppy disk */
        if (major(st.st_dev) == FLOPPY_MAJOR)
            info->Characteristics |= FILE_REMOVABLE_MEDIA;

        if (fstatfs( fd, &stfs ) < 0) stfs.f_type = 0;
        switch (stfs.f_type)
        {
        case 0x9660:      /* iso9660 */
        case 0x15013346:  /* udf */
            info->DeviceType = FILE_DEVICE_CD_ROM_FILE_SYSTEM;
            info->Characteristics |= FILE_REMOVABLE_MEDIA|FILE_READ_ONLY_DEVICE;
            break;
        case 0x6969:  /* nfs */
        case 0x517B:  /* smbfs */
        case 0x564c:  /* ncpfs */
            info->DeviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
            info->Characteristics |= FILE_REMOTE_DEVICE;
            break;
        case 0x01021994:  /* tmpfs */
        case 0x28cd3d45:  /* cramfs */
        case 0x1373:      /* devfs */
        case 0x9fa0:      /* procfs */
            info->DeviceType = FILE_DEVICE_VIRTUAL_DISK;
            break;
        default:
            info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
            break;
        }
#elif defined(__FreeBSD__)
        struct statfs stfs;

        /* The proper way to do this in FreeBSD seems to be with the
         * name rather than the type, since their linux-compatible
         * fstatfs call converts the name to one of the Linux types.
         */
        if (fstatfs( fd, &stfs ) < 0)
            info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
        else if (!strncmp("cd9660", stfs.f_fstypename,
                          sizeof(stfs.f_fstypename)))
        {
            info->DeviceType = FILE_DEVICE_CD_ROM_FILE_SYSTEM;
            /* Don't assume read-only, let the mount options set it
             * below
             */
            info->Characteristics |= FILE_REMOVABLE_MEDIA;
        }
        else if (!strncmp("nfs", stfs.f_fstypename,
                          sizeof(stfs.f_fstypename)))
        {
            info->DeviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
            info->Characteristics |= FILE_REMOTE_DEVICE;
        }
        else if (!strncmp("nwfs", stfs.f_fstypename,
                          sizeof(stfs.f_fstypename)))
        {
            info->DeviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
            info->Characteristics |= FILE_REMOTE_DEVICE;
        }
        else if (!strncmp("procfs", stfs.f_fstypename,
                          sizeof(stfs.f_fstypename)))
            info->DeviceType = FILE_DEVICE_VIRTUAL_DISK;
        else
            info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
        if (stfs.f_flags & MNT_RDONLY)
            info->Characteristics |= FILE_READ_ONLY_DEVICE;
        if (!(stfs.f_flags & MNT_LOCAL))
        {
            info->DeviceType = FILE_DEVICE_NETWORK_FILE_SYSTEM;
            info->Characteristics |= FILE_REMOTE_DEVICE;
        }
#elif defined (__APPLE__)
        struct statfs stfs;

        info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;

        if (fstatfs( fd, &stfs ) < 0) return FILE_GetNtStatus();

        /* stfs.f_type is reserved (always set to 0) so use IOKit */
        kern_return_t kernResult = KERN_FAILURE;
        mach_port_t masterPort;

        char bsdName[6]; /* disk#\0 */
        const char *name = stfs.f_mntfromname + strlen(_PATH_DEV);
        memcpy( bsdName, name, min(strlen(name)+1,sizeof(bsdName)) );
        bsdName[sizeof(bsdName)-1] = 0;

        kernResult = IOMasterPort(MACH_PORT_NULL, &masterPort);

        if (kernResult == KERN_SUCCESS)
        {
            CFMutableDictionaryRef matching = IOBSDNameMatching(masterPort, 0, bsdName);

            if (matching)
            {
                CFMutableDictionaryRef properties;
                io_service_t devService = IOServiceGetMatchingService(masterPort, matching);

                if (IORegistryEntryCreateCFProperties(devService,
                                                      &properties,
                                                      kCFAllocatorDefault, 0) != KERN_SUCCESS)
                    return FILE_GetNtStatus();  /* FIXME */
                if ( CFEqual(
                         CFDictionaryGetValue(properties, CFSTR("Removable")),
                         kCFBooleanTrue)
                    ) info->Characteristics |= FILE_REMOVABLE_MEDIA;

                if ( CFEqual(
                         CFDictionaryGetValue(properties, CFSTR("Writable")),
                         kCFBooleanFalse)
                    ) info->Characteristics |= FILE_READ_ONLY_DEVICE;

                /*
                  NB : mounted disk image (.img/.dmg) don't provide specific type
                */
                CFStringRef type;
                if ( (type = CFDictionaryGetValue(properties, CFSTR("Type"))) )
                {
                    if ( CFStringCompare(type, CFSTR("CD-ROM"), 0) == kCFCompareEqualTo
                         || CFStringCompare(type, CFSTR("DVD-ROM"), 0) == kCFCompareEqualTo
                        )
                    {
                        info->DeviceType = FILE_DEVICE_CD_ROM_FILE_SYSTEM;
                    }
                }

                if (properties)
                    CFRelease(properties);
            }
        }
#elif defined(sun)
        /* Use dkio to work out device types */
        {
# include <sys/dkio.h>
# include <sys/vtoc.h>
            struct dk_cinfo dkinf;
            int retval = ioctl(fd, DKIOCINFO, &dkinf);
            if(retval==-1){
                WARN("Unable to get disk device type information - assuming a disk like device\n");
                info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
            }
            switch (dkinf.dki_ctype)
            {
            case DKC_CDROM:
                info->DeviceType = FILE_DEVICE_CD_ROM_FILE_SYSTEM;
                info->Characteristics |= FILE_REMOVABLE_MEDIA|FILE_READ_ONLY_DEVICE;
                break;
            case DKC_NCRFLOPPY:
            case DKC_SMSFLOPPY:
            case DKC_INTEL82072:
            case DKC_INTEL82077:
                info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
                info->Characteristics |= FILE_REMOVABLE_MEDIA;
                break;
            case DKC_MD:
                info->DeviceType = FILE_DEVICE_VIRTUAL_DISK;
                break;
            default:
                info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
            }
        }
#else
        static int warned;
        if (!warned++) FIXME( "device info not properly supported on this platform\n" );
        info->DeviceType = FILE_DEVICE_DISK_FILE_SYSTEM;
#endif
        info->Characteristics |= FILE_DEVICE_IS_MOUNTED;
    }
    return STATUS_SUCCESS;
}


/******************************************************************************
 *  NtQueryVolumeInformationFile		[NTDLL.@]
 *  ZwQueryVolumeInformationFile		[NTDLL.@]
 *
 * Get volume information for an open file handle.
 *
 * PARAMS
 *  handle      [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  io          [O] Receives information about the operation on return
 *  buffer      [O] Destination for volume information
 *  length      [I] Size of FsInformation
 *  info_class  [I] Type of volume information to set
 *
 * RETURNS
 *  Success: 0. io and buffer are updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtQueryVolumeInformationFile( HANDLE handle, PIO_STATUS_BLOCK io,
                                              PVOID buffer, ULONG length,
                                              FS_INFORMATION_CLASS info_class )
{
    int fd;
    struct stat st;

    if ((io->u.Status = wine_server_handle_to_fd( handle, 0, &fd, NULL )) != STATUS_SUCCESS)
        return io->u.Status;

    io->u.Status = STATUS_NOT_IMPLEMENTED;
    io->Information = 0;

    switch( info_class )
    {
    case FileFsVolumeInformation:
        FIXME( "%p: volume info not supported\n", handle );
        break;
    case FileFsLabelInformation:
        FIXME( "%p: label info not supported\n", handle );
        break;
    case FileFsSizeInformation:
        if (length < sizeof(FILE_FS_SIZE_INFORMATION))
            io->u.Status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            FILE_FS_SIZE_INFORMATION *info = buffer;

            if (fstat( fd, &st ) < 0)
            {
                io->u.Status = FILE_GetNtStatus();
                break;
            }
            if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode))
            {
                io->u.Status = STATUS_INVALID_DEVICE_REQUEST;
            }
            else
            {
                /* Linux's fstatvfs is buggy */
#if !defined(linux) || !defined(HAVE_FSTATFS)
                struct statvfs stfs;

                if (fstatvfs( fd, &stfs ) < 0)
                {
                    io->u.Status = FILE_GetNtStatus();
                    break;
                }
                info->BytesPerSector = stfs.f_frsize;
#else
                struct statfs stfs;
                if (fstatfs( fd, &stfs ) < 0)
                {
                    io->u.Status = FILE_GetNtStatus();
                    break;
                }
                info->BytesPerSector = stfs.f_bsize;
#endif
                info->TotalAllocationUnits.QuadPart = stfs.f_blocks;
                info->AvailableAllocationUnits.QuadPart = stfs.f_bavail;
                info->SectorsPerAllocationUnit = 1;
                io->Information = sizeof(*info);
                io->u.Status = STATUS_SUCCESS;
            }
        }
        break;
    case FileFsDeviceInformation:
        if (length < sizeof(FILE_FS_DEVICE_INFORMATION))
            io->u.Status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            FILE_FS_DEVICE_INFORMATION *info = buffer;

            if ((io->u.Status = FILE_GetDeviceInfo( fd, info )) == STATUS_SUCCESS)
                io->Information = sizeof(*info);
        }
        break;
    case FileFsAttributeInformation:
        FIXME( "%p: attribute info not supported\n", handle );
        break;
    case FileFsControlInformation:
        FIXME( "%p: control info not supported\n", handle );
        break;
    case FileFsFullSizeInformation:
        FIXME( "%p: full size info not supported\n", handle );
        break;
    case FileFsObjectIdInformation:
        FIXME( "%p: object id info not supported\n", handle );
        break;
    case FileFsMaximumInformation:
        FIXME( "%p: maximum info not supported\n", handle );
        break;
    default:
        io->u.Status = STATUS_INVALID_PARAMETER;
        break;
    }
    wine_server_release_fd( handle, fd );
    return io->u.Status;
}


/******************************************************************
 *		NtFlushBuffersFile  (NTDLL.@)
 *
 * Flush any buffered data on an open file handle.
 *
 * PARAMS
 *  FileHandle         [I] Handle returned from ZwOpenFile() or ZwCreateFile()
 *  IoStatusBlock      [O] Receives information about the operation on return
 *
 * RETURNS
 *  Success: 0. IoStatusBlock is updated.
 *  Failure: An NTSTATUS error code describing the error.
 */
NTSTATUS WINAPI NtFlushBuffersFile( HANDLE hFile, IO_STATUS_BLOCK* IoStatusBlock )
{
    NTSTATUS ret;
    HANDLE hEvent = NULL;

    SERVER_START_REQ( flush_file )
    {
        req->handle = hFile;
        ret = wine_server_call( req );
        hEvent = reply->event;
    }
    SERVER_END_REQ;
    if (!ret && hEvent)
    {
        ret = NtWaitForSingleObject( hEvent, FALSE, NULL );
        NtClose( hEvent );
    }
    return ret;
}

/******************************************************************
 *		NtLockFile       (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtLockFile( HANDLE hFile, HANDLE lock_granted_event,
                            PIO_APC_ROUTINE apc, void* apc_user,
                            PIO_STATUS_BLOCK io_status, PLARGE_INTEGER offset,
                            PLARGE_INTEGER count, ULONG* key, BOOLEAN dont_wait,
                            BOOLEAN exclusive )
{
    NTSTATUS    ret;
    HANDLE      handle;
    BOOLEAN     async;

    if (apc || io_status || key)
    {
        FIXME("Unimplemented yet parameter\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    for (;;)
    {
        SERVER_START_REQ( lock_file )
        {
            req->handle      = hFile;
            req->offset_low  = offset->u.LowPart;
            req->offset_high = offset->u.HighPart;
            req->count_low   = count->u.LowPart;
            req->count_high  = count->u.HighPart;
            req->shared      = !exclusive;
            req->wait        = !dont_wait;
            ret = wine_server_call( req );
            handle = reply->handle;
            async  = reply->overlapped;
        }
        SERVER_END_REQ;
        if (ret != STATUS_PENDING)
        {
            if (!ret && lock_granted_event) NtSetEvent(lock_granted_event, NULL);
            return ret;
        }

        if (async)
        {
            FIXME( "Async I/O lock wait not implemented, might deadlock\n" );
            if (handle) NtClose( handle );
            return STATUS_PENDING;
        }
        if (handle)
        {
            NtWaitForSingleObject( handle, FALSE, NULL );
            NtClose( handle );
        }
        else
        {
            LARGE_INTEGER time;
    
            /* Unix lock conflict, sleep a bit and retry */
            time.QuadPart = 100 * (ULONGLONG)10000;
            time.QuadPart = -time.QuadPart;
            NtDelayExecution( FALSE, &time );
        }
    }
}


/******************************************************************
 *		NtUnlockFile    (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtUnlockFile( HANDLE hFile, PIO_STATUS_BLOCK io_status,
                              PLARGE_INTEGER offset, PLARGE_INTEGER count,
                              PULONG key )
{
    NTSTATUS status;

    TRACE( "%p %lx%08lx %lx%08lx\n",
           hFile, offset->u.HighPart, offset->u.LowPart, count->u.HighPart, count->u.LowPart );

    if (io_status || key)
    {
        FIXME("Unimplemented yet parameter\n");
        return STATUS_NOT_IMPLEMENTED;
    }

    SERVER_START_REQ( unlock_file )
    {
        req->handle      = hFile;
        req->offset_low  = offset->u.LowPart;
        req->offset_high = offset->u.HighPart;
        req->count_low   = count->u.LowPart;
        req->count_high  = count->u.HighPart;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *		NtCreateNamedPipeFile    (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtCreateNamedPipeFile( PHANDLE handle, ULONG access,
                                       POBJECT_ATTRIBUTES attr, PIO_STATUS_BLOCK iosb,
                                       ULONG sharing, ULONG dispo, ULONG options,
                                       ULONG pipe_type, ULONG read_mode, 
                                       ULONG completion_mode, ULONG max_inst,
                                       ULONG inbound_quota, ULONG outbound_quota,
                                       PLARGE_INTEGER timeout)
{
    NTSTATUS    status;
    static const WCHAR leadin[] = {'\\','?','?','\\','P','I','P','E','\\'};

    TRACE("(%p %lx %s %p %lx %ld %lx %ld %ld %ld %ld %ld %ld %p)\n",
          handle, access, debugstr_w(attr->ObjectName->Buffer), iosb, sharing, dispo,
          options, pipe_type, read_mode, completion_mode, max_inst, inbound_quota,
          outbound_quota, timeout);

    if (attr->ObjectName->Length < sizeof(leadin) ||
        strncmpiW( attr->ObjectName->Buffer, 
                   leadin, sizeof(leadin)/sizeof(leadin[0]) ))
        return STATUS_OBJECT_NAME_INVALID;
    /* assume we only get relative timeout, and storable in a DWORD as ms */
    if (timeout->QuadPart > 0 || (timeout->QuadPart / -10000) >> 32)
        FIXME("Wrong time %s\n", wine_dbgstr_longlong(timeout->QuadPart));

    SERVER_START_REQ( create_named_pipe )
    {
        req->access  = access;
        req->attributes = (attr) ? attr->Attributes : 0;
        req->options = options;
        req->flags = 
            (pipe_type) ? NAMED_PIPE_MESSAGE_STREAM_WRITE : 0 |
            (read_mode) ? NAMED_PIPE_MESSAGE_STREAM_READ  : 0 |
            (completion_mode) ? NAMED_PIPE_NONBLOCKING_MODE  : 0;
        req->maxinstances = max_inst;
        req->outsize = outbound_quota;
        req->insize  = inbound_quota;
        req->timeout = timeout->QuadPart / -10000;
        wine_server_add_data( req, attr->ObjectName->Buffer + 4, 
                              attr->ObjectName->Length - 4 * sizeof(WCHAR) );
        status = wine_server_call( req );
        if (!status) *handle = reply->handle;
    }
    SERVER_END_REQ;
    return status;
}

/******************************************************************
 *		NtDeleteFile    (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtDeleteFile( POBJECT_ATTRIBUTES ObjectAttributes )
{
    NTSTATUS status;
    HANDLE hFile;
    IO_STATUS_BLOCK io;

    TRACE("%p\n", ObjectAttributes);
    status = NtCreateFile( &hFile, GENERIC_READ | GENERIC_WRITE, ObjectAttributes, 
                           &io, NULL, 0,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 
                           FILE_OPEN, FILE_DELETE_ON_CLOSE, NULL, 0 );
    if (status == STATUS_SUCCESS) status = NtClose(hFile);
    return status;
}

/******************************************************************
 *		NtCancelIoFile    (NTDLL.@)
 *
 *
 */
NTSTATUS WINAPI NtCancelIoFile( HANDLE hFile, PIO_STATUS_BLOCK io_status )
{
    LARGE_INTEGER timeout;

    TRACE("%p %p\n", hFile, io_status );

    SERVER_START_REQ( cancel_async )
    {
        req->handle = hFile;
        wine_server_call( req );
    }
    SERVER_END_REQ;
    /* Let some APC be run, so that we can run the remaining APCs on hFile
     * either the cancelation of the pending one, but also the execution
     * of the queued APC, but not yet run. This is needed to ensure proper
     * clean-up of allocated data.
     */
    timeout.u.LowPart = timeout.u.HighPart = 0;
    return io_status->u.Status = NtDelayExecution( TRUE, &timeout );
}

/******************************************************************************
 *  NtCreateMailslotFile	[NTDLL.@]
 *  ZwCreateMailslotFile	[NTDLL.@]
 *
 * PARAMS
 *  pHandle          [O] pointer to receive the handle created
 *  DesiredAccess    [I] access mode (read, write, etc)
 *  ObjectAttributes [I] fully qualified NT path of the mailslot
 *  IoStatusBlock    [O] receives completion status and other info
 *  CreateOptions    [I]
 *  MailslotQuota    [I]
 *  MaxMessageSize   [I]
 *  TimeOut          [I]
 *
 * RETURNS
 *  An NT status code
 */
NTSTATUS WINAPI NtCreateMailslotFile(PHANDLE pHandle, ULONG DesiredAccess,
     POBJECT_ATTRIBUTES attr, PIO_STATUS_BLOCK IoStatusBlock,
     ULONG CreateOptions, ULONG MailslotQuota, ULONG MaxMessageSize,
     PLARGE_INTEGER TimeOut)
{
    static const WCHAR leadin[] = {
        '\\','?','?','\\','M','A','I','L','S','L','O','T','\\'};
    NTSTATUS ret;

    TRACE("%p %08lx %p %p %08lx %08lx %08lx %p\n",
              pHandle, DesiredAccess, attr, IoStatusBlock,
              CreateOptions, MailslotQuota, MaxMessageSize, TimeOut);

    if (attr->ObjectName->Length < sizeof(leadin) ||
        strncmpiW( attr->ObjectName->Buffer, 
                   leadin, sizeof(leadin)/sizeof(leadin[0]) ))
    {
        return STATUS_OBJECT_NAME_INVALID;
    }

    SERVER_START_REQ( create_mailslot )
    {
        req->access = DesiredAccess;
        req->attributes = (attr) ? attr->Attributes : 0;
        req->max_msgsize = MaxMessageSize;
        req->read_timeout = TimeOut->QuadPart / -10000;
        wine_server_add_data( req, attr->ObjectName->Buffer + 4,
                              attr->ObjectName->Length - 4*sizeof(WCHAR) );
        ret = wine_server_call( req );
        if( ret == STATUS_SUCCESS )
            *pHandle = reply->handle;
    }
    SERVER_END_REQ;
 
    return ret;
}
