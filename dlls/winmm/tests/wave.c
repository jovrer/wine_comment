/*
 * Test winmm sound playback in each sound format
 *
 * Copyright (c) 2002 Francois Gouget
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "wine/test.h"
#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "mmsystem.h"
#define NOBITMAP
#include "mmreg.h"
#include "ks.h"
#include "ksguid.h"
#include "ksmedia.h"

#include "winmm_test.h"

/*
 * Note that in most of this test we may get MMSYSERR_BADDEVICEID errors
 * at about any time if the user starts another application that uses the
 * sound device. So we should not report these as test failures.
 *
 * This test can play a test tone. But this only makes sense if someone
 * is going to carefully listen to it, and would only bother everyone else.
 * So this is only done if the test is being run in interactive mode.
 */

#define PI 3.14159265358979323846
static char* wave_generate_la(WAVEFORMATEX* wfx, double duration, DWORD* size)
{
    int i,j;
    int nb_samples;
    char* buf;
    char* b;
    WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE*)wfx;

    nb_samples=(int)(duration*wfx->nSamplesPerSec);
    *size=nb_samples*wfx->nBlockAlign;
    b=buf=malloc(*size);
    for (i=0;i<nb_samples;i++) {
        double y=sin(440.0*2*PI*i/wfx->nSamplesPerSec);
        if (wfx->wBitsPerSample==8) {
            unsigned char sample=(unsigned char)((double)127.5*(y+1.0));
            for (j = 0; j < wfx->nChannels; j++)
                *b++=sample;
        } else if (wfx->wBitsPerSample==16) {
            signed short sample=(signed short)((double)32767.5*y-0.5);
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=sample & 0xff;
                b[1]=sample >> 8;
                b+=2;
            }
        } else if (wfx->wBitsPerSample==24) {
            signed int sample=(signed int)(((double)0x7fffff+0.5)*y-0.5);
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=sample & 0xff;
                b[1]=(sample >> 8) & 0xff;
                b[2]=(sample >> 16) & 0xff;
                b+=3;
            }
        } else if ((wfx->wBitsPerSample==32) && ((wfx->wFormatTag == WAVE_FORMAT_PCM) ||
            ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) &&
            IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)))) {
            signed int sample=(signed int)(((double)0x7fffffff+0.5)*y-0.5);
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=sample & 0xff;
                b[1]=(sample >> 8) & 0xff;
                b[2]=(sample >> 16) & 0xff;
                b[3]=(sample >> 24) & 0xff;
                b+=4;
            }
        } else if ((wfx->wBitsPerSample==32) && (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) &&
            IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            union { float f; char c[4]; } sample;
            sample.f=(float)y;
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=sample.c[0];
                b[1]=sample.c[1];
                b[2]=sample.c[2];
                b[3]=sample.c[3];
                b+=4;
            }
        }
    }
    return buf;
}

static char* wave_generate_silence(WAVEFORMATEX* wfx, double duration, DWORD* size)
{
    int i,j;
    int nb_samples;
    char* buf;
    char* b;
    WAVEFORMATEXTENSIBLE *wfex = (WAVEFORMATEXTENSIBLE*)wfx;

    nb_samples=(int)(duration*wfx->nSamplesPerSec);
    *size=nb_samples*wfx->nBlockAlign;
    b=buf=malloc(*size);
    for (i=0;i<nb_samples;i++) {
        if (wfx->wBitsPerSample==8) {
            for (j = 0; j < wfx->nChannels; j++)
                *b++=128;
        } else if (wfx->wBitsPerSample==16) {
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=0;
                b[1]=0;
                b+=2;
            }
        } else if (wfx->wBitsPerSample==24) {
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=0;
                b[1]=0;
                b[2]=0;
                b+=3;
            }
        } else if ((wfx->wBitsPerSample==32) && ((wfx->wFormatTag == WAVE_FORMAT_PCM) ||
            ((wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) &&
            IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM)))) {
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=0;
                b[1]=0;
                b[2]=0;
                b[3]=0;
                b+=4;
            }
        } else if ((wfx->wBitsPerSample==32) && (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) &&
            IsEqualGUID(&wfex->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            union { float f; char c[4]; } sample;
            sample.f=0;
            for (j = 0; j < wfx->nChannels; j++) {
                b[0]=sample.c[0];
                b[1]=sample.c[1];
                b[2]=sample.c[2];
                b[3]=sample.c[3];
                b+=4;
            }
        }
    }
    return buf;
}

const char * dev_name(int device)
{
    static char name[16];
    if (device == WAVE_MAPPER)
        return "WAVE_MAPPER";
    sprintf(name, "%d", device);
    return name;
}

