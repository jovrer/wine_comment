/*
 * Copyright 1999 Ian Schmidt
 * Copyright 2001 Travis Michielsen
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

/***********************************************************************
 *
 *  TODO:
 *  - Reference counting
 *  - Thread-safing
 */

#include "config.h"
#include "wine/port.h"

#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "crypt.h"
#include "winnls.h"
#include "winreg.h"
#include "wine/debug.h"
#include "wine/unicode.h"
#include "winternl.h"
#include "ntstatus.h"

WINE_DEFAULT_DEBUG_CHANNEL(crypt);

HWND crypt_hWindow = 0;

#define CRYPT_ReturnLastError(err) do {SetLastError(err); return FALSE;} while(0)

#define CRYPT_Alloc(size) ((LPVOID)LocalAlloc(LMEM_ZEROINIT, size))
#define CRYPT_Free(buffer) (LocalFree((HLOCAL)buffer))

static inline PWSTR CRYPT_GetProvKeyName(PCWSTR pProvName)
{
	static const WCHAR KEYSTR[] = {
                'S','o','f','t','w','a','r','e','\\',
                'M','i','c','r','o','s','o','f','t','\\',
                'C','r','y','p','t','o','g','r','a','p','h','y','\\',
                'D','e','f','a','u','l','t','s','\\',
                'P','r','o','v','i','d','e','r','\\',0
	};
	PWSTR keyname;

	keyname = CRYPT_Alloc((strlenW(KEYSTR) + strlenW(pProvName) +1)*sizeof(WCHAR));
	if (keyname)
	{
		strcpyW(keyname, KEYSTR);
		strcpyW(keyname + strlenW(KEYSTR), pProvName);
	} else
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return keyname;
}

static inline PWSTR CRYPT_GetTypeKeyName(DWORD dwType, BOOL user)
{
	static const WCHAR MACHINESTR[] = {
                'S','o','f','t','w','a','r','e','\\',
                'M','i','c','r','o','s','o','f','t','\\',
                'C','r','y','p','t','o','g','r','a','p','h','y','\\',
                'D','e','f','a','u','l','t','s','\\',
                'P','r','o','v','i','d','e','r',' ','T','y','p','e','s','\\',
                'T','y','p','e',' ','X','X','X',0
	};
	static const WCHAR USERSTR[] = {
                'S','o','f','t','w','a','r','e','\\',
                'M','i','c','r','o','s','o','f','t','\\',
                'C','r','y','p','t','o','g','r','a','p','h','y','\\',
                'P','r','o','v','i','d','e','r',' ','T','y','p','e',' ','X','X','X',0
	};
	PWSTR keyname;
	PWSTR ptr;

	keyname = CRYPT_Alloc( ((user ? strlenW(USERSTR) : strlenW(MACHINESTR)) +1)*sizeof(WCHAR));
	if (keyname)
	{
		user ? strcpyW(keyname, USERSTR) : strcpyW(keyname, MACHINESTR);
		ptr = keyname + strlenW(keyname);
		*(--ptr) = (dwType % 10) + '0';
		*(--ptr) = ((dwType / 10) % 10) + '0';
		*(--ptr) = (dwType / 100) + '0';
	} else
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return keyname;
}

/* CRYPT_UnicodeTOANSI
 * wstr - unicode string
 * str - pointer to ANSI string
 * strsize - size of buffer pointed to by str or -1 if we have to do the allocation
 *
 * returns TRUE if unsuccessfull, FALSE otherwise.
 * if wstr is NULL, returns TRUE and sets str to NULL! Value of str should be checked after call
 */
static inline BOOL CRYPT_UnicodeToANSI(LPCWSTR wstr, LPSTR* str, int strsize)
{
	int count;

	if (!wstr)
	{
		*str = NULL;
		return TRUE;
	}
	count = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (strsize == -1)
		*str = CRYPT_Alloc(count * sizeof(CHAR));
	else
		count = min( count, strsize );
	if (*str)
	{
		WideCharToMultiByte(CP_ACP, 0, wstr, -1, *str, count, NULL, NULL);
		return TRUE;
	}
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
}

/* CRYPT_ANSITOUnicode
 * str - ANSI string
 * wstr - pointer to unicode string
 * wstrsize - size of buffer pointed to by wstr or -1 if we have to do the allocation
 */
static inline BOOL CRYPT_ANSIToUnicode(LPCSTR str, LPWSTR* wstr, int wstrsize)
{
	int wcount;

	if (!str)
	{
		*wstr = NULL;
		return TRUE;
	}
	wcount = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0);
	if (wstrsize == -1)
		*wstr = CRYPT_Alloc(wcount * sizeof(WCHAR));
	else
		wcount = min( wcount, wstrsize/sizeof(WCHAR) );
	if (*wstr)
	{
		MultiByteToWideChar(CP_ACP, 0, str, -1, *wstr, wcount);
		return TRUE;
	}
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
	return FALSE;
}

/* These next 2 functions are used by the VTableProvStruc structure */
static BOOL CALLBACK CRYPT_VerifyImage(LPCSTR lpszImage, BYTE* pData)
{
	if (!lpszImage || !pData)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	FIXME("(%s, %p): not verifying image\n", lpszImage, pData);

	return TRUE;
}

static BOOL CALLBACK CRYPT_ReturnhWnd(HWND *phWnd)
{
	if (!phWnd)
		return FALSE;
	*phWnd = crypt_hWindow;
	return TRUE;
}

