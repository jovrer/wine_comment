/*
 * File handling functions
 *
 * Copyright 1993 John Burton
 * Copyright 1996 Alexandre Julliard
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
 *
 * TODO:
 *    Fix the CopyFileEx methods to implement the "extended" functionality.
 *    Right now, they simply call the CopyFile method.
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "winerror.h"
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/winbase16.h"
#include "wine/server.h"
#include "kernel_private.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(file);


/***********************************************************************
 *           GetProfileInt   (KERNEL.57)
 */
UINT16 WINAPI GetProfileInt16( LPCSTR section, LPCSTR entry, INT16 def_val )
{
    return GetPrivateProfileInt16( section, entry, def_val, "win.ini" );
}


/***********************************************************************
 *           GetProfileString   (KERNEL.58)
 */
INT16 WINAPI GetProfileString16( LPCSTR section, LPCSTR entry, LPCSTR def_val,
                                 LPSTR buffer, UINT16 len )
{
    return GetPrivateProfileString16( section, entry, def_val,
                                      buffer, len, "win.ini" );
}


/***********************************************************************
 *           WriteProfileString   (KERNEL.59)
 */
BOOL16 WINAPI WriteProfileString16( LPCSTR section, LPCSTR entry,
                                    LPCSTR string )
{
    return WritePrivateProfileString16( section, entry, string, "win.ini" );
}


/* get the search path for the current module; helper for OpenFile16 */
static char *get_search_path(void)
{
    UINT len;
    char *ret, *p, module[OFS_MAXPATHNAME];

    module[0] = 0;
    if (GetCurrentTask() && GetModuleFileName16( GetCurrentTask(), module, sizeof(module) ))
    {
        if (!(p = strrchr( module, '\\' ))) p = module;
        *p = 0;
    }

    len = (2 +                                              /* search order: first current dir */
           GetSystemDirectoryA( NULL, 0 ) + 1 +             /* then system dir */
           GetWindowsDirectoryA( NULL, 0 ) + 1 +            /* then windows dir */
           strlen( module ) + 1 +                           /* then module path */
           GetEnvironmentVariableA( "PATH", NULL, 0 ) + 1); /* then look in PATH */
    if (!(ret = HeapAlloc( GetProcessHeap(), 0, len ))) return NULL;
    strcpy( ret, ".;" );
    p = ret + 2;
    GetSystemDirectoryA( p, ret + len - p );
    p += strlen( p );
    *p++ = ';';
    GetWindowsDirectoryA( p, ret + len - p );
    p += strlen( p );
    *p++ = ';';
    if (module[0])
    {
        strcpy( p, module );
        p += strlen( p );
        *p++ = ';';
    }
    GetEnvironmentVariableA( "PATH", p, ret + len - p );
    return ret;
}

/***********************************************************************
 *           OpenFile   (KERNEL.74)
 *           OpenFileEx (KERNEL.360)
 */