const char* mmsys_error(MMRESULT error)
{
#define ERR_TO_STR(dev) case dev: return #dev
    static char	unknown[32];
    switch (error) {
    ERR_TO_STR(MMSYSERR_NOERROR);
    ERR_TO_STR(MMSYSERR_ERROR);
    ERR_TO_STR(MMSYSERR_BADDEVICEID);
    ERR_TO_STR(MMSYSERR_NOTENABLED);
    ERR_TO_STR(MMSYSERR_ALLOCATED);
    ERR_TO_STR(MMSYSERR_INVALHANDLE);
    ERR_TO_STR(MMSYSERR_NODRIVER);
    ERR_TO_STR(MMSYSERR_NOMEM);
    ERR_TO_STR(MMSYSERR_NOTSUPPORTED);
    ERR_TO_STR(MMSYSERR_BADERRNUM);
    ERR_TO_STR(MMSYSERR_INVALFLAG);
    ERR_TO_STR(MMSYSERR_INVALPARAM);
    ERR_TO_STR(WAVERR_BADFORMAT);
    ERR_TO_STR(WAVERR_STILLPLAYING);
    ERR_TO_STR(WAVERR_UNPREPARED);
    ERR_TO_STR(WAVERR_SYNC);
    ERR_TO_STR(MIDIERR_UNPREPARED);
    ERR_TO_STR(MIDIERR_STILLPLAYING);
    ERR_TO_STR(MIDIERR_NOTREADY);
    ERR_TO_STR(MIDIERR_NODEVICE);
    ERR_TO_STR(MIDIERR_INVALIDSETUP);
    ERR_TO_STR(TIMERR_NOCANDO);
    ERR_TO_STR(TIMERR_STRUCT);
    ERR_TO_STR(JOYERR_PARMS);
    ERR_TO_STR(JOYERR_NOCANDO);
    ERR_TO_STR(JOYERR_UNPLUGGED);
    ERR_TO_STR(MIXERR_INVALLINE);
    ERR_TO_STR(MIXERR_INVALCONTROL);
    ERR_TO_STR(MIXERR_INVALVALUE);
    ERR_TO_STR(MMIOERR_FILENOTFOUND);
    ERR_TO_STR(MMIOERR_OUTOFMEMORY);
    ERR_TO_STR(MMIOERR_CANNOTOPEN);
    ERR_TO_STR(MMIOERR_CANNOTCLOSE);
    ERR_TO_STR(MMIOERR_CANNOTREAD);
    ERR_TO_STR(MMIOERR_CANNOTWRITE);
    ERR_TO_STR(MMIOERR_CANNOTSEEK);
    ERR_TO_STR(MMIOERR_CANNOTEXPAND);
    ERR_TO_STR(MMIOERR_CHUNKNOTFOUND);
    ERR_TO_STR(MMIOERR_UNBUFFERED);
    }
    sprintf(unknown, "Unknown(0x%08x)", error);
    return unknown;
#undef ERR_TO_STR
}

const char* wave_out_error(MMRESULT error)
{
    static char msg[1024];
    static char long_msg[1100];
    MMRESULT rc;

    rc = waveOutGetErrorText(error, msg, sizeof(msg));
    if (rc != MMSYSERR_NOERROR)
        sprintf(long_msg, "waveOutGetErrorText(%x) failed with error %x",
                error, rc);
    else
        sprintf(long_msg, "%s(%s)", mmsys_error(error), msg);
    return long_msg;
}

const char * wave_open_flags(DWORD flags)
{
    static char msg[1024];
    int first = TRUE;
    msg[0] = 0;
    if ((flags & CALLBACK_TYPEMASK) == CALLBACK_EVENT) {
        strcat(msg, "CALLBACK_EVENT");
        first = FALSE;
    }
    if ((flags & CALLBACK_TYPEMASK) == CALLBACK_FUNCTION) {
        if (!first) strcat(msg, "|");
        strcat(msg, "CALLBACK_FUNCTION");
        first = FALSE;
    }
    if ((flags & CALLBACK_TYPEMASK) == CALLBACK_NULL) {
        if (!first) strcat(msg, "|");
        strcat(msg, "CALLBACK_NULL");
        first = FALSE;
    }
    if ((flags & CALLBACK_TYPEMASK) == CALLBACK_THREAD) {
        if (!first) strcat(msg, "|");
        strcat(msg, "CALLBACK_THREAD");
        first = FALSE;
    }
    if ((flags & CALLBACK_TYPEMASK) == CALLBACK_WINDOW) {
        if (!first) strcat(msg, "|");
        strcat(msg, "CALLBACK_WINDOW");
        first = FALSE;
    }
    if ((flags & WAVE_ALLOWSYNC) == WAVE_ALLOWSYNC) {
        if (!first) strcat(msg, "|");
        strcat(msg, "WAVE_ALLOWSYNC");
        first = FALSE;
    }
    if ((flags & WAVE_FORMAT_DIRECT) == WAVE_FORMAT_DIRECT) {
        if (!first) strcat(msg, "|");
        strcat(msg, "WAVE_FORMAT_DIRECT");
        first = FALSE;
    }
    if ((flags & WAVE_FORMAT_QUERY) == WAVE_FORMAT_QUERY) {
        if (!first) strcat(msg, "|");
        strcat(msg, "WAVE_FORMAT_QUERY");
        first = FALSE;
    }
    if ((flags & WAVE_MAPPED) == WAVE_MAPPED) {
        if (!first) strcat(msg, "|");
        strcat(msg, "WAVE_MAPPED");
        first = FALSE;
    }
    return msg;
}

static const char * wave_out_caps(DWORD dwSupport)
{
#define ADD_FLAG(f) if (dwSupport & f) strcat(msg, " " #f)
    static char msg[256];
    msg[0] = 0;

    ADD_FLAG(WAVECAPS_PITCH);
    ADD_FLAG(WAVECAPS_PLAYBACKRATE);
    ADD_FLAG(WAVECAPS_VOLUME);
    ADD_FLAG(WAVECAPS_LRVOLUME);
    ADD_FLAG(WAVECAPS_SYNC);
    ADD_FLAG(WAVECAPS_SAMPLEACCURATE);

    return msg[0] ? msg + 1 : "";
#undef ADD_FLAG
}

