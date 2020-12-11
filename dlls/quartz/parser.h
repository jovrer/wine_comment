/*
 * Parser declarations
 *
 * Copyright 2005 Christian Costa
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

typedef struct ParserImpl ParserImpl;

typedef HRESULT (*PFN_PROCESS_SAMPLE) (LPVOID iface, IMediaSample * pSample);
typedef HRESULT (*PFN_QUERY_ACCEPT) (LPVOID iface, const AM_MEDIA_TYPE * pmt);
typedef HRESULT (*PFN_PRE_CONNECT) (IPin * iface, IPin * pConnectPin);

struct ParserImpl
{
    const IBaseFilterVtbl * lpVtbl;

    LONG refCount;
    CRITICAL_SECTION csFilter;
    FILTER_STATE state;
    REFERENCE_TIME rtStreamStart;
    IReferenceClock * pClock;
    FILTER_INFO filterInfo;
    CLSID clsid;

    PullPin * pInputPin;
    IPin ** ppPins;
    ULONG cStreams;
};

typedef struct Parser_OutputPin
{
    OutputPin pin;

    AM_MEDIA_TYPE * pmt;
    float fSamplesPerSec;
    DWORD dwSamplesProcessed;
    DWORD dwSampleSize;
    DWORD dwLength;
    MediaSeekingImpl mediaSeeking;
} Parser_OutputPin;

HRESULT Parser_AddPin(ParserImpl * This, PIN_INFO * piOutput, ALLOCATOR_PROPERTIES * props, AM_MEDIA_TYPE * amt, float fSamplesPerSec, DWORD dwSampleSize, DWORD dwLength);
HRESULT Parser_Create(ParserImpl*, const CLSID*, PFN_PROCESS_SAMPLE, PFN_QUERY_ACCEPT, PFN_PRE_CONNECT);
