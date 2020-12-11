/*
 * IDirect3D9 implementation
 *
 * Copyright 2002 Jason Edmeades
 * Copyright 2005 Oliver Stieber
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
#include "d3d9_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d9);

/* IDirect3D9 IUnknown parts follow: */
HRESULT WINAPI IDirect3D9Impl_QueryInterface(LPDIRECT3D9 iface, REFIID riid, LPVOID* ppobj)
{
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3D9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3D9Impl_AddRef(LPDIRECT3D9 iface) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3D9Impl_Release(LPDIRECT3D9 iface) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3D_Release(This->WineD3D);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

/* IDirect3D9 Interface follow: */
HRESULT  WINAPI  IDirect3D9Impl_RegisterSoftwareDevice(LPDIRECT3D9 iface, void* pInitializeFunction) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_RegisterSoftwareDevice(This->WineD3D, pInitializeFunction);
}

UINT     WINAPI  IDirect3D9Impl_GetAdapterCount(LPDIRECT3D9 iface) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_GetAdapterCount(This->WineD3D);
}

HRESULT WINAPI IDirect3D9Impl_GetAdapterIdentifier(LPDIRECT3D9 iface, UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    WINED3DADAPTER_IDENTIFIER adapter_id;

    /* dx8 and dx9 have different structures to be filled in, with incompatible 
       layouts so pass in pointers to the places to be filled via an internal 
       structure                                                                */
    adapter_id.Driver           = pIdentifier->Driver;          
    adapter_id.Description      = pIdentifier->Description;     
    adapter_id.DeviceName       = pIdentifier->DeviceName;      
    adapter_id.DriverVersion    = &pIdentifier->DriverVersion;   
    adapter_id.VendorId         = &pIdentifier->VendorId;        
    adapter_id.DeviceId         = &pIdentifier->DeviceId;        
    adapter_id.SubSysId         = &pIdentifier->SubSysId;        
    adapter_id.Revision         = &pIdentifier->Revision;        
    adapter_id.DeviceIdentifier = &pIdentifier->DeviceIdentifier;
    adapter_id.WHQLLevel        = &pIdentifier->WHQLLevel;       

    return IWineD3D_GetAdapterIdentifier(This->WineD3D, Adapter, Flags, &adapter_id);
}

UINT WINAPI IDirect3D9Impl_GetAdapterModeCount(LPDIRECT3D9 iface, UINT Adapter, D3DFORMAT Format) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_GetAdapterModeCount(This->WineD3D, Adapter, Format);
}

HRESULT WINAPI IDirect3D9Impl_EnumAdapterModes(LPDIRECT3D9 iface, UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_EnumAdapterModes(This->WineD3D, Adapter, Format, Mode, pMode);
}

HRESULT WINAPI IDirect3D9Impl_GetAdapterDisplayMode(LPDIRECT3D9 iface, UINT Adapter, D3DDISPLAYMODE* pMode) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_GetAdapterDisplayMode(This->WineD3D, Adapter, pMode);
}

HRESULT WINAPI IDirect3D9Impl_CheckDeviceType(LPDIRECT3D9 iface,
					      UINT Adapter, D3DDEVTYPE CheckType, D3DFORMAT DisplayFormat,
					      D3DFORMAT BackBufferFormat, BOOL Windowed) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_CheckDeviceType(This->WineD3D, Adapter, CheckType, DisplayFormat,
                                    BackBufferFormat, Windowed);
}

HRESULT  WINAPI  IDirect3D9Impl_CheckDeviceFormat(LPDIRECT3D9 iface,
						  UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
						  DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_CheckDeviceFormat(This->WineD3D, Adapter, DeviceType, AdapterFormat,
                                    Usage, RType, CheckFormat);
}