const char * wave_time_format(UINT type)
{
    static char msg[32];
#define TIME_FORMAT(f) case f: return #f
    switch (type) {
    TIME_FORMAT(TIME_MS);
    TIME_FORMAT(TIME_SAMPLES);
    TIME_FORMAT(TIME_BYTES);
    TIME_FORMAT(TIME_SMPTE);
    TIME_FORMAT(TIME_MIDI);
    TIME_FORMAT(TIME_TICKS);
    }
#undef TIME_FORMAT
    sprintf(msg, "Unknown(0x%04x)", type);
    return msg;
}

const char * get_format_str(WORD format)
{
    static char msg[32];
#define WAVE_FORMAT(f) case f: return #f
    switch (format) {
    WAVE_FORMAT(WAVE_FORMAT_PCM);
    WAVE_FORMAT(WAVE_FORMAT_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_IBM_CVSD);
    WAVE_FORMAT(WAVE_FORMAT_ALAW);
    WAVE_FORMAT(WAVE_FORMAT_MULAW);
    WAVE_FORMAT(WAVE_FORMAT_OKI_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_IMA_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_MEDIASPACE_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_SIERRA_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_G723_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_DIGISTD);
    WAVE_FORMAT(WAVE_FORMAT_DIGIFIX);
    WAVE_FORMAT(WAVE_FORMAT_DIALOGIC_OKI_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_YAMAHA_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_SONARC);
    WAVE_FORMAT(WAVE_FORMAT_DSPGROUP_TRUESPEECH);
    WAVE_FORMAT(WAVE_FORMAT_ECHOSC1);
    WAVE_FORMAT(WAVE_FORMAT_AUDIOFILE_AF36);
    WAVE_FORMAT(WAVE_FORMAT_APTX);
    WAVE_FORMAT(WAVE_FORMAT_AUDIOFILE_AF10);
    WAVE_FORMAT(WAVE_FORMAT_DOLBY_AC2);
    WAVE_FORMAT(WAVE_FORMAT_GSM610);
    WAVE_FORMAT(WAVE_FORMAT_ANTEX_ADPCME);
    WAVE_FORMAT(WAVE_FORMAT_CONTROL_RES_VQLPC);
    WAVE_FORMAT(WAVE_FORMAT_DIGIREAL);
    WAVE_FORMAT(WAVE_FORMAT_DIGIADPCM);
    WAVE_FORMAT(WAVE_FORMAT_CONTROL_RES_CR10);
    WAVE_FORMAT(WAVE_FORMAT_NMS_VBXADPCM);
    WAVE_FORMAT(WAVE_FORMAT_G721_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_MPEG);
    WAVE_FORMAT(WAVE_FORMAT_MPEGLAYER3);
    WAVE_FORMAT(WAVE_FORMAT_CREATIVE_ADPCM);
    WAVE_FORMAT(WAVE_FORMAT_CREATIVE_FASTSPEECH8);
    WAVE_FORMAT(WAVE_FORMAT_CREATIVE_FASTSPEECH10);
    WAVE_FORMAT(WAVE_FORMAT_FM_TOWNS_SND);
    WAVE_FORMAT(WAVE_FORMAT_OLIGSM);
    WAVE_FORMAT(WAVE_FORMAT_OLIADPCM);
    WAVE_FORMAT(WAVE_FORMAT_OLICELP);
    WAVE_FORMAT(WAVE_FORMAT_OLISBC);
    WAVE_FORMAT(WAVE_FORMAT_OLIOPR);
    WAVE_FORMAT(WAVE_FORMAT_DEVELOPMENT);
    WAVE_FORMAT(WAVE_FORMAT_EXTENSIBLE);
    }
#undef WAVE_FORMAT
    sprintf(msg, "Unknown(0x%04x)", format);
    return msg;
}

DWORD bytes_to_samples(DWORD bytes, LPWAVEFORMATEX pwfx)
{
    return bytes / pwfx->nBlockAlign;
}

DWORD bytes_to_ms(DWORD bytes, LPWAVEFORMATEX pwfx)
{
    return bytes_to_samples(bytes, pwfx) * 1000 / pwfx->nSamplesPerSec;
}

DWORD time_to_bytes(LPMMTIME mmtime, LPWAVEFORMATEX pwfx)
{
    if (mmtime->wType == TIME_BYTES)
        return mmtime->u.cb;
    else if (mmtime->wType == TIME_SAMPLES)
        return mmtime->u.sample * pwfx->nBlockAlign;
    else if (mmtime->wType == TIME_MS)
        return mmtime->u.ms * pwfx->nAvgBytesPerSec / 1000;
    else if (mmtime->wType == TIME_SMPTE)
        return ((mmtime->u.smpte.hour * 60.0 * 60.0) +
                (mmtime->u.smpte.min * 60.0) +
                (mmtime->u.smpte.sec) +
                (mmtime->u.smpte.frame / 30.0)) * pwfx->nAvgBytesPerSec;

    trace("FIXME: time_to_bytes() type not supported\n");
    return -1;
}

