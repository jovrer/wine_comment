/*
 * Direct Sound Audio Renderer
 *
 * Copyright 2004 Christian Costa
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

#include "quartz_private.h"
#include "control_private.h"
#include "pin.h"

#include "uuids.h"
#include "mmreg.h"
#include "vfwmsgs.h"
#include "fourcc.h"
#include "windef.h"
#include "winbase.h"
#include "dshow.h"
#include "evcode.h"
#include "strmif.h"
#include "dsound.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(quartz);

static const WCHAR wcsInputPinName[] = {'i','n','p','u','t',' ','p','i','n',0};

static const IBaseFilterVtbl DSoundRender_Vtbl;
static const IPinVtbl DSoundRender_InputPin_Vtbl;
static const IMemInputPinVtbl MemInputPin_Vtbl; 
static const IBasicAudioVtbl IBasicAudio_Vtbl;

typedef struct DSoundRenderImpl
{
    const IBaseFilterVtbl * lpVtbl;
    const IBasicAudioVtbl *IBasicAudio_vtbl;

    LONG refCount;
    CRITICAL_SECTION csFilter;
    FILTER_STATE state;
    REFERENCE_TIME rtStreamStart;
    IReferenceClock * pClock;
    FILTER_INFO filterInfo;

    InputPin * pInputPin;
    IPin ** ppPins;

    LPDIRECTSOUND dsound;
    LPDIRECTSOUNDBUFFER dsbuffer;
    DWORD write_pos;
    BOOL init;
    BOOL started;
} DSoundRenderImpl;

static HRESULT DSoundRender_InputPin_Construct(const PIN_INFO * pPinInfo, SAMPLEPROC pSampleProc, LPVOID pUserData, QUERYACCEPTPROC pQueryAccept, LPCRITICAL_SECTION pCritSec, IPin ** ppPin)
{
    InputPin * pPinImpl;

    *ppPin = NULL;

    if (pPinInfo->dir != PINDIR_INPUT)
    {
        ERR("Pin direction(%x) != PINDIR_INPUT\n", pPinInfo->dir);
        return E_INVALIDARG;
    }

    pPinImpl = CoTaskMemAlloc(sizeof(*pPinImpl));

    if (!pPinImpl)
        return E_OUTOFMEMORY;

    if (SUCCEEDED(InputPin_Init(pPinInfo, pSampleProc, pUserData, pQueryAccept, pCritSec, pPinImpl)))
    {
        pPinImpl->pin.lpVtbl = &DSoundRender_InputPin_Vtbl;
        pPinImpl->lpVtblMemInput = &MemInputPin_Vtbl;
        
        *ppPin = (IPin *)(&pPinImpl->pin.lpVtbl);
        return S_OK;
    }
    return E_FAIL;
}


#define DSBUFFERSIZE 8192

static HRESULT DSoundRender_CreateSoundBuffer(IBaseFilter * iface)
{
    HRESULT hr;
    WAVEFORMATEX wav_fmt;
    AM_MEDIA_TYPE amt;
    WAVEFORMATEX* format;
    DSBUFFERDESC buf_desc;
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    hr = IPin_ConnectionMediaType(This->ppPins[0], &amt);
    if (FAILED(hr)) {
	ERR("Unable to retrieve media type\n");
	return hr;
    }

    TRACE("MajorType %s\n", debugstr_guid(&amt.majortype));
    TRACE("SubType %s\n", debugstr_guid(&amt.subtype));
    TRACE("Format %s\n", debugstr_guid(&amt.formattype));
    TRACE("Size %ld\n", amt.cbFormat);

    dump_AM_MEDIA_TYPE(&amt);
    
    format = (WAVEFORMATEX*)amt.pbFormat;
    TRACE("wFormatTag = %x %x\n", format->wFormatTag, WAVE_FORMAT_PCM);
    TRACE("nChannels = %d\n", format->nChannels);
    TRACE("nSamplesPerSec = %lu\n", format->nSamplesPerSec);
    TRACE("nAvgBytesPerSec = %lu\n", format->nAvgBytesPerSec);
    TRACE("nBlockAlign = %d\n", format->nBlockAlign);
    TRACE("wBitsPerSample = %d\n", format->wBitsPerSample);
    TRACE("cbSize = %d\n", format->cbSize);
    
    hr = DirectSoundCreate(NULL, &This->dsound, NULL);
    if (FAILED(hr)) {
	ERR("Cannot create Direct Sound object\n");
	return hr;
    }

    wav_fmt = *format;
    wav_fmt.cbSize = 0;

    memset(&buf_desc,0,sizeof(DSBUFFERDESC));
    buf_desc.dwSize = sizeof(DSBUFFERDESC);
    buf_desc.dwBufferBytes = DSBUFFERSIZE;
    buf_desc.lpwfxFormat = &wav_fmt;
    hr = IDirectSound_CreateSoundBuffer(This->dsound, &buf_desc, &This->dsbuffer, NULL);
    if (FAILED(hr)) {
        ERR("Can't create sound buffer !\n");
        IDirectSound_Release(This->dsound);
        return hr;
    }

    This->write_pos = 0;
    
    return hr;
}

static HRESULT DSoundRender_SendSampleData(DSoundRenderImpl* This, LPBYTE data, DWORD size)
{
    HRESULT hr;
    LPBYTE lpbuf1 = NULL;
    LPBYTE lpbuf2 = NULL;
    DWORD dwsize1 = 0;
    DWORD dwsize2 = 0;
    DWORD size2;
    DWORD play_pos,buf_free;

    while (1)
    {
        hr = IDirectSoundBuffer_GetCurrentPosition(This->dsbuffer, &play_pos, NULL);
        if (hr != DS_OK)
        {
            ERR("Error GetCurrentPosition: %lx\n", hr);
            break;
        }
        if (This->write_pos < play_pos)
             buf_free = play_pos-This->write_pos;
        else
             buf_free = DSBUFFERSIZE - This->write_pos + play_pos;

        size2 = min(buf_free, size);
        hr = IDirectSoundBuffer_Lock(This->dsbuffer, This->write_pos, size2, &lpbuf1, &dwsize1, &lpbuf2, &dwsize2, 0);
        if (hr != DS_OK) {
            ERR("Unable to lock sound buffer! (%lx)\n", hr);
            break;
        }
        /* TRACE("write_pos=%ld, size=%ld, sz1=%ld, sz2=%ld\n", This->write_pos, size2, dwsize1, dwsize2); */

        memcpy(lpbuf1, data, dwsize1);
        if (dwsize2)
            memcpy(lpbuf2, data + dwsize1, dwsize2);

        hr = IDirectSoundBuffer_Unlock(This->dsbuffer, lpbuf1, dwsize1, lpbuf2, dwsize2);
        if (hr != DS_OK)
            ERR("Unable to unlock sound buffer! (%lx)\n", hr);
        if (!This->started)
        {
            hr = IDirectSoundBuffer_Play(This->dsbuffer, 0, 0, DSBPLAY_LOOPING);
            if (hr == DS_OK)
                This->started = TRUE;
            else
                ERR("Can't start playing! (%lx)\n", hr);
        }
        size -= dwsize1 + dwsize2;
        data += dwsize1 + dwsize2;
        This->write_pos = (This->write_pos + dwsize1 + dwsize2) % DSBUFFERSIZE;

        if (!size)
            break;
        Sleep(10);
    }

    return hr;
}

