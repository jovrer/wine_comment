/*
 * Window procedure callbacks
 *
 * Copyright 1995 Martin von Loewis
 * Copyright 1996 Alexandre Julliard
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

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wownt32.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "controls.h"
#include "win.h"
#include "winproc.h"
#include "user_private.h"
#include "dde.h"
#include "winternl.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DECLARE_DEBUG_CHANNEL(msg);
WINE_DECLARE_DEBUG_CHANNEL(relay);
WINE_DEFAULT_DEBUG_CHANNEL(win);

#ifdef __i386__

#include "pshpack1.h"

/* Window procedure 16-to-32-bit thunk */
typedef struct
{
    BYTE       popl_eax;             /* popl  %eax (return address) */
    BYTE       pushl_func;           /* pushl $proc */
    WNDPROC    proc;
    BYTE       pushl_eax;            /* pushl %eax */
    BYTE       ljmp;                 /* ljmp relay*/
    DWORD      relay_offset;         /* __wine_call_wndproc_32A/W */
    WORD       relay_sel;
} WINPROC_THUNK_FROM16;

/* Window procedure 32-to-16-bit thunk */
typedef struct
{
    BYTE       popl_eax;             /* popl  %eax (return address) */
    BYTE       pushl_func;           /* pushl $proc */
    WNDPROC16  proc;
    BYTE       pushl_eax;            /* pushl %eax */
    BYTE       jmp;                  /* jmp   relay (relative jump)*/
    void     (*relay)();             /* WINPROC_CallProc32ATo16() */
} WINPROC_THUNK_FROM32;

/* Simple jmp to call 32-bit procedure directly */
typedef struct
{
    BYTE       jmp;                  /* jmp  proc (relative jump) */
    WNDPROC    proc;
} WINPROC_JUMP;

#include "poppack.h"

#else /* __i386__ */

typedef struct
{
    WNDPROC    proc;
} WINPROC_THUNK_FROM16;

typedef struct
{
    WNDPROC16  proc;
} WINPROC_THUNK_FROM32;

typedef struct
{
    WNDPROC    proc;
} WINPROC_JUMP;

#endif /* __i386__ */

typedef union
{
    WINPROC_THUNK_FROM16  t_from16;
    WINPROC_THUNK_FROM32  t_from32;
} WINPROC_THUNK;

typedef struct tagWINDOWPROC
{
    WINPROC_THUNK  thunk;    /* Thunk */
    WINPROC_JUMP   jmp;      /* Jump */
    BYTE           type;     /* Function type */
} WINDOWPROC;

static LRESULT WINAPI WINPROC_CallProc32ATo16( WNDPROC16 func, HWND hwnd,
                                               UINT msg, WPARAM wParam,
                                               LPARAM lParam );
static LRESULT WINAPI WINPROC_CallProc32WTo16( WNDPROC16 func, HWND hwnd,
                                               UINT msg, WPARAM wParam,
                                               LPARAM lParam );

#define MAX_WINPROCS  (0x10000 / sizeof(WINDOWPROC))

static WINDOWPROC winproc_array[MAX_WINPROCS];
static UINT winproc_used;

static CRITICAL_SECTION winproc_cs;
static CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &winproc_cs,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": winproc_cs") }
};
static CRITICAL_SECTION winproc_cs = { &critsect_debug, -1, 0, 0, 0, 0 };

static BOOL is_valid_winproc( WINDOWPROC *proc )
{
    /* check alignment */
    if (((BYTE *)proc - (BYTE *)winproc_array) % sizeof(*proc)) return FALSE;
    /* check array limits */
    if (proc < winproc_array || proc >= winproc_array + winproc_used) return FALSE;
    return (proc->type != WIN_PROC_INVALID);
}

/* find an existing winproc for a given function and type */
/* FIXME: probably should do something more clever than a linear search */
static inline WINDOWPROC *find_winproc( WNDPROC func, WINDOWPROCTYPE type )
{
    unsigned int i;

    if (type == WIN_PROC_16)
    {
        for (i = 0; i < winproc_used; i++)
        {
            if (winproc_array[i].type == type &&
                winproc_array[i].thunk.t_from32.proc == (WNDPROC16)func)
                return &winproc_array[i];
        }
    }
    else
    {
        for (i = 0; i < winproc_used; i++)
        {
            if (winproc_array[i].type == type &&
                winproc_array[i].thunk.t_from16.proc == func)
                return &winproc_array[i];
        }
    }
    return NULL;
}

/* initialize a new winproc */
static inline void set_winproc( WINDOWPROC *proc, WNDPROC func, WINDOWPROCTYPE type )
{
#ifdef __i386__
    static FARPROC16 relay_32A, relay_32W;

    switch(type)
    {
    case WIN_PROC_16:
        proc->thunk.t_from32.popl_eax    = 0x58;   /* popl  %eax */
        proc->thunk.t_from32.pushl_func  = 0x68;   /* pushl $proc */
        proc->thunk.t_from32.proc        = (WNDPROC16)func;
        proc->thunk.t_from32.pushl_eax   = 0x50;   /* pushl %eax */
        proc->thunk.t_from32.jmp         = 0xe9;   /* jmp   relay*/
        proc->thunk.t_from32.relay =  /* relative jump */
            (void(*)())((DWORD)WINPROC_CallProc32ATo16 -
                        (DWORD)(&proc->thunk.t_from32.relay + 1));
        break;
    case WIN_PROC_32A:
        if (!relay_32A) relay_32A = GetProcAddress16( GetModuleHandle16("user"),
                                                      "__wine_call_wndproc_32A" );
        proc->thunk.t_from16.popl_eax     = 0x58;   /* popl  %eax */
        proc->thunk.t_from16.pushl_func   = 0x68;   /* pushl $proc */
        proc->thunk.t_from16.proc         = func;
        proc->thunk.t_from16.pushl_eax    = 0x50;   /* pushl %eax */
        proc->thunk.t_from16.ljmp         = 0xea;   /* ljmp   relay*/
        proc->thunk.t_from16.relay_offset = OFFSETOF(relay_32A);
        proc->thunk.t_from16.relay_sel    = SELECTOROF(relay_32A);
        proc->jmp.jmp  = 0xe9;
        /* Fixup relative jump */
        proc->jmp.proc = (WNDPROC)((DWORD)func - (DWORD)(&proc->jmp.proc + 1));
        break;
    case WIN_PROC_32W:
        if (!relay_32W) relay_32W = GetProcAddress16( GetModuleHandle16("user"),
                                                      "__wine_call_wndproc_32W" );
        proc->thunk.t_from16.popl_eax     = 0x58;   /* popl  %eax */
        proc->thunk.t_from16.pushl_func   = 0x68;   /* pushl $proc */
        proc->thunk.t_from16.proc         = func;
        proc->thunk.t_from16.pushl_eax    = 0x50;   /* pushl %eax */
        proc->thunk.t_from16.ljmp         = 0xea;   /* ljmp   relay*/
        proc->thunk.t_from16.relay_offset = OFFSETOF(relay_32W);
        proc->thunk.t_from16.relay_sel    = SELECTOROF(relay_32W);
        proc->jmp.jmp  = 0xe9;
        /* Fixup relative jump */
        proc->jmp.proc = (WNDPROC)((char *)func - (char *)(&proc->jmp.proc + 1));
        break;
    default:
        /* Should not happen */
        break;
    }
#else /* __i386__ */
    switch(type)
    {
    case WIN_PROC_16:
        proc->thunk.t_from32.proc = (WNDPROC16)func;
        break;
    case WIN_PROC_32A:
    case WIN_PROC_32W:
        proc->thunk.t_from16.proc = func;
        break;
    default:
        /* Should not happen */
        break;
    }
#endif /* __i386__ */

    proc->type = type;
}

static WORD get_winproc_selector(void)
{
    static LONG winproc_selector;
    WORD ret;

    if (!(ret = winproc_selector))
    {
        LDT_ENTRY entry;
        WORD sel = wine_ldt_alloc_entries(1);
        wine_ldt_set_base( &entry, winproc_array );
        wine_ldt_set_limit( &entry, sizeof(winproc_array) - 1 );
        wine_ldt_set_flags( &entry, WINE_LDT_FLAGS_CODE | WINE_LDT_FLAGS_32BIT );
        wine_ldt_set_entry( sel, &entry );
        if (!(ret = InterlockedCompareExchange( &winproc_selector, sel, 0 ))) ret = sel;
        else wine_ldt_free_entries( sel, 1 );  /* somebody beat us to it */
    }
    return ret;
}


#ifdef __i386__
/* Some window procedures modify register they shouldn't, or are not
 * properly declared stdcall; so we need a small assembly wrapper to
 * call them. */
extern LRESULT WINPROC_wrapper( WNDPROC proc, HWND hwnd, UINT msg,
                                WPARAM wParam, LPARAM lParam );
__ASM_GLOBAL_FUNC( WINPROC_wrapper,
                   "pushl %ebp\n\t"
                   "movl %esp,%ebp\n\t"
                   "pushl %edi\n\t"
                   "pushl %esi\n\t"
                   "pushl %ebx\n\t"
                   "pushl 24(%ebp)\n\t"
                   "pushl 20(%ebp)\n\t"
                   "pushl 16(%ebp)\n\t"
                   "pushl 12(%ebp)\n\t"
                   "movl 8(%ebp),%eax\n\t"
                   "call *%eax\n\t"
                   "leal -12(%ebp),%esp\n\t"
                   "popl %ebx\n\t"
                   "popl %esi\n\t"
                   "popl %edi\n\t"
                   "leave\n\t"
                   "ret" );
#else
static inline LRESULT WINPROC_wrapper( WNDPROC proc, HWND hwnd, UINT msg,
                                       WPARAM wParam, LPARAM lParam )
{
    return proc( hwnd, msg, wParam, lParam );
}
#endif  /* __i386__ */


static void MINMAXINFO32to16( const MINMAXINFO *from, MINMAXINFO16 *to )
{
    to->ptReserved.x     = from->ptReserved.x;
    to->ptReserved.y     = from->ptReserved.y;
    to->ptMaxSize.x      = from->ptMaxSize.x;
    to->ptMaxSize.y      = from->ptMaxSize.y;
    to->ptMaxPosition.x  = from->ptMaxPosition.x;
    to->ptMaxPosition.y  = from->ptMaxPosition.y;
    to->ptMinTrackSize.x = from->ptMinTrackSize.x;
    to->ptMinTrackSize.y = from->ptMinTrackSize.y;
    to->ptMaxTrackSize.x = from->ptMaxTrackSize.x;
    to->ptMaxTrackSize.y = from->ptMaxTrackSize.y;
}

static void MINMAXINFO16to32( const MINMAXINFO16 *from, MINMAXINFO *to )
{
    to->ptReserved.x     = from->ptReserved.x;
    to->ptReserved.y     = from->ptReserved.y;
    to->ptMaxSize.x      = from->ptMaxSize.x;
    to->ptMaxSize.y      = from->ptMaxSize.y;
    to->ptMaxPosition.x  = from->ptMaxPosition.x;
    to->ptMaxPosition.y  = from->ptMaxPosition.y;
    to->ptMinTrackSize.x = from->ptMinTrackSize.x;
    to->ptMinTrackSize.y = from->ptMinTrackSize.y;
    to->ptMaxTrackSize.x = from->ptMaxTrackSize.x;
    to->ptMaxTrackSize.y = from->ptMaxTrackSize.y;
}

static void WINDOWPOS32to16( const WINDOWPOS* from, WINDOWPOS16* to )
{
    to->hwnd            = HWND_16(from->hwnd);
    to->hwndInsertAfter = HWND_16(from->hwndInsertAfter);
    to->x               = from->x;
    to->y               = from->y;
    to->cx              = from->cx;
    to->cy              = from->cy;
    to->flags           = from->flags;
}

static void WINDOWPOS16to32( const WINDOWPOS16* from, WINDOWPOS* to )
{
    to->hwnd            = WIN_Handle32(from->hwnd);
    to->hwndInsertAfter = (from->hwndInsertAfter == (HWND16)-1) ?
                           HWND_TOPMOST : WIN_Handle32(from->hwndInsertAfter);
    to->x               = from->x;
    to->y               = from->y;
    to->cx              = from->cx;
    to->cy              = from->cy;
    to->flags           = from->flags;
}

/* The strings are not copied */
static void CREATESTRUCT32Ato16( const CREATESTRUCTA* from, CREATESTRUCT16* to )
{
    to->lpCreateParams = (SEGPTR)from->lpCreateParams;
    to->hInstance      = HINSTANCE_16(from->hInstance);
    to->hMenu          = HMENU_16(from->hMenu);
    to->hwndParent     = HWND_16(from->hwndParent);
    to->cy             = from->cy;
    to->cx             = from->cx;
    to->y              = from->y;
    to->x              = from->x;
    to->style          = from->style;
    to->dwExStyle      = from->dwExStyle;
}

static void CREATESTRUCT16to32A( const CREATESTRUCT16* from, CREATESTRUCTA *to )

{
    to->lpCreateParams = (LPVOID)from->lpCreateParams;
    to->hInstance      = HINSTANCE_32(from->hInstance);
    to->hMenu          = HMENU_32(from->hMenu);
    to->hwndParent     = WIN_Handle32(from->hwndParent);
    to->cy             = from->cy;
    to->cx             = from->cx;
    to->y              = from->y;
    to->x              = from->x;
    to->style          = from->style;
    to->dwExStyle      = from->dwExStyle;
}

/* The strings are not copied */
static void MDICREATESTRUCT32Ato16( const MDICREATESTRUCTA* from, MDICREATESTRUCT16* to )
{
    to->hOwner = HINSTANCE_16(from->hOwner);
    to->x      = from->x;
    to->y      = from->y;
    to->cx     = from->cx;
    to->cy     = from->cy;
    to->style  = from->style;
    to->lParam = from->lParam;
}

static void MDICREATESTRUCT16to32A( const MDICREATESTRUCT16* from, MDICREATESTRUCTA *to )
{
    to->hOwner = HINSTANCE_32(from->hOwner);
    to->x      = from->x;
    to->y      = from->y;
    to->cx     = from->cx;
    to->cy     = from->cy;
    to->style  = from->style;
    to->lParam = from->lParam;
}

/**********************************************************************
 *	     WINPROC_CallWndProc32
 *
 * Call a 32-bit WndProc.
 */