static void check_position(int device, HWAVEOUT wout, DWORD bytes,
                           LPWAVEFORMATEX pwfx )
{
    MMTIME mmtime;
    DWORD samples;
    double duration;
    MMRESULT rc;
    DWORD returned;

    samples=bytes/(pwfx->wBitsPerSample/8*pwfx->nChannels);
    duration=((double)samples)/pwfx->nSamplesPerSec;

    mmtime.wType = TIME_BYTES;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_BYTES && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_BYTES not supported, returned %s\n",
              dev_name(device),wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): returned %ld bytes, "
       "should be %ld\n", dev_name(device), returned, bytes);

    mmtime.wType = TIME_SAMPLES;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_SAMPLES && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_SAMPLES not supported, "
              "returned %s\n",dev_name(device),wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): returned %ld samples, "
       "should be %ld\n", dev_name(device), bytes_to_samples(returned, pwfx),
       bytes_to_samples(bytes, pwfx));

    mmtime.wType = TIME_MS;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_MS && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_MS not supported, returned %s\n",
              dev_name(device), wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): returned %ld ms, "
       "should be %ld\n", dev_name(device), bytes_to_ms(returned, pwfx),
       bytes_to_ms(bytes, pwfx));

    mmtime.wType = TIME_SMPTE;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_SMPTE && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_SMPTE not supported, returned %s\n",
              dev_name(device),wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): SMPTE test failed\n",
       dev_name(device));

    mmtime.wType = TIME_MIDI;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_MIDI && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_MIDI not supported, returned %s\n",
              dev_name(device),wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): MIDI test failed\n",
       dev_name(device));

    mmtime.wType = TIME_TICKS;
    rc=waveOutGetPosition(wout, &mmtime, sizeof(mmtime));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetPosition(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
    if (mmtime.wType != TIME_TICKS && winetest_debug > 1)
        trace("waveOutGetPosition(%s): TIME_TICKS not supported, returned %s\n",
              dev_name(device),wave_time_format(mmtime.wType));
    returned = time_to_bytes(&mmtime, pwfx);
    ok(returned == bytes, "waveOutGetPosition(%s): TICKS test failed\n",
       dev_name(device));
}

