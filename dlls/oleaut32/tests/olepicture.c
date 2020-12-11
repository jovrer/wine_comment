/*
 * OLEPICTURE test program
 *
 * Copyright 2005 Marcus Meissner
 *
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
#include <math.h>
#include <float.h>
#include <time.h>

#define COBJMACROS

#include <wine/test.h>
#include <windef.h>
#include <winbase.h>
#include <winuser.h>
#include <wingdi.h>
#include <winnls.h>
#include <winerror.h>
#include <winnt.h>

#include <wtypes.h>
#include <olectl.h>
#include <objidl.h>

static HMODULE hOleaut32;

static HRESULT (WINAPI *pOleLoadPicture)(LPSTREAM,LONG,BOOL,REFIID,LPVOID*);

/* 1x1 pixel gif */
static const unsigned char gifimage[35] = {
0x47,0x49,0x46,0x38,0x37,0x61,0x01,0x00,0x01,0x00,0x80,0x00,0x00,0xff,0xff,0xff,
0xff,0xff,0xff,0x2c,0x00,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x02,0x02,0x44,
0x01,0x00,0x3b
};

/* 1x1 pixel jpg */
static const unsigned char jpgimage[285] = {
0xff,0xd8,0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,0x01,0x01,0x01,0x2c,
0x01,0x2c,0x00,0x00,0xff,0xdb,0x00,0x43,0x00,0x05,0x03,0x04,0x04,0x04,0x03,0x05,
0x04,0x04,0x04,0x05,0x05,0x05,0x06,0x07,0x0c,0x08,0x07,0x07,0x07,0x07,0x0f,0x0b,
0x0b,0x09,0x0c,0x11,0x0f,0x12,0x12,0x11,0x0f,0x11,0x11,0x13,0x16,0x1c,0x17,0x13,
0x14,0x1a,0x15,0x11,0x11,0x18,0x21,0x18,0x1a,0x1d,0x1d,0x1f,0x1f,0x1f,0x13,0x17,
0x22,0x24,0x22,0x1e,0x24,0x1c,0x1e,0x1f,0x1e,0xff,0xdb,0x00,0x43,0x01,0x05,0x05,
0x05,0x07,0x06,0x07,0x0e,0x08,0x08,0x0e,0x1e,0x14,0x11,0x14,0x1e,0x1e,0x1e,0x1e,
0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,
0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,
0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0x1e,0xff,0xc0,
0x00,0x11,0x08,0x00,0x01,0x00,0x01,0x03,0x01,0x22,0x00,0x02,0x11,0x01,0x03,0x11,
0x01,0xff,0xc4,0x00,0x15,0x00,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0xff,0xc4,0x00,0x14,0x10,0x01,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xc4,
0x00,0x14,0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0xff,0xc4,0x00,0x14,0x11,0x01,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xda,0x00,0x0c,0x03,0x01,
0x00,0x02,0x11,0x03,0x11,0x00,0x3f,0x00,0xb2,0xc0,0x07,0xff,0xd9
};

#if 0 /* no png support yet */
/* 1x1 pixel png */
static const unsigned char pngimage[285] = {
0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
0xde,0x00,0x00,0x00,0x09,0x70,0x48,0x59,0x73,0x00,0x00,0x0b,0x13,0x00,0x00,0x0b,
0x13,0x01,0x00,0x9a,0x9c,0x18,0x00,0x00,0x00,0x07,0x74,0x49,0x4d,0x45,0x07,0xd5,
0x06,0x03,0x0f,0x07,0x2d,0x12,0x10,0xf0,0xfd,0x00,0x00,0x00,0x0c,0x49,0x44,0x41,
0x54,0x08,0xd7,0x63,0xf8,0xff,0xff,0x3f,0x00,0x05,0xfe,0x02,0xfe,0xdc,0xcc,0x59,
0xe7,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,0x60,0x82
};
#endif

