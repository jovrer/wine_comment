/*
 * 16-bit messaging support
 *
 * Copyright 2001 Alexandre Julliard
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

#include "wine/winuser16.h"
#include "wownt32.h"
#include "winerror.h"
#include "win.h"
#include "user_private.h"
#include "winproc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msg);

DWORD USER16_AlertableWait = 0;

/***********************************************************************
 *		SendMessage  (USER.111)
 */
LRESULT WINAPI SendMessage16( HWND16 hwnd16, UINT16 msg, WPARAM16 wparam, LPARAM lparam )
{
    LRESULT result;
    UINT msg32;
    WPARAM wparam32;
    HWND hwnd = WIN_Handle32( hwnd16 );

    if (hwnd != HWND_BROADCAST && WIN_IsCurrentThread(hwnd))
    {
        /* call 16-bit window proc directly */
        WNDPROC16 winproc;

        /* first the WH_CALLWNDPROC hook */
        if (HOOK_IsHooked( WH_CALLWNDPROC ))
        {
            LPARAM lparam32 = lparam;

            if (WINPROC_MapMsg16To32A( hwnd, msg, wparam, &msg32, &wparam32, &lparam32 ) != -1)
            {
                CWPSTRUCT cwp;

                cwp.hwnd    = hwnd;
                cwp.message = msg32;
                cwp.wParam  = wparam32;
                cwp.lParam  = lparam32;
                HOOK_CallHooks( WH_CALLWNDPROC, HC_ACTION, 1, (LPARAM)&cwp, FALSE );
                WINPROC_UnmapMsg16To32A( hwnd, msg32, wparam32, lparam32, 0 );
                /* FIXME: should reflect changes back into the message we send */
            }
        }

        if (!(winproc = (WNDPROC16)GetWindowLong16( hwnd16, GWLP_WNDPROC ))) return 0;

        SPY_EnterMessage( SPY_SENDMESSAGE16, hwnd, msg, wparam, lparam );
        result = CallWindowProc16( (WNDPROC16)winproc, hwnd16, msg, wparam, lparam );
        SPY_ExitMessage( SPY_RESULT_OK16, hwnd, msg, result, wparam, lparam );
    }
    else  /* map to 32-bit unicode for inter-thread/process message */
    {
        if (WINPROC_MapMsg16To32W( hwnd, msg, wparam, &msg32, &wparam32, &lparam ) == -1)
            return 0;
        result = WINPROC_UnmapMsg16To32W( hwnd, msg32, wparam32, lparam,
                                          SendMessageW( hwnd, msg32, wparam32, lparam ),
                                          NULL );
    }
    return result;
}


/***********************************************************************
 *		PostMessage  (USER.110)
 */
BOOL16 WINAPI PostMessage16( HWND16 hwnd16, UINT16 msg, WPARAM16 wparam, LPARAM lparam )
{
    WPARAM wparam32;
    UINT msg32;
    HWND hwnd = WIN_Handle32( hwnd16 );

    switch (WINPROC_MapMsg16To32W( hwnd, msg, wparam, &msg32, &wparam32, &lparam ))
    {
    case 0:
        return PostMessageW( hwnd, msg32, wparam32, lparam );
    case 1:
        ERR( "16-bit message 0x%04x contains pointer, cannot post\n", msg );
        return FALSE;
    default:
        return FALSE;
    }
}


/***********************************************************************
 *		PostAppMessage (USER.116)
 */
BOOL16 WINAPI PostAppMessage16( HTASK16 hTask, UINT16 msg, WPARAM16 wparam, LPARAM lparam )
{
    WPARAM wparam32;
    UINT msg32;
    DWORD tid = HTASK_32( hTask );
    if (!tid) return FALSE;

    switch (WINPROC_MapMsg16To32W( 0, msg, wparam, &msg32, &wparam32, &lparam ))
    {
    case 0:
        return PostThreadMessageW( tid, msg32, wparam32, lparam );
    case 1:
        ERR( "16-bit message %x contains pointer, cannot post\n", msg );
        return FALSE;
    default:
        return FALSE;
    }
}


/***********************************************************************
 *		InSendMessage  (USER.192)
 */
BOOL16 WINAPI InSendMessage16(void)
{
    return InSendMessage();
}


/***********************************************************************
 *		ReplyMessage  (USER.115)
 */
void WINAPI ReplyMessage16( LRESULT result )
{
    ReplyMessage( result );
}


/***********************************************************************
 *		PeekMessage32 (USER.819)
 */
