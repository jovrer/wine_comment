/*
 * GDI device-independent bitmaps
 *
 * Copyright 1993,1994  Alexandre Julliard
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

/*
  Important information:
  
  * Current Windows versions support two different DIB structures:

    - BITMAPCOREINFO / BITMAPCOREHEADER (legacy structures; used in OS/2)
    - BITMAPINFO / BITMAPINFOHEADER
  
    Most Windows API functions taking a BITMAPINFO* / BITMAPINFOHEADER* also
    accept the old "core" structures, and so must WINE.
    You can distinguish them by looking at the first member (bcSize/biSize),
    or use the internal function DIB_GetBitmapInfo.

    
  * The palettes are stored in different formats:

    - BITMAPCOREINFO: Array of RGBTRIPLE
    - BITMAPINFO:     Array of RGBQUAD

    
  * There are even more DIB headers, but they all extend BITMAPINFOHEADER:
    
    - BITMAPV4HEADER: Introduced in Windows 95 / NT 4.0
    - BITMAPV5HEADER: Introduced in Windows 98 / 2000
    
    If biCompression is BI_BITFIELDS, the color masks are at the same position
    in all the headers (they start at bmiColors of BITMAPINFOHEADER), because
    the new headers have structure members for the masks.


  * You should never access the color table using the bmiColors member,
    because the passed structure may have one of the extended headers
    mentioned above. Use this to calculate the location:
    
    BITMAPINFO* info;
    void* colorPtr = (LPBYTE) info + (WORD) info->bmiHeader.biSize;

    
  * More information:
    Search for "Bitmap Structures" in MSDN
*/

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "gdi.h"
#include "wownt32.h"
#include "gdi_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(bitmap);


/*
  Some of the following helper functions are duplicated in
  dlls/x11drv/dib.c
*/

/***********************************************************************
 *           DIB_GetDIBWidthBytes
 *
 * Return the width of a DIB bitmap in bytes. DIB bitmap data is 32-bit aligned.
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/bitmaps_87eb.asp
 */
int DIB_GetDIBWidthBytes( int width, int depth )
{
    int words;

    switch(depth)
    {
	case 1:  words = (width + 31) / 32; break;
	case 4:  words = (width + 7) / 8; break;
	case 8:  words = (width + 3) / 4; break;
	case 15:
	case 16: words = (width + 1) / 2; break;
	case 24: words = (width * 3 + 3)/4; break;

	default:
            WARN("(%d): Unsupported depth\n", depth );
	/* fall through */
	case 32:
	        words = width;
    }
    return 4 * words;
}

/***********************************************************************
 *           DIB_GetDIBImageBytes
 *
 * Return the number of bytes used to hold the image in a DIB bitmap.
 */
int DIB_GetDIBImageBytes( int width, int height, int depth )
{
    return DIB_GetDIBWidthBytes( width, depth ) * abs( height );
}


/***********************************************************************
 *           DIB_BitmapInfoSize
 *
 * Return the size of the bitmap info structure including color table.
 */
int DIB_BitmapInfoSize( const BITMAPINFO * info, WORD coloruse )
{
    int colors;

    if (info->bmiHeader.biSize == sizeof(BITMAPCOREHEADER))
    {
        BITMAPCOREHEADER *core = (BITMAPCOREHEADER *)info;
        colors = (core->bcBitCount <= 8) ? 1 << core->bcBitCount : 0;
        return sizeof(BITMAPCOREHEADER) + colors *
             ((coloruse == DIB_RGB_COLORS) ? sizeof(RGBTRIPLE) : sizeof(WORD));
    }
    else  /* assume BITMAPINFOHEADER */
    {
        colors = info->bmiHeader.biClrUsed;
        if (colors > 256) colors = 256;
        if (!colors && (info->bmiHeader.biBitCount <= 8))
            colors = 1 << info->bmiHeader.biBitCount;
        return sizeof(BITMAPINFOHEADER) + colors *
               ((coloruse == DIB_RGB_COLORS) ? sizeof(RGBQUAD) : sizeof(WORD));
    }
}


/***********************************************************************
 *           DIB_GetBitmapInfo
 *
 * Get the info from a bitmap header.
 * Return 1 for INFOHEADER, 0 for COREHEADER,
 * 4 for V4HEADER, 5 for V5HEADER, -1 for error.
 */
static int DIB_GetBitmapInfo( const BITMAPINFOHEADER *header, LONG *width,
                              LONG *height, WORD *planes, WORD *bpp, DWORD *compr, DWORD *size )
{
    if (header->biSize == sizeof(BITMAPINFOHEADER))
    {
        *width  = header->biWidth;
        *height = header->biHeight;
        *planes = header->biPlanes;
        *bpp    = header->biBitCount;
        *compr  = header->biCompression;
        *size   = header->biSizeImage;
        return 1;
    }
    if (header->biSize == sizeof(BITMAPCOREHEADER))
    {
        BITMAPCOREHEADER *core = (BITMAPCOREHEADER *)header;
        *width  = core->bcWidth;
        *height = core->bcHeight;
        *planes = core->bcPlanes;
        *bpp    = core->bcBitCount;
        *compr  = 0;
        *size   = 0;
        return 0;
    }
    if (header->biSize == sizeof(BITMAPV4HEADER))
    {
        BITMAPV4HEADER *v4hdr = (BITMAPV4HEADER *)header;
        *width  = v4hdr->bV4Width;
        *height = v4hdr->bV4Height;
        *planes = v4hdr->bV4Planes;
        *bpp    = v4hdr->bV4BitCount;
        *compr  = v4hdr->bV4V4Compression;
        *size   = v4hdr->bV4SizeImage;
        return 4;
    }
    if (header->biSize == sizeof(BITMAPV5HEADER))
    {
        BITMAPV5HEADER *v5hdr = (BITMAPV5HEADER *)header;
        *width  = v5hdr->bV5Width;
        *height = v5hdr->bV5Height;
        *planes = v5hdr->bV5Planes;
        *bpp    = v5hdr->bV5BitCount;
        *compr  = v5hdr->bV5Compression;
        *size   = v5hdr->bV5SizeImage;
        return 5;
    }
    ERR("(%ld): unknown/wrong size for header\n", header->biSize );
    return -1;
}


