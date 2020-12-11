/*
 * secur32 private definitions.
 *
 * Copyright (C) 2004 Juan Lang
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

#ifndef __SECUR32_PRIV_H__
#define __SECUR32_PRIV_H__

#include <sys/types.h>
#include "wine/list.h"

/* Memory allocation functions for memory accessible by callers of secur32.
 * There is no REALLOC, because LocalReAlloc can only work if used in
 * conjunction with LMEM_MOVEABLE and LocalLock, but callers aren't using
 * LocalLock.  I don't use the Heap functions because there seems to be an
 * implicit assumption that LocalAlloc and Free will be used--MS' secur32
 * imports them (but not the heap functions), the sample SSP uses them, and
 * there isn't an exported secur32 function to allocate memory.
 */
#define SECUR32_ALLOC(bytes) LocalAlloc(0, (bytes))
#define SECUR32_FREE(p)      LocalFree(p)

typedef struct _SecureProvider
{
    struct list             entry;
    BOOL                    loaded;
    PWSTR                   moduleName;
    HMODULE                 lib;
    SecurityFunctionTableA  fnTableA;
    SecurityFunctionTableW  fnTableW;
} SecureProvider;

typedef struct _SecurePackage
{
    struct list     entry;
    SecPkgInfoW     infoW;
    SecureProvider *provider;
} SecurePackage;

typedef enum _helper_mode {
    NTLM_SERVER,
    NTLM_CLIENT,
    NEGO_SERVER,
    NEGO_CLIENT,
    NUM_HELPER_MODES
} HelperMode;

typedef struct _NegoHelper {
    pid_t helper_pid;
    HelperMode mode;
    SEC_CHAR *password;
    int pwlen;
    int pipe_in;
    int pipe_out;
    int version;
    char *com_buf;
    int com_buf_size;
    int com_buf_offset;
} NegoHelper, *PNegoHelper;

/* Allocates space for and initializes a new provider.  If fnTableA or fnTableW
 * is non-NULL, assumes the provider is built-in (and is thus already loaded.)
 * Otherwise moduleName must not be NULL.
 * Returns a pointer to the stored provider entry, for use adding packages.
 */
SecureProvider *SECUR32_addProvider(PSecurityFunctionTableA fnTableA,
 PSecurityFunctionTableW fnTableW, PWSTR moduleName);

/* Allocates space for and adds toAdd packages with the given provider.
 * provider must not be NULL, and either infoA or infoW may be NULL, but not
 * both.
 */
void SECUR32_addPackages(SecureProvider *provider, ULONG toAdd,
 const SecPkgInfoA *infoA, const SecPkgInfoW *infoW);

/* Tries to find the package named packageName.  If it finds it, implicitly
 * loads the package if it isn't already loaded.
 */
SecurePackage *SECUR32_findPackageW(PWSTR packageName);

/* Tries to find the package named packageName.  (Thunks to _findPackageW)
 */
SecurePackage *SECUR32_findPackageA(PSTR packageName);

/* A few string helpers; will return NULL if str is NULL.  Free return with
 * SECUR32_FREE */
PWSTR SECUR32_strdupW(PCWSTR str);
PWSTR SECUR32_AllocWideFromMultiByte(PCSTR str);
PSTR  SECUR32_AllocMultiByteFromWide(PCWSTR str);

/* Initialization functions for built-in providers */
void SECUR32_initSchannelSP(void);
void SECUR32_initNegotiateSP(void);
void SECUR32_initNTLMSP(void);

/* Functions from dispatcher.c used elsewhere in the code */
SECURITY_STATUS fork_helper(PNegoHelper *new_helper, const char *prog,
        char * const argv[]);

SECURITY_STATUS run_helper(PNegoHelper helper, char *buffer,
        unsigned int max_buflen, int *buflen);

void cleanup_helper(PNegoHelper helper);

void check_version(PNegoHelper helper);

/* Functions from base64_codec.c used elsewhere */
SECURITY_STATUS encodeBase64(PBYTE in_buf, int in_len, char* out_buf, 
        int max_len, int *out_len);

SECURITY_STATUS decodeBase64(char *in_buf, int in_len, BYTE *out_buf, 
        int max_len, int *out_len);

#endif /* ndef __SECUR32_PRIV_H__ */