static LRESULT WINPROC_CallWndProc( WNDPROC proc, HWND hwnd, UINT msg,
                                      WPARAM wParam, LPARAM lParam )
{
    LRESULT retvalue;

    USER_CheckNotLock();

    hwnd = WIN_GetFullHandle( hwnd );
    if (TRACE_ON(relay))
        DPRINTF( "%04lx:Call window proc %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                 GetCurrentThreadId(), proc, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam );

    retvalue = WINPROC_wrapper( proc, hwnd, msg, wParam, lParam );

    if (TRACE_ON(relay))
        DPRINTF( "%04lx:Ret  window proc %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx) retval=%08lx\n",
                 GetCurrentThreadId(), proc, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam, retvalue );
    return retvalue;
}

/***********************************************************************
 *           WINPROC_CallWndProc16
 *
 * Call a 16-bit window procedure
 */
static LRESULT WINAPI WINPROC_CallWndProc16( WNDPROC16 proc, HWND16 hwnd,
                                             UINT16 msg, WPARAM16 wParam,
                                             LPARAM lParam )
{
    CONTEXT86 context;
    size_t size = 0;
    struct
    {
        WORD params[5];
        union
        {
            CREATESTRUCT16 cs16;
            DRAWITEMSTRUCT16 dis16;
            COMPAREITEMSTRUCT16 cis16;
        } u;
    } args;

    USER_CheckNotLock();

    /* Window procedures want ax = hInstance, ds = es = ss */

    memset(&context, 0, sizeof(context));
    context.SegDs = context.SegEs = SELECTOROF(NtCurrentTeb()->WOW32Reserved);
    context.SegFs = wine_get_fs();
    context.SegGs = wine_get_gs();
    if (!(context.Eax = GetWindowWord( HWND_32(hwnd), GWLP_HINSTANCE ))) context.Eax = context.SegDs;
    context.SegCs = SELECTOROF(proc);
    context.Eip   = OFFSETOF(proc);
    context.Ebp   = OFFSETOF(NtCurrentTeb()->WOW32Reserved) + (WORD)&((STACK16FRAME*)0)->bp;

    if (lParam)
    {
        /* Some programs (eg. the "Undocumented Windows" examples, JWP) only
           work if structures passed in lParam are placed in the stack/data
           segment. Programmers easily make the mistake of converting lParam
           to a near rather than a far pointer, since Windows apparently
           allows this. We copy the structures to the 16 bit stack; this is
           ugly but makes these programs work. */
        switch (msg)
        {
          case WM_CREATE:
          case WM_NCCREATE:
            size = sizeof(CREATESTRUCT16); break;
          case WM_DRAWITEM:
            size = sizeof(DRAWITEMSTRUCT16); break;
          case WM_COMPAREITEM:
            size = sizeof(COMPAREITEMSTRUCT16); break;
        }
        if (size)
        {
            memcpy( &args.u, MapSL(lParam), size );
            lParam = (SEGPTR)NtCurrentTeb()->WOW32Reserved - size;
        }
    }

    args.params[4] = hwnd;
    args.params[3] = msg;
    args.params[2] = wParam;
    args.params[1] = HIWORD(lParam);
    args.params[0] = LOWORD(lParam);
    WOWCallback16Ex( 0, WCB16_REGS, sizeof(args.params) + size, &args, (DWORD *)&context );
    return MAKELONG( LOWORD(context.Eax), LOWORD(context.Edx) );
}


/**********************************************************************
 *	     WINPROC_GetPtr
 *
 * Return a pointer to the win proc.
 */
static WINDOWPROC *WINPROC_GetPtr( WNDPROC handle )
{
    BYTE *ptr;
    WINDOWPROC *proc;

    /* ptr cannot be < 64K */
    if (!HIWORD(handle)) return NULL;

    /* Check for a linear pointer */

    ptr = (BYTE *)handle;
    /* First check if it is the jmp address */
    proc = (WINDOWPROC *)(ptr - FIELD_OFFSET(WINDOWPROC,jmp));
    if (is_valid_winproc(proc)) return proc;

    /* Now it must be the thunk address */
    proc = (WINDOWPROC *)(ptr - FIELD_OFFSET(WINDOWPROC,thunk));
    if (is_valid_winproc(proc)) return proc;

    /* Check for a segmented pointer */

    if (HIWORD(handle) == get_winproc_selector())
    {
        ptr = (BYTE *)winproc_array + LOWORD(handle);
        /* It must be the thunk address */
        proc = (WINDOWPROC *)(ptr - FIELD_OFFSET(WINDOWPROC,thunk));
        if (is_valid_winproc(proc)) return proc;
    }
    return NULL;
}


/**********************************************************************
 *	     WINPROC_GetProc
 *
 * Get a window procedure pointer that can be passed to the Windows program.
 */
WNDPROC16 WINPROC_GetProc( WNDPROC proc, WINDOWPROCTYPE type )
{
    WINDOWPROC *ptr = (WINDOWPROC *)proc;

    if (!proc) return NULL;
    if (type == WIN_PROC_16)  /* We want a 16:16 address */
    {
        if (ptr->type == WIN_PROC_16)
            return ptr->thunk.t_from32.proc;
        else
            return (WNDPROC16)MAKESEGPTR( get_winproc_selector(),
                                          (char *)&ptr->thunk - (char *)winproc_array );
    }
    else  /* We want a 32-bit address */
    {
        if (ptr->type == WIN_PROC_16)
            return (WNDPROC16)&ptr->thunk;
        else if (type != ptr->type)
            /* Have to return the jmp address if types don't match */
            return (WNDPROC16)&ptr->jmp;
        else
            /* Some Win16 programs want to get back the proc they set */
            return (WNDPROC16)ptr->thunk.t_from16.proc;
    }
}


/**********************************************************************
 *	     WINPROC_AllocProc
 *
 * Allocate a window procedure for a window or class.
 *
 * Note that allocated winprocs are never freed; the idea is that even if an app creates a
 * lot of windows, it will usually only have a limited number of window procedures, so the
 * array won't grow too large, and this way we avoid the need to track allocations per window.
 */
WNDPROC WINPROC_AllocProc( WNDPROC func, WINDOWPROCTYPE type )
{
    WINDOWPROC *proc;

    if (!func) return NULL;

    EnterCriticalSection( &winproc_cs );

    /* check if the function is already a win proc */
    if (!(proc = WINPROC_GetPtr( func )))
    {
        /* then check if we already have a winproc for that function */
        if (!(proc = find_winproc( func, type )))
        {
            if (winproc_used >= MAX_WINPROCS)
                FIXME( "too many winprocs, cannot allocate one for %p/%d\n", func, type );
            else
            {
                proc = &winproc_array[winproc_used++];
                set_winproc( proc, func, type );
                TRACE( "allocated %p for %p/%d (%d/%d used)\n",
                       proc, func, type, winproc_used, MAX_WINPROCS );
            }
        }
        else TRACE( "reusing %p for %p/%d\n", proc, func, type );
    }

    LeaveCriticalSection( &winproc_cs );
    return (WNDPROC)proc;
}


/**********************************************************************
 *	     WINPROC_GetProcType
 *
 * Return the window procedure type.
 */
WINDOWPROCTYPE WINPROC_GetProcType( WNDPROC proc )
{
    if (!proc) return WIN_PROC_INVALID;
    return ((WINDOWPROC *)proc)->type;
}
/**********************************************************************
 *	     WINPROC_TestCBForStr
 *
 * Return TRUE if the lparam is a string
 */
inline static BOOL WINPROC_TestCBForStr( HWND hwnd )
{
    DWORD style = GetWindowLongA( hwnd, GWL_STYLE );
    return (!(style & (CBS_OWNERDRAWFIXED | CBS_OWNERDRAWVARIABLE)) || (style & CBS_HASSTRINGS));
}
/**********************************************************************
 *	     WINPROC_TestLBForStr
 *
 * Return TRUE if the lparam is a string
 */
inline static BOOL WINPROC_TestLBForStr( HWND hwnd )
{
    DWORD style = GetWindowLongA( hwnd, GWL_STYLE );
    return (!(style & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE)) || (style & LBS_HASSTRINGS));

}
/**********************************************************************
 *	     WINPROC_MapMsg32ATo32W
 *
 * Map a message from Ansi to Unicode.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 *
 * FIXME:
 *  WM_GETTEXT/WM_SETTEXT and static control with SS_ICON style:
 *  the first four bytes are the handle of the icon
 *  when the WM_SETTEXT message has been used to set the icon
 */
INT WINPROC_MapMsg32ATo32W( HWND hwnd, UINT msg, WPARAM *pwparam, LPARAM *plparam )
{
    switch(msg)
    {
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0,
                                     *pwparam * sizeof(WCHAR) + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
        }
        return 1;
    /* lparam is string (0-terminated) */
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case CB_DIR:
    case LB_DIR:
    case LB_ADDFILE:
    case EM_REPLACESEL:
        if (!*plparam) return 0;
        else
        {
            DWORD len = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)*plparam, -1, NULL, 0);
            WCHAR *buf = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));
            MultiByteToWideChar(CP_ACP, 0, (LPCSTR)*plparam, -1, buf, len);
            *plparam = (LPARAM)buf;
            return (*plparam ? 1 : -1);
        }
    case WM_GETTEXTLENGTH:
    case CB_GETLBTEXTLEN:
    case LB_GETTEXTLEN:
        return 1;  /* need to map result */
    case WM_NCCREATE:
    case WM_CREATE:
        {
            UNICODE_STRING usBuffer;
            struct s
            { CREATESTRUCTW cs;         /* new structure */
              LPCWSTR lpszName;         /* allocated Name */
              LPCWSTR lpszClass;        /* allocated Class */
            };

            struct s *xs = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct s));
            if (!xs) return -1;
            xs->cs = *(CREATESTRUCTW *)*plparam;
            if (HIWORD(xs->cs.lpszName))
            {
                RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)xs->cs.lpszName);
                xs->lpszName = xs->cs.lpszName = usBuffer.Buffer;
            }
            if (HIWORD(xs->cs.lpszClass))
            {
                RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)xs->cs.lpszClass);
                xs->lpszClass = xs->cs.lpszClass = usBuffer.Buffer;
            }

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCTW *mdi_cs = HeapAlloc(GetProcessHeap(), 0, sizeof(*mdi_cs));
                *mdi_cs = *(MDICREATESTRUCTW *)xs->cs.lpCreateParams;
                if (HIWORD(mdi_cs->szTitle))
                {
                    RtlCreateUnicodeStringFromAsciiz(&usBuffer, (LPCSTR)mdi_cs->szTitle);
                    mdi_cs->szTitle = usBuffer.Buffer;
                }
                if (HIWORD(mdi_cs->szClass))
                {
                    RtlCreateUnicodeStringFromAsciiz(&usBuffer, (LPCSTR)mdi_cs->szClass);
                    mdi_cs->szClass = usBuffer.Buffer;
                }
                xs->cs.lpCreateParams = mdi_cs;
            }

            *plparam = (LPARAM)xs;
        }
        return 1;
    case WM_MDICREATE:
        {
            MDICREATESTRUCTW *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) );
            if (!cs) return -1;
            *cs = *(MDICREATESTRUCTW *)*plparam;
            if (HIWORD(cs->szClass))
            {
                UNICODE_STRING usBuffer;
                RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)cs->szClass);
                cs->szClass = usBuffer.Buffer;
            }
            if (HIWORD(cs->szTitle))
            {
                UNICODE_STRING usBuffer;
                RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)cs->szTitle);
                cs->szTitle = usBuffer.Buffer;
            }
            *plparam = (LPARAM)cs;
        }
        return 1;

/* Listbox */
    case LB_ADDSTRING:
    case LB_INSERTSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING:
        if(!*plparam) return 0;
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            UNICODE_STRING usBuffer;
            RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)*plparam);
            *plparam = (LPARAM)usBuffer.Buffer;
        }
        return (*plparam ? 1 : -1);

    case LB_GETTEXT:                /* FIXME: fixed sized buffer */
        { if ( WINPROC_TestLBForStr( hwnd ))
          { LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, 512 * sizeof(WCHAR) + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
          }
        }
        return 1;

/* Combobox */
    case CB_ADDSTRING:
    case CB_INSERTSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_FINDSTRING:
    case CB_SELECTSTRING:
        if(!*plparam) return 0;
        if ( WINPROC_TestCBForStr( hwnd ))
        {
            UNICODE_STRING usBuffer;
            RtlCreateUnicodeStringFromAsciiz(&usBuffer,(LPCSTR)*plparam);
            *plparam = (LPARAM)usBuffer.Buffer;
        }
        return (*plparam ? 1 : -1);

    case CB_GETLBTEXT:    /* FIXME: fixed sized buffer */
        { if ( WINPROC_TestCBForStr( hwnd ))
          { LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, 512 * sizeof(WCHAR) + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
          }
        }
        return 1;

/* Multiline edit */
    case EM_GETLINE:
        { WORD len = (WORD)*plparam;
          LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, sizeof(LPARAM) + sizeof (WORD) + len*sizeof(WCHAR) );
          if (!ptr) return -1;
          *ptr++ = *plparam;  /* Store previous lParam */
          *((WORD *) ptr) = len;   /* Store the length */
          *plparam = (LPARAM)ptr;
        }
        return 1;

    case WM_CHARTOITEM:
    case WM_MENUCHAR:
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
    case EM_SETPASSWORDCHAR:
        {
            CHAR ch = LOWORD(*pwparam);
            WCHAR wch;
            MultiByteToWideChar(CP_ACP, 0, &ch, 1, &wch, 1);
            *pwparam = MAKEWPARAM( wch, HIWORD(*pwparam) );
        }
        return 0;

    case WM_IME_CHAR:
        {
            CHAR ch[2];
            WCHAR wch;
            ch[0] = (*pwparam >> 8);
            ch[1] = *pwparam & 0xff;
            if (ch[0])
                MultiByteToWideChar(CP_ACP, 0, ch, 2, &wch, 1);
            else
                MultiByteToWideChar(CP_ACP, 0, &ch[1], 1, &wch, 1);
            *pwparam = MAKEWPARAM( wch, HIWORD(*pwparam) );
        }
        return 0;

    case WM_PAINTCLIPBOARD:
    case WM_SIZECLIPBOARD:
        FIXME_(msg)("message %s (0x%x) needs translation, please report\n", SPY_GetMsgName(msg, hwnd), msg );
        return -1;
    default:  /* No translation needed */
        return 0;
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg32ATo32W
 *
 * Unmap a message that was mapped from Ansi to Unicode.
 */