/***********************************************************************
 *           StretchDIBits   (GDI32.@)
 */
INT WINAPI StretchDIBits(HDC hdc, INT xDst, INT yDst, INT widthDst,
                       INT heightDst, INT xSrc, INT ySrc, INT widthSrc,
                       INT heightSrc, const void *bits,
                       const BITMAPINFO *info, UINT wUsage, DWORD dwRop )
{
    DC *dc;

    if (!bits || !info)
	return 0;

    dc = DC_GetDCUpdate( hdc );
    if(!dc) return FALSE;

    if(dc->funcs->pStretchDIBits)
    {
        heightSrc = dc->funcs->pStretchDIBits(dc->physDev, xDst, yDst, widthDst,
                                              heightDst, xSrc, ySrc, widthSrc,
                                              heightSrc, bits, info, wUsage, dwRop);
        GDI_ReleaseObj( hdc );
    }
    else /* use StretchBlt */
    {
        HBITMAP hBitmap, hOldBitmap;
        HPALETTE hpal = NULL;
        HDC hdcMem;
        LONG height;
        LONG width;
        WORD planes, bpp;
        DWORD compr, size;

        GDI_ReleaseObj( hdc );

        if (DIB_GetBitmapInfo( &info->bmiHeader, &width, &height, &planes, &bpp, &compr, &size ) == -1)
        {
            ERR("Invalid bitmap\n");
            return 0;
        }

        if (width < 0)
        {
            ERR("Bitmap has a negative width\n");
            return 0;
        }

	hdcMem = CreateCompatibleDC( hdc );
        hBitmap = CreateCompatibleBitmap(hdc, width, height);
        hOldBitmap = SelectObject( hdcMem, hBitmap );
        if(wUsage == DIB_PAL_COLORS)
        {
            hpal = GetCurrentObject(hdc, OBJ_PAL);
            hpal = SelectPalette(hdcMem, hpal, FALSE);
        }

	if (info->bmiHeader.biCompression == BI_RLE4 ||
	    info->bmiHeader.biCompression == BI_RLE8) {

	   /* when RLE compression is used, there may be some gaps (ie the DIB doesn't
	    * contain all the rectangle described in bmiHeader, but only part of it.
	    * This mean that those undescribed pixels must be left untouched.
	    * So, we first copy on a memory bitmap the current content of the
	    * destination rectangle, blit the DIB bits on top of it - hence leaving
	    * the gaps untouched -, and blitting the rectangle back.
	    * This insure that gaps are untouched on the destination rectangle
	    * Not doing so leads to trashed images (the gaps contain what was on the
	    * memory bitmap => generally black or garbage)
	    * Unfortunately, RLE DIBs without gaps will be slowed down. But this is
	    * another speed vs correctness issue. Anyway, if speed is needed, then the
	    * pStretchDIBits function shall be implemented.
	    * ericP (2000/09/09)
	    */

            /* copy existing bitmap from destination dc */
            StretchBlt( hdcMem, xSrc, abs(height) - heightSrc - ySrc,
                        widthSrc, heightSrc, hdc, xDst, yDst, widthDst, heightDst,
                        dwRop );
        }

        SetDIBits(hdcMem, hBitmap, 0, height, bits, info, wUsage);

        /* Origin for DIBitmap may be bottom left (positive biHeight) or top
           left (negative biHeight) */
        StretchBlt( hdc, xDst, yDst, widthDst, heightDst,
		    hdcMem, xSrc, abs(height) - heightSrc - ySrc,
		    widthSrc, heightSrc, dwRop );
        if(hpal)
            SelectPalette(hdcMem, hpal, FALSE);
	SelectObject( hdcMem, hOldBitmap );
	DeleteDC( hdcMem );
	DeleteObject( hBitmap );
    }
    return heightSrc;
}


/******************************************************************************
 * SetDIBits [GDI32.@]
 *
 * Sets pixels in a bitmap using colors from DIB.
 *
 * PARAMS
 *    hdc       [I] Handle to device context
 *    hbitmap   [I] Handle to bitmap
 *    startscan [I] Starting scan line
 *    lines     [I] Number of scan lines
 *    bits      [I] Array of bitmap bits
 *    info      [I] Address of structure with data
 *    coloruse  [I] Type of color indexes to use
 *
 * RETURNS
 *    Success: Number of scan lines copied
 *    Failure: 0
 */
INT WINAPI SetDIBits( HDC hdc, HBITMAP hbitmap, UINT startscan,
		      UINT lines, LPCVOID bits, const BITMAPINFO *info,
		      UINT coloruse )
{
    DC *dc;
    BITMAPOBJ *bitmap;
    INT result = 0;

    if (!(dc = DC_GetDCUpdate( hdc )))
    {
        if (coloruse == DIB_RGB_COLORS) FIXME( "shouldn't require a DC for DIB_RGB_COLORS\n" );
        return 0;
    }

    if (!(bitmap = GDI_GetObjPtr( hbitmap, BITMAP_MAGIC )))
    {
        GDI_ReleaseObj( hdc );
        return 0;
    }

    if (!bitmap->funcs && !BITMAP_SetOwnerDC( hbitmap, dc )) goto done;

    if (bitmap->funcs && bitmap->funcs->pSetDIBits)
        result = bitmap->funcs->pSetDIBits( dc->physDev, hbitmap, startscan, lines,
                                            bits, info, coloruse );
    else
        result = lines;

 done:
    GDI_ReleaseObj( hbitmap );
    GDI_ReleaseObj( hdc );
    return result;
}


/***********************************************************************
 *           SetDIBitsToDevice   (GDI32.@)
 */
