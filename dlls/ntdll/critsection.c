/*
 * Win32 critical sections
 *
 * Copyright 1998 Alexandre Julliard
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
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <time.h>
#include "windef.h"
#include "winternl.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);
WINE_DECLARE_DEBUG_CHANNEL(relay);

inline static LONG interlocked_inc( PLONG dest )
{
    return interlocked_xchg_add( (int *)dest, 1 ) + 1;
}

inline static LONG interlocked_dec( PLONG dest )
{
    return interlocked_xchg_add( (int *)dest, -1 ) - 1;
}

inline static void small_pause(void)
{
#ifdef __i386__
    __asm__ __volatile__( "rep;nop" : : : "memory" );
#else
    __asm__ __volatile__( "" : : : "memory" );
#endif
}

#if defined(linux) && defined(__i386__)

static inline int futex_wait( int *addr, int val, struct timespec *timeout )
{
    int res;
    __asm__ __volatile__( "xchgl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "xchgl %2,%%ebx"
                          : "=a" (res)
                          : "0" (240) /* SYS_futex */, "D" (addr),
                            "c" (0) /* FUTEX_WAIT */, "d" (val), "S" (timeout) );
    return res;
}

static inline int futex_wake( int *addr, int val )
{
    int res;
    __asm__ __volatile__( "xchgl %2,%%ebx\n\t"
                          "int $0x80\n\t"
                          "xchgl %2,%%ebx"
                          : "=a" (res)
                          : "0" (240) /* SYS_futex */, "D" (addr),
                            "c" (1)  /* FUTEX_WAKE */, "d" (val) );
    return res;
}

static inline int use_futexes(void)
{
    static int supported = -1;

    if (supported == -1) supported = (futex_wait( &supported, 10, NULL ) != -ENOSYS);
    return supported;
}

#else  /* linux */

static inline int futex_wait( int *addr, int val, struct timespec *timeout ) { return -ENOSYS; }
static inline int futex_wake( int *addr, int val ) { return -ENOSYS; }
static inline int use_futexes(void) { return 0; }

#endif

/***********************************************************************
 *           get_semaphore
 */
static inline HANDLE get_semaphore( RTL_CRITICAL_SECTION *crit )
{
    HANDLE ret = crit->LockSemaphore;
    if (!ret)
    {
        HANDLE sem;
        if (NtCreateSemaphore( &sem, SEMAPHORE_ALL_ACCESS, NULL, 0, 1 )) return 0;
        if (!(ret = (HANDLE)interlocked_cmpxchg_ptr( (PVOID *)&crit->LockSemaphore,
                                                     (PVOID)sem, 0 )))
            ret = sem;
        else
            NtClose(sem);  /* somebody beat us to it */
    }
    return ret;
}

/***********************************************************************
 *           wait_semaphore
 */
static inline NTSTATUS wait_semaphore( RTL_CRITICAL_SECTION *crit, int timeout )
{
    if (use_futexes() && crit->DebugInfo)  /* debug info is cleared by MakeCriticalSectionGlobal */
    {
        int val;
        struct timespec timespec;

        timespec.tv_sec  = timeout;
        timespec.tv_nsec = 0;
        while ((val = interlocked_cmpxchg( (int *)&crit->LockSemaphore, 0, 1 )) != 1)
        {
            /* note: this may wait longer than specified in case of signals or */
            /*       multiple wake-ups, but that shouldn't be a problem */
            if (futex_wait( (int *)&crit->LockSemaphore, val, &timespec ) == -ETIMEDOUT)
                return STATUS_TIMEOUT;
        }
        return STATUS_WAIT_0;
    }
    else
    {
        HANDLE sem = get_semaphore( crit );
        LARGE_INTEGER time;

        time.QuadPart = timeout * (LONGLONG)-10000000;
        return NtWaitForSingleObject( sem, FALSE, &time );
    }
}

/***********************************************************************
 *           RtlInitializeCriticalSection   (NTDLL.@)
 *
 * Initialises a new critical section.
 *
 * PARAMS
 *  crit [O] Critical section to initialise
 *
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * SEE
 *  RtlInitializeCriticalSectionAndSpinCount(), RtlDeleteCriticalSection(),
 *  RtlEnterCriticalSection(), RtlLeaveCriticalSection(),
 *  RtlTryEnterCriticalSection(), RtlSetCriticalSectionSpinCount()
 */
NTSTATUS WINAPI RtlInitializeCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    return RtlInitializeCriticalSectionAndSpinCount( crit, 0 );
}

/***********************************************************************
 *           RtlInitializeCriticalSectionAndSpinCount   (NTDLL.@)
 *
 * Initialises a new critical section with a given spin count.
 *
 * PARAMS
 *   crit      [O] Critical section to initialise
 *   spincount [I] Spin count for crit
 * 
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * NOTES
 *  Available on NT4 SP3 or later.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlDeleteCriticalSection(),
 *  RtlEnterCriticalSection(), RtlLeaveCriticalSection(),
 *  RtlTryEnterCriticalSection(), RtlSetCriticalSectionSpinCount()
 */