BOOL16 WINAPI PeekMessage32_16( MSG32_16 *msg16, HWND16 hwnd16,
                                UINT16 first, UINT16 last, UINT16 flags,
                                BOOL16 wHaveParamHigh )
{
    MSG msg;
    HWND hwnd = WIN_Handle32( hwnd16 );

    if(USER16_AlertableWait)
        MsgWaitForMultipleObjectsEx( 0, NULL, 1, 0, MWMO_ALERTABLE );
    if (!PeekMessageW( &msg, hwnd, first, last, flags )) return FALSE;

    msg16->msg.hwnd    = HWND_16( msg.hwnd );
    msg16->msg.lParam  = msg.lParam;
    msg16->msg.time    = msg.time;
    msg16->msg.pt.x    = (INT16)msg.pt.x;
    msg16->msg.pt.y    = (INT16)msg.pt.y;
    if (wHaveParamHigh) msg16->wParamHigh = HIWORD(msg.wParam);

    return (WINPROC_MapMsg32WTo16( msg.hwnd, msg.message, msg.wParam,
                                   &msg16->msg.message, &msg16->msg.wParam,
                                   &msg16->msg.lParam ) != -1);
}


/***********************************************************************
 *		DefWindowProc (USER.107)
 */
LRESULT WINAPI DefWindowProc16( HWND16 hwnd16, UINT16 msg, WPARAM16 wParam, LPARAM lParam )
{
    LRESULT result;
    HWND hwnd = WIN_Handle32( hwnd16 );

    SPY_EnterMessage( SPY_DEFWNDPROC16, hwnd, msg, wParam, lParam );

    switch(msg)
    {
    case WM_NCCREATE:
        {
            CREATESTRUCT16 *cs16 = MapSL(lParam);
            CREATESTRUCTA cs32;

            cs32.lpCreateParams = (LPVOID)cs16->lpCreateParams;
            cs32.hInstance      = HINSTANCE_32(cs16->hInstance);
            cs32.hMenu          = HMENU_32(cs16->hMenu);
            cs32.hwndParent     = WIN_Handle32(cs16->hwndParent);
            cs32.cy             = cs16->cy;
            cs32.cx             = cs16->cx;
            cs32.y              = cs16->y;
            cs32.x              = cs16->x;
            cs32.style          = cs16->style;
            cs32.dwExStyle      = cs16->dwExStyle;
            cs32.lpszName       = MapSL(cs16->lpszName);
            cs32.lpszClass      = MapSL(cs16->lpszClass);
            result = DefWindowProcA( hwnd, msg, wParam, (LPARAM)&cs32 );
        }
        break;

    case WM_NCCALCSIZE:
        {
            RECT16 *rect16 = MapSL(lParam);
            RECT rect32;

            rect32.left    = rect16->left;
            rect32.top     = rect16->top;
            rect32.right   = rect16->right;
            rect32.bottom  = rect16->bottom;

            result = DefWindowProcA( hwnd, msg, wParam, (LPARAM)&rect32 );

            rect16->left   = rect32.left;
            rect16->top    = rect32.top;
            rect16->right  = rect32.right;
            rect16->bottom = rect32.bottom;
        }
        break;

    case WM_WINDOWPOSCHANGING:
    case WM_WINDOWPOSCHANGED:
        {
            WINDOWPOS16 *pos16 = MapSL(lParam);
            WINDOWPOS pos32;

            pos32.hwnd             = WIN_Handle32(pos16->hwnd);
            pos32.hwndInsertAfter  = WIN_Handle32(pos16->hwndInsertAfter);
            pos32.x                = pos16->x;
            pos32.y                = pos16->y;
            pos32.cx               = pos16->cx;
            pos32.cy               = pos16->cy;
            pos32.flags            = pos16->flags;

            result = DefWindowProcA( hwnd, msg, wParam, (LPARAM)&pos32 );

            pos16->hwnd            = HWND_16(pos32.hwnd);
            pos16->hwndInsertAfter = HWND_16(pos32.hwndInsertAfter);
            pos16->x               = pos32.x;
            pos16->y               = pos32.y;
            pos16->cx              = pos32.cx;
            pos16->cy              = pos32.cy;
            pos16->flags           = pos32.flags;
        }
        break;

    case WM_GETTEXT:
    case WM_SETTEXT:
        result = DefWindowProcA( hwnd, msg, wParam, (LPARAM)MapSL(lParam) );
        break;

    default:
        result = DefWindowProcA( hwnd, msg, wParam, lParam );
        break;
    }

    SPY_ExitMessage( SPY_RESULT_DEFWND16, hwnd, msg, result, wParam, lParam );
    return result;
}


/***********************************************************************
 *		PeekMessage  (USER.109)
 */
BOOL16 WINAPI PeekMessage16( MSG16 *msg, HWND16 hwnd,
                             UINT16 first, UINT16 last, UINT16 flags )
{
    return PeekMessage32_16( (MSG32_16 *)msg, hwnd, first, last, flags, FALSE );
}