LRESULT WINPROC_UnmapMsg32ATo32W( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                  LRESULT result, WNDPROC dispatch )
{
    switch(msg)
    {
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            if (!wParam) result = 0;
            else if (!(result = WideCharToMultiByte( CP_ACP, 0, (LPWSTR)lParam, -1,
                                                    (LPSTR)*ptr, wParam, NULL, NULL )))
            {
                ((LPSTR)*ptr)[wParam-1] = 0;
                result = wParam - 1;
            }
            else result--;  /* do not count terminating null */
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;
    case WM_GETTEXTLENGTH:
    case CB_GETLBTEXTLEN:
    case LB_GETTEXTLEN:
        if (result >= 0)
        {
            /* Determine respective GETTEXT message */
            UINT msgGetText =
              (msg == WM_GETTEXTLENGTH) ? WM_GETTEXT :
              ((msg == CB_GETLBTEXTLEN) ? CB_GETLBTEXT : LB_GETTEXT);
            /* wParam differs between the messages */
            WPARAM wp = (msg == WM_GETTEXTLENGTH) ? (WPARAM)(result + 1) : wParam;

            WCHAR* p = HeapAlloc (GetProcessHeap(), 0, (result + 1) * sizeof(WCHAR));

            if (p)
            {
                LRESULT n;

                if (dispatch)
                    n = WINPROC_CallWndProc(dispatch, hwnd, msgGetText, wp, (LPARAM)p);
                else
                    n = SendMessageW (hwnd, msgGetText, wp, (LPARAM)p);

                result = WideCharToMultiByte( CP_ACP, 0, p, n, NULL, 0, 0, NULL );
                HeapFree (GetProcessHeap(), 0, p);
            }
        }
        break;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            struct s
            { CREATESTRUCTW cs;         /* new structure */
              LPWSTR lpszName;          /* allocated Name */
              LPWSTR lpszClass;         /* allocated Class */
            };
            struct s *xs = (struct s *)lParam;
            HeapFree( GetProcessHeap(), 0, xs->lpszName );
            HeapFree( GetProcessHeap(), 0, xs->lpszClass );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCTW *mdi_cs = (MDICREATESTRUCTW *)xs->cs.lpCreateParams;
                if (HIWORD(mdi_cs->szTitle))
                    HeapFree(GetProcessHeap(), 0, (LPVOID)mdi_cs->szTitle);
                if (HIWORD(mdi_cs->szClass))
                    HeapFree(GetProcessHeap(), 0, (LPVOID)mdi_cs->szClass);
                HeapFree(GetProcessHeap(), 0, mdi_cs);
            }
            HeapFree( GetProcessHeap(), 0, xs );
        }
        break;

    case WM_MDICREATE:
        {
            MDICREATESTRUCTW *cs = (MDICREATESTRUCTW *)lParam;
            if (HIWORD(cs->szTitle))
                HeapFree( GetProcessHeap(), 0, (LPVOID)cs->szTitle );
            if (HIWORD(cs->szClass))
                HeapFree( GetProcessHeap(), 0, (LPVOID)cs->szClass );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;

    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case CB_DIR:
    case LB_DIR:
    case LB_ADDFILE:
    case EM_REPLACESEL:
        HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

/* Listbox */
    case LB_ADDSTRING:
    case LB_INSERTSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING:
        if ( WINPROC_TestLBForStr( hwnd ))
          HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

    case LB_GETTEXT:
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            result = WideCharToMultiByte( CP_ACP, 0, (LPWSTR)lParam, -1,
                                          (LPSTR)*ptr, 0x7fffffff, NULL, NULL ) - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;

/* Combobox */
    case CB_ADDSTRING:
    case CB_INSERTSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING:
        if ( WINPROC_TestCBForStr( hwnd ))
          HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

    case CB_GETLBTEXT:
        if ( result < 0) /* CB_ERR and CB_ERRSPACE */
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        else if ( WINPROC_TestCBForStr( hwnd ))
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            result = WideCharToMultiByte( CP_ACP, 0, (LPWSTR)lParam, -1,
                                          (LPSTR)*ptr, 0x7fffffff, NULL, NULL ) - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;

/* Multiline edit */
    case EM_GETLINE:
        {
            LPARAM * ptr = (LPARAM *)lParam - 1;  /* get the old lParam */
            WORD len = *(WORD *) lParam;
            result = WideCharToMultiByte( CP_ACP, 0, (LPWSTR)lParam, result,
                                          (LPSTR)*ptr, len, NULL, NULL );
            if (result < len) ((LPSTR)*ptr)[result] = 0;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;
    }
    return result;
}


/**********************************************************************
 *	     WINPROC_MapMsg32WTo32A
 *
 * Map a message from Unicode to Ansi.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 */
static INT WINPROC_MapMsg32WTo32A( HWND hwnd, UINT msg, WPARAM *pwparam, LPARAM *plparam )
{
    switch(msg)
    {
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, *pwparam + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
        }
        return 1;

    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case CB_DIR:
    case LB_DIR:
    case LB_ADDFILE:
    case EM_REPLACESEL:
        if (*plparam)
        {
            LPCWSTR str = (LPCWSTR)*plparam;
            int len = WideCharToMultiByte(CP_ACP, 0, str, -1, NULL, 0, 0, 0);
            *plparam = (LPARAM)HeapAlloc(GetProcessHeap(), 0, len);
            if (!*plparam) return -1;
            WideCharToMultiByte(CP_ACP, 0, str, -1, (LPSTR)*plparam, len, 0, 0);
            return 1;
        }
        return 0;

    case WM_MDICREATE:
        {
            MDICREATESTRUCTA *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) );
            if (!cs) return -1;
            *cs = *(MDICREATESTRUCTA *)*plparam;
            if (HIWORD(cs->szTitle))
            {
                int len = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)cs->szTitle, -1, NULL, 0, 0, 0);
                LPSTR buf = HeapAlloc(GetProcessHeap(), 0, len);
                if (buf) WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)cs->szTitle, -1, buf, len, 0, 0);
                cs->szTitle = buf;
            }
            if (HIWORD(cs->szClass))
            {
                int len = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)cs->szClass, -1, NULL, 0, 0, 0);
                LPSTR buf = HeapAlloc(GetProcessHeap(), 0, len);
                if (buf) WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)cs->szClass, -1, buf, len, 0, 0);
                cs->szClass = buf;
            }
            *plparam = (LPARAM)cs;
        }
        return 1;

/* Listbox */
    case LB_ADDSTRING:
    case LB_INSERTSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING:
        if(!*plparam) return 0;
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            int len = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)*plparam, -1, NULL, 0, 0, 0);
            LPSTR buf = HeapAlloc(GetProcessHeap(), 0, len);
            if (buf) WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)*plparam, -1, buf, len, 0, 0);
            *plparam = (LPARAM)buf;
        }
        return (*plparam ? 1 : -1);

    case LB_GETTEXT:                    /* FIXME: fixed sized buffer */
        { if ( WINPROC_TestLBForStr( hwnd ))
          { LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, 512 + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
          }
        }
        return 1;

/* Combobox */
    case CB_ADDSTRING:
    case CB_INSERTSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING:
        if(!*plparam) return 0;
        if ( WINPROC_TestCBForStr( hwnd ))
        {
            int len = WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)*plparam, -1, NULL, 0, 0, 0);
            LPSTR buf = HeapAlloc(GetProcessHeap(), 0, len);
            if (buf) WideCharToMultiByte(CP_ACP, 0, (LPCWSTR)*plparam, -1, buf, len, 0, 0);
            *plparam = (LPARAM)buf;
        }
        return (*plparam ? 1 : -1);

    case CB_GETLBTEXT:          /* FIXME: fixed sized buffer */
        { if ( WINPROC_TestCBForStr( hwnd ))
          { LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, 512 + sizeof(LPARAM) );
            if (!ptr) return -1;
            *ptr++ = *plparam;  /* Store previous lParam */
            *plparam = (LPARAM)ptr;
          }
        }
        return 1;

/* Multiline edit */
    case EM_GETLINE:
        { WORD len = (WORD)*plparam;
          LPARAM *ptr = HeapAlloc( GetProcessHeap(), 0, sizeof(LPARAM) + sizeof (WORD) + len*sizeof(CHAR) );
          if (!ptr) return -1;
          *ptr++ = *plparam;  /* Store previous lParam */
          *((WORD *) ptr) = len;   /* Store the length */
          *plparam = (LPARAM)ptr;
        }
        return 1;

    case WM_CHARTOITEM:
    case WM_MENUCHAR:
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
    case EM_SETPASSWORDCHAR:
        {
            WCHAR wch = LOWORD(*pwparam);
            BYTE ch;
            WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)&ch, 1, NULL, NULL );
            *pwparam = MAKEWPARAM( ch, HIWORD(*pwparam) );
        }
        return 0;

    case WM_IME_CHAR:
        {
            WCHAR wch = LOWORD(*pwparam);
            BYTE ch[2];

            if (WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)ch, 2, NULL, NULL ) == 2)
                *pwparam = MAKEWPARAM( (ch[0] << 8) | ch[1], HIWORD(*pwparam) );
            else
                *pwparam = MAKEWPARAM( ch[0], HIWORD(*pwparam) );
        }
        return 0;

    case WM_PAINTCLIPBOARD:
    case WM_SIZECLIPBOARD:
        FIXME_(msg)("message %s (%04x) needs translation, please report\n",SPY_GetMsgName(msg, hwnd),msg );
        return -1;
    default:  /* No translation needed */
        return 0;
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg32WTo32A
 *
 * Unmap a message that was mapped from Unicode to Ansi.
 */
