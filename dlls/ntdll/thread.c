/*
 * NT threads support
 *
 * Copyright 1996, 2003 Alexandre Julliard
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

#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#define NONAMELESSUNION
#include "ntstatus.h"
#include "thread.h"
#include "winternl.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/pthread.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(thread);

/* info passed to a starting thread */
struct startup_info
{
    struct wine_pthread_thread_info pthread_info;
    PRTL_THREAD_START_ROUTINE       entry_point;
    void                           *entry_arg;
};

static PEB peb;
static PEB_LDR_DATA ldr;
static RTL_USER_PROCESS_PARAMETERS params;  /* default parameters if no parent */
static WCHAR current_dir[MAX_NT_PATH_LENGTH];
static RTL_BITMAP tls_bitmap;
static RTL_BITMAP tls_expansion_bitmap;
static LIST_ENTRY tls_links;
static size_t sigstack_total_size;

struct wine_pthread_functions pthread_functions = { NULL };

/***********************************************************************
 *           init_teb
 */
static inline NTSTATUS init_teb( TEB *teb )
{
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)teb->SystemReserved2;

    teb->Tib.ExceptionList = (void *)~0UL;
    teb->Tib.StackBase     = (void *)~0UL;
    teb->Tib.Self          = &teb->Tib;
    teb->Peb               = &peb;
    teb->StaticUnicodeString.Buffer        = teb->StaticUnicodeBuffer;
    teb->StaticUnicodeString.MaximumLength = sizeof(teb->StaticUnicodeBuffer);

    if (!(thread_data->teb_sel = wine_ldt_alloc_fs())) return STATUS_TOO_MANY_THREADS;
    thread_data->request_fd = -1;
    thread_data->reply_fd   = -1;
    thread_data->wait_fd[0] = -1;
    thread_data->wait_fd[1] = -1;

    return STATUS_SUCCESS;
}


/***********************************************************************
 *           free_teb
 */
static inline void free_teb( TEB *teb )
{
    SIZE_T size = 0;
    void *addr = teb;
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)teb->SystemReserved2;

    NtFreeVirtualMemory( NtCurrentProcess(), &addr, &size, MEM_RELEASE );
    wine_ldt_free_fs( thread_data->teb_sel );
    munmap( teb, sigstack_total_size );
}


/***********************************************************************
 *           thread_init
 *
 * Setup the initial thread.
 *
 * NOTES: The first allocated TEB on NT is at 0x7ffde000.
 */
