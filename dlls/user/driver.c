/*
 * USER driver support
 *
 * Copyright 2000, 2005 Alexandre Julliard
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

#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "winuser.h"

#include "user_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(user);

static const USER_DRIVER null_driver, lazy_load_driver;

const USER_DRIVER *USER_Driver = &lazy_load_driver;

/* load the graphics driver */
static const USER_DRIVER *load_driver(void)
{
    char buffer[MAX_PATH], libname[32], *name, *next;
    HKEY hkey;
    void *ptr;
    HMODULE graphics_driver;
    USER_DRIVER *driver, *prev;

    strcpy( buffer, "x11" );  /* default value */
    /* @@ Wine registry key: HKCU\Software\Wine\Drivers */
    if (!RegOpenKeyA( HKEY_CURRENT_USER, "Software\\Wine\\Drivers", &hkey ))
    {
        DWORD type, count = sizeof(buffer);
        RegQueryValueExA( hkey, "Graphics", 0, &type, (LPBYTE) buffer, &count );
        RegCloseKey( hkey );
    }

    name = buffer;
    while (name)
    {
        next = strchr( name, ',' );
        if (next) *next++ = 0;

        snprintf( libname, sizeof(libname), "wine%s.drv", name );
        if ((graphics_driver = LoadLibraryA( libname )) != 0) break;
        name = next;
    }

    driver = HeapAlloc( GetProcessHeap(), 0, sizeof(*driver) );
    memcpy( driver, &null_driver, sizeof(*driver) );

    if (graphics_driver)
    {
#define GET_USER_FUNC(name) \
    do { if ((ptr = GetProcAddress( graphics_driver, #name ))) driver->p##name = ptr; } while(0)

        GET_USER_FUNC(ActivateKeyboardLayout);
        GET_USER_FUNC(Beep);
        GET_USER_FUNC(GetAsyncKeyState);
        GET_USER_FUNC(GetKeyNameText);
        GET_USER_FUNC(GetKeyboardLayout);
        GET_USER_FUNC(GetKeyboardLayoutList);
        GET_USER_FUNC(GetKeyboardLayoutName);
        GET_USER_FUNC(LoadKeyboardLayout);
        GET_USER_FUNC(MapVirtualKeyEx);
        GET_USER_FUNC(SendInput);
        GET_USER_FUNC(ToUnicodeEx);
        GET_USER_FUNC(UnloadKeyboardLayout);
        GET_USER_FUNC(VkKeyScanEx);
        GET_USER_FUNC(SetCursor);
        GET_USER_FUNC(GetCursorPos);
        GET_USER_FUNC(SetCursorPos);
        GET_USER_FUNC(GetScreenSaveActive);
        GET_USER_FUNC(SetScreenSaveActive);
        GET_USER_FUNC(AcquireClipboard);
        GET_USER_FUNC(EmptyClipboard);
        GET_USER_FUNC(SetClipboardData);
        GET_USER_FUNC(GetClipboardData);
        GET_USER_FUNC(CountClipboardFormats);
        GET_USER_FUNC(EnumClipboardFormats);
        GET_USER_FUNC(IsClipboardFormatAvailable);
        GET_USER_FUNC(RegisterClipboardFormat);
        GET_USER_FUNC(GetClipboardFormatName);
        GET_USER_FUNC(EndClipboardUpdate);
        GET_USER_FUNC(ResetSelectionOwner);
        GET_USER_FUNC(ChangeDisplaySettingsEx);
        GET_USER_FUNC(EnumDisplaySettingsEx);
        GET_USER_FUNC(CreateDesktopWindow);
        GET_USER_FUNC(CreateWindow);
        GET_USER_FUNC(DestroyWindow);
        GET_USER_FUNC(GetDCEx);
        GET_USER_FUNC(MsgWaitForMultipleObjectsEx);
        GET_USER_FUNC(ReleaseDC);
        GET_USER_FUNC(ScrollDC);
        GET_USER_FUNC(SetFocus);
        GET_USER_FUNC(SetParent);
        GET_USER_FUNC(SetWindowPos);
        GET_USER_FUNC(SetWindowRgn);
        GET_USER_FUNC(SetWindowIcon);
        GET_USER_FUNC(SetWindowStyle);
        GET_USER_FUNC(SetWindowText);
        GET_USER_FUNC(ShowWindow);
        GET_USER_FUNC(SysCommandSizeMove);
        GET_USER_FUNC(WindowFromDC);
        GET_USER_FUNC(WindowMessage);
#undef GET_USER_FUNC
    }

    prev = InterlockedCompareExchangePointer( (void **)&USER_Driver, driver, (void *)&lazy_load_driver );
    if (prev != &lazy_load_driver)
    {
        /* another thread beat us to it */
        HeapFree( GetProcessHeap(), 0, driver );
        FreeLibrary( graphics_driver );
        driver = prev;
    }
    return driver;
}

/* unload the graphics driver on process exit */
void USER_unload_driver(void)
{
    /* make sure we don't try to call the driver after it has been detached */
    USER_Driver = &null_driver;
}


/**********************************************************************
 * Null user driver
 *
 * These are fallbacks for entry points that are not implemented in the real driver.
 */

static HKL nulldrv_ActivateKeyboardLayout( HKL layout, UINT flags )
{
    return 0;
}

static void nulldrv_Beep(void)
{
}

static SHORT nulldrv_GetAsyncKeyState( INT key )
{
    return 0;
}

static INT nulldrv_GetKeyNameText( LONG lparam, LPWSTR buffer, INT size )
{
    return 0;
}

static HKL nulldrv_GetKeyboardLayout( DWORD layout )
{
    return 0;
}

static UINT nulldrv_GetKeyboardLayoutList( INT count, HKL *layouts )
{
    return 0;
}

static BOOL nulldrv_GetKeyboardLayoutName( LPWSTR name )
{
    return FALSE;
}

static HKL nulldrv_LoadKeyboardLayout( LPCWSTR name, UINT flags )
{
    return 0;
}

static UINT nulldrv_MapVirtualKeyEx( UINT code, UINT type, HKL layout )
{
    return 0;
}

static UINT nulldrv_SendInput( UINT count, LPINPUT inputs, int size )
{
    return 0;
}

static INT nulldrv_ToUnicodeEx( UINT virt, UINT scan, LPBYTE state, LPWSTR str,
                                int size, UINT flags, HKL layout )
{
    return 0;
}

static BOOL nulldrv_UnloadKeyboardLayout( HKL layout )
{
    return 0;
}

static SHORT nulldrv_VkKeyScanEx( WCHAR ch, HKL layout )
{
    return -1;
}

static void nulldrv_SetCursor( struct tagCURSORICONINFO *info )
{
}

static BOOL nulldrv_GetCursorPos( LPPOINT pt )
{
    return FALSE;
}

static BOOL nulldrv_SetCursorPos( INT x, INT y )
{
    return FALSE;
}

static BOOL nulldrv_GetScreenSaveActive(void)
{
    return FALSE;
}

static void nulldrv_SetScreenSaveActive( BOOL on )
{
}

static void nulldrv_AcquireClipboard( HWND hwnd )
{
}

static BOOL nulldrv_CountClipboardFormats(void)
{
    return 0;
}

static void nulldrv_EmptyClipboard( BOOL keepunowned )
{
}

static void nulldrv_EndClipboardUpdate(void)
{
}

static UINT nulldrv_EnumClipboardFormats( UINT format )
{
    return 0;
}

static BOOL nulldrv_GetClipboardData( UINT format, HANDLE16 *h16, HANDLE *h32 )
{
    return FALSE;
}

static INT nulldrv_GetClipboardFormatName( UINT format, LPWSTR buffer, UINT len )
{
    return FALSE;
}

static BOOL nulldrv_IsClipboardFormatAvailable( UINT format )
{
    return FALSE;
}

static UINT nulldrv_RegisterClipboardFormat( LPCWSTR name )
{
    return 0;
}

static void nulldrv_ResetSelectionOwner( HWND hwnd, BOOL flag )
{
}

static BOOL nulldrv_SetClipboardData( UINT format, HANDLE16 h16, HANDLE h32, BOOL owner )
{
    return FALSE;
}

static LONG nulldrv_ChangeDisplaySettingsEx( LPCWSTR name, LPDEVMODEW mode, HWND hwnd,
                                             DWORD flags, LPVOID lparam )
{
    return DISP_CHANGE_FAILED;
}

static BOOL nulldrv_EnumDisplaySettingsEx( LPCWSTR name, DWORD num, LPDEVMODEW mode, DWORD flags )
{
    return FALSE;
}

static BOOL nulldrv_CreateDesktopWindow( HWND hwnd )
{
    return TRUE;
}

static BOOL nulldrv_CreateWindow( HWND hwnd, CREATESTRUCTA *cs, BOOL unicode )
{
    static int warned;

    if (!warned++)
        MESSAGE( "Application tries to create a window, but no driver could be loaded.\n"
                 "Make sure that your X server is running and that $DISPLAY is set correctly.\n" );
    return FALSE;
}

static void nulldrv_DestroyWindow( HWND hwnd )
{
}

static HDC nulldrv_GetDCEx( HWND hwnd, HRGN hrgn, DWORD flags )
{
    return 0;
}

static DWORD nulldrv_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                  DWORD mask, DWORD flags )
{
    return WaitForMultipleObjectsEx( count, handles, flags & MWMO_WAITALL,
                                     timeout, flags & MWMO_ALERTABLE );
}

static INT nulldrv_ReleaseDC( HWND hwnd, HDC hdc, BOOL end_paint )
{
    return 0;
}

static BOOL nulldrv_ScrollDC( HDC hdc, INT dx, INT dy, const RECT *scroll, const RECT *clip,
                              HRGN hrgn, LPRECT update )
{
    return FALSE;
}

static void nulldrv_SetFocus( HWND hwnd )
{
}

static HWND nulldrv_SetParent( HWND hwnd, HWND parent )
{
    return 0;
}

static BOOL nulldrv_SetWindowPos( WINDOWPOS *pos )
{
    return FALSE;
}

static int nulldrv_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
    return 1;
}

static void nulldrv_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
}

static void nulldrv_SetWindowStyle( HWND hwnd, DWORD old_style )
{
}

static void nulldrv_SetWindowText( HWND hwnd, LPCWSTR text )
{
}

static BOOL nulldrv_ShowWindow( HWND hwnd, INT cmd )
{
    return FALSE;
}

static void nulldrv_SysCommandSizeMove( HWND hwnd, WPARAM wparam )
{
}

static HWND nulldrv_WindowFromDC( HDC hdc )
{
    return 0;
}

static LRESULT nulldrv_WindowMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    return 0;
}