static LRESULT WINPROC_UnmapMsg32WTo32A( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LRESULT result )
{
    switch(msg)
    {
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            if (!wParam) result = 0;
            else if (!(result = MultiByteToWideChar( CP_ACP, 0, (LPSTR)lParam, -1,
                                                     (LPWSTR)*ptr, wParam )))
            {
                ((LPWSTR)*ptr)[wParam-1] = 0;
                result = wParam - 1;
            }
            else result--;  /* do not count terminating null */
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;

    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case CB_DIR:
    case LB_DIR:
    case LB_ADDFILE:
    case EM_REPLACESEL:
        HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

    case WM_MDICREATE:
        {
            MDICREATESTRUCTA *cs = (MDICREATESTRUCTA *)lParam;
            if (HIWORD(cs->szTitle))
                HeapFree( GetProcessHeap(), 0, (LPVOID)cs->szTitle );
            if (HIWORD(cs->szClass))
                HeapFree( GetProcessHeap(), 0, (LPVOID)cs->szClass );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;

/* Listbox */
    case LB_ADDSTRING:
    case LB_INSERTSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_SELECTSTRING:
        if ( WINPROC_TestLBForStr( hwnd ))
          HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

    case LB_GETTEXT:
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            result = MultiByteToWideChar( CP_ACP, 0, (LPSTR)lParam, -1, (LPWSTR)*ptr, 0x7fffffff ) - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;

/* Combobox */
    case CB_ADDSTRING:
    case CB_INSERTSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_SELECTSTRING:
        if ( WINPROC_TestCBForStr( hwnd ))
          HeapFree( GetProcessHeap(), 0, (void *)lParam );
        break;

    case CB_GETLBTEXT:
        if ( result < 0) /* CB_ERR and CB_ERRSPACE */
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        else if ( WINPROC_TestCBForStr( hwnd ))
        {
            LPARAM *ptr = (LPARAM *)lParam - 1;
            result = MultiByteToWideChar( CP_ACP, 0, (LPSTR)lParam, -1, (LPWSTR)*ptr, 0x7fffffff ) - 1;
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;

/* Multiline edit */
    case EM_GETLINE:
        {
            LPARAM * ptr = (LPARAM *)lParam - 1;  /* get the old lparam */
            WORD len = *(WORD *)ptr;
            if (len)
            {
                result = MultiByteToWideChar( CP_ACP, 0, (LPSTR)lParam, result, (LPWSTR)*ptr, len );
                if (result < len) ((LPWSTR)*ptr)[result] = 0;
            }
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;
    }
    return result;
}

static UINT convert_handle_16_to_32(HANDLE16 src, unsigned int flags)
{
    HANDLE      dst;
    UINT        sz = GlobalSize16(src);
    LPSTR       ptr16, ptr32;

    if (!(dst = GlobalAlloc(flags, sz)))
        return 0;
    ptr16 = GlobalLock16(src);
    ptr32 = GlobalLock(dst);
    if (ptr16 != NULL && ptr32 != NULL) memcpy(ptr32, ptr16, sz);
    GlobalUnlock16(src);
    GlobalUnlock(dst);

    return (UINT)dst;
}

/**********************************************************************
 *	     WINPROC_MapMsg16To32A
 *
 * Map a message from 16- to 32-bit Ansi.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 */
INT WINPROC_MapMsg16To32A( HWND hwnd, UINT16 msg16, WPARAM16 wParam16, UINT *pmsg32,
                             WPARAM *pwparam32, LPARAM *plparam )
{
    *pmsg32 = (UINT)msg16;
    *pwparam32 = (WPARAM)wParam16;
    switch(msg16)
    {
    case WM_ACTIVATE:
    case WM_CHARTOITEM:
    case WM_COMMAND:
    case WM_VKEYTOITEM:
        *pwparam32 = MAKEWPARAM( wParam16, HIWORD(*plparam) );
        *plparam   = (LPARAM)WIN_Handle32( LOWORD(*plparam) );
        return 0;
    case WM_HSCROLL:
    case WM_VSCROLL:
        *pwparam32 = MAKEWPARAM( wParam16, LOWORD(*plparam) );
        *plparam   = (LPARAM)WIN_Handle32( HIWORD(*plparam) );
        return 0;
    case WM_CTLCOLOR:
        if ( HIWORD(*plparam) > CTLCOLOR_STATIC ) return -1;
        *pmsg32    = WM_CTLCOLORMSGBOX + HIWORD(*plparam);
        *pwparam32 = (WPARAM)HDC_32(wParam16);
        *plparam   = (LPARAM)WIN_Handle32( LOWORD(*plparam) );
        return 0;
    case WM_COMPAREITEM:
        {
            COMPAREITEMSTRUCT16* cis16 = MapSL(*plparam);
            COMPAREITEMSTRUCT *cis = HeapAlloc(GetProcessHeap(), 0, sizeof(*cis));
            if (!cis) return -1;
            cis->CtlType    = cis16->CtlType;
            cis->CtlID      = cis16->CtlID;
            cis->hwndItem   = WIN_Handle32( cis16->hwndItem );
            cis->itemID1    = cis16->itemID1;
            cis->itemData1  = cis16->itemData1;
            cis->itemID2    = cis16->itemID2;
            cis->itemData2  = cis16->itemData2;
            cis->dwLocaleId = 0;  /* FIXME */
            *plparam = (LPARAM)cis;
        }
        return 1;
    case WM_COPYDATA:
        {
            PCOPYDATASTRUCT16 pcds16 =  MapSL(*plparam);
            PCOPYDATASTRUCT pcds = HeapAlloc ( GetProcessHeap(), 0, sizeof(*pcds));
            pcds->dwData = pcds16->dwData;
            pcds->cbData = pcds16->cbData;
            pcds->lpData = MapSL( pcds16->lpData);
            *plparam = (LPARAM)pcds;
        }
        return 1;
    case WM_DELETEITEM:
        {
            DELETEITEMSTRUCT16* dis16 = MapSL(*plparam);
            DELETEITEMSTRUCT *dis = HeapAlloc(GetProcessHeap(), 0, sizeof(*dis));
            if (!dis) return -1;
            dis->CtlType  = dis16->CtlType;
            dis->CtlID    = dis16->CtlID;
            dis->hwndItem = WIN_Handle32( dis16->hwndItem );
            dis->itemData = dis16->itemData;
            *plparam = (LPARAM)dis;
        }
        return 1;
    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT16* mis16 = MapSL(*plparam);
            MEASUREITEMSTRUCT *mis = HeapAlloc(GetProcessHeap(), 0,
                                                sizeof(*mis) + sizeof(LPARAM));
            if (!mis) return -1;
            mis->CtlType    = mis16->CtlType;
            mis->CtlID      = mis16->CtlID;
            mis->itemID     = mis16->itemID;
            mis->itemWidth  = mis16->itemWidth;
            mis->itemHeight = mis16->itemHeight;
            mis->itemData   = mis16->itemData;
            *(LPARAM *)(mis + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)mis;
        }
        return 1;
    case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT16* dis16 = MapSL(*plparam);
            DRAWITEMSTRUCT *dis = HeapAlloc(GetProcessHeap(), 0, sizeof(*dis));
            if (!dis) return -1;
            dis->CtlType       = dis16->CtlType;
            dis->CtlID         = dis16->CtlID;
            dis->itemID        = dis16->itemID;
            dis->itemAction    = dis16->itemAction;
            dis->itemState     = dis16->itemState;
            dis->hwndItem      = (dis->CtlType == ODT_MENU) ? (HWND)HMENU_32(dis16->hwndItem)
                                                            : WIN_Handle32( dis16->hwndItem );
            dis->hDC           = HDC_32(dis16->hDC);
            dis->itemData      = dis16->itemData;
            dis->rcItem.left   = dis16->rcItem.left;
            dis->rcItem.top    = dis16->rcItem.top;
            dis->rcItem.right  = dis16->rcItem.right;
            dis->rcItem.bottom = dis16->rcItem.bottom;
            *plparam = (LPARAM)dis;
        }
        return 1;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mmi = HeapAlloc( GetProcessHeap(), 0, sizeof(*mmi) + sizeof(LPARAM));
            if (!mmi) return -1;
            MINMAXINFO16to32( MapSL(*plparam), mmi );
            *(LPARAM *)(mmi + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)mmi;
        }
        return 1;
    case WM_GETTEXT:
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case WM_ASKCBFORMATNAME:
        *plparam = (LPARAM)MapSL(*plparam);
        return 0;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs16 = MapSL(*plparam);
            MDICREATESTRUCTA *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) + sizeof(LPARAM) );
            if (!cs) return -1;
            MDICREATESTRUCT16to32A( cs16, cs );
            cs->szTitle = MapSL(cs16->szTitle);
            cs->szClass = MapSL(cs16->szClass);
            *(LPARAM *)(cs + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)cs;
        }
        return 1;
    case WM_MDIGETACTIVE:
        *plparam = (LPARAM)HeapAlloc( GetProcessHeap(), 0, sizeof(BOOL) );
        *(BOOL*)(*plparam) = 0;
        return 1;
    case WM_MDISETMENU:
        if(wParam16) *pmsg32=WM_MDIREFRESHMENU;
        *pwparam32 = (WPARAM)HMENU_32(LOWORD(*plparam));
        *plparam   = (LPARAM)HMENU_32(HIWORD(*plparam));
        return 0;
    case WM_MENUCHAR:
        *pwparam32 = MAKEWPARAM( wParam16, LOWORD(*plparam) );
        *plparam   = (LPARAM)HMENU_32(HIWORD(*plparam));
        return 0;
    case WM_MENUSELECT:
        if((LOWORD(*plparam) & MF_POPUP) && (LOWORD(*plparam) != 0xFFFF))
        {
            HMENU hmenu=HMENU_32(HIWORD(*plparam));
            UINT Pos=MENU_FindSubMenu( &hmenu, HMENU_32(wParam16));
            if(Pos==0xFFFF) Pos=0; /* NO_SELECTED_ITEM */
            *pwparam32 = MAKEWPARAM( Pos, LOWORD(*plparam) );
        }
        else *pwparam32 = MAKEWPARAM( wParam16, LOWORD(*plparam) );
        *plparam   = (LPARAM)HMENU_32(HIWORD(*plparam));
        return 0;
    case WM_MDIACTIVATE:
        if( *plparam )
        {
            *pwparam32 = (WPARAM)WIN_Handle32( HIWORD(*plparam) );
            *plparam   = (LPARAM)WIN_Handle32( LOWORD(*plparam) );
        }
        else /* message sent to MDI client */
            *pwparam32 = wParam16;
        return 0;
    case WM_NCCALCSIZE:
        {
            NCCALCSIZE_PARAMS16 *nc16;
            NCCALCSIZE_PARAMS *nc;

            nc = HeapAlloc( GetProcessHeap(), 0, sizeof(*nc) + sizeof(LPARAM) );
            if (!nc) return -1;
            nc16 = MapSL(*plparam);
            nc->rgrc[0].left   = nc16->rgrc[0].left;
            nc->rgrc[0].top    = nc16->rgrc[0].top;
            nc->rgrc[0].right  = nc16->rgrc[0].right;
            nc->rgrc[0].bottom = nc16->rgrc[0].bottom;
            if (wParam16)
            {
                nc->lppos = HeapAlloc( GetProcessHeap(), 0, sizeof(*nc->lppos) );
                nc->rgrc[1].left   = nc16->rgrc[1].left;
                nc->rgrc[1].top    = nc16->rgrc[1].top;
                nc->rgrc[1].right  = nc16->rgrc[1].right;
                nc->rgrc[1].bottom = nc16->rgrc[1].bottom;
                nc->rgrc[2].left   = nc16->rgrc[2].left;
                nc->rgrc[2].top    = nc16->rgrc[2].top;
                nc->rgrc[2].right  = nc16->rgrc[2].right;
                nc->rgrc[2].bottom = nc16->rgrc[2].bottom;
                if (nc->lppos) WINDOWPOS16to32( MapSL(nc16->lppos), nc->lppos );
            }
            *(LPARAM *)(nc + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)nc;
        }
        return 1;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs16 = MapSL(*plparam);
            CREATESTRUCTA *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) + sizeof(LPARAM) );
            if (!cs) return -1;
            CREATESTRUCT16to32A( cs16, cs );
            cs->lpszName  = MapSL(cs16->lpszName);
            cs->lpszClass = MapSL(cs16->lpszClass);

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16;
                MDICREATESTRUCTA *mdi_cs = HeapAlloc(GetProcessHeap(), 0, sizeof(*mdi_cs));
                if (!mdi_cs)
                {
                    HeapFree(GetProcessHeap(), 0, cs);
                    return -1;
                }
                mdi_cs16 = (MDICREATESTRUCT16 *)MapSL(cs16->lpCreateParams);
                MDICREATESTRUCT16to32A(mdi_cs16, mdi_cs);
                mdi_cs->szTitle = MapSL(mdi_cs16->szTitle);
                mdi_cs->szClass = MapSL(mdi_cs16->szClass);

                cs->lpCreateParams = mdi_cs;
            }
            *(LPARAM *)(cs + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)cs;
        }
        return 1;
    case WM_PARENTNOTIFY:
        if ((wParam16 == WM_CREATE) || (wParam16 == WM_DESTROY))
        {
            *pwparam32 = MAKEWPARAM( wParam16, HIWORD(*plparam) );
            *plparam   = (LPARAM)WIN_Handle32( LOWORD(*plparam) );
        }
        return 0;
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS *wp = HeapAlloc( GetProcessHeap(), 0, sizeof(*wp) + sizeof(LPARAM) );
            if (!wp) return -1;
            WINDOWPOS16to32( MapSL(*plparam), wp );
            *(LPARAM *)(wp + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)wp;
        }
        return 1;
    case WM_GETDLGCODE:
        if (*plparam)
        {
            LPMSG16 msg16 = MapSL(*plparam);
            LPMSG msg32 = HeapAlloc( GetProcessHeap(), 0, sizeof(MSG) );

            if (!msg32) return -1;
            msg32->hwnd = WIN_Handle32( msg16->hwnd );
            msg32->lParam = msg16->lParam;
            msg32->time = msg16->time;
            msg32->pt.x = msg16->pt.x;
            msg32->pt.y = msg16->pt.y;
            /* this is right, right? */
            if (WINPROC_MapMsg16To32A( msg32->hwnd, msg16->message,msg16->wParam,
                                     &msg32->message,&msg32->wParam,
                                     &msg32->lParam)<0) {
                HeapFree( GetProcessHeap(), 0, msg32 );
                return -1;
            }
            *plparam = (LPARAM)msg32;
            return 1;
        }
        else return 0;
    case WM_NOTIFY:
        *plparam = (LPARAM)MapSL(*plparam);
        return 0;
    case WM_ACTIVATEAPP:
        /* We need this when SetActiveWindow sends a Sendmessage16() to
         * a 32bit window. Might be superflous with 32bit interprocess
         * message queues. */
        if (*plparam) *plparam = HTASK_32( *plparam );
        return 0;
    case WM_NEXTMENU:
        {
            MDINEXTMENU *next = HeapAlloc( GetProcessHeap(), 0, sizeof(*next) );
            if (!next) return -1;
            next->hmenuIn = (HMENU)*plparam;
            next->hmenuNext = 0;
            next->hwndNext = 0;
            *plparam = (LPARAM)next;
            return 1;
        }
    case WM_PAINTCLIPBOARD:
    case WM_SIZECLIPBOARD:
        FIXME_(msg)("message %04x needs translation\n",msg16 );
        return -1;
    case WM_DDE_INITIATE:
    case WM_DDE_TERMINATE:
    case WM_DDE_UNADVISE:
    case WM_DDE_REQUEST:
        *pwparam32 = (WPARAM)WIN_Handle32(wParam16);
        return 0;
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
        {
            HANDLE16    lo16;
            ATOM        hi;
            UINT lo32 = 0;

            *pwparam32 = (WPARAM)WIN_Handle32(wParam16);
            lo16 = LOWORD(*plparam);
            hi = HIWORD(*plparam);
            if (lo16 && !(lo32 = convert_handle_16_to_32(lo16, GMEM_DDESHARE)))
                return -1;
            *plparam = PackDDElParam(msg16, lo32, hi);
        }
        return 0; /* FIXME don't know how to free allocated memory (handle)  !! */
    case WM_DDE_ACK:
        {
            UINT        lo, hi;
            int         flag = 0;
            char        buf[2];

            *pwparam32 = (WPARAM)WIN_Handle32(wParam16);

            lo = LOWORD(*plparam);
            hi = HIWORD(*plparam);

            if (GlobalGetAtomNameA(hi, buf, 2) > 0) flag |= 1;
            if (GlobalSize16(hi) != 0) flag |= 2;
            switch (flag)
            {
            case 0:
                if (hi)
                {
                    MESSAGE("DDE_ACK: neither atom nor handle!!!\n");
                    hi = 0;
                }
                break;
            case 1:
                break; /* atom, nothing to do */
            case 3:
                MESSAGE("DDE_ACK: %x both atom and handle... choosing handle\n", hi);
                /* fall thru */
            case 2:
                hi = convert_handle_16_to_32(hi, GMEM_DDESHARE);
                break;
            }
            *plparam = PackDDElParam(WM_DDE_ACK, lo, hi);
        }
        return 0; /* FIXME don't know how to free allocated memory (handle) !! */
    case WM_DDE_EXECUTE:
        *plparam = convert_handle_16_to_32(*plparam, GMEM_DDESHARE);
        return 0; /* FIXME don't know how to free allocated memory (handle) !! */
    default:  /* No translation needed */
        return 0;
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg16To32A
 *
 * Unmap a message that was mapped from 16- to 32-bit Ansi.
 */
LRESULT WINPROC_UnmapMsg16To32A( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                 LRESULT result )
{
    switch(msg)
    {
    case WM_COMPAREITEM:
    case WM_DELETEITEM:
    case WM_DRAWITEM:
    case WM_COPYDATA:
        HeapFree( GetProcessHeap(), 0, (LPVOID)lParam );
        break;
    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT16 *mis16;
            MEASUREITEMSTRUCT *mis = (MEASUREITEMSTRUCT *)lParam;
            lParam = *(LPARAM *)(mis + 1);
            mis16 = MapSL(lParam);
            mis16->itemWidth  = (UINT16)mis->itemWidth;
            mis16->itemHeight = (UINT16)mis->itemHeight;
            HeapFree( GetProcessHeap(), 0, mis );
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            lParam = *(LPARAM *)(mmi + 1);
            MINMAXINFO32to16( mmi, MapSL(lParam));
            HeapFree( GetProcessHeap(), 0, mmi );
        }
        break;
    case WM_MDICREATE:
        {
            MDICREATESTRUCTA *cs = (MDICREATESTRUCTA *)lParam;
            lParam = *(LPARAM *)(cs + 1);
            MDICREATESTRUCT32Ato16( cs, MapSL(lParam) );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_MDIGETACTIVE:
        result = MAKELONG( LOWORD(result), (BOOL16)(*(BOOL *)lParam) );
        HeapFree( GetProcessHeap(), 0, (BOOL *)lParam );
        break;
    case WM_NCCALCSIZE:
        {
            NCCALCSIZE_PARAMS16 *nc16;
            NCCALCSIZE_PARAMS *nc = (NCCALCSIZE_PARAMS *)lParam;
            lParam = *(LPARAM *)(nc + 1);
            nc16 = MapSL(lParam);
            nc16->rgrc[0].left   = nc->rgrc[0].left;
            nc16->rgrc[0].top    = nc->rgrc[0].top;
            nc16->rgrc[0].right  = nc->rgrc[0].right;
            nc16->rgrc[0].bottom = nc->rgrc[0].bottom;
            if (wParam)
            {
                nc16->rgrc[1].left   = nc->rgrc[1].left;
                nc16->rgrc[1].top    = nc->rgrc[1].top;
                nc16->rgrc[1].right  = nc->rgrc[1].right;
                nc16->rgrc[1].bottom = nc->rgrc[1].bottom;
                nc16->rgrc[2].left   = nc->rgrc[2].left;
                nc16->rgrc[2].top    = nc->rgrc[2].top;
                nc16->rgrc[2].right  = nc->rgrc[2].right;
                nc16->rgrc[2].bottom = nc->rgrc[2].bottom;
                if (nc->lppos)
                {
                    WINDOWPOS32to16( nc->lppos, MapSL(nc16->lppos));
                    HeapFree( GetProcessHeap(), 0, nc->lppos );
                }
            }
            HeapFree( GetProcessHeap(), 0, nc );
        }
        break;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCTA *cs = (CREATESTRUCTA *)lParam;
            lParam = *(LPARAM *)(cs + 1);
            CREATESTRUCT32Ato16( cs, MapSL(lParam) );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
                HeapFree(GetProcessHeap(), 0, cs->lpCreateParams);

            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS *wp = (WINDOWPOS *)lParam;
            lParam = *(LPARAM *)(wp + 1);
            WINDOWPOS32to16(wp, MapSL(lParam));
            HeapFree( GetProcessHeap(), 0, wp );
        }
        break;
    case WM_GETDLGCODE:
        if (lParam)
        {
            LPMSG msg32 = (LPMSG)lParam;

            WINPROC_UnmapMsg16To32A( hwnd, msg32->message, msg32->wParam, msg32->lParam,
                                     result);
            HeapFree( GetProcessHeap(), 0, msg32 );
        }
        break;
    case WM_NEXTMENU:
        {
            MDINEXTMENU *next = (MDINEXTMENU *)lParam;
            result = MAKELONG( HMENU_16(next->hmenuNext), HWND_16(next->hwndNext) );
            HeapFree( GetProcessHeap(), 0, next );
        }
        break;
    }
    return result;
}


