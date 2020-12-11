/* IDirectMusicSynthSink Implementation
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "dmsynth_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmsynth);

/* IDirectMusicSynthSinkImpl IUnknown part: */
HRESULT WINAPI IDirectMusicSynthSinkImpl_QueryInterface (LPDIRECTMUSICSYNTHSINK iface, REFIID riid, LPVOID *ppobj) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	TRACE("(%p, %s, %p)\n", This, debugstr_dmguid(riid), ppobj);

	if (IsEqualIID (riid, &IID_IUnknown) || 
	    IsEqualIID (riid, &IID_IDirectMusicSynthSink)) {
		IDirectMusicSynthSinkImpl_AddRef(iface);
		*ppobj = This;
		return S_OK;
	}
	WARN("(%p, %s, %p): not found\n", This, debugstr_dmguid(riid), ppobj);
	return E_NOINTERFACE;
}

ULONG WINAPI IDirectMusicSynthSinkImpl_AddRef (LPDIRECTMUSICSYNTHSINK iface) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	ULONG refCount = InterlockedIncrement(&This->ref);

	TRACE("(%p)->(ref before=%lu)\n", This, refCount - 1);

	DMSYNTH_LockModule();

	return refCount;
}

ULONG WINAPI IDirectMusicSynthSinkImpl_Release (LPDIRECTMUSICSYNTHSINK iface) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	ULONG refCount = InterlockedDecrement(&This->ref);

	TRACE("(%p)->(ref before=%lu)\n", This, refCount + 1);

	if (!refCount) {
		HeapFree(GetProcessHeap(), 0, This);
	}

	DMSYNTH_UnlockModule();

	return refCount;
}

/* IDirectMusicSynthSinkImpl IDirectMusicSynthSink part: */
HRESULT WINAPI IDirectMusicSynthSinkImpl_Init (LPDIRECTMUSICSYNTHSINK iface, IDirectMusicSynth* pSynth) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %p): stub\n", This, pSynth);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_SetMasterClock (LPDIRECTMUSICSYNTHSINK iface, IReferenceClock* pClock) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %p): stub\n", This, pClock);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_GetLatencyClock (LPDIRECTMUSICSYNTHSINK iface, IReferenceClock** ppClock) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %p): stub\n", This, ppClock);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_Activate (LPDIRECTMUSICSYNTHSINK iface, BOOL fEnable) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %d): stub\n", This, fEnable);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_SampleToRefTime (LPDIRECTMUSICSYNTHSINK iface, LONGLONG llSampleTime, REFERENCE_TIME* prfTime) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %lli, %p): stub\n", This, llSampleTime, prfTime);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_RefTimeToSample (LPDIRECTMUSICSYNTHSINK iface, REFERENCE_TIME rfTime, LONGLONG* pllSampleTime) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %lli, %p): stub\n", This, rfTime, pllSampleTime );
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_SetDirectSound (LPDIRECTMUSICSYNTHSINK iface, LPDIRECTSOUND pDirectSound, LPDIRECTSOUNDBUFFER pDirectSoundBuffer) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %p, %p): stub\n", This, pDirectSound, pDirectSoundBuffer);
	return S_OK;
}

HRESULT WINAPI IDirectMusicSynthSinkImpl_GetDesiredBufferSize (LPDIRECTMUSICSYNTHSINK iface, LPDWORD pdwBufferSizeInSamples) {
	IDirectMusicSynthSinkImpl *This = (IDirectMusicSynthSinkImpl *)iface;
	FIXME("(%p, %p): stub\n", This, pdwBufferSizeInSamples);
	return S_OK;
}

static const IDirectMusicSynthSinkVtbl DirectMusicSynthSink_Vtbl = {
	IDirectMusicSynthSinkImpl_QueryInterface,
	IDirectMusicSynthSinkImpl_AddRef,
	IDirectMusicSynthSinkImpl_Release,
	IDirectMusicSynthSinkImpl_Init,
	IDirectMusicSynthSinkImpl_SetMasterClock,
	IDirectMusicSynthSinkImpl_GetLatencyClock,
	IDirectMusicSynthSinkImpl_Activate,
	IDirectMusicSynthSinkImpl_SampleToRefTime,
	IDirectMusicSynthSinkImpl_RefTimeToSample,
	IDirectMusicSynthSinkImpl_SetDirectSound,
	IDirectMusicSynthSinkImpl_GetDesiredBufferSize
};

/* for ClassFactory */
HRESULT WINAPI DMUSIC_CreateDirectMusicSynthSinkImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter) {
	IDirectMusicSynthSinkImpl *obj;
	
	TRACE("(%p,%p,%p)\n", lpcGUID, ppobj, pUnkOuter);
	obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectMusicSynthSinkImpl));
	if (NULL == obj) {
		*ppobj = (LPDIRECTMUSICSYNTHSINK) NULL;
		return E_OUTOFMEMORY;
	}
	obj->lpVtbl = &DirectMusicSynthSink_Vtbl;
	obj->ref = 0;
	
	return IDirectMusicSynthSinkImpl_QueryInterface((LPDIRECTMUSICSYNTHSINK)obj, lpcGUID, ppobj);
}