void thread_init(void)
{
    TEB *teb;
    void *addr;
    SIZE_T info_size;
    struct ntdll_thread_data *thread_data;
    struct wine_pthread_thread_info thread_info;
    static struct debug_info debug_info;  /* debug info for initial thread */

    peb.NumberOfProcessors = 1;
    peb.ProcessParameters  = &params;
    peb.TlsBitmap          = &tls_bitmap;
    peb.TlsExpansionBitmap = &tls_expansion_bitmap;
    peb.LdrData            = &ldr;
    params.CurrentDirectory.DosPath.Buffer = current_dir;
    params.CurrentDirectory.DosPath.MaximumLength = sizeof(current_dir);
    RtlInitializeBitMap( &tls_bitmap, peb.TlsBitmapBits, sizeof(peb.TlsBitmapBits) * 8 );
    RtlInitializeBitMap( &tls_expansion_bitmap, peb.TlsExpansionBitmapBits,
                         sizeof(peb.TlsExpansionBitmapBits) * 8 );
    InitializeListHead( &ldr.InLoadOrderModuleList );
    InitializeListHead( &ldr.InMemoryOrderModuleList );
    InitializeListHead( &ldr.InInitializationOrderModuleList );
    InitializeListHead( &tls_links );

    sigstack_total_size = get_signal_stack_total_size();
    thread_info.teb_size = sigstack_total_size;
    VIRTUAL_alloc_teb( &addr, thread_info.teb_size, TRUE );
    teb = addr;
    init_teb( teb );
    thread_data = (struct ntdll_thread_data *)teb->SystemReserved2;
    thread_data->debug_info = &debug_info;
    InsertHeadList( &tls_links, &teb->TlsLinks );

    thread_info.stack_base = NULL;
    thread_info.stack_size = 0;
    thread_info.teb_base   = teb;
    thread_info.teb_sel    = thread_data->teb_sel;
    wine_pthread_get_functions( &pthread_functions, sizeof(pthread_functions) );
    pthread_functions.init_current_teb( &thread_info );
    pthread_functions.init_thread( &thread_info );

    debug_info.str_pos = debug_info.strings;
    debug_info.out_pos = debug_info.output;
    debug_init();

    /* setup the server connection */
    server_init_process();
    info_size = server_init_thread( thread_info.pid, thread_info.tid, NULL );

    /* create the process heap */
    if (!(peb.ProcessHeap = RtlCreateHeap( HEAP_GROWABLE, NULL, 0, 0, NULL, NULL )))
    {
        MESSAGE( "wine: failed to create the process heap\n" );
        exit(1);
    }

    /* allocate user parameters */
    if (info_size)
    {
        RTL_USER_PROCESS_PARAMETERS *params = NULL;

        if (NtAllocateVirtualMemory( NtCurrentProcess(), (void **)&params, 0, &info_size,
                                     MEM_COMMIT, PAGE_READWRITE ) == STATUS_SUCCESS)
        {
            params->AllocationSize = info_size;
            NtCurrentTeb()->Peb->ProcessParameters = params;
        }
    }
    else
    {
        /* This is wine specific: we have no parent (we're started from unix)
         * so, create a simple console with bare handles to unix stdio
         */
        wine_server_fd_to_handle( 0, GENERIC_READ|SYNCHRONIZE,  TRUE, &params.hStdInput );
        wine_server_fd_to_handle( 1, GENERIC_WRITE|SYNCHRONIZE, TRUE, &params.hStdOutput );
        wine_server_fd_to_handle( 2, GENERIC_WRITE|SYNCHRONIZE, TRUE, &params.hStdError );
    }
}


/***********************************************************************
 *           start_thread
 *
 * Startup routine for a newly created thread.
 */
static void start_thread( struct wine_pthread_thread_info *info )
{
    TEB *teb = info->teb_base;
    struct ntdll_thread_data *thread_data = (struct ntdll_thread_data *)teb->SystemReserved2;
    struct startup_info *startup_info = (struct startup_info *)info;
    PRTL_THREAD_START_ROUTINE func = startup_info->entry_point;
    void *arg = startup_info->entry_arg;
    struct debug_info debug_info;
    SIZE_T size;

    debug_info.str_pos = debug_info.strings;
    debug_info.out_pos = debug_info.output;
    thread_data->debug_info = &debug_info;

    pthread_functions.init_current_teb( info );
    SIGNAL_Init();
    server_init_thread( info->pid, info->tid, func );
    pthread_functions.init_thread( info );

    /* allocate a memory view for the stack */
    size = info->stack_size;
    teb->DeallocationStack = info->stack_base;
    NtAllocateVirtualMemory( NtCurrentProcess(), &teb->DeallocationStack, 0,
                             &size, MEM_SYSTEM, PAGE_READWRITE );
    /* limit is lower than base since the stack grows down */
    teb->Tib.StackBase  = (char *)info->stack_base + info->stack_size;
    teb->Tib.StackLimit = info->stack_base;

    /* setup the guard page */
    size = 1;
    NtProtectVirtualMemory( NtCurrentProcess(), &teb->DeallocationStack, &size,
                            PAGE_READWRITE | PAGE_GUARD, NULL );
    RtlFreeHeap( GetProcessHeap(), 0, info );

    RtlAcquirePebLock();
    InsertHeadList( &tls_links, &teb->TlsLinks );
    RtlReleasePebLock();

    func( arg );
}


/***********************************************************************
 *              RtlCreateUserThread   (NTDLL.@)
 */
