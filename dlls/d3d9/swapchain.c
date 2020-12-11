/*
 * IDirect3DSwapChain9 implementation
 *
 * Copyright 2002-2003 Jason Edmeades
 *                     Raphael Junqueira
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

/* IDirect3DSwapChain IUnknown parts follow: */
HRESULT WINAPI IDirect3DSwapChain9Impl_QueryInterface(LPDIRECT3DSWAPCHAIN9 iface, REFIID riid, LPVOID* ppobj)
{
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DSwapChain9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DSwapChain9Impl_AddRef(LPDIRECT3DSWAPCHAIN9 iface) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DSwapChain9Impl_Release(LPDIRECT3DSWAPCHAIN9 iface) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DSwapChain_Release(This->wineD3DSwapChain);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DSwapChain9 parts follow: */
HRESULT WINAPI IDirect3DSwapChain9Impl_Present(LPDIRECT3DSWAPCHAIN9 iface, CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion, DWORD dwFlags) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DSwapChain_Present(This->wineD3DSwapChain, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetFrontBufferData(LPDIRECT3DSWAPCHAIN9 iface, IDirect3DSurface9* pDestSurface) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DSwapChain_GetFrontBufferData(This->wineD3DSwapChain,  ((IDirect3DSurface9Impl *)pDestSurface)->wineD3DSurface);
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetBackBuffer(LPDIRECT3DSWAPCHAIN9 iface, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DSurface *mySurface = NULL;

    TRACE("(%p) Relay\n", This);

    hrc = IWineD3DSwapChain_GetBackBuffer(This->wineD3DSwapChain, iBackBuffer, Type, &mySurface);
    if (hrc == D3D_OK && NULL != mySurface) {
       IWineD3DSurface_GetParent(mySurface, (IUnknown **)ppBackBuffer);
       IWineD3DSurface_Release(mySurface);
    }
    return hrc;
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetRasterStatus(LPDIRECT3DSWAPCHAIN9 iface, D3DRASTER_STATUS* pRasterStatus) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DSwapChain_GetRasterStatus(This->wineD3DSwapChain, pRasterStatus);
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetDisplayMode(LPDIRECT3DSWAPCHAIN9 iface, D3DDISPLAYMODE* pMode) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DSwapChain_GetDisplayMode(This->wineD3DSwapChain, pMode);
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetDevice(LPDIRECT3DSWAPCHAIN9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DDevice *device = NULL;

    TRACE("(%p) Relay\n", This);

    hrc = IWineD3DSwapChain_GetDevice(This->wineD3DSwapChain, &device);
    if (hrc == D3D_OK && NULL != device) {
       IWineD3DDevice_GetParent(device, (IUnknown **)ppDevice);
       IWineD3DDevice_Release(device);
    }
    return hrc;
}

HRESULT WINAPI IDirect3DSwapChain9Impl_GetPresentParameters(LPDIRECT3DSWAPCHAIN9 iface, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    IDirect3DSwapChain9Impl *This = (IDirect3DSwapChain9Impl *)iface;
    FIXME("(%p) : inplement using WINED3DPRESENT_PARAMERS\n", This);
    return IWineD3DSwapChain_GetPresentParameters(This->wineD3DSwapChain, pPresentationParameters);
}


const IDirect3DSwapChain9Vtbl Direct3DSwapChain9_Vtbl =
{
    IDirect3DSwapChain9Impl_QueryInterface,
    IDirect3DSwapChain9Impl_AddRef,
    IDirect3DSwapChain9Impl_Release,
    IDirect3DSwapChain9Impl_Present,
    IDirect3DSwapChain9Impl_GetFrontBufferData,
    IDirect3DSwapChain9Impl_GetBackBuffer,
    IDirect3DSwapChain9Impl_GetRasterStatus,
    IDirect3DSwapChain9Impl_GetDisplayMode,
    IDirect3DSwapChain9Impl_GetDevice,
    IDirect3DSwapChain9Impl_GetPresentParameters
};


/* IDirect3DDevice9 IDirect3DSwapChain9 Methods follow: */
HRESULT  WINAPI  IDirect3DDevice9Impl_CreateAdditionalSwapChain(LPDIRECT3DDEVICE9 iface, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSwapChain9Impl* object;
    HRESULT hrc = D3D_OK;
    WINED3DPRESENT_PARAMETERS localParameters;

    TRACE("(%p) Relay\n", This);

    object = HeapAlloc(GetProcessHeap(),  HEAP_ZERO_MEMORY, sizeof(*object));
    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *pSwapChain = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }
    object->ref = 1;
    object->lpVtbl = &Direct3DSwapChain9_Vtbl;

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


    hrc = IWineD3DDevice_CreateAdditionalSwapChain(This->WineD3DDevice, &localParameters, &object->wineD3DSwapChain, (IUnknown*)object, D3D9CB_CreateRenderTarget, D3D9CB_CreateDepthStencilSurface);
    if (hrc != D3D_OK) {
        FIXME("(%p) call to IWineD3DDevice_CreateAdditionalSwapChain failed\n", This);
        HeapFree(GetProcessHeap(), 0 , object);
        *pSwapChain = NULL;
    }else{
        *pSwapChain = (IDirect3DSwapChain9 *)object;
    }
    TRACE("(%p) returning %p\n", This, *pSwapChain);
    return hrc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetSwapChain(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DSwapChain *swapchain = NULL;

    TRACE("(%p) Relay\n", This);

    hrc = IWineD3DDevice_GetSwapChain(This->WineD3DDevice, iSwapChain, &swapchain);
    if (hrc == D3D_OK && NULL != swapchain) {
       IWineD3DSwapChain_GetParent(swapchain, (IUnknown **)pSwapChain);
       IWineD3DSwapChain_Release(swapchain);
    }
    return hrc;
}

UINT     WINAPI  IDirect3DDevice9Impl_GetNumberOfSwapChains(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_GetNumberOfSwapChains(This->WineD3DDevice);
}