/**********************************************************************
 *	     WINPROC_MapMsg16To32W
 *
 * Map a message from 16- to 32-bit Unicode.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 */
INT WINPROC_MapMsg16To32W( HWND hwnd, UINT16 msg16, WPARAM16 wParam16, UINT *pmsg32,
                           WPARAM *pwparam32, LPARAM *plparam )
{
    CHAR ch;
    WCHAR wch;

    *pmsg32=(UINT)msg16;
    *pwparam32 = (WPARAM)wParam16;
    switch(msg16)
    {
    case WM_GETTEXT:
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
    case WM_ASKCBFORMATNAME:
        *plparam = (LPARAM)MapSL(*plparam);
        return WINPROC_MapMsg32ATo32W( hwnd, *pmsg32, pwparam32, plparam );
    case WM_GETTEXTLENGTH:
    case CB_GETLBTEXTLEN:
    case LB_GETTEXTLEN:
        return 1;  /* need to map result */
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs16 = MapSL(*plparam);
            CREATESTRUCTW *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) + sizeof(LPARAM) );
            if (!cs) return -1;
            CREATESTRUCT16to32A( cs16, (CREATESTRUCTA *)cs );
            cs->lpszName  = map_str_16_to_32W(cs16->lpszName);
            cs->lpszClass = map_str_16_to_32W(cs16->lpszClass);

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16;
                MDICREATESTRUCTW *mdi_cs = HeapAlloc(GetProcessHeap(), 0, sizeof(*mdi_cs));
                if (!mdi_cs)
                {
                    HeapFree(GetProcessHeap(), 0, cs);
                    return -1;
                }
                mdi_cs16 = (MDICREATESTRUCT16 *)MapSL(cs16->lpCreateParams);
                MDICREATESTRUCT16to32A(mdi_cs16, (MDICREATESTRUCTA *)mdi_cs);
                mdi_cs->szTitle = map_str_16_to_32W(mdi_cs16->szTitle);
                mdi_cs->szClass = map_str_16_to_32W(mdi_cs16->szClass);

                cs->lpCreateParams = mdi_cs;
            }
            *(LPARAM *)(cs + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)cs;
        }
        return 1;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs16 = MapSL(*plparam);
            MDICREATESTRUCTW *cs = HeapAlloc( GetProcessHeap(), 0, sizeof(*cs) + sizeof(LPARAM) );
            if (!cs) return -1;
            MDICREATESTRUCT16to32A( cs16, (MDICREATESTRUCTA *)cs );
            cs->szTitle = map_str_16_to_32W(cs16->szTitle);
            cs->szClass = map_str_16_to_32W(cs16->szClass);
            *(LPARAM *)(cs + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)cs;
        }
        return 1;
    case WM_GETDLGCODE:
        if (*plparam)
        {
            LPMSG16 msg16 = MapSL(*plparam);
            LPMSG msg32 = HeapAlloc( GetProcessHeap(), 0, sizeof(MSG) );

            if (!msg32) return -1;
            msg32->hwnd = WIN_Handle32( msg16->hwnd );
            msg32->lParam = msg16->lParam;
            msg32->time = msg16->time;
            msg32->pt.x = msg16->pt.x;
            msg32->pt.y = msg16->pt.y;
            /* this is right, right? */
            if (WINPROC_MapMsg16To32W(hwnd, msg16->message,msg16->wParam,
                                     &msg32->message,&msg32->wParam,
                                     &msg32->lParam)<0) {
                HeapFree( GetProcessHeap(), 0, msg32 );
                return -1;
            }
            *plparam = (LPARAM)msg32;
            return 1;
        }
        else return 0;

    case WM_CHARTOITEM:
        ch = wParam16;
        MultiByteToWideChar( CP_ACP, 0, &ch, 1, &wch, 1);
        *pwparam32 = MAKEWPARAM( wch, HIWORD(*plparam) );
        *plparam   = (LPARAM)WIN_Handle32( LOWORD(*plparam) );
        return 0;
    case WM_MENUCHAR:
        ch = wParam16;
        MultiByteToWideChar( CP_ACP, 0, &ch, 1, &wch, 1);
        *pwparam32 = MAKEWPARAM( wch, LOWORD(*plparam) );
        *plparam   = (LPARAM)HMENU_32(HIWORD(*plparam));
        return 0;
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
        ch = wParam16;
        MultiByteToWideChar( CP_ACP, 0, &ch, 1, &wch, 1);
        *pwparam32 = wch;
        return 0;
    case WM_IME_CHAR:
        return WINPROC_MapMsg32ATo32W( hwnd, *pmsg32, pwparam32, plparam );

    default:  /* No Unicode translation needed */
        return WINPROC_MapMsg16To32A( hwnd, msg16, wParam16, pmsg32,
                                      pwparam32, plparam );
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg16To32W
 *
 * Unmap a message that was mapped from 16- to 32-bit Unicode.
 */
LRESULT WINPROC_UnmapMsg16To32W( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                 LRESULT result, WNDPROC dispatch )
{
    switch(msg)
    {
    case WM_GETTEXT:
    case WM_SETTEXT:
    case WM_GETTEXTLENGTH:
    case CB_GETLBTEXTLEN:
    case LB_GETTEXTLEN:
    case WM_ASKCBFORMATNAME:
        return WINPROC_UnmapMsg32ATo32W( hwnd, msg, wParam, lParam, result, dispatch );
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCTW *cs = (CREATESTRUCTW *)lParam;
            lParam = *(LPARAM *)(cs + 1);
            CREATESTRUCT32Ato16( (CREATESTRUCTA *)cs, MapSL(lParam) );
            unmap_str_16_to_32W( cs->lpszName );
            unmap_str_16_to_32W( cs->lpszClass );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCTW *mdi_cs = (MDICREATESTRUCTW *)cs->lpCreateParams;
                unmap_str_16_to_32W( mdi_cs->szTitle );
                unmap_str_16_to_32W( mdi_cs->szClass );
                HeapFree(GetProcessHeap(), 0, cs->lpCreateParams);
            }
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_MDICREATE:
        {
            MDICREATESTRUCTW *cs = (MDICREATESTRUCTW *)lParam;
            lParam = *(LPARAM *)(cs + 1);
            MDICREATESTRUCT32Ato16( (MDICREATESTRUCTA *)cs, MapSL(lParam) );
            unmap_str_16_to_32W( cs->szTitle );
            unmap_str_16_to_32W( cs->szClass );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_GETDLGCODE:
        if (lParam)
        {
            LPMSG msg32 = (LPMSG)lParam;

            WINPROC_UnmapMsg16To32W( hwnd, msg32->message, msg32->wParam, msg32->lParam,
                                     result, dispatch );
            HeapFree( GetProcessHeap(), 0, msg32 );
        }
        break;
    default:
        return WINPROC_UnmapMsg16To32A( hwnd, msg, wParam, lParam, result );
    }
    return result;
}

static HANDLE16 convert_handle_32_to_16(UINT src, unsigned int flags)
{
    HANDLE16    dst;
    UINT        sz = GlobalSize((HANDLE)src);
    LPSTR       ptr16, ptr32;

    if (!(dst = GlobalAlloc16(flags, sz)))
        return 0;
    ptr32 = GlobalLock((HANDLE)src);
    ptr16 = GlobalLock16(dst);
    if (ptr16 != NULL && ptr32 != NULL) memcpy(ptr16, ptr32, sz);
    GlobalUnlock((HANDLE)src);
    GlobalUnlock16(dst);

    return dst;
}


/**********************************************************************
 *	     WINPROC_MapMsg32ATo16
 *
 * Map a message from 32-bit Ansi to 16-bit.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 */
