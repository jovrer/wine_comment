/*
 * Window related functions
 *
 * Copyright 1993, 1994 Alexandre Julliard
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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "wownt32.h"
#include "wine/server.h"
#include "wine/unicode.h"
#include "win.h"
#include "user_private.h"
#include "controls.h"
#include "winpos.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

#define NB_USER_HANDLES  ((LAST_USER_HANDLE - FIRST_USER_HANDLE + 1) >> 1)
#define USER_HANDLE_TO_INDEX(hwnd) ((LOWORD(hwnd) - FIRST_USER_HANDLE) >> 1)

/**********************************************************************/


static void *user_handles[NB_USER_HANDLES];

/***********************************************************************
 *           create_window_handle
 *
 * Create a window handle with the server.
 */
static WND *create_window_handle( HWND parent, HWND owner, ATOM atom,
                                  HINSTANCE instance, WINDOWPROCTYPE type )
{
    WORD index;
    WND *win;
    struct tagCLASS *class = NULL;
    user_handle_t handle = 0;
    int extra_bytes = 0;

    /* if 16-bit instance, map to module handle */
    if (instance && !HIWORD(instance))
        instance = HINSTANCE_32(GetExePtr(HINSTANCE_16(instance)));

    SERVER_START_REQ( create_window )
    {
        req->parent   = parent;
        req->owner    = owner;
        req->atom     = atom;
        req->instance = instance;
        if (!wine_server_call_err( req ))
        {
            handle = reply->handle;
            extra_bytes = reply->extra;
            class = reply->class_ptr;
        }
    }
    SERVER_END_REQ;

    if (!handle)
    {
        WARN( "error %ld creating window\n", GetLastError() );
        return NULL;
    }

    if (!(win = HeapAlloc( GetProcessHeap(), 0, sizeof(WND) + extra_bytes - sizeof(win->wExtra) )))
    {
        SERVER_START_REQ( destroy_window )
        {
            req->handle = handle;
            wine_server_call( req );
        }
        SERVER_END_REQ;
        SetLastError( ERROR_NOT_ENOUGH_MEMORY );
        return NULL;
    }

    USER_Lock();

    index = USER_HANDLE_TO_INDEX(handle);
    assert( index < NB_USER_HANDLES );
    user_handles[index] = win;
    win->hwndSelf   = handle;
    win->dwMagic    = WND_MAGIC;
    win->cbWndExtra = extra_bytes;
    memset( win->wExtra, 0, extra_bytes );
    CLASS_AddWindow( class, win, type );
    return win;
}


/***********************************************************************
 *           free_window_handle
 *
 * Free a window handle.
 */
static WND *free_window_handle( HWND hwnd )
{
    WND *ptr;
    WORD index = USER_HANDLE_TO_INDEX(hwnd);

    if (index >= NB_USER_HANDLES) return NULL;
    USER_Lock();
    if ((ptr = user_handles[index]))
    {
        SERVER_START_REQ( destroy_window )
        {
            req->handle = hwnd;
            if (!wine_server_call_err( req ))
            {
                user_handles[index] = NULL;
                ptr->dwMagic = 0;
            }
            else
                ptr = NULL;
        }
        SERVER_END_REQ;
    }
    USER_Unlock();
    HeapFree( GetProcessHeap(), 0, ptr );
    return ptr;
}


/*******************************************************************
 *           list_window_children
 *
 * Build an array of the children of a given window. The array must be
 * freed with HeapFree. Returns NULL when no windows are found.
 */
static HWND *list_window_children( HWND hwnd, ATOM atom, DWORD tid )
{
    HWND *list;
    int size = 32;

    for (;;)
    {
        int count = 0;

        if (!(list = HeapAlloc( GetProcessHeap(), 0, size * sizeof(HWND) ))) break;

        SERVER_START_REQ( get_window_children )
        {
            req->parent = hwnd;
            req->atom = atom;
            req->tid = tid;
            wine_server_set_reply( req, list, (size-1) * sizeof(HWND) );
            if (!wine_server_call( req )) count = reply->count;
        }
        SERVER_END_REQ;
        if (count && count < size)
        {
            list[count] = 0;
            return list;
        }
        HeapFree( GetProcessHeap(), 0, list );
        if (!count) break;
        size = count + 1;  /* restart with a large enough buffer */
    }
    return NULL;
}


/*******************************************************************
 *           list_window_parents
 *
 * Build an array of all parents of a given window, starting with
 * the immediate parent. The array must be freed with HeapFree.
 * Returns NULL if window is a top-level window.
 */
static HWND *list_window_parents( HWND hwnd )
{
    WND *win;
    HWND current, *list;
    int pos = 0, size = 16, count = 0;

    if (!(list = HeapAlloc( GetProcessHeap(), 0, size * sizeof(HWND) ))) return NULL;

    current = hwnd;
    for (;;)
    {
        if (!(win = WIN_GetPtr( current ))) goto empty;
        if (win == WND_OTHER_PROCESS) break;  /* need to do it the hard way */
        if (win == WND_DESKTOP)
        {
            if (!pos) goto empty;
            list[pos] = 0;
            return list;
        }
        list[pos] = current = win->parent;
        WIN_ReleasePtr( win );
        if (++pos == size - 1)
        {
            /* need to grow the list */
            HWND *new_list = HeapReAlloc( GetProcessHeap(), 0, list, (size+16) * sizeof(HWND) );
            if (!new_list) goto empty;
            list = new_list;
            size += 16;
        }
    }

    /* at least one parent belongs to another process, have to query the server */

    for (;;)
    {
        count = 0;
        SERVER_START_REQ( get_window_parents )
        {
            req->handle = hwnd;
            wine_server_set_reply( req, list, (size-1) * sizeof(HWND) );
            if (!wine_server_call( req )) count = reply->count;
        }
        SERVER_END_REQ;
        if (!count) goto empty;
        if (size > count)
        {
            list[count] = 0;
            return list;
        }
        HeapFree( GetProcessHeap(), 0, list );
        size = count + 1;
        if (!(list = HeapAlloc( GetProcessHeap(), 0, size * sizeof(HWND) ))) return NULL;
    }

 empty:
    HeapFree( GetProcessHeap(), 0, list );
    return NULL;
}


/*******************************************************************
 *           send_parent_notify
 */
static void send_parent_notify( HWND hwnd, UINT msg )
{
    if ((GetWindowLongW( hwnd, GWL_STYLE ) & (WS_CHILD | WS_POPUP)) == WS_CHILD &&
        !(GetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_NOPARENTNOTIFY))
        SendMessageW( GetParent(hwnd), WM_PARENTNOTIFY,
                      MAKEWPARAM( msg, GetWindowLongPtrW( hwnd, GWLP_ID )), (LPARAM)hwnd );
}


/*******************************************************************
 *		get_server_window_text
 *
 * Retrieve the window text from the server.
 */
static void get_server_window_text( HWND hwnd, LPWSTR text, INT count )
{
    size_t len = 0;

    SERVER_START_REQ( get_window_text )
    {
        req->handle = hwnd;
        wine_server_set_reply( req, text, (count - 1) * sizeof(WCHAR) );
        if (!wine_server_call_err( req )) len = wine_server_reply_size(reply);
    }
    SERVER_END_REQ;
    text[len / sizeof(WCHAR)] = 0;
}


/***********************************************************************
 *           WIN_GetPtr
 *
 * Return a pointer to the WND structure if local to the process,
 * or WND_OTHER_PROCESS if handle may be valid in other process.
 * If ret value is a valid pointer, it must be released with WIN_ReleasePtr.
 */
WND *WIN_GetPtr( HWND hwnd )
{
    WND * ptr;
    WORD index = USER_HANDLE_TO_INDEX(hwnd);

    if (index >= NB_USER_HANDLES) return NULL;

    USER_Lock();
    if ((ptr = user_handles[index]))
    {
        if (ptr->dwMagic == WND_MAGIC &&
            (hwnd == ptr->hwndSelf || !HIWORD(hwnd) || HIWORD(hwnd) == 0xffff))
            return ptr;
        ptr = NULL;
    }
    else if (index == USER_HANDLE_TO_INDEX(GetDesktopWindow()))
    {
        if (hwnd == GetDesktopWindow() || !HIWORD(hwnd) || HIWORD(hwnd) == 0xffff) ptr = WND_DESKTOP;
        else ptr = NULL;
    }
    else ptr = WND_OTHER_PROCESS;
    USER_Unlock();
    return ptr;
}


/***********************************************************************
 *           WIN_IsCurrentProcess
 *
 * Check whether a given window belongs to the current process (and return the full handle).
 */
HWND WIN_IsCurrentProcess( HWND hwnd )
{
    WND *ptr;
    HWND ret;

    if (!(ptr = WIN_GetPtr( hwnd )) || ptr == WND_OTHER_PROCESS || ptr == WND_DESKTOP) return 0;
    ret = ptr->hwndSelf;
    WIN_ReleasePtr( ptr );
    return ret;
}


/***********************************************************************
 *           WIN_IsCurrentThread
 *
 * Check whether a given window belongs to the current thread (and return the full handle).
 */
HWND WIN_IsCurrentThread( HWND hwnd )
{
    WND *ptr;
    HWND ret = 0;

    if (!(ptr = WIN_GetPtr( hwnd )) || ptr == WND_OTHER_PROCESS || ptr == WND_DESKTOP) return 0;
    if (ptr->tid == GetCurrentThreadId()) ret = ptr->hwndSelf;
    WIN_ReleasePtr( ptr );
    return ret;
}


/***********************************************************************
 *           WIN_Handle32
 *
 * Convert a 16-bit window handle to a full 32-bit handle.
 */
HWND WIN_Handle32( HWND16 hwnd16 )
{
    WND *ptr;
    HWND hwnd = (HWND)(ULONG_PTR)hwnd16;

    if (hwnd16 <= 1 || hwnd16 == 0xffff) return hwnd;
    /* do sign extension for -2 and -3 */
    if (hwnd16 >= (HWND16)-3) return (HWND)(LONG_PTR)(INT16)hwnd16;

    if (!(ptr = WIN_GetPtr( hwnd ))) return hwnd;

    if (ptr == WND_DESKTOP) return GetDesktopWindow();

    if (ptr != WND_OTHER_PROCESS)
    {
        hwnd = ptr->hwndSelf;
        WIN_ReleasePtr( ptr );
    }
    else  /* may belong to another process */
    {
        SERVER_START_REQ( get_window_info )
        {
            req->handle = hwnd;
            if (!wine_server_call_err( req )) hwnd = reply->full_handle;
        }
        SERVER_END_REQ;
    }
    return hwnd;
}


/***********************************************************************
 *           WIN_SetOwner
 *
 * Change the owner of a window.
 */
HWND WIN_SetOwner( HWND hwnd, HWND owner )
{
    WND *win = WIN_GetPtr( hwnd );
    HWND ret = 0;

    if (!win || win == WND_DESKTOP) return 0;
    if (win == WND_OTHER_PROCESS)
    {
        if (IsWindow(hwnd)) ERR( "cannot set owner %p on other process window %p\n", owner, hwnd );
        return 0;
    }
    SERVER_START_REQ( set_window_owner )
    {
        req->handle = hwnd;
        req->owner  = owner;
        if (!wine_server_call( req ))
        {
            win->owner = reply->full_owner;
            ret = reply->prev_owner;
        }
    }
    SERVER_END_REQ;
    WIN_ReleasePtr( win );
    return ret;
}


/***********************************************************************
 *           WIN_SetStyle
 *
 * Change the style of a window.
 */
ULONG WIN_SetStyle( HWND hwnd, ULONG set_bits, ULONG clear_bits )
{
    BOOL ok;
    ULONG new_style, old_style = 0;
    WND *win = WIN_GetPtr( hwnd );

    if (!win || win == WND_DESKTOP) return 0;
    if (win == WND_OTHER_PROCESS)
    {
        if (IsWindow(hwnd))
            ERR( "cannot set style %lx/%lx on other process window %p\n",
                 set_bits, clear_bits, hwnd );
        return 0;
    }
    new_style = (win->dwStyle | set_bits) & ~clear_bits;
    if (new_style == win->dwStyle)
    {
        WIN_ReleasePtr( win );
        return new_style;
    }
    SERVER_START_REQ( set_window_info )
    {
        req->handle = hwnd;
        req->flags  = SET_WIN_STYLE;
        req->style  = new_style;
        req->extra_offset = -1;
        if ((ok = !wine_server_call( req )))
        {
            old_style = reply->old_style;
            win->dwStyle = new_style;
        }
    }
    SERVER_END_REQ;
    WIN_ReleasePtr( win );
    if (ok) USER_Driver->pSetWindowStyle( hwnd, old_style );
    return old_style;
}