static const USER_DRIVER null_driver =
{
    /* keyboard functions */
    nulldrv_ActivateKeyboardLayout,
    nulldrv_Beep,
    nulldrv_GetAsyncKeyState,
    nulldrv_GetKeyNameText,
    nulldrv_GetKeyboardLayout,
    nulldrv_GetKeyboardLayoutList,
    nulldrv_GetKeyboardLayoutName,
    nulldrv_LoadKeyboardLayout,
    nulldrv_MapVirtualKeyEx,
    nulldrv_SendInput,
    nulldrv_ToUnicodeEx,
    nulldrv_UnloadKeyboardLayout,
    nulldrv_VkKeyScanEx,
    /* mouse functions */
    nulldrv_SetCursor,
    nulldrv_GetCursorPos,
    nulldrv_SetCursorPos,
    /* screen saver functions */
    nulldrv_GetScreenSaveActive,
    nulldrv_SetScreenSaveActive,
    /* clipboard functions */
    nulldrv_AcquireClipboard,
    nulldrv_CountClipboardFormats,
    nulldrv_EmptyClipboard,
    nulldrv_EndClipboardUpdate,
    nulldrv_EnumClipboardFormats,
    nulldrv_GetClipboardData,
    nulldrv_GetClipboardFormatName,
    nulldrv_IsClipboardFormatAvailable,
    nulldrv_RegisterClipboardFormat,
    nulldrv_ResetSelectionOwner,
    nulldrv_SetClipboardData,
    /* display modes */
    nulldrv_ChangeDisplaySettingsEx,
    nulldrv_EnumDisplaySettingsEx,
    /* windowing functions */
    nulldrv_CreateDesktopWindow,
    nulldrv_CreateWindow,
    nulldrv_DestroyWindow,
    nulldrv_GetDCEx,
    nulldrv_MsgWaitForMultipleObjectsEx,
    nulldrv_ReleaseDC,
    nulldrv_ScrollDC,
    nulldrv_SetFocus,
    nulldrv_SetParent,
    nulldrv_SetWindowPos,
    nulldrv_SetWindowRgn,
    nulldrv_SetWindowIcon,
    nulldrv_SetWindowStyle,
    nulldrv_SetWindowText,
    nulldrv_ShowWindow,
    nulldrv_SysCommandSizeMove,
    nulldrv_WindowFromDC,
    nulldrv_WindowMessage
};