/***********************************************************************
 *		GetMessage32  (USER.820)
 */
BOOL16 WINAPI GetMessage32_16( MSG32_16 *msg16, HWND16 hwnd16, UINT16 first,
                               UINT16 last, BOOL16 wHaveParamHigh )
{
    MSG msg;
    HWND hwnd = WIN_Handle32( hwnd16 );

    do
    {
        if(USER16_AlertableWait)
            MsgWaitForMultipleObjectsEx( 0, NULL, INFINITE, 0, MWMO_ALERTABLE );
        GetMessageW( &msg, hwnd, first, last );
        msg16->msg.hwnd    = HWND_16( msg.hwnd );
        msg16->msg.lParam  = msg.lParam;
        msg16->msg.time    = msg.time;
        msg16->msg.pt.x    = (INT16)msg.pt.x;
        msg16->msg.pt.y    = (INT16)msg.pt.y;
        if (wHaveParamHigh) msg16->wParamHigh = HIWORD(msg.wParam);
    }
    while (WINPROC_MapMsg32WTo16( msg.hwnd, msg.message, msg.wParam,
                                  &msg16->msg.message, &msg16->msg.wParam,
                                  &msg16->msg.lParam ) == -1);

    TRACE( "message %04x, hwnd %p, filter(%04x - %04x)\n",
           msg16->msg.message, hwnd, first, last );

    return msg16->msg.message != WM_QUIT;
}


/***********************************************************************
 *		GetMessage  (USER.108)
 */
BOOL16 WINAPI GetMessage16( MSG16 *msg, HWND16 hwnd, UINT16 first, UINT16 last )
{
    return GetMessage32_16( (MSG32_16 *)msg, hwnd, first, last, FALSE );
}


/***********************************************************************
 *		TranslateMessage32 (USER.821)
 */
BOOL16 WINAPI TranslateMessage32_16( const MSG32_16 *msg, BOOL16 wHaveParamHigh )
{
    MSG msg32;

    msg32.hwnd    = WIN_Handle32( msg->msg.hwnd );
    msg32.message = msg->msg.message;
    msg32.wParam  = MAKEWPARAM( msg->msg.wParam, wHaveParamHigh ? msg->wParamHigh : 0 );
    msg32.lParam  = msg->msg.lParam;
    return TranslateMessage( &msg32 );
}


/***********************************************************************
 *		TranslateMessage (USER.113)
 */
BOOL16 WINAPI TranslateMessage16( const MSG16 *msg )
{
    return TranslateMessage32_16( (const MSG32_16 *)msg, FALSE );
}


/***********************************************************************
 *		DispatchMessage (USER.114)
 */
LONG WINAPI DispatchMessage16( const MSG16* msg )
{
    WND * wndPtr;
    WNDPROC16 winproc;
    LONG retval;
    HWND hwnd = WIN_Handle32( msg->hwnd );

      /* Process timer messages */
    if ((msg->message == WM_TIMER) || (msg->message == WM_SYSTIMER))
    {
        if (msg->lParam)
            return CallWindowProc16( (WNDPROC16)msg->lParam, msg->hwnd,
                                     msg->message, msg->wParam, GetTickCount() );
    }

    if (!(wndPtr = WIN_GetPtr( hwnd )))
    {
        if (msg->hwnd) SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }
    if (wndPtr == WND_OTHER_PROCESS || wndPtr == WND_DESKTOP)
    {
        if (IsWindow( hwnd )) SetLastError( ERROR_MESSAGE_SYNC_ONLY );
        else SetLastError( ERROR_INVALID_WINDOW_HANDLE );
        return 0;
    }
    winproc = (WNDPROC16)wndPtr->winproc;
    WIN_ReleasePtr( wndPtr );

    SPY_EnterMessage( SPY_DISPATCHMESSAGE16, hwnd, msg->message, msg->wParam, msg->lParam );
    retval = CallWindowProc16( winproc, msg->hwnd, msg->message, msg->wParam, msg->lParam );
    SPY_ExitMessage( SPY_RESULT_OK16, hwnd, msg->message, retval, msg->wParam, msg->lParam );

    return retval;
}


/***********************************************************************
 *		DispatchMessage32 (USER.822)
 */
LONG WINAPI DispatchMessage32_16( const MSG32_16 *msg16, BOOL16 wHaveParamHigh )
{
    if (wHaveParamHigh == FALSE)
        return DispatchMessage16( &msg16->msg );
    else
    {
        MSG msg;

        msg.hwnd    = WIN_Handle32( msg16->msg.hwnd );
        msg.message = msg16->msg.message;
        msg.wParam  = MAKEWPARAM( msg16->msg.wParam, msg16->wParamHigh );
        msg.lParam  = msg16->msg.lParam;
        msg.time    = msg16->msg.time;
        msg.pt.x    = msg16->msg.pt.x;
        msg.pt.y    = msg16->msg.pt.y;
        return DispatchMessageA( &msg );
    }
}