#define CRYPT_GetProvFunc(name) \
	if ( !(provider->pFuncs->p##name = (void*)GetProcAddress(provider->hModule, #name)) ) goto error
#define CRYPT_GetProvFuncOpt(name) \
	provider->pFuncs->p##name = (void*)GetProcAddress(provider->hModule, #name)
static PCRYPTPROV CRYPT_LoadProvider(PWSTR pImage)
{
	PCRYPTPROV provider;
	DWORD errorcode = ERROR_NOT_ENOUGH_MEMORY;

	if ( !(provider = CRYPT_Alloc(sizeof(CRYPTPROV))) ) goto error;
	if ( !(provider->pFuncs = CRYPT_Alloc(sizeof(PROVFUNCS))) ) goto error;
	if ( !(provider->pVTable = CRYPT_Alloc(sizeof(VTableProvStruc))) ) goto error;
	if ( !(provider->hModule = LoadLibraryW(pImage)) )
	{
		errorcode = (GetLastError() == ERROR_FILE_NOT_FOUND) ? NTE_PROV_DLL_NOT_FOUND : NTE_PROVIDER_DLL_FAIL;
		FIXME("Failed to load dll %s\n", debugstr_w(pImage));
		goto error;
	}
	provider->dwMagic = MAGIC_CRYPTPROV;
	provider->refcount = 1;

	errorcode = NTE_PROVIDER_DLL_FAIL;
	CRYPT_GetProvFunc(CPAcquireContext);
	CRYPT_GetProvFunc(CPCreateHash);
	CRYPT_GetProvFunc(CPDecrypt);
	CRYPT_GetProvFunc(CPDeriveKey);
	CRYPT_GetProvFunc(CPDestroyHash);
	CRYPT_GetProvFunc(CPDestroyKey);
	CRYPT_GetProvFuncOpt(CPDuplicateHash);
	CRYPT_GetProvFuncOpt(CPDuplicateKey);
	CRYPT_GetProvFunc(CPEncrypt);
	CRYPT_GetProvFunc(CPExportKey);
	CRYPT_GetProvFunc(CPGenKey);
	CRYPT_GetProvFunc(CPGenRandom);
	CRYPT_GetProvFunc(CPGetHashParam);
	CRYPT_GetProvFunc(CPGetKeyParam);
	CRYPT_GetProvFunc(CPGetProvParam);
	CRYPT_GetProvFunc(CPGetUserKey);
	CRYPT_GetProvFunc(CPHashData);
	CRYPT_GetProvFunc(CPHashSessionKey);
	CRYPT_GetProvFunc(CPImportKey);
	CRYPT_GetProvFunc(CPReleaseContext);
	CRYPT_GetProvFunc(CPSetHashParam);
	CRYPT_GetProvFunc(CPSetKeyParam);
	CRYPT_GetProvFunc(CPSetProvParam);
	CRYPT_GetProvFunc(CPSignHash);
	CRYPT_GetProvFunc(CPVerifySignature);

	/* FIXME: Not sure what the pbContextInfo field is for.
	 *        Does it need memory allocation?
         */
	provider->pVTable->Version = 3;
	provider->pVTable->pFuncVerifyImage = (FARPROC)CRYPT_VerifyImage;
	provider->pVTable->pFuncReturnhWnd = (FARPROC)CRYPT_ReturnhWnd;
	provider->pVTable->dwProvType = 0;
	provider->pVTable->pbContextInfo = NULL;
	provider->pVTable->cbContextInfo = 0;
	provider->pVTable->pszProvName = NULL;
	return provider;

error:
	SetLastError(errorcode);
	if (provider)
	{
		provider->dwMagic = 0;
		if (provider->hModule)
			FreeLibrary(provider->hModule);
		CRYPT_Free(provider->pVTable);
		CRYPT_Free(provider->pFuncs);
		CRYPT_Free(provider);
	}
	return NULL;
}
#undef CRYPT_GetProvFunc
#undef CRYPT_GetProvFuncOpt


/******************************************************************************
 * CryptAcquireContextW (ADVAPI32.@)
 *
 * Acquire a crypto provider context handle.
 *
 * PARAMS
 *  phProv       [O] Pointer to HCRYPTPROV for the output.
 *  pszContainer [I] Key Container Name
 *  pszProvider  [I] Cryptographic Service Provider Name
 *  dwProvType   [I] Crypto provider type to get a handle.
 *  dwFlags      [I] flags for the operation
 *
 * RETURNS 
 *  TRUE on success, FALSE on failure.
 */
BOOL WINAPI CryptAcquireContextW (HCRYPTPROV *phProv, LPCWSTR pszContainer,
		LPCWSTR pszProvider, DWORD dwProvType, DWORD dwFlags)
{
	PCRYPTPROV pProv = NULL;
	HKEY key;
	PWSTR imagepath = NULL, keyname = NULL, provname = NULL, temp = NULL;
	PSTR provnameA = NULL, pszContainerA = NULL;
	DWORD keytype, type, len;
	ULONG r;
	static const WCHAR nameW[] = {'N','a','m','e',0};
	static const WCHAR typeW[] = {'T','y','p','e',0};
	static const WCHAR imagepathW[] = {'I','m','a','g','e',' ','P','a','t','h',0};

	TRACE("(%p, %s, %s, %ld, %08lx)\n", phProv, debugstr_w(pszContainer),
		debugstr_w(pszProvider), dwProvType, dwFlags);

	if (dwProvType < 1 || dwProvType > MAXPROVTYPES)
	{
		SetLastError(NTE_BAD_PROV_TYPE);
		return FALSE;
	}
	
	if (!phProv)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (!pszProvider || !*pszProvider)
	{
		/* No CSP name specified so try the user default CSP first
		 * then try the machine default CSP
		 */
		if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, TRUE)) ) {
			TRACE("No provider registered for crypto provider type %ld.\n", dwProvType);
			SetLastError(NTE_PROV_TYPE_NOT_DEF);
			return FALSE;
		}
		if (RegOpenKeyW(HKEY_CURRENT_USER, keyname, &key))
		{
			CRYPT_Free(keyname);
			if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, FALSE)) ) {
				TRACE("No type registered for crypto provider type %ld.\n", dwProvType);
				RegCloseKey(key);
				SetLastError(NTE_PROV_TYPE_NOT_DEF);
				goto error;
			}
			if (RegOpenKeyW(HKEY_LOCAL_MACHINE, keyname, &key)) {
				TRACE("Did not find registry entry of crypto provider for %s.\n", debugstr_w(keyname));
				CRYPT_Free(keyname);
				RegCloseKey(key);
				SetLastError(NTE_PROV_TYPE_NOT_DEF);
				goto error;
			}
		}
		CRYPT_Free(keyname);
		r = RegQueryValueExW(key, nameW, NULL, &keytype, NULL, &len);
		if( r != ERROR_SUCCESS || !len || keytype != REG_SZ)
		{
			TRACE("error %ld reading size of 'Name' from registry\n", r );
			RegCloseKey(key);
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
			goto error;
		}
		if(!(provname = CRYPT_Alloc(len)))
		{
			RegCloseKey(key);
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			goto error;
		}
		r = RegQueryValueExW(key, nameW, NULL, NULL, (LPBYTE)provname, &len);
		if( r != ERROR_SUCCESS )
		{
			TRACE("error %ld reading 'Name' from registry\n", r );
			RegCloseKey(key);
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
			goto error;
		}
		RegCloseKey(key);
	} else {
		if ( !(provname = CRYPT_Alloc((strlenW(pszProvider) +1)*sizeof(WCHAR))) )
		{
			SetLastError(ERROR_NOT_ENOUGH_MEMORY);
			goto error;
		}
		strcpyW(provname, pszProvider);
	}

	keyname = CRYPT_GetProvKeyName(provname);
	r = RegOpenKeyW(HKEY_LOCAL_MACHINE, keyname, &key);
	CRYPT_Free(keyname);
	if (r != ERROR_SUCCESS)
	{
		SetLastError(NTE_KEYSET_NOT_DEF);
		goto error;
	}
	len = sizeof(DWORD);
	r = RegQueryValueExW(key, typeW, NULL, NULL, (BYTE*)&type, &len);
	if (r != ERROR_SUCCESS)
	{
		SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		goto error;
	}
	if (type != dwProvType)
	{
		TRACE("Crypto provider has wrong type (%ld vs expected %ld).\n", type, dwProvType);
		SetLastError(NTE_PROV_TYPE_NO_MATCH);
		goto error;
	}

	r = RegQueryValueExW(key, imagepathW, NULL, &keytype, NULL, &len);
	if ( r != ERROR_SUCCESS || keytype != REG_SZ)
	{
		TRACE("error %ld reading size of 'Image Path' from registry\n", r );
		RegCloseKey(key);
		SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		goto error;
	}
	if (!(temp = CRYPT_Alloc(len)))
	{
		RegCloseKey(key);
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		goto error;
	}
	r = RegQueryValueExW(key, imagepathW, NULL, NULL, (LPBYTE)temp, &len);
	if( r != ERROR_SUCCESS )
	{
		TRACE("error %ld reading 'Image Path' from registry\n", r );
		RegCloseKey(key);
		SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		goto error;
	}
	RegCloseKey(key);
	len = ExpandEnvironmentStringsW(temp, NULL, 0);
	if ( !(imagepath = CRYPT_Alloc(len*sizeof(WCHAR))) )
	{
		SetLastError(ERROR_NOT_ENOUGH_MEMORY);
		goto error;
	}
	if (!ExpandEnvironmentStringsW(temp, imagepath, len))
	{
		/* ExpandEnvironmentStrings will call SetLastError */
		goto error;
	}
	pProv = CRYPT_LoadProvider(imagepath);
	if (!pProv) {
		/* CRYPT_LoadProvider calls SetLastError */
		goto error;
	}
	pProv->pVTable->dwProvType = dwProvType;
	if(!CRYPT_UnicodeToANSI(provname, &provnameA, -1))
	{
		/* CRYPT_UnicodeToANSI calls SetLastError */
		goto error;
	}
	pProv->pVTable->pszProvName = provnameA;
	if(!CRYPT_UnicodeToANSI(pszContainer, &pszContainerA, -1))
	{
		/* CRYPT_UnicodeToANSI calls SetLastError */
		goto error;
	}
	if (pProv->pFuncs->pCPAcquireContext(&pProv->hPrivate, pszContainerA, dwFlags, pProv->pVTable))
	{
		/* MSDN: When this flag is set, the value returned in phProv is undefined,
		 *       and thus, the CryptReleaseContext function need not be called afterwards.
		 *       Therefore, we must clean up everything now.
		 */
		if (dwFlags & CRYPT_DELETEKEYSET)
		{
			pProv->dwMagic = 0;
			FreeLibrary(pProv->hModule);
			CRYPT_Free(provnameA);
			CRYPT_Free(pProv->pVTable);
			CRYPT_Free(pProv->pFuncs);
			CRYPT_Free(pProv);
		} else {
			*phProv = (HCRYPTPROV)pProv;
		}
		CRYPT_Free(pszContainerA);
		CRYPT_Free(provname);
		CRYPT_Free(temp);
		CRYPT_Free(imagepath);
		return TRUE;
	}
	/* FALLTHROUGH TO ERROR IF FALSE - CSP internal error! */
