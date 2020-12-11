/*
 * Implementation of IAMMultiMediaStream Interface
 *
 * Copyright 2004 Christian Costa
 *
 * This file contains the (internal) driver registration functions,
 * driver enumeration APIs and DirectDraw creation functions.
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
#include "wine/debug.h"

#define COBJMACROS

#include "winbase.h"
#include "wingdi.h"

#include "amstream_private.h"
#include "amstream.h"

WINE_DEFAULT_DEBUG_CHANNEL(amstream);

typedef struct {
    IAMMultiMediaStream lpVtbl;
    LONG ref;
    IGraphBuilder* pFilterGraph;
    ULONG nbStreams;
    IMediaStream** pStreams;
    STREAM_TYPE StreamType;
} IAMMultiMediaStreamImpl;

static const struct IAMMultiMediaStreamVtbl AM_Vtbl;

HRESULT AM_create(IUnknown *pUnkOuter, LPVOID *ppObj)
{
    IAMMultiMediaStreamImpl* object; 

    TRACE("(%p,%p)\n", pUnkOuter, ppObj);

    if( pUnkOuter )
        return CLASS_E_NOAGGREGATION;

    object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IAMMultiMediaStreamImpl));

    object->lpVtbl.lpVtbl = &AM_Vtbl;
    object->ref = 1;

    *ppObj = object;

    return S_OK;
}

/*** IUnknown methods ***/
static HRESULT WINAPI IAMMultiMediaStreamImpl_QueryInterface(IAMMultiMediaStream* iface, REFIID riid, void** ppvObject)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%s,%p)\n", iface, This, debugstr_guid(riid), ppvObject);

    if (IsEqualGUID(riid, &IID_IUnknown) ||
        IsEqualGUID(riid, &IID_IAMMultiMediaStream))
    {
        IClassFactory_AddRef(iface);
        *ppvObject = This;
        return S_OK;
    }

    ERR("(%p)->(%s,%p),not found\n",This,debugstr_guid(riid),ppvObject);

    return E_NOINTERFACE;
}

static ULONG WINAPI IAMMultiMediaStreamImpl_AddRef(IAMMultiMediaStream* iface)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    TRACE("(%p/%p)\n", iface, This);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI IAMMultiMediaStreamImpl_Release(IAMMultiMediaStream* iface)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p/%p)\n", iface, This);

    if (!ref)
        HeapFree(GetProcessHeap(), 0, This);

    return ref;
}

/*** IMultiMediaStream methods ***/
static HRESULT WINAPI IAMMultiMediaStreamImpl_GetInformation(IAMMultiMediaStream* iface, char* pdwFlags, STREAM_TYPE* pStreamType)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p,%p) stub!\n", This, iface, pdwFlags, pStreamType); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetMediaStream(IAMMultiMediaStream* iface, REFMSPID idPurpose, IMediaStream** ppMediaStream)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;
    MSPID PurposeId;
    int i;

    TRACE("(%p/%p)->(%p,%p)\n", This, iface, idPurpose, ppMediaStream);

    for (i = 0; i < This->nbStreams; i++)
    {
        IMediaStream_GetInformation(This->pStreams[i], &PurposeId, NULL);
        if (IsEqualIID(&PurposeId, idPurpose))
        {
            *ppMediaStream = This->pStreams[i];
            IMediaStream_AddRef(*ppMediaStream);
            return S_OK;
        }
    }

    return MS_E_NOSTREAM;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_EnumMediaStreams(IAMMultiMediaStream* iface, long Index, IMediaStream** ppMediaStream)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%ld,%p) stub!\n", This, iface, Index, ppMediaStream); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetState(IAMMultiMediaStream* iface, STREAM_STATE* pCurrentState)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, pCurrentState); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_SetState(IAMMultiMediaStream* iface, STREAM_STATE NewState)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->() stub!\n", This, iface); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetTime(IAMMultiMediaStream* iface, STREAM_TIME* pCurrentTime)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, pCurrentTime); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetDuration(IAMMultiMediaStream* iface, STREAM_TIME* pDuration)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, pDuration); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_Seek(IAMMultiMediaStream* iface, STREAM_TIME SeekTime)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->() stub!\n", This, iface); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetEndOfStream(IAMMultiMediaStream* iface, HANDLE* phEOS)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, phEOS); 

    return E_NOTIMPL;
}

