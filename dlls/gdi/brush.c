/*
 * GDI brush objects
 *
 * Copyright 1993, 1994  Alexandre Julliard
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
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "wine/wingdi16.h"
#include "gdi.h"
#include "wownt32.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(gdi);

/* GDI logical brush object */
typedef struct
{
    GDIOBJHDR header;
    LOGBRUSH  logbrush;
} BRUSHOBJ;

#define NB_HATCH_STYLES  6

static HGDIOBJ BRUSH_SelectObject( HGDIOBJ handle, void *obj, HDC hdc );
static INT BRUSH_GetObject16( HGDIOBJ handle, void *obj, INT count, LPVOID buffer );
static INT BRUSH_GetObject( HGDIOBJ handle, void *obj, INT count, LPVOID buffer );
static BOOL BRUSH_DeleteObject( HGDIOBJ handle, void *obj );

static const struct gdi_obj_funcs brush_funcs =
{
    BRUSH_SelectObject,  /* pSelectObject */
    BRUSH_GetObject16,   /* pGetObject16 */
    BRUSH_GetObject,     /* pGetObjectA */
    BRUSH_GetObject,     /* pGetObjectW */
    NULL,                /* pUnrealizeObject */
    BRUSH_DeleteObject   /* pDeleteObject */
};

static HGLOBAL16 dib_copy(BITMAPINFO *info, UINT coloruse)
{
    BITMAPINFO  *newInfo;
    HGLOBAL16   hmem;
    INT         size;

    if (info->bmiHeader.biCompression)
        size = info->bmiHeader.biSizeImage;
    else
        size = DIB_GetDIBImageBytes(info->bmiHeader.biWidth,
                                    info->bmiHeader.biHeight,
                                    info->bmiHeader.biBitCount);
    size += DIB_BitmapInfoSize( info, coloruse );

    if (!(hmem = GlobalAlloc16( GMEM_MOVEABLE, size )))
    {
        return 0;
    }
    newInfo = (BITMAPINFO *) GlobalLock16( hmem );
    memcpy( newInfo, info, size );
    GlobalUnlock16( hmem );
    return hmem;
}


/***********************************************************************
 *           CreateBrushIndirect    (GDI32.@)
 *
 * Create a logical brush with a given style, color or pattern.
 *
 * PARAMS
 *  brush [I] Pointer to a LOGBRUSH structure describing the desired brush.
 *
 * RETURNS
 *  A handle to the created brush, or a NULL handle if the brush cannot be 
 *  created.
 *
 * NOTES
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 * - Windows 95 and earlier cannot create brushes from bitmaps or DIBs larger
 *   than 8x8 pixels. If a larger bitmap is given, only a portion of the bitmap
 *   is used.
 */
HBRUSH WINAPI CreateBrushIndirect( const LOGBRUSH * brush )
{
    BRUSHOBJ * ptr;
    HBRUSH hbrush;

    if (!(ptr = GDI_AllocObject( sizeof(BRUSHOBJ), BRUSH_MAGIC,
                                (HGDIOBJ *)&hbrush, &brush_funcs ))) return 0;
    ptr->logbrush.lbStyle = brush->lbStyle;
    ptr->logbrush.lbColor = brush->lbColor;
    ptr->logbrush.lbHatch = brush->lbHatch;

    switch (ptr->logbrush.lbStyle)
    {
    case BS_PATTERN8X8:
        ptr->logbrush.lbStyle = BS_PATTERN;
        /* fall through */
    case BS_PATTERN:
        ptr->logbrush.lbHatch = (ULONG_PTR)BITMAP_CopyBitmap( (HBITMAP) ptr->logbrush.lbHatch );
        if (!ptr->logbrush.lbHatch) goto error;
        break;

    case BS_DIBPATTERNPT:
        ptr->logbrush.lbStyle = BS_DIBPATTERN;
        ptr->logbrush.lbHatch = (ULONG_PTR)dib_copy( (BITMAPINFO *) ptr->logbrush.lbHatch,
                                                     ptr->logbrush.lbColor);
        if (!ptr->logbrush.lbHatch) goto error;
        break;

    case BS_DIBPATTERN8X8:
    case BS_DIBPATTERN:
       {
            BITMAPINFO* bmi;
            HGLOBAL h = (HGLOBAL)ptr->logbrush.lbHatch;

            ptr->logbrush.lbStyle = BS_DIBPATTERN;
            if (!(bmi = (BITMAPINFO *)GlobalLock( h ))) goto error;
            ptr->logbrush.lbHatch = dib_copy( bmi, ptr->logbrush.lbColor);
            GlobalUnlock( h );
            if (!ptr->logbrush.lbHatch) goto error;
            break;
       }

    default:
        if(ptr->logbrush.lbStyle > BS_MONOPATTERN) goto error;
        break;
    }

    GDI_ReleaseObj( hbrush );
    TRACE("%p\n", hbrush);
    return hbrush;

 error:
    GDI_FreeObject( hbrush, ptr );
    return 0;
}


