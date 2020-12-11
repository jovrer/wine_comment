/*
 * Copyright (c) 2004-2005 Robert Reif
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

#define DIRECTINPUT_VERSION 0x0700

#define NONAMELESSSTRUCT
#define NONAMELESSUNION
#include <windows.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "wine/test.h"
#include "windef.h"
#include "wingdi.h"
#include "dinput.h"
#include "dxerr8.h"
#include "dinput_test.h"

#define numObjects(x) (sizeof(x) / sizeof(x[0]))

typedef struct tagUserData {
    LPDIRECTINPUT pDI;
    DWORD version;
} UserData;

static const DIOBJECTDATAFORMAT dfDIJoystickTest[] = {
  { &GUID_XAxis,DIJOFS_X,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_YAxis,DIJOFS_Y,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_ZAxis,DIJOFS_Z,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RxAxis,DIJOFS_RX,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RyAxis,DIJOFS_RY,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_RzAxis,DIJOFS_RZ,DIDFT_OPTIONAL|DIDFT_AXIS|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(0),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(1),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(2),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(3),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(4),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(5),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(6),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(7),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(8),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(9),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
  { &GUID_Button,DIJOFS_BUTTON(10),DIDFT_OPTIONAL|DIDFT_BUTTON|DIDFT_ANYINSTANCE,0},
};

static const DIDATAFORMAT c_dfDIJoystickTest = {
    sizeof(DIDATAFORMAT),
    sizeof(DIOBJECTDATAFORMAT),
    DIDF_ABSAXIS,
    sizeof(DIJOYSTATE2),
    numObjects(dfDIJoystickTest),
    (LPDIOBJECTDATAFORMAT)dfDIJoystickTest
};

HWND get_hwnd(void)
{
    HWND hwnd=GetForegroundWindow();
    if (!hwnd)
        hwnd=GetDesktopWindow();
    return hwnd;
}

typedef struct tagJoystickInfo
{
    LPDIRECTINPUTDEVICE pJoystick;
    DWORD axis;
    DWORD pov;
    DWORD button;
} JoystickInfo;

static BOOL CALLBACK EnumAxes(
    const DIDEVICEOBJECTINSTANCE* pdidoi,
    VOID* pContext)
{
    HRESULT hr;
    JoystickInfo * info = (JoystickInfo *)pContext;

    if (IsEqualIID(&pdidoi->guidType, &GUID_XAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_YAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_ZAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_RxAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_RyAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_RzAxis) ||
        IsEqualIID(&pdidoi->guidType, &GUID_Slider)) {
        DIPROPRANGE diprg;
        diprg.diph.dwSize       = sizeof(DIPROPRANGE);
        diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        diprg.diph.dwHow        = DIPH_BYID;
        diprg.diph.dwObj        = pdidoi->dwType;
        diprg.lMin              = -1000;
        diprg.lMax              = +1000;

        hr = IDirectInputDevice_SetProperty(info->pJoystick, DIPROP_RANGE, NULL);
        ok(hr==E_INVALIDARG,"IDirectInputDevice_SetProperty() should have returned "
           "E_INVALIDARG, returned: %s\n", DXGetErrorString8(hr));

        hr = IDirectInputDevice_SetProperty(info->pJoystick, DIPROP_RANGE, &diprg.diph);
        ok(hr==DI_OK,"IDirectInputDevice_SetProperty() failed: %s\n", DXGetErrorString8(hr));

        info->axis++;
    } else if (IsEqualIID(&pdidoi->guidType, &GUID_POV))
        info->pov++;
    else if (IsEqualIID(&pdidoi->guidType, &GUID_Button))
        info->button++;

    return DIENUM_CONTINUE;
}

static BOOL CALLBACK EnumJoysticks(
    LPCDIDEVICEINSTANCE lpddi,
    LPVOID pvRef)
{
    HRESULT hr;
    UserData * data = (UserData *)pvRef;
    LPDIRECTINPUTDEVICE pJoystick;
    DIDATAFORMAT format;
    DIDEVCAPS caps;
    DIJOYSTATE2 js;
    JoystickInfo info;
    int i, count;
    ULONG ref;
    DIDEVICEINSTANCE inst;
    DIDEVICEINSTANCE_DX3 inst3;
    HWND hWnd = get_hwnd();
    char oldstate[248], curstate[248];

    ok(data->version > 0x0300, "Joysticks not supported in version 0x%04lx\n", data->version);
 
    hr = IDirectInput_CreateDevice(data->pDI, &lpddi->guidInstance, NULL, NULL);
    ok(hr==E_POINTER,"IDirectInput_CreateDevice() should have returned "
       "E_POINTER, returned: %s\n", DXGetErrorString8(hr));

    hr = IDirectInput_CreateDevice(data->pDI, NULL, &pJoystick, NULL);
    ok(hr==E_POINTER,"IDirectInput_CreateDevice() should have returned "
       "E_POINTER, returned: %s\n", DXGetErrorString8(hr));

    hr = IDirectInput_CreateDevice(data->pDI, NULL, NULL, NULL);
    ok(hr==E_POINTER,"IDirectInput_CreateDevice() should have returned "
       "E_POINTER, returned: %s\n", DXGetErrorString8(hr));

    hr = IDirectInput_CreateDevice(data->pDI, &lpddi->guidInstance,
                                   &pJoystick, NULL);
    ok(hr==DI_OK,"IDirectInput_CreateDevice() failed: %s\n",
       DXGetErrorString8(hr));
    if (hr!=DI_OK)
        goto DONE;

    trace("---- %s ----\n", lpddi->tszProductName);

    hr = IDirectInputDevice_SetDataFormat(pJoystick, NULL);
    ok(hr==E_POINTER,"IDirectInputDevice_SetDataFormat() should have returned "
       "E_POINTER, returned: %s\n", DXGetErrorString8(hr));

    ZeroMemory(&format, sizeof(format));
    hr = IDirectInputDevice_SetDataFormat(pJoystick, &format);
    ok(hr==DIERR_INVALIDPARAM,"IDirectInputDevice_SetDataFormat() should have "
       "returned DIERR_INVALIDPARAM, returned: %s\n", DXGetErrorString8(hr));

    /* try the default formats */
    hr = IDirectInputDevice_SetDataFormat(pJoystick, &c_dfDIJoystick);
    ok(hr==DI_OK,"IDirectInputDevice_SetDataFormat() failed: %s\n",
       DXGetErrorString8(hr));

    hr = IDirectInputDevice_SetDataFormat(pJoystick, &c_dfDIJoystick2);
    ok(hr==DI_OK,"IDirectInputDevice_SetDataFormat() failed: %s\n",
       DXGetErrorString8(hr));

    /* try an alternate format */
    hr = IDirectInputDevice_SetDataFormat(pJoystick, &c_dfDIJoystickTest);
    ok(hr==DI_OK,"IDirectInputDevice_SetDataFormat() failed: %s\n",
       DXGetErrorString8(hr));

    hr = IDirectInputDevice_SetDataFormat(pJoystick, &c_dfDIJoystick2);
    ok(hr==DI_OK,"IDirectInputDevice_SetDataFormat() failed: %s\n",
       DXGetErrorString8(hr));
    if (hr != DI_OK)
        goto RELEASE;

    hr = IDirectInputDevice_SetCooperativeLevel(pJoystick, hWnd,
                                                DISCL_NONEXCLUSIVE | DISCL_BACKGROUND);
    ok(hr==DI_OK,"IDirectInputDevice_SetCooperativeLevel() failed: %s\n",
       DXGetErrorString8(hr));

    /* get capabilities */
    hr = IDirectInputDevice_GetCapabilities(pJoystick, NULL);
    ok(hr==E_POINTER,"IDirectInputDevice_GetCapabilities() "
       "should have returned E_POINTER, returned: %s\n",
       DXGetErrorString8(hr));

    ZeroMemory(&caps, sizeof(caps));
    hr = IDirectInputDevice_GetCapabilities(pJoystick, &caps);
    ok(hr==DIERR_INVALIDPARAM,"IDirectInputDevice_GetCapabilities() "
       "should have returned DIERR_INVALIDPARAM, returned: %s\n",
       DXGetErrorString8(hr));

    caps.dwSize = sizeof(caps);
    hr = IDirectInputDevice_GetCapabilities(pJoystick, &caps);
    ok(hr==DI_OK,"IDirectInputDevice_GetCapabilities() failed: %s\n",
       DXGetErrorString8(hr));

    ZeroMemory(&info, sizeof(info));
    info.pJoystick = pJoystick;

    /* enumerate objects */
    hr = IDirectInputDevice_EnumObjects(pJoystick, EnumAxes, (VOID*)&info, DIDFT_ALL);
    ok(hr==DI_OK,"IDirectInputDevice_EnumObjects() failed: %s\n",
       DXGetErrorString8(hr));

    ok(caps.dwAxes == info.axis, "Number of enumerated axes doesn't match capabilities\n");
    ok(caps.dwButtons == info.button, "Number of enumerated buttons doesn't match capabilities\n");
    ok(caps.dwPOVs == info.pov, "Number of enumerated POVs doesn't match capabilities\n");

    hr = IDirectInputDevice_GetDeviceInfo(pJoystick, 0);
    ok(hr==E_POINTER, "IDirectInputDevice_GetDeviceInfo() "
       "should have returned E_POINTER, returned: %s\n",
       DXGetErrorString8(hr));

    ZeroMemory(&inst, sizeof(inst));
    ZeroMemory(&inst3, sizeof(inst3));

    hr = IDirectInputDevice_GetDeviceInfo(pJoystick, &inst);
    ok(hr==DIERR_INVALIDPARAM, "IDirectInputDevice_GetDeviceInfo() "
       "should have returned DIERR_INVALIDPARAM, returned: %s\n",
       DXGetErrorString8(hr));

    inst.dwSize = sizeof(inst);
    hr = IDirectInputDevice_GetDeviceInfo(pJoystick, &inst);
    ok(hr==DI_OK,"IDirectInputDevice_GetDeviceInfo() failed: %s\n",
       DXGetErrorString8(hr));

    inst3.dwSize = sizeof(inst3);
    hr = IDirectInputDevice_GetDeviceInfo(pJoystick, (LPDIDEVICEINSTANCE)&inst3);
    ok(hr==DI_OK,"IDirectInputDevice_GetDeviceInfo() failed: %s\n",
       DXGetErrorString8(hr));

    hr = IDirectInputDevice_Acquire(pJoystick);
    ok(hr==DI_OK,"IDirectInputDevice_Acquire() failed: %s\n",
       DXGetErrorString8(hr));
    if (hr != DI_OK)
        goto RELEASE;

    if (winetest_interactive) {
        trace("You have 30 seconds to test all axes, sliders, POVs and buttons\n");
        count = 300;
    } else
        count = 1;

    trace("\n");
    oldstate[0]='\0';
    for (i = 0; i < count; i++) {
        hr = IDirectInputDevice_GetDeviceState(pJoystick, sizeof(DIJOYSTATE2), &js);
        ok(hr==DI_OK,"IDirectInputDevice_GetDeviceState() failed: %s\n",
           DXGetErrorString8(hr));
        if (hr != DI_OK)
            break;
        sprintf(curstate, "X%5ld Y%5ld Z%5ld Rx%5ld Ry%5ld Rz%5ld "
              "S0%5ld S1%5ld POV0%5ld POV1%5ld POV2%5ld POV3%5ld "
              "B %d %d %d %d %d %d %d %d %d %d %d %d\n",
              js.lX, js.lY, js.lZ, js.lRx, js.lRy, js.lRz,
              js.rglSlider[0], js.rglSlider[1],
              js.rgdwPOV[0], js.rgdwPOV[1], js.rgdwPOV[2], js.rgdwPOV[3],
              js.rgbButtons[0]>>7, js.rgbButtons[1]>>7, js.rgbButtons[2]>>7,
              js.rgbButtons[3]>>7, js.rgbButtons[4]>>7, js.rgbButtons[5]>>7,
              js.rgbButtons[6]>>7, js.rgbButtons[7]>>7, js.rgbButtons[8]>>7,
              js.rgbButtons[9]>>7, js.rgbButtons[10]>>7, js.rgbButtons[11]>>7);
        if (strcmp(oldstate, curstate) != 0)
        {
            trace(curstate);
            strcpy(oldstate, curstate);
        }
        Sleep(100);
    }
    trace("\n");

    hr = IDirectInputDevice_Unacquire(pJoystick);
    ok(hr==DI_OK,"IDirectInputDevice_Unacquire() failed: %s\n",
       DXGetErrorString8(hr));