HFILE16 WINAPI OpenFile16( LPCSTR name, OFSTRUCT *ofs, UINT16 mode )
{
    HFILE hFileRet;
    HANDLE handle;
    FILETIME filetime;
    WORD filedatetime[2];
    const char *p, *filename;

    if (!ofs) return HFILE_ERROR;

    TRACE("%s %s %s %s%s%s%s%s%s%s%s%s\n",debugstr_a(name),
          ((mode & 0x3 )==OF_READ)?"OF_READ":
          ((mode & 0x3 )==OF_WRITE)?"OF_WRITE":
          ((mode & 0x3 )==OF_READWRITE)?"OF_READWRITE":"unknown",
          ((mode & 0x70 )==OF_SHARE_COMPAT)?"OF_SHARE_COMPAT":
          ((mode & 0x70 )==OF_SHARE_DENY_NONE)?"OF_SHARE_DENY_NONE":
          ((mode & 0x70 )==OF_SHARE_DENY_READ)?"OF_SHARE_DENY_READ":
          ((mode & 0x70 )==OF_SHARE_DENY_WRITE)?"OF_SHARE_DENY_WRITE":
          ((mode & 0x70 )==OF_SHARE_EXCLUSIVE)?"OF_SHARE_EXCLUSIVE":"unknown",
          ((mode & OF_PARSE )==OF_PARSE)?"OF_PARSE ":"",
          ((mode & OF_DELETE )==OF_DELETE)?"OF_DELETE ":"",
          ((mode & OF_VERIFY )==OF_VERIFY)?"OF_VERIFY ":"",
          ((mode & OF_SEARCH )==OF_SEARCH)?"OF_SEARCH ":"",
          ((mode & OF_CANCEL )==OF_CANCEL)?"OF_CANCEL ":"",
          ((mode & OF_CREATE )==OF_CREATE)?"OF_CREATE ":"",
          ((mode & OF_PROMPT )==OF_PROMPT)?"OF_PROMPT ":"",
          ((mode & OF_EXIST )==OF_EXIST)?"OF_EXIST ":"",
          ((mode & OF_REOPEN )==OF_REOPEN)?"OF_REOPEN ":""
        );

    if (mode & OF_PARSE)
    {
        OpenFile( name, ofs, mode );
        return 0;
    }

    if (mode & OF_CREATE)
    {
        handle = (HANDLE)OpenFile( name, ofs, mode );
        if (handle == (HANDLE)HFILE_ERROR) goto error;
    }
    else
    {
        ofs->cBytes = sizeof(OFSTRUCT);
        ofs->nErrCode = 0;
        if (mode & OF_REOPEN) name = ofs->szPathName;

        if (!name) return HFILE_ERROR;

        /* the watcom 10.6 IDE relies on a valid path returned in ofs->szPathName
           Are there any cases where getting the path here is wrong?
           Uwe Bonnes 1997 Apr 2 */
        if (!GetFullPathNameA( name, sizeof(ofs->szPathName), ofs->szPathName, NULL )) goto error;

        /* If OF_SEARCH is set, ignore the given path */

        filename = name;
        if ((mode & OF_SEARCH) && !(mode & OF_REOPEN))
        {
            /* First try the file name as is */
            if (GetFileAttributesA( filename ) != INVALID_FILE_ATTRIBUTES) filename = NULL;
            else
            {
                /* Now remove the path */
                if (filename[0] && (filename[1] == ':')) filename += 2;
                if ((p = strrchr( filename, '\\' ))) filename = p + 1;
                if ((p = strrchr( filename, '/' ))) filename = p + 1;
                if (!filename[0])
                {
                    SetLastError( ERROR_FILE_NOT_FOUND );
                    goto error;
                }
            }
        }

        /* Now look for the file */

        if (filename)
        {
            BOOL found;
            char *path = get_search_path();

            if (!path) goto error;
            found = SearchPathA( path, filename, NULL, sizeof(ofs->szPathName),
                                 ofs->szPathName, NULL );
            HeapFree( GetProcessHeap(), 0, path );
            if (!found) goto error;
        }

        TRACE("found %s\n", debugstr_a(ofs->szPathName) );

        if (mode & OF_DELETE)
        {
            if (!DeleteFileA( ofs->szPathName )) goto error;
            TRACE("(%s): OF_DELETE return = OK\n", name);
            return 1;
        }

        handle = (HANDLE)_lopen( ofs->szPathName, mode );
        if (handle == INVALID_HANDLE_VALUE) goto error;

        GetFileTime( handle, NULL, NULL, &filetime );
        FileTimeToDosDateTime( &filetime, &filedatetime[0], &filedatetime[1] );
        if ((mode & OF_VERIFY) && (mode & OF_REOPEN))
        {
            if (ofs->Reserved1 != filedatetime[0] || ofs->Reserved2 != filedatetime[1] )
            {
                CloseHandle( handle );
                WARN("(%s): OF_VERIFY failed\n", name );
                /* FIXME: what error here? */
                SetLastError( ERROR_FILE_NOT_FOUND );
                goto error;
            }
        }
        ofs->Reserved1 = filedatetime[0];
        ofs->Reserved2 = filedatetime[1];
    }

    TRACE("(%s): OK, return = %p\n", name, handle );
    hFileRet = Win32HandleToDosFileHandle( handle );
    if (hFileRet == HFILE_ERROR16) goto error;
    if (mode & OF_EXIST) _lclose16( hFileRet ); /* Return the handle, but close it first */
    return hFileRet;

error:  /* We get here if there was an error opening the file */
    ofs->nErrCode = GetLastError();
    WARN("(%s): return = HFILE_ERROR error= %d\n", name,ofs->nErrCode );
    return HFILE_ERROR16;
}