HRESULT  WINAPI  IDirect3D9Impl_CheckDeviceMultiSampleType(LPDIRECT3D9 iface,
							   UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat,
							   BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_CheckDeviceMultiSampleType(This->WineD3D, Adapter, DeviceType, SurfaceFormat,
                                               Windowed, MultiSampleType, pQualityLevels);
}

HRESULT  WINAPI  IDirect3D9Impl_CheckDepthStencilMatch(LPDIRECT3D9 iface, 
						       UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
						       D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_CheckDepthStencilMatch(This->WineD3D, Adapter, DeviceType, AdapterFormat,
                                           RenderTargetFormat, DepthStencilFormat);
}

HRESULT  WINAPI  IDirect3D9Impl_CheckDeviceFormatConversion(LPDIRECT3D9 iface, UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_CheckDeviceFormatConversion(This->WineD3D, Adapter, DeviceType, SourceFormat,
                                                TargetFormat);
}

HRESULT  WINAPI  IDirect3D9Impl_GetDeviceCaps(LPDIRECT3D9 iface, UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    HRESULT hrc = D3D_OK;
    WINED3DCAPS *pWineCaps;

    TRACE("(%p) Relay %d %u %p \n", This, Adapter, DeviceType, pCaps);

    if(NULL == pCaps){
        return D3DERR_INVALIDCALL;
    }
    pWineCaps = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WINED3DCAPS));
    if(pWineCaps == NULL){
        return D3DERR_INVALIDCALL; /*well this is what MSDN says to return*/
    }
    D3D9CAPSTOWINECAPS(pCaps, pWineCaps)
    hrc = IWineD3D_GetDeviceCaps(This->WineD3D, Adapter, DeviceType, pWineCaps);
    HeapFree(GetProcessHeap(), 0, pWineCaps);
    TRACE("(%p) returning %p\n", This, pCaps);
    return hrc;
}

HMONITOR WINAPI  IDirect3D9Impl_GetAdapterMonitor(LPDIRECT3D9 iface, UINT Adapter) {
    IDirect3D9Impl *This = (IDirect3D9Impl *)iface;
    return IWineD3D_GetAdapterMonitor(This->WineD3D, Adapter);
}

/* Internal function called back during the CreateDevice to create a render target */
HRESULT WINAPI D3D9CB_CreateRenderTarget(IUnknown *device, UINT Width, UINT Height,
                                         WINED3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
                                         DWORD MultisampleQuality, BOOL Lockable,
                                         IWineD3DSurface** ppSurface, HANDLE* pSharedHandle) {
    HRESULT res = D3D_OK;
    IDirect3DSurface9Impl *d3dSurface = NULL;
    TRACE("(%p) call back\n", device);
    res = IDirect3DDevice9_CreateRenderTarget((IDirect3DDevice9 *)device, Width, Height, 
                                         (D3DFORMAT)Format, MultiSample, MultisampleQuality, Lockable,
                                         (IDirect3DSurface9 **)&d3dSurface, pSharedHandle);

    if (SUCCEEDED(res)) {
        *ppSurface = d3dSurface->wineD3DSurface;
    } else {
        *ppSurface = NULL;
    }
    return res;
}

