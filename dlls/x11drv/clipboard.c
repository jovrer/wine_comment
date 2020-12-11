/*
 * X11 clipboard windows driver
 *
 * Copyright 1994 Martin Ayotte
 *	     1996 Alex Korobka
 *	     1999 Noel Borthwick
 *           2003 Ulrich Czekalla for CodeWeavers
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
 *
 * NOTES:
 *    This file contains the X specific implementation for the windows
 *    Clipboard API.
 *
 *    Wine's internal clipboard is exposed to external apps via the X
 *    selection mechanism.
 *    Currently the driver asserts ownership via two selection atoms:
 *    1. PRIMARY(XA_PRIMARY)
 *    2. CLIPBOARD
 *
 *    In our implementation, the CLIPBOARD selection takes precedence over PRIMARY,
 *    i.e. if a CLIPBOARD selection is available, it is used instead of PRIMARY.
 *    When Wine takes ownership of the clipboard, it takes ownership of BOTH selections.
 *    While giving up selection ownership, if the CLIPBOARD selection is lost,
 *    it will lose both PRIMARY and CLIPBOARD and empty the clipboard.
 *    However if only PRIMARY is lost, it will continue to hold the CLIPBOARD selection
 *    (leaving the clipboard cache content unaffected).
 *
 *      Every format exposed via a windows clipboard format is also exposed through
 *    a corresponding X selection target. A selection target atom is synthesized
 *    whenever a new Windows clipboard format is registered via RegisterClipboardFormat,
 *    or when a built-in format is used for the first time.
 *    Windows native format are exposed by prefixing the format name with "<WCF>"
 *    This allows us to uniquely identify windows native formats exposed by other
 *    running WINE apps.
 *
 *      In order to allow external applications to query WINE for supported formats,
 *    we respond to the "TARGETS" selection target. (See EVENT_SelectionRequest
 *    for implementation) We use the same mechanism to query external clients for
 *    availability of a particular format, by caching the list of available targets
 *    by using the clipboard cache's "delayed render" mechanism. If a selection client
 *    does not support the "TARGETS" selection target, we actually attempt to retrieve
 *    the format requested as a fallback mechanism.
 *
 *      Certain Windows native formats are automatically converted to X native formats
 *    and vice versa. If a native format is available in the selection, it takes
 *    precedence, in order to avoid unnecessary conversions.
 *
 * FIXME: global format list needs a critical section
 */

#include "config.h"
#include "wine/port.h"

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <assert.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wine/wingdi16.h"
#include "x11drv.h"
#include "wine/debug.h"
#include "wine/unicode.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(clipboard);

#define HGDIOBJ_32(handle16)  ((HGDIOBJ)(ULONG_PTR)(handle16))

/* Maximum wait time for selection notify */
#define SELECTION_RETRIES 500  /* wait for .1 seconds */
#define SELECTION_WAIT    1000 /* us */
/* Minimum seconds that must lapse between owner queries */
#define OWNERQUERYLAPSETIME 1

/* Selection masks */
#define S_NOSELECTION    0
#define S_PRIMARY        1
#define S_CLIPBOARD      2

typedef struct
{
    HWND hWndOpen;
    HWND hWndOwner;
    HWND hWndViewer;
    UINT seqno;
    UINT flags;
} CLIPBOARDINFO, *LPCLIPBOARDINFO;

struct tagWINE_CLIPDATA; /* Forward */

typedef HANDLE (*DRVEXPORTFUNC)(Window requestor, Atom aTarget, Atom rprop,
    struct tagWINE_CLIPDATA* lpData, LPDWORD lpBytes);
typedef HANDLE (*DRVIMPORTFUNC)(Window w, Atom prop);

typedef struct tagWINE_CLIPFORMAT {
    UINT        wFormatID;
    LPCWSTR     Name;
    UINT        drvData;
    UINT        wFlags;
    DRVIMPORTFUNC  lpDrvImportFunc;
    DRVEXPORTFUNC  lpDrvExportFunc;
    struct tagWINE_CLIPFORMAT *PrevFormat;
    struct tagWINE_CLIPFORMAT *NextFormat;
} WINE_CLIPFORMAT, *LPWINE_CLIPFORMAT;

typedef struct tagWINE_CLIPDATA {
    UINT        wFormatID;
    HANDLE16    hData16;
    HANDLE      hData32;
    UINT        wFlags;
    UINT        drvData;
    LPWINE_CLIPFORMAT lpFormat;
    struct tagWINE_CLIPDATA *PrevData;
    struct tagWINE_CLIPDATA *NextData;
} WINE_CLIPDATA, *LPWINE_CLIPDATA;

#define CF_FLAG_BUILTINFMT   0x0001 /* Built-in windows format */
#define CF_FLAG_UNOWNED      0x0002 /* cached data is not owned */
#define CF_FLAG_SYNTHESIZED  0x0004 /* Implicitly converted data */
#define CF_FLAG_UNICODE      0x0008 /* Data is in unicode */

static int selectionAcquired = 0;              /* Contains the current selection masks */
static Window selectionWindow = None;          /* The top level X window which owns the selection */
static Atom selectionCacheSrc = XA_PRIMARY;    /* The selection source from which the clipboard cache was filled */