/* 1x1 pixel bmp */
static const unsigned char bmpimage[66] = {
0x42,0x4d,0x42,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x28,0x00,
0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0x00,
0x00,0x00,0x04,0x00,0x00,0x00,0x12,0x0b,0x00,0x00,0x12,0x0b,0x00,0x00,0x02,0x00,
0x00,0x00,0x02,0x00,0x00,0x00,0xff,0xff,0xff,0x00,0xff,0xff,0xff,0x00,0x00,0x00,
0x00,0x00
};

/* 2x2 pixel gif */
static const unsigned char gif4pixel[42] = {
0x47,0x49,0x46,0x38,0x37,0x61,0x02,0x00,0x02,0x00,0xa1,0x00,0x00,0x00,0x00,0x00,
0x39,0x62,0xfc,0xff,0x1a,0xe5,0xff,0xff,0xff,0x2c,0x00,0x00,0x00,0x00,0x02,0x00,
0x02,0x00,0x00,0x02,0x03,0x14,0x16,0x05,0x00,0x3b
};

struct NoStatStreamImpl
{
	const IStreamVtbl	*lpVtbl;   
	LONG			ref;

	HGLOBAL			supportHandle;
	ULARGE_INTEGER		streamSize;
	ULARGE_INTEGER		currentPosition;
};
typedef struct NoStatStreamImpl NoStatStreamImpl;
static NoStatStreamImpl* NoStatStreamImpl_Construct(HGLOBAL hGlobal);

static void
test_pic_with_stream(LPSTREAM stream, unsigned int imgsize)
{
	IPicture*	pic = NULL;
	HRESULT		hres;
	LPVOID		pvObj = NULL;
	OLE_HANDLE	handle, hPal;
	OLE_XSIZE_HIMETRIC	width;
	OLE_YSIZE_HIMETRIC	height;
	short		type;
	DWORD		attr;
	ULONG		res;

	pvObj = NULL;
	hres = pOleLoadPicture(stream, imgsize, TRUE, &IID_IPicture, &pvObj);
	pic = pvObj;

	ok(hres == S_OK,"OLP (NULL,..) does not return 0, but 0x%08lx\n",hres);
	ok(pic != NULL,"OLP (NULL,..) returns NULL, instead of !NULL\n");
	if (pic == NULL)
		return;

	pvObj = NULL;
	hres = IPicture_QueryInterface (pic, &IID_IPicture, &pvObj);

	ok(hres == S_OK,"IPicture_QI does not return S_OK, but 0x%08lx\n", hres);
	ok(pvObj != NULL,"IPicture_QI does return NULL, instead of a ptr\n");

	IPicture_Release ((IPicture*)pvObj);

	handle = 0;
	hres = IPicture_get_Handle (pic, &handle);
	ok(hres == S_OK,"IPicture_get_Handle does not return S_OK, but 0x%08lx\n", hres);
	ok(handle != 0, "IPicture_get_Handle returns a NULL handle, but it should be non NULL\n");

	width = 0;
	hres = IPicture_get_Width (pic, &width);
	ok(hres == S_OK,"IPicture_get_Width does not return S_OK, but 0x%08lx\n", hres);
	ok(width != 0, "IPicture_get_Width returns 0, but it should not be 0.\n");

	height = 0;
	hres = IPicture_get_Height (pic, &height);
	ok(hres == S_OK,"IPicture_get_Height does not return S_OK, but 0x%08lx\n", hres);
	ok(height != 0, "IPicture_get_Height returns 0, but it should not be 0.\n");

	type = 0;
	hres = IPicture_get_Type (pic, &type);
	ok(hres == S_OK,"IPicture_get_Type does not return S_OK, but 0x%08lx\n", hres);
	ok(type == PICTYPE_BITMAP, "IPicture_get_Type returns %d, but it should be PICTYPE_BITMAP(%d).\n", type, PICTYPE_BITMAP);

	attr = 0;
	hres = IPicture_get_Attributes (pic, &attr);
	ok(hres == S_OK,"IPicture_get_Attributes does not return S_OK, but 0x%08lx\n", hres);
	ok(attr == 0, "IPicture_get_Attributes returns %ld, but it should be 0.\n", attr);

	hPal = 0;
	hres = IPicture_get_hPal (pic, &hPal);
	ok(hres == S_OK,"IPicture_get_hPal does not return S_OK, but 0x%08lx\n", hres);
	/* a single pixel b/w image has no palette */
	ok(hPal == 0, "IPicture_get_hPal returns %ld, but it should be 0.\n", (long)hPal);

	res = IPicture_Release (pic);
	ok (res == 0, "refcount after release is %ld, but should be 0?\n", res);
}

