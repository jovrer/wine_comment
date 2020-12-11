/*
 * Copyright (C) the Wine project
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

#ifndef __WINE_OLECTL_H
#define __WINE_OLECTL_H

#if !defined(__midl) && !defined(__WIDL__)

#include <ocidl.h>

#ifdef __cplusplus
extern "C" {
#endif



/*
 * Ole Control Interfaces
 */
DEFINE_GUID(CLSID_ConvertVBX,
            0xfb8f0822,0x0164,0x101b,0x84,0xed,0x08,0x00,0x2b,0x2e,0xc7,0x13);
DEFINE_GUID(CLSID_PersistPropset,
            0xfb8f0821,0x0164,0x101b,0x84,0xed,0x08,0x00,0x2b,0x2e,0xc7,0x13);

DEFINE_GUID(CLSID_StdFont,
            0x0be35203,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);
DEFINE_GUID(CLSID_StdPicture,
            0x0be35204,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);

DEFINE_GUID(CLSID_CFontPropPage,
            0x0be35200,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);
DEFINE_GUID(CLSID_CColorPropPage,
            0x0be35201,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);
DEFINE_GUID(CLSID_CPicturePropPage,
            0x0be35202,0x8f91,0x11ce,0x9d,0xe3,0x00,0xaa,0x00,0x4b,0xb8,0x51);

DEFINE_GUID(GUID_HIMETRIC,
            0x66504300,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_COLOR,
            0x66504301,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_XPOSPIXEL,
            0x66504302,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_YPOSPIXEL,
            0x66504303,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_XSIZEPIXEL,
            0x66504304,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_YSIZEPIXEL,
            0x66504305,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_XPOS,
            0x66504306,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_YPOS,
            0x66504307,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_XSIZE,
            0x66504308,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);
DEFINE_GUID(GUID_YSIZE,
            0x66504309,0xBE0F,0x101A,0x8B,0xBB,0x00,0xAA,0x00,0x30,0x0C,0xAB);

typedef struct tagOCPFIPARAMS
{
	ULONG cbStructSize;
	HWND hWndOwner;
	int x;
	int y;
	LPCOLESTR lpszCaption;
	ULONG cObjects;
	LPUNKNOWN *lplpUnk;
	ULONG cPages;
	CLSID *lpPages;
	LCID lcid;
	DISPID dispidInitialProperty;
} OCPFIPARAMS, *LPOCPFIPARAMS;

/*
 * FONTDESC is used as an OLE encapsulation of the GDI fonts
 */
typedef struct tagFONTDESC {
  UINT     cbSizeofstruct;
  LPOLESTR lpstrName;
  CY         cySize;
  SHORT      sWeight;
  SHORT      sCharset;
  BOOL     fItalic;
  BOOL     fUnderline;
  BOOL     fStrikethrough;
} FONTDESC, *LPFONTDESC;

#define FONTSIZE(n) { n##0000, 0 }

#define PICTYPE_UNINITIALIZED (-1)
#define PICTYPE_NONE          0
#define PICTYPE_BITMAP        1
#define PICTYPE_METAFILE      2
#define PICTYPE_ICON          3
#define PICTYPE_ENHMETAFILE   4

typedef struct tagPICTDESC {
	UINT cbSizeofstruct;
	UINT picType;
	union {
			struct {
					HBITMAP hbitmap;
					HPALETTE hpal;
			} bmp;
			struct {
					HMETAFILE hmeta;
					int xExt;
					int yExt;
			} wmf;
			struct {
					HICON hicon;
			} icon;
			struct {
					HENHMETAFILE hemf;
			} emf;
	} DUMMYUNIONNAME;
} PICTDESC, *LPPICTDESC;

typedef long OLE_XPOS_PIXELS;
typedef long OLE_YPOS_PIXELS;
typedef long OLE_XSIZE_PIXELS;
typedef long OLE_YSIZE_PIXELS;
typedef float OLE_XPOS_CONTAINER;
typedef float OLE_YPOS_CONTAINER;
typedef float OLE_XSIZE_CONTAINER;
typedef float OLE_YSIZE_CONTAINER;

typedef enum
{
	triUnchecked = 0,
	triChecked = 1,
	triGray = 2
} OLE_TRISTATE;

typedef VARIANT_BOOL OLE_OPTEXCLUSIVE;
typedef VARIANT_BOOL OLE_CANCELBOOL;
typedef VARIANT_BOOL OLE_ENABLEDEFAULTBOOL;

HCURSOR WINAPI OleIconToCursor( HINSTANCE hinstExe, HICON hicon);

