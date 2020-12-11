/*
 * Registry management
 *
 * Copyright (C) 1999 Alexandre Julliard
 *
 * Based on misc/registry.c code
 * Copyright (C) 1996 Marcus Meissner
 * Copyright (C) 1998 Matthew Becker
 * Copyright (C) 1999 Sylvain St-Germain
 *
 * This file is concerned about handle management and interaction with the Wine server.
 * Registry file I/O is in misc/registry.c.
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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winerror.h"
#include "ntstatus.h"
#include "winternl.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(reg);

/* allowed bits for access mask */
#define KEY_ACCESS_MASK (KEY_ALL_ACCESS | MAXIMUM_ALLOWED)

#define HKEY_SPECIAL_ROOT_FIRST   HKEY_CLASSES_ROOT
#define HKEY_SPECIAL_ROOT_LAST    HKEY_DYN_DATA
#define NB_SPECIAL_ROOT_KEYS      ((UINT)HKEY_SPECIAL_ROOT_LAST - (UINT)HKEY_SPECIAL_ROOT_FIRST + 1)

static HKEY special_root_keys[NB_SPECIAL_ROOT_KEYS];

static const WCHAR name_CLASSES_ROOT[] =
    {'M','a','c','h','i','n','e','\\',
     'S','o','f','t','w','a','r','e','\\',
     'C','l','a','s','s','e','s',0};
static const WCHAR name_LOCAL_MACHINE[] =
    {'M','a','c','h','i','n','e',0};
static const WCHAR name_USERS[] =
    {'U','s','e','r',0};
static const WCHAR name_PERFORMANCE_DATA[] =
    {'P','e','r','f','D','a','t','a',0};
static const WCHAR name_CURRENT_CONFIG[] =
    {'M','a','c','h','i','n','e','\\',
     'S','y','s','t','e','m','\\',
     'C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t','\\',
     'H','a','r','d','w','a','r','e',' ','P','r','o','f','i','l','e','s','\\',
     'C','u','r','r','e','n','t',0};
static const WCHAR name_DYN_DATA[] =
    {'D','y','n','D','a','t','a',0};

#define DECL_STR(key) { sizeof(name_##key)-sizeof(WCHAR), sizeof(name_##key), (LPWSTR)name_##key }
static UNICODE_STRING root_key_names[NB_SPECIAL_ROOT_KEYS] =
{
    DECL_STR(CLASSES_ROOT),
    { 0, 0, NULL },         /* HKEY_CURRENT_USER is determined dynamically */
    DECL_STR(LOCAL_MACHINE),
    DECL_STR(USERS),
    DECL_STR(PERFORMANCE_DATA),
    DECL_STR(CURRENT_CONFIG),
    DECL_STR(DYN_DATA)
};
#undef DECL_STR


/* check if value type needs string conversion (Ansi<->Unicode) */
inline static int is_string( DWORD type )
{
    return (type == REG_SZ) || (type == REG_EXPAND_SZ) || (type == REG_MULTI_SZ);
}

/* check if current version is NT or Win95 */
inline static int is_version_nt(void)
{
    return !(GetVersion() & 0x80000000);
}

/* create one of the HKEY_* special root keys */
static HKEY create_special_root_hkey( HANDLE hkey, DWORD access )
{
    HKEY ret = 0;
    int idx = (UINT_PTR)hkey - (UINT_PTR)HKEY_SPECIAL_ROOT_FIRST;

    if (hkey == HKEY_CURRENT_USER)
    {
        if (RtlOpenCurrentUser( access, &hkey )) return 0;
        TRACE( "HKEY_CURRENT_USER -> %p\n", hkey );
    }
    else
    {
        OBJECT_ATTRIBUTES attr;

        attr.Length = sizeof(attr);
        attr.RootDirectory = 0;
        attr.ObjectName = &root_key_names[idx];
        attr.Attributes = 0;
        attr.SecurityDescriptor = NULL;
        attr.SecurityQualityOfService = NULL;
        if (NtCreateKey( &hkey, access, &attr, 0, NULL, 0, NULL )) return 0;
        TRACE( "%s -> %p\n", debugstr_w(attr.ObjectName->Buffer), hkey );
    }

    if (!(ret = InterlockedCompareExchangePointer( (void **)&special_root_keys[idx], hkey, 0 )))
        ret = hkey;
    else
        NtClose( hkey );  /* somebody beat us to it */
    return ret;
}

/* map the hkey from special root to normal key if necessary */
inline static HKEY get_special_root_hkey( HKEY hkey )
{
    HKEY ret = hkey;

    if ((hkey >= HKEY_SPECIAL_ROOT_FIRST) && (hkey <= HKEY_SPECIAL_ROOT_LAST))
    {
        if (!(ret = special_root_keys[(UINT_PTR)hkey - (UINT_PTR)HKEY_SPECIAL_ROOT_FIRST]))
            ret = create_special_root_hkey( hkey, KEY_ALL_ACCESS );
    }
    return ret;
}


/******************************************************************************
 * RegCreateKeyExW   [ADVAPI32.@]
 *
 * See RegCreateKeyExA.
 */
DWORD WINAPI RegCreateKeyExW( HKEY hkey, LPCWSTR name, DWORD reserved, LPWSTR class,
                              DWORD options, REGSAM access, SECURITY_ATTRIBUTES *sa,
                              PHKEY retkey, LPDWORD dispos )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW, classW;

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!(access & KEY_ACCESS_MASK) || (access & ~KEY_ACCESS_MASK)) return ERROR_ACCESS_DENIED;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, name );
    RtlInitUnicodeString( &classW, class );

    return RtlNtStatusToDosError( NtCreateKey( (PHANDLE)retkey, access, &attr, 0,
                                               &classW, options, dispos ) );
}


/******************************************************************************
 * RegCreateKeyExA   [ADVAPI32.@]
 *
 * Open a registry key, creating it if it doesn't exist.
 *
 * PARAMS
 *  hkey       [I] Handle of the parent registry key
 *  name       [I] Name of the new key to open or create
 *  reserved   [I] Reserved, pass 0
 *  class      [I] The object type of the new key
 *  options    [I] Flags controlling the key creation (REG_OPTION_* flags from "winnt.h")
 *  access     [I] Access level desired
 *  sa         [I] Security attributes for the key
 *  retkey     [O] Destination for the resulting handle
 *  dispos     [O] Receives REG_CREATED_NEW_KEY or REG_OPENED_EXISTING_KEY
 *
 * RETURNS
 *  Success: ERROR_SUCCESS.
 *  Failure: A standard Win32 error code. retkey remains untouched.
 *
 * FIXME
 *  MAXIMUM_ALLOWED in access mask not supported by server
 */
DWORD WINAPI RegCreateKeyExA( HKEY hkey, LPCSTR name, DWORD reserved, LPSTR class,
                              DWORD options, REGSAM access, SECURITY_ATTRIBUTES *sa,
                              PHKEY retkey, LPDWORD dispos )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING classW;
    ANSI_STRING nameA, classA;
    NTSTATUS status;

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!is_version_nt()) access = KEY_ALL_ACCESS;  /* Win95 ignores the access mask */
    else if (!(access & KEY_ACCESS_MASK) || (access & ~KEY_ACCESS_MASK)) return ERROR_ACCESS_DENIED;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &NtCurrentTeb()->StaticUnicodeString;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitAnsiString( &nameA, name );
    RtlInitAnsiString( &classA, class );

    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        if (!(status = RtlAnsiStringToUnicodeString( &classW, &classA, TRUE )))
        {
            status = NtCreateKey( (PHANDLE)retkey, access, &attr, 0, &classW, options, dispos );
            RtlFreeUnicodeString( &classW );
        }
    }
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegCreateKeyW   [ADVAPI32.@]
 *
 * Creates the specified reg key.
 *
 * PARAMS
 *  hKey      [I] Handle to an open key.
 *  lpSubKey  [I] Name of a key that will be opened or created.
 *  phkResult [O] Receives a handle to the opened or created key.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code defined in Winerror.h
 */
DWORD WINAPI RegCreateKeyW( HKEY hkey, LPCWSTR lpSubKey, PHKEY phkResult )
{
    /* FIXME: previous implementation converted ERROR_INVALID_HANDLE to ERROR_BADKEY, */
    /* but at least my version of NT (4.0 SP5) doesn't do this.  -- AJ */
    return RegCreateKeyExW( hkey, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS, NULL, phkResult, NULL );
}


/******************************************************************************
 * RegCreateKeyA   [ADVAPI32.@]
 *
 * See RegCreateKeyW.
 */
DWORD WINAPI RegCreateKeyA( HKEY hkey, LPCSTR lpSubKey, PHKEY phkResult )
{
    return RegCreateKeyExA( hkey, lpSubKey, 0, NULL, REG_OPTION_NON_VOLATILE,
                            KEY_ALL_ACCESS, NULL, phkResult, NULL );
}



/******************************************************************************
 * RegOpenKeyExW   [ADVAPI32.@]
 * 
 * See RegOpenKeyExA.
 */
DWORD WINAPI RegOpenKeyExW( HKEY hkey, LPCWSTR name, DWORD reserved, REGSAM access, PHKEY retkey )
{
    OBJECT_ATTRIBUTES attr;
    UNICODE_STRING nameW;

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;
    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtOpenKey( (PHANDLE)retkey, access, &attr ) );
}