/***********************************************************************
 *           _lclose   (KERNEL.81)
 */
HFILE16 WINAPI _lclose16( HFILE16 hFile )
{
    if ((hFile >= DOS_TABLE_SIZE) || !dos_handles[hFile])
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return HFILE_ERROR16;
    }
    TRACE("%d (handle32=%p)\n", hFile, dos_handles[hFile] );
    CloseHandle( dos_handles[hFile] );
    dos_handles[hFile] = 0;
    return 0;
}

/***********************************************************************
 *           _lcreat   (KERNEL.83)
 */
HFILE16 WINAPI _lcreat16( LPCSTR path, INT16 attr )
{
    return Win32HandleToDosFileHandle( (HANDLE)_lcreat( path, attr ) );
}

/***********************************************************************
 *           _llseek   (KERNEL.84)
 *
 * FIXME:
 *   Seeking before the start of the file should be allowed for _llseek16,
 *   but cause subsequent I/O operations to fail (cf. interrupt list)
 *
 */
LONG WINAPI _llseek16( HFILE16 hFile, LONG lOffset, INT16 nOrigin )
{
    return SetFilePointer( DosFileHandleToWin32Handle(hFile), lOffset, NULL, nOrigin );
}


/***********************************************************************
 *           _lopen   (KERNEL.85)
 */
HFILE16 WINAPI _lopen16( LPCSTR path, INT16 mode )
{
    return Win32HandleToDosFileHandle( (HANDLE)_lopen( path, mode ) );
}


/***********************************************************************
 *           _lread16   (KERNEL.82)
 */
UINT16 WINAPI _lread16( HFILE16 hFile, LPVOID buffer, UINT16 count )
{
    return (UINT16)_lread((HFILE)DosFileHandleToWin32Handle(hFile), buffer, (LONG)count );
}


/***********************************************************************
 *           _lwrite   (KERNEL.86)
 */
UINT16 WINAPI _lwrite16( HFILE16 hFile, LPCSTR buffer, UINT16 count )
{
    return (UINT16)_hwrite( (HFILE)DosFileHandleToWin32Handle(hFile), buffer, (LONG)count );
}

/***********************************************************************
 *           _hread (KERNEL.349)
 */
LONG WINAPI WIN16_hread( HFILE16 hFile, SEGPTR buffer, LONG count )
{
    LONG maxlen;

    TRACE("%d %08lx %ld\n", hFile, (DWORD)buffer, count );

    /* Some programs pass a count larger than the allocated buffer */
    maxlen = GetSelectorLimit16( SELECTOROF(buffer) ) - OFFSETOF(buffer) + 1;
    if (count > maxlen) count = maxlen;
    return _lread((HFILE)DosFileHandleToWin32Handle(hFile), MapSL(buffer), count );
}


/***********************************************************************
 *           _lread (KERNEL.82)
 */
UINT16 WINAPI WIN16_lread( HFILE16 hFile, SEGPTR buffer, UINT16 count )
{
    return (UINT16)WIN16_hread( hFile, buffer, (LONG)count );
}


/***********************************************************************
 *           GetTempDrive   (KERNEL.92)
 * A closer look at krnl386.exe shows what the SDK doesn't mention:
 *
 * returns:
 *   AL: driveletter
 *   AH: ':'		- yes, some kernel code even does stosw with
 *                            the returned AX.
 *   DX: 1 for success
 */