/*** IAMMultiMediaStream methods ***/
static HRESULT WINAPI IAMMultiMediaStreamImpl_Initialize(IAMMultiMediaStream* iface, STREAM_TYPE StreamType, DWORD dwFlags, IGraphBuilder* pFilterGraph)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;
    HRESULT hr = S_OK;

    FIXME("(%p/%p)->(%lx,%lx,%p) partial stub!\n", This, iface, (DWORD)StreamType, dwFlags, pFilterGraph); 

    if (pFilterGraph)
    {
        This->pFilterGraph = pFilterGraph;
        IGraphBuilder_AddRef(This->pFilterGraph);
    }
    else
    {
        hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IGraphBuilder, (LPVOID*)&This->pFilterGraph);
    }

    if (SUCCEEDED(hr))
    {
        This->StreamType = StreamType;
    }

    return hr;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetFilterGraph(IAMMultiMediaStream* iface, IGraphBuilder** ppGraphBuilder)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, ppGraphBuilder); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_GetFilter(IAMMultiMediaStream* iface, IMediaStreamFilter** ppFilter)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p) stub!\n", This, iface, ppFilter); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_AddMediaStream(IAMMultiMediaStream* iface, IUnknown* pStreamObject, const MSPID* PurposeId,
                                          DWORD dwFlags, IMediaStream** ppNewStream)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;
    HRESULT hr;
    IMediaStream* pStream;
    IMediaStream** pNewStreams;

    FIXME("(%p/%p)->(%p,%p,%lx,%p) partial stub!\n", This, iface, pStreamObject, PurposeId, dwFlags, ppNewStream); 

    hr = MediaStream_create((IMultiMediaStream*)iface, PurposeId, This->StreamType, &pStream);
    if (SUCCEEDED(hr))
    {
        pNewStreams = (IMediaStream**)CoTaskMemAlloc((This->nbStreams+1)*sizeof(IMediaStream*));
        if (!pNewStreams)
        {
            IMediaStream_Release(pStream);
            return E_OUTOFMEMORY;
        }
        if (This->nbStreams)
            CopyMemory(pNewStreams, This->pStreams, This->nbStreams*sizeof(IMediaStream*));
        CoTaskMemFree(This->pStreams);
        This->pStreams = pNewStreams;
        This->pStreams[This->nbStreams] = pStream;
        This->nbStreams++;

        if (ppNewStream)
            *ppNewStream = pStream;
    }

    return hr;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_OpenFile(IAMMultiMediaStream* iface, LPCWSTR pszFileName, DWORD dwFlags)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%s,%lx) stub!\n", This, iface, debugstr_w(pszFileName), dwFlags);

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_OpenMoniker(IAMMultiMediaStream* iface, IBindCtx* pCtx, IMoniker* pMoniker, DWORD dwFlags)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%p,%p,%lx) stub!\n", This, iface, pCtx, pMoniker, dwFlags); 

    return E_NOTIMPL;
}

static HRESULT WINAPI IAMMultiMediaStreamImpl_Render(IAMMultiMediaStream* iface, DWORD dwFlags)
{
    IAMMultiMediaStreamImpl *This = (IAMMultiMediaStreamImpl *)iface;

    FIXME("(%p/%p)->(%lx) stub!\n", This, iface, dwFlags); 

    return E_NOTIMPL;
}

static const IAMMultiMediaStreamVtbl AM_Vtbl =
{
    IAMMultiMediaStreamImpl_QueryInterface,
    IAMMultiMediaStreamImpl_AddRef,
    IAMMultiMediaStreamImpl_Release,
    IAMMultiMediaStreamImpl_GetInformation,
    IAMMultiMediaStreamImpl_GetMediaStream,
    IAMMultiMediaStreamImpl_EnumMediaStreams,
    IAMMultiMediaStreamImpl_GetState,
    IAMMultiMediaStreamImpl_SetState,
    IAMMultiMediaStreamImpl_GetTime,
    IAMMultiMediaStreamImpl_GetDuration,
    IAMMultiMediaStreamImpl_Seek,
    IAMMultiMediaStreamImpl_GetEndOfStream,
    IAMMultiMediaStreamImpl_Initialize,
    IAMMultiMediaStreamImpl_GetFilterGraph,
    IAMMultiMediaStreamImpl_GetFilter,
    IAMMultiMediaStreamImpl_AddMediaStream,
    IAMMultiMediaStreamImpl_OpenFile,
    IAMMultiMediaStreamImpl_OpenMoniker,
    IAMMultiMediaStreamImpl_Render
};
