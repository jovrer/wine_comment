/*
 * Unit test suite for crypt32.dll's CryptEncodeObjectEx/CryptDecodeObjectEx
 *
 * Copyright 2005 Juan Lang
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
#include <stdio.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <winerror.h>
#include <wincrypt.h>

#include "wine/test.h"

struct encodedInt
{
    int val;
    const BYTE *encoded;
};

static const BYTE bin1[] = {0x02,0x01,0x01,0};
static const BYTE bin2[] = {0x02,0x01,0x7f,0};
static const BYTE bin3[] = {0x02,0x02,0x00,0x80,0};
static const BYTE bin4[] = {0x02,0x02,0x01,0x00,0};
static const BYTE bin5[] = {0x02,0x01,0x80,0};
static const BYTE bin6[] = {0x02,0x02,0xff,0x7f,0};
static const BYTE bin7[] = {0x02,0x04,0xba,0xdd,0xf0,0x0d,0};

static const struct encodedInt ints[] = {
 { 1,          bin1 },
 { 127,        bin2 },
 { 128,        bin3 },
 { 256,        bin4 },
 { -128,       bin5 },
 { -129,       bin6 },
 { 0xbaddf00d, bin7 },
};

struct encodedBigInt
{
    const BYTE *val;
    const BYTE *encoded;
    const BYTE *decoded;
};

static const BYTE bin8[] = {0xff,0xff,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0};
static const BYTE bin9[] = {0x02,0x0a,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0xff,0xff,0};
static const BYTE bin10[] = {0xff,0xff,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0};

static const BYTE bin11[] = {0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0xff,0xff,0xff,0};
static const BYTE bin12[] = {0x02,0x09,0xff,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0};
static const BYTE bin13[] = {0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0xff,0};

static const struct encodedBigInt bigInts[] = {
 { bin8, bin9, bin10 },
 { bin11, bin12, bin13 },
};

static const BYTE bin14[] = {0xff,0xff,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0};
static const BYTE bin15[] = {0x02,0x0a,0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0xff,0xff,0};
static const BYTE bin16[] = {0x08,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0xff,0xff,0xff,0};
static const BYTE bin17[] = {0x02,0x0c,0x00,0xff,0xff,0xff,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0};

/* Decoded is the same as original, so don't bother storing a separate copy */
static const struct encodedBigInt bigUInts[] = {
 { bin14, bin15, NULL },
 { bin16, bin17, NULL },
};

