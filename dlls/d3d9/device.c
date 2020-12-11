/*
 * IDirect3DDevice9 implementation
 *
 * Copyright 2002-2005 Jason Edmeades
 * Copyright 2002-2005 Raphael Junqueira
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


/* IDirect3D IUnknown parts follow: */
HRESULT WINAPI IDirect3DDevice9Impl_QueryInterface(LPDIRECT3DDEVICE9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DDevice9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DDevice9Impl_AddRef(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DDevice9Impl_Release(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
      IWineD3DDevice_Release(This->WineD3DDevice);
      HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DDevice Interface follow: */
HRESULT  WINAPI  IDirect3DDevice9Impl_TestCooperativeLevel(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_TestCooperativeLevel(This->WineD3DDevice);
}

UINT     WINAPI  IDirect3DDevice9Impl_GetAvailableTextureMem(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_GetAvailableTextureMem(This->WineD3DDevice);    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_EvictManagedResources(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;

    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_EvictManagedResources(This->WineD3DDevice);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetDirect3D(LPDIRECT3DDEVICE9 iface, IDirect3D9** ppD3D9) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hr = D3D_OK;
    IWineD3D* pWineD3D;

    TRACE("(%p) Relay\n", This);

    if (NULL == ppD3D9) {
        return D3DERR_INVALIDCALL;
    }
    hr = IWineD3DDevice_GetDirect3D(This->WineD3DDevice, &pWineD3D);
    if (hr == D3D_OK && pWineD3D != NULL)
    {
        IWineD3DResource_GetParent((IWineD3DResource *)pWineD3D,(IUnknown **)ppD3D9);
        IWineD3DResource_Release((IWineD3DResource *)pWineD3D);
    } else {
        FIXME("Call to IWineD3DDevice_GetDirect3D failed\n");
        *ppD3D9 = NULL;
    }
    TRACE("(%p) returning %p\b",This , *ppD3D9);
    return hr;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetDeviceCaps(LPDIRECT3DDEVICE9 iface, D3DCAPS9* pCaps) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;
    WINED3DCAPS *pWineCaps;

    TRACE("(%p) : Relay pCaps %p \n", This, pCaps);
    if(NULL == pCaps){
        return D3DERR_INVALIDCALL;
    }
    pWineCaps = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WINED3DCAPS));
    if(pWineCaps == NULL){
        return D3DERR_INVALIDCALL; /* well this is what MSDN says to return */
    }

    D3D9CAPSTOWINECAPS(pCaps, pWineCaps)
    hrc = IWineD3DDevice_GetDeviceCaps(This->WineD3DDevice, pWineCaps);
    HeapFree(GetProcessHeap(), 0, pWineCaps);
    TRACE("Returning %p %p\n", This, pCaps);
    return hrc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetDisplayMode(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, D3DDISPLAYMODE* pMode) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_GetDisplayMode(This->WineD3DDevice, iSwapChain, pMode);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetCreationParameters(LPDIRECT3DDEVICE9 iface, D3DDEVICE_CREATION_PARAMETERS *pParameters) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_GetCreationParameters(This->WineD3DDevice, pParameters);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetCursorProperties(LPDIRECT3DDEVICE9 iface, UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *pSurface = (IDirect3DSurface9Impl*)pCursorBitmap;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_SetCursorProperties(This->WineD3DDevice,XHotSpot,YHotSpot,(IWineD3DSurface*)pSurface->wineD3DSurface);
}

void     WINAPI  IDirect3DDevice9Impl_SetCursorPosition(LPDIRECT3DDEVICE9 iface, int XScreenSpace, int YScreenSpace, DWORD Flags) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_SetCursorPosition(This->WineD3DDevice, XScreenSpace, YScreenSpace, Flags);
}

BOOL     WINAPI  IDirect3DDevice9Impl_ShowCursor(LPDIRECT3DDEVICE9 iface, BOOL bShow) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);

    return IWineD3DDevice_ShowCursor(This->WineD3DDevice, bShow);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_Reset(LPDIRECT3DDEVICE9 iface, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    WINED3DPRESENT_PARAMETERS localParameters;
    TRACE("(%p) Relay pPresentationParameters(%p)\n", This, pPresentationParameters);
        
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
    return IWineD3DDevice_Reset(This->WineD3DDevice, &localParameters);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_Present(LPDIRECT3DDEVICE9 iface, CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA*
 pDirtyRegion) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_Present(This->WineD3DDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetBackBuffer(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, UINT BackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 ** ppBackBuffer) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IWineD3DSurface *retSurface = NULL;
    HRESULT rc = D3D_OK;

    TRACE("(%p) Relay\n", This);

    rc = IWineD3DDevice_GetBackBuffer(This->WineD3DDevice, iSwapChain, BackBuffer, Type, (IWineD3DSurface **)&retSurface);
    if (rc == D3D_OK && NULL != retSurface && NULL != ppBackBuffer) {
        IWineD3DSurface_GetParent(retSurface, (IUnknown **)ppBackBuffer);
        IWineD3DSurface_Release(retSurface);
    }
    return rc;
}
HRESULT  WINAPI  IDirect3DDevice9Impl_GetRasterStatus(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    
    return IWineD3DDevice_GetRasterStatus(This->WineD3DDevice, iSwapChain, pRasterStatus);        
}

HRESULT WINAPI IDirect3DDevice9Impl_SetDialogBoxMode(LPDIRECT3DDEVICE9 iface, BOOL bEnableDialogs) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    
    return IWineD3DDevice_SetDialogBoxMode(This->WineD3DDevice, bEnableDialogs);
}

void WINAPI IDirect3DDevice9Impl_SetGammaRamp(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) {
    
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    
    return IWineD3DDevice_SetGammaRamp(This->WineD3DDevice, iSwapChain, Flags, pRamp);
}

void WINAPI IDirect3DDevice9Impl_GetGammaRamp(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, D3DGAMMARAMP* pRamp) {    
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    
    return IWineD3DDevice_GetGammaRamp(This->WineD3DDevice, iSwapChain, pRamp);
}


HRESULT  WINAPI IDirect3DDevice9Impl_CreateSurface(LPDIRECT3DDEVICE9 iface, UINT Width, UINT Height, D3DFORMAT Format, BOOL Lockable, BOOL Discard, UINT Level, IDirect3DSurface9 **ppSurface,D3DRESOURCETYPE Type, UINT Usage,D3DPOOL Pool, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality,HANDLE* pSharedHandle )  {
    HRESULT hrc;
    IDirect3DSurface9Impl *object;
    IDirect3DDevice9Impl  *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    if(MultisampleQuality < 0) { 
        FIXME("MultisampleQuality out of range %ld, substituting 0 \n", MultisampleQuality);
    /*FIXME: Find out what windows does with a MultisampleQuality < 0 */
        MultisampleQuality=0;
    }
    
    if(MultisampleQuality > 0){
        FIXME("MultisampleQuality set to %ld, bstituting 0  \n" , MultisampleQuality);
    /*
    MultisampleQuality
 [in] Quality level. The valid range is between zero and one less than the level returned by pQualityLevels used by IDirect3D9::CheckDeviceMultiSampleType. Passing a larger value returns the error D3DERR_INVALIDCALL. The MultisampleQuality values of paired render targets, depth stencil surfaces, and the MultiSample type must all match.
 */
 
        MultisampleQuality=0;
    }
    /*FIXME: Check MAX bounds of MultisampleQuality*/

    /* Allocate the storage for the device */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DSurface9Impl));
    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *ppSurface = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->lpVtbl = &Direct3DSurface9_Vtbl;
    object->ref = 1;
    
    TRACE("(%p) : w(%d) h(%d) fmt(%d) surf@%p\n", This, Width, Height, Format, *ppSurface);
           
    hrc = IWineD3DDevice_CreateSurface(This->WineD3DDevice, Width, Height, Format, Lockable, Discard, Level,  &object->wineD3DSurface, Type, Usage, Pool,MultiSample,MultisampleQuality,pSharedHandle,(IUnknown *)object);
    
    if (hrc != D3D_OK || NULL == object->wineD3DSurface) {
       /* free up object */
        FIXME("(%p) call to IWineD3DDevice_CreateSurface failed\n", This);
        HeapFree(GetProcessHeap(), 0, object);
        *ppSurface = NULL;
    } else {
        *ppSurface = (LPDIRECT3DSURFACE9) object;
    }  
    return hrc;
}



