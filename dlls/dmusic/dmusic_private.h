/* DirectMusic Private Include
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

#ifndef __WINE_DMUSIC_PRIVATE_H
#define __WINE_DMUSIC_PRIVATE_H

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
typedef struct IDirectMusic8Impl IDirectMusic8Impl;
typedef struct IDirectMusicBufferImpl IDirectMusicBufferImpl;
typedef struct IDirectMusicDownloadedInstrumentImpl IDirectMusicDownloadedInstrumentImpl;
typedef struct IDirectMusicDownloadImpl IDirectMusicDownloadImpl;
typedef struct IDirectMusicPortDownloadImpl IDirectMusicPortDownloadImpl;
typedef struct IDirectMusicPortImpl IDirectMusicPortImpl;
typedef struct IDirectMusicThruImpl IDirectMusicThruImpl;
typedef struct IReferenceClockImpl IReferenceClockImpl;

typedef struct IDirectMusicCollectionImpl IDirectMusicCollectionImpl;
typedef struct IDirectMusicInstrumentImpl IDirectMusicInstrumentImpl;

	
/*****************************************************************************
 * Predeclare the interface implementation structures
 */
extern const IDirectMusicPortVtbl DirectMusicPort_Vtbl;

/*****************************************************************************
 * Some stuff to make my life easier :=)
 */
 
/* some sort of aux. midi channel: big fake at the moment; accepts only priority
   changes... more coming soon */
typedef struct DMUSIC_PRIVATE_MCHANNEL_ {
	DWORD priority;
} DMUSIC_PRIVATE_MCHANNEL, *LPDMUSIC_PRIVATE_MCHANNEL;

/* some sort of aux. channel group: collection of 16 midi channels */
typedef struct DMUSIC_PRIVATE_CHANNEL_GROUP_ {
	DMUSIC_PRIVATE_MCHANNEL channel[16]; /* 16 channels in a group */
} DMUSIC_PRIVATE_CHANNEL_GROUP, *LPDMUSIC_PRIVATE_CHANNEL_GROUP;


/*****************************************************************************
 * ClassFactory
 */
extern HRESULT WINAPI DMUSIC_CreateDirectMusicImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateDirectMusicBufferImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateDirectMusicDownloadedInstrumentImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateDirectMusicDownloadImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateReferenceClockImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);

extern HRESULT WINAPI DMUSIC_CreateDirectMusicCollectionImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);
extern HRESULT WINAPI DMUSIC_CreateDirectMusicInstrumentImpl (LPCGUID lpcGUID, LPVOID* ppobj, LPUNKNOWN pUnkOuter);

/*****************************************************************************
 * IDirectMusic8Impl implementation structure
 */
struct IDirectMusic8Impl {
  /* IUnknown fields */
  const IDirectMusic8Vtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicImpl fields */
  IReferenceClockImpl* pMasterClock;
  IDirectMusicPortImpl** ppPorts;
  int nrofports;
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusic8Impl_AddRef (LPDIRECTMUSIC8 iface);

/*****************************************************************************
 * IDirectMusicBufferImpl implementation structure
 */
struct IDirectMusicBufferImpl {
  /* IUnknown fields */
  const IDirectMusicBufferVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicBufferImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicBufferImpl_AddRef (LPDIRECTMUSICBUFFER iface);

/*****************************************************************************
 * IDirectMusicDownloadedInstrumentImpl implementation structure
 */
struct IDirectMusicDownloadedInstrumentImpl {
  /* IUnknown fields */
  const IDirectMusicDownloadedInstrumentVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicDownloadedInstrumentImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicDownloadedInstrumentImpl_AddRef (LPDIRECTMUSICDOWNLOADEDINSTRUMENT iface);
/* IDirectMusicDownloadedInstrumentImpl: */
/* none yet */

/*****************************************************************************
 * IDirectMusicDownloadImpl implementation structure
 */
struct IDirectMusicDownloadImpl {
  /* IUnknown fields */
  const IDirectMusicDownloadVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicDownloadImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicDownloadImpl_AddRef (LPDIRECTMUSICDOWNLOAD iface);

/*****************************************************************************
 * IDirectMusicPortDownloadImpl implementation structure
 */
struct IDirectMusicPortDownloadImpl {
  /* IUnknown fields */
  const IDirectMusicPortDownloadVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicPortDownloadImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicPortDownloadImpl_AddRef (LPDIRECTMUSICPORTDOWNLOAD iface);

/*****************************************************************************
 * IDirectMusicPortImpl implementation structure
 */
struct IDirectMusicPortImpl {
  /* IUnknown fields */
  const IDirectMusicPortVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicPortImpl fields */
  IDirectSound* pDirectSound;
  IReferenceClock* pLatencyClock;
  BOOL fActive;
  LPDMUS_PORTCAPS pCaps;
  LPDMUS_PORTPARAMS pParams;
  int nrofgroups;
  DMUSIC_PRIVATE_CHANNEL_GROUP group[1];
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicPortImpl_AddRef (LPDIRECTMUSICPORT iface);

/*****************************************************************************
 * IDirectMusicThruImpl implementation structure
 */
struct IDirectMusicThruImpl {
  /* IUnknown fields */
  const IDirectMusicThruVtbl *lpVtbl;
  LONG           ref;

  /* IDirectMusicThruImpl fields */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicThruImpl_AddRef (LPDIRECTMUSICTHRU iface);

/*****************************************************************************
 * IReferenceClockImpl implementation structure
 */
struct IReferenceClockImpl {
  /* IUnknown fields */
  const IReferenceClockVtbl *lpVtbl;
  LONG           ref;