/***********************************************************************
 *		IsDialogMessage (USER.90)
 */
BOOL16 WINAPI IsDialogMessage16( HWND16 hwndDlg, MSG16 *msg16 )
{
    MSG msg;
    HWND hwndDlg32;

    msg.hwnd  = WIN_Handle32(msg16->hwnd);
    hwndDlg32 = WIN_Handle32(hwndDlg);

    switch(msg16->message)
    {
    case WM_KEYDOWN:
    case WM_CHAR:
    case WM_SYSCHAR:
        msg.lParam = msg16->lParam;
        WINPROC_MapMsg16To32W( msg.hwnd, msg16->message, msg16->wParam,
                               &msg.message, &msg.wParam, &msg.lParam );
        /* these messages don't need an unmap */
        return IsDialogMessageW( hwndDlg32, &msg );
    }

    if ((hwndDlg32 != msg.hwnd) && !IsChild( hwndDlg32, msg.hwnd )) return FALSE;
    TranslateMessage16( msg16 );
    DispatchMessage16( msg16 );
    return TRUE;
}


/***********************************************************************
 *		MsgWaitForMultipleObjects  (USER.640)
 */
DWORD WINAPI MsgWaitForMultipleObjects16( DWORD count, CONST HANDLE *handles,
                                          BOOL wait_all, DWORD timeout, DWORD mask )
{
    return MsgWaitForMultipleObjectsEx( count, handles, timeout, mask,
                                        wait_all ? MWMO_WAITALL : 0 );
}


/**********************************************************************
 *		SetDoubleClickTime (USER.20)
 */
void WINAPI SetDoubleClickTime16( UINT16 interval )
{
    SetDoubleClickTime( interval );
}


/**********************************************************************
 *		GetDoubleClickTime (USER.21)
 */
UINT16 WINAPI GetDoubleClickTime16(void)
{
    return GetDoubleClickTime();
}


/***********************************************************************
 *		PostQuitMessage (USER.6)
 */
void WINAPI PostQuitMessage16( INT16 exitCode )
{
    PostQuitMessage( exitCode );
}


/**********************************************************************
 *		GetKeyState (USER.106)
 */
INT16 WINAPI GetKeyState16(INT16 vkey)
{
    return GetKeyState(vkey);
}


/**********************************************************************
 *		GetKeyboardState (USER.222)
 */
BOOL WINAPI GetKeyboardState16( LPBYTE state )
{
    return GetKeyboardState( state );
}


/**********************************************************************
 *		SetKeyboardState (USER.223)
 */
BOOL WINAPI SetKeyboardState16( LPBYTE state )
{
    return SetKeyboardState( state );
}


/***********************************************************************
 *		SetMessageQueue (USER.266)
 */
BOOL16 WINAPI SetMessageQueue16( INT16 size )
{
    return SetMessageQueue( size );
}


/***********************************************************************
 *		GetQueueStatus (USER.334)
 */
DWORD WINAPI GetQueueStatus16( UINT16 flags )
{
    return GetQueueStatus( flags );
}


/***********************************************************************
 *		GetInputState (USER.335)
 */
BOOL16 WINAPI GetInputState16(void)
{
    return GetInputState();
}


/**********************************************************************
 *           TranslateAccelerator      (USER.178)
 */
INT16 WINAPI TranslateAccelerator16( HWND16 hwnd, HACCEL16 hAccel, LPMSG16 msg )
{
    MSG msg32;

    if (!msg) return 0;
    msg32.message = msg->message;
    /* msg32.hwnd not used */
    msg32.wParam  = msg->wParam;
    msg32.lParam  = msg->lParam;
    return TranslateAcceleratorW( WIN_Handle32(hwnd), HACCEL_32(hAccel), &msg32 );
}


/**********************************************************************
 *		TranslateMDISysAccel (USER.451)
 */
BOOL16 WINAPI TranslateMDISysAccel16( HWND16 hwndClient, LPMSG16 msg )
{
    if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
    {
        MSG msg32;
        msg32.hwnd    = WIN_Handle32(msg->hwnd);
        msg32.message = msg->message;
        msg32.wParam  = msg->wParam;
        msg32.lParam  = msg->lParam;
        /* MDICLIENTINFO is still the same for win32 and win16 ... */
        return TranslateMDISysAccel( WIN_Handle32(hwndClient), &msg32 );
    }
    return 0;
}