error:
	if (pProv)
	{
		pProv->dwMagic = 0;
		if (pProv->hModule)
			FreeLibrary(pProv->hModule);
		if (pProv->pVTable)
			CRYPT_Free(pProv->pVTable);
		if (pProv->pFuncs)
			CRYPT_Free(pProv->pFuncs);
		CRYPT_Free(pProv);
	}
	if (pszContainerA)
		CRYPT_Free(pszContainerA);
	if (provnameA)
		CRYPT_Free(provnameA);
	if (provname)
		CRYPT_Free(provname);
	if (temp)
		CRYPT_Free(temp);
	if (imagepath)
		CRYPT_Free(imagepath);
	return FALSE;
}

/******************************************************************************
 * CryptAcquireContextA (ADVAPI32.@)
 *
 * See CryptAcquireContextW.
 */
BOOL WINAPI CryptAcquireContextA (HCRYPTPROV *phProv, LPCSTR pszContainer,
		LPCSTR pszProvider, DWORD dwProvType, DWORD dwFlags)
{
	PWSTR pProvider = NULL, pContainer = NULL;
	BOOL ret = FALSE;

	TRACE("(%p, %s, %s, %ld, %08lx)\n", phProv, pszContainer,
		pszProvider, dwProvType, dwFlags);

	if ( !CRYPT_ANSIToUnicode(pszContainer, &pContainer, -1) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if ( !CRYPT_ANSIToUnicode(pszProvider, &pProvider, -1) )
	{
		CRYPT_Free(pContainer);
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	}

	ret = CryptAcquireContextW(phProv, pContainer, pProvider, dwProvType, dwFlags);

	if (pContainer)
		CRYPT_Free(pContainer);
	if (pProvider)
		CRYPT_Free(pProvider);

	return ret;
}

/******************************************************************************
 * CryptContextAddRef (ADVAPI32.@)
 *
 * Increases reference count of a cryptographic service provider handle
 * by one.
 *
 * PARAMS
 *  hProv       [I] Handle to the CSP whose reference is being incremented.
 *  pdwReserved [IN] Reserved for future use and must be NULL.
 *  dwFlags     [I] Reserved for future use and must be NULL.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptContextAddRef (HCRYPTPROV hProv, DWORD *pdwReserved, DWORD dwFlags)
{
	PCRYPTPROV pProv = (PCRYPTPROV)hProv;	

	TRACE("(0x%lx, %p, %08lx)\n", hProv, pdwReserved, dwFlags);

	if (!pProv)
	{
		SetLastError(NTE_BAD_UID);
		return FALSE;
	}

	if (pProv->dwMagic != MAGIC_CRYPTPROV)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	pProv->refcount++;
	return TRUE;
}

/******************************************************************************
 * CryptReleaseContext (ADVAPI32.@)
 *
 * Releases the handle of a CSP.  Reference count is decreased.
 *
 * PARAMS
 *  hProv   [I] Handle of a CSP.
 *  dwFlags [I] Reserved for future use and must be NULL.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptReleaseContext (HCRYPTPROV hProv, DWORD dwFlags)
{
	PCRYPTPROV pProv = (PCRYPTPROV)hProv;
	BOOL ret = TRUE;

	TRACE("(0x%lx, %08lx)\n", hProv, dwFlags);

	if (!pProv)
	{
		SetLastError(NTE_BAD_UID);
		return FALSE;
	}

	if (pProv->dwMagic != MAGIC_CRYPTPROV)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	pProv->refcount--;
	if (pProv->refcount <= 0) 
	{
		ret = pProv->pFuncs->pCPReleaseContext(pProv->hPrivate, dwFlags);
		pProv->dwMagic = 0;
		FreeLibrary(pProv->hModule);
#if 0
		CRYPT_Free(pProv->pVTable->pContextInfo);
#endif
		CRYPT_Free(pProv->pVTable->pszProvName);
		CRYPT_Free(pProv->pVTable);
		CRYPT_Free(pProv->pFuncs);
		CRYPT_Free(pProv);
	}
	return ret;
}

/******************************************************************************
 * CryptGenRandom (ADVAPI32.@)
 *
 * Fills a buffer with cryptographically random bytes.
 *
 * PARAMS
 *  hProv    [I] Handle of a CSP.
 *  dwLen    [I] Number of bytes to generate.
 *  pbBuffer [I/O] Buffer to contain random bytes.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  pdBuffer must be at least dwLen bytes long.
 */
BOOL WINAPI CryptGenRandom (HCRYPTPROV hProv, DWORD dwLen, BYTE *pbBuffer)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %ld, %p)\n", hProv, dwLen, pbBuffer);

	if (!hProv)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	if (prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGenRandom(prov->hPrivate, dwLen, pbBuffer);
}

/******************************************************************************
 * CryptCreateHash (ADVAPI32.@)
 *
 * Initiates the hashing of a stream of data.
 *
 * PARAMS
 *  hProv   [I] Handle of a CSP.
 *  Algid   [I] Identifies the hash algorithm to use.
 *  hKey    [I] Key for the hash (if required).
 *  dwFlags [I] Reserved for future use and must be NULL.
 *  phHash  [O] Address of the future handle to the new hash object.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  If the algorithm is a keyed hash, hKey is the key.
 */
BOOL WINAPI CryptCreateHash (HCRYPTPROV hProv, ALG_ID Algid, HCRYPTKEY hKey,
		DWORD dwFlags, HCRYPTHASH *phHash)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash;

	TRACE("(0x%lx, 0x%x, 0x%lx, %08lx, %p)\n", hProv, Algid, hKey, dwFlags, phHash);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!phHash || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags)
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);
	if ( !(hash = CRYPT_Alloc(sizeof(CRYPTHASH))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	hash->pProvider = prov;

	if (prov->pFuncs->pCPCreateHash(prov->hPrivate, Algid,
			key ? key->hPrivate : 0, 0, &hash->hPrivate))
        {
            *phHash = (HCRYPTHASH)hash;
            return TRUE;
        }

	/* CSP error! */
	CRYPT_Free(hash);
	*phHash = 0;
	return FALSE;
}

/******************************************************************************
 * CryptDecrypt (ADVAPI32.@)
 *
 * Decrypts data encrypted by CryptEncrypt.
 *
 * PARAMS
 *  hKey       [I] Handle to the decryption key.
 *  hHash      [I] Handle to a hash object.
 *  Final      [I] TRUE if this is the last section to be decrypted.
 *  dwFlags    [I] Reserved for future use. Can be CRYPT_OAEP.
 *  pbData     [I/O] Buffer that holds the encrypted data. Holds decrypted
 *                   data on return
 *  pdwDataLen [I/O] Length of pbData before and after the call.
 *
 *  RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 */
BOOL WINAPI CryptDecrypt (HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, 0x%lx, %d, %08lx, %p, %p)\n", hKey, hHash, Final, dwFlags, pbData, pdwDataLen);

	if (!key || !pbData || !pdwDataLen || !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	return prov->pFuncs->pCPDecrypt(prov->hPrivate, key->hPrivate, hash ? hash->hPrivate : 0,
			Final, dwFlags, pbData, pdwDataLen);
}

/******************************************************************************
 * CryptDeriveKey (ADVAPI32.@)
 *
 * Generates session keys derived from a base data value.
 *
 * PARAMS
 *  hProv     [I] Handle to a CSP.
 *  Algid     [I] Identifies the symmetric encryption algorithm to use.
 *  hBaseData [I] Handle to a hash object.
 *  dwFlags   [I] Type of key to generate.
 *  phKey     [I/O] Address of the newly generated key.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptDeriveKey (HCRYPTPROV hProv, ALG_ID Algid, HCRYPTHASH hBaseData,
		DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTHASH hash = (PCRYPTHASH)hBaseData;
	PCRYPTKEY key;

	TRACE("(0x%lx, 0x%08x, 0x%lx, 0x%08lx, %p)\n", hProv, Algid, hBaseData, dwFlags, phKey);

	if (!prov || !hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!phKey || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;
	if (prov->pFuncs->pCPDeriveKey(prov->hPrivate, Algid, hash->hPrivate, dwFlags, &key->hPrivate))
        {
            *phKey = (HCRYPTKEY)key;
            return TRUE;
        }

	/* CSP error! */
	CRYPT_Free(key);
	*phKey = 0;
	return FALSE;
}