/***********************************************************************
 *           WIN_GetRectangles
 *
 * Get the window and client rectangles.
 */
BOOL WIN_GetRectangles( HWND hwnd, RECT *rectWindow, RECT *rectClient )
{
    WND *win = WIN_GetPtr( hwnd );
    BOOL ret = TRUE;

    if (!win) return FALSE;
    if (win == WND_DESKTOP)
    {
        RECT rect;
        rect.left = rect.top = 0;
        rect.right  = GetSystemMetrics(SM_CXSCREEN);
        rect.bottom = GetSystemMetrics(SM_CYSCREEN);
        if (rectWindow) *rectWindow = rect;
        if (rectClient) *rectClient = rect;
    }
    else if (win == WND_OTHER_PROCESS)
    {
        SERVER_START_REQ( get_window_rectangles )
        {
            req->handle = hwnd;
            if ((ret = !wine_server_call( req )))
            {
                if (rectWindow)
                {
                    rectWindow->left   = reply->window.left;
                    rectWindow->top    = reply->window.top;
                    rectWindow->right  = reply->window.right;
                    rectWindow->bottom = reply->window.bottom;
                }
                if (rectClient)
                {
                    rectClient->left   = reply->client.left;
                    rectClient->top    = reply->client.top;
                    rectClient->right  = reply->client.right;
                    rectClient->bottom = reply->client.bottom;
                }
            }
        }
        SERVER_END_REQ;
    }
    else
    {
        if (rectWindow) *rectWindow = win->rectWindow;
        if (rectClient) *rectClient = win->rectClient;
        WIN_ReleasePtr( win );
    }
    return ret;
}


/***********************************************************************
 *           WIN_DestroyWindow
 *
 * Destroy storage associated to a window. "Internals" p.358
 */
LRESULT WIN_DestroyWindow( HWND hwnd )
{
    WND *wndPtr;
    HWND *list;
    HMENU menu = 0, sys_menu;

    TRACE("%p\n", hwnd );

    /* free child windows */
    if ((list = WIN_ListChildren( hwnd )))
    {
        int i;
        for (i = 0; list[i]; i++)
        {
            if (WIN_IsCurrentThread( list[i] )) WIN_DestroyWindow( list[i] );
            else SendMessageW( list[i], WM_WINE_DESTROYWINDOW, 0, 0 );
        }
        HeapFree( GetProcessHeap(), 0, list );
    }

    /* Unlink now so we won't bother with the children later on */
    SERVER_START_REQ( set_parent )
    {
        req->handle = hwnd;
        req->parent = 0;
        wine_server_call( req );
    }
    SERVER_END_REQ;

    /*
     * Send the WM_NCDESTROY to the window being destroyed.
     */
    SendMessageW( hwnd, WM_NCDESTROY, 0, 0 );

    /* FIXME: do we need to fake QS_MOUSEMOVE wakebit? */

    WINPOS_CheckInternalPos( hwnd );

    /* free resources associated with the window */

    if (!(wndPtr = WIN_GetPtr( hwnd )) || wndPtr == WND_OTHER_PROCESS) return 0;
    if (!(wndPtr->dwStyle & WS_CHILD)) menu = (HMENU)wndPtr->wIDmenu;
    sys_menu = wndPtr->hSysMenu;
    WIN_ReleasePtr( wndPtr );

    if (menu) DestroyMenu( menu );
    if (sys_menu) DestroyMenu( sys_menu );

    USER_Driver->pDestroyWindow( hwnd );

    free_window_handle( hwnd );
    return 0;
}

/***********************************************************************
 *           WIN_DestroyThreadWindows
 *
 * Destroy all children of 'wnd' owned by the current thread.
 * Return TRUE if something was done.
 */
void WIN_DestroyThreadWindows( HWND hwnd )
{
    HWND *list;
    int i;

    if (!(list = WIN_ListChildren( hwnd ))) return;
    for (i = 0; list[i]; i++)
    {
        if (WIN_IsCurrentThread( list[i] ))
            DestroyWindow( list[i] );
        else
            WIN_DestroyThreadWindows( list[i] );
    }
    HeapFree( GetProcessHeap(), 0, list );
}


/***********************************************************************
 *           WIN_FixCoordinates
 *
 * Fix the coordinates - Helper for WIN_CreateWindowEx.
 * returns default show mode in sw.
 * Note: the feature presented as undocumented *is* in the MSDN since 1993.
 */
static void WIN_FixCoordinates( CREATESTRUCTA *cs, INT *sw)
{
    POINT pos[2];

    if (cs->dwExStyle & WS_EX_MDICHILD)
    {
        UINT id = 0;

        MDI_CalcDefaultChildPos(cs->hwndParent, -1, pos, 0, &id);
        if (!(cs->style & WS_POPUP)) cs->hMenu = (HMENU)id;

        TRACE("MDI child id %04x\n", id);
    }

    if (cs->x == CW_USEDEFAULT || cs->x == CW_USEDEFAULT16 ||
        cs->cx == CW_USEDEFAULT || cs->cx == CW_USEDEFAULT16)
    {
        if (cs->style & (WS_CHILD | WS_POPUP))
        {
            if (cs->dwExStyle & WS_EX_MDICHILD)
            {
                if (cs->x == CW_USEDEFAULT || cs->x == CW_USEDEFAULT16)
                {
                    cs->x = pos[0].x;
                    cs->y = pos[0].y;
                }
                if (cs->cx == CW_USEDEFAULT || cs->cx == CW_USEDEFAULT16 || !cs->cx)
                    cs->cx = pos[1].x;
                if (cs->cy == CW_USEDEFAULT || cs->cy == CW_USEDEFAULT16 || !cs->cy)
                    cs->cy = pos[1].y;
            }
            else
            {
                if (cs->x == CW_USEDEFAULT || cs->x == CW_USEDEFAULT16)
                    cs->x = cs->y = 0;
                if (cs->cx == CW_USEDEFAULT || cs->cx == CW_USEDEFAULT16)
                    cs->cx = cs->cy = 0;
            }
        }
        else  /* overlapped window */
        {
            STARTUPINFOW info;

            GetStartupInfoW( &info );

            if (cs->x == CW_USEDEFAULT || cs->x == CW_USEDEFAULT16)
            {
                /* Never believe Microsoft's documentation... CreateWindowEx doc says
                 * that if an overlapped window is created with WS_VISIBLE style bit
                 * set and the x parameter is set to CW_USEDEFAULT, the system ignores
                 * the y parameter. However, disassembling NT implementation (WIN32K.SYS)
                 * reveals that
                 *
                 * 1) not only it checks for CW_USEDEFAULT but also for CW_USEDEFAULT16
                 * 2) it does not ignore the y parameter as the docs claim; instead, it
                 *    uses it as second parameter to ShowWindow() unless y is either
                 *    CW_USEDEFAULT or CW_USEDEFAULT16.
                 *
                 * The fact that we didn't do 2) caused bogus windows pop up when wine
                 * was running apps that were using this obscure feature. Example -
                 * calc.exe that comes with Win98 (only Win98, it's different from
                 * the one that comes with Win95 and NT)
                 */
                if (cs->y != CW_USEDEFAULT && cs->y != CW_USEDEFAULT16) *sw = cs->y;
                cs->x = (info.dwFlags & STARTF_USEPOSITION) ? info.dwX : 0;
                cs->y = (info.dwFlags & STARTF_USEPOSITION) ? info.dwY : 0;
            }

            if (cs->cx == CW_USEDEFAULT || cs->cx == CW_USEDEFAULT16)
            {
                if (info.dwFlags & STARTF_USESIZE)
                {
                    cs->cx = info.dwXSize;
                    cs->cy = info.dwYSize;
                }
                else  /* if no other hint from the app, pick 3/4 of the screen real estate */
                {
                    RECT r;
                    SystemParametersInfoW( SPI_GETWORKAREA, 0, &r, 0);
                    cs->cx = (((r.right - r.left) * 3) / 4) - cs->x;
                    cs->cy = (((r.bottom - r.top) * 3) / 4) - cs->y;
                }
            }
            /* Handle case where only the cy values is set to default */
            else if (cs->cy == CW_USEDEFAULT || cs->cy == CW_USEDEFAULT16)
            {
                RECT r;
                SystemParametersInfoW( SPI_GETWORKAREA, 0, &r, 0);
                cs->cy = (((r.bottom - r.top) * 3) / 4) - cs->y;
            }
        }
    }
    else
    {
        /* neither x nor cx are default. Check the y values .
         * In the trace we see Outlook and Outlook Express using
         * cy set to CW_USEDEFAULT when opening the address book.
         */
        if (cs->cy == CW_USEDEFAULT || cs->cy == CW_USEDEFAULT16) {
            RECT r;
            FIXME("Strange use of CW_USEDEFAULT in nHeight\n");
            SystemParametersInfoW( SPI_GETWORKAREA, 0, &r, 0);
            cs->cy = (((r.bottom - r.top) * 3) / 4) - cs->y;
        }
    }
}

/***********************************************************************
 *           dump_window_styles
 */