INT WINAPI SetDIBitsToDevice(HDC hdc, INT xDest, INT yDest, DWORD cx,
                           DWORD cy, INT xSrc, INT ySrc, UINT startscan,
                           UINT lines, LPCVOID bits, const BITMAPINFO *info,
                           UINT coloruse )
{
    INT ret;
    DC *dc;

    if (!(dc = DC_GetDCUpdate( hdc ))) return 0;

    if(dc->funcs->pSetDIBitsToDevice)
        ret = dc->funcs->pSetDIBitsToDevice( dc->physDev, xDest, yDest, cx, cy, xSrc,
					     ySrc, startscan, lines, bits,
					     info, coloruse );
    else {
        FIXME("unimplemented on hdc %p\n", hdc);
	ret = 0;
    }

    GDI_ReleaseObj( hdc );
    return ret;
}

/***********************************************************************
 *           SetDIBColorTable    (GDI32.@)
 */
UINT WINAPI SetDIBColorTable( HDC hdc, UINT startpos, UINT entries, CONST RGBQUAD *colors )
{
    DC * dc;
    UINT result = 0;

    if (!(dc = DC_GetDCUpdate( hdc ))) return 0;

    if (dc->funcs->pSetDIBColorTable)
        result = dc->funcs->pSetDIBColorTable(dc->physDev, startpos, entries, colors);

    GDI_ReleaseObj( hdc );
    return result;
}


/***********************************************************************
 *           GetDIBColorTable    (GDI32.@)
 */
UINT WINAPI GetDIBColorTable( HDC hdc, UINT startpos, UINT entries, RGBQUAD *colors )
{
    DC * dc;
    UINT result = 0;

    if (!(dc = DC_GetDCUpdate( hdc ))) return 0;

    if (dc->funcs->pGetDIBColorTable)
        result = dc->funcs->pGetDIBColorTable(dc->physDev, startpos, entries, colors);

    GDI_ReleaseObj( hdc );
    return result;
}

/* FIXME the following two structs should be combined with __sysPalTemplate in
   objects/color.c - this should happen after de-X11-ing both of these
   files.
   NB. RGBQUAD and PALETTEENTRY have different orderings of red, green
   and blue - sigh */

static RGBQUAD EGAColorsQuads[16] = {
/* rgbBlue, rgbGreen, rgbRed, rgbReserved */
    { 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x80, 0x00 },
    { 0x00, 0x80, 0x00, 0x00 },
    { 0x00, 0x80, 0x80, 0x00 },
    { 0x80, 0x00, 0x00, 0x00 },
    { 0x80, 0x00, 0x80, 0x00 },
    { 0x80, 0x80, 0x00, 0x00 },
    { 0x80, 0x80, 0x80, 0x00 },
    { 0xc0, 0xc0, 0xc0, 0x00 },
    { 0x00, 0x00, 0xff, 0x00 },
    { 0x00, 0xff, 0x00, 0x00 },
    { 0x00, 0xff, 0xff, 0x00 },
    { 0xff, 0x00, 0x00, 0x00 },
    { 0xff, 0x00, 0xff, 0x00 },
    { 0xff, 0xff, 0x00, 0x00 },
    { 0xff, 0xff, 0xff, 0x00 }
};

static RGBTRIPLE EGAColorsTriples[16] = {
/* rgbBlue, rgbGreen, rgbRed */
    { 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x80 },
    { 0x00, 0x80, 0x00 },
    { 0x00, 0x80, 0x80 },
    { 0x80, 0x00, 0x00 },
    { 0x80, 0x00, 0x80 },
    { 0x80, 0x80, 0x00 },
    { 0x80, 0x80, 0x80 },
    { 0xc0, 0xc0, 0xc0 },
    { 0x00, 0x00, 0xff },
    { 0x00, 0xff, 0x00 },
    { 0x00, 0xff, 0xff },
    { 0xff, 0x00, 0x00 } ,
    { 0xff, 0x00, 0xff },
    { 0xff, 0xff, 0x00 },
    { 0xff, 0xff, 0xff }
};

static RGBQUAD DefLogPaletteQuads[20] = { /* Copy of Default Logical Palette */
/* rgbBlue, rgbGreen, rgbRed, rgbReserved */
    { 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x80, 0x00 },
    { 0x00, 0x80, 0x00, 0x00 },
    { 0x00, 0x80, 0x80, 0x00 },
    { 0x80, 0x00, 0x00, 0x00 },
    { 0x80, 0x00, 0x80, 0x00 },
    { 0x80, 0x80, 0x00, 0x00 },
    { 0xc0, 0xc0, 0xc0, 0x00 },
    { 0xc0, 0xdc, 0xc0, 0x00 },
    { 0xf0, 0xca, 0xa6, 0x00 },
    { 0xf0, 0xfb, 0xff, 0x00 },
    { 0xa4, 0xa0, 0xa0, 0x00 },
    { 0x80, 0x80, 0x80, 0x00 },
    { 0x00, 0x00, 0xf0, 0x00 },
    { 0x00, 0xff, 0x00, 0x00 },
    { 0x00, 0xff, 0xff, 0x00 },
    { 0xff, 0x00, 0x00, 0x00 },
    { 0xff, 0x00, 0xff, 0x00 },
    { 0xff, 0xff, 0x00, 0x00 },
    { 0xff, 0xff, 0xff, 0x00 }
};

static RGBTRIPLE DefLogPaletteTriples[20] = { /* Copy of Default Logical Palette */
/* rgbBlue, rgbGreen, rgbRed */
    { 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x80 },
    { 0x00, 0x80, 0x00 },
    { 0x00, 0x80, 0x80 },
    { 0x80, 0x00, 0x00 },
    { 0x80, 0x00, 0x80 },
    { 0x80, 0x80, 0x00 },
    { 0xc0, 0xc0, 0xc0 },
    { 0xc0, 0xdc, 0xc0 },
    { 0xf0, 0xca, 0xa6 },
    { 0xf0, 0xfb, 0xff },
    { 0xa4, 0xa0, 0xa0 },
    { 0x80, 0x80, 0x80 },
    { 0x00, 0x00, 0xf0 },
    { 0x00, 0xff, 0x00 },
    { 0x00, 0xff, 0xff },
    { 0xff, 0x00, 0x00 },
    { 0xff, 0x00, 0xff },
    { 0xff, 0xff, 0x00 },
    { 0xff, 0xff, 0xff}
};