INT WINPROC_MapMsg32ATo16( HWND hwnd, UINT msg32, WPARAM wParam32,
                             UINT16 *pmsg16, WPARAM16 *pwparam16,
                             LPARAM *plparam )
{
    *pmsg16 = (UINT16)msg32;
    *pwparam16 = (WPARAM16)LOWORD(wParam32);
    switch(msg32)
    {
    case SBM_SETRANGE:
        *pmsg16 = SBM_SETRANGE16;
        *plparam = MAKELPARAM(wParam32, *plparam);
        *pwparam16 = 0;
        return 0;

    case SBM_GETRANGE:
        *pmsg16 = SBM_GETRANGE16;
        return 1;

    case BM_GETCHECK:
    case BM_SETCHECK:
    case BM_GETSTATE:
    case BM_SETSTATE:
    case BM_SETSTYLE:
        *pmsg16 = (UINT16)msg32 + (BM_GETCHECK16 - BM_GETCHECK);
        return 0;

    case EM_GETSEL:
    case EM_GETRECT:
    case EM_SETRECT:
    case EM_SETRECTNP:
    case EM_SCROLL:
    case EM_LINESCROLL:
    case EM_SCROLLCARET:
    case EM_GETMODIFY:
    case EM_SETMODIFY:
    case EM_GETLINECOUNT:
    case EM_LINEINDEX:
    case EM_SETHANDLE:
    case EM_GETHANDLE:
    case EM_GETTHUMB:
    case EM_LINELENGTH:
    case EM_REPLACESEL:
    case EM_GETLINE:
    case EM_LIMITTEXT:
    case EM_CANUNDO:
    case EM_UNDO:
    case EM_FMTLINES:
    case EM_LINEFROMCHAR:
    case EM_SETTABSTOPS:
    case EM_SETPASSWORDCHAR:
    case EM_EMPTYUNDOBUFFER:
    case EM_GETFIRSTVISIBLELINE:
    case EM_SETREADONLY:
    case EM_SETWORDBREAKPROC:
    case EM_GETWORDBREAKPROC:
    case EM_GETPASSWORDCHAR:
        *pmsg16 = (UINT16)msg32 + (EM_GETSEL16 - EM_GETSEL);
        return 0;

    case LB_CARETOFF:
    case LB_CARETON:
    case LB_DELETESTRING:
    case LB_GETANCHORINDEX:
    case LB_GETCARETINDEX:
    case LB_GETCOUNT:
    case LB_GETCURSEL:
    case LB_GETHORIZONTALEXTENT:
    case LB_GETITEMDATA:
    case LB_GETITEMHEIGHT:
    case LB_GETSEL:
    case LB_GETSELCOUNT:
    case LB_GETTEXTLEN:
    case LB_GETTOPINDEX:
    case LB_RESETCONTENT:
    case LB_SELITEMRANGE:
    case LB_SELITEMRANGEEX:
    case LB_SETANCHORINDEX:
    case LB_SETCARETINDEX:
    case LB_SETCOLUMNWIDTH:
    case LB_SETCURSEL:
    case LB_SETHORIZONTALEXTENT:
    case LB_SETITEMDATA:
    case LB_SETITEMHEIGHT:
    case LB_SETSEL:
    case LB_SETTOPINDEX:
        *pmsg16 = (UINT16)msg32 + (LB_ADDSTRING16 - LB_ADDSTRING);
        return 0;
    case CB_DELETESTRING:
    case CB_GETCOUNT:
    case CB_GETLBTEXTLEN:
    case CB_LIMITTEXT:
    case CB_RESETCONTENT:
    case CB_SETEDITSEL:
    case CB_GETCURSEL:
    case CB_SETCURSEL:
    case CB_SHOWDROPDOWN:
    case CB_SETITEMDATA:
    case CB_SETITEMHEIGHT:
    case CB_GETITEMHEIGHT:
    case CB_SETEXTENDEDUI:
    case CB_GETEXTENDEDUI:
    case CB_GETDROPPEDSTATE:
        *pmsg16 = (UINT16)msg32 + (CB_GETEDITSEL16 - CB_GETEDITSEL);
        return 0;
    case CB_GETEDITSEL:
        *pmsg16 = CB_GETEDITSEL16;
        return 1;

    case LB_ADDSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_INSERTSTRING:
    case LB_SELECTSTRING:
    case LB_DIR:
    case LB_ADDFILE:
        *plparam = (LPARAM)MapLS( (LPSTR)*plparam );
        *pmsg16 = (UINT16)msg32 + (LB_ADDSTRING16 - LB_ADDSTRING);
        return 1;

    case CB_ADDSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_INSERTSTRING:
    case CB_SELECTSTRING:
    case CB_DIR:
        *plparam = (LPARAM)MapLS( (LPSTR)*plparam );
        *pmsg16 = (UINT16)msg32 + (CB_GETEDITSEL16 - CB_GETEDITSEL);
        return 1;

    case LB_GETITEMRECT:
        {
            RECT16 *rect = HeapAlloc( GetProcessHeap(), 0, sizeof(RECT16) + sizeof(LPARAM) );
            if (!rect) return -1;
            *(LPARAM *)(rect + 1) = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( rect );
        }
        *pmsg16 = LB_GETITEMRECT16;
        return 1;
    case LB_GETSELITEMS:
        {
            LPARAM *items; /* old LPARAM first, then *pwparam16 x INT16 entries */

            *pwparam16 = (WPARAM16)min( wParam32, 0x7f80 ); /* Must be < 64K */
            if (!(items = HeapAlloc( GetProcessHeap(), 0,
                                     *pwparam16 * sizeof(INT16) + sizeof(LPARAM)))) return -1;
            *items++ = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( items );
        }
        *pmsg16 = LB_GETSELITEMS16;
        return 1;
    case LB_SETTABSTOPS:
        if (wParam32)
        {
            INT i;
            LPINT16 stops;
            *pwparam16 = (WPARAM16)min( wParam32, 0x7f80 ); /* Must be < 64K */
            if (!(stops = HeapAlloc( GetProcessHeap(), 0,
                                     *pwparam16 * sizeof(INT16) + sizeof(LPARAM)))) return -1;
            for (i = 0; i < *pwparam16; i++) stops[i] = *((LPINT)*plparam+i);
            *plparam = MapLS( stops );
            return 1;
        }
        *pmsg16 = LB_SETTABSTOPS16;
        return 0;

    case CB_GETDROPPEDCONTROLRECT:
        {
            RECT16 *rect = HeapAlloc( GetProcessHeap(), 0, sizeof(RECT16) + sizeof(LPARAM) );
            if (!rect) return -1;
            *(LPARAM *)(rect + 1) = *plparam;  /* Store the previous lParam */
            *plparam = (LPARAM)MapLS(rect);
        }
        *pmsg16 = CB_GETDROPPEDCONTROLRECT16;
        return 1;

    case LB_GETTEXT:
        *plparam = (LPARAM)MapLS( (LPVOID)(*plparam) );
        *pmsg16 = LB_GETTEXT16;
        return 1;

    case CB_GETLBTEXT:
        *plparam = (LPARAM)MapLS( (LPVOID)(*plparam) );
        *pmsg16 = CB_GETLBTEXT16;
        return 1;

    case EM_SETSEL:
        *pwparam16 = 0;
        *plparam = MAKELONG( (INT16)(INT)wParam32, (INT16)*plparam );
        *pmsg16 = EM_SETSEL16;
        return 0;

    case WM_ACTIVATE:
    case WM_CHARTOITEM:
    case WM_COMMAND:
    case WM_VKEYTOITEM:
        *plparam = MAKELPARAM( (HWND16)*plparam, HIWORD(wParam32) );
        return 0;
    case WM_HSCROLL:
    case WM_VSCROLL:
        *plparam = MAKELPARAM( HIWORD(wParam32), (HWND16)*plparam );
        return 0;
    case WM_COPYDATA:
        {
            PCOPYDATASTRUCT pcds32 = (PCOPYDATASTRUCT) *plparam;
            PCOPYDATASTRUCT16 pcds = HeapAlloc( GetProcessHeap(), 0, sizeof( *pcds));
            pcds->dwData = pcds32->dwData;
            pcds->cbData = pcds32->cbData;
            pcds->lpData = MapLS( pcds32->lpData);
            *plparam = MapLS( pcds );
        }
        return 1;
    case WM_CTLCOLORMSGBOX:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSCROLLBAR:
    case WM_CTLCOLORSTATIC:
        *pmsg16  = WM_CTLCOLOR;
        *plparam = MAKELPARAM( (HWND16)*plparam,
                               (WORD)msg32 - WM_CTLCOLORMSGBOX );
        return 0;
    case WM_COMPAREITEM:
        {
            COMPAREITEMSTRUCT *cis32 = (COMPAREITEMSTRUCT *)*plparam;
            COMPAREITEMSTRUCT16 *cis = HeapAlloc( GetProcessHeap(), 0, sizeof(COMPAREITEMSTRUCT16));
            if (!cis) return -1;
            cis->CtlType    = (UINT16)cis32->CtlType;
            cis->CtlID      = (UINT16)cis32->CtlID;
            cis->hwndItem   = HWND_16( cis32->hwndItem );
            cis->itemID1    = (UINT16)cis32->itemID1;
            cis->itemData1  = cis32->itemData1;
            cis->itemID2    = (UINT16)cis32->itemID2;
            cis->itemData2  = cis32->itemData2;
            *plparam = MapLS( cis );
        }
        return 1;
    case WM_DELETEITEM:
        {
            DELETEITEMSTRUCT *dis32 = (DELETEITEMSTRUCT *)*plparam;
            DELETEITEMSTRUCT16 *dis = HeapAlloc( GetProcessHeap(), 0, sizeof(DELETEITEMSTRUCT16) );
            if (!dis) return -1;
            dis->CtlType  = (UINT16)dis32->CtlType;
            dis->CtlID    = (UINT16)dis32->CtlID;
            dis->itemID   = (UINT16)dis32->itemID;
            dis->hwndItem = (dis->CtlType == ODT_MENU) ? (HWND16)LOWORD(dis32->hwndItem)
                                                       : HWND_16( dis32->hwndItem );
            dis->itemData = dis32->itemData;
            *plparam = MapLS( dis );
        }
        return 1;
    case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT *dis32 = (DRAWITEMSTRUCT *)*plparam;
            DRAWITEMSTRUCT16 *dis = HeapAlloc( GetProcessHeap(), 0, sizeof(DRAWITEMSTRUCT16) );
            if (!dis) return -1;
            dis->CtlType       = (UINT16)dis32->CtlType;
            dis->CtlID         = (UINT16)dis32->CtlID;
            dis->itemID        = (UINT16)dis32->itemID;
            dis->itemAction    = (UINT16)dis32->itemAction;
            dis->itemState     = (UINT16)dis32->itemState;
            dis->hwndItem      = HWND_16( dis32->hwndItem );
            dis->hDC           = HDC_16(dis32->hDC);
            dis->itemData      = dis32->itemData;
            dis->rcItem.left   = dis32->rcItem.left;
            dis->rcItem.top    = dis32->rcItem.top;
            dis->rcItem.right  = dis32->rcItem.right;
            dis->rcItem.bottom = dis32->rcItem.bottom;
            *plparam = MapLS( dis );
        }
        return 1;
    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT *mis32 = (MEASUREITEMSTRUCT *)*plparam;
            MEASUREITEMSTRUCT16 *mis = HeapAlloc( GetProcessHeap(), 0, sizeof(*mis)+sizeof(LPARAM));
            if (!mis) return -1;
            mis->CtlType    = (UINT16)mis32->CtlType;
            mis->CtlID      = (UINT16)mis32->CtlID;
            mis->itemID     = (UINT16)mis32->itemID;
            mis->itemWidth  = (UINT16)mis32->itemWidth;
            mis->itemHeight = (UINT16)mis32->itemHeight;
            mis->itemData   = mis32->itemData;
            *(LPARAM *)(mis + 1) = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( mis );
        }
        return 1;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO16 *mmi = HeapAlloc( GetProcessHeap(), 0, sizeof(*mmi) + sizeof(LPARAM) );
            if (!mmi) return -1;
            MINMAXINFO32to16( (MINMAXINFO *)*plparam, mmi );
            *(LPARAM *)(mmi + 1) = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( mmi );
        }
        return 1;
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPARAM *str; /* store LPARAM, then *pwparam16 char space */
            *pwparam16 = (WPARAM16)min( wParam32, 0xff80 ); /* Must be < 64K */
            if (!(str = HeapAlloc( GetProcessHeap(), 0, *pwparam16 + sizeof(LPARAM)))) return -1;
            *str++ = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( str );
        }
        return 1;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs;
            MDICREATESTRUCTA *cs32 = (MDICREATESTRUCTA *)*plparam;

            if (!(cs = HeapAlloc( GetProcessHeap(), 0, sizeof(MDICREATESTRUCT16) ))) return -1;
            MDICREATESTRUCT32Ato16( cs32, cs );
            cs->szTitle = MapLS( cs32->szTitle );
            cs->szClass = MapLS( cs32->szClass );
            *plparam = MapLS( cs );
        }
        return 1;
    case WM_MDIGETACTIVE:
        return 1;
    case WM_MDISETMENU:
        *plparam   = MAKELPARAM( (HMENU16)LOWORD(wParam32),
                                 (HMENU16)LOWORD(*plparam) );
        *pwparam16 = (*plparam == 0);
        return 0;
    case WM_MENUSELECT:
        if(HIWORD(wParam32) & MF_POPUP)
        {
            HMENU hmenu;
            if (((UINT)HIWORD(wParam32) != 0xFFFF) || (*plparam))
            {
                if((hmenu = GetSubMenu((HMENU)*plparam, *pwparam16)))
                    *pwparam16=HMENU_16(hmenu);
            }
        }
        /* fall through */
    case WM_MENUCHAR:
        *plparam = MAKELPARAM( HIWORD(wParam32), (HMENU16)*plparam );
        return 0;
    case WM_MDIACTIVATE:
        if (GetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_MDICHILD)
        {
            *pwparam16 = ((HWND)*plparam == hwnd);
            *plparam = MAKELPARAM( (HWND16)LOWORD(*plparam),
                                   (HWND16)LOWORD(wParam32) );
        }
        else
        {
            *pwparam16 = HWND_16( (HWND)wParam32 );
            *plparam = 0;
        }
        return 0;
    case WM_NCCALCSIZE:
        {
            NCCALCSIZE_PARAMS *nc32 = (NCCALCSIZE_PARAMS *)*plparam;
            NCCALCSIZE_PARAMS16 *nc = HeapAlloc( GetProcessHeap(), 0, sizeof(*nc) + sizeof(LPARAM));
            if (!nc) return -1;

            nc->rgrc[0].left   = nc32->rgrc[0].left;
            nc->rgrc[0].top    = nc32->rgrc[0].top;
            nc->rgrc[0].right  = nc32->rgrc[0].right;
            nc->rgrc[0].bottom = nc32->rgrc[0].bottom;
            if (wParam32)
            {
                WINDOWPOS16 *wp;
                nc->rgrc[1].left   = nc32->rgrc[1].left;
                nc->rgrc[1].top    = nc32->rgrc[1].top;
                nc->rgrc[1].right  = nc32->rgrc[1].right;
                nc->rgrc[1].bottom = nc32->rgrc[1].bottom;
                nc->rgrc[2].left   = nc32->rgrc[2].left;
                nc->rgrc[2].top    = nc32->rgrc[2].top;
                nc->rgrc[2].right  = nc32->rgrc[2].right;
                nc->rgrc[2].bottom = nc32->rgrc[2].bottom;
                if (!(wp = HeapAlloc( GetProcessHeap(), 0, sizeof(WINDOWPOS16) )))
                {
                    HeapFree( GetProcessHeap(), 0, nc );
                    return -1;
                }
                WINDOWPOS32to16( nc32->lppos, wp );
                nc->lppos = MapLS( wp );
            }
            *(LPARAM *)(nc + 1) = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( nc );
        }
        return 1;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs;
            CREATESTRUCTA *cs32 = (CREATESTRUCTA *)*plparam;

            if (!(cs = HeapAlloc( GetProcessHeap(), 0, sizeof(CREATESTRUCT16) ))) return -1;
            CREATESTRUCT32Ato16( cs32, cs );
            cs->lpszName  = MapLS( cs32->lpszName );
            cs->lpszClass = MapLS( cs32->lpszClass );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16;
                MDICREATESTRUCTA *mdi_cs = (MDICREATESTRUCTA *)cs32->lpCreateParams;
                mdi_cs16 = HeapAlloc(GetProcessHeap(), 0, sizeof(*mdi_cs16));
                if (!mdi_cs16)
                {
                    HeapFree(GetProcessHeap(), 0, cs);
                    return -1;
                }
                MDICREATESTRUCT32Ato16(mdi_cs, mdi_cs16);
                mdi_cs16->szTitle = MapLS( mdi_cs->szTitle );
                mdi_cs16->szClass = MapLS( mdi_cs->szClass );
                cs->lpCreateParams = MapLS( mdi_cs16 );
            }
            *plparam = MapLS( cs );
        }
        return 1;
    case WM_PARENTNOTIFY:
        if ((LOWORD(wParam32)==WM_CREATE) || (LOWORD(wParam32)==WM_DESTROY))
            *plparam = MAKELPARAM( (HWND16)*plparam, HIWORD(wParam32));
        /* else nothing to do */
        return 0;
    case WM_NOTIFY:
        *plparam = MapLS( (NMHDR *)*plparam ); /* NMHDR is already 32-bit */
        return 1;
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
        *plparam = MapLS( (LPSTR)*plparam );
        return 1;
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS16 *wp = HeapAlloc( GetProcessHeap(), 0, sizeof(*wp) + sizeof(LPARAM) );
            if (!wp) return -1;
            WINDOWPOS32to16( (WINDOWPOS *)*plparam, wp );
            *(LPARAM *)(wp + 1) = *plparam;  /* Store the previous lParam */
            *plparam = MapLS( wp );
        }
        return 1;
    case WM_GETDLGCODE:
         if (*plparam) {
            LPMSG msg32 = (LPMSG) *plparam;
            LPMSG16 msg16 = HeapAlloc( GetProcessHeap(), 0, sizeof(MSG16) );

            if (!msg16) return -1;
            msg16->hwnd = HWND_16( msg32->hwnd );
            msg16->lParam = msg32->lParam;
            msg16->time = msg32->time;
            msg16->pt.x = msg32->pt.x;
            msg16->pt.y = msg32->pt.y;
            /* this is right, right? */
            if (WINPROC_MapMsg32ATo16(msg32->hwnd,msg32->message,msg32->wParam,
                         &msg16->message,&msg16->wParam, &msg16->lParam)<0)
            {
                HeapFree( GetProcessHeap(), 0, msg16 );
                return -1;
            }
            *plparam = MapLS( msg16 );
            return 1;
        }
        return 0;

    case WM_ACTIVATEAPP:
        if (*plparam) *plparam = HTASK_16( (HANDLE)*plparam );
        return 0;
    case WM_NEXTMENU:
        {
            MDINEXTMENU *next = (MDINEXTMENU *)*plparam;
            *plparam = (LPARAM)next->hmenuIn;
            return 1;
        }
    case WM_PAINT:
        if (IsIconic( hwnd ) && GetClassLongPtrW( hwnd, GCLP_HICON ))
        {
            *pmsg16 = WM_PAINTICON;
            *pwparam16 = 1;
        }
        return 0;
    case WM_ERASEBKGND:
        if (IsIconic( hwnd ) && GetClassLongPtrW( hwnd, GCLP_HICON ))
            *pmsg16 = WM_ICONERASEBKGND;
        return 0;
    case WM_PAINTCLIPBOARD:
    case WM_SIZECLIPBOARD:
        FIXME_(msg)("message %04x needs translation\n", msg32 );
        return -1;
    /* following messages should not be sent to 16-bit apps */
    case WM_SIZING:
    case WM_MOVING:
    case WM_CAPTURECHANGED:
    case WM_STYLECHANGING:
    case WM_STYLECHANGED:
        return -1;
    case WM_DDE_INITIATE:
    case WM_DDE_TERMINATE:
    case WM_DDE_UNADVISE:
    case WM_DDE_REQUEST:
        *pwparam16 = HWND_16((HWND)wParam32);
        return 0;
    case WM_DDE_ADVISE:
    case WM_DDE_DATA:
    case WM_DDE_POKE:
        {
            UINT_PTR lo32, hi;
            HANDLE16    lo16 = 0;

            *pwparam16 = HWND_16((HWND)wParam32);
            UnpackDDElParam(msg32, *plparam, &lo32, &hi);
            if (lo32 && !(lo16 = convert_handle_32_to_16(lo32, GMEM_DDESHARE)))
                return -1;
            *plparam = MAKELPARAM(lo16, hi);
        }
        return 0; /* FIXME don't know how to free allocated memory (handle)  !! */
    case WM_DDE_ACK:
        {
            UINT_PTR    lo, hi;
            int         flag = 0;
            char        buf[2];

            *pwparam16 = HWND_16((HWND)wParam32);

            UnpackDDElParam(msg32, *plparam, &lo, &hi);

            if (GlobalGetAtomNameA((ATOM)hi, buf, sizeof(buf)) > 0) flag |= 1;
            if (GlobalSize((HANDLE)hi) != 0) flag |= 2;
            switch (flag)
            {
            case 0:
                if (hi)
                {
                    MESSAGE("DDE_ACK: neither atom nor handle!!!\n");
                    hi = 0;
                }
                break;
            case 1:
                break; /* atom, nothing to do */
            case 3:
                MESSAGE("DDE_ACK: %x both atom and handle... choosing handle\n", hi);
                /* fall thru */
            case 2:
                hi = convert_handle_32_to_16(hi, GMEM_DDESHARE);
                break;
            }
            *plparam = MAKELPARAM(lo, hi);
        }
        return 0; /* FIXME don't know how to free allocated memory (handle) !! */
    case WM_DDE_EXECUTE:
        *plparam = convert_handle_32_to_16(*plparam, GMEM_DDESHARE);
        return 0; /* FIXME don't know how to free allocated memory (handle) !! */
    default:  /* No translation needed */
        return 0;
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg32ATo16
 *
 * Unmap a message that was mapped from 32-bit Ansi to 16-bit.
 */
