/*
 * IDirect3DStateBlock9 implementation
 *
 * Copyright 2002-2003 Raphael Junqueira
 * Copyright 2002-2003 Jason Edmeades
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

/* IDirect3DStateBlock9 IUnknown parts follow: */
HRESULT WINAPI IDirect3DStateBlock9Impl_QueryInterface(LPDIRECT3DSTATEBLOCK9 iface, REFIID riid, LPVOID* ppobj) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;

    if (IsEqualGUID(riid, &IID_IUnknown)
        || IsEqualGUID(riid, &IID_IDirect3DStateBlock9)) {
        IUnknown_AddRef(iface);
        *ppobj = This;
        return D3D_OK;
    }

    WARN("(%p)->(%s,%p),not found\n", This, debugstr_guid(riid), ppobj);
    return E_NOINTERFACE;
}

ULONG WINAPI IDirect3DStateBlock9Impl_AddRef(LPDIRECT3DSTATEBLOCK9 iface) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;
    ULONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) : AddRef from %ld\n", This, ref - 1);

    return ref;
}

ULONG WINAPI IDirect3DStateBlock9Impl_Release(LPDIRECT3DSTATEBLOCK9 iface) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) : ReleaseRef to %ld\n", This, ref);

    if (ref == 0) {
        IWineD3DStateBlock_Release(This->wineD3DStateBlock);    
        HeapFree(GetProcessHeap(), 0, This);
    }
    return ref;
}

/* IDirect3DStateBlock9 Interface follow: */
HRESULT WINAPI IDirect3DStateBlock9Impl_GetDevice(LPDIRECT3DSTATEBLOCK9 iface, IDirect3DDevice9** ppDevice) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;
    TRACE("(%p) Relay\n", This); 
    return IDirect3DResource9Impl_GetDevice((LPDIRECT3DRESOURCE9) This, ppDevice);
}

HRESULT WINAPI IDirect3DStateBlock9Impl_Capture(LPDIRECT3DSTATEBLOCK9 iface) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;
    TRACE("(%p) Relay\n", This); 
    return IWineD3DStateBlock_Capture(This->wineD3DStateBlock);
}

HRESULT WINAPI IDirect3DStateBlock9Impl_Apply(LPDIRECT3DSTATEBLOCK9 iface) {
    IDirect3DStateBlock9Impl *This = (IDirect3DStateBlock9Impl *)iface;
    TRACE("(%p) Relay\n", This); 
    return IWineD3DStateBlock_Apply(This->wineD3DStateBlock);
}


const IDirect3DStateBlock9Vtbl Direct3DStateBlock9_Vtbl =
{
    /* IUnknown */
    IDirect3DStateBlock9Impl_QueryInterface,
    IDirect3DStateBlock9Impl_AddRef,
    IDirect3DStateBlock9Impl_Release,
    /* IDirect3DStateBlock9 */
    IDirect3DStateBlock9Impl_GetDevice,
    IDirect3DStateBlock9Impl_Capture,
    IDirect3DStateBlock9Impl_Apply
};


/* IDirect3DDevice9 IDirect3DStateBlock9 Methods follow: */
HRESULT WINAPI IDirect3DDevice9Impl_CreateStateBlock(LPDIRECT3DDEVICE9 iface, D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppStateBlock) {
   IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;
   IDirect3DStateBlock9Impl* object;
   HRESULT hrc = D3D_OK;
   
   TRACE("(%p) Relay\n", This);
   
   object  = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirect3DStateBlock9Impl));
   if (NULL == object) {
      FIXME("(%p)  Failed to allocate %d bytes\n", This, sizeof(IDirect3DStateBlock9Impl));
      *ppStateBlock = NULL;
      return E_OUTOFMEMORY;
   }
   object->lpVtbl = &Direct3DStateBlock9_Vtbl;
   object->ref = 1;
   
   hrc = IWineD3DDevice_CreateStateBlock(This->WineD3DDevice, (WINED3DSTATEBLOCKTYPE)Type, &object->wineD3DStateBlock, (IUnknown*)object);
   if(hrc != D3D_OK){
       FIXME("(%p) Call to IWineD3DDevice_CreateStateBlock failed.\n", This);
       HeapFree(GetProcessHeap(), 0, object);
       *ppStateBlock = NULL;
   } else {
       *ppStateBlock = (IDirect3DStateBlock9*)object;
   }
   TRACE("(%p) returning token (ptr to stateblock) of %p\n", This, object);    
   return hrc;
}

HRESULT  WINAPI  IDirect3DDevice9Impl_BeginStateBlock(LPDIRECT3DDEVICE9 iface) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;    
    TRACE("(%p) Relay\n", This); 
    return IWineD3DDevice_BeginStateBlock(This->WineD3DDevice);
}

HRESULT  WINAPI  IDirect3DDevice9Impl_EndStateBlock(LPDIRECT3DDEVICE9 iface, IDirect3DStateBlock9** ppSB) {
    IDirect3DDevice9Impl *This = (IDirect3DDevice9Impl *)iface;   
    HRESULT hr;
    IWineD3DStateBlock* wineD3DStateBlock;    
    IDirect3DStateBlock9Impl* object;

    TRACE("(%p) Relay\n", This); 
    
    /* Tell wineD3D to endstatablock before anything else (in case we run out
     * of memory later and cause locking problems)
     */
    hr=IWineD3DDevice_EndStateBlock(This->WineD3DDevice,&wineD3DStateBlock);
    if(hr!= D3D_OK){
       FIXME("IWineD3DDevice_EndStateBlock returned an error\n");
       return hr;
    }    
    /* allocate a new IDirectD3DStateBlock */
    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY ,sizeof(IDirect3DStateBlock9Impl));      
    object->ref = 1;
    object->lpVtbl = &Direct3DStateBlock9_Vtbl;
      
    object->wineD3DStateBlock=wineD3DStateBlock;
  
    *ppSB=(IDirect3DStateBlock9*)object;        
    TRACE("(%p)Returning %p %p\n", This, *ppSB, wineD3DStateBlock);
    return D3D_OK;
}
