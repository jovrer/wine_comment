/* DirectMusicSynthesizer Private Include
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

#ifndef __WINE_DMSYNTH_PRIVATE_H
#define __WINE_DMSYNTH_PRIVATE_H

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "wingdi.h"
#include "winuser.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/unicode.h"
#include "winreg.h"
#include "objbase.h"

#include "dmusici.h"
#include "dmusicf.h"
#include "dmusics.h"

/*****************************************************************************
 * Interfaces
 */
typedef struct IDirectMusicSynth8Impl IDirectMusicSynth8Impl;
typedef struct IDirectMusicSynthSinkImpl IDirectMusicSynthSinkImpl;

/*****************************************************************************
 * ClassFactory
 */
extern HRESULT WINAPI DMUSIC_CreateDirectMusicSynthImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateDirectMusicSynthSinkImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);

/*****************************************************************************
 * IDirectMusicSynth8Impl implementation structure
 */
struct IDirectMusicSynth8Impl {
  /* IUnknown fields */
  const IDirectMusicSynth8Vtbl *lpVtbl;
  LONG          ref;

  /* IDirectMusicSynth8 fields */
  DMUS_PORTCAPS pCaps;
  BOOL fActive;
  IReferenceClock* pLatencyClock;
  IDirectMusicSynthSinkImpl* pSynthSink;
};

/* IUnknown: */
extern HRESULT WINAPI IDirectMusicSynth8Impl_QueryInterface (LPDIRECTMUSICSYNTH8 iface, REFIID riid, LPVOID *ppobj);
extern ULONG WINAPI   IDirectMusicSynth8Impl_AddRef (LPDIRECTMUSICSYNTH8 iface);

/*****************************************************************************
 * IDirectMusicSynthSinkImpl implementation structure
 */
struct IDirectMusicSynthSinkImpl {
  /* IUnknown fields */
  const IDirectMusicSynthSinkVtbl *lpVtbl;
  LONG          ref;

  /* IDirectMusicSynthSinkImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicSynthSinkImpl_AddRef (LPDIRECTMUSICSYNTHSINK iface);

/**********************************************************************
 * Dll lifetime tracking declaration for dmsynth.dll
 */
extern LONG DMSYNTH_refCount;
static inline void DMSYNTH_LockModule(void) { InterlockedIncrement( &DMSYNTH_refCount ); }
static inline void DMSYNTH_UnlockModule(void) { InterlockedDecrement( &DMSYNTH_refCount ); }

/*****************************************************************************
 * Misc.
 */
/* used for generic dumping (copied from ddraw) */
typedef struct {
    DWORD val;
    const char* name;
} flag_info;

typedef struct {
    const GUID *guid;
    const char* name;
} guid_info;

/* used for initialising structs (primarily for DMUS_OBJECTDESC) */
#define DM_STRUCT_INIT(x) 				\
	do {								\
		memset((x), 0, sizeof(*(x)));	\
		(x)->dwSize = sizeof(*x);		\
	} while (0)

#define FE(x) { x, #x }	
#define GE(x) { &x, #x }

/* FOURCC to string conversion for debug messages */
extern const char *debugstr_fourcc (DWORD fourcc);
/* DMUS_VERSION struct to string conversion for debug messages */
extern const char *debugstr_dmversion (LPDMUS_VERSION version);
/* returns name of given GUID */
extern const char *debugstr_dmguid (const GUID *id);
/* returns name of given error code */
extern const char *debugstr_dmreturn (DWORD code);
/* generic flags-dumping function */
extern const char *debugstr_flags (DWORD flags, const flag_info* names, size_t num_names);


#endif	/* __WINE_DMSYNTH_PRIVATE_H */