  /* IReferenceClockImpl fields */
  REFERENCE_TIME rtTime;
  DMUS_CLOCKINFO pClockInfo;
};

/* IUnknown: */
extern ULONG WINAPI   IReferenceClockImpl_AddRef (IReferenceClock *iface);

typedef struct _DMUS_PRIVATE_INSTRUMENT_ENTRY {
	struct list entry; /* for listing elements */
	IDirectMusicInstrument* pInstrument;
} DMUS_PRIVATE_INSTRUMENTENTRY, *LPDMUS_PRIVATE_INSTRUMENTENTRY;

typedef struct _DMUS_PRIVATE_POOLCUE {
	struct list entry; /* for listing elements */
} DMUS_PRIVATE_POOLCUE, *LPDMUS_PRIVATE_POOLCUE;

/*****************************************************************************
 * IDirectMusicCollectionImpl implementation structure
 */
struct IDirectMusicCollectionImpl {
  /* IUnknown fields */
  const IUnknownVtbl *UnknownVtbl;
  const IDirectMusicCollectionVtbl *CollectionVtbl;
  const IDirectMusicObjectVtbl *ObjectVtbl;
  const IPersistStreamVtbl *PersistStreamVtbl;
  LONG           ref;

  /* IDirectMusicCollectionImpl fields */
  IStream *pStm; /* stream from which we load collection and later instruments */
  LARGE_INTEGER liCollectionPosition; /* offset in a stream where collection was loaded from */
  LARGE_INTEGER liWavePoolTablePosition; /* offset in a stream where wave pool table can be found */
  LPDMUS_OBJECTDESC pDesc;
  CHAR* szCopyright; /* FIXME: should probably placed somewhere else */
  LPDLSHEADER pHeader;
  /* pool table */
  LPPOOLTABLE pPoolTable;
  LPPOOLCUE pPoolCues;
  /* instruments */
  struct list Instruments;
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicCollectionImpl_IUnknown_AddRef (LPUNKNOWN iface);
/* IDirectMusicCollection: */
extern ULONG WINAPI   IDirectMusicCollectionImpl_IDirectMusicCollection_AddRef (LPDIRECTMUSICCOLLECTION iface);
/* IDirectMusicObject: */
extern ULONG WINAPI   IDirectMusicCollectionImpl_IDirectMusicObject_AddRef (LPDIRECTMUSICOBJECT iface);
/* IPersistStream: */
extern ULONG WINAPI   IDirectMusicCollectionImpl_IPersistStream_AddRef (LPPERSISTSTREAM iface);

/*****************************************************************************
 * IDirectMusicInstrumentImpl implementation structure
 */
struct IDirectMusicInstrumentImpl {
  /* IUnknown fields */
  const IUnknownVtbl *UnknownVtbl;
  const IDirectMusicInstrumentVtbl *InstrumentVtbl;
  LONG           ref;

  /* IDirectMusicInstrumentImpl fields */
  LARGE_INTEGER liInstrumentPosition; /* offset in a stream where instrument chunk can be found */
  LPGUID pInstrumentID;
  LPINSTHEADER pHeader;
  WCHAR wszName[DMUS_MAX_NAME];
  /* instrument data */
};

/* IUnknown: */
extern ULONG WINAPI   IDirectMusicInstrumentImpl_IUnknown_AddRef (LPUNKNOWN iface);
/* IDirectMusicInstrumentImpl: */
extern ULONG WINAPI   IDirectMusicInstrumentImpl_IDirectMusicInstrument_AddRef (LPDIRECTMUSICINSTRUMENT iface);
/* custom :) */
extern HRESULT WINAPI IDirectMusicInstrumentImpl_Custom_Load (LPDIRECTMUSICINSTRUMENT iface, LPSTREAM pStm);

/**********************************************************************
 * Dll lifetime tracking declaration for dmusic.dll
 */
extern LONG DMUSIC_refCount;
static inline void DMUSIC_LockModule(void) { InterlockedIncrement( &DMUSIC_refCount ); }
static inline void DMUSIC_UnlockModule(void) { InterlockedDecrement( &DMUSIC_refCount ); }

/*****************************************************************************
 * Helper Functions
 */
void register_waveport (LPGUID lpGUID, LPCSTR lpszDesc, LPCSTR lpszDrvName, LPVOID lpContext);


/*****************************************************************************
 * Misc.
 */
/* my custom ICOM stuff */
#define ICOM_NAME_MULTI(impl,field,iface,name)  impl* const name=(impl*)((char*)(iface) - offsetof(impl,field))
#define ICOM_THIS_MULTI(impl,field,iface) ICOM_NAME_MULTI(impl,field,iface,This)
 
/* for simpler reading */
typedef struct _DMUS_PRIVATE_CHUNK {
	FOURCC fccID; /* FOURCC ID of the chunk */
	DWORD dwSize; /* size of the chunk */
} DMUS_PRIVATE_CHUNK, *LPDMUS_PRIVATE_CHUNK;

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

/* dwPatch from MIDILOCALE */
extern DWORD MIDILOCALE2Patch (LPMIDILOCALE pLocale);
/* MIDILOCALE from dwPatch */
extern void Patch2MIDILOCALE (DWORD dwPatch, LPMIDILOCALE pLocale);

/* check whether the given DWORD is even (return 0) or odd (return 1) */
extern int even_or_odd (DWORD number);
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
extern const char *debugstr_DMUS_OBJ_FLAGS (DWORD flagmask);
/* dump whole DMUS_OBJECTDESC struct */
extern const char *debugstr_DMUS_OBJECTDESC (LPDMUS_OBJECTDESC pDesc);

#endif	/* __WINE_DMUSIC_PRIVATE_H */