UINT WINAPI GetTempDrive( BYTE ignored )
{
    WCHAR buffer[8];
    BYTE ret;

    if (GetTempPathW( 8, buffer )) ret = (BYTE)toupperW(buffer[0]);
    else ret = 'C';
    return MAKELONG( ret | (':' << 8), 1 );
}


/***********************************************************************
 *           GetTempFileName   (KERNEL.97)
 */
UINT16 WINAPI GetTempFileName16( BYTE drive, LPCSTR prefix, UINT16 unique,
                                 LPSTR buffer )
{
    char temppath[MAX_PATH];
    char *prefix16 = NULL;
    UINT16 ret;

    if (!(drive & ~TF_FORCEDRIVE)) /* drive 0 means current default drive */
    {
        GetCurrentDirectoryA(sizeof(temppath), temppath); 
        drive |= temppath[0];
    }

    if (drive & TF_FORCEDRIVE)
    {
        char    d[3];

        d[0] = drive & ~TF_FORCEDRIVE;
        d[1] = ':';
        d[2] = '\0';
        if (GetDriveTypeA(d) == DRIVE_NO_ROOT_DIR)
        {
            drive &= ~TF_FORCEDRIVE;
            WARN("invalid drive %d specified\n", drive );
        }
    }

    if (drive & TF_FORCEDRIVE)
        sprintf(temppath,"%c:", drive & ~TF_FORCEDRIVE );
    else
        GetTempPathA( MAX_PATH, temppath );

    if (prefix)
    {
        prefix16 = HeapAlloc(GetProcessHeap(), 0, strlen(prefix) + 2);
        *prefix16 = '~';
        strcpy(prefix16 + 1, prefix);
    }

    ret = GetTempFileNameA( temppath, prefix16, unique, buffer );

    HeapFree(GetProcessHeap(), 0, prefix16);
    return ret;
}


/***********************************************************************
 *           GetPrivateProfileInt   (KERNEL.127)
 */
UINT16 WINAPI GetPrivateProfileInt16( LPCSTR section, LPCSTR entry,
                                      INT16 def_val, LPCSTR filename )
{
    /* we used to have some elaborate return value limitation (<= -32768 etc.)
     * here, but Win98SE doesn't care about this at all, so I deleted it.
     * AFAIR versions prior to Win9x had these limits, though. */
    return (INT16)GetPrivateProfileIntA(section,entry,def_val,filename);
}


/***********************************************************************
 *           WritePrivateProfileString   (KERNEL.129)
 */
BOOL16 WINAPI WritePrivateProfileString16( LPCSTR section, LPCSTR entry,
                                           LPCSTR string, LPCSTR filename )
{
    return WritePrivateProfileStringA(section,entry,string,filename);
}


/***********************************************************************
 *           GetWindowsDirectory   (KERNEL.134)
 */
UINT16 WINAPI GetWindowsDirectory16( LPSTR path, UINT16 count )
{
    return GetWindowsDirectoryA( path, count );
}


/***********************************************************************
 *           GetSystemDirectory   (KERNEL.135)
 */
UINT16 WINAPI GetSystemDirectory16( LPSTR path, UINT16 count )
{
    return GetSystemDirectoryA( path, count );
}


/***********************************************************************
 *           GetDriveType   (KERNEL.136)
 * Get the type of a drive in Win16.
 *
 * RETURNS
 *  The type of the Drive. For a list see GetDriveTypeW from kernel32.
 *
 * NOTES
 *  Note that it returns DRIVE_REMOTE for CD-ROMs, since MSCDEX uses the
 *  remote drive API. The return value DRIVE_REMOTE for CD-ROMs has been
 *  verified on Win 3.11 and Windows 95. Some programs rely on it, so don't
 *  do any pseudo-clever changes.
 */
UINT16 WINAPI GetDriveType16( UINT16 drive ) /* [in] number (NOT letter) of drive */
{
    UINT type;
    WCHAR root[3];

    root[0] = 'A' + drive;
    root[1] = ':';
    root[2] = 0;
    type = GetDriveTypeW( root );
    if (type == DRIVE_CDROM) type = DRIVE_REMOTE;
    else if (type == DRIVE_NO_ROOT_DIR) type = DRIVE_UNKNOWN;
    return type;
}


