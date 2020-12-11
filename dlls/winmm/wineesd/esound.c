/*
 * Wine Driver for EsounD Sound Server
 * http://www.tux.org/~ricdude/EsounD.html
 *
 * Copyright 2004 Zhangrong Huang <hzhr@users.sourceforge.net>
 *
 * Code massively copied from Eric Pouech's OSS driver
 * and Chris Morgan aRts driver
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

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "mmddk.h"
#include "esound.h"

#ifdef HAVE_ESD
static		int esd = 0;

/**************************************************************************
 * 				ESD_drvOpen			[internal]
 */
static	DWORD	ESD_drvOpen(LPSTR str)
{
    if (esd)
	return 0;

    /* I know, this is ugly, but who cares... */
    esd = 1;
    return 1;
}

/**************************************************************************
 * 				ESD_drvClose			[internal]
 */
static	DWORD	ESD_drvClose(DWORD dwDevID)
{
    if (esd) {
	esd = 0;
	return 1;
    }
    return 0;
}
#endif /* #ifdef HAVE_ESD */


/**************************************************************************
 * 				DriverProc (WINEESD.@)
 */
LONG CALLBACK	ESD_DriverProc(DWORD dwDevID, HDRVR hDriv, DWORD wMsg,
			       DWORD dwParam1, DWORD dwParam2)
{
/* EPP     TRACE("(%08lX, %04X, %08lX, %08lX, %08lX)\n",  */
/* EPP 	  dwDevID, hDriv, wMsg, dwParam1, dwParam2); */

    switch(wMsg) {
#ifdef HAVE_ESD
    case DRV_LOAD:		if (ESD_WaveInit()<0) return 0;
				return 1;
    case DRV_FREE:	        return ESD_WaveClose();
    case DRV_OPEN:		return ESD_drvOpen((LPSTR)dwParam1);
    case DRV_CLOSE:		return ESD_drvClose(dwDevID);
    case DRV_ENABLE:		return 1;
    case DRV_DISABLE:		return 1;
    case DRV_QUERYCONFIGURE:	return 1;
    case DRV_CONFIGURE:		MessageBoxA(0, "EsounD MultiMedia Driver!", "EsounD Driver", MB_OK);	return 1;
    case DRV_INSTALL:		return DRVCNF_RESTART;
    case DRV_REMOVE:		return DRVCNF_RESTART;
#endif
    default:
	return DefDriverProc(dwDevID, hDriv, wMsg, dwParam1, dwParam2);
    }
}