/**********************************************************************
 * Lazy loading user driver
 *
 * Initial driver used before another driver is loaded.
 * Each entry point simply loads the real driver and chains to it.
 */

static HKL loaderdrv_ActivateKeyboardLayout( HKL layout, UINT flags )
{
    return load_driver()->pActivateKeyboardLayout( layout, flags );
}

static void loaderdrv_Beep(void)
{
    load_driver()->pBeep();
}

static SHORT loaderdrv_GetAsyncKeyState( INT key )
{
    return load_driver()->pGetAsyncKeyState( key );
}

static INT loaderdrv_GetKeyNameText( LONG lparam, LPWSTR buffer, INT size )
{
    return load_driver()->pGetKeyNameText( lparam, buffer, size );
}

static HKL loaderdrv_GetKeyboardLayout( DWORD layout )
{
    return load_driver()->pGetKeyboardLayout( layout );
}

static UINT loaderdrv_GetKeyboardLayoutList( INT count, HKL *layouts )
{
    return load_driver()->pGetKeyboardLayoutList( count, layouts );
}

static BOOL loaderdrv_GetKeyboardLayoutName( LPWSTR name )
{
    return load_driver()->pGetKeyboardLayoutName( name );
}

static HKL loaderdrv_LoadKeyboardLayout( LPCWSTR name, UINT flags )
{
    return load_driver()->pLoadKeyboardLayout( name, flags );
}