static void
test_pic(const unsigned char *imgdata, unsigned int imgsize)
{
	LPSTREAM 	stream;
	HGLOBAL		hglob;
	LPBYTE		data;
	HRESULT		hres;
	LARGE_INTEGER	seekto;
	ULARGE_INTEGER	newpos1;

	/* Let the fun begin */
	hglob = GlobalAlloc (0, imgsize);
	data = GlobalLock (hglob);
	memcpy(data, imgdata, imgsize);

	hres = CreateStreamOnHGlobal (hglob, FALSE, &stream);
	ok (hres == S_OK, "createstreamonhglobal failed? doubt it... hres 0x%08lx\n", hres);

	memset(&seekto,0,sizeof(seekto));
	hres = IStream_Seek(stream,seekto,SEEK_CUR,&newpos1);
	ok (hres == S_OK, "istream seek failed? doubt it... hres 0x%08lx\n", hres);
	test_pic_with_stream(stream, imgsize);

	/* again with Non Statable and Non Seekable stream */
	stream = (LPSTREAM)NoStatStreamImpl_Construct(hglob);
	test_pic_with_stream(stream, 0);
}

static void test_empty_image(void) {
	LPBYTE		data;
	LPSTREAM	stream;
	IPicture*	pic = NULL;
	HRESULT		hres;
	LPVOID		pvObj = NULL;
	HGLOBAL		hglob;
	OLE_HANDLE	handle;
	ULARGE_INTEGER	newpos1;
	LARGE_INTEGER	seekto;
	short		type;

	/* Empty image. Happens occasionally in VB programs. */
	hglob = GlobalAlloc (0, 8);
	data = GlobalLock (hglob);
	memcpy(data,"lt\0\0",4);
	((DWORD*)data)[1] = 0;
	hres = CreateStreamOnHGlobal (hglob, TRUE, &stream);
	ok (hres == S_OK, "CreatestreamOnHGlobal failed? doubt it... hres 0x%08lx\n", hres);

	memset(&seekto,0,sizeof(seekto));
	hres = IStream_Seek(stream,seekto,SEEK_CUR,&newpos1);
	ok (hres == S_OK, "istream seek failed? doubt it... hres 0x%08lx\n", hres);

	pvObj = NULL;
	hres = pOleLoadPicture(stream, 8, TRUE, &IID_IPicture, &pvObj);
	pic = pvObj;
	ok(hres == S_OK,"empty picture not loaded, hres 0x%08lx\n", hres);
	ok(pic != NULL,"empty picture not loaded, pic is NULL\n");

	hres = IPicture_get_Type (pic, &type);
	ok (hres == S_OK,"empty picture get type failed with hres 0x%08lx\n", hres);
	ok (type == PICTYPE_NONE,"type is %d, but should be PICTYPE_NONE(0)\n", type);

	hres = IPicture_get_Handle (pic, &handle);
	ok (hres == S_OK,"empty picture get handle failed with hres 0x%08lx\n", hres);
	ok (handle == 0, "empty picture get handle did not return 0, but 0x%08x\n", handle);
	IPicture_Release (pic);
}

