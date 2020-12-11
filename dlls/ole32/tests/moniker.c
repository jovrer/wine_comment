/*
 * Moniker Tests
 *
 * Copyright 2004 Robert Shearman
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

#define _WIN32_DCOM
#define COBJMACROS

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "comcat.h"

#include "wine/test.h"

#define ok_ole_success(hr, func) ok(hr == S_OK, #func " failed with error 0x%08lx\n", hr)
#define COUNTOF(x) (sizeof(x) / sizeof(x[0]))

static const WCHAR wszFileName1[] = {'c',':','\\','w','i','n','d','o','w','s','\\','t','e','s','t','1','.','d','o','c',0};
static const WCHAR wszFileName2[] = {'c',':','\\','w','i','n','d','o','w','s','\\','t','e','s','t','2','.','d','o','c',0};

static int count_moniker_matches(IBindCtx * pbc, IEnumMoniker * spEM)
{
    IMoniker * spMoniker;
    int monCnt=0, matchCnt=0;

    while ((IEnumMoniker_Next(spEM, 1, &spMoniker, NULL)==S_OK))
    {
        HRESULT hr;
        WCHAR * szDisplayn;
        monCnt++;
        hr=IMoniker_GetDisplayName(spMoniker, pbc, NULL, &szDisplayn);
        if (SUCCEEDED(hr))
        {
            if (!lstrcmpW(szDisplayn, wszFileName1) || !lstrcmpW(szDisplayn, wszFileName2))
                matchCnt++;
            CoTaskMemFree(szDisplayn);
        }
    }
    trace("Total number of monikers is %i\n", monCnt);
    return matchCnt;
}

static void test_MkParseDisplayName(void)
{
    IBindCtx * pbc = NULL;
    HRESULT hr;
    IMoniker * pmk  = NULL;
    IMoniker * pmk1 = NULL;
    IMoniker * pmk2 = NULL;
    ULONG eaten;
    int matchCnt;
    IUnknown * object = NULL;

    IUnknown *lpEM1;

    IEnumMoniker *spEM1  = NULL;
    IEnumMoniker *spEM2  = NULL;
    IEnumMoniker *spEM3  = NULL;

    DWORD pdwReg1=0;
    DWORD grflags=0;
    DWORD pdwReg2=0;
    IRunningObjectTable * pprot=NULL;

    /* CLSID of My Computer */
    static const WCHAR wszDisplayName[] = {'c','l','s','i','d',':',
        '2','0','D','0','4','F','E','0','-','3','A','E','A','-','1','0','6','9','-','A','2','D','8','-','0','8','0','0','2','B','3','0','3','0','9','D',':',0};

    hr = CreateBindCtx(0, &pbc);
    ok_ole_success(hr, CreateBindCtx);

    hr = MkParseDisplayName(pbc, wszDisplayName, &eaten, &pmk);
    todo_wine { ok_ole_success(hr, MkParseDisplayName); }

    if (object)
    {
        hr = IMoniker_BindToObject(pmk, pbc, NULL, &IID_IUnknown, (LPVOID*)&object);
        ok_ole_success(hr, IMoniker_BindToObject);

        IUnknown_Release(object);
    }
    IBindCtx_Release(pbc);

    /* Test the EnumMoniker interface */
    hr = CreateBindCtx(0, &pbc);
    ok_ole_success(hr, CreateBindCtx);

    hr = CreateFileMoniker(wszFileName1, &pmk1);
    ok(hr==0, "CreateFileMoniker for file hr=%08lx\n", hr);
    hr = CreateFileMoniker(wszFileName2, &pmk2);
    ok(hr==0, "CreateFileMoniker for file hr=%08lx\n", hr);
    hr = IBindCtx_GetRunningObjectTable(pbc, &pprot);
    ok(hr==0, "IBindCtx_GetRunningObjectTable hr=%08lx\n", hr);

    /* Check EnumMoniker before registering */
    hr = IRunningObjectTable_EnumRunning(pprot, &spEM1);
    ok(hr==0, "IRunningObjectTable_EnumRunning hr=%08lx\n", hr);
    hr = IEnumMoniker_QueryInterface(spEM1, &IID_IUnknown, (void*) &lpEM1);
    /* Register a couple of Monikers and check is ok */
    ok(hr==0, "IEnumMoniker_QueryInterface hr %08lx %p\n", hr, lpEM1);
    hr = MK_E_NOOBJECT;
    
    matchCnt = count_moniker_matches(pbc, spEM1);
    trace("Number of matches is %i\n", matchCnt);

    grflags= grflags | ROTFLAGS_REGISTRATIONKEEPSALIVE;
    hr = IRunningObjectTable_Register(pprot, grflags, lpEM1, pmk1, &pdwReg1);
    ok(hr==0, "IRunningObjectTable_Register hr=%08lx %p %08lx %p %p %ld\n",
        hr, pprot, grflags, lpEM1, pmk1, pdwReg1);

    trace("IROT::Register\n");
    grflags=0;
    grflags= grflags | ROTFLAGS_REGISTRATIONKEEPSALIVE;
    hr = IRunningObjectTable_Register(pprot, grflags, lpEM1, pmk2, &pdwReg2);
    ok(hr==0, "IRunningObjectTable_Register hr=%08lx %p %08lx %p %p %ld\n", hr,
       pprot, grflags, lpEM1, pmk2, pdwReg2);

    hr = IRunningObjectTable_EnumRunning(pprot, &spEM2);
    ok(hr==0, "IRunningObjectTable_EnumRunning hr=%08lx\n", hr);

    matchCnt = count_moniker_matches(pbc, spEM2);
    ok(matchCnt==2, "Number of matches should be equal to 2 not %i\n", matchCnt);

    trace("IEnumMoniker::Clone\n");
    IEnumMoniker_Clone(spEM2, &spEM3);

    matchCnt = count_moniker_matches(pbc, spEM3);
    ok(matchCnt==0, "Number of matches should be equal to 0 not %i\n", matchCnt);
    trace("IEnumMoniker::Reset\n");
    IEnumMoniker_Reset(spEM3);

    matchCnt = count_moniker_matches(pbc, spEM3);
    ok(matchCnt==2, "Number of matches should be equal to 2 not %i\n", matchCnt);

    IRunningObjectTable_Revoke(pprot,pdwReg1);
    IRunningObjectTable_Revoke(pprot,pdwReg2);
    IEnumMoniker_Release(spEM1);
    IEnumMoniker_Release(spEM1);
    IEnumMoniker_Release(spEM2);
    IEnumMoniker_Release(spEM3);
    IMoniker_Release(pmk1);
    IMoniker_Release(pmk2);
    IRunningObjectTable_Release(pprot);

    IBindCtx_Release(pbc);
}