/***********************************************************************
 *           GetProfileSectionNames   (KERNEL.142)
 */
WORD WINAPI GetProfileSectionNames16(LPSTR buffer, WORD size)

{
    return GetPrivateProfileSectionNamesA(buffer,size,"win.ini");
}


/***********************************************************************
 *           GetPrivateProfileSectionNames   (KERNEL.143)
 */
WORD WINAPI GetPrivateProfileSectionNames16( LPSTR buffer, WORD size,
                                             LPCSTR filename )
{
    return GetPrivateProfileSectionNamesA(buffer,size,filename);
}


/***********************************************************************
 *           CreateDirectory   (KERNEL.144)
 */
BOOL16 WINAPI CreateDirectory16( LPCSTR path, LPVOID dummy )
{
    return CreateDirectoryA( path, NULL );
}


/***********************************************************************
 *           RemoveDirectory   (KERNEL.145)
 */
BOOL16 WINAPI RemoveDirectory16( LPCSTR path )
{
    return RemoveDirectoryA( path );
}


/***********************************************************************
 *           DeleteFile   (KERNEL.146)
 */
BOOL16 WINAPI DeleteFile16( LPCSTR path )
{
    return DeleteFileA( path );
}


/***********************************************************************
 *           SetHandleCount   (KERNEL.199)
 */
UINT16 WINAPI SetHandleCount16( UINT16 count )
{
    return SetHandleCount( count );
}


/***********************************************************************
 *           _hread16   (KERNEL.349)
 */
LONG WINAPI _hread16( HFILE16 hFile, LPVOID buffer, LONG count)
{
    return _lread( (HFILE)DosFileHandleToWin32Handle(hFile), buffer, count );
}


/***********************************************************************
 *           _hwrite   (KERNEL.350)
 */
LONG WINAPI _hwrite16( HFILE16 hFile, LPCSTR buffer, LONG count )
{
    return _hwrite( (HFILE)DosFileHandleToWin32Handle(hFile), buffer, count );
}


/***********************************************************************
 *           WritePrivateProfileStruct (KERNEL.406)
 */
BOOL16 WINAPI WritePrivateProfileStruct16 (LPCSTR section, LPCSTR key,
                                           LPVOID buf, UINT16 bufsize, LPCSTR filename)
{
    return WritePrivateProfileStructA( section, key, buf, bufsize, filename );
}


/***********************************************************************
 *           GetPrivateProfileStruct (KERNEL.407)
 */
BOOL16 WINAPI GetPrivateProfileStruct16(LPCSTR section, LPCSTR key,
                                        LPVOID buf, UINT16 len, LPCSTR filename)
{
    return GetPrivateProfileStructA( section, key, buf, len, filename );
}


/***********************************************************************
 *           GetCurrentDirectory   (KERNEL.411)
 */
UINT16 WINAPI GetCurrentDirectory16( UINT16 buflen, LPSTR buf )
{
    return GetCurrentDirectoryA( buflen, buf );
}


/***********************************************************************
 *           SetCurrentDirectory   (KERNEL.412)
 */
BOOL16 WINAPI SetCurrentDirectory16( LPCSTR dir )
{
    char fulldir[MAX_PATH];

    if (!GetFullPathNameA( dir, MAX_PATH, fulldir, NULL )) return FALSE;

    if (!SetCurrentDirectoryA( dir )) return FALSE;

    if (fulldir[0] && fulldir[1] == ':')
    {
        TDB *pTask = GlobalLock16( GetCurrentTask() );
        char env_var[4] = "=A:";

        env_var[1] = fulldir[0];
        SetEnvironmentVariableA( env_var, fulldir );

        /* update the directory in the TDB */
        if (pTask)
        {
            pTask->curdrive = 0x80 | (fulldir[0] - 'A');
            GetShortPathNameA( fulldir + 2, pTask->curdir, sizeof(pTask->curdir) );
        }
    }
    return TRUE;
}


