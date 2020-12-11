/*
 * IDirect3DIndexBuffer9 implementation
 *
 * Copyright 2002-2004 Jason Edmeades
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

/* IDirect3DIndexBuffer9 IUnknown parts follow: */
HRESULT WINAPI IDirect3DIndexBuffer9Impl_QueryInterface(LPDIRECT3DINDEXBUFFER9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DResource9)
        || IsEqualGUID(riid, &IID_IDirect3DIndexBuffer9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DIndexBuffer9Impl_AddRef(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DIndexBuffer9Impl_Release(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DIndexBuffer_Release(This->wineD3DIndexBuffer);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DIndexBuffer9 IDirect3DResource9 Interface follow: */
HRESULT WINAPI IDirect3DIndexBuffer9Impl_GetDevice(LPDIRECT3DINDEXBUFFER9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IDirect3DResource9Impl_GetDevice((LPDIRECT3DRESOURCE9) This, ppDevice);
}

HRESULT WINAPI IDirect3DIndexBuffer9Impl_SetPrivateData(LPDIRECT3DINDEXBUFFER9 iface, REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_SetPrivateData(This->wineD3DIndexBuffer, refguid, pData, SizeOfData, Flags);
}

HRESULT WINAPI IDirect3DIndexBuffer9Impl_GetPrivateData(LPDIRECT3DINDEXBUFFER9 iface, REFGUID refguid, void* pData, DWORD* pSizeOfData) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_GetPrivateData(This->wineD3DIndexBuffer, refguid, pData, pSizeOfData);
}

HRESULT WINAPI IDirect3DIndexBuffer9Impl_FreePrivateData(LPDIRECT3DINDEXBUFFER9 iface, REFGUID refguid) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_FreePrivateData(This->wineD3DIndexBuffer, refguid);
}

DWORD WINAPI IDirect3DIndexBuffer9Impl_SetPriority(LPDIRECT3DINDEXBUFFER9 iface, DWORD PriorityNew) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_SetPriority(This->wineD3DIndexBuffer, PriorityNew);
}

DWORD WINAPI IDirect3DIndexBuffer9Impl_GetPriority(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_GetPriority(This->wineD3DIndexBuffer);
}

void WINAPI IDirect3DIndexBuffer9Impl_PreLoad(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_PreLoad(This->wineD3DIndexBuffer);
}

D3DRESOURCETYPE WINAPI IDirect3DIndexBuffer9Impl_GetType(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_GetType(This->wineD3DIndexBuffer);
}

/* IDirect3DIndexBuffer9 Interface follow: */
HRESULT WINAPI IDirect3DIndexBuffer9Impl_Lock(LPDIRECT3DINDEXBUFFER9 iface, UINT OffsetToLock, UINT SizeToLock, void** ppbData, DWORD Flags) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_Lock(This->wineD3DIndexBuffer, OffsetToLock, SizeToLock, (BYTE **)ppbData, Flags);
}

HRESULT WINAPI IDirect3DIndexBuffer9Impl_Unlock(LPDIRECT3DINDEXBUFFER9 iface) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_Unlock(This->wineD3DIndexBuffer);
}

HRESULT  WINAPI        IDirect3DIndexBuffer9Impl_GetDesc(LPDIRECT3DINDEXBUFFER9 iface, D3DINDEXBUFFER_DESC *pDesc) {
    IDirect3DIndexBuffer9Impl *This = (IDirect3DIndexBuffer9Impl *)iface;
    TRACE("(%p) Relay\n", This);
    return IWineD3DIndexBuffer_GetDesc(This->wineD3DIndexBuffer, pDesc);
}


const IDirect3DIndexBuffer9Vtbl Direct3DIndexBuffer9_Vtbl =
{
    /* IUnknown */
    IDirect3DIndexBuffer9Impl_QueryInterface,
    IDirect3DIndexBuffer9Impl_AddRef,
    IDirect3DIndexBuffer9Impl_Release,
    /* IDirect3DResource9 */
    IDirect3DIndexBuffer9Impl_GetDevice,
    IDirect3DIndexBuffer9Impl_SetPrivateData,
    IDirect3DIndexBuffer9Impl_GetPrivateData,
    IDirect3DIndexBuffer9Impl_FreePrivateData,
    IDirect3DIndexBuffer9Impl_SetPriority,
    IDirect3DIndexBuffer9Impl_GetPriority,
    IDirect3DIndexBuffer9Impl_PreLoad,
    IDirect3DIndexBuffer9Impl_GetType,
    /* IDirect3DIndexBuffer9 */
    IDirect3DIndexBuffer9Impl_Lock,
    IDirect3DIndexBuffer9Impl_Unlock,
    IDirect3DIndexBuffer9Impl_GetDesc
};


/* IDirect3DDevice9 IDirect3DIndexBuffer9 Methods follow: */
HRESULT WINAPI IDirect3DDevice9Impl_CreateIndexBuffer(LPDIRECT3DDEVICE9 iface, 
                              UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                              IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) {
    
    IDirect3DIndexBuffer9Impl *object;
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;
    
    TRACE("(%p) Relay\n", This);
    /* Allocate the storage for the device */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *ppIndexBuffer = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->lpVtbl = &Direct3DIndexBuffer9_Vtbl;
    object->ref = 1;
    TRACE("Calling wined3d create index buffer\n");
    hrc = IWineD3DDevice_CreateIndexBuffer(This->WineD3DDevice, Length, Usage, Format, Pool, &object->wineD3DIndexBuffer, pSharedHandle, (IUnknown *)object);
    if (hrc != D3D_OK) {
        /* free up object */ 
        FIXME("(%p) call to IWineD3DDevice_CreateIndexBuffer failed\n", This);
        HeapFree(GetProcessHeap(), 0, object);
        *ppIndexBuffer = NULL;
    } else {
        *ppIndexBuffer = (LPDIRECT3DINDEXBUFFER9) object;
    }
    return hrc;
}