static const BYTE expected_moniker_data[] =
{
	0x4d,0x45,0x4f,0x57,0x04,0x00,0x00,0x00,
	0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46,
	0x1a,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
	0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46,
	0x00,0x00,0x00,0x00,0x14,0x00,0x00,0x00,
	0x05,0xe0,0x02,0x00,0x00,0x00,0x00,0x00,
	0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46,
	0x00,0x00,0x00,0x00,
};

static const LARGE_INTEGER llZero;

static void test_class_moniker(void)
{
    IStream * stream;
    IMoniker * moniker;
    HRESULT hr;
    HGLOBAL hglobal;
    LPBYTE moniker_data;
    DWORD moniker_size;
    DWORD i;
    BOOL same = TRUE;

    hr = CreateClassMoniker(&CLSID_StdComponentCategoriesMgr, &moniker);
    todo_wine { ok_ole_success(hr, CreateClassMoniker); }

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);

    hr = CoMarshalInterface(stream, &IID_IMoniker, (IUnknown *)moniker, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    todo_wine { ok_ole_success(hr, CoMarshalInterface); }

    hr = GetHGlobalFromStream(stream, &hglobal);
    ok_ole_success(hr, GetHGlobalFromStream);

    moniker_size = GlobalSize(hglobal);

    moniker_data = GlobalLock(hglobal);

    /* first check we have the right amount of data */
    todo_wine {
    ok(moniker_size == sizeof(expected_moniker_data),
        "Size of marshaled data differs (expected %d, actual %ld)\n",
        sizeof(expected_moniker_data), moniker_size);
    }

    /* then do a byte-by-byte comparison */
    for (i = 0; i < min(moniker_size, sizeof(expected_moniker_data)); i++)
    {
        if (expected_moniker_data[i] != moniker_data[i])
        {
            same = FALSE;
            break;
        }
    }

    ok(same, "Marshaled data differs\n");
    if (!same)
    {
        trace("Dumping marshaled moniker data:\n");
        for (i = 0; i < moniker_size; i++)
        {
            trace("0x%02x,", moniker_data[i]);
            if (i % 8 == 7) trace("\n");
            if (i % 8 == 0) trace("     ");
        }
    }

    GlobalUnlock(hglobal);

    IStream_Seek(stream, llZero, STREAM_SEEK_SET, NULL);
    hr = CoReleaseMarshalData(stream);
    todo_wine { ok_ole_success(hr, CoReleaseMarshalData); }

    IStream_Release(stream);
    if (moniker) IMoniker_Release(moniker);
}