static HRESULT DSoundRender_Sample(LPVOID iface, IMediaSample * pSample)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    LPBYTE pbSrcStream = NULL;
    long cbSrcStream = 0;
    REFERENCE_TIME tStart, tStop;
    HRESULT hr;

    TRACE("%p %p\n", iface, pSample);
    
    hr = IMediaSample_GetPointer(pSample, &pbSrcStream);
    if (FAILED(hr))
    {
        ERR("Cannot get pointer to sample data (%lx)\n", hr);
	return hr;
    }

    hr = IMediaSample_GetTime(pSample, &tStart, &tStop);
    if (FAILED(hr))
        ERR("Cannot get sample time (%lx)\n", hr);

    cbSrcStream = IMediaSample_GetActualDataLength(pSample);

    TRACE("Sample data ptr = %p, size = %ld\n", pbSrcStream, cbSrcStream);

#if 0 /* For debugging purpose */
    {
        int i;
        for(i = 0; i < cbSrcStream; i++)
        {
	    if ((i!=0) && !(i%16))
                TRACE("\n");
            TRACE("%02x ", pbSrcStream[i]);
        }
        TRACE("\n");
    }
#endif
  
    if (!This->init)
    {
        hr = DSoundRender_CreateSoundBuffer(iface);
        if (SUCCEEDED(hr))
            This->init = TRUE;
        else
        {
            ERR("Unable to create DSound buffer\n");
            return hr;
        }
    }
    
    hr = DSoundRender_SendSampleData(This, pbSrcStream, cbSrcStream);

    return hr;
}