NTSTATUS WINAPI RtlCreateUserThread( HANDLE process, const SECURITY_DESCRIPTOR *descr,
                                     BOOLEAN suspended, PVOID stack_addr,
                                     SIZE_T stack_reserve, SIZE_T stack_commit,
                                     PRTL_THREAD_START_ROUTINE start, void *param,
                                     HANDLE *handle_ptr, CLIENT_ID *id )
{
    struct ntdll_thread_data *thread_data = NULL;
    struct startup_info *info = NULL;
    void *addr;
    HANDLE handle = 0;
    TEB *teb;
    DWORD tid = 0;
    int request_pipe[2];
    NTSTATUS status;

    if( ! is_current_process( process ) )
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    if (pipe( request_pipe ) == -1) return STATUS_TOO_MANY_OPENED_FILES;
    fcntl( request_pipe[1], F_SETFD, 1 ); /* set close on exec flag */
    wine_server_send_fd( request_pipe[0] );

    SERVER_START_REQ( new_thread )
    {
        req->suspend    = suspended;
        req->inherit    = 0;  /* FIXME */
        req->request_fd = request_pipe[0];
        if (!(status = wine_server_call( req )))
        {
            handle = reply->handle;
            tid = reply->tid;
        }
        close( request_pipe[0] );
    }
    SERVER_END_REQ;

    if (status) goto error;

    if (!(info = RtlAllocateHeap( GetProcessHeap(), 0, sizeof(*info) )))
    {
        status = STATUS_NO_MEMORY;
        goto error;
    }

    info->pthread_info.teb_size = sigstack_total_size;
    if ((status = VIRTUAL_alloc_teb( &addr, info->pthread_info.teb_size, FALSE ))) goto error;
    teb = addr;
    if ((status = init_teb( teb ))) goto error;

    teb->ClientId.UniqueProcess = (HANDLE)GetCurrentProcessId();
    teb->ClientId.UniqueThread  = (HANDLE)tid;

    thread_data = (struct ntdll_thread_data *)teb->SystemReserved2;
    thread_data->request_fd  = request_pipe[1];

    info->pthread_info.teb_base = teb;
    info->pthread_info.teb_sel  = thread_data->teb_sel;

    if (!stack_reserve || !stack_commit)
    {
        IMAGE_NT_HEADERS *nt = RtlImageNtHeader( NtCurrentTeb()->Peb->ImageBaseAddress );
        if (!stack_reserve) stack_reserve = nt->OptionalHeader.SizeOfStackReserve;
        if (!stack_commit) stack_commit = nt->OptionalHeader.SizeOfStackCommit;
    }
    if (stack_reserve < stack_commit) stack_reserve = stack_commit;
    stack_reserve = (stack_reserve + 0xffff) & ~0xffff;  /* round to 64K boundary */
    if (stack_reserve < 1024 * 1024) stack_reserve = 1024 * 1024;  /* Xlib needs a large stack */

    info->pthread_info.stack_base = NULL;
    info->pthread_info.stack_size = stack_reserve;
    info->pthread_info.entry      = start_thread;
    info->entry_point             = start;
    info->entry_arg               = param;

    if (pthread_functions.create_thread( &info->pthread_info ) == -1)
    {
        status = STATUS_NO_MEMORY;
        goto error;
    }

    if (id) id->UniqueThread = (HANDLE)tid;
    if (handle_ptr) *handle_ptr = handle;
    else NtClose( handle );

    return STATUS_SUCCESS;

error:
    if (thread_data) wine_ldt_free_fs( thread_data->teb_sel );
    if (addr)
    {
        SIZE_T size = 0;
        NtFreeVirtualMemory( NtCurrentProcess(), &addr, &size, MEM_RELEASE );
    }
    if (info) RtlFreeHeap( GetProcessHeap(), 0, info );
    if (handle) NtClose( handle );
    close( request_pipe[1] );
    return status;
}


/***********************************************************************
 *           RtlExitUserThread  (NTDLL.@)
 */
void WINAPI RtlExitUserThread( ULONG status )
{
    LdrShutdownThread();
    server_exit_thread( status );
}