HRESULT  WINAPI  IDirect3DDevice9Impl_CreateRenderTarget(LPDIRECT3DDEVICE9 iface, UINT Width, UINT Height, 
                                                         D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, 
                                                         DWORD MultisampleQuality, BOOL Lockable, 
                                                         IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle) {
   TRACE("Relay\n");
   /* Is this correct? */
   return IDirect3DDevice9Impl_CreateSurface(iface,Width,Height,Format,Lockable,FALSE/*Discard*/, 0/*Level*/, ppSurface,D3DRTYPE_SURFACE,D3DUSAGE_RENDERTARGET,D3DPOOL_DEFAULT,MultiSample,MultisampleQuality,pSharedHandle);
     

}

HRESULT  WINAPI  IDirect3DDevice9Impl_CreateDepthStencilSurface(LPDIRECT3DDEVICE9 iface, UINT Width, UINT Height,
                                                                D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
                                                                DWORD MultisampleQuality, BOOL Discard,
                                                                IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle) {
     TRACE("Relay\n");
     return IDirect3DDevice9Impl_CreateSurface(iface,Width,Height,Format,TRUE/* Lockable */,Discard, 0/* Level */
                                               ,ppSurface,D3DRTYPE_SURFACE,D3DUSAGE_DEPTHSTENCIL,
                                                D3DPOOL_DEFAULT,MultiSample,MultisampleQuality,pSharedHandle);
}