/******************************************************************************
 * RegOpenKeyExA   [ADVAPI32.@]
 *
 * Open a registry key.
 *
 * PARAMS
 *  hkey       [I] Handle of open key
 *  name       [I] Name of subkey to open
 *  reserved   [I] Reserved - must be zero
 *  access     [I] Security access mask
 *  retkey     [O] Handle to open key
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: A standard Win32 error code. retkey is set to 0.
 *
 * NOTES
 *  Unlike RegCreateKeyExA(), this function will not create the key if it
 *  does not exist.
 */
DWORD WINAPI RegOpenKeyExA( HKEY hkey, LPCSTR name, DWORD reserved, REGSAM access, PHKEY retkey )
{
    OBJECT_ATTRIBUTES attr;
    STRING nameA;
    NTSTATUS status;

    if (!is_version_nt()) access = KEY_ALL_ACCESS;  /* Win95 ignores the access mask */

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    attr.Length = sizeof(attr);
    attr.RootDirectory = hkey;
    attr.ObjectName = &NtCurrentTeb()->StaticUnicodeString;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        status = NtOpenKey( (PHANDLE)retkey, access, &attr );
    }
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegOpenKeyW   [ADVAPI32.@]
 *
 * See RegOpenKeyA.
 */
DWORD WINAPI RegOpenKeyW( HKEY hkey, LPCWSTR name, PHKEY retkey )
{
    if (!name || !*name)
    {
        *retkey = hkey;
        return ERROR_SUCCESS;
    }
    return RegOpenKeyExW( hkey, name, 0, KEY_ALL_ACCESS, retkey );
}


/******************************************************************************
 * RegOpenKeyA   [ADVAPI32.@]
 *           
 * Open a registry key.
 *
 * PARAMS
 *  hkey    [I] Handle of parent key to open the new key under
 *  name    [I] Name of the key under hkey to open
 *  retkey  [O] Destination for the resulting Handle
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: A standard Win32 error code. retkey is set to 0.
 */
DWORD WINAPI RegOpenKeyA( HKEY hkey, LPCSTR name, PHKEY retkey )
{
    if (!name || !*name)
    {
        *retkey = hkey;
        return ERROR_SUCCESS;
    }
    return RegOpenKeyExA( hkey, name, 0, KEY_ALL_ACCESS, retkey );
}


/******************************************************************************
 * RegOpenCurrentUser   [ADVAPI32.@]
 * 
 * FIXME: This function is supposed to retrieve a handle to the
 * HKEY_CURRENT_USER for the user the current thread is impersonating.
 * Since Wine does not currently allow threads to impersonate other users,
 * this stub should work fine.
 */
DWORD WINAPI RegOpenCurrentUser( REGSAM access, PHKEY retkey )
{
    return RegOpenKeyExA( HKEY_CURRENT_USER, "", 0, access, retkey );
}



/******************************************************************************
 * RegEnumKeyExW   [ADVAPI32.@]
 *
 * PARAMS
 *  hkey         [I] Handle to key to enumerate
 *  index        [I] Index of subkey to enumerate
 *  name         [O] Buffer for subkey name
 *  name_len     [O] Size of subkey buffer
 *  reserved     [I] Reserved
 *  class        [O] Buffer for class string
 *  class_len    [O] Size of class buffer
 *  ft           [O] Time key last written to
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: System error code. If there are no more subkeys available, the
 *           function returns ERROR_NO_MORE_ITEMS.
 */
DWORD WINAPI RegEnumKeyExW( HKEY hkey, DWORD index, LPWSTR name, LPDWORD name_len,
                            LPDWORD reserved, LPWSTR class, LPDWORD class_len, FILETIME *ft )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_NODE_INFORMATION *info = (KEY_NODE_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(%p,%ld,%p,%p(%ld),%p,%p,%p,%p)\n", hkey, index, name, name_len,
           name_len ? *name_len : -1, reserved, class, class_len, ft );

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                             buffer, sizeof(buffer), &total_size );

    while (status == STATUS_BUFFER_OVERFLOW)
    {
        /* retry with a dynamically allocated buffer */
        if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
        if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            return ERROR_NOT_ENOUGH_MEMORY;
        info = (KEY_NODE_INFORMATION *)buf_ptr;
        status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                                 buf_ptr, total_size, &total_size );
    }

    if (!status)
    {
        DWORD len = info->NameLength / sizeof(WCHAR);
        DWORD cls_len = info->ClassLength / sizeof(WCHAR);

        if (ft) *ft = *(FILETIME *)&info->LastWriteTime;

        if (len >= *name_len || (class && class_len && (cls_len >= *class_len)))
            status = STATUS_BUFFER_OVERFLOW;
        else
        {
            *name_len = len;
            memcpy( name, info->Name, info->NameLength );
            name[len] = 0;
            if (class_len)
            {
                *class_len = cls_len;
                if (class)
                {
                    memcpy( class, buf_ptr + info->ClassOffset, info->ClassLength );
                    class[cls_len] = 0;
                }
            }
        }
    }

    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegEnumKeyExA   [ADVAPI32.@]
 *
 * See RegEnumKeyExW.
 */
DWORD WINAPI RegEnumKeyExA( HKEY hkey, DWORD index, LPSTR name, LPDWORD name_len,
                            LPDWORD reserved, LPSTR class, LPDWORD class_len, FILETIME *ft )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_NODE_INFORMATION *info = (KEY_NODE_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(%p,%ld,%p,%p(%ld),%p,%p,%p,%p)\n", hkey, index, name, name_len,
           name_len ? *name_len : -1, reserved, class, class_len, ft );

    if (reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                             buffer, sizeof(buffer), &total_size );

    while (status == STATUS_BUFFER_OVERFLOW)
    {
        /* retry with a dynamically allocated buffer */
        if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
        if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            return ERROR_NOT_ENOUGH_MEMORY;
        info = (KEY_NODE_INFORMATION *)buf_ptr;
        status = NtEnumerateKey( hkey, index, KeyNodeInformation,
                                 buf_ptr, total_size, &total_size );
    }

    if (!status)
    {
        DWORD len, cls_len;

        RtlUnicodeToMultiByteSize( &len, info->Name, info->NameLength );
        RtlUnicodeToMultiByteSize( &cls_len, (WCHAR *)(buf_ptr + info->ClassOffset),
                                   info->ClassLength );
        if (ft) *ft = *(FILETIME *)&info->LastWriteTime;

        if (len >= *name_len || (class && class_len && (cls_len >= *class_len)))
            status = STATUS_BUFFER_OVERFLOW;
        else
        {
            *name_len = len;
            RtlUnicodeToMultiByteN( name, len, NULL, info->Name, info->NameLength );
            name[len] = 0;
            if (class_len)
            {
                *class_len = cls_len;
                if (class)
                {
                    RtlUnicodeToMultiByteN( class, cls_len, NULL,
                                            (WCHAR *)(buf_ptr + info->ClassOffset),
                                            info->ClassLength );
                    class[cls_len] = 0;
                }
            }
        }
    }

    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegEnumKeyW   [ADVAPI32.@]
 *
 * Enumerates subkyes of the specified open reg key.
 *
 * PARAMS
 *  hKey    [I] Handle to an open key.
 *  dwIndex [I] Index of the subkey of hKey to retrieve.
 *  lpName  [O] Name of the subkey.
 *  cchName [I] Size of lpName in TCHARS.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: system error code. If there are no more subkeys available, the
 *           function returns ERROR_NO_MORE_ITEMS.
 */
DWORD WINAPI RegEnumKeyW( HKEY hkey, DWORD index, LPWSTR name, DWORD name_len )
{
    return RegEnumKeyExW( hkey, index, name, &name_len, NULL, NULL, NULL, NULL );
}


/******************************************************************************
 * RegEnumKeyA   [ADVAPI32.@]
 *
 * See RegEnumKeyW.
 */
DWORD WINAPI RegEnumKeyA( HKEY hkey, DWORD index, LPSTR name, DWORD name_len )
{
    return RegEnumKeyExA( hkey, index, name, &name_len, NULL, NULL, NULL, NULL );
}


/******************************************************************************
 * RegQueryInfoKeyW   [ADVAPI32.@]
 *
 * Retrieves information about the specified registry key.
 *
 * PARAMS
 *    hkey       [I] Handle to key to query
 *    class      [O] Buffer for class string
 *    class_len  [O] Size of class string buffer
 *    reserved   [I] Reserved
 *    subkeys    [O] Buffer for number of subkeys
 *    max_subkey [O] Buffer for longest subkey name length
 *    max_class  [O] Buffer for longest class string length
 *    values     [O] Buffer for number of value entries
 *    max_value  [O] Buffer for longest value name length
 *    max_data   [O] Buffer for longest value data length
 *    security   [O] Buffer for security descriptor length
 *    modif      [O] Modification time
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: system error code.
 *
 * NOTES
 *  - win95 allows class to be valid and class_len to be NULL
 *  - winnt returns ERROR_INVALID_PARAMETER if class is valid and class_len is NULL
 *  - both allow class to be NULL and class_len to be NULL
 *    (it's hard to test validity, so test !NULL instead)
 */