/***********************************************************************
 *              NtOpenThread   (NTDLL.@)
 *              ZwOpenThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenThread( HANDLE *handle, ACCESS_MASK access,
                              const OBJECT_ATTRIBUTES *attr, const CLIENT_ID *id )
{
    NTSTATUS ret;

    SERVER_START_REQ( open_thread )
    {
        req->tid     = (thread_id_t)id->UniqueThread;
        req->access  = access;
        req->inherit = attr && (attr->Attributes & OBJ_INHERIT);
        ret = wine_server_call( req );
        *handle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtSuspendThread   (NTDLL.@)
 *              ZwSuspendThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtSuspendThread( HANDLE handle, PULONG count )
{
    NTSTATUS ret;

    SERVER_START_REQ( suspend_thread )
    {
        req->handle = handle;
        if (!(ret = wine_server_call( req ))) *count = reply->count;
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtResumeThread   (NTDLL.@)
 *              ZwResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtResumeThread( HANDLE handle, PULONG count )
{
    NTSTATUS ret;

    SERVER_START_REQ( resume_thread )
    {
        req->handle = handle;
        if (!(ret = wine_server_call( req ))) *count = reply->count;
    }
    SERVER_END_REQ;
    return ret;
}


/******************************************************************************
 *              NtAlertResumeThread   (NTDLL.@)
 *              ZwAlertResumeThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertResumeThread( HANDLE handle, PULONG count )
{
    FIXME( "stub: should alert thread %p\n", handle );
    return NtResumeThread( handle, count );
}


/******************************************************************************
 *              NtAlertThread   (NTDLL.@)
 *              ZwAlertThread   (NTDLL.@)
 */
NTSTATUS WINAPI NtAlertThread( HANDLE handle )
{
    FIXME( "stub: %p\n", handle );
    return STATUS_NOT_IMPLEMENTED;
}