/******************************************************************************
 * GetDIBits [GDI32.@]
 *
 * Retrieves bits of bitmap and copies to buffer.
 *
 * RETURNS
 *    Success: Number of scan lines copied from bitmap
 *    Failure: 0
 *
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/gdi/bitmaps_87eb.asp
 */
INT WINAPI GetDIBits(
    HDC hdc,         /* [in]  Handle to device context */
    HBITMAP hbitmap, /* [in]  Handle to bitmap */
    UINT startscan,  /* [in]  First scan line to set in dest bitmap */
    UINT lines,      /* [in]  Number of scan lines to copy */
    LPVOID bits,       /* [out] Address of array for bitmap bits */
    BITMAPINFO * info, /* [out] Address of structure with bitmap data */
    UINT coloruse)   /* [in]  RGB or palette index */
{
    DC * dc;
    BITMAPOBJ * bmp;
    int i;
    HDC memdc;
    int bitmap_type;
    BOOL core_header;
    LONG width;
    LONG height;
    WORD planes, bpp;
    DWORD compr, size;
    void* colorPtr;
    RGBTRIPLE* rgbTriples;
    RGBQUAD* rgbQuads;

    if (!info) return 0;

    bitmap_type = DIB_GetBitmapInfo( &info->bmiHeader, &width, &height, &planes, &bpp, &compr, &size);
    if (bitmap_type == -1)
    {
        ERR("Invalid bitmap format\n");
        return 0;
    }
    core_header = (bitmap_type == 0);
    memdc = CreateCompatibleDC(hdc);
    if (!(dc = DC_GetDCUpdate( hdc )))
    {
        DeleteDC(memdc);
        return 0;
    }
    if (!(bmp = (BITMAPOBJ *)GDI_GetObjPtr( hbitmap, BITMAP_MAGIC )))
    {
        GDI_ReleaseObj( hdc );
        DeleteDC(memdc);
	return 0;
    }

    colorPtr = (LPBYTE) info + (WORD) info->bmiHeader.biSize;
    rgbTriples = (RGBTRIPLE *) colorPtr;
    rgbQuads = (RGBQUAD *) colorPtr;

    /* Transfer color info */

    if (bpp <= 8 && bpp > 0)
    {
        if (!core_header) info->bmiHeader.biClrUsed = 0;

	/* If the bitmap object already has a dib section at the
	   same color depth then get the color map from it */
	if (bmp->dib && bmp->dib->dsBm.bmBitsPixel == bpp) {
            if(coloruse == DIB_RGB_COLORS) {
                HBITMAP oldbm = SelectObject(memdc, hbitmap);
                unsigned int colors = 1 << bpp;

                if (core_header)
                {
                    /* Convert the color table (RGBQUAD to RGBTRIPLE) */		    
                    RGBQUAD* buffer = HeapAlloc(GetProcessHeap(), 0, colors * sizeof(RGBQUAD));

                    if (buffer)
                    {
                        RGBTRIPLE* index = rgbTriples;
                        GetDIBColorTable(memdc, 0, colors, buffer);

                        for (i=0; i < colors; i++, index++)
                        {
                            index->rgbtRed   = buffer[i].rgbRed;
                            index->rgbtGreen = buffer[i].rgbGreen;
                            index->rgbtBlue  = buffer[i].rgbBlue;
                        }

                        HeapFree(GetProcessHeap(), 0, buffer);
                    }
                }
                else
                {
                    GetDIBColorTable(memdc, 0, colors, colorPtr);
                }
                SelectObject(memdc, oldbm);
            }
            else {
                WORD *index = colorPtr;
                for(i = 0; i < 1 << info->bmiHeader.biBitCount; i++, index++)
                    *index = i;
            }
        }
        else {
            if(bpp >= bmp->bitmap.bmBitsPixel) {
                /* Generate the color map from the selected palette */
                PALETTEENTRY * palEntry;
                PALETTEOBJ * palette;
                if (!(palette = (PALETTEOBJ*)GDI_GetObjPtr( dc->hPalette, PALETTE_MAGIC ))) {
                    GDI_ReleaseObj( hdc );
                    GDI_ReleaseObj( hbitmap );
                    DeleteDC(memdc);
                    return 0;
                }
                palEntry = palette->logpalette.palPalEntry;
                for (i = 0; i < (1 << bmp->bitmap.bmBitsPixel); i++, palEntry++) {
                    if (coloruse == DIB_RGB_COLORS) {
                        if (core_header)
                        {
                            rgbTriples[i].rgbtRed   = palEntry->peRed;
                            rgbTriples[i].rgbtGreen = palEntry->peGreen;
                            rgbTriples[i].rgbtBlue  = palEntry->peBlue;
                        }
                        else
                        {
                            rgbQuads[i].rgbRed      = palEntry->peRed;
                            rgbQuads[i].rgbGreen    = palEntry->peGreen;
                            rgbQuads[i].rgbBlue     = palEntry->peBlue;
                            rgbQuads[i].rgbReserved = 0;
                        }
                    }
                    else ((WORD *)colorPtr)[i] = (WORD)i;
                }
                GDI_ReleaseObj( dc->hPalette );
            } else {
                switch (bpp) {
                case 1:
                    if (core_header)
                    {
                        rgbTriples[0].rgbtRed = rgbTriples[0].rgbtGreen =
                            rgbTriples[0].rgbtBlue = 0;
                        rgbTriples[1].rgbtRed = rgbTriples[1].rgbtGreen =
                            rgbTriples[1].rgbtBlue = 0xff;
                    }
                    else
                    {    
                        rgbQuads[0].rgbRed = rgbQuads[0].rgbGreen =
                            rgbQuads[0].rgbBlue = 0;
                        rgbQuads[0].rgbReserved = 0;
                        rgbQuads[1].rgbRed = rgbQuads[1].rgbGreen =
                            rgbQuads[1].rgbBlue = 0xff;
                        rgbQuads[1].rgbReserved = 0;
                    }
                    break;

                case 4:
                    if (core_header)
                        memcpy(colorPtr, EGAColorsTriples, sizeof(EGAColorsTriples));
                    else
                        memcpy(colorPtr, EGAColorsQuads, sizeof(EGAColorsQuads));

                    break;

                case 8:
                    {
                        if (core_header)
                        {
                            INT r, g, b;
                            RGBTRIPLE *color;

                            memcpy(rgbTriples, DefLogPaletteTriples,
                                       10 * sizeof(RGBTRIPLE));
                            memcpy(rgbTriples + 246, DefLogPaletteTriples + 10,
                                       10 * sizeof(RGBTRIPLE));
                            color = rgbTriples + 10;
                            for(r = 0; r <= 5; r++) /* FIXME */
                                for(g = 0; g <= 5; g++)
                                    for(b = 0; b <= 5; b++) {
                                        color->rgbtRed =   (r * 0xff) / 5;
                                        color->rgbtGreen = (g * 0xff) / 5;
                                        color->rgbtBlue =  (b * 0xff) / 5;
                                        color++;
                                    }
                        }
                        else
                        {
                            INT r, g, b;
                            RGBQUAD *color;

                            memcpy(rgbQuads, DefLogPaletteQuads,
                                       10 * sizeof(RGBQUAD));
                            memcpy(rgbQuads + 246, DefLogPaletteQuads + 10,
                                   10 * sizeof(RGBQUAD));
                            color = rgbQuads + 10;
                            for(r = 0; r <= 5; r++) /* FIXME */
                                for(g = 0; g <= 5; g++)
                                    for(b = 0; b <= 5; b++) {
                                        color->rgbRed =   (r * 0xff) / 5;
                                        color->rgbGreen = (g * 0xff) / 5;
                                        color->rgbBlue =  (b * 0xff) / 5;
                                        color->rgbReserved = 0;
                                        color++;
                                    }
                        }
                    }
                }
            }
        }
    }

    if (bits && lines)
    {
        /* If the bitmap object already have a dib section that contains image data, get the bits from it */
        if(bmp->dib && bmp->dib->dsBm.bmBitsPixel >= 15 && bpp >= 15)
        {
            /*FIXME: Only RGB dibs supported for now */
            unsigned int srcwidth = bmp->dib->dsBm.bmWidth, srcwidthb = bmp->dib->dsBm.bmWidthBytes;
            unsigned int dstwidth = width;
            int dstwidthb = DIB_GetDIBWidthBytes( width, bpp );
            LPBYTE dbits = bits, sbits = (LPBYTE) bmp->dib->dsBm.bmBits + (startscan * srcwidthb);
            unsigned int x, y, width, widthb;

            if ((height < 0) ^ (bmp->dib->dsBmih.biHeight < 0))
            {
                dbits = (LPBYTE)bits + (dstwidthb * (lines-1));
                dstwidthb = -dstwidthb;
            }

            switch( bpp ) {

	    case 15:
            case 16: /* 16 bpp dstDIB */
                {
                    LPWORD dstbits = (LPWORD)dbits;
                    WORD rmask = 0x7c00, gmask= 0x03e0, bmask = 0x001f;

                    /* FIXME: BI_BITFIELDS not supported yet */

                    switch(bmp->dib->dsBm.bmBitsPixel) {

                    case 16: /* 16 bpp srcDIB -> 16 bpp dstDIB */
                        {
                            widthb = min(srcwidthb, abs(dstwidthb));
                            /* FIXME: BI_BITFIELDS not supported yet */
                            for (y = 0; y < lines; y++, dbits+=dstwidthb, sbits+=srcwidthb)
                                memcpy(dbits, sbits, widthb);
                        }
                        break;

                    case 24: /* 24 bpp srcDIB -> 16 bpp dstDIB */
                        {
                            LPBYTE srcbits = sbits;

                            width = min(srcwidth, dstwidth);
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++, srcbits += 3)
                                    *dstbits++ = ((srcbits[0] >> 3) & bmask) |
                                                 (((WORD)srcbits[1] << 2) & gmask) |
                                                 (((WORD)srcbits[2] << 7) & rmask);

                                dstbits = (LPWORD)(dbits+=dstwidthb);
                                srcbits = (sbits += srcwidthb);
                            }
                        }
                        break;

                    case 32: /* 32 bpp srcDIB -> 16 bpp dstDIB */
                        {
                            LPDWORD srcbits = (LPDWORD)sbits;
                            DWORD val;

                            width = min(srcwidth, dstwidth);
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++ ) {
                                    val = *srcbits++;
                                    *dstbits++ = (WORD)(((val >> 3) & bmask) | ((val >> 6) & gmask) |
                                                       ((val >> 9) & rmask));
                                }
                                dstbits = (LPWORD)(dbits+=dstwidthb);
                                srcbits = (LPDWORD)(sbits+=srcwidthb);
                            }
                        }
                        break;

                    default: /* ? bit bmp -> 16 bit DIB */
                        FIXME("15/16 bit DIB %d bit bitmap\n",
                        bmp->bitmap.bmBitsPixel);
                        break;
                    }
                }
                break;

            case 24: /* 24 bpp dstDIB */
                {
                    LPBYTE dstbits = dbits;

                    switch(bmp->dib->dsBm.bmBitsPixel) {

                    case 16: /* 16 bpp srcDIB -> 24 bpp dstDIB */
                        {
                            LPWORD srcbits = (LPWORD)sbits;
                            WORD val;

                            width = min(srcwidth, dstwidth);
                            /* FIXME: BI_BITFIELDS not supported yet */
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++ ) {
                                    val = *srcbits++;
                                    *dstbits++ = (BYTE)(((val << 3) & 0xf8) | ((val >> 2) & 0x07));
                                    *dstbits++ = (BYTE)(((val >> 2) & 0xf8) | ((val >> 7) & 0x07));
                                    *dstbits++ = (BYTE)(((val >> 7) & 0xf8) | ((val >> 12) & 0x07));
                                }
                                dstbits = (LPBYTE)(dbits+=dstwidthb);
                                srcbits = (LPWORD)(sbits+=srcwidthb);
                            }
                        }
                        break;

                    case 24: /* 24 bpp srcDIB -> 24 bpp dstDIB */
                        {
                            widthb = min(srcwidthb, abs(dstwidthb));
                            for (y = 0; y < lines; y++, dbits+=dstwidthb, sbits+=srcwidthb)
                                memcpy(dbits, sbits, widthb);
                        }
                        break;

                    case 32: /* 32 bpp srcDIB -> 24 bpp dstDIB */
                        {
                            LPBYTE srcbits = (LPBYTE)sbits;

                            width = min(srcwidth, dstwidth);
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++, srcbits++ ) {
                                    *dstbits++ = *srcbits++;
                                    *dstbits++ = *srcbits++;
                                    *dstbits++ = *srcbits++;
                                }
                                dstbits=(LPBYTE)(dbits+=dstwidthb);
                                srcbits = (LPBYTE)(sbits+=srcwidthb);
                            }
                        }
                        break;

                    default: /* ? bit bmp -> 24 bit DIB */
                        FIXME("24 bit DIB %d bit bitmap\n",
                              bmp->bitmap.bmBitsPixel);
                        break;
                    }
                }
                break;

            case 32: /* 32 bpp dstDIB */
                {
                    LPDWORD dstbits = (LPDWORD)dbits;

                    /* FIXME: BI_BITFIELDS not supported yet */

                    switch(bmp->dib->dsBm.bmBitsPixel) {
                        case 16: /* 16 bpp srcDIB -> 32 bpp dstDIB */
                        {
                            LPWORD srcbits = (LPWORD)sbits;
                            DWORD val;

                            width = min(srcwidth, dstwidth);
                            /* FIXME: BI_BITFIELDS not supported yet */
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++ ) {
                                    val = (DWORD)*srcbits++;
                                    *dstbits++ = ((val << 3) & 0xf8) | ((val >> 2) & 0x07) |
                                                 ((val << 6) & 0xf800) | ((val << 1) & 0x0700) |
                                                 ((val << 9) & 0xf80000) | ((val << 4) & 0x070000);
                                }
                                dstbits=(LPDWORD)(dbits+=dstwidthb);
                                srcbits=(LPWORD)(sbits+=srcwidthb);
                            }
                        }
                        break;

                    case 24: /* 24 bpp srcDIB -> 32 bpp dstDIB */
                        {
                            LPBYTE srcbits = sbits;

                            width = min(srcwidth, dstwidth);
                            for( y = 0; y < lines; y++) {
                                for( x = 0; x < width; x++, srcbits+=3 )
                                    *dstbits++ = ((DWORD)*srcbits) & 0x00ffffff;
                                dstbits=(LPDWORD)(dbits+=dstwidthb);
                                srcbits=(sbits+=srcwidthb);
                            }
                        }
                        break;

                    case 32: /* 32 bpp srcDIB -> 32 bpp dstDIB */
                        {
                            widthb = min(srcwidthb, abs(dstwidthb));
                            /* FIXME: BI_BITFIELDS not supported yet */
                            for (y = 0; y < lines; y++, dbits+=dstwidthb, sbits+=srcwidthb) {
                                memcpy(dbits, sbits, widthb);
                            }
                        }
                        break;

                    default: /* ? bit bmp -> 32 bit DIB */
                        FIXME("32 bit DIB %d bit bitmap\n",
                        bmp->bitmap.bmBitsPixel);
                        break;
                    }
                }
                break;

            default: /* ? bit DIB */
                FIXME("Unsupported DIB depth %d\n", info->bmiHeader.biBitCount);
                break;
            }
        }
        /* Otherwise, get bits from the XImage */
        else
        {
            if (!bmp->funcs && !BITMAP_SetOwnerDC( hbitmap, dc )) lines = 0;
            else
            {
                if (bmp->funcs && bmp->funcs->pGetDIBits)
                    lines = bmp->funcs->pGetDIBits( dc->physDev, hbitmap, startscan,
                                                    lines, bits, info, coloruse );
                else
                    lines = 0;  /* FIXME: should copy from bmp->bitmap.bmBits */
            }
        }
    }
    else
    {
	/* fill in struct members */

        if (bpp == 0)
        {
            if (core_header)
            {
                BITMAPCOREHEADER* coreheader = (BITMAPCOREHEADER*) info;
                coreheader->bcWidth = bmp->bitmap.bmWidth;
                coreheader->bcHeight = bmp->bitmap.bmHeight;
                coreheader->bcPlanes = 1;
                coreheader->bcBitCount = bmp->bitmap.bmBitsPixel;
            }
            else
            {
                info->bmiHeader.biWidth = bmp->bitmap.bmWidth;
                info->bmiHeader.biHeight = bmp->bitmap.bmHeight;
                info->bmiHeader.biPlanes = 1;
                info->bmiHeader.biSizeImage =
                                 DIB_GetDIBImageBytes( bmp->bitmap.bmWidth,
                                                       bmp->bitmap.bmHeight,
                                                       bmp->bitmap.bmBitsPixel );
                switch(bmp->bitmap.bmBitsPixel)
                {
                case 15:
                    info->bmiHeader.biBitCount = 16;
                    info->bmiHeader.biCompression = BI_RGB;
                    break;
                    
                case 16:
                    info->bmiHeader.biBitCount = 16;
                    info->bmiHeader.biCompression = BI_BITFIELDS;
                    ((PDWORD)info->bmiColors)[0] = 0xf800;
                    ((PDWORD)info->bmiColors)[1] = 0x07e0;
                    ((PDWORD)info->bmiColors)[2] = 0x001f;
                    break;
    
                default:
                    info->bmiHeader.biBitCount = bmp->bitmap.bmBitsPixel;
                    info->bmiHeader.biCompression = BI_RGB;
                    break;
                }
                info->bmiHeader.biXPelsPerMeter = 0;
                info->bmiHeader.biYPelsPerMeter = 0;
                info->bmiHeader.biClrUsed = 0;
                info->bmiHeader.biClrImportant = 0;
                
                /* Windows 2000 doesn't touch the additional struct members if
                   it's a BITMAPV4HEADER or a BITMAPV5HEADER */
            }
            lines = abs(bmp->bitmap.bmHeight);
        }
        else
        {
            /* The knowledge base article Q81498 ("DIBs and Their Uses") states that
               if bits == NULL and bpp != 0, only biSizeImage and the color table are
               filled in. */
            if (!core_header)
            {
                /* FIXME: biSizeImage should be calculated according to the selected
                          compression algorithm if biCompression != BI_RGB */
                info->bmiHeader.biSizeImage = DIB_GetDIBImageBytes( width, height, bpp );
            }
            lines = abs(height);
        }
    }

    if (!core_header)
    {
        TRACE("biSizeImage = %ld, ", info->bmiHeader.biSizeImage);
    }
    TRACE("biWidth = %ld, biHeight = %ld\n", width, height);

    GDI_ReleaseObj( hdc );
    GDI_ReleaseObj( hbitmap );
    DeleteDC(memdc);
    return lines;
}