static void dump_window_styles( DWORD style, DWORD exstyle )
{
    TRACE( "style:" );
    if(style & WS_POPUP) TRACE(" WS_POPUP");
    if(style & WS_CHILD) TRACE(" WS_CHILD");
    if(style & WS_MINIMIZE) TRACE(" WS_MINIMIZE");
    if(style & WS_VISIBLE) TRACE(" WS_VISIBLE");
    if(style & WS_DISABLED) TRACE(" WS_DISABLED");
    if(style & WS_CLIPSIBLINGS) TRACE(" WS_CLIPSIBLINGS");
    if(style & WS_CLIPCHILDREN) TRACE(" WS_CLIPCHILDREN");
    if(style & WS_MAXIMIZE) TRACE(" WS_MAXIMIZE");
    if((style & WS_CAPTION) == WS_CAPTION) TRACE(" WS_CAPTION");
    else
    {
        if(style & WS_BORDER) TRACE(" WS_BORDER");
        if(style & WS_DLGFRAME) TRACE(" WS_DLGFRAME");
    }
    if(style & WS_VSCROLL) TRACE(" WS_VSCROLL");
    if(style & WS_HSCROLL) TRACE(" WS_HSCROLL");
    if(style & WS_SYSMENU) TRACE(" WS_SYSMENU");
    if(style & WS_THICKFRAME) TRACE(" WS_THICKFRAME");
    if(style & WS_GROUP) TRACE(" WS_GROUP");
    if(style & WS_TABSTOP) TRACE(" WS_TABSTOP");
    if(style & WS_MINIMIZEBOX) TRACE(" WS_MINIMIZEBOX");
    if(style & WS_MAXIMIZEBOX) TRACE(" WS_MAXIMIZEBOX");

    /* FIXME: Add dumping of BS_/ES_/SBS_/LBS_/CBS_/DS_/etc. styles */
#define DUMPED_STYLES \
    (WS_POPUP | \
     WS_CHILD | \
     WS_MINIMIZE | \
     WS_VISIBLE | \
     WS_DISABLED | \
     WS_CLIPSIBLINGS | \
     WS_CLIPCHILDREN | \
     WS_MAXIMIZE | \
     WS_BORDER | \
     WS_DLGFRAME | \
     WS_VSCROLL | \
     WS_HSCROLL | \
     WS_SYSMENU | \
     WS_THICKFRAME | \
     WS_GROUP | \
     WS_TABSTOP | \
     WS_MINIMIZEBOX | \
     WS_MAXIMIZEBOX)

    if(style & ~DUMPED_STYLES) TRACE(" %08lx", style & ~DUMPED_STYLES);
    TRACE("\n");
#undef DUMPED_STYLES

    TRACE( "exstyle:" );
    if(exstyle & WS_EX_DLGMODALFRAME) TRACE(" WS_EX_DLGMODALFRAME");
    if(exstyle & WS_EX_DRAGDETECT) TRACE(" WS_EX_DRAGDETECT");
    if(exstyle & WS_EX_NOPARENTNOTIFY) TRACE(" WS_EX_NOPARENTNOTIFY");
    if(exstyle & WS_EX_TOPMOST) TRACE(" WS_EX_TOPMOST");
    if(exstyle & WS_EX_ACCEPTFILES) TRACE(" WS_EX_ACCEPTFILES");
    if(exstyle & WS_EX_TRANSPARENT) TRACE(" WS_EX_TRANSPARENT");
    if(exstyle & WS_EX_MDICHILD) TRACE(" WS_EX_MDICHILD");
    if(exstyle & WS_EX_TOOLWINDOW) TRACE(" WS_EX_TOOLWINDOW");
    if(exstyle & WS_EX_WINDOWEDGE) TRACE(" WS_EX_WINDOWEDGE");
    if(exstyle & WS_EX_CLIENTEDGE) TRACE(" WS_EX_CLIENTEDGE");
    if(exstyle & WS_EX_CONTEXTHELP) TRACE(" WS_EX_CONTEXTHELP");
    if(exstyle & WS_EX_RIGHT) TRACE(" WS_EX_RIGHT");
    if(exstyle & WS_EX_RTLREADING) TRACE(" WS_EX_RTLREADING");
    if(exstyle & WS_EX_LEFTSCROLLBAR) TRACE(" WS_EX_LEFTSCROLLBAR");
    if(exstyle & WS_EX_CONTROLPARENT) TRACE(" WS_EX_CONTROLPARENT");
    if(exstyle & WS_EX_STATICEDGE) TRACE(" WS_EX_STATICEDGE");
    if(exstyle & WS_EX_APPWINDOW) TRACE(" WS_EX_APPWINDOW");
    if(exstyle & WS_EX_LAYERED) TRACE(" WS_EX_LAYERED");

#define DUMPED_EX_STYLES \
    (WS_EX_DLGMODALFRAME | \
     WS_EX_DRAGDETECT | \
     WS_EX_NOPARENTNOTIFY | \
     WS_EX_TOPMOST | \
     WS_EX_ACCEPTFILES | \
     WS_EX_TRANSPARENT | \
     WS_EX_MDICHILD | \
     WS_EX_TOOLWINDOW | \
     WS_EX_WINDOWEDGE | \
     WS_EX_CLIENTEDGE | \
     WS_EX_CONTEXTHELP | \
     WS_EX_RIGHT | \
     WS_EX_RTLREADING | \
     WS_EX_LEFTSCROLLBAR | \
     WS_EX_CONTROLPARENT | \
     WS_EX_STATICEDGE | \
     WS_EX_APPWINDOW | \
     WS_EX_LAYERED)

    if(exstyle & ~DUMPED_EX_STYLES) TRACE(" %08lx", exstyle & ~DUMPED_EX_STYLES);
    TRACE("\n");
#undef DUMPED_EX_STYLES
}


/***********************************************************************
 *           WIN_CreateWindowEx
 *
 * Implementation of CreateWindowEx().
 */
static HWND WIN_CreateWindowEx( CREATESTRUCTA *cs, ATOM classAtom,
				WINDOWPROCTYPE type )
{
    INT sw = SW_SHOW;
    WND *wndPtr;
    HWND hwnd, parent, owner, top_child = 0;
    BOOL unicode = (type == WIN_PROC_32W);

    TRACE("%s %s ex=%08lx style=%08lx %d,%d %dx%d parent=%p menu=%p inst=%p params=%p\n",
          (type == WIN_PROC_32W) ? debugstr_w((LPCWSTR)cs->lpszName) : debugstr_a(cs->lpszName),
          (type == WIN_PROC_32W) ? debugstr_w((LPCWSTR)cs->lpszClass) : debugstr_a(cs->lpszClass),
          cs->dwExStyle, cs->style, cs->x, cs->y, cs->cx, cs->cy,
          cs->hwndParent, cs->hMenu, cs->hInstance, cs->lpCreateParams );

    if(TRACE_ON(win)) dump_window_styles( cs->style, cs->dwExStyle );

    TRACE("winproc type is %d (%s)\n", type, (type == WIN_PROC_16) ? "WIN_PROC_16" :
	    ((type == WIN_PROC_32A) ? "WIN_PROC_32A" : "WIN_PROC_32W") );

    /* Fix the styles for MDI children */
    if (cs->dwExStyle & WS_EX_MDICHILD)
    {
        MDICREATESTRUCTA mdi_cs;
        UINT flags = 0;

        wndPtr = WIN_GetPtr(cs->hwndParent);
        if (wndPtr && wndPtr != WND_OTHER_PROCESS && wndPtr != WND_DESKTOP)
        {
            flags = wndPtr->flags;
            WIN_ReleasePtr(wndPtr);
        }

        if (!(flags & WIN_ISMDICLIENT))
        {
            WARN("WS_EX_MDICHILD, but parent %p is not MDIClient\n", cs->hwndParent);
            return 0;
        }

        /* cs->lpCreateParams of WM_[NC]CREATE is different for MDI children.
         * MDICREATESTRUCT members have the originally passed values.
         *
         * Note: we rely on the fact that MDICREATESTRUCTA and MDICREATESTRUCTW
         * have the same layout.
         */
        mdi_cs.szClass = cs->lpszClass;
        mdi_cs.szTitle = cs->lpszName;
        mdi_cs.hOwner = cs->hInstance;
        mdi_cs.x = cs->x;
        mdi_cs.y = cs->y;
        mdi_cs.cx = cs->cx;
        mdi_cs.cy = cs->cy;
        mdi_cs.style = cs->style;
        mdi_cs.lParam = (LPARAM)cs->lpCreateParams;

        cs->lpCreateParams = (LPVOID)&mdi_cs;

        if (GetWindowLongW(cs->hwndParent, GWL_STYLE) & MDIS_ALLCHILDSTYLES)
        {
            if (cs->style & WS_POPUP)
            {
                TRACE("WS_POPUP with MDIS_ALLCHILDSTYLES is not allowed\n");
                return 0;
            }
            cs->style |= WS_CHILD | WS_CLIPSIBLINGS;
        }
        else
        {
            cs->style &= ~WS_POPUP;
            cs->style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CAPTION |
                WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        }

        top_child = GetWindow(cs->hwndParent, GW_CHILD);

        if (top_child)
        {
            /* Restore current maximized child */
            if((cs->style & WS_VISIBLE) && IsZoomed(top_child))
            {
                TRACE("Restoring current maximized child %p\n", top_child);
                SendMessageW( top_child, WM_SETREDRAW, FALSE, 0 );
                ShowWindow(top_child, SW_RESTORE);
                SendMessageW( top_child, WM_SETREDRAW, TRUE, 0 );
            }
        }
    }

    /* Find the parent window */

    parent = GetDesktopWindow();
    owner = 0;

    if (cs->hwndParent == HWND_MESSAGE)
    {
      /* native ole32.OleInitialize uses HWND_MESSAGE to create the
       * message window (style: WS_POPUP|WS_DISABLED)
       */
      FIXME("Parent is HWND_MESSAGE\n");
    }
    else if (cs->hwndParent)
    {
	/* Make sure parent is valid */
        if (!IsWindow( cs->hwndParent ))
        {
            WARN("Bad parent %p\n", cs->hwndParent );
	    return 0;
	}
        if ((cs->style & (WS_CHILD|WS_POPUP)) == WS_CHILD)
            parent = WIN_GetFullHandle(cs->hwndParent);
        else
            owner = GetAncestor( cs->hwndParent, GA_ROOT );
    }
    else if ((cs->style & (WS_CHILD|WS_POPUP)) == WS_CHILD)
    {
        WARN("No parent for child window\n" );
        return 0;  /* WS_CHILD needs a parent, but WS_POPUP doesn't */
    }

    WIN_FixCoordinates(cs, &sw); /* fix default coordinates */

    if ((cs->dwExStyle & WS_EX_DLGMODALFRAME) ||
        ((!(cs->dwExStyle & WS_EX_STATICEDGE)) &&
          (cs->style & (WS_DLGFRAME | WS_THICKFRAME))))
        cs->dwExStyle |= WS_EX_WINDOWEDGE;
    else
        cs->dwExStyle &= ~WS_EX_WINDOWEDGE;

    /* Create the window structure */

    if (!(wndPtr = create_window_handle( parent, owner, classAtom, cs->hInstance, type )))
    {
	TRACE("out of memory\n" );
	return 0;
    }
    hwnd = wndPtr->hwndSelf;

    /* Fill the window structure */

    wndPtr->tid            = GetCurrentThreadId();
    wndPtr->owner          = owner;
    wndPtr->parent         = parent;
    wndPtr->hInstance      = cs->hInstance;
    wndPtr->text           = NULL;
    wndPtr->dwStyle        = cs->style & ~WS_VISIBLE;
    wndPtr->dwExStyle      = cs->dwExStyle;
    wndPtr->wIDmenu        = 0;
    wndPtr->helpContext    = 0;
    wndPtr->flags          = (type == WIN_PROC_16) ? 0 : WIN_ISWIN32;
    wndPtr->pVScroll       = NULL;
    wndPtr->pHScroll       = NULL;
    wndPtr->userdata       = 0;
    wndPtr->hIcon          = 0;
    wndPtr->hIconSmall     = 0;
    wndPtr->hSysMenu       = 0;

    if (wndPtr->dwStyle & WS_SYSMENU) SetSystemMenu( hwnd, 0 );

    /*
     * Correct the window styles.
     *
     * It affects only the style loaded into the WIN structure.
     */

    if (!(wndPtr->dwStyle & WS_CHILD))
    {
        wndPtr->dwStyle |= WS_CLIPSIBLINGS;
        if (!(wndPtr->dwStyle & WS_POPUP))
            wndPtr->dwStyle |= WS_CAPTION;
    }

    /*
     * WS_EX_WINDOWEDGE appears to be enforced based on the other styles, so
     * why does the user get to set it?
     */

    if ((wndPtr->dwExStyle & WS_EX_DLGMODALFRAME) ||
          (wndPtr->dwStyle & (WS_DLGFRAME | WS_THICKFRAME)))
        wndPtr->dwExStyle |= WS_EX_WINDOWEDGE;
    else
        wndPtr->dwExStyle &= ~WS_EX_WINDOWEDGE;

    if (!(wndPtr->dwStyle & (WS_CHILD | WS_POPUP)))
        wndPtr->flags |= WIN_NEED_SIZE;

    SERVER_START_REQ( set_window_info )
    {
        req->handle    = hwnd;
        req->flags     = SET_WIN_STYLE | SET_WIN_EXSTYLE | SET_WIN_INSTANCE | SET_WIN_UNICODE;
        req->style     = wndPtr->dwStyle;
        req->ex_style  = wndPtr->dwExStyle;
        req->instance  = (void *)wndPtr->hInstance;
        req->is_unicode = (type == WIN_PROC_32W);
        req->extra_offset = -1;
        wine_server_call( req );
    }
    SERVER_END_REQ;

    /* Set the window menu */

    if (((wndPtr->dwStyle & (WS_CAPTION|WS_CHILD)) == WS_CAPTION) ||
        (wndPtr->dwExStyle & WS_EX_APPWINDOW))
    {
        if (cs->hMenu)
        {
            if (!MENU_SetMenu(hwnd, cs->hMenu))
            {
                WIN_ReleasePtr( wndPtr );
                free_window_handle( hwnd );
                return 0;
            }
        }
        else
        {
            LPCSTR menuName = (LPCSTR)GetClassLongPtrA( hwnd, GCLP_MENUNAME );
            if (menuName)
            {
                if (!cs->hInstance || HIWORD(cs->hInstance))
                    cs->hMenu = LoadMenuA(cs->hInstance,menuName);
                else
                    cs->hMenu = HMENU_32(LoadMenu16(HINSTANCE_16(cs->hInstance),menuName));

                if (cs->hMenu) MENU_SetMenu( hwnd, cs->hMenu );
            }
        }
    }
    else SetWindowLongPtrW( hwnd, GWLP_ID, (ULONG_PTR)cs->hMenu );
    WIN_ReleasePtr( wndPtr );

    if (!USER_Driver->pCreateWindow( hwnd, cs, unicode))
    {
        WIN_DestroyWindow( hwnd );
        return 0;
    }

    /* Notify the parent window only */

    send_parent_notify( hwnd, WM_CREATE );
    if (!IsWindow( hwnd )) return 0;

    if (cs->style & WS_VISIBLE)
    {
        if (cs->style & WS_MAXIMIZE)
            sw = SW_SHOWMAXIMIZED;
        else if (cs->style & WS_MINIMIZE)
            sw = SW_SHOWMINIMIZED;

        ShowWindow( hwnd, sw );
        if (cs->dwExStyle & WS_EX_MDICHILD)
        {
            SendMessageW(cs->hwndParent, WM_MDIREFRESHMENU, 0, 0);
            /* ShowWindow won't activate child windows */
            SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE );
        }
    }

    /* Call WH_SHELL hook */

    if (!(GetWindowLongW( hwnd, GWL_STYLE ) & WS_CHILD) && !GetWindow( hwnd, GW_OWNER ))
        HOOK_CallHooks( WH_SHELL, HSHELL_WINDOWCREATED, (WPARAM)hwnd, 0, TRUE );

    TRACE("created window %p\n", hwnd);
    return hwnd;
}