DWORD WINAPI RegQueryInfoKeyW( HKEY hkey, LPWSTR class, LPDWORD class_len, LPDWORD reserved,
                               LPDWORD subkeys, LPDWORD max_subkey, LPDWORD max_class,
                               LPDWORD values, LPDWORD max_value, LPDWORD max_data,
                               LPDWORD security, FILETIME *modif )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_FULL_INFORMATION *info = (KEY_FULL_INFORMATION *)buffer;
    DWORD total_size;

    TRACE( "(%p,%p,%ld,%p,%p,%p,%p,%p,%p,%p,%p)\n", hkey, class, class_len ? *class_len : 0,
           reserved, subkeys, max_subkey, values, max_value, max_data, security, modif );

    if (class && !class_len && is_version_nt()) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    status = NtQueryKey( hkey, KeyFullInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (class)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_FULL_INFORMATION *)buf_ptr;
            status = NtQueryKey( hkey, KeyFullInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (class_len && (info->ClassLength/sizeof(WCHAR) + 1 > *class_len))
        {
            status = STATUS_BUFFER_OVERFLOW;
        }
        else
        {
            memcpy( class, buf_ptr + info->ClassOffset, info->ClassLength );
            class[info->ClassLength/sizeof(WCHAR)] = 0;
        }
    }
    else status = STATUS_SUCCESS;

    if (class_len) *class_len = info->ClassLength / sizeof(WCHAR);
    if (subkeys) *subkeys = info->SubKeys;
    if (max_subkey) *max_subkey = info->MaxNameLen;
    if (max_class) *max_class = info->MaxClassLen;
    if (values) *values = info->Values;
    if (max_value) *max_value = info->MaxValueNameLen;
    if (max_data) *max_data = info->MaxValueDataLen;
    if (modif) *modif = *(FILETIME *)&info->LastWriteTime;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegQueryMultipleValuesA   [ADVAPI32.@]
 *
 * Retrieves the type and data for a list of value names associated with a key.
 *
 * PARAMS
 *  hKey       [I] Handle to an open key.
 *  val_list   [O] Array of VALENT structures that describes the entries.
 *  num_vals   [I] Number of elements in val_list.
 *  lpValueBuf [O] Pointer to a buffer that receives the data for each value.
 *  ldwTotsize [I/O] Size of lpValueBuf.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS. ldwTotsize contains num bytes copied.
 *  Failure: nonzero error code from Winerror.h ldwTotsize contains num needed
 *           bytes.
 */
DWORD WINAPI RegQueryMultipleValuesA(HKEY hkey, PVALENTA val_list, DWORD num_vals,
                                     LPSTR lpValueBuf, LPDWORD ldwTotsize)
{
    unsigned int i;
    DWORD maxBytes = *ldwTotsize;
    HRESULT status;
    LPSTR bufptr = lpValueBuf;
    *ldwTotsize = 0;

    TRACE("(%p,%p,%ld,%p,%p=%ld)\n", hkey, val_list, num_vals, lpValueBuf, ldwTotsize, *ldwTotsize);

    for(i=0; i < num_vals; ++i)
    {

        val_list[i].ve_valuelen=0;
        status = RegQueryValueExA(hkey, val_list[i].ve_valuename, NULL, NULL, NULL, &val_list[i].ve_valuelen);
        if(status != ERROR_SUCCESS)
        {
            return status;
        }

        if(lpValueBuf != NULL && *ldwTotsize + val_list[i].ve_valuelen <= maxBytes)
        {
            status = RegQueryValueExA(hkey, val_list[i].ve_valuename, NULL, &val_list[i].ve_type,
                                      (LPBYTE)bufptr, &val_list[i].ve_valuelen);
            if(status != ERROR_SUCCESS)
            {
                return status;
            }

            val_list[i].ve_valueptr = (DWORD_PTR)bufptr;

            bufptr += val_list[i].ve_valuelen;
        }

        *ldwTotsize += val_list[i].ve_valuelen;
    }
    return lpValueBuf != NULL && *ldwTotsize <= maxBytes ? ERROR_SUCCESS : ERROR_MORE_DATA;
}


/******************************************************************************
 * RegQueryMultipleValuesW   [ADVAPI32.@]
 *
 * See RegQueryMultipleValuesA.
 */
DWORD WINAPI RegQueryMultipleValuesW(HKEY hkey, PVALENTW val_list, DWORD num_vals,
                                     LPWSTR lpValueBuf, LPDWORD ldwTotsize)
{
    unsigned int i;
    DWORD maxBytes = *ldwTotsize;
    HRESULT status;
    LPSTR bufptr = (LPSTR)lpValueBuf;
    *ldwTotsize = 0;

    TRACE("(%p,%p,%ld,%p,%p=%ld)\n", hkey, val_list, num_vals, lpValueBuf, ldwTotsize, *ldwTotsize);

    for(i=0; i < num_vals; ++i)
    {
        val_list[i].ve_valuelen=0;
        status = RegQueryValueExW(hkey, val_list[i].ve_valuename, NULL, NULL, NULL, &val_list[i].ve_valuelen);
        if(status != ERROR_SUCCESS)
        {
            return status;
        }

        if(lpValueBuf != NULL && *ldwTotsize + val_list[i].ve_valuelen <= maxBytes)
        {
            status = RegQueryValueExW(hkey, val_list[i].ve_valuename, NULL, &val_list[i].ve_type,
                                      (LPBYTE)bufptr, &val_list[i].ve_valuelen);
            if(status != ERROR_SUCCESS)
            {
                return status;
            }

            val_list[i].ve_valueptr = (DWORD_PTR)bufptr;

            bufptr += val_list[i].ve_valuelen;
        }

        *ldwTotsize += val_list[i].ve_valuelen;
    }
    return lpValueBuf != NULL && *ldwTotsize <= maxBytes ? ERROR_SUCCESS : ERROR_MORE_DATA;
}

/******************************************************************************
 * RegQueryInfoKeyA   [ADVAPI32.@]
 *
 * Retrieves information about a registry key.
 *
 * PARAMS
 *  hKey                   [I] Handle to an open key.
 *  lpClass                [O] Class string of the key.
 *  lpcClass               [I/O] size of lpClass.
 *  lpReserved             [I] Reserved; must be NULL.
 *  lpcSubKeys             [O] Number of subkeys contained by the key.
 *  lpcMaxSubKeyLen        [O] Size of the key's subkey with the longest name.
 *  lpcMaxClassLen         [O] Size of the longest string specifying a subkey
 *                             class in TCHARS.
 *  lpcValues              [O] Number of values associated with the key.
 *  lpcMaxValueNameLen     [O] Size of the key's longest value name in TCHARS.
 *  lpcMaxValueLen         [O] Longest data component among the key's values
 *  lpcbSecurityDescriptor [O] Size of the key's security descriptor.
 *  lpftLastWriteTime      [O] FILETIME strucutre that is the last write time.
 *
 *  RETURNS
 *   Success: ERROR_SUCCESS
 *   Failure: nonzero error code from Winerror.h
 */
DWORD WINAPI RegQueryInfoKeyA( HKEY hkey, LPSTR class, LPDWORD class_len, LPDWORD reserved,
                               LPDWORD subkeys, LPDWORD max_subkey, LPDWORD max_class,
                               LPDWORD values, LPDWORD max_value, LPDWORD max_data,
                               LPDWORD security, FILETIME *modif )
{
    NTSTATUS status;
    char buffer[256], *buf_ptr = buffer;
    KEY_FULL_INFORMATION *info = (KEY_FULL_INFORMATION *)buffer;
    DWORD total_size, len;

    TRACE( "(%p,%p,%ld,%p,%p,%p,%p,%p,%p,%p,%p)\n", hkey, class, class_len ? *class_len : 0,
           reserved, subkeys, max_subkey, values, max_value, max_data, security, modif );

    if (class && !class_len && is_version_nt()) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    status = NtQueryKey( hkey, KeyFullInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (class || class_len)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_FULL_INFORMATION *)buf_ptr;
            status = NtQueryKey( hkey, KeyFullInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info->ClassOffset), info->ClassLength);
        if (class_len)
        {
            if (len + 1 > *class_len) status = STATUS_BUFFER_OVERFLOW;
            *class_len = len;
        }
        if (class && !status)
        {
            RtlUnicodeToMultiByteN( class, len, NULL, (WCHAR *)(buf_ptr + info->ClassOffset),
                                    info->ClassLength );
            class[len] = 0;
        }
    }
    else status = STATUS_SUCCESS;

    if (subkeys) *subkeys = info->SubKeys;
    if (max_subkey) *max_subkey = info->MaxNameLen;
    if (max_class) *max_class = info->MaxClassLen;
    if (values) *values = info->Values;
    if (max_value) *max_value = info->MaxValueNameLen;
    if (max_data) *max_data = info->MaxValueDataLen;
    if (modif) *modif = *(FILETIME *)&info->LastWriteTime;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegCloseKey   [ADVAPI32.@]
 *
 * Close an open registry key.
 *
 * PARAMS
 *  hkey [I] Handle of key to close
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: Error code
 */
DWORD WINAPI RegCloseKey( HKEY hkey )
{
    if (!hkey) return ERROR_INVALID_HANDLE;
    if (hkey >= (HKEY)0x80000000) return ERROR_SUCCESS;
    return RtlNtStatusToDosError( NtClose( hkey ) );
}


/******************************************************************************
 * RegDeleteKeyW   [ADVAPI32.@]
 *
 * See RegDeleteKeyA.
 */