void WINPROC_UnmapMsg32ATo16( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                              MSGPARAM16* p16 )
{
    switch(msg)
    {
    case SBM_GETRANGE:
        *(LPINT)wParam = LOWORD(p16->lResult);
        *(LPINT)lParam = HIWORD(p16->lResult);
        break;

    case LB_ADDFILE:
    case LB_ADDSTRING:
    case LB_DIR:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_INSERTSTRING:
    case LB_SELECTSTRING:
    case LB_GETTEXT:
    case CB_ADDSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_INSERTSTRING:
    case CB_SELECTSTRING:
    case CB_DIR:
    case CB_GETLBTEXT:
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
        UnMapLS( (SEGPTR)p16->lParam );
        break;
    case LB_SETTABSTOPS:
    case WM_COMPAREITEM:
    case WM_DELETEITEM:
    case WM_DRAWITEM:
        {
            void *ptr = MapSL( p16->lParam );
            UnMapLS( p16->lParam );
            HeapFree( GetProcessHeap(), 0, ptr );
        }
        break;
    case WM_COPYDATA:
        {
            PCOPYDATASTRUCT16 pcds = MapSL( p16->lParam );
            UnMapLS( p16->lParam );
            UnMapLS( pcds->lpData );
            HeapFree( GetProcessHeap(), 0, pcds );
        }
        break;
    case CB_GETDROPPEDCONTROLRECT:
    case LB_GETITEMRECT:
        {
            RECT *r32;
            RECT16 *rect = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *(LPARAM *)(rect + 1);
            r32 = (RECT *)p16->lParam;
            r32->left   = rect->left;
            r32->top    = rect->top;
            r32->right  = rect->right;
            r32->bottom = rect->bottom;
            HeapFree( GetProcessHeap(), 0, rect );
        }
        break;
    case LB_GETSELITEMS:
        {
            INT i;
            LPINT16 items = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *((LPARAM *)items - 1);
            for (i = 0; i < p16->wParam; i++) *((LPINT)(p16->lParam) + i) = items[i];
            HeapFree( GetProcessHeap(), 0, (LPARAM *)items - 1 );
        }
        break;

    case CB_GETEDITSEL:
        if( wParam )
            *((PUINT)(wParam)) = LOWORD(p16->lResult);
        if( lParam )
            *((PUINT)(lParam)) = HIWORD(p16->lResult);  /* FIXME: substract 1? */
        break;

    case WM_MEASUREITEM:
        {
            MEASUREITEMSTRUCT16 *mis = MapSL(p16->lParam);
            MEASUREITEMSTRUCT *mis32 = *(MEASUREITEMSTRUCT **)(mis + 1);
            mis32->itemWidth  = mis->itemWidth;
            mis32->itemHeight = mis->itemHeight;
            UnMapLS( p16->lParam );
            HeapFree( GetProcessHeap(), 0, mis );
        }
        break;
    case WM_GETMINMAXINFO:
        {
            MINMAXINFO16 *mmi = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *(LPARAM *)(mmi + 1);
            MINMAXINFO16to32( mmi, (MINMAXINFO *)(p16->lParam) );
            HeapFree( GetProcessHeap(), 0, mmi );
        }
        break;
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPSTR str = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *((LPARAM *)str - 1);
            lstrcpynA( (LPSTR)(p16->lParam), str, p16->wParam );
            HeapFree( GetProcessHeap(), 0, (LPARAM *)str - 1 );
        }
        break;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs = MapSL(p16->lParam);
            UnMapLS( cs->szTitle );
            UnMapLS( cs->szClass );
            UnMapLS( p16->lParam );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_MDIGETACTIVE:
        if (lParam) *(BOOL *)lParam = (BOOL16)HIWORD(p16->lResult);
        p16->lResult = (LRESULT)WIN_Handle32( LOWORD(p16->lResult) );
        break;
    case WM_NCCALCSIZE:
        {
            NCCALCSIZE_PARAMS *nc32;
            NCCALCSIZE_PARAMS16 *nc = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *(LPARAM *)(nc + 1);
            nc32 = (NCCALCSIZE_PARAMS *)(p16->lParam);
            nc32->rgrc[0].left   = nc->rgrc[0].left;
            nc32->rgrc[0].top    = nc->rgrc[0].top;
            nc32->rgrc[0].right  = nc->rgrc[0].right;
            nc32->rgrc[0].bottom = nc->rgrc[0].bottom;
            if (p16->wParam)
            {
                WINDOWPOS16 *pos = MapSL(nc->lppos);
                UnMapLS( nc->lppos );
                nc32->rgrc[1].left   = nc->rgrc[1].left;
                nc32->rgrc[1].top    = nc->rgrc[1].top;
                nc32->rgrc[1].right  = nc->rgrc[1].right;
                nc32->rgrc[1].bottom = nc->rgrc[1].bottom;
                nc32->rgrc[2].left   = nc->rgrc[2].left;
                nc32->rgrc[2].top    = nc->rgrc[2].top;
                nc32->rgrc[2].right  = nc->rgrc[2].right;
                nc32->rgrc[2].bottom = nc->rgrc[2].bottom;
                WINDOWPOS16to32( pos, nc32->lppos );
                HeapFree( GetProcessHeap(), 0, pos );
            }
            HeapFree( GetProcessHeap(), 0, nc );
        }
        break;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            UnMapLS( cs->lpszName );
            UnMapLS( cs->lpszClass );
            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16 = (MDICREATESTRUCT16 *)MapSL(cs->lpCreateParams);
                UnMapLS( cs->lpCreateParams );
                UnMapLS( mdi_cs16->szTitle );
                UnMapLS( mdi_cs16->szClass );
                HeapFree(GetProcessHeap(), 0, mdi_cs16);
            }
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS16 *wp = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *(LPARAM *)(wp + 1);
            WINDOWPOS16to32( wp, (WINDOWPOS *)p16->lParam );
            HeapFree( GetProcessHeap(), 0, wp );
        }
        break;
    case WM_NOTIFY:
        UnMapLS(p16->lParam);
        break;
    case WM_GETDLGCODE:
        if (p16->lParam)
        {
            LPMSG16 msg16 = MapSL(p16->lParam);
            MSGPARAM16 msgp16;
            UnMapLS( p16->lParam );
            msgp16.wParam=msg16->wParam;
            msgp16.lParam=msg16->lParam;
            WINPROC_UnmapMsg32ATo16(((LPMSG)lParam)->hwnd, ((LPMSG)lParam)->message,
                    ((LPMSG)lParam)->wParam, ((LPMSG)lParam)->lParam,
                    &msgp16 );
            HeapFree( GetProcessHeap(), 0, msg16 );
        }
        break;
    case WM_NEXTMENU:
        {
            MDINEXTMENU *next = (MDINEXTMENU *)lParam;
            next->hmenuNext = HMENU_32( LOWORD(p16->lResult) );
            next->hwndNext = WIN_Handle32( HIWORD(p16->lResult) );
            p16->lResult = 0;
        }
        break;
    }
}


/**********************************************************************
 *	     WINPROC_MapMsg32WTo16
 *
 * Map a message from 32-bit Unicode to 16-bit.
 * Return value is -1 on error, 0 if OK, 1 if an UnmapMsg call is needed.
 */
INT WINPROC_MapMsg32WTo16( HWND hwnd, UINT msg32, WPARAM wParam32,
                             UINT16 *pmsg16, WPARAM16 *pwparam16,
                             LPARAM *plparam )
{
    BYTE ch;
    WCHAR wch;

    *pmsg16    = LOWORD(msg32);
    *pwparam16 = LOWORD(wParam32);
    switch(msg32)
    {
    case LB_ADDSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_INSERTSTRING:
    case LB_SELECTSTRING:
    case LB_DIR:
    case LB_ADDFILE:
        *plparam = map_str_32W_to_16( (LPWSTR)*plparam );
        *pmsg16 = (UINT16)msg32 + (LB_ADDSTRING16 - LB_ADDSTRING);
        return 1;

    case CB_ADDSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_INSERTSTRING:
    case CB_SELECTSTRING:
    case CB_DIR:
        *plparam = map_str_32W_to_16( (LPWSTR)*plparam );
        *pmsg16 = (UINT16)msg32 + (CB_ADDSTRING16 - CB_ADDSTRING);
        return 1;

    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs;
            CREATESTRUCTW *cs32 = (CREATESTRUCTW *)*plparam;

            if (!(cs = HeapAlloc( GetProcessHeap(), 0, sizeof(CREATESTRUCT16) ))) return -1;
            CREATESTRUCT32Ato16( (CREATESTRUCTA *)cs32, cs );
            cs->lpszName  = map_str_32W_to_16( cs32->lpszName );
            cs->lpszClass = map_str_32W_to_16( cs32->lpszClass );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16;
                MDICREATESTRUCTW *mdi_cs = (MDICREATESTRUCTW *)cs32->lpCreateParams;
                mdi_cs16 = HeapAlloc(GetProcessHeap(), 0, sizeof(*mdi_cs16));
                if (!mdi_cs16)
                {
                    HeapFree(GetProcessHeap(), 0, cs);
                    return -1;
                }
                MDICREATESTRUCT32Ato16((MDICREATESTRUCTA *)mdi_cs, mdi_cs16);
                mdi_cs16->szTitle = map_str_32W_to_16(mdi_cs->szTitle);
                mdi_cs16->szClass = map_str_32W_to_16(mdi_cs->szClass);
                cs->lpCreateParams = MapLS(mdi_cs16);
            }
            *plparam   = MapLS(cs);
        }
        return 1;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs;
            MDICREATESTRUCTW *cs32 = (MDICREATESTRUCTW *)*plparam;

            if (!(cs = HeapAlloc( GetProcessHeap(), 0, sizeof(MDICREATESTRUCT16) ))) return -1;
            MDICREATESTRUCT32Ato16( (MDICREATESTRUCTA *)cs32, cs );
            cs->szTitle = map_str_32W_to_16( cs32->szTitle );
            cs->szClass = map_str_32W_to_16( cs32->szClass );
            *plparam   = MapLS(cs);
        }
        return 1;
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
        *plparam = map_str_32W_to_16( (LPWSTR)*plparam );
        return 1;
    case LB_GETTEXT:
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            LPSTR str = HeapAlloc( GetProcessHeap(), 0, 512 ); /* FIXME: fixed sized buffer */
            if (!str) return -1;
            *pmsg16    = LB_GETTEXT16;
            *plparam   = (LPARAM)MapLS(str);
        }
        return 1;
    case CB_GETLBTEXT:
        if ( WINPROC_TestCBForStr( hwnd ))
        {
            LPSTR str = HeapAlloc( GetProcessHeap(), 0, 512 ); /* FIXME: fixed sized buffer */
            if (!str) return -1;
            *pmsg16    = CB_GETLBTEXT16;
            *plparam   = (LPARAM)MapLS(str);
        }
        return 1;

    case WM_CHARTOITEM:
        wch = LOWORD(wParam32);
        WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)&ch, 1, NULL, NULL);
        *pwparam16 = ch;
        *plparam = MAKELPARAM( (HWND16)*plparam, HIWORD(wParam32) );
        return 0;
    case WM_MENUCHAR:
        wch = LOWORD(wParam32);
        WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)&ch, 1, NULL, NULL);
        *pwparam16 = ch;
        *plparam = MAKELPARAM( HIWORD(wParam32), (HMENU16)*plparam );
        return 0;
    case WM_CHAR:
    case WM_DEADCHAR:
    case WM_SYSCHAR:
    case WM_SYSDEADCHAR:
        wch = wParam32;
        WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)&ch, 1, NULL, NULL);
        *pwparam16 = ch;
        return 0;
    case WM_IME_CHAR:
        {
            BYTE ch[2];

            wch = wParam32;
            if (WideCharToMultiByte( CP_ACP, 0, &wch, 1, (LPSTR)ch, 2, NULL, NULL ) == 2)
                *pwparam16 = (ch[0] << 8) | ch[1];
            else
                *pwparam16 = ch[0];
        }
        return 0;

    default:  /* No Unicode translation needed (?) */
        return WINPROC_MapMsg32ATo16( hwnd, msg32, wParam32, pmsg16,
                                      pwparam16, plparam );
    }
}


/**********************************************************************
 *	     WINPROC_UnmapMsg32WTo16
 *
 * Unmap a message that was mapped from 32-bit Unicode to 16-bit.
 */
