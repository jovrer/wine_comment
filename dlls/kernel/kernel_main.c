/*
 * Kernel initialization code
 *
 * Copyright 2000 Alexandre Julliard
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
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <time.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <signal.h>

#include "windef.h"
#include "winbase.h"
#include "wincon.h"
#include "winternl.h"
#include "wownt32.h"

#include "wine/winbase16.h"
#include "wine/library.h"
#include "wincon.h"
#include "toolhelp.h"
#include "kernel_private.h"
#include "kernel16_private.h"
#include "console_private.h"

extern  int __wine_set_signal_handler(unsigned, int (*)(unsigned));

extern int main_create_flags;

static CRITICAL_SECTION ldt_section;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &ldt_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": ldt_section") }
};
static CRITICAL_SECTION ldt_section = { &critsect_debug, -1, 0, 0, 0, 0 };

static DWORD server_start_time;

/***********************************************************************
 *           locking for LDT routines
 */
static void ldt_lock(void)
{
    RtlEnterCriticalSection( &ldt_section );
}

static void ldt_unlock(void)
{
    RtlLeaveCriticalSection( &ldt_section );
}


/***********************************************************************
 *           KERNEL thread initialisation routine
 */
static void thread_attach(void)
{
    /* allocate the 16-bit stack (FIXME: should be done lazily) */
    HGLOBAL16 hstack = WOWGlobalAlloc16( GMEM_FIXED, 0x10000 );
    kernel_get_thread_data()->stack_sel = GlobalHandleToSel16( hstack );
    NtCurrentTeb()->WOW32Reserved = (void *)MAKESEGPTR( kernel_get_thread_data()->stack_sel,
                                                        0x10000 - sizeof(STACK16FRAME) );
}


/***********************************************************************
 *           KERNEL thread finalisation routine
 */
static void thread_detach(void)
{
    /* free the 16-bit stack */
    WOWGlobalFree16( kernel_get_thread_data()->stack_sel );
    NtCurrentTeb()->WOW32Reserved = 0;
    if (NtCurrentTeb()->Tib.SubSystemTib) TASK_ExitTask();
}


/***********************************************************************
 *           KERNEL process initialisation routine
 */
static BOOL process_attach(void)
{
    SYSTEM_INFO si;
    SYSTEM_TIMEOFDAY_INFORMATION sti;

    NtQuerySystemInformation( SystemTimeOfDayInformation, &sti, sizeof(sti), NULL );
    RtlTimeToSecondsSince1970( &sti.liKeBootTime, &server_start_time );

    /* FIXME: should probably be done in ntdll */
    GetSystemInfo( &si );
    NtCurrentTeb()->Peb->NumberOfProcessors = si.dwNumberOfProcessors;

    /* Setup registry locale information */
    LOCALE_InitRegistry();

    /* Setup computer name */
    COMPUTERNAME_Init();

    /* copy process information from ntdll */
    ENV_CopyStartupInformation();

#ifdef __i386__
    if (GetVersion() & 0x80000000)
    {
        /* create the shared heap for broken win95 native dlls */
        HeapCreate( HEAP_SHARED, 0, 0 );
        /* setup emulation of protected instructions from 32-bit code */
        RtlAddVectoredExceptionHandler( TRUE, INSTR_vectored_handler );
    }
#endif

    /* initialize LDT locking */
    wine_ldt_init_locking( ldt_lock, ldt_unlock );

    /* finish the process initialisation for console bits, if needed */
    __wine_set_signal_handler(SIGINT, CONSOLE_HandleCtrlC);

    if (main_create_flags & CREATE_NEW_CONSOLE)
    {
        HMODULE mod = GetModuleHandleA(0);
        if (RtlImageNtHeader(mod)->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
            AllocConsole();
    }
    else if (!(main_create_flags & DETACHED_PROCESS))
    {
        /* 1/ shall inherit console + handles
         * 2/ shall create std handles, if handles are not inherited
         * TBD when not using wineserver handles for console handles
         */
    }

    if (main_create_flags & CREATE_NEW_PROCESS_GROUP)
        SetConsoleCtrlHandler(NULL, TRUE);

    /* Create 16-bit task */
    LoadLibrary16( "krnl386.exe" );
    thread_attach();
    TASK_CreateMainTask();
    return TRUE;
}

/***********************************************************************
 *           KERNEL initialisation routine
 */
BOOL WINAPI DllMain( HINSTANCE hinst, DWORD reason, LPVOID reserved )
{
    switch(reason)
    {
    case DLL_PROCESS_ATTACH:
        return process_attach();
    case DLL_THREAD_ATTACH:
        thread_attach();
        break;
    case DLL_THREAD_DETACH:
        thread_detach();
        break;
    case DLL_PROCESS_DETACH:
        WriteOutProfiles16();
        break;
    }
    return TRUE;
}

/***********************************************************************
 *           MulDiv   (KERNEL32.@)
 * RETURNS
 *	Result of multiplication and division
 *	-1: Overflow occurred or Divisor was 0
 */
INT WINAPI MulDiv( INT nMultiplicand, INT nMultiplier, INT nDivisor)
{
    LONGLONG ret;

    if (!nDivisor) return -1;

    /* We want to deal with a positive divisor to simplify the logic. */
    if (nDivisor < 0)
    {
      nMultiplicand = - nMultiplicand;
      nDivisor = -nDivisor;
    }

    /* If the result is positive, we "add" to round. else, we subtract to round. */
    if ( ( (nMultiplicand <  0) && (nMultiplier <  0) ) ||
         ( (nMultiplicand >= 0) && (nMultiplier >= 0) ) )
      ret = (((LONGLONG)nMultiplicand * nMultiplier) + (nDivisor/2)) / nDivisor;
    else
      ret = (((LONGLONG)nMultiplicand * nMultiplier) - (nDivisor/2)) / nDivisor;

    if ((ret > 2147483647) || (ret < -2147483647)) return -1;
    return ret;
}


/***********************************************************************
 *           GetTickCount       (KERNEL32.@)
 *
 * Get the number of milliseconds the system has been running.
 *
 * PARAMS
 *  None.
 *
 * RETURNS
 *  The current tick count.
 *
 * NOTES
 *  -The value returned will wrap arounf every 2^32 milliseconds.
 *  -Under Windows, tick 0 is the moment at which the system is rebooted.
 *  Under Wine, tick 0 begins at the moment the wineserver process is started,
 */
DWORD WINAPI GetTickCount(void)
{
    struct timeval t;
    gettimeofday( &t, NULL );
    return ((t.tv_sec - server_start_time) * 1000) + (t.tv_usec / 1000);
}