/******************************************************************************
 *              NtTerminateThread  (NTDLL.@)
 *              ZwTerminateThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtTerminateThread( HANDLE handle, LONG exit_code )
{
    NTSTATUS ret;
    BOOL self, last;

    SERVER_START_REQ( terminate_thread )
    {
        req->handle    = handle;
        req->exit_code = exit_code;
        ret = wine_server_call( req );
        self = !ret && reply->self;
        last = reply->last;
    }
    SERVER_END_REQ;

    if (self)
    {
        if (last) exit( exit_code );
        else server_abort_thread( exit_code );
    }
    return ret;
}


/******************************************************************************
 *              NtQueueApcThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueueApcThread( HANDLE handle, PNTAPCFUNC func, ULONG_PTR arg1,
                                  ULONG_PTR arg2, ULONG_PTR arg3 )
{
    NTSTATUS ret;
    SERVER_START_REQ( queue_apc )
    {
        req->handle = handle;
        req->user   = 1;
        req->func   = func;
        req->arg1   = (void *)arg1;
        req->arg2   = (void *)arg2;
        req->arg3   = (void *)arg3;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *              NtSetContextThread  (NTDLL.@)
 *              ZwSetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetContextThread( HANDLE handle, const CONTEXT *context )
{
    NTSTATUS ret;
    DWORD dummy, i;

    SERVER_START_REQ( set_thread_context )
    {
        req->handle  = handle;
        req->flags   = context->ContextFlags;
        req->suspend = 0;
        wine_server_add_data( req, context, sizeof(*context) );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    if (ret == STATUS_PENDING)
    {
        if (NtSuspendThread( handle, &dummy ) == STATUS_SUCCESS)
        {
            for (i = 0; i < 100; i++)
            {
                SERVER_START_REQ( set_thread_context )
                {
                    req->handle  = handle;
                    req->flags   = context->ContextFlags;
                    req->suspend = 0;
                    wine_server_add_data( req, context, sizeof(*context) );
                    ret = wine_server_call( req );
                }
                SERVER_END_REQ;
                if (ret != STATUS_PENDING) break;
                NtYieldExecution();
            }
            NtResumeThread( handle, &dummy );
        }
    }

    if (ret == STATUS_PENDING) ret = STATUS_ACCESS_DENIED;
    return ret;
}


/* copy a context structure according to the flags */
static inline void copy_context( CONTEXT *to, const CONTEXT *from, DWORD flags )
{
#ifdef __i386__
    flags &= ~CONTEXT_i386;  /* get rid of CPU id */
    if (flags & CONTEXT_INTEGER)
    {
        to->Eax = from->Eax;
        to->Ebx = from->Ebx;
        to->Ecx = from->Ecx;
        to->Edx = from->Edx;
        to->Esi = from->Esi;
        to->Edi = from->Edi;
    }
    if (flags & CONTEXT_CONTROL)
    {
        to->Ebp    = from->Ebp;
        to->Esp    = from->Esp;
        to->Eip    = from->Eip;
        to->SegCs  = from->SegCs;
        to->SegSs  = from->SegSs;
        to->EFlags = from->EFlags;
    }
    if (flags & CONTEXT_SEGMENTS)
    {
        to->SegDs = from->SegDs;
        to->SegEs = from->SegEs;
        to->SegFs = from->SegFs;
        to->SegGs = from->SegGs;
    }
    if (flags & CONTEXT_DEBUG_REGISTERS)
    {
        to->Dr0 = from->Dr0;
        to->Dr1 = from->Dr1;
        to->Dr2 = from->Dr2;
        to->Dr3 = from->Dr3;
        to->Dr6 = from->Dr6;
        to->Dr7 = from->Dr7;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->FloatSave = from->FloatSave;
    }
#elif defined(__x86_64__)
    flags &= ~CONTEXT_AMD64;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL)
    {
        to->Rbp    = from->Rbp;
        to->Rip    = from->Rip;
        to->Rsp    = from->Rsp;
        to->SegCs  = from->SegCs;
        to->SegSs  = from->SegSs;
        to->EFlags = from->EFlags;
        to->MxCsr  = from->MxCsr;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->Rax = from->Rax;
        to->Rcx = from->Rcx;
        to->Rdx = from->Rdx;
        to->Rbx = from->Rbx;
        to->Rsi = from->Rsi;
        to->Rdi = from->Rdi;
        to->R8  = from->R8;
        to->R9  = from->R9;
        to->R10 = from->R10;
        to->R11 = from->R11;
        to->R12 = from->R12;
        to->R13 = from->R13;
        to->R14 = from->R14;
        to->R15 = from->R15;
    }
    if (flags & CONTEXT_SEGMENTS)
    {
        to->SegDs = from->SegDs;
        to->SegEs = from->SegEs;
        to->SegFs = from->SegFs;
        to->SegGs = from->SegGs;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->u.FltSave = from->u.FltSave;
    }
    if (flags & CONTEXT_DEBUG_REGISTERS)
    {
        to->Dr0 = from->Dr0;
        to->Dr1 = from->Dr1;
        to->Dr2 = from->Dr2;
        to->Dr3 = from->Dr3;
        to->Dr6 = from->Dr6;
        to->Dr7 = from->Dr7;
    }
#elif defined(__sparc__)
    flags &= ~CONTEXT_SPARC;  /* get rid of CPU id */
    if (flags & CONTEXT_CONTROL)
    {
        to->psr = from->psr;
        to->pc  = from->pc;
        to->npc = from->npc;
        to->y   = from->y;
        to->wim = from->wim;
        to->tbr = from->tbr;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->g0 = from->g0;
        to->g1 = from->g1;
        to->g2 = from->g2;
        to->g3 = from->g3;
        to->g4 = from->g4;
        to->g5 = from->g5;
        to->g6 = from->g6;
        to->g7 = from->g7;
        to->o0 = from->o0;
        to->o1 = from->o1;
        to->o2 = from->o2;
        to->o3 = from->o3;
        to->o4 = from->o4;
        to->o5 = from->o5;
        to->o6 = from->o6;
        to->o7 = from->o7;
        to->l0 = from->l0;
        to->l1 = from->l1;
        to->l2 = from->l2;
        to->l3 = from->l3;
        to->l4 = from->l4;
        to->l5 = from->l5;
        to->l6 = from->l6;
        to->l7 = from->l7;
        to->i0 = from->i0;
        to->i1 = from->i1;
        to->i2 = from->i2;
        to->i3 = from->i3;
        to->i4 = from->i4;
        to->i5 = from->i5;
        to->i6 = from->i6;
        to->i7 = from->i7;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        /* FIXME */
    }
