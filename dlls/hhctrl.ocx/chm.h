/*
 * CHM Utility API
 *
 * Copyright 2005 James Hawkins
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

#ifndef CHM_H
#define CHM_H

#include "wine/itss.h"

typedef struct CHMInfo
{
    IITStorage *pITStorage;
    IStorage *pStorage;
    LPCWSTR szFile;
} CHMInfo;

BOOL CHM_OpenCHM(CHMInfo *pCHMInfo, LPCWSTR szFile);
BOOL CHM_LoadWinTypeFromCHM(CHMInfo *pCHMInfo, HH_WINTYPEW *pHHWinType);
void CHM_CloseCHM(CHMInfo *pCHMInfo);
void CHM_CreateITSUrl(CHMInfo *pCHMInfo, LPCWSTR szIndex, LPWSTR szUrl);

#endif
