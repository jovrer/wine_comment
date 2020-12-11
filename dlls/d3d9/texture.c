/*
 * IDirect3DTexture9 implementation
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

/* IDirect3DTexture9 IUnknown parts follow: */
HRESULT WINAPI IDirect3DTexture9Impl_QueryInterface(LPDIRECT3DTEXTURE9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DResource9)
        || IsEqualGUID(riid, &IID_IDirect3DBaseTexture9)
        || IsEqualGUID(riid, &IID_IDirect3DTexture9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p) not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DTexture9Impl_AddRef(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DTexture9Impl_Release(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DTexture_Release(This->wineD3DTexture);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DTexture9 IDirect3DResource9 Interface follow: */
HRESULT WINAPI IDirect3DTexture9Impl_GetDevice(LPDIRECT3DTEXTURE9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IDirect3DResource9Impl_GetDevice((LPDIRECT3DRESOURCE9) This, ppDevice);
}

HRESULT WINAPI IDirect3DTexture9Impl_SetPrivateData(LPDIRECT3DTEXTURE9 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_SetPrivateData(This->wineD3DTexture, refguid, pData, SizeOfData, Flags);
}

HRESULT WINAPI IDirect3DTexture9Impl_GetPrivateData(LPDIRECT3DTEXTURE9 iface, REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetPrivateData(This->wineD3DTexture, refguid, pData, pSizeOfData);
}

HRESULT WINAPI IDirect3DTexture9Impl_FreePrivateData(LPDIRECT3DTEXTURE9 iface, REFGUID refguid) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_FreePrivateData(This->wineD3DTexture, refguid);
}

DWORD WINAPI IDirect3DTexture9Impl_SetPriority(LPDIRECT3DTEXTURE9 iface, DWORD PriorityNew) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_SetPriority(This->wineD3DTexture, PriorityNew);
}

DWORD WINAPI IDirect3DTexture9Impl_GetPriority(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetPriority(This->wineD3DTexture);
}

void WINAPI IDirect3DTexture9Impl_PreLoad(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_PreLoad(This->wineD3DTexture);
}

D3DRESOURCETYPE WINAPI IDirect3DTexture9Impl_GetType(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetType(This->wineD3DTexture);
}

/* IDirect3DTexture9 IDirect3DBaseTexture9 Interface follow: */
DWORD WINAPI IDirect3DTexture9Impl_SetLOD(LPDIRECT3DTEXTURE9 iface, DWORD LODNew) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_SetLOD(This->wineD3DTexture, LODNew);
}

DWORD WINAPI IDirect3DTexture9Impl_GetLOD(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetLOD(This->wineD3DTexture);
}

DWORD WINAPI IDirect3DTexture9Impl_GetLevelCount(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetLevelCount(This->wineD3DTexture);
}

HRESULT WINAPI IDirect3DTexture9Impl_SetAutoGenFilterType(LPDIRECT3DTEXTURE9 iface, D3DTEXTUREFILTERTYPE FilterType) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_SetAutoGenFilterType(This->wineD3DTexture, FilterType);
}

D3DTEXTUREFILTERTYPE WINAPI IDirect3DTexture9Impl_GetAutoGenFilterType(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GetAutoGenFilterType(This->wineD3DTexture);
}

void WINAPI IDirect3DTexture9Impl_GenerateMipSubLevels(LPDIRECT3DTEXTURE9 iface) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_GenerateMipSubLevels(This->wineD3DTexture);
}

/* IDirect3DTexture9 Interface follow: */
HRESULT WINAPI IDirect3DTexture9Impl_GetLevelDesc(LPDIRECT3DTEXTURE9 iface, UINT Level, D3DSURFACE_DESC* pDesc) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;

    WINED3DSURFACE_DESC    wined3ddesc;
    UINT                   tmpInt = -1;
    TRACE("(%p) Relay\n", This);

    /* As d3d8 and d3d9 structures differ, pass in ptrs to where data needs to go */
    wined3ddesc.Format              = (WINED3DFORMAT *)&pDesc->Format;
    wined3ddesc.Type                = &pDesc->Type;
    wined3ddesc.Usage               = &pDesc->Usage;
    wined3ddesc.Pool                = &pDesc->Pool;
    wined3ddesc.Size                = &tmpInt; /* required for d3d8 */
    wined3ddesc.MultiSampleType     = &pDesc->MultiSampleType;
    wined3ddesc.MultiSampleQuality  = &pDesc->MultiSampleQuality;
    wined3ddesc.Width               = &pDesc->Width;
    wined3ddesc.Height              = &pDesc->Height;

    return IWineD3DTexture_GetLevelDesc(This->wineD3DTexture, Level, &wined3ddesc);
}

