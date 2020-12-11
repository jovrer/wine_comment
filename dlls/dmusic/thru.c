/* IDirectMusicThru Implementation
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILIY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dmusic_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmusic);

/* IDirectMusicThru IUnknown parts follow: */
HRESULT WINAPI IDirectMusicThruImpl_QueryInterface (LPDIRECTMUSICTHRU iface, REFIID riid, LPVOID *ppobj) {
	IDirectMusicThruImpl *This = (IDirectMusicThruImpl *)iface;
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);

	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IDirectMusicThru)) {
		IDirectMusicThruImpl_AddRef(iface);
		*ppobj = This;
		return S_OK;
	}
	WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ppobj);
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicThruImpl_AddRef (LPDIRECTMUSICTHRU iface) {
	IDirectMusicThruImpl *This = (IDirectMusicThruImpl *)iface;
	ULONG refCount = InterlockedIncrement(&This->ref);

	TRACE("(%p)->(ref before=%lu)\n", This, refCount - 1);

	DMUSIC_LockModule();

	return refCount;
}

ULONG WINAPI IDirectMusicThruImpl_Release (LPDIRECTMUSICTHRU iface) {
	IDirectMusicThruImpl *This = (IDirectMusicThruImpl *)iface;
	ULONG refCount = InterlockedDecrement(&This->ref);

	TRACE("(%p)->(ref before=%lu)\n", This, refCount + 1);

	if (!refCount) {
		HeapFree(GetProcessHeap(), 0, This);
	}

	DMUSIC_UnlockModule();

	return refCount;
}

/* IDirectMusicThru Interface follow: */
HRESULT WINAPI IDirectMusicThruImpl_ThruChannel (LPDIRECTMUSICTHRU iface, DWORD dwSourceChannelGroup, DWORD dwSourceChannel, DWORD dwDestinationChannelGroup, DWORD dwDestinationChannel, LPDIRECTMUSICPORT pDestinationPort) {
	IDirectMusicThruImpl *This = (IDirectMusicThruImpl *)iface;
	FIXME("(%p, %ld, %ld, %ld, %ld, %p): stub\n", This, dwSourceChannelGroup, dwSourceChannel, dwDestinationChannelGroup, dwDestinationChannel, pDestinationPort);
	return S_OK;
}

static const IDirectMusicThruVtbl DirectMusicThru_Vtbl = {
	IDirectMusicThruImpl_QueryInterface,
	IDirectMusicThruImpl_AddRef,
	IDirectMusicThruImpl_Release,
	IDirectMusicThruImpl_ThruChannel
};