static void test_file_moniker(WCHAR* path)
{
    IStream *stream;
    IMoniker *moniker1 = NULL, *moniker2 = NULL;
    HRESULT hr;

    hr = CreateFileMoniker(path, &moniker1);
    ok_ole_success(hr, CreateFileMoniker); 

    hr = CreateStreamOnHGlobal(NULL, TRUE, &stream);

    /* Marshal */
    hr = CoMarshalInterface(stream, &IID_IMoniker, (IUnknown *)moniker1, MSHCTX_INPROC, NULL, MSHLFLAGS_NORMAL);
    ok_ole_success(hr, CoMarshalInterface);
    
    /* Rewind */
    hr = IStream_Seek(stream, llZero, STREAM_SEEK_SET, NULL);
    ok_ole_success(hr, IStream_Seek);

    /* Unmarshal */
    hr = CoUnmarshalInterface(stream, &IID_IMoniker, (void**)&moniker2);
    ok_ole_success(hr, CoUnmarshalInterface);

    hr = IMoniker_IsEqual(moniker1, moniker2);
    ok_ole_success(hr, IsEqual);

    IStream_Release(stream);
    if (moniker1) 
        IMoniker_Release(moniker1);
    if (moniker2) 
        IMoniker_Release(moniker2);
}

static void test_file_monikers(void)
{
    static WCHAR wszFile[][30] = {
        {'\\', 'w','i','n','d','o','w','s','\\','s','y','s','t','e','m','\\','t','e','s','t','1','.','d','o','c',0},
        {'\\', 'a','b','c','d','e','f','g','\\','h','i','j','k','l','\\','m','n','o','p','q','r','s','t','u','.','m','n','o',0},
        /* These map to themselves in Windows-1252 & 932 (Shift-JIS) */
        {0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0},
        /* U+2020 = DAGGER     = 0x86 (1252) = 0x813f (932)
         * U+20AC = EURO SIGN  = 0x80 (1252) =  undef (932)
         * U+0100 .. = Latin extended-A
         */ 
        {0x20ac, 0x2020, 0x100, 0x101, 0x102, 0x103, 0x104, 0x105, 0x106, 0x107, 0x108, 0x109, 0x10a, 0x10b, 0x10c,  0},
        };

    int i; 

    trace("ACP is %u\n", GetACP());

    for (i = 0; i < COUNTOF(wszFile); ++i)
    {
        int j ;
        for (j = lstrlenW(wszFile[i]); j > 0; --j)
        {
            wszFile[i][j] = 0;
            test_file_moniker(wszFile[i]);
        }
    }
}

START_TEST(moniker)
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    test_MkParseDisplayName();
    test_class_moniker();
    test_file_monikers();

    /* FIXME: test moniker creation funcs and parsing other moniker formats */

    CoUninitialize();
}
