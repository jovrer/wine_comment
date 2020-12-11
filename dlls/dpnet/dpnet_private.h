/*
 * DirectPlay8 (dpnet) private include file
 *
 * Copyright 2004 Raphael Junqueira
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

#ifndef __WINE_DPNET_PRIVATE_H
#define __WINE_DPNET_PRIVATE_H

#ifndef __WINE_CONFIG_H
# error You must include config.h to use this header
#endif

#include "dplay8.h"
/*
 *#include "dplobby8.h"
 *#include "dplay8sp.h"
 */

/* DirectPlay8 Interfaces: */
typedef struct IDirectPlay8ClientImpl IDirectPlay8ClientImpl;
typedef struct IDirectPlay8AddressImpl IDirectPlay8AddressImpl;

/* ------------------ */
/* IDirectPlay8Client */
/* ------------------ */

/*****************************************************************************
 * IDirectPlay8Client implementation structure
 */
struct IDirectPlay8ClientImpl
{
  /* IUnknown fields */
  const IDirectPlay8ClientVtbl *lpVtbl;
  LONG          ref;
  /* IDirectPlay8Client fields */
};

/* IUnknown: */
extern ULONG WINAPI IDirectPlay8ClientImpl_AddRef(PDIRECTPLAY8CLIENT iface);

/* ------------------- */
/* IDirectPlay8Address */
/* ------------------- */

/*****************************************************************************
 * IDirectPlay8Address implementation structure
 */
struct IDirectPlay8AddressImpl
{
  /* IUnknown fields */
  const IDirectPlay8AddressVtbl *lpVtbl;
  LONG          ref;
  /* IDirectPlay8Address fields */
  GUID SP_guid;
  const WCHAR *url;
};

/* IUnknown: */
extern HRESULT WINAPI IDirectPlay8AddressImpl_QueryInterface(PDIRECTPLAY8ADDRESS iface, REFIID riid, LPVOID *ppobj);
extern ULONG WINAPI IDirectPlay8AddressImpl_AddRef(PDIRECTPLAY8ADDRESS iface);
extern ULONG WINAPI IDirectPlay8AddressImpl_Release(PDIRECTPLAY8ADDRESS iface);

/* IDirectPlay8Address: */



/**
 * factories
 */
extern HRESULT DPNET_CreateDirectPlay8Client(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);
extern HRESULT DPNET_CreateDirectPlay8Server(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);
extern HRESULT DPNET_CreateDirectPlay8Peer(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);
extern HRESULT DPNET_CreateDirectPlay8Address(LPCLASSFACTORY iface, LPUNKNOWN punkOuter, REFIID riid, LPVOID *ppobj);

/**
 * debug
 */
extern const char *debugstr_SP(const GUID *id);

/* used for generic dumping (copied from ddraw) */
typedef struct {
    DWORD val;
    const char* name;
} flag_info;

typedef struct {
    const GUID *guid;
    const char* name;
} guid_info;

#define FE(x) { x, #x }	
#define GE(x) { &x, #x }



#endif
