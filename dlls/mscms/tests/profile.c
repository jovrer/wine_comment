/*
 * Tests for color profile functions
 *
 * Copyright 2004, 2005 Hans Leidekker
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

#include "windef.h"
#include "winbase.h"
#include "winnls.h"
#include "wingdi.h"
#include "winuser.h"
#include "icm.h"

#include "wine/test.h"

HMODULE hmscms;

static BOOL     (WINAPI *pCloseColorProfile)(HPROFILE);
static BOOL     (WINAPI *pGetColorDirectoryA)(PCHAR,PCHAR,PDWORD);
static BOOL     (WINAPI *pGetColorDirectoryW)(PWCHAR,PWCHAR,PDWORD);
static BOOL     (WINAPI *pGetColorProfileElement)(HPROFILE,TAGTYPE,DWORD,PDWORD,PVOID,PBOOL);
static BOOL     (WINAPI *pGetColorProfileElementTag)(HPROFILE,DWORD,PTAGTYPE);
static BOOL     (WINAPI *pGetColorProfileFromHandle)(HPROFILE,PBYTE,PDWORD);
static BOOL     (WINAPI *pGetColorProfileHeader)(HPROFILE,PPROFILEHEADER);
static BOOL     (WINAPI *pGetCountColorProfileElements)(HPROFILE,PDWORD);
static BOOL     (WINAPI *pGetStandardColorSpaceProfileA)(PCSTR,DWORD,PSTR,PDWORD);
static BOOL     (WINAPI *pGetStandardColorSpaceProfileW)(PCWSTR,DWORD,PWSTR,PDWORD);
static BOOL     (WINAPI *pInstallColorProfileA)(PCSTR,PCSTR);
static BOOL     (WINAPI *pInstallColorProfileW)(PCWSTR,PCWSTR);
static BOOL     (WINAPI *pIsColorProfileTagPresent)(HPROFILE,TAGTYPE,PBOOL);
static HPROFILE (WINAPI *pOpenColorProfileA)(PPROFILE,DWORD,DWORD,DWORD);
static HPROFILE (WINAPI *pOpenColorProfileW)(PPROFILE,DWORD,DWORD,DWORD);
static BOOL     (WINAPI *pSetColorProfileElement)(HPROFILE,TAGTYPE,DWORD,PDWORD,PVOID);
static BOOL     (WINAPI *pSetColorProfileHeader)(HPROFILE,PPROFILEHEADER);
static BOOL     (WINAPI *pSetStandardColorSpaceProfileA)(PCSTR,DWORD,PSTR);
static BOOL     (WINAPI *pSetStandardColorSpaceProfileW)(PCWSTR,DWORD,PWSTR);
static BOOL     (WINAPI *pUninstallColorProfileA)(PCSTR,PCSTR,BOOL);
static BOOL     (WINAPI *pUninstallColorProfileW)(PCWSTR,PCWSTR,BOOL);

#define GETFUNCPTR(func) p##func = (void *)GetProcAddress( hmscms, #func ); \
    if (!p##func) return FALSE;

static BOOL init_function_ptrs( void )
{
    GETFUNCPTR( CloseColorProfile )
    GETFUNCPTR( GetColorDirectoryA )
    GETFUNCPTR( GetColorDirectoryW )
    GETFUNCPTR( GetColorProfileElement )
    GETFUNCPTR( GetColorProfileElementTag )
    GETFUNCPTR( GetColorProfileFromHandle )
    GETFUNCPTR( GetColorProfileHeader )
    GETFUNCPTR( GetCountColorProfileElements )
    GETFUNCPTR( GetStandardColorSpaceProfileA )
    GETFUNCPTR( GetStandardColorSpaceProfileW )
    GETFUNCPTR( InstallColorProfileA )
    GETFUNCPTR( InstallColorProfileW )
    GETFUNCPTR( IsColorProfileTagPresent )
    GETFUNCPTR( OpenColorProfileA )
    GETFUNCPTR( OpenColorProfileW )
    GETFUNCPTR( SetColorProfileElement )
    GETFUNCPTR( SetColorProfileHeader )
    GETFUNCPTR( SetStandardColorSpaceProfileA )
    GETFUNCPTR( SetStandardColorSpaceProfileW )
    GETFUNCPTR( UninstallColorProfileA )
    GETFUNCPTR( UninstallColorProfileW )

    return TRUE;
}

static const char machine[] = "dummy";
static const WCHAR machineW[] = { 'd','u','m','m','y',0 };

/*  To do any real functionality testing with this suite you need a copy of
 *  the freely distributable standard RGB color space profile. It comes
 *  standard with Windows, but on Wine you probably need to install it yourself
 *  in one of the locations mentioned below. Here's a link to the profile in
 *  a self extracting zip file:
 *
 *  http://download.microsoft.com/download/whistler/hwdev1/1.0/wxp/en-us/ColorProfile.exe
 */

