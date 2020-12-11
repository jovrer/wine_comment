/* Unit tests for the progress bar control.
 *
 * Copyright 2005 Michael Kaufmann
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

#include <assert.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "commctrl.h" 

#include "wine/test.h"


HWND hProgressParentWnd, hProgressWnd;
static char progressTestClass[] = "ProgressBarTestClass";


LRESULT CALLBACK ProgressTestWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg) {
    
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
  
    default:
        return DefWindowProcA(hWnd, msg, wParam, lParam);
    }
    
    return 0L;
}


static void update_window(HWND hWnd)
{
    UpdateWindow(hWnd);
    ok(!GetUpdateRect(hWnd, NULL, FALSE), "GetUpdateRect must return zero after UpdateWindow\n");    
}


static void init(void)
{
    WNDCLASSA wc;
    INITCOMMONCONTROLSEX icex;
    RECT rect;
    
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC   = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);
  
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.hIcon = NULL;
    wc.hCursor = LoadCursorA(NULL, MAKEINTRESOURCEA(IDC_ARROW));
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = progressTestClass;
    wc.lpfnWndProc = ProgressTestWndProc;
    RegisterClassA(&wc);
    
    rect.left = 0;
    rect.top = 0;
    rect.right = 400;
    rect.bottom = 20;
    assert(AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE));
    
    hProgressParentWnd = CreateWindowExA(0, progressTestClass, "Progress Bar Test", WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, GetModuleHandleA(NULL), 0);
    assert(hProgressParentWnd != NULL);
    
    GetClientRect(hProgressParentWnd, &rect);
    hProgressWnd = CreateWindowEx(0, PROGRESS_CLASS, "", WS_CHILD | WS_VISIBLE,
      0, 0, rect.right, rect.bottom, hProgressParentWnd, NULL, GetModuleHandleA(NULL), 0);
    assert(hProgressWnd != NULL);
    
    ShowWindow(hProgressParentWnd, SW_SHOWNORMAL);
    ok(GetUpdateRect(hProgressParentWnd, NULL, FALSE), "GetUpdateRect: There should be a region that needs to be updated\n");
    update_window(hProgressParentWnd);    
}


static void cleanup(void)
{
    MSG msg;
    
    PostMessageA(hProgressParentWnd, WM_CLOSE, 0, 0);
    while (GetMessageA(&msg,0,0,0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    
    UnregisterClassA(progressTestClass, GetModuleHandleA(NULL));
}


/*
 * Tests if a progress bar repaints itself immediately when it receives
 * some specific messages.
 */
static void test_redraw(void)
{
    SendMessageA(hProgressWnd, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageA(hProgressWnd, PBM_SETPOS, 10, 0);
    SendMessageA(hProgressWnd, PBM_SETSTEP, 20, 0);
    update_window(hProgressWnd);

    /* PBM_SETPOS */
    ok(SendMessageA(hProgressWnd, PBM_SETPOS, 50, 0) == 10, "PBM_SETPOS must return the previous position\n");
    ok(!GetUpdateRect(hProgressWnd, NULL, FALSE), "PBM_SETPOS: The progress bar should be redrawn immediately\n");
    
    /* PBM_DELTAPOS */
    ok(SendMessageA(hProgressWnd, PBM_DELTAPOS, 15, 0) == 50, "PBM_DELTAPOS must return the previous position\n");
    ok(!GetUpdateRect(hProgressWnd, NULL, FALSE), "PBM_DELTAPOS: The progress bar should be redrawn immediately\n");
    
    /* PBM_SETPOS */
    ok(SendMessageA(hProgressWnd, PBM_SETPOS, 80, 0) == 65, "PBM_SETPOS must return the previous position\n");
    ok(!GetUpdateRect(hProgressWnd, NULL, FALSE), "PBM_SETPOS: The progress bar should be redrawn immediately\n");
    
    /* PBM_STEPIT */
    ok(SendMessageA(hProgressWnd, PBM_STEPIT, 0, 0) == 80, "PBM_STEPIT must return the previous position\n");
    ok(!GetUpdateRect(hProgressWnd, NULL, FALSE), "PBM_STEPIT: The progress bar should be redrawn immediately\n");
    ok((UINT)SendMessageA(hProgressWnd, PBM_GETPOS, 0, 0) == 100, "PBM_GETPOS returned a wrong position\n");
    
    /* PBM_SETRANGE and PBM_SETRANGE32:
    Usually the progress bar doesn't repaint itself immediately. If the
    position is not in the new range, it does.
    Don't test this, it may change in future Windows versions. */
}


START_TEST(progress)
{
    init();
    
    test_redraw();
    
    cleanup();
}