/******************************************************************************
 * CryptDestroyHash (ADVAPI32.@)
 *
 * Destroys the hash object referenced by hHash.
 *
 * PARAMS
 *  hHash [I] Handle of the hash object to be destroyed.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptDestroyHash (HCRYPTHASH hHash)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;
	BOOL ret;

	TRACE("(0x%lx)\n", hHash);

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	if (!hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	
	prov = hash->pProvider;
	ret = prov->pFuncs->pCPDestroyHash(prov->hPrivate, hash->hPrivate);
	CRYPT_Free(hash);
	return ret;
}

/******************************************************************************
 * CryptDestroyKey (ADVAPI32.@)
 *
 * Releases the handle referenced by hKey.
 *
 * PARAMS
 *  hKey [I] Handle of the key to be destroyed.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptDestroyKey (HCRYPTKEY hKey)
{
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTPROV prov;
	BOOL ret;

	TRACE("(0x%lx)\n", hKey);

	if (!key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	if (!key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	ret = prov->pFuncs->pCPDestroyKey(prov->hPrivate, key->hPrivate);
	CRYPT_Free(key);
	return ret;
}

/******************************************************************************
 * CryptDuplicateHash (ADVAPI32.@)
 *
 * Duplicates a hash.
 *
 * PARAMS
 *  hHash       [I] Handle to the hash to be copied.
 *  pdwReserved [I] Reserved for future use and must be zero.
 *  dwFlags     [I] Reserved for future use and must be zero.
 *  phHash      [O] Address of the handle to receive the copy.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptDuplicateHash (HCRYPTHASH hHash, DWORD *pdwReserved,
		DWORD dwFlags, HCRYPTHASH *phHash)
{
	PCRYPTPROV prov;
	PCRYPTHASH orghash, newhash;

	TRACE("(0x%lx, %p, %08lx, %p)\n", hHash, pdwReserved, dwFlags, phHash);

	orghash = (PCRYPTHASH)hHash;
	if (!orghash || pdwReserved || !phHash || !orghash->pProvider || 
		orghash->pProvider->dwMagic != MAGIC_CRYPTPROV)
	{
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	}

	prov = orghash->pProvider;
	if (!prov->pFuncs->pCPDuplicateHash)
		CRYPT_ReturnLastError(ERROR_CALL_NOT_IMPLEMENTED);

	if ( !(newhash = CRYPT_Alloc(sizeof(CRYPTHASH))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	newhash->pProvider = prov;
	if (prov->pFuncs->pCPDuplicateHash(prov->hPrivate, orghash->hPrivate, pdwReserved, dwFlags, &newhash->hPrivate))
	{
		*phHash = (HCRYPTHASH)newhash;
		return TRUE;
	}
	CRYPT_Free(newhash);
	return FALSE;
}

/******************************************************************************
 * CryptDuplicateKey (ADVAPI32.@)
 *
 * Duplicate a key and the key's state.
 *
 * PARAMS
 *  hKey        [I] Handle of the key to copy.
 *  pdwReserved [I] Reserved for future use and must be NULL.
 *  dwFlags     [I] Reserved for future use and must be zero.
 *  phKey       [I] Address of the handle to the duplicated key.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptDuplicateKey (HCRYPTKEY hKey, DWORD *pdwReserved, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov;
	PCRYPTKEY orgkey, newkey;

	TRACE("(0x%lx, %p, %08lx, %p)\n", hKey, pdwReserved, dwFlags, phKey);

	orgkey = (PCRYPTKEY)hKey;
	if (!orgkey || pdwReserved || !phKey || !orgkey->pProvider || 
		orgkey->pProvider->dwMagic != MAGIC_CRYPTPROV)
	{
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	}

	prov = orgkey->pProvider;
	if (!prov->pFuncs->pCPDuplicateKey)
		CRYPT_ReturnLastError(ERROR_CALL_NOT_IMPLEMENTED);

	if ( !(newkey = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	newkey->pProvider = prov;
	if (prov->pFuncs->pCPDuplicateKey(prov->hPrivate, orgkey->hPrivate, pdwReserved, dwFlags, &newkey->hPrivate))
	{
		*phKey = (HCRYPTKEY)newkey;
		return TRUE;
	}
	CRYPT_Free(newkey);
	return FALSE;
}

/******************************************************************************
 * CryptEncrypt (ADVAPI32.@)
 *
 * Encrypts data.
 *
 * PARAMS
 *  hKey       [I] Handle to the enryption key.
 *  hHash      [I] Handle to a hash object.
 *  Final      [I] TRUE if this is the last section to encrypt.
 *  dwFlags    [I] Can be CRYPT_OAEP.
 *  pbData     [I/O] Data to be encrypted. Contains encrypted data after call.
 *  pdwDataLen [I/O] Length of the data to encrypt. Contains the length of the
 *                   encrypted data after call.
 *  dwBufLen   [I] Length of the input pbData buffer.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 *  NOTES
 *   If pbData is NULL, CryptEncrypt determines stores the number of bytes
 *   required for the returned data in pdwDataLen.
 */
BOOL WINAPI CryptEncrypt (HCRYPTKEY hKey, HCRYPTHASH hHash, BOOL Final,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen, DWORD dwBufLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, 0x%lx, %d, %08lx, %p, %p, %ld)\n", hKey, hHash, Final, dwFlags, pbData, pdwDataLen, dwBufLen);

	if (!key || !pdwDataLen || !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	return prov->pFuncs->pCPEncrypt(prov->hPrivate, key->hPrivate, hash ? hash->hPrivate : 0,
			Final, dwFlags, pbData, pdwDataLen, dwBufLen);
}

/******************************************************************************
 * CryptEnumProvidersW (ADVAPI32.@)
 *
 * Returns the next availabe CSP.
 *
 * PARAMS
 *  dwIndex     [I] Index of the next provider to be enumerated.
 *  pdwReserved [I] Reserved for future use and must be NULL.
 *  dwFlags     [I] Reserved for future use and must be zero.
 *  pdwProvType [O] DWORD designating the type of the provider.
 *  pszProvName [O] Buffer that receives data from the provider.
 *  pcbProvName [I/O] Specifies the size of pszProvName. Contains the number
 *                    of bytes stored in the buffer on return.
 *
 *  RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 *
 *  NOTES
 *   If pszProvName is NULL, CryptEnumProvidersW sets the size of the name
 *   for memory allocation purposes.
 */
BOOL WINAPI CryptEnumProvidersW (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPWSTR pszProvName, DWORD *pcbProvName)
{
	HKEY hKey;
	static const WCHAR providerW[] = {
                'S','o','f','t','w','a','r','e','\\',
                'M','i','c','r','o','s','o','f','t','\\',
                'C','r','y','p','t','o','g','r','a','p','h','y','\\',
                'D','e','f','a','u','l','t','s','\\',
                'P','r','o','v','i','d','e','r',0
        };
	static const WCHAR typeW[] = {'T','y','p','e',0};

	TRACE("(%ld, %p, %ld, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszProvName, pcbProvName);

	if (pdwReserved || !pcbProvName) CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags) CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	if (RegOpenKeyW(HKEY_LOCAL_MACHINE, providerW, &hKey))
		CRYPT_ReturnLastError(NTE_FAIL);

	if (!pszProvName)
	{
		DWORD numkeys;
		WCHAR *provNameW;
		
		RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &numkeys, pcbProvName,
				 NULL, NULL, NULL, NULL, NULL, NULL);
		
		if (!(provNameW = CRYPT_Alloc(*pcbProvName * sizeof(WCHAR))))
			CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
		
		RegEnumKeyExW(hKey, dwIndex, provNameW, pcbProvName, NULL, NULL, NULL, NULL);
		CRYPT_Free(provNameW);
		(*pcbProvName)++;
		*pcbProvName *= sizeof(WCHAR);

		if (dwIndex >= numkeys)
			CRYPT_ReturnLastError(ERROR_NO_MORE_ITEMS);
	} else {
		DWORD size = sizeof(DWORD);
		DWORD result;
		HKEY subkey;
		
		result = RegEnumKeyW(hKey, dwIndex, pszProvName, *pcbProvName / sizeof(WCHAR));
		if (result)
			CRYPT_ReturnLastError(result);
		if (RegOpenKeyW(hKey, pszProvName, &subkey))
			return FALSE;
		if (RegQueryValueExW(subkey, typeW, NULL, NULL, (BYTE*)pdwProvType, &size))
			return FALSE;
		RegCloseKey(subkey);
	}
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptEnumProvidersA (ADVAPI32.@)
 *
 * See CryptEnumProvidersW.
 */