static UINT loaderdrv_MapVirtualKeyEx( UINT code, UINT type, HKL layout )
{
    return load_driver()->pMapVirtualKeyEx( code, type, layout );
}

static UINT loaderdrv_SendInput( UINT count, LPINPUT inputs, int size )
{
    return load_driver()->pSendInput( count, inputs, size );
}

static INT loaderdrv_ToUnicodeEx( UINT virt, UINT scan, LPBYTE state, LPWSTR str,
                                  int size, UINT flags, HKL layout )
{
    return load_driver()->pToUnicodeEx( virt, scan, state, str, size, flags, layout );
}

static BOOL loaderdrv_UnloadKeyboardLayout( HKL layout )
{
    return load_driver()->pUnloadKeyboardLayout( layout );
}

static SHORT loaderdrv_VkKeyScanEx( WCHAR ch, HKL layout )
{
    return load_driver()->pVkKeyScanEx( ch, layout );
}

static void loaderdrv_SetCursor( struct tagCURSORICONINFO *info )
{
    load_driver()->pSetCursor( info );
}

static BOOL loaderdrv_GetCursorPos( LPPOINT pt )
{
    return load_driver()->pGetCursorPos( pt );
}

static BOOL loaderdrv_SetCursorPos( INT x, INT y )
{
    return load_driver()->pSetCursorPos( x, y );
}

static BOOL loaderdrv_GetScreenSaveActive(void)
{
    return load_driver()->pGetScreenSaveActive();
}

