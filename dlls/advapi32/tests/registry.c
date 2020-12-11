/*
 * Unit tests for registry functions
 *
 * Copyright (c) 2002 Alexandre Julliard
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

#include <assert.h>
#include <stdarg.h>
#include "wine/test.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winsvc.h"
#include "winerror.h"

static HKEY hkey_main;

static const char * sTestpath1 = "%LONGSYSTEMVAR%\\subdir1";
static const char * sTestpath2 = "%FOO%\\subdir1";

/* delete key and all its subkeys */
static DWORD delete_key( HKEY hkey )
{
    char name[MAX_PATH];
    DWORD ret;

    while (!(ret = RegEnumKeyA(hkey, 0, name, sizeof(name))))
    {
        HKEY tmp;
        if (!(ret = RegOpenKeyExA( hkey, name, 0, KEY_ENUMERATE_SUB_KEYS, &tmp )))
        {
            ret = delete_key( tmp );
            RegCloseKey( tmp );
        }
        if (ret) break;
    }
    if (ret != ERROR_NO_MORE_ITEMS) return ret;
    RegDeleteKeyA( hkey, "" );
    return 0;
}

static void setup_main_key(void)
{
    if (RegOpenKeyA( HKEY_CURRENT_USER, "Software\\Wine\\Test", &hkey_main )) delete_key( hkey_main );

    assert (!RegCreateKeyA( HKEY_CURRENT_USER, "Software\\Wine\\Test", &hkey_main ));
}

static void create_test_entries(void)
{
    DWORD qw[2] = { 0x12345678, 0x87654321 };

    SetEnvironmentVariableA("LONGSYSTEMVAR", "bar");
    SetEnvironmentVariableA("FOO", "ImARatherLongButIndeedNeededString");

    ok(!RegSetValueExA(hkey_main,"TP1_EXP_SZ",0,REG_EXPAND_SZ, (LPBYTE)sTestpath1, strlen(sTestpath1)+1), 
        "RegSetValueExA failed\n");
    ok(!RegSetValueExA(hkey_main,"TP1_SZ",0,REG_SZ, (LPBYTE)sTestpath1, strlen(sTestpath1)+1), 
        "RegSetValueExA failed\n");
    ok(!RegSetValueExA(hkey_main,"TP2_EXP_SZ",0,REG_EXPAND_SZ, (LPBYTE)sTestpath2, strlen(sTestpath2)+1), 
        "RegSetValueExA failed\n");
    ok(!RegSetValueExA(hkey_main,"DWORD",0,REG_DWORD, (LPBYTE)qw, 4),
        "RegSetValueExA failed\n");
    ok(!RegSetValueExA(hkey_main,"BIN32",0,REG_BINARY, (LPBYTE)qw, 4),
        "RegSetValueExA failed\n");
    ok(!RegSetValueExA(hkey_main,"BIN64",0,REG_BINARY, (LPBYTE)qw, 8),
        "RegSetValueExA failed\n");
}
        