static HRESULT DSoundRender_QueryAccept(LPVOID iface, const AM_MEDIA_TYPE * pmt)
{
    WAVEFORMATEX* format = (WAVEFORMATEX*)pmt->pbFormat;
    TRACE("wFormatTag = %x %x\n", format->wFormatTag, WAVE_FORMAT_PCM);
    TRACE("nChannels = %d\n", format->nChannels);
    TRACE("nSamplesPerSec = %ld\n", format->nAvgBytesPerSec);
    TRACE("nAvgBytesPerSec = %ld\n", format->nAvgBytesPerSec);
    TRACE("nBlockAlign = %d\n", format->nBlockAlign);
    TRACE("wBitsPerSample = %d\n", format->wBitsPerSample);

    if (IsEqualIID(&pmt->majortype, &MEDIATYPE_Audio) && IsEqualIID(&pmt->subtype, &MEDIASUBTYPE_PCM))
        return S_OK;
    return S_FALSE;
}

HRESULT DSoundRender_create(IUnknown * pUnkOuter, LPVOID * ppv)
{
    HRESULT hr;
    PIN_INFO piInput;
    DSoundRenderImpl * pDSoundRender;

    TRACE("(%p, %p)\n", pUnkOuter, ppv);

    *ppv = NULL;

    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;
    
    pDSoundRender = CoTaskMemAlloc(sizeof(DSoundRenderImpl));

    pDSoundRender->lpVtbl = &DSoundRender_Vtbl;
    pDSoundRender->IBasicAudio_vtbl = &IBasicAudio_Vtbl;
    pDSoundRender->refCount = 1;
    InitializeCriticalSection(&pDSoundRender->csFilter);
    pDSoundRender->state = State_Stopped;
    pDSoundRender->pClock = NULL;
    pDSoundRender->init = FALSE;
    pDSoundRender->started = FALSE;
    ZeroMemory(&pDSoundRender->filterInfo, sizeof(FILTER_INFO));

    pDSoundRender->ppPins = CoTaskMemAlloc(1 * sizeof(IPin *));

    /* construct input pin */
    piInput.dir = PINDIR_INPUT;
    piInput.pFilter = (IBaseFilter *)pDSoundRender;
    lstrcpynW(piInput.achName, wcsInputPinName, sizeof(piInput.achName) / sizeof(piInput.achName[0]));
    hr = DSoundRender_InputPin_Construct(&piInput, DSoundRender_Sample, (LPVOID)pDSoundRender, DSoundRender_QueryAccept, &pDSoundRender->csFilter, (IPin **)&pDSoundRender->pInputPin);

    if (SUCCEEDED(hr))
    {
        pDSoundRender->ppPins[0] = (IPin *)pDSoundRender->pInputPin;
        *ppv = (LPVOID)pDSoundRender;
    }
    else
    {
        CoTaskMemFree(pDSoundRender->ppPins);
        DeleteCriticalSection(&pDSoundRender->csFilter);
        CoTaskMemFree(pDSoundRender);
    }

    return hr;
}

static HRESULT WINAPI DSoundRender_QueryInterface(IBaseFilter * iface, REFIID riid, LPVOID * ppv)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    TRACE("(%p, %p)->(%s, %p)\n", This, iface, qzdebugstr_guid(riid), ppv);

    *ppv = NULL;

    if (IsEqualIID(riid, &IID_IUnknown))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IPersist))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IMediaFilter))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IBaseFilter))
        *ppv = (LPVOID)This;
    else if (IsEqualIID(riid, &IID_IBaseFilter))
        *ppv = (LPVOID)&(This->IBasicAudio_vtbl);

    if (*ppv)
    {
        IUnknown_AddRef((IUnknown *)(*ppv));
        return S_OK;
    }

    FIXME("No interface for %s!\n", qzdebugstr_guid(riid));

    return E_NOINTERFACE;
}