HRESULT WINAPI OleCreatePropertyFrameIndirect( LPOCPFIPARAMS lpParams);

HRESULT WINAPI OleCreatePropertyFrame(
	HWND hwndOwner, UINT x, UINT y,
	LPCOLESTR lpszCaption, ULONG cObjects, LPUNKNOWN* ppUnk,
	ULONG cPages, LPCLSID pPageClsID, LCID lcid, DWORD dwReserved,
	LPVOID pvReserved );

HRESULT WINAPI OleLoadPicture(	LPSTREAM lpstream, LONG lSize, BOOL fRunmode,
		REFIID riid, LPVOID *lplpvObj );

HRESULT WINAPI OleLoadPictureEx( LPSTREAM lpstream, LONG lSize, BOOL fRunMode,
                REFIID riid, DWORD xSizeDesired, DWORD ySizeDesired,
                DWORD dwFlags, LPVOID *lplpvObj );

HRESULT WINAPI OleLoadPicturePath( LPOLESTR szURLorPath, LPUNKNOWN punkCaller,
		DWORD dwReserved, OLE_COLOR clrReserved, REFIID riid,
		LPVOID *ppvRet );

HRESULT WINAPI OleCreatePictureIndirect(LPPICTDESC lpPictDesc, REFIID riid,
		BOOL fOwn, LPVOID * lplpvObj );

HRESULT WINAPI OleCreateFontIndirect(LPFONTDESC lpFontDesc, REFIID riid,
		LPVOID* lplpvObj);

HRESULT WINAPI OleTranslateColor( OLE_COLOR clr, HPALETTE hpal,
		COLORREF* lpcolorref);

/* Reflected Window Message IDs */
#define OCM__BASE           (WM_USER+0x1c00)
#define OCM_COMMAND         (OCM__BASE + WM_COMMAND)

#define OCM_CTLCOLORBTN     (OCM__BASE + WM_CTLCOLORBTN)
#define OCM_CTLCOLOREDIT    (OCM__BASE + WM_CTLCOLOREDIT)
#define OCM_CTLCOLORDLG     (OCM__BASE + WM_CTLCOLORDLG)
#define OCM_CTLCOLORLISTBOX (OCM__BASE + WM_CTLCOLORLISTBOX)
#define OCM_CTLCOLORMSGBOX  (OCM__BASE + WM_CTLCOLORMSGBOX)
#define OCM_CTLCOLORSCROLLBAR   (OCM__BASE + WM_CTLCOLORSCROLLBAR)
#define OCM_CTLCOLORSTATIC  (OCM__BASE + WM_CTLCOLORSTATIC)

#define OCM_DRAWITEM        (OCM__BASE + WM_DRAWITEM)
#define OCM_MEASUREITEM     (OCM__BASE + WM_MEASUREITEM)
#define OCM_DELETEITEM      (OCM__BASE + WM_DELETEITEM)
#define OCM_VKEYTOITEM      (OCM__BASE + WM_VKEYTOITEM)
#define OCM_CHARTOITEM      (OCM__BASE + WM_CHARTOITEM)
#define OCM_COMPAREITEM     (OCM__BASE + WM_COMPAREITEM)
#define OCM_HSCROLL         (OCM__BASE + WM_HSCROLL)
#define OCM_VSCROLL         (OCM__BASE + WM_VSCROLL)
#define OCM_PARENTNOTIFY    (OCM__BASE + WM_PARENTNOTIFY)
#define OCM_NOTIFY            (OCM__BASE + WM_NOTIFY)

#define CONNECT_E_FIRST    MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x0200)
#define CONNECT_E_LAST     MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x020F)
#define CONNECT_S_FIRST    MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x0200)
#define CONNECT_S_LAST     MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x020F)

#define CONNECT_E_NOCONNECTION      (CONNECT_E_FIRST+0)
#define CONNECT_E_ADVISELIMIT       (CONNECT_E_FIRST+1)
#define CONNECT_E_CANNOTCONNECT     (CONNECT_E_FIRST+2)
#define CONNECT_E_OVERRIDDEN        (CONNECT_E_FIRST+3)

#define SELFREG_E_FIRST             MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x0200)
#define SELFREG_E_LAST              MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x020F)
#define SELFREG_S_FIRST             MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x0200)
#define SELFREG_S_LAST              MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x020F)
#define SELFREG_E_TYPELIB           (SELFREG_E_FIRST+0)
#define SELFREG_E_CLASS             (SELFREG_E_FIRST+1)

