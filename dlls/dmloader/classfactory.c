 /* IDirectMusicLoaderCF
 *  IDirectMusicContainerCF
 *
 * Copyright (C) 2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dmloader_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmloader);

/*****************************************************************************
 * IDirectMusicLoaderCF implementation
 */
HRESULT WINAPI IDirectMusicLoaderCF_QueryInterface (LPCLASSFACTORY iface, REFIID riid, LPVOID *ppobj) {
	IDirectMusicLoaderCF *This = (IDirectMusicLoaderCF *)iface;
	
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);
	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IClassFactory)) {
		IDirectMusicLoaderCF_AddRef (iface);
		*ppobj = This;
		return S_OK;
	}
	
	WARN(": not found\n");
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicLoaderCF_AddRef (LPCLASSFACTORY iface) {
	IDirectMusicLoaderCF *This = (IDirectMusicLoaderCF *)iface;
	TRACE("(%p): AddRef from %ld\n", This, This->dwRef);
	return InterlockedIncrement (&This->dwRef);
}

ULONG WINAPI IDirectMusicLoaderCF_Release (LPCLASSFACTORY iface) {
	IDirectMusicLoaderCF *This = (IDirectMusicLoaderCF *)iface;
	
	DWORD dwRef = InterlockedDecrement (&This->dwRef);
	TRACE("(%p): ReleaseRef to %ld\n", This, dwRef);
	if (dwRef == 0) {
		HeapFree(GetProcessHeap (), 0, This);
		/* decrease number of instances */
		InterlockedDecrement(&dwDirectMusicLoader);
	}
	
	return dwRef;	
}

HRESULT WINAPI IDirectMusicLoaderCF_CreateInstance (LPCLASSFACTORY iface, LPUNKNOWN pOuter, REFIID riid, LPVOID *ppobj) {
	IDirectMusicLoaderCF *This = (IDirectMusicLoaderCF *)iface;
	
	TRACE ("(%p, %p, %s, %p)\n", This, pOuter, debugstr_dmguid(riid), ppobj);
	if (pOuter) {
		ERR(": pOuter should be NULL\n");
		return CLASS_E_NOAGGREGATION;
	}
	
	return DMUSIC_CreateDirectMusicLoaderImpl (riid, ppobj, pOuter);
}

HRESULT WINAPI IDirectMusicLoaderCF_LockServer (LPCLASSFACTORY iface, BOOL dolock) {
	IDirectMusicLoaderCF *This = (IDirectMusicLoaderCF *)iface;
	TRACE("(%p, %d)\n", This, dolock);
	if (dolock)
		InterlockedIncrement (&dwDirectMusicLoader);
	else
		InterlockedDecrement (&dwDirectMusicLoader);
	return S_OK;
}

static const IClassFactoryVtbl DirectMusicLoaderCF_Vtbl = {
	IDirectMusicLoaderCF_QueryInterface,
	IDirectMusicLoaderCF_AddRef,
	IDirectMusicLoaderCF_Release,
	IDirectMusicLoaderCF_CreateInstance,
	IDirectMusicLoaderCF_LockServer
};

HRESULT WINAPI DMUSIC_CreateDirectMusicLoaderCF (LPCGUID lpcGUID, LPVOID *ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicLoaderCF *obj;

	TRACE("(%s, %p, %p)\n", debugstr_dmguid(lpcGUID), ppobj, pUnkOuter);
	obj = HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicLoaderCF));
	if (NULL == obj) {
		*ppobj = (LPCLASSFACTORY)NULL;
		return E_OUTOFMEMORY;
	}
	obj->lpVtbl = &DirectMusicLoaderCF_Vtbl;
	obj->dwRef = 0; /* will be inited with QueryInterface */

	/* increase number of instances */
	InterlockedIncrement (&dwDirectMusicLoader);	
	
	return IDirectMusicLoaderCF_QueryInterface ((LPCLASSFACTORY)obj, lpcGUID, ppobj);
}


/*****************************************************************************
 * IDirectMusicContainerCF implementation
 */
HRESULT WINAPI IDirectMusicContainerCF_QueryInterface (LPCLASSFACTORY iface, REFIID riid, LPVOID *ppobj) {
	IDirectMusicContainerCF *This = (IDirectMusicContainerCF *)iface;
	
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);
	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IClassFactory)) {
		IDirectMusicContainerCF_AddRef (iface);
		*ppobj = This;
		return S_OK;
	}
	
	WARN(": not found\n");
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicContainerCF_AddRef (LPCLASSFACTORY iface) {
	IDirectMusicContainerCF *This = (IDirectMusicContainerCF *)iface;
	TRACE("(%p): AddRef from %ld\n", This, This->dwRef);
	return InterlockedIncrement (&This->dwRef);
}

ULONG WINAPI IDirectMusicContainerCF_Release (LPCLASSFACTORY iface) {
	IDirectMusicContainerCF *This = (IDirectMusicContainerCF *)iface;
	
	DWORD dwRef = InterlockedDecrement (&This->dwRef);
	TRACE("(%p): ReleaseRef to %ld\n", This, dwRef);
	if (dwRef == 0) {
		HeapFree(GetProcessHeap (), 0, This);
		/* decrease number of instances */
		InterlockedDecrement(&dwDirectMusicContainer);
	}
	
	return dwRef;
}

HRESULT WINAPI IDirectMusicContainerCF_CreateInstance (LPCLASSFACTORY iface, LPUNKNOWN pOuter, REFIID riid, LPVOID *ppobj) {
	IDirectMusicContainerCF *This = (IDirectMusicContainerCF *)iface;

	TRACE ("(%p, %p, %s, %p)\n", This, pOuter, debugstr_dmguid(riid), ppobj);
	if (pOuter) {
		ERR(": pOuter should be NULL\n");
		return CLASS_E_NOAGGREGATION;
	}

	return DMUSIC_CreateDirectMusicContainerImpl (riid, ppobj, pOuter);
}

HRESULT WINAPI IDirectMusicContainerCF_LockServer (LPCLASSFACTORY iface, BOOL dolock) {
	IDirectMusicContainerCF *This = (IDirectMusicContainerCF *)iface;
	TRACE("(%p, %d)\n", This, dolock);
	if (dolock)
		InterlockedIncrement (&dwDirectMusicContainer);
	else
		InterlockedDecrement (&dwDirectMusicContainer);
	return S_OK;
}

static const IClassFactoryVtbl DirectMusicContainerCF_Vtbl = {
	IDirectMusicContainerCF_QueryInterface,
	IDirectMusicContainerCF_AddRef,
	IDirectMusicContainerCF_Release,
	IDirectMusicContainerCF_CreateInstance,
	IDirectMusicContainerCF_LockServer
};

HRESULT WINAPI DMUSIC_CreateDirectMusicContainerCF (LPCGUID lpcGUID, LPVOID *ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicContainerCF *obj;

	TRACE("(%s, %p, %p)\n", debugstr_dmguid(lpcGUID), ppobj, pUnkOuter);
	obj = HeapAlloc (GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicContainerCF));
	if (NULL == obj) {
		*ppobj = (LPCLASSFACTORY)NULL;
		return E_OUTOFMEMORY;
	}
	obj->lpVtbl = &DirectMusicContainerCF_Vtbl;
	obj->dwRef = 0; /* will be inited with QueryInterface */

	/* increase number of instances */
	InterlockedIncrement (&dwDirectMusicContainer);
	
	return IDirectMusicContainerCF_QueryInterface ((LPCLASSFACTORY)obj, lpcGUID, ppobj);
}