/***********************************************************************
 *		CreateWindow (USER.41)
 */
HWND16 WINAPI CreateWindow16( LPCSTR className, LPCSTR windowName,
                              DWORD style, INT16 x, INT16 y, INT16 width,
                              INT16 height, HWND16 parent, HMENU16 menu,
                              HINSTANCE16 instance, LPVOID data )
{
    return CreateWindowEx16( 0, className, windowName, style,
                             x, y, width, height, parent, menu, instance, data );
}


/***********************************************************************
 *		CreateWindowEx (USER.452)
 */
HWND16 WINAPI CreateWindowEx16( DWORD exStyle, LPCSTR className,
                                LPCSTR windowName, DWORD style, INT16 x,
                                INT16 y, INT16 width, INT16 height,
                                HWND16 parent, HMENU16 menu,
                                HINSTANCE16 instance, LPVOID data )
{
    ATOM classAtom;
    CREATESTRUCTA cs;
    char buffer[256];

    /* Find the class atom */

    if (HIWORD(className))
    {
        if (!(classAtom = GlobalFindAtomA( className )))
        {
            ERR( "bad class name %s\n", debugstr_a(className) );
            return 0;
        }
    }
    else
    {
        classAtom = LOWORD(className);
        if (!GlobalGetAtomNameA( classAtom, buffer, sizeof(buffer) ))
        {
            ERR( "bad atom %x\n", classAtom);
            return 0;
        }
        className = buffer;
    }

    /* Fix the coordinates */

    cs.x  = (x == CW_USEDEFAULT16) ? CW_USEDEFAULT : (INT)x;
    cs.y  = (y == CW_USEDEFAULT16) ? CW_USEDEFAULT : (INT)y;
    cs.cx = (width == CW_USEDEFAULT16) ? CW_USEDEFAULT : (INT)width;
    cs.cy = (height == CW_USEDEFAULT16) ? CW_USEDEFAULT : (INT)height;

    /* Create the window */

    cs.lpCreateParams = data;
    cs.hInstance      = HINSTANCE_32(instance);
    cs.hMenu          = HMENU_32(menu);
    cs.hwndParent     = WIN_Handle32( parent );
    cs.style          = style;
    cs.lpszName       = windowName;
    cs.lpszClass      = className;
    cs.dwExStyle      = exStyle;

    return HWND_16( WIN_CreateWindowEx( &cs, classAtom, WIN_PROC_16 ));
}


/***********************************************************************
 *		CreateWindowExA (USER32.@)
 */
HWND WINAPI CreateWindowExA( DWORD exStyle, LPCSTR className,
                                 LPCSTR windowName, DWORD style, INT x,
                                 INT y, INT width, INT height,
                                 HWND parent, HMENU menu,
                                 HINSTANCE instance, LPVOID data )
{
    ATOM classAtom;
    CREATESTRUCTA cs;
    char buffer[256];

    /* Find the class atom */

    if (HIWORD(className))
    {
        if (!(classAtom = GlobalFindAtomA( className )))
        {
            ERR( "bad class name %s\n", debugstr_a(className) );
            return 0;
        }
    }
    else
    {
        classAtom = LOWORD(className);
        if (!GlobalGetAtomNameA( classAtom, buffer, sizeof(buffer) ))
        {
            ERR( "bad atom %x\n", classAtom);
            return 0;
        }
        className = buffer;
    }

    /* Create the window */

    cs.lpCreateParams = data;
    cs.hInstance      = instance;
    cs.hMenu          = menu;
    cs.hwndParent     = parent;
    cs.x              = x;
    cs.y              = y;
    cs.cx             = width;
    cs.cy             = height;
    cs.style          = style;
    cs.lpszName       = windowName;
    cs.lpszClass      = className;
    cs.dwExStyle      = exStyle;

    return WIN_CreateWindowEx( &cs, classAtom, WIN_PROC_32A );
}


/***********************************************************************
 *		CreateWindowExW (USER32.@)
 */
HWND WINAPI CreateWindowExW( DWORD exStyle, LPCWSTR className,
                                 LPCWSTR windowName, DWORD style, INT x,
                                 INT y, INT width, INT height,
                                 HWND parent, HMENU menu,
                                 HINSTANCE instance, LPVOID data )
{
    ATOM classAtom;
    CREATESTRUCTW cs;
    WCHAR buffer[256];

    /* Find the class atom */

    if (HIWORD(className))
    {
        if (!(classAtom = GlobalFindAtomW( className )))
        {
            ERR( "bad class name %s\n", debugstr_w(className) );
            return 0;
        }
    }
    else
    {
        classAtom = LOWORD(className);
        if (!GlobalGetAtomNameW( classAtom, buffer, sizeof(buffer)/sizeof(WCHAR) ))
        {
            ERR( "bad atom %x\n", classAtom);
            return 0;
        }
        className = buffer;
    }

    /* Create the window */

    cs.lpCreateParams = data;
    cs.hInstance      = instance;
    cs.hMenu          = menu;
    cs.hwndParent     = parent;
    cs.x              = x;
    cs.y              = y;
    cs.cx             = width;
    cs.cy             = height;
    cs.style          = style;
    cs.lpszName       = windowName;
    cs.lpszClass      = className;
    cs.dwExStyle      = exStyle;

    /* Note: we rely on the fact that CREATESTRUCTA and */
    /* CREATESTRUCTW have the same layout. */
    return WIN_CreateWindowEx( (CREATESTRUCTA *)&cs, classAtom, WIN_PROC_32W );
}


/***********************************************************************
 *           WIN_SendDestroyMsg
 */
static void WIN_SendDestroyMsg( HWND hwnd )
{
    GUITHREADINFO info;

    if (GetGUIThreadInfo( GetCurrentThreadId(), &info ))
    {
        if (hwnd == info.hwndCaret) DestroyCaret();
        if (hwnd == info.hwndActive) WINPOS_ActivateOtherWindow( hwnd );
    }
    USER_Driver->pResetSelectionOwner( hwnd, TRUE );

    /*
     * Send the WM_DESTROY to the window.
     */
    SendMessageW( hwnd, WM_DESTROY, 0, 0);

    /*
     * This WM_DESTROY message can trigger re-entrant calls to DestroyWindow
     * make sure that the window still exists when we come back.
     */
    if (IsWindow(hwnd))
    {
        HWND* pWndArray;
        int i;

        if (!(pWndArray = WIN_ListChildren( hwnd ))) return;

        for (i = 0; pWndArray[i]; i++)
        {
            if (IsWindow( pWndArray[i] )) WIN_SendDestroyMsg( pWndArray[i] );
        }
        HeapFree( GetProcessHeap(), 0, pWndArray );
    }
    else
      WARN("\tdestroyed itself while in WM_DESTROY!\n");
}


/***********************************************************************
 *		DestroyWindow (USER32.@)
 */
BOOL WINAPI DestroyWindow( HWND hwnd )
{
    BOOL is_child;

    if (!(hwnd = WIN_IsCurrentThread( hwnd )) || (hwnd == GetDesktopWindow()))
    {
        SetLastError( ERROR_ACCESS_DENIED );
        return FALSE;
    }

    TRACE("(%p)\n", hwnd);

      /* Call hooks */

    if (HOOK_CallHooks( WH_CBT, HCBT_DESTROYWND, (WPARAM)hwnd, 0, TRUE )) return FALSE;

    if (MENU_IsMenuActive() == hwnd)
        EndMenu();

    is_child = (GetWindowLongW( hwnd, GWL_STYLE ) & WS_CHILD) != 0;

    if (is_child)
    {
        if (!USER_IsExitingThread( GetCurrentThreadId() ))
            send_parent_notify( hwnd, WM_DESTROY );
    }
    else if (!GetWindow( hwnd, GW_OWNER ))
    {
        HOOK_CallHooks( WH_SHELL, HSHELL_WINDOWDESTROYED, (WPARAM)hwnd, 0L, TRUE );
        /* FIXME: clean up palette - see "Internals" p.352 */
    }

    if (!IsWindow(hwnd)) return TRUE;

    USER_Driver->pResetSelectionOwner( hwnd, FALSE ); /* before the window is unmapped */

      /* Hide the window */
    if (GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE)
    {
        /* Only child windows receive WM_SHOWWINDOW in DestroyWindow() */
        if (is_child)
            ShowWindow( hwnd, SW_HIDE );
        else
            SetWindowPos( hwnd, 0, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE |
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW );
    }

    if (!IsWindow(hwnd)) return TRUE;

      /* Recursively destroy owned windows */

    if (!is_child)
    {
        for (;;)
        {
            int i, got_one = 0;
            HWND *list = WIN_ListChildren( GetDesktopWindow() );
            if (list)
            {
                for (i = 0; list[i]; i++)
                {
                    if (GetWindow( list[i], GW_OWNER ) != hwnd) continue;
                    if (WIN_IsCurrentThread( list[i] ))
                    {
                        DestroyWindow( list[i] );
                        got_one = 1;
                        continue;
                    }
                    WIN_SetOwner( list[i], 0 );
                }
                HeapFree( GetProcessHeap(), 0, list );
            }
            if (!got_one) break;
        }
    }

      /* Send destroy messages */

    WIN_SendDestroyMsg( hwnd );
    if (!IsWindow( hwnd )) return TRUE;

    if (GetClipboardOwner() == hwnd)
        CLIPBOARD_ReleaseOwner();

      /* Destroy the window storage */

    WIN_DestroyWindow( hwnd );
    return TRUE;
}


/***********************************************************************
 *		CloseWindow (USER32.@)
 */
BOOL WINAPI CloseWindow( HWND hwnd )
{
    if (GetWindowLongW( hwnd, GWL_STYLE ) & WS_CHILD) return FALSE;
    ShowWindow( hwnd, SW_MINIMIZE );
    return TRUE;
}


/***********************************************************************
 *		OpenIcon (USER32.@)
 */
BOOL WINAPI OpenIcon( HWND hwnd )
{
    if (!IsIconic( hwnd )) return FALSE;
    ShowWindow( hwnd, SW_SHOWNORMAL );
    return TRUE;
}


/***********************************************************************
 *           WIN_FindWindow
 *
 * Implementation of FindWindow() and FindWindowEx().
 */
static HWND WIN_FindWindow( HWND parent, HWND child, ATOM className, LPCWSTR title )
{
    HWND *list = NULL;
    HWND retvalue = 0;
    int i = 0, len = 0;
    WCHAR *buffer = NULL;

    if (!parent) parent = GetDesktopWindow();
    if (title)
    {
        len = strlenW(title) + 1;  /* one extra char to check for chars beyond the end */
        if (!(buffer = HeapAlloc( GetProcessHeap(), 0, (len + 1) * sizeof(WCHAR) ))) return 0;
    }

    if (!(list = list_window_children( parent, className, 0 ))) goto done;

    if (child)
    {
        child = WIN_GetFullHandle( child );
        while (list[i] && list[i] != child) i++;
        if (!list[i]) goto done;
        i++;  /* start from next window */
    }

    if (title)
    {
        while (list[i])
        {
            if (GetWindowTextW( list[i], buffer, len + 1 ) && !strcmpiW( buffer, title )) break;
            i++;
        }
    }
    retvalue = list[i];

 done:
    HeapFree( GetProcessHeap(), 0, list );
    HeapFree( GetProcessHeap(), 0, buffer );
    return retvalue;
}