static void loaderdrv_SetScreenSaveActive( BOOL on )
{
    load_driver()->pSetScreenSaveActive( on );
}

static void loaderdrv_AcquireClipboard( HWND hwnd )
{
    load_driver()->pAcquireClipboard( hwnd );
}

static BOOL loaderdrv_CountClipboardFormats(void)
{
    return load_driver()->pCountClipboardFormats();
}

static void loaderdrv_EmptyClipboard( BOOL keepunowned )
{
    load_driver()->pEmptyClipboard( keepunowned );
}

static void loaderdrv_EndClipboardUpdate(void)
{
    load_driver()->pEndClipboardUpdate();
}

static UINT loaderdrv_EnumClipboardFormats( UINT format )
{
    return load_driver()->pEnumClipboardFormats( format );
}

static BOOL loaderdrv_GetClipboardData( UINT format, HANDLE16 *h16, HANDLE *h32 )
{
    return load_driver()->pGetClipboardData( format, h16, h32 );
}

static INT loaderdrv_GetClipboardFormatName( UINT format, LPWSTR buffer, UINT len )
{
    return load_driver()->pGetClipboardFormatName( format, buffer, len );
}

static BOOL loaderdrv_IsClipboardFormatAvailable( UINT format )
{
    return load_driver()->pIsClipboardFormatAvailable( format );
}

static UINT loaderdrv_RegisterClipboardFormat( LPCWSTR name )
{
    return load_driver()->pRegisterClipboardFormat( name );
}

static void loaderdrv_ResetSelectionOwner( HWND hwnd, BOOL flag )
{
    load_driver()->pResetSelectionOwner( hwnd, flag );
}

static BOOL loaderdrv_SetClipboardData( UINT format, HANDLE16 h16, HANDLE h32, BOOL owner )
{
    return load_driver()->pSetClipboardData( format, h16, h32, owner );
}

static LONG loaderdrv_ChangeDisplaySettingsEx( LPCWSTR name, LPDEVMODEW mode, HWND hwnd,
                                               DWORD flags, LPVOID lparam )
{
    return load_driver()->pChangeDisplaySettingsEx( name, mode, hwnd, flags, lparam );
}

static BOOL loaderdrv_EnumDisplaySettingsEx( LPCWSTR name, DWORD num, LPDEVMODEW mode, DWORD flags )
{
    return load_driver()->pEnumDisplaySettingsEx( name, num, mode, flags );
}

static BOOL loaderdrv_CreateDesktopWindow( HWND hwnd )
{
    return load_driver()->pCreateDesktopWindow( hwnd );
}

static BOOL loaderdrv_CreateWindow( HWND hwnd, CREATESTRUCTA *cs, BOOL unicode )
{
    return load_driver()->pCreateWindow( hwnd, cs, unicode );
}

static void loaderdrv_DestroyWindow( HWND hwnd )
{
    load_driver()->pDestroyWindow( hwnd );
}

static HDC loaderdrv_GetDCEx( HWND hwnd, HRGN hrgn, DWORD flags )
{
    return load_driver()->pGetDCEx( hwnd, hrgn, flags );
}

static DWORD loaderdrv_MsgWaitForMultipleObjectsEx( DWORD count, const HANDLE *handles, DWORD timeout,
                                                    DWORD mask, DWORD flags )
{
    return load_driver()->pMsgWaitForMultipleObjectsEx( count, handles, timeout, mask, flags );
}

static INT loaderdrv_ReleaseDC( HWND hwnd, HDC hdc, BOOL end_paint )
{
    return load_driver()->pReleaseDC( hwnd, hdc, end_paint );
}

static BOOL loaderdrv_ScrollDC( HDC hdc, INT dx, INT dy, const RECT *scroll, const RECT *clip,
                                HRGN hrgn, LPRECT update )
{
    return load_driver()->pScrollDC( hdc, dx, dy, scroll, clip, hrgn, update );
}

static void loaderdrv_SetFocus( HWND hwnd )
{
    load_driver()->pSetFocus( hwnd );
}