HRESULT  WINAPI  IDirect3DDevice9Impl_UpdateSurface(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;     
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_UpdateSurface(This->WineD3DDevice, ((IDirect3DSurface9Impl *)pSourceSurface)->wineD3DSurface, pSourceRect, ((IDirect3DSurface9Impl *)pDestinationSurface)->wineD3DSurface, pDestPoint);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_UpdateTexture(LPDIRECT3DDEVICE9 iface, IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_UpdateTexture(This->WineD3DDevice,  ((IDirect3DBaseTexture9Impl *)pSourceTexture)->wineD3DBaseTexture, ((IDirect3DBaseTexture9Impl *)pDestinationTexture)->wineD3DBaseTexture);
}

/* This isn't in MSDN!
HRESULT  WINAPI  IDirect3DDevice9Impl_GetFrontBuffer(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pDestSurface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    FIXME("(%p) : stub\n", This);
    return D3D_OK;
}
*/

HRESULT  WINAPI  IDirect3DDevice9Impl_GetRenderTargetData(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *renderTarget = (IDirect3DSurface9Impl *)pRenderTarget;
    IDirect3DSurface9Impl *destSurface = (IDirect3DSurface9Impl *)pDestSurface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetRenderTargetData(This->WineD3DDevice, renderTarget->wineD3DSurface, destSurface->wineD3DSurface);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetFrontBufferData(LPDIRECT3DDEVICE9 iface, UINT iSwapChain, IDirect3DSurface9* pDestSurface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *destSurface = (IDirect3DSurface9Impl *)pDestSurface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetFrontBufferData(This->WineD3DDevice, iSwapChain, destSurface->wineD3DSurface);    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_StretchRect(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_StretchRect(This->WineD3DDevice, ((IDirect3DSurface9Impl *)pSourceSurface)->wineD3DSurface, pSourceRect, ((IDirect3DSurface9Impl *)pDestSurface)->wineD3DSurface, pDestRect, Filter);            
}

HRESULT  WINAPI  IDirect3DDevice9Impl_ColorFill(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *surface = (IDirect3DSurface9Impl *)pSurface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_ColorFill(This->WineD3DDevice, surface->wineD3DSurface, (CONST D3DRECT*)pRect, color);    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_CreateOffscreenPlainSurface(LPDIRECT3DDEVICE9 iface, UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle) {
    TRACE("Relay\n");
    if(Pool == D3DPOOL_MANAGED ){
        FIXME("Attempting to create a managed offscreen plain surface\n");
        return D3DERR_INVALIDCALL;
    }    
        /*MSDN: D3DPOOL_SCRATCH will return a surface that has identical characteristics to a surface created by the Microsoft DirectX 8.x method CreateImageSurface.
        
        'Off-screen plain surfaces are always lockable, regardless of their pool types.'
        but then...
        D3DPOOL_DEFAULT is the appropriate pool for use with the IDirect3DDevice9::StretchRect and IDirect3DDevice9::ColorFill.
        Why, their always lockable?
        should I change the usage to dynamic?        
        */
    return IDirect3DDevice9Impl_CreateSurface(iface,Width,Height,Format,TRUE/*Loackable*/,FALSE/*Discard*/,0/*Level*/ , ppSurface,D3DRTYPE_SURFACE, 0/*Usage (undefined/none)*/,Pool,D3DMULTISAMPLE_NONE,0/*MultisampleQuality*/,pSharedHandle);
}

/* TODO: move to wineD3D */
HRESULT  WINAPI  IDirect3DDevice9Impl_SetRenderTarget(LPDIRECT3DDEVICE9 iface, DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *pSurface = (IDirect3DSurface9Impl*)pRenderTarget;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetRenderTarget(This->WineD3DDevice,RenderTargetIndex,(IWineD3DSurface*)pSurface->wineD3DSurface);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetRenderTarget(LPDIRECT3DDEVICE9 iface, DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hr = D3D_OK;
    IWineD3DSurface *pRenderTarget;

    TRACE("(%p) Relay\n" , This);

    if (ppRenderTarget == NULL) {
        return D3DERR_INVALIDCALL;
    }
    hr=IWineD3DDevice_GetRenderTarget(This->WineD3DDevice,RenderTargetIndex,&pRenderTarget);

    if (hr == D3D_OK && pRenderTarget != NULL) {
        IWineD3DResource_GetParent((IWineD3DResource *)pRenderTarget,(IUnknown**)ppRenderTarget);
        IWineD3DResource_Release((IWineD3DResource *)pRenderTarget);
    } else {
        FIXME("Call to IWineD3DDevice_GetRenderTarget failed\n");
        *ppRenderTarget = NULL;
    }
    return hr;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetDepthStencilSurface(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9* pZStencilSurface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IDirect3DSurface9Impl *pSurface;

    TRACE("(%p) Relay\n" , This);

    pSurface = (IDirect3DSurface9Impl*)pZStencilSurface;
    return IWineD3DDevice_SetDepthStencilSurface(This->WineD3DDevice,NULL==pSurface?NULL:(IWineD3DSurface*)pSurface->wineD3DSurface);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetDepthStencilSurface(LPDIRECT3DDEVICE9 iface, IDirect3DSurface9 **ppZStencilSurface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hr = D3D_OK;
    IWineD3DSurface *pZStencilSurface;

    TRACE("(%p) Relay\n" , This);
    if(ppZStencilSurface == NULL){
        return D3DERR_INVALIDCALL;
    }

    hr=IWineD3DDevice_GetDepthStencilSurface(This->WineD3DDevice,&pZStencilSurface);
    if(hr == D3D_OK && pZStencilSurface != NULL){
        IWineD3DResource_GetParent((IWineD3DResource *)pZStencilSurface,(IUnknown**)ppZStencilSurface);
        IWineD3DResource_Release((IWineD3DResource *)pZStencilSurface);
    }else{
        FIXME("Call to IWineD3DDevice_GetRenderTarget failed\n");
        *ppZStencilSurface = NULL;
    }
    return D3D_OK;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_BeginScene(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_BeginScene(This->WineD3DDevice);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_EndScene(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_EndScene(This->WineD3DDevice);
    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_Clear(LPDIRECT3DDEVICE9 iface, DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_Clear(This->WineD3DDevice, Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetTransform(LPDIRECT3DDEVICE9 iface, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* lpMatrix) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetTransform(This->WineD3DDevice, State, lpMatrix);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetTransform(LPDIRECT3DDEVICE9 iface, D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetTransform(This->WineD3DDevice, State, pMatrix);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_MultiplyTransform(LPDIRECT3DDEVICE9 iface, D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_MultiplyTransform(This->WineD3DDevice, State, pMatrix);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetViewport(LPDIRECT3DDEVICE9 iface, CONST D3DVIEWPORT9* pViewport) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetViewport(This->WineD3DDevice, (const WINED3DVIEWPORT *)pViewport);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetViewport(LPDIRECT3DDEVICE9 iface, D3DVIEWPORT9* pViewport) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetViewport(This->WineD3DDevice, (WINED3DVIEWPORT *)pViewport);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetMaterial(LPDIRECT3DDEVICE9 iface, CONST D3DMATERIAL9* pMaterial) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetMaterial(This->WineD3DDevice, (const WINED3DMATERIAL *)pMaterial);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetMaterial(LPDIRECT3DDEVICE9 iface, D3DMATERIAL9* pMaterial) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetMaterial(This->WineD3DDevice, (WINED3DMATERIAL *)pMaterial);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetLight(LPDIRECT3DDEVICE9 iface, DWORD Index, CONST D3DLIGHT9* pLight) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetLight(This->WineD3DDevice, Index, (const WINED3DLIGHT *)pLight);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetLight(LPDIRECT3DDEVICE9 iface, DWORD Index, D3DLIGHT9* pLight) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetLight(This->WineD3DDevice, Index, (WINED3DLIGHT *)pLight);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_LightEnable(LPDIRECT3DDEVICE9 iface, DWORD Index, BOOL Enable) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetLightEnable(This->WineD3DDevice, Index, Enable);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetLightEnable(LPDIRECT3DDEVICE9 iface, DWORD Index, BOOL* pEnable) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetLightEnable(This->WineD3DDevice, Index, pEnable);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetClipPlane(LPDIRECT3DDEVICE9 iface, DWORD Index, CONST float* pPlane) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetClipPlane(This->WineD3DDevice, Index, pPlane);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetClipPlane(LPDIRECT3DDEVICE9 iface, DWORD Index, float* pPlane) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetClipPlane(This->WineD3DDevice, Index, pPlane);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetRenderState(LPDIRECT3DDEVICE9 iface, D3DRENDERSTATETYPE State, DWORD Value) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetRenderState(This->WineD3DDevice, State, Value);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetRenderState(LPDIRECT3DDEVICE9 iface, D3DRENDERSTATETYPE State, DWORD* pValue) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetRenderState(This->WineD3DDevice, State, pValue);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetClipStatus(LPDIRECT3DDEVICE9 iface, CONST D3DCLIPSTATUS9* pClipStatus) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetClipStatus(This->WineD3DDevice, (const WINED3DCLIPSTATUS *)pClipStatus);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetClipStatus(LPDIRECT3DDEVICE9 iface, D3DCLIPSTATUS9* pClipStatus) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetClipStatus(This->WineD3DDevice, (WINED3DCLIPSTATUS *)pClipStatus);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetTexture(LPDIRECT3DDEVICE9 iface, DWORD Stage, IDirect3DBaseTexture9 **ppTexture) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IWineD3DBaseTexture *retTexture = NULL;
    HRESULT rc = D3D_OK;

    TRACE("(%p) Relay\n" , This);

    if(ppTexture == NULL){
        return D3DERR_INVALIDCALL;
    }

    rc = IWineD3DDevice_GetTexture(This->WineD3DDevice, Stage, (IWineD3DBaseTexture **)&retTexture);
    if (rc == D3D_OK && NULL != retTexture) {
        IWineD3DBaseTexture_GetParent(retTexture, (IUnknown **)ppTexture);
        IWineD3DBaseTexture_Release(retTexture);
    }else{
        FIXME("Call to get texture  (%ld) failed (%p) \n", Stage, retTexture);
        *ppTexture = NULL;
    }
    return rc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetTexture(LPDIRECT3DDEVICE9 iface, DWORD Stage, IDirect3DBaseTexture9* pTexture) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay %ld %p\n" , This, Stage, pTexture);
    return IWineD3DDevice_SetTexture(This->WineD3DDevice, Stage,
                                     pTexture==NULL ? NULL:((IDirect3DBaseTexture9Impl *)pTexture)->wineD3DBaseTexture); 
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetTextureStageState(LPDIRECT3DDEVICE9 iface, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetTextureStageState(This->WineD3DDevice, Stage, Type, pValue);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetTextureStageState(LPDIRECT3DDEVICE9 iface, DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetTextureStageState(This->WineD3DDevice, Stage, Type, Value);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetSamplerState(LPDIRECT3DDEVICE9 iface, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetSamplerState(This->WineD3DDevice, Sampler, Type, pValue);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetSamplerState(LPDIRECT3DDEVICE9 iface, DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetSamplerState(This->WineD3DDevice, Sampler, Type, Value);    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_ValidateDevice(LPDIRECT3DDEVICE9 iface, DWORD* pNumPasses) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_ValidateDevice(This->WineD3DDevice, pNumPasses);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetPaletteEntries(LPDIRECT3DDEVICE9 iface, UINT PaletteNumber, CONST PALETTEENTRY* pEntries) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetPaletteEntries(This->WineD3DDevice, PaletteNumber, pEntries);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetPaletteEntries(LPDIRECT3DDEVICE9 iface, UINT PaletteNumber, PALETTEENTRY* pEntries) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetPaletteEntries(This->WineD3DDevice, PaletteNumber, pEntries);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetCurrentTexturePalette(LPDIRECT3DDEVICE9 iface, UINT PaletteNumber) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetCurrentTexturePalette(This->WineD3DDevice, PaletteNumber);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetCurrentTexturePalette(LPDIRECT3DDEVICE9 iface, UINT* PaletteNumber) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetCurrentTexturePalette(This->WineD3DDevice, PaletteNumber);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetScissorRect(LPDIRECT3DDEVICE9 iface, CONST RECT* pRect) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetScissorRect(This->WineD3DDevice, pRect);    
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetScissorRect(LPDIRECT3DDEVICE9 iface, RECT* pRect) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetScissorRect(This->WineD3DDevice, pRect);   
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetSoftwareVertexProcessing(LPDIRECT3DDEVICE9 iface, BOOL bSoftware) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetSoftwareVertexProcessing(This->WineD3DDevice, bSoftware);
}

BOOL     WINAPI  IDirect3DDevice9Impl_GetSoftwareVertexProcessing(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetSoftwareVertexProcessing(This->WineD3DDevice);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetNPatchMode(LPDIRECT3DDEVICE9 iface, float nSegments) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetNPatchMode(This->WineD3DDevice, nSegments);
}

float    WINAPI  IDirect3DDevice9Impl_GetNPatchMode(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetNPatchMode(This->WineD3DDevice);
}

HRESULT WINAPI IDirect3DDevice9Impl_DrawPrimitive(LPDIRECT3DDEVICE9 iface, D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_DrawPrimitive(This->WineD3DDevice, PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_DrawIndexedPrimitive(LPDIRECT3DDEVICE9 iface, D3DPRIMITIVETYPE PrimitiveType,
                                                           INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface; 
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_DrawIndexedPrimitive(This->WineD3DDevice, PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_DrawPrimitiveUP(LPDIRECT3DDEVICE9 iface, D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_DrawPrimitiveUP(This->WineD3DDevice, PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_DrawIndexedPrimitiveUP(LPDIRECT3DDEVICE9 iface, D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex,
                                                             UINT NumVertexIndices, UINT PrimitiveCount, CONST void* pIndexData,
                                                             D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_DrawIndexedPrimitiveUP(This->WineD3DDevice, PrimitiveType, MinVertexIndex, NumVertexIndices, PrimitiveCount,
                                                 pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_ProcessVertices(LPDIRECT3DDEVICE9 iface, UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_ProcessVertices(This->WineD3DDevice,SrcStartIndex, DestIndex, VertexCount, ((IDirect3DVertexBuffer9Impl *)pDestBuffer)->wineD3DVertexBuffer, ((IDirect3DVertexBuffer9Impl *)pVertexDecl)->wineD3DVertexBuffer, Flags);       
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetFVF(LPDIRECT3DDEVICE9 iface, DWORD FVF) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetFVF(This->WineD3DDevice, FVF);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetFVF(LPDIRECT3DDEVICE9 iface, DWORD* pFVF) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetFVF(This->WineD3DDevice, pFVF);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetStreamSource(LPDIRECT3DDEVICE9 iface, UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_SetStreamSource(This->WineD3DDevice, StreamNumber, 
                                          pStreamData==NULL ? NULL:((IDirect3DVertexBuffer9Impl *)pStreamData)->wineD3DVertexBuffer, 
                                          OffsetInBytes, Stride);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetStreamSource(LPDIRECT3DDEVICE9 iface, UINT StreamNumber, IDirect3DVertexBuffer9 **pStream, UINT* OffsetInBytes, UINT* pStride) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IWineD3DVertexBuffer *retStream = NULL;
    HRESULT rc = D3D_OK;

    TRACE("(%p) Relay\n" , This);

    if(pStream == NULL){
        return D3DERR_INVALIDCALL;
    }

    rc = IWineD3DDevice_GetStreamSource(This->WineD3DDevice, StreamNumber, (IWineD3DVertexBuffer **)&retStream, OffsetInBytes, pStride);
    if (rc == D3D_OK  && NULL != retStream) {
        IWineD3DVertexBuffer_GetParent(retStream, (IUnknown **)pStream);
        IWineD3DVertexBuffer_Release(retStream);
    }else{
        FIXME("Call to GetStreamSource failed %p %p\n", OffsetInBytes, pStride);
        *pStream = NULL;
    }
    return rc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetStreamSourceFreq(LPDIRECT3DDEVICE9 iface, UINT StreamNumber, UINT Divider) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n" , This);
    IWineD3DDevice_SetStreamSourceFreq(This->WineD3DDevice, StreamNumber, Divider);
    return D3D_OK;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetStreamSourceFreq(LPDIRECT3DDEVICE9 iface, UINT StreamNumber, UINT* Divider) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n" , This);
    return IWineD3DDevice_GetStreamSourceFreq(This->WineD3DDevice, StreamNumber, Divider);
    return D3D_OK;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_SetIndices(LPDIRECT3DDEVICE9 iface, IDirect3DIndexBuffer9* pIndexData) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_SetIndices(This->WineD3DDevice, 
                                     pIndexData==NULL ? NULL:((IDirect3DIndexBuffer9Impl *)pIndexData)->wineD3DIndexBuffer, 
                                     0);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_GetIndices(LPDIRECT3DDEVICE9 iface, IDirect3DIndexBuffer9 **ppIndexData) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IWineD3DIndexBuffer *retIndexData = NULL;
    HRESULT rc = D3D_OK;
    UINT tmp;

    TRACE("(%p) Relay\n", This);

    if(ppIndexData == NULL){
        return D3DERR_INVALIDCALL;
    }

    rc = IWineD3DDevice_GetIndices(This->WineD3DDevice, &retIndexData, &tmp);
    if (rc == D3D_OK && NULL != retIndexData) {
        IWineD3DVertexBuffer_GetParent(retIndexData, (IUnknown **)ppIndexData);
        IWineD3DVertexBuffer_Release(retIndexData);
    }else{
        if(rc != D3D_OK)  FIXME("Call to GetIndices failed\n");
        *ppIndexData = NULL;
    }
    return rc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_DrawRectPatch(LPDIRECT3DDEVICE9 iface, UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_DrawRectPatch(This->WineD3DDevice, Handle, pNumSegs, pRectPatchInfo);
}
/*http://msdn.microsoft.com/library/default.asp?url=/library/en-us/directx9_c/directx/graphics/reference/d3d/interfaces/idirect3ddevice9/DrawTriPatch.asp*/
HRESULT  WINAPI  IDirect3DDevice9Impl_DrawTriPatch(LPDIRECT3DDEVICE9 iface, UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_DrawTriPatch(This->WineD3DDevice, Handle, pNumSegs, pTriPatchInfo);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_DeletePatch(LPDIRECT3DDEVICE9 iface, UINT Handle) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DDevice_DeletePatch(This->WineD3DDevice, Handle);
}

const IDirect3DDevice9Vtbl Direct3DDevice9_Vtbl =
{
    /* IUnknown */
    IDirect3DDevice9Impl_QueryInterface,
    IDirect3DDevice9Impl_AddRef,
    IDirect3DDevice9Impl_Release,
    /* IDirect3DDevice9 */
    IDirect3DDevice9Impl_TestCooperativeLevel,
    IDirect3DDevice9Impl_GetAvailableTextureMem,
    IDirect3DDevice9Impl_EvictManagedResources,
    IDirect3DDevice9Impl_GetDirect3D,
    IDirect3DDevice9Impl_GetDeviceCaps,
    IDirect3DDevice9Impl_GetDisplayMode,
    IDirect3DDevice9Impl_GetCreationParameters,
    IDirect3DDevice9Impl_SetCursorProperties,
    IDirect3DDevice9Impl_SetCursorPosition,
    IDirect3DDevice9Impl_ShowCursor,
    IDirect3DDevice9Impl_CreateAdditionalSwapChain,
    IDirect3DDevice9Impl_GetSwapChain,
    IDirect3DDevice9Impl_GetNumberOfSwapChains,
    IDirect3DDevice9Impl_Reset,
    IDirect3DDevice9Impl_Present,
    IDirect3DDevice9Impl_GetBackBuffer,
    IDirect3DDevice9Impl_GetRasterStatus,
    IDirect3DDevice9Impl_SetDialogBoxMode,
    IDirect3DDevice9Impl_SetGammaRamp,
    IDirect3DDevice9Impl_GetGammaRamp,
    IDirect3DDevice9Impl_CreateTexture,
    IDirect3DDevice9Impl_CreateVolumeTexture,
    IDirect3DDevice9Impl_CreateCubeTexture,
    IDirect3DDevice9Impl_CreateVertexBuffer,
    IDirect3DDevice9Impl_CreateIndexBuffer,
    IDirect3DDevice9Impl_CreateRenderTarget,
    IDirect3DDevice9Impl_CreateDepthStencilSurface,
    IDirect3DDevice9Impl_UpdateSurface,
    IDirect3DDevice9Impl_UpdateTexture,
    IDirect3DDevice9Impl_GetRenderTargetData,
    IDirect3DDevice9Impl_GetFrontBufferData,
    IDirect3DDevice9Impl_StretchRect,
    IDirect3DDevice9Impl_ColorFill,
    IDirect3DDevice9Impl_CreateOffscreenPlainSurface,
    IDirect3DDevice9Impl_SetRenderTarget,
    IDirect3DDevice9Impl_GetRenderTarget,
    IDirect3DDevice9Impl_SetDepthStencilSurface,
    IDirect3DDevice9Impl_GetDepthStencilSurface,
    IDirect3DDevice9Impl_BeginScene,
    IDirect3DDevice9Impl_EndScene,
    IDirect3DDevice9Impl_Clear,
    IDirect3DDevice9Impl_SetTransform,
    IDirect3DDevice9Impl_GetTransform,
    IDirect3DDevice9Impl_MultiplyTransform,
    IDirect3DDevice9Impl_SetViewport,
    IDirect3DDevice9Impl_GetViewport,
    IDirect3DDevice9Impl_SetMaterial,
    IDirect3DDevice9Impl_GetMaterial,
    IDirect3DDevice9Impl_SetLight,
    IDirect3DDevice9Impl_GetLight,
    IDirect3DDevice9Impl_LightEnable,
    IDirect3DDevice9Impl_GetLightEnable,
    IDirect3DDevice9Impl_SetClipPlane,
    IDirect3DDevice9Impl_GetClipPlane,
    IDirect3DDevice9Impl_SetRenderState,
    IDirect3DDevice9Impl_GetRenderState,
    IDirect3DDevice9Impl_CreateStateBlock,
    IDirect3DDevice9Impl_BeginStateBlock,
    IDirect3DDevice9Impl_EndStateBlock,
    IDirect3DDevice9Impl_SetClipStatus,
    IDirect3DDevice9Impl_GetClipStatus,
    IDirect3DDevice9Impl_GetTexture,
    IDirect3DDevice9Impl_SetTexture,
    IDirect3DDevice9Impl_GetTextureStageState,
    IDirect3DDevice9Impl_SetTextureStageState,
    IDirect3DDevice9Impl_GetSamplerState,
    IDirect3DDevice9Impl_SetSamplerState,
    IDirect3DDevice9Impl_ValidateDevice,
    IDirect3DDevice9Impl_SetPaletteEntries,
    IDirect3DDevice9Impl_GetPaletteEntries,
    IDirect3DDevice9Impl_SetCurrentTexturePalette,
    IDirect3DDevice9Impl_GetCurrentTexturePalette,
    IDirect3DDevice9Impl_SetScissorRect,
    IDirect3DDevice9Impl_GetScissorRect,
    IDirect3DDevice9Impl_SetSoftwareVertexProcessing,
    IDirect3DDevice9Impl_GetSoftwareVertexProcessing,
    IDirect3DDevice9Impl_SetNPatchMode,
    IDirect3DDevice9Impl_GetNPatchMode,
    IDirect3DDevice9Impl_DrawPrimitive,
    IDirect3DDevice9Impl_DrawIndexedPrimitive,
    IDirect3DDevice9Impl_DrawPrimitiveUP,
    IDirect3DDevice9Impl_DrawIndexedPrimitiveUP,
    IDirect3DDevice9Impl_ProcessVertices,
    IDirect3DDevice9Impl_CreateVertexDeclaration,
    IDirect3DDevice9Impl_SetVertexDeclaration,
    IDirect3DDevice9Impl_GetVertexDeclaration,
    IDirect3DDevice9Impl_SetFVF,
    IDirect3DDevice9Impl_GetFVF,
    IDirect3DDevice9Impl_CreateVertexShader,
    IDirect3DDevice9Impl_SetVertexShader,
    IDirect3DDevice9Impl_GetVertexShader,
    IDirect3DDevice9Impl_SetVertexShaderConstantF,
    IDirect3DDevice9Impl_GetVertexShaderConstantF,
    IDirect3DDevice9Impl_SetVertexShaderConstantI,
    IDirect3DDevice9Impl_GetVertexShaderConstantI,
    IDirect3DDevice9Impl_SetVertexShaderConstantB,
    IDirect3DDevice9Impl_GetVertexShaderConstantB,
    IDirect3DDevice9Impl_SetStreamSource,
    IDirect3DDevice9Impl_GetStreamSource,
    IDirect3DDevice9Impl_SetStreamSourceFreq,
    IDirect3DDevice9Impl_GetStreamSourceFreq,
    IDirect3DDevice9Impl_SetIndices,
    IDirect3DDevice9Impl_GetIndices,
    IDirect3DDevice9Impl_CreatePixelShader,
    IDirect3DDevice9Impl_SetPixelShader,
    IDirect3DDevice9Impl_GetPixelShader,
    IDirect3DDevice9Impl_SetPixelShaderConstantF,
    IDirect3DDevice9Impl_GetPixelShaderConstantF,
    IDirect3DDevice9Impl_SetPixelShaderConstantI,
    IDirect3DDevice9Impl_GetPixelShaderConstantI,
    IDirect3DDevice9Impl_SetPixelShaderConstantB,
    IDirect3DDevice9Impl_GetPixelShaderConstantB,
    IDirect3DDevice9Impl_DrawRectPatch,
    IDirect3DDevice9Impl_DrawTriPatch,
    IDirect3DDevice9Impl_DeletePatch,
    IDirect3DDevice9Impl_CreateQuery
};


/* Internal function called back during the CreateDevice to create a render target  */
HRESULT WINAPI D3D9CB_CreateSurface(IUnknown *device, UINT Width, UINT Height, 
                                         WINED3DFORMAT Format, DWORD Usage, D3DPOOL Pool, UINT Level,
                                         IWineD3DSurface** ppSurface, HANDLE* pSharedHandle) {
    
    HRESULT res = D3D_OK;
    IDirect3DSurface9Impl *d3dSurface = NULL;
    BOOL Lockable = TRUE;
    
    if((Pool == D3DPOOL_DEFAULT &&  Usage != D3DUSAGE_DYNAMIC)) 
        Lockable = FALSE;
        
    TRACE("relay\n");
    res = IDirect3DDevice9Impl_CreateSurface((IDirect3DDevice9 *)device, Width, Height, (D3DFORMAT)Format, Lockable, FALSE/*Discard*/, Level,  (IDirect3DSurface9 **)&d3dSurface, D3DRTYPE_SURFACE, Usage, Pool, D3DMULTISAMPLE_NONE, 0 /* MultisampleQuality */, pSharedHandle);  

    if (res == D3D_OK) {
        *ppSurface = d3dSurface->wineD3DSurface;
    }else{
        FIXME("(%p) IDirect3DDevice9_CreateSurface failed\n", device);
    }
    return res;
}