/***********************************************************************
 *		FindWindowA (USER32.@)
 */
HWND WINAPI FindWindowA( LPCSTR className, LPCSTR title )
{
    HWND ret = FindWindowExA( 0, 0, className, title );
    if (!ret) SetLastError (ERROR_CANNOT_FIND_WND_CLASS);
    return ret;
}


/***********************************************************************
 *		FindWindowExA (USER32.@)
 */
HWND WINAPI FindWindowExA( HWND parent, HWND child,
                               LPCSTR className, LPCSTR title )
{
    ATOM atom = 0;
    LPWSTR buffer;
    HWND hwnd;
    INT len;

    if (className)
    {
        /* If the atom doesn't exist, then no class */
        /* with this name exists either. */
        if (!(atom = GlobalFindAtomA( className )))
        {
            SetLastError (ERROR_CANNOT_FIND_WND_CLASS);
            return 0;
        }
    }
    if (!title) return WIN_FindWindow( parent, child, atom, NULL );

    len = MultiByteToWideChar( CP_ACP, 0, title, -1, NULL, 0 );
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, len * sizeof(WCHAR) ))) return 0;
    MultiByteToWideChar( CP_ACP, 0, title, -1, buffer, len );
    hwnd = WIN_FindWindow( parent, child, atom, buffer );
    HeapFree( GetProcessHeap(), 0, buffer );
    return hwnd;
}


/***********************************************************************
 *		FindWindowExW (USER32.@)
 */
HWND WINAPI FindWindowExW( HWND parent, HWND child,
                               LPCWSTR className, LPCWSTR title )
{
    ATOM atom = 0;

    if (className)
    {
        /* If the atom doesn't exist, then no class */
        /* with this name exists either. */
        if (!(atom = GlobalFindAtomW( className )))
        {
            SetLastError (ERROR_CANNOT_FIND_WND_CLASS);
            return 0;
        }
    }
    return WIN_FindWindow( parent, child, atom, title );
}


/***********************************************************************
 *		FindWindowW (USER32.@)
 */
HWND WINAPI FindWindowW( LPCWSTR className, LPCWSTR title )
{
    return FindWindowExW( 0, 0, className, title );
}


/**********************************************************************
 *		GetDesktopWindow (USER32.@)
 */
HWND WINAPI GetDesktopWindow(void)
{
    struct user_thread_info *thread_info = get_user_thread_info();

    if (!thread_info->desktop)
    {
        SERVER_START_REQ( get_desktop_window )
        {
            if (!wine_server_call( req )) thread_info->desktop = reply->handle;
        }
        SERVER_END_REQ;
        if (!thread_info->desktop || !USER_Driver->pCreateDesktopWindow( thread_info->desktop ))
            ERR( "failed to create desktop window\n" );
    }
    return thread_info->desktop;
}


/*******************************************************************
 *		EnableWindow (USER32.@)
 */
BOOL WINAPI EnableWindow( HWND hwnd, BOOL enable )
{
    BOOL retvalue;
    HWND full_handle;

    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if (!(full_handle = WIN_IsCurrentThread( hwnd )))
        return SendMessageW( hwnd, WM_WINE_ENABLEWINDOW, enable, 0 );

    hwnd = full_handle;

    TRACE("( %p, %d )\n", hwnd, enable);

    retvalue = !IsWindowEnabled( hwnd );

    if (enable && retvalue)
    {
        WIN_SetStyle( hwnd, 0, WS_DISABLED );
        SendMessageW( hwnd, WM_ENABLE, TRUE, 0 );
    }
    else if (!enable && !retvalue)
    {
        HWND capture_wnd;

        SendMessageW( hwnd, WM_CANCELMODE, 0, 0);

        WIN_SetStyle( hwnd, WS_DISABLED, 0 );

        if (hwnd == GetFocus())
            SetFocus( 0 );  /* A disabled window can't have the focus */

        capture_wnd = GetCapture();
        if (hwnd == capture_wnd || IsChild(hwnd, capture_wnd))
            ReleaseCapture();  /* A disabled window can't capture the mouse */

        SendMessageW( hwnd, WM_ENABLE, FALSE, 0 );
    }
    return retvalue;
}


/***********************************************************************
 *		IsWindowEnabled (USER32.@)
 */
BOOL WINAPI IsWindowEnabled(HWND hWnd)
{
    return !(GetWindowLongW( hWnd, GWL_STYLE ) & WS_DISABLED);
}


/***********************************************************************
 *		IsWindowUnicode (USER32.@)
 */
BOOL WINAPI IsWindowUnicode( HWND hwnd )
{
    WND * wndPtr;
    BOOL retvalue = FALSE;

    if (!(wndPtr = WIN_GetPtr(hwnd))) return FALSE;

    if (wndPtr == WND_DESKTOP) return TRUE;

    if (wndPtr != WND_OTHER_PROCESS)
    {
        retvalue = (WINPROC_GetProcType( wndPtr->winproc ) == WIN_PROC_32W);
        WIN_ReleasePtr( wndPtr );
    }
    else
    {
        SERVER_START_REQ( get_window_info )
        {
            req->handle = hwnd;
            if (!wine_server_call_err( req )) retvalue = reply->is_unicode;
        }
        SERVER_END_REQ;
    }
    return retvalue;
}


/**********************************************************************
 *		GetWindowWord (USER32.@)
 */
WORD WINAPI GetWindowWord( HWND hwnd, INT offset )
{
    if (offset >= 0)
    {
        WORD retvalue = 0;
        WND *wndPtr = WIN_GetPtr( hwnd );
        if (!wndPtr)
        {
            SetLastError( ERROR_INVALID_WINDOW_HANDLE );
            return 0;
        }
        if (wndPtr == WND_OTHER_PROCESS || wndPtr == WND_DESKTOP)
        {
            SERVER_START_REQ( set_window_info )
            {
                req->handle = hwnd;
                req->flags  = 0;  /* don't set anything, just retrieve */
                req->extra_offset = offset;
                req->extra_size = sizeof(retvalue);
                if (!wine_server_call_err( req ))
                    memcpy( &retvalue, &reply->old_extra_value, sizeof(retvalue) );
            }
            SERVER_END_REQ;
            return retvalue;
        }
        if (offset > (int)(wndPtr->cbWndExtra - sizeof(WORD)))
        {
            WARN("Invalid offset %d\n", offset );
            SetLastError( ERROR_INVALID_INDEX );
        }
        else memcpy( &retvalue,  (char *)wndPtr->wExtra + offset, sizeof(retvalue) );
        WIN_ReleasePtr( wndPtr );
        return retvalue;
    }

    switch(offset)
    {
    case GWLP_HWNDPARENT:
        return GetWindowLongPtrW( hwnd, offset );
    case GWLP_ID:
    case GWLP_HINSTANCE:
        {
            LONG_PTR ret = GetWindowLongPtrW( hwnd, offset );
            if (HIWORD(ret))
                WARN("%d: discards high bits of 0x%08lx!\n", offset, ret );
            return LOWORD(ret);
        }
    default:
        WARN("Invalid offset %d\n", offset );
        return 0;
    }
}


/**********************************************************************
 *		SetWindowWord (USER32.@)
 */
WORD WINAPI SetWindowWord( HWND hwnd, INT offset, WORD newval )
{
    WORD retval = 0;
    WND * wndPtr;

    switch(offset)
    {
    case GWLP_ID:
    case GWLP_HINSTANCE:
    case GWLP_HWNDPARENT:
        return SetWindowLongPtrW( hwnd, offset, (ULONG_PTR)newval );
    default:
        if (offset < 0)
        {
            WARN("Invalid offset %d\n", offset );
            SetLastError( ERROR_INVALID_INDEX );
            return 0;
        }
    }

    wndPtr = WIN_GetPtr( hwnd );
    if (wndPtr == WND_DESKTOP)
    {
        SetLastError( ERROR_ACCESS_DENIED );
        return 0;
    }
    if (wndPtr == WND_OTHER_PROCESS)
    {
        if (IsWindow(hwnd))
            FIXME( "set %d <- %x not supported yet on other process window %p\n",
                   offset, newval, hwnd );
        wndPtr = NULL;
    }
    if (!wndPtr)
    {
       SetLastError( ERROR_INVALID_WINDOW_HANDLE );
       return 0;
    }

    if (offset > (int)(wndPtr->cbWndExtra - sizeof(WORD)))
    {
        WARN("Invalid offset %d\n", offset );
        WIN_ReleasePtr(wndPtr);
        SetLastError( ERROR_INVALID_INDEX );
        return 0;
    }

    SERVER_START_REQ( set_window_info )
    {
        req->handle = hwnd;
        req->flags = SET_WIN_EXTRA;
        req->extra_offset = offset;
        req->extra_size = sizeof(newval);
        memcpy( &req->extra_value, &newval, sizeof(newval) );
        if (!wine_server_call_err( req ))
        {
            void *ptr = (char *)wndPtr->wExtra + offset;
            memcpy( &retval, ptr, sizeof(retval) );
            memcpy( ptr, &newval, sizeof(newval) );
        }
    }
    SERVER_END_REQ;
    WIN_ReleasePtr( wndPtr );
    return retval;
}


/**********************************************************************
 *	     WIN_GetWindowLong
 *
 * Helper function for GetWindowLong().
 */
static LONG_PTR WIN_GetWindowLong( HWND hwnd, INT offset, WINDOWPROCTYPE type )
{
    LONG_PTR retvalue = 0;
    WND *wndPtr;

    if (offset == GWLP_HWNDPARENT)
    {
        HWND parent = GetAncestor( hwnd, GA_PARENT );
        if (parent == GetDesktopWindow()) parent = GetWindow( hwnd, GW_OWNER );
        return (ULONG_PTR)parent;
    }

    if (!(wndPtr = WIN_GetPtr( hwnd )))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }

    if (wndPtr == WND_OTHER_PROCESS || wndPtr == WND_DESKTOP)
    {
        if (offset == GWLP_WNDPROC)
        {
            SetLastError( ERROR_ACCESS_DENIED );
            return 0;
        }
        SERVER_START_REQ( set_window_info )
        {
            req->handle = hwnd;
            req->flags  = 0;  /* don't set anything, just retrieve */
            req->extra_offset = (offset >= 0) ? offset : -1;
            req->extra_size = (offset >= 0) ? sizeof(retvalue) : 0;
            if (!wine_server_call_err( req ))
            {
                switch(offset)
                {
                case GWL_STYLE:      retvalue = reply->old_style; break;
                case GWL_EXSTYLE:    retvalue = reply->old_ex_style; break;
                case GWLP_ID:        retvalue = reply->old_id; break;
                case GWLP_HINSTANCE: retvalue = (ULONG_PTR)reply->old_instance; break;
                case GWLP_USERDATA:  retvalue = (ULONG_PTR)reply->old_user_data; break;
                default:
                    if (offset >= 0) retvalue = reply->old_extra_value;
                    else SetLastError( ERROR_INVALID_INDEX );
                    break;
                }
            }
        }
        SERVER_END_REQ;
        return retvalue;
    }

    /* now we have a valid wndPtr */

    if (offset >= 0)
    {
        if (offset > (int)(wndPtr->cbWndExtra - sizeof(LONG)))
        {
          /*
            * Some programs try to access last element from 16 bit
            * code using illegal offset value. Hopefully this is
            * what those programs really expect.
            */
           if (type == WIN_PROC_16 &&
               wndPtr->cbWndExtra >= 4 &&
               offset == wndPtr->cbWndExtra - sizeof(WORD))
           {
               INT offset2 = wndPtr->cbWndExtra - sizeof(LONG);

               ERR( "- replaced invalid offset %d with %d\n",
                    offset, offset2 );

                retvalue = *(LONG_PTR *)(((char *)wndPtr->wExtra) + offset2);
                WIN_ReleasePtr( wndPtr );
                return retvalue;
            }
            WARN("Invalid offset %d\n", offset );
            WIN_ReleasePtr( wndPtr );
            SetLastError( ERROR_INVALID_INDEX );
            return 0;
        }
        retvalue = *(LONG_PTR *)(((char *)wndPtr->wExtra) + offset);
        /* Special case for dialog window procedure */
        if ((offset == DWLP_DLGPROC) && (wndPtr->flags & WIN_ISDIALOG))
            retvalue = (LONG_PTR)WINPROC_GetProc( (WNDPROC)retvalue, type );
        WIN_ReleasePtr( wndPtr );
        return retvalue;
    }

    switch(offset)
    {
    case GWLP_USERDATA:  retvalue = wndPtr->userdata; break;
    case GWL_STYLE:      retvalue = wndPtr->dwStyle; break;
    case GWL_EXSTYLE:    retvalue = wndPtr->dwExStyle; break;
    case GWLP_ID:        retvalue = (ULONG_PTR)wndPtr->wIDmenu; break;
    case GWLP_WNDPROC:   retvalue = (ULONG_PTR)WINPROC_GetProc( wndPtr->winproc, type ); break;
    case GWLP_HINSTANCE: retvalue = (ULONG_PTR)wndPtr->hInstance; break;
    default:
        WARN("Unknown offset %d\n", offset );
        SetLastError( ERROR_INVALID_INDEX );
        break;
    }
    WIN_ReleasePtr(wndPtr);
    return retvalue;
}


