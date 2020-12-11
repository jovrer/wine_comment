/*
 * Qcap main header file
 *
 * Copyright (C) 2005 Rolf Kalbermatter
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
#ifndef _QCAP_MAIN_H_DEFINED
#define _QCAP_MAIN_H_DEFINED

extern DWORD ObjectRefCount(BOOL increment);

extern IUnknown * WINAPI QCAP_createAudioCaptureFilter(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createAVICompressor(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createVFWCaptureFilter(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createVFWCaptureFilterPropertyPage(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createAVImux(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createAVImuxPropertyPage(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createAVImuxPropertyPage1(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createFileWriter(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createCaptureGraphBuilder2(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createInfinitePinTeeFilter(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createSmartTeeFilter(IUnknown *pUnkOuter, HRESULT *phr);
extern IUnknown * WINAPI QCAP_createAudioInputMixerPropertyPage(IUnknown *pUnkOuter, HRESULT *phr);

typedef struct tagENUMPINDETAILS
{
       ULONG cPins;
       IPin ** ppPins;
} ENUMPINDETAILS;

typedef struct tagENUMEDIADETAILS
{
       ULONG cMediaTypes;
       AM_MEDIA_TYPE * pMediaTypes;
} ENUMMEDIADETAILS;

HRESULT IEnumPinsImpl_Construct(const ENUMPINDETAILS * pDetails, IEnumPins ** ppEnum);
HRESULT IEnumMediaTypesImpl_Construct(const ENUMMEDIADETAILS * pDetails, IEnumMediaTypes ** ppEnum);

HRESULT CopyMediaType(AM_MEDIA_TYPE * pDest, const AM_MEDIA_TYPE *pSrc);
void FreeMediaType(AM_MEDIA_TYPE * pmt);
void DeleteMediaType(AM_MEDIA_TYPE * pmt);
BOOL CompareMediaTypes(const AM_MEDIA_TYPE * pmt1, const AM_MEDIA_TYPE * pmt2, BOOL bWildcards); 
void dump_AM_MEDIA_TYPE(const AM_MEDIA_TYPE * pmt);

enum YUV_Format {
    /* Last 2 numbers give the skip info, the smaller they are the better
     * Planar:
     *      HSKIP : VSKIP */
    YUVP_421, /*  2 : 1 */
    YUVP_422, /*  2 : 2 */
    YUVP_441, /*  4 : 1 */
    YUVP_444, /*  4 : 4 */
    ENDPLANAR, /* No format, just last planar item so we can check on it */

    /* Non-planar */
    YUYV, /* Order: YUYV (Guess why it's named like that) */
    UYVY, /* Order: UYVY (Looks like someone got bored and swapped the Y's) */
    UYYVYY, /* YUV411 linux style, perhaps YUV420 is YYUYYV? */
};

void YUV_Init(void);
void YUV_To_RGB24(enum YUV_Format format, unsigned char *target, const unsigned char *source, int width, int height);

#endif /* _QCAP_MAIN_H_DEFINED */