#elif defined(__powerpc__)
    /* Has no CPU id */
    if (flags & CONTEXT_CONTROL)
    {
        to->Msr = from->Msr;
        to->Ctr = from->Ctr;
        to->Iar = from->Iar;
    }
    if (flags & CONTEXT_INTEGER)
    {
        to->Gpr0  = from->Gpr0;
        to->Gpr1  = from->Gpr1;
        to->Gpr2  = from->Gpr2;
        to->Gpr3  = from->Gpr3;
        to->Gpr4  = from->Gpr4;
        to->Gpr5  = from->Gpr5;
        to->Gpr6  = from->Gpr6;
        to->Gpr7  = from->Gpr7;
        to->Gpr8  = from->Gpr8;
        to->Gpr9  = from->Gpr9;
        to->Gpr10 = from->Gpr10;
        to->Gpr11 = from->Gpr11;
        to->Gpr12 = from->Gpr12;
        to->Gpr13 = from->Gpr13;
        to->Gpr14 = from->Gpr14;
        to->Gpr15 = from->Gpr15;
        to->Gpr16 = from->Gpr16;
        to->Gpr17 = from->Gpr17;
        to->Gpr18 = from->Gpr18;
        to->Gpr19 = from->Gpr19;
        to->Gpr20 = from->Gpr20;
        to->Gpr21 = from->Gpr21;
        to->Gpr22 = from->Gpr22;
        to->Gpr23 = from->Gpr23;
        to->Gpr24 = from->Gpr24;
        to->Gpr25 = from->Gpr25;
        to->Gpr26 = from->Gpr26;
        to->Gpr27 = from->Gpr27;
        to->Gpr28 = from->Gpr28;
        to->Gpr29 = from->Gpr29;
        to->Gpr30 = from->Gpr30;
        to->Gpr31 = from->Gpr31;
        to->Xer   = from->Xer;
        to->Cr    = from->Cr;
    }
    if (flags & CONTEXT_FLOATING_POINT)
    {
        to->Fpr0  = from->Fpr0;
        to->Fpr1  = from->Fpr1;
        to->Fpr2  = from->Fpr2;
        to->Fpr3  = from->Fpr3;
        to->Fpr4  = from->Fpr4;
        to->Fpr5  = from->Fpr5;
        to->Fpr6  = from->Fpr6;
        to->Fpr7  = from->Fpr7;
        to->Fpr8  = from->Fpr8;
        to->Fpr9  = from->Fpr9;
        to->Fpr10 = from->Fpr10;
        to->Fpr11 = from->Fpr11;
        to->Fpr12 = from->Fpr12;
        to->Fpr13 = from->Fpr13;
        to->Fpr14 = from->Fpr14;
        to->Fpr15 = from->Fpr15;
        to->Fpr16 = from->Fpr16;
        to->Fpr17 = from->Fpr17;
        to->Fpr18 = from->Fpr18;
        to->Fpr19 = from->Fpr19;
        to->Fpr20 = from->Fpr20;
        to->Fpr21 = from->Fpr21;
        to->Fpr22 = from->Fpr22;
        to->Fpr23 = from->Fpr23;
        to->Fpr24 = from->Fpr24;
        to->Fpr25 = from->Fpr25;
        to->Fpr26 = from->Fpr26;
        to->Fpr27 = from->Fpr27;
        to->Fpr28 = from->Fpr28;
        to->Fpr29 = from->Fpr29;
        to->Fpr30 = from->Fpr30;
        to->Fpr31 = from->Fpr31;
        to->Fpscr = from->Fpscr;
    }
#else
#error You must implement context copying for your CPU
#endif
}