/**********************************************************************
 *	     WIN_SetWindowLong
 *
 * Helper function for SetWindowLong().
 *
 * 0 is the failure code. However, in the case of failure SetLastError
 * must be set to distinguish between a 0 return value and a failure.
 */
static LONG_PTR WIN_SetWindowLong( HWND hwnd, INT offset, LONG_PTR newval,
                                   WINDOWPROCTYPE type )
{
    STYLESTRUCT style;
    BOOL ok;
    LONG_PTR retval = 0;
    WND *wndPtr;

    TRACE( "%p %d %lx %x\n", hwnd, offset, newval, type );

    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if (!(wndPtr = WIN_GetPtr( hwnd )))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }
    if (wndPtr == WND_DESKTOP)
    {
        /* can't change anything on the desktop window */
        SetLastError( ERROR_ACCESS_DENIED );
        return 0;
    }
    if (wndPtr == WND_OTHER_PROCESS)
    {
        if (offset == GWLP_WNDPROC)
        {
            SetLastError( ERROR_ACCESS_DENIED );
            return 0;
        }
        return SendMessageW( hwnd, WM_WINE_SETWINDOWLONG, offset, newval );
    }

    /* first some special cases */
    switch( offset )
    {
    case GWL_STYLE:
    case GWL_EXSTYLE:
        style.styleOld =
            offset == GWL_STYLE ? wndPtr->dwStyle : wndPtr->dwExStyle;
        style.styleNew = newval;
        WIN_ReleasePtr( wndPtr );
        SendMessageW( hwnd, WM_STYLECHANGING, offset, (LPARAM)&style );
        if (!(wndPtr = WIN_GetPtr( hwnd )) || wndPtr == WND_OTHER_PROCESS) return 0;
        newval = style.styleNew;
        break;
    case GWLP_HWNDPARENT:
        if (wndPtr->parent == GetDesktopWindow())
        {
            WIN_ReleasePtr( wndPtr );
            return (ULONG_PTR)WIN_SetOwner( hwnd, (HWND)newval );
        }
        else
        {
            WIN_ReleasePtr( wndPtr );
            return (ULONG_PTR)SetParent( hwnd, (HWND)newval );
        }
    case GWLP_WNDPROC:
    {
        WINDOWPROCTYPE old_type = WINPROC_GetProcType( wndPtr->winproc );
        retval = (ULONG_PTR)WINPROC_GetProc( wndPtr->winproc, type );
        wndPtr->winproc = WINPROC_AllocProc( (WNDPROC)newval, type );
        if (old_type == type)
        {
            WIN_ReleasePtr( wndPtr );
            return retval;
        }
        /* update is_unicode flag on the server side */
        break;
    }
    case GWLP_ID:
    case GWLP_HINSTANCE:
    case GWLP_USERDATA:
        break;
    case DWLP_DLGPROC:
        if ((wndPtr->cbWndExtra + sizeof(LONG_PTR) >= DWLP_DLGPROC) && (wndPtr->flags & WIN_ISDIALOG))
        {
            WNDPROC *ptr = (WNDPROC *)((char *)wndPtr->wExtra + DWLP_DLGPROC);
            retval = (ULONG_PTR)WINPROC_GetProc( *ptr, type );
            *ptr = WINPROC_AllocProc( (WNDPROC)newval, type );
            WIN_ReleasePtr( wndPtr );
            return retval;
        }
        /* fall through */
    default:
        if (offset < 0 || offset > (int)(wndPtr->cbWndExtra - sizeof(LONG_PTR)))
        {
            WARN("Invalid offset %d\n", offset );
            WIN_ReleasePtr( wndPtr );
            SetLastError( ERROR_INVALID_INDEX );
            return 0;
        }
        else
        {
            LONG_PTR *ptr = (LONG_PTR *)((char *)wndPtr->wExtra + offset);
            if (*ptr == newval)  /* already set to the same value */
            {
                WIN_ReleasePtr( wndPtr );
                return newval;
            }
        }
        break;
    }

    SERVER_START_REQ( set_window_info )
    {
        req->handle = hwnd;
        req->extra_offset = -1;
        switch(offset)
        {
        case GWL_STYLE:
            req->flags = SET_WIN_STYLE;
            req->style = newval;
            break;
        case GWL_EXSTYLE:
            req->flags = SET_WIN_EXSTYLE;
            req->ex_style = newval;
            break;
        case GWLP_ID:
            req->flags = SET_WIN_ID;
            req->id = newval;
            break;
        case GWLP_HINSTANCE:
            req->flags = SET_WIN_INSTANCE;
            req->instance = (void *)newval;
            break;
        case GWLP_WNDPROC:
            req->flags = SET_WIN_UNICODE;
            req->is_unicode = (type == WIN_PROC_32W);
            break;
        case GWLP_USERDATA:
            req->flags = SET_WIN_USERDATA;
            req->user_data = (void *)newval;
            break;
        default:
            req->flags = SET_WIN_EXTRA;
            req->extra_offset = offset;
            req->extra_size = sizeof(newval);
            memcpy( &req->extra_value, &newval, sizeof(newval) );
        }
        if ((ok = !wine_server_call_err( req )))
        {
            switch(offset)
            {
            case GWL_STYLE:
                wndPtr->dwStyle = newval;
                retval = reply->old_style;
                break;
            case GWL_EXSTYLE:
                wndPtr->dwExStyle = newval;
                retval = reply->old_ex_style;
                break;
            case GWLP_ID:
                wndPtr->wIDmenu = newval;
                retval = reply->old_id;
                break;
            case GWLP_HINSTANCE:
                wndPtr->hInstance = (HINSTANCE)newval;
                retval = (ULONG_PTR)reply->old_instance;
                break;
            case GWLP_WNDPROC:
                break;
            case GWLP_USERDATA:
                wndPtr->userdata = newval;
                retval = (ULONG_PTR)reply->old_user_data;
                break;
            default:
                {
                    void *ptr = (char *)wndPtr->wExtra + offset;
                    memcpy( &retval, ptr, sizeof(retval) );
                    memcpy( ptr, &newval, sizeof(newval) );
                }
                break;
            }
        }
    }
    SERVER_END_REQ;
    WIN_ReleasePtr( wndPtr );

    if (!ok) return 0;

    if (offset == GWL_STYLE) USER_Driver->pSetWindowStyle( hwnd, retval );

    if (offset == GWL_STYLE || offset == GWL_EXSTYLE)
        SendMessageW( hwnd, WM_STYLECHANGED, offset, (LPARAM)&style );

    return retval;
}


/**********************************************************************
 *		GetWindowLong (USER.135)
 */
LONG WINAPI GetWindowLong16( HWND16 hwnd, INT16 offset )
{
    return WIN_GetWindowLong( WIN_Handle32(hwnd), offset, WIN_PROC_16 );
}


/**********************************************************************
 *		GetWindowLongA (USER32.@)
 */
LONG WINAPI GetWindowLongA( HWND hwnd, INT offset )
{
    return WIN_GetWindowLong( hwnd, offset, WIN_PROC_32A );
}


/**********************************************************************
 *		GetWindowLongW (USER32.@)
 */
LONG WINAPI GetWindowLongW( HWND hwnd, INT offset )
{
    return WIN_GetWindowLong( hwnd, offset, WIN_PROC_32W );
}


/**********************************************************************
 *		SetWindowLong (USER.136)
 */
LONG WINAPI SetWindowLong16( HWND16 hwnd, INT16 offset, LONG newval )
{
    return WIN_SetWindowLong( WIN_Handle32(hwnd), offset, newval, WIN_PROC_16 );
}


/**********************************************************************
 *		SetWindowLongA (USER32.@)
 *
 * See SetWindowLongW.
 */
LONG WINAPI SetWindowLongA( HWND hwnd, INT offset, LONG newval )
{
    return WIN_SetWindowLong( hwnd, offset, newval, WIN_PROC_32A );
}


/**********************************************************************
 *		SetWindowLongW (USER32.@) Set window attribute
 *
 * SetWindowLong() alters one of a window's attributes or sets a 32-bit (long)
 * value in a window's extra memory.
 *
 * The _hwnd_ parameter specifies the window.  is the handle to a
 * window that has extra memory. The _newval_ parameter contains the
 * new attribute or extra memory value.  If positive, the _offset_
 * parameter is the byte-addressed location in the window's extra
 * memory to set.  If negative, _offset_ specifies the window
 * attribute to set, and should be one of the following values:
 *
 * GWL_EXSTYLE      The window's extended window style
 *
 * GWL_STYLE        The window's window style.
 *
 * GWLP_WNDPROC     Pointer to the window's window procedure.
 *
 * GWLP_HINSTANCE   The window's pplication instance handle.
 *
 * GWLP_ID          The window's identifier.
 *
 * GWLP_USERDATA    The window's user-specified data.
 *
 * If the window is a dialog box, the _offset_ parameter can be one of
 * the following values:
 *
 * DWLP_DLGPROC     The address of the window's dialog box procedure.
 *
 * DWLP_MSGRESULT   The return value of a message
 *                  that the dialog box procedure processed.
 *
 * DWLP_USER        Application specific information.
 *
 * RETURNS
 *
 * If successful, returns the previous value located at _offset_. Otherwise,
 * returns 0.
 *
 * NOTES
 *
 * Extra memory for a window class is specified by a nonzero cbWndExtra
 * parameter of the WNDCLASS structure passed to RegisterClass() at the
 * time of class creation.
 *
 * Using GWL_WNDPROC to set a new window procedure effectively creates
 * a window subclass. Use CallWindowProc() in the new windows procedure
 * to pass messages to the superclass's window procedure.
 *
 * The user data is reserved for use by the application which created
 * the window.
 *
 * Do not use GWL_STYLE to change the window's WS_DISABLED style;
 * instead, call the EnableWindow() function to change the window's
 * disabled state.
 *
 * Do not use GWL_HWNDPARENT to reset the window's parent, use
 * SetParent() instead.
 *
 * Win95:
 * When offset is GWL_STYLE and the calling app's ver is 4.0,
 * it sends WM_STYLECHANGING before changing the settings
 * and WM_STYLECHANGED afterwards.
 * App ver 4.0 can't use SetWindowLong to change WS_EX_TOPMOST.
 */
LONG WINAPI SetWindowLongW(
    HWND hwnd,  /* [in] window to alter */
    INT offset, /* [in] offset, in bytes, of location to alter */
    LONG newval /* [in] new value of location */
) {
    return WIN_SetWindowLong( hwnd, offset, newval, WIN_PROC_32W );
}


/*******************************************************************
 *		GetWindowTextA (USER32.@)
 */