/***********************************************************************
 *           CreateDIBitmap    (GDI32.@)
 *
 * Creates a DDB (device dependent bitmap) from a DIB.
 * The DDB will have the same color depth as the reference DC.
 */
HBITMAP WINAPI CreateDIBitmap( HDC hdc, const BITMAPINFOHEADER *header,
                            DWORD init, LPCVOID bits, const BITMAPINFO *data,
                            UINT coloruse )
{
    HBITMAP handle;
    LONG width;
    LONG height;
    WORD planes, bpp;
    DWORD compr, size;
    DC *dc;

    if (DIB_GetBitmapInfo( header, &width, &height, &planes, &bpp, &compr, &size ) == -1) return 0;
    
    if (width < 0)
    {
        TRACE("Bitmap has a negative width\n");
        return 0;
    }
    
    /* Top-down DIBs have a negative height */
    if (height < 0) height = -height;

    TRACE("hdc=%p, header=%p, init=%lu, bits=%p, data=%p, coloruse=%u (bitmap: width=%ld, height=%ld, bpp=%u, compr=%lu)\n",
           hdc, header, init, bits, data, coloruse, width, height, bpp, compr);
    
    if (hdc == NULL)
        handle = CreateBitmap( width, height, 1, 1, NULL );
    else
        handle = CreateCompatibleBitmap( hdc, width, height );

    if (handle)
    {
        if (init == CBM_INIT) SetDIBits( hdc, handle, 0, height, bits, data, coloruse );

        else if (hdc && ((dc = DC_GetDCPtr( hdc )) != NULL) )
        {
            if (!BITMAP_SetOwnerDC( handle, dc ))
            {
                DeleteObject( handle );
                handle = 0;
            }
            GDI_ReleaseObj( hdc );
        }
    }

    return handle;
}