void X11DRV_EmptyClipboard(BOOL keepunowned);
void X11DRV_EndClipboardUpdate(void);
static HANDLE X11DRV_CLIPBOARD_ImportClipboardData(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportEnhMetaFile(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportMetaFilePict(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportXAPIXMAP(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportXAString(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportUTF8(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ImportCompoundText(Window w, Atom prop);
static HANDLE X11DRV_CLIPBOARD_ExportClipboardData(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpData, LPDWORD lpBytes);
static HANDLE X11DRV_CLIPBOARD_ExportString(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpData, LPDWORD lpBytes);
static HANDLE X11DRV_CLIPBOARD_ExportXAPIXMAP(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpdata, LPDWORD lpBytes);
static HANDLE X11DRV_CLIPBOARD_ExportMetaFilePict(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpdata, LPDWORD lpBytes);
static HANDLE X11DRV_CLIPBOARD_ExportEnhMetaFile(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpdata, LPDWORD lpBytes);
static WINE_CLIPFORMAT *X11DRV_CLIPBOARD_InsertClipboardFormat(LPCWSTR FormatName, Atom prop);
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedText(UINT wFormatID);
static void X11DRV_CLIPBOARD_FreeData(LPWINE_CLIPDATA lpData);
static BOOL X11DRV_CLIPBOARD_IsSelectionOwner(void);
static int X11DRV_CLIPBOARD_QueryAvailableData(LPCLIPBOARDINFO lpcbinfo);
static BOOL X11DRV_CLIPBOARD_ReadSelectionData(LPWINE_CLIPDATA lpData);
static BOOL X11DRV_CLIPBOARD_ReadProperty(Window w, Atom prop,
    unsigned char** data, unsigned long* datasize);
static BOOL X11DRV_CLIPBOARD_RenderFormat(LPWINE_CLIPDATA lpData);
static HANDLE X11DRV_CLIPBOARD_SerializeMetafile(INT wformat, HANDLE hdata, LPDWORD lpcbytes, BOOL out);
static BOOL X11DRV_CLIPBOARD_SynthesizeData(UINT wFormatID);
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedFormat(LPWINE_CLIPDATA lpData);
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedDIB(void);
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedBitmap(void);
static void X11DRV_HandleSelectionRequest( HWND hWnd, XSelectionRequestEvent *event, BOOL bIsMultiple );

/* Clipboard formats
 * WARNING: This data ordering is dependent on the WINE_CLIPFORMAT structure
 * declared in clipboard.h
 */
static const WCHAR wszCF_TEXT[] = {'W','C','F','_','T','E','X','T',0};
static const WCHAR wszCF_BITMAP[] = {'W','C','F','_','B','I','T','M','A','P',0};
static const WCHAR wszCF_METAFILEPICT[] = {'W','C','F','_','M','E','T','A','F','I','L','E','P','I','C','T',0};
static const WCHAR wszCF_SYLK[] = {'W','C','F','_','S','Y','L','K',0};
static const WCHAR wszCF_DIF[] = {'W','C','F','_','D','I','F',0};
static const WCHAR wszCF_TIFF[] = {'W','C','F','_','T','I','F','F',0};
static const WCHAR wszCF_OEMTEXT[] = {'W','C','F','_','O','E','M','T','E','X','T',0};
static const WCHAR wszCF_DIB[] = {'W','C','F','_','D','I','B',0};
static const WCHAR wszCF_PALETTE[] = {'W','C','F','_','P','A','L','E','T','T','E',0};
static const WCHAR wszCF_PENDATA[] = {'W','C','F','_','P','E','N','D','A','T','A',0};
static const WCHAR wszCF_RIFF[] = {'W','C','F','_','R','I','F','F',0};
static const WCHAR wszCF_WAVE[] = {'W','C','F','_','W','A','V','E',0};
static const WCHAR wszCOMPOUNDTEXT[] = {'C','O','M','P','O','U','N','D','_','T','E','X','T',0};
static const WCHAR wszUTF8STRING[] = {'U','T','F','8','_','S','T','R','I','N','G',0};
static const WCHAR wszCF_ENHMETAFILE[] = {'W','C','F','_','E','N','H','M','E','T','A','F','I','L','E',0};
static const WCHAR wszCF_HDROP[] = {'W','C','F','_','H','D','R','O','P',0};
static const WCHAR wszCF_LOCALE[] = {'W','C','F','_','L','O','C','A','L','E',0};
static const WCHAR wszCF_DIBV5[] = {'W','C','F','_','D','I','B','V','5',0};
static const WCHAR wszCF_OWNERDISPLAY[] = {'W','C','F','_','O','W','N','E','R','D','I','S','P','L','A','Y',0};
static const WCHAR wszCF_DSPTEXT[] = {'W','C','F','_','D','S','P','T','E','X','T',0};
static const WCHAR wszCF_DSPBITMAP[] = {'W','C','F','_','D','S','P','B','I','T','M','A','P',0};
static const WCHAR wszCF_DSPMETAFILEPICT[] = {'W','C','F','_','D','S','P','M','E','T','A','F','I','L','E','P','I','C','T',0};
static const WCHAR wszCF_DSPENHMETAFILE[] = {'W','C','F','_','D','S','P','E','N','H','M','E','T','A','F','I','L','E',0};

static WINE_CLIPFORMAT ClipFormats[]  =
{
    { CF_TEXT, wszCF_TEXT, XA_STRING, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportXAString,
        X11DRV_CLIPBOARD_ExportString, NULL, &ClipFormats[1]},

    { CF_BITMAP, wszCF_BITMAP, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        NULL, &ClipFormats[0], &ClipFormats[2]},

    { CF_METAFILEPICT, wszCF_METAFILEPICT, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportMetaFilePict,
        X11DRV_CLIPBOARD_ExportMetaFilePict, &ClipFormats[1], &ClipFormats[3]},

    { CF_SYLK, wszCF_SYLK, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[2], &ClipFormats[4]},

    { CF_DIF, wszCF_DIF, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[3], &ClipFormats[5]},

    { CF_TIFF, wszCF_TIFF, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[4], &ClipFormats[6]},

    { CF_OEMTEXT, wszCF_OEMTEXT, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[5], &ClipFormats[7]},

    { CF_DIB, wszCF_DIB, XA_PIXMAP, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportXAPIXMAP,
        X11DRV_CLIPBOARD_ExportXAPIXMAP, &ClipFormats[6], &ClipFormats[8]},

    { CF_PALETTE, wszCF_PALETTE, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[7], &ClipFormats[9]},

    { CF_PENDATA, wszCF_PENDATA, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[8], &ClipFormats[10]},

    { CF_RIFF, wszCF_RIFF, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[9], &ClipFormats[11]},

    { CF_WAVE, wszCF_WAVE, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[10], &ClipFormats[12]},

    { CF_UNICODETEXT, wszUTF8STRING, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportUTF8,
        X11DRV_CLIPBOARD_ExportString, &ClipFormats[11], &ClipFormats[13]},

    /* If UTF8_STRING is not available, attempt COMPUND_TEXT */
    { CF_UNICODETEXT, wszCOMPOUNDTEXT, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportCompoundText,
        X11DRV_CLIPBOARD_ExportString, &ClipFormats[12], &ClipFormats[14]},

    { CF_ENHMETAFILE, wszCF_ENHMETAFILE, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportEnhMetaFile,
        X11DRV_CLIPBOARD_ExportEnhMetaFile, &ClipFormats[13], &ClipFormats[15]},

    { CF_HDROP, wszCF_HDROP, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[14], &ClipFormats[16]},

    { CF_LOCALE, wszCF_LOCALE, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[15], &ClipFormats[17]},

    { CF_DIBV5, wszCF_DIBV5, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[16], &ClipFormats[18]},

    { CF_OWNERDISPLAY, wszCF_OWNERDISPLAY, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[17], &ClipFormats[19]},

    { CF_DSPTEXT, wszCF_DSPTEXT, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[18], &ClipFormats[20]},

    { CF_DSPBITMAP, wszCF_DSPBITMAP, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[19], &ClipFormats[21]},

    { CF_DSPMETAFILEPICT, wszCF_DSPMETAFILEPICT, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[20], &ClipFormats[22]},

    { CF_DSPENHMETAFILE, wszCF_DSPENHMETAFILE, 0, CF_FLAG_BUILTINFMT, X11DRV_CLIPBOARD_ImportClipboardData,
        X11DRV_CLIPBOARD_ExportClipboardData, &ClipFormats[21], NULL}
};

#define GET_ATOM(prop)  (((prop) < FIRST_XATOM) ? (Atom)(prop) : X11DRV_Atoms[(prop) - FIRST_XATOM])

/* Maps X properties to Windows formats */
static const WCHAR wszRichTextFormat[] = {'R','i','c','h',' ','T','e','x','t',' ','F','o','r','m','a','t',0};
static const WCHAR wszGIF[] = {'G','I','F',0};
static const WCHAR wszHTMLFormat[] = {'H','T','M','L',' ','F','o','r','m','a','t',0};
static const WCHAR wszRICHHTMLFormat[] = {'H','T','M','L',' ','F','o','r','m','a','t',0};
static const struct
{
    LPCWSTR lpszFormat;
    UINT   prop;
} PropertyFormatMap[] =
{
    { wszRichTextFormat, XATOM_text_rtf },
    { wszRichTextFormat, XATOM_text_richtext },
    { wszGIF, XATOM_image_gif },
    { wszHTMLFormat, XATOM_text_html },
};


/* Maps equivalent X properties. It is assumed that lpszProperty must already 
   be in ClipFormats or PropertyFormatMap. */
static const struct
{
    UINT drvDataProperty;
    UINT drvDataAlias;
} PropertyAliasMap[] =
{
    /* DataProperty,     DataAlias */
};


/*
 * Cached clipboard data.
 */
static LPWINE_CLIPDATA ClipData = NULL;
static UINT ClipDataCount = 0;

/*
 * Clipboard sequence number
 */
static UINT wSeqNo = 0;

/**************************************************************************
 *                Internal Clipboard implementation methods
 **************************************************************************/

static Window thread_selection_wnd(void)
{
    Window w = x11drv_thread_data()->selection_wnd;

    if (!w)
    {
        wine_tsx11_lock();
        w = XCreateWindow(thread_display(), root_window, 0, 0, 1, 1, 0, screen_depth,
                          InputOutput, CopyFromParent, 0, NULL);
        wine_tsx11_unlock();

        if (w)
            x11drv_thread_data()->selection_wnd = w;
        else
            FIXME("Failed to create window. Fetching selection data will fail.\n");
    }

    return w;
}

/**************************************************************************
 *		X11DRV_InitClipboard
 */
void X11DRV_InitClipboard(void)
{
    UINT i;

    /* Register known mapping between window formats and X properties */
    for (i = 0; i < sizeof(PropertyFormatMap)/sizeof(PropertyFormatMap[0]); i++)
        X11DRV_CLIPBOARD_InsertClipboardFormat(PropertyFormatMap[i].lpszFormat, GET_ATOM(PropertyFormatMap[i].prop));
}


/**************************************************************************
 *                intern_atoms
 *
 * Intern atoms for formats that don't have one yet.
 */
static void intern_atoms(void)
{
    LPWINE_CLIPFORMAT format;
    int i, count, len;
    char **names;
    Atom *atoms;

    for (format = ClipFormats, count = 0; format; format = format->NextFormat)
        if (!format->drvData) count++;
    if (!count) return;

    names = HeapAlloc( GetProcessHeap(), 0, count * sizeof(*names) );
    atoms = HeapAlloc( GetProcessHeap(), 0, count * sizeof(*atoms) );

    for (format = ClipFormats, i = 0; format; format = format->NextFormat) {
        if (!format->drvData) {
            len = WideCharToMultiByte(CP_UNIXCP, 0, format->Name, -1, NULL, 0, NULL, NULL);
            names[i] = HeapAlloc(GetProcessHeap(), 0, len);
            WideCharToMultiByte(CP_UNIXCP, 0, format->Name, -1, names[i++], len, NULL, NULL);
        }
    }

    wine_tsx11_lock();
    XInternAtoms( thread_display(), names, count, False, atoms );
    wine_tsx11_unlock();

    for (format = ClipFormats, i = 0; format; format = format->NextFormat) {
        if (!format->drvData) {
            HeapFree(GetProcessHeap(), 0, names[i]);
            format->drvData = atoms[i++];
        }
    }

    HeapFree( GetProcessHeap(), 0, names );
    HeapFree( GetProcessHeap(), 0, atoms );
}


/**************************************************************************
 *		register_format
 *
 * Register a custom X clipboard format.
 */
static WINE_CLIPFORMAT *register_format( LPCWSTR FormatName, Atom prop )
{
    LPWINE_CLIPFORMAT lpFormat = ClipFormats;

    TRACE("%s\n", debugstr_w(FormatName));

    /* walk format chain to see if it's already registered */
    while (lpFormat)
    {
	if (CompareStringW(LOCALE_SYSTEM_DEFAULT, NORM_IGNORECASE, lpFormat->Name, -1, FormatName, -1) == CSTR_EQUAL
	    && (lpFormat->wFlags & CF_FLAG_BUILTINFMT) == 0)
	     return lpFormat;
	lpFormat = lpFormat->NextFormat;
    }

    return X11DRV_CLIPBOARD_InsertClipboardFormat(FormatName, prop);
}


/**************************************************************************
 *                X11DRV_CLIPBOARD_LookupFormat
 */
static LPWINE_CLIPFORMAT X11DRV_CLIPBOARD_LookupFormat(WORD wID)
{
    LPWINE_CLIPFORMAT lpFormat = ClipFormats;

    while(lpFormat)
    {
        if (lpFormat->wFormatID == wID) 
            break;

	lpFormat = lpFormat->NextFormat;
    }
    if (lpFormat && !lpFormat->drvData) intern_atoms();
    return lpFormat;
}


/**************************************************************************
 *                X11DRV_CLIPBOARD_LookupProperty
 */
static LPWINE_CLIPFORMAT X11DRV_CLIPBOARD_LookupProperty(LPWINE_CLIPFORMAT current, UINT drvData)
{
    for (;;)
    {
        LPWINE_CLIPFORMAT lpFormat = ClipFormats;
        BOOL need_intern = FALSE;

        if (current)
            lpFormat = current->NextFormat;

        while(lpFormat)
        {
            if (lpFormat->drvData == drvData) return lpFormat;
            if (!lpFormat->drvData) need_intern = TRUE;
            lpFormat = lpFormat->NextFormat;
        }
        if (!need_intern) return NULL;
        intern_atoms();
        /* restart the search for the new atoms */
    }
}


/**************************************************************************
 *               X11DRV_CLIPBOARD_LookupData
 */
static LPWINE_CLIPDATA X11DRV_CLIPBOARD_LookupData(DWORD wID)
{
    LPWINE_CLIPDATA lpData = ClipData;

    if (lpData)
    {
        do
        {
            if (lpData->wFormatID == wID) 
                break;

	    lpData = lpData->NextData;
        }
        while(lpData != ClipData);

        if (lpData->wFormatID != wID)
            lpData = NULL;
    }

    return lpData;
}


/**************************************************************************
 *		InsertClipboardFormat
 */
static WINE_CLIPFORMAT *X11DRV_CLIPBOARD_InsertClipboardFormat(LPCWSTR FormatName, Atom prop)
{
    LPWINE_CLIPFORMAT lpFormat;
    LPWINE_CLIPFORMAT lpNewFormat;
   
    /* allocate storage for new format entry */
    lpNewFormat = HeapAlloc(GetProcessHeap(), 0, sizeof(WINE_CLIPFORMAT));

    if(lpNewFormat == NULL) 
    {
        WARN("No more memory for a new format!\n");
        return NULL;
    }

    if (!(lpNewFormat->Name = HeapAlloc(GetProcessHeap(), 0, (strlenW(FormatName)+1)*sizeof(WCHAR))))
    {
        WARN("No more memory for the new format name!\n");
        HeapFree(GetProcessHeap(), 0, lpNewFormat);
        return NULL;
    }

    strcpyW((LPWSTR)lpNewFormat->Name, FormatName);
    lpNewFormat->wFlags = 0;
    lpNewFormat->wFormatID = GlobalAddAtomW(lpNewFormat->Name);
    lpNewFormat->drvData = prop;
    lpNewFormat->lpDrvImportFunc = X11DRV_CLIPBOARD_ImportClipboardData;
    lpNewFormat->lpDrvExportFunc = X11DRV_CLIPBOARD_ExportClipboardData;

    /* Link Format */
    lpFormat = ClipFormats;

    while(lpFormat->NextFormat) /* Move to last entry */
	lpFormat = lpFormat->NextFormat;

    lpNewFormat->NextFormat = NULL;
    lpFormat->NextFormat = lpNewFormat;
    lpNewFormat->PrevFormat = lpFormat;

    TRACE("Registering format(%d): %s drvData %d\n",
        lpNewFormat->wFormatID, debugstr_w(FormatName), lpNewFormat->drvData);

    return lpNewFormat;
}




/**************************************************************************
 *                      X11DRV_CLIPBOARD_GetClipboardInfo
 */
static BOOL X11DRV_CLIPBOARD_GetClipboardInfo(LPCLIPBOARDINFO cbInfo)
{
    BOOL bRet = FALSE;

    SERVER_START_REQ( set_clipboard_info )
    {
        req->flags = 0;

        if (wine_server_call_err( req ))
        {
            ERR("Failed to get clipboard owner.\n");
        }
        else
        {
            cbInfo->hWndOpen = reply->old_clipboard;
            cbInfo->hWndOwner = reply->old_owner;
            cbInfo->hWndViewer = reply->old_viewer;
            cbInfo->seqno = reply->seqno;
            cbInfo->flags = reply->flags;

            bRet = TRUE;
        }
    }
    SERVER_END_REQ;

    return bRet;
}


/**************************************************************************
 *	X11DRV_CLIPBOARD_ReleaseOwnership
 */
static BOOL X11DRV_CLIPBOARD_ReleaseOwnership(void)
{
    BOOL bRet = FALSE;

    SERVER_START_REQ( set_clipboard_info )
    {
        req->flags = SET_CB_RELOWNER | SET_CB_SEQNO;

        if (wine_server_call_err( req ))
        {
            ERR("Failed to set clipboard.\n");
        }
        else
        {
            bRet = TRUE;
        }
    }
    SERVER_END_REQ;

    return bRet;
}



/**************************************************************************
 *                      X11DRV_CLIPBOARD_InsertClipboardData
 *
 * Caller *must* have the clipboard open and be the owner.
 */
static BOOL X11DRV_CLIPBOARD_InsertClipboardData(UINT wFormatID, HANDLE16 hData16,
    HANDLE hData32, DWORD flags, LPWINE_CLIPFORMAT lpFormat, BOOL override)
{
    LPWINE_CLIPDATA lpData = X11DRV_CLIPBOARD_LookupData(wFormatID);

    TRACE("format=%d lpData=%p hData16=%08x hData32=%p flags=0x%08lx lpFormat=%p override=%d\n",
        wFormatID, lpData, hData16, hData32, flags, lpFormat, override);

    if (lpData && !override)
        return TRUE;

    if (lpData)
    {
        X11DRV_CLIPBOARD_FreeData(lpData);

        lpData->hData16 = hData16;  /* 0 is legal, see WM_RENDERFORMAT */
        lpData->hData32 = hData32;
    }
    else
    {
        lpData = HeapAlloc(GetProcessHeap(), 0, sizeof(WINE_CLIPDATA));

        lpData->wFormatID = wFormatID;
        lpData->hData16 = hData16;  /* 0 is legal, see WM_RENDERFORMAT */
        lpData->hData32 = hData32;
        lpData->lpFormat = lpFormat;
        lpData->drvData = 0;

        if (ClipData)
        {
            LPWINE_CLIPDATA lpPrevData = ClipData->PrevData;

            lpData->PrevData = lpPrevData;
            lpData->NextData = ClipData;

            lpPrevData->NextData = lpData;
            ClipData->PrevData = lpData;
        }
        else
        {
            lpData->NextData = lpData;
            lpData->PrevData = lpData;
            ClipData = lpData;
        }

        ClipDataCount++;
    }

    lpData->wFlags = flags;

    return TRUE;
}


/**************************************************************************
 *			X11DRV_CLIPBOARD_FreeData
 *
 * Free clipboard data handle.
 */
static void X11DRV_CLIPBOARD_FreeData(LPWINE_CLIPDATA lpData)
{
    TRACE("%d\n", lpData->wFormatID);

    if ((lpData->wFormatID >= CF_GDIOBJFIRST &&
        lpData->wFormatID <= CF_GDIOBJLAST) || 
        lpData->wFormatID == CF_BITMAP || 
        lpData->wFormatID == CF_DIB || 
        lpData->wFormatID == CF_PALETTE)
    {
      if (lpData->hData32)
	DeleteObject(lpData->hData32);

      if (lpData->hData16)
	DeleteObject(HGDIOBJ_32(lpData->hData16));

      if ((lpData->wFormatID == CF_DIB) && lpData->drvData)
          XFreePixmap(gdi_display, lpData->drvData);
    }
    else if (lpData->wFormatID == CF_METAFILEPICT)
    {
      if (lpData->hData32)
      {
        DeleteMetaFile(((METAFILEPICT *)GlobalLock( lpData->hData32 ))->hMF );
        GlobalFree(lpData->hData32);

	if (lpData->hData16)
	  /* HMETAFILE16 and HMETAFILE32 are apparently the same thing,
	     and a shallow copy is enough to share a METAFILEPICT
	     structure between 16bit and 32bit clipboards.  The MetaFile
	     should of course only be deleted once. */
	  GlobalFree16(lpData->hData16);
      }

      if (lpData->hData16)
      {
        METAFILEPICT16* lpMetaPict = (METAFILEPICT16 *) GlobalLock16(lpData->hData16);

        if (lpMetaPict)
        {
            /* To delete 16-bit meta file, we just need to free the associated
               handle. See DeleteMetaFile16() in dlls/gdi/metafile.c. */
            GlobalFree16(lpMetaPict->hMF);
            lpMetaPict->hMF = 0;
        }

	GlobalFree16(lpData->hData16);
      }
    }
    else if (lpData->wFormatID == CF_ENHMETAFILE)
    {
        if (lpData->hData32)
            DeleteEnhMetaFile(lpData->hData32);
    }
    else if (lpData->wFormatID < CF_PRIVATEFIRST ||
             lpData->wFormatID > CF_PRIVATELAST)
    {
      if (lpData->hData32)
        GlobalFree(lpData->hData32);

      if (lpData->hData16)
	GlobalFree16(lpData->hData16);
    }

    lpData->hData16 = 0;
    lpData->hData32 = 0;
    lpData->drvData = 0;
}


/**************************************************************************
 *			X11DRV_CLIPBOARD_UpdateCache
 */
static BOOL X11DRV_CLIPBOARD_UpdateCache(LPCLIPBOARDINFO lpcbinfo)
{
    BOOL bret = TRUE;

    if (!X11DRV_CLIPBOARD_IsSelectionOwner())
    {
        if (!X11DRV_CLIPBOARD_GetClipboardInfo(lpcbinfo))
        {
            ERR("Failed to retrieve clipboard information.\n");
            bret = FALSE;
        }
        else if (wSeqNo < lpcbinfo->seqno)
        {
            X11DRV_EmptyClipboard(TRUE);

            if (X11DRV_CLIPBOARD_QueryAvailableData(lpcbinfo) < 0)
            {
                ERR("Failed to cache clipboard data owned by another process.\n");
                bret = FALSE;
            }
            else
            {
                X11DRV_EndClipboardUpdate();
            }

            wSeqNo = lpcbinfo->seqno;
        }
    }

    return bret;
}


/**************************************************************************
 *			X11DRV_CLIPBOARD_RenderFormat
 */
static BOOL X11DRV_CLIPBOARD_RenderFormat(LPWINE_CLIPDATA lpData)
{
    BOOL bret = TRUE;

    TRACE(" 0x%04x hData32(%p) hData16(0x%08x)\n",
        lpData->wFormatID, lpData->hData32, lpData->hData16);

    if (lpData->hData32 || lpData->hData16)
        return bret; /* Already rendered */

    if (lpData->wFlags & CF_FLAG_SYNTHESIZED)
        bret = X11DRV_CLIPBOARD_RenderSynthesizedFormat(lpData);
    else if (!X11DRV_CLIPBOARD_IsSelectionOwner())
    {
        if (!X11DRV_CLIPBOARD_ReadSelectionData(lpData))
        {
            ERR("Failed to cache clipboard data owned by another process. Format=%d\n", 
                lpData->wFormatID);
            bret = FALSE;
        }
    }
    else
    {
        CLIPBOARDINFO cbInfo;

        if (X11DRV_CLIPBOARD_GetClipboardInfo(&cbInfo) && cbInfo.hWndOwner)
        {
            /* Send a WM_RENDERFORMAT message to notify the owner to render the
             * data requested into the clipboard.
             */
            TRACE("Sending WM_RENDERFORMAT message to hwnd(%p)\n", cbInfo.hWndOwner);
            SendMessageW(cbInfo.hWndOwner, WM_RENDERFORMAT, (WPARAM)lpData->wFormatID, 0);

            if (!lpData->hData32 && !lpData->hData16)
                bret = FALSE;
        }
        else
        {
            ERR("hWndClipOwner is lost!\n");
            bret = FALSE;
        }
    }

    return bret;
}


/**************************************************************************
 *                      CLIPBOARD_ConvertText
 * Returns number of required/converted characters - not bytes!
 */
static INT CLIPBOARD_ConvertText(WORD src_fmt, void const *src, INT src_size,
				 WORD dst_fmt, void *dst, INT dst_size)
{
    UINT cp;

    if(src_fmt == CF_UNICODETEXT)
    {
	switch(dst_fmt)
	{
	case CF_TEXT:
	    cp = CP_ACP;
	    break;
	case CF_OEMTEXT:
	    cp = CP_OEMCP;
	    break;
	default:
	    return 0;
	}
	return WideCharToMultiByte(cp, 0, src, src_size, dst, dst_size, NULL, NULL);
    }

    if(dst_fmt == CF_UNICODETEXT)
    {
	switch(src_fmt)
	{
	case CF_TEXT:
	    cp = CP_ACP;
	    break;
	case CF_OEMTEXT:
	    cp = CP_OEMCP;
	    break;
	default:
	    return 0;
	}
	return MultiByteToWideChar(cp, 0, src, src_size, dst, dst_size);
    }

    if(!dst_size) return src_size;

    if(dst_size > src_size) dst_size = src_size;

    if(src_fmt == CF_TEXT )
	CharToOemBuffA(src, dst, dst_size);
    else
	OemToCharBuffA(src, dst, dst_size);

    return dst_size;
}


/**************************************************************************
 *                      X11DRV_CLIPBOARD_RenderSynthesizedFormat
 */
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedFormat(LPWINE_CLIPDATA lpData)
{
    BOOL bret = FALSE;

    TRACE("\n");

    if (lpData->wFlags & CF_FLAG_SYNTHESIZED)
    {
        UINT wFormatID = lpData->wFormatID;

        if (wFormatID == CF_UNICODETEXT || wFormatID == CF_TEXT || wFormatID == CF_OEMTEXT)
            bret = X11DRV_CLIPBOARD_RenderSynthesizedText(wFormatID);
        else 
        {
            switch (wFormatID)
	    {
                case CF_DIB:
                    bret = X11DRV_CLIPBOARD_RenderSynthesizedDIB();
                    break;

                case CF_BITMAP:
                    bret = X11DRV_CLIPBOARD_RenderSynthesizedBitmap();
                    break;

                case CF_ENHMETAFILE:
                case CF_METAFILEPICT:
		    FIXME("Synthesizing wFormatID(0x%08x) not implemented\n", wFormatID);
                    break;

                default:
		    FIXME("Called to synthesize unknown format\n");
                    break;
            }
        }

        lpData->wFlags &= ~CF_FLAG_SYNTHESIZED;
    }

    return bret;
}


/**************************************************************************
 *                      X11DRV_CLIPBOARD_RenderSynthesizedText
 *
 * Renders synthesized text
 */
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedText(UINT wFormatID)
{
    LPCSTR lpstrS;
    LPSTR  lpstrT;
    HANDLE hData32;
    INT src_chars, dst_chars, alloc_size;
    LPWINE_CLIPDATA lpSource = NULL;

    TRACE(" %d\n", wFormatID);

    if ((lpSource = X11DRV_CLIPBOARD_LookupData(wFormatID)) &&
        lpSource->hData32)
        return TRUE;

    /* Look for rendered source or non-synthesized source */
    if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_UNICODETEXT)) &&
        (!(lpSource->wFlags & CF_FLAG_SYNTHESIZED) || lpSource->hData32))
    {
        TRACE("UNICODETEXT -> %d\n", wFormatID);
    }
    else if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_TEXT)) &&
        (!(lpSource->wFlags & CF_FLAG_SYNTHESIZED) || lpSource->hData32))
    {
        TRACE("TEXT -> %d\n", wFormatID);
    }
    else if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_OEMTEXT)) &&
        (!(lpSource->wFlags & CF_FLAG_SYNTHESIZED) || lpSource->hData32))
    {
        TRACE("OEMTEXT -> %d\n", wFormatID);
    }

    if (!lpSource || (lpSource->wFlags & CF_FLAG_SYNTHESIZED &&
        !lpSource->hData32))
        return FALSE;

    /* Ask the clipboard owner to render the source text if necessary */
    if (!lpSource->hData32 && !X11DRV_CLIPBOARD_RenderFormat(lpSource))
        return FALSE;

    if (lpSource->hData32)
    {
        lpstrS = (LPSTR)GlobalLock(lpSource->hData32);
    }
    else
    {
        lpstrS = (LPSTR)GlobalLock16(lpSource->hData16);
    }

    if (!lpstrS)
        return FALSE;

    /* Text always NULL terminated */
    if(lpSource->wFormatID == CF_UNICODETEXT)
        src_chars = strlenW((LPCWSTR)lpstrS) + 1;
    else
        src_chars = strlen(lpstrS) + 1;

    /* Calculate number of characters in the destination buffer */
    dst_chars = CLIPBOARD_ConvertText(lpSource->wFormatID, lpstrS, 
        src_chars, wFormatID, NULL, 0);

    if (!dst_chars)
        return FALSE;

    TRACE("Converting from '%d' to '%d', %i chars\n",
    	lpSource->wFormatID, wFormatID, src_chars);

    /* Convert characters to bytes */
    if(wFormatID == CF_UNICODETEXT)
        alloc_size = dst_chars * sizeof(WCHAR);
    else
        alloc_size = dst_chars;

    hData32 = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | 
        GMEM_DDESHARE, alloc_size);

    lpstrT = (LPSTR)GlobalLock(hData32);

    if (lpstrT)
    {
        CLIPBOARD_ConvertText(lpSource->wFormatID, lpstrS, src_chars,
            wFormatID, lpstrT, dst_chars);
        GlobalUnlock(hData32);
    }

    /* Unlock source */
    if (lpSource->hData32)
        GlobalUnlock(lpSource->hData32);
    else
        GlobalUnlock16(lpSource->hData16);

    return X11DRV_CLIPBOARD_InsertClipboardData(wFormatID, 0, hData32, 0, NULL, TRUE);
}