HRESULT WINAPI D3D9CB_CreateAdditionalSwapChain(IUnknown *device,
                                                WINED3DPRESENT_PARAMETERS* pPresentationParameters,
                                                IWineD3DSwapChain ** ppSwapChain) {
    HRESULT res = D3D_OK;
    IDirect3DSwapChain9Impl *d3dSwapChain = NULL;
    D3DPRESENT_PARAMETERS localParameters;
    TRACE("(%p) call back\n", device);

    localParameters.BackBufferWidth                = *(pPresentationParameters->BackBufferWidth);
    localParameters.BackBufferHeight               = *(pPresentationParameters->BackBufferHeight);
    localParameters.BackBufferFormat               = *(pPresentationParameters->BackBufferFormat);
    localParameters.BackBufferCount                = *(pPresentationParameters->BackBufferCount);
    localParameters.MultiSampleType                = *(pPresentationParameters->MultiSampleType);
    localParameters.MultiSampleQuality             = *(pPresentationParameters->MultiSampleQuality);
    localParameters.SwapEffect                     = *(pPresentationParameters->SwapEffect);
    localParameters.hDeviceWindow                  = *(pPresentationParameters->hDeviceWindow);
    localParameters.Windowed                       = *(pPresentationParameters->Windowed);
    localParameters.EnableAutoDepthStencil         = *(pPresentationParameters->EnableAutoDepthStencil);
    localParameters.AutoDepthStencilFormat         = *(pPresentationParameters->AutoDepthStencilFormat);
    localParameters.Flags                          = *(pPresentationParameters->Flags);
    localParameters.FullScreen_RefreshRateInHz     = *(pPresentationParameters->FullScreen_RefreshRateInHz);
    localParameters.PresentationInterval           = *(pPresentationParameters->PresentationInterval);

    /*copy the presentation parameters*/
    res = IDirect3DDevice9_CreateAdditionalSwapChain((IDirect3DDevice9 *)device, &localParameters, (IDirect3DSwapChain9 **)&d3dSwapChain);

    if (res == D3D_OK && d3dSwapChain != NULL) {
        *ppSwapChain = d3dSwapChain->wineD3DSwapChain;
    } else {
        *ppSwapChain = NULL;
    }
    /*Copy back the presentation parameters*/
   *pPresentationParameters->BackBufferWidth               = localParameters.BackBufferWidth;
   *pPresentationParameters->BackBufferHeight              = localParameters.BackBufferHeight;
   *pPresentationParameters->BackBufferFormat              = localParameters.BackBufferFormat;
   *pPresentationParameters->BackBufferCount               = localParameters.BackBufferCount;
   *pPresentationParameters->MultiSampleType               = localParameters.MultiSampleType;
   *pPresentationParameters->MultiSampleQuality            = localParameters.MultiSampleQuality;
   *pPresentationParameters->SwapEffect                    = localParameters.SwapEffect;
   *pPresentationParameters->hDeviceWindow                 = localParameters.hDeviceWindow;
   *pPresentationParameters->Windowed                      = localParameters.Windowed;
   *pPresentationParameters->EnableAutoDepthStencil        = localParameters.EnableAutoDepthStencil;
   *pPresentationParameters->AutoDepthStencilFormat        = localParameters.AutoDepthStencilFormat;
   *pPresentationParameters->Flags                         = localParameters.Flags;
   *pPresentationParameters->FullScreen_RefreshRateInHz    = localParameters.FullScreen_RefreshRateInHz;
   *pPresentationParameters->PresentationInterval          = localParameters.PresentationInterval;

   return res;
}

/* Internal function called back during the CreateDevice to create a render target */
HRESULT WINAPI D3D9CB_CreateDepthStencilSurface(IUnknown *device, UINT Width, UINT Height, 
                                         WINED3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
                                         DWORD MultisampleQuality, BOOL Discard,
                                         IWineD3DSurface** ppSurface, HANDLE* pSharedHandle) {
    HRESULT res = D3D_OK;
    IDirect3DSurface9Impl *d3dSurface = NULL;
    TRACE("(%p) call back\n", device);

    res = IDirect3DDevice9_CreateDepthStencilSurface((IDirect3DDevice9 *)device, Width, Height, 
                                         (D3DFORMAT)Format, MultiSample, MultisampleQuality, Discard, 
                                         (IDirect3DSurface9 **)&d3dSurface, pSharedHandle);
    if (res == D3D_OK) {
        *ppSurface = d3dSurface->wineD3DSurface;
    }
    return res;
}


