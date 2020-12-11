/*
 * IDirect3DVertexShader9 implementation
 *
 * Copyright 2002-2003 Jason Edmeades
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

/* IDirect3DVertexShader9 IUnknown parts follow: */
HRESULT WINAPI IDirect3DVertexShader9Impl_QueryInterface(LPDIRECT3DVERTEXSHADER9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DVertexShader9Impl *This = (IDirect3DVertexShader9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DVertexShader9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DVertexShader9Impl_AddRef(LPDIRECT3DVERTEXSHADER9 iface) {
    IDirect3DVertexShader9Impl *This = (IDirect3DVertexShader9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DVertexShader9Impl_Release(LPDIRECT3DVERTEXSHADER9 iface) {
    IDirect3DVertexShader9Impl *This = (IDirect3DVertexShader9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DVertexShader_Release(This->wineD3DVertexShader);
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DVertexShader9 Interface follow: */
HRESULT WINAPI IDirect3DVertexShader9Impl_GetDevice(LPDIRECT3DVERTEXSHADER9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DVertexShader9Impl *This = (IDirect3DVertexShader9Impl *)iface;
    IWineD3DDevice *myDevice = NULL;
    HRESULT hr = D3D_OK;
    TRACE("(%p) : Relay\n", This);

    if (D3D_OK == (hr = IWineD3DVertexShader_GetDevice(This->wineD3DVertexShader, &myDevice) && myDevice != NULL)) {
        hr = IWineD3DDevice_GetParent(myDevice, (IUnknown **)ppDevice);
        IWineD3DDevice_Release(myDevice);
    } else {
        *ppDevice = NULL;
    }
    TRACE("(%p) returing (%p)", This, *ppDevice);
    return hr;
}

HRESULT WINAPI IDirect3DVertexShader9Impl_GetFunction(LPDIRECT3DVERTEXSHADER9 iface, VOID* pData, UINT* pSizeOfData) {
    IDirect3DVertexShader9Impl *This = (IDirect3DVertexShader9Impl *)iface;

    TRACE("(%p) : Relay\n", This);
    return IWineD3DVertexShader_GetFunction(This->wineD3DVertexShader, pData, pSizeOfData);
}


const IDirect3DVertexShader9Vtbl Direct3DVertexShader9_Vtbl =
{
    /* IUnknown */
    IDirect3DVertexShader9Impl_QueryInterface,
    IDirect3DVertexShader9Impl_AddRef,
    IDirect3DVertexShader9Impl_Release,
    /* IDirect3DVertexShader9 */
    IDirect3DVertexShader9Impl_GetDevice,
    IDirect3DVertexShader9Impl_GetFunction
};


/* IDirect3DDevice9 IDirect3DVertexShader9 Methods follow: */
HRESULT WINAPI IDirect3DDevice9Impl_CreateVertexShader(LPDIRECT3DDEVICE9 iface, CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;
    IDirect3DVertexShader9Impl *object;

    /* Setup a stub object for now */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object));
    TRACE("(%p) : pFunction(%p), ppShader(%p)\n", This, pFunction, ppShader);
    if (NULL == object) {
        FIXME("Allocation of memory failed\n");
        *ppShader = NULL;
        return D3DERR_OUTOFVIDEOMEMORY;
    }

    object->ref = 1;
    object->lpVtbl = &Direct3DVertexShader9_Vtbl;
    hrc= IWineD3DDevice_CreateVertexShader(This->WineD3DDevice, pFunction, &object->wineD3DVertexShader, (IUnknown *)object);

    if (FAILED(hrc)) {
        /* free up object */
        FIXME("Call to IWineD3DDevice_CreateVertexShader failed\n");
        HeapFree(GetProcessHeap(), 0, object);
        *ppShader = NULL;
    }else{
        *ppShader = (IDirect3DVertexShader9 *)object;
    }

    TRACE("(%p) : returning %p\n", This, *ppShader);
    return hrc;
}

HRESULT WINAPI IDirect3DDevice9Impl_SetVertexShader(LPDIRECT3DDEVICE9 iface, IDirect3DVertexShader9* pShader) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    HRESULT hrc = D3D_OK;

    TRACE("(%p) : Relay  \n", This);
    hrc =  IWineD3DDevice_SetVertexShader(This->WineD3DDevice, pShader==NULL?NULL:((IDirect3DVertexShader9Impl *)pShader)->wineD3DVertexShader);

    TRACE("(%p) : returning hr(%lu) \n", This, hrc);
    return hrc;
}

HRESULT WINAPI IDirect3DDevice9Impl_GetVertexShader(LPDIRECT3DDEVICE9 iface, IDirect3DVertexShader9** ppShader) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    IWineD3DVertexShader *pShader;
    HRESULT hrc = D3D_OK;

    TRACE("(%p) : Relay  device@%p \n", This, This->WineD3DDevice);
    hrc = IWineD3DDevice_GetVertexShader(This->WineD3DDevice, &pShader);
    if(hrc == D3D_OK){
       hrc = IWineD3DVertexShader_GetParent(pShader, (IUnknown **)ppShader);
       IWineD3DVertexShader_Release(pShader);
    } else {
        WARN("(%p) : Call to IWineD3DDevice_GetVertexShader failed %lu (device %p)\n", This, hrc, This->WineD3DDevice);
    }
    TRACE("(%p) : returning %p\n", This, *ppShader);
    return hrc;
}

HRESULT WINAPI IDirect3DDevice9Impl_SetVertexShaderConstantF(LPDIRECT3DDEVICE9 iface, UINT Register, CONST float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_SetVertexShaderConstantF(This->WineD3DDevice, Register, pConstantData, Vector4fCount);
}

HRESULT WINAPI IDirect3DDevice9Impl_GetVertexShaderConstantF(LPDIRECT3DDEVICE9 iface, UINT Register, float* pConstantData, UINT Vector4fCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_GetVertexShaderConstantF(This->WineD3DDevice, Register, pConstantData, Vector4fCount);
}

HRESULT WINAPI IDirect3DDevice9Impl_SetVertexShaderConstantI(LPDIRECT3DDEVICE9 iface, UINT Register, CONST int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_SetVertexShaderConstantI(This->WineD3DDevice, Register, pConstantData, Vector4iCount);
}

HRESULT WINAPI IDirect3DDevice9Impl_GetVertexShaderConstantI(LPDIRECT3DDEVICE9 iface, UINT Register, int* pConstantData, UINT Vector4iCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_GetVertexShaderConstantI(This->WineD3DDevice, Register, pConstantData, Vector4iCount);
}

HRESULT WINAPI IDirect3DDevice9Impl_SetVertexShaderConstantB(LPDIRECT3DDEVICE9 iface, UINT Register, CONST BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_SetVertexShaderConstantB(This->WineD3DDevice, Register, pConstantData, BoolCount);
}

HRESULT WINAPI IDirect3DDevice9Impl_GetVertexShaderConstantB(LPDIRECT3DDEVICE9 iface, UINT Register, BOOL* pConstantData, UINT BoolCount) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
    TRACE("(%p) : Relay\n", This);
    return IWineD3DDevice_GetVertexShaderConstantB(This->WineD3DDevice, Register, pConstantData, BoolCount);
}