static void test_enum_value(void)
{
    DWORD res;
    HKEY test_key;
    char value[20], data[20];
    WCHAR valueW[20], dataW[20];
    DWORD val_count, data_count, type;
    static const WCHAR foobarW[] = {'f','o','o','b','a','r',0};
    static const WCHAR testW[] = {'T','e','s','t',0};
    static const WCHAR xxxW[] = {'x','x','x','x','x','x','x','x',0};

    /* create the working key for new 'Test' value */
    res = RegCreateKeyA( hkey_main, "TestKey", &test_key );
    ok( res == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", res);

    /* check NULL data with zero length */
    res = RegSetValueExA( test_key, "Test", 0, REG_SZ, NULL, 0 );
    if (GetVersion() & 0x80000000)
        ok( res == ERROR_INVALID_PARAMETER, "RegSetValueExA returned %ld\n", res );
    else
        ok( !res, "RegSetValueExA returned %ld\n", res );
    res = RegSetValueExA( test_key, "Test", 0, REG_EXPAND_SZ, NULL, 0 );
    ok( ERROR_SUCCESS == res || ERROR_INVALID_PARAMETER == res, "RegSetValueExA returned %ld\n", res );
    res = RegSetValueExA( test_key, "Test", 0, REG_BINARY, NULL, 0 );
    ok( ERROR_SUCCESS == res || ERROR_INVALID_PARAMETER == res, "RegSetValueExA returned %ld\n", res );

    res = RegSetValueExA( test_key, "Test", 0, REG_SZ, (BYTE *)"foobar", 7 );
    ok( res == 0, "RegSetValueExA failed error %ld\n", res );

    /* overflow both name and data */
    val_count = 2;
    data_count = 2;
    type = 1234;
    strcpy( value, "xxxxxxxxxx" );
    strcpy( data, "xxxxxxxxxx" );
    res = RegEnumValueA( test_key, 0, value, &val_count, NULL, &type, (LPBYTE)data, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 2, "val_count set to %ld\n", val_count );
    ok( data_count == 7, "data_count set to %ld instead of 7\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !strcmp( value, "xxxxxxxxxx" ), "value set to '%s'\n", value );
    ok( !strcmp( data, "xxxxxxxxxx" ), "data set to '%s'\n", data );

    /* overflow name */
    val_count = 3;
    data_count = 20;
    type = 1234;
    strcpy( value, "xxxxxxxxxx" );
    strcpy( data, "xxxxxxxxxx" );
    res = RegEnumValueA( test_key, 0, value, &val_count, NULL, &type, (LPBYTE)data, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    /* Win9x returns 2 as specified by MSDN but NT returns 3... */
    ok( val_count == 2 || val_count == 3, "val_count set to %ld\n", val_count );
    ok( data_count == 7, "data_count set to %ld instead of 7\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
#if 0
    /* v5.1.2600.0 (XP Home) does not touch value or data in this case */
    ok( !strcmp( value, "Te" ), "value set to '%s' instead of 'Te'\n", value );
    ok( !strcmp( data, "foobar" ), "data set to '%s' instead of 'foobar'\n", data );
#endif

    /* overflow empty name */
    val_count = 0;
    data_count = 20;
    type = 1234;
    strcpy( value, "xxxxxxxxxx" );
    strcpy( data, "xxxxxxxxxx" );
    res = RegEnumValueA( test_key, 0, value, &val_count, NULL, &type, (LPBYTE)data, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 0, "val_count set to %ld\n", val_count );
    ok( data_count == 7, "data_count set to %ld instead of 7\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !strcmp( value, "xxxxxxxxxx" ), "value set to '%s'\n", value );
#if 0
    /* v5.1.2600.0 (XP Home) does not touch data in this case */
    ok( !strcmp( data, "foobar" ), "data set to '%s' instead of 'foobar'\n", data );
#endif

    /* overflow data */
    val_count = 20;
    data_count = 2;
    type = 1234;
    strcpy( value, "xxxxxxxxxx" );
    strcpy( data, "xxxxxxxxxx" );
    res = RegEnumValueA( test_key, 0, value, &val_count, NULL, &type, (LPBYTE)data, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 20, "val_count set to %ld\n", val_count );
    ok( data_count == 7, "data_count set to %ld instead of 7\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !strcmp( value, "xxxxxxxxxx" ), "value set to '%s'\n", value );
    ok( !strcmp( data, "xxxxxxxxxx" ), "data set to '%s'\n", data );

    /* no overflow */
    val_count = 20;
    data_count = 20;
    type = 1234;
    strcpy( value, "xxxxxxxxxx" );
    strcpy( data, "xxxxxxxxxx" );
    res = RegEnumValueA( test_key, 0, value, &val_count, NULL, &type, (LPBYTE)data, &data_count );
    ok( res == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", res );
    ok( val_count == 4, "val_count set to %ld instead of 4\n", val_count );
    ok( data_count == 7, "data_count set to %ld instead of 7\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !strcmp( value, "Test" ), "value is '%s' instead of Test\n", value );
    ok( !strcmp( data, "foobar" ), "data is '%s' instead of foobar\n", data );

    /* Unicode tests */

    SetLastError(0);
    res = RegSetValueExW( test_key, testW, 0, REG_SZ, (const BYTE *)foobarW, 7*sizeof(WCHAR) );
    if (res==0 && GetLastError()==ERROR_CALL_NOT_IMPLEMENTED)
        return;
    ok( res == 0, "RegSetValueExW failed error %ld\n", res );

    /* overflow both name and data */
    val_count = 2;
    data_count = 2;
    type = 1234;
    memcpy( valueW, xxxW, sizeof(xxxW) );
    memcpy( dataW, xxxW, sizeof(xxxW) );
    res = RegEnumValueW( test_key, 0, valueW, &val_count, NULL, &type, (BYTE*)dataW, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 2, "val_count set to %ld\n", val_count );
    ok( data_count == 7*sizeof(WCHAR), "data_count set to %ld instead of 7*sizeof(WCHAR)\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !memcmp( valueW, xxxW, sizeof(xxxW) ), "value modified\n" );
    ok( !memcmp( dataW, xxxW, sizeof(xxxW) ), "data modified\n" );

    /* overflow name */
    val_count = 3;
    data_count = 20;
    type = 1234;
    memcpy( valueW, xxxW, sizeof(xxxW) );
    memcpy( dataW, xxxW, sizeof(xxxW) );
    res = RegEnumValueW( test_key, 0, valueW, &val_count, NULL, &type, (BYTE*)dataW, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 3, "val_count set to %ld\n", val_count );
    ok( data_count == 7*sizeof(WCHAR), "data_count set to %ld instead of 7*sizeof(WCHAR)\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !memcmp( valueW, xxxW, sizeof(xxxW) ), "value modified\n" );
    ok( !memcmp( dataW, xxxW, sizeof(xxxW) ), "data modified\n" );

    /* overflow data */
    val_count = 20;
    data_count = 2;
    type = 1234;
    memcpy( valueW, xxxW, sizeof(xxxW) );
    memcpy( dataW, xxxW, sizeof(xxxW) );
    res = RegEnumValueW( test_key, 0, valueW, &val_count, NULL, &type, (BYTE*)dataW, &data_count );
    ok( res == ERROR_MORE_DATA, "expected ERROR_MORE_DATA, got %ld\n", res );
    ok( val_count == 4, "val_count set to %ld instead of 4\n", val_count );
    ok( data_count == 7*sizeof(WCHAR), "data_count set to %ld instead of 7*sizeof(WCHAR)\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !memcmp( valueW, testW, sizeof(testW) ), "value is not 'Test'\n" );
    ok( !memcmp( dataW, xxxW, sizeof(xxxW) ), "data modified\n" );

    /* no overflow */
    val_count = 20;
    data_count = 20;
    type = 1234;
    memcpy( valueW, xxxW, sizeof(xxxW) );
    memcpy( dataW, xxxW, sizeof(xxxW) );
    res = RegEnumValueW( test_key, 0, valueW, &val_count, NULL, &type, (BYTE*)dataW, &data_count );
    ok( res == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", res );
    ok( val_count == 4, "val_count set to %ld instead of 4\n", val_count );
    ok( data_count == 7*sizeof(WCHAR), "data_count set to %ld instead of 7*sizeof(WCHAR)\n", data_count );
    ok( type == REG_SZ, "type %ld is not REG_SZ\n", type );
    ok( !memcmp( valueW, testW, sizeof(testW) ), "value is not 'Test'\n" );
    ok( !memcmp( dataW, foobarW, sizeof(foobarW) ), "data is not 'foobar'\n" );
}

static void test_query_value_ex(void)
{
    DWORD ret;
    DWORD size;
    DWORD type;
    
    ret = RegQueryValueExA(hkey_main, "TP1_SZ", NULL, &type, NULL, &size);
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
    ok(size == strlen(sTestpath1) + 1, "(%ld,%ld)\n", (DWORD)strlen(sTestpath1) + 1, size);
    ok(type == REG_SZ, "type %ld is not REG_SZ\n", type);
}

static void test_get_value(void)
{
    HMODULE hadvapi32;
    DWORD (WINAPI *pRegGetValueA)(HKEY,LPCSTR,LPCSTR,DWORD,LPDWORD,PVOID,LPDWORD);
    
    DWORD ret;
    DWORD size;
    DWORD type;
    DWORD dw, qw[2];
    CHAR buf[80];
    CHAR expanded[] = "bar\\subdir1";
   
    /* This function was introduced with Windows 2003 SP1 */
    hadvapi32 = LoadLibraryA("advapi32.dll");
    if(!hadvapi32) 
    {
        ok(0, "error=%ld\n", GetLastError());
        return;
    }
    pRegGetValueA = (PVOID)GetProcAddress(hadvapi32, "RegGetValueA");
    if(!pRegGetValueA) 
        return;

    /* Query REG_DWORD using RRF_RT_REG_DWORD (ok) */
    size = type = dw = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "DWORD", RRF_RT_REG_DWORD, &type, &dw, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == 4, "size=%ld\n", size);
    ok(type == REG_DWORD, "type=%ld\n", type);
    ok(dw == 0x12345678, "dw=%ld\n", dw);

    /* Query by subkey-name */
    ret = pRegGetValueA(HKEY_CURRENT_USER, "Software\\Wine\\Test", "DWORD", RRF_RT_REG_DWORD, NULL, NULL, NULL);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);

    /* Query REG_DWORD using RRF_RT_REG_BINARY (restricted) */
    size = type = dw = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "DWORD", RRF_RT_REG_BINARY, &type, &dw, &size);
    ok(ret == ERROR_UNSUPPORTED_TYPE, "ret=%ld\n", ret);
    /* Although the function failed all values are retrieved */
    ok(size == 4, "size=%ld\n", size);
    ok(type == REG_DWORD, "type=%ld\n", type);
    ok(dw == 0x12345678, "dw=%ld\n", dw);

    /* Test RRF_ZEROONFAILURE */
    type = dw = 0xdeadbeef; size = 4;
    ret = pRegGetValueA(hkey_main, NULL, "DWORD", RRF_RT_REG_SZ|RRF_ZEROONFAILURE, &type, &dw, &size);
    ok(ret == ERROR_UNSUPPORTED_TYPE, "ret=%ld\n", ret);
    /* Again all values are retrieved ... */
    ok(size == 4, "size=%ld\n", size);
    ok(type == REG_DWORD, "type=%ld\n", type);
    /* ... except the buffer, which is zeroed out */
    ok(dw == 0, "dw=%ld\n", dw);

    /* Query REG_DWORD using RRF_RT_DWORD (ok) */
    size = type = dw = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "DWORD", RRF_RT_DWORD, &type, &dw, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == 4, "size=%ld\n", size);
    ok(type == REG_DWORD, "type=%ld\n", type);
    ok(dw == 0x12345678, "dw=%ld\n", dw);

    /* Query 32-bit REG_BINARY using RRF_RT_DWORD (ok) */
    size = type = dw = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "BIN32", RRF_RT_DWORD, &type, &dw, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == 4, "size=%ld\n", size);
    ok(type == REG_BINARY, "type=%ld\n", type);
    ok(dw == 0x12345678, "dw=%ld\n", dw);
    
    /* Query 64-bit REG_BINARY using RRF_RT_DWORD (type mismatch) */
    qw[0] = qw[1] = size = type = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "BIN64", RRF_RT_DWORD, &type, qw, &size);
    ok(ret == ERROR_DATATYPE_MISMATCH, "ret=%ld\n", ret);
    ok(size == 8, "size=%ld\n", size);
    ok(type == REG_BINARY, "type=%ld\n", type);
    ok(qw[0] == 0x12345678 && 
       qw[1] == 0x87654321, "qw={%ld,%ld}\n", qw[0], qw[1]);
    
    /* Query 64-bit REG_BINARY using 32-bit buffer (buffer too small) */
    type = dw = 0xdeadbeef; size = 4;
    ret = pRegGetValueA(hkey_main, NULL, "BIN64", RRF_RT_REG_BINARY, &type, &dw, &size);
    ok(ret == ERROR_MORE_DATA, "ret=%ld\n", ret);
    ok(dw == 0xdeadbeef, "dw=%ld\n", dw);
    ok(size == 8, "size=%ld\n", size);

    /* Query 64-bit REG_BINARY using RRF_RT_QWORD (ok) */
    qw[0] = qw[1] = size = type = 0xdeadbeef;
    ret = pRegGetValueA(hkey_main, NULL, "BIN64", RRF_RT_QWORD, &type, qw, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == 8, "size=%ld\n", size);
    ok(type == REG_BINARY, "type=%ld\n", type);
    ok(qw[0] == 0x12345678 &&
       qw[1] == 0x87654321, "qw={%ld,%ld}\n", qw[0], qw[1]);

    /* Query REG_SZ using RRF_RT_REG_SZ (ok) */
    buf[0] = 0; type = 0xdeadbeef; size = sizeof(buf);
    ret = pRegGetValueA(hkey_main, NULL, "TP1_SZ", RRF_RT_REG_SZ, &type, buf, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == strlen(sTestpath1)+1, "strlen(sTestpath1)=%d size=%ld\n", strlen(sTestpath1), size);
    ok(type == REG_SZ, "type=%ld\n", type);
    ok(!strcmp(sTestpath1, buf), "sTestpath=\"%s\" buf=\"%s\"\n", sTestpath1, buf);

    /* Query REG_SZ using RRF_RT_REG_SZ|RRF_NOEXPAND (ok) */
    buf[0] = 0; type = 0xdeadbeef; size = sizeof(buf);
    ret = pRegGetValueA(hkey_main, NULL, "TP1_SZ", RRF_RT_REG_SZ|RRF_NOEXPAND, &type, buf, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == strlen(sTestpath1)+1, "strlen(sTestpath1)=%d size=%ld\n", strlen(sTestpath1), size);
    ok(type == REG_SZ, "type=%ld\n", type);
    ok(!strcmp(sTestpath1, buf), "sTestpath=\"%s\" buf=\"%s\"\n", sTestpath1, buf);

    /* Query REG_EXPAND_SZ using RRF_RT_REG_SZ (ok, expands) */
    buf[0] = 0; type = 0xdeadbeef; size = sizeof(buf);
    ret = pRegGetValueA(hkey_main, NULL, "TP1_EXP_SZ", RRF_RT_REG_SZ, &type, buf, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
#if 0
     /* At least v5.2.3790.1830 (2003 SP1) returns the unexpanded length + 1 here. */
     ok(size == strlen(expanded)+1, "strlen(expanded)=%ld size=%ld\n", strlen(expanded), size);
#endif
    ok(type == REG_SZ, "type=%ld\n", type);
    ok(!strcmp(expanded, buf), "expanded=\"%s\" buf=\"%s\"\n", expanded, buf);
    
    /* Query REG_EXPAND_SZ using RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND (ok, doesn't expand) */
    buf[0] = 0; type = 0xdeadbeef; size = sizeof(buf);
    ret = pRegGetValueA(hkey_main, NULL, "TP1_EXP_SZ", RRF_RT_REG_EXPAND_SZ|RRF_NOEXPAND, &type, buf, &size);
    ok(ret == ERROR_SUCCESS, "ret=%ld\n", ret);
    ok(size == strlen(sTestpath1)+1, "strlen(sTestpath1)=%d size=%ld\n", strlen(sTestpath1), size);
    ok(type == REG_EXPAND_SZ, "type=%ld\n", type);
    ok(!strcmp(sTestpath1, buf), "sTestpath=\"%s\" buf=\"%s\"\n", sTestpath1, buf);
    
    /* Query REG_EXPAND_SZ using RRF_RT_REG_SZ|RRF_NOEXPAND (type mismatch) */
    ret = pRegGetValueA(hkey_main, NULL, "TP1_EXP_SZ", RRF_RT_REG_SZ|RRF_NOEXPAND, NULL, NULL, NULL);
    ok(ret == ERROR_UNSUPPORTED_TYPE, "ret=%ld\n", ret);

    /* Query REG_EXPAND_SZ using RRF_RT_REG_EXPAND_SZ (not allowed without RRF_NOEXPAND) */
    ret = pRegGetValueA(hkey_main, NULL, "TP1_EXP_SZ", RRF_RT_REG_EXPAND_SZ, NULL, NULL, NULL);
    ok(ret == ERROR_INVALID_PARAMETER, "ret=%ld\n", ret);
} 

static void test_reg_open_key(void)
{
    DWORD ret = 0;
    HKEY hkResult = NULL;
    HKEY hkPreserve = NULL;

    /* successful open */
    ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", &hkResult);
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
    ok(hkResult != NULL, "expected hkResult != NULL\n");
    hkPreserve = hkResult;

    /* these tests fail on Win9x, but we want to be compatible with NT, so
     * run them if we can */
    if (!(GetVersion() & 0x80000000))
    {
        /* open same key twice */
        ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", &hkResult);
        ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
        ok(hkResult != hkPreserve, "epxected hkResult != hkPreserve\n");
        ok(hkResult != NULL, "hkResult != NULL\n");
        RegCloseKey(hkResult);
    
        /* open nonexistent key
        * check that hkResult is set to NULL
        */
        hkResult = hkPreserve;
        ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Nonexistent", &hkResult);
        ok(ret == ERROR_FILE_NOT_FOUND, "expected ERROR_FILE_NOT_FOUND, got %ld\n", ret);
        ok(hkResult == NULL, "expected hkResult == NULL\n");
    
        /* open the same nonexistent key again to make sure the key wasn't created */
        hkResult = hkPreserve;
        ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Nonexistent", &hkResult);
        ok(ret == ERROR_FILE_NOT_FOUND, "expected ERROR_FILE_NOT_FOUND, got %ld\n", ret);
        ok(hkResult == NULL, "expected hkResult == NULL\n");
    
        /* send in NULL lpSubKey
        * check that hkResult receives the value of hKey
        */
        hkResult = hkPreserve;
        ret = RegOpenKeyA(HKEY_CURRENT_USER, NULL, &hkResult);
        ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
        ok(hkResult == HKEY_CURRENT_USER, "expected hkResult == HKEY_CURRENT_USER\n");
    
        /* send empty-string in lpSubKey */
        hkResult = hkPreserve;
        ret = RegOpenKeyA(HKEY_CURRENT_USER, "", &hkResult);
        ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
        ok(hkResult == HKEY_CURRENT_USER, "expected hkResult == HKEY_CURRENT_USER\n");
    
        /* send in NULL lpSubKey and NULL hKey
        * hkResult is set to NULL
        */
        hkResult = hkPreserve;
        ret = RegOpenKeyA(NULL, NULL, &hkResult);
        ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
        ok(hkResult == NULL, "expected hkResult == NULL\n");
    }

    /* only send NULL hKey
     * the value of hkResult remains unchanged
     */
    hkResult = hkPreserve;
    ret = RegOpenKeyA(NULL, "Software\\Wine\\Test", &hkResult);
    ok(ret == ERROR_INVALID_HANDLE || ret == ERROR_BADKEY, /* Windows 95 returns BADKEY */
       "expected ERROR_INVALID_HANDLE or ERROR_BADKEY, got %ld\n", ret);
    ok(hkResult == hkPreserve, "expected hkResult == hkPreserve\n");
    RegCloseKey(hkResult);

    /* send in NULL hkResult */
    ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", NULL);
    ok(ret == ERROR_INVALID_PARAMETER, "expected ERROR_INVALID_PARAMETER, got %ld\n", ret);
}

static void test_reg_create_key(void)
{
    LONG ret;
    HKEY hkey1, hkey2;
    ret = RegCreateKeyExA(hkey_main, "Subkey1", 0, NULL, 0, KEY_NOTIFY, NULL, &hkey1, NULL);
    ok(!ret, "RegCreateKeyExA failed with error %ld\n", ret);
    /* should succeed: all versions of Windows ignore the access rights
     * to the parent handle */
    ret = RegCreateKeyExA(hkey1, "Subkey2", 0, NULL, 0, KEY_SET_VALUE, NULL, &hkey2, NULL);
    ok(!ret, "RegCreateKeyExA failed with error %ld\n", ret);

    /* clean up */
    RegDeleteKey(hkey2, NULL);
    RegDeleteKey(hkey1, NULL);
}

static void test_reg_close_key(void)
{
    DWORD ret = 0;
    HKEY hkHandle;

    /* successfully close key
     * hkHandle remains changed after call to RegCloseKey
     */
    ret = RegOpenKeyA(HKEY_CURRENT_USER, "Software\\Wine\\Test", &hkHandle);
    ret = RegCloseKey(hkHandle);
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);

    /* try to close the key twice */
    ret = RegCloseKey(hkHandle); /* Windows 95 doesn't mind. */
    ok(ret == ERROR_INVALID_HANDLE || ret == ERROR_SUCCESS,
       "expected ERROR_INVALID_HANDLE or ERROR_SUCCESS, got %ld\n", ret);
    
    /* try to close a NULL handle */
    ret = RegCloseKey(NULL);
    ok(ret == ERROR_INVALID_HANDLE || ret == ERROR_BADKEY, /* Windows 95 returns BADKEY */
       "expected ERROR_INVALID_HANDLE or ERROR_BADKEY, got %ld\n", ret);
}

static void test_reg_delete_key(void)
{
    DWORD ret;

    ret = RegDeleteKey(hkey_main, NULL);
    ok(ret == ERROR_INVALID_PARAMETER ||
       ret == ERROR_ACCESS_DENIED ||
       ret == ERROR_BADKEY, /* Win95 */
       "ret=%ld\n", ret);
}

static void test_reg_save_key(void)
{
    DWORD ret;

    ret = RegSaveKey(hkey_main, "saved_key", NULL);
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);
}

static void test_reg_load_key(void)
{
    DWORD ret;
    HKEY hkHandle;

    ret = RegLoadKey(HKEY_LOCAL_MACHINE, "Test", "saved_key");
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);

    ret = RegOpenKey(HKEY_LOCAL_MACHINE, "Test", &hkHandle);
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);

    RegCloseKey(hkHandle);
}

static void test_reg_unload_key(void)
{
    DWORD ret;

    ret = RegUnLoadKey(HKEY_LOCAL_MACHINE, "Test");
    ok(ret == ERROR_SUCCESS, "expected ERROR_SUCCESS, got %ld\n", ret);

    DeleteFile("saved_key");
    DeleteFile("saved_key.LOG");
}

static BOOL set_privileges(LPCSTR privilege, BOOL set)
{
    TOKEN_PRIVILEGES tp;
    HANDLE hToken;
    LUID luid;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
        return FALSE;

    if(!LookupPrivilegeValue(NULL, privilege, &luid))
    {
        CloseHandle(hToken);
        return FALSE;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    
    if (set)
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    else
        tp.Privileges[0].Attributes = 0;

    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (GetLastError() != ERROR_SUCCESS)
    {
        CloseHandle(hToken);
        return FALSE;
    }

    CloseHandle(hToken);
    return TRUE;
}

/* tests that show that RegConnectRegistry and 
   OpenSCManager accept computer names without the
   \\ prefix (what MSDN says).   */
static void test_regconnectregistry( void)
{
    CHAR compName[MAX_COMPUTERNAME_LENGTH + 1];
    CHAR netwName[MAX_COMPUTERNAME_LENGTH + 3]; /* 2 chars for double backslash */
    DWORD len = sizeof(compName) ;
    BOOL ret;
    LONG retl;
    HKEY hkey;
    SC_HANDLE schnd;
    DWORD GLE;

    ret = GetComputerNameA(compName, &len);
    ok( ret, "GetComputerName failed err = %ld\n", GetLastError());
    if( !ret) return;

    lstrcpyA(netwName, "\\\\");
    lstrcpynA(netwName+2, compName, MAX_COMPUTERNAME_LENGTH + 1);

    retl = RegConnectRegistryA( compName, HKEY_LOCAL_MACHINE, &hkey);
    ok( !retl || retl == ERROR_DLL_INIT_FAILED, "RegConnectRegistryA failed err = %ld\n", retl);
    if( !retl) RegCloseKey( hkey);

    retl = RegConnectRegistryA( netwName, HKEY_LOCAL_MACHINE, &hkey);
    ok( !retl || retl == ERROR_DLL_INIT_FAILED, "RegConnectRegistryA failed err = %ld\n", retl);
    if( !retl) RegCloseKey( hkey);

    schnd = OpenSCManagerA( compName, NULL, GENERIC_READ); 
    GLE = GetLastError();
    ok( schnd != NULL || GLE==ERROR_CALL_NOT_IMPLEMENTED, 
        "OpenSCManagerA failed err = %ld\n", GLE);
    CloseServiceHandle( schnd);

    schnd = OpenSCManagerA( netwName, NULL, GENERIC_READ); 
    GLE = GetLastError();
    ok( schnd != NULL || GLE==ERROR_CALL_NOT_IMPLEMENTED, 
        "OpenSCManagerA failed err = %ld\n", GLE);
    CloseServiceHandle( schnd);

}

START_TEST(registry)
{
    setup_main_key();
    create_test_entries();
    test_enum_value();
    test_query_value_ex();
    test_get_value();
    test_reg_open_key();
    test_reg_create_key();
    test_reg_close_key();
    test_reg_delete_key();

    /* SaveKey/LoadKey require the SE_BACKUP_NAME privilege to be set */
    if (set_privileges(SE_BACKUP_NAME, TRUE) &&
        set_privileges(SE_RESTORE_NAME, TRUE))
    {
        test_reg_save_key();
        test_reg_load_key();
        test_reg_unload_key();

        set_privileges(SE_BACKUP_NAME, FALSE);
        set_privileges(SE_RESTORE_NAME, FALSE);
    }

    /* cleanup */
    delete_key( hkey_main );
    
    test_regconnectregistry();
}