/***********************************************************************
 *           CreateDIBSection    (GDI.489)
 */
HBITMAP16 WINAPI CreateDIBSection16 (HDC16 hdc, const BITMAPINFO *bmi, UINT16 usage,
                                     SEGPTR *bits16, HANDLE section, DWORD offset)
{
    LPVOID bits32;
    HBITMAP hbitmap;

    hbitmap = CreateDIBSection( HDC_32(hdc), bmi, usage, &bits32, section, offset );
    if (hbitmap)
    {
        BITMAPOBJ *bmp = (BITMAPOBJ *) GDI_GetObjPtr(hbitmap, BITMAP_MAGIC);
        if (bmp && bmp->dib && bits32)
        {
            const BITMAPINFOHEADER *bi = &bmi->bmiHeader;
            LONG width, height;
            WORD planes, bpp;
            DWORD compr, size;
            INT width_bytes;
            WORD count, sel;
            int i;

            DIB_GetBitmapInfo(bi, &width, &height, &planes, &bpp, &compr, &size);

            height = height >= 0 ? height : -height;
            width_bytes = DIB_GetDIBWidthBytes(width, bpp);

            if (!size || (compr != BI_RLE4 && compr != BI_RLE8)) size = width_bytes * height;

            /* calculate number of sel's needed for size with 64K steps */
            count = (size + 0xffff) / 0x10000;
            sel = AllocSelectorArray16(count);

            for (i = 0; i < count; i++)
            {
                SetSelectorBase(sel + (i << __AHSHIFT), (DWORD)bits32 + i * 0x10000);
                SetSelectorLimit16(sel + (i << __AHSHIFT), size - 1); /* yep, limit is correct */
                size -= 0x10000;
            }
            bmp->segptr_bits = MAKESEGPTR( sel, 0 );
            if (bits16) *bits16 = bmp->segptr_bits;
        }
        if (bmp) GDI_ReleaseObj( hbitmap );
    }
    return HBITMAP_16(hbitmap);
}