/**************************************************************************
 *                      X11DRV_CLIPBOARD_RenderSynthesizedDIB
 *
 * Renders synthesized DIB
 */
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedDIB()
{
    BOOL bret = FALSE;
    LPWINE_CLIPDATA lpSource = NULL;

    TRACE("\n");

    if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_DIB)) && lpSource->hData32)
    {
        bret = TRUE;
    }
    /* If we have a bitmap and it's not synthesized or it has been rendered */
    else if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_BITMAP)) &&
        (!(lpSource->wFlags & CF_FLAG_SYNTHESIZED) || lpSource->hData32))
    {
        /* Render source if required */
        if (lpSource->hData32 || X11DRV_CLIPBOARD_RenderFormat(lpSource))
        {
            HDC hdc;
            HGLOBAL hData32;

            hdc = GetDC(NULL);
            hData32 = X11DRV_DIB_CreateDIBFromBitmap(hdc, lpSource->hData32);
            ReleaseDC(NULL, hdc);

            if (hData32)
            {
                X11DRV_CLIPBOARD_InsertClipboardData(CF_DIB, 0, hData32, 0, NULL, TRUE);
                bret = TRUE;
            }
        }
    }

    return bret;
}


/**************************************************************************
 *                      X11DRV_CLIPBOARD_RenderSynthesizedBitmap
 *
 * Renders synthesized bitmap
 */