static void test_empty_image_2(void) {
	LPBYTE		data;
	LPSTREAM	stream;
	IPicture*	pic = NULL;
	HRESULT		hres;
	LPVOID		pvObj = NULL;
	HGLOBAL		hglob;
	ULARGE_INTEGER	newpos1;
	LARGE_INTEGER	seekto;
	short		type;

	/* Empty image at random stream position. */
	hglob = GlobalAlloc (0, 200);
	data = GlobalLock (hglob);
	data += 42;
	memcpy(data,"lt\0\0",4);
	((DWORD*)data)[1] = 0;
	hres = CreateStreamOnHGlobal (hglob, TRUE, &stream);
	ok (hres == S_OK, "CreatestreamOnHGlobal failed? doubt it... hres 0x%08lx\n", hres);

	memset(&seekto,0,sizeof(seekto));
	seekto.u.LowPart = 42;
	hres = IStream_Seek(stream,seekto,SEEK_CUR,&newpos1);
	ok (hres == S_OK, "istream seek failed? doubt it... hres 0x%08lx\n", hres);

	pvObj = NULL;
	hres = pOleLoadPicture(stream, 8, TRUE, &IID_IPicture, &pvObj);
	pic = pvObj;
	ok(hres == S_OK,"empty picture not loaded, hres 0x%08lx\n", hres);
	ok(pic != NULL,"empty picture not loaded, pic is NULL\n");

	hres = IPicture_get_Type (pic, &type);
	ok (hres == S_OK,"empty picture get type failed with hres 0x%08lx\n", hres);
	ok (type == PICTYPE_NONE,"type is %d, but should be PICTYPE_NONE(0)\n", type);

	IPicture_Release (pic);
}

START_TEST(olepicture)
{
	hOleaut32 = LoadLibraryA("oleaut32.dll");
	pOleLoadPicture = (void*)GetProcAddress(hOleaut32, "OleLoadPicture");
	if (!pOleLoadPicture)
	    return;

	/* Test regular 1x1 pixel images of gif, jpg, bmp type */
        test_pic(gifimage, sizeof(gifimage));
	test_pic(jpgimage, sizeof(jpgimage));
	test_pic(bmpimage, sizeof(bmpimage));
        test_pic(gif4pixel, sizeof(gif4pixel));
	/* No PNG support yet here or in older Windows...
	test_pic(pngimage, sizeof(pngimage));
	 */
	test_empty_image();
	test_empty_image_2();
}


/* Helper functions only ... */


static void NoStatStreamImpl_Destroy(NoStatStreamImpl* This)
{
  GlobalFree(This->supportHandle);
  This->supportHandle=0;
  HeapFree(GetProcessHeap(), 0, This);
}

static ULONG WINAPI NoStatStreamImpl_AddRef(
		IStream* iface)
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  return InterlockedIncrement(&This->ref);
}

static HRESULT WINAPI NoStatStreamImpl_QueryInterface(
		  IStream*     iface,
		  REFIID         riid,	      /* [in] */
		  void**         ppvObject)   /* [iid_is][out] */
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  if (ppvObject==0) return E_INVALIDARG;
  *ppvObject = 0;
  if (memcmp(&IID_IUnknown, riid, sizeof(IID_IUnknown)) == 0)
  {
    *ppvObject = (IStream*)This;
  }
  else if (memcmp(&IID_IStream, riid, sizeof(IID_IStream)) == 0)
  {
    *ppvObject = (IStream*)This;
  }

  if ((*ppvObject)==0)
    return E_NOINTERFACE;
  NoStatStreamImpl_AddRef(iface);
  return S_OK;
}

static ULONG WINAPI NoStatStreamImpl_Release(
		IStream* iface)
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  ULONG newRef = InterlockedDecrement(&This->ref);
  if (newRef==0)
    NoStatStreamImpl_Destroy(This);
  return newRef;
}

static HRESULT WINAPI NoStatStreamImpl_Read(
		  IStream*     iface,
		  void*          pv,        /* [length_is][size_is][out] */
		  ULONG          cb,        /* [in] */
		  ULONG*         pcbRead)   /* [out] */
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  void* supportBuffer;
  ULONG bytesReadBuffer;
  ULONG bytesToReadFromBuffer;

  if (pcbRead==0)
    pcbRead = &bytesReadBuffer;
  bytesToReadFromBuffer = min( This->streamSize.u.LowPart - This->currentPosition.u.LowPart, cb);
  supportBuffer = GlobalLock(This->supportHandle);
  memcpy(pv, (char *) supportBuffer+This->currentPosition.u.LowPart, bytesToReadFromBuffer);
  This->currentPosition.u.LowPart+=bytesToReadFromBuffer;
  *pcbRead = bytesToReadFromBuffer;
  GlobalUnlock(This->supportHandle);
  if(*pcbRead == cb)
    return S_OK;
  return S_FALSE;
}