/*************************************************************************
 *           FindFirstFile   (KERNEL.413)
 */
HANDLE16 WINAPI FindFirstFile16( LPCSTR path, WIN32_FIND_DATAA *data )
{
    HGLOBAL16 h16;
    HANDLE handle, *ptr;

    if (!(h16 = GlobalAlloc16( GMEM_MOVEABLE, sizeof(handle) ))) return INVALID_HANDLE_VALUE16;
    ptr = GlobalLock16( h16 );
    *ptr = handle = FindFirstFileA( path, data );
    GlobalUnlock16( h16 );

    if (handle == INVALID_HANDLE_VALUE)
    {
        GlobalFree16( h16 );
        h16 = INVALID_HANDLE_VALUE16;
    }
    return h16;
}


/*************************************************************************
 *           FindNextFile   (KERNEL.414)
 */
BOOL16 WINAPI FindNextFile16( HANDLE16 handle, WIN32_FIND_DATAA *data )
{
    HANDLE *ptr;
    BOOL ret = FALSE;

    if ((handle == INVALID_HANDLE_VALUE16) || !(ptr = GlobalLock16( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return ret;
    }
    ret = FindNextFileA( *ptr, data );
    GlobalUnlock16( handle );
    return ret;
}


/*************************************************************************
 *           FindClose   (KERNEL.415)
 */
BOOL16 WINAPI FindClose16( HANDLE16 handle )
{
    HANDLE *ptr;

    if ((handle == INVALID_HANDLE_VALUE16) || !(ptr = GlobalLock16( handle )))
    {
        SetLastError( ERROR_INVALID_HANDLE );
        return FALSE;
    }
    FindClose( *ptr );
    GlobalUnlock16( handle );
    GlobalFree16( handle );
    return TRUE;
}


/***********************************************************************
 *           WritePrivateProfileSection   (KERNEL.416)
 */
BOOL16 WINAPI WritePrivateProfileSection16( LPCSTR section,
                                            LPCSTR string, LPCSTR filename )
{
    return WritePrivateProfileSectionA( section, string, filename );
}


/***********************************************************************
 *           WriteProfileSection   (KERNEL.417)
 */
BOOL16 WINAPI WriteProfileSection16( LPCSTR section, LPCSTR keys_n_values)
{
    return WritePrivateProfileSection16( section, keys_n_values, "win.ini");
}


/***********************************************************************
 *           GetPrivateProfileSection   (KERNEL.418)
 */
INT16 WINAPI GetPrivateProfileSection16( LPCSTR section, LPSTR buffer,
                                         UINT16 len, LPCSTR filename )
{
    return GetPrivateProfileSectionA( section, buffer, len, filename );
}


/***********************************************************************
 *           GetProfileSection   (KERNEL.419)
 */
INT16 WINAPI GetProfileSection16( LPCSTR section, LPSTR buffer, UINT16 len )
{
    return GetPrivateProfileSection16( section, buffer, len, "win.ini" );
}


/**************************************************************************
 *           GetFileAttributes   (KERNEL.420)
 */
DWORD WINAPI GetFileAttributes16( LPCSTR name )
{
    return GetFileAttributesA( name );
}


/**************************************************************************
 *              SetFileAttributes	(KERNEL.421)
 */
BOOL16 WINAPI SetFileAttributes16( LPCSTR lpFileName, DWORD attributes )
{
    return SetFileAttributesA( lpFileName, attributes );
}


/***********************************************************************
 *           GetDiskFreeSpace   (KERNEL.422)
 */
BOOL16 WINAPI GetDiskFreeSpace16( LPCSTR root, LPDWORD cluster_sectors,
                                  LPDWORD sector_bytes, LPDWORD free_clusters,
                                  LPDWORD total_clusters )
{
    return GetDiskFreeSpaceA( root, cluster_sectors, sector_bytes,
                                free_clusters, total_clusters );
}