DWORD WINAPI RegDeleteKeyW( HKEY hkey, LPCWSTR name )
{
    DWORD ret;
    HKEY tmp;

    if (!name) return ERROR_INVALID_PARAMETER;

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    if (!(ret = RegOpenKeyExW( hkey, name, 0, DELETE, &tmp )))
    {
        ret = RtlNtStatusToDosError( NtDeleteKey( tmp ) );
        RegCloseKey( tmp );
    }
    TRACE("%s ret=%08lx\n", debugstr_w(name), ret);
    return ret;
}


/******************************************************************************
 * RegDeleteKeyA   [ADVAPI32.@]
 *
 * Delete a registry key.
 *
 * PARAMS
 *  hkey   [I] Handle to parent key containing the key to delete
 *  name   [I] Name of the key user hkey to delete
 *
 * NOTES
 *
 * MSDN is wrong when it says that hkey must be opened with the DELETE access
 * right. In reality, it opens a new handle with DELETE access.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: Error code
 */
DWORD WINAPI RegDeleteKeyA( HKEY hkey, LPCSTR name )
{
    DWORD ret;
    HKEY tmp;

    if (!name) return ERROR_INVALID_PARAMETER;

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    if (!(ret = RegOpenKeyExA( hkey, name, 0, DELETE, &tmp )))
    {
        if (!is_version_nt()) /* win95 does recursive key deletes */
        {
            CHAR name[MAX_PATH];

            while(!RegEnumKeyA(tmp, 0, name, sizeof(name)))
            {
                if(RegDeleteKeyA(tmp, name))  /* recurse */
                    break;
            }
        }
        ret = RtlNtStatusToDosError( NtDeleteKey( tmp ) );
        RegCloseKey( tmp );
    }
    TRACE("%s ret=%08lx\n", debugstr_a(name), ret);
    return ret;
}



/******************************************************************************
 * RegSetValueExW   [ADVAPI32.@]
 *
 * Set the data and contents of a registry value.
 *
 * PARAMS
 *  hkey       [I] Handle of key to set value for
 *  name       [I] Name of value to set
 *  reserved   [I] Reserved, must be zero
 *  type       [I] Type of the value being set
 *  data       [I] The new contents of the value to set
 *  count      [I] Size of data
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: Error code
 */
DWORD WINAPI RegSetValueExW( HKEY hkey, LPCWSTR name, DWORD reserved,
                             DWORD type, CONST BYTE *data, DWORD count )
{
    UNICODE_STRING nameW;

    /* no need for version check, not implemented on win9x anyway */
    if (count && is_string(type))
    {
        LPCWSTR str = (LPCWSTR)data;
        /* if user forgot to count terminating null, add it (yes NT does this) */
        if (str[count / sizeof(WCHAR) - 1] && !str[count / sizeof(WCHAR)])
            count += sizeof(WCHAR);
    }
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtSetValueKey( hkey, &nameW, 0, type, data, count ) );
}


/******************************************************************************
 * RegSetValueExA   [ADVAPI32.@]
 *
 * See RegSetValueExW.
 *
 * NOTES
 *  win95 does not care about count for REG_SZ and finds out the len by itself (js)
 *  NT does definitely care (aj)
 */
DWORD WINAPI RegSetValueExA( HKEY hkey, LPCSTR name, DWORD reserved, DWORD type,
                             CONST BYTE *data, DWORD count )
{
    ANSI_STRING nameA;
    WCHAR *dataW = NULL;
    NTSTATUS status;

    if (!is_version_nt())  /* win95 */
    {
        if (type == REG_SZ)
        {
            if (!data) return ERROR_INVALID_PARAMETER;
            count = strlen((const char *)data) + 1;
        }
    }
    else if (count && is_string(type))
    {
        /* if user forgot to count terminating null, add it (yes NT does this) */
        if (data[count-1] && !data[count]) count++;
    }

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    if (is_string( type )) /* need to convert to Unicode */
    {
        DWORD lenW;
        RtlMultiByteToUnicodeSize( &lenW, (const char *)data, count );
        if (!(dataW = HeapAlloc( GetProcessHeap(), 0, lenW ))) return ERROR_OUTOFMEMORY;
        RtlMultiByteToUnicodeN( dataW, lenW, NULL, (const char *)data, count );
        count = lenW;
        data = (BYTE *)dataW;
    }

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
    {
        status = NtSetValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString, 0, type, data, count );
    }
    HeapFree( GetProcessHeap(), 0, dataW );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegSetValueW   [ADVAPI32.@]
 *
 * Sets the data for the default or unnamed value of a reg key.
 *
 * PARAMS
 *  hKey     [I] Handle to an open key.
 *  lpSubKey [I] Name of a subkey of hKey.
 *  dwType   [I] Type of information to store.
 *  lpData   [I] String that contains the data to set for the default value.
 *  cbData   [I] Size of lpData.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
