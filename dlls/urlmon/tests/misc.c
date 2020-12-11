/*
 * Copyright 2005 Jacek Caban
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

#define COBJMACROS

#include <wine/test.h>
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "ole2.h"
#include "urlmon.h"

static void test_CreateFormatEnum(void)
{
    IEnumFORMATETC *fenum = NULL, *fenum2 = NULL;
    FORMATETC fetc[5];
    ULONG ul;
    HRESULT hres;

    static DVTARGETDEVICE dev = {sizeof(dev),0,0,0,0,{0}};
    static FORMATETC formatetc[] = {
        {0,&dev,0,0,0},
        {0,&dev,0,1,0},
        {0,NULL,0,2,0},
        {0,NULL,0,3,0},
        {0,NULL,0,4,0}
    };

    hres = CreateFormatEnumerator(0, formatetc, &fenum);
    ok(hres == E_FAIL, "CreateFormatEnumerator failed: %08lx, expected E_FAIL\n", hres);
    hres = CreateFormatEnumerator(0, formatetc, NULL);
    ok(hres == E_INVALIDARG, "CreateFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = CreateFormatEnumerator(5, formatetc, NULL);
    ok(hres == E_INVALIDARG, "CreateFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);


    hres = CreateFormatEnumerator(5, formatetc, &fenum);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = IEnumFORMATETC_Next(fenum, 2, NULL, &ul);
    ok(hres == E_INVALIDARG, "Next failed: %08lx, expected E_INVALIDARG\n", hres);
    ul = 100;
    hres = IEnumFORMATETC_Next(fenum, 0, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);

    hres = IEnumFORMATETC_Next(fenum, 2, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 0, "fetc[0].lindex=%ld, expected 0\n", fetc[0].lindex);
    ok(fetc[1].lindex == 1, "fetc[1].lindex=%ld, expected 1\n", fetc[1].lindex);
    ok(fetc[0].ptd == &dev, "fetc[0].ptd=%p, expected %p\n", fetc[0].ptd, &dev);
    ok(ul == 2, "ul=%ld, expected 2\n", ul);

    hres = IEnumFORMATETC_Skip(fenum, 1);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Next(fenum, 4, fetc, &ul);
    ok(hres == S_FALSE, "Next failed: %08lx, expected S_FALSE\n", hres);
    ok(fetc[0].lindex == 3, "fetc[0].lindex=%ld, expected 3\n", fetc[0].lindex);
    ok(fetc[1].lindex == 4, "fetc[1].lindex=%ld, expected 4\n", fetc[1].lindex);
    ok(fetc[0].ptd == NULL, "fetc[0].ptd=%p, expected NULL\n", fetc[0].ptd);
    ok(ul == 2, "ul=%ld, expected 2\n", ul);

    hres = IEnumFORMATETC_Next(fenum, 4, fetc, &ul);
    ok(hres == S_FALSE, "Next failed: %08lx, expected S_FALSE\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);
    ul = 100;
    hres = IEnumFORMATETC_Next(fenum, 0, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(ul == 0, "ul=%ld, expected 0\n", ul);

    hres = IEnumFORMATETC_Skip(fenum, 3);
    ok(hres == S_FALSE, "Skip failed: %08lx, expected S_FALSE\n", hres);

    hres = IEnumFORMATETC_Reset(fenum);
    ok(hres == S_OK, "Reset failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Next(fenum, 5, fetc, NULL);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 0, "fetc[0].lindex=%ld, expected 0\n", fetc[0].lindex);

    hres = IEnumFORMATETC_Reset(fenum);
    ok(hres == S_OK, "Reset failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Skip(fenum, 2);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);

    hres = IEnumFORMATETC_Clone(fenum, NULL);
    ok(hres == E_INVALIDARG, "Clone failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IEnumFORMATETC_Clone(fenum, &fenum2);
    ok(hres == S_OK, "Clone failed: %08lx\n", hres);

    if(SUCCEEDED(hres)) {
        ok(fenum != fenum2, "fenum == fenum2\n");

        hres = IEnumFORMATETC_Next(fenum2, 2, fetc, &ul);
        ok(hres == S_OK, "Next failed: %08lx\n", hres);
        ok(fetc[0].lindex == 2, "fetc[0].lindex=%ld, expected 2\n", fetc[0].lindex);

        IEnumFORMATETC_Release(fenum2);
    }

    hres = IEnumFORMATETC_Next(fenum, 2, fetc, &ul);
    ok(hres == S_OK, "Next failed: %08lx\n", hres);
    ok(fetc[0].lindex == 2, "fetc[0].lindex=%ld, expected 2\n", fetc[0].lindex);

    hres = IEnumFORMATETC_Skip(fenum, 1);
    ok(hres == S_OK, "Skip failed: %08lx\n", hres);
    
    IEnumFORMATETC_Release(fenum);
}

static void test_RegisterFormatEnumerator(void)
{
    IBindCtx *bctx = NULL;
    IEnumFORMATETC *format = NULL, *format2 = NULL;
    IUnknown *unk = NULL;
    HRESULT hres;

    static FORMATETC formatetc = {0,NULL,0,0,0};
    static WCHAR wszEnumFORMATETC[] =
        {'_','E','n','u','m','F','O','R','M','A','T','E','T','C','_',0};

    CreateBindCtx(0, &bctx);

    hres = CreateFormatEnumerator(1, &formatetc, &format);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = RegisterFormatEnumerator(NULL, format, 0);
    ok(hres == E_INVALIDARG,
            "RegisterFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);
    hres = RegisterFormatEnumerator(bctx, NULL, 0);
    ok(hres == E_INVALIDARG,
            "RegisterFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == S_OK, "GetObjectParam failed: %08lx\n", hres);
    ok(unk == (IUnknown*)format, "unk != format\n");

    hres = RevokeFormatEnumerator(NULL, format);
    ok(hres == E_INVALIDARG,
            "RevokeFormatEnumerator failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = RevokeFormatEnumerator(bctx, format);
    ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);

    hres = RevokeFormatEnumerator(bctx, format);
    ok(hres == E_FAIL, "RevokeFormatEnumerator failed: %08lx, expected E_FAIL\n", hres);

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);

    hres = CreateFormatEnumerator(1, &formatetc, &format2);
    ok(hres == S_OK, "CreateFormatEnumerator failed: %08lx\n", hres);

    if(SUCCEEDED(hres)) {
        hres = RevokeFormatEnumerator(bctx, format);
        ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);

        IEnumFORMATETC_Release(format2);
    }

    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    IEnumFORMATETC_Release(format);

    hres = RegisterFormatEnumerator(bctx, format, 0);
    ok(hres == S_OK, "RegisterFormatEnumerator failed: %08lx\n", hres);
    hres = RevokeFormatEnumerator(bctx, NULL);
    ok(hres == S_OK, "RevokeFormatEnumerator failed: %08lx\n", hres);
    hres = IBindCtx_GetObjectParam(bctx, wszEnumFORMATETC, &unk);
    ok(hres == E_FAIL, "GetObjectParam failed: %08lx, expected E_FAIL\n", hres);

    IBindCtx_Release(bctx);
}

static const WCHAR url1[] = {'r','e','s',':','/','/','m','s','h','t','m','l','.','d','l','l',
        '/','b','l','a','n','k','.','h','t','m',0};
static const WCHAR url2[] = {'i','n','d','e','x','.','h','t','m',0};
static const WCHAR url3[] = {'f','i','l','e',':','c',':','\\','I','n','d','e','x','.','h','t','m',0};
static const WCHAR url4[] = {'f','i','l','e',':','s','o','m','e','%','2','0','f','i','l','e',
        '%','2','e','j','p','g',0};
static const WCHAR url5[] = {'h','t','t','p',':','/','/','w','w','w','.','w','i','n','e','h','q',
        '.','o','r','g',0};
static const WCHAR url6[] = {'a','b','o','u','t',':','b','l','a','n','k',0};
static const WCHAR url7[] = {'f','t','p',':','/','/','w','i','n','e','h','q','.','o','r','g','/',
        'f','i','l','e','.','t','e','s','t',0};

static const WCHAR url4e[] = {'f','i','l','e',':','s','o','m','e',' ','f','i','l','e',
        '.','j','p','g',0};

static const WCHAR path3[] = {'c',':','\\','I','n','d','e','x','.','h','t','m',0};
static const WCHAR path4[] = {'s','o','m','e',' ','f','i','l','e','.','j','p','g',0};

static const WCHAR wszRes[] = {'r','e','s',0};
static const WCHAR wszFile[] = {'f','i','l','e',0};
static const WCHAR wszHttp[] = {'h','t','t','p',0};
static const WCHAR wszAbout[] = {'a','b','o','u','t',0};
static const WCHAR wszEmpty[] = {0};

struct parse_test {
    LPCWSTR url;
    HRESULT secur_hres;
    LPCWSTR encoded_url;
    HRESULT path_hres;
    LPCWSTR path;
    LPCWSTR schema;
};

static const struct parse_test parse_tests[] = {
    {url1, S_OK,   url1,  E_INVALIDARG, NULL, wszRes},
    {url2, E_FAIL, url2,  E_INVALIDARG, NULL, wszEmpty},
    {url3, E_FAIL, url3,  S_OK, path3,        wszFile},
    {url4, E_FAIL, url4e, S_OK, path4,        wszFile},
    {url5, E_FAIL, url5,  E_INVALIDARG, NULL, wszHttp},
    {url6, S_OK,   url6,  E_INVALIDARG, NULL, wszAbout}
};

static void test_CoInternetParseUrl(void)
{
    HRESULT hres;
    DWORD size;
    int i;

    static WCHAR buf[4096];

    memset(buf, 0xf0, sizeof(buf));
    hres = CoInternetParseUrl(parse_tests[0].url, PARSE_SCHEMA, 0, buf,
            3, &size, 0);
    ok(hres == E_POINTER, "schema failed: %08lx, expected E_POINTER\n", hres);

    for(i=0; i < sizeof(parse_tests)/sizeof(parse_tests[0]); i++) {
        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_SECURITY_URL, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == parse_tests[i].secur_hres, "[%d] security url failed: %08lx, expected %08lx\n",
                i, hres, parse_tests[i].secur_hres);

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_ENCODE, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == S_OK, "[%d] encoding failed: %08lx\n", i, hres);
        ok(size == lstrlenW(parse_tests[i].encoded_url), "[%d] wrong size\n", i);
        ok(!lstrcmpW(parse_tests[i].encoded_url, buf), "[%d] wrong encoded url\n", i);

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_PATH_FROM_URL, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == parse_tests[i].path_hres, "[%d] path failed: %08lx, expected %08lx\n",
                i, hres, parse_tests[i].path_hres);
        if(parse_tests[i].path) {
            ok(size == lstrlenW(parse_tests[i].path), "[%d] wrong size\n", i);
            ok(!lstrcmpW(parse_tests[i].path, buf), "[%d] wrong path\n", i);
        }

        memset(buf, 0xf0, sizeof(buf));
        hres = CoInternetParseUrl(parse_tests[i].url, PARSE_SCHEMA, 0, buf,
                sizeof(buf)/sizeof(WCHAR), &size, 0);
        ok(hres == S_OK, "[%d] schema failed: %08lx\n", i, hres);
        ok(size == lstrlenW(parse_tests[i].schema), "[%d] wrong size\n", i);
        ok(!lstrcmpW(parse_tests[i].schema, buf), "[%d] wrong schema\n", i);
    }
}

static const WCHAR mimeTextHtml[] = {'t','e','x','t','/','h','t','m','l',0};
static const WCHAR mimeTextPlain[] = {'t','e','x','t','/','p','l','a','i','n',0};
static const WCHAR mimeAppOctetStream[] = {'a','p','p','l','i','c','a','t','i','o','n','/',
    'o','c','t','e','t','-','s','t','r','e','a','m',0};

static const struct {
    LPCWSTR url;
    LPCWSTR mime;
} mime_tests[] = {
    {url1, mimeTextHtml},
    {url2, mimeTextHtml},
    {url3, mimeTextHtml},
    {url4, NULL},
    {url5, NULL},
    {url6, NULL},
    {url7, NULL}
};

static BYTE data1[] = "test data\n";
static BYTE data2[] = {31,'t','e','s',0xfa,'t',' ','d','a','t','a','\n',0};
static BYTE data3[] = {0,0,0};
static BYTE data4[] = {'t','e','s',0xfa,'t',' ','d','a','t','a','\n',0,0};
static BYTE data5[] = {0xa,0xa,0xa,'x',32,'x',0};
static BYTE data6[] = {0xfa,0xfa,0xfa,0xfa,'\n','\r','\t','x','x','x',1};

static const struct {
    BYTE *data;
    DWORD size;
    LPCWSTR mime;
} mime_tests2[] = {
    {data1, sizeof(data1), mimeTextPlain},
    {data2, sizeof(data2), mimeAppOctetStream},
    {data3, sizeof(data3), mimeAppOctetStream},
    {data4, sizeof(data4), mimeAppOctetStream},
    {data5, sizeof(data5), mimeTextPlain},
    {data6, sizeof(data6), mimeTextPlain}
};

static void test_FindMimeFromData(void)
{
    HRESULT hres;
    LPWSTR mime;
    int i;

    for(i=0; i<sizeof(mime_tests)/sizeof(mime_tests[0]); i++) {
        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, NULL, 0, &mime, 0);
        if(mime_tests[i].mime) {
            ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
            ok(!lstrcmpW(mime, mime_tests[i].mime), "[%d] wrong mime\n", i);
            CoTaskMemFree(mime);
        }else {
            ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);
            ok(mime == (LPWSTR)0xf0f0f0f0, "[%d] mime != 0xf0f0f0f0\n", i);
        }

        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, mimeTextPlain, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mimeTextPlain), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);

        mime = (LPWSTR)0xf0f0f0f0;
        hres = FindMimeFromData(NULL, mime_tests[i].url, NULL, 0, mimeAppOctetStream, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mimeAppOctetStream), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);
    }

    for(i=0; i < sizeof(mime_tests2)/sizeof(mime_tests2[0]); i++) {
        hres = FindMimeFromData(NULL, NULL, mime_tests2[i].data, mime_tests2[i].size,
                NULL, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mime_tests2[i].mime), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);

        hres = FindMimeFromData(NULL, NULL, mime_tests2[i].data, mime_tests2[i].size,
                mimeTextHtml, 0, &mime, 0);
        ok(hres == S_OK, "[%d] FindMimeFromData failed: %08lx\n", i, hres);
        ok(!lstrcmpW(mime, mimeTextHtml), "[%d] wrong mime\n", i);
        CoTaskMemFree(mime);
    }

    hres = FindMimeFromData(NULL, url1, data1, sizeof(data1), NULL, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, url1, data1, sizeof(data1), mimeAppOctetStream, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, url4, data1, sizeof(data1), mimeAppOctetStream, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, NULL, NULL, 0, NULL, 0, &mime, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, excepted E_INVALIDARG\n", hres);

    hres = FindMimeFromData(NULL, NULL, NULL, 0, mimeTextPlain, 0, &mime, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = FindMimeFromData(NULL, NULL, data1, 0, NULL, 0, &mime, 0);
    ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);

    hres = FindMimeFromData(NULL, url1, data1, 0, NULL, 0, &mime, 0);
    ok(hres == E_FAIL, "FindMimeFromData failed: %08lx, expected E_FAIL\n", hres);

    hres = FindMimeFromData(NULL, NULL, data1, 0, mimeTextPlain, 0, &mime, 0);
    ok(hres == S_OK, "FindMimeFromData failed: %08lx\n", hres);
    ok(!lstrcmpW(mime, mimeTextPlain), "wrong mime\n");
    CoTaskMemFree(mime);

    hres = FindMimeFromData(NULL, NULL, data1, 0, mimeTextPlain, 0, NULL, 0);
    ok(hres == E_INVALIDARG, "FindMimeFromData failed: %08lx, expected E_INVALIDARG\n", hres);
}

static struct secmgr_test {
    LPCWSTR url;
    DWORD zone;
    HRESULT zone_hres;
} secmgr_tests[] = {
    {url1, 0,   S_OK},
    {url2, 100, 0x80041001},
    {url3, 0,   S_OK},
    {url4, 3,   S_OK},
    {url5, 3,   S_OK},
    {url6, 3,   S_OK},
    {url7, 3,   S_OK}
};

static void test_SecurityManager(void)
{
    int i;
    IInternetSecurityManager *secmgr = NULL;
    DWORD zone;
    HRESULT hres;

    hres = CoInternetCreateSecurityManager(NULL, &secmgr, 0);
    ok(hres == S_OK, "CoInternetCreateSecurityManager failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    for(i=0; i < sizeof(secmgr_tests)/sizeof(secmgr_tests[0]); i++) {
        zone = 100;
        hres = IInternetSecurityManager_MapUrlToZone(secmgr, secmgr_tests[i].url, &zone, 0);
        ok(hres == secmgr_tests[i].zone_hres, "[%d] MapUrlToZone failed: %08lx, expected %08lx\n",
                i, hres, secmgr_tests[i].zone_hres);
        ok(zone == secmgr_tests[i].zone, "[%d] zone=%ld, expected %ld\n", i, zone,
                secmgr_tests[i].zone);
    }

    zone = 100;
    hres = IInternetSecurityManager_MapUrlToZone(secmgr, NULL, &zone, 0);
    ok(hres == E_INVALIDARG, "MapUrlToZone failed: %08lx, expected E_INVALIDARG\n", hres);

    IInternetSecurityManager_Release(secmgr);
}

static void test_ZoneManager(void)
{
    IInternetZoneManager *zonemgr = NULL;
    BYTE buf[32];
    HRESULT hres;

    hres = CoInternetCreateZoneManager(NULL, &zonemgr, 0);
    ok(hres == S_OK, "CoInternetCreateZoneManager failed: %08lx\n", hres);
    if(FAILED(hres))
        return;

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == S_OK, "GetZoneActionPolicy failed: %08lx\n", hres);
    ok(*(DWORD*)buf == 1, "policy=%ld, expected 1\n", *(DWORD*)buf);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, NULL,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1a10, buf,
            2, URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 3, 0x1fff, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_FAIL, "GetZoneActionPolicy failed: %08lx, expected E_FAIL\n", hres);

    hres = IInternetZoneManager_GetZoneActionPolicy(zonemgr, 13, 0x1a10, buf,
            sizeof(DWORD), URLZONEREG_DEFAULT);
    ok(hres == E_INVALIDARG, "GetZoneActionPolicy failed: %08lx, expected E_INVALIDARG\n", hres);

    IInternetZoneManager_Release(zonemgr);
}

START_TEST(misc)
{
    test_CreateFormatEnum();
    test_RegisterFormatEnumerator();
    test_CoInternetParseUrl();
    test_FindMimeFromData();
    test_SecurityManager();
    test_ZoneManager();
}