static ULONG WINAPI DSoundRender_AddRef(IBaseFilter * iface)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    ULONG refCount = InterlockedIncrement(&This->refCount);

    TRACE("(%p/%p)->() AddRef from %ld\n", This, iface, refCount - 1);

    return refCount;
}

static ULONG WINAPI DSoundRender_Release(IBaseFilter * iface)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    ULONG refCount = InterlockedDecrement(&This->refCount);

    TRACE("(%p/%p)->() Release from %ld\n", This, iface, refCount + 1);

    if (!refCount)
    {
        DeleteCriticalSection(&This->csFilter);
	if (This->pClock)
            IReferenceClock_Release(This->pClock);
       
        IPin_Release(This->ppPins[0]);
        
        HeapFree(GetProcessHeap(), 0, This->ppPins);
        This->lpVtbl = NULL;
        This->IBasicAudio_vtbl = NULL;
        
        TRACE("Destroying Audio Renderer\n");
        CoTaskMemFree(This);
        
        return 0;
    }
    else
        return refCount;
}

/** IPersist methods **/

static HRESULT WINAPI DSoundRender_GetClassID(IBaseFilter * iface, CLSID * pClsid)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    TRACE("(%p/%p)->(%p)\n", This, iface, pClsid);

    *pClsid = CLSID_DSoundRender;

    return S_OK;
}

/** IMediaFilter methods **/

static HRESULT WINAPI DSoundRender_Stop(IBaseFilter * iface)
{
    HRESULT hr = S_OK;
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->()\n", This, iface);

    EnterCriticalSection(&This->csFilter);
    {
        This->state = State_Stopped;
    }
    LeaveCriticalSection(&This->csFilter);
    
    return hr;
}

static HRESULT WINAPI DSoundRender_Pause(IBaseFilter * iface)
{
    HRESULT hr = S_OK;
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    
    TRACE("(%p/%p)->()\n", This, iface);

    EnterCriticalSection(&This->csFilter);
    {
        This->state = State_Paused;
    }
    LeaveCriticalSection(&This->csFilter);

    return hr;
}

static HRESULT WINAPI DSoundRender_Run(IBaseFilter * iface, REFERENCE_TIME tStart)
{
    HRESULT hr = S_OK;
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%s)\n", This, iface, wine_dbgstr_longlong(tStart));

    EnterCriticalSection(&This->csFilter);
    {
        This->rtStreamStart = tStart;
        This->state = State_Running;
    }
    LeaveCriticalSection(&This->csFilter);

    return hr;
}

static HRESULT WINAPI DSoundRender_GetState(IBaseFilter * iface, DWORD dwMilliSecsTimeout, FILTER_STATE *pState)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%ld, %p)\n", This, iface, dwMilliSecsTimeout, pState);

    EnterCriticalSection(&This->csFilter);
    {
        *pState = This->state;
    }
    LeaveCriticalSection(&This->csFilter);

    return S_OK;
}

static HRESULT WINAPI DSoundRender_SetSyncSource(IBaseFilter * iface, IReferenceClock *pClock)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%p)\n", This, iface, pClock);

    EnterCriticalSection(&This->csFilter);
    {
        if (This->pClock)
            IReferenceClock_Release(This->pClock);
        This->pClock = pClock;
        if (This->pClock)
            IReferenceClock_AddRef(This->pClock);
    }
    LeaveCriticalSection(&This->csFilter);

    return S_OK;
}

static HRESULT WINAPI DSoundRender_GetSyncSource(IBaseFilter * iface, IReferenceClock **ppClock)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%p)\n", This, iface, ppClock);

    EnterCriticalSection(&This->csFilter);
    {
        *ppClock = This->pClock;
        IReferenceClock_AddRef(This->pClock);
    }
    LeaveCriticalSection(&This->csFilter);
    
    return S_OK;
}

/** IBaseFilter implementation **/