DWORD WINAPI RegSetValueW( HKEY hkey, LPCWSTR name, DWORD type, LPCWSTR data, DWORD count )
{
    HKEY subkey = hkey;
    DWORD ret;

    TRACE("(%p,%s,%ld,%s,%ld)\n", hkey, debugstr_w(name), type, debugstr_w(data), count );

    if (type != REG_SZ) return ERROR_INVALID_PARAMETER;

    if (name && name[0])  /* need to create the subkey */
    {
        if ((ret = RegCreateKeyW( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }

    ret = RegSetValueExW( subkey, NULL, 0, REG_SZ, (const BYTE*)data,
                          (strlenW( data ) + 1) * sizeof(WCHAR) );
    if (subkey != hkey) RegCloseKey( subkey );
    return ret;
}


/******************************************************************************
 * RegSetValueA   [ADVAPI32.@]
 *
 * See RegSetValueW.
 */
DWORD WINAPI RegSetValueA( HKEY hkey, LPCSTR name, DWORD type, LPCSTR data, DWORD count )
{
    HKEY subkey = hkey;
    DWORD ret;

    TRACE("(%p,%s,%ld,%s,%ld)\n", hkey, debugstr_a(name), type, debugstr_a(data), count );

    if (type != REG_SZ) return ERROR_INVALID_PARAMETER;

    if (name && name[0])  /* need to create the subkey */
    {
        if ((ret = RegCreateKeyA( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegSetValueExA( subkey, NULL, 0, REG_SZ, (const BYTE*)data, strlen(data)+1 );
    if (subkey != hkey) RegCloseKey( subkey );
    return ret;
}



/******************************************************************************
 * RegQueryValueExW   [ADVAPI32.@]
 *
 * See RegQueryValueExA.
 */
DWORD WINAPI RegQueryValueExW( HKEY hkey, LPCWSTR name, LPDWORD reserved, LPDWORD type,
                               LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    UNICODE_STRING name_str;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    static const int info_size = offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data );

    TRACE("(%p,%s,%p,%p,%p,%p=%ld)\n",
          hkey, debugstr_w(name), reserved, type, data, count,
          (count && data) ? *count : 0 );

    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    RtlInitUnicodeString( &name_str, name );

    if (data) total_size = min( sizeof(buffer), *count + info_size );
    else total_size = info_size;

    status = NtQueryValueKey( hkey, &name_str, KeyValuePartialInformation,
                              buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (data)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW && total_size - info_size <= *count)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_PARTIAL_INFORMATION *)buf_ptr;
            status = NtQueryValueKey( hkey, &name_str, KeyValuePartialInformation,
                                      buf_ptr, total_size, &total_size );
        }

        if (!status)
        {
            memcpy( data, buf_ptr + info_size, total_size - info_size );
            /* if the type is REG_SZ and data is not 0-terminated
             * and there is enough space in the buffer NT appends a \0 */
            if (total_size - info_size <= *count-sizeof(WCHAR) && is_string(info->Type))
            {
                WCHAR *ptr = (WCHAR *)(data + total_size - info_size);
                if (ptr > (WCHAR *)data && ptr[-1]) *ptr = 0;
            }
        }
        else if (status != STATUS_BUFFER_OVERFLOW) goto done;
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = total_size - info_size;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 * RegQueryValueExA   [ADVAPI32.@]
 *
 * Get the type and contents of a specified value under with a key.
 *
 * PARAMS
 *  hkey      [I]   Handle of the key to query
 *  name      [I]   Name of value under hkey to query
 *  reserved  [I]   Reserved - must be NULL
 *  type      [O]   Destination for the value type, or NULL if not required
 *  data      [O]   Destination for the values contents, or NULL if not required
 *  count     [I/O] Size of data, updated with the number of bytes returned
 *
 * RETURNS
 *  Success: ERROR_SUCCESS. *count is updated with the number of bytes copied to data.
 *  Failure: ERROR_INVALID_HANDLE, if hkey is invalid.
 *           ERROR_INVALID_PARAMETER, if any other parameter is invalid.
 *           ERROR_MORE_DATA, if on input *count is too small to hold the contents.
 *                     
 * NOTES
 *   MSDN states that if data is too small it is partially filled. In reality 
 *   it remains untouched.
 */
DWORD WINAPI RegQueryValueExA( HKEY hkey, LPCSTR name, LPDWORD reserved, LPDWORD type,
                               LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    ANSI_STRING nameA;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_PARTIAL_INFORMATION *info = (KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    static const int info_size = offsetof( KEY_VALUE_PARTIAL_INFORMATION, Data );

    TRACE("(%p,%s,%p,%p,%p,%p=%ld)\n",
          hkey, debugstr_a(name), reserved, type, data, count, count ? *count : 0 );

    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    RtlInitAnsiString( &nameA, name );
    if ((status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                &nameA, FALSE )))
        return RtlNtStatusToDosError(status);

    status = NtQueryValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString,
                              KeyValuePartialInformation, buffer, sizeof(buffer), &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    /* we need to fetch the contents for a string type even if not requested,
     * because we need to compute the length of the ASCII string. */
    if (data || is_string(info->Type))
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
            {
                status = STATUS_NO_MEMORY;
                goto done;
            }
            info = (KEY_VALUE_PARTIAL_INFORMATION *)buf_ptr;
            status = NtQueryValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString,
                                    KeyValuePartialInformation, buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (is_string(info->Type))
        {
            DWORD len;

            RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info_size),
                                       total_size - info_size );
            if (data && len)
            {
                if (len > *count) status = STATUS_BUFFER_OVERFLOW;
                else
                {
                    RtlUnicodeToMultiByteN( (char*)data, len, NULL, (WCHAR *)(buf_ptr + info_size),
                                            total_size - info_size );
                    /* if the type is REG_SZ and data is not 0-terminated
                     * and there is enough space in the buffer NT appends a \0 */
                    if (len < *count && data[len-1]) data[len] = 0;
                }
            }
            total_size = len + info_size;
        }
        else if (data)
        {
            if (total_size - info_size > *count) status = STATUS_BUFFER_OVERFLOW;
            else memcpy( data, buf_ptr + info_size, total_size - info_size );
        }
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = total_size - info_size;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 * RegQueryValueW   [ADVAPI32.@]
 *
 * Retrieves the data associated with the default or unnamed value of a key.
 *
 * PARAMS
 *  hkey      [I] Handle to an open key.
 *  name      [I] Name of the subkey of hKey.
 *  data      [O] Receives the string associated with the default value
 *                of the key.
 *  count     [I/O] Size of lpValue in bytes.
 *
 *  RETURNS
 *   Success: ERROR_SUCCESS
 *   Failure: nonzero error code from Winerror.h
 */
DWORD WINAPI RegQueryValueW( HKEY hkey, LPCWSTR name, LPWSTR data, LPLONG count )
{
    DWORD ret;
    HKEY subkey = hkey;

    TRACE("(%p,%s,%p,%ld)\n", hkey, debugstr_w(name), data, count ? *count : 0 );

    if (name && name[0])
    {
        if ((ret = RegOpenKeyW( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegQueryValueExW( subkey, NULL, NULL, NULL, (LPBYTE)data, (LPDWORD)count );
    if (subkey != hkey) RegCloseKey( subkey );
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        /* return empty string if default value not found */
        if (data) *data = 0;
        if (count) *count = sizeof(WCHAR);
        ret = ERROR_SUCCESS;
    }
    return ret;
}


/******************************************************************************
 * RegQueryValueA   [ADVAPI32.@]
 *
 * See RegQueryValueW.
 */
DWORD WINAPI RegQueryValueA( HKEY hkey, LPCSTR name, LPSTR data, LPLONG count )
{
    DWORD ret;
    HKEY subkey = hkey;

    TRACE("(%p,%s,%p,%ld)\n", hkey, debugstr_a(name), data, count ? *count : 0 );

    if (name && name[0])
    {
        if ((ret = RegOpenKeyA( hkey, name, &subkey )) != ERROR_SUCCESS) return ret;
    }
    ret = RegQueryValueExA( subkey, NULL, NULL, NULL, (LPBYTE)data, (LPDWORD)count );
    if (subkey != hkey) RegCloseKey( subkey );
    if (ret == ERROR_FILE_NOT_FOUND)
    {
        /* return empty string if default value not found */
        if (data) *data = 0;
        if (count) *count = 1;
        ret = ERROR_SUCCESS;
    }
    return ret;
}


/******************************************************************************
 * ADVAPI_ApplyRestrictions   [internal]
 *
 * Helper function for RegGetValueA/W.
 */
VOID ADVAPI_ApplyRestrictions( DWORD dwFlags, DWORD dwType, DWORD cbData,
                               PLONG ret )
{
    /* Check if the type is restricted by the passed flags */
    if (*ret == ERROR_SUCCESS || *ret == ERROR_MORE_DATA)
    {
        DWORD dwMask = 0;

        switch (dwType)
        {
        case REG_NONE: dwMask = RRF_RT_REG_NONE; break;
        case REG_SZ: dwMask = RRF_RT_REG_SZ; break;
        case REG_EXPAND_SZ: dwMask = RRF_RT_REG_EXPAND_SZ; break;
        case REG_MULTI_SZ: dwMask = RRF_RT_REG_MULTI_SZ; break;
        case REG_BINARY: dwMask = RRF_RT_REG_BINARY; break;
        case REG_DWORD: dwMask = RRF_RT_REG_DWORD; break;
        case REG_QWORD: dwMask = RRF_RT_REG_QWORD; break;
        }

        if (dwFlags & dwMask)
        {
            /* Type is not restricted, check for size mismatch */
            if (dwType == REG_BINARY)
            {
                DWORD cbExpect = 0;

                if ((dwFlags & RRF_RT_DWORD) == RRF_RT_DWORD)
                    cbExpect = 4;
                else if ((dwFlags & RRF_RT_DWORD) == RRF_RT_QWORD)
                    cbExpect = 8;

                if (cbExpect && cbData != cbExpect)
                    *ret = ERROR_DATATYPE_MISMATCH;
            }
        }
        else *ret = ERROR_UNSUPPORTED_TYPE;
    }
}


/******************************************************************************
 * RegGetValueW   [ADVAPI32.@]
 *
 * Retrieves the type and data for a value name associated with a key 
 * optionally expanding it's content and restricting it's type.
 *
 * PARAMS
 *  hKey      [I] Handle to an open key.
 *  pszSubKey [I] Name of the subkey of hKey.
 *  pszValue  [I] Name of value under hKey/szSubKey to query.
 *  dwFlags   [I] Flags restricting the value type to retrieve.
 *  pdwType   [O] Destination for the values type, may be NULL.
 *  pvData    [O] Destination for the values content, may be NULL.
 *  pcbData   [I/O] Size of pvData, updated with the size required to 
 *                  retrieve the whole content.
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 *
 * NOTES
 *  - Unless RRF_NOEXPAND is specified REG_EXPAND_SZ is automatically expanded
 *    and REG_SZ is retrieved instead.
 *  - Restrictions are applied after expanding, using RRF_RT_REG_EXPAND_SZ 
 *    without RRF_NOEXPAND is thus not allowed.
 */
LONG WINAPI RegGetValueW( HKEY hKey, LPCWSTR pszSubKey, LPCWSTR pszValue, 
                          DWORD dwFlags, LPDWORD pdwType, PVOID pvData,
                          LPDWORD pcbData )
{
    DWORD dwType, cbData = pcbData ? *pcbData : 0;
    PVOID pvBuf = NULL;
    LONG ret;

    TRACE("(%p,%s,%s,%ld,%p,%p,%p=%ld)\n", 
          hKey, debugstr_w(pszSubKey), debugstr_w(pszValue), dwFlags, pdwType,
          pvData, pcbData, cbData);

    if ((dwFlags & RRF_RT_REG_EXPAND_SZ) && !(dwFlags & RRF_NOEXPAND))
        return ERROR_INVALID_PARAMETER;

    if (pszSubKey && pszSubKey[0])
    {
        ret = RegOpenKeyExW(hKey, pszSubKey, 0, KEY_QUERY_VALUE, &hKey);
        if (ret != ERROR_SUCCESS) return ret;
    }

    ret = RegQueryValueExW(hKey, pszValue, NULL, &dwType, pvData, &cbData);
    
    /* If we are going to expand we need to read in the whole the value even
     * if the passed buffer was too small as the expanded string might be
     * smaller than the unexpanded one and could fit into cbData bytes. */
    if ((ret == ERROR_SUCCESS || ret == ERROR_MORE_DATA) &&
        (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND)))
    {
        do {
            if (pvBuf) HeapFree(GetProcessHeap(), 0, pvBuf);
            
            pvBuf = HeapAlloc(GetProcessHeap(), 0, cbData);
            if (!pvBuf)
            {
                ret = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            if (ret == ERROR_MORE_DATA)
                ret = RegQueryValueExW(hKey, pszValue, NULL, 
                                       &dwType, pvBuf, &cbData);
            else
            {
                /* Even if cbData was large enough we have to copy the 
                 * string since ExpandEnvironmentStrings can't handle
                 * overlapping buffers. */
                CopyMemory(pvBuf, pvData, cbData);
            }

            /* Both the type or the value itself could have been modified in
             * between so we have to keep retrying until the buffer is large
             * enough or we no longer have to expand the value. */
        } while (dwType == REG_EXPAND_SZ && ret == ERROR_MORE_DATA);

        if (ret == ERROR_SUCCESS)
        {
            if (dwType == REG_EXPAND_SZ)
            {
                cbData = ExpandEnvironmentStringsW(pvBuf, pvData,
                                                   pcbData ? *pcbData : 0);
                dwType = REG_SZ;
                if(pcbData && cbData > *pcbData)
                    ret = ERROR_MORE_DATA;
            }
            else if (pcbData)
                CopyMemory(pvData, pvBuf, *pcbData);
        }

        if (pvBuf) HeapFree(GetProcessHeap(), 0, pvBuf);
    }

    if (pszSubKey && pszSubKey[0])
        RegCloseKey(hKey);

    ADVAPI_ApplyRestrictions(dwFlags, dwType, cbData, &ret);

    if (pcbData && ret != ERROR_SUCCESS && (dwFlags & RRF_ZEROONFAILURE))
        ZeroMemory(pvData, *pcbData);

    if (pdwType) *pdwType = dwType;
    if (pcbData) *pcbData = cbData;

    return ret;
}


/******************************************************************************
 * RegGetValueA   [ADVAPI32.@]
 *
 * See RegGetValueW.
 */
LONG WINAPI RegGetValueA( HKEY hKey, LPCSTR pszSubKey, LPCSTR pszValue, 
                          DWORD dwFlags, LPDWORD pdwType, PVOID pvData, 
                          LPDWORD pcbData )
{
    DWORD dwType, cbData = pcbData ? *pcbData : 0;
    PVOID pvBuf = NULL;
    LONG ret;

    TRACE("(%p,%s,%s,%ld,%p,%p,%p=%ld)\n", 
          hKey, pszSubKey, pszValue, dwFlags, pdwType, pvData, pcbData,
          cbData);

    if ((dwFlags & RRF_RT_REG_EXPAND_SZ) && !(dwFlags & RRF_NOEXPAND))
        return ERROR_INVALID_PARAMETER;

    if (pszSubKey && pszSubKey[0])
    {
        ret = RegOpenKeyExA(hKey, pszSubKey, 0, KEY_QUERY_VALUE, &hKey);
        if (ret != ERROR_SUCCESS) return ret;
    }

    ret = RegQueryValueExA(hKey, pszValue, NULL, &dwType, pvData, &cbData);

    /* If we are going to expand we need to read in the whole the value even
     * if the passed buffer was too small as the expanded string might be
     * smaller than the unexpanded one and could fit into cbData bytes. */
    if ((ret == ERROR_SUCCESS || ret == ERROR_MORE_DATA) &&
        (dwType == REG_EXPAND_SZ && !(dwFlags & RRF_NOEXPAND)))
    {
        do {
            if (pvBuf) HeapFree(GetProcessHeap(), 0, pvBuf);

            pvBuf = HeapAlloc(GetProcessHeap(), 0, cbData);
            if (!pvBuf)
            {
                ret = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            if (ret == ERROR_MORE_DATA)
                ret = RegQueryValueExA(hKey, pszValue, NULL, 
                                       &dwType, pvBuf, &cbData);
            else
            {
                /* Even if cbData was large enough we have to copy the 
                 * string since ExpandEnvironmentStrings can't handle
                 * overlapping buffers. */
                CopyMemory(pvBuf, pvData, cbData);
            }

            /* Both the type or the value itself could have been modified in
             * between so we have to keep retrying until the buffer is large
             * enough or we no longer have to expand the value. */
        } while (dwType == REG_EXPAND_SZ && ret == ERROR_MORE_DATA);

        if (ret == ERROR_SUCCESS)
        {
            if (dwType == REG_EXPAND_SZ)
            {
                cbData = ExpandEnvironmentStringsA(pvBuf, pvData,
                                                   pcbData ? *pcbData : 0);
                dwType = REG_SZ;
                if(pcbData && cbData > *pcbData)
                    ret = ERROR_MORE_DATA;
            }
            else if (pcbData)
                CopyMemory(pvData, pvBuf, *pcbData);
        }

        if (pvBuf) HeapFree(GetProcessHeap(), 0, pvBuf);
    }

    if (pszSubKey && pszSubKey[0])
        RegCloseKey(hKey);

    ADVAPI_ApplyRestrictions(dwFlags, dwType, cbData, &ret);

    if (pcbData && ret != ERROR_SUCCESS && (dwFlags & RRF_ZEROONFAILURE))
        ZeroMemory(pvData, *pcbData);

    if (pdwType) *pdwType = dwType;
    if (pcbData) *pcbData = cbData;

    return ret;
}


/******************************************************************************
 * RegEnumValueW   [ADVAPI32.@]
 *
 * Enumerates the values for the specified open registry key.
 *
 * PARAMS
 *  hkey       [I] Handle to key to query
 *  index      [I] Index of value to query
 *  value      [O] Value string
 *  val_count  [I/O] Size of value buffer (in wchars)
 *  reserved   [I] Reserved
 *  type       [O] Type code
 *  data       [O] Value data
 *  count      [I/O] Size of data buffer (in bytes)
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */

DWORD WINAPI RegEnumValueW( HKEY hkey, DWORD index, LPWSTR value, LPDWORD val_count,
                            LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;
    static const int info_size = offsetof( KEY_VALUE_FULL_INFORMATION, Name );

    TRACE("(%p,%ld,%p,%p,%p,%p,%p,%p)\n",
          hkey, index, value, val_count, reserved, type, data, count );

    /* NT only checks count, not val_count */
    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    total_size = info_size + (MAX_PATH + 1) * sizeof(WCHAR);
    if (data) total_size += *count;
    total_size = min( sizeof(buffer), total_size );

    status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                  buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    if (value || data)
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_FULL_INFORMATION *)buf_ptr;
            status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                          buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (value)
        {
            if (info->NameLength/sizeof(WCHAR) >= *val_count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                goto overflow;
            }
            memcpy( value, info->Name, info->NameLength );
            *val_count = info->NameLength / sizeof(WCHAR);
            value[*val_count] = 0;
        }

        if (data)
        {
            if (total_size - info->DataOffset > *count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                goto overflow;
            }
            memcpy( data, buf_ptr + info->DataOffset, total_size - info->DataOffset );
            if (total_size - info->DataOffset <= *count-sizeof(WCHAR) && is_string(info->Type))
            {
                /* if the type is REG_SZ and data is not 0-terminated
                 * and there is enough space in the buffer NT appends a \0 */
                WCHAR *ptr = (WCHAR *)(data + total_size - info->DataOffset);
                if (ptr > (WCHAR *)data && ptr[-1]) *ptr = 0;
            }
        }
    }
    else status = STATUS_SUCCESS;

 overflow:
    if (type) *type = info->Type;
    if (count) *count = info->DataLength;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}


/******************************************************************************
 * RegEnumValueA   [ADVAPI32.@]
 *
 * See RegEnumValueW.
 */
DWORD WINAPI RegEnumValueA( HKEY hkey, DWORD index, LPSTR value, LPDWORD val_count,
                            LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD count )
{
    NTSTATUS status;
    DWORD total_size;
    char buffer[256], *buf_ptr = buffer;
    KEY_VALUE_FULL_INFORMATION *info = (KEY_VALUE_FULL_INFORMATION *)buffer;
    static const int info_size = offsetof( KEY_VALUE_FULL_INFORMATION, Name );

    TRACE("(%p,%ld,%p,%p,%p,%p,%p,%p)\n",
          hkey, index, value, val_count, reserved, type, data, count );

    /* NT only checks count, not val_count */
    if ((data && !count) || reserved) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    total_size = info_size + (MAX_PATH + 1) * sizeof(WCHAR);
    if (data) total_size += *count;
    total_size = min( sizeof(buffer), total_size );

    status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                  buffer, total_size, &total_size );
    if (status && status != STATUS_BUFFER_OVERFLOW) goto done;

    /* we need to fetch the contents for a string type even if not requested,
     * because we need to compute the length of the ASCII string. */
    if (value || data || is_string(info->Type))
    {
        /* retry with a dynamically allocated buffer */
        while (status == STATUS_BUFFER_OVERFLOW)
        {
            if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
            if (!(buf_ptr = HeapAlloc( GetProcessHeap(), 0, total_size )))
                return ERROR_NOT_ENOUGH_MEMORY;
            info = (KEY_VALUE_FULL_INFORMATION *)buf_ptr;
            status = NtEnumerateValueKey( hkey, index, KeyValueFullInformation,
                                          buf_ptr, total_size, &total_size );
        }

        if (status) goto done;

        if (is_string(info->Type))
        {
            DWORD len;
            RtlUnicodeToMultiByteSize( &len, (WCHAR *)(buf_ptr + info->DataOffset),
                                       total_size - info->DataOffset );
            if (data && len)
            {
                if (len > *count) status = STATUS_BUFFER_OVERFLOW;
                else
                {
                    RtlUnicodeToMultiByteN( (char*)data, len, NULL, (WCHAR *)(buf_ptr + info->DataOffset),
                                            total_size - info->DataOffset );
                    /* if the type is REG_SZ and data is not 0-terminated
                     * and there is enough space in the buffer NT appends a \0 */
                    if (len < *count && data[len-1]) data[len] = 0;
                }
            }
            info->DataLength = len;
        }
        else if (data)
        {
            if (total_size - info->DataOffset > *count) status = STATUS_BUFFER_OVERFLOW;
            else memcpy( data, buf_ptr + info->DataOffset, total_size - info->DataOffset );
        }

        if (value && !status)
        {
            DWORD len;

            RtlUnicodeToMultiByteSize( &len, info->Name, info->NameLength );
            if (len >= *val_count)
            {
                status = STATUS_BUFFER_OVERFLOW;
                if (*val_count)
                {
                    len = *val_count - 1;
                    RtlUnicodeToMultiByteN( value, len, NULL, info->Name, info->NameLength );
                    value[len] = 0;
                }
            }
            else
            {
                RtlUnicodeToMultiByteN( value, len, NULL, info->Name, info->NameLength );
                value[len] = 0;
                *val_count = len;
            }
        }
    }
    else status = STATUS_SUCCESS;

    if (type) *type = info->Type;
    if (count) *count = info->DataLength;

 done:
    if (buf_ptr != buffer) HeapFree( GetProcessHeap(), 0, buf_ptr );
    return RtlNtStatusToDosError(status);
}



/******************************************************************************
 * RegDeleteValueW   [ADVAPI32.@]
 *
 * See RegDeleteValueA.
 */
DWORD WINAPI RegDeleteValueW( HKEY hkey, LPCWSTR name )
{
    UNICODE_STRING nameW;

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    RtlInitUnicodeString( &nameW, name );
    return RtlNtStatusToDosError( NtDeleteValueKey( hkey, &nameW ) );
}


/******************************************************************************
 * RegDeleteValueA   [ADVAPI32.@]
 *
 * Delete a value from the registry.
 *
 * PARAMS
 *  hkey [I] Registry handle of the key holding the value
 *  name [I] Name of the value under hkey to delete
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
DWORD WINAPI RegDeleteValueA( HKEY hkey, LPCSTR name )
{
    STRING nameA;
    NTSTATUS status;

    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    RtlInitAnsiString( &nameA, name );
    if (!(status = RtlAnsiStringToUnicodeString( &NtCurrentTeb()->StaticUnicodeString,
                                                 &nameA, FALSE )))
        status = NtDeleteValueKey( hkey, &NtCurrentTeb()->StaticUnicodeString );
    return RtlNtStatusToDosError( status );
}


/******************************************************************************
 * RegLoadKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *  hkey      [I] Handle of open key
 *  subkey    [I] Address of name of subkey
 *  filename  [I] Address of filename for registry information
 *
 * RETURNS
 *  Success: ERROR_SUCCES
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegLoadKeyW( HKEY hkey, LPCWSTR subkey, LPCWSTR filename )
{
    OBJECT_ATTRIBUTES destkey, file;
    UNICODE_STRING subkeyW, filenameW;

    if (!(hkey = get_special_root_hkey(hkey))) return ERROR_INVALID_HANDLE;

    destkey.Length = sizeof(destkey);
    destkey.RootDirectory = hkey;               /* root key: HKLM or HKU */
    destkey.ObjectName = &subkeyW;              /* name of the key */
    destkey.Attributes = 0;
    destkey.SecurityDescriptor = NULL;
    destkey.SecurityQualityOfService = NULL;
    RtlInitUnicodeString(&subkeyW, subkey);

    file.Length = sizeof(file);
    file.RootDirectory = NULL;
    file.ObjectName = &filenameW;               /* file containing the hive */
    file.Attributes = OBJ_CASE_INSENSITIVE;
    file.SecurityDescriptor = NULL;
    file.SecurityQualityOfService = NULL;
    RtlDosPathNameToNtPathName_U(filename, &filenameW, NULL, NULL);

    return RtlNtStatusToDosError( NtLoadKey(&destkey, &file) );
}


/******************************************************************************
 * RegLoadKeyA   [ADVAPI32.@]
 *
 * See RegLoadKeyW.
 */
LONG WINAPI RegLoadKeyA( HKEY hkey, LPCSTR subkey, LPCSTR filename )
{
    UNICODE_STRING subkeyW, filenameW;
    STRING subkeyA, filenameA;
    NTSTATUS status;

    RtlInitAnsiString(&subkeyA, subkey);
    RtlInitAnsiString(&filenameA, filename);

    if ((status = RtlAnsiStringToUnicodeString(&subkeyW, &subkeyA, TRUE)))
        return RtlNtStatusToDosError(status);

    if ((status = RtlAnsiStringToUnicodeString(&filenameW, &filenameA, TRUE)))
        return RtlNtStatusToDosError(status);

    return RegLoadKeyW(hkey, subkeyW.Buffer, filenameW.Buffer);
}


/******************************************************************************
 * RegSaveKeyW   [ADVAPI32.@]
 *
 * PARAMS
 *  hkey   [I] Handle of key where save begins
 *  lpFile [I] Address of filename to save to
 *  sa     [I] Address of security structure
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegSaveKeyW( HKEY hkey, LPCWSTR file, LPSECURITY_ATTRIBUTES sa )
{
    static const WCHAR format[] =
        {'r','e','g','%','0','4','x','.','t','m','p',0};
    WCHAR buffer[MAX_PATH];
    int count = 0;
    LPWSTR nameW;
    DWORD ret, err;
    HANDLE handle;

    TRACE( "(%p,%s,%p)\n", hkey, debugstr_w(file), sa );

    if (!file || !*file) return ERROR_INVALID_PARAMETER;
    if (!(hkey = get_special_root_hkey( hkey ))) return ERROR_INVALID_HANDLE;

    err = GetLastError();
    GetFullPathNameW( file, sizeof(buffer)/sizeof(WCHAR), buffer, &nameW );

    for (;;)
    {
        snprintfW( nameW, 16, format, count++ );
        handle = CreateFileW( buffer, GENERIC_WRITE, 0, NULL,
                            CREATE_NEW, FILE_ATTRIBUTE_NORMAL, 0 );
        if (handle != INVALID_HANDLE_VALUE) break;
        if ((ret = GetLastError()) != ERROR_ALREADY_EXISTS) goto done;

        /* Something gone haywire ? Please report if this happens abnormally */
        if (count >= 100)
            MESSAGE("Wow, we are already fiddling with a temp file %s with an ordinal as high as %d !\nYou might want to delete all corresponding temp files in that directory.\n", debugstr_w(buffer), count);
    }

    ret = RtlNtStatusToDosError(NtSaveKey(hkey, handle));

    CloseHandle( handle );
    if (!ret)
    {
        if (!MoveFileExW( buffer, file, MOVEFILE_REPLACE_EXISTING ))
        {
            ERR( "Failed to move %s to %s\n", debugstr_w(buffer),
	        debugstr_w(file) );
            ret = GetLastError();
        }
    }
    if (ret) DeleteFileW( buffer );

done:
    SetLastError( err );  /* restore last error code */
    return ret;
}


/******************************************************************************
 * RegSaveKeyA  [ADVAPI32.@]
 *
 * See RegSaveKeyW.
 */
LONG WINAPI RegSaveKeyA( HKEY hkey, LPCSTR file, LPSECURITY_ATTRIBUTES sa )
{
    UNICODE_STRING *fileW = &NtCurrentTeb()->StaticUnicodeString;
    NTSTATUS status;
    STRING fileA;

    RtlInitAnsiString(&fileA, file);
    if ((status = RtlAnsiStringToUnicodeString(fileW, &fileA, FALSE)))
        return RtlNtStatusToDosError( status );
    return RegSaveKeyW(hkey, fileW->Buffer, sa);
}


/******************************************************************************
 * RegRestoreKeyW [ADVAPI32.@]
 *
 * PARAMS
 *  hkey    [I] Handle of key where restore begins
 *  lpFile  [I] Address of filename containing saved tree
 *  dwFlags [I] Optional flags
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegRestoreKeyW( HKEY hkey, LPCWSTR lpFile, DWORD dwFlags )
{
    TRACE("(%p,%s,%ld)\n",hkey,debugstr_w(lpFile),dwFlags);

    /* It seems to do this check before the hkey check */
    if (!lpFile || !*lpFile)
        return ERROR_INVALID_PARAMETER;

    FIXME("(%p,%s,%ld): stub\n",hkey,debugstr_w(lpFile),dwFlags);

    /* Check for file existence */

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegRestoreKeyA [ADVAPI32.@]
 *
 * See RegRestoreKeyW.
 */
LONG WINAPI RegRestoreKeyA( HKEY hkey, LPCSTR lpFile, DWORD dwFlags )
{
    UNICODE_STRING lpFileW;
    LONG ret;

    RtlCreateUnicodeStringFromAsciiz( &lpFileW, lpFile );
    ret = RegRestoreKeyW( hkey, lpFileW.Buffer, dwFlags );
    RtlFreeUnicodeString( &lpFileW );
    return ret;
}


/******************************************************************************
 * RegUnLoadKeyW [ADVAPI32.@]
 *
 * PARAMS
 *  hkey     [I] Handle of open key
 *  lpSubKey [I] Address of name of subkey to unload
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegUnLoadKeyW( HKEY hkey, LPCWSTR lpSubKey )
{
    DWORD ret;
    HKEY shkey;

    TRACE("(%p,%s)\n",hkey, debugstr_w(lpSubKey));

    ret = RegOpenKeyW(hkey,lpSubKey,&shkey);
    if( ret )
        return ERROR_INVALID_PARAMETER;

    ret = RtlNtStatusToDosError(NtUnloadKey(shkey));

    RegCloseKey(shkey);

    return ret;
}


/******************************************************************************
 * RegUnLoadKeyA [ADVAPI32.@]
 *
 * See RegUnLoadKeyW.
 */
LONG WINAPI RegUnLoadKeyA( HKEY hkey, LPCSTR lpSubKey )
{
    UNICODE_STRING lpSubKeyW;
    LONG ret;

    RtlCreateUnicodeStringFromAsciiz( &lpSubKeyW, lpSubKey );
    ret = RegUnLoadKeyW( hkey, lpSubKeyW.Buffer );
    RtlFreeUnicodeString( &lpSubKeyW );
    return ret;
}


/******************************************************************************
 * RegReplaceKeyW [ADVAPI32.@]
 *
 * PARAMS
 *  hkey      [I] Handle of open key
 *  lpSubKey  [I] Address of name of subkey
 *  lpNewFile [I] Address of filename for file with new data
 *  lpOldFile [I] Address of filename for backup file
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegReplaceKeyW( HKEY hkey, LPCWSTR lpSubKey, LPCWSTR lpNewFile,
                              LPCWSTR lpOldFile )
{
    FIXME("(%p,%s,%s,%s): stub\n", hkey, debugstr_w(lpSubKey),
          debugstr_w(lpNewFile),debugstr_w(lpOldFile));
    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegReplaceKeyA [ADVAPI32.@]
 *
 * See RegReplaceKeyW.
 */
LONG WINAPI RegReplaceKeyA( HKEY hkey, LPCSTR lpSubKey, LPCSTR lpNewFile,
                              LPCSTR lpOldFile )
{
    UNICODE_STRING lpSubKeyW;
    UNICODE_STRING lpNewFileW;
    UNICODE_STRING lpOldFileW;
    LONG ret;

    RtlCreateUnicodeStringFromAsciiz( &lpSubKeyW, lpSubKey );
    RtlCreateUnicodeStringFromAsciiz( &lpOldFileW, lpOldFile );
    RtlCreateUnicodeStringFromAsciiz( &lpNewFileW, lpNewFile );
    ret = RegReplaceKeyW( hkey, lpSubKeyW.Buffer, lpNewFileW.Buffer, lpOldFileW.Buffer );
    RtlFreeUnicodeString( &lpOldFileW );
    RtlFreeUnicodeString( &lpNewFileW );
    RtlFreeUnicodeString( &lpSubKeyW );
    return ret;
}


/******************************************************************************
 * RegSetKeySecurity [ADVAPI32.@]
 *
 * PARAMS
 *  hkey          [I] Open handle of key to set
 *  SecurityInfo  [I] Descriptor contents
 *  pSecurityDesc [I] Address of descriptor for key
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegSetKeySecurity( HKEY hkey, SECURITY_INFORMATION SecurityInfo,
                               PSECURITY_DESCRIPTOR pSecurityDesc )
{
    TRACE("(%p,%ld,%p)\n",hkey,SecurityInfo,pSecurityDesc);

    /* It seems to perform this check before the hkey check */
    if ((SecurityInfo & OWNER_SECURITY_INFORMATION) ||
        (SecurityInfo & GROUP_SECURITY_INFORMATION) ||
        (SecurityInfo & DACL_SECURITY_INFORMATION) ||
        (SecurityInfo & SACL_SECURITY_INFORMATION)) {
        /* Param OK */
    } else
        return ERROR_INVALID_PARAMETER;

    if (!pSecurityDesc)
        return ERROR_INVALID_PARAMETER;

    FIXME(":(%p,%ld,%p): stub\n",hkey,SecurityInfo,pSecurityDesc);

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegGetKeySecurity [ADVAPI32.@]
 *
 * Get a copy of the security descriptor for a given registry key.
 *
 * PARAMS
 *  hkey                   [I]   Open handle of key to set
 *  SecurityInformation    [I]   Descriptor contents
 *  pSecurityDescriptor    [O]   Address of descriptor for key
 *  lpcbSecurityDescriptor [I/O] Address of size of buffer and description
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: Error code
 */
LONG WINAPI RegGetKeySecurity( HKEY hkey, SECURITY_INFORMATION SecurityInformation,
                               PSECURITY_DESCRIPTOR pSecurityDescriptor,
                               LPDWORD lpcbSecurityDescriptor )
{
    TRACE("(%p,%ld,%p,%ld)\n",hkey,SecurityInformation,pSecurityDescriptor,
          lpcbSecurityDescriptor?*lpcbSecurityDescriptor:0);

    /* FIXME: Check for valid SecurityInformation values */

    if (*lpcbSecurityDescriptor < sizeof(SECURITY_DESCRIPTOR))
        return ERROR_INSUFFICIENT_BUFFER;

    FIXME("(%p,%ld,%p,%ld): stub\n",hkey,SecurityInformation,
          pSecurityDescriptor,lpcbSecurityDescriptor?*lpcbSecurityDescriptor:0);

    /* Do not leave security descriptor filled with garbage */
    RtlCreateSecurityDescriptor(pSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);

    return ERROR_SUCCESS;
}


/******************************************************************************
 * RegFlushKey [ADVAPI32.@]
 * 
 * Immediately write a registry key to registry.
 *
 * PARAMS
 *  hkey [I] Handle of key to write
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: Error code
 */
DWORD WINAPI RegFlushKey( HKEY hkey )
{
    hkey = get_special_root_hkey( hkey );
    if (!hkey) return ERROR_INVALID_HANDLE;

    return RtlNtStatusToDosError( NtFlushKey( hkey ) );
}


/******************************************************************************
 * RegConnectRegistryW [ADVAPI32.@]
 *
 * PARAMS
 *  lpMachineName [I] Address of name of remote computer
 *  hHey          [I] Predefined registry handle
 *  phkResult     [I] Address of buffer for remote registry handle
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegConnectRegistryW( LPCWSTR lpMachineName, HKEY hKey,
                                   PHKEY phkResult )
{
    LONG ret;

    TRACE("(%s,%p,%p): stub\n",debugstr_w(lpMachineName),hKey,phkResult);

    if (!lpMachineName || !*lpMachineName) {
        /* Use the local machine name */
        ret = RegOpenKeyW( hKey, NULL, phkResult );
    }
    else {
        WCHAR compName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD len = sizeof(compName) / sizeof(WCHAR);

        /* MSDN says lpMachineName must start with \\ : not so */
        if( lpMachineName[0] == '\\' &&  lpMachineName[1] == '\\')
            lpMachineName += 2;
        if (GetComputerNameW(compName, &len))
        {
            if (!strcmpiW(lpMachineName, compName))
                ret = RegOpenKeyW(hKey, NULL, phkResult);
            else
            {
                FIXME("Connect to %s is not supported.\n",debugstr_w(lpMachineName));
                ret = ERROR_BAD_NETPATH;
            }
        }
        else
            ret = GetLastError();
    }
    return ret;
}