#ifndef FACILITY_CONTROL
#define FACILITY_CONTROL 0xa
#endif
#define STD_CTL_SCODE(n) MAKE_SCODE(SEVERITY_ERROR, FACILITY_CONTROL, n)
#define CTL_E_ILLEGALFUNCTIONCALL       STD_CTL_SCODE(5)
#define CTL_E_OVERFLOW                  STD_CTL_SCODE(6)
#define CTL_E_OUTOFMEMORY               STD_CTL_SCODE(7)
#define CTL_E_DIVISIONBYZERO            STD_CTL_SCODE(11)
#define CTL_E_OUTOFSTRINGSPACE          STD_CTL_SCODE(14)
#define CTL_E_OUTOFSTACKSPACE           STD_CTL_SCODE(28)
#define CTL_E_BADFILENAMEORNUMBER       STD_CTL_SCODE(52)
#define CTL_E_FILENOTFOUND              STD_CTL_SCODE(53)
#define CTL_E_BADFILEMODE               STD_CTL_SCODE(54)
#define CTL_E_FILEALREADYOPEN           STD_CTL_SCODE(55)
#define CTL_E_DEVICEIOERROR             STD_CTL_SCODE(57)
#define CTL_E_FILEALREADYEXISTS         STD_CTL_SCODE(58)
#define CTL_E_BADRECORDLENGTH           STD_CTL_SCODE(59)
#define CTL_E_DISKFULL                  STD_CTL_SCODE(61)
#define CTL_E_BADRECORDNUMBER           STD_CTL_SCODE(63)
#define CTL_E_BADFILENAME               STD_CTL_SCODE(64)
#define CTL_E_TOOMANYFILES              STD_CTL_SCODE(67)
#define CTL_E_DEVICEUNAVAILABLE         STD_CTL_SCODE(68)
#define CTL_E_PERMISSIONDENIED          STD_CTL_SCODE(70)
#define CTL_E_DISKNOTREADY              STD_CTL_SCODE(71)
#define CTL_E_PATHFILEACCESSERROR       STD_CTL_SCODE(75)
#define CTL_E_PATHNOTFOUND              STD_CTL_SCODE(76)
#define CTL_E_INVALIDPATTERNSTRING      STD_CTL_SCODE(93)
#define CTL_E_INVALIDUSEOFNULL          STD_CTL_SCODE(94)
#define CTL_E_INVALIDFILEFORMAT         STD_CTL_SCODE(321)
#define CTL_E_INVALIDPROPERTYVALUE      STD_CTL_SCODE(380)
#define CTL_E_INVALIDPROPERTYARRAYINDEX STD_CTL_SCODE(381)
#define CTL_E_SETNOTSUPPORTEDATRUNTIME  STD_CTL_SCODE(382)
#define CTL_E_SETNOTSUPPORTED           STD_CTL_SCODE(383)
#define CTL_E_NEEDPROPERTYARRAYINDEX    STD_CTL_SCODE(385)
#define CTL_E_SETNOTPERMITTED           STD_CTL_SCODE(387)
#define CTL_E_GETNOTSUPPORTEDATRUNTIME  STD_CTL_SCODE(393)
#define CTL_E_GETNOTSUPPORTED           STD_CTL_SCODE(394)
#define CTL_E_PROPERTYNOTFOUND          STD_CTL_SCODE(422)
#define CTL_E_INVALIDCLIPBOARDFORMAT    STD_CTL_SCODE(460)
#define CTL_E_INVALIDPICTURE            STD_CTL_SCODE(481)
#define CTL_E_PRINTERERROR              STD_CTL_SCODE(482)
#define CTL_E_CANTSAVEFILETOTEMP        STD_CTL_SCODE(735)
#define CTL_E_SEARCHTEXTNOTFOUND        STD_CTL_SCODE(744)
#define CTL_E_REPLACEMENTSTOOLONG       STD_CTL_SCODE(746)

#define VT_COLOR            VT_I4
#define VT_FONT             VT_DISPATCH
#define VT_STREAMED_PROPSET 73  /*       [P]  Stream contains a property set */
#define VT_STORED_PROPSET   74  /*       [P]  Storage contains a property set */
#define VT_BLOB_PROPSET     75  /*       [P]  Blob contains a property set */
#define VT_VERBOSE_ENUM     76  /*       [P]  Enum value with text string */

#define PERPROP_E_FIRST    MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x0200)
#define PERPROP_E_LAST     MAKE_SCODE(SEVERITY_ERROR,   FACILITY_ITF, 0x020F)
#define PERPROP_S_FIRST    MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x0200)
#define PERPROP_S_LAST     MAKE_SCODE(SEVERITY_SUCCESS, FACILITY_ITF, 0x020F)