HRESULT WINAPI IDirect3DTexture9Impl_GetSurfaceLevel(LPDIRECT3DTEXTURE9 iface, UINT Level, IDirect3DSurface9** ppSurfaceLevel) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DSurface *mySurface = NULL;

    TRACE("(%p) Relay\n", This);
    hrc = IWineD3DTexture_GetSurfaceLevel(This->wineD3DTexture, Level, &mySurface);
    if (hrc == D3D_OK && NULL != ppSurfaceLevel) {
       IWineD3DSurface_GetParent(mySurface, (IUnknown **)ppSurfaceLevel);
       IWineD3DSurface_Release(mySurface);
    }
    return hrc;
}

HRESULT WINAPI IDirect3DTexture9Impl_LockRect(LPDIRECT3DTEXTURE9 iface, UINT Level, D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_LockRect(This->wineD3DTexture, Level, pLockedRect, pRect, Flags);
}

HRESULT WINAPI IDirect3DTexture9Impl_UnlockRect(LPDIRECT3DTEXTURE9 iface, UINT Level) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_UnlockRect(This->wineD3DTexture, Level);
}

HRESULT WINAPI IDirect3DTexture9Impl_AddDirtyRect(LPDIRECT3DTEXTURE9 iface, CONST RECT* pDirtyRect) {
    IDirect3DTexture9Impl *This = (IDirect3DTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DTexture_AddDirtyRect(This->wineD3DTexture, pDirtyRect);
}

const IDirect3DTexture9Vtbl Direct3DTexture9_Vtbl =
{
    /* IUnknown */
    IDirect3DTexture9Impl_QueryInterface,
    IDirect3DTexture9Impl_AddRef,
    IDirect3DTexture9Impl_Release,
     /* IDirect3DResource9 */
    IDirect3DTexture9Impl_GetDevice,
    IDirect3DTexture9Impl_SetPrivateData,
    IDirect3DTexture9Impl_GetPrivateData,
    IDirect3DTexture9Impl_FreePrivateData,
    IDirect3DTexture9Impl_SetPriority,
    IDirect3DTexture9Impl_GetPriority,
    IDirect3DTexture9Impl_PreLoad,
    IDirect3DTexture9Impl_GetType,
    /* IDirect3dBaseTexture9 */
    IDirect3DTexture9Impl_SetLOD,
    IDirect3DTexture9Impl_GetLOD,
    IDirect3DTexture9Impl_GetLevelCount,
    IDirect3DTexture9Impl_SetAutoGenFilterType,
    IDirect3DTexture9Impl_GetAutoGenFilterType,
    IDirect3DTexture9Impl_GenerateMipSubLevels,
    /* IDirect3DTexture9 */
    IDirect3DTexture9Impl_GetLevelDesc,
    IDirect3DTexture9Impl_GetSurfaceLevel,
    IDirect3DTexture9Impl_LockRect,
    IDirect3DTexture9Impl_UnlockRect,
    IDirect3DTexture9Impl_AddDirtyRect
};


/* IDirect3DDevice9 IDirect3DTexture9 Methods follow: */
HRESULT  WINAPI  IDirect3DDevice9Impl_CreateTexture(LPDIRECT3DDEVICE9 iface, UINT Width, UINT Height, UINT Levels, DWORD Usage,
                                                    D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) {
    IDirect3DTexture9Impl *object;
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;
    
    TRACE("(%p) : W(%d) H(%d), Lvl(%d) d(%ld), Fmt(%u), Pool(%d)\n", This, Width, Height, Levels, Usage, Format,  Pool);

    /* Allocate the storage for the device */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DTexture9Impl));

    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *ppTexture = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->lpVtbl = &Direct3DTexture9_Vtbl;
    object->ref = 1;
    hrc = IWineD3DDevice_CreateTexture(This->WineD3DDevice, Width, Height, Levels, Usage,
                                 (WINED3DFORMAT)Format, Pool, &object->wineD3DTexture, pSharedHandle, (IUnknown *)object, D3D9CB_CreateSurface);

    if (FAILED(hrc)) {
        /* free up object */ 
        FIXME("(%p) call to IWineD3DDevice_CreateTexture failed\n", This);
        HeapFree(GetProcessHeap(), 0, object);
        *ppTexture = NULL;
   } else {
        *ppTexture= (LPDIRECT3DTEXTURE9) object;
   }

   TRACE("(%p) Created Texture %p, %p\n",This,object,object->wineD3DTexture);
   return hrc;
}