static void wave_out_test_deviceOut(int device, double duration,
                                    LPWAVEFORMATEX pwfx, DWORD format,
                                    DWORD flags, LPWAVEOUTCAPS pcaps,
                                    BOOL interactive, BOOL sine, BOOL pause)
{
    HWAVEOUT wout;
    HANDLE hevent;
    WAVEHDR frag;
    MMRESULT rc;
    DWORD volume;
    WORD nChannels = pwfx->nChannels;
    WORD wBitsPerSample = pwfx->wBitsPerSample;
    DWORD nSamplesPerSec = pwfx->nSamplesPerSec;
    BOOL has_volume = pcaps->dwSupport & WAVECAPS_VOLUME;
    double paused = 0.0;
    double actual;

    hevent=CreateEvent(NULL,FALSE,FALSE,NULL);
    ok(hevent!=NULL,"CreateEvent(): error=%ld\n",GetLastError());
    if (hevent==NULL)
        return;

    wout=NULL;
    rc=waveOutOpen(&wout,device,pwfx,(DWORD)hevent,0,CALLBACK_EVENT|flags);
    /* Note: Win9x doesn't know WAVE_FORMAT_DIRECT */
    /* It is acceptable to fail on formats that are not specified to work */
    ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_BADDEVICEID ||
       rc==MMSYSERR_NOTENABLED || rc==MMSYSERR_NODRIVER ||
       rc==MMSYSERR_ALLOCATED ||
       ((rc==WAVERR_BADFORMAT || rc==MMSYSERR_NOTSUPPORTED) &&
       (flags & WAVE_FORMAT_DIRECT) && !(pcaps->dwFormats & format)) ||
       ((rc==WAVERR_BADFORMAT || rc==MMSYSERR_NOTSUPPORTED) &&
       (!(flags & WAVE_FORMAT_DIRECT) || (flags & WAVE_MAPPED)) &&
       !(pcaps->dwFormats & format)) ||
       (rc==MMSYSERR_INVALFLAG && (flags & WAVE_FORMAT_DIRECT)),
       "waveOutOpen(%s): format=%ldx%2dx%d flags=%lx(%s) rc=%s\n",
       dev_name(device),pwfx->nSamplesPerSec,pwfx->wBitsPerSample,
       pwfx->nChannels,CALLBACK_EVENT|flags,
       wave_open_flags(CALLBACK_EVENT|flags),wave_out_error(rc));
    if ((rc==WAVERR_BADFORMAT || rc==MMSYSERR_NOTSUPPORTED) &&
       (flags & WAVE_FORMAT_DIRECT) && (pcaps->dwFormats & format))
        trace(" Reason: The device lists this format as supported in it's "
              "capabilities but opening it failed.\n");
    if ((rc==WAVERR_BADFORMAT || rc==MMSYSERR_NOTSUPPORTED) &&
       !(pcaps->dwFormats & format))
        trace("waveOutOpen(%s): format=%ldx%2dx%d %s rc=%s failed but format "
              "not supported so OK.\n", dev_name(device), pwfx->nSamplesPerSec,
              pwfx->wBitsPerSample,pwfx->nChannels,
              flags & WAVE_FORMAT_DIRECT ? "flags=WAVE_FORMAT_DIRECT" :
              flags & WAVE_MAPPED ? "flags=WAVE_MAPPED" : "", mmsys_error(rc));
    if (rc!=MMSYSERR_NOERROR) {
        CloseHandle(hevent);
        return;
    }

    ok(pwfx->nChannels==nChannels &&
       pwfx->wBitsPerSample==wBitsPerSample &&
       pwfx->nSamplesPerSec==nSamplesPerSec,
       "got the wrong format: %ldx%2dx%d instead of %ldx%2dx%d\n",
       pwfx->nSamplesPerSec, pwfx->wBitsPerSample,
       pwfx->nChannels, nSamplesPerSec, wBitsPerSample, nChannels);

    if (sine)
        frag.lpData=wave_generate_la(pwfx,duration,&frag.dwBufferLength);
    else
        frag.lpData=wave_generate_silence(pwfx,duration,&frag.dwBufferLength);

    frag.dwFlags=0;
    frag.dwLoops=0;

    rc=waveOutGetVolume(wout,&volume);
    ok(has_volume ? rc==MMSYSERR_NOERROR : rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetVolume(%s): rc=%s\n",dev_name(device),wave_out_error(rc));

    rc=waveOutPrepareHeader(wout, &frag, sizeof(frag));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutPrepareHeader(%s): rc=%s\n",dev_name(device),wave_out_error(rc));

    if (interactive && rc==MMSYSERR_NOERROR) {
        DWORD start,end;
        trace("Playing %g second %s at %5ldx%2dx%d %s %s\n",duration,
              sine ? "440Hz tone" : "silence",pwfx->nSamplesPerSec,
              pwfx->wBitsPerSample,pwfx->nChannels,
              get_format_str(pwfx->wFormatTag),
              flags & WAVE_FORMAT_DIRECT ? "WAVE_FORMAT_DIRECT" :
              flags & WAVE_MAPPED ? "WAVE_MAPPED" : "");
        if (sine && !volume)
            trace("*** Warning the sound is muted, you will not hear the test\n");

        /* Check that the position is 0 at start */
        check_position(device, wout, 0, pwfx);

        rc=waveOutSetVolume(wout,0x20002000);
        ok(has_volume ? rc==MMSYSERR_NOERROR : rc==MMSYSERR_NOTSUPPORTED,
           "waveOutSetVolume(%s): rc=%s\n",dev_name(device),wave_out_error(rc));
        WaitForSingleObject(hevent,INFINITE);

        rc=waveOutSetVolume(wout,volume);
        ok(has_volume ? rc==MMSYSERR_NOERROR : rc==MMSYSERR_NOTSUPPORTED,
           "waveOutSetVolume(%s): rc=%s\n",dev_name(device),wave_out_error(rc));

        start=GetTickCount();
        rc=waveOutWrite(wout, &frag, sizeof(frag));
        ok(rc==MMSYSERR_NOERROR,"waveOutWrite(%s): rc=%s\n",
           dev_name(device),wave_out_error(rc));

        if (pause) {
            paused = duration / 2;
            Sleep(paused * 1000);
            rc=waveOutPause(wout);
            ok(rc==MMSYSERR_NOERROR,"waveOutPause(%s): rc=%s\n",
               dev_name(device),wave_out_error(rc));
            trace("pausing for %g seconds\n", paused);
            Sleep(paused * 1000);
            rc=waveOutRestart(wout);
            ok(rc==MMSYSERR_NOERROR,"waveOutRestart(%s): rc=%s\n",
               dev_name(device),wave_out_error(rc));
        }

        WaitForSingleObject(hevent,INFINITE);

        /* Check the sound duration was between -2% and +10% of the expected value */
        end=GetTickCount();
        actual = (end - start) / 1000.0;
        if (winetest_debug > 1)
            trace("sound duration=%g ms\n",1000*actual);
        ok((actual > (0.98 * (duration+paused))) &&
           (actual < (1.1 * (duration+paused))),
           "The sound played for %g ms instead of %g ms\n",
           1000*actual,1000*(duration+paused));

        check_position(device, wout, frag.dwBufferLength, pwfx);
    }

    rc=waveOutUnprepareHeader(wout, &frag, sizeof(frag));
    ok(rc==MMSYSERR_NOERROR,
       "waveOutUnprepareHeader(%s): rc=%s\n",dev_name(device),
       wave_out_error(rc));
    free(frag.lpData);

    CloseHandle(hevent);
    rc=waveOutClose(wout);
    ok(rc==MMSYSERR_NOERROR,"waveOutClose(%s): rc=%s\n",dev_name(device),
       wave_out_error(rc));
}

static void wave_out_test_device(int device)
{
    WAVEOUTCAPSA capsA;
    WAVEOUTCAPSW capsW;
    WAVEFORMATEX format, oformat;
    WAVEFORMATEXTENSIBLE wfex;
    HWAVEOUT wout;
    MMRESULT rc;
    UINT f;
    WCHAR * nameW;
    CHAR * nameA;
    DWORD size;
    DWORD dwPageSize;
    BYTE * twoPages;
    SYSTEM_INFO sSysInfo;
    DWORD flOldProtect;
    BOOL res;

    GetSystemInfo(&sSysInfo);
    dwPageSize = sSysInfo.dwPageSize;

    rc=waveOutGetDevCapsA(device,&capsA,sizeof(capsA));
    ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_BADDEVICEID ||
       rc==MMSYSERR_NODRIVER,
       "waveOutGetDevCapsA(%s): failed to get capabilities: rc=%s\n",
       dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_BADDEVICEID || rc==MMSYSERR_NODRIVER)
        return;

    rc=waveOutGetDevCapsW(device,&capsW,sizeof(capsW));
    ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetDevCapsW(%s): MMSYSERR_NOERROR or MMSYSERR_NOTSUPPORTED "
       "expected, got %s\n",dev_name(device),wave_out_error(rc));

    rc=waveOutGetDevCapsA(device,0,sizeof(capsA));
    ok(rc==MMSYSERR_INVALPARAM,
       "waveOutGetDevCapsA(%s): MMSYSERR_INVALPARAM expected, "
       "got %s\n",dev_name(device),wave_out_error(rc));

    rc=waveOutGetDevCapsW(device,0,sizeof(capsW));
    ok(rc==MMSYSERR_INVALPARAM || rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetDevCapsW(%s): MMSYSERR_INVALPARAM or MMSYSERR_NOTSUPPORTED "
       "expected, got %s\n",dev_name(device),wave_out_error(rc));