static void test_encodeInt(DWORD dwEncoding)
{
    DWORD bufSize = 0;
    int i;
    BOOL ret;
    CRYPT_INTEGER_BLOB blob;
    BYTE *buf = NULL;

    /* CryptEncodeObjectEx with NULL bufSize crashes..
    ret = CryptEncodeObjectEx(3, X509_INTEGER, &ints[0].val, 0, NULL, NULL,
     NULL);
     */
    /* check bogus encoding */
    ret = CryptEncodeObjectEx(0, X509_INTEGER, &ints[0].val, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %ld\n", GetLastError());
    /* check with NULL integer buffer.  Windows XP incorrectly returns an
     * NTSTATUS.
     */
    ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, NULL, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
    {
        /* encode as normal integer */
        ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, &ints[i].val, 0,
         NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_INTEGER, &ints[i].val,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == ints[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], ints[i].encoded[1]);
            ok(!memcmp(buf + 1, ints[i].encoded + 1, ints[i].encoded[1] + 1),
             "Encoded value of 0x%08x didn't match expected\n", ints[i].val);
            LocalFree(buf);
        }
        /* encode as multibyte integer */
        blob.cbData = sizeof(ints[i].val);
        blob.pbData = (BYTE *)&ints[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == ints[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], ints[i].encoded[1]);
            ok(!memcmp(buf + 1, ints[i].encoded + 1, ints[i].encoded[1] + 1),
             "Encoded value of 0x%08x didn't match expected\n", ints[i].val);
            LocalFree(buf);
        }
    }
    /* encode a couple bigger ints, just to show it's little-endian and leading
     * sign bytes are dropped
     */
    for (i = 0; i < sizeof(bigInts) / sizeof(bigInts[0]); i++)
    {
        blob.cbData = strlen((const char*)bigInts[i].val);
        blob.pbData = (BYTE *)bigInts[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == bigInts[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], bigInts[i].encoded[1]);
            ok(!memcmp(buf + 1, bigInts[i].encoded + 1,
             bigInts[i].encoded[1] + 1),
             "Encoded value didn't match expected\n");
            LocalFree(buf);
        }
    }
    /* and, encode some uints */
    for (i = 0; i < sizeof(bigUInts) / sizeof(bigUInts[0]); i++)
    {
        blob.cbData = strlen((const char*)bigUInts[i].val);
        blob.pbData = (BYTE*)bigUInts[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT, &blob,
         0, NULL, NULL, &bufSize);
        ok(ret, "Expected success, got %ld\n", GetLastError());
        ret = CryptEncodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 2, "Got unexpected type %d for integer (expected 2)\n",
             buf[0]);
            ok(buf[1] == bigUInts[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], bigUInts[i].encoded[1]);
            ok(!memcmp(buf + 1, bigUInts[i].encoded + 1,
             bigUInts[i].encoded[1] + 1),
             "Encoded value didn't match expected\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeInt(DWORD dwEncoding)
{
    static const BYTE bigInt[] = { 2, 5, 0xff, 0xfe, 0xff, 0xfe, 0xff };
    static const BYTE testStr[] = { 0x16, 4, 't', 'e', 's', 't' };
    static const BYTE longForm[] = { 2, 0x81, 0x01, 0x01 };
    static const BYTE bigBogus[] = { 0x02, 0x84, 0x01, 0xff, 0xff, 0xf9 };
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    int i;
    BOOL ret;

    /* CryptDecodeObjectEx with NULL bufSize crashes..
    ret = CryptDecodeObjectEx(3, X509_INTEGER, &ints[0].encoded, 
     ints[0].encoded[1] + 2, 0, NULL, NULL, NULL);
     */
    /* check bogus encoding */
    ret = CryptDecodeObjectEx(3, X509_INTEGER, (BYTE *)&ints[0].encoded, 
     ints[0].encoded[1] + 2, 0, NULL, NULL, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %ld\n", GetLastError());
    /* check with NULL integer buffer */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, NULL, 0, 0, NULL, NULL,
     &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_EOD,
     "Expected CRYPT_E_ASN1_EOD, got %08lx\n", GetLastError());
    /* check with a valid, but too large, integer */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, bigInt, bigInt[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_LARGE,
     "Expected CRYPT_E_ASN1_LARGE, got %ld\n", GetLastError());
    /* check with a DER-encoded string */
    ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER, testStr, testStr[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_BADTAG,
     "Expected CRYPT_E_ASN1_BADTAG, got %ld\n", GetLastError());
    for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
    {
        /* When the output buffer is NULL, this always succeeds */
        SetLastError(0xdeadbeef);
        ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER,
         (BYTE *)ints[i].encoded, ints[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_INTEGER,
         (BYTE *)ints[i].encoded, ints[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize == sizeof(int), "Expected size %d, got %ld\n", sizeof(int),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            ok(!memcmp(buf, &ints[i].val, bufSize), "Expected %d, got %d\n",
             ints[i].val, *(int *)buf);
            LocalFree(buf);
        }
    }
    for (i = 0; i < sizeof(bigInts) / sizeof(bigInts[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER,
         (BYTE *)bigInts[i].encoded, bigInts[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER,
         (BYTE *)bigInts[i].encoded, bigInts[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_INTEGER_BLOB),
         "Expected size at least %d, got %ld\n", sizeof(CRYPT_INTEGER_BLOB),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_INTEGER_BLOB *blob = (CRYPT_INTEGER_BLOB *)buf;

            ok(blob->cbData == strlen((const char*)bigInts[i].decoded),
             "Expected len %d, got %ld\n", strlen((const char*)bigInts[i].decoded),
             blob->cbData);
            ok(!memcmp(blob->pbData, bigInts[i].decoded, blob->cbData),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
    for (i = 0; i < sizeof(bigUInts) / sizeof(bigUInts[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT,
         (BYTE *)bigUInts[i].encoded, bigUInts[i].encoded[1] + 2, 0, NULL, NULL,
         &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %ld\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_UINT,
         (BYTE *)bigUInts[i].encoded, bigUInts[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %ld\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_INTEGER_BLOB),
         "Expected size at least %d, got %ld\n", sizeof(CRYPT_INTEGER_BLOB),
         bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_INTEGER_BLOB *blob = (CRYPT_INTEGER_BLOB *)buf;

            ok(blob->cbData == strlen((const char*)bigUInts[i].val),
             "Expected len %d, got %ld\n", strlen((const char*)bigUInts[i].val),
             blob->cbData);
            ok(!memcmp(blob->pbData, bigUInts[i].val, blob->cbData),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
    /* Decode the value 1 with long-form length */
    ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, longForm,
     sizeof(longForm), CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(*(int *)buf == 1, "Expected 1, got %d\n", *(int *)buf);
        LocalFree(buf);
    }
    /* Try to decode some bogus large items */
    /* The buffer size is smaller than the encoded length, so this should fail
     * with CRYPT_E_ASN1_EOD if it's being decoded.
     * Under XP it fails with CRYPT_E_ASN1_LARGE, which means there's a limit
     * on the size decoded, but in ME it fails with CRYPT_E_ASN1_EOD or crashes.
     * So this test unfortunately isn't useful.
    ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, tooBig,
     0x7fffffff, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_LARGE,
     "Expected CRYPT_E_ASN1_LARGE, got %08lx\n", GetLastError());
     */
    /* This will try to decode the buffer and overflow it, check that it's
     * caught.
     */
    ret = CryptDecodeObjectEx(dwEncoding, X509_MULTI_BYTE_INTEGER, bigBogus,
     0x01ffffff, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
}

static const BYTE bin18[] = {0x0a,0x01,0x01,0};
static const BYTE bin19[] = {0x0a,0x05,0x00,0xff,0xff,0xff,0x80,0};

/* These are always encoded unsigned, and aren't constrained to be any
 * particular value
 */
static const struct encodedInt enums[] = {
 { 1,    bin18 },
 { -128, bin19 },
};

/* X509_CRL_REASON_CODE is also an enumerated type, but it's #defined to
 * X509_ENUMERATED.
 */
static const LPCSTR enumeratedTypes[] = { X509_ENUMERATED,
 szOID_CRL_REASON_CODE };

static void test_encodeEnumerated(DWORD dwEncoding)
{
    DWORD i, j;

    for (i = 0; i < sizeof(enumeratedTypes) / sizeof(enumeratedTypes[0]); i++)
    {
        for (j = 0; j < sizeof(enums) / sizeof(enums[0]); j++)
        {
            BOOL ret;
            BYTE *buf = NULL;
            DWORD bufSize = 0;

            ret = CryptEncodeObjectEx(dwEncoding, enumeratedTypes[i],
             &enums[j].val, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
             &bufSize);
            ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
            if (buf)
            {
                ok(buf[0] == 0xa,
                 "Got unexpected type %d for enumerated (expected 0xa)\n",
                 buf[0]);
                ok(buf[1] == enums[j].encoded[1],
                 "Got length %d, expected %d\n", buf[1], enums[j].encoded[1]);
                ok(!memcmp(buf + 1, enums[j].encoded + 1,
                 enums[j].encoded[1] + 1),
                 "Encoded value of 0x%08x didn't match expected\n",
                 enums[j].val);
                LocalFree(buf);
            }
        }
    }
}

static void test_decodeEnumerated(DWORD dwEncoding)
{
    DWORD i, j;

    for (i = 0; i < sizeof(enumeratedTypes) / sizeof(enumeratedTypes[0]); i++)
    {
        for (j = 0; j < sizeof(enums) / sizeof(enums[0]); j++)
        {
            BOOL ret;
            DWORD bufSize = sizeof(int);
            int val;

            ret = CryptDecodeObjectEx(dwEncoding, enumeratedTypes[i],
             enums[j].encoded, enums[j].encoded[1] + 2, 0, NULL,
             (BYTE *)&val, &bufSize);
            ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
            ok(bufSize == sizeof(int),
             "Got unexpected size %ld for enumerated (expected %d)\n",
             bufSize, sizeof(int));
            ok(val == enums[j].val, "Unexpected value %d, expected %d\n",
             val, enums[j].val);
        }
    }
}

struct encodedFiletime
{
    SYSTEMTIME sysTime;
    const BYTE *encodedTime;
};

static void testTimeEncoding(DWORD dwEncoding, LPCSTR structType,
 const struct encodedFiletime *time)
{
    FILETIME ft = { 0 };
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    BOOL ret;

    ret = SystemTimeToFileTime(&time->sysTime, &ft);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    ret = CryptEncodeObjectEx(dwEncoding, structType, &ft,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    /* years other than 1950-2050 are not allowed for encodings other than
     * X509_CHOICE_OF_TIME.
     */
    if (structType == X509_CHOICE_OF_TIME ||
     (time->sysTime.wYear >= 1950 && time->sysTime.wYear <= 2050))
    {
        ok(ret, "CryptEncodeObjectEx failed: %ld (0x%08lx)\n", GetLastError(),
         GetLastError());
        ok(buf != NULL, "Expected an allocated buffer\n");
        if (buf)
        {
            ok(buf[0] == time->encodedTime[0],
             "Expected type 0x%02x, got 0x%02x\n", time->encodedTime[0],
             buf[0]);
            ok(buf[1] == time->encodedTime[1], "Expected %d bytes, got %ld\n",
             time->encodedTime[1], bufSize);
            ok(!memcmp(time->encodedTime + 2, buf + 2, time->encodedTime[1]),
             "Got unexpected value for time encoding\n");
            LocalFree(buf);
        }
    }
    else
        ok(!ret && GetLastError() == CRYPT_E_BAD_ENCODE,
         "Expected CRYPT_E_BAD_ENCODE, got 0x%08lx\n", GetLastError());
}

static void testTimeDecoding(DWORD dwEncoding, LPCSTR structType,
 const struct encodedFiletime *time)
{
    FILETIME ft1 = { 0 }, ft2 = { 0 };
    DWORD size = sizeof(ft2);
    BOOL ret;

    ret = SystemTimeToFileTime(&time->sysTime, &ft1);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    ret = CryptDecodeObjectEx(dwEncoding, structType, time->encodedTime,
     time->encodedTime[1] + 2, 0, NULL, &ft2, &size);
    /* years other than 1950-2050 are not allowed for encodings other than
     * X509_CHOICE_OF_TIME.
     */
    if (structType == X509_CHOICE_OF_TIME ||
     (time->sysTime.wYear >= 1950 && time->sysTime.wYear <= 2050))
    {
        ok(ret, "CryptDecodeObjectEx failed: %ld (0x%08lx)\n", GetLastError(),
         GetLastError());
        ok(!memcmp(&ft1, &ft2, sizeof(ft1)),
         "Got unexpected value for time decoding\n");
    }
    else
        ok(!ret && GetLastError() == CRYPT_E_ASN1_BADTAG,
         "Expected CRYPT_E_ASN1_BADTAG, got 0x%08lx\n", GetLastError());
}

static const BYTE bin20[] = {
    0x17,0x0d,'0','5','0','6','0','6','1','6','1','0','0','0','Z',0};
static const BYTE bin21[] = {
    0x18,0x0f,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','Z',0};
static const BYTE bin22[] = {
    0x18,0x0f,'2','1','4','5','0','6','0','6','1','6','1','0','0','0','Z',0};

static const struct encodedFiletime times[] = {
 { { 2005, 6, 1, 6, 16, 10, 0, 0 }, bin20 },
 { { 1945, 6, 1, 6, 16, 10, 0, 0 }, bin21 },
 { { 2145, 6, 1, 6, 16, 10, 0, 0 }, bin22 },
};

static void test_encodeFiletime(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(times) / sizeof(times[0]); i++)
    {
        testTimeEncoding(dwEncoding, X509_CHOICE_OF_TIME, &times[i]);
        testTimeEncoding(dwEncoding, PKCS_UTC_TIME, &times[i]);
        testTimeEncoding(dwEncoding, szOID_RSA_signingTime, &times[i]);
    }
}

static const BYTE bin23[] = {
    0x18,0x13,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','.','0','0','0','Z',0};
static const BYTE bin24[] = {
    0x18,0x13,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','.','9','9','9','Z',0};
static const BYTE bin25[] = {
    0x18,0x13,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','+','0','1','0','0',0};
static const BYTE bin26[] = {
    0x18,0x13,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','-','0','1','0','0',0};
static const BYTE bin27[] = {
    0x18,0x13,'1','9','4','5','0','6','0','6','1','6','1','0','0','0','-','0','1','1','5',0};
static const BYTE bin28[] = {
    0x18,0x0a,'2','1','4','5','0','6','0','6','1','6',0};
static const BYTE bin29[] = {
    0x17,0x0a,'4','5','0','6','0','6','1','6','1','0',0};
static const BYTE bin30[] = {
    0x17,0x0b,'4','5','0','6','0','6','1','6','1','0','Z',0};
static const BYTE bin31[] = {
    0x17,0x0d,'4','5','0','6','0','6','1','6','1','0','+','0','1',0};
static const BYTE bin32[] = {
    0x17,0x0d,'4','5','0','6','0','6','1','6','1','0','-','0','1',0};
static const BYTE bin33[] = {
    0x17,0x0f,'4','5','0','6','0','6','1','6','1','0','+','0','1','0','0',0};
static const BYTE bin34[] = {
    0x17,0x0f,'4','5','0','6','0','6','1','6','1','0','-','0','1','0','0',0};
static const BYTE bin35[] = {
    0x17,0x08, '4','5','0','6','0','6','1','6',0};
static const BYTE bin36[] = {
    0x18,0x0f, 'a','a','a','a','a','a','a','a','a','a','a','a','a','a','Z',0};
static const BYTE bin37[] = {
    0x18,0x04, '2','1','4','5',0};
static const BYTE bin38[] = {
    0x18,0x08, '2','1','4','5','0','6','0','6',0};

static void test_decodeFiletime(DWORD dwEncoding)
{
    static const struct encodedFiletime otherTimes[] = {
     { { 1945, 6, 1, 6, 16, 10, 0, 0 },   bin23 },
     { { 1945, 6, 1, 6, 16, 10, 0, 999 }, bin24 },
     { { 1945, 6, 1, 6, 17, 10, 0, 0 },   bin25 },
     { { 1945, 6, 1, 6, 15, 10, 0, 0 },   bin26 },
     { { 1945, 6, 1, 6, 14, 55, 0, 0 },   bin27 },
     { { 2145, 6, 1, 6, 16,  0, 0, 0 },   bin28 },
     { { 2045, 6, 1, 6, 16, 10, 0, 0 },   bin29 },
     { { 2045, 6, 1, 6, 16, 10, 0, 0 },   bin30 },
     { { 2045, 6, 1, 6, 17, 10, 0, 0 },   bin31 },
     { { 2045, 6, 1, 6, 15, 10, 0, 0 },   bin32 },
     { { 2045, 6, 1, 6, 17, 10, 0, 0 },   bin33 },
     { { 2045, 6, 1, 6, 15, 10, 0, 0 },   bin34 },
    };
    /* An oddball case that succeeds in Windows, but doesn't seem correct
     { { 2145, 6, 1, 2, 11, 31, 0, 0 },   "\x18" "\x13" "21450606161000-9999" },
     */
    static const unsigned char *bogusTimes[] = {
     /* oddly, this succeeds on Windows, with year 2765
     "\x18" "\x0f" "21r50606161000Z",
      */
     bin35,
     bin36,
     bin37,
     bin38,
    };
    DWORD i, size;
    FILETIME ft1 = { 0 }, ft2 = { 0 };
    BOOL ret;

    /* Check bogus length with non-NULL buffer */
    ret = SystemTimeToFileTime(&times[0].sysTime, &ft1);
    ok(ret, "SystemTimeToFileTime failed: %ld\n", GetLastError());
    size = 1;
    ret = CryptDecodeObjectEx(dwEncoding, X509_CHOICE_OF_TIME,
     times[0].encodedTime, times[0].encodedTime[1] + 2, 0, NULL, &ft2, &size);
    ok(!ret && GetLastError() == ERROR_MORE_DATA,
     "Expected ERROR_MORE_DATA, got %ld\n", GetLastError());
    /* Normal tests */
    for (i = 0; i < sizeof(times) / sizeof(times[0]); i++)
    {
        testTimeDecoding(dwEncoding, X509_CHOICE_OF_TIME, &times[i]);
        testTimeDecoding(dwEncoding, PKCS_UTC_TIME, &times[i]);
        testTimeDecoding(dwEncoding, szOID_RSA_signingTime, &times[i]);
    }
    for (i = 0; i < sizeof(otherTimes) / sizeof(otherTimes[0]); i++)
    {
        testTimeDecoding(dwEncoding, X509_CHOICE_OF_TIME, &otherTimes[i]);
        testTimeDecoding(dwEncoding, PKCS_UTC_TIME, &otherTimes[i]);
        testTimeDecoding(dwEncoding, szOID_RSA_signingTime, &otherTimes[i]);
    }
    for (i = 0; i < sizeof(bogusTimes) / sizeof(bogusTimes[0]); i++)
    {
        size = sizeof(ft1);
        ret = CryptDecodeObjectEx(dwEncoding, X509_CHOICE_OF_TIME,
         bogusTimes[i], bogusTimes[i][1] + 2, 0, NULL, &ft1, &size);
        ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
         "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    }
}

struct EncodedName
{
    CERT_RDN_ATTR attr;
    const BYTE *encoded;
};

static const char commonName[] = "Juan Lang";
static const char surName[] = "Lang";
static const char bogusIA5[] = "\x80";
static const char bogusPrintable[] = "~";
static const char bogusNumeric[] = "A";
static const unsigned char bin39[] = {
    0x30,0x15,0x31,0x13,0x30,0x11,0x06,0x03,0x55,0x04,0x03,0x13,0x0a,'J','u','a','n',' ','L','a','n','g',0};
static const unsigned char bin40[] = {
    0x30,0x15,0x31,0x13,0x30,0x11,0x06,0x03,0x55,0x04,0x03,0x16,0x0a,'J','u','a','n',' ','L','a','n','g',0};
static const unsigned char bin41[] = {
    0x30,0x10,0x31,0x0e,0x30,0x0c,0x06,0x03,0x55,0x04,0x04,0x16,0x05,'L','a','n','g',0};
static const unsigned char bin42[] = {
    0x30,0x12,0x31,0x10,0x30,0x0e,0x06,0x00,0x13,0x0a,'J','u','a','n',' ','L','a','n','g',0};
static const unsigned char bin43[] = {
    0x30,0x0d,0x31,0x0b,0x30,0x09,0x06,0x03,0x55,0x04,0x03,0x16,0x02,0x80,0};
static const unsigned char bin44[] = {
    0x30,0x0d,0x31,0x0b,0x30,0x09,0x06,0x03,0x55,0x04,0x03,0x13,0x02,0x7e,0};
static const unsigned char bin45[] = {
    0x30,0x0d,0x31,0x0b,0x30,0x09,0x06,0x03,0x55,0x04,0x03,0x12,0x02,0x41,0};
static const struct EncodedName names[] = {
 { { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING,
   { sizeof(commonName), (BYTE *)commonName } }, bin39 },
 { { szOID_COMMON_NAME, CERT_RDN_IA5_STRING,
   { sizeof(commonName), (BYTE *)commonName } }, bin40 },
 { { szOID_SUR_NAME, CERT_RDN_IA5_STRING,
   { sizeof(surName), (BYTE *)surName } }, bin41 },
 { { NULL, CERT_RDN_PRINTABLE_STRING,
   { sizeof(commonName), (BYTE *)commonName } }, bin42 },
/* The following test isn't a very good one, because it doesn't encode any
 * Japanese characters.  I'm leaving it out for now.
 { { szOID_COMMON_NAME, CERT_RDN_T61_STRING,
   { sizeof(commonName), (BYTE *)commonName } },
 "\x30\x15\x31\x13\x30\x11\x06\x03\x55\x04\x03\x14\x0aJuan Lang" },
 */
 /* The following tests succeed under Windows, but really should fail,
  * they contain characters that are illegal for the encoding.  I'm
  * including them to justify my lazy encoding.
  */
 { { szOID_COMMON_NAME, CERT_RDN_IA5_STRING,
   { sizeof(bogusIA5), (BYTE *)bogusIA5 } }, bin43 },
 { { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING,
   { sizeof(bogusPrintable), (BYTE *)bogusPrintable } }, bin44 },
 { { szOID_COMMON_NAME, CERT_RDN_NUMERIC_STRING,
   { sizeof(bogusNumeric), (BYTE *)bogusNumeric } }, bin45 },
};

static const BYTE emptyName[] = { 0x30, 0 };
static const BYTE emptyRDNs[] = { 0x30, 0x02, 0x31, 0 };
static const BYTE twoRDNs[] = {
    0x30,0x23,0x31,0x21,0x30,0x0c,0x06,0x03,0x55,0x04,0x04,
    0x13,0x05,0x4c,0x61,0x6e,0x67,0x00,0x30,0x11,0x06,0x03,0x55,0x04,0x03,
    0x13,0x0a,0x4a,0x75,0x61,0x6e,0x20,0x4c,0x61,0x6e,0x67,0};

static void test_encodeName(DWORD dwEncoding)
{
    CERT_RDN_ATTR attrs[2];
    CERT_RDN rdn;
    CERT_NAME_INFO info;
    BYTE *buf = NULL;
    DWORD size = 0, i;
    BOOL ret;

    /* Test with NULL pvStructInfo */
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, NULL,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* Test with empty CERT_NAME_INFO */
    info.cRDN = 0;
    info.rgRDN = NULL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, emptyName, sizeof(emptyName)),
         "Got unexpected encoding for empty name\n");
        LocalFree(buf);
    }
    /* Test with bogus CERT_RDN */
    info.cRDN = 1;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* Test with empty CERT_RDN */
    rdn.cRDNAttr = 0;
    rdn.rgRDNAttr = NULL;
    info.cRDN = 1;
    info.rgRDN = &rdn;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, emptyRDNs, sizeof(emptyRDNs)),
         "Got unexpected encoding for empty RDN array\n");
        LocalFree(buf);
    }
    /* Test with bogus attr array */
    rdn.cRDNAttr = 1;
    rdn.rgRDNAttr = NULL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* oddly, a bogus OID is accepted by Windows XP; not testing.
    attrs[0].pszObjId = "bogus";
    attrs[0].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[0].Value.cbData = sizeof(commonName);
    attrs[0].Value.pbData = (BYTE *)commonName;
    rdn.cRDNAttr = 1;
    rdn.rgRDNAttr = attrs;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret, "Expected failure, got success\n");
     */
    /* Check with two CERT_RDN_ATTRs.  Note DER encoding forces the order of
     * the encoded attributes to be swapped.
     */
    attrs[0].pszObjId = szOID_COMMON_NAME;
    attrs[0].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[0].Value.cbData = sizeof(commonName);
    attrs[0].Value.pbData = (BYTE *)commonName;
    attrs[1].pszObjId = szOID_SUR_NAME;
    attrs[1].dwValueType = CERT_RDN_PRINTABLE_STRING;
    attrs[1].Value.cbData = sizeof(surName);
    attrs[1].Value.pbData = (BYTE *)surName;
    rdn.cRDNAttr = 2;
    rdn.rgRDNAttr = attrs;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(!memcmp(buf, twoRDNs, sizeof(twoRDNs)),
         "Got unexpected encoding for two RDN array\n");
        LocalFree(buf);
    }
    /* CERT_RDN_ANY_TYPE is too vague for X509_NAMEs, check the return */
    rdn.cRDNAttr = 1;
    attrs[0].dwValueType = CERT_RDN_ANY_TYPE;
    ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        rdn.cRDNAttr = 1;
        rdn.rgRDNAttr = (CERT_RDN_ATTR *)&names[i].attr;
        ret = CryptEncodeObjectEx(dwEncoding, X509_NAME, &info,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(size == names[i].encoded[1] + 2, "Expected size %d, got %ld\n",
             names[i].encoded[1] + 2, size);
            ok(!memcmp(buf, names[i].encoded, names[i].encoded[1] + 2),
             "Got unexpected encoding\n");
            LocalFree(buf);
        }
    }
}

static void compareNames(const CERT_NAME_INFO *expected,
 const CERT_NAME_INFO *got)
{
    ok(got->cRDN == expected->cRDN, "Expected %ld RDNs, got %ld\n",
     expected->cRDN, got->cRDN);
    if (expected->cRDN)
    {
        ok(got->rgRDN[0].cRDNAttr == expected->rgRDN[0].cRDNAttr,
         "Expected %ld RDN attrs, got %ld\n", expected->rgRDN[0].cRDNAttr,
         got->rgRDN[0].cRDNAttr);
        if (expected->rgRDN[0].cRDNAttr)
        {
            if (expected->rgRDN[0].rgRDNAttr[0].pszObjId &&
             strlen(expected->rgRDN[0].rgRDNAttr[0].pszObjId))
            {
                ok(got->rgRDN[0].rgRDNAttr[0].pszObjId != NULL,
                 "Expected OID %s, got NULL\n",
                 expected->rgRDN[0].rgRDNAttr[0].pszObjId);
                if (got->rgRDN[0].rgRDNAttr[0].pszObjId)
                    ok(!strcmp(got->rgRDN[0].rgRDNAttr[0].pszObjId,
                     expected->rgRDN[0].rgRDNAttr[0].pszObjId),
                     "Got unexpected OID %s, expected %s\n",
                     got->rgRDN[0].rgRDNAttr[0].pszObjId,
                     expected->rgRDN[0].rgRDNAttr[0].pszObjId);
            }
            ok(got->rgRDN[0].rgRDNAttr[0].Value.cbData ==
             expected->rgRDN[0].rgRDNAttr[0].Value.cbData,
             "Unexpected data size, got %ld, expected %ld\n",
             got->rgRDN[0].rgRDNAttr[0].Value.cbData,
             expected->rgRDN[0].rgRDNAttr[0].Value.cbData);
            if (expected->rgRDN[0].rgRDNAttr[0].Value.pbData)
                ok(!memcmp(got->rgRDN[0].rgRDNAttr[0].Value.pbData,
                 expected->rgRDN[0].rgRDNAttr[0].Value.pbData,
                 expected->rgRDN[0].rgRDNAttr[0].Value.cbData),
                 "Unexpected value\n");
        }
    }
}

static void test_decodeName(DWORD dwEncoding)
{
    int i;
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    BOOL ret;
    CERT_RDN rdn;
    CERT_NAME_INFO info = { 1, &rdn };

    for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
    {
        /* When the output buffer is NULL, this always succeeds */
        SetLastError(0xdeadbeef);
        ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, names[i].encoded,
         names[i].encoded[1] + 2, 0, NULL, NULL, &bufSize);
        ok(ret && GetLastError() == NOERROR,
         "Expected success and NOERROR, got %08lx\n", GetLastError());
        ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, names[i].encoded,
         names[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
         (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        rdn.cRDNAttr = 1;
        rdn.rgRDNAttr = (CERT_RDN_ATTR *)&names[i].attr;
        if (buf)
        {
            compareNames((CERT_NAME_INFO *)buf, &info);
            LocalFree(buf);
        }
    }
    /* test empty name */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, emptyName,
     emptyName[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    /* Interestingly, in Windows, if cRDN is 0, rgRGN may not be NULL.  My
     * decoder works the same way, so only test the count.
     */
    if (buf)
    {
        ok(bufSize == sizeof(CERT_NAME_INFO),
         "Expected bufSize %d, got %ld\n", sizeof(CERT_NAME_INFO), bufSize);
        ok(((CERT_NAME_INFO *)buf)->cRDN == 0,
         "Expected 0 RDNs in empty info, got %ld\n",
         ((CERT_NAME_INFO *)buf)->cRDN);
        LocalFree(buf);
    }
    /* test empty RDN */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, emptyRDNs,
     emptyRDNs[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_NAME_INFO *info = (CERT_NAME_INFO *)buf;

        ok(bufSize == sizeof(CERT_NAME_INFO) + sizeof(CERT_RDN) &&
         info->cRDN == 1 && info->rgRDN && info->rgRDN[0].cRDNAttr == 0,
         "Got unexpected value for empty RDN\n");
        LocalFree(buf);
    }
    /* test two RDN attrs */
    bufSize = 0;
    ret = CryptDecodeObjectEx(dwEncoding, X509_NAME, twoRDNs,
     twoRDNs[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG | CRYPT_DECODE_SHARE_OID_STRING_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_RDN_ATTR attrs[] = {
         { szOID_SUR_NAME, CERT_RDN_PRINTABLE_STRING, { sizeof(surName),
          (BYTE *)surName } },
         { szOID_COMMON_NAME, CERT_RDN_PRINTABLE_STRING, { sizeof(commonName),
          (BYTE *)commonName } },
        };

        rdn.cRDNAttr = sizeof(attrs) / sizeof(attrs[0]);
        rdn.rgRDNAttr = attrs;
        compareNames((CERT_NAME_INFO *)buf, &info);
        LocalFree(buf);
    }
}

static const BYTE emptyAltName[] = { 0x30, 0x00 };
static const BYTE emptyURL[] = { 0x30, 0x02, 0x86, 0x00 };
static const WCHAR url[] = { 'h','t','t','p',':','/','/','w','i','n','e',
 'h','q','.','o','r','g',0 };
static const BYTE encodedURL[] = { 0x30, 0x13, 0x86, 0x11, 0x68, 0x74,
 0x74, 0x70, 0x3a, 0x2f, 0x2f, 0x77, 0x69, 0x6e, 0x65, 0x68, 0x71, 0x2e,
 0x6f, 0x72, 0x67 };
static const WCHAR nihongoURL[] = { 'h','t','t','p',':','/','/',0x226f,
 0x575b, 0 };
static const WCHAR dnsName[] = { 'w','i','n','e','h','q','.','o','r','g',0 };
static const BYTE encodedDnsName[] = { 0x30, 0x0c, 0x82, 0x0a, 0x77, 0x69,
 0x6e, 0x65, 0x68, 0x71, 0x2e, 0x6f, 0x72, 0x67 };
static const BYTE localhost[] = { 127, 0, 0, 1 };
static const BYTE encodedIPAddr[] = { 0x30, 0x06, 0x87, 0x04, 0x7f, 0x00, 0x00,
 0x01 };

static void test_encodeAltName(DWORD dwEncoding)
{
    CERT_ALT_NAME_INFO info = { 0 };
    CERT_ALT_NAME_ENTRY entry = { 0 };
    BYTE *buf = NULL;
    DWORD size = 0;
    BOOL ret;

    /* Test with empty info */
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(emptyAltName), "Expected size %d, got %ld\n",
         sizeof(emptyAltName), size);
        ok(!memcmp(buf, emptyAltName, size), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Test with an empty entry */
    info.cAltEntry = 1;
    info.rgAltEntry = &entry;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
     "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %08lx\n",
     GetLastError());
    /* Test with an empty pointer */
    entry.dwAltNameChoice = CERT_ALT_NAME_URL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(emptyURL), "Expected size %d, got %ld\n",
         sizeof(emptyURL), size);
        ok(!memcmp(buf, emptyURL, size), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Test with a real URL */
    U(entry).pwszURL = (LPWSTR)url;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(encodedURL), "Expected size %d, got %ld\n",
         sizeof(encodedURL), size);
        ok(!memcmp(buf, encodedURL, size), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Now with the URL containing an invalid IA5 char */
    U(entry).pwszURL = (LPWSTR)nihongoURL;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == CRYPT_E_INVALID_IA5_STRING,
     "Expected CRYPT_E_INVALID_IA5_STRING, got %08lx\n", GetLastError());
    /* The first invalid character is at index 7 */
    ok(GET_CERT_ALT_NAME_VALUE_ERR_INDEX(size) == 7,
     "Expected invalid char at index 7, got %ld\n",
     GET_CERT_ALT_NAME_VALUE_ERR_INDEX(size));
    /* Now with the URL missing a scheme */
    U(entry).pwszURL = (LPWSTR)dnsName;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        /* This succeeds, but it shouldn't, so don't worry about conforming */
        LocalFree(buf);
    }
    /* Now with a DNS name */
    entry.dwAltNameChoice = CERT_ALT_NAME_DNS_NAME;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(encodedDnsName), "Expected size %d, got %ld\n",
         sizeof(encodedDnsName), size);
        ok(!memcmp(buf, encodedDnsName, size), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Test with an IP address */
    entry.dwAltNameChoice = CERT_ALT_NAME_IP_ADDRESS;
    U(entry).IPAddress.cbData = sizeof(localhost);
    U(entry).IPAddress.pbData = (LPBYTE)localhost;
    ret = CryptEncodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(encodedIPAddr), "Expected size %d, got %ld\n",
         sizeof(encodedIPAddr), size);
        ok(!memcmp(buf, encodedIPAddr, size), "Unexpected value\n");
        LocalFree(buf);
    }
}

static void test_decodeAltName(DWORD dwEncoding)
{
    static const BYTE unimplementedType[] = { 0x30, 0x06, 0x85, 0x04, 0x7f,
     0x00, 0x00, 0x01 };
    static const BYTE bogusType[] = { 0x30, 0x06, 0x89, 0x04, 0x7f, 0x00, 0x00,
     0x01 };
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;
    CERT_ALT_NAME_INFO *info;

    /* Test some bogus ones first */
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME,
     unimplementedType, sizeof(unimplementedType), CRYPT_DECODE_ALLOC_FLAG,
     NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_BADTAG,
     "Expected CRYPT_E_ASN1_BADTAG, got %08lx\n", GetLastError());
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME,
     bogusType, sizeof(bogusType), CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
     "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    /* Now expected cases */
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, emptyAltName,
     emptyAltName[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        info = (CERT_ALT_NAME_INFO *)buf;

        ok(info->cAltEntry == 0, "Expected 0 entries, got %ld\n",
         info->cAltEntry);
        LocalFree(buf);
    }
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, emptyURL,
     emptyURL[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        info = (CERT_ALT_NAME_INFO *)buf;

        ok(info->cAltEntry == 1, "Expected 1 entries, got %ld\n",
         info->cAltEntry);
        ok(info->rgAltEntry[0].dwAltNameChoice == CERT_ALT_NAME_URL,
         "Expected CERT_ALT_NAME_URL, got %ld\n",
         info->rgAltEntry[0].dwAltNameChoice);
        ok(U(info->rgAltEntry[0]).pwszURL == NULL || !*U(info->rgAltEntry[0]).pwszURL,
         "Expected empty URL\n");
        LocalFree(buf);
    }
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, encodedURL,
     encodedURL[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        info = (CERT_ALT_NAME_INFO *)buf;

        ok(info->cAltEntry == 1, "Expected 1 entries, got %ld\n",
         info->cAltEntry);
        ok(info->rgAltEntry[0].dwAltNameChoice == CERT_ALT_NAME_URL,
         "Expected CERT_ALT_NAME_URL, got %ld\n",
         info->rgAltEntry[0].dwAltNameChoice);
        ok(!lstrcmpW(U(info->rgAltEntry[0]).pwszURL, url), "Unexpected URL\n");
        LocalFree(buf);
    }
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, encodedDnsName,
     encodedDnsName[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        info = (CERT_ALT_NAME_INFO *)buf;

        ok(info->cAltEntry == 1, "Expected 1 entries, got %ld\n",
         info->cAltEntry);
        ok(info->rgAltEntry[0].dwAltNameChoice == CERT_ALT_NAME_DNS_NAME,
         "Expected CERT_ALT_NAME_DNS_NAME, got %ld\n",
         info->rgAltEntry[0].dwAltNameChoice);
        ok(!lstrcmpW(U(info->rgAltEntry[0]).pwszDNSName, dnsName),
         "Unexpected DNS name\n");
        LocalFree(buf);
    }
    ret = CryptDecodeObjectEx(dwEncoding, X509_ALTERNATE_NAME, encodedIPAddr,
     encodedIPAddr[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        info = (CERT_ALT_NAME_INFO *)buf;

        ok(info->cAltEntry == 1, "Expected 1 entries, got %ld\n",
         info->cAltEntry);
        ok(info->rgAltEntry[0].dwAltNameChoice == CERT_ALT_NAME_IP_ADDRESS,
         "Expected CERT_ALT_NAME_IP_ADDRESS, got %ld\n",
         info->rgAltEntry[0].dwAltNameChoice);
        ok(U(info->rgAltEntry[0]).IPAddress.cbData == sizeof(localhost),
         "Unexpected IP address length %ld\n",
          U(info->rgAltEntry[0]).IPAddress.cbData);
        ok(!memcmp(U(info->rgAltEntry[0]).IPAddress.pbData, localhost,
         sizeof(localhost)), "Unexpected IP address value\n");
        LocalFree(buf);
    }
}

struct encodedOctets
{
    const BYTE *val;
    const BYTE *encoded;
};

static const unsigned char bin46[] = { 'h','i',0 };
static const unsigned char bin47[] = { 0x04,0x02,'h','i',0 };
static const unsigned char bin48[] = {
     's','o','m','e','l','o','n','g',0xff,'s','t','r','i','n','g',0 };
static const unsigned char bin49[] = {
     0x04,0x0f,'s','o','m','e','l','o','n','g',0xff,'s','t','r','i','n','g',0 };
static const unsigned char bin50[] = { 0 };
static const unsigned char bin51[] = { 0x04,0x00,0 };

static const struct encodedOctets octets[] = {
    { bin46, bin47 },
    { bin48, bin49 },
    { bin50, bin51 },
};

static void test_encodeOctets(DWORD dwEncoding)
{
    CRYPT_DATA_BLOB blob;
    DWORD i;

    for (i = 0; i < sizeof(octets) / sizeof(octets[0]); i++)
    {
        BYTE *buf = NULL;
        BOOL ret;
        DWORD bufSize = 0;

        blob.cbData = strlen((const char*)octets[i].val);
        blob.pbData = (BYTE*)octets[i].val;
        ret = CryptEncodeObjectEx(dwEncoding, X509_OCTET_STRING, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %ld\n", GetLastError());
        if (buf)
        {
            ok(buf[0] == 4,
             "Got unexpected type %d for octet string (expected 4)\n", buf[0]);
            ok(buf[1] == octets[i].encoded[1], "Got length %d, expected %d\n",
             buf[1], octets[i].encoded[1]);
            ok(!memcmp(buf + 1, octets[i].encoded + 1,
             octets[i].encoded[1] + 1), "Got unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeOctets(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(octets) / sizeof(octets[0]); i++)
    {
        BYTE *buf = NULL;
        BOOL ret;
        DWORD bufSize = 0;

        ret = CryptDecodeObjectEx(dwEncoding, X509_OCTET_STRING,
         (BYTE *)octets[i].encoded, octets[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        ok(bufSize >= sizeof(CRYPT_DATA_BLOB) + octets[i].encoded[1],
         "Expected size >= %d, got %ld\n",
         sizeof(CRYPT_DATA_BLOB) + octets[i].encoded[1], bufSize);
        ok(buf != NULL, "Expected allocated buffer\n");
        if (buf)
        {
            CRYPT_DATA_BLOB *blob = (CRYPT_DATA_BLOB *)buf;

            if (blob->cbData)
                ok(!memcmp(blob->pbData, octets[i].val, blob->cbData),
                 "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static const BYTE bytesToEncode[] = { 0xff, 0xff };

struct encodedBits
{
    DWORD cUnusedBits;
    const BYTE *encoded;
    DWORD cbDecoded;
    const BYTE *decoded;
};

static const unsigned char bin52[] = { 0x03,0x03,0x00,0xff,0xff,0 };
static const unsigned char bin53[] = { 0xff,0xff,0 };
static const unsigned char bin54[] = { 0x03,0x03,0x01,0xff,0xfe,0 };
static const unsigned char bin55[] = { 0xff,0xfe,0 };
static const unsigned char bin56[] = { 0x03,0x02,0x01,0xfe,0 };
static const unsigned char bin57[] = { 0xfe,0 };
static const unsigned char bin58[] = { 0x03,0x01,0x00,0 };

static const struct encodedBits bits[] = {
    /* normal test cases */
    { 0, bin52, 2, bin53 },
    { 1, bin54, 2, bin55 },
    /* strange test case, showing cUnusedBits >= 8 is allowed */
    { 9, bin56, 1, bin57 },
    /* even stranger test case, showing cUnusedBits > cbData * 8 is allowed */
    { 17, bin58, 0, NULL },
};

static void test_encodeBits(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
        CRYPT_BIT_BLOB blob;
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        blob.cbData = sizeof(bytesToEncode);
        blob.pbData = (BYTE *)bytesToEncode;
        blob.cUnusedBits = bits[i].cUnusedBits;
        ret = CryptEncodeObjectEx(dwEncoding, X509_BITS, &blob,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == bits[i].encoded[1] + 2,
             "Got unexpected size %ld, expected %d\n", bufSize,
             bits[i].encoded[1] + 2);
            ok(!memcmp(buf, bits[i].encoded, bits[i].encoded[1] + 2),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeBits(DWORD dwEncoding)
{
    static const BYTE ber[] = "\x03\x02\x01\xff";
    static const BYTE berDecoded = 0xfe;
    DWORD i;
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    /* normal cases */
    for (i = 0; i < sizeof(bits) / sizeof(bits[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_BITS, bits[i].encoded,
         bits[i].encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
         &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            CRYPT_BIT_BLOB *blob;

            ok(bufSize >= sizeof(CRYPT_BIT_BLOB) + bits[i].cbDecoded,
             "Got unexpected size %ld, expected >= %ld\n", bufSize,
             sizeof(CRYPT_BIT_BLOB) + bits[i].cbDecoded);
            blob = (CRYPT_BIT_BLOB *)buf;
            ok(blob->cbData == bits[i].cbDecoded,
             "Got unexpected length %ld, expected %ld\n", blob->cbData,
             bits[i].cbDecoded);
            if (blob->cbData && bits[i].cbDecoded)
                ok(!memcmp(blob->pbData, bits[i].decoded, bits[i].cbDecoded),
                 "Unexpected value\n");
            LocalFree(buf);
        }
    }
    /* special case: check that something that's valid in BER but not in DER
     * decodes successfully
     */
    ret = CryptDecodeObjectEx(dwEncoding, X509_BITS, ber, ber[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRYPT_BIT_BLOB *blob;

        ok(bufSize >= sizeof(CRYPT_BIT_BLOB) + sizeof(berDecoded),
         "Got unexpected size %ld, expected >= %d\n", bufSize,
         sizeof(CRYPT_BIT_BLOB) + berDecoded);
        blob = (CRYPT_BIT_BLOB *)buf;
        ok(blob->cbData == sizeof(berDecoded),
         "Got unexpected length %ld, expected %d\n", blob->cbData,
         sizeof(berDecoded));
        if (blob->cbData)
            ok(*blob->pbData == berDecoded, "Unexpected value\n");
        LocalFree(buf);
    }
}

struct Constraints2
{
    CERT_BASIC_CONSTRAINTS2_INFO info;
    const BYTE *encoded;
};

static const unsigned char bin59[] = { 0x30,0x00,0 };
static const unsigned char bin60[] = { 0x30,0x03,0x01,0x01,0xff,0 };
static const unsigned char bin61[] = { 0x30,0x03,0x02,0x01,0x00,0 };
static const unsigned char bin62[] = { 0x30,0x06,0x01,0x01,0xff,0x02,0x01,0x01,0 };
static const struct Constraints2 constraints2[] = {
 /* empty constraints */
 { { FALSE, FALSE, 0}, bin59 },
 /* can be a CA */
 { { TRUE,  FALSE, 0}, bin60 },
 /* has path length constraints set (MSDN implies fCA needs to be TRUE as well,
  * but that's not the case
  */
 { { FALSE, TRUE,  0}, bin61 },
 /* can be a CA and has path length constraints set */
 { { TRUE,  TRUE,  1}, bin62 },
};

static void test_encodeBasicConstraints(DWORD dwEncoding)
{
    DWORD i;

    /* First test with the simpler info2 */
    for (i = 0; i < sizeof(constraints2) / sizeof(constraints2[0]); i++)
    {
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        ret = CryptEncodeObjectEx(dwEncoding, X509_BASIC_CONSTRAINTS2,
         &constraints2[i].info, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
         &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == constraints2[i].encoded[1] + 2,
             "Expected %d bytes, got %ld\n", constraints2[i].encoded[1] + 2,
             bufSize);
            ok(!memcmp(buf, constraints2[i].encoded,
             constraints2[i].encoded[1] + 2), "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static const unsigned char bin63[] = { 0x30,0x06,0x01,0x01,0x01,0x02,0x01,0x01,0 };

static void test_decodeBasicConstraints(DWORD dwEncoding)
{
    static const BYTE inverted[] = "\x30\x06\x02\x01\x01\x01\x01\xff";
    static const struct Constraints2 badBool = { { TRUE, TRUE, 1 }, bin63 };
    DWORD i;
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    /* First test with simpler info2 */
    for (i = 0; i < sizeof(constraints2) / sizeof(constraints2[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_BASIC_CONSTRAINTS2,
         constraints2[i].encoded, constraints2[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            CERT_BASIC_CONSTRAINTS2_INFO *info =
             (CERT_BASIC_CONSTRAINTS2_INFO *)buf;

            ok(!memcmp(info, &constraints2[i].info, sizeof(*info)),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
    /* Check with the order of encoded elements inverted */
    buf = (PBYTE)1;
    ret = CryptDecodeObjectEx(dwEncoding, X509_BASIC_CONSTRAINTS2,
     inverted, inverted[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
     "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    ok(!buf, "Expected buf to be set to NULL\n");
    /* Check with a non-DER bool */
    ret = CryptDecodeObjectEx(dwEncoding, X509_BASIC_CONSTRAINTS2,
     badBool.encoded, badBool.encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_BASIC_CONSTRAINTS2_INFO *info =
         (CERT_BASIC_CONSTRAINTS2_INFO *)buf;

        ok(!memcmp(info, &badBool.info, sizeof(*info)), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Check with a non-basic constraints value */
    ret = CryptDecodeObjectEx(dwEncoding, X509_BASIC_CONSTRAINTS2,
     names[0].encoded, names[0].encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL,
     (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
     "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
}

/* These are terrible public keys of course, I'm just testing encoding */
static const BYTE modulus1[] = { 0,0,0,1,1,1,1,1 };
static const BYTE modulus2[] = { 1,1,1,1,1,0,0,0 };
static const BYTE modulus3[] = { 0x80,1,1,1,1,0,0,0 };
static const BYTE modulus4[] = { 1,1,1,1,1,0,0,0x80 };
static const BYTE mod1_encoded[] = { 0x30,0x0f,0x02,0x08,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x02,0x03,0x01,0x00,0x01 };
static const BYTE mod2_encoded[] = { 0x30,0x0c,0x02,0x05,0x01,0x01,0x01,0x01,0x01,0x02,0x03,0x01,0x00,0x01 };
static const BYTE mod3_encoded[] = { 0x30,0x0c,0x02,0x05,0x01,0x01,0x01,0x01,0x80,0x02,0x03,0x01,0x00,0x01 };
static const BYTE mod4_encoded[] = { 0x30,0x10,0x02,0x09,0x00,0x80,0x00,0x00,0x01,0x01,0x01,0x01,0x01,0x02,0x03,0x01,0x00,0x01 };

struct EncodedRSAPubKey
{
    const BYTE *modulus;
    size_t modulusLen;
    const BYTE *encoded;
    size_t decodedModulusLen;
};

struct EncodedRSAPubKey rsaPubKeys[] = {
    { modulus1, sizeof(modulus1), mod1_encoded, sizeof(modulus1) },
    { modulus2, sizeof(modulus2), mod2_encoded, 5 },
    { modulus3, sizeof(modulus3), mod3_encoded, 5 },
    { modulus4, sizeof(modulus4), mod4_encoded, 8 },
};

static void test_encodeRsaPublicKey(DWORD dwEncoding)
{
    BYTE toEncode[sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) + sizeof(modulus1)];
    BLOBHEADER *hdr = (BLOBHEADER *)toEncode;
    RSAPUBKEY *rsaPubKey = (RSAPUBKEY *)(toEncode + sizeof(BLOBHEADER));
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0, i;

    /* Try with a bogus blob type */
    hdr->bType = 2;
    hdr->bVersion = CUR_BLOB_VERSION;
    hdr->reserved = 0;
    hdr->aiKeyAlg = CALG_RSA_KEYX;
    rsaPubKey->magic = 0x31415352;
    rsaPubKey->bitlen = sizeof(modulus1) * 8;
    rsaPubKey->pubexp = 65537;
    memcpy(toEncode + sizeof(BLOBHEADER) + sizeof(RSAPUBKEY), modulus1,
     sizeof(modulus1));

    ret = CryptEncodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(!ret && GetLastError() == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER),
     "Expected HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER), got %08lx\n",
     GetLastError());
    /* Now with a bogus reserved field */
    hdr->bType = PUBLICKEYBLOB;
    hdr->reserved = 1;
    ret = CryptEncodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    if (buf)
    {
        ok(bufSize == rsaPubKeys[0].encoded[1] + 2,
         "Expected size %d, got %ld\n", rsaPubKeys[0].encoded[1] + 2, bufSize);
        ok(!memcmp(buf, rsaPubKeys[0].encoded, bufSize), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Now with a bogus blob version */
    hdr->reserved = 0;
    hdr->bVersion = 0;
    ret = CryptEncodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    if (buf)
    {
        ok(bufSize == rsaPubKeys[0].encoded[1] + 2,
         "Expected size %d, got %ld\n", rsaPubKeys[0].encoded[1] + 2, bufSize);
        ok(!memcmp(buf, rsaPubKeys[0].encoded, bufSize), "Unexpected value\n");
        LocalFree(buf);
    }
    /* And with a bogus alg ID */
    hdr->bVersion = CUR_BLOB_VERSION;
    hdr->aiKeyAlg = CALG_DES;
    ret = CryptEncodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    if (buf)
    {
        ok(bufSize == rsaPubKeys[0].encoded[1] + 2,
         "Expected size %d, got %ld\n", rsaPubKeys[0].encoded[1] + 2, bufSize);
        ok(!memcmp(buf, rsaPubKeys[0].encoded, bufSize), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Check a couple of RSA-related OIDs */
    hdr->aiKeyAlg = CALG_RSA_KEYX;
    ret = CryptEncodeObjectEx(dwEncoding, szOID_RSA_RSA,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    ret = CryptEncodeObjectEx(dwEncoding, szOID_RSA_SHA1RSA,
     toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    /* Finally, all valid */
    hdr->aiKeyAlg = CALG_RSA_KEYX;
    for (i = 0; i < sizeof(rsaPubKeys) / sizeof(rsaPubKeys[0]); i++)
    {
        memcpy(toEncode + sizeof(BLOBHEADER) + sizeof(RSAPUBKEY),
         rsaPubKeys[i].modulus, rsaPubKeys[i].modulusLen);
        ret = CryptEncodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
         toEncode, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == rsaPubKeys[i].encoded[1] + 2,
             "Expected size %d, got %ld\n", rsaPubKeys[i].encoded[1] + 2,
             bufSize);
            ok(!memcmp(buf, rsaPubKeys[i].encoded, bufSize),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeRsaPublicKey(DWORD dwEncoding)
{
    DWORD i;
    LPBYTE buf = NULL;
    DWORD bufSize = 0;
    BOOL ret;

    /* Try with a bad length */
    ret = CryptDecodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
     rsaPubKeys[0].encoded, rsaPubKeys[0].encoded[1],
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_EOD,
     "Expected CRYPT_E_ASN1_EOD, got %08lx\n", CRYPT_E_ASN1_EOD);
    /* Try with a couple of RSA-related OIDs */
    ret = CryptDecodeObjectEx(dwEncoding, szOID_RSA_RSA,
     rsaPubKeys[0].encoded, rsaPubKeys[0].encoded[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    ret = CryptDecodeObjectEx(dwEncoding, szOID_RSA_SHA1RSA,
     rsaPubKeys[0].encoded, rsaPubKeys[0].encoded[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    /* Now try success cases */
    for (i = 0; i < sizeof(rsaPubKeys) / sizeof(rsaPubKeys[0]); i++)
    {
        bufSize = 0;
        ret = CryptDecodeObjectEx(dwEncoding, RSA_CSP_PUBLICKEYBLOB,
         rsaPubKeys[i].encoded, rsaPubKeys[i].encoded[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            BLOBHEADER *hdr = (BLOBHEADER *)buf;
            RSAPUBKEY *rsaPubKey = (RSAPUBKEY *)(buf + sizeof(BLOBHEADER));

            ok(bufSize >= sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) +
             rsaPubKeys[i].decodedModulusLen,
             "Expected size at least %d, got %ld\n",
             sizeof(BLOBHEADER) + sizeof(RSAPUBKEY) +
             rsaPubKeys[i].decodedModulusLen, bufSize);
            ok(hdr->bType == PUBLICKEYBLOB,
             "Expected type PUBLICKEYBLOB (%d), got %d\n", PUBLICKEYBLOB,
             hdr->bType);
            ok(hdr->bVersion == CUR_BLOB_VERSION,
             "Expected version CUR_BLOB_VERSION (%d), got %d\n",
             CUR_BLOB_VERSION, hdr->bVersion);
            ok(hdr->reserved == 0, "Expected reserved 0, got %d\n",
             hdr->reserved);
            ok(hdr->aiKeyAlg == CALG_RSA_KEYX,
             "Expected CALG_RSA_KEYX, got %08x\n", hdr->aiKeyAlg);
            ok(rsaPubKey->magic == 0x31415352,
             "Expected magic RSA1, got %08lx\n", rsaPubKey->magic);
            ok(rsaPubKey->bitlen == rsaPubKeys[i].decodedModulusLen * 8,
             "Expected bit len %d, got %ld\n",
             rsaPubKeys[i].decodedModulusLen * 8, rsaPubKey->bitlen);
            ok(rsaPubKey->pubexp == 65537, "Expected pubexp 65537, got %ld\n",
             rsaPubKey->pubexp);
            ok(!memcmp(buf + sizeof(BLOBHEADER) + sizeof(RSAPUBKEY),
             rsaPubKeys[i].modulus, rsaPubKeys[i].decodedModulusLen),
             "Unexpected modulus\n");
            LocalFree(buf);
        }
    }
}

static const BYTE intSequence[] = { 0x30, 0x1b, 0x02, 0x01, 0x01, 0x02, 0x01,
 0x7f, 0x02, 0x02, 0x00, 0x80, 0x02, 0x02, 0x01, 0x00, 0x02, 0x01, 0x80, 0x02,
 0x02, 0xff, 0x7f, 0x02, 0x04, 0xba, 0xdd, 0xf0, 0x0d };

static const BYTE mixedSequence[] = { 0x30, 0x27, 0x17, 0x0d, 0x30, 0x35, 0x30,
 0x36, 0x30, 0x36, 0x31, 0x36, 0x31, 0x30, 0x30, 0x30, 0x5a, 0x02, 0x01, 0x7f,
 0x02, 0x02, 0x00, 0x80, 0x02, 0x02, 0x01, 0x00, 0x02, 0x01, 0x80, 0x02, 0x02,
 0xff, 0x7f, 0x02, 0x04, 0xba, 0xdd, 0xf0, 0x0d };

static void test_encodeSequenceOfAny(DWORD dwEncoding)
{
    CRYPT_DER_BLOB blobs[sizeof(ints) / sizeof(ints[0])];
    CRYPT_SEQUENCE_OF_ANY seq;
    DWORD i;
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    /* Encode a homogenous sequence */
    for (i = 0; i < sizeof(ints) / sizeof(ints[0]); i++)
    {
        blobs[i].cbData = ints[i].encoded[1] + 2;
        blobs[i].pbData = (BYTE *)ints[i].encoded;
    }
    seq.cValue = sizeof(ints) / sizeof(ints[0]);
    seq.rgValue = blobs;

    ret = CryptEncodeObjectEx(dwEncoding, X509_SEQUENCE_OF_ANY, &seq,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(bufSize == sizeof(intSequence), "Expected %d bytes, got %ld\n",
         sizeof(intSequence), bufSize);
        ok(!memcmp(buf, intSequence, intSequence[1] + 2), "Unexpected value\n");
        LocalFree(buf);
    }
    /* Change the type of the first element in the sequence, and give it
     * another go
     */
    blobs[0].cbData = times[0].encodedTime[1] + 2;
    blobs[0].pbData = (BYTE *)times[0].encodedTime;
    ret = CryptEncodeObjectEx(dwEncoding, X509_SEQUENCE_OF_ANY, &seq,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(bufSize == sizeof(mixedSequence), "Expected %d bytes, got %ld\n",
         sizeof(mixedSequence), bufSize);
        ok(!memcmp(buf, mixedSequence, mixedSequence[1] + 2),
         "Unexpected value\n");
        LocalFree(buf);
    }
}

static void test_decodeSequenceOfAny(DWORD dwEncoding)
{
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    ret = CryptDecodeObjectEx(dwEncoding, X509_SEQUENCE_OF_ANY, intSequence,
     intSequence[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRYPT_SEQUENCE_OF_ANY *seq = (CRYPT_SEQUENCE_OF_ANY *)buf;
        DWORD i;

        ok(seq->cValue == sizeof(ints) / sizeof(ints[0]),
         "Expected %d elements, got %ld\n", sizeof(ints) / sizeof(ints[0]),
         seq->cValue);
        for (i = 0; i < min(seq->cValue, sizeof(ints) / sizeof(ints[0])); i++)
        {
            ok(seq->rgValue[i].cbData == ints[i].encoded[1] + 2,
             "Expected %d bytes, got %ld\n", ints[i].encoded[1] + 2,
             seq->rgValue[i].cbData);
            ok(!memcmp(seq->rgValue[i].pbData, ints[i].encoded,
             ints[i].encoded[1] + 2), "Unexpected value\n");
        }
        LocalFree(buf);
    }
    ret = CryptDecodeObjectEx(dwEncoding, X509_SEQUENCE_OF_ANY, mixedSequence,
     mixedSequence[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
     &bufSize);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRYPT_SEQUENCE_OF_ANY *seq = (CRYPT_SEQUENCE_OF_ANY *)buf;

        ok(seq->cValue == sizeof(ints) / sizeof(ints[0]),
         "Expected %d elements, got %ld\n", sizeof(ints) / sizeof(ints[0]),
         seq->cValue);
        /* Just check the first element since it's all that changed */
        ok(seq->rgValue[0].cbData == times[0].encodedTime[1] + 2,
         "Expected %d bytes, got %ld\n", times[0].encodedTime[1] + 2,
         seq->rgValue[0].cbData);
        ok(!memcmp(seq->rgValue[0].pbData, times[0].encodedTime,
         times[0].encodedTime[1] + 2), "Unexpected value\n");
        LocalFree(buf);
    }
}

struct encodedExtensions
{
    CERT_EXTENSIONS exts;
    const BYTE *encoded;
};

static BYTE crit_ext_data[] = { 0x30,0x06,0x01,0x01,0xff,0x02,0x01,0x01 };
static BYTE noncrit_ext_data[] = { 0x30,0x06,0x01,0x01,0xff,0x02,0x01,0x01 };

static CERT_EXTENSION criticalExt =
 { szOID_BASIC_CONSTRAINTS2, TRUE, { 8, crit_ext_data } };
static CERT_EXTENSION nonCriticalExt =
 { szOID_BASIC_CONSTRAINTS2, FALSE, { 8, noncrit_ext_data } };

static const BYTE ext0[] = { 0x30,0x00 };
static const BYTE ext1[] = { 0x30,0x14,0x30,0x12,0x06,0x03,0x55,0x1d,0x13,0x01,0x01,
                             0xff,0x04,0x08,0x30,0x06,0x01,0x01,0xff,0x02,0x01,0x01 };
static const BYTE ext2[] = { 0x30,0x11,0x30,0x0f,0x06,0x03,0x55,0x1d,0x13,0x04,
                             0x08,0x30,0x06,0x01,0x01,0xff,0x02,0x01,0x01 };

static const struct encodedExtensions exts[] = {
 { { 0, NULL }, ext0 },
 { { 1, &criticalExt }, ext1 },
 { { 1, &nonCriticalExt }, ext2 },
};

static void test_encodeExtensions(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(exts) / sizeof(exts[i]); i++)
    {
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        ret = CryptEncodeObjectEx(dwEncoding, X509_EXTENSIONS, &exts[i].exts,
         CRYPT_ENCODE_ALLOC_FLAG, NULL, &buf, &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == exts[i].encoded[1] + 2,
             "Expected %d bytes, got %ld\n", exts[i].encoded[1] + 2, bufSize);
            ok(!memcmp(buf, exts[i].encoded, exts[i].encoded[1] + 2),
             "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void test_decodeExtensions(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(exts) / sizeof(exts[i]); i++)
    {
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        ret = CryptDecodeObjectEx(dwEncoding, X509_EXTENSIONS,
         exts[i].encoded, exts[i].encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG,
         NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            CERT_EXTENSIONS *ext = (CERT_EXTENSIONS *)buf;
            DWORD j;

            ok(ext->cExtension == exts[i].exts.cExtension,
             "Expected %ld extensions, see %ld\n", exts[i].exts.cExtension,
             ext->cExtension);
            for (j = 0; j < min(ext->cExtension, exts[i].exts.cExtension); j++)
            {
                ok(!strcmp(ext->rgExtension[j].pszObjId,
                 exts[i].exts.rgExtension[j].pszObjId),
                 "Expected OID %s, got %s\n",
                 exts[i].exts.rgExtension[j].pszObjId,
                 ext->rgExtension[j].pszObjId);
                ok(!memcmp(ext->rgExtension[j].Value.pbData,
                 exts[i].exts.rgExtension[j].Value.pbData,
                 exts[i].exts.rgExtension[j].Value.cbData),
                 "Unexpected value\n");
            }
            LocalFree(buf);
        }
    }
}

/* MS encodes public key info with a NULL if the algorithm identifier's
 * parameters are empty.  However, when encoding an algorithm in a CERT_INFO,
 * it encodes them by omitting the algorithm parameters.  This latter approach
 * seems more correct, so accept either form.
 */
struct encodedPublicKey
{
    CERT_PUBLIC_KEY_INFO info;
    const BYTE *encoded;
    const BYTE *encodedNoNull;
    CERT_PUBLIC_KEY_INFO decoded;
};

static const BYTE aKey[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd,
 0xe, 0xf };
static const BYTE params[] = { 0x02, 0x01, 0x01 };

static const unsigned char bin64[] = {
    0x30,0x0b,0x30,0x06,0x06,0x02,0x2a,0x03,0x05,0x00,0x03,0x01,0x00,0};
static const unsigned char bin65[] = {
    0x30,0x09,0x30,0x04,0x06,0x02,0x2a,0x03,0x03,0x01,0x00,0};
static const unsigned char bin66[] = {
    0x30,0x0f,0x30,0x0a,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x05,0x00,0x03,0x01,0x00,0};
static const unsigned char bin67[] = {
    0x30,0x0d,0x30,0x08,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x03,0x01,0x00,0};
static const unsigned char bin68[] = {
    0x30,0x1f,0x30,0x0a,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x05,0x00,0x03,0x11,0x00,0x00,0x01,
    0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0};
static const unsigned char bin69[] = {
    0x30,0x1d,0x30,0x08,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x03,0x11,0x00,0x00,0x01,
    0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f,0};
static const unsigned char bin70[] = {
    0x30,0x20,0x30,0x0b,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x02,0x01,0x01,
    0x03,0x11,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
    0x0f,0};
static const unsigned char bin71[] = {
    0x30,0x20,0x30,0x0b,0x06,0x06,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x02,0x01,0x01,
    0x03,0x11,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,
    0x0f,0};
static unsigned char bin72[] = { 0x05,0x00,0};

static const struct encodedPublicKey pubKeys[] = {
 /* with a bogus OID */
 { { { "1.2.3", { 0, NULL } }, { 0, NULL, 0 } },
  bin64, bin65,
  { { "1.2.3", { 2, bin72 } }, { 0, NULL, 0 } } },
 /* some normal keys */
 { { { szOID_RSA, { 0, NULL } }, { 0, NULL, 0} },
  bin66, bin67,
  { { szOID_RSA, { 2, bin72 } }, { 0, NULL, 0 } } },
 { { { szOID_RSA, { 0, NULL } }, { sizeof(aKey), (BYTE *)aKey, 0} },
  bin68, bin69,
  { { szOID_RSA, { 2, bin72 } }, { sizeof(aKey), (BYTE *)aKey, 0} } },
 /* with add'l parameters--note they must be DER-encoded */
 { { { szOID_RSA, { sizeof(params), (BYTE *)params } }, { sizeof(aKey),
  (BYTE *)aKey, 0 } },
  bin70, bin71,
  { { szOID_RSA, { sizeof(params), (BYTE *)params } }, { sizeof(aKey),
  (BYTE *)aKey, 0 } } },
};

static void test_encodePublicKeyInfo(DWORD dwEncoding)
{
    DWORD i;

    for (i = 0; i < sizeof(pubKeys) / sizeof(pubKeys[0]); i++)
    {
        BOOL ret;
        BYTE *buf = NULL;
        DWORD bufSize = 0;

        ret = CryptEncodeObjectEx(dwEncoding, X509_PUBLIC_KEY_INFO,
         &pubKeys[i].info, CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf,
         &bufSize);
        ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            ok(bufSize == pubKeys[i].encoded[1] + 2 ||
             bufSize == pubKeys[i].encodedNoNull[1] + 2,
             "Expected %d or %d bytes, got %ld\n", pubKeys[i].encoded[1] + 2,
             pubKeys[i].encodedNoNull[1] + 2, bufSize);
            if (bufSize == pubKeys[i].encoded[1] + 2)
                ok(!memcmp(buf, pubKeys[i].encoded, pubKeys[i].encoded[1] + 2),
                 "Unexpected value\n");
            else if (bufSize == pubKeys[i].encodedNoNull[1] + 2)
                ok(!memcmp(buf, pubKeys[i].encodedNoNull,
                 pubKeys[i].encodedNoNull[1] + 2), "Unexpected value\n");
            LocalFree(buf);
        }
    }
}

static void comparePublicKeyInfo(const CERT_PUBLIC_KEY_INFO *expected,
 const CERT_PUBLIC_KEY_INFO *got)
{
    ok(!strcmp(expected->Algorithm.pszObjId, got->Algorithm.pszObjId),
     "Expected OID %s, got %s\n", expected->Algorithm.pszObjId,
     got->Algorithm.pszObjId);
    ok(expected->Algorithm.Parameters.cbData ==
     got->Algorithm.Parameters.cbData,
     "Expected parameters of %ld bytes, got %ld\n",
     expected->Algorithm.Parameters.cbData, got->Algorithm.Parameters.cbData);
    if (expected->Algorithm.Parameters.cbData)
        ok(!memcmp(expected->Algorithm.Parameters.pbData,
         got->Algorithm.Parameters.pbData, got->Algorithm.Parameters.cbData),
         "Unexpected algorithm parameters\n");
    ok(expected->PublicKey.cbData == got->PublicKey.cbData,
     "Expected public key of %ld bytes, got %ld\n",
     expected->PublicKey.cbData, got->PublicKey.cbData);
    if (expected->PublicKey.cbData)
        ok(!memcmp(expected->PublicKey.pbData, got->PublicKey.pbData,
         got->PublicKey.cbData), "Unexpected public key value\n");
}

static void test_decodePublicKeyInfo(DWORD dwEncoding)
{
    static const BYTE bogusPubKeyInfo[] =
     "\x30\x22\x30\x0d\x06\x06\x2a\x86\x48\x86\xf7\x0d\x01\x01\x01\x01\x01"
     "\x03\x11\x00\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
     "\x0f";
    DWORD i;
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    for (i = 0; i < sizeof(pubKeys) / sizeof(pubKeys[0]); i++)
    {
        /* The NULL form decodes to the decoded member */
        ret = CryptDecodeObjectEx(dwEncoding, X509_PUBLIC_KEY_INFO,
         pubKeys[i].encoded, pubKeys[i].encoded[1] + 2, CRYPT_DECODE_ALLOC_FLAG,
         NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            comparePublicKeyInfo(&pubKeys[i].decoded,
             (CERT_PUBLIC_KEY_INFO *)buf);
            LocalFree(buf);
        }
        /* The non-NULL form decodes to the original */
        ret = CryptDecodeObjectEx(dwEncoding, X509_PUBLIC_KEY_INFO,
         pubKeys[i].encodedNoNull, pubKeys[i].encodedNoNull[1] + 2,
         CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
        ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
        if (buf)
        {
            comparePublicKeyInfo(&pubKeys[i].info, (CERT_PUBLIC_KEY_INFO *)buf);
            LocalFree(buf);
        }
    }
    /* Test with bogus (not valid DER) parameters */
    ret = CryptDecodeObjectEx(dwEncoding, X509_PUBLIC_KEY_INFO,
     bogusPubKeyInfo, bogusPubKeyInfo[1] + 2, CRYPT_DECODE_ALLOC_FLAG,
     NULL, (BYTE *)&buf, &bufSize);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
     "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
}

static const BYTE v1Cert[] = { 0x30, 0x33, 0x02, 0x00, 0x30, 0x02, 0x06, 0x00,
 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x07, 0x30,
 0x02, 0x06, 0x00, 0x03, 0x01, 0x00 };
static const BYTE v2Cert[] = { 0x30, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x01, 0x02,
 0x00, 0x30, 0x02, 0x06, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f,
 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x5a, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01, 0x00 };
static const BYTE v3Cert[] = { 0x30, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
 0x00, 0x30, 0x02, 0x06, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f,
 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x5a, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01, 0x00 };
static const BYTE v1CertWithConstraints[] = { 0x30, 0x4b, 0x02, 0x00, 0x30,
 0x02, 0x06, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a,
 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01, 0x00, 0xa3, 0x16, 0x30, 0x14,
 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30,
 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE v1CertWithSerial[] = { 0x30, 0x4c, 0x02, 0x01, 0x01, 0x30,
 0x02, 0x06, 0x00, 0x30, 0x22, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a,
 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01, 0x00, 0xa3, 0x16, 0x30, 0x14,
 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30,
 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };
static const BYTE bigCert[] = { 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22,
 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30,
 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20,
 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01,
 0x00, 0xa3, 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01,
 0x01, 0xff, 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };

/* This is the encoded form of the printable string "Juan Lang" */
static const BYTE encodedCommonName[] = { 0x30, 0x15, 0x31, 0x13, 0x30, 0x11,
 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c,
 0x61, 0x6e, 0x67, 0x00 };
static const BYTE serialNum[] = { 0x01 };

static void test_encodeCertToBeSigned(DWORD dwEncoding)
{
    BOOL ret;
    BYTE *buf = NULL;
    DWORD size = 0;
    CERT_INFO info = { 0 };

    /* Test with NULL pvStructInfo */
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, NULL,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* Test with a V1 cert */
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == v1Cert[1] + 2, "Expected size %d, got %ld\n",
         v1Cert[1] + 2, size);
        ok(!memcmp(buf, v1Cert, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* Test v2 cert */
    info.dwVersion = CERT_V2;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v2Cert), "Expected size %d, got %ld\n",
         sizeof(v2Cert), size);
        ok(!memcmp(buf, v2Cert, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* Test v3 cert */
    info.dwVersion = CERT_V3;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v3Cert), "Expected size %d, got %ld\n",
         sizeof(v3Cert), size);
        ok(!memcmp(buf, v3Cert, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* see if a V1 cert can have basic constraints set (RFC3280 says no, but
     * API doesn't prevent it)
     */
    info.dwVersion = CERT_V1;
    info.cExtension = 1;
    info.rgExtension = &criticalExt;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v1CertWithConstraints), "Expected size %d, got %ld\n",
         sizeof(v1CertWithConstraints), size);
        ok(!memcmp(buf, v1CertWithConstraints, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* test v1 cert with a serial number */
    info.SerialNumber.cbData = sizeof(serialNum);
    info.SerialNumber.pbData = (BYTE *)serialNum;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(v1CertWithSerial), "Expected size %d, got %ld\n",
         sizeof(v1CertWithSerial), size);
        ok(!memcmp(buf, v1CertWithSerial, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* Test v1 cert with an issuer name, a subject name, and a serial number */
    info.Issuer.cbData = sizeof(encodedCommonName);
    info.Issuer.pbData = (BYTE *)encodedCommonName;
    info.Subject.cbData = sizeof(encodedCommonName);
    info.Subject.pbData = (BYTE *)encodedCommonName;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(bigCert), "Expected size %d, got %ld\n",
         sizeof(bigCert), size);
        ok(!memcmp(buf, bigCert, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* for now, I let more interesting tests be done for each subcomponent,
     * rather than retesting them all here.
     */
}

static void test_decodeCertToBeSigned(DWORD dwEncoding)
{
    static const BYTE *corruptCerts[] = { v1Cert, v2Cert, v3Cert,
     v1CertWithConstraints, v1CertWithSerial };
    BOOL ret;
    BYTE *buf = NULL;
    DWORD size = 0, i;

    /* Test with NULL pbEncoded */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, NULL, 0,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == CRYPT_E_ASN1_EOD,
     "Expected CRYPT_E_ASN1_EOD, got %08lx\n", GetLastError());
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, NULL, 1,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* The following certs all fail with CRYPT_E_ASN1_CORRUPT, because at a
     * minimum a cert must have a non-zero serial number, an issuer, and a
     * subject.
     */
    for (i = 0; i < sizeof(corruptCerts) / sizeof(corruptCerts[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED,
         corruptCerts[i], corruptCerts[i][1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL,
         (BYTE *)&buf, &size);
        ok(!ret && (GetLastError() == CRYPT_E_ASN1_CORRUPT),
         "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    }
    /* Now check with serial number, subject and issuer specified */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_TO_BE_SIGNED, bigCert,
     sizeof(bigCert), CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_INFO *info = (CERT_INFO *)buf;

        ok(size >= sizeof(CERT_INFO), "Expected size at least %d, got %ld\n",
         sizeof(CERT_INFO), size);
        ok(info->SerialNumber.cbData == 1,
         "Expected serial number size 1, got %ld\n", info->SerialNumber.cbData);
        ok(*info->SerialNumber.pbData == *serialNum,
         "Expected serial number %d, got %d\n", *serialNum,
         *info->SerialNumber.pbData);
        ok(info->Issuer.cbData == sizeof(encodedCommonName),
         "Expected issuer of %d bytes, got %ld\n", sizeof(encodedCommonName),
         info->Issuer.cbData);
        ok(!memcmp(info->Issuer.pbData, encodedCommonName, info->Issuer.cbData),
         "Unexpected issuer\n");
        ok(info->Subject.cbData == sizeof(encodedCommonName),
         "Expected subject of %d bytes, got %ld\n", sizeof(encodedCommonName),
         info->Subject.cbData);
        ok(!memcmp(info->Subject.pbData, encodedCommonName,
         info->Subject.cbData), "Unexpected subject\n");
        LocalFree(buf);
    }
}

static const BYTE hash[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc, 0xd,
 0xe, 0xf };

static const BYTE signedBigCert[] = {
 0x30, 0x81, 0x93, 0x30, 0x7a, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06, 0x00, 0x30,
 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a,
 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x30, 0x22, 0x18, 0x0f,
 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x5a, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30,
 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06,
 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61,
 0x6e, 0x67, 0x00, 0x30, 0x07, 0x30, 0x02, 0x06, 0x00, 0x03, 0x01, 0x00, 0xa3,
 0x16, 0x30, 0x14, 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff,
 0x04, 0x08, 0x30, 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x03, 0x11, 0x00, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x07,
 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00 };

static void test_encodeCert(DWORD dwEncoding)
{
    /* Note the SignatureAlgorithm must match that in the encoded cert.  Note
     * also that bigCert is a NULL-terminated string, so don't count its
     * last byte (otherwise the signed cert won't decode.)
     */
    CERT_SIGNED_CONTENT_INFO info = { { sizeof(bigCert), (BYTE *)bigCert },
     { NULL, { 0, NULL } }, { sizeof(hash), (BYTE *)hash, 0 } };
    BOOL ret;
    BYTE *buf = NULL;
    DWORD bufSize = 0;

    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &bufSize);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(bufSize == sizeof(signedBigCert), "Expected size %d, got %ld\n",
         sizeof(signedBigCert), bufSize);
        ok(!memcmp(buf, signedBigCert, bufSize), "Unexpected cert\n");
        LocalFree(buf);
    }
}

static void test_decodeCert(DWORD dwEncoding)
{
    BOOL ret;
    BYTE *buf = NULL;
    DWORD size = 0;

    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT, signedBigCert,
     sizeof(signedBigCert), CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CERT_SIGNED_CONTENT_INFO *info = (CERT_SIGNED_CONTENT_INFO *)buf;

        ok(info->ToBeSigned.cbData == sizeof(bigCert),
         "Expected cert to be %d bytes, got %ld\n", sizeof(bigCert),
         info->ToBeSigned.cbData);
        ok(!memcmp(info->ToBeSigned.pbData, bigCert, info->ToBeSigned.cbData),
         "Unexpected cert\n");
        ok(info->Signature.cbData == sizeof(hash),
         "Expected signature size %d, got %ld\n", sizeof(hash),
         info->Signature.cbData);
        ok(!memcmp(info->Signature.pbData, hash, info->Signature.cbData),
         "Unexpected signature\n");
        LocalFree(buf);
    }
}

static const BYTE v1CRL[] = { 0x30, 0x15, 0x30, 0x02, 0x06, 0x00, 0x18, 0x0f,
 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x5a };
static const BYTE v2CRL[] = { 0x30, 0x18, 0x02, 0x01, 0x01, 0x30, 0x02, 0x06,
 0x00, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30,
 0x30, 0x30, 0x30, 0x30, 0x5a };
static const BYTE v1CRLWithIssuer[] = { 0x30, 0x2c, 0x30, 0x02, 0x06, 0x00,
 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a,
 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x18, 0x0f, 0x31,
 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x5a };
static const BYTE v1CRLWithIssuerAndEmptyEntry[] = { 0x30, 0x43, 0x30, 0x02,
 0x06, 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03,
 0x13, 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x18,
 0x0f, 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x30, 0x5a, 0x30, 0x15, 0x30, 0x13, 0x02, 0x00, 0x18, 0x0f, 0x31, 0x36,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a };
static const BYTE v1CRLWithIssuerAndEntry[] = { 0x30, 0x44, 0x30, 0x02, 0x06,
 0x00, 0x30, 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
 0x0a, 0x4a, 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x18, 0x0f,
 0x31, 0x36, 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30,
 0x30, 0x5a, 0x30, 0x16, 0x30, 0x14, 0x02, 0x01, 0x01, 0x18, 0x0f, 0x31, 0x36,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a };
static const BYTE v1CRLWithExt[] = { 0x30, 0x5a, 0x30, 0x02, 0x06, 0x00, 0x30,
 0x15, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x0a, 0x4a,
 0x75, 0x61, 0x6e, 0x20, 0x4c, 0x61, 0x6e, 0x67, 0x00, 0x18, 0x0f, 0x31, 0x36,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a,
 0x30, 0x2c, 0x30, 0x2a, 0x02, 0x01, 0x01, 0x18, 0x0f, 0x31, 0x36, 0x30, 0x31,
 0x30, 0x31, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x5a, 0x30, 0x14,
 0x30, 0x12, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x08, 0x30,
 0x06, 0x01, 0x01, 0xff, 0x02, 0x01, 0x01 };

static void test_encodeCRLToBeSigned(DWORD dwEncoding)
{
    BOOL ret;
    BYTE *buf = NULL;
    DWORD size = 0;
    CRL_INFO info = { 0 };
    CRL_ENTRY entry = { { 0 }, { 0 }, 0, 0 };

    /* Test with a V1 CRL */
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v1CRL), "Expected size %d, got %ld\n",
         sizeof(v1CRL), size);
        ok(!memcmp(buf, v1CRL, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* Test v2 CRL */
    info.dwVersion = CRL_V2;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == v2CRL[1] + 2, "Expected size %d, got %ld\n",
         v2CRL[1] + 2, size);
        ok(!memcmp(buf, v2CRL, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* v1 CRL with a name */
    info.dwVersion = CRL_V1;
    info.Issuer.cbData = sizeof(encodedCommonName);
    info.Issuer.pbData = (BYTE *)encodedCommonName;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v1CRLWithIssuer), "Expected size %d, got %ld\n",
         sizeof(v1CRLWithIssuer), size);
        ok(!memcmp(buf, v1CRLWithIssuer, size), "Got unexpected value\n");
        LocalFree(buf);
    }
    /* v1 CRL with a name and a NULL entry pointer */
    info.cCRLEntry = 1;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(!ret && GetLastError() == STATUS_ACCESS_VIOLATION,
     "Expected STATUS_ACCESS_VIOLATION, got %08lx\n", GetLastError());
    /* now set an empty entry */
    info.rgCRLEntry = &entry;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(v1CRLWithIssuerAndEmptyEntry),
         "Expected size %d, got %ld\n", sizeof(v1CRLWithIssuerAndEmptyEntry),
         size);
        ok(!memcmp(buf, v1CRLWithIssuerAndEmptyEntry, size),
         "Got unexpected value\n");
        LocalFree(buf);
    }
    /* an entry with a serial number */
    entry.SerialNumber.cbData = sizeof(serialNum);
    entry.SerialNumber.pbData = (BYTE *)serialNum;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    if (buf)
    {
        ok(size == sizeof(v1CRLWithIssuerAndEntry),
         "Expected size %d, got %ld\n", sizeof(v1CRLWithIssuerAndEntry), size);
        ok(!memcmp(buf, v1CRLWithIssuerAndEntry, size),
         "Got unexpected value\n");
        LocalFree(buf);
    }
    /* and finally, an entry with an extension */
    entry.cExtension = 1;
    entry.rgExtension = &criticalExt;
    ret = CryptEncodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED, &info,
     CRYPT_ENCODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptEncodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        ok(size == sizeof(v1CRLWithExt), "Expected size %d, got %ld\n",
         sizeof(v1CRLWithExt), size);
        ok(!memcmp(buf, v1CRLWithExt, size), "Got unexpected value\n");
        LocalFree(buf);
    }
}

static void test_decodeCRLToBeSigned(DWORD dwEncoding)
{
    static const BYTE *corruptCRLs[] = { v1CRL, v2CRL };
    BOOL ret;
    BYTE *buf = NULL;
    DWORD size = 0, i;

    for (i = 0; i < sizeof(corruptCRLs) / sizeof(corruptCRLs[0]); i++)
    {
        ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED,
         corruptCRLs[i], corruptCRLs[i][1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL,
         (BYTE *)&buf, &size);
        ok(!ret && (GetLastError() == CRYPT_E_ASN1_CORRUPT),
         "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    }
    /* at a minimum, a CRL must contain an issuer: */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED,
     v1CRLWithIssuer, v1CRLWithIssuer[1] + 2, CRYPT_DECODE_ALLOC_FLAG, NULL,
     (BYTE *)&buf, &size);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRL_INFO *info = (CRL_INFO *)buf;

        ok(size >= sizeof(CRL_INFO), "Expected size at least %d, got %ld\n",
         sizeof(CRL_INFO), size);
        ok(info->cCRLEntry == 0, "Expected 0 CRL entries, got %ld\n",
         info->cCRLEntry);
        ok(info->Issuer.cbData == sizeof(encodedCommonName),
         "Expected issuer of %d bytes, got %ld\n", sizeof(encodedCommonName),
         info->Issuer.cbData);
        ok(!memcmp(info->Issuer.pbData, encodedCommonName, info->Issuer.cbData),
         "Unexpected issuer\n");
        LocalFree(buf);
    }
    /* check decoding with an empty CRL entry */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED,
     v1CRLWithIssuerAndEmptyEntry, v1CRLWithIssuerAndEmptyEntry[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    todo_wine ok(!ret && GetLastError() == CRYPT_E_ASN1_CORRUPT,
     "Expected CRYPT_E_ASN1_CORRUPT, got %08lx\n", GetLastError());
    /* with a real CRL entry */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED,
     v1CRLWithIssuerAndEntry, v1CRLWithIssuerAndEntry[1] + 2,
     CRYPT_DECODE_ALLOC_FLAG, NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRL_INFO *info = (CRL_INFO *)buf;
        CRL_ENTRY *entry;

        ok(size >= sizeof(CRL_INFO), "Expected size at least %d, got %ld\n",
         sizeof(CRL_INFO), size);
        ok(info->cCRLEntry == 1, "Expected 1 CRL entries, got %ld\n",
         info->cCRLEntry);
        ok(info->rgCRLEntry != NULL, "Expected a valid CRL entry array\n");
        entry = info->rgCRLEntry;
        ok(entry->SerialNumber.cbData == 1,
         "Expected serial number size 1, got %ld\n",
         entry->SerialNumber.cbData);
        ok(*entry->SerialNumber.pbData == *serialNum,
         "Expected serial number %d, got %d\n", *serialNum,
         *entry->SerialNumber.pbData);
        ok(info->Issuer.cbData == sizeof(encodedCommonName),
         "Expected issuer of %d bytes, got %ld\n", sizeof(encodedCommonName),
         info->Issuer.cbData);
        ok(!memcmp(info->Issuer.pbData, encodedCommonName, info->Issuer.cbData),
         "Unexpected issuer\n");
    }
    /* and finally, with an extension */
    ret = CryptDecodeObjectEx(dwEncoding, X509_CERT_CRL_TO_BE_SIGNED,
     v1CRLWithExt, sizeof(v1CRLWithExt), CRYPT_DECODE_ALLOC_FLAG,
     NULL, (BYTE *)&buf, &size);
    ok(ret, "CryptDecodeObjectEx failed: %08lx\n", GetLastError());
    if (buf)
    {
        CRL_INFO *info = (CRL_INFO *)buf;
        CRL_ENTRY *entry;

        ok(size >= sizeof(CRL_INFO), "Expected size at least %d, got %ld\n",
         sizeof(CRL_INFO), size);
        ok(info->cCRLEntry == 1, "Expected 1 CRL entries, got %ld\n",
         info->cCRLEntry);
        ok(info->rgCRLEntry != NULL, "Expected a valid CRL entry array\n");
        entry = info->rgCRLEntry;
        ok(entry->SerialNumber.cbData == 1,
         "Expected serial number size 1, got %ld\n",
         entry->SerialNumber.cbData);
        ok(*entry->SerialNumber.pbData == *serialNum,
         "Expected serial number %d, got %d\n", *serialNum,
         *entry->SerialNumber.pbData);
        ok(info->Issuer.cbData == sizeof(encodedCommonName),
         "Expected issuer of %d bytes, got %ld\n", sizeof(encodedCommonName),
         info->Issuer.cbData);
        ok(!memcmp(info->Issuer.pbData, encodedCommonName, info->Issuer.cbData),
         "Unexpected issuer\n");
        /* Oddly, the extensions don't seem to be decoded. Is this just an MS
         * bug, or am I missing something?
         */
        ok(info->cExtension == 0, "Expected 0 extensions, got %ld\n",
         info->cExtension);
    }
}

static void test_registerOIDFunction(void)
{
    static const WCHAR bogusDll[] = { 'b','o','g','u','s','.','d','l','l',0 };
    BOOL ret;

    /* oddly, this succeeds under WinXP; the function name key is merely
     * omitted.  This may be a side effect of the registry code, I don't know.
     * I don't check it because I doubt anyone would depend on it.
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, NULL,
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
     */
    /* On windows XP, GetLastError is incorrectly being set with an HRESULT,
     * HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)
     */
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "foo", NULL, bogusDll,
     NULL);
    ok(!ret && (GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() ==
     HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER)),
     "Expected ERROR_INVALID_PARAMETER: %ld\n", GetLastError());
    /* This has no effect, but "succeeds" on XP */
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "foo",
     "1.2.3.4.5.6.7.8.9.10", NULL, NULL);
    ok(ret, "Expected pseudo-success, got %ld\n", GetLastError());
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(X509_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptRegisterOIDFunction(X509_ASN_ENCODING, "bogus",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(X509_ASN_ENCODING, "bogus",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
    /* This has no effect */
    ret = CryptRegisterOIDFunction(PKCS_7_ASN_ENCODING, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    /* Check with bogus encoding type: */
    ret = CryptRegisterOIDFunction(0, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    /* This is written with value 3 verbatim.  Thus, the encoding type isn't
     * (for now) treated as a mask.
     */
    ret = CryptRegisterOIDFunction(3, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10", bogusDll, NULL);
    ok(ret, "CryptRegisterOIDFunction failed: %ld\n", GetLastError());
    ret = CryptUnregisterOIDFunction(3, "CryptDllEncodeObject",
     "1.2.3.4.5.6.7.8.9.10");
    ok(ret, "CryptUnregisterOIDFunction failed: %ld\n", GetLastError());
}

/* Free *pInfo with HeapFree */
static void testExportPublicKey(HCRYPTPROV csp, PCERT_PUBLIC_KEY_INFO *pInfo)
{
    BOOL ret;
    DWORD size = 0;
    HCRYPTKEY key;

    /* This crashes
    ret = CryptExportPublicKeyInfoEx(0, 0, 0, NULL, 0, NULL, NULL, NULL);
     */
    ret = CryptExportPublicKeyInfoEx(0, 0, 0, NULL, 0, NULL, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    ret = CryptExportPublicKeyInfoEx(0, AT_SIGNATURE, 0, NULL, 0, NULL, NULL,
     &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    ret = CryptExportPublicKeyInfoEx(0, 0, X509_ASN_ENCODING, NULL, 0, NULL,
     NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    ret = CryptExportPublicKeyInfoEx(0, AT_SIGNATURE, X509_ASN_ENCODING, NULL,
     0, NULL, NULL, &size);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    /* Test with no key */
    ret = CryptExportPublicKeyInfoEx(csp, AT_SIGNATURE, X509_ASN_ENCODING, NULL,
     0, NULL, NULL, &size);
    ok(!ret && GetLastError() == NTE_NO_KEY, "Expected NTE_NO_KEY, got %08lx\n",
     GetLastError());
    ret = CryptGenKey(csp, AT_SIGNATURE, 0, &key);
    ok(ret, "CryptGenKey failed: %08lx\n", GetLastError());
    if (ret)
    {
        ret = CryptExportPublicKeyInfoEx(csp, AT_SIGNATURE, X509_ASN_ENCODING,
         NULL, 0, NULL, NULL, &size);
        ok(ret, "CryptExportPublicKeyInfoEx failed: %08lx\n", GetLastError());
        *pInfo = HeapAlloc(GetProcessHeap(), 0, size);
        if (*pInfo)
        {
            ret = CryptExportPublicKeyInfoEx(csp, AT_SIGNATURE,
             X509_ASN_ENCODING, NULL, 0, NULL, *pInfo, &size);
            ok(ret, "CryptExportPublicKeyInfoEx failed: %08lx\n",
             GetLastError());
            if (ret)
            {
                /* By default (we passed NULL as the OID) the OID is
                 * szOID_RSA_RSA.
                 */
                ok(!strcmp((*pInfo)->Algorithm.pszObjId, szOID_RSA_RSA),
                 "Expected %s, got %s\n", szOID_RSA_RSA,
                 (*pInfo)->Algorithm.pszObjId);
            }
        }
    }
}

static void testImportPublicKey(HCRYPTPROV csp, PCERT_PUBLIC_KEY_INFO info)
{
    BOOL ret;
    HCRYPTKEY key;

    /* These crash
    ret = CryptImportPublicKeyInfoEx(0, 0, NULL, 0, 0, NULL, NULL);
    ret = CryptImportPublicKeyInfoEx(0, 0, NULL, 0, 0, NULL, &key);
    ret = CryptImportPublicKeyInfoEx(0, 0, info, 0, 0, NULL, NULL);
    ret = CryptImportPublicKeyInfoEx(csp, X509_ASN_ENCODING, info, 0, 0, NULL,
     NULL);
     */
    ret = CryptImportPublicKeyInfoEx(0, 0, info, 0, 0, NULL, &key);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    ret = CryptImportPublicKeyInfoEx(csp, 0, info, 0, 0, NULL, &key);
    ok(!ret && GetLastError() == ERROR_FILE_NOT_FOUND,
     "Expected ERROR_FILE_NOT_FOUND, got %08lx\n", GetLastError());
    ret = CryptImportPublicKeyInfoEx(0, X509_ASN_ENCODING, info, 0, 0, NULL,
     &key);
    ok(!ret && GetLastError() == ERROR_INVALID_PARAMETER,
     "Expected ERROR_INVALID_PARAMETER, got %08lx\n", GetLastError());
    ret = CryptImportPublicKeyInfoEx(csp, X509_ASN_ENCODING, info, 0, 0, NULL,
     &key);
    ok(ret, "CryptImportPublicKeyInfoEx failed: %08lx\n", GetLastError());
    CryptDestroyKey(key);
}

static const char cspName[] = "WineCryptTemp";

static void testPortPublicKeyInfo(void)
{
    HCRYPTPROV csp;
    BOOL ret;
    PCERT_PUBLIC_KEY_INFO info = NULL;

    /* Just in case a previous run failed, delete this thing */
    CryptAcquireContextA(&csp, cspName, MS_DEF_PROV, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
    ret = CryptAcquireContextA(&csp, cspName, MS_DEF_PROV, PROV_RSA_FULL,
     CRYPT_NEWKEYSET);

    testExportPublicKey(csp, &info);
    testImportPublicKey(csp, info);

    HeapFree(GetProcessHeap(), 0, info);
    CryptReleaseContext(csp, 0);
    ret = CryptAcquireContextA(&csp, cspName, MS_DEF_PROV, PROV_RSA_FULL,
     CRYPT_DELETEKEYSET);
}

START_TEST(encode)
{
    static const DWORD encodings[] = { X509_ASN_ENCODING, PKCS_7_ASN_ENCODING,
     X509_ASN_ENCODING | PKCS_7_ASN_ENCODING };
    DWORD i;

    for (i = 0; i < sizeof(encodings) / sizeof(encodings[0]); i++)
    {
        test_encodeInt(encodings[i]);
        test_decodeInt(encodings[i]);
        test_encodeEnumerated(encodings[i]);
        test_decodeEnumerated(encodings[i]);
        test_encodeFiletime(encodings[i]);
        test_decodeFiletime(encodings[i]);
        test_encodeName(encodings[i]);
        test_decodeName(encodings[i]);
        test_encodeAltName(encodings[i]);
        test_decodeAltName(encodings[i]);
        test_encodeOctets(encodings[i]);
        test_decodeOctets(encodings[i]);
        test_encodeBits(encodings[i]);
        test_decodeBits(encodings[i]);
        test_encodeBasicConstraints(encodings[i]);
        test_decodeBasicConstraints(encodings[i]);
        test_encodeRsaPublicKey(encodings[i]);
        test_decodeRsaPublicKey(encodings[i]);
        test_encodeSequenceOfAny(encodings[i]);
        test_decodeSequenceOfAny(encodings[i]);
        test_encodeExtensions(encodings[i]);
        test_decodeExtensions(encodings[i]);
        test_encodePublicKeyInfo(encodings[i]);
        test_decodePublicKeyInfo(encodings[i]);
        test_encodeCertToBeSigned(encodings[i]);
        test_decodeCertToBeSigned(encodings[i]);
        test_encodeCert(encodings[i]);
        test_decodeCert(encodings[i]);
        test_encodeCRLToBeSigned(encodings[i]);
        test_decodeCRLToBeSigned(encodings[i]);
    }
    test_registerOIDFunction();
    testPortPublicKeyInfo();
}