static HRESULT WINAPI DSoundRender_EnumPins(IBaseFilter * iface, IEnumPins **ppEnum)
{
    ENUMPINDETAILS epd;
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%p)\n", This, iface, ppEnum);

    epd.cPins = 1; /* input pin */
    epd.ppPins = This->ppPins;
    return IEnumPinsImpl_Construct(&epd, ppEnum);
}

static HRESULT WINAPI DSoundRender_FindPin(IBaseFilter * iface, LPCWSTR Id, IPin **ppPin)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%s,%p)\n", This, iface, debugstr_w(Id), ppPin);
    
    FIXME("DSoundRender::FindPin(...)\n");

    /* FIXME: critical section */

    return E_NOTIMPL;
}

static HRESULT WINAPI DSoundRender_QueryFilterInfo(IBaseFilter * iface, FILTER_INFO *pInfo)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%p)\n", This, iface, pInfo);

    strcpyW(pInfo->achName, This->filterInfo.achName);
    pInfo->pGraph = This->filterInfo.pGraph;

    if (pInfo->pGraph)
        IFilterGraph_AddRef(pInfo->pGraph);
    
    return S_OK;
}

static HRESULT WINAPI DSoundRender_JoinFilterGraph(IBaseFilter * iface, IFilterGraph *pGraph, LPCWSTR pName)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;

    TRACE("(%p/%p)->(%p, %s)\n", This, iface, pGraph, debugstr_w(pName));

    EnterCriticalSection(&This->csFilter);
    {
        if (pName)
            strcpyW(This->filterInfo.achName, pName);
        else
            *This->filterInfo.achName = '\0';
        This->filterInfo.pGraph = pGraph; /* NOTE: do NOT increase ref. count */
    }
    LeaveCriticalSection(&This->csFilter);

    return S_OK;
}

static HRESULT WINAPI DSoundRender_QueryVendorInfo(IBaseFilter * iface, LPWSTR *pVendorInfo)
{
    DSoundRenderImpl *This = (DSoundRenderImpl *)iface;
    TRACE("(%p/%p)->(%p)\n", This, iface, pVendorInfo);
    return E_NOTIMPL;
}

static const IBaseFilterVtbl DSoundRender_Vtbl =
{
    DSoundRender_QueryInterface,
    DSoundRender_AddRef,
    DSoundRender_Release,
    DSoundRender_GetClassID,
    DSoundRender_Stop,
    DSoundRender_Pause,
    DSoundRender_Run,
    DSoundRender_GetState,
    DSoundRender_SetSyncSource,
    DSoundRender_GetSyncSource,
    DSoundRender_EnumPins,
    DSoundRender_FindPin,
    DSoundRender_QueryFilterInfo,
    DSoundRender_JoinFilterGraph,
    DSoundRender_QueryVendorInfo
};

static HRESULT WINAPI DSoundRender_InputPin_EndOfStream(IPin * iface)
{
    InputPin* This = (InputPin*)iface;
    IMediaEventSink* pEventSink;
    HRESULT hr;

    TRACE("(%p/%p)->()\n", This, iface);

    hr = IFilterGraph_QueryInterface(((DSoundRenderImpl*)This->pin.pinInfo.pFilter)->filterInfo.pGraph, &IID_IMediaEventSink, (LPVOID*)&pEventSink);
    if (SUCCEEDED(hr))
    {
        /* FIXME: We should wait that all audio data has been played */
        hr = IMediaEventSink_Notify(pEventSink, EC_COMPLETE, S_OK, 0);
        IMediaEventSink_Release(pEventSink);
    }

    return hr;
}

static const IPinVtbl DSoundRender_InputPin_Vtbl = 
{
    InputPin_QueryInterface,
    IPinImpl_AddRef,
    InputPin_Release,
    InputPin_Connect,
    InputPin_ReceiveConnection,
    IPinImpl_Disconnect,
    IPinImpl_ConnectedTo,
    IPinImpl_ConnectionMediaType,
    IPinImpl_QueryPinInfo,
    IPinImpl_QueryDirection,
    IPinImpl_QueryId,
    IPinImpl_QueryAccept,
    IPinImpl_EnumMediaTypes,
    IPinImpl_QueryInternalConnections,
    DSoundRender_InputPin_EndOfStream,
    InputPin_BeginFlush,
    InputPin_EndFlush,
    InputPin_NewSegment
};