static HWND loaderdrv_SetParent( HWND hwnd, HWND parent )
{
    return load_driver()->pSetParent( hwnd, parent );
}

static BOOL loaderdrv_SetWindowPos( WINDOWPOS *pos )
{
    return load_driver()->pSetWindowPos( pos );
}

static int loaderdrv_SetWindowRgn( HWND hwnd, HRGN hrgn, BOOL redraw )
{
    return load_driver()->pSetWindowRgn( hwnd, hrgn, redraw );
}

static void loaderdrv_SetWindowIcon( HWND hwnd, UINT type, HICON icon )
{
    load_driver()->pSetWindowIcon( hwnd, type, icon );
}

static void loaderdrv_SetWindowStyle( HWND hwnd, DWORD old_style )
{
    load_driver()->pSetWindowStyle( hwnd, old_style );
}

static void loaderdrv_SetWindowText( HWND hwnd, LPCWSTR text )
{
    load_driver()->pSetWindowText( hwnd, text );
}

static BOOL loaderdrv_ShowWindow( HWND hwnd, INT cmd )
{
    return load_driver()->pShowWindow( hwnd, cmd );
}

static void loaderdrv_SysCommandSizeMove( HWND hwnd, WPARAM wparam )
{
    load_driver()->pSysCommandSizeMove( hwnd, wparam );
}

static HWND loaderdrv_WindowFromDC( HDC hdc )
{
    return load_driver()->pWindowFromDC( hdc );
}

static LRESULT loaderdrv_WindowMessage( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    return load_driver()->pWindowMessage( hwnd, msg, wparam, lparam );
}

static const USER_DRIVER lazy_load_driver =
{
    /* keyboard functions */
    loaderdrv_ActivateKeyboardLayout,
    loaderdrv_Beep,
    loaderdrv_GetAsyncKeyState,
    loaderdrv_GetKeyNameText,
    loaderdrv_GetKeyboardLayout,
    loaderdrv_GetKeyboardLayoutList,
    loaderdrv_GetKeyboardLayoutName,
    loaderdrv_LoadKeyboardLayout,
    loaderdrv_MapVirtualKeyEx,
    loaderdrv_SendInput,
    loaderdrv_ToUnicodeEx,
    loaderdrv_UnloadKeyboardLayout,
    loaderdrv_VkKeyScanEx,
    /* mouse functions */
    loaderdrv_SetCursor,
    loaderdrv_GetCursorPos,
    loaderdrv_SetCursorPos,
    /* screen saver functions */
    loaderdrv_GetScreenSaveActive,
    loaderdrv_SetScreenSaveActive,
    /* clipboard functions */
    loaderdrv_AcquireClipboard,
    loaderdrv_CountClipboardFormats,
    loaderdrv_EmptyClipboard,
    loaderdrv_EndClipboardUpdate,
    loaderdrv_EnumClipboardFormats,
    loaderdrv_GetClipboardData,
    loaderdrv_GetClipboardFormatName,
    loaderdrv_IsClipboardFormatAvailable,
    loaderdrv_RegisterClipboardFormat,
    loaderdrv_ResetSelectionOwner,
    loaderdrv_SetClipboardData,
    /* display modes */
    loaderdrv_ChangeDisplaySettingsEx,
    loaderdrv_EnumDisplaySettingsEx,
    /* windowing functions */
    loaderdrv_CreateDesktopWindow,
    loaderdrv_CreateWindow,
    loaderdrv_DestroyWindow,
    loaderdrv_GetDCEx,
    loaderdrv_MsgWaitForMultipleObjectsEx,
    loaderdrv_ReleaseDC,
    loaderdrv_ScrollDC,
    loaderdrv_SetFocus,
    loaderdrv_SetParent,
    loaderdrv_SetWindowPos,
    loaderdrv_SetWindowRgn,
    loaderdrv_SetWindowIcon,
    loaderdrv_SetWindowStyle,
    loaderdrv_SetWindowText,
    loaderdrv_ShowWindow,
    loaderdrv_SysCommandSizeMove,
    loaderdrv_WindowFromDC,
    loaderdrv_WindowMessage
};