/***********************************************************************
 *              NtGetContextThread  (NTDLL.@)
 *              ZwGetContextThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtGetContextThread( HANDLE handle, CONTEXT *context )
{
    NTSTATUS ret;
    CONTEXT ctx;
    DWORD dummy, i;

    SERVER_START_REQ( get_thread_context )
    {
        req->handle  = handle;
        req->flags   = context->ContextFlags;
        req->suspend = 0;
        wine_server_set_reply( req, &ctx, sizeof(ctx) );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;

    if (ret == STATUS_PENDING)
    {
        if (NtSuspendThread( handle, &dummy ) == STATUS_SUCCESS)
        {
            for (i = 0; i < 100; i++)
            {
                SERVER_START_REQ( get_thread_context )
                {
                    req->handle  = handle;
                    req->flags   = context->ContextFlags;
                    req->suspend = 0;
                    wine_server_set_reply( req, &ctx, sizeof(ctx) );
                    ret = wine_server_call( req );
                }
                SERVER_END_REQ;
                if (ret != STATUS_PENDING) break;
                NtYieldExecution();
            }
            NtResumeThread( handle, &dummy );
        }
    }

    if (ret == STATUS_SUCCESS) copy_context( context, &ctx, context->ContextFlags );
    else if (ret == STATUS_PENDING) ret = STATUS_ACCESS_DENIED;
    return ret;
}


/******************************************************************************
 *              NtQueryInformationThread  (NTDLL.@)
 *              ZwQueryInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryInformationThread( HANDLE handle, THREADINFOCLASS class,
                                          void *data, ULONG length, ULONG *ret_len )
{
    NTSTATUS status;

    switch(class)
    {
    case ThreadBasicInformation:
        {
            THREAD_BASIC_INFORMATION info;

            SERVER_START_REQ( get_thread_info )
            {
                req->handle = handle;
                req->tid_in = 0;
                if (!(status = wine_server_call( req )))
                {
                    info.ExitStatus             = reply->exit_code;
                    info.TebBaseAddress         = reply->teb;
                    info.ClientId.UniqueProcess = (HANDLE)reply->pid;
                    info.ClientId.UniqueThread  = (HANDLE)reply->tid;
                    info.AffinityMask           = reply->affinity;
                    info.Priority               = reply->priority;
                    info.BasePriority           = reply->priority;  /* FIXME */
                }
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                if (data) memcpy( data, &info, min( length, sizeof(info) ));
                if (ret_len) *ret_len = min( length, sizeof(info) );
            }
        }
        return status;
    case ThreadTimes:
        {
            KERNEL_USER_TIMES   kusrt;
            /* We need to do a server call to get the creation time or exit time */
            /* This works on any thread */
            SERVER_START_REQ( get_thread_info )
            {
                req->handle = handle;
                req->tid_in = 0;
                status = wine_server_call( req );
                if (status == STATUS_SUCCESS)
                {
                    RtlSecondsSince1970ToTime( reply->creation_time, &kusrt.CreateTime );
                    RtlSecondsSince1970ToTime( reply->exit_time, &kusrt.ExitTime );
                }
            }
            SERVER_END_REQ;
            if (status == STATUS_SUCCESS)
            {
                /* We call times(2) for kernel time or user time */
                /* We can only (portably) do this for the current thread */
                if (handle == GetCurrentThread())
                {
                    struct tms time_buf;
                    long clocks_per_sec = sysconf(_SC_CLK_TCK);

                    times(&time_buf);
                    kusrt.KernelTime.QuadPart = (ULONGLONG)time_buf.tms_stime * 10000000 / clocks_per_sec;
                    kusrt.UserTime.QuadPart = (ULONGLONG)time_buf.tms_utime * 10000000 / clocks_per_sec;
                }
                else
                {
                    kusrt.KernelTime.QuadPart = 0;
                    kusrt.UserTime.QuadPart = 0;
                    FIXME("Cannot get kerneltime or usertime of other threads\n");
                }
                if (data) memcpy( data, &kusrt, min( length, sizeof(kusrt) ));
                if (ret_len) *ret_len = min( length, sizeof(kusrt) );
            }
        }
        return status;
    case ThreadPriority:
    case ThreadBasePriority:
    case ThreadAffinityMask:
    case ThreadImpersonationToken:
    case ThreadDescriptorTableEntry:
    case ThreadEnableAlignmentFaultFixup:
    case ThreadEventPair_Reusable:
    case ThreadQuerySetWin32StartAddress:
    case ThreadZeroTlsCell:
    case ThreadPerformanceCount:
    case ThreadAmILastThread:
    case ThreadIdealProcessor:
    case ThreadPriorityBoost:
    case ThreadSetTlsArrayAddress:
    case ThreadIsIoPending:
    default:
        FIXME( "info class %d not supported yet\n", class );
        return STATUS_NOT_IMPLEMENTED;
    }
}