#if 0 /* FIXME: this works on windows but crashes wine */
    rc=waveOutGetDevCapsA(device,(LPWAVEOUTCAPSA)1,sizeof(capsA));
    ok(rc==MMSYSERR_INVALPARAM,
       "waveOutGetDevCapsA(%s): MMSYSERR_INVALPARAM expected, got %s\n",
       dev_name(device),wave_out_error(rc));

    rc=waveOutGetDevCapsW(device,(LPWAVEOUTCAPSW)1,sizeof(capsW));
    ok(rc==MMSYSERR_INVALPARAM || rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetDevCapsW(%s): MMSYSERR_INVALPARAM or MMSYSERR_NOTSUPPORTED "
       "expected, got %s\n",dev_name(device),wave_out_error(rc));
#endif

    rc=waveOutGetDevCapsA(device,&capsA,4);
    ok(rc==MMSYSERR_NOERROR,
       "waveOutGetDevCapsA(%s): MMSYSERR_NOERROR expected, got %s\n",
       dev_name(device),wave_out_error(rc));

    rc=waveOutGetDevCapsW(device,&capsW,4);
    ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetDevCapsW(%s): MMSYSERR_NOERROR or MMSYSERR_NOTSUPPORTED "
       "expected, got %s\n",dev_name(device),wave_out_error(rc));

    nameA=NULL;
    rc=waveOutMessage((HWAVEOUT)device, DRV_QUERYDEVICEINTERFACESIZE,
                      (DWORD_PTR)&size, 0);
    ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_INVALPARAM ||
       rc==MMSYSERR_NOTSUPPORTED,
       "waveOutMessage(%s): failed to get interface size, rc=%s\n",
       dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        nameW = (WCHAR *)malloc(size);
        rc=waveOutMessage((HWAVEOUT)device, DRV_QUERYDEVICEINTERFACE,
                          (DWORD_PTR)nameW, size);
        ok(rc==MMSYSERR_NOERROR,"waveOutMessage(%s): failed to get interface "
           "name, rc=%s\n",dev_name(device),wave_out_error(rc));
        ok(lstrlenW(nameW)+1==size/sizeof(WCHAR),"got an incorrect size: "
           "%ld instead of %d\n",size,(lstrlenW(nameW)+1)*sizeof(WCHAR));
        if (rc==MMSYSERR_NOERROR) {
            nameA = malloc(size/sizeof(WCHAR));
            WideCharToMultiByte(CP_ACP, 0, nameW, size/sizeof(WCHAR), nameA,
                                size/sizeof(WCHAR), NULL, NULL);
        }
        free(nameW);
    }
    else if (rc==MMSYSERR_NOTSUPPORTED) {
        nameA=strdup("not supported");
    }

    trace("  %s: \"%s\" (%s) %d.%d (%d:%d)\n",dev_name(device),capsA.szPname,
          (nameA?nameA:"failed"),capsA.vDriverVersion >> 8,
          capsA.vDriverVersion & 0xff, capsA.wMid,capsA.wPid);
    trace("     channels=%d formats=%05lx support=%04lx\n",
          capsA.wChannels,capsA.dwFormats,capsA.dwSupport);
    trace("     %s\n",wave_out_caps(capsA.dwSupport));
    free(nameA);

    if (winetest_interactive && (device != WAVE_MAPPER))
    {
        trace("Playing a 5 seconds reference tone.\n");
        trace("All subsequent tones should be identical to this one.\n");
        trace("Listen for stutter, changes in pitch, volume, etc.\n");
        format.wFormatTag=WAVE_FORMAT_PCM;
        format.nChannels=1;
        format.wBitsPerSample=8;
        format.nSamplesPerSec=22050;
        format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
        format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
        format.cbSize=0;
        wave_out_test_deviceOut(device,5.0,&format,WAVE_FORMAT_2M08,0,&capsA,
                                TRUE,TRUE,FALSE);
    } else {
        format.wFormatTag=WAVE_FORMAT_PCM;
        format.nChannels=1;
        format.wBitsPerSample=8;
        format.nSamplesPerSec=22050;
        format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
        format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
        format.cbSize=0;
        wave_out_test_deviceOut(device,1.0,&format,WAVE_FORMAT_2M08,0,&capsA,
                                TRUE,FALSE,FALSE);
        wave_out_test_deviceOut(device,1.0,&format,WAVE_FORMAT_2M08,0,&capsA,
                                TRUE,FALSE,TRUE);
    }

    for (f=0;f<NB_WIN_FORMATS;f++) {
        format.wFormatTag=WAVE_FORMAT_PCM;
        format.nChannels=win_formats[f][3];
        format.wBitsPerSample=win_formats[f][2];
        format.nSamplesPerSec=win_formats[f][1];
        format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
        format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
        format.cbSize=0;
        wave_out_test_deviceOut(device,1.0,&format,win_formats[f][0],
                                0,&capsA,winetest_interactive,TRUE,FALSE);
        if (winetest_interactive)
            wave_out_test_deviceOut(device,1.0,&format,win_formats[f][0],
                                    0,&capsA,winetest_interactive,TRUE,TRUE);
        if (device != WAVE_MAPPER)
        {
            wave_out_test_deviceOut(device,1.0,&format,win_formats[f][0],
                                WAVE_FORMAT_DIRECT,&capsA,winetest_interactive,
                                TRUE,FALSE);
            wave_out_test_deviceOut(device,1.0,&format,win_formats[f][0],
                                    WAVE_MAPPED,&capsA,winetest_interactive,
                                    TRUE,FALSE);
        }
    }

    /* Try a PCMWAVEFORMAT aligned next to an unaccessible page for bounds
     * checking */
    twoPages = VirtualAlloc(NULL, 2 * dwPageSize, MEM_RESERVE | MEM_COMMIT,
                            PAGE_READWRITE);
    ok(twoPages!=NULL,"Failed to allocate 2 pages of memory\n");
    if (twoPages) {
        res = VirtualProtect(twoPages + dwPageSize, dwPageSize, PAGE_NOACCESS,
                             &flOldProtect);
        ok(res, "Failed to set memory access on second page\n");
        if (res) {
            LPWAVEFORMATEX pwfx = (LPWAVEFORMATEX)(twoPages + dwPageSize -
                sizeof(PCMWAVEFORMAT));
            pwfx->wFormatTag=WAVE_FORMAT_PCM;
            pwfx->nChannels=1;
            pwfx->wBitsPerSample=8;
            pwfx->nSamplesPerSec=22050;
            pwfx->nBlockAlign=pwfx->nChannels*pwfx->wBitsPerSample/8;
            pwfx->nAvgBytesPerSec=pwfx->nSamplesPerSec*pwfx->nBlockAlign;
            wave_out_test_deviceOut(device,1.0,pwfx,WAVE_FORMAT_2M08,0,
                                    &capsA,winetest_interactive,TRUE,FALSE);
            if (device != WAVE_MAPPER)
            {
                wave_out_test_deviceOut(device,1.0,pwfx,WAVE_FORMAT_2M08,
                                    WAVE_FORMAT_DIRECT,&capsA,
                                    winetest_interactive,TRUE,FALSE);
                wave_out_test_deviceOut(device,1.0,pwfx,WAVE_FORMAT_2M08,
                                        WAVE_MAPPED,&capsA,winetest_interactive,
                                        TRUE,FALSE);
            }
        }
        VirtualFree(twoPages, 0, MEM_RELEASE);
    }

    /* Testing invalid format: 11 bits per sample */
    format.wFormatTag=WAVE_FORMAT_PCM;
    format.nChannels=2;
    format.wBitsPerSample=11;
    format.nSamplesPerSec=22050;
    format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
    format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    format.cbSize=0;
    oformat=format;
    rc=waveOutOpen(&wout,device,&format,0,0,CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==WAVERR_BADFORMAT || rc==MMSYSERR_INVALFLAG ||
       rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): opening the device in 11 bits mode should fail: "
       "rc=%s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        trace("     got %ldx%2dx%d for %ldx%2dx%d\n",
              format.nSamplesPerSec, format.wBitsPerSample,
              format.nChannels,
              oformat.nSamplesPerSec, oformat.wBitsPerSample,
              oformat.nChannels);
        waveOutClose(wout);
    }

    /* Testing invalid format: 2 MHz sample rate */
    format.wFormatTag=WAVE_FORMAT_PCM;
    format.nChannels=2;
    format.wBitsPerSample=16;
    format.nSamplesPerSec=2000000;
    format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
    format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    format.cbSize=0;
    oformat=format;
    rc=waveOutOpen(&wout,device,&format,0,0,CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==WAVERR_BADFORMAT || rc==MMSYSERR_INVALFLAG ||
       rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): opening the device at 2 MHz sample rate should fail: "
       "rc=%s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        trace("     got %ldx%2dx%d for %ldx%2dx%d\n",
              format.nSamplesPerSec, format.wBitsPerSample,
              format.nChannels,
              oformat.nSamplesPerSec, oformat.wBitsPerSample,
              oformat.nChannels);
        waveOutClose(wout);
    }

    /* try some non PCM formats */
    format.wFormatTag=WAVE_FORMAT_MULAW;
    format.nChannels=1;
    format.wBitsPerSample=8;
    format.nSamplesPerSec=8000;
    format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
    format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    format.cbSize=0;
    oformat=format;
    rc=waveOutOpen(&wout,device,&format,0,0,CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR ||rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&format,0,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): WAVE_FORMAT_MULAW not supported\n",
              dev_name(device));

    format.wFormatTag=WAVE_FORMAT_ADPCM;
    format.nChannels=2;
    format.wBitsPerSample=4;
    format.nSamplesPerSec=22050;
    format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
    format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    format.cbSize=0;
    oformat=format;
    rc=waveOutOpen(&wout,device,&format,0,0,CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR ||rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&format,0,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): WAVE_FORMAT_ADPCM not supported\n",
              dev_name(device));

    /* test if WAVEFORMATEXTENSIBLE supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=2;
    wfex.Format.wBitsPerSample=16;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,WAVE_FORMAT_2M16,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): WAVE_FORMAT_EXTENSIBLE not supported\n",
              dev_name(device));

    /* test if 4 channels supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=4;
    wfex.Format.wBitsPerSample=16;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,0,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): 4 channels not supported\n",
              dev_name(device));

    /* test if 6 channels supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=6;
    wfex.Format.wBitsPerSample=16;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,WAVE_FORMAT_2M16,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): 6 channels not supported\n",
              dev_name(device));

#if 0	/* ALSA doesn't like this format */
    /* test if 24 bit samples supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=2;
    wfex.Format.wBitsPerSample=24;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,WAVE_FORMAT_2M16,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): 24 bit samples not supported\n",
              dev_name(device));
#endif

    /* test if 32 bit samples supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=2;
    wfex.Format.wBitsPerSample=32;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_PCM;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,WAVE_FORMAT_2M16,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): 32 bit samples not supported\n",
              dev_name(device));

    /* test if 32 bit float samples supported */
    wfex.Format.wFormatTag=WAVE_FORMAT_EXTENSIBLE;
    wfex.Format.nChannels=2;
    wfex.Format.wBitsPerSample=32;
    wfex.Format.nSamplesPerSec=22050;
    wfex.Format.nBlockAlign=wfex.Format.nChannels*wfex.Format.wBitsPerSample/8;
    wfex.Format.nAvgBytesPerSec=wfex.Format.nSamplesPerSec*
        wfex.Format.nBlockAlign;
    wfex.Format.cbSize=22;
    wfex.Samples.wValidBitsPerSample=wfex.Format.wBitsPerSample;
    wfex.dwChannelMask=SPEAKER_ALL;
    wfex.SubFormat=KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
    rc=waveOutOpen(&wout,device,&wfex.Format,0,0,
                   CALLBACK_NULL|WAVE_FORMAT_DIRECT);
    ok(rc==MMSYSERR_NOERROR || rc==WAVERR_BADFORMAT ||
       rc==MMSYSERR_INVALFLAG || rc==MMSYSERR_INVALPARAM,
       "waveOutOpen(%s): returned %s\n",dev_name(device),wave_out_error(rc));
    if (rc==MMSYSERR_NOERROR) {
        waveOutClose(wout);
        wave_out_test_deviceOut(device,1.0,&wfex.Format,WAVE_FORMAT_2M16,0,
                                &capsA,winetest_interactive,TRUE,FALSE);
    } else
        trace("waveOutOpen(%s): 32 bit float samples not supported\n",
              dev_name(device));
}