#define PERPROP_E_NOPAGEAVAILABLE   (PERPROP_E_FIRST+0)


#ifdef __cplusplus
}
#endif

#endif  /* !__midl && !__WIDL__ */

/* standard dispatch ID's */
#define DISPID_AUTOSIZE                 (-500)
#define DISPID_BACKCOLOR                (-501)
#define DISPID_BACKSTYLE                (-502)
#define DISPID_BORDERCOLOR              (-503)
#define DISPID_BORDERSTYLE              (-504)
#define DISPID_BORDERWIDTH              (-505)
#define DISPID_DRAWMODE                 (-507)
#define DISPID_DRAWSTYLE                (-508)
#define DISPID_DRAWWIDTH                (-509)
#define DISPID_FILLCOLOR                (-510)
#define DISPID_FILLSTYLE                (-511)
#define DISPID_FONT                     (-512)
#define DISPID_FORECOLOR                (-513)
#define DISPID_ENABLED                  (-514)
#define DISPID_HWND                     (-515)
#define DISPID_TABSTOP                  (-516)
#define DISPID_TEXT                     (-517)
#define DISPID_CAPTION                  (-518)
#define DISPID_BORDERVISIBLE            (-519)
#define DISPID_APPEARANCE               (-520)
#define DISPID_MOUSEPOINTER             (-521)
#define DISPID_MOUSEICON                (-522)
#define DISPID_PICTURE                  (-523)
#define DISPID_VALID                    (-524)
#define DISPID_READYSTATE               (-525)

#define DISPID_REFRESH                  (-550)
#define DISPID_DOCLICK                  (-551)
#define DISPID_ABOUTBOX                 (-552)

#define DISPID_CLICK                    (-600)
#define DISPID_DBLCLICK                 (-601)
#define DISPID_KEYDOWN                  (-602)
#define DISPID_KEYPRESS                 (-603)
#define DISPID_KEYUP                    (-604)
#define DISPID_MOUSEDOWN                (-605)
#define DISPID_MOUSEMOVE                (-606)
#define DISPID_MOUSEUP                  (-607)
#define DISPID_ERROREVENT               (-608)
#define DISPID_READYSTATECHANGE         (-609)

#define DISPID_AMBIENT_BACKCOLOR        (-701)
#define DISPID_AMBIENT_DISPLAYNAME      (-702)
#define DISPID_AMBIENT_FONT             (-703)
#define DISPID_AMBIENT_FORECOLOR        (-704)
#define DISPID_AMBIENT_LOCALEID         (-705)
#define DISPID_AMBIENT_MESSAGEREFLECT   (-706)
#define DISPID_AMBIENT_SCALEUNITS       (-707)
#define DISPID_AMBIENT_TEXTALIGN        (-708)
#define DISPID_AMBIENT_USERMODE         (-709)
#define DISPID_AMBIENT_UIDEAD           (-710)
#define DISPID_AMBIENT_SHOWGRABHANDLES  (-711)
#define DISPID_AMBIENT_SHOWHATCHING     (-712)
#define DISPID_AMBIENT_DISPLAYASDEFAULT (-713)
#define DISPID_AMBIENT_SUPPORTSMNEMONICS (-714)
#define DISPID_AMBIENT_AUTOCLIP         (-715)
#define DISPID_AMBIENT_APPEARANCE       (-716)
#define DISPID_AMBIENT_PALETTE          (-726)
#define DISPID_AMBIENT_TRANSFERPRIORITY (-728)

#define DISPID_Name                     (-800)
#define DISPID_Delete                   (-801)
#define DISPID_Object                   (-802)
#define DISPID_Parent                   (-803)

#define DISPID_FONT_NAME 0
#define DISPID_FONT_SIZE 2
#define DISPID_FONT_BOLD 3
#define DISPID_FONT_ITALIC 4
#define DISPID_FONT_UNDER 5
#define DISPID_FONT_STRIKE 6
#define DISPID_FONT_WEIGHT 7
#define DISPID_FONT_CHARSET 8

/* IPicture */
#define DISPID_PICT_HANDLE	0
#define DISPID_PICT_HPAL	2
#define DISPID_PICT_TYPE	3
#define DISPID_PICT_WIDTH	4
#define DISPID_PICT_HEIGHT	5
#define DISPID_PICT_RENDER	6

#endif /*  __WINE_OLECTL_H */