/***********************************************************************
 *           CreateHatchBrush    (GDI32.@)
 *
 * Create a logical brush with a hatched pattern.
 *
 * PARAMS
 *  style [I] Direction of lines for the hatch pattern (HS_* values from "wingdi.h")
 *  color [I] Colour of the hatched pattern
 *
 * RETURNS
 *  A handle to the created brush, or a NULL handle if the brush cannot
 *  be created.
 *
 * NOTES
 * - This function uses CreateBrushIndirect() to create the brush.
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 */
HBRUSH WINAPI CreateHatchBrush( INT style, COLORREF color )
{
    LOGBRUSH logbrush;

    TRACE("%d %06lx\n", style, color );

    logbrush.lbStyle = BS_HATCHED;
    logbrush.lbColor = color;
    logbrush.lbHatch = style;

    return CreateBrushIndirect( &logbrush );
}


/***********************************************************************
 *           CreatePatternBrush    (GDI32.@)
 *
 * Create a logical brush with a pattern from a bitmap.
 *
 * PARAMS
 *  hbitmap  [I] Bitmap containing pattern for the brush
 *
 * RETURNS
 *  A handle to the created brush, or a NULL handle if the brush cannot 
 *  be created.
 *
 * NOTES
 * - This function uses CreateBrushIndirect() to create the brush.
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 */
HBRUSH WINAPI CreatePatternBrush( HBITMAP hbitmap )
{
    LOGBRUSH logbrush = { BS_PATTERN, 0, 0 };
    TRACE("%p\n", hbitmap );

    logbrush.lbHatch = (ULONG_PTR)hbitmap;
    return CreateBrushIndirect( &logbrush );
}


/***********************************************************************
 *           CreateDIBPatternBrush    (GDI32.@)
 *
 * Create a logical brush with a pattern from a DIB.
 *
 * PARAMS
 *  hbitmap  [I] Global object containing BITMAPINFO structure for the pattern
 *  coloruse [I] Specifies color format, if provided
 *
 * RETURNS
 *  A handle to the created brush, or a NULL handle if the brush cannot 
 *  be created.
 *
 * NOTES
 * - This function uses CreateBrushIndirect() to create the brush.
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 * - This function is for compatibility only. CreateDIBPatternBrushPt() should 
 *   be used instead.
 */
HBRUSH WINAPI CreateDIBPatternBrush( HGLOBAL hbitmap, UINT coloruse )
{
    LOGBRUSH logbrush;

    TRACE("%p\n", hbitmap );

    logbrush.lbStyle = BS_DIBPATTERN;
    logbrush.lbColor = coloruse;

    logbrush.lbHatch = (ULONG_PTR)hbitmap;

    return CreateBrushIndirect( &logbrush );
}


/***********************************************************************
 *           CreateDIBPatternBrushPt    (GDI32.@)
 *
 * Create a logical brush with a pattern from a DIB.
 *
 * PARAMS
 *  data     [I] Pointer to a BITMAPINFO structure and image data  for the pattern
 *  coloruse [I] Specifies color format, if provided
 *
 * RETURNS
 *  A handle to the created brush, or a NULL handle if the brush cannot
 *  be created.
 *
 * NOTES
 * - This function uses CreateBrushIndirect() to create the brush.
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 */
HBRUSH WINAPI CreateDIBPatternBrushPt( const void* data, UINT coloruse )
{
    BITMAPINFO *info=(BITMAPINFO*)data;
    LOGBRUSH logbrush;

    if (!data)
        return NULL;

    TRACE("%p %ldx%ld %dbpp\n", info, info->bmiHeader.biWidth,
	  info->bmiHeader.biHeight,  info->bmiHeader.biBitCount);

    logbrush.lbStyle = BS_DIBPATTERNPT;
    logbrush.lbColor = coloruse;
    logbrush.lbHatch = (ULONG_PTR)data;

    return CreateBrushIndirect( &logbrush );
}


/***********************************************************************
 *           CreateSolidBrush    (GDI32.@)
 *
 * Create a logical brush consisting of a single colour.
 *
 * PARAMS
 *  color [I] Colour to make the solid brush
 *
 * RETURNS
 *  A handle to the newly created brush, or a NULL handle if the brush cannot
 *  be created.
 *
 * NOTES
 * - This function uses CreateBrushIndirect() to create the brush.
 * - The brush returned should be freed by the caller using DeleteObject()
 *   when it is no longer required.
 */
HBRUSH WINAPI CreateSolidBrush( COLORREF color )
{
    LOGBRUSH logbrush;

    TRACE("%06lx\n", color );

    logbrush.lbStyle = BS_SOLID;
    logbrush.lbColor = color;
    logbrush.lbHatch = 0;

    return CreateBrushIndirect( &logbrush );
}