/******************************************************************************
 *              NtSetInformationThread  (NTDLL.@)
 *              ZwSetInformationThread  (NTDLL.@)
 */
NTSTATUS WINAPI NtSetInformationThread( HANDLE handle, THREADINFOCLASS class,
                                        LPCVOID data, ULONG length )
{
    NTSTATUS status;
    switch(class)
    {
    case ThreadZeroTlsCell:
        if (handle == GetCurrentThread())
        {
            LIST_ENTRY *entry;
            DWORD index;

            if (length != sizeof(DWORD)) return STATUS_INVALID_PARAMETER;
            index = *(const DWORD *)data;
            if (index < TLS_MINIMUM_AVAILABLE)
            {
                RtlAcquirePebLock();
                for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
                {
                    TEB *teb = CONTAINING_RECORD(entry, TEB, TlsLinks);
                    teb->TlsSlots[index] = 0;
                }
                RtlReleasePebLock();
            }
            else
            {
                index -= TLS_MINIMUM_AVAILABLE;
                if (index >= 8 * sizeof(NtCurrentTeb()->Peb->TlsExpansionBitmapBits))
                    return STATUS_INVALID_PARAMETER;
                RtlAcquirePebLock();
                for (entry = tls_links.Flink; entry != &tls_links; entry = entry->Flink)
                {
                    TEB *teb = CONTAINING_RECORD(entry, TEB, TlsLinks);
                    if (teb->TlsExpansionSlots) teb->TlsExpansionSlots[index] = 0;
                }
                RtlReleasePebLock();
            }
            return STATUS_SUCCESS;
        }
        FIXME( "ZeroTlsCell not supported on other threads\n" );
        return STATUS_NOT_IMPLEMENTED;

    case ThreadImpersonationToken:
        {
            const HANDLE *phToken = data;
            if (length != sizeof(HANDLE)) return STATUS_INVALID_PARAMETER;
            TRACE("Setting ThreadImpersonationToken handle to %p\n", *phToken );
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = handle;
                req->token    = *phToken;
                req->mask     = SET_THREAD_INFO_TOKEN;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadBasePriority:
        {
            const DWORD *pprio = data;
            if (length != sizeof(DWORD)) return STATUS_INVALID_PARAMETER;
            SERVER_START_REQ( set_thread_info )
            {
                req->handle   = handle;
                req->priority = *pprio;
                req->mask     = SET_THREAD_INFO_PRIORITY;
                status = wine_server_call( req );
            }
            SERVER_END_REQ;
        }
        return status;
    case ThreadBasicInformation:
    case ThreadTimes:
    case ThreadPriority:
    case ThreadAffinityMask:
    case ThreadDescriptorTableEntry:
    case ThreadEnableAlignmentFaultFixup:
    case ThreadEventPair_Reusable:
    case ThreadQuerySetWin32StartAddress:
    case ThreadPerformanceCount:
    case ThreadAmILastThread:
    case ThreadIdealProcessor:
    case ThreadPriorityBoost:
    case ThreadSetTlsArrayAddress:
    case ThreadIsIoPending:
    default:
        FIXME( "info class %d not supported yet\n", class );
        return STATUS_NOT_IMPLEMENTED;
    }
}


/**********************************************************************
 *           NtCurrentTeb   (NTDLL.@)
 */
#if defined(__i386__) && defined(__GNUC__)

__ASM_GLOBAL_FUNC( NtCurrentTeb, ".byte 0x64\n\tmovl 0x18,%eax\n\tret" );

#elif defined(__i386__) && defined(_MSC_VER)

/* Nothing needs to be done. MS C "magically" exports the inline version from winnt.h */

#else

/**********************************************************************/

TEB * WINAPI NtCurrentTeb(void)
{
    return pthread_functions.get_current_teb();
}

#endif  /* __i386__ */
