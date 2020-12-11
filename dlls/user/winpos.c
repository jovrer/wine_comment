/*
 * Window position related functions.
 *
 * Copyright 1993, 1994, 1995 Alexandre Julliard
 *                       1995, 1996, 1999 Alex Korobka
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
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winerror.h"
#include "ntstatus.h"
#include "wine/winuser16.h"
#include "wine/server.h"
#include "controls.h"
#include "user_private.h"
#include "win.h"
#include "winpos.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win);

#define HAS_DLGFRAME(style,exStyle) \
    (((exStyle) & WS_EX_DLGMODALFRAME) || \
     (((style) & WS_DLGFRAME) && !((style) & WS_BORDER)))

#define HAS_THICKFRAME(style) \
    (((style) & WS_THICKFRAME) && \
     !(((style) & (WS_DLGFRAME|WS_BORDER)) == WS_DLGFRAME))

#define EMPTYPOINT(pt)          ((*(LONG*)&(pt)) == -1)

#define PLACE_MIN		0x0001
#define PLACE_MAX		0x0002
#define PLACE_RECT		0x0004


#define DWP_MAGIC  ((INT)('W' | ('P' << 8) | ('O' << 16) | ('S' << 24)))

typedef struct
{
    INT       actualCount;
    INT       suggestedCount;
    BOOL      valid;
    INT       wMagic;
    HWND      hwndParent;
    WINDOWPOS winPos[1];
} DWP;

typedef struct
{
    RECT16   rectNormal;
    POINT16  ptIconPos;
    POINT16  ptMaxPos;
    HWND     hwndIconTitle;
} INTERNALPOS, *LPINTERNALPOS;

/* ----- internal functions ----- */

static inline INTERNALPOS *get_internal_pos( HWND hwnd )
{
    return GetPropA( hwnd, "SysIP" );
}

static inline void set_internal_pos( HWND hwnd, INTERNALPOS *pos )
{
    SetPropA( hwnd, "SysIP", pos );
}

/***********************************************************************
 *           WINPOS_CheckInternalPos
 *
 * Called when a window is destroyed.
 */
void WINPOS_CheckInternalPos( HWND hwnd )
{
    LPINTERNALPOS lpPos = get_internal_pos( hwnd );

    if ( lpPos )
    {
	if( IsWindow(lpPos->hwndIconTitle) )
	    DestroyWindow( lpPos->hwndIconTitle );
	HeapFree( GetProcessHeap(), 0, lpPos );
    }
}

/***********************************************************************
 *		ArrangeIconicWindows (USER32.@)
 */
UINT WINAPI ArrangeIconicWindows( HWND parent )
{
    RECT rectParent;
    HWND hwndChild;
    INT x, y, xspacing, yspacing;

    GetClientRect( parent, &rectParent );
    x = rectParent.left;
    y = rectParent.bottom;
    xspacing = GetSystemMetrics(SM_CXICONSPACING);
    yspacing = GetSystemMetrics(SM_CYICONSPACING);

    hwndChild = GetWindow( parent, GW_CHILD );
    while (hwndChild)
    {
        if( IsIconic( hwndChild ) )
        {
            WINPOS_ShowIconTitle( hwndChild, FALSE );

            SetWindowPos( hwndChild, 0, x + (xspacing - GetSystemMetrics(SM_CXICON)) / 2,
                            y - yspacing - GetSystemMetrics(SM_CYICON)/2, 0, 0,
                            SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
	    if( IsWindow(hwndChild) )
                WINPOS_ShowIconTitle(hwndChild , TRUE );

            if (x <= rectParent.right - xspacing) x += xspacing;
            else
            {
                x = rectParent.left;
                y -= yspacing;
            }
        }
        hwndChild = GetWindow( hwndChild, GW_HWNDNEXT );
    }
    return yspacing;
}


/***********************************************************************
 *		SwitchToThisWindow (USER32.@)
 */
void WINAPI SwitchToThisWindow( HWND hwnd, BOOL restore )
{
    ShowWindow( hwnd, restore ? SW_RESTORE : SW_SHOWMINIMIZED );
}


/***********************************************************************
 *		GetWindowRect (USER32.@)
 */
BOOL WINAPI GetWindowRect( HWND hwnd, LPRECT rect )
{
    BOOL ret = WIN_GetRectangles( hwnd, rect, NULL );
    if (ret)
    {
        MapWindowPoints( GetAncestor( hwnd, GA_PARENT ), 0, (POINT *)rect, 2 );
        TRACE( "hwnd %p (%ld,%ld)-(%ld,%ld)\n",
               hwnd, rect->left, rect->top, rect->right, rect->bottom);
    }
    return ret;
}


/***********************************************************************
 *		GetWindowRgn (USER32.@)
 */
int WINAPI GetWindowRgn ( HWND hwnd, HRGN hrgn )
{
    int nRet = ERROR;
    NTSTATUS status;
    HRGN win_rgn = 0;
    RGNDATA *data;
    size_t size = 256;

    do
    {
        if (!(data = HeapAlloc( GetProcessHeap(), 0, sizeof(*data) + size - 1 )))
        {
            SetLastError( ERROR_OUTOFMEMORY );
            return ERROR;
        }
        SERVER_START_REQ( get_window_region )
        {
            req->window = hwnd;
            wine_server_set_reply( req, data->Buffer, size );
            if (!(status = wine_server_call( req )))
            {
                size_t reply_size = wine_server_reply_size( reply );
                if (reply_size)
                {
                    data->rdh.dwSize   = sizeof(data->rdh);
                    data->rdh.iType    = RDH_RECTANGLES;
                    data->rdh.nCount   = reply_size / sizeof(RECT);
                    data->rdh.nRgnSize = reply_size;
                    win_rgn = ExtCreateRegion( NULL, size, data );
                }
            }
            else size = reply->total_size;
        }
        SERVER_END_REQ;
        HeapFree( GetProcessHeap(), 0, data );
    } while (status == STATUS_BUFFER_OVERFLOW);

    if (status) SetLastError( RtlNtStatusToDosError(status) );
    else if (win_rgn)
    {
        nRet = CombineRgn( hrgn, win_rgn, 0, RGN_COPY );
        DeleteObject( win_rgn );
    }
    return nRet;
}


/***********************************************************************
 *		SetWindowRgn (USER32.@)
 */
int WINAPI SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL bRedraw )
{
    static const RECT empty_rect;
    BOOL ret;

    if (hrgn)
    {
        RGNDATA *data;
        DWORD size;

        if (!(size = GetRegionData( hrgn, 0, NULL ))) return FALSE;
        if (!(data = HeapAlloc( GetProcessHeap(), 0, size ))) return FALSE;
        if (!GetRegionData( hrgn, size, data ))
        {
            HeapFree( GetProcessHeap(), 0, data );
            return FALSE;
        }
        SERVER_START_REQ( set_window_region )
        {
            req->window = hwnd;
            if (data->rdh.nCount)
                wine_server_add_data( req, data->Buffer, data->rdh.nCount * sizeof(RECT) );
            else
                wine_server_add_data( req, &empty_rect, sizeof(empty_rect) );
            ret = !wine_server_call_err( req );
        }
        SERVER_END_REQ;
    }
    else  /* clear existing region */
    {
        SERVER_START_REQ( set_window_region )
        {
            req->window = hwnd;
            ret = !wine_server_call_err( req );
        }
        SERVER_END_REQ;
    }

    if (ret) ret = USER_Driver->pSetWindowRgn( hwnd, hrgn, bRedraw );

    if (ret && bRedraw) RedrawWindow( hwnd, NULL, 0, RDW_FRAME | RDW_INVALIDATE | RDW_ERASE );
    return ret;
}