INT WINAPI GetWindowTextA( HWND hwnd, LPSTR lpString, INT nMaxCount )
{
    WCHAR *buffer;

    if (!lpString) return 0;

    if (WIN_IsCurrentProcess( hwnd ))
        return (INT)SendMessageA( hwnd, WM_GETTEXT, nMaxCount, (LPARAM)lpString );

    /* when window belongs to other process, don't send a message */
    if (nMaxCount <= 0) return 0;
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, nMaxCount * sizeof(WCHAR) ))) return 0;
    get_server_window_text( hwnd, buffer, nMaxCount );
    if (!WideCharToMultiByte( CP_ACP, 0, buffer, -1, lpString, nMaxCount, NULL, NULL ))
        lpString[nMaxCount-1] = 0;
    HeapFree( GetProcessHeap(), 0, buffer );
    return strlen(lpString);
}


/*******************************************************************
 *		InternalGetWindowText (USER32.@)
 */
INT WINAPI InternalGetWindowText(HWND hwnd,LPWSTR lpString,INT nMaxCount )
{
    WND *win;

    if (nMaxCount <= 0) return 0;
    if (!(win = WIN_GetPtr( hwnd ))) return 0;
    if (win == WND_DESKTOP) lpString[0] = 0;
    else if (win != WND_OTHER_PROCESS)
    {
        if (win->text) lstrcpynW( lpString, win->text, nMaxCount );
        else lpString[0] = 0;
        WIN_ReleasePtr( win );
    }
    else
    {
        get_server_window_text( hwnd, lpString, nMaxCount );
    }
    return strlenW(lpString);
}


/*******************************************************************
 *		GetWindowTextW (USER32.@)
 */
INT WINAPI GetWindowTextW( HWND hwnd, LPWSTR lpString, INT nMaxCount )
{
    if (!lpString) return 0;

    if (WIN_IsCurrentProcess( hwnd ))
        return (INT)SendMessageW( hwnd, WM_GETTEXT, nMaxCount, (LPARAM)lpString );

    /* when window belongs to other process, don't send a message */
    if (nMaxCount <= 0) return 0;
    get_server_window_text( hwnd, lpString, nMaxCount );
    return strlenW(lpString);
}


/*******************************************************************
 *		SetWindowTextA (USER32.@)
 *		SetWindowText  (USER32.@)
 */
BOOL WINAPI SetWindowTextA( HWND hwnd, LPCSTR lpString )
{
    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if (!WIN_IsCurrentProcess( hwnd ))
        FIXME( "setting text %s of other process window %p should not use SendMessage\n",
               debugstr_a(lpString), hwnd );
    return (BOOL)SendMessageA( hwnd, WM_SETTEXT, 0, (LPARAM)lpString );
}


/*******************************************************************
 *		SetWindowTextW (USER32.@)
 */
BOOL WINAPI SetWindowTextW( HWND hwnd, LPCWSTR lpString )
{
    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if (!WIN_IsCurrentProcess( hwnd ))
        FIXME( "setting text %s of other process window %p should not use SendMessage\n",
               debugstr_w(lpString), hwnd );
    return (BOOL)SendMessageW( hwnd, WM_SETTEXT, 0, (LPARAM)lpString );
}


/*******************************************************************
 *		GetWindowTextLengthA (USER32.@)
 */
INT WINAPI GetWindowTextLengthA( HWND hwnd )
{
    return SendMessageA( hwnd, WM_GETTEXTLENGTH, 0, 0 );
}

/*******************************************************************
 *		GetWindowTextLengthW (USER32.@)
 */
INT WINAPI GetWindowTextLengthW( HWND hwnd )
{
    return SendMessageW( hwnd, WM_GETTEXTLENGTH, 0, 0 );
}


/*******************************************************************
 *		IsWindow (USER32.@)
 */
BOOL WINAPI IsWindow( HWND hwnd )
{
    WND *ptr;
    BOOL ret;

    if (!(ptr = WIN_GetPtr( hwnd ))) return FALSE;
    if (ptr == WND_DESKTOP) return TRUE;

    if (ptr != WND_OTHER_PROCESS)
    {
        WIN_ReleasePtr( ptr );
        return TRUE;
    }

    /* check other processes */
    SERVER_START_REQ( get_window_info )
    {
        req->handle = hwnd;
        ret = !wine_server_call_err( req );
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *		GetWindowThreadProcessId (USER32.@)
 */
DWORD WINAPI GetWindowThreadProcessId( HWND hwnd, LPDWORD process )
{
    WND *ptr;
    DWORD tid = 0;

    if (!(ptr = WIN_GetPtr( hwnd )))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE);
        return 0;
    }

    if (ptr != WND_OTHER_PROCESS && ptr != WND_DESKTOP)
    {
        /* got a valid window */
        tid = ptr->tid;
        if (process) *process = GetCurrentProcessId();
        WIN_ReleasePtr( ptr );
        return tid;
    }

    /* check other processes */
    SERVER_START_REQ( get_window_info )
    {
        req->handle = hwnd;
        if (!wine_server_call_err( req ))
        {
            tid = (DWORD)reply->tid;
            if (process) *process = (DWORD)reply->pid;
        }
    }
    SERVER_END_REQ;
    return tid;
}


/*****************************************************************
 *		GetParent (USER32.@)
 */
HWND WINAPI GetParent( HWND hwnd )
{
    WND *wndPtr;
    HWND retvalue = 0;

    if (!(wndPtr = WIN_GetPtr( hwnd )))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }
    if (wndPtr == WND_DESKTOP) return 0;
    if (wndPtr == WND_OTHER_PROCESS)
    {
        LONG style = GetWindowLongW( hwnd, GWL_STYLE );
        if (style & (WS_POPUP | WS_CHILD))
        {
            SERVER_START_REQ( get_window_tree )
            {
                req->handle = hwnd;
                if (!wine_server_call_err( req ))
                {
                    if (style & WS_POPUP) retvalue = reply->owner;
                    else if (style & WS_CHILD) retvalue = reply->parent;
                }
            }
            SERVER_END_REQ;
        }
    }
    else
    {
        if (wndPtr->dwStyle & WS_POPUP) retvalue = wndPtr->owner;
        else if (wndPtr->dwStyle & WS_CHILD) retvalue = wndPtr->parent;
        WIN_ReleasePtr( wndPtr );
    }
    return retvalue;
}


/*****************************************************************
 *		GetAncestor (USER32.@)
 */
HWND WINAPI GetAncestor( HWND hwnd, UINT type )
{
    WND *win;
    HWND *list, ret = 0;

    switch(type)
    {
    case GA_PARENT:
        if (!(win = WIN_GetPtr( hwnd )))
        {
            SetLastError( ERROR_INVALID_WINDOW_HANDLE );
            return 0;
        }
        if (win == WND_DESKTOP) return 0;
        if (win != WND_OTHER_PROCESS)
        {
            ret = win->parent;
            WIN_ReleasePtr( win );
        }
        else /* need to query the server */
        {
            SERVER_START_REQ( get_window_tree )
            {
                req->handle = hwnd;
                if (!wine_server_call_err( req )) ret = reply->parent;
            }
            SERVER_END_REQ;
        }
        break;

    case GA_ROOT:
        if (!(list = list_window_parents( hwnd ))) return 0;

        if (!list[0] || !list[1]) ret = WIN_GetFullHandle( hwnd );  /* top-level window */
        else
        {
            int count = 2;
            while (list[count]) count++;
            ret = list[count - 2];  /* get the one before the desktop */
        }
        HeapFree( GetProcessHeap(), 0, list );
        break;

    case GA_ROOTOWNER:
        if ((ret = WIN_GetFullHandle( hwnd )) == GetDesktopWindow()) return 0;
        for (;;)
        {
            HWND parent = GetParent( ret );
            if (!parent) break;
            ret = parent;
        }
        break;
    }
    return ret;
}


/*****************************************************************
 *		SetParent (USER32.@)
 */
HWND WINAPI SetParent( HWND hwnd, HWND parent )
{
    HWND full_handle;

    if (is_broadcast(hwnd) || is_broadcast(parent))
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!parent) parent = GetDesktopWindow();
    else parent = WIN_GetFullHandle( parent );

    if (!IsWindow( parent ))
    {
        SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }

    /* Some applications try to set a child as a parent */
    if (IsChild(hwnd, parent))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return 0;
    }

    if (!(full_handle = WIN_IsCurrentThread( hwnd )))
        return (HWND)SendMessageW( hwnd, WM_WINE_SETPARENT, (WPARAM)parent, 0 );

    return USER_Driver->pSetParent( full_handle, parent );
}


/*******************************************************************
 *		IsChild (USER32.@)
 */
BOOL WINAPI IsChild( HWND parent, HWND child )
{
    HWND *list = list_window_parents( child );
    int i;
    BOOL ret;

    if (!list) return FALSE;
    parent = WIN_GetFullHandle( parent );
    for (i = 0; list[i]; i++) if (list[i] == parent) break;
    ret = (list[i] != 0);
    HeapFree( GetProcessHeap(), 0, list );
    return ret;
}


/***********************************************************************
 *		IsWindowVisible (USER32.@)
 */
BOOL WINAPI IsWindowVisible( HWND hwnd )
{
    HWND *list;
    BOOL retval;
    int i;

    if (!(GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE)) return FALSE;
    if (!(list = list_window_parents( hwnd ))) return TRUE;
    for (i = 0; list[i]; i++)
        if (!(GetWindowLongW( list[i], GWL_STYLE ) & WS_VISIBLE)) break;
    retval = !list[i];
    HeapFree( GetProcessHeap(), 0, list );
    return retval;
}


/***********************************************************************
 *           WIN_IsWindowDrawable
 *
 * hwnd is drawable when it is visible, all parents are not
 * minimized, and it is itself not minimized unless we are
 * trying to draw its default class icon.
 */
BOOL WIN_IsWindowDrawable( HWND hwnd, BOOL icon )
{
    HWND *list;
    BOOL retval;
    int i;
    LONG style = GetWindowLongW( hwnd, GWL_STYLE );

    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & WS_MINIMIZE) && icon && GetClassLongPtrW( hwnd, GCLP_HICON ))  return FALSE;

    if (!(list = list_window_parents( hwnd ))) return TRUE;
    for (i = 0; list[i]; i++)
        if ((GetWindowLongW( list[i], GWL_STYLE ) & (WS_VISIBLE|WS_MINIMIZE)) != WS_VISIBLE)
            break;
    retval = !list[i];
    HeapFree( GetProcessHeap(), 0, list );
    return retval;
}


/*******************************************************************
 *		GetTopWindow (USER32.@)
 */
HWND WINAPI GetTopWindow( HWND hwnd )
{
    if (!hwnd) hwnd = GetDesktopWindow();
    return GetWindow( hwnd, GW_CHILD );
}


/*******************************************************************
 *		GetWindow (USER32.@)
 */
HWND WINAPI GetWindow( HWND hwnd, UINT rel )
{
    HWND retval = 0;

    if (rel == GW_OWNER)  /* this one may be available locally */
    {
        WND *wndPtr = WIN_GetPtr( hwnd );
        if (!wndPtr)
        {
            SetLastError( ERROR_INVALID_HANDLE );
            return 0;
        }
        if (wndPtr == WND_DESKTOP) return 0;
        if (wndPtr != WND_OTHER_PROCESS)
        {
            retval = wndPtr->owner;
            WIN_ReleasePtr( wndPtr );
            return retval;
        }
        /* else fall through to server call */
    }

    SERVER_START_REQ( get_window_tree )
    {
        req->handle = hwnd;
        if (!wine_server_call_err( req ))
        {
            switch(rel)
            {
            case GW_HWNDFIRST:
                retval = reply->first_sibling;
                break;
            case GW_HWNDLAST:
                retval = reply->last_sibling;
                break;
            case GW_HWNDNEXT:
                retval = reply->next_sibling;
                break;
            case GW_HWNDPREV:
                retval = reply->prev_sibling;
                break;
            case GW_OWNER:
                retval = reply->owner;
                break;
            case GW_CHILD:
                retval = reply->first_child;
                break;
            }
        }
    }
    SERVER_END_REQ;
    return retval;
}


/*******************************************************************
 *		ShowOwnedPopups (USER32.@)
 */