/* Two common places to find the standard color space profile, relative
 * to the system directory.
 */
static const char profile1[] =
"\\color\\srgb color space profile.icm";
static const char profile2[] =
"\\spool\\drivers\\color\\srgb color space profile.icm";

static const WCHAR profile1W[] =
{ '\\','c','o','l','o','r','\\','s','r','g','b',' ','c','o','l','o','r',' ',
  's','p','a','c','e',' ','p','r','o','f','i','l','e','.','i','c','m',0 };
static const WCHAR profile2W[] =
{ '\\','s','p','o','o','l','\\','d','r','i','v','e','r','s','\\',
  'c','o','l','o','r','\\','s','r','g','b',' ','c','o','l','o','r',' ',
  's','p','a','c','e',' ','p','r','o','f','i','l','e','.','i','c','m',0 };

static const unsigned char rgbheader[] =
{ 0x48, 0x0c, 0x00, 0x00, 0x6f, 0x6e, 0x69, 0x4c, 0x00, 0x00, 0x10, 0x02,
  0x72, 0x74, 0x6e, 0x6d, 0x20, 0x42, 0x47, 0x52, 0x20, 0x5a, 0x59, 0x58,
  0x02, 0x00, 0xce, 0x07, 0x06, 0x00, 0x09, 0x00, 0x00, 0x00, 0x31, 0x00,
  0x70, 0x73, 0x63, 0x61, 0x54, 0x46, 0x53, 0x4d, 0x00, 0x00, 0x00, 0x00,
  0x20, 0x43, 0x45, 0x49, 0x42, 0x47, 0x52, 0x73, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd6, 0xf6, 0x00, 0x00,
  0x00, 0x00, 0x01, 0x00, 0x2d, 0xd3, 0x00, 0x00, 0x20, 0x20, 0x50, 0x48 };

static LPSTR standardprofile;
static LPWSTR standardprofileW;

static LPSTR testprofile;
static LPWSTR testprofileW;

#define IS_SEPARATOR(ch)  ((ch) == '\\' || (ch) == '/')

static void MSCMS_basenameA( LPCSTR path, LPSTR name )
{
    INT i = strlen( path );

    while (i > 0 && !IS_SEPARATOR(path[i - 1])) i--;
    strcpy( name, &path[i] );
}

static void MSCMS_basenameW( LPCWSTR path, LPWSTR name )
{
    INT i = lstrlenW( path );

    while (i > 0 && !IS_SEPARATOR(path[i - 1])) i--;
    lstrcpyW( name, &path[i] );
}