NTSTATUS WINAPI RtlInitializeCriticalSectionAndSpinCount( RTL_CRITICAL_SECTION *crit, ULONG spincount )
{
    crit->DebugInfo = RtlAllocateHeap(GetProcessHeap(), 0, sizeof(RTL_CRITICAL_SECTION_DEBUG));
    if (crit->DebugInfo)
    {
        crit->DebugInfo->Type = 0;
        crit->DebugInfo->CreatorBackTraceIndex = 0;
        crit->DebugInfo->CriticalSection = crit;
        crit->DebugInfo->ProcessLocksList.Blink = &(crit->DebugInfo->ProcessLocksList);
        crit->DebugInfo->ProcessLocksList.Flink = &(crit->DebugInfo->ProcessLocksList);
        crit->DebugInfo->EntryCount = 0;
        crit->DebugInfo->ContentionCount = 0;
        memset( crit->DebugInfo->Spare, 0, sizeof(crit->DebugInfo->Spare) );
    }
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    crit->LockSemaphore  = 0;
    if (NtCurrentTeb()->Peb->NumberOfProcessors <= 1) spincount = 0;
    crit->SpinCount = spincount & ~0x80000000;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *           RtlSetCriticalSectionSpinCount   (NTDLL.@)
 *
 * Sets the spin count of a critical section.
 *
 * PARAMS
 *   crit      [I/O] Critical section
 *   spincount [I] Spin count for crit
 *
 * RETURNS
 *  The previous spin count.
 *
 * NOTES
 *  If the system is not SMP, spincount is ignored and set to 0.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlLeaveCriticalSection(), RtlTryEnterCriticalSection()
 */
ULONG WINAPI RtlSetCriticalSectionSpinCount( RTL_CRITICAL_SECTION *crit, ULONG spincount )
{
    ULONG oldspincount = crit->SpinCount;
    if (NtCurrentTeb()->Peb->NumberOfProcessors <= 1) spincount = 0;
    crit->SpinCount = spincount;
    return oldspincount;
}

/***********************************************************************
 *           RtlDeleteCriticalSection   (NTDLL.@)
 *
 * Frees the resources used by a critical section.
 *
 * PARAMS
 *  crit [I/O] Critical section to free
 *
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlLeaveCriticalSection(), RtlTryEnterCriticalSection()
 */
NTSTATUS WINAPI RtlDeleteCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    crit->LockCount      = -1;
    crit->RecursionCount = 0;
    crit->OwningThread   = 0;
    if (crit->LockSemaphore) NtClose( crit->LockSemaphore );
    crit->LockSemaphore  = 0;
    if (crit->DebugInfo)
    {
        /* only free the ones we made in here */
        if (!crit->DebugInfo->Spare[0])
        {
            RtlFreeHeap( GetProcessHeap(), 0, crit->DebugInfo );
            crit->DebugInfo = NULL;
        }
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlpWaitForCriticalSection   (NTDLL.@)
 *
 * Waits for a busy critical section to become free.
 * 
 * PARAMS
 *  crit [I/O] Critical section to wait for
 *
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * NOTES
 *  Use RtlEnterCriticalSection() instead of this function as it is often much
 *  faster.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlLeaveCriticalSection(), RtlTryEnterCriticalSection()
 */
NTSTATUS WINAPI RtlpWaitForCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    for (;;)
    {
        EXCEPTION_RECORD rec;
        NTSTATUS status = wait_semaphore( crit, 5 );

        if ( status == STATUS_TIMEOUT )
        {
            const char *name = NULL;
            if (crit->DebugInfo) name = (char *)crit->DebugInfo->Spare[0];
            if (!name) name = "?";
            ERR( "section %p %s wait timed out in thread %04lx, blocked by %04lx, retrying (60 sec)\n",
                 crit, debugstr_a(name), GetCurrentThreadId(), (DWORD)crit->OwningThread );
            status = wait_semaphore( crit, 60 );
            if ( status == STATUS_TIMEOUT && TRACE_ON(relay) )
            {
                ERR( "section %p %s wait timed out in thread %04lx, blocked by %04lx, retrying (5 min)\n",
                     crit, debugstr_a(name), GetCurrentThreadId(), (DWORD) crit->OwningThread );
                status = wait_semaphore( crit, 300 );
            }
        }
        if (status == STATUS_WAIT_0) break;

        /* Throw exception only for Wine internal locks */
        if ((!crit->DebugInfo) || (!crit->DebugInfo->Spare[0])) continue;

        rec.ExceptionCode    = STATUS_POSSIBLE_DEADLOCK;
        rec.ExceptionFlags   = 0;
        rec.ExceptionRecord  = NULL;
        rec.ExceptionAddress = RtlRaiseException;  /* sic */
        rec.NumberParameters = 1;
        rec.ExceptionInformation[0] = (ULONG_PTR)crit;
        RtlRaiseException( &rec );
    }
    if (crit->DebugInfo) crit->DebugInfo->ContentionCount++;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlpUnWaitCriticalSection   (NTDLL.@)
 *
 * Notifies other threads waiting on the busy critical section that it has
 * become free.
 * 
 * PARAMS
 *  crit [I/O] Critical section
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: Any error returned by NtReleaseSemaphore()
 *
 * NOTES
 *  Use RtlLeaveCriticalSection() instead of this function as it is often much
 *  faster.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlLeaveCriticalSection(), RtlTryEnterCriticalSection()
 */
NTSTATUS WINAPI RtlpUnWaitCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (use_futexes() && crit->DebugInfo)  /* debug info is cleared by MakeCriticalSectionGlobal */
    {
        *(int *)&crit->LockSemaphore = 1;
        futex_wake( (int *)&crit->LockSemaphore, 1 );
        return STATUS_SUCCESS;
    }
    else
    {
        HANDLE sem = get_semaphore( crit );
        NTSTATUS res = NtReleaseSemaphore( sem, 1, NULL );
        if (res) RtlRaiseStatus( res );
        return res;
    }
}


/***********************************************************************
 *           RtlEnterCriticalSection   (NTDLL.@)
 *
 * Enters a critical section, waiting for it to become available if necessary.
 *
 * PARAMS
 *  crit [I/O] Critical section to enter
 *
 * RETURNS
 *  STATUS_SUCCESS. The critical section is held by the caller.
 *  
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlSetCriticalSectionSpinCount(),
 *  RtlLeaveCriticalSection(), RtlTryEnterCriticalSection()
 */
NTSTATUS WINAPI RtlEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (crit->SpinCount)
    {
        ULONG count;

        if (RtlTryEnterCriticalSection( crit )) return STATUS_SUCCESS;
        for (count = crit->SpinCount; count > 0; count--)
        {
            if (crit->LockCount > 0) break;  /* more than one waiter, don't bother spinning */
            if (crit->LockCount == -1)       /* try again */
            {
                if (interlocked_cmpxchg( (int *)&crit->LockCount, 0, -1 ) == -1) goto done;
            }
            small_pause();
        }
    }

    if (interlocked_inc( &crit->LockCount ))
    {
        if (crit->OwningThread == (HANDLE)GetCurrentThreadId())
        {
            crit->RecursionCount++;
            return STATUS_SUCCESS;
        }

        /* Now wait for it */
        RtlpWaitForCriticalSection( crit );
    }
done:
    crit->OwningThread   = (HANDLE)GetCurrentThreadId();
    crit->RecursionCount = 1;
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           RtlTryEnterCriticalSection   (NTDLL.@)
 *
 * Tries to enter a critical section without waiting.
 *
 * PARAMS
 *  crit [I/O] Critical section to enter
 *
 * RETURNS
 *  Success: TRUE. The critical section is held by the caller.
 *  Failure: FALSE. The critical section is currently held by another thread.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlLeaveCriticalSection(), RtlSetCriticalSectionSpinCount()
 */
BOOL WINAPI RtlTryEnterCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    BOOL ret = FALSE;
    if (interlocked_cmpxchg( (int *)&crit->LockCount, 0, -1 ) == -1)
    {
        crit->OwningThread   = (HANDLE)GetCurrentThreadId();
        crit->RecursionCount = 1;
        ret = TRUE;
    }
    else if (crit->OwningThread == (HANDLE)GetCurrentThreadId())
    {
        interlocked_inc( &crit->LockCount );
        crit->RecursionCount++;
        ret = TRUE;
    }
    return ret;
}


/***********************************************************************
 *           RtlLeaveCriticalSection   (NTDLL.@)
 *
 * Leaves a critical section.
 *
 * PARAMS
 *  crit [I/O] Critical section to leave.
 *
 * RETURNS
 *  STATUS_SUCCESS.
 *
 * SEE
 *  RtlInitializeCriticalSection(), RtlInitializeCriticalSectionAndSpinCount(),
 *  RtlDeleteCriticalSection(), RtlEnterCriticalSection(),
 *  RtlSetCriticalSectionSpinCount(), RtlTryEnterCriticalSection()
 */
NTSTATUS WINAPI RtlLeaveCriticalSection( RTL_CRITICAL_SECTION *crit )
{
    if (--crit->RecursionCount) interlocked_dec( &crit->LockCount );
    else
    {
        crit->OwningThread = 0;
        if (interlocked_dec( &crit->LockCount ) >= 0)
        {
            /* someone is waiting */
            RtlpUnWaitCriticalSection( crit );
        }
    }
    return STATUS_SUCCESS;
}