static HRESULT WINAPI NoStatStreamImpl_Write(
	          IStream*     iface,
		  const void*    pv,          /* [size_is][in] */
		  ULONG          cb,          /* [in] */
		  ULONG*         pcbWritten)  /* [out] */
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  void*          supportBuffer;
  ULARGE_INTEGER newSize;
  ULONG          bytesWritten = 0;

  if (pcbWritten == 0)
    pcbWritten = &bytesWritten;
  if (cb == 0)
    return S_OK;
  newSize.u.HighPart = 0;
  newSize.u.LowPart = This->currentPosition.u.LowPart + cb;
  if (newSize.u.LowPart > This->streamSize.u.LowPart)
   IStream_SetSize(iface, newSize);

  supportBuffer = GlobalLock(This->supportHandle);
  memcpy((char *) supportBuffer+This->currentPosition.u.LowPart, pv, cb);
  This->currentPosition.u.LowPart+=cb;
  *pcbWritten = cb;
  GlobalUnlock(This->supportHandle);
  return S_OK;
}

static HRESULT WINAPI NoStatStreamImpl_Seek(
		  IStream*      iface,
		  LARGE_INTEGER   dlibMove,         /* [in] */
		  DWORD           dwOrigin,         /* [in] */
		  ULARGE_INTEGER* plibNewPosition) /* [out] */
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  ULARGE_INTEGER newPosition;
  switch (dwOrigin)
  {
    case STREAM_SEEK_SET:
      newPosition.u.HighPart = 0;
      newPosition.u.LowPart = 0;
      break;
    case STREAM_SEEK_CUR:
      newPosition = This->currentPosition;
      break;
    case STREAM_SEEK_END:
      newPosition = This->streamSize;
      break;
    default:
      return STG_E_INVALIDFUNCTION;
  }
  if (dlibMove.QuadPart < 0 && newPosition.QuadPart < -dlibMove.QuadPart)
      return STG_E_INVALIDFUNCTION;
  newPosition.QuadPart += dlibMove.QuadPart;
  if (plibNewPosition) *plibNewPosition = newPosition;
  This->currentPosition = newPosition;
  return S_OK;
}

static HRESULT WINAPI NoStatStreamImpl_SetSize(
				     IStream*      iface,
				     ULARGE_INTEGER  libNewSize)   /* [in] */
{
  NoStatStreamImpl* const This=(NoStatStreamImpl*)iface;
  HGLOBAL supportHandle;
  if (libNewSize.u.HighPart != 0)
    return STG_E_INVALIDFUNCTION;
  if (This->streamSize.u.LowPart == libNewSize.u.LowPart)
    return S_OK;
  supportHandle = GlobalReAlloc(This->supportHandle, libNewSize.u.LowPart, 0);
  if (supportHandle == 0)
    return STG_E_MEDIUMFULL;
  This->supportHandle = supportHandle;
  This->streamSize.u.LowPart = libNewSize.u.LowPart;
  return S_OK;
}