void WINPROC_UnmapMsg32WTo16( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                              MSGPARAM16* p16 )
{
    switch(msg)
    {
    case LB_ADDSTRING:
    case LB_FINDSTRING:
    case LB_FINDSTRINGEXACT:
    case LB_INSERTSTRING:
    case LB_SELECTSTRING:
    case LB_DIR:
    case LB_ADDFILE:
    case CB_ADDSTRING:
    case CB_FINDSTRING:
    case CB_FINDSTRINGEXACT:
    case CB_INSERTSTRING:
    case CB_SELECTSTRING:
    case CB_DIR:
    case WM_SETTEXT:
    case WM_WININICHANGE:
    case WM_DEVMODECHANGE:
        unmap_str_32W_to_16( p16->lParam );
        break;
    case WM_NCCREATE:
    case WM_CREATE:
        {
            CREATESTRUCT16 *cs = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            unmap_str_32W_to_16( cs->lpszName );
            unmap_str_32W_to_16( cs->lpszClass );

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCT16 *mdi_cs16 = (MDICREATESTRUCT16 *)MapSL(cs->lpCreateParams);
                UnMapLS( cs->lpCreateParams );
                unmap_str_32W_to_16(mdi_cs16->szTitle);
                unmap_str_32W_to_16(mdi_cs16->szClass);
                HeapFree(GetProcessHeap(), 0, mdi_cs16);
            }
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_MDICREATE:
        {
            MDICREATESTRUCT16 *cs = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            unmap_str_32W_to_16( cs->szTitle );
            unmap_str_32W_to_16( cs->szClass );
            HeapFree( GetProcessHeap(), 0, cs );
        }
        break;
    case WM_GETTEXT:
    case WM_ASKCBFORMATNAME:
        {
            LPSTR str = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lParam = *((LPARAM *)str - 1);
            MultiByteToWideChar( CP_ACP, 0, str, -1, (LPWSTR)p16->lParam, 0x7fffffff );
            p16->lResult = strlenW( (LPWSTR)p16->lParam );
            HeapFree( GetProcessHeap(), 0, (LPARAM *)str - 1 );
        }
        break;
    case LB_GETTEXT:
        if ( WINPROC_TestLBForStr( hwnd ))
        {
            LPSTR str = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lResult = MultiByteToWideChar( CP_ACP, 0, str, -1, (LPWSTR)lParam, 0x7fffffff ) - 1;
            HeapFree( GetProcessHeap(), 0, (LPARAM *)str );
        }
        break;
    case CB_GETLBTEXT:
        if ( WINPROC_TestCBForStr( hwnd ))
        {
            LPSTR str = MapSL(p16->lParam);
            UnMapLS( p16->lParam );
            p16->lResult = MultiByteToWideChar( CP_ACP, 0, str, -1, (LPWSTR)lParam, 0x7fffffff ) - 1;
            HeapFree( GetProcessHeap(), 0, (LPARAM *)str );
        }
        break;
    default:
        WINPROC_UnmapMsg32ATo16( hwnd, msg, wParam, lParam, p16 );
        break;
    }
}


/**********************************************************************
 *	     WINPROC_CallProc32ATo32W
 *
 * Call a window procedure, translating args from Ansi to Unicode.
 */
static LRESULT WINPROC_CallProc32ATo32W( WNDPROC func, HWND hwnd,
                                         UINT msg, WPARAM wParam,
                                         LPARAM lParam )
{
    LRESULT result;
    int unmap;

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                func, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam);

    if( (unmap = WINPROC_MapMsg32ATo32W( hwnd, msg, &wParam, &lParam )) == -1) {
        ERR_(msg)("Message translation failed. (msg=%s,wp=%08x,lp=%08lx)\n",
                       SPY_GetMsgName(msg, hwnd), wParam, lParam );
        return 0;
    }
    result = WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );
    if (unmap) result = WINPROC_UnmapMsg32ATo32W( hwnd, msg, wParam, lParam, result, func );
    return result;
}


/**********************************************************************
 *	     WINPROC_CallProc32WTo32A_fast
 *
 */
static BOOL WINPROC_CallProc32WTo32A_fast( WNDPROC func, HWND hwnd,
                                           UINT msg, WPARAM wParam,
                                           LPARAM lParam, LRESULT *result )
{
    switch(msg)
    {
    case WM_NCCREATE:
    case WM_CREATE:
        {   /* csW->lpszName and csW->lpszClass are NOT supposed to be atoms
             * at this point.
             */
            char buffer[1024];
            char *cls = buffer, *name;
            CREATESTRUCTW *csW = (CREATESTRUCTW *)lParam;
            CREATESTRUCTA csA = *(CREATESTRUCTA *)csW;
            DWORD name_lenA, name_lenW, class_lenA, class_lenW;

            class_lenW = strlenW(csW->lpszClass) * sizeof(WCHAR);
            RtlUnicodeToMultiByteSize(&class_lenA, csW->lpszClass, class_lenW);

            if (csW->lpszName)
            {
                name_lenW = strlenW(csW->lpszName) * sizeof(WCHAR);
                RtlUnicodeToMultiByteSize(&name_lenA, csW->lpszName, name_lenW);
            }
            else
                name_lenW = name_lenA = 0;

            if (class_lenA + name_lenA + 2 > sizeof(buffer))
            {
                cls = HeapAlloc(GetProcessHeap(), 0, class_lenA + name_lenA + 2);
                if (!cls) return FALSE;
            }

            RtlUnicodeToMultiByteN(cls, class_lenA, NULL, csW->lpszClass, class_lenW);
            cls[class_lenA] = 0;
            csA.lpszClass = cls;

            if (csW->lpszName)
            {
                name = cls + class_lenA + 1;
                RtlUnicodeToMultiByteN(name, name_lenA, NULL, csW->lpszName, name_lenW);
                name[name_lenA] = 0;
                csA.lpszName = name;
            }

            if (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_MDICHILD)
            {
                MDICREATESTRUCTA mdi_cs;

                mdi_cs = *(MDICREATESTRUCTA *)csW->lpCreateParams;
                mdi_cs.szTitle = csA.lpszName;
                mdi_cs.szClass = csA.lpszClass;
                csA.lpCreateParams = &mdi_cs;
            }

            lParam = (LPARAM)&csA;
            *result = WINPROC_CallWndProc(func, hwnd, msg, wParam, lParam);

            if (cls != buffer) HeapFree(GetProcessHeap(), 0, cls);
        }
        return TRUE;

    default:
        return FALSE;
    }
}

/**********************************************************************
 *	     WINPROC_CallProc32WTo32A
 *
 * Call a window procedure, translating args from Unicode to Ansi.
 */
static LRESULT WINPROC_CallProc32WTo32A( WNDPROC func, HWND hwnd,
                                         UINT msg, WPARAM wParam,
                                         LPARAM lParam )
{
    LRESULT result;
    int unmap;

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                func, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam);

    if (WINPROC_CallProc32WTo32A_fast( func, hwnd, msg, wParam, lParam, &result ))
        return result;

    if ((unmap = WINPROC_MapMsg32WTo32A( hwnd, msg, &wParam, &lParam )) == -1) {
        ERR_(msg)("Message translation failed. (msg=%s,wp=%08x,lp=%08lx)\n",
                       SPY_GetMsgName(msg, hwnd), wParam, lParam );
        return 0;
    }
    result = WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );
    if( unmap ) result = WINPROC_UnmapMsg32WTo32A( hwnd, msg, wParam, lParam, result );
    return result;
}


/**********************************************************************
 *	     __wine_call_wndproc_32A   (USER.1010)
 */
LRESULT WINAPI __wine_call_wndproc_32A( HWND16 hwnd, UINT16 msg, WPARAM16 wParam, LPARAM lParam,
                                        WNDPROC func )
{
    LRESULT result;
    UINT msg32;
    WPARAM wParam32;
    HWND hwnd32 = WIN_Handle32( hwnd );

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                 func, hwnd32, SPY_GetMsgName(msg, hwnd32), wParam, lParam);

    if (WINPROC_MapMsg16To32A( hwnd32, msg, wParam, &msg32, &wParam32, &lParam ) == -1)
        return 0;
    result = WINPROC_CallWndProc( func, hwnd32, msg32, wParam32, lParam );
    return WINPROC_UnmapMsg16To32A( hwnd32, msg32, wParam32, lParam, result );
}


/**********************************************************************
 *	     __wine_call_wndproc_32W   (USER.1011)
 */
LRESULT WINAPI  __wine_call_wndproc_32W( HWND16 hwnd, UINT16 msg, WPARAM16 wParam, LPARAM lParam,
                                         WNDPROC func )
{
    LRESULT result;
    UINT msg32;
    WPARAM wParam32;
    HWND hwnd32 = WIN_Handle32( hwnd );

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                 func, hwnd32, SPY_GetMsgName(msg, hwnd32), wParam, lParam);

    if (WINPROC_MapMsg16To32W( hwnd32, msg, wParam, &msg32, &wParam32, &lParam ) == -1)
        return 0;
    result = WINPROC_CallWndProc( func, hwnd32, msg32, wParam32, lParam );
    return WINPROC_UnmapMsg16To32W( hwnd32, msg32, wParam32, lParam, result,
        func );
}


/**********************************************************************
 *	     WINPROC_CallProc32ATo16
 *
 * Call a 16-bit window procedure, translating the 32-bit args.
 */
static LRESULT WINAPI WINPROC_CallProc32ATo16( WNDPROC16 func, HWND hwnd,
                                               UINT msg, WPARAM wParam,
                                               LPARAM lParam )
{
    UINT16 msg16;
    MSGPARAM16 mp16;

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                func, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam);

    mp16.lParam = lParam;
    if (WINPROC_MapMsg32ATo16( hwnd, msg, wParam, &msg16, &mp16.wParam, &mp16.lParam ) == -1)
        return 0;
    mp16.lResult = WINPROC_CallWndProc16( func, HWND_16(hwnd), msg16,
                                          mp16.wParam, mp16.lParam );
    WINPROC_UnmapMsg32ATo16( hwnd, msg, wParam, lParam, &mp16 );
    return mp16.lResult;
}


/**********************************************************************
 *	     WINPROC_CallProc32WTo16
 *
 * Call a 16-bit window procedure, translating the 32-bit args.
 */
static LRESULT WINAPI WINPROC_CallProc32WTo16( WNDPROC16 func, HWND hwnd,
                                               UINT msg, WPARAM wParam,
                                               LPARAM lParam )
{
    UINT16 msg16;
    MSGPARAM16 mp16;

    TRACE_(msg)("func %p (hwnd=%p,msg=%s,wp=%08x,lp=%08lx)\n",
                func, hwnd, SPY_GetMsgName(msg, hwnd), wParam, lParam);

    mp16.lParam = lParam;
    if (WINPROC_MapMsg32WTo16( hwnd, msg, wParam, &msg16, &mp16.wParam,
                               &mp16.lParam ) == -1)
        return 0;
    mp16.lResult = WINPROC_CallWndProc16( func, HWND_16(hwnd), msg16,
                                          mp16.wParam, mp16.lParam );
    WINPROC_UnmapMsg32WTo16( hwnd, msg, wParam, lParam, &mp16 );
    return mp16.lResult;
}


/**********************************************************************
 *		CallWindowProc (USER.122)
 */
LRESULT WINAPI CallWindowProc16( WNDPROC16 func, HWND16 hwnd, UINT16 msg,
                                 WPARAM16 wParam, LPARAM lParam )
{
    WINDOWPROC *proc;

    if (!func) return 0;

    if (!(proc = WINPROC_GetPtr( (WNDPROC)func )))
        return WINPROC_CallWndProc16( func, hwnd, msg, wParam, lParam );

#if testing
    func = WINPROC_GetProc( (WNDPROC)proc, WIN_PROC_16 );
    return WINPROC_CallWndProc16( func, hwnd, msg, wParam, lParam );
#endif

    switch(proc->type)
    {
    case WIN_PROC_16:
        if (!proc->thunk.t_from32.proc) return 0;
        return WINPROC_CallWndProc16( proc->thunk.t_from32.proc,
                                      hwnd, msg, wParam, lParam );
    case WIN_PROC_32A:
        if (!proc->thunk.t_from16.proc) return 0;
        return __wine_call_wndproc_32A( hwnd, msg, wParam, lParam, proc->thunk.t_from16.proc );
    case WIN_PROC_32W:
        if (!proc->thunk.t_from16.proc) return 0;
        return __wine_call_wndproc_32W( hwnd, msg, wParam, lParam, proc->thunk.t_from16.proc );
    default:
        WARN_(relay)("Invalid proc %p\n", proc );
        return 0;
    }
}


/**********************************************************************
 *		CallWindowProcA (USER32.@)
 *
 * The CallWindowProc() function invokes the windows procedure _func_,
 * with _hwnd_ as the target window, the message specified by _msg_, and
 * the message parameters _wParam_ and _lParam_.
 *
 * Some kinds of argument conversion may be done, I'm not sure what.
 *
 * CallWindowProc() may be used for windows subclassing. Use
 * SetWindowLong() to set a new windows procedure for windows of the
 * subclass, and handle subclassed messages in the new windows
 * procedure. The new windows procedure may then use CallWindowProc()
 * with _func_ set to the parent class's windows procedure to dispatch
 * the message to the superclass.
 *
 * RETURNS
 *
 *    The return value is message dependent.
 *
 * CONFORMANCE
 *
 *   ECMA-234, Win32
 */
LRESULT WINAPI CallWindowProcA(
    WNDPROC func,  /* [in] window procedure */
    HWND hwnd,     /* [in] target window */
    UINT msg,      /* [in] message */
    WPARAM wParam, /* [in] message dependent parameter */
    LPARAM lParam  /* [in] message dependent parameter */
) {
    WINDOWPROC *proc;

    if (!func) return 0;

    if (!(proc = WINPROC_GetPtr( func )))
        return WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );

#if testing
    func = WINPROC_GetProc( (WNDPROC)proc, WIN_PROC_32A );
    return WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );
#endif

    switch(proc->type)
    {
    case WIN_PROC_16:
        if (!proc->thunk.t_from32.proc) return 0;
        return WINPROC_CallProc32ATo16( proc->thunk.t_from32.proc,
                                        hwnd, msg, wParam, lParam );
    case WIN_PROC_32A:
        if (!proc->thunk.t_from16.proc) return 0;
        return WINPROC_CallWndProc( proc->thunk.t_from16.proc,
                                      hwnd, msg, wParam, lParam );
    case WIN_PROC_32W:
        if (!proc->thunk.t_from16.proc) return 0;
        return WINPROC_CallProc32ATo32W( proc->thunk.t_from16.proc,
                                         hwnd, msg, wParam, lParam );
    default:
        WARN_(relay)("Invalid proc %p\n", proc );
        return 0;
    }
}


/**********************************************************************
 *		CallWindowProcW (USER32.@)
 *
 * See CallWindowProcA.
 */
LRESULT WINAPI CallWindowProcW( WNDPROC func, HWND hwnd, UINT msg,
                                  WPARAM wParam, LPARAM lParam )
{
    WINDOWPROC *proc;

    if (!func) return 0;

    if (!(proc = WINPROC_GetPtr( (WNDPROC)func )))
        return WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );

#if testing
    func = WINPROC_GetProc( (WNDPROC)proc, WIN_PROC_32W );
    return WINPROC_CallWndProc( func, hwnd, msg, wParam, lParam );
#endif

    switch(proc->type)
    {
    case WIN_PROC_16:
        if (!proc->thunk.t_from32.proc) return 0;
        return WINPROC_CallProc32WTo16( proc->thunk.t_from32.proc,
                                        hwnd, msg, wParam, lParam );
    case WIN_PROC_32A:
        if (!proc->thunk.t_from16.proc) return 0;
        return WINPROC_CallProc32WTo32A( proc->thunk.t_from16.proc,
                                         hwnd, msg, wParam, lParam );
    case WIN_PROC_32W:
        if (!proc->thunk.t_from16.proc) return 0;
        return WINPROC_CallWndProc( proc->thunk.t_from16.proc,
                                      hwnd, msg, wParam, lParam );
    default:
        WARN_(relay)("Invalid proc %p\n", proc );
        return 0;
    }
}