static void test_GetColorDirectoryA(void)
{
    BOOL ret;
    DWORD size;
    char buffer[MAX_PATH];

    /* Parameter checks */

    ret = pGetColorDirectoryA( NULL, NULL, NULL );
    ok( !ret, "GetColorDirectoryA() succeeded (%ld)\n", GetLastError() );

    size = 0;

    ret = pGetColorDirectoryA( NULL, NULL, &size );
    ok( !ret && size > 0, "GetColorDirectoryA() succeeded (%ld)\n", GetLastError() );

    size = 0;

    ret = pGetColorDirectoryA( NULL, buffer, &size );
    ok( !ret && size > 0, "GetColorDirectoryA() succeeded (%ld)\n", GetLastError() );

    size = 1;

    ret = pGetColorDirectoryA( NULL, buffer, &size );
    ok( !ret && size > 0, "GetColorDirectoryA() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    size = sizeof(buffer);

    ret = pGetColorDirectoryA( NULL, buffer, &size );
    ok( ret && size > 0, "GetColorDirectoryA() failed (%ld)\n", GetLastError() );
}

static void test_GetColorDirectoryW(void)
{
    BOOL ret;
    DWORD size;
    WCHAR buffer[MAX_PATH];

    /* Parameter checks */

    /* This one crashes win2k
    
    ret = pGetColorDirectoryW( NULL, NULL, NULL );
    ok( !ret, "GetColorDirectoryW() succeeded (%ld)\n", GetLastError() );

     */

    size = 0;

    ret = pGetColorDirectoryW( NULL, NULL, &size );
    ok( !ret && size > 0, "GetColorDirectoryW() succeeded (%ld)\n", GetLastError() );

    size = 0;

    ret = pGetColorDirectoryW( NULL, buffer, &size );
    ok( !ret && size > 0, "GetColorDirectoryW() succeeded (%ld)\n", GetLastError() );

    size = 1;

    ret = pGetColorDirectoryW( NULL, buffer, &size );
    ok( !ret && size > 0, "GetColorDirectoryW() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    size = sizeof(buffer);

    ret = pGetColorDirectoryW( NULL, buffer, &size );
    ok( ret && size > 0, "GetColorDirectoryW() failed (%ld)\n", GetLastError() );
}

static void test_GetColorProfileElement(void)
{
    if (standardprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret, ref;
        DWORD size;
        TAGTYPE tag = 0x63707274;  /* 'cprt' */
        static char buffer[51];
        static const char expect[] =
            { 0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x43, 0x6f, 0x70,
              0x79, 0x72, 0x69, 0x67, 0x68, 0x74, 0x20, 0x28, 0x63, 0x29, 0x20,
              0x31, 0x39, 0x39, 0x38, 0x20, 0x48, 0x65, 0x77, 0x6c, 0x65, 0x74,
              0x74, 0x2d, 0x50, 0x61, 0x63, 0x6b, 0x61, 0x72, 0x64, 0x20, 0x43,
              0x6f, 0x6d, 0x70, 0x61, 0x6e, 0x79, 0x00 };

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = standardprofile;
        profile.cbDataSize = strlen(standardprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        ret = pGetColorProfileElement( handle, tag, 0, NULL, NULL, &ref );
        ok( !ret, "GetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileElement( handle, tag, 0, &size, NULL, NULL );
        ok( !ret, "GetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        size = 0;

        ret = pGetColorProfileElement( handle, tag, 0, &size, NULL, &ref );
        ok( !ret && size > 0, "GetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        size = sizeof(buffer);

        /* Functional checks */

        ret = pGetColorProfileElement( handle, tag, 0, &size, buffer, &ref );
        ok( ret && size > 0, "GetColorProfileElement() failed (%ld)\n", GetLastError() );

        ok( !memcmp( buffer, expect, sizeof(expect) ), "Unexpected tag data\n" );

        pCloseColorProfile( handle );
    }
}

static void test_GetColorProfileElementTag(void)
{
    if (standardprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret;
        DWORD index = 1;
        TAGTYPE tag, expect = 0x63707274;  /* 'cprt' */

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = standardprofile;
        profile.cbDataSize = strlen(standardprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        ret = pGetColorProfileElementTag( NULL, index, &tag );
        ok( !ret, "GetColorProfileElementTag() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileElementTag( handle, 0, &tag );
        ok( !ret, "GetColorProfileElementTag() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileElementTag( handle, index, NULL );
        ok( !ret, "GetColorProfileElementTag() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileElementTag( handle, 18, NULL );
        ok( !ret, "GetColorProfileElementTag() succeeded (%ld)\n", GetLastError() );

        /* Functional checks */

        ret = pGetColorProfileElementTag( handle, index, &tag );
        ok( ret && tag == expect, "GetColorProfileElementTag() failed (%ld)\n",
            GetLastError() );

        pCloseColorProfile( handle );
    }
}

static void test_GetColorProfileFromHandle(void)
{
    if (testprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        DWORD size;
        BOOL ret;
        static const unsigned char expect[] =
            { 0x00, 0x00, 0x0c, 0x48, 0x4c, 0x69, 0x6e, 0x6f, 0x02, 0x10, 0x00,
              0x00, 0x6d, 0x6e, 0x74, 0x72, 0x52, 0x47, 0x42, 0x20, 0x58, 0x59,
              0x5a, 0x20, 0x07, 0xce, 0x00, 0x02, 0x00, 0x09, 0x00, 0x06, 0x00,
              0x31, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x4d, 0x53, 0x46, 0x54,
              0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x43, 0x20, 0x73, 0x52, 0x47,
              0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00,
              0x00, 0xd3, 0x2d, 0x48, 0x50, 0x20, 0x20 };

        unsigned char *buffer;

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = testprofile;
        profile.cbDataSize = strlen(testprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        size = 0;

        ret = pGetColorProfileFromHandle( handle, NULL, &size );
        ok( !ret && size > 0, "GetColorProfileFromHandle() failed (%ld)\n", GetLastError() );

        buffer = HeapAlloc( GetProcessHeap(), 0, size );

        if (buffer)
        {
            ret = pGetColorProfileFromHandle( NULL, buffer, &size );
            ok( !ret, "GetColorProfileFromHandle() succeeded (%ld)\n", GetLastError() );

            ret = pGetColorProfileFromHandle( handle, buffer, NULL );
            ok( !ret, "GetColorProfileFromHandle() succeeded (%ld)\n", GetLastError() );

            /* Functional checks */

            ret = pGetColorProfileFromHandle( handle, buffer, &size );
            ok( ret && size > 0, "GetColorProfileFromHandle() failed (%ld)\n", GetLastError() );

            ok( !memcmp( buffer, expect, sizeof(expect) ), "Unexpected header data\n" );

            HeapFree( GetProcessHeap(), 0, buffer );
        }

        pCloseColorProfile( handle );
    }
}

static void test_GetColorProfileHeader(void)
{
    if (testprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret;
        PROFILEHEADER header;

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = testprofile;
        profile.cbDataSize = strlen(testprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        ret = pGetColorProfileHeader( NULL, NULL );
        ok( !ret, "GetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileHeader( NULL, &header );
        ok( !ret, "GetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        ret = pGetColorProfileHeader( handle, NULL );
        ok( !ret, "GetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        /* Functional checks */

        ret = pGetColorProfileHeader( handle, &header );
        ok( ret, "GetColorProfileHeader() failed (%ld)\n", GetLastError() );

        ok( !memcmp( &header, rgbheader, sizeof(rgbheader) ), "Unexpected header data\n" );

        pCloseColorProfile( handle );
    }
}

static void test_GetCountColorProfileElements(void)
{
    if (standardprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret;
        DWORD count, expect = 17;

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = standardprofile;
        profile.cbDataSize = strlen(standardprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        ret = pGetCountColorProfileElements( NULL, &count );
        ok( !ret, "GetCountColorProfileElements() succeeded (%ld)\n",
            GetLastError() );

        ret = pGetCountColorProfileElements( handle, NULL );
        ok( !ret, "GetCountColorProfileElements() succeeded (%ld)\n",
            GetLastError() );

        /* Functional checks */

        ret = pGetCountColorProfileElements( handle, &count );
        ok( ret && count == expect,
            "GetCountColorProfileElements() failed (%ld)\n", GetLastError() );

        pCloseColorProfile( handle );
    }
}

static void test_GetStandardColorSpaceProfileA(void)
{
    BOOL ret;
    DWORD size;
    CHAR oldprofile[MAX_PATH];
    CHAR newprofile[MAX_PATH];

    /* Parameter checks */

    ret = pGetStandardColorSpaceProfileA( NULL, 0, newprofile, NULL );
    ok( !ret, "GetStandardColorSpaceProfileA() succeeded (%ld)\n", GetLastError() );

    ret = pGetStandardColorSpaceProfileA( machine, 0, newprofile, &size );
    ok( !ret, "GetStandardColorSpaceProfileA() succeeded (%ld)\n", GetLastError() );

    size = 0;

    ret = pGetStandardColorSpaceProfileA( NULL, 0, NULL, &size );
    ok( !ret, "GetStandardColorSpaceProfileA() succeeded (%ld)\n", GetLastError() );

    size = sizeof(newprofile);

    ret = pGetStandardColorSpaceProfileA( NULL, 0, newprofile, &size );
    ok( !ret, "GetStandardColorSpaceProfileA() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    if (standardprofile)
    {
        size = sizeof(oldprofile);

        ret = pGetStandardColorSpaceProfileA( NULL, SPACE_RGB, oldprofile, &size );
        ok( ret, "GetStandardColorSpaceProfileA() failed (%ld)\n", GetLastError() );

        ret = pSetStandardColorSpaceProfileA( NULL, SPACE_RGB, standardprofile );
        ok( ret, "SetStandardColorSpaceProfileA() failed (%ld)\n", GetLastError() );

        size = sizeof(newprofile);

        ret = pGetStandardColorSpaceProfileA( NULL, SPACE_RGB, newprofile, &size );
        ok( ret, "GetStandardColorSpaceProfileA() failed (%ld)\n", GetLastError() );

        ok( !lstrcmpiA( (LPSTR)&newprofile, standardprofile ), "Unexpected profile\n" );

        ret = pSetStandardColorSpaceProfileA( NULL, SPACE_RGB, oldprofile );
        ok( ret, "SetStandardColorSpaceProfileA() failed (%ld)\n", GetLastError() );
    }
}

static void test_GetStandardColorSpaceProfileW(void)
{
    BOOL ret;
    DWORD size;
    WCHAR oldprofile[MAX_PATH];
    WCHAR newprofile[MAX_PATH];

    /* Parameter checks */

    ret = pGetStandardColorSpaceProfileW( NULL, 0, newprofile, NULL );
    ok( !ret, "GetStandardColorSpaceProfileW() succeeded (%ld)\n", GetLastError() );

    ret = pGetStandardColorSpaceProfileW( machineW, 0, newprofile, &size );
    ok( !ret, "GetStandardColorSpaceProfileW() succeeded (%ld)\n", GetLastError() );

    size = 0;

    ret = pGetStandardColorSpaceProfileW( NULL, 0, NULL, &size );
    ok( !ret, "GetStandardColorSpaceProfileW() succeeded (%ld)\n", GetLastError() );

    size = sizeof(newprofile);

    ret = pGetStandardColorSpaceProfileW( NULL, 0, newprofile, &size );
    ok( !ret, "GetStandardColorSpaceProfileW() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    if (standardprofileW)
    {
        size = sizeof(oldprofile);

        ret = pGetStandardColorSpaceProfileW( NULL, SPACE_RGB, oldprofile, &size );
        ok( ret, "GetStandardColorSpaceProfileW() failed (%ld)\n", GetLastError() );

        ret = pSetStandardColorSpaceProfileW( NULL, SPACE_RGB, standardprofileW );
        ok( ret, "SetStandardColorSpaceProfileW() failed (%ld)\n", GetLastError() );

        size = sizeof(newprofile);

        ret = pGetStandardColorSpaceProfileW( NULL, SPACE_RGB, newprofile, &size );
        ok( ret, "GetStandardColorSpaceProfileW() failed (%ld)\n", GetLastError() );

        ok( !lstrcmpiW( (LPWSTR)&newprofile, standardprofileW ), "Unexpected profile\n" );

        ret = pSetStandardColorSpaceProfileW( NULL, SPACE_RGB, oldprofile );
        ok( ret, "SetStandardColorSpaceProfileW() failed (%ld)\n", GetLastError() );
    }
}

static void test_InstallColorProfileA(void)
{
    BOOL ret;

    /* Parameter checks */

    ret = pInstallColorProfileA( NULL, NULL );
    ok( !ret, "InstallColorProfileA() succeeded (%ld)\n", GetLastError() );

    ret = pInstallColorProfileA( machine, NULL );
    ok( !ret, "InstallColorProfileA() succeeded (%ld)\n", GetLastError() );

    ret = pInstallColorProfileA( NULL, machine );
    ok( !ret, "InstallColorProfileA() succeeded (%ld)\n", GetLastError() );

    if (standardprofile)
    {
        ret = pInstallColorProfileA( NULL, standardprofile );
        ok( ret, "InstallColorProfileA() failed (%ld)\n", GetLastError() );
    }

    /* Functional checks */

    if (testprofile)
    {
        CHAR dest[MAX_PATH], base[MAX_PATH];
        DWORD size = sizeof(dest);
        CHAR slash[] = "\\";
        HANDLE handle;

        ret = pInstallColorProfileA( NULL, testprofile );
        ok( ret, "InstallColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pGetColorDirectoryA( NULL, dest, &size );
        ok( ret, "GetColorDirectoryA() failed (%ld)\n", GetLastError() );

        MSCMS_basenameA( testprofile, base );

        lstrcatA( dest, slash );
        lstrcatA( dest, base );

        /* Check if the profile is really there */ 
        handle = CreateFileA( dest, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );
        ok( handle != INVALID_HANDLE_VALUE, "Couldn't find the profile (%ld)\n", GetLastError() );
        CloseHandle( handle );
        
        ret = pUninstallColorProfileA( NULL, dest, TRUE );
        ok( ret, "UninstallColorProfileA() failed (%ld)\n", GetLastError() );
    }
}

static void test_InstallColorProfileW(void)
{
    BOOL ret;

    /* Parameter checks */

    ret = pInstallColorProfileW( NULL, NULL );
    ok( !ret, "InstallColorProfileW() succeeded (%ld)\n", GetLastError() );

    ret = pInstallColorProfileW( machineW, NULL );
    ok( !ret, "InstallColorProfileW() succeeded (%ld)\n", GetLastError() );

    ret = pInstallColorProfileW( NULL, machineW );
    ok( !ret, "InstallColorProfileW() failed (%ld)\n", GetLastError() );

    if (standardprofileW)
    {
        ret = pInstallColorProfileW( NULL, standardprofileW );
        ok( ret, "InstallColorProfileW() failed (%ld)\n", GetLastError() );
    }

    /* Functional checks */

    if (testprofileW)
    {
        WCHAR dest[MAX_PATH], base[MAX_PATH];
        DWORD size = sizeof(dest);
        WCHAR slash[] = { '\\', 0 };
        HANDLE handle;

        ret = pInstallColorProfileW( NULL, testprofileW );
        ok( ret, "InstallColorProfileW() failed (%ld)\n", GetLastError() );

        ret = pGetColorDirectoryW( NULL, dest, &size );
        ok( ret, "GetColorDirectoryW() failed (%ld)\n", GetLastError() );

        MSCMS_basenameW( testprofileW, base );

        lstrcatW( dest, slash );
        lstrcatW( dest, base );

        /* Check if the profile is really there */
        handle = CreateFileW( dest, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );
        ok( handle != INVALID_HANDLE_VALUE, "Couldn't find the profile (%ld)\n", GetLastError() );
        CloseHandle( handle );

        ret = pUninstallColorProfileW( NULL, dest, TRUE );
        ok( ret, "UninstallColorProfileW() failed (%ld)\n", GetLastError() );
    }
}

static void test_IsColorProfileTagPresent(void)
{
    if (standardprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret, present;
        TAGTYPE tag;

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = standardprofile;
        profile.cbDataSize = strlen(standardprofile);

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Parameter checks */

        tag = 0;

        ret = pIsColorProfileTagPresent( handle, tag, &present );
        ok( !(ret && present), "IsColorProfileTagPresent() succeeded (%ld)\n", GetLastError() );

        tag = 0x63707274;  /* 'cprt' */

        ret = pIsColorProfileTagPresent( NULL, tag, &present );
        ok( !ret, "IsColorProfileTagPresent() succeeded (%ld)\n", GetLastError() );

        ret = pIsColorProfileTagPresent( handle, tag, NULL );
        ok( !ret, "IsColorProfileTagPresent() succeeded (%ld)\n", GetLastError() );

        /* Functional checks */

        ret = pIsColorProfileTagPresent( handle, tag, &present );
        ok( ret && present, "IsColorProfileTagPresent() failed (%ld)\n", GetLastError() );

        pCloseColorProfile( handle );
    }
}

static void test_OpenColorProfileA(void)
{
    PROFILE profile;
    HPROFILE handle;
    BOOL ret;

    profile.dwType = PROFILE_FILENAME;
    profile.pProfileData = NULL;
    profile.cbDataSize = 0;

    /* Parameter checks */

    handle = pOpenColorProfileA( NULL, 0, 0, 0 );
    ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileA( &profile, 0, 0, 0 );
    ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, 0 );
    ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileA( &profile, PROFILE_READWRITE, 0, 0 );
    ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

    ok ( !pCloseColorProfile( NULL ), "CloseColorProfile() succeeded\n" );

    if (standardprofile)
    {
        profile.pProfileData = standardprofile;
        profile.cbDataSize = strlen(standardprofile);

        handle = pOpenColorProfileA( &profile, 0, 0, 0 );
        ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, 0 );
        ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        handle = pOpenColorProfileA( &profile, PROFILE_READ|PROFILE_READWRITE, 0, 0 );
        ok( handle == NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        /* Functional checks */

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pCloseColorProfile( handle );
        ok( ret, "CloseColorProfile() failed (%ld)\n", GetLastError() );
    }
}

static void test_OpenColorProfileW(void)
{
    PROFILE profile;
    HPROFILE handle;
    BOOL ret;

    profile.dwType = PROFILE_FILENAME;
    profile.pProfileData = NULL;
    profile.cbDataSize = 0;

    /* Parameter checks */

    handle = pOpenColorProfileW( NULL, 0, 0, 0 );
    ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileW( &profile, 0, 0, 0 );
    ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileW( &profile, PROFILE_READ, 0, 0 );
    ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

    handle = pOpenColorProfileW( &profile, PROFILE_READWRITE, 0, 0 );
    ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

    ok ( !pCloseColorProfile( NULL ), "CloseColorProfile() succeeded\n" );

    if (standardprofileW)
    {
        profile.pProfileData = standardprofileW;
        profile.cbDataSize = lstrlenW(standardprofileW) * sizeof(WCHAR);

        handle = pOpenColorProfileW( &profile, 0, 0, 0 );
        ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

        handle = pOpenColorProfileW( &profile, PROFILE_READ, 0, 0 );
        ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

        handle = pOpenColorProfileW( &profile, PROFILE_READ|PROFILE_READWRITE, 0, 0 );
        ok( handle == NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

        /* Functional checks */

        handle = pOpenColorProfileW( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileW() failed (%ld)\n", GetLastError() );

        ret = pCloseColorProfile( handle );
        ok( ret, "CloseColorProfile() failed (%ld)\n", GetLastError() );
    }
}

static void test_SetColorProfileElement(void)
{
    if (testprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        DWORD size;
        BOOL ret, ref;

        TAGTYPE tag = 0x63707274;  /* 'cprt' */
        static char data[] = "(c) The Wine Project";
        static char buffer[51];

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = testprofile;
        profile.cbDataSize = strlen(testprofile);

        /* Parameter checks */

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pSetColorProfileElement( handle, tag, 0, &size, data );
        ok( !ret, "SetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        pCloseColorProfile( handle );

        handle = pOpenColorProfileA( &profile, PROFILE_READWRITE, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pSetColorProfileElement( NULL, 0, 0, NULL, NULL );
        ok( !ret, "SetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        ret = pSetColorProfileElement( handle, 0, 0, NULL, NULL );
        ok( !ret, "SetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        ret = pSetColorProfileElement( handle, tag, 0, NULL, NULL );
        ok( !ret, "SetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        ret = pSetColorProfileElement( handle, tag, 0, &size, NULL );
        ok( !ret, "SetColorProfileElement() succeeded (%ld)\n", GetLastError() );

        /* Functional checks */

        size = sizeof(data);

        ret = pSetColorProfileElement( handle, tag, 0, &size, data );
        ok( ret, "SetColorProfileElement() failed (%ld)\n", GetLastError() );

        size = sizeof(buffer);

        ret = pGetColorProfileElement( handle, tag, 0, &size, buffer, &ref );
        ok( ret && size > 0, "GetColorProfileElement() failed (%ld)\n", GetLastError() );

        ok( !memcmp( data, buffer, sizeof(data) ),
            "Unexpected tag data, expected %s, got %s (%ld)\n",
            data, buffer, GetLastError() );

        pCloseColorProfile( handle );
    }
}

static void test_SetColorProfileHeader(void)
{
    if (testprofile)
    {
        PROFILE profile;
        HPROFILE handle;
        BOOL ret;
        PROFILEHEADER header;

        profile.dwType = PROFILE_FILENAME;
        profile.pProfileData = testprofile;
        profile.cbDataSize = strlen(testprofile);

        header.phSize = 0x00000c48;
        header.phCMMType = 0x4c696e6f;
        header.phVersion = 0x02100000;
        header.phClass = 0x6d6e7472;
        header.phDataColorSpace = 0x52474220;
        header.phConnectionSpace  = 0x58595a20;
        header.phDateTime[0] = 0x07ce0002;
        header.phDateTime[1] = 0x00090006;
        header.phDateTime[2] = 0x00310000;
        header.phSignature = 0x61637370;
        header.phPlatform = 0x4d534654;
        header.phProfileFlags = 0x00000000;
        header.phManufacturer = 0x49454320;
        header.phModel = 0x73524742;
        header.phAttributes[0] = 0x00000000;
        header.phAttributes[1] = 0x00000000;
        header.phRenderingIntent = 0x00000000;
        header.phIlluminant.ciexyzX = 0x0000f6d6;
        header.phIlluminant.ciexyzY = 0x00010000;
        header.phIlluminant.ciexyzZ = 0x0000d32d;
        header.phCreator = 0x48502020;

        /* Parameter checks */

        handle = pOpenColorProfileA( &profile, PROFILE_READ, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pSetColorProfileHeader( handle, &header );
        ok( !ret, "SetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        pCloseColorProfile( handle );

        handle = pOpenColorProfileA( &profile, PROFILE_READWRITE, 0, OPEN_EXISTING );
        ok( handle != NULL, "OpenColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pSetColorProfileHeader( NULL, NULL );
        ok( !ret, "SetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        ret = pSetColorProfileHeader( handle, NULL );
        ok( !ret, "SetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        ret = pSetColorProfileHeader( NULL, &header );
        ok( !ret, "SetColorProfileHeader() succeeded (%ld)\n", GetLastError() );

        /* Functional checks */

        ret = pSetColorProfileHeader( handle, &header );
        ok( ret, "SetColorProfileHeader() failed (%ld)\n", GetLastError() );

        ret = pGetColorProfileHeader( handle, &header );
        ok( ret, "GetColorProfileHeader() failed (%ld)\n", GetLastError() );

        ok( !memcmp( &header, rgbheader, sizeof(rgbheader) ), "Unexpected header data\n" );

        pCloseColorProfile( handle );
    }
}

static void test_UninstallColorProfileA(void)
{
    BOOL ret;

    /* Parameter checks */

    ret = pUninstallColorProfileA( NULL, NULL, FALSE );
    ok( !ret, "UninstallColorProfileA() succeeded (%ld)\n", GetLastError() );

    ret = pUninstallColorProfileA( machine, NULL, FALSE );
    ok( !ret, "UninstallColorProfileA() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    if (testprofile)
    {
        CHAR dest[MAX_PATH], base[MAX_PATH];
        DWORD size = sizeof(dest);
        CHAR slash[] = "\\";
        HANDLE handle;

        ret = pInstallColorProfileA( NULL, testprofile );
        ok( ret, "InstallColorProfileA() failed (%ld)\n", GetLastError() );

        ret = pGetColorDirectoryA( NULL, dest, &size );
        ok( ret, "GetColorDirectoryA() failed (%ld)\n", GetLastError() );

        MSCMS_basenameA( testprofile, base );

        lstrcatA( dest, slash );
        lstrcatA( dest, base );

        ret = pUninstallColorProfileA( NULL, dest, TRUE );
        ok( ret, "UninstallColorProfileA() failed (%ld)\n", GetLastError() );

        /* Check if the profile is really gone */
        handle = CreateFileA( dest, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );
        ok( handle == INVALID_HANDLE_VALUE, "Found the profile (%ld)\n", GetLastError() );
        CloseHandle( handle );
    }
}

static void test_UninstallColorProfileW(void)
{
    BOOL ret;

    /* Parameter checks */

    ret = pUninstallColorProfileW( NULL, NULL, FALSE );
    ok( !ret, "UninstallColorProfileW() succeeded (%ld)\n", GetLastError() );

    ret = pUninstallColorProfileW( machineW, NULL, FALSE );
    ok( !ret, "UninstallColorProfileW() succeeded (%ld)\n", GetLastError() );

    /* Functional checks */

    if (testprofileW)
    {
        WCHAR dest[MAX_PATH], base[MAX_PATH];
        DWORD size = sizeof(dest);
        WCHAR slash[] = { '\\', 0 };
        HANDLE handle;

        ret = pInstallColorProfileW( NULL, testprofileW );
        ok( ret, "InstallColorProfileW() failed (%ld)\n", GetLastError() );

        ret = pGetColorDirectoryW( NULL, dest, &size );
        ok( ret, "GetColorDirectoryW() failed (%ld)\n", GetLastError() );

        MSCMS_basenameW( testprofileW, base );

        lstrcatW( dest, slash );
        lstrcatW( dest, base );

        ret = pUninstallColorProfileW( NULL, dest, TRUE );
        ok( ret, "UninstallColorProfileW() failed (%ld)\n", GetLastError() );

        /* Check if the profile is really gone */
        handle = CreateFileW( dest, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );
        ok( handle == INVALID_HANDLE_VALUE, "Found the profile (%ld)\n", GetLastError() );
        CloseHandle( handle );
    }
}

START_TEST(profile)
{
    UINT len;
    HANDLE handle;
    char path[MAX_PATH], file[MAX_PATH];
    char profilefile1[MAX_PATH], profilefile2[MAX_PATH];
    WCHAR profilefile1W[MAX_PATH], profilefile2W[MAX_PATH];
    WCHAR fileW[MAX_PATH];

    hmscms = LoadLibraryA( "mscms.dll" );
    if (!hmscms) return;

    if (!init_function_ptrs())
    {
        FreeLibrary( hmscms );
        return;
    }

    /* See if we can find the standard color profile */
    GetSystemDirectoryA( profilefile1, sizeof(profilefile1) );
    GetSystemDirectoryW( profilefile1W, sizeof(profilefile1W) / sizeof(WCHAR) );
    lstrcatA( profilefile1, profile1 );
    lstrcatW( profilefile1W, profile1W );
    handle = CreateFileA( profilefile1, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );

    if (handle != INVALID_HANDLE_VALUE)
    {
        standardprofile = profilefile1;
        standardprofileW = profilefile1W;
        CloseHandle( handle );
    }

    GetSystemDirectoryA( profilefile2, sizeof(profilefile2) );
    GetSystemDirectoryW( profilefile2W, sizeof(profilefile2W) / sizeof(WCHAR) );
    lstrcatA( profilefile2, profile2 );
    lstrcatW( profilefile2W, profile2W );
    handle = CreateFileA( profilefile2, 0 , 0, NULL, OPEN_EXISTING, 0, NULL );

    if (handle != INVALID_HANDLE_VALUE)
    {
        standardprofile = profilefile2;
        standardprofileW = profilefile2W;
        CloseHandle( handle );
    }

    /* If found, create a temporary copy for testing purposes */
    if (standardprofile && GetTempPath( sizeof(path), path ))
    {
        if (GetTempFileName( path, "rgb", 0, file ))
        {
            if (CopyFileA( standardprofile, file, FALSE ))
            {
                testprofile = (LPSTR)&file;

                len = MultiByteToWideChar( CP_ACP, 0, testprofile, -1, NULL, 0 );
                MultiByteToWideChar( CP_ACP, 0, testprofile, -1, fileW, len );

                testprofileW = (LPWSTR)&fileW;
            }
        }
    }

    test_GetColorDirectoryA();
    test_GetColorDirectoryW();

    test_GetColorProfileElement();
    test_GetColorProfileElementTag();

    test_GetColorProfileFromHandle();
    test_GetColorProfileHeader();

    test_GetCountColorProfileElements();

    test_GetStandardColorSpaceProfileA();
    test_GetStandardColorSpaceProfileW();

    test_InstallColorProfileA();
    test_InstallColorProfileW();

    test_IsColorProfileTagPresent();

    test_OpenColorProfileA();
    test_OpenColorProfileW();

    test_SetColorProfileElement();
    test_SetColorProfileHeader();

    test_UninstallColorProfileA();
    test_UninstallColorProfileW();

    /* Clean up */
    if (testprofile)
        DeleteFileA( testprofile );
    
    FreeLibrary( hmscms );
}