static BOOL X11DRV_CLIPBOARD_RenderSynthesizedBitmap()
{
    BOOL bret = FALSE;
    LPWINE_CLIPDATA lpSource = NULL;

    TRACE("\n");

    if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_BITMAP)) && lpSource->hData32)
    {
        bret = TRUE;
    }
    /* If we have a dib and it's not synthesized or it has been rendered */
    else if ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_DIB)) &&
        (!(lpSource->wFlags & CF_FLAG_SYNTHESIZED) || lpSource->hData32))
    {
        /* Render source if required */
        if (lpSource->hData32 || X11DRV_CLIPBOARD_RenderFormat(lpSource))
        {
            HDC hdc;
            HBITMAP hData32;
            unsigned int offset;
            LPBITMAPINFOHEADER lpbmih;

            hdc = GetDC(NULL);
            lpbmih = (LPBITMAPINFOHEADER) GlobalLock(lpSource->hData32);

            offset = sizeof(BITMAPINFOHEADER)
                  + ((lpbmih->biBitCount <= 8) ? (sizeof(RGBQUAD) *
                    (1 << lpbmih->biBitCount)) : 0);

            hData32 = CreateDIBitmap(hdc, lpbmih, CBM_INIT, (LPBYTE)lpbmih +
                offset, (LPBITMAPINFO) lpbmih, DIB_RGB_COLORS);

            GlobalUnlock(lpSource->hData32);
            ReleaseDC(NULL, hdc);

            if (hData32)
            {
                X11DRV_CLIPBOARD_InsertClipboardData(CF_BITMAP, 0, hData32, 0, NULL, TRUE);
                bret = TRUE;
            }
        }
    }

    return bret;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ImportXAString
 *
 *  Import XA_STRING, converting the string to CF_TEXT.
 */