/******************************************************************************
 * RegConnectRegistryA [ADVAPI32.@]
 *
 * See RegConnectRegistryW.
 */
LONG WINAPI RegConnectRegistryA( LPCSTR machine, HKEY hkey, PHKEY reskey )
{
    UNICODE_STRING machineW;
    LONG ret;

    RtlCreateUnicodeStringFromAsciiz( &machineW, machine );
    ret = RegConnectRegistryW( machineW.Buffer, hkey, reskey );
    RtlFreeUnicodeString( &machineW );
    return ret;
}


/******************************************************************************
 * RegNotifyChangeKeyValue [ADVAPI32.@]
 *
 * PARAMS
 *  hkey            [I] Handle of key to watch
 *  fWatchSubTree   [I] Flag for subkey notification
 *  fdwNotifyFilter [I] Changes to be reported
 *  hEvent          [I] Handle of signaled event
 *  fAsync          [I] Flag for asynchronous reporting
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 */
LONG WINAPI RegNotifyChangeKeyValue( HKEY hkey, BOOL fWatchSubTree,
                                     DWORD fdwNotifyFilter, HANDLE hEvent,
                                     BOOL fAsync )
{
    NTSTATUS status;
    IO_STATUS_BLOCK iosb;

    hkey = get_special_root_hkey( hkey );
    if (!hkey) return ERROR_INVALID_HANDLE;

    TRACE("(%p,%i,%ld,%p,%i)\n", hkey, fWatchSubTree, fdwNotifyFilter,
          hEvent, fAsync);

    status = NtNotifyChangeKey( hkey, hEvent, NULL, NULL, &iosb,
                                fdwNotifyFilter, fWatchSubTree, NULL, 0,
                                fAsync );

    if (status && status != STATUS_TIMEOUT)
        return RtlNtStatusToDosError( status );

    return ERROR_SUCCESS;
}

/******************************************************************************
 * RegOpenUserClassesRoot [ADVAPI32.@]
 *
 * Open the HKEY_CLASSES_ROOT key for a user.
 *
 * PARAMS
 *  hToken     [I] Handle of token representing the user
 *  dwOptions  [I] Reserved, nust be 0
 *  samDesired [I] Desired access rights
 *  phkResult  [O] Destination for the resulting key handle
 *
 * RETURNS
 *  Success: ERROR_SUCCESS
 *  Failure: nonzero error code from Winerror.h
 * 
 * NOTES
 *  On Windows 2000 and upwards the HKEY_CLASSES_ROOT key is a view of the
 *  "HKEY_LOCAL_MACHINE\Software\Classes" and the
 *  "HKEY_CURRENT_USER\Software\Classes" keys merged together.
 */
LONG WINAPI RegOpenUserClassesRoot(
    HANDLE hToken,
    DWORD dwOptions,
    REGSAM samDesired,
    PHKEY phkResult
)
{
    FIXME("(%p, 0x%lx, 0x%lx, %p) semi-stub\n", hToken, dwOptions, samDesired, phkResult);

    *phkResult = HKEY_CLASSES_ROOT;
    return ERROR_SUCCESS;
}