BOOL WINAPI ShowOwnedPopups( HWND owner, BOOL fShow )
{
    int count = 0;
    WND *pWnd;
    HWND *win_array = WIN_ListChildren( GetDesktopWindow() );

    if (!win_array) return TRUE;

    while (win_array[count]) count++;
    while (--count >= 0)
    {
        if (GetWindow( win_array[count], GW_OWNER ) != owner) continue;
        if (!(pWnd = WIN_GetPtr( win_array[count] ))) continue;
        if (pWnd == WND_OTHER_PROCESS) continue;
        if (fShow)
        {
            if (pWnd->flags & WIN_NEEDS_SHOW_OWNEDPOPUP)
            {
                WIN_ReleasePtr( pWnd );
                /* In Windows, ShowOwnedPopups(TRUE) generates
                 * WM_SHOWWINDOW messages with SW_PARENTOPENING,
                 * regardless of the state of the owner
                 */
                SendMessageW(win_array[count], WM_SHOWWINDOW, SW_SHOWNORMAL, SW_PARENTOPENING);
                continue;
            }
        }
        else
        {
            if (pWnd->dwStyle & WS_VISIBLE)
            {
                WIN_ReleasePtr( pWnd );
                /* In Windows, ShowOwnedPopups(FALSE) generates
                 * WM_SHOWWINDOW messages with SW_PARENTCLOSING,
                 * regardless of the state of the owner
                 */
                SendMessageW(win_array[count], WM_SHOWWINDOW, SW_HIDE, SW_PARENTCLOSING);
                continue;
            }
        }
        WIN_ReleasePtr( pWnd );
    }
    HeapFree( GetProcessHeap(), 0, win_array );
    return TRUE;
}


/*******************************************************************
 *		GetLastActivePopup (USER32.@)
 */
HWND WINAPI GetLastActivePopup( HWND hwnd )
{
    HWND retval = hwnd;

    SERVER_START_REQ( get_window_info )
    {
        req->handle = hwnd;
        if (!wine_server_call_err( req )) retval = reply->last_active;
    }
    SERVER_END_REQ;
    return retval;
}


/*******************************************************************
 *           WIN_ListChildren
 *
 * Build an array of the children of a given window. The array must be
 * freed with HeapFree. Returns NULL when no windows are found.
 */
HWND *WIN_ListChildren( HWND hwnd )
{
    return list_window_children( hwnd, 0, 0 );
}


/*******************************************************************
 *		EnumWindows (USER32.@)
 */
BOOL WINAPI EnumWindows( WNDENUMPROC lpEnumFunc, LPARAM lParam )
{
    HWND *list;
    BOOL ret = TRUE;
    int i;

    USER_CheckNotLock();

    /* We have to build a list of all windows first, to avoid */
    /* unpleasant side-effects, for instance if the callback */
    /* function changes the Z-order of the windows.          */

    if (!(list = WIN_ListChildren( GetDesktopWindow() ))) return TRUE;

    /* Now call the callback function for every window */

    for (i = 0; list[i]; i++)
    {
        /* Make sure that the window still exists */
        if (!IsWindow( list[i] )) continue;
        if (!(ret = lpEnumFunc( list[i], lParam ))) break;
    }
    HeapFree( GetProcessHeap(), 0, list );
    return ret;
}


/**********************************************************************
 *		EnumThreadWindows (USER32.@)
 */
BOOL WINAPI EnumThreadWindows( DWORD id, WNDENUMPROC func, LPARAM lParam )
{
    HWND *list;
    int i;

    USER_CheckNotLock();

    if (!(list = list_window_children( GetDesktopWindow(), 0, id ))) return TRUE;

    /* Now call the callback function for every window */

    for (i = 0; list[i]; i++)
        if (!func( list[i], lParam )) break;
    HeapFree( GetProcessHeap(), 0, list );
    return TRUE;
}


/**********************************************************************
 *           WIN_EnumChildWindows
 *
 * Helper function for EnumChildWindows().
 */
static BOOL WIN_EnumChildWindows( HWND *list, WNDENUMPROC func, LPARAM lParam )
{
    HWND *childList;
    BOOL ret = FALSE;

    for ( ; *list; list++)
    {
        /* Make sure that the window still exists */
        if (!IsWindow( *list )) continue;
        /* skip owned windows */
        if (GetWindow( *list, GW_OWNER )) continue;
        /* Build children list first */
        childList = WIN_ListChildren( *list );

        ret = func( *list, lParam );

        if (childList)
        {
            if (ret) ret = WIN_EnumChildWindows( childList, func, lParam );
            HeapFree( GetProcessHeap(), 0, childList );
        }
        if (!ret) return FALSE;
    }
    return TRUE;
}


/**********************************************************************
 *		EnumChildWindows (USER32.@)
 */
BOOL WINAPI EnumChildWindows( HWND parent, WNDENUMPROC func, LPARAM lParam )
{
    HWND *list;

    USER_CheckNotLock();

    if (!(list = WIN_ListChildren( parent ))) return FALSE;
    WIN_EnumChildWindows( list, func, lParam );
    HeapFree( GetProcessHeap(), 0, list );
    return TRUE;
}


/*******************************************************************
 *		AnyPopup (USER.52)
 */
BOOL16 WINAPI AnyPopup16(void)
{
    return AnyPopup();
}


/*******************************************************************
 *		AnyPopup (USER32.@)
 */
BOOL WINAPI AnyPopup(void)
{
    int i;
    BOOL retvalue;
    HWND *list = WIN_ListChildren( GetDesktopWindow() );

    if (!list) return FALSE;
    for (i = 0; list[i]; i++)
    {
        if (IsWindowVisible( list[i] ) && GetWindow( list[i], GW_OWNER )) break;
    }
    retvalue = (list[i] != 0);
    HeapFree( GetProcessHeap(), 0, list );
    return retvalue;
}


/*******************************************************************
 *		FlashWindow (USER32.@)
 */
BOOL WINAPI FlashWindow( HWND hWnd, BOOL bInvert )
{
    WND *wndPtr;

    TRACE("%p\n", hWnd);

    if (IsIconic( hWnd ))
    {
        RedrawWindow( hWnd, 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW | RDW_FRAME );

        wndPtr = WIN_GetPtr(hWnd);
        if (!wndPtr || wndPtr == WND_OTHER_PROCESS || wndPtr == WND_DESKTOP) return FALSE;
        if (bInvert && !(wndPtr->flags & WIN_NCACTIVATED))
        {
            wndPtr->flags |= WIN_NCACTIVATED;
        }
        else
        {
            wndPtr->flags &= ~WIN_NCACTIVATED;
        }
        WIN_ReleasePtr( wndPtr );
        return TRUE;
    }
    else
    {
        WPARAM wparam;

        wndPtr = WIN_GetPtr(hWnd);
        if (!wndPtr || wndPtr == WND_OTHER_PROCESS || wndPtr == WND_DESKTOP) return FALSE;
        hWnd = wndPtr->hwndSelf;  /* make it a full handle */

        if (bInvert) wparam = !(wndPtr->flags & WIN_NCACTIVATED);
        else wparam = (hWnd == GetForegroundWindow());

        WIN_ReleasePtr( wndPtr );
        SendMessageW( hWnd, WM_NCACTIVATE, wparam, (LPARAM)0 );
        return wparam;
    }
}

/*******************************************************************
 *		FlashWindowEx (USER32.@)
 */
BOOL WINAPI FlashWindowEx( PFLASHWINFO pfwi )
{
    FIXME("%p\n", pfwi);
    return TRUE;
}

/*******************************************************************
 *		GetWindowContextHelpId (USER32.@)
 */
DWORD WINAPI GetWindowContextHelpId( HWND hwnd )
{
    DWORD retval;
    WND *wnd = WIN_GetPtr( hwnd );
    if (!wnd || wnd == WND_DESKTOP) return 0;
    if (wnd == WND_OTHER_PROCESS)
    {
        if (IsWindow( hwnd )) FIXME( "not supported on other process window %p\n", hwnd );
        return 0;
    }
    retval = wnd->helpContext;
    WIN_ReleasePtr( wnd );
    return retval;
}


/*******************************************************************
 *		SetWindowContextHelpId (USER32.@)
 */
BOOL WINAPI SetWindowContextHelpId( HWND hwnd, DWORD id )
{
    WND *wnd = WIN_GetPtr( hwnd );
    if (!wnd || wnd == WND_DESKTOP) return FALSE;
    if (wnd == WND_OTHER_PROCESS)
    {
        if (IsWindow( hwnd )) FIXME( "not supported on other process window %p\n", hwnd );
        return 0;
    }
    wnd->helpContext = id;
    WIN_ReleasePtr( wnd );
    return TRUE;
}


/*******************************************************************
 *		DragDetect (USER32.@)
 */
BOOL WINAPI DragDetect( HWND hWnd, POINT pt )
{
    MSG msg;
    RECT rect;
    WORD wDragWidth = GetSystemMetrics(SM_CXDRAG);
    WORD wDragHeight= GetSystemMetrics(SM_CYDRAG);

    rect.left = pt.x - wDragWidth;
    rect.right = pt.x + wDragWidth;

    rect.top = pt.y - wDragHeight;
    rect.bottom = pt.y + wDragHeight;

    SetCapture(hWnd);

    while(1)
    {
        while (PeekMessageW( &msg, 0, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE ))
        {
            if( msg.message == WM_LBUTTONUP )
            {
                ReleaseCapture();
                return 0;
            }
            if( msg.message == WM_MOUSEMOVE )
            {
                POINT tmp;
                tmp.x = LOWORD(msg.lParam);
                tmp.y = HIWORD(msg.lParam);
                if( !PtInRect( &rect, tmp ))
                {
                    ReleaseCapture();
                    return 1;
                }
            }
        }
        WaitMessage();
    }
    return 0;
}

/******************************************************************************
 *		GetWindowModuleFileNameA (USER32.@)
 */
UINT WINAPI GetWindowModuleFileNameA( HWND hwnd, LPSTR lpszFileName, UINT cchFileNameMax)
{
    FIXME("GetWindowModuleFileNameA(hwnd %p, lpszFileName %p, cchFileNameMax %u) stub!\n",
          hwnd, lpszFileName, cchFileNameMax);
    return 0;
}

/******************************************************************************
 *		GetWindowModuleFileNameW (USER32.@)
 */
UINT WINAPI GetWindowModuleFileNameW( HWND hwnd, LPWSTR lpszFileName, UINT cchFileNameMax)
{
    FIXME("GetWindowModuleFileNameW(hwnd %p, lpszFileName %p, cchFileNameMax %u) stub!\n",
          hwnd, lpszFileName, cchFileNameMax);
    return 0;
}

/******************************************************************************
 *              GetWindowInfo (USER32.@)
 *
 * Note: tests show that Windows doesn't check cbSize of the structure.
 */
BOOL WINAPI GetWindowInfo( HWND hwnd, PWINDOWINFO pwi)
{
    if (!pwi) return FALSE;
    if (!IsWindow(hwnd)) return FALSE;

    GetWindowRect(hwnd, &pwi->rcWindow);
    GetClientRect(hwnd, &pwi->rcClient);
    /* translate to screen coordinates */
    MapWindowPoints(hwnd, 0, (LPPOINT)&pwi->rcClient, 2);

    pwi->dwStyle = GetWindowLongW(hwnd, GWL_STYLE);
    pwi->dwExStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    pwi->dwWindowStatus = ((GetActiveWindow() == hwnd) ? WS_ACTIVECAPTION : 0);

    pwi->cxWindowBorders = pwi->rcClient.left - pwi->rcWindow.left;
    pwi->cyWindowBorders = pwi->rcWindow.bottom - pwi->rcClient.bottom;

    pwi->atomWindowType = GetClassLongW( hwnd, GCW_ATOM );
    pwi->wCreatorVersion = 0x0400;

    return TRUE;
}

/******************************************************************************
 *              SwitchDesktop (USER32.@)
 *
 * NOTES: Sets the current input or interactive desktop.
 */
BOOL WINAPI SwitchDesktop( HDESK hDesktop)
{
    FIXME("SwitchDesktop(hwnd %p) stub!\n", hDesktop);
    return TRUE;
}

/*****************************************************************************
 *              SetLayeredWindowAttributes (USER32.@)
 */
BOOL WINAPI SetLayeredWindowAttributes( HWND hWnd, COLORREF rgbKey, 
                                        BYTE bAlpha, DWORD dwFlags )
{
    FIXME("(%p,0x%.8lx,%d,%ld): stub!\n", hWnd, rgbKey, bAlpha, dwFlags);
    return TRUE;
}