HANDLE X11DRV_CLIPBOARD_ImportXAString(Window w, Atom prop)
{
    LPBYTE lpdata;
    ULONG cbytes;
    LPSTR lpstr;
    UINT i, inlcount = 0;
    HANDLE hText = 0;

    if (!X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
        return 0;

    for (i = 0; i <= cbytes; i++)
    {
        if (lpdata[i] == '\n')
            inlcount++;
    }

    if ((hText = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbytes + inlcount + 1)))
    {
        lpstr = GlobalLock(hText);

        for (i = 0, inlcount = 0; i <= cbytes; i++)
        {
            if (lpdata[i] == '\n')
                lpstr[inlcount++] = '\r';

            lpstr[inlcount++] = lpdata[i];
        }

        GlobalUnlock(hText);
    }

    /* Free the retrieved property data */
    HeapFree(GetProcessHeap(), 0, lpdata);

    return hText;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ImportUTF8
 *
 *  Import XA_STRING, converting the string to CF_UNICODE.
 */
HANDLE X11DRV_CLIPBOARD_ImportUTF8(Window w, Atom prop)
{
    LPBYTE lpdata;
    ULONG cbytes;
    LPSTR lpstr;
    UINT i, inlcount = 0;
    HANDLE hUnicodeText = 0;

    if (!X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
        return 0;

    for (i = 0; i <= cbytes; i++)
    {
        if (lpdata[i] == '\n')
            inlcount++;
    }

    if ((lpstr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbytes + inlcount + 1)))
    {
        UINT count;

        for (i = 0, inlcount = 0; i <= cbytes; i++)
        {
            if (lpdata[i] == '\n')
                lpstr[inlcount++] = '\r';

            lpstr[inlcount++] = lpdata[i];
        }

        count = MultiByteToWideChar(CP_UTF8, 0, lpstr, -1, NULL, 0);
        hUnicodeText = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, count * sizeof(WCHAR));

        if (hUnicodeText)
        {
            WCHAR *textW = GlobalLock(hUnicodeText);
            MultiByteToWideChar(CP_UTF8, 0, lpstr, -1, textW, count);
            GlobalUnlock(hUnicodeText);
        }

        HeapFree(GetProcessHeap(), 0, lpstr);
    }

    /* Free the retrieved property data */
    HeapFree(GetProcessHeap(), 0, lpdata);

    return hUnicodeText;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ImportCompoundText
 *
 *  Import COMPOUND_TEXT to CF_UNICODE
 */
static HANDLE X11DRV_CLIPBOARD_ImportCompoundText(Window w, Atom prop)
{
    Display *display = thread_display();
    int i, j;
    char** srcstr;
    int count, lcount;
    int srclen, destlen;
    HANDLE hUnicodeText;
    XTextProperty txtprop;

    wine_tsx11_lock();
    if (!XGetTextProperty(display, w, &txtprop, prop))
    {
        wine_tsx11_unlock();
        return 0;
    }

    XmbTextPropertyToTextList(display, &txtprop, &srcstr, &count);
    wine_tsx11_unlock();

    TRACE("Importing %d line(s)\n", count);

    /* Compute number of lines */
    srclen = strlen(srcstr[0]);
    for (i = 0, lcount = 0; i <= srclen; i++)
    {
        if (srcstr[0][i] == '\n')
            lcount++;
    }

    destlen = MultiByteToWideChar(CP_UNIXCP, 0, srcstr[0], -1, NULL, 0);

    TRACE("lcount = %d, destlen=%d, srcstr %s\n", lcount, destlen, srcstr[0]);

    if ((hUnicodeText = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (destlen + lcount + 1) * sizeof(WCHAR))))
    {
        WCHAR *deststr = GlobalLock(hUnicodeText);
        MultiByteToWideChar(CP_UNIXCP, 0, srcstr[0], -1, deststr, destlen);

        if (lcount)
        {
            for (i = destlen - 1, j = destlen + lcount - 1; i >= 0; i--, j--)
            {
                deststr[j] = deststr[i];

                if (deststr[i] == '\n')
                    deststr[--j] = '\r';
            }
        }

        GlobalUnlock(hUnicodeText);
    }

    wine_tsx11_lock();
    XFreeStringList(srcstr);
    XFree(txtprop.value);
    wine_tsx11_unlock();

    return hUnicodeText;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ImportXAPIXMAP
 *
 *  Import XA_PIXMAP, converting the image to CF_DIB.
 */
HANDLE X11DRV_CLIPBOARD_ImportXAPIXMAP(Window w, Atom prop)
{
    HWND hwnd;
    HDC hdc;
    LPBYTE lpdata;
    ULONG cbytes;
    Pixmap *pPixmap;
    HANDLE hClipData = 0;

    if (X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
    {
        pPixmap = (Pixmap *) lpdata;

        hwnd = GetOpenClipboardWindow();
        hdc = GetDC(hwnd);

        hClipData = X11DRV_DIB_CreateDIBFromPixmap(*pPixmap, hdc);
        ReleaseDC(hwnd, hdc);

        /* Free the retrieved property data */
        HeapFree(GetProcessHeap(), 0, lpdata);
    }

    return hClipData;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ImportMetaFilePict
 *
 *  Import MetaFilePict.
 */
HANDLE X11DRV_CLIPBOARD_ImportMetaFilePict(Window w, Atom prop)
{
    LPBYTE lpdata;
    ULONG cbytes;
    HANDLE hClipData = 0;

    if (X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
    {
        if (cbytes)
            hClipData = X11DRV_CLIPBOARD_SerializeMetafile(CF_METAFILEPICT, (HANDLE)lpdata, (LPDWORD)&cbytes, FALSE);

        /* Free the retrieved property data */
        HeapFree(GetProcessHeap(), 0, lpdata);
    }

    return hClipData;
}


/**************************************************************************
 *		X11DRV_ImportEnhMetaFile
 *
 *  Import EnhMetaFile.
 */
HANDLE X11DRV_CLIPBOARD_ImportEnhMetaFile(Window w, Atom prop)
{
    LPBYTE lpdata;
    ULONG cbytes;
    HANDLE hClipData = 0;

    if (X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
    {
        if (cbytes)
            hClipData = X11DRV_CLIPBOARD_SerializeMetafile(CF_ENHMETAFILE, (HANDLE)lpdata, (LPDWORD)&cbytes, FALSE);

        /* Free the retrieved property data */
        HeapFree(GetProcessHeap(), 0, lpdata);
    }

    return hClipData;
}


/**************************************************************************
 *		X11DRV_ImportClipbordaData
 *
 *  Generic import clipboard data routine.
 */
HANDLE X11DRV_CLIPBOARD_ImportClipboardData(Window w, Atom prop)
{
    LPVOID lpClipData;
    LPBYTE lpdata;
    ULONG cbytes;
    HANDLE hClipData = 0;

    if (X11DRV_CLIPBOARD_ReadProperty(w, prop, &lpdata, &cbytes))
    {
        if (cbytes)
        {
            /* Turn on the DDESHARE flag to enable shared 32 bit memory */
            hClipData = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, cbytes);
            if (hClipData == 0)
                return NULL;

            if ((lpClipData = GlobalLock(hClipData)))
            {
                memcpy(lpClipData, lpdata, cbytes);
                GlobalUnlock(hClipData);
            }
            else
            {
                GlobalFree(hClipData);
                hClipData = 0;
            }
        }

        /* Free the retrieved property data */
        HeapFree(GetProcessHeap(), 0, lpdata);
    }

    return hClipData;
}


/**************************************************************************
 		X11DRV_CLIPBOARD_ExportClipboardData
 *
 *  Generic export clipboard data routine.
 */
HANDLE X11DRV_CLIPBOARD_ExportClipboardData(Window requestor, Atom aTarget,
    Atom rprop, LPWINE_CLIPDATA lpData, LPDWORD lpBytes)
{
    LPVOID lpClipData;
    UINT datasize = 0;
    HANDLE hClipData = 0;

    *lpBytes = 0; /* Assume failure */

    if (!X11DRV_CLIPBOARD_RenderFormat(lpData))
        ERR("Failed to export %d format\n", lpData->wFormatID);
    else
    {
        datasize = GlobalSize(lpData->hData32);

        hClipData = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, datasize);
        if (hClipData == 0) return NULL;

        if ((lpClipData = GlobalLock(hClipData)))
        {
            LPVOID lpdata = GlobalLock(lpData->hData32);

            memcpy(lpClipData, lpdata, datasize);
            *lpBytes = datasize;

            GlobalUnlock(lpData->hData32);
            GlobalUnlock(hClipData);
        } else {
            GlobalFree(hClipData);
            hClipData = 0;
        }
    }

    return hClipData;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportXAString
 *
 *  Export CF_TEXT converting the string to XA_STRING.
 *  Helper function for X11DRV_CLIPBOARD_ExportString.
 */
static HANDLE X11DRV_CLIPBOARD_ExportXAString(LPWINE_CLIPDATA lpData, LPDWORD lpBytes)
{
    UINT i, j;
    UINT size;
    LPSTR text, lpstr = NULL;

    *lpBytes = 0; /* Assume return has zero bytes */

    text = GlobalLock(lpData->hData32);
    size = strlen(text);

    /* remove carriage returns */
    lpstr = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, size + 1);
    if (lpstr == NULL)
        goto done;

    for (i = 0,j = 0; i < size && text[i]; i++)
    {
        if (text[i] == '\r' && (text[i+1] == '\n' || text[i+1] == '\0'))
            continue;
        lpstr[j++] = text[i];
    }

    lpstr[j]='\0';
    *lpBytes = j; /* Number of bytes in string */

done:
    HeapFree(GetProcessHeap(), 0, text);
    GlobalUnlock(lpData->hData32);

    return lpstr;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportUTF8String
 *
 *  Export CF_UNICODE converting the string to UTF8.
 *  Helper function for X11DRV_CLIPBOARD_ExportString.
 */
static HANDLE X11DRV_CLIPBOARD_ExportUTF8String(LPWINE_CLIPDATA lpData, LPDWORD lpBytes)
{
    UINT i, j;
    UINT size;
    LPWSTR uni_text;
    LPSTR text, lpstr = NULL;

    *lpBytes = 0; /* Assume return has zero bytes */

    uni_text = GlobalLock(lpData->hData32);

    size = WideCharToMultiByte(CP_UTF8, 0, uni_text, -1, NULL, 0, NULL, NULL);

    text = HeapAlloc(GetProcessHeap(), 0, size);
    if (!text)
        goto done;
    WideCharToMultiByte(CP_UTF8, 0, uni_text, -1, text, size, NULL, NULL);

    /* remove carriage returns */
    lpstr = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size--);
    if (lpstr == NULL)
        goto done;

    for (i = 0,j = 0; i < size && text[i]; i++)
    {
        if (text[i] == '\r' && (text[i+1] == '\n' || text[i+1] == '\0'))
            continue;
        lpstr[j++] = text[i];
    }
    lpstr[j]='\0';

    *lpBytes = j; /* Number of bytes in string */

done:
    HeapFree(GetProcessHeap(), 0, text);
    GlobalUnlock(lpData->hData32);

    return lpstr;
}



/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportCompoundText
 *
 *  Export CF_UNICODE to COMPOUND_TEXT
 *  Helper function for X11DRV_CLIPBOARD_ExportString.
 */
static HANDLE X11DRV_CLIPBOARD_ExportCompoundText(Window requestor, Atom aTarget, Atom rprop,
    LPWINE_CLIPDATA lpData, LPDWORD lpBytes)
{
    Display *display = thread_display();
    char* lpstr = 0;
    XTextProperty prop;
    XICCEncodingStyle style;
    UINT i, j;
    UINT size;
    LPWSTR uni_text;

    uni_text = GlobalLock(lpData->hData32);

    size = WideCharToMultiByte(CP_UNIXCP, 0, uni_text, -1, NULL, 0, NULL, NULL);
    lpstr = HeapAlloc(GetProcessHeap(), 0, size);
    if (!lpstr)
        return 0;

    WideCharToMultiByte(CP_UNIXCP, 0, uni_text, -1, lpstr, size, NULL, NULL);

    /* remove carriage returns */
    for (i = 0, j = 0; i < size && lpstr[i]; i++)
    {
        if (lpstr[i] == '\r' && (lpstr[i+1] == '\n' || lpstr[i+1] == '\0'))
            continue;
        lpstr[j++] = lpstr[i];
    }
    lpstr[j]='\0';

    GlobalUnlock(lpData->hData32);

    if (aTarget == x11drv_atom(COMPOUND_TEXT))
        style = XCompoundTextStyle;
    else
        style = XStdICCTextStyle;

    /* Update the X property */
    wine_tsx11_lock();
    if (XmbTextListToTextProperty(display, &lpstr, 1, style, &prop) == Success)
    {
        XSetTextProperty(display, requestor, &prop, rprop);
        XFree(prop.value);
    }
    wine_tsx11_unlock();

    HeapFree(GetProcessHeap(), 0, lpstr);

    return 0;
}

/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportString
 *
 *  Export string
 */
HANDLE X11DRV_CLIPBOARD_ExportString(Window requestor, Atom aTarget, Atom rprop,
    LPWINE_CLIPDATA lpData, LPDWORD lpBytes)
{
    if (X11DRV_CLIPBOARD_RenderFormat(lpData))
    {
        if (aTarget == XA_STRING)
            return X11DRV_CLIPBOARD_ExportXAString(lpData, lpBytes);
        else if (aTarget == x11drv_atom(COMPOUND_TEXT) || aTarget == x11drv_atom(TEXT))
            return X11DRV_CLIPBOARD_ExportCompoundText(requestor, aTarget,
                rprop, lpData, lpBytes);
        else
        {
            TRACE("Exporting target %ld to default UTF8_STRING\n", aTarget);
            return X11DRV_CLIPBOARD_ExportUTF8String(lpData, lpBytes);
        }
    }
    else
        ERR("Failed to render %d format\n", lpData->wFormatID);

    return 0;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportXAPIXMAP
 *
 *  Export CF_DIB to XA_PIXMAP.
 */
HANDLE X11DRV_CLIPBOARD_ExportXAPIXMAP(Window requestor, Atom aTarget, Atom rprop,
    LPWINE_CLIPDATA lpdata, LPDWORD lpBytes)
{
    HDC hdc;
    HANDLE hData;
    unsigned char* lpData;

    if (!X11DRV_CLIPBOARD_RenderFormat(lpdata))
    {
        ERR("Failed to export %d format\n", lpdata->wFormatID);
        return 0;
    }

    if (!lpdata->drvData) /* If not already rendered */
    {
        /* For convert from packed DIB to Pixmap */
        hdc = GetDC(0);
        lpdata->drvData = (UINT) X11DRV_DIB_CreatePixmapFromDIB(lpdata->hData32, hdc);
        ReleaseDC(0, hdc);
    }

    *lpBytes = sizeof(Pixmap); /* pixmap is a 32bit value */

    /* Wrap pixmap so we can return a handle */
    hData = GlobalAlloc(0, *lpBytes);
    lpData = GlobalLock(hData);
    memcpy(lpData, &lpdata->drvData, *lpBytes);
    GlobalUnlock(hData);

    return (HANDLE) hData;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportMetaFilePict
 *
 *  Export MetaFilePict.
 */
HANDLE X11DRV_CLIPBOARD_ExportMetaFilePict(Window requestor, Atom aTarget, Atom rprop,
    LPWINE_CLIPDATA lpdata, LPDWORD lpBytes)
{
    if (!X11DRV_CLIPBOARD_RenderFormat(lpdata))
    {
        ERR("Failed to export %d format\n", lpdata->wFormatID);
        return 0;
    }

    return X11DRV_CLIPBOARD_SerializeMetafile(CF_METAFILEPICT, (HANDLE)lpdata->hData32, 
        lpBytes, TRUE);
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ExportEnhMetaFile
 *
 *  Export EnhMetaFile.
 */
HANDLE X11DRV_CLIPBOARD_ExportEnhMetaFile(Window requestor, Atom aTarget, Atom rprop,
    LPWINE_CLIPDATA lpdata, LPDWORD lpBytes)
{
    if (!X11DRV_CLIPBOARD_RenderFormat(lpdata))
    {
        ERR("Failed to export %d format\n", lpdata->wFormatID);
        return 0;
    }

    return X11DRV_CLIPBOARD_SerializeMetafile(CF_ENHMETAFILE, (HANDLE)lpdata->hData32, 
        lpBytes, TRUE);
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_QueryTargets
 */
static BOOL X11DRV_CLIPBOARD_QueryTargets(Display *display, Window w, Atom selection,
    Atom target, XEvent *xe)
{
    INT i;
    Bool res;

    wine_tsx11_lock();
    XConvertSelection(display, selection, target,
        x11drv_atom(SELECTION_DATA), w, CurrentTime);
    wine_tsx11_unlock();

    /*
     * Wait until SelectionNotify is received
     */
    for (i = 0; i < SELECTION_RETRIES; i++)
    {
        wine_tsx11_lock();
        res = XCheckTypedWindowEvent(display, w, SelectionNotify, xe);
        wine_tsx11_unlock();
        if (res && xe->xselection.selection == selection) break;

        usleep(SELECTION_WAIT);
    }

    /* Verify that the selection returned a valid TARGETS property */
    if ((xe->xselection.target != target) || (xe->xselection.property == None))
    {
        /* Selection owner failed to respond or we missed the SelectionNotify */
        WARN("Failed to retrieve TARGETS for selection %ld.\n", selection);
        return FALSE;
    }

    return TRUE;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_InsertSelectionProperties
 *
 * Mark properties available for future retrieval.
 */
static VOID X11DRV_CLIPBOARD_InsertSelectionProperties(Display *display, Atom* properties, UINT count)
{
     UINT i, nb_atoms = 0;
     Atom *atoms = NULL;

     /* Cache these formats in the clipboard cache */
     for (i = 0; i < count; i++)
     {
         LPWINE_CLIPFORMAT lpFormat = X11DRV_CLIPBOARD_LookupProperty(NULL, properties[i]);

         if (lpFormat)
         {
             /* We found at least one Window's format that mapps to the property.
              * Continue looking for more.
              *
              * If more than one property map to a Window's format then we use the first 
              * one and ignore the rest.
              */
             while (lpFormat)
             {
                 TRACE("Atom#%d Property(%d): --> FormatID(%d) %s\n",
                       i, lpFormat->drvData, lpFormat->wFormatID, debugstr_w(lpFormat->Name));
                 X11DRV_CLIPBOARD_InsertClipboardData(lpFormat->wFormatID, 0, 0, 0, lpFormat, FALSE);
                 lpFormat = X11DRV_CLIPBOARD_LookupProperty(lpFormat, properties[i]);
             }
         }
         else
         {
             /* add it to the list of atoms that we don't know about yet */
             if (!atoms) atoms = HeapAlloc( GetProcessHeap(), 0,
                                            (count - i) * sizeof(*atoms) );
             if (atoms) atoms[nb_atoms++] = properties[i];
         }
     }

     /* query all unknown atoms in one go */
     if (atoms)
     {
         char **names = HeapAlloc( GetProcessHeap(), 0, nb_atoms * sizeof(*names) );
         if (names)
         {
             wine_tsx11_lock();
             XGetAtomNames( display, atoms, nb_atoms, names );
             wine_tsx11_unlock();
             for (i = 0; i < nb_atoms; i++)
             {
                 WINE_CLIPFORMAT *lpFormat;
                 LPWSTR wname;
                 int len = MultiByteToWideChar(CP_UNIXCP, 0, names[i], -1, NULL, 0);
                 wname = HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
                 MultiByteToWideChar(CP_UNIXCP, 0, names[i], -1, wname, len);

                 lpFormat = register_format( wname, atoms[i] );
                 HeapFree(GetProcessHeap(), 0, wname);
                 if (!lpFormat)
                 {
                     ERR("Failed to register %s property. Type will not be cached.\n", names[i]);
                     continue;
                 }
                 TRACE("Atom#%d Property(%d): --> FormatID(%d) %s\n",
                       i, lpFormat->drvData, lpFormat->wFormatID, debugstr_w(lpFormat->Name));
                 X11DRV_CLIPBOARD_InsertClipboardData(lpFormat->wFormatID, 0, 0, 0, lpFormat, FALSE);
             }
             wine_tsx11_lock();
             for (i = 0; i < nb_atoms; i++) XFree( names[i] );
             wine_tsx11_unlock();
             HeapFree( GetProcessHeap(), 0, names );
         }
         HeapFree( GetProcessHeap(), 0, atoms );
     }
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_QueryAvailableData
 *
 * Caches the list of data formats available from the current selection.
 * This queries the selection owner for the TARGETS property and saves all
 * reported property types.
 */
static int X11DRV_CLIPBOARD_QueryAvailableData(LPCLIPBOARDINFO lpcbinfo)
{
    Display *display = thread_display();
    XEvent         xe;
    Atom           atype=AnyPropertyType;
    int		   aformat;
    unsigned long  remain;
    Atom*	   targetList=NULL;
    Window         w;
    unsigned long  cSelectionTargets = 0;

    if (selectionAcquired & (S_PRIMARY | S_CLIPBOARD))
    {
        ERR("Received request to cache selection but process is owner=(%08x)\n", 
            (unsigned) selectionWindow);
        return -1; /* Prevent self request */
    }

    w = thread_selection_wnd();
    if (!w)
    {
        ERR("No window available to retrieve selection!\n");
        return -1;
    }

    /*
     * Query the selection owner for the TARGETS property
     */
    wine_tsx11_lock();
    if ((use_primary_selection && XGetSelectionOwner(display,XA_PRIMARY)) ||
        XGetSelectionOwner(display,x11drv_atom(CLIPBOARD)))
    {
        wine_tsx11_unlock();
        if (use_primary_selection && (X11DRV_CLIPBOARD_QueryTargets(display, w, XA_PRIMARY, x11drv_atom(TARGETS), &xe)))
            selectionCacheSrc = XA_PRIMARY;
        else if (X11DRV_CLIPBOARD_QueryTargets(display, w, x11drv_atom(CLIPBOARD), x11drv_atom(TARGETS), &xe))
            selectionCacheSrc = x11drv_atom(CLIPBOARD);
        else
        {
            Atom xstr = XA_PRIMARY;

            /* Selection Owner doesn't understand TARGETS, try retrieving XA_STRING */
            if (X11DRV_CLIPBOARD_QueryTargets(display, w, XA_PRIMARY, XA_STRING, &xe))
            {
                X11DRV_CLIPBOARD_InsertSelectionProperties(display, &xstr, 1);
                selectionCacheSrc = XA_PRIMARY;
                return 1;
            }
            else if (X11DRV_CLIPBOARD_QueryTargets(display, w, x11drv_atom(CLIPBOARD), XA_STRING, &xe))
            {
                X11DRV_CLIPBOARD_InsertSelectionProperties(display, &xstr, 1);
                selectionCacheSrc = x11drv_atom(CLIPBOARD);
                return 1;
            }
            else
            {
                WARN("Failed to query selection owner for available data.\n");
                return -1;
            }
        }
    }
    else /* No selection owner so report 0 targets available */
    {
        wine_tsx11_unlock();
        return 0;
    }

    /* Read the TARGETS property contents */
    wine_tsx11_lock();
    if(XGetWindowProperty(display, xe.xselection.requestor, xe.xselection.property,
        0, 0x3FFF, True, AnyPropertyType/*XA_ATOM*/, &atype, &aformat, &cSelectionTargets, 
        &remain, (unsigned char**)&targetList) != Success)
    {
        wine_tsx11_unlock();
        WARN("Failed to read TARGETS property\n");
    }
    else
    {
        wine_tsx11_unlock();
       TRACE("Type %lx,Format %d,nItems %ld, Remain %ld\n",
             atype, aformat, cSelectionTargets, remain);
       /*
        * The TARGETS property should have returned us a list of atoms
        * corresponding to each selection target format supported.
        */
       if ((atype == XA_ATOM || atype == x11drv_atom(TARGETS)) && aformat == 32)
           X11DRV_CLIPBOARD_InsertSelectionProperties(display, targetList, cSelectionTargets);

       /* Free the list of targets */
       wine_tsx11_lock();
       XFree(targetList);
       wine_tsx11_unlock();
    }

    return cSelectionTargets;
}


/**************************************************************************
 *	X11DRV_CLIPBOARD_ReadSelectionData
 *
 * This method is invoked only when we DO NOT own the X selection
 *
 * We always get the data from the selection client each time,
 * since we have no way of determining if the data in our cache is stale.
 */
static BOOL X11DRV_CLIPBOARD_ReadSelectionData(LPWINE_CLIPDATA lpData)
{
    Display *display = thread_display();
    Bool res;
    DWORD i;
    XEvent xe;
    BOOL bRet = FALSE;

    TRACE("%d\n", lpData->wFormatID);

    if (!lpData->lpFormat)
    {
        ERR("Requesting format %d but no source format linked to data.\n",
            lpData->wFormatID);
        return FALSE;
    }

    if (!selectionAcquired)
    {
        Window w = thread_selection_wnd();
        if(!w)
        {
            ERR("No window available to read selection data!\n");
            return FALSE;
        }

        TRACE("Requesting conversion of %s property (%d) from selection type %08x\n",
            debugstr_w(lpData->lpFormat->Name), lpData->lpFormat->drvData, (UINT)selectionCacheSrc);

        wine_tsx11_lock();
        XConvertSelection(display, selectionCacheSrc, lpData->lpFormat->drvData,
            x11drv_atom(SELECTION_DATA), w, CurrentTime);
        wine_tsx11_unlock();

        /* wait until SelectionNotify is received */
        for (i = 0; i < SELECTION_RETRIES; i++)
        {
            wine_tsx11_lock();
            res = XCheckTypedWindowEvent(display, w, SelectionNotify, &xe);
            wine_tsx11_unlock();
            if (res && xe.xselection.selection == selectionCacheSrc) break;

            usleep(SELECTION_WAIT);
        }

        /* Verify that the selection returned a valid TARGETS property */
        if (xe.xselection.property != None)
        {
            /*
             *  Read the contents of the X selection property 
             *  into WINE's clipboard cache and converting the 
             *  data format if necessary.
             */
             HANDLE hData = lpData->lpFormat->lpDrvImportFunc(xe.xselection.requestor,
                 xe.xselection.property);

             bRet = X11DRV_CLIPBOARD_InsertClipboardData(lpData->wFormatID, 0, hData, 0, lpData->lpFormat, TRUE);
        }
        else
        {
            TRACE("Failed to convert selection\n");
        }
    }
    else
    {
        ERR("Received request to cache selection data but process is owner\n");
    }

    TRACE("Returning %d\n", bRet);

    return bRet;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ReadProperty
 *  Reads the contents of the X selection property.
 */
static BOOL X11DRV_CLIPBOARD_ReadProperty(Window w, Atom prop,
    unsigned char** data, unsigned long* datasize)
{
    Display *display = thread_display();
    Atom atype = AnyPropertyType;
    int aformat;
    unsigned long total, nitems, remain, val_cnt;
    long  reqlen, bwc;
    unsigned char* val;
    unsigned char* buffer;

    if (prop == None)
        return FALSE;

    TRACE("Reading property %d from X window %d\n",
        (unsigned int)prop, (unsigned int)w);

    /*
     * First request a zero length in order to figure out the request size.
     */
    wine_tsx11_lock();
    if(XGetWindowProperty(display,w,prop,0,0,False, AnyPropertyType,
        &atype, &aformat, &nitems, &remain, &buffer) != Success)
    {
        wine_tsx11_unlock();
        WARN("Failed to get property size\n");
        return FALSE;
    }

    /* Free zero length return data if any */
    if (buffer)
    {
       XFree(buffer);
       buffer = NULL;
    }

    bwc = aformat/8;
    reqlen = remain * bwc;

    TRACE("Retrieving %ld bytes\n", reqlen);

    val = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, reqlen);

    /* Read property in 4K blocks */
    for (total = 0, val_cnt = 0; remain;)
    {
       if (XGetWindowProperty(display, w, prop, (total / 4), 4096, False,
           AnyPropertyType, &atype, &aformat, &nitems, &remain, &buffer) != Success)
       {
           wine_tsx11_unlock();
           WARN("Failed to read property\n");
           HeapFree(GetProcessHeap(), 0, val);
           return FALSE;
       }

       bwc = aformat/8;
       memcpy(&val[val_cnt], buffer, nitems * bwc);
       val_cnt += nitems * bwc;
       total += nitems*bwc;
       XFree(buffer);
    }

    /* Delete the property on the window now that we are done
     * This will send a PropertyNotify event to the selection owner. */
    XDeleteProperty(display, w, prop);

    wine_tsx11_unlock();

    *data = val;
    *datasize = total;

    return TRUE;
}


/**************************************************************************
 *		CLIPBOARD_SerializeMetafile
 */
static HANDLE X11DRV_CLIPBOARD_SerializeMetafile(INT wformat, HANDLE hdata, LPDWORD lpcbytes, BOOL out)
{
    HANDLE h = 0;

    TRACE(" wFormat=%d hdata=%p out=%d\n", wformat, hdata, out);

    if (out) /* Serialize out, caller should free memory */
    {
        *lpcbytes = 0; /* Assume failure */

        if (wformat == CF_METAFILEPICT)
        {
            LPMETAFILEPICT lpmfp = (LPMETAFILEPICT) GlobalLock(hdata);
            unsigned int size = GetMetaFileBitsEx(lpmfp->hMF, 0, NULL);

            h = GlobalAlloc(0, size + sizeof(METAFILEPICT));
            if (h)
            {
                char *pdata = GlobalLock(h);

                memcpy(pdata, lpmfp, sizeof(METAFILEPICT));
                GetMetaFileBitsEx(lpmfp->hMF, size, pdata + sizeof(METAFILEPICT));

                *lpcbytes = size + sizeof(METAFILEPICT);

                GlobalUnlock(h);
            }

            GlobalUnlock(hdata);
        }
        else if (wformat == CF_ENHMETAFILE)
        {
            int size = GetEnhMetaFileBits(hdata, 0, NULL);

            h = GlobalAlloc(0, size);
            if (h)
            {
                LPVOID pdata = GlobalLock(h);

                GetEnhMetaFileBits(hdata, size, pdata);
                *lpcbytes = size;

                GlobalUnlock(h);
            }
        }
    }
    else
    {
        if (wformat == CF_METAFILEPICT)
        {
            h = GlobalAlloc(0, sizeof(METAFILEPICT));
            if (h)
            {
                unsigned int wiresize, size;
                LPMETAFILEPICT lpmfp = (LPMETAFILEPICT) GlobalLock(h);

                memcpy(lpmfp, (LPVOID)hdata, sizeof(METAFILEPICT));
                wiresize = *lpcbytes - sizeof(METAFILEPICT);
                lpmfp->hMF = SetMetaFileBitsEx(wiresize,
                    ((const BYTE *)hdata) + sizeof(METAFILEPICT));
                size = GetMetaFileBitsEx(lpmfp->hMF, 0, NULL);
                GlobalUnlock(h);
            }
        }
        else if (wformat == CF_ENHMETAFILE)
        {
            h = SetEnhMetaFileBits(*lpcbytes, (LPVOID)hdata);
        }
    }

    return h;
}


/**************************************************************************
 *		X11DRV_CLIPBOARD_ReleaseSelection
 *
 * Release XA_CLIPBOARD and XA_PRIMARY in response to a SelectionClear event.
 */
static void X11DRV_CLIPBOARD_ReleaseSelection(Atom selType, Window w, HWND hwnd, Time time)
{
    Display *display = thread_display();

    /* w is the window that lost the selection
     */
    TRACE("event->window = %08x (selectionWindow = %08x) selectionAcquired=0x%08x\n",
	  (unsigned)w, (unsigned)selectionWindow, (unsigned)selectionAcquired);

    if (selectionAcquired && (w == selectionWindow))
    {
        CLIPBOARDINFO cbinfo;

        /* completely give up the selection */
        TRACE("Lost CLIPBOARD (+PRIMARY) selection\n");

        X11DRV_CLIPBOARD_GetClipboardInfo(&cbinfo);

        if (cbinfo.flags & CB_PROCESS)
        {
            /* Since we're still the owner, this wasn't initiated by
               another Wine process */
            if (OpenClipboard(hwnd))
            {
                /* Destroy private objects */
                SendMessageW(cbinfo.hWndOwner, WM_DESTROYCLIPBOARD, 0, 0);

                /* Give up ownership of the windows clipboard */
                X11DRV_CLIPBOARD_ReleaseOwnership();
                CloseClipboard();
            }
        }

        if ((selType == x11drv_atom(CLIPBOARD)) && (selectionAcquired & S_PRIMARY))
        {
            TRACE("Lost clipboard. Check if we need to release PRIMARY\n");

            wine_tsx11_lock();
            if (selectionWindow == XGetSelectionOwner(display, XA_PRIMARY))
            {
                TRACE("We still own PRIMARY. Releasing PRIMARY.\n");
                XSetSelectionOwner(display, XA_PRIMARY, None, time);
            }
            else
                TRACE("We no longer own PRIMARY\n");
            wine_tsx11_unlock();
        }
        else if ((selType == XA_PRIMARY) && (selectionAcquired & S_CLIPBOARD))
        {
            TRACE("Lost PRIMARY. Check if we need to release CLIPBOARD\n");

            wine_tsx11_lock();
            if (selectionWindow == XGetSelectionOwner(display,x11drv_atom(CLIPBOARD)))
            {
                TRACE("We still own CLIPBOARD. Releasing CLIPBOARD.\n");
                XSetSelectionOwner(display, x11drv_atom(CLIPBOARD), None, time);
            }
            else
                TRACE("We no longer own CLIPBOARD\n");
            wine_tsx11_unlock();
        }

        selectionWindow = None;

        X11DRV_EmptyClipboard(FALSE);

        /* Reset the selection flags now that we are done */
        selectionAcquired = S_NOSELECTION;
    }
}


/**************************************************************************
 *		IsSelectionOwner (X11DRV.@)
 *
 * Returns: TRUE if the selection is owned by this process, FALSE otherwise
 */
static BOOL X11DRV_CLIPBOARD_IsSelectionOwner(void)
{
    return selectionAcquired;
}


/**************************************************************************
 *                X11DRV Clipboard Exports
 **************************************************************************/


/**************************************************************************
 *		RegisterClipboardFormat (X11DRV.@)
 *
 * Registers a custom X clipboard format
 * Returns: Format id or 0 on failure
 */
UINT X11DRV_RegisterClipboardFormat(LPCWSTR FormatName)
{
    LPWINE_CLIPFORMAT lpFormat;

    if (FormatName == NULL) return 0;
    if (!(lpFormat = register_format( FormatName, 0 ))) return 0;
    return lpFormat->wFormatID;
}


/**************************************************************************
 *		X11DRV_GetClipboardFormatName
 */
INT X11DRV_GetClipboardFormatName(UINT wFormat, LPWSTR retStr, INT maxlen)
{
    LPWINE_CLIPFORMAT lpFormat;

    TRACE("(%04X, %p, %d) !\n", wFormat, retStr, maxlen);

    if (wFormat < 0xc000)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0;
    }

    lpFormat = X11DRV_CLIPBOARD_LookupFormat(wFormat);

    if (!lpFormat || (lpFormat->wFlags & CF_FLAG_BUILTINFMT))
    {
        TRACE("Unknown format 0x%08x!\n", wFormat);
        SetLastError(ERROR_INVALID_HANDLE);
        return 0;
    }

    lstrcpynW(retStr, lpFormat->Name, maxlen);

    return strlenW(retStr);
}


/**************************************************************************
 *		AcquireClipboard (X11DRV.@)
 */
void X11DRV_AcquireClipboard(HWND hWndClipWindow)
{
    Display *display = thread_display();

    /*
     * Acquire X selection if we don't already own it.
     * Note that we only acquire the selection if it hasn't been already
     * acquired by us, and ignore the fact that another X window may be
     * asserting ownership. The reason for this is we need *any* top level
     * X window to hold selection ownership. The actual clipboard data requests
     * are made via GetClipboardData from EVENT_SelectionRequest and this
     * ensures that the real HWND owner services the request.
     * If the owning X window gets destroyed the selection ownership is
     * re-cycled to another top level X window in X11DRV_CLIPBOARD_ResetOwner.
     *
     */
    if (!(selectionAcquired == (S_PRIMARY | S_CLIPBOARD)))
    {
        Window owner;

        if (!hWndClipWindow)
            hWndClipWindow = GetActiveWindow();

        hWndClipWindow = GetAncestor(hWndClipWindow, GA_ROOT);

        if (GetCurrentThreadId() != GetWindowThreadProcessId(hWndClipWindow, NULL))
        {
            TRACE("Thread %lx is acquiring selection with thread %lx's window %p\n",
                GetCurrentThreadId(),
                GetWindowThreadProcessId(hWndClipWindow, NULL),
                hWndClipWindow);
            if (!SendMessageW(hWndClipWindow, WM_X11DRV_ACQUIRE_SELECTION, 0, 0))
                ERR("Failed to acquire selection\n");
            return;
        }

        owner = X11DRV_get_whole_window(hWndClipWindow);

        wine_tsx11_lock();
        /* Grab PRIMARY selection if not owned */
        if (use_primary_selection && !(selectionAcquired & S_PRIMARY))
            XSetSelectionOwner(display, XA_PRIMARY, owner, CurrentTime);

        /* Grab CLIPBOARD selection if not owned */
        if (!(selectionAcquired & S_CLIPBOARD))
            XSetSelectionOwner(display, x11drv_atom(CLIPBOARD), owner, CurrentTime);

        if (use_primary_selection && XGetSelectionOwner(display,XA_PRIMARY) == owner)
	    selectionAcquired |= S_PRIMARY;

        if (XGetSelectionOwner(display,x11drv_atom(CLIPBOARD)) == owner)
	    selectionAcquired |= S_CLIPBOARD;
        wine_tsx11_unlock();

        if (selectionAcquired)
        {
	    selectionWindow = owner;
	    TRACE("Grabbed X selection, owner=(%08x)\n", (unsigned) owner);
        }
    }
    else
    {
        ERR("Received request to acquire selection but process is already owner=(%08x)\n", (unsigned) selectionWindow);
    }
}


/**************************************************************************
 *	X11DRV_EmptyClipboard
 *
 * Empty cached clipboard data. 
 */
void X11DRV_EmptyClipboard(BOOL keepunowned)
{
    if (ClipData)
    {
        LPWINE_CLIPDATA lpData, lpStart;
        LPWINE_CLIPDATA lpNext = ClipData;

        TRACE(" called with %d entries in cache.\n", ClipDataCount);

        do
        {
            lpStart = ClipData;
            lpData = lpNext;
            lpNext = lpData->NextData;

            if (!keepunowned || !(lpData->wFlags & CF_FLAG_UNOWNED))
            {
            lpData->PrevData->NextData = lpData->NextData;
            lpData->NextData->PrevData = lpData->PrevData;

                if (lpData == ClipData)
                    ClipData = lpNext != lpData ? lpNext : NULL;

            X11DRV_CLIPBOARD_FreeData(lpData);
            HeapFree(GetProcessHeap(), 0, lpData);

                ClipDataCount--;
            }
        } while (lpNext != lpStart);
    }

    TRACE(" %d entries remaining in cache.\n", ClipDataCount);
}



/**************************************************************************
 *		X11DRV_SetClipboardData
 */
BOOL X11DRV_SetClipboardData(UINT wFormat, HANDLE16 hData16, HANDLE hData32, BOOL owner)
{
    DWORD flags = 0;
    BOOL bResult = TRUE;

    /* If it's not owned, data can only be set if the format data is not already owned
       and its rendering is not delayed */
    if (!owner)
    {
        CLIPBOARDINFO cbinfo;
        LPWINE_CLIPDATA lpRender;

        X11DRV_CLIPBOARD_UpdateCache(&cbinfo);

        if ((!hData16 && !hData32) ||
            ((lpRender = X11DRV_CLIPBOARD_LookupData(wFormat)) &&
            !(lpRender->wFlags & CF_FLAG_UNOWNED)))
            bResult = FALSE;
        else
            flags = CF_FLAG_UNOWNED;
    }

    bResult &= X11DRV_CLIPBOARD_InsertClipboardData(wFormat, hData16, hData32, flags, NULL, TRUE);

    return bResult;
}


/**************************************************************************
 *		CountClipboardFormats
 */
INT X11DRV_CountClipboardFormats(void)
{
    CLIPBOARDINFO cbinfo;

    X11DRV_CLIPBOARD_UpdateCache(&cbinfo);

    TRACE(" count=%d\n", ClipDataCount);

    return ClipDataCount;
}


/**************************************************************************
 *		X11DRV_EnumClipboardFormats
 */
UINT X11DRV_EnumClipboardFormats(UINT wFormat)
{
    CLIPBOARDINFO cbinfo;
    UINT wNextFormat = 0;

    TRACE("(%04X)\n", wFormat);

    X11DRV_CLIPBOARD_UpdateCache(&cbinfo);

    if (!wFormat)
    {
        if (ClipData)
            wNextFormat = ClipData->wFormatID;
    }
    else
    {
        LPWINE_CLIPDATA lpData = X11DRV_CLIPBOARD_LookupData(wFormat);

        if (lpData && lpData->NextData != ClipData)
            wNextFormat = lpData->NextData->wFormatID;
    }

    return wNextFormat;
}


/**************************************************************************
 *		X11DRV_IsClipboardFormatAvailable
 */
BOOL X11DRV_IsClipboardFormatAvailable(UINT wFormat)
{
    BOOL bRet = FALSE;
    CLIPBOARDINFO cbinfo;

    TRACE("(%04X)\n", wFormat);

    X11DRV_CLIPBOARD_UpdateCache(&cbinfo);

    if (wFormat != 0 && X11DRV_CLIPBOARD_LookupData(wFormat))
        bRet = TRUE;

    TRACE("(%04X)- ret(%d)\n", wFormat, bRet);

    return bRet;
}


/**************************************************************************
 *		GetClipboardData (USER.142)
 */
BOOL X11DRV_GetClipboardData(UINT wFormat, HANDLE16* phData16, HANDLE* phData32)
{
    CLIPBOARDINFO cbinfo;
    LPWINE_CLIPDATA lpRender;

    TRACE("(%04X)\n", wFormat);

    X11DRV_CLIPBOARD_UpdateCache(&cbinfo);

    if ((lpRender = X11DRV_CLIPBOARD_LookupData(wFormat)))
    {
        if ( !lpRender->hData32 )
            X11DRV_CLIPBOARD_RenderFormat(lpRender);

        /* Convert between 32 -> 16 bit data, if necessary */
        if (lpRender->hData32 && !lpRender->hData16)
        {
            int size;

            if (lpRender->wFormatID == CF_METAFILEPICT)
                size = sizeof(METAFILEPICT16);
            else
                size = GlobalSize(lpRender->hData32);

            lpRender->hData16 = GlobalAlloc16(GMEM_ZEROINIT, size);

            if (!lpRender->hData16)
                ERR("(%04X) -- not enough memory in 16b heap\n", wFormat);
            else
            {
                if (lpRender->wFormatID == CF_METAFILEPICT)
                {
                    FIXME("\timplement function CopyMetaFilePict32to16\n");
                    FIXME("\tin the appropriate file.\n");
#ifdef SOMEONE_IMPLEMENTED_ME
                    CopyMetaFilePict32to16(GlobalLock16(lpRender->hData16),
                        GlobalLock(lpRender->hData32));
#endif
                }
                else
                {
                    memcpy(GlobalLock16(lpRender->hData16),
                        GlobalLock(lpRender->hData32), size);
                }

                GlobalUnlock16(lpRender->hData16);
                GlobalUnlock(lpRender->hData32);
            }
        }

        /* Convert between 32 -> 16 bit data, if necessary */
        if (lpRender->hData16 && !lpRender->hData32)
        {
            int size;

            if (lpRender->wFormatID == CF_METAFILEPICT)
                size = sizeof(METAFILEPICT16);
            else
                size = GlobalSize(lpRender->hData32);

            lpRender->hData32 = GlobalAlloc(GMEM_ZEROINIT | GMEM_MOVEABLE | 
                GMEM_DDESHARE, size);

            if (lpRender->wFormatID == CF_METAFILEPICT)
            {
                FIXME("\timplement function CopyMetaFilePict16to32\n");
                FIXME("\tin the appropriate file.\n");
#ifdef SOMEONE_IMPLEMENTED_ME
                CopyMetaFilePict16to32(GlobalLock16(lpRender->hData32),
                    GlobalLock(lpRender->hData16));
#endif
            }
            else
            {
                memcpy(GlobalLock(lpRender->hData32),
                    GlobalLock16(lpRender->hData16), size);
            }

            GlobalUnlock(lpRender->hData32);
            GlobalUnlock16(lpRender->hData16);
        }

        if (phData16)
            *phData16 = lpRender->hData16;

        if (phData32)
            *phData32 = lpRender->hData32;

        TRACE(" returning hData16(%04x) hData32(%p) (type %d)\n",
            lpRender->hData16, lpRender->hData32, lpRender->wFormatID);

        return lpRender->hData16 || lpRender->hData32;
    }

    return 0;
}


/**************************************************************************
 *		ResetSelectionOwner (X11DRV.@)
 *
 * Called from DestroyWindow() to prevent X selection from being lost when
 * a top level window is destroyed, by switching ownership to another top
 * level window.
 * Any top level window can own the selection. See X11DRV_CLIPBOARD_Acquire
 * for a more detailed description of this.
 */
void X11DRV_ResetSelectionOwner(HWND hwnd, BOOL bFooBar)
{
    Display *display = thread_display();
    HWND hWndClipOwner = 0;
    HWND tmp;
    Window XWnd = X11DRV_get_whole_window(hwnd);
    BOOL bLostSelection = FALSE;
    Window selectionPrevWindow;

    /* There is nothing to do if we don't own the selection,
     * or if the X window which currently owns the selection is different
     * from the one passed in.
     */
    if (!selectionAcquired || XWnd != selectionWindow
         || selectionWindow == None )
       return;

    if ((bFooBar && XWnd) || (!bFooBar && !XWnd))
       return;

    hWndClipOwner = GetClipboardOwner();

    TRACE("clipboard owner = %p, selection window = %08x\n",
          hWndClipOwner, (unsigned)selectionWindow);

    /* now try to salvage current selection from being destroyed by X */
    TRACE("checking %08x\n", (unsigned) XWnd);

    selectionPrevWindow = selectionWindow;
    selectionWindow = None;

    if (!(tmp = GetWindow(hwnd, GW_HWNDNEXT)))
        tmp = GetWindow(hwnd, GW_HWNDFIRST);

    if (tmp && tmp != hwnd) 
        selectionWindow = X11DRV_get_whole_window(tmp);

    if (selectionWindow != None)
    {
        /* We must pretend that we don't own the selection while making the switch
         * since a SelectionClear event will be sent to the last owner.
         * If there is no owner X11DRV_CLIPBOARD_ReleaseSelection will do nothing.
         */
        int saveSelectionState = selectionAcquired;
        selectionAcquired = S_NOSELECTION;

        TRACE("\tswitching selection from %08x to %08x\n",
                    (unsigned)selectionPrevWindow, (unsigned)selectionWindow);

        wine_tsx11_lock();

        /* Assume ownership for the PRIMARY and CLIPBOARD selection */
        if (saveSelectionState & S_PRIMARY)
            XSetSelectionOwner(display, XA_PRIMARY, selectionWindow, CurrentTime);

        XSetSelectionOwner(display, x11drv_atom(CLIPBOARD), selectionWindow, CurrentTime);

        /* Restore the selection masks */
        selectionAcquired = saveSelectionState;

        /* Lose the selection if something went wrong */
        if (((saveSelectionState & S_PRIMARY) &&
           (XGetSelectionOwner(display, XA_PRIMARY) != selectionWindow)) || 
           (XGetSelectionOwner(display, x11drv_atom(CLIPBOARD)) != selectionWindow))
        {
            bLostSelection = TRUE;
        }
        wine_tsx11_unlock();
    }
    else
    {
        bLostSelection = TRUE;
    }

    if (bLostSelection)
    {
        TRACE("Lost the selection!\n");

        X11DRV_CLIPBOARD_ReleaseOwnership();
        selectionAcquired = S_NOSELECTION;
        selectionWindow = 0;
    }
}


/**************************************************************************
 *                      X11DRV_CLIPBOARD_SynthesizeData
 */
static BOOL X11DRV_CLIPBOARD_SynthesizeData(UINT wFormatID)
{
    BOOL bsyn = TRUE;
    LPWINE_CLIPDATA lpSource = NULL;

    TRACE(" %d\n", wFormatID);

    /* Don't need to synthesize if it already exists */
    if (X11DRV_CLIPBOARD_LookupData(wFormatID))
        return TRUE;

    if (wFormatID == CF_UNICODETEXT || wFormatID == CF_TEXT || wFormatID == CF_OEMTEXT)
    {
        bsyn = ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_UNICODETEXT)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED) ||
            ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_TEXT)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED) ||
            ((lpSource = X11DRV_CLIPBOARD_LookupData(CF_OEMTEXT)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED);
    }
    else if (wFormatID == CF_ENHMETAFILE)
    {
        bsyn = (lpSource = X11DRV_CLIPBOARD_LookupData(CF_METAFILEPICT)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED;
    }
    else if (wFormatID == CF_METAFILEPICT)
    {
        bsyn = (lpSource = X11DRV_CLIPBOARD_LookupData(CF_METAFILEPICT)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED;
    }
    else if (wFormatID == CF_DIB)
    {
        bsyn = (lpSource = X11DRV_CLIPBOARD_LookupData(CF_BITMAP)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED;
    }
    else if (wFormatID == CF_BITMAP)
    {
        bsyn = (lpSource = X11DRV_CLIPBOARD_LookupData(CF_DIB)) &&
            ~lpSource->wFlags & CF_FLAG_SYNTHESIZED;
    }

    if (bsyn)
        X11DRV_CLIPBOARD_InsertClipboardData(wFormatID, 0, 0, CF_FLAG_SYNTHESIZED, NULL, TRUE);

    return bsyn;
}



/**************************************************************************
 *              X11DRV_EndClipboardUpdate
 * TODO:
 *  Add locale if it hasn't already been added
 */
void X11DRV_EndClipboardUpdate(void)
{
    INT count = ClipDataCount;

    /* Do Unicode <-> Text <-> OEM mapping */
    X11DRV_CLIPBOARD_SynthesizeData(CF_UNICODETEXT);
    X11DRV_CLIPBOARD_SynthesizeData(CF_TEXT);
    X11DRV_CLIPBOARD_SynthesizeData(CF_OEMTEXT);

    /* Enhmetafile <-> MetafilePict mapping */
    X11DRV_CLIPBOARD_SynthesizeData(CF_ENHMETAFILE);
    X11DRV_CLIPBOARD_SynthesizeData(CF_METAFILEPICT);

    /* DIB <-> Bitmap mapping */
    X11DRV_CLIPBOARD_SynthesizeData(CF_DIB);
    X11DRV_CLIPBOARD_SynthesizeData(CF_BITMAP);

    TRACE("%d formats added to cached data\n", ClipDataCount - count);
}


/***********************************************************************
 *           X11DRV_SelectionRequest_TARGETS
 *  Service a TARGETS selection request event
 */
static Atom X11DRV_SelectionRequest_TARGETS( Display *display, Window requestor,
                                             Atom target, Atom rprop )
{
    UINT i;
    Atom* targets;
    ULONG cTargets;
    LPWINE_CLIPFORMAT lpFormats;
    LPWINE_CLIPDATA lpData;

    /*
     * Count the number of items we wish to expose as selection targets.
     */
    cTargets = 1; /* Include TARGETS */

    lpData = ClipData;

    do
    {
        lpFormats = ClipFormats;

        while (lpFormats)
        {
            if ((lpFormats->wFormatID == lpData->wFormatID) &&
                lpFormats->lpDrvExportFunc)
                cTargets++;

            lpFormats = lpFormats->NextFormat;
        }

        lpData = lpData->NextData;
    }
    while (lpData != ClipData);

    TRACE(" found %ld formats\n", cTargets);

    /* Allocate temp buffer */
    targets = HeapAlloc( GetProcessHeap(), 0, cTargets * sizeof(Atom));
    if(targets == NULL)
        return None;

    i = 0;
    lpData = ClipData;
    targets[i++] = x11drv_atom(TARGETS);

    do
    {
        lpFormats = ClipFormats;

        while (lpFormats)
        {
            if ((lpFormats->wFormatID == lpData->wFormatID) &&
                lpFormats->lpDrvExportFunc)
                targets[i++] = lpFormats->drvData;

            lpFormats = lpFormats->NextFormat;
        }

        lpData = lpData->NextData;
    }
    while (lpData != ClipData);

    wine_tsx11_lock();

    if (TRACE_ON(clipboard))
    {
        unsigned int i;
        for ( i = 0; i < cTargets; i++)
        {
            if (targets[i])
            {
                char *itemFmtName = XGetAtomName(display, targets[i]);
                TRACE("\tAtom# %d:  Property %ld Type %s\n", i, targets[i], itemFmtName);
                XFree(itemFmtName);
            }
        }
    }

    /* We may want to consider setting the type to xaTargets instead,
     * in case some apps expect this instead of XA_ATOM */
    XChangeProperty(display, requestor, rprop, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)targets, cTargets);
    wine_tsx11_unlock();

    HeapFree(GetProcessHeap(), 0, targets);

    return rprop;
}


/***********************************************************************
 *           X11DRV_SelectionRequest_MULTIPLE
 *  Service a MULTIPLE selection request event
 *  rprop contains a list of (target,property) atom pairs.
 *  The first atom names a target and the second names a property.
 *  The effect is as if we have received a sequence of SelectionRequest events
 *  (one for each atom pair) except that:
 *  1. We reply with a SelectionNotify only when all the requested conversions
 *  have been performed.
 *  2. If we fail to convert the target named by an atom in the MULTIPLE property,
 *  we replace the atom in the property by None.
 */
static Atom X11DRV_SelectionRequest_MULTIPLE( HWND hWnd, XSelectionRequestEvent *pevent )
{
    Display *display = pevent->display;
    Atom           rprop;
    Atom           atype=AnyPropertyType;
    int            aformat;
    unsigned long  remain;
    Atom*          targetPropList=NULL;
    unsigned long  cTargetPropList = 0;

    /* If the specified property is None the requestor is an obsolete client.
     * We support these by using the specified target atom as the reply property.
     */
    rprop = pevent->property;
    if( rprop == None )
        rprop = pevent->target;
    if (!rprop)
        return 0;

    /* Read the MULTIPLE property contents. This should contain a list of
     * (target,property) atom pairs.
     */
    wine_tsx11_lock();
    if(XGetWindowProperty(display, pevent->requestor, rprop,
                          0, 0x3FFF, False, AnyPropertyType, &atype,&aformat,
                          &cTargetPropList, &remain,
                          (unsigned char**)&targetPropList) != Success)
    {
        wine_tsx11_unlock();
        TRACE("\tCouldn't read MULTIPLE property\n");
    }
    else
    {
        TRACE("\tType %s,Format %d,nItems %ld, Remain %ld\n",
              XGetAtomName(display, atype), aformat, cTargetPropList, remain);
        wine_tsx11_unlock();

        /*
         * Make sure we got what we expect.
         * NOTE: According to the X-ICCCM Version 2.0 documentation the property sent
         * in a MULTIPLE selection request should be of type ATOM_PAIR.
         * However some X apps(such as XPaint) are not compliant with this and return
         * a user defined atom in atype when XGetWindowProperty is called.
         * The data *is* an atom pair but is not denoted as such.
         */
        if(aformat == 32 /* atype == xAtomPair */ )
        {
            unsigned int i;

            /* Iterate through the ATOM_PAIR list and execute a SelectionRequest
             * for each (target,property) pair */

            for (i = 0; i < cTargetPropList; i+=2)
            {
                XSelectionRequestEvent event;

                if (TRACE_ON(clipboard))
                {
                    char *targetName, *propName;
                    wine_tsx11_lock();
                    targetName = XGetAtomName(display, targetPropList[i]);
                    propName = XGetAtomName(display, targetPropList[i+1]);
                    TRACE("MULTIPLE(%d): Target='%s' Prop='%s'\n",
                          i/2, targetName, propName);
                    XFree(targetName);
                    XFree(propName);
                    wine_tsx11_unlock();
                }

                /* We must have a non "None" property to service a MULTIPLE target atom */
                if ( !targetPropList[i+1] )
                {
                    TRACE("\tMULTIPLE(%d): Skipping target with empty property!\n", i);
                    continue;
                }

                /* Set up an XSelectionRequestEvent for this (target,property) pair */
                memcpy( &event, pevent, sizeof(XSelectionRequestEvent) );
                event.target = targetPropList[i];
                event.property = targetPropList[i+1];

                /* Fire a SelectionRequest, informing the handler that we are processing
                 * a MULTIPLE selection request event.
                 */
                X11DRV_HandleSelectionRequest( hWnd, &event, TRUE );
            }
        }

        /* Free the list of targets/properties */
        wine_tsx11_lock();
        XFree(targetPropList);
        wine_tsx11_unlock();
    }

    return rprop;
}


/***********************************************************************
 *           X11DRV_HandleSelectionRequest
 *  Process an event selection request event.
 *  The bIsMultiple flag is used to signal when EVENT_SelectionRequest is called
 *  recursively while servicing a "MULTIPLE" selection target.
 *
 *  Note: We only receive this event when WINE owns the X selection
 */
static void X11DRV_HandleSelectionRequest( HWND hWnd, XSelectionRequestEvent *event, BOOL bIsMultiple )
{
    Display *display = event->display;
    XSelectionEvent result;
    Atom rprop = None;
    Window request = event->requestor;

    TRACE("\n");

    /*
     * We can only handle the selection request if :
     * The selection is PRIMARY or CLIPBOARD, AND we can successfully open the clipboard.
     * Don't do these checks or open the clipboard while recursively processing MULTIPLE,
     * since this has been already done.
     */
    if ( !bIsMultiple )
    {
        if (((event->selection != XA_PRIMARY) && (event->selection != x11drv_atom(CLIPBOARD))))
            goto END;
    }

    /* If the specified property is None the requestor is an obsolete client.
     * We support these by using the specified target atom as the reply property.
     */
    rprop = event->property;
    if( rprop == None )
        rprop = event->target;

    if(event->target == x11drv_atom(TARGETS))  /*  Return a list of all supported targets */
    {
        /* TARGETS selection request */
        rprop = X11DRV_SelectionRequest_TARGETS( display, request, event->target, rprop );
    }
    else if(event->target == x11drv_atom(MULTIPLE))  /*  rprop contains a list of (target, property) atom pairs */
    {
        /* MULTIPLE selection request */
        rprop = X11DRV_SelectionRequest_MULTIPLE( hWnd, event );
    }
    else
    {
        LPWINE_CLIPFORMAT lpFormat = X11DRV_CLIPBOARD_LookupProperty(NULL, event->target);

        if (lpFormat && lpFormat->lpDrvExportFunc)
        {
            LPWINE_CLIPDATA lpData = X11DRV_CLIPBOARD_LookupData(lpFormat->wFormatID);

            if (lpData)
            {
                unsigned char* lpClipData;
                DWORD cBytes;
                HANDLE hClipData = lpFormat->lpDrvExportFunc(request, event->target,
                                                             rprop, lpData, &cBytes);

                if (hClipData && (lpClipData = GlobalLock(hClipData)))
                {
                    TRACE("\tUpdating property %s, %ld bytes\n", debugstr_w(lpFormat->Name), cBytes);

                    wine_tsx11_lock();
                    XChangeProperty(display, request, rprop, event->target,
                                    8, PropModeReplace, (unsigned char *)lpClipData, cBytes);
                    wine_tsx11_unlock();

                    GlobalUnlock(hClipData);
                    GlobalFree(hClipData);
                }
            }
        }
    }

END:
    /* reply to sender
     * SelectionNotify should be sent only at the end of a MULTIPLE request
     */
    if ( !bIsMultiple )
    {
        result.type = SelectionNotify;
        result.display = display;
        result.requestor = request;
        result.selection = event->selection;
        result.property = rprop;
        result.target = event->target;
        result.time = event->time;
        TRACE("Sending SelectionNotify event...\n");
        wine_tsx11_lock();
        XSendEvent(display,event->requestor,False,NoEventMask,(XEvent*)&result);
        wine_tsx11_unlock();
    }
}


/***********************************************************************
 *           X11DRV_SelectionRequest
 */
void X11DRV_SelectionRequest( HWND hWnd, XEvent *event )
{
    if (!hWnd) return;
    X11DRV_HandleSelectionRequest( hWnd, &event->xselectionrequest, FALSE );
}


/***********************************************************************
 *           X11DRV_SelectionClear
 */
void X11DRV_SelectionClear( HWND hWnd, XEvent *xev )
{
    XSelectionClearEvent *event = &xev->xselectionclear;
    if (!hWnd) return;
    if (event->selection == XA_PRIMARY || event->selection == x11drv_atom(CLIPBOARD))
        X11DRV_CLIPBOARD_ReleaseSelection( event->selection, event->window, hWnd, event->time );
}