RELEASE:
    ref = IDirectInputDevice_Release(pJoystick);
    ok(ref==0,"IDirectInputDevice_Release() reference count = %ld\n", ref);

DONE:
    return DIENUM_CONTINUE;
}

static void joystick_tests(DWORD version)
{
    HRESULT hr;
    LPDIRECTINPUT pDI;
    ULONG ref;
    HINSTANCE hInstance = GetModuleHandle(NULL);

    trace("-- Testing Direct Input Version 0x%04lx --\n", version);
    hr = DirectInputCreate(hInstance, version, &pDI, NULL);
    ok(hr==DI_OK||hr==DIERR_OLDDIRECTINPUTVERSION,
       "DirectInputCreate() failed: %s\n", DXGetErrorString8(hr));
    if (hr==DI_OK && pDI!=0) {
        UserData data;
        data.pDI = pDI;
        data.version = version;
        hr = IDirectInput_EnumDevices(pDI, DIDEVTYPE_JOYSTICK, EnumJoysticks,
                                      &data, DIEDFL_ALLDEVICES);
        ok(hr==DI_OK,"IDirectInput_EnumDevices() failed: %s\n",
           DXGetErrorString8(hr));
        ref = IDirectInput_Release(pDI);
        ok(ref==0,"IDirectInput_Release() reference count = %ld\n", ref);
    } else if (hr==DIERR_OLDDIRECTINPUTVERSION)
        trace("  Version Not Supported\n");
}

START_TEST(joystick)
{
    CoInitialize(NULL);

    trace("DLL Version: %s\n", get_file_version("dinput.dll"));

    joystick_tests(0x0700);
    joystick_tests(0x0500);
    joystick_tests(0x0300);

    CoUninitialize();
}