static const IMemInputPinVtbl MemInputPin_Vtbl = 
{
    MemInputPin_QueryInterface,
    MemInputPin_AddRef,
    MemInputPin_Release,
    MemInputPin_GetAllocator,
    MemInputPin_NotifyAllocator,
    MemInputPin_GetAllocatorRequirements,
    MemInputPin_Receive,
    MemInputPin_ReceiveMultiple,
    MemInputPin_ReceiveCanBlock
};

/*** IUnknown methods ***/
static HRESULT WINAPI Basicaudio_QueryInterface(IBasicAudio *iface,
						REFIID riid,
						LPVOID*ppvObj) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%s (%p), %p)\n", This, iface, debugstr_guid(riid), riid, ppvObj);

    return DSoundRender_QueryInterface((IBaseFilter*)This, riid, ppvObj);
}

static ULONG WINAPI Basicaudio_AddRef(IBasicAudio *iface) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->()\n", This, iface);

    return DSoundRender_AddRef((IBaseFilter*)This);
}

static ULONG WINAPI Basicaudio_Release(IBasicAudio *iface) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->()\n", This, iface);

    return DSoundRender_Release((IBaseFilter*)This);
}

/*** IDispatch methods ***/
static HRESULT WINAPI Basicaudio_GetTypeInfoCount(IBasicAudio *iface,
						  UINT*pctinfo) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%p): stub !!!\n", This, iface, pctinfo);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_GetTypeInfo(IBasicAudio *iface,
					     UINT iTInfo,
					     LCID lcid,
					     ITypeInfo**ppTInfo) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%d, %ld, %p): stub !!!\n", This, iface, iTInfo, lcid, ppTInfo);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_GetIDsOfNames(IBasicAudio *iface,
					       REFIID riid,
					       LPOLESTR*rgszNames,
					       UINT cNames,
					       LCID lcid,
					       DISPID*rgDispId) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%s (%p), %p, %d, %ld, %p): stub !!!\n", This, iface, debugstr_guid(riid), riid, rgszNames, cNames, lcid, rgDispId);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_Invoke(IBasicAudio *iface,
					DISPID dispIdMember,
					REFIID riid,
					LCID lcid,
					WORD wFlags,
					DISPPARAMS*pDispParams,
					VARIANT*pVarResult,
					EXCEPINFO*pExepInfo,
					UINT*puArgErr) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%ld, %s (%p), %ld, %04x, %p, %p, %p, %p): stub !!!\n", This, iface, dispIdMember, debugstr_guid(riid), riid, lcid, wFlags, pDispParams, pVarResult, pExepInfo, puArgErr);

    return S_OK;
}

/*** IBasicAudio methods ***/
static HRESULT WINAPI Basicaudio_put_Volume(IBasicAudio *iface,
					    long lVolume) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%ld): stub !!!\n", This, iface, lVolume);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_get_Volume(IBasicAudio *iface,
					    long *plVolume) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%p): stub !!!\n", This, iface, plVolume);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_put_Balance(IBasicAudio *iface,
					     long lBalance) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%ld): stub !!!\n", This, iface, lBalance);

    return S_OK;
}

static HRESULT WINAPI Basicaudio_get_Balance(IBasicAudio *iface,
					     long *plBalance) {
    ICOM_THIS_MULTI(DSoundRenderImpl, IBasicAudio_vtbl, iface);

    TRACE("(%p/%p)->(%p): stub !!!\n", This, iface, plBalance);

    return S_OK;
}

static const IBasicAudioVtbl IBasicAudio_Vtbl =
{
    Basicaudio_QueryInterface,
    Basicaudio_AddRef,
    Basicaudio_Release,
    Basicaudio_GetTypeInfoCount,
    Basicaudio_GetTypeInfo,
    Basicaudio_GetIDsOfNames,
    Basicaudio_Invoke,
    Basicaudio_put_Volume,
    Basicaudio_get_Volume,
    Basicaudio_put_Balance,
    Basicaudio_get_Balance
};