static HRESULT WINAPI NoStatStreamImpl_CopyTo(
				    IStream*      iface,
				    IStream*      pstm,         /* [unique][in] */
				    ULARGE_INTEGER  cb,           /* [in] */
				    ULARGE_INTEGER* pcbRead,      /* [out] */
				    ULARGE_INTEGER* pcbWritten)   /* [out] */
{
  HRESULT        hr = S_OK;
  BYTE           tmpBuffer[128];
  ULONG          bytesRead, bytesWritten, copySize;
  ULARGE_INTEGER totalBytesRead;
  ULARGE_INTEGER totalBytesWritten;

  if ( pstm == 0 )
    return STG_E_INVALIDPOINTER;
  totalBytesRead.u.LowPart = totalBytesRead.u.HighPart = 0;
  totalBytesWritten.u.LowPart = totalBytesWritten.u.HighPart = 0;

  while ( cb.u.LowPart > 0 )
  {
    if ( cb.u.LowPart >= 128 )
      copySize = 128;
    else
      copySize = cb.u.LowPart;
    IStream_Read(iface, tmpBuffer, copySize, &bytesRead);
    totalBytesRead.u.LowPart += bytesRead;
    IStream_Write(pstm, tmpBuffer, bytesRead, &bytesWritten);
    totalBytesWritten.u.LowPart += bytesWritten;
    if (bytesRead != bytesWritten)
    {
      hr = STG_E_MEDIUMFULL;
      break;
    }
    if (bytesRead!=copySize)
      cb.u.LowPart = 0;
    else
      cb.u.LowPart -= bytesRead;
  }
  if (pcbRead)
  {
    pcbRead->u.LowPart = totalBytesRead.u.LowPart;
    pcbRead->u.HighPart = totalBytesRead.u.HighPart;
  }

  if (pcbWritten)
  {
    pcbWritten->u.LowPart = totalBytesWritten.u.LowPart;
    pcbWritten->u.HighPart = totalBytesWritten.u.HighPart;
  }
  return hr;
}

static HRESULT WINAPI NoStatStreamImpl_Commit(IStream* iface,DWORD grfCommitFlags)
{
  return S_OK;
}
static HRESULT WINAPI NoStatStreamImpl_Revert(IStream* iface) { return S_OK; }

static HRESULT WINAPI NoStatStreamImpl_LockRegion(
		  IStream*       iface,
		  ULARGE_INTEGER libOffset,   /* [in] */
		  ULARGE_INTEGER cb,          /* [in] */
		  DWORD          dwLockType)  /* [in] */
{
  return S_OK;
}

static HRESULT WINAPI NoStatStreamImpl_UnlockRegion(
		  IStream*       iface,
		  ULARGE_INTEGER libOffset,   /* [in] */
		  ULARGE_INTEGER cb,          /* [in] */
		  DWORD          dwLockType)  /* [in] */
{
  return S_OK;
}

static HRESULT WINAPI NoStatStreamImpl_Stat(
		  IStream*     iface,
		  STATSTG*     pstatstg,     /* [out] */
		  DWORD        grfStatFlag)  /* [in] */
{
  return E_NOTIMPL;
}

static HRESULT WINAPI NoStatStreamImpl_Clone(
		  IStream*     iface,
		  IStream**    ppstm) /* [out] */
{
  return E_NOTIMPL;
}
static const IStreamVtbl NoStatStreamImpl_Vtbl;

static NoStatStreamImpl* NoStatStreamImpl_Construct(HGLOBAL hGlobal)
{
  NoStatStreamImpl* newStream;

  newStream = HeapAlloc(GetProcessHeap(), 0, sizeof(NoStatStreamImpl));
  if (newStream!=0)
  {
    newStream->lpVtbl = &NoStatStreamImpl_Vtbl;
    newStream->ref    = 0;
    newStream->supportHandle = hGlobal;

    if (!newStream->supportHandle)
      newStream->supportHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_NODISCARD |
					     GMEM_SHARE, 0);
    newStream->currentPosition.u.HighPart = 0;
    newStream->currentPosition.u.LowPart = 0;
    newStream->streamSize.u.HighPart = 0;
    newStream->streamSize.u.LowPart  = GlobalSize(newStream->supportHandle);
  }
  return newStream;
}


static const IStreamVtbl NoStatStreamImpl_Vtbl =
{
    NoStatStreamImpl_QueryInterface,
    NoStatStreamImpl_AddRef,
    NoStatStreamImpl_Release,
    NoStatStreamImpl_Read,
    NoStatStreamImpl_Write,
    NoStatStreamImpl_Seek,
    NoStatStreamImpl_SetSize,
    NoStatStreamImpl_CopyTo,
    NoStatStreamImpl_Commit,
    NoStatStreamImpl_Revert,
    NoStatStreamImpl_LockRegion,
    NoStatStreamImpl_UnlockRegion,
    NoStatStreamImpl_Stat,
    NoStatStreamImpl_Clone
};