/***********************************************************************
 *		GetClientRect (USER32.@)
 */
BOOL WINAPI GetClientRect( HWND hwnd, LPRECT rect )
{
    BOOL ret;

    if ((ret = WIN_GetRectangles( hwnd, NULL, rect )))
    {
        rect->right -= rect->left;
        rect->bottom -= rect->top;
        rect->left = rect->top = 0;
    }
    return ret;
}


/*******************************************************************
 *		ClientToScreen (USER32.@)
 */
BOOL WINAPI ClientToScreen( HWND hwnd, LPPOINT lppnt )
{
    MapWindowPoints( hwnd, 0, lppnt, 1 );
    return TRUE;
}


/*******************************************************************
 *		ScreenToClient (USER32.@)
 */
BOOL WINAPI ScreenToClient( HWND hwnd, LPPOINT lppnt )
{
    MapWindowPoints( 0, hwnd, lppnt, 1 );
    return TRUE;
}


/***********************************************************************
 *           list_children_from_point
 *
 * Get the list of children that can contain point from the server.
 * Point is in screen coordinates.
 * Returned list must be freed by caller.
 */
static HWND *list_children_from_point( HWND hwnd, POINT pt )
{
    HWND *list;
    int size = 32;

    for (;;)
    {
        int count = 0;

        if (!(list = HeapAlloc( GetProcessHeap(), 0, size * sizeof(HWND) ))) break;

        SERVER_START_REQ( get_window_children_from_point )
        {
            req->parent = hwnd;
            req->x = pt.x;
            req->y = pt.y;
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


/***********************************************************************
 *           WINPOS_WindowFromPoint
 *
 * Find the window and hittest for a given point.
 */
HWND WINPOS_WindowFromPoint( HWND hwndScope, POINT pt, INT *hittest )
{
    int i, res;
    HWND ret, *list;

    if (!hwndScope) hwndScope = GetDesktopWindow();

    *hittest = HTNOWHERE;

    if (!(list = list_children_from_point( hwndScope, pt ))) return 0;

    /* now determine the hittest */

    for (i = 0; list[i]; i++)
    {
        LONG style = GetWindowLongW( list[i], GWL_STYLE );

        /* If window is minimized or disabled, return at once */
        if (style & WS_MINIMIZE)
        {
            *hittest = HTCAPTION;
            break;
        }
        if (style & WS_DISABLED)
        {
            *hittest = HTERROR;
            break;
        }
        /* Send WM_NCCHITTEST (if same thread) */
        if (!WIN_IsCurrentThread( list[i] ))
        {
            *hittest = HTCLIENT;
            break;
        }
        res = SendMessageA( list[i], WM_NCHITTEST, 0, MAKELONG(pt.x,pt.y) );
        if (res != HTTRANSPARENT)
        {
            *hittest = res;  /* Found the window */
            break;
        }
        /* continue search with next window in z-order */
    }
    ret = list[i];
    HeapFree( GetProcessHeap(), 0, list );
    TRACE( "scope %p (%ld,%ld) returning %p\n", hwndScope, pt.x, pt.y, ret );
    return ret;
}


/*******************************************************************
 *		WindowFromPoint (USER32.@)
 */
HWND WINAPI WindowFromPoint( POINT pt )
{
    INT hittest;
    return WINPOS_WindowFromPoint( 0, pt, &hittest );
}


/*******************************************************************
 *		ChildWindowFromPoint (USER32.@)
 */
HWND WINAPI ChildWindowFromPoint( HWND hwndParent, POINT pt )
{
    return ChildWindowFromPointEx( hwndParent, pt, CWP_ALL );
}

/*******************************************************************
 *		ChildWindowFromPointEx (USER32.@)
 */
HWND WINAPI ChildWindowFromPointEx( HWND hwndParent, POINT pt, UINT uFlags)
{
    /* pt is in the client coordinates */
    HWND *list;
    int i;
    RECT rect;
    HWND retvalue;

    GetClientRect( hwndParent, &rect );
    if (!PtInRect( &rect, pt )) return 0;
    if (!(list = WIN_ListChildren( hwndParent ))) return hwndParent;

    for (i = 0; list[i]; i++)
    {
        if (!WIN_GetRectangles( list[i], &rect, NULL )) continue;
        if (!PtInRect( &rect, pt )) continue;
        if (uFlags & (CWP_SKIPINVISIBLE|CWP_SKIPDISABLED))
        {
            LONG style = GetWindowLongW( list[i], GWL_STYLE );
            if ((uFlags & CWP_SKIPINVISIBLE) && !(style & WS_VISIBLE)) continue;
            if ((uFlags & CWP_SKIPDISABLED) && (style & WS_DISABLED)) continue;
        }
        if (uFlags & CWP_SKIPTRANSPARENT)
        {
            if (GetWindowLongW( list[i], GWL_EXSTYLE ) & WS_EX_TRANSPARENT) continue;
        }
        break;
    }
    retvalue = list[i];
    HeapFree( GetProcessHeap(), 0, list );
    if (!retvalue) retvalue = hwndParent;
    return retvalue;
}


/*******************************************************************
 *         WINPOS_GetWinOffset
 *
 * Calculate the offset between the origin of the two windows. Used
 * to implement MapWindowPoints.
 */
static void WINPOS_GetWinOffset( HWND hwndFrom, HWND hwndTo, POINT *offset )
{
    WND * wndPtr;

    offset->x = offset->y = 0;

    /* Translate source window origin to screen coords */
    if (hwndFrom)
    {
        HWND hwnd = hwndFrom;

        while (hwnd)
        {
            if (hwnd == hwndTo) return;
            if (!(wndPtr = WIN_GetPtr( hwnd )))
            {
                ERR( "bad hwndFrom = %p\n", hwnd );
                return;
            }
            if (wndPtr == WND_DESKTOP) break;
            if (wndPtr == WND_OTHER_PROCESS) goto other_process;
            offset->x += wndPtr->rectClient.left;
            offset->y += wndPtr->rectClient.top;
            hwnd = wndPtr->parent;
            WIN_ReleasePtr( wndPtr );
        }
    }

    /* Translate origin to destination window coords */
    if (hwndTo)
    {
        HWND hwnd = hwndTo;

        while (hwnd)
        {
            if (!(wndPtr = WIN_GetPtr( hwnd )))
            {
                ERR( "bad hwndTo = %p\n", hwnd );
                return;
            }
            if (wndPtr == WND_DESKTOP) break;
            if (wndPtr == WND_OTHER_PROCESS) goto other_process;
            offset->x -= wndPtr->rectClient.left;
            offset->y -= wndPtr->rectClient.top;
            hwnd = wndPtr->parent;
            WIN_ReleasePtr( wndPtr );
        }
    }
    return;

 other_process:  /* one of the parents may belong to another process, do it the hard way */
    offset->x = offset->y = 0;
    SERVER_START_REQ( get_windows_offset )
    {
        req->from = hwndFrom;
        req->to   = hwndTo;
        if (!wine_server_call( req ))
        {
            offset->x = reply->x;
            offset->y = reply->y;
        }
    }
    SERVER_END_REQ;
}


/*******************************************************************
 *		MapWindowPoints (USER.258)
 */
void WINAPI MapWindowPoints16( HWND16 hwndFrom, HWND16 hwndTo,
                               LPPOINT16 lppt, UINT16 count )
{
    POINT offset;

    WINPOS_GetWinOffset( WIN_Handle32(hwndFrom), WIN_Handle32(hwndTo), &offset );
    while (count--)
    {
	lppt->x += offset.x;
	lppt->y += offset.y;
        lppt++;
    }
}


/*******************************************************************
 *		MapWindowPoints (USER32.@)
 */
INT WINAPI MapWindowPoints( HWND hwndFrom, HWND hwndTo, LPPOINT lppt, UINT count )
{
    POINT offset;

    WINPOS_GetWinOffset( hwndFrom, hwndTo, &offset );
    while (count--)
    {
	lppt->x += offset.x;
	lppt->y += offset.y;
        lppt++;
    }
    return MAKELONG( LOWORD(offset.x), LOWORD(offset.y) );
}


/***********************************************************************
 *		IsIconic (USER32.@)
 */
BOOL WINAPI IsIconic(HWND hWnd)
{
    return (GetWindowLongW( hWnd, GWL_STYLE ) & WS_MINIMIZE) != 0;
}


/***********************************************************************
 *		IsZoomed (USER32.@)
 */
BOOL WINAPI IsZoomed(HWND hWnd)
{
    return (GetWindowLongW( hWnd, GWL_STYLE ) & WS_MAXIMIZE) != 0;
}


/*******************************************************************
 *		AllowSetForegroundWindow (USER32.@)
 */
BOOL WINAPI AllowSetForegroundWindow( DWORD procid )
{
    /* FIXME: If Win98/2000 style SetForegroundWindow behavior is
     * implemented, then fix this function. */
    return TRUE;
}


/*******************************************************************
 *		LockSetForegroundWindow (USER32.@)
 */
BOOL WINAPI LockSetForegroundWindow( UINT lockcode )
{
    /* FIXME: If Win98/2000 style SetForegroundWindow behavior is
     * implemented, then fix this function. */
    return TRUE;
}


/***********************************************************************
 *		BringWindowToTop (USER32.@)
 */
BOOL WINAPI BringWindowToTop( HWND hwnd )
{
    return SetWindowPos( hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE|SWP_NOSIZE );
}


/***********************************************************************
 *		MoveWindow (USER32.@)
 */
BOOL WINAPI MoveWindow( HWND hwnd, INT x, INT y, INT cx, INT cy,
                            BOOL repaint )
{
    int flags = SWP_NOZORDER | SWP_NOACTIVATE;
    if (!repaint) flags |= SWP_NOREDRAW;
    TRACE("%p %d,%d %dx%d %d\n", hwnd, x, y, cx, cy, repaint );
    return SetWindowPos( hwnd, 0, x, y, cx, cy, flags );
}

/***********************************************************************
 *           WINPOS_InitInternalPos
 */
static LPINTERNALPOS WINPOS_InitInternalPos( WND* wnd )
{
    LPINTERNALPOS lpPos = get_internal_pos( wnd->hwndSelf );
    if( !lpPos )
    {
	/* this happens when the window is minimized/maximized
	 * for the first time (rectWindow is not adjusted yet) */

	lpPos = HeapAlloc( GetProcessHeap(), 0, sizeof(INTERNALPOS) );
	if( !lpPos ) return NULL;

	set_internal_pos( wnd->hwndSelf, lpPos );
	lpPos->hwndIconTitle = 0; /* defer until needs to be shown */
        lpPos->rectNormal.left   = wnd->rectWindow.left;
        lpPos->rectNormal.top    = wnd->rectWindow.top;
        lpPos->rectNormal.right  = wnd->rectWindow.right;
        lpPos->rectNormal.bottom = wnd->rectWindow.bottom;
        lpPos->ptIconPos.x = lpPos->ptIconPos.y = -1;
        lpPos->ptMaxPos.x = lpPos->ptMaxPos.y = -1;
    }

    if( wnd->dwStyle & WS_MINIMIZE )
    {
        lpPos->ptIconPos.x = wnd->rectWindow.left;
        lpPos->ptIconPos.y = wnd->rectWindow.top;
    }
    else if( wnd->dwStyle & WS_MAXIMIZE )
    {
        lpPos->ptMaxPos.x = wnd->rectWindow.left;
        lpPos->ptMaxPos.y = wnd->rectWindow.top;
    }
    else
    {
        lpPos->rectNormal.left   = wnd->rectWindow.left;
        lpPos->rectNormal.top    = wnd->rectWindow.top;
        lpPos->rectNormal.right  = wnd->rectWindow.right;
        lpPos->rectNormal.bottom = wnd->rectWindow.bottom;
    }
    return lpPos;
}

/***********************************************************************
 *           WINPOS_RedrawIconTitle
 */
BOOL WINPOS_RedrawIconTitle( HWND hWnd )
{
    LPINTERNALPOS lpPos = get_internal_pos( hWnd );
    if( lpPos )
    {
	if( lpPos->hwndIconTitle )
	{
	    SendMessageA( lpPos->hwndIconTitle, WM_SHOWWINDOW, TRUE, 0);
	    InvalidateRect( lpPos->hwndIconTitle, NULL, TRUE );
	    return TRUE;
	}
    }
    return FALSE;
}

/***********************************************************************
 *           WINPOS_ShowIconTitle
 */
BOOL WINPOS_ShowIconTitle( HWND hwnd, BOOL bShow )
{
    LPINTERNALPOS lpPos = get_internal_pos( hwnd );

    if (lpPos && !GetPropA( hwnd, "__wine_x11_managed" ))
    {
        HWND title = lpPos->hwndIconTitle;

	TRACE("%p %i\n", hwnd, (bShow != 0) );

	if( !title )
	    lpPos->hwndIconTitle = title = ICONTITLE_Create( hwnd );
	if( bShow )
        {
            if (!IsWindowVisible(title))
            {
                SendMessageA( title, WM_SHOWWINDOW, TRUE, 0 );
                SetWindowPos( title, 0, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE |
                              SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
            }
	}
	else ShowWindow( title, SW_HIDE );
    }
    return FALSE;
}

/*******************************************************************
 *           WINPOS_GetMinMaxInfo
 *
 * Get the minimized and maximized information for a window.
 */
void WINPOS_GetMinMaxInfo( HWND hwnd, POINT *maxSize, POINT *maxPos,
			   POINT *minTrack, POINT *maxTrack )
{
    LPINTERNALPOS lpPos;
    MINMAXINFO MinMax;
    INT xinc, yinc;
    LONG style = GetWindowLongA( hwnd, GWL_STYLE );
    LONG exstyle = GetWindowLongA( hwnd, GWL_EXSTYLE );
    RECT rc;

    /* Compute default values */

    GetWindowRect(hwnd, &rc);
    MinMax.ptReserved.x = rc.left;
    MinMax.ptReserved.y = rc.top;

    if (style & WS_CHILD)
    {
        if ((style & WS_CAPTION) == WS_CAPTION)
            style &= ~WS_BORDER; /* WS_CAPTION = WS_DLGFRAME | WS_BORDER */

        GetClientRect(GetAncestor(hwnd,GA_PARENT), &rc);
        AdjustWindowRectEx(&rc, style, ((style & WS_POPUP) && GetMenu(hwnd)), exstyle);

        /* avoid calculating this twice */
        style &= ~(WS_DLGFRAME | WS_BORDER | WS_THICKFRAME);

        MinMax.ptMaxSize.x = rc.right - rc.left;
        MinMax.ptMaxSize.y = rc.bottom - rc.top;
    }
    else
    {
        MinMax.ptMaxSize.x = GetSystemMetrics(SM_CXSCREEN);
        MinMax.ptMaxSize.y = GetSystemMetrics(SM_CYSCREEN);
    }
    MinMax.ptMinTrackSize.x = GetSystemMetrics(SM_CXMINTRACK);
    MinMax.ptMinTrackSize.y = GetSystemMetrics(SM_CYMINTRACK);
    MinMax.ptMaxTrackSize.x = GetSystemMetrics(SM_CXSCREEN) + 2*GetSystemMetrics(SM_CXFRAME);
    MinMax.ptMaxTrackSize.y = GetSystemMetrics(SM_CYSCREEN) + 2*GetSystemMetrics(SM_CYFRAME);

    if (HAS_DLGFRAME( style, exstyle ))
    {
        xinc = GetSystemMetrics(SM_CXDLGFRAME);
        yinc = GetSystemMetrics(SM_CYDLGFRAME);
    }
    else
    {
        xinc = yinc = 0;
        if (HAS_THICKFRAME(style))
        {
            xinc += GetSystemMetrics(SM_CXFRAME);
            yinc += GetSystemMetrics(SM_CYFRAME);
        }
        if (style & WS_BORDER)
        {
            xinc += GetSystemMetrics(SM_CXBORDER);
            yinc += GetSystemMetrics(SM_CYBORDER);
        }
    }
    MinMax.ptMaxSize.x += 2 * xinc;
    MinMax.ptMaxSize.y += 2 * yinc;

    lpPos = get_internal_pos( hwnd );
    if( lpPos && !EMPTYPOINT(lpPos->ptMaxPos) )
    {
        MinMax.ptMaxPosition.x = lpPos->ptMaxPos.x;
        MinMax.ptMaxPosition.y = lpPos->ptMaxPos.y;
    }
    else
    {
        MinMax.ptMaxPosition.x = -xinc;
        MinMax.ptMaxPosition.y = -yinc;
    }

    SendMessageA( hwnd, WM_GETMINMAXINFO, 0, (LPARAM)&MinMax );

      /* Some sanity checks */

    TRACE("%ld %ld / %ld %ld / %ld %ld / %ld %ld\n",
                      MinMax.ptMaxSize.x, MinMax.ptMaxSize.y,
                      MinMax.ptMaxPosition.x, MinMax.ptMaxPosition.y,
                      MinMax.ptMaxTrackSize.x, MinMax.ptMaxTrackSize.y,
                      MinMax.ptMinTrackSize.x, MinMax.ptMinTrackSize.y);
    MinMax.ptMaxTrackSize.x = max( MinMax.ptMaxTrackSize.x,
                                   MinMax.ptMinTrackSize.x );
    MinMax.ptMaxTrackSize.y = max( MinMax.ptMaxTrackSize.y,
                                   MinMax.ptMinTrackSize.y );

    if (maxSize) *maxSize = MinMax.ptMaxSize;
    if (maxPos) *maxPos = MinMax.ptMaxPosition;
    if (minTrack) *minTrack = MinMax.ptMinTrackSize;
    if (maxTrack) *maxTrack = MinMax.ptMaxTrackSize;
}

/***********************************************************************
 *		ShowWindowAsync (USER32.@)
 *
 * doesn't wait; returns immediately.
 * used by threads to toggle windows in other (possibly hanging) threads
 */
BOOL WINAPI ShowWindowAsync( HWND hwnd, INT cmd )
{
    HWND full_handle;

    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    if ((full_handle = WIN_IsCurrentThread( hwnd )))
        return USER_Driver->pShowWindow( full_handle, cmd );

    return SendNotifyMessageW( hwnd, WM_WINE_SHOWWINDOW, cmd, 0 );
}


/***********************************************************************
 *		ShowWindow (USER32.@)
 */
BOOL WINAPI ShowWindow( HWND hwnd, INT cmd )
{
    HWND full_handle;

    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }
    if ((full_handle = WIN_IsCurrentThread( hwnd )))
        return USER_Driver->pShowWindow( full_handle, cmd );

    return SendMessageW( hwnd, WM_WINE_SHOWWINDOW, cmd, 0 );
}


/***********************************************************************
 *		GetInternalWindowPos (USER32.@)
 */
UINT WINAPI GetInternalWindowPos( HWND hwnd, LPRECT rectWnd,
                                      LPPOINT ptIcon )
{
    WINDOWPLACEMENT wndpl;
    if (GetWindowPlacement( hwnd, &wndpl ))
    {
	if (rectWnd) *rectWnd = wndpl.rcNormalPosition;
	if (ptIcon)  *ptIcon = wndpl.ptMinPosition;
	return wndpl.showCmd;
    }
    return 0;
}


/***********************************************************************
 *		GetWindowPlacement (USER32.@)
 *
 * Win95:
 * Fails if wndpl->length of Win95 (!) apps is invalid.
 */
BOOL WINAPI GetWindowPlacement( HWND hwnd, WINDOWPLACEMENT *wndpl )
{
    WND *pWnd = WIN_GetPtr( hwnd );
    LPINTERNALPOS lpPos;

    if (!pWnd || pWnd == WND_DESKTOP) return FALSE;
    if (pWnd == WND_OTHER_PROCESS)
    {
        if (IsWindow( hwnd )) FIXME( "not supported on other process window %p\n", hwnd );
        return FALSE;
    }

    lpPos = WINPOS_InitInternalPos( pWnd );
    wndpl->length  = sizeof(*wndpl);
    if( pWnd->dwStyle & WS_MINIMIZE )
        wndpl->showCmd = SW_SHOWMINIMIZED;
    else
        wndpl->showCmd = ( pWnd->dwStyle & WS_MAXIMIZE ) ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL ;
    if( pWnd->flags & WIN_RESTORE_MAX )
        wndpl->flags = WPF_RESTORETOMAXIMIZED;
    else
        wndpl->flags = 0;
    wndpl->ptMinPosition.x = lpPos->ptIconPos.x;
    wndpl->ptMinPosition.y = lpPos->ptIconPos.y;
    wndpl->ptMaxPosition.x = lpPos->ptMaxPos.x;
    wndpl->ptMaxPosition.y = lpPos->ptMaxPos.y;
    wndpl->rcNormalPosition.left   = lpPos->rectNormal.left;
    wndpl->rcNormalPosition.top    = lpPos->rectNormal.top;
    wndpl->rcNormalPosition.right  = lpPos->rectNormal.right;
    wndpl->rcNormalPosition.bottom = lpPos->rectNormal.bottom;
    WIN_ReleasePtr( pWnd );
    return TRUE;
}


/***********************************************************************
 *           WINPOS_SetPlacement
 */
static BOOL WINPOS_SetPlacement( HWND hwnd, const WINDOWPLACEMENT *wndpl, UINT flags )
{
    LPINTERNALPOS lpPos;
    DWORD style;
    WND *pWnd = WIN_GetPtr( hwnd );

    if (!pWnd || pWnd == WND_OTHER_PROCESS || pWnd == WND_DESKTOP) return FALSE;
    lpPos = WINPOS_InitInternalPos( pWnd );

    if( flags & PLACE_MIN )
    {
        lpPos->ptIconPos.x = wndpl->ptMinPosition.x;
        lpPos->ptIconPos.y = wndpl->ptMinPosition.y;
    }
    if( flags & PLACE_MAX )
    {
        lpPos->ptMaxPos.x = wndpl->ptMaxPosition.x;
        lpPos->ptMaxPos.y = wndpl->ptMaxPosition.y;
    }
    if( flags & PLACE_RECT)
    {
        lpPos->rectNormal.left   = wndpl->rcNormalPosition.left;
        lpPos->rectNormal.top    = wndpl->rcNormalPosition.top;
        lpPos->rectNormal.right  = wndpl->rcNormalPosition.right;
        lpPos->rectNormal.bottom = wndpl->rcNormalPosition.bottom;
    }

    style = pWnd->dwStyle;
    WIN_ReleasePtr( pWnd );

    if( style & WS_MINIMIZE )
    {
        WINPOS_ShowIconTitle( hwnd, FALSE );
        if( wndpl->flags & WPF_SETMINPOSITION && !EMPTYPOINT(lpPos->ptIconPos))
            SetWindowPos( hwnd, 0, lpPos->ptIconPos.x, lpPos->ptIconPos.y,
                          0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
    }
    else if( style & WS_MAXIMIZE )
    {
        if( !EMPTYPOINT(lpPos->ptMaxPos) )
            SetWindowPos( hwnd, 0, lpPos->ptMaxPos.x, lpPos->ptMaxPos.y,
                          0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
    }
    else if( flags & PLACE_RECT )
        SetWindowPos( hwnd, 0, lpPos->rectNormal.left, lpPos->rectNormal.top,
                      lpPos->rectNormal.right - lpPos->rectNormal.left,
                      lpPos->rectNormal.bottom - lpPos->rectNormal.top,
                      SWP_NOZORDER | SWP_NOACTIVATE );

    ShowWindow( hwnd, wndpl->showCmd );

    if (IsIconic( hwnd ))
    {
        if (GetWindowLongW( hwnd, GWL_STYLE ) & WS_VISIBLE) WINPOS_ShowIconTitle( hwnd, TRUE );

        /* SDK: ...valid only the next time... */
        if( wndpl->flags & WPF_RESTORETOMAXIMIZED )
        {
            pWnd = WIN_GetPtr( hwnd );
            if (pWnd && pWnd != WND_OTHER_PROCESS)
            {
                pWnd->flags |= WIN_RESTORE_MAX;
                WIN_ReleasePtr( pWnd );
            }
        }
    }
    return TRUE;
}


/***********************************************************************
 *		SetWindowPlacement (USER32.@)
 *
 * Win95:
 * Fails if wndpl->length of Win95 (!) apps is invalid.
 */
BOOL WINAPI SetWindowPlacement( HWND hwnd, const WINDOWPLACEMENT *wpl )
{
    if (!wpl) return FALSE;
    return WINPOS_SetPlacement( hwnd, wpl, PLACE_MIN | PLACE_MAX | PLACE_RECT );
}


/***********************************************************************
 *		AnimateWindow (USER32.@)
 *		Shows/Hides a window with an animation
 *		NO ANIMATION YET
 */
BOOL WINAPI AnimateWindow(HWND hwnd, DWORD dwTime, DWORD dwFlags)
{
	FIXME("partial stub\n");

	/* If trying to show/hide and it's already   *
	 * shown/hidden or invalid window, fail with *
	 * invalid parameter                         */
	if(!IsWindow(hwnd) ||
	   (IsWindowVisible(hwnd) && !(dwFlags & AW_HIDE)) ||
	   (!IsWindowVisible(hwnd) && (dwFlags & AW_HIDE)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	ShowWindow(hwnd, (dwFlags & AW_HIDE) ? SW_HIDE : ((dwFlags & AW_ACTIVATE) ? SW_SHOW : SW_SHOWNA));

	return TRUE;
}

/***********************************************************************
 *		SetInternalWindowPos (USER32.@)
 */
void WINAPI SetInternalWindowPos( HWND hwnd, UINT showCmd,
                                    LPRECT rect, LPPOINT pt )
{
    if( IsWindow(hwnd) )
    {
        WINDOWPLACEMENT wndpl;
	UINT flags;

	wndpl.length  = sizeof(wndpl);
	wndpl.showCmd = showCmd;
	wndpl.flags = flags = 0;

	if( pt )
	{
            flags |= PLACE_MIN;
            wndpl.flags |= WPF_SETMINPOSITION;
            wndpl.ptMinPosition = *pt;
	}
	if( rect )
	{
            flags |= PLACE_RECT;
            wndpl.rcNormalPosition = *rect;
	}
        WINPOS_SetPlacement( hwnd, &wndpl, flags );
    }
}


/*******************************************************************
 *         can_activate_window
 *
 * Check if we can activate the specified window.
 */
static BOOL can_activate_window( HWND hwnd )
{
    LONG style;

    if (!hwnd) return FALSE;
    style = GetWindowLongW( hwnd, GWL_STYLE );
    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & (WS_POPUP|WS_CHILD)) == WS_CHILD) return FALSE;
    return !(style & WS_DISABLED);
}


/*******************************************************************
 *         WINPOS_ActivateOtherWindow
 *
 *  Activates window other than pWnd.
 */
void WINPOS_ActivateOtherWindow(HWND hwnd)
{
    HWND hwndTo, fg;

    if ((GetWindowLongW( hwnd, GWL_STYLE ) & WS_POPUP) && (hwndTo = GetWindow( hwnd, GW_OWNER )))
    {
        hwndTo = GetAncestor( hwndTo, GA_ROOT );
        if (can_activate_window( hwndTo )) goto done;
    }

    hwndTo = hwnd;
    for (;;)
    {
        if (!(hwndTo = GetWindow( hwndTo, GW_HWNDNEXT ))) break;
        if (can_activate_window( hwndTo )) break;
    }

 done:
    fg = GetForegroundWindow();
    TRACE("win = %p fg = %p\n", hwndTo, fg);
    if (!fg || (hwnd == fg))
    {
        if (SetForegroundWindow( hwndTo )) return;
    }
    if (!SetActiveWindow( hwndTo )) SetActiveWindow(0);
}


/***********************************************************************
 *           WINPOS_HandleWindowPosChanging
 *
 * Default handling for a WM_WINDOWPOSCHANGING. Called from DefWindowProc().
 */
LONG WINPOS_HandleWindowPosChanging( HWND hwnd, WINDOWPOS *winpos )
{
    POINT minTrack, maxTrack;
    LONG style = GetWindowLongW( hwnd, GWL_STYLE );

    if (winpos->flags & SWP_NOSIZE) return 0;
    if ((style & WS_THICKFRAME) || ((style & (WS_POPUP | WS_CHILD)) == 0))
    {
	WINPOS_GetMinMaxInfo( hwnd, NULL, NULL, &minTrack, &maxTrack );
	if (winpos->cx > maxTrack.x) winpos->cx = maxTrack.x;
	if (winpos->cy > maxTrack.y) winpos->cy = maxTrack.y;
	if (!(style & WS_MINIMIZE))
	{
	    if (winpos->cx < minTrack.x ) winpos->cx = minTrack.x;
	    if (winpos->cy < minTrack.y ) winpos->cy = minTrack.y;
	}
    }
    return 0;
}


/***********************************************************************
 *           dump_winpos_flags
 */
static void dump_winpos_flags(UINT flags)
{
    TRACE("flags:");
    if(flags & SWP_NOSIZE) TRACE(" SWP_NOSIZE");
    if(flags & SWP_NOMOVE) TRACE(" SWP_NOMOVE");
    if(flags & SWP_NOZORDER) TRACE(" SWP_NOZORDER");
    if(flags & SWP_NOREDRAW) TRACE(" SWP_NOREDRAW");
    if(flags & SWP_NOACTIVATE) TRACE(" SWP_NOACTIVATE");
    if(flags & SWP_FRAMECHANGED) TRACE(" SWP_FRAMECHANGED");
    if(flags & SWP_SHOWWINDOW) TRACE(" SWP_SHOWWINDOW");
    if(flags & SWP_HIDEWINDOW) TRACE(" SWP_HIDEWINDOW");
    if(flags & SWP_NOCOPYBITS) TRACE(" SWP_NOCOPYBITS");
    if(flags & SWP_NOOWNERZORDER) TRACE(" SWP_NOOWNERZORDER");
    if(flags & SWP_NOSENDCHANGING) TRACE(" SWP_NOSENDCHANGING");
    if(flags & SWP_DEFERERASE) TRACE(" SWP_DEFERERASE");
    if(flags & SWP_ASYNCWINDOWPOS) TRACE(" SWP_ASYNCWINDOWPOS");

#define DUMPED_FLAGS \
    (SWP_NOSIZE | \
    SWP_NOMOVE | \
    SWP_NOZORDER | \
    SWP_NOREDRAW | \
    SWP_NOACTIVATE | \
    SWP_FRAMECHANGED | \
    SWP_SHOWWINDOW | \
    SWP_HIDEWINDOW | \
    SWP_NOCOPYBITS | \
    SWP_NOOWNERZORDER | \
    SWP_NOSENDCHANGING | \
    SWP_DEFERERASE | \
    SWP_ASYNCWINDOWPOS)

    if(flags & ~DUMPED_FLAGS) TRACE(" %08x", flags & ~DUMPED_FLAGS);
    TRACE("\n");
#undef DUMPED_FLAGS
}

/***********************************************************************
 *		SetWindowPos (USER32.@)
 */
BOOL WINAPI SetWindowPos( HWND hwnd, HWND hwndInsertAfter,
                          INT x, INT y, INT cx, INT cy, UINT flags )
{
    WINDOWPOS winpos;

    TRACE("hwnd %p, after %p, %d,%d (%dx%d), flags %08x\n",
          hwnd, hwndInsertAfter, x, y, cx, cy, flags);
    if(TRACE_ON(win)) dump_winpos_flags(flags);

    if (is_broadcast(hwnd))
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    winpos.hwnd = WIN_GetFullHandle(hwnd);
    winpos.hwndInsertAfter = WIN_GetFullHandle(hwndInsertAfter);
    winpos.x = x;
    winpos.y = y;
    winpos.cx = cx;
    winpos.cy = cy;
    winpos.flags = flags;
    if (WIN_IsCurrentThread( hwnd ))
        return USER_Driver->pSetWindowPos( &winpos );

    return SendMessageW( winpos.hwnd, WM_WINE_SETWINDOWPOS, 0, (LPARAM)&winpos );
}


/***********************************************************************
 *		BeginDeferWindowPos (USER32.@)
 */
HDWP WINAPI BeginDeferWindowPos( INT count )
{
    HDWP handle;
    DWP *pDWP;

    TRACE("%d\n", count);

    if (count < 0)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }
    /* Windows allows zero count, in which case it allocates context for 8 moves */
    if (count == 0) count = 8;

    handle = USER_HEAP_ALLOC( sizeof(DWP) + (count-1)*sizeof(WINDOWPOS) );
    if (!handle) return 0;
    pDWP = (DWP *) USER_HEAP_LIN_ADDR( handle );
    pDWP->actualCount    = 0;
    pDWP->suggestedCount = count;
    pDWP->valid          = TRUE;
    pDWP->wMagic         = DWP_MAGIC;
    pDWP->hwndParent     = 0;

    TRACE("returning hdwp %p\n", handle);
    return handle;
}


/***********************************************************************
 *		DeferWindowPos (USER32.@)
 */
HDWP WINAPI DeferWindowPos( HDWP hdwp, HWND hwnd, HWND hwndAfter,
                                INT x, INT y, INT cx, INT cy,
                                UINT flags )
{
    DWP *pDWP;
    int i;
    HDWP newhdwp = hdwp,retvalue;

    TRACE("hdwp %p, hwnd %p, after %p, %d,%d (%dx%d), flags %08x\n",
          hdwp, hwnd, hwndAfter, x, y, cx, cy, flags);

    hwnd = WIN_GetFullHandle( hwnd );
    if (hwnd == GetDesktopWindow()) return 0;

    if (!(pDWP = USER_HEAP_LIN_ADDR( hdwp ))) return 0;

    USER_Lock();

    for (i = 0; i < pDWP->actualCount; i++)
    {
        if (pDWP->winPos[i].hwnd == hwnd)
        {
              /* Merge with the other changes */
            if (!(flags & SWP_NOZORDER))
            {
                pDWP->winPos[i].hwndInsertAfter = WIN_GetFullHandle(hwndAfter);
            }
            if (!(flags & SWP_NOMOVE))
            {
                pDWP->winPos[i].x = x;
                pDWP->winPos[i].y = y;
            }
            if (!(flags & SWP_NOSIZE))
            {
                pDWP->winPos[i].cx = cx;
                pDWP->winPos[i].cy = cy;
            }
            pDWP->winPos[i].flags &= flags | ~(SWP_NOSIZE | SWP_NOMOVE |
                                               SWP_NOZORDER | SWP_NOREDRAW |
                                               SWP_NOACTIVATE | SWP_NOCOPYBITS|
                                               SWP_NOOWNERZORDER);
            pDWP->winPos[i].flags |= flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW |
                                              SWP_FRAMECHANGED);
            retvalue = hdwp;
            goto END;
        }
    }
    if (pDWP->actualCount >= pDWP->suggestedCount)
    {
        newhdwp = USER_HEAP_REALLOC( hdwp,
                      sizeof(DWP) + pDWP->suggestedCount*sizeof(WINDOWPOS) );
        if (!newhdwp)
        {
            retvalue = 0;
            goto END;
        }
        pDWP = (DWP *) USER_HEAP_LIN_ADDR( newhdwp );
        pDWP->suggestedCount++;
    }
    pDWP->winPos[pDWP->actualCount].hwnd = hwnd;
    pDWP->winPos[pDWP->actualCount].hwndInsertAfter = hwndAfter;
    pDWP->winPos[pDWP->actualCount].x = x;
    pDWP->winPos[pDWP->actualCount].y = y;
    pDWP->winPos[pDWP->actualCount].cx = cx;
    pDWP->winPos[pDWP->actualCount].cy = cy;
    pDWP->winPos[pDWP->actualCount].flags = flags;
    pDWP->actualCount++;
    retvalue = newhdwp;
END:
    USER_Unlock();
    return retvalue;
}


/***********************************************************************
 *		EndDeferWindowPos (USER32.@)
 */
BOOL WINAPI EndDeferWindowPos( HDWP hdwp )
{
    DWP *pDWP;
    WINDOWPOS *winpos;
    BOOL res = TRUE;
    int i;

    TRACE("%p\n", hdwp);

    pDWP = (DWP *) USER_HEAP_LIN_ADDR( hdwp );
    if (!pDWP) return FALSE;
    for (i = 0, winpos = pDWP->winPos; i < pDWP->actualCount; i++, winpos++)
    {
        if (!(res = USER_Driver->pSetWindowPos( winpos ))) break;
    }
    USER_HEAP_FREE( hdwp );
    return res;
}


/***********************************************************************
 *		TileChildWindows (USER.199)
 */
void WINAPI TileChildWindows16( HWND16 parent, WORD action )
{
    FIXME("(%04x, %d): stub\n", parent, action);
}

/***********************************************************************
 *		CascadeChildWindows (USER.198)
 */
void WINAPI CascadeChildWindows16( HWND16 parent, WORD action )
{
    FIXME("(%04x, %d): stub\n", parent, action);
}