/***********************************************************************
 *           SetBrushOrgEx    (GDI32.@)
 *
 * Set the brush origin for a device context.
 *
 * PARAMS
 *  hdc    [I] Device context to set the brush origin for 
 *  x      [I] New x origin
 *  y      [I] Ney y origin
 *  oldorg [O] If non NULL, destination for previously set brush origin.
 *
 * RETURNS
 *  Success: TRUE. The origin is set to (x,y), and oldorg is updated if given.
 */
BOOL WINAPI SetBrushOrgEx( HDC hdc, INT x, INT y, LPPOINT oldorg )
{
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return FALSE;
    if (oldorg)
    {
        oldorg->x = dc->brushOrgX;
        oldorg->y = dc->brushOrgY;
    }
    dc->brushOrgX = x;
    dc->brushOrgY = y;
    GDI_ReleaseObj( hdc );
    return TRUE;
}

/***********************************************************************
 *           FixBrushOrgEx    (GDI32.@)
 *
 * See SetBrushOrgEx.
 *
 * NOTES
 *  This function is no longer documented by MSDN, but in Win95 GDI32 it
 *  is the same as SetBrushOrgEx().
 */
BOOL WINAPI FixBrushOrgEx( HDC hdc, INT x, INT y, LPPOINT oldorg )
{
    return SetBrushOrgEx(hdc,x,y,oldorg);
}


/***********************************************************************
 *           BRUSH_SelectObject
 */
static HGDIOBJ BRUSH_SelectObject( HGDIOBJ handle, void *obj, HDC hdc )
{
    BRUSHOBJ *brush = obj;
    HGDIOBJ ret;
    DC *dc = DC_GetDCPtr( hdc );

    if (!dc) return 0;

    if (brush->logbrush.lbStyle == BS_PATTERN)
        BITMAP_SetOwnerDC( (HBITMAP)brush->logbrush.lbHatch, dc );

    ret = dc->hBrush;
    if (dc->funcs->pSelectBrush) handle = dc->funcs->pSelectBrush( dc->physDev, handle );
    if (handle) dc->hBrush = handle;
    else ret = 0;
    GDI_ReleaseObj( hdc );
    return ret;
}


/***********************************************************************
 *           BRUSH_DeleteObject
 */
static BOOL BRUSH_DeleteObject( HGDIOBJ handle, void *obj )
{
    BRUSHOBJ *brush = obj;

    switch(brush->logbrush.lbStyle)
    {
      case BS_PATTERN:
	  DeleteObject( (HGDIOBJ)brush->logbrush.lbHatch );
	  break;
      case BS_DIBPATTERN:
	  GlobalFree16( (HGLOBAL16)brush->logbrush.lbHatch );
	  break;
    }
    return GDI_FreeObject( handle, obj );
}


/***********************************************************************
 *           BRUSH_GetObject16
 */
static INT BRUSH_GetObject16( HGDIOBJ handle, void *obj, INT count, LPVOID buffer )
{
    BRUSHOBJ *brush = obj;
    LOGBRUSH16 logbrush;

    logbrush.lbStyle = brush->logbrush.lbStyle;
    logbrush.lbColor = brush->logbrush.lbColor;
    logbrush.lbHatch = brush->logbrush.lbHatch;
    if (count > sizeof(logbrush)) count = sizeof(logbrush);
    memcpy( buffer, &logbrush, count );
    return count;
}


/***********************************************************************
 *           BRUSH_GetObject
 */
static INT BRUSH_GetObject( HGDIOBJ handle, void *obj, INT count, LPVOID buffer )
{
    BRUSHOBJ *brush = obj;

    if( !buffer )
        return sizeof(brush->logbrush);

    if (count > sizeof(brush->logbrush)) count = sizeof(brush->logbrush);
    memcpy( buffer, &brush->logbrush, count );
    return count;
}


/***********************************************************************
 *           SetSolidBrush   (GDI.604)
 *
 * Change the color of a solid brush.
 *
 * PARAMS
 *  hBrush   [I] Brush to change the color of
 *  newColor [I] New color for hBrush
 *
 * RETURNS
 *  Success: TRUE. The color of hBrush is set to newColor.
 *  Failure: FALSE.
 *
 * FIXME
 *  This function is undocumented and untested. The implementation may
 *  not be correct.
 */
BOOL16 WINAPI SetSolidBrush16(HBRUSH16 hBrush, COLORREF newColor )
{
    BRUSHOBJ * brushPtr;
    BOOL16 res = FALSE;

    TRACE("(hBrush %04x, newColor %08lx)\n", hBrush, (DWORD)newColor);
    if (!(brushPtr = (BRUSHOBJ *) GDI_GetObjPtr( HBRUSH_32(hBrush), BRUSH_MAGIC )))
	return FALSE;

    if (brushPtr->logbrush.lbStyle == BS_SOLID)
    {
        brushPtr->logbrush.lbColor = newColor;
	res = TRUE;
    }

     GDI_ReleaseObj( HBRUSH_32(hBrush) );
     return res;
}
