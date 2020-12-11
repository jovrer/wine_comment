/*
 * IDirect3DVolumeTexture9 implementation
 *
 * Copyright 2002-2005 Jason Edmeades
 *                     Raphael Junqueira
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

/* IDirect3DVolumeTexture9 IUnknown parts follow: */
HRESULT WINAPI IDirect3DVolumeTexture9Impl_QueryInterface(LPDIRECT3DVOLUMETEXTURE9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
    || IsEqualGUID(riid, &IID_IDirect3DResource9)
    || IsEqualGUID(riid, &IID_IDirect3DBaseTexture9)
    || IsEqualGUID(riid, &IID_IDirect3DVolumeTexture9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DVolumeTexture9Impl_AddRef(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DVolumeTexture9Impl_Release(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DVolumeTexture_Release(This->wineD3DVolumeTexture);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DVolumeTexture9 IDirect3DResource9 Interface follow: */
HRESULT WINAPI IDirect3DVolumeTexture9Impl_GetDevice(LPDIRECT3DVOLUMETEXTURE9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IDirect3DResource9Impl_GetDevice((LPDIRECT3DRESOURCE9) This, ppDevice);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_SetPrivateData(LPDIRECT3DVOLUMETEXTURE9 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_SetPrivateData(This->wineD3DVolumeTexture, refguid, pData, SizeOfData, Flags);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_GetPrivateData(LPDIRECT3DVOLUMETEXTURE9 iface, REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetPrivateData(This->wineD3DVolumeTexture, refguid, pData, pSizeOfData);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_FreePrivateData(LPDIRECT3DVOLUMETEXTURE9 iface, REFGUID refguid) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_FreePrivateData(This->wineD3DVolumeTexture, refguid);
}

DWORD WINAPI IDirect3DVolumeTexture9Impl_SetPriority(LPDIRECT3DVOLUMETEXTURE9 iface, DWORD PriorityNew) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_SetPriority(This->wineD3DVolumeTexture, PriorityNew);
}

DWORD WINAPI IDirect3DVolumeTexture9Impl_GetPriority(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetPriority(This->wineD3DVolumeTexture);
}

void WINAPI IDirect3DVolumeTexture9Impl_PreLoad(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_PreLoad(This->wineD3DVolumeTexture);
}

D3DRESOURCETYPE WINAPI IDirect3DVolumeTexture9Impl_GetType(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetType(This->wineD3DVolumeTexture);
}

/* IDirect3DVolumeTexture9 IDirect3DBaseTexture9 Interface follow: */
DWORD WINAPI IDirect3DVolumeTexture9Impl_SetLOD(LPDIRECT3DVOLUMETEXTURE9 iface, DWORD LODNew) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_SetLOD(This->wineD3DVolumeTexture, LODNew);
}

DWORD WINAPI IDirect3DVolumeTexture9Impl_GetLOD(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetLOD(This->wineD3DVolumeTexture);
}

DWORD WINAPI IDirect3DVolumeTexture9Impl_GetLevelCount(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetLevelCount(This->wineD3DVolumeTexture);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_SetAutoGenFilterType(LPDIRECT3DVOLUMETEXTURE9 iface, D3DTEXTUREFILTERTYPE FilterType) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_SetAutoGenFilterType(This->wineD3DVolumeTexture, FilterType);
}

D3DTEXTUREFILTERTYPE WINAPI IDirect3DVolumeTexture9Impl_GetAutoGenFilterType(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GetAutoGenFilterType(This->wineD3DVolumeTexture);
}

void WINAPI IDirect3DVolumeTexture9Impl_GenerateMipSubLevels(LPDIRECT3DVOLUMETEXTURE9 iface) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_GenerateMipSubLevels(This->wineD3DVolumeTexture);
}

/* IDirect3DVolumeTexture9 Interface follow: */
HRESULT WINAPI IDirect3DVolumeTexture9Impl_GetLevelDesc(LPDIRECT3DVOLUMETEXTURE9 iface, UINT Level, D3DVOLUME_DESC* pDesc) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    WINED3DVOLUME_DESC     wined3ddesc;
    UINT                   tmpInt = -1;

    TRACE("(%p) Relay\n", This);

    /* As d3d8 and d3d9 structures differ, pass in ptrs to where data needs to go */
    wined3ddesc.Format              = (WINED3DFORMAT *)&pDesc->Format;
    wined3ddesc.Type                = &pDesc->Type;
    wined3ddesc.Usage               = &pDesc->Usage;
    wined3ddesc.Pool                = &pDesc->Pool;
    wined3ddesc.Size                = &tmpInt;
    wined3ddesc.Width               = &pDesc->Width;
    wined3ddesc.Height              = &pDesc->Height;
    wined3ddesc.Depth               = &pDesc->Depth;

    return IWineD3DVolumeTexture_GetLevelDesc(This->wineD3DVolumeTexture, Level, &wined3ddesc);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_GetVolumeLevel(LPDIRECT3DVOLUMETEXTURE9 iface, UINT Level, IDirect3DVolume9** ppVolumeLevel) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IWineD3DVolume *myVolume = NULL;

    TRACE("(%p) Relay\n", This);

    hrc = IWineD3DVolumeTexture_GetVolumeLevel(This->wineD3DVolumeTexture, Level, &myVolume);
    if (hrc == D3D_OK && NULL != ppVolumeLevel) {
       IWineD3DVolumeTexture_GetParent(myVolume, (IUnknown **)ppVolumeLevel);
       IWineD3DVolumeTexture_Release(myVolume);
    }
    return hrc;
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_LockBox(LPDIRECT3DVOLUMETEXTURE9 iface, UINT Level, D3DLOCKED_BOX* pLockedVolume, CONST D3DBOX* pBox, DWORD Flags) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay %p %p %p %ld\n", This, This->wineD3DVolumeTexture, pLockedVolume, pBox,Flags);
    return IWineD3DVolumeTexture_LockBox(This->wineD3DVolumeTexture, Level, pLockedVolume, pBox, Flags);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_UnlockBox(LPDIRECT3DVOLUMETEXTURE9 iface, UINT Level) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay %p %d\n", This, This->wineD3DVolumeTexture, Level);
    return IWineD3DVolumeTexture_UnlockBox(This->wineD3DVolumeTexture, Level);
}

HRESULT WINAPI IDirect3DVolumeTexture9Impl_AddDirtyBox(LPDIRECT3DVOLUMETEXTURE9 iface, CONST D3DBOX* pDirtyBox) {
    IDirect3DVolumeTexture9Impl *This = (IDirect3DVolumeTexture9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DVolumeTexture_AddDirtyBox(This->wineD3DVolumeTexture, pDirtyBox);
}


const IDirect3DVolumeTexture9Vtbl Direct3DVolumeTexture9_Vtbl =
{
    /* IUnknown */
    IDirect3DVolumeTexture9Impl_QueryInterface,
    IDirect3DVolumeTexture9Impl_AddRef,
    IDirect3DVolumeTexture9Impl_Release,
    /* IDirect3DResource9 */
    IDirect3DVolumeTexture9Impl_GetDevice,
    IDirect3DVolumeTexture9Impl_SetPrivateData,
    IDirect3DVolumeTexture9Impl_GetPrivateData,
    IDirect3DVolumeTexture9Impl_FreePrivateData,
    IDirect3DVolumeTexture9Impl_SetPriority,
    IDirect3DVolumeTexture9Impl_GetPriority,
    IDirect3DVolumeTexture9Impl_PreLoad,
    IDirect3DVolumeTexture9Impl_GetType,
    /* IDirect3DBaseTexture9 */
    IDirect3DVolumeTexture9Impl_SetLOD,
    IDirect3DVolumeTexture9Impl_GetLOD,
    IDirect3DVolumeTexture9Impl_GetLevelCount,
    IDirect3DVolumeTexture9Impl_SetAutoGenFilterType,
    IDirect3DVolumeTexture9Impl_GetAutoGenFilterType,
    IDirect3DVolumeTexture9Impl_GenerateMipSubLevels,
    /* IDirect3DVolumeTexture9 */
    IDirect3DVolumeTexture9Impl_GetLevelDesc,
    IDirect3DVolumeTexture9Impl_GetVolumeLevel,
    IDirect3DVolumeTexture9Impl_LockBox,
    IDirect3DVolumeTexture9Impl_UnlockBox,
    IDirect3DVolumeTexture9Impl_AddDirtyBox
};


/* IDirect3DDevice9 IDirect3DVolumeTexture9 Methods follow: */
HRESULT  WINAPI  IDirect3DDevice9Impl_CreateVolumeTexture(LPDIRECT3DDEVICE9 iface, 
                                                          UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, 
                                                          D3DFORMAT Format, D3DPOOL Pool, 
                                                          IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) {

    IDirect3DVolumeTexture9Impl *object;
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;

    TRACE("(%p) Relay\n", This);

    /* Allocate the storage for the device */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DVolumeTexture9Impl));
    if (NULL == object) {
        FIXME("(%p) allocation of memory failed\n", This);
        *ppVolumeTexture = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->lpVtbl = &Direct3DVolumeTexture9_Vtbl;
    object->ref = 1;
    hrc = IWineD3DDevice_CreateVolumeTexture(This->WineD3DDevice, Width, Height, Depth, Levels, Usage,
                                 (WINED3DFORMAT)Format, Pool, &object->wineD3DVolumeTexture, pSharedHandle,
                                 (IUnknown *)object, D3D9CB_CreateVolume);


    if (hrc != D3D_OK) {
        /* free up object */ 
        FIXME("(%p) call to IWineD3DDevice_CreateVolumeTexture failed\n", This);
        HeapFree(GetProcessHeap(), 0, object);
        *ppVolumeTexture = NULL;
    } else {
        *ppVolumeTexture = (LPDIRECT3DVOLUMETEXTURE9) object;
    }
    TRACE("(%p)  returning %p\n", This , *ppVolumeTexture);
    return hrc;
}