static void wave_out_tests(void)
{
    WAVEOUTCAPSA capsA;
    WAVEOUTCAPSW capsW;
    WAVEFORMATEX format;
    HWAVEOUT wout;
    MMRESULT rc;
    UINT ndev,d;

    ndev=waveOutGetNumDevs();
    trace("found %d WaveOut devices\n",ndev);

    rc=waveOutGetDevCapsA(ndev+1,&capsA,sizeof(capsA));
    ok(rc==MMSYSERR_BADDEVICEID,
       "waveOutGetDevCapsA(%s): MMSYSERR_BADDEVICEID expected, got %s\n",
       dev_name(ndev+1),mmsys_error(rc));

    rc=waveOutGetDevCapsW(ndev+1,&capsW,sizeof(capsW));
    ok(rc==MMSYSERR_BADDEVICEID || rc==MMSYSERR_NOTSUPPORTED,
       "waveOutGetDevCapsW(%s): MMSYSERR_BADDEVICEID or MMSYSERR_NOTSUPPORTED "
       "expected, got %s\n",dev_name(ndev+1),mmsys_error(rc));

    rc=waveOutGetDevCapsA(WAVE_MAPPER,&capsA,sizeof(capsA));
    if (ndev>0)
        ok(rc==MMSYSERR_NOERROR,
           "waveOutGetDevCapsA(%s): MMSYSERR_NOERROR expected, got %s\n",
           dev_name(WAVE_MAPPER),mmsys_error(rc));
    else
        ok(rc==MMSYSERR_BADDEVICEID || rc==MMSYSERR_NODRIVER,
           "waveOutGetDevCapsA(%s): MMSYSERR_BADDEVICEID or MMSYSERR_NODRIVER "
           "expected, got %s\n",dev_name(WAVE_MAPPER),mmsys_error(rc));

    rc=waveOutGetDevCapsW(WAVE_MAPPER,&capsW,sizeof(capsW));
    if (ndev>0)
        ok(rc==MMSYSERR_NOERROR || rc==MMSYSERR_NOTSUPPORTED,
           "waveOutGetDevCapsW(%s): MMSYSERR_NOERROR or MMSYSERR_NOTSUPPORTED "
           "expected, got %s\n",dev_name(WAVE_MAPPER),mmsys_error(rc));
    else
        ok(rc==MMSYSERR_BADDEVICEID || rc==MMSYSERR_NODRIVER ||
           rc==MMSYSERR_NOTSUPPORTED,
           "waveOutGetDevCapsW(%s): MMSYSERR_BADDEVICEID or MMSYSERR_NODRIVER "
           " or MMSYSERR_NOTSUPPORTED expected, got %s\n",
           dev_name(WAVE_MAPPER),mmsys_error(rc));

    format.wFormatTag=WAVE_FORMAT_PCM;
    format.nChannels=2;
    format.wBitsPerSample=16;
    format.nSamplesPerSec=44100;
    format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;
    format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    format.cbSize=0;
    rc=waveOutOpen(&wout,ndev+1,&format,0,0,CALLBACK_NULL);
    ok(rc==MMSYSERR_BADDEVICEID,
       "waveOutOpen(%s): MMSYSERR_BADDEVICEID expected, got %s\n",
       dev_name(ndev+1),mmsys_error(rc));

    for (d=0;d<ndev;d++)
        wave_out_test_device(d);

    if (ndev>0)
        wave_out_test_device(WAVE_MAPPER);
}

START_TEST(wave)
{
    wave_out_tests();
}