HRESULT  WINAPI  IDirect3D9Impl_CreateDevice(LPDIRECT3D9 iface, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                            DWORD BehaviourFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                                            IDirect3DDevice9** ppReturnedDeviceInterface) {

    IDirect3D9Impl       *This   = (IDirect3D9Impl *)iface;
    IDirect3DDevice9Impl *object = NULL;
    WINED3DPRESENT_PARAMETERS localParameters;
    HRESULT hr;
    TRACE("(%p) Relay \n", This);

    /* Check the validity range of the adapter parameter */
    if (Adapter >= IDirect3D9Impl_GetAdapterCount(iface)) {
        *ppReturnedDeviceInterface = NULL;
        return D3DERR_INVALIDCALL;
    }

    /* Allocate the storage for the device object */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DDevice9Impl));
    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *ppReturnedDeviceInterface = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->lpVtbl = &Direct3DDevice9_Vtbl;
    object->ref = 1;
    *ppReturnedDeviceInterface = (IDirect3DDevice9 *)object;

    /* Allocate an associated WineD3DDevice object */
    localParameters.BackBufferWidth                = &pPresentationParameters->BackBufferWidth;
    localParameters.BackBufferHeight               = &pPresentationParameters->BackBufferHeight;
    localParameters.BackBufferFormat               = (WINED3DFORMAT *)&pPresentationParameters->BackBufferFormat;
    localParameters.BackBufferCount                = &pPresentationParameters->BackBufferCount;
    localParameters.MultiSampleType                = &pPresentationParameters->MultiSampleType;
    localParameters.MultiSampleQuality             = &pPresentationParameters->MultiSampleQuality;
    localParameters.SwapEffect                     = &pPresentationParameters->SwapEffect;
    localParameters.hDeviceWindow                  = &pPresentationParameters->hDeviceWindow;
    localParameters.Windowed                       = &pPresentationParameters->Windowed;
    localParameters.EnableAutoDepthStencil         = &pPresentationParameters->EnableAutoDepthStencil;
    localParameters.AutoDepthStencilFormat         = (WINED3DFORMAT *)&pPresentationParameters->AutoDepthStencilFormat;
    localParameters.Flags                          = &pPresentationParameters->Flags;
    localParameters.FullScreen_RefreshRateInHz     = &pPresentationParameters->FullScreen_RefreshRateInHz; 
    localParameters.PresentationInterval           = &pPresentationParameters->PresentationInterval;

    hr =IWineD3D_CreateDevice(This->WineD3D, Adapter, DeviceType, hFocusWindow, BehaviourFlags, &localParameters, &object->WineD3DDevice, (IUnknown *)object, D3D9CB_CreateAdditionalSwapChain);

    if (hr == D3D_OK) {

        TRACE("(%p) : Created Device %p\n", This, object);
    } else {
        HeapFree(GetProcessHeap(), 0, object);
        *ppReturnedDeviceInterface = NULL;
    }
    return hr;
}



const IDirect3D9Vtbl Direct3D9_Vtbl =
{
    /* IUnknown */
    IDirect3D9Impl_QueryInterface,
    IDirect3D9Impl_AddRef,
    IDirect3D9Impl_Release,
    /* IDirect3D9 */
    IDirect3D9Impl_RegisterSoftwareDevice,
    IDirect3D9Impl_GetAdapterCount,
    IDirect3D9Impl_GetAdapterIdentifier,
    IDirect3D9Impl_GetAdapterModeCount,
    IDirect3D9Impl_EnumAdapterModes,
    IDirect3D9Impl_GetAdapterDisplayMode,
    IDirect3D9Impl_CheckDeviceType,
    IDirect3D9Impl_CheckDeviceFormat,
    IDirect3D9Impl_CheckDeviceMultiSampleType,
    IDirect3D9Impl_CheckDepthStencilMatch,
    IDirect3D9Impl_CheckDeviceFormatConversion,
    IDirect3D9Impl_GetDeviceCaps,
    IDirect3D9Impl_GetAdapterMonitor,
    IDirect3D9Impl_CreateDevice
};