/***********************************************************************
 *           DIB_CreateDIBSection
 */
HBITMAP DIB_CreateDIBSection(HDC hdc, const BITMAPINFO *bmi, UINT usage,
                             VOID **bits, HANDLE section,
                             DWORD offset, DWORD ovr_pitch)
{
    HBITMAP ret = 0;
    DC *dc;
    BOOL bDesktopDC = FALSE;
    DIBSECTION *dib;
    BITMAPOBJ *bmp;
    int bitmap_type;
    LONG width, height;
    WORD planes, bpp;
    DWORD compression, sizeImage;
    void *mapBits = NULL;

    if (((bitmap_type = DIB_GetBitmapInfo( &bmi->bmiHeader, &width, &height,
                                           &planes, &bpp, &compression, &sizeImage )) == -1))
        return 0;

    if (!(dib = HeapAlloc( GetProcessHeap(), 0, sizeof(*dib) ))) return 0;

    TRACE("format (%ld,%ld), planes %d, bpp %d, size %ld, %s\n",
          width, height, planes, bpp, sizeImage, usage == DIB_PAL_COLORS? "PAL" : "RGB");

    dib->dsBm.bmType       = 0;
    dib->dsBm.bmWidth      = width;
    dib->dsBm.bmHeight     = height >= 0 ? height : -height;
    dib->dsBm.bmWidthBytes = ovr_pitch ? ovr_pitch : DIB_GetDIBWidthBytes(width, bpp);
    dib->dsBm.bmPlanes     = planes;
    dib->dsBm.bmBitsPixel  = bpp;
    dib->dsBm.bmBits       = NULL;

    if (!bitmap_type)  /* core header */
    {
        /* convert the BITMAPCOREHEADER to a BITMAPINFOHEADER */
        dib->dsBmih.biSize = sizeof(BITMAPINFOHEADER);
        dib->dsBmih.biWidth = width;
        dib->dsBmih.biHeight = height;
        dib->dsBmih.biPlanes = planes;
        dib->dsBmih.biBitCount = bpp;
        dib->dsBmih.biCompression = compression;
        dib->dsBmih.biXPelsPerMeter = 0;
        dib->dsBmih.biYPelsPerMeter = 0;
        dib->dsBmih.biClrUsed = 0;
        dib->dsBmih.biClrImportant = 0;
    }
    else
    {
        /* truncate extended bitmap headers (BITMAPV4HEADER etc.) */
        dib->dsBmih = bmi->bmiHeader;
        dib->dsBmih.biSize = sizeof(BITMAPINFOHEADER);
    }

    /* only use sizeImage if it's valid and we're dealing with a compressed bitmap */
    if (sizeImage && (compression == BI_RLE4 || compression == BI_RLE8))
        dib->dsBmih.biSizeImage = sizeImage;
    else
        dib->dsBmih.biSizeImage = dib->dsBm.bmWidthBytes * dib->dsBm.bmHeight;


    /* set dsBitfields values */
    if (usage == DIB_PAL_COLORS || bpp <= 8)
    {
        dib->dsBitfields[0] = dib->dsBitfields[1] = dib->dsBitfields[2] = 0;
    }
    else switch( bpp )
    {
    case 15:
    case 16:
        dib->dsBitfields[0] = (compression == BI_BITFIELDS) ? *(const DWORD *)bmi->bmiColors       : 0x7c00;
        dib->dsBitfields[1] = (compression == BI_BITFIELDS) ? *((const DWORD *)bmi->bmiColors + 1) : 0x03e0;
        dib->dsBitfields[2] = (compression == BI_BITFIELDS) ? *((const DWORD *)bmi->bmiColors + 2) : 0x001f;
        break;
    case 24:
    case 32:
        dib->dsBitfields[0] = (compression == BI_BITFIELDS) ? *(const DWORD *)bmi->bmiColors       : 0xff0000;
        dib->dsBitfields[1] = (compression == BI_BITFIELDS) ? *((const DWORD *)bmi->bmiColors + 1) : 0x00ff00;
        dib->dsBitfields[2] = (compression == BI_BITFIELDS) ? *((const DWORD *)bmi->bmiColors + 2) : 0x0000ff;
        break;
    }

    /* get storage location for DIB bits */

    if (section)
    {
        SYSTEM_INFO SystemInfo;
        DWORD mapOffset;
        INT mapSize;

        GetSystemInfo( &SystemInfo );
        mapOffset = offset - (offset % SystemInfo.dwAllocationGranularity);
        mapSize = dib->dsBmih.biSizeImage + (offset - mapOffset);
        mapBits = MapViewOfFile( section, FILE_MAP_ALL_ACCESS, 0, mapOffset, mapSize );
        if (mapBits) dib->dsBm.bmBits = (char *)mapBits + (offset - mapOffset);
    }
    else if (ovr_pitch && offset)
        dib->dsBm.bmBits = (LPVOID) offset;
    else
    {
        offset = 0;
        dib->dsBm.bmBits = VirtualAlloc( NULL, dib->dsBmih.biSizeImage,
                                         MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE );
    }
    dib->dshSection = section;
    dib->dsOffset = offset;

    if (!dib->dsBm.bmBits)
    {
        HeapFree( GetProcessHeap(), 0, dib );
        return 0;
    }

    /* If the reference hdc is null, take the desktop dc */
    if (hdc == 0)
    {
        hdc = CreateCompatibleDC(0);
        bDesktopDC = TRUE;
    }

    if (!(dc = DC_GetDCPtr( hdc ))) goto error;

    /* create Device Dependent Bitmap and add DIB pointer */
    ret = CreateBitmap( dib->dsBm.bmWidth, dib->dsBm.bmHeight, 1,
                        (bpp == 1) ? 1 : GetDeviceCaps(hdc, BITSPIXEL), NULL );

    if (ret && ((bmp = GDI_GetObjPtr(ret, BITMAP_MAGIC))))
    {
        bmp->dib = dib;
        bmp->funcs = dc->funcs;
        GDI_ReleaseObj( ret );

        if (dc->funcs->pCreateDIBSection)
        {
            if (!dc->funcs->pCreateDIBSection(dc->physDev, ret, bmi, usage))
            {
                DeleteObject( ret );
                ret = 0;
            }
        }
    }

    GDI_ReleaseObj(hdc);
    if (bDesktopDC) DeleteDC( hdc );
    if (ret && bits) *bits = dib->dsBm.bmBits;
    return ret;

error:
    if (bDesktopDC) DeleteDC( hdc );
    if (section) UnmapViewOfFile( mapBits );
    else if (!offset) VirtualFree( dib->dsBm.bmBits, 0, MEM_RELEASE );
    HeapFree( GetProcessHeap(), 0, dib );
    return 0;
}

/***********************************************************************
 *           CreateDIBSection    (GDI32.@)
 */
HBITMAP WINAPI CreateDIBSection(HDC hdc, CONST BITMAPINFO *bmi, UINT usage,
				VOID **bits, HANDLE section,
				DWORD offset)
{
    return DIB_CreateDIBSection(hdc, bmi, usage, bits, section, offset, 0);
}