BOOL WINAPI CryptEnumProvidersA (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPSTR pszProvName, DWORD *pcbProvName)
{
	PWSTR str = NULL;
	DWORD strlen;
	BOOL ret; /* = FALSE; */

	TRACE("(%ld, %p, %08lx, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszProvName, pcbProvName);

	CryptEnumProvidersW(dwIndex, pdwReserved, dwFlags, pdwProvType, NULL, &strlen);
	if ( pszProvName && !(str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptEnumProvidersW(dwIndex, pdwReserved, dwFlags, pdwProvType, str, &strlen);
	if (str)
		CRYPT_UnicodeToANSI(str, &pszProvName, *pcbProvName);
	*pcbProvName = strlen / sizeof(WCHAR);  /* FIXME: not correct */
	if (str)
	{
		CRYPT_Free(str);
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			CRYPT_ReturnLastError(ERROR_MORE_DATA);
	}
	return ret;
}

/******************************************************************************
 * CryptEnumProviderTypesW (ADVAPI32.@)
 *
 * Retrieves the next type of CSP supported.
 *
 * PARAMS
 *  dwIndex     [I] Index of the next provider to be enumerated.
 *  pdwReserved [I] Reserved for future use and must be NULL.
 *  dwFlags     [I] Reserved for future use and must be zero.
 *  pdwProvType [O] DWORD designating the type of the provider.
 *  pszTypeName [O] Buffer that receives data from the provider type.
 *  pcbTypeName [I/O] Specifies the size of pszTypeName. Contains the number
 *                    of bytes stored in the buffer on return.
 *
 *  RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 *
 *  NOTES
 *   If pszTypeName is NULL, CryptEnumProviderTypesW sets the size of the name
 *   for memory allocation purposes.
 */
BOOL WINAPI CryptEnumProviderTypesW (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPWSTR pszTypeName, DWORD *pcbTypeName)
{
	HKEY hKey, hSubkey;
	DWORD keylen, numkeys, dwType;
	PWSTR keyname, ch;
	DWORD result;
	static const WCHAR KEYSTR[] = {
                'S','o','f','t','w','a','r','e','\\',
                'M','i','c','r','o','s','o','f','t','\\',
                'C','r','y','p','t','o','g','r','a','p','h','y','\\',
                'D','e','f','a','u','l','t','s','\\',
                'P','r','o','v','i','d','e','r',' ','T','y','p','e','s',0
	};
	static const WCHAR typenameW[] = {'T','y','p','e','N','a','m','e',0};

	TRACE("(%ld, %p, %08lx, %p, %p, %p)\n", dwIndex, pdwReserved,
		dwFlags, pdwProvType, pszTypeName, pcbTypeName);

	if (pdwReserved || !pdwProvType || !pcbTypeName)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags) CRYPT_ReturnLastError(NTE_BAD_FLAGS);

	if (RegOpenKeyW(HKEY_LOCAL_MACHINE, KEYSTR, &hKey))
		return FALSE;

	RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &numkeys, &keylen, NULL, NULL, NULL, NULL, NULL, NULL);
	if (dwIndex >= numkeys)
		CRYPT_ReturnLastError(ERROR_NO_MORE_ITEMS);
	keylen++;
	if ( !(keyname = CRYPT_Alloc(keylen*sizeof(WCHAR))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if ( RegEnumKeyW(hKey, dwIndex, keyname, keylen) ) {
                CRYPT_Free(keyname);
		return FALSE;
        }
	RegOpenKeyW(hKey, keyname, &hSubkey);
	ch = keyname + strlenW(keyname);
	/* Convert "Type 000" to 0, etc/ */
	*pdwProvType = *(--ch) - '0';
	*pdwProvType += (*(--ch) - '0') * 10;
	*pdwProvType += (*(--ch) - '0') * 100;
	CRYPT_Free(keyname);
	
	result = RegQueryValueExW(hSubkey, typenameW, NULL, &dwType, (LPBYTE)pszTypeName, pcbTypeName);
	if (result)
		CRYPT_ReturnLastError(result);

	RegCloseKey(hSubkey);
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptEnumProviderTypesA (ADVAPI32.@)
 *
 * See CryptEnumProviderTypesW.
 */
BOOL WINAPI CryptEnumProviderTypesA (DWORD dwIndex, DWORD *pdwReserved,
		DWORD dwFlags, DWORD *pdwProvType, LPSTR pszTypeName, DWORD *pcbTypeName)
{
	PWSTR str = NULL;
	DWORD strlen;
	BOOL ret;

	TRACE("(%ld, %p, %08lx, %p, %p, %p)\n", dwIndex, pdwReserved, dwFlags,
			pdwProvType, pszTypeName, pcbTypeName);

	CryptEnumProviderTypesW(dwIndex, pdwReserved, dwFlags, pdwProvType, NULL, &strlen);
	if ( pszTypeName && !(str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptEnumProviderTypesW(dwIndex, pdwReserved, dwFlags, pdwProvType, str, &strlen);
	if (str)
		CRYPT_UnicodeToANSI(str, &pszTypeName, *pcbTypeName);
	*pcbTypeName = strlen / sizeof(WCHAR);
	if (str)
	{
		CRYPT_Free(str);
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			CRYPT_ReturnLastError(ERROR_MORE_DATA);
	}
	return ret;
}

/******************************************************************************
 * CryptExportKey (ADVAPI32.@)
 * 
 * Exports a cryptographic key from a CSP.
 *
 * PARAMS
 *  hKey       [I] Handle to the key to export.
 *  hExpKey    [I] Handle to a cryptographic key of the end user.
 *  dwBlobType [I] Type of BLOB to be exported.
 *  dwFlags    [I] CRYPT_DESTROYKEY/SSL2_FALLBACK/OAEP.
 *  pbData     [O] Buffer to receive BLOB data.
 *  pdwDataLen [I/O] Specifies the size of pbData.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  if pbData is NULL, CryptExportKey sets pdwDataLen as the size of the
 *  buffer needed to hold the BLOB.
 */
BOOL WINAPI CryptExportKey (HCRYPTKEY hKey, HCRYPTKEY hExpKey, DWORD dwBlobType,
		DWORD dwFlags, BYTE *pbData, DWORD *pdwDataLen)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey, expkey = (PCRYPTKEY)hExpKey;

	TRACE("(0x%lx, 0x%lx, %ld, %08lx, %p, %p)\n", hKey, hExpKey, dwBlobType, dwFlags, pbData, pdwDataLen);

	if (!key || !pdwDataLen || !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	return prov->pFuncs->pCPExportKey(prov->hPrivate, key->hPrivate, expkey ? expkey->hPrivate : 0,
			dwBlobType, dwFlags, pbData, pdwDataLen);
}

/******************************************************************************
 * CryptGenKey (ADVAPI32.@)
 *
 * Generates a random cryptographic session key or a pub/priv key pair.
 *
 * PARAMS
 *  hProv   [I] Handle to a CSP.
 *  Algid   [I] Algorithm to use to make key.
 *  dwFlags [I] Specifies type of key to make.
 *  phKey   [I] Address of the handle to which the new key is copied.
 *
 *  RETURNS
 *   Success: TRUE
 *   Failure: FALSE
 */
BOOL WINAPI CryptGenKey (HCRYPTPROV hProv, ALG_ID Algid, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key;

	TRACE("(0x%lx, %d, %08lx, %p)\n", hProv, Algid, dwFlags, phKey);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!phKey || !prov || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;

	if (prov->pFuncs->pCPGenKey(prov->hPrivate, Algid, dwFlags, &key->hPrivate))
        {
            *phKey = (HCRYPTKEY)key;
            return TRUE;
        }

	/* CSP error! */
	CRYPT_Free(key);
	return FALSE;
}

/******************************************************************************
 * CryptGetDefaultProviderW (ADVAPI32.@)
 *
 * Finds the default CSP of a certain provider type.
 *
 * PARAMS
 *  dwProvType  [I] Provider type to look for.
 *  pdwReserved [I] Reserved for future use and must be NULL.
 *  dwFlags     [I] CRYPT_MACHINE_DEFAULT/USER_DEFAULT
 *  pszProvName [O] Name of the default CSP.
 *  pcbProvName [I/O] Size of pszProvName
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  If pszProvName is NULL, pcbProvName will hold the size of the buffer for
 *  memory allocation purposes on return.
 */
BOOL WINAPI CryptGetDefaultProviderW (DWORD dwProvType, DWORD *pdwReserved,
		DWORD dwFlags, LPWSTR pszProvName, DWORD *pcbProvName)
{
	HKEY hKey;
	PWSTR keyname;
	DWORD result;
	static const WCHAR nameW[] = {'N','a','m','e',0};

	if (pdwReserved || !pcbProvName)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags & ~(CRYPT_USER_DEFAULT | CRYPT_MACHINE_DEFAULT))
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);
	if (dwProvType > 999)
		CRYPT_ReturnLastError(NTE_BAD_PROV_TYPE);
	if ( !(keyname = CRYPT_GetTypeKeyName(dwProvType, dwFlags & CRYPT_USER_DEFAULT)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if (RegOpenKeyW((dwFlags & CRYPT_USER_DEFAULT) ?  HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE ,keyname, &hKey))
	{
		CRYPT_Free(keyname);
		CRYPT_ReturnLastError(NTE_PROV_TYPE_NOT_DEF);
	}
	CRYPT_Free(keyname);
	
	result = RegQueryValueExW(hKey, nameW, NULL, NULL, (LPBYTE)pszProvName, pcbProvName); 
	if (result)
	{
		if (result != ERROR_MORE_DATA)
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		else
			SetLastError(result);
		
		return FALSE;
	}
	
	RegCloseKey(hKey);
	return TRUE;
}

/******************************************************************************
 * CryptGetDefaultProviderA (ADVAPI32.@)
 *
 * See CryptGetDefaultProviderW.
 */
BOOL WINAPI CryptGetDefaultProviderA (DWORD dwProvType, DWORD *pdwReserved,
		DWORD dwFlags, LPSTR pszProvName, DWORD *pcbProvName)
{
	PWSTR str = NULL;
	DWORD strlen;
	BOOL ret = FALSE;

	TRACE("(%ld, %p, %08lx, %p, %p)\n", dwProvType, pdwReserved, dwFlags, pszProvName, pcbProvName);

	CryptGetDefaultProviderW(dwProvType, pdwReserved, dwFlags, NULL, &strlen);
	if ( pszProvName && !(str = CRYPT_Alloc(strlen)) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	ret = CryptGetDefaultProviderW(dwProvType, pdwReserved, dwFlags, str, &strlen);
	if (str)
		CRYPT_UnicodeToANSI(str, &pszProvName, *pcbProvName);
	*pcbProvName = strlen / sizeof(WCHAR);
	if (str)
	{
		CRYPT_Free(str);
		if (GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			CRYPT_ReturnLastError(ERROR_MORE_DATA);
	}
	return ret;
}

/******************************************************************************
 * CryptGetHashParam (ADVAPI32.@)
 *
 * Retrieves data that controls the operations of a hash object.
 *
 * PARAMS
 *  hHash      [I] Handle of the hash object to question.
 *  dwParam    [I] Query type.
 *  pbData     [O] Buffer that receives the value data.
 *  pdwDataLen [I/O] Size of the pbData buffer.
 *  dwFlags    [I] Reserved for future use and must be zero.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  If pbData is NULL, pdwDataLen will contain the length required.
 */
BOOL WINAPI CryptGetHashParam (HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, %ld, %p, %p, %08lx)\n", hHash, dwParam, pbData, pdwDataLen, dwFlags);

	if (!hash || !pdwDataLen || !hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	return prov->pFuncs->pCPGetHashParam(prov->hPrivate, hash->hPrivate, dwParam,
			pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetKeyParam (ADVAPI32.@)
 *
 * Retrieves data that controls the operations of a key.
 *
 * PARAMS
 *  hKey       [I] Handle to they key in question.
 *  dwParam    [I] Specifies query type.
 *  pbData     [O] Sequence of bytes to receive data.
 *  pdwDataLen [I/O] Size of pbData.
 *  dwFlags    [I] Reserved for future use and must be zero.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  If pbData is NULL, pdwDataLen is set to the needed length of the buffer.
 */
BOOL WINAPI CryptGetKeyParam (HCRYPTKEY hKey, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;

	TRACE("(0x%lx, %ld, %p, %p, %08lx)\n", hKey, dwParam, pbData, pdwDataLen, dwFlags);

	if (!key || !pdwDataLen || !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	return prov->pFuncs->pCPGetKeyParam(prov->hPrivate, key->hPrivate, dwParam,
			pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetProvParam (ADVAPI32.@)
 *
 * Retrieves parameters that control the operations of a CSP.
 *
 * PARAMS
 *  hProv      [I] Handle of the CSP in question.
 *  dwParam    [I] Specifies query type.
 *  pbData     [O] Buffer to receive the data.
 *  pdwDataLen [I/O] Size of pbData.
 *  dwFlags    [I] see MSDN Docs.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  If pbData is NULL, pdwDataLen is set to the needed buffer length.
 */
BOOL WINAPI CryptGetProvParam (HCRYPTPROV hProv, DWORD dwParam, BYTE *pbData,
		DWORD *pdwDataLen, DWORD dwFlags)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %ld, %p, %p, %08lx)\n", hProv, dwParam, pbData, pdwDataLen, dwFlags);

	if (!prov || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	return prov->pFuncs->pCPGetProvParam(prov->hPrivate, dwParam, pbData, pdwDataLen, dwFlags);
}

/******************************************************************************
 * CryptGetUserKey (ADVAPI32.@)
 *
 * Gets a handle of one of a user's two public/private key pairs.
 *
 * PARAMS
 *  hProv     [I] Handle of a CSP.
 *  dwKeySpec [I] Private key to use.
 *  phUserKey [O] Pointer to the handle of the retrieved keys.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptGetUserKey (HCRYPTPROV hProv, DWORD dwKeySpec, HCRYPTKEY *phUserKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY key;

	TRACE("(0x%lx, %ld, %p)\n", hProv, dwKeySpec, phUserKey);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!phUserKey || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if ( !(key = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	key->pProvider = prov;

	if (prov->pFuncs->pCPGetUserKey(prov->hPrivate, dwKeySpec, &key->hPrivate))
        {
            *phUserKey = (HCRYPTKEY)key;
            return TRUE;
        }

	/* CSP Error */
	CRYPT_Free(key);
	*phUserKey = 0;
	return FALSE;
}

/******************************************************************************
 * CryptHashData (ADVAPI32.@)
 *
 * Adds data to a hash object.
 *
 * PARAMS
 *  hHash     [I] Handle of the hash object.
 *  pbData    [I] Buffer of data to be hashed.
 *  dwDataLen [I] Number of bytes to add.
 *  dwFlags   [I] Can be CRYPT_USERDATA
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptHashData (HCRYPTHASH hHash, const BYTE *pbData, DWORD dwDataLen, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %p, %ld, %08lx)\n", hHash, pbData, dwDataLen, dwFlags);

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	return prov->pFuncs->pCPHashData(prov->hPrivate, hash->hPrivate, pbData, dwDataLen, dwFlags);
}

/******************************************************************************
 * CryptHashSessionKey (ADVAPI32.@)
 *
 * PARAMS 
 *  hHash   [I] Handle to the hash object.
 *  hKey    [I] Handle to the key to be hashed.
 *  dwFlags [I] Can be CRYPT_LITTLE_ENDIAN.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptHashSessionKey (HCRYPTHASH hHash, HCRYPTKEY hKey, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTKEY key = (PCRYPTKEY)hKey;
	PCRYPTPROV prov;

	TRACE("(0x%lx, 0x%lx, %08lx)\n", hHash, hKey, dwFlags);

	if (!hash || !key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);

	if (!hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	return prov->pFuncs->pCPHashSessionKey(prov->hPrivate, hash->hPrivate, key->hPrivate, dwFlags);
}

/******************************************************************************
 * CryptImportKey (ADVAPI32.@)
 *
 * PARAMS
 *  hProv     [I] Handle of a CSP.
 *  pbData    [I] Contains the key to be imported.
 *  dwDataLen [I] Length of the key.
 *  hPubKey   [I] Cryptographic key that decrypts pdData
 *  dwFlags   [I] Used only with a public/private key pair.
 *  phKey     [O] Imported key.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptImportKey (HCRYPTPROV hProv, BYTE *pbData, DWORD dwDataLen,
		HCRYPTKEY hPubKey, DWORD dwFlags, HCRYPTKEY *phKey)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;
	PCRYPTKEY pubkey = (PCRYPTKEY)hPubKey, importkey;

	TRACE("(0x%lx, %p, %ld, 0x%lx, %08lx, %p)\n", hProv, pbData, dwDataLen, hPubKey, dwFlags, phKey);

	if (!prov || !pbData || !dwDataLen || !phKey || prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	if ( !(importkey = CRYPT_Alloc(sizeof(CRYPTKEY))) )
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);

	importkey->pProvider = prov;
	if (prov->pFuncs->pCPImportKey(prov->hPrivate, pbData, dwDataLen,
			pubkey ? pubkey->hPrivate : 0, dwFlags, &importkey->hPrivate))
	{
		*phKey = (HCRYPTKEY)importkey;
		return TRUE;
	}

	CRYPT_Free(importkey);
	return FALSE;
}

/******************************************************************************
 * CryptSignHashW (ADVAPI32.@)
 *
 * Signs data.
 *
 * PARAMS
 *  hHash        [I] Handle of the hash object to be signed.
 *  dwKeySpec    [I] Private key to use.
 *  sDescription [I] Should be NULL.
 *  dwFlags      [I] CRYPT_NOHASHOID/X931_FORMAT.
 *  pbSignature  [O] Buffer of the signature data.
 *  pdwSigLen    [I/O] Size of the pbSignature buffer.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 *
 * NOTES
 *  Because of security flaws sDescription should not be used and should thus be
 *  NULL. It is supported only for compatibility with Microsoft's Cryptographic
 *  Providers.
 */
BOOL WINAPI CryptSignHashW (HCRYPTHASH hHash, DWORD dwKeySpec, LPCWSTR sDescription,
		DWORD dwFlags, BYTE *pbSignature, DWORD *pdwSigLen)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %ld, %s, %08lx, %p, %p)\n", 
		hHash, dwKeySpec, debugstr_w(sDescription), dwFlags, pbSignature, pdwSigLen);

	if (!hash)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!pdwSigLen || !hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	return prov->pFuncs->pCPSignHash(prov->hPrivate, hash->hPrivate, dwKeySpec, sDescription,
		dwFlags, pbSignature, pdwSigLen);
}

/******************************************************************************
 * CryptSignHashA (ADVAPI32.@)
 *
 * See CryptSignHashW.
 */
BOOL WINAPI CryptSignHashA (HCRYPTHASH hHash, DWORD dwKeySpec, LPCSTR sDescription,
		DWORD dwFlags, BYTE *pbSignature, DWORD *pdwSigLen)
{
	LPWSTR wsDescription;
	BOOL result;

	TRACE("(0x%lx, %ld, %s, %08lx, %p, %p)\n", 
		hHash, dwKeySpec, debugstr_a(sDescription), dwFlags, pbSignature, pdwSigLen);

	CRYPT_ANSIToUnicode(sDescription, &wsDescription, -1);
	result = CryptSignHashW(hHash, dwKeySpec, wsDescription, dwFlags, pbSignature, pdwSigLen);
	if (wsDescription) CRYPT_Free(wsDescription);

	return result;
}

/******************************************************************************
 * CryptSetHashParam (ADVAPI32.@)
 *
 * Customizes the operations of a hash object.
 *
 * PARAMS
 *  hHash   [I] Handle of the hash object to set parameters.
 *  dwParam [I] HP_HMAC_INFO/HASHVAL.
 *  pbData  [I] Value data buffer.
 *  dwFlags [I] Reserved for future use and must be zero.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptSetHashParam (HCRYPTHASH hHash, DWORD dwParam, BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTHASH hash = (PCRYPTHASH)hHash;

	TRACE("(0x%lx, %ld, %p, %08lx)\n", hHash, dwParam, pbData, dwFlags);

	if (!hash || !pbData || !hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = hash->pProvider;
	return prov->pFuncs->pCPSetHashParam(prov->hPrivate, hash->hPrivate,
			dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptSetKeyParam (ADVAPI32.@)
 *
 * Customizes a session key's operations.
 *
 * PARAMS
 *  hKey    [I] Handle to the key to set values.
 *  dwParam [I] See MSDN Doc.
 *  pbData  [I] Buffer of values to set.
 *  dwFlags [I] Only used when dwParam == KP_ALGID.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptSetKeyParam (HCRYPTKEY hKey, DWORD dwParam, BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov;
	PCRYPTKEY key = (PCRYPTKEY)hKey;

	TRACE("(0x%lx, %ld, %p, %08lx)\n", hKey, dwParam, pbData, dwFlags);

	if (!key || !pbData || !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);

	prov = key->pProvider;
	return prov->pFuncs->pCPSetKeyParam(prov->hPrivate, key->hPrivate,
			dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptSetProviderA (ADVAPI32.@)
 *
 * Specifies the current user's default CSP.
 *
 * PARAMS
 *  pszProvName [I] Name of the new default CSP.
 *  dwProvType  [I] Provider type of the CSP.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptSetProviderA (LPCSTR pszProvName, DWORD dwProvType)
{
	TRACE("(%s, %ld)\n", pszProvName, dwProvType);
	return CryptSetProviderExA(pszProvName, dwProvType, NULL, CRYPT_USER_DEFAULT);
}

/******************************************************************************
 * CryptSetProviderW (ADVAPI32.@)
 *
 * See CryptSetProviderA.
 */
BOOL WINAPI CryptSetProviderW (LPCWSTR pszProvName, DWORD dwProvType)
{
	TRACE("(%s, %ld)\n", debugstr_w(pszProvName), dwProvType);
	return CryptSetProviderExW(pszProvName, dwProvType, NULL, CRYPT_USER_DEFAULT);
}

/******************************************************************************
 * CryptSetProviderExW (ADVAPI32.@)
 *
 * Specifies the default CSP.
 *
 * PARAMS
 *  pszProvName [I] Name of the new default CSP.
 *  dwProvType  [I] Provider type of the CSP.
 *  pdwReserved [I] Reserved for future use and must be NULL.
 *  dwFlags     [I] See MSDN Doc.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptSetProviderExW (LPCWSTR pszProvName, DWORD dwProvType, DWORD *pdwReserved, DWORD dwFlags)
{
	HKEY hProvKey, hTypeKey;
	PWSTR keyname;
	static const WCHAR nameW[] = {'N','a','m','e',0};

	TRACE("(%s, %ld, %p, %08lx)\n", debugstr_w(pszProvName), dwProvType, pdwReserved, dwFlags);

	if (!pszProvName || pdwReserved)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwProvType > MAXPROVTYPES)
		CRYPT_ReturnLastError(NTE_BAD_PROV_TYPE);
	if (dwFlags & ~(CRYPT_MACHINE_DEFAULT | CRYPT_USER_DEFAULT | CRYPT_DELETE_DEFAULT)
			|| dwFlags == CRYPT_DELETE_DEFAULT)
		CRYPT_ReturnLastError(NTE_BAD_FLAGS);
	
	if (!(keyname = CRYPT_GetTypeKeyName(dwProvType, dwFlags & CRYPT_USER_DEFAULT)))
		CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
	if (RegOpenKeyW((dwFlags & CRYPT_USER_DEFAULT) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
		keyname, &hTypeKey))
	{
		CRYPT_Free(keyname);
		CRYPT_ReturnLastError(NTE_BAD_PROVIDER);
	}
	CRYPT_Free(keyname);
	
	if (dwFlags & CRYPT_DELETE_DEFAULT)
	{
		RegDeleteValueW(hTypeKey, nameW);
	}
	else
	{
		if (!(keyname = CRYPT_GetProvKeyName(pszProvName)))
		{
			RegCloseKey(hTypeKey);
			CRYPT_ReturnLastError(ERROR_NOT_ENOUGH_MEMORY);
		}
		if (RegOpenKeyW((dwFlags & CRYPT_USER_DEFAULT) ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
			keyname, &hProvKey))
		{
			CRYPT_Free(keyname);
			RegCloseKey(hTypeKey);
			CRYPT_ReturnLastError(NTE_BAD_PROVIDER);
		}
		CRYPT_Free(keyname);
		
		if (RegSetValueExW(hTypeKey, nameW, 0, REG_SZ, (LPBYTE)pszProvName, (strlenW(pszProvName) + 1)*sizeof(WCHAR)))
		{
			RegCloseKey(hTypeKey);
			RegCloseKey(hProvKey);
			return FALSE;
		}
		
		RegCloseKey(hProvKey);
	}
	RegCloseKey(hTypeKey);

	return TRUE;
}

/******************************************************************************
 * CryptSetProviderExA (ADVAPI32.@)
 *
 * See CryptSetProviderExW.
 */
BOOL WINAPI CryptSetProviderExA (LPCSTR pszProvName, DWORD dwProvType, DWORD *pdwReserved, DWORD dwFlags)
{
	BOOL ret = FALSE;
	PWSTR str = NULL;

	TRACE("(%s, %ld, %p, %08lx)\n", pszProvName, dwProvType, pdwReserved, dwFlags);

	if (CRYPT_ANSIToUnicode(pszProvName, &str, -1))
	{
		ret = CryptSetProviderExW(str, dwProvType, pdwReserved, dwFlags);
		CRYPT_Free(str);
	}
	return ret;
}

/******************************************************************************
 * CryptSetProvParam (ADVAPI32.@)
 *
 * Customizes the operations of a CSP.
 *
 * PARAMS
 *  hProv   [I] Handle of a CSP.
 *  dwParam [I] See MSDN Doc.
 *  pbData  [I] Buffer that contains a value to set as a parameter.
 *  dwFlags [I] if dwParam is PP_USE_HARDWARE_RNG, dwFlags must be zero.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */
BOOL WINAPI CryptSetProvParam (HCRYPTPROV hProv, DWORD dwParam, BYTE *pbData, DWORD dwFlags)
{
	PCRYPTPROV prov = (PCRYPTPROV)hProv;

	TRACE("(0x%lx, %ld, %p, %08lx)\n", hProv, dwParam, pbData, dwFlags);

	if (!prov)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (prov->dwMagic != MAGIC_CRYPTPROV)
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	if (dwFlags & PP_USE_HARDWARE_RNG)
	{
		FIXME("PP_USE_HARDWARE_RNG: What do I do with this?\n");
		FIXME("\tLetting the CSP decide.\n");
	}
	if (dwFlags & PP_CLIENT_HWND)
	{
		/* FIXME: Should verify the parameter */
		if (pbData /* && IsWindow((HWND)pbData) */)
		{
			crypt_hWindow = (HWND)(pbData);
			return TRUE;
		} else {
			SetLastError(ERROR_INVALID_PARAMETER);
			return FALSE;
		}
	}
	/* All other flags go to the CSP */
	return prov->pFuncs->pCPSetProvParam(prov->hPrivate, dwParam, pbData, dwFlags);
}

/******************************************************************************
 * CryptVerifySignatureW (ADVAPI32.@)
 *
 * Verifies the signature of a hash object.
 *
 * PARAMS
 *  hHash        [I] Handle of the hash object to verify.
 *  pbSignature  [I] Signature data to verify.
 *  dwSigLen     [I] Size of pbSignature.
 *  hPubKey      [I] Handle to the public key to authenticate signature.
 *  sDescription [I] Should be NULL.
 *  dwFlags      [I] See MSDN doc.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 * 
 * NOTES
 *  Because of security flaws sDescription should not be used and should thus be
 *  NULL. It is supported only for compatibility with Microsoft's Cryptographic
 *  Providers.
 */
BOOL WINAPI CryptVerifySignatureW (HCRYPTHASH hHash, BYTE *pbSignature, DWORD dwSigLen,
		HCRYPTKEY hPubKey, LPCWSTR sDescription, DWORD dwFlags)
{
	PCRYPTHASH hash = (PCRYPTHASH)hHash;
	PCRYPTKEY key = (PCRYPTKEY)hPubKey;
	PCRYPTPROV prov;

	TRACE("(0x%lx, %p, %ld, 0x%lx, %s, %08lx)\n", hHash, pbSignature,
			dwSigLen, hPubKey, debugstr_w(sDescription), dwFlags);

	if (!hash || !key)
		CRYPT_ReturnLastError(ERROR_INVALID_HANDLE);
	if (!pbSignature || !dwSigLen || 
	    !hash->pProvider || hash->pProvider->dwMagic != MAGIC_CRYPTPROV ||
	    !key->pProvider || key->pProvider->dwMagic != MAGIC_CRYPTPROV)
	{
		CRYPT_ReturnLastError(ERROR_INVALID_PARAMETER);
	}
		
	prov = hash->pProvider;
	return prov->pFuncs->pCPVerifySignature(prov->hPrivate, hash->hPrivate, pbSignature, dwSigLen,
		key->hPrivate, sDescription, dwFlags);
}

/******************************************************************************
 * CryptVerifySignatureA (ADVAPI32.@)
 *
 * See CryptVerifySignatureW.
 */
BOOL WINAPI CryptVerifySignatureA (HCRYPTHASH hHash, BYTE *pbSignature, DWORD dwSigLen,
		HCRYPTKEY hPubKey, LPCSTR sDescription, DWORD dwFlags)
{
	LPWSTR wsDescription;
	BOOL result;

	TRACE("(0x%lx, %p, %ld, 0x%lx, %s, %08lx)\n", hHash, pbSignature,
			dwSigLen, hPubKey, debugstr_a(sDescription), dwFlags);

	CRYPT_ANSIToUnicode(sDescription, &wsDescription, -1);
	result = CryptVerifySignatureW(hHash, pbSignature, dwSigLen, hPubKey, wsDescription, dwFlags);
	if (wsDescription) CRYPT_Free(wsDescription);

	return result;
}

/******************************************************************************
 * SystemFunction036   (ADVAPI32.@)
 *
 * MSDN documents this function as RtlGenRandom and declares it in ntsecapi.h
 *
 * PARAMS
 *  pbBufer [O] Pointer to memory to receive random bytes.
 *  dwLen   [I] Number of random bytes to fetch.
 *
 * RETURNS
 *  Success: TRUE
 *  Failure: FALSE
 */

BOOL WINAPI SystemFunction036(PVOID pbBuffer, ULONG dwLen)
{
    int dev_random;

    /* FIXME: /dev/urandom does not provide random numbers of a sufficient
     * quality for cryptographic applications. /dev/random is much better,  
     * but it blocks if the kernel has not yet collected enough entropy for
     * the request, which will suspend the calling thread for an indefinite
     * amount of time. */
    dev_random = open("/dev/urandom", O_RDONLY);
    if (dev_random != -1)
    {
        if (read(dev_random, pbBuffer, dwLen) == (ssize_t)dwLen)
        {
            close(dev_random);
            return TRUE;
        }
        close(dev_random);
    }
    SetLastError(NTE_FAIL);
    return FALSE;
}    
    
/*
   These functions have nearly identical prototypes to CryptProtectMemory and CryptUnprotectMemory,
   in crypt32.dll.
 */

/******************************************************************************
 * SystemFunction040   (ADVAPI32.@)
 *
 * MSDN documents this function as RtlEncryptMemory.
 *
 * PARAMS
 *  memory [I/O] Pointer to memory to encrypt.
 *  length [I] Length of region to encrypt in bytes.
 *  flags  [I] Control whether other processes are able to decrypt the memory.
 *    RTL_ENCRYPT_OPTION_SAME_PROCESS 
 *    RTL_ENCRYPT_OPTION_CROSS_PROCESS 
 *    RTL_ENCRYPT_OPTION_SAME_LOGON
 *    
 * RETURNS
 *  Success: STATUS_SUCCESS
 *  Failure: NTSTATUS error code
 *
 * NOTES
 *  length must be a multiple of RTL_ENCRYPT_MEMORY_SIZE.
 *  If flags are specified when encrypting, the same flag value must be given
 *  when decrypting the memory.
 */
NTSTATUS WINAPI SystemFunction040(PVOID memory, ULONG length, ULONG flags)
{
	FIXME("(%p, %lx, %lx): stub [RtlEncryptMemory]\n", memory, length, flags);
	return STATUS_SUCCESS;
}

/******************************************************************************
 * SystemFunction041  (ADVAPI32.@)
 *
 * MSDN documents this function as RtlDecryptMemory.
 *
 * PARAMS
 *  memory [I/O] Pointer to memory to decrypt.
 *  length [I] Length of region to decrypt in bytes.
 *  flags  [I] Control whether other processes are able to decrypt the memory.
 *    RTL_ENCRYPT_OPTION_SAME_PROCESS
 *    RTL_ENCRYPT_OPTION_CROSS_PROCESS
 *    RTL_ENCRYPT_OPTION_SAME_LOGON
 *
 * RETURNS
 *  Success: STATUS_SUCCESS
 *  Failure: NTSTATUS error code
 *
 * NOTES
 *  length must be a multiple of RTL_ENCRYPT_MEMORY_SIZE.
 *  If flags are specified when encrypting, the same flag value must be given
 *  when decrypting the memory.
 */
NTSTATUS WINAPI SystemFunction041(PVOID memory, ULONG length, ULONG flags)
{
	FIXME("(%p, %lx, %lx): stub [RtlDecryptMemory]\n", memory, length, flags);
	return STATUS_SUCCESS;
}