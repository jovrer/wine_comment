/*
 * Copyright 1999, 2000 Juergen Schmied <juergen.schmied@debitel.net>
 * Copyright 2003 CodeWeavers Inc. (Ulrich Czekalla)
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
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "rpcnterr.h"
#include "winreg.h"
#include "winternl.h"
#include "winioctl.h"
#include "ntstatus.h"
#include "ntsecapi.h"
#include "accctrl.h"
#include "sddl.h"
#include "winsvc.h"
#include "aclapi.h"

#include "wine/debug.h"
#include "wine/unicode.h"

WINE_DEFAULT_DEBUG_CHANNEL(advapi);

static BOOL ParseStringSidToSid(LPCWSTR StringSid, PSID pSid, LPDWORD cBytes);
static BOOL ParseStringAclToAcl(LPCWSTR StringAcl, LPDWORD lpdwFlags, 
    PACL pAcl, LPDWORD cBytes);
static BYTE ParseAceStringFlags(LPCWSTR* StringAcl);
static BYTE ParseAceStringType(LPCWSTR* StringAcl);
static DWORD ParseAceStringRights(LPCWSTR* StringAcl);
static BOOL ParseStringSecurityDescriptorToSecurityDescriptor(
    LPCWSTR StringSecurityDescriptor,
    SECURITY_DESCRIPTOR* SecurityDescriptor,
    LPDWORD cBytes);
static DWORD ParseAclStringFlags(LPCWSTR* StringAcl);

typedef struct _ACEFLAG
{
   LPCWSTR wstr;
   DWORD value;
} ACEFLAG, *LPACEFLAG;

static SID const sidWorld = { SID_REVISION, 1, { SECURITY_WORLD_SID_AUTHORITY} , { SECURITY_WORLD_RID } };

/*
 * ACE access rights
 */
static const WCHAR SDDL_READ_CONTROL[]     = {'R','C',0};
static const WCHAR SDDL_WRITE_DAC[]        = {'W','D',0};
static const WCHAR SDDL_WRITE_OWNER[]      = {'W','O',0};
static const WCHAR SDDL_STANDARD_DELETE[]  = {'S','D',0};
static const WCHAR SDDL_GENERIC_ALL[]      = {'G','A',0};
static const WCHAR SDDL_GENERIC_READ[]     = {'G','R',0};
static const WCHAR SDDL_GENERIC_WRITE[]    = {'G','W',0};
static const WCHAR SDDL_GENERIC_EXECUTE[]  = {'G','X',0};

/*
 * ACE types
 */
static const WCHAR SDDL_ACCESS_ALLOWED[]        = {'A',0};
static const WCHAR SDDL_ACCESS_DENIED[]         = {'D',0};
static const WCHAR SDDL_OBJECT_ACCESS_ALLOWED[] = {'O','A',0};
static const WCHAR SDDL_OBJECT_ACCESS_DENIED[]  = {'O','D',0};
static const WCHAR SDDL_AUDIT[]                 = {'A','U',0};
static const WCHAR SDDL_ALARM[]                 = {'A','L',0};
static const WCHAR SDDL_OBJECT_AUDIT[]          = {'O','U',0};
static const WCHAR SDDL_OBJECT_ALARMp[]         = {'O','L',0};

/*
 * ACE flags
 */
static const WCHAR SDDL_CONTAINER_INHERIT[]  = {'C','I',0};
static const WCHAR SDDL_OBJECT_INHERIT[]     = {'O','I',0};
static const WCHAR SDDL_NO_PROPAGATE[]       = {'N','P',0};
static const WCHAR SDDL_INHERIT_ONLY[]       = {'I','O',0};
static const WCHAR SDDL_INHERITED[]          = {'I','D',0};
static const WCHAR SDDL_AUDIT_SUCCESS[]      = {'S','A',0};
static const WCHAR SDDL_AUDIT_FAILURE[]      = {'F','A',0};

/* set last error code from NT status and get the proper boolean return value */
/* used for functions that are a simple wrapper around the corresponding ntdll API */
static inline BOOL set_ntstatus( NTSTATUS status )
{
    if (status) SetLastError( RtlNtStatusToDosError( status ));
    return !status;
}

#define	WINE_SIZE_OF_WORLD_ACCESS_ACL	(sizeof(ACL) + sizeof(ACCESS_ALLOWED_ACE) + sizeof(sidWorld) - sizeof(DWORD))

static void GetWorldAccessACL(PACL pACL)
{
    PACCESS_ALLOWED_ACE pACE = (PACCESS_ALLOWED_ACE) (pACL + 1);

    pACL->AclRevision = ACL_REVISION;
    pACL->Sbz1 = 0;
    pACL->AclSize = WINE_SIZE_OF_WORLD_ACCESS_ACL;
    pACL->AceCount = 1;
    pACL->Sbz2 = 0;

    pACE->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    pACE->Header.AceFlags = CONTAINER_INHERIT_ACE;
    pACE->Header.AceSize = sizeof(ACCESS_ALLOWED_ACE) + sizeof(sidWorld) - sizeof(DWORD);
    pACE->Mask = 0xf3ffffff; /* Everything except reserved bits */
    memcpy(&pACE->SidStart, &sidWorld, sizeof(sidWorld));
}

/************************************************************
 *                ADVAPI_IsLocalComputer
 *
 * Checks whether the server name indicates local machine.
 */
static BOOL ADVAPI_IsLocalComputer(LPCWSTR ServerName)
{
    DWORD dwSize = MAX_COMPUTERNAME_LENGTH + 1;
    BOOL Result;
    LPWSTR buf;

    if (!ServerName || !ServerName[0])
        return TRUE;
    
    buf = HeapAlloc(GetProcessHeap(), 0, dwSize * sizeof(WCHAR));
    Result = GetComputerNameW(buf,  &dwSize);
    if (Result && (ServerName[0] == '\\') && (ServerName[1] == '\\'))
        ServerName += 2;
    Result = Result && !lstrcmpW(ServerName, buf);
    HeapFree(GetProcessHeap(), 0, buf);

    return Result;
}

/*	##############################
	######	TOKEN FUNCTIONS ######
	##############################
*/

/******************************************************************************
 * OpenProcessToken			[ADVAPI32.@]
 * Opens the access token associated with a process handle.
 *
 * PARAMS
 *   ProcessHandle [I] Handle to process
 *   DesiredAccess [I] Desired access to process
 *   TokenHandle   [O] Pointer to handle of open access token
 *
 * RETURNS
 *  Success: TRUE. TokenHandle contains the access token.
 *  Failure: FALSE.
 *
 * NOTES
 *  See NtOpenProcessToken.
 */
BOOL WINAPI
OpenProcessToken( HANDLE ProcessHandle, DWORD DesiredAccess,
                  HANDLE *TokenHandle )
{
	return set_ntstatus(NtOpenProcessToken( ProcessHandle, DesiredAccess, TokenHandle ));
}

/******************************************************************************
 * OpenThreadToken [ADVAPI32.@]
 *
 * Opens the access token associated with a thread handle.
 *
 * PARAMS
 *   ThreadHandle  [I] Handle to process
 *   DesiredAccess [I] Desired access to the thread
 *   OpenAsSelf    [I] ???
 *   TokenHandle   [O] Destination for the token handle
 *
 * RETURNS
 *  Success: TRUE. TokenHandle contains the access token.
 *  Failure: FALSE.
 *
 * NOTES
 *  See NtOpenThreadToken.
 */
BOOL WINAPI
OpenThreadToken( HANDLE ThreadHandle, DWORD DesiredAccess,
		 BOOL OpenAsSelf, HANDLE *TokenHandle)
{
	return set_ntstatus( NtOpenThreadToken(ThreadHandle, DesiredAccess, OpenAsSelf, TokenHandle));
}

BOOL WINAPI
AdjustTokenGroups( HANDLE TokenHandle, BOOL ResetToDefault, PTOKEN_GROUPS NewState,
                   DWORD BufferLength, PTOKEN_GROUPS PreviousState, PDWORD ReturnLength )
{
    return set_ntstatus( NtAdjustGroupsToken(TokenHandle, ResetToDefault, NewState, BufferLength,
                                             PreviousState, ReturnLength));
}

/******************************************************************************
 * AdjustTokenPrivileges [ADVAPI32.@]
 *
 * Adjust the privileges of an open token handle.
 * 
 * PARAMS
 *  TokenHandle          [I]   Handle from OpenProcessToken() or OpenThreadToken() 
 *  DisableAllPrivileges [I]   TRUE=Remove all privileges, FALSE=Use NewState
 *  NewState             [I]   Desired new privileges of the token
 *  BufferLength         [I]   Length of NewState
 *  PreviousState        [O]   Destination for the previous state
 *  ReturnLength         [I/O] Size of PreviousState
 *
 *
 * RETURNS
 *  Success: TRUE. Privileges are set to NewState and PreviousState is updated.
 *  Failure: FALSE.
 *
 * NOTES
 *  See NtAdjustPrivilegesToken.
 */
BOOL WINAPI
AdjustTokenPrivileges( HANDLE TokenHandle, BOOL DisableAllPrivileges,
                       LPVOID NewState, DWORD BufferLength,
                       LPVOID PreviousState, LPDWORD ReturnLength )
{
    NTSTATUS status;

    TRACE("\n");
    
    status = NtAdjustPrivilegesToken(TokenHandle, DisableAllPrivileges,
                                                     NewState, BufferLength, PreviousState,
                                                     ReturnLength);
    SetLastError( RtlNtStatusToDosError( status ));
    if ((status == STATUS_SUCCESS) || (status == STATUS_NOT_ALL_ASSIGNED))
        return TRUE;
    else
        return FALSE;
}

/******************************************************************************
 * CheckTokenMembership [ADVAPI32.@]
 *
 * Determine if an access token is a member of a SID.
 * 
 * PARAMS
 *   TokenHandle [I] Handle from OpenProcessToken() or OpenThreadToken()
 *   SidToCheck  [I] SID that possibly contains the token
 *   IsMember    [O] Destination for result.
 *
 * RETURNS
 *  Success: TRUE. IsMember is TRUE if TokenHandle is a member, FALSE otherwise.
 *  Failure: FALSE.
 */
BOOL WINAPI
CheckTokenMembership( HANDLE TokenHandle, PSID SidToCheck,
                      PBOOL IsMember )
{
  FIXME("(%p %p %p) stub!\n", TokenHandle, SidToCheck, IsMember);

  *IsMember = TRUE;
  return(TRUE);
}

/******************************************************************************
 * GetTokenInformation [ADVAPI32.@]
 *
 * PARAMS
 *   token           [I] Handle from OpenProcessToken() or OpenThreadToken()
 *   tokeninfoclass  [I] A TOKEN_INFORMATION_CLASS from "winnt.h"
 *   tokeninfo       [O] Destination for token information
 *   tokeninfolength [I] Length of tokeninfo
 *   retlen          [O] Destination for returned token information length
 *
 * RETURNS
 *  Success: TRUE. tokeninfo contains retlen bytes of token information
 *  Failure: FALSE.
 *
 * NOTES
 *  See NtQueryInformationToken.
 */
BOOL WINAPI
GetTokenInformation( HANDLE token, TOKEN_INFORMATION_CLASS tokeninfoclass,
		     LPVOID tokeninfo, DWORD tokeninfolength, LPDWORD retlen )
{
    TRACE("(%p, %s, %p, %ld, %p): \n",
          token,
          (tokeninfoclass == TokenUser) ? "TokenUser" :
          (tokeninfoclass == TokenGroups) ? "TokenGroups" :
          (tokeninfoclass == TokenPrivileges) ? "TokenPrivileges" :
          (tokeninfoclass == TokenOwner) ? "TokenOwner" :
          (tokeninfoclass == TokenPrimaryGroup) ? "TokenPrimaryGroup" :
          (tokeninfoclass == TokenDefaultDacl) ? "TokenDefaultDacl" :
          (tokeninfoclass == TokenSource) ? "TokenSource" :
          (tokeninfoclass == TokenType) ? "TokenType" :
          (tokeninfoclass == TokenImpersonationLevel) ? "TokenImpersonationLevel" :
          (tokeninfoclass == TokenStatistics) ? "TokenStatistics" :
          (tokeninfoclass == TokenRestrictedSids) ? "TokenRestrictedSids" :
          (tokeninfoclass == TokenSessionId) ? "TokenSessionId" :
          (tokeninfoclass == TokenGroupsAndPrivileges) ? "TokenGroupsAndPrivileges" :
          (tokeninfoclass == TokenSessionReference) ? "TokenSessionReference" :
          (tokeninfoclass == TokenSandBoxInert) ? "TokenSandBoxInert" :
          "Unknown",
          tokeninfo, tokeninfolength, retlen);
    return set_ntstatus( NtQueryInformationToken( token, tokeninfoclass, tokeninfo,
                                                  tokeninfolength, retlen));
}

/******************************************************************************
 * SetTokenInformation [ADVAPI32.@]
 *
 * Set information for an access token.
 *
 * PARAMS
 *   token           [I] Handle from OpenProcessToken() or OpenThreadToken()
 *   tokeninfoclass  [I] A TOKEN_INFORMATION_CLASS from "winnt.h"
 *   tokeninfo       [I] Token information to set
 *   tokeninfolength [I] Length of tokeninfo
 *
 * RETURNS
 *  Success: TRUE. The information for the token is set to tokeninfo.
 *  Failure: FALSE.
 */
BOOL WINAPI
SetTokenInformation( HANDLE token, TOKEN_INFORMATION_CLASS tokeninfoclass,
		     LPVOID tokeninfo, DWORD tokeninfolength )
{
    TRACE("(%p, %s, %p, %ld): stub\n",
          token,
          (tokeninfoclass == TokenUser) ? "TokenUser" :
          (tokeninfoclass == TokenGroups) ? "TokenGroups" :
          (tokeninfoclass == TokenPrivileges) ? "TokenPrivileges" :
          (tokeninfoclass == TokenOwner) ? "TokenOwner" :
          (tokeninfoclass == TokenPrimaryGroup) ? "TokenPrimaryGroup" :
          (tokeninfoclass == TokenDefaultDacl) ? "TokenDefaultDacl" :
          (tokeninfoclass == TokenSource) ? "TokenSource" :
          (tokeninfoclass == TokenType) ? "TokenType" :
          (tokeninfoclass == TokenImpersonationLevel) ? "TokenImpersonationLevel" :
          (tokeninfoclass == TokenStatistics) ? "TokenStatistics" :
          (tokeninfoclass == TokenRestrictedSids) ? "TokenRestrictedSids" :
          (tokeninfoclass == TokenSessionId) ? "TokenSessionId" :
          (tokeninfoclass == TokenGroupsAndPrivileges) ? "TokenGroupsAndPrivileges" :
          (tokeninfoclass == TokenSessionReference) ? "TokenSessionReference" :
          (tokeninfoclass == TokenSandBoxInert) ? "TokenSandBoxInert" :
          "Unknown",
          tokeninfo, tokeninfolength);

    return set_ntstatus( NtSetInformationToken( token, tokeninfoclass, tokeninfo, tokeninfolength ));
}

/*************************************************************************
 * SetThreadToken [ADVAPI32.@]
 *
 * Assigns an 'impersonation token' to a thread so it can assume the
 * security privileges of another thread or process.  Can also remove
 * a previously assigned token. 
 *
 * PARAMS
 *   thread          [O] Handle to thread to set the token for
 *   token           [I] Token to set
 *
 * RETURNS
 *  Success: TRUE. The threads access token is set to token
 *  Failure: FALSE.
 *
 * NOTES
 *  Only supported on NT or higher. On Win9X this function does nothing.
 *  See SetTokenInformation.
 */
BOOL WINAPI SetThreadToken(PHANDLE thread, HANDLE token)
{
    return set_ntstatus( NtSetInformationThread( thread ? *thread : GetCurrentThread(),
                                                 ThreadImpersonationToken, &token, sizeof token ));
}

/*	##############################
	######	SID FUNCTIONS	######
	##############################
*/

/******************************************************************************
 * AllocateAndInitializeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pIdentifierAuthority []
 *   nSubAuthorityCount   []
 *   nSubAuthority0       []
 *   nSubAuthority1       []
 *   nSubAuthority2       []
 *   nSubAuthority3       []
 *   nSubAuthority4       []
 *   nSubAuthority5       []
 *   nSubAuthority6       []
 *   nSubAuthority7       []
 *   pSid                 []
 */
BOOL WINAPI
AllocateAndInitializeSid( PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
                          BYTE nSubAuthorityCount,
                          DWORD nSubAuthority0, DWORD nSubAuthority1,
                          DWORD nSubAuthority2, DWORD nSubAuthority3,
                          DWORD nSubAuthority4, DWORD nSubAuthority5,
                          DWORD nSubAuthority6, DWORD nSubAuthority7,
                          PSID *pSid )
{
    return set_ntstatus( RtlAllocateAndInitializeSid(
                             pIdentifierAuthority, nSubAuthorityCount,
                             nSubAuthority0, nSubAuthority1, nSubAuthority2, nSubAuthority3,
                             nSubAuthority4, nSubAuthority5, nSubAuthority6, nSubAuthority7,
                             pSid ));
}

/******************************************************************************
 * FreeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PVOID WINAPI
FreeSid( PSID pSid )
{
    	RtlFreeSid(pSid);
	return NULL; /* is documented like this */
}

/******************************************************************************
 * CopySid [ADVAPI32.@]
 *
 * PARAMS
 *   nDestinationSidLength []
 *   pDestinationSid       []
 *   pSourceSid            []
 */
BOOL WINAPI
CopySid( DWORD nDestinationSidLength, PSID pDestinationSid, PSID pSourceSid )
{
	return RtlCopySid(nDestinationSidLength, pDestinationSid, pSourceSid);
}

BOOL WINAPI
IsTokenRestricted( HANDLE TokenHandle )
{
    TOKEN_GROUPS *groups;
    DWORD size;
    NTSTATUS status;
    BOOL restricted;

    TRACE("(%p)\n", TokenHandle);
 
    status = NtQueryInformationToken(TokenHandle, TokenRestrictedSids, NULL, 0, &size);
    if (status != STATUS_BUFFER_TOO_SMALL)
        return FALSE;
 
    groups = HeapAlloc(GetProcessHeap(), 0, size);
    if (!groups)
    {
        SetLastError(ERROR_OUTOFMEMORY);
        return FALSE;
    }
 
    status = NtQueryInformationToken(TokenHandle, TokenRestrictedSids, groups, size, &size);
    if (status != STATUS_SUCCESS)
    {
        HeapFree(GetProcessHeap(), 0, groups);
        return set_ntstatus(status);
    }
 
    if (groups->GroupCount)
        restricted = TRUE;
    else
        restricted = FALSE;
     
    HeapFree(GetProcessHeap(), 0, groups);
 
    return restricted;
}

/******************************************************************************
 * IsValidSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
BOOL WINAPI
IsValidSid( PSID pSid )
{
	return RtlValidSid( pSid );
}

/******************************************************************************
 * EqualSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid1 []
 *   pSid2 []
 */
BOOL WINAPI
EqualSid( PSID pSid1, PSID pSid2 )
{
	return RtlEqualSid( pSid1, pSid2 );
}

/******************************************************************************
 * EqualPrefixSid [ADVAPI32.@]
 */
BOOL WINAPI EqualPrefixSid (PSID pSid1, PSID pSid2)
{
	return RtlEqualPrefixSid(pSid1, pSid2);
}

/******************************************************************************
 * GetSidLengthRequired [ADVAPI32.@]
 *
 * PARAMS
 *   nSubAuthorityCount []
 */
DWORD WINAPI
GetSidLengthRequired( BYTE nSubAuthorityCount )
{
	return RtlLengthRequiredSid(nSubAuthorityCount);
}

/******************************************************************************
 * InitializeSid [ADVAPI32.@]
 *
 * PARAMS
 *   pIdentifierAuthority []
 */
BOOL WINAPI
InitializeSid (
	PSID pSid,
	PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
	BYTE nSubAuthorityCount)
{
	return RtlInitializeSid(pSid, pIdentifierAuthority, nSubAuthorityCount);
}

DWORD WINAPI
GetEffectiveRightsFromAclA( PACL pacl, PTRUSTEEA pTrustee, PACCESS_MASK pAccessRights )
{
    FIXME("%p %p %p - stub\n", pacl, pTrustee, pAccessRights);

    return 1;
}

DWORD WINAPI
GetEffectiveRightsFromAclW( PACL pacl, PTRUSTEEW pTrustee, PACCESS_MASK pAccessRights )
{
    FIXME("%p %p %p - stub\n", pacl, pTrustee, pAccessRights);

    return 1;
}

/******************************************************************************
 * GetSidIdentifierAuthority [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PSID_IDENTIFIER_AUTHORITY WINAPI
GetSidIdentifierAuthority( PSID pSid )
{
	return RtlIdentifierAuthoritySid(pSid);
}

/******************************************************************************
 * GetSidSubAuthority [ADVAPI32.@]
 *
 * PARAMS
 *   pSid          []
 *   nSubAuthority []
 */
PDWORD WINAPI
GetSidSubAuthority( PSID pSid, DWORD nSubAuthority )
{
	return RtlSubAuthoritySid(pSid, nSubAuthority);
}

/******************************************************************************
 * GetSidSubAuthorityCount [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
PUCHAR WINAPI
GetSidSubAuthorityCount (PSID pSid)
{
	return RtlSubAuthorityCountSid(pSid);
}

/******************************************************************************
 * GetLengthSid [ADVAPI32.@]
 *
 * PARAMS
 *   pSid []
 */
DWORD WINAPI
GetLengthSid (PSID pSid)
{
	return RtlLengthSid(pSid);
}

/*	##############################################
	######	SECURITY DESCRIPTOR FUNCTIONS	######
	##############################################
*/

 /****************************************************************************** 
 * BuildSecurityDescriptorA [ADVAPI32.@]
 *
 * Builds a SD from 
 *
 * PARAMS
 *  pOwner                [I]
 *  pGroup                [I]
 *  cCountOfAccessEntries [I]
 *  pListOfAccessEntries  [I]
 *  cCountOfAuditEntries  [I]
 *  pListofAuditEntries   [I]
 *  pOldSD                [I]
 *  lpdwBufferLength      [I/O]
 *  pNewSD                [O]
 */
DWORD WINAPI BuildSecurityDescriptorA(
    IN PTRUSTEE_A pOwner,
    IN PTRUSTEE_A pGroup,
    IN DWORD cCountOfAccessEntries,
    IN PEXPLICIT_ACCESS_A pListOfAccessEntries,
    IN DWORD cCountOfAuditEntries,
    IN PEXPLICIT_ACCESS_A pListofAuditEntries,
    IN PSECURITY_DESCRIPTOR pOldSD,
    IN OUT PDWORD lpdwBufferLength,
    OUT PSECURITY_DESCRIPTOR pNewSD)
{ 
    FIXME("(%p,%p,%ld,%p,%ld,%p,%p,%p,%p) stub!\n",pOwner,pGroup,
          cCountOfAccessEntries,pListOfAccessEntries,cCountOfAuditEntries,
          pListofAuditEntries,pOldSD,lpdwBufferLength,pNewSD);
 
    return ERROR_CALL_NOT_IMPLEMENTED;
} 
 
/******************************************************************************
 * BuildSecurityDescriptorW [ADVAPI32.@]
 *
 * See BuildSecurityDescriptorA.
 */
DWORD WINAPI BuildSecurityDescriptorW(
    IN PTRUSTEE_W pOwner,
    IN PTRUSTEE_W pGroup,
    IN DWORD cCountOfAccessEntries,
    IN PEXPLICIT_ACCESS_W pListOfAccessEntries,
    IN DWORD cCountOfAuditEntries,
    IN PEXPLICIT_ACCESS_W pListofAuditEntries,
    IN PSECURITY_DESCRIPTOR pOldSD,
    IN OUT PDWORD lpdwBufferLength,
    OUT PSECURITY_DESCRIPTOR pNewSD)
{ 
    FIXME("(%p,%p,%ld,%p,%ld,%p,%p,%p,%p) stub!\n",pOwner,pGroup,
          cCountOfAccessEntries,pListOfAccessEntries,cCountOfAuditEntries,
          pListofAuditEntries,pOldSD,lpdwBufferLength,pNewSD);
 
    return ERROR_CALL_NOT_IMPLEMENTED;
} 

/******************************************************************************
 * InitializeSecurityDescriptor [ADVAPI32.@]
 *
 * PARAMS
 *   pDescr   []
 *   revision []
 */
BOOL WINAPI
InitializeSecurityDescriptor( PSECURITY_DESCRIPTOR pDescr, DWORD revision )
{
	return set_ntstatus( RtlCreateSecurityDescriptor(pDescr, revision ));
}


/******************************************************************************
 * MakeAbsoluteSD [ADVAPI32.@]
 */
BOOL WINAPI MakeAbsoluteSD (
        IN PSECURITY_DESCRIPTOR pSelfRelativeSecurityDescriptor,
	OUT PSECURITY_DESCRIPTOR pAbsoluteSecurityDescriptor,
	OUT LPDWORD lpdwAbsoluteSecurityDescriptorSize,
	OUT PACL pDacl,
	OUT LPDWORD lpdwDaclSize,
	OUT PACL pSacl,
	OUT LPDWORD lpdwSaclSize,
	OUT PSID pOwner,
	OUT LPDWORD lpdwOwnerSize,
	OUT PSID pPrimaryGroup,
	OUT LPDWORD lpdwPrimaryGroupSize)
{
    return set_ntstatus( RtlSelfRelativeToAbsoluteSD(pSelfRelativeSecurityDescriptor,
                                                     pAbsoluteSecurityDescriptor,
                                                     lpdwAbsoluteSecurityDescriptorSize,
                                                     pDacl, lpdwDaclSize, pSacl, lpdwSaclSize,
                                                     pOwner, lpdwOwnerSize,
                                                     pPrimaryGroup, lpdwPrimaryGroupSize));
}

/******************************************************************************
 * GetKernelObjectSecurity [ADVAPI32.@]
 */
BOOL WINAPI GetKernelObjectSecurity(
        HANDLE Handle,
        SECURITY_INFORMATION RequestedInformation,
        PSECURITY_DESCRIPTOR pSecurityDescriptor,
        DWORD nLength,
        LPDWORD lpnLengthNeeded )
{
    TRACE("(%p,0x%08lx,%p,0x%08lx,%p)\n", Handle, RequestedInformation,
          pSecurityDescriptor, nLength, lpnLengthNeeded);

    return set_ntstatus( NtQuerySecurityObject(Handle, RequestedInformation, pSecurityDescriptor,
                                               nLength, lpnLengthNeeded ));
}

/******************************************************************************
 * GetPrivateObjectSecurity [ADVAPI32.@]
 */
BOOL WINAPI GetPrivateObjectSecurity(
        PSECURITY_DESCRIPTOR ObjectDescriptor,
        SECURITY_INFORMATION SecurityInformation,
        PSECURITY_DESCRIPTOR ResultantDescriptor,
        DWORD DescriptorLength,
        PDWORD ReturnLength )
{
    TRACE("(%p,0x%08lx,%p,0x%08lx,%p)\n", ObjectDescriptor, SecurityInformation,
          ResultantDescriptor, DescriptorLength, ReturnLength);

    return set_ntstatus( NtQuerySecurityObject(ObjectDescriptor, SecurityInformation,
                                               ResultantDescriptor, DescriptorLength, ReturnLength ));
}

/******************************************************************************
 * GetSecurityDescriptorLength [ADVAPI32.@]
 */
DWORD WINAPI GetSecurityDescriptorLength( PSECURITY_DESCRIPTOR pDescr)
{
	return RtlLengthSecurityDescriptor(pDescr);
}

/******************************************************************************
 * GetSecurityDescriptorOwner [ADVAPI32.@]
 *
 * PARAMS
 *   pOwner            []
 *   lpbOwnerDefaulted []
 */
BOOL WINAPI
GetSecurityDescriptorOwner( PSECURITY_DESCRIPTOR pDescr, PSID *pOwner,
			    LPBOOL lpbOwnerDefaulted )
{
    BOOLEAN defaulted;
    BOOL ret = set_ntstatus( RtlGetOwnerSecurityDescriptor( pDescr, pOwner, &defaulted ));
    *lpbOwnerDefaulted = defaulted;
    return ret;
}

/******************************************************************************
 * SetSecurityDescriptorOwner [ADVAPI32.@]
 *
 * PARAMS
 */
BOOL WINAPI SetSecurityDescriptorOwner( PSECURITY_DESCRIPTOR pSecurityDescriptor,
				   PSID pOwner, BOOL bOwnerDefaulted)
{
    return set_ntstatus( RtlSetOwnerSecurityDescriptor(pSecurityDescriptor, pOwner, bOwnerDefaulted));
}
/******************************************************************************
 * GetSecurityDescriptorGroup			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorGroup(
	PSECURITY_DESCRIPTOR SecurityDescriptor,
	PSID *Group,
	LPBOOL GroupDefaulted)
{
    BOOLEAN defaulted;
    BOOL ret = set_ntstatus( RtlGetGroupSecurityDescriptor(SecurityDescriptor, Group, &defaulted ));
    *GroupDefaulted = defaulted;
    return ret;
}
/******************************************************************************
 * SetSecurityDescriptorGroup [ADVAPI32.@]
 */
BOOL WINAPI SetSecurityDescriptorGroup ( PSECURITY_DESCRIPTOR SecurityDescriptor,
					   PSID Group, BOOL GroupDefaulted)
{
    return set_ntstatus( RtlSetGroupSecurityDescriptor( SecurityDescriptor, Group, GroupDefaulted));
}

/******************************************************************************
 * IsValidSecurityDescriptor [ADVAPI32.@]
 *
 * PARAMS
 *   lpsecdesc []
 */
BOOL WINAPI
IsValidSecurityDescriptor( PSECURITY_DESCRIPTOR SecurityDescriptor )
{
    return set_ntstatus( RtlValidSecurityDescriptor(SecurityDescriptor));
}

/******************************************************************************
 *  GetSecurityDescriptorDacl			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorDacl(
	IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
	OUT LPBOOL lpbDaclPresent,
	OUT PACL *pDacl,
	OUT LPBOOL lpbDaclDefaulted)
{
    BOOLEAN present, defaulted;
    BOOL ret = set_ntstatus( RtlGetDaclSecurityDescriptor(pSecurityDescriptor, &present, pDacl, &defaulted));
    *lpbDaclPresent = present;
    *lpbDaclDefaulted = defaulted;
    return ret;
}

/******************************************************************************
 *  SetSecurityDescriptorDacl			[ADVAPI32.@]
 */
BOOL WINAPI
SetSecurityDescriptorDacl (
	PSECURITY_DESCRIPTOR lpsd,
	BOOL daclpresent,
	PACL dacl,
	BOOL dacldefaulted )
{
    return set_ntstatus( RtlSetDaclSecurityDescriptor (lpsd, daclpresent, dacl, dacldefaulted ) );
}
/******************************************************************************
 *  GetSecurityDescriptorSacl			[ADVAPI32.@]
 */
BOOL WINAPI GetSecurityDescriptorSacl(
	IN PSECURITY_DESCRIPTOR lpsd,
	OUT LPBOOL lpbSaclPresent,
	OUT PACL *pSacl,
	OUT LPBOOL lpbSaclDefaulted)
{
    BOOLEAN present, defaulted;
    BOOL ret = set_ntstatus( RtlGetSaclSecurityDescriptor(lpsd, &present, pSacl, &defaulted) );
    *lpbSaclPresent = present;
    *lpbSaclDefaulted = defaulted;
    return ret;
}

/**************************************************************************
 * SetSecurityDescriptorSacl			[ADVAPI32.@]
 */
BOOL WINAPI SetSecurityDescriptorSacl (
	PSECURITY_DESCRIPTOR lpsd,
	BOOL saclpresent,
	PACL lpsacl,
	BOOL sacldefaulted)
{
    return set_ntstatus (RtlSetSaclSecurityDescriptor(lpsd, saclpresent, lpsacl, sacldefaulted));
}
/******************************************************************************
 * MakeSelfRelativeSD [ADVAPI32.@]
 *
 * PARAMS
 *   lpabssecdesc  []
 *   lpselfsecdesc []
 *   lpbuflen      []
 */
BOOL WINAPI
MakeSelfRelativeSD(
	IN PSECURITY_DESCRIPTOR pAbsoluteSecurityDescriptor,
	IN PSECURITY_DESCRIPTOR pSelfRelativeSecurityDescriptor,
	IN OUT LPDWORD lpdwBufferLength)
{
    return set_ntstatus( RtlMakeSelfRelativeSD( pAbsoluteSecurityDescriptor,
                                                pSelfRelativeSecurityDescriptor, lpdwBufferLength));
}

/******************************************************************************
 * GetSecurityDescriptorControl			[ADVAPI32.@]
 */

BOOL WINAPI GetSecurityDescriptorControl ( PSECURITY_DESCRIPTOR  pSecurityDescriptor,
		 PSECURITY_DESCRIPTOR_CONTROL pControl, LPDWORD lpdwRevision)
{
    return set_ntstatus( RtlGetControlSecurityDescriptor(pSecurityDescriptor,pControl,lpdwRevision));
}

/*	##############################
	######	ACL FUNCTIONS	######
	##############################
*/

/*************************************************************************
 * InitializeAcl [ADVAPI32.@]
 */
BOOL WINAPI InitializeAcl(PACL acl, DWORD size, DWORD rev)
{
    return set_ntstatus( RtlCreateAcl(acl, size, rev));
}

BOOL WINAPI ImpersonateNamedPipeClient( HANDLE hNamedPipe )
{
    TRACE("(%p)\n", hNamedPipe);

    return set_ntstatus( NtFsControlFile(hNamedPipe, NULL, NULL, NULL, NULL,
                         FSCTL_PIPE_IMPERSONATE, NULL, 0, NULL, 0) );
}

/******************************************************************************
 *  AddAccessAllowedAce [ADVAPI32.@]
 */
BOOL WINAPI AddAccessAllowedAce(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
        IN DWORD AccessMask,
        IN PSID pSid)
{
    return set_ntstatus(RtlAddAccessAllowedAce(pAcl, dwAceRevision, AccessMask, pSid));
}

/******************************************************************************
 *  AddAccessAllowedAceEx [ADVAPI32.@]
 */
BOOL WINAPI AddAccessAllowedAceEx(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
	IN DWORD AceFlags,
        IN DWORD AccessMask,
        IN PSID pSid)
{
    return set_ntstatus(RtlAddAccessAllowedAceEx(pAcl, dwAceRevision, AceFlags, AccessMask, pSid));
}

/******************************************************************************
 *  AddAccessDeniedAce [ADVAPI32.@]
 */
BOOL WINAPI AddAccessDeniedAce(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
        IN DWORD AccessMask,
        IN PSID pSid)
{
    return set_ntstatus(RtlAddAccessDeniedAce(pAcl, dwAceRevision, AccessMask, pSid));
}

/******************************************************************************
 *  AddAccessDeniedAceEx [ADVAPI32.@]
 */
BOOL WINAPI AddAccessDeniedAceEx(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
	IN DWORD AceFlags,
        IN DWORD AccessMask,
        IN PSID pSid)
{
    return set_ntstatus(RtlAddAccessDeniedAceEx(pAcl, dwAceRevision, AceFlags, AccessMask, pSid));
}

/******************************************************************************
 *  AddAce [ADVAPI32.@]
 */
BOOL WINAPI AddAce(
        IN OUT PACL pAcl,
        IN DWORD dwAceRevision,
        IN DWORD dwStartingAceIndex,
        LPVOID pAceList,
        DWORD nAceListLength)
{
    return set_ntstatus(RtlAddAce(pAcl, dwAceRevision, dwStartingAceIndex, pAceList, nAceListLength));
}

/******************************************************************************
 * DeleteAce [ADVAPI32.@]
 */
BOOL WINAPI DeleteAce(PACL pAcl, DWORD dwAceIndex)
{
    return set_ntstatus(RtlDeleteAce(pAcl, dwAceIndex));
}

/******************************************************************************
 *  FindFirstFreeAce [ADVAPI32.@]
 */
BOOL WINAPI FindFirstFreeAce(IN PACL pAcl, LPVOID * pAce)
{
	return RtlFirstFreeAce(pAcl, (PACE_HEADER *)pAce);
}

/******************************************************************************
 * GetAce [ADVAPI32.@]
 */
BOOL WINAPI GetAce(PACL pAcl,DWORD dwAceIndex,LPVOID *pAce )
{
    return set_ntstatus(RtlGetAce(pAcl, dwAceIndex, pAce));
}

/******************************************************************************
 * GetAclInformation [ADVAPI32.@]
 */
BOOL WINAPI GetAclInformation(
  PACL pAcl,
  LPVOID pAclInformation,
  DWORD nAclInformationLength,
  ACL_INFORMATION_CLASS dwAclInformationClass)
{
    return set_ntstatus(RtlQueryInformationAcl(pAcl, pAclInformation,
                                               nAclInformationLength, dwAclInformationClass));
}

/******************************************************************************
 *  IsValidAcl [ADVAPI32.@]
 */
BOOL WINAPI IsValidAcl(IN PACL pAcl)
{
	return RtlValidAcl(pAcl);
}

/*	##############################
	######	MISC FUNCTIONS	######
	##############################
*/

/******************************************************************************
 * AllocateLocallyUniqueId [ADVAPI32.@]
 *
 * PARAMS
 *   lpLuid []
 */
BOOL WINAPI AllocateLocallyUniqueId( PLUID lpLuid )
{
    return set_ntstatus(NtAllocateLocallyUniqueId(lpLuid));
}

static const WCHAR SE_CREATE_TOKEN_NAME_W[] =
 { 'S','e','C','r','e','a','t','e','T','o','k','e','n','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_ASSIGNPRIMARYTOKEN_NAME_W[] =
 { 'S','e','A','s','s','i','g','n','P','r','i','m','a','r','y','T','o','k','e','n','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_LOCK_MEMORY_NAME_W[] =
 { 'S','e','L','o','c','k','M','e','m','o','r','y','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_INCREASE_QUOTA_NAME_W[] =
 { 'S','e','I','n','c','r','e','a','s','e','Q','u','o','t','a','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_MACHINE_ACCOUNT_NAME_W[] =
 { 'S','e','M','a','c','h','i','n','e','A','c','c','o','u','n','t','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_TCB_NAME_W[] =
 { 'S','e','T','c','b','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SECURITY_NAME_W[] =
 { 'S','e','S','e','c','u','r','i','t','y','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_TAKE_OWNERSHIP_NAME_W[] =
 { 'S','e','T','a','k','e','O','w','n','e','r','s','h','i','p','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_LOAD_DRIVER_NAME_W[] =
 { 'S','e','L','o','a','d','D','r','i','v','e','r','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SYSTEM_PROFILE_NAME_W[] =
 { 'S','e','S','y','s','t','e','m','P','r','o','f','i','l','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SYSTEMTIME_NAME_W[] =
 { 'S','e','S','y','s','t','e','m','t','i','m','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_PROF_SINGLE_PROCESS_NAME_W[] =
 { 'S','e','P','r','o','f','i','l','e','S','i','n','g','l','e','P','r','o','c','e','s','s','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_INC_BASE_PRIORITY_NAME_W[] =
 { 'S','e','I','n','c','r','e','a','s','e','B','a','s','e','P','r','i','o','r','i','t','y','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_CREATE_PAGEFILE_NAME_W[] =
 { 'S','e','C','r','e','a','t','e','P','a','g','e','f','i','l','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_CREATE_PERMANENT_NAME_W[] =
 { 'S','e','C','r','e','a','t','e','P','e','r','m','a','n','e','n','t','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_BACKUP_NAME_W[] =
 { 'S','e','B','a','c','k','u','p','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_RESTORE_NAME_W[] =
 { 'S','e','R','e','s','t','o','r','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SHUTDOWN_NAME_W[] =
 { 'S','e','S','h','u','t','d','o','w','n','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_DEBUG_NAME_W[] =
 { 'S','e','D','e','b','u','g','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_AUDIT_NAME_W[] =
 { 'S','e','A','u','d','i','t','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SYSTEM_ENVIRONMENT_NAME_W[] =
 { 'S','e','S','y','s','t','e','m','E','n','v','i','r','o','n','m','e','n','t','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_CHANGE_NOTIFY_NAME_W[] =
 { 'S','e','C','h','a','n','g','e','N','o','t','i','f','y','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_REMOTE_SHUTDOWN_NAME_W[] =
 { 'S','e','R','e','m','o','t','e','S','h','u','t','d','o','w','n','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_UNDOCK_NAME_W[] =
 { 'S','e','U','n','d','o','c','k','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_SYNC_AGENT_NAME_W[] =
 { 'S','e','S','y','n','c','A','g','e','n','t','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_ENABLE_DELEGATION_NAME_W[] =
 { 'S','e','E','n','a','b','l','e','D','e','l','e','g','a','t','i','o','n','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_MANAGE_VOLUME_NAME_W[] =
 { 'S','e','M','a','n','a','g','e','V','o','l','u','m','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_IMPERSONATE_NAME_W[] =
 { 'S','e','I','m','p','e','r','s','o','n','a','t','e','P','r','i','v','i','l','e','g','e',0 };
static const WCHAR SE_CREATE_GLOBAL_NAME_W[] =
 { 'S','e','C','r','e','a','t','e','G','l','o','b','a','l','P','r','i','v','i','l','e','g','e',0 };

static const WCHAR * const WellKnownPrivNames[SE_MAX_WELL_KNOWN_PRIVILEGE + 1] =
{
    NULL,
    NULL,
    SE_CREATE_TOKEN_NAME_W,
    SE_ASSIGNPRIMARYTOKEN_NAME_W,
    SE_LOCK_MEMORY_NAME_W,
    SE_INCREASE_QUOTA_NAME_W,
    SE_MACHINE_ACCOUNT_NAME_W,
    SE_TCB_NAME_W,
    SE_SECURITY_NAME_W,
    SE_TAKE_OWNERSHIP_NAME_W,
    SE_LOAD_DRIVER_NAME_W,
    SE_SYSTEM_PROFILE_NAME_W,
    SE_SYSTEMTIME_NAME_W,
    SE_PROF_SINGLE_PROCESS_NAME_W,
    SE_INC_BASE_PRIORITY_NAME_W,
    SE_CREATE_PAGEFILE_NAME_W,
    SE_CREATE_PERMANENT_NAME_W,
    SE_BACKUP_NAME_W,
    SE_RESTORE_NAME_W,
    SE_SHUTDOWN_NAME_W,
    SE_DEBUG_NAME_W,
    SE_AUDIT_NAME_W,
    SE_SYSTEM_ENVIRONMENT_NAME_W,
    SE_CHANGE_NOTIFY_NAME_W,
    SE_REMOTE_SHUTDOWN_NAME_W,
    SE_UNDOCK_NAME_W,
    SE_SYNC_AGENT_NAME_W,
    SE_ENABLE_DELEGATION_NAME_W,
    SE_MANAGE_VOLUME_NAME_W,
    SE_IMPERSONATE_NAME_W,
    SE_CREATE_GLOBAL_NAME_W,
};

/******************************************************************************
 * LookupPrivilegeValueW			[ADVAPI32.@]
 *
 * See LookupPrivilegeValueA.
 */
BOOL WINAPI
LookupPrivilegeValueW( LPCWSTR lpSystemName, LPCWSTR lpName, PLUID lpLuid )
{
    UINT i;

    TRACE("%s,%s,%p\n",debugstr_w(lpSystemName), debugstr_w(lpName), lpLuid);

    if (!ADVAPI_IsLocalComputer(lpSystemName))
    {
        SetLastError(RPC_S_SERVER_UNAVAILABLE);
        return FALSE;
    }
    if (!lpName)
    {
        SetLastError(ERROR_NO_SUCH_PRIVILEGE);
        return FALSE;
    }
    for( i=SE_MIN_WELL_KNOWN_PRIVILEGE; i<SE_MAX_WELL_KNOWN_PRIVILEGE; i++ )
    {
        if( !WellKnownPrivNames[i] )
            continue;
        if( strcmpiW( WellKnownPrivNames[i], lpName) )
            continue;
        lpLuid->LowPart = i;
        lpLuid->HighPart = 0;
        TRACE( "%s -> %08lx-%08lx\n",debugstr_w( lpSystemName ),
               lpLuid->HighPart, lpLuid->LowPart );
        return TRUE;
    }
    SetLastError(ERROR_NO_SUCH_PRIVILEGE);
    return FALSE;
}

/******************************************************************************
 * LookupPrivilegeValueA			[ADVAPI32.@]
 *
 * Retrieves LUID used on a system to represent the privilege name.
 *
 * PARAMS
 *  lpSystemName [I] Name of the system
 *  lpName       [I] Name of the privilege
 *  lpLuid       [O] Destination for the resulting LUID
 *
 * RETURNS
 *  Success: TRUE. lpLuid contains the requested LUID.
 *  Failure: FALSE.
 */
BOOL WINAPI
LookupPrivilegeValueA( LPCSTR lpSystemName, LPCSTR lpName, PLUID lpLuid )
{
    UNICODE_STRING lpSystemNameW;
    UNICODE_STRING lpNameW;
    BOOL ret;

    RtlCreateUnicodeStringFromAsciiz(&lpSystemNameW, lpSystemName);
    RtlCreateUnicodeStringFromAsciiz(&lpNameW,lpName);
    ret = LookupPrivilegeValueW(lpSystemNameW.Buffer, lpNameW.Buffer, lpLuid);
    RtlFreeUnicodeString(&lpNameW);
    RtlFreeUnicodeString(&lpSystemNameW);
    return ret;
}

BOOL WINAPI LookupPrivilegeDisplayNameA( LPCSTR lpSystemName, LPCSTR lpName, LPSTR lpDisplayName,
                                         LPDWORD cchDisplayName, LPDWORD lpLanguageId )
{
    FIXME("%s %s %s %p %p - stub\n", debugstr_a(lpSystemName), debugstr_a(lpName),
          debugstr_a(lpDisplayName), cchDisplayName, lpLanguageId);

    return FALSE;
}

BOOL WINAPI LookupPrivilegeDisplayNameW( LPCWSTR lpSystemName, LPCWSTR lpName, LPWSTR lpDisplayName,
                                         LPDWORD cchDisplayName, LPDWORD lpLanguageId )
{
    FIXME("%s %s %s %p %p - stub\n", debugstr_w(lpSystemName), debugstr_w(lpName),
          debugstr_w(lpDisplayName), cchDisplayName, lpLanguageId);

    return FALSE;
}

/******************************************************************************
 * LookupPrivilegeNameA			[ADVAPI32.@]
 *
 * See LookupPrivilegeNameW.
 */
BOOL WINAPI
LookupPrivilegeNameA( LPCSTR lpSystemName, PLUID lpLuid, LPSTR lpName,
 LPDWORD cchName)
{
    UNICODE_STRING lpSystemNameW;
    BOOL ret;
    DWORD wLen = 0;

    TRACE("%s %p %p %p\n", debugstr_a(lpSystemName), lpLuid, lpName, cchName);

    RtlCreateUnicodeStringFromAsciiz(&lpSystemNameW, lpSystemName);
    ret = LookupPrivilegeNameW(lpSystemNameW.Buffer, lpLuid, NULL, &wLen);
    if (!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
    {
        LPWSTR lpNameW = HeapAlloc(GetProcessHeap(), 0, wLen * sizeof(WCHAR));

        ret = LookupPrivilegeNameW(lpSystemNameW.Buffer, lpLuid, lpNameW,
         &wLen);
        if (ret)
        {
            /* Windows crashes if cchName is NULL, so will I */
            int len = WideCharToMultiByte(CP_ACP, 0, lpNameW, -1, lpName,
             *cchName, NULL, NULL);

            if (len == 0)
            {
                /* WideCharToMultiByte failed */
                ret = FALSE;
            }
            else if (len > *cchName)
            {
                *cchName = len;
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                ret = FALSE;
            }
            else
            {
                /* WideCharToMultiByte succeeded, output length needs to be
                 * length not including NULL terminator
                 */
                *cchName = len - 1;
            }
        }
        HeapFree(GetProcessHeap(), 0, lpNameW);
    }
    RtlFreeUnicodeString(&lpSystemNameW);
    return ret;
}

/******************************************************************************
 * LookupPrivilegeNameW			[ADVAPI32.@]
 *
 * Retrieves the privilege name referred to by the LUID lpLuid.
 *
 * PARAMS
 *  lpSystemName [I]   Name of the system
 *  lpLuid       [I]   Privilege value
 *  lpName       [O]   Name of the privilege
 *  cchName      [I/O] Number of characters in lpName.
 *
 * RETURNS
 *  Success: TRUE. lpName contains the name of the privilege whose value is
 *  *lpLuid.
 *  Failure: FALSE.
 *
 * REMARKS
 *  Only well-known privilege names (those defined in winnt.h) can be retrieved
 *  using this function.
 *  If the length of lpName is too small, on return *cchName will contain the
 *  number of WCHARs needed to contain the privilege, including the NULL
 *  terminator, and GetLastError will return ERROR_INSUFFICIENT_BUFFER.
 *  On success, *cchName will contain the number of characters stored in
 *  lpName, NOT including the NULL terminator.
 */
BOOL WINAPI
LookupPrivilegeNameW( LPCWSTR lpSystemName, PLUID lpLuid, LPWSTR lpName,
 LPDWORD cchName)
{
    size_t privNameLen;

    TRACE("%s,%p,%p,%p\n",debugstr_w(lpSystemName), lpLuid, lpName, cchName);

    if (!ADVAPI_IsLocalComputer(lpSystemName))
    {
        SetLastError(RPC_S_SERVER_UNAVAILABLE);
        return FALSE;
    }
    if (lpLuid->HighPart || (lpLuid->LowPart < SE_MIN_WELL_KNOWN_PRIVILEGE ||
     lpLuid->LowPart > SE_MAX_WELL_KNOWN_PRIVILEGE))
    {
        SetLastError(ERROR_NO_SUCH_PRIVILEGE);
        return FALSE;
    }
    privNameLen = strlenW(WellKnownPrivNames[lpLuid->LowPart]);
    /* Windows crashes if cchName is NULL, so will I */
    if (*cchName <= privNameLen)
    {
        *cchName = privNameLen + 1;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
        return FALSE;
    }
    else
    {
        strcpyW(lpName, WellKnownPrivNames[lpLuid->LowPart]);
        *cchName = privNameLen;
        return TRUE;
    }
}

/******************************************************************************
 * GetFileSecurityA [ADVAPI32.@]
 *
 * Obtains Specified information about the security of a file or directory.
 *
 * PARAMS
 *  lpFileName           [I] Name of the file to get info for
 *  RequestedInformation [I] SE_ flags from "winnt.h"
 *  pSecurityDescriptor  [O] Destination for security information
 *  nLength              [I] Length of pSecurityDescriptor
 *  lpnLengthNeeded      [O] Destination for length of returned security information
 *
 * RETURNS
 *  Success: TRUE. pSecurityDescriptor contains the requested information.
 *  Failure: FALSE. lpnLengthNeeded contains the required space to return the info. 
 *
 * NOTES
 *  The information returned is constrained by the callers access rights and
 *  privileges.
 */
BOOL WINAPI
GetFileSecurityA( LPCSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor,
                    DWORD nLength, LPDWORD lpnLengthNeeded )
{
    DWORD len;
    BOOL r;
    LPWSTR name = NULL;

    if( lpFileName )
    {
        len = MultiByteToWideChar( CP_ACP, 0, lpFileName, -1, NULL, 0 );
        name = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, lpFileName, -1, name, len );
    }

    r = GetFileSecurityW( name, RequestedInformation, pSecurityDescriptor,
                          nLength, lpnLengthNeeded );
    HeapFree( GetProcessHeap(), 0, name );

    return r;
}

/******************************************************************************
 * GetFileSecurityW [ADVAPI32.@]
 *
 * See GetFileSecurityA.
 */
BOOL WINAPI
GetFileSecurityW( LPCWSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor,
                    DWORD nLength, LPDWORD lpnLengthNeeded )
{
    DWORD		nNeeded;
    LPBYTE	pBuffer;
    DWORD		iLocNow;
    SECURITY_DESCRIPTOR_RELATIVE *pSDRelative;

    if(INVALID_FILE_ATTRIBUTES == GetFileAttributesW(lpFileName))
        return FALSE;

    FIXME("(%s) : returns fake SECURITY_DESCRIPTOR\n", debugstr_w(lpFileName) );

    nNeeded = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
    if (RequestedInformation & OWNER_SECURITY_INFORMATION)
	nNeeded += sizeof(sidWorld);
    if (RequestedInformation & GROUP_SECURITY_INFORMATION)
	nNeeded += sizeof(sidWorld);
    if (RequestedInformation & DACL_SECURITY_INFORMATION)
	nNeeded += WINE_SIZE_OF_WORLD_ACCESS_ACL;
    if (RequestedInformation & SACL_SECURITY_INFORMATION)
	nNeeded += WINE_SIZE_OF_WORLD_ACCESS_ACL;

    *lpnLengthNeeded = nNeeded;

    if (nNeeded > nLength)
	    return TRUE;

    if (!InitializeSecurityDescriptor(pSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
	return FALSE;

    pSDRelative = (PISECURITY_DESCRIPTOR_RELATIVE) pSecurityDescriptor;
    pSDRelative->Control |= SE_SELF_RELATIVE;
    pBuffer = (LPBYTE) pSDRelative;
    iLocNow = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

    if (RequestedInformation & OWNER_SECURITY_INFORMATION)
    {
	memcpy(pBuffer + iLocNow, &sidWorld, sizeof(sidWorld));
	pSDRelative->Owner = iLocNow;
	iLocNow += sizeof(sidWorld);
    }
    if (RequestedInformation & GROUP_SECURITY_INFORMATION)
    {
	memcpy(pBuffer + iLocNow, &sidWorld, sizeof(sidWorld));
	pSDRelative->Group = iLocNow;
	iLocNow += sizeof(sidWorld);
    }
    if (RequestedInformation & DACL_SECURITY_INFORMATION)
    {
	GetWorldAccessACL((PACL) (pBuffer + iLocNow));
	pSDRelative->Dacl = iLocNow;
	iLocNow += WINE_SIZE_OF_WORLD_ACCESS_ACL;
    }
    if (RequestedInformation & SACL_SECURITY_INFORMATION)
    {
	GetWorldAccessACL((PACL) (pBuffer + iLocNow));
	pSDRelative->Sacl = iLocNow;
	/* iLocNow += WINE_SIZE_OF_WORLD_ACCESS_ACL; */
    }
    return TRUE;
}


/******************************************************************************
 * LookupAccountSidA [ADVAPI32.@]
 */
BOOL WINAPI
LookupAccountSidA(
	IN LPCSTR system,
	IN PSID sid,
	OUT LPSTR account,
	IN OUT LPDWORD accountSize,
	OUT LPSTR domain,
	IN OUT LPDWORD domainSize,
	OUT PSID_NAME_USE name_use )
{
	static const char ac[] = "Administrator";
	static const char dm[] = "DOMAIN";
	FIXME("(%s,sid=%p,%p,%p(%lu),%p,%p(%lu),%p): semi-stub\n",
	      debugstr_a(system),sid,
	      account,accountSize,accountSize?*accountSize:0,
	      domain,domainSize,domainSize?*domainSize:0,
	      name_use);

	if (accountSize) *accountSize = strlen(ac)+1;
	if (account && (*accountSize > strlen(ac)))
	  strcpy(account, ac);

	if (domainSize) *domainSize = strlen(dm)+1;
	if (domain && (*domainSize > strlen(dm)))
	  strcpy(domain,dm);

	if (name_use) *name_use = SidTypeUser;
	return TRUE;
}

/******************************************************************************
 * LookupAccountSidW [ADVAPI32.@]
 *
 * PARAMS
 *   system      []
 *   sid         []
 *   account     []
 *   accountSize []
 *   domain      []
 *   domainSize  []
 *   name_use    []
 */
BOOL WINAPI
LookupAccountSidW(
	IN LPCWSTR system,
	IN PSID sid,
	OUT LPWSTR account,
	IN OUT LPDWORD accountSize,
	OUT LPWSTR domain,
	IN OUT LPDWORD domainSize,
	OUT PSID_NAME_USE name_use )
{
    static const WCHAR ac[] = {'A','d','m','i','n','i','s','t','r','a','t','o','r',0};
    static const WCHAR dm[] = {'D','O','M','A','I','N',0};
	FIXME("(%s,sid=%p,%p,%p(%lu),%p,%p(%lu),%p): semi-stub\n",
	      debugstr_w(system),sid,
	      account,accountSize,accountSize?*accountSize:0,
	      domain,domainSize,domainSize?*domainSize:0,
	      name_use);

	if (accountSize) *accountSize = strlenW(ac)+1;
	if (account && (*accountSize > strlenW(ac)))
            strcpyW(account, ac);

	if (domainSize) *domainSize = strlenW(dm)+1;
	if (domain && (*domainSize > strlenW(dm)))
            strcpyW(domain,dm);

	if (name_use) *name_use = SidTypeUser;
	return TRUE;
}

/******************************************************************************
 * SetFileSecurityA [ADVAPI32.@]
 *
 * See SetFileSecurityW.
 */
BOOL WINAPI SetFileSecurityA( LPCSTR lpFileName,
                                SECURITY_INFORMATION RequestedInformation,
                                PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
    DWORD len;
    BOOL r;
    LPWSTR name = NULL;

    if( lpFileName )
    {
        len = MultiByteToWideChar( CP_ACP, 0, lpFileName, -1, NULL, 0 );
        name = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR) );
        MultiByteToWideChar( CP_ACP, 0, lpFileName, -1, name, len );
    }

    r = SetFileSecurityW( name, RequestedInformation, pSecurityDescriptor );
    HeapFree( GetProcessHeap(), 0, name );

    return r;
}

/******************************************************************************
 * SetFileSecurityW [ADVAPI32.@]
 *
 * Sets the security of a file or directory.
 *
 * PARAMS
 *   lpFileName           []
 *   RequestedInformation []
 *   pSecurityDescriptor  []
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE.
 */
BOOL WINAPI
SetFileSecurityW( LPCWSTR lpFileName,
                    SECURITY_INFORMATION RequestedInformation,
                    PSECURITY_DESCRIPTOR pSecurityDescriptor )
{
  FIXME("(%s) : stub\n", debugstr_w(lpFileName) );
  return TRUE;
}

/******************************************************************************
 * QueryWindows31FilesMigration [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 */
BOOL WINAPI
QueryWindows31FilesMigration( DWORD x1 )
{
	FIXME("(%ld):stub\n",x1);
	return TRUE;
}

/******************************************************************************
 * SynchronizeWindows31FilesAndWindowsNTRegistry [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 *   x2 []
 *   x3 []
 *   x4 []
 */
BOOL WINAPI
SynchronizeWindows31FilesAndWindowsNTRegistry( DWORD x1, DWORD x2, DWORD x3,
                                               DWORD x4 )
{
	FIXME("(0x%08lx,0x%08lx,0x%08lx,0x%08lx):stub\n",x1,x2,x3,x4);
	return TRUE;
}

/******************************************************************************
 * NotifyBootConfigStatus [ADVAPI32.@]
 *
 * PARAMS
 *   x1 []
 */
BOOL WINAPI
NotifyBootConfigStatus( DWORD x1 )
{
	FIXME("(0x%08lx):stub\n",x1);
	return 1;
}

/******************************************************************************
 * RevertToSelf [ADVAPI32.@]
 *
 * Ends the impersonation of a user.
 *
 * PARAMS
 *   void []
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE.
 */
BOOL WINAPI
RevertToSelf( void )
{
    HANDLE Token = NULL;
    return set_ntstatus( NtSetInformationThread( GetCurrentThread(),
        ThreadImpersonationToken, &Token, sizeof(Token) ) );
}

/******************************************************************************
 * ImpersonateSelf [ADVAPI32.@]
 *
 * Makes an impersonation token that represents the process user and assigns
 * to the current thread.
 *
 * PARAMS
 *  ImpersonationLevel [I] Level at which to impersonate.
 *
 * RETURNS
 *  Success: TRUE.
 *  Failure: FALSE.
 */
BOOL WINAPI
ImpersonateSelf(SECURITY_IMPERSONATION_LEVEL ImpersonationLevel)
{
    return set_ntstatus( RtlImpersonateSelf( ImpersonationLevel ) );
}

/******************************************************************************
 * ImpersonateLoggedOnUser [ADVAPI32.@]
 */
BOOL WINAPI ImpersonateLoggedOnUser(HANDLE hToken)
{
    FIXME("(%p):stub returning FALSE\n", hToken);
    return FALSE;
}

/******************************************************************************
 * AccessCheck [ADVAPI32.@]
 */
BOOL WINAPI
AccessCheck(
	PSECURITY_DESCRIPTOR SecurityDescriptor,
	HANDLE ClientToken,
	DWORD DesiredAccess,
	PGENERIC_MAPPING GenericMapping,
	PPRIVILEGE_SET PrivilegeSet,
	LPDWORD PrivilegeSetLength,
	LPDWORD GrantedAccess,
	LPBOOL AccessStatus)
{
    NTSTATUS access_status;
    BOOL ret = set_ntstatus( NtAccessCheck(SecurityDescriptor, ClientToken, DesiredAccess,
                                           GenericMapping, PrivilegeSet, PrivilegeSetLength,
                                           GrantedAccess, &access_status) );
    if (ret) *AccessStatus = set_ntstatus( access_status );
    return ret;
}


/******************************************************************************
 * AccessCheckByType [ADVAPI32.@]
 */
BOOL WINAPI AccessCheckByType(
    PSECURITY_DESCRIPTOR pSecurityDescriptor, 
    PSID PrincipalSelfSid,
    HANDLE ClientToken, 
    DWORD DesiredAccess, 
    POBJECT_TYPE_LIST ObjectTypeList,
    DWORD ObjectTypeListLength,
    PGENERIC_MAPPING GenericMapping,
    PPRIVILEGE_SET PrivilegeSet,
    LPDWORD PrivilegeSetLength, 
    LPDWORD GrantedAccess,
    LPBOOL AccessStatus)
{
	FIXME("stub\n");

	*AccessStatus = TRUE;

	return !*AccessStatus;
}

/******************************************************************************
 * MapGenericMask [ADVAPI32.@]
 *
 * Maps generic access rights into specific access rights according to the
 * supplied mapping.
 *
 * PARAMS
 *  AccessMask     [I/O] Access rights.
 *  GenericMapping [I] The mapping between generic and specific rights.
 *
 * RETURNS
 *  Nothing.
 */
VOID WINAPI MapGenericMask( PDWORD AccessMask, PGENERIC_MAPPING GenericMapping )
{
    RtlMapGenericMask( AccessMask, GenericMapping );
}

/*************************************************************************
 * SetKernelObjectSecurity [ADVAPI32.@]
 */
BOOL WINAPI SetKernelObjectSecurity (
	IN HANDLE Handle,
	IN SECURITY_INFORMATION SecurityInformation,
	IN PSECURITY_DESCRIPTOR SecurityDescriptor )
{
    return set_ntstatus (NtSetSecurityObject (Handle, SecurityInformation, SecurityDescriptor));
}


/******************************************************************************
 *  AddAuditAccessAce [ADVAPI32.@]
 */
BOOL WINAPI AddAuditAccessAce(
    IN OUT PACL pAcl, 
    IN DWORD dwAceRevision, 
    IN DWORD dwAccessMask, 
    IN PSID pSid, 
    IN BOOL bAuditSuccess, 
    IN BOOL bAuditFailure) 
{
    return set_ntstatus( RtlAddAuditAccessAce(pAcl, dwAceRevision, dwAccessMask, pSid, 
                                              bAuditSuccess, bAuditFailure) ); 
}

/******************************************************************************
 * LookupAccountNameA [ADVAPI32.@]
 */
BOOL WINAPI
LookupAccountNameA(
	IN LPCSTR system,
	IN LPCSTR account,
	OUT PSID sid,
	OUT LPDWORD cbSid,
	LPSTR ReferencedDomainName,
	IN OUT LPDWORD cbReferencedDomainName,
	OUT PSID_NAME_USE name_use )
{
    /* Default implementation: Always return a default SID */
    SID_IDENTIFIER_AUTHORITY identifierAuthority = {SECURITY_NT_AUTHORITY};
    BOOL ret;
    PSID pSid;
    static const char dm[] = "DOMAIN";

    FIXME("(%s,%s,%p,%p,%p,%p,%p), stub.\n",system,account,sid,cbSid,ReferencedDomainName,cbReferencedDomainName,name_use);

    ret = AllocateAndInitializeSid(&identifierAuthority,
        2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0,
        &pSid);

    if (!ret)
       return FALSE;
    if(!RtlValidSid(pSid))
    {
       FreeSid(pSid);
       return FALSE;
    }

    if (sid != NULL && (*cbSid >= GetLengthSid(pSid)))
       CopySid(*cbSid, sid, pSid);
    if (*cbSid < GetLengthSid(pSid))
    {
       SetLastError(ERROR_INSUFFICIENT_BUFFER);
       ret = FALSE;
    }
    *cbSid = GetLengthSid(pSid);
    
    if (ReferencedDomainName != NULL && (*cbReferencedDomainName > strlen(dm)))
      strcpy(ReferencedDomainName, dm);
    if (*cbReferencedDomainName <= strlen(dm))
    {
       SetLastError(ERROR_INSUFFICIENT_BUFFER);
       ret = FALSE;
    }
    *cbReferencedDomainName = strlen(dm)+1;

    FreeSid(pSid);

    return ret;
}

/******************************************************************************
 * LookupAccountNameW [ADVAPI32.@]
 */
BOOL WINAPI LookupAccountNameW( LPCWSTR lpSystemName, LPCWSTR lpAccountName, PSID Sid,
                                LPDWORD cbSid, LPWSTR ReferencedDomainName,
                                LPDWORD cchReferencedDomainName, PSID_NAME_USE peUse )
{
    FIXME("%s %s %p %p %p %p %p - stub\n", debugstr_w(lpSystemName), debugstr_w(lpAccountName),
          Sid, cbSid, ReferencedDomainName, cchReferencedDomainName, peUse);

    return FALSE;
}

/******************************************************************************
 * PrivilegeCheck [ADVAPI32.@]
 */
BOOL WINAPI PrivilegeCheck( HANDLE ClientToken, PPRIVILEGE_SET RequiredPrivileges, LPBOOL pfResult)
{
    BOOL ret;
    BOOLEAN Result;

    TRACE("%p %p %p\n", ClientToken, RequiredPrivileges, pfResult);

    ret = set_ntstatus (NtPrivilegeCheck (ClientToken, RequiredPrivileges, &Result));
    if (ret)
        *pfResult = Result;
    return ret;
}

/******************************************************************************
 * AccessCheckAndAuditAlarmA [ADVAPI32.@]
 */
BOOL WINAPI AccessCheckAndAuditAlarmA(LPCSTR Subsystem, LPVOID HandleId, LPSTR ObjectTypeName,
  LPSTR ObjectName, PSECURITY_DESCRIPTOR SecurityDescriptor, DWORD DesiredAccess,
  PGENERIC_MAPPING GenericMapping, BOOL ObjectCreation, LPDWORD GrantedAccess,
  LPBOOL AccessStatus, LPBOOL pfGenerateOnClose)
{
	FIXME("stub (%s,%p,%s,%s,%p,%08lx,%p,%x,%p,%p,%p)\n", debugstr_a(Subsystem),
		HandleId, debugstr_a(ObjectTypeName), debugstr_a(ObjectName),
		SecurityDescriptor, DesiredAccess, GenericMapping,
		ObjectCreation, GrantedAccess, AccessStatus, pfGenerateOnClose);
	return TRUE;
}

/******************************************************************************
 * AccessCheckAndAuditAlarmW [ADVAPI32.@]
 */
BOOL WINAPI AccessCheckAndAuditAlarmW(LPCWSTR Subsystem, LPVOID HandleId, LPWSTR ObjectTypeName,
  LPWSTR ObjectName, PSECURITY_DESCRIPTOR SecurityDescriptor, DWORD DesiredAccess,
  PGENERIC_MAPPING GenericMapping, BOOL ObjectCreation, LPDWORD GrantedAccess,
  LPBOOL AccessStatus, LPBOOL pfGenerateOnClose)
{
	FIXME("stub (%s,%p,%s,%s,%p,%08lx,%p,%x,%p,%p,%p)\n", debugstr_w(Subsystem),
		HandleId, debugstr_w(ObjectTypeName), debugstr_w(ObjectName),
		SecurityDescriptor, DesiredAccess, GenericMapping,
		ObjectCreation, GrantedAccess, AccessStatus, pfGenerateOnClose);
	return TRUE;
}

BOOL WINAPI ObjectCloseAuditAlarmA(LPCSTR SubsystemName, LPVOID HandleId, BOOL GenerateOnClose)
{
    FIXME("stub (%s,%p,%x)\n", debugstr_a(SubsystemName), HandleId, GenerateOnClose);

    return TRUE;
}

BOOL WINAPI ObjectCloseAuditAlarmW(LPCWSTR SubsystemName, LPVOID HandleId, BOOL GenerateOnClose)
{
    FIXME("stub (%s,%p,%x)\n", debugstr_w(SubsystemName), HandleId, GenerateOnClose);

    return TRUE;
}

BOOL WINAPI ObjectOpenAuditAlarmA(LPCSTR SubsystemName, LPVOID HandleId, LPSTR ObjectTypeName,
  LPSTR ObjectName, PSECURITY_DESCRIPTOR pSecurityDescriptor, HANDLE ClientToken, DWORD DesiredAccess,
  DWORD GrantedAccess, PPRIVILEGE_SET Privileges, BOOL ObjectCreation, BOOL AccessGranted,
  LPBOOL GenerateOnClose)
{
	FIXME("stub (%s,%p,%s,%s,%p,%p,0x%08lx,0x%08lx,%p,%x,%x,%p)\n", debugstr_a(SubsystemName),
		HandleId, debugstr_a(ObjectTypeName), debugstr_a(ObjectName), pSecurityDescriptor,
        ClientToken, DesiredAccess, GrantedAccess, Privileges, ObjectCreation, AccessGranted,
        GenerateOnClose);

    return TRUE;
}

BOOL WINAPI ObjectOpenAuditAlarmW(LPCWSTR SubsystemName, LPVOID HandleId, LPWSTR ObjectTypeName,
  LPWSTR ObjectName, PSECURITY_DESCRIPTOR pSecurityDescriptor, HANDLE ClientToken, DWORD DesiredAccess,
  DWORD GrantedAccess, PPRIVILEGE_SET Privileges, BOOL ObjectCreation, BOOL AccessGranted,
  LPBOOL GenerateOnClose)
{
    FIXME("stub (%s,%p,%s,%s,%p,%p,0x%08lx,0x%08lx,%p,%x,%x,%p)\n", debugstr_w(SubsystemName),
        HandleId, debugstr_w(ObjectTypeName), debugstr_w(ObjectName), pSecurityDescriptor,
        ClientToken, DesiredAccess, GrantedAccess, Privileges, ObjectCreation, AccessGranted,
        GenerateOnClose);

    return TRUE;
}

BOOL WINAPI ObjectPrivilegeAuditAlarmA( LPCSTR SubsystemName, LPVOID HandleId, HANDLE ClientToken,
  DWORD DesiredAccess, PPRIVILEGE_SET Privileges, BOOL AccessGranted)
{
    FIXME("stub (%s,%p,%p,0x%08lx,%p,%x)\n", debugstr_a(SubsystemName), HandleId, ClientToken,
          DesiredAccess, Privileges, AccessGranted);

    return TRUE;
}

BOOL WINAPI ObjectPrivilegeAuditAlarmW( LPCWSTR SubsystemName, LPVOID HandleId, HANDLE ClientToken,
  DWORD DesiredAccess, PPRIVILEGE_SET Privileges, BOOL AccessGranted)
{
    FIXME("stub (%s,%p,%p,0x%08lx,%p,%x)\n", debugstr_w(SubsystemName), HandleId, ClientToken,
          DesiredAccess, Privileges, AccessGranted);

    return TRUE;
}

BOOL WINAPI PrivilegedServiceAuditAlarmA( LPCSTR SubsystemName, LPCSTR ServiceName, HANDLE ClientToken,
                                   PPRIVILEGE_SET Privileges, BOOL AccessGranted)
{
    FIXME("stub (%s,%s,%p,%p,%x)\n", debugstr_a(SubsystemName), debugstr_a(ServiceName),
          ClientToken, Privileges, AccessGranted);

    return TRUE;
}

BOOL WINAPI PrivilegedServiceAuditAlarmW( LPCWSTR SubsystemName, LPCWSTR ServiceName, HANDLE ClientToken,
                                   PPRIVILEGE_SET Privileges, BOOL AccessGranted)
{
    FIXME("stub %s,%s,%p,%p,%x)\n", debugstr_w(SubsystemName), debugstr_w(ServiceName),
          ClientToken, Privileges, AccessGranted);

    return TRUE;
}

/******************************************************************************
 * GetSecurityInfo [ADVAPI32.@]
 */
DWORD WINAPI GetSecurityInfo(
    HANDLE hObject, SE_OBJECT_TYPE ObjectType,
    SECURITY_INFORMATION SecurityInfo, PSID *ppsidOwner,
    PSID *ppsidGroup, PACL *ppDacl, PACL *ppSacl,
    PSECURITY_DESCRIPTOR *ppSecurityDescriptor
)
{
  FIXME("stub!\n");
  return ERROR_BAD_PROVIDER;
}

/******************************************************************************
 * GetSecurityInfoExW [ADVAPI32.@]
 */
DWORD WINAPI GetSecurityInfoExW(
	HANDLE hObject, SE_OBJECT_TYPE ObjectType, 
	SECURITY_INFORMATION SecurityInfo, LPCWSTR lpProvider,
	LPCWSTR lpProperty, PACTRL_ACCESSW *ppAccessList, 
	PACTRL_AUDITW *ppAuditList, LPWSTR *lppOwner, LPWSTR *lppGroup
)
{
  FIXME("stub!\n");
  return ERROR_BAD_PROVIDER; 
}

/******************************************************************************
 * BuildExplicitAccessWithNameA [ADVAPI32.@]
 */
VOID WINAPI BuildExplicitAccessWithNameA( PEXPLICIT_ACCESSA pExplicitAccess,
                                          LPSTR pTrusteeName, DWORD AccessPermissions,
                                          ACCESS_MODE AccessMode, DWORD Inheritance )
{
    TRACE("%p %s 0x%08lx 0x%08x 0x%08lx\n", pExplicitAccess, debugstr_a(pTrusteeName),
          AccessPermissions, AccessMode, Inheritance);

    pExplicitAccess->grfAccessPermissions = AccessPermissions;
    pExplicitAccess->grfAccessMode = AccessMode;
    pExplicitAccess->grfInheritance = Inheritance;

    pExplicitAccess->Trustee.pMultipleTrustee = NULL;
    pExplicitAccess->Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pExplicitAccess->Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    pExplicitAccess->Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    pExplicitAccess->Trustee.ptstrName = pTrusteeName;
}

/******************************************************************************
 * BuildExplicitAccessWithNameW [ADVAPI32.@]
 */
VOID WINAPI BuildExplicitAccessWithNameW( PEXPLICIT_ACCESSW pExplicitAccess,
                                          LPWSTR pTrusteeName, DWORD AccessPermissions,
                                          ACCESS_MODE AccessMode, DWORD Inheritance )
{
    TRACE("%p %s 0x%08lx 0x%08x 0x%08lx\n", pExplicitAccess, debugstr_w(pTrusteeName),
          AccessPermissions, AccessMode, Inheritance);

    pExplicitAccess->grfAccessPermissions = AccessPermissions;
    pExplicitAccess->grfAccessMode = AccessMode;
    pExplicitAccess->grfInheritance = Inheritance;

    pExplicitAccess->Trustee.pMultipleTrustee = NULL;
    pExplicitAccess->Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pExplicitAccess->Trustee.TrusteeForm = TRUSTEE_IS_NAME;
    pExplicitAccess->Trustee.TrusteeType = TRUSTEE_IS_UNKNOWN;
    pExplicitAccess->Trustee.ptstrName = pTrusteeName;
}

/******************************************************************************
 * BuildTrusteeWithObjectsAndNameA [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithObjectsAndNameA( PTRUSTEEA pTrustee, POBJECTS_AND_NAME_A pObjName,
                                             SE_OBJECT_TYPE ObjectType, LPSTR ObjectTypeName,
                                             LPSTR InheritedObjectTypeName, LPSTR Name )
{
    TRACE("%p %p 0x%08x %p %p %s\n", pTrustee, pObjName,
          ObjectType, ObjectTypeName, InheritedObjectTypeName, debugstr_a(Name));

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_OBJECTS_AND_NAME;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = Name;
}

/******************************************************************************
 * BuildTrusteeWithObjectsAndNameW [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithObjectsAndNameW( PTRUSTEEW pTrustee, POBJECTS_AND_NAME_W pObjName,
                                             SE_OBJECT_TYPE ObjectType, LPWSTR ObjectTypeName,
                                             LPWSTR InheritedObjectTypeName, LPWSTR Name )
{
    TRACE("%p %p 0x%08x %p %p %s\n", pTrustee, pObjName,
          ObjectType, ObjectTypeName, InheritedObjectTypeName, debugstr_w(Name));

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_OBJECTS_AND_NAME;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = Name;
}

VOID WINAPI BuildTrusteeWithObjectsAndSidA( PTRUSTEEA pTrustee, POBJECTS_AND_SID pObjSid,
                                            GUID* pObjectGuid, GUID* pInheritedObjectGuid, PSID pSid )
{
    TRACE("%p %p %p %p %p\n", pTrustee, pObjSid, pObjectGuid, pInheritedObjectGuid, pSid);

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_OBJECTS_AND_SID;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = (LPSTR) pSid;
}

VOID WINAPI BuildTrusteeWithObjectsAndSidW( PTRUSTEEW pTrustee, POBJECTS_AND_SID pObjSid,
                                            GUID* pObjectGuid, GUID* pInheritedObjectGuid, PSID pSid )
{
    TRACE("%p %p %p %p %p\n", pTrustee, pObjSid, pObjectGuid, pInheritedObjectGuid, pSid);

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_OBJECTS_AND_SID;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = (LPWSTR) pSid;
}

/******************************************************************************
 * BuildTrusteeWithSidA [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithSidA(PTRUSTEEA pTrustee, PSID pSid)
{
    TRACE("%p %p\n", pTrustee, pSid);

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_SID;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = (LPSTR) pSid;
}

/******************************************************************************
 * BuildTrusteeWithSidW [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithSidW(PTRUSTEEW pTrustee, PSID pSid)
{
    TRACE("%p %p\n", pTrustee, pSid);

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_SID;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = (LPWSTR) pSid;
}

/******************************************************************************
 * BuildTrusteeWithNameA [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithNameA(PTRUSTEEA pTrustee, LPSTR name)
{
    TRACE("%p %s\n", pTrustee, debugstr_a(name) );

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_NAME;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = name;
}

/******************************************************************************
 * BuildTrusteeWithNameW [ADVAPI32.@]
 */
VOID WINAPI BuildTrusteeWithNameW(PTRUSTEEW pTrustee, LPWSTR name)
{
    TRACE("%p %s\n", pTrustee, debugstr_w(name) );

    pTrustee->pMultipleTrustee = NULL;
    pTrustee->MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
    pTrustee->TrusteeForm = TRUSTEE_IS_NAME;
    pTrustee->TrusteeType = TRUSTEE_IS_UNKNOWN;
    pTrustee->ptstrName = name;
}

/****************************************************************************** 
 * GetTrusteeFormA [ADVAPI32.@] 
 */ 
TRUSTEE_FORM WINAPI GetTrusteeFormA(PTRUSTEEA pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return TRUSTEE_BAD_FORM; 
  
    return pTrustee->TrusteeForm; 
}  
  
/****************************************************************************** 
 * GetTrusteeFormW [ADVAPI32.@] 
 */ 
TRUSTEE_FORM WINAPI GetTrusteeFormW(PTRUSTEEW pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return TRUSTEE_BAD_FORM; 
  
    return pTrustee->TrusteeForm; 
}  
  
/****************************************************************************** 
 * GetTrusteeNameA [ADVAPI32.@] 
 */ 
LPSTR WINAPI GetTrusteeNameA(PTRUSTEEA pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return NULL; 
  
    return pTrustee->ptstrName; 
}  
  
/****************************************************************************** 
 * GetTrusteeNameW [ADVAPI32.@] 
 */ 
LPWSTR WINAPI GetTrusteeNameW(PTRUSTEEW pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return NULL; 
  
    return pTrustee->ptstrName; 
}  
  
/****************************************************************************** 
 * GetTrusteeTypeA [ADVAPI32.@] 
 */ 
TRUSTEE_TYPE WINAPI GetTrusteeTypeA(PTRUSTEEA pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return TRUSTEE_IS_UNKNOWN; 
  
    return pTrustee->TrusteeType; 
}  
  
/****************************************************************************** 
 * GetTrusteeTypeW [ADVAPI32.@] 
 */ 
TRUSTEE_TYPE WINAPI GetTrusteeTypeW(PTRUSTEEW pTrustee) 
{  
    TRACE("(%p)\n", pTrustee); 
  
    if (!pTrustee) 
        return TRUSTEE_IS_UNKNOWN; 
  
    return pTrustee->TrusteeType; 
} 
 
BOOL WINAPI SetAclInformation( PACL pAcl, LPVOID pAclInformation,
                               DWORD nAclInformationLength,
                               ACL_INFORMATION_CLASS dwAclInformationClass )
{
    FIXME("%p %p 0x%08lx 0x%08x - stub\n", pAcl, pAclInformation,
          nAclInformationLength, dwAclInformationClass);

    return TRUE;
}

/******************************************************************************
 * SetEntriesInAclA [ADVAPI32.@]
 */
DWORD WINAPI SetEntriesInAclA( ULONG count, PEXPLICIT_ACCESSA pEntries,
                               PACL OldAcl, PACL* NewAcl )
{
    FIXME("%ld %p %p %p\n",count,pEntries,OldAcl,NewAcl);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************************
 * SetEntriesInAclW [ADVAPI32.@]
 */
DWORD WINAPI SetEntriesInAclW( ULONG count, PEXPLICIT_ACCESSW pEntries,
                               PACL OldAcl, PACL* NewAcl )
{
    FIXME("%ld %p %p %p\n",count,pEntries,OldAcl,NewAcl);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************************
 * SetNamedSecurityInfoA [ADVAPI32.@]
 */
DWORD WINAPI SetNamedSecurityInfoA(LPSTR pObjectName,
        SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
        PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl)
{
    DWORD len;
    LPWSTR wstr = NULL;
    DWORD r;

    TRACE("%s %d %ld %p %p %p %p\n", debugstr_a(pObjectName), ObjectType,
           SecurityInfo, psidOwner, psidGroup, pDacl, pSacl);

    if( pObjectName )
    {
        len = MultiByteToWideChar( CP_ACP, 0, pObjectName, -1, NULL, 0 );
        wstr = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar( CP_ACP, 0, pObjectName, -1, wstr, len );
    }

    r = SetNamedSecurityInfoW( wstr, ObjectType, SecurityInfo, psidOwner,
                           psidGroup, pDacl, pSacl );

    HeapFree( GetProcessHeap(), 0, wstr );

    return r;
}

BOOL WINAPI SetPrivateObjectSecurity( SECURITY_INFORMATION SecurityInformation,
    PSECURITY_DESCRIPTOR ModificationDescriptor,
    PSECURITY_DESCRIPTOR* ObjectsSecurityDescriptor,
    PGENERIC_MAPPING GenericMapping,
    HANDLE Token )
{
    FIXME("0x%08lx %p %p %p %p - stub\n", SecurityInformation, ModificationDescriptor,
          ObjectsSecurityDescriptor, GenericMapping, Token);

    return TRUE;
}

BOOL WINAPI SetSecurityDescriptorControl( PSECURITY_DESCRIPTOR pSecurityDescriptor,
  SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest,
  SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet )
{
    FIXME("%p 0x%08x 0x%08x - stub\n", pSecurityDescriptor, ControlBitsOfInterest,
          ControlBitsToSet);

    return TRUE;
}

BOOL WINAPI AreAllAccessesGranted( DWORD GrantedAccess, DWORD DesiredAccess )
{
    return RtlAreAllAccessesGranted( GrantedAccess, DesiredAccess );
}

/******************************************************************************
 * AreAnyAccessesGranted [ADVAPI32.@]
 *
 * Determines whether or not any of a set of specified access permissions have
 * been granted or not.
 *
 * PARAMS
 *   GrantedAccess [I] The permissions that have been granted.
 *   DesiredAccess [I] The permissions that you want to have.
 *
 * RETURNS
 *   Nonzero if any of the permissions have been granted, zero if none of the
 *   permissions have been granted.
 */

BOOL WINAPI AreAnyAccessesGranted( DWORD GrantedAccess, DWORD DesiredAccess )
{
    return RtlAreAnyAccessesGranted( GrantedAccess, DesiredAccess );
}

/******************************************************************************
 * SetNamedSecurityInfoW [ADVAPI32.@]
 */
DWORD WINAPI SetNamedSecurityInfoW(LPWSTR pObjectName,
        SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
        PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl)
{
    FIXME("%s %d %ld %p %p %p %p\n", debugstr_w(pObjectName), ObjectType,
           SecurityInfo, psidOwner, psidGroup, pDacl, pSacl);
    return ERROR_SUCCESS;
}

/******************************************************************************
 * GetExplicitEntriesFromAclA [ADVAPI32.@]
 */
DWORD WINAPI GetExplicitEntriesFromAclA( PACL pacl, PULONG pcCountOfExplicitEntries,
        PEXPLICIT_ACCESSA* pListOfExplicitEntries)
{
    FIXME("%p %p %p\n",pacl, pcCountOfExplicitEntries, pListOfExplicitEntries);
    return ERROR_CALL_NOT_IMPLEMENTED;
}

/******************************************************************************
 * GetExplicitEntriesFromAclW [ADVAPI32.@]
 */
DWORD WINAPI GetExplicitEntriesFromAclW( PACL pacl, PULONG pcCountOfExplicitEntries,
        PEXPLICIT_ACCESSW* pListOfExplicitEntries)
{
    FIXME("%p %p %p\n",pacl, pcCountOfExplicitEntries, pListOfExplicitEntries);
    return ERROR_CALL_NOT_IMPLEMENTED;
}


/******************************************************************************
 * ParseAclStringFlags
 */
static DWORD ParseAclStringFlags(LPCWSTR* StringAcl)
{
    DWORD flags = 0;
    LPCWSTR szAcl = *StringAcl;

    while (*szAcl != '(')
    {
        if (*szAcl == 'P')
	{
            flags |= SE_DACL_PROTECTED;
	}
        else if (*szAcl == 'A')
        {
            szAcl++;
            if (*szAcl == 'R')
                flags |= SE_DACL_AUTO_INHERIT_REQ;
	    else if (*szAcl == 'I')
                flags |= SE_DACL_AUTO_INHERITED;
        }
        szAcl++;
    }

    *StringAcl = szAcl;
    return flags;
}

/******************************************************************************
 * ParseAceStringType
 */
ACEFLAG AceType[] =
{
    { SDDL_ACCESS_ALLOWED, ACCESS_ALLOWED_ACE_TYPE },
    { SDDL_ALARM,          SYSTEM_ALARM_ACE_TYPE },
    { SDDL_AUDIT,          SYSTEM_AUDIT_ACE_TYPE },
    { SDDL_ACCESS_DENIED,  ACCESS_DENIED_ACE_TYPE },
    /*
    { SDDL_OBJECT_ACCESS_ALLOWED, ACCESS_ALLOWED_OBJECT_ACE_TYPE },
    { SDDL_OBJECT_ACCESS_DENIED,  ACCESS_DENIED_OBJECT_ACE_TYPE },
    { SDDL_OBJECT_ALARM,          SYSTEM_ALARM_OBJECT_ACE_TYPE },
    { SDDL_OBJECT_AUDIT,          SYSTEM_AUDIT_OBJECT_ACE_TYPE },
    */
    { NULL, 0 },
};

static BYTE ParseAceStringType(LPCWSTR* StringAcl)
{
    UINT len = 0;
    LPCWSTR szAcl = *StringAcl;
    LPACEFLAG lpaf = AceType;

    while (lpaf->wstr &&
        (len = strlenW(lpaf->wstr)) &&
        strncmpW(lpaf->wstr, szAcl, len))
        lpaf++;

    if (!lpaf->wstr)
        return 0;

    *StringAcl += len;
    return lpaf->value;
}


/******************************************************************************
 * ParseAceStringFlags
 */
ACEFLAG AceFlags[] =
{
    { SDDL_CONTAINER_INHERIT, CONTAINER_INHERIT_ACE },
    { SDDL_AUDIT_FAILURE,     FAILED_ACCESS_ACE_FLAG },
    { SDDL_INHERITED,         INHERITED_ACE },
    { SDDL_INHERIT_ONLY,      INHERIT_ONLY_ACE },
    { SDDL_NO_PROPAGATE,      NO_PROPAGATE_INHERIT_ACE },
    { SDDL_OBJECT_INHERIT,    OBJECT_INHERIT_ACE },
    { SDDL_AUDIT_SUCCESS,     SUCCESSFUL_ACCESS_ACE_FLAG },
    { NULL, 0 },
};

static BYTE ParseAceStringFlags(LPCWSTR* StringAcl)
{
    UINT len = 0;
    BYTE flags = 0;
    LPCWSTR szAcl = *StringAcl;

    while (*szAcl != ';')
    {
        LPACEFLAG lpaf = AceFlags;

        while (lpaf->wstr &&
               (len = strlenW(lpaf->wstr)) &&
               strncmpW(lpaf->wstr, szAcl, len))
            lpaf++;

        if (!lpaf->wstr)
            return 0;

	flags |= lpaf->value;
        szAcl += len;
    }

    *StringAcl = szAcl;
    return flags;
}


/******************************************************************************
 * ParseAceStringRights
 */
ACEFLAG AceRights[] =
{
    { SDDL_GENERIC_ALL,     GENERIC_ALL },
    { SDDL_GENERIC_READ,    GENERIC_READ },
    { SDDL_GENERIC_WRITE,   GENERIC_WRITE },
    { SDDL_GENERIC_EXECUTE, GENERIC_EXECUTE },
    { SDDL_READ_CONTROL,    READ_CONTROL },
    { SDDL_STANDARD_DELETE, DELETE },
    { SDDL_WRITE_DAC,       WRITE_DAC },
    { SDDL_WRITE_OWNER,     WRITE_OWNER },
    { NULL, 0 },
};

static DWORD ParseAceStringRights(LPCWSTR* StringAcl)
{
    UINT len = 0;
    DWORD rights = 0;
    LPCWSTR szAcl = *StringAcl;

    if ((*szAcl == '0') && (*(szAcl + 1) == 'x'))
    {
        LPCWSTR p = szAcl;

	while (*p && *p != ';')
            p++;

	if (p - szAcl <= 8)
	{
	    rights = strtoulW(szAcl, NULL, 16);
	    *StringAcl = p;
	}
	else
            WARN("Invalid rights string format: %s\n", debugstr_wn(szAcl, p - szAcl));
    }
    else
    {
        while (*szAcl != ';')
        {
            LPACEFLAG lpaf = AceRights;

            while (lpaf->wstr &&
               (len = strlenW(lpaf->wstr)) &&
               strncmpW(lpaf->wstr, szAcl, len))
	    {
               lpaf++;
	    }

            if (!lpaf->wstr)
                return 0;

	    rights |= lpaf->value;
            szAcl += len;
        }
    }

    *StringAcl = szAcl;
    return rights;
}


/******************************************************************************
 * ParseStringAclToAcl
 * 
 * dacl_flags(string_ace1)(string_ace2)... (string_acen) 
 */
static BOOL ParseStringAclToAcl(LPCWSTR StringAcl, LPDWORD lpdwFlags, 
    PACL pAcl, LPDWORD cBytes)
{
    DWORD val;
    DWORD sidlen;
    DWORD length = sizeof(ACL);
    PACCESS_ALLOWED_ACE pAce = NULL; /* pointer to current ACE */

    TRACE("%s\n", debugstr_w(StringAcl));

    if (!StringAcl)
	return FALSE;

    if (pAcl) /* pAce is only useful if we're setting values */
        pAce = (PACCESS_ALLOWED_ACE) ((LPBYTE)pAcl + sizeof(PACL));

    /* Parse ACL flags */
    *lpdwFlags = ParseAclStringFlags(&StringAcl);

    /* Parse ACE */
    while (*StringAcl == '(')
    {
        StringAcl++;

        /* Parse ACE type */
        val = ParseAceStringType(&StringAcl);
	if (pAce)
            pAce->Header.AceType = (BYTE) val;
        if (*StringAcl != ';')
            goto lerr;
        StringAcl++;

        /* Parse ACE flags */
	val = ParseAceStringFlags(&StringAcl);
	if (pAce)
            pAce->Header.AceFlags = (BYTE) val;
        if (*StringAcl != ';')
            goto lerr;
        StringAcl++;

        /* Parse ACE rights */
	val = ParseAceStringRights(&StringAcl);
	if (pAce)
            pAce->Mask = val;
        if (*StringAcl != ';')
            goto lerr;
        StringAcl++;

        /* Parse ACE object guid */
        if (*StringAcl != ';')
        {
            FIXME("Support for *_OBJECT_ACE_TYPE not implemented\n");
            goto lerr;
        }
        StringAcl++;

        /* Parse ACE inherit object guid */
        if (*StringAcl != ';')
        {
            FIXME("Support for *_OBJECT_ACE_TYPE not implemented\n");
            goto lerr;
        }
        StringAcl++;

        /* Parse ACE account sid */
        if (ParseStringSidToSid(StringAcl, pAce ? (PSID)&pAce->SidStart : NULL, &sidlen))
	{
            while (*StringAcl && *StringAcl != ')')
                StringAcl++;
	}

        if (*StringAcl != ')')
            goto lerr;
        StringAcl++;

	length += sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD) + sidlen;
    }

    *cBytes = length;
    return TRUE;

lerr:
    WARN("Invalid ACE string format\n");
    return FALSE;
}


/******************************************************************************
 * ParseStringSecurityDescriptorToSecurityDescriptor
 */
static BOOL ParseStringSecurityDescriptorToSecurityDescriptor(
    LPCWSTR StringSecurityDescriptor,
    SECURITY_DESCRIPTOR* SecurityDescriptor,
    LPDWORD cBytes)
{
    BOOL bret = FALSE;
    WCHAR toktype;
    WCHAR tok[MAX_PATH];
    LPCWSTR lptoken;
    LPBYTE lpNext = NULL;
    DWORD len;

    *cBytes = 0;

    if (SecurityDescriptor)
        lpNext = ((LPBYTE) SecurityDescriptor) + sizeof(SECURITY_DESCRIPTOR);

    while (*StringSecurityDescriptor)
    {
        toktype = *StringSecurityDescriptor;

	/* Expect char identifier followed by ':' */
	StringSecurityDescriptor++;
        if (*StringSecurityDescriptor != ':')
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            goto lend;
        }
	StringSecurityDescriptor++;

	/* Extract token */
	lptoken = StringSecurityDescriptor;
	while (*lptoken && *lptoken != ':')
            lptoken++;

	if (*lptoken)
            lptoken--;

        len = lptoken - StringSecurityDescriptor;
        memcpy( tok, StringSecurityDescriptor, len * sizeof(WCHAR) );
        tok[len] = 0;

        switch (toktype)
	{
            case 'O':
            {
                DWORD bytes;

                if (!ParseStringSidToSid(tok, (PSID)lpNext, &bytes))
                    goto lend;

                if (SecurityDescriptor)
                {
                    SecurityDescriptor->Owner = (PSID)(lpNext - (LPBYTE)SecurityDescriptor);
                    lpNext += bytes; /* Advance to next token */
                }

		*cBytes += bytes;

                break;
            }

            case 'G':
            {
                DWORD bytes;

                if (!ParseStringSidToSid(tok, (PSID)lpNext, &bytes))
                    goto lend;

                if (SecurityDescriptor)
                {
                    SecurityDescriptor->Group = (PSID)(lpNext - (LPBYTE)SecurityDescriptor);
                    lpNext += bytes; /* Advance to next token */
                }

		*cBytes += bytes;

                break;
            }

            case 'D':
	    {
                DWORD flags;
                DWORD bytes;

                if (!ParseStringAclToAcl(tok, &flags, (PACL)lpNext, &bytes))
                    goto lend;

                if (SecurityDescriptor)
                {
                    SecurityDescriptor->Control |= SE_DACL_PRESENT | flags;
                    SecurityDescriptor->Dacl = (PACL)(lpNext - (LPBYTE)SecurityDescriptor);
                    lpNext += bytes; /* Advance to next token */
		}

		*cBytes += bytes;

		break;
            }

            case 'S':
            {
                DWORD flags;
                DWORD bytes;

                if (!ParseStringAclToAcl(tok, &flags, (PACL)lpNext, &bytes))
                    goto lend;

                if (SecurityDescriptor)
                {
                    SecurityDescriptor->Control |= SE_SACL_PRESENT | flags;
                    SecurityDescriptor->Sacl = (PACL)(lpNext - (LPBYTE)SecurityDescriptor);
                    lpNext += bytes; /* Advance to next token */
		}

		*cBytes += bytes;

		break;
            }

            default:
                FIXME("Unknown token\n");
                SetLastError(ERROR_INVALID_PARAMETER);
		goto lend;
	}

        StringSecurityDescriptor = lptoken;
    }

    bret = TRUE;

lend:
    return bret;
}

/******************************************************************************
 * ConvertStringSecurityDescriptorToSecurityDescriptorA [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorA(
        LPCSTR StringSecurityDescriptor,
        DWORD StringSDRevision,
        PSECURITY_DESCRIPTOR* SecurityDescriptor,
        PULONG SecurityDescriptorSize)
{
    UINT len;
    BOOL ret = FALSE;
    LPWSTR StringSecurityDescriptorW;

    len = MultiByteToWideChar(CP_ACP, 0, StringSecurityDescriptor, -1, NULL, 0);
    StringSecurityDescriptorW = HeapAlloc(GetProcessHeap(), 0, len * sizeof(WCHAR));

    if (StringSecurityDescriptorW)
    {
        MultiByteToWideChar(CP_ACP, 0, StringSecurityDescriptor, -1, StringSecurityDescriptorW, len);

        ret = ConvertStringSecurityDescriptorToSecurityDescriptorW(StringSecurityDescriptorW,
                                                                   StringSDRevision, SecurityDescriptor,
                                                                   SecurityDescriptorSize);
        HeapFree(GetProcessHeap(), 0, StringSecurityDescriptorW);
    }

    return ret;
}

/******************************************************************************
 * ConvertStringSecurityDescriptorToSecurityDescriptorW [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSecurityDescriptorToSecurityDescriptorW(
        LPCWSTR StringSecurityDescriptor,
        DWORD StringSDRevision,
        PSECURITY_DESCRIPTOR* SecurityDescriptor,
        PULONG SecurityDescriptorSize)
{
    DWORD cBytes;
    SECURITY_DESCRIPTOR* psd;
    BOOL bret = FALSE;

    TRACE("%s\n", debugstr_w(StringSecurityDescriptor));

    if (GetVersion() & 0x80000000)
    {
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        goto lend;
    }
    else if (StringSDRevision != SID_REVISION)
    {
        SetLastError(ERROR_UNKNOWN_REVISION);
	goto lend;
    }

    /* Compute security descriptor length */
    if (!ParseStringSecurityDescriptorToSecurityDescriptor(StringSecurityDescriptor,
        NULL, &cBytes))
	goto lend;

    psd = *SecurityDescriptor = (SECURITY_DESCRIPTOR*) LocalAlloc(
        GMEM_ZEROINIT, cBytes);

    psd->Revision = SID_REVISION;
    psd->Control |= SE_SELF_RELATIVE;

    if (!ParseStringSecurityDescriptorToSecurityDescriptor(StringSecurityDescriptor,
        psd, &cBytes))
    {
        LocalFree(psd);
	goto lend;
    }

    if (SecurityDescriptorSize)
        *SecurityDescriptorSize = cBytes;

    bret = TRUE;
 
lend:
    TRACE(" ret=%d\n", bret);
    return bret;
}

/******************************************************************************
 * ConvertStringSidToSidW [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSidToSidW(LPCWSTR StringSid, PSID* Sid)
{
    BOOL bret = FALSE;
    DWORD cBytes;

    TRACE("%s, %p\n", debugstr_w(StringSid), Sid);
    if (GetVersion() & 0x80000000)
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    else if (!StringSid || !Sid)
        SetLastError(ERROR_INVALID_PARAMETER);
    else if (ParseStringSidToSid(StringSid, NULL, &cBytes))
    {
        PSID pSid = *Sid = (PSID) LocalAlloc(0, cBytes);

        bret = ParseStringSidToSid(StringSid, pSid, &cBytes);
        if (!bret)
            LocalFree(*Sid); 
    }
    TRACE("returning %s\n", bret ? "TRUE" : "FALSE");
    return bret;
}

/******************************************************************************
 * ConvertStringSidToSidA [ADVAPI32.@]
 */
BOOL WINAPI ConvertStringSidToSidA(LPCSTR StringSid, PSID* Sid)
{
    BOOL bret = FALSE;

    TRACE("%s, %p\n", debugstr_a(StringSid), Sid);
    if (GetVersion() & 0x80000000)
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    else if (!StringSid || !Sid)
        SetLastError(ERROR_INVALID_PARAMETER);
    else
    {
        UINT len = MultiByteToWideChar(CP_ACP, 0, StringSid, -1, NULL, 0);
        LPWSTR wStringSid = HeapAlloc(GetProcessHeap(), 0,
         len * sizeof(WCHAR));

        MultiByteToWideChar(CP_ACP, 0, StringSid, -1, wStringSid, len);
        bret = ConvertStringSidToSidW(wStringSid, Sid);
        HeapFree(GetProcessHeap(), 0, wStringSid);
    }
    TRACE("returning %s\n", bret ? "TRUE" : "FALSE");
    return bret;
}

/******************************************************************************
 * ConvertSidToStringSidW [ADVAPI32.@]
 *
 *  format of SID string is:
 *    S-<count>-<auth>-<subauth1>-<subauth2>-<subauth3>...
 *  where
 *    <rev> is the revision of the SID encoded as decimal
 *    <auth> is the identifier authority encoded as hex
 *    <subauthN> is the subauthority id encoded as decimal
 */
BOOL WINAPI ConvertSidToStringSidW( PSID pSid, LPWSTR *pstr )
{
    DWORD sz, i;
    LPWSTR str;
    WCHAR fmt[] = { 'S','-','%','u','-','%','d',0 };
    WCHAR subauthfmt[] = { '-','%','u',0 };
    SID* pisid=pSid;

    TRACE("%p %p\n", pSid, pstr );

    if( !IsValidSid( pSid ) )
        return FALSE;

    if (pisid->Revision != SDDL_REVISION)
        return FALSE;
    if (pisid->IdentifierAuthority.Value[0] ||
     pisid->IdentifierAuthority.Value[1])
    {
        FIXME("not matching MS' bugs\n");
        return FALSE;
    }

    sz = 14 + pisid->SubAuthorityCount * 11;
    str = LocalAlloc( 0, sz*sizeof(WCHAR) );
    sprintfW( str, fmt, pisid->Revision, MAKELONG(
     MAKEWORD( pisid->IdentifierAuthority.Value[5],
     pisid->IdentifierAuthority.Value[4] ),
     MAKEWORD( pisid->IdentifierAuthority.Value[3],
     pisid->IdentifierAuthority.Value[2] ) ) );
    for( i=0; i<pisid->SubAuthorityCount; i++ )
        sprintfW( str + strlenW(str), subauthfmt, pisid->SubAuthority[i] );
    *pstr = str;

    return TRUE;
}

/******************************************************************************
 * ConvertSidToStringSidA [ADVAPI32.@]
 */
BOOL WINAPI ConvertSidToStringSidA(PSID pSid, LPSTR *pstr)
{
    LPWSTR wstr = NULL;
    LPSTR str;
    UINT len;

    TRACE("%p %p\n", pSid, pstr );

    if( !ConvertSidToStringSidW( pSid, &wstr ) )
        return FALSE;

    len = WideCharToMultiByte( CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL );
    str = LocalAlloc( 0, len );
    WideCharToMultiByte( CP_ACP, 0, wstr, -1, str, len, NULL, NULL );
    LocalFree( wstr );

    *pstr = str;

    return TRUE;
}

BOOL WINAPI CreatePrivateObjectSecurity(
        PSECURITY_DESCRIPTOR ParentDescriptor,
        PSECURITY_DESCRIPTOR CreatorDescriptor,
        PSECURITY_DESCRIPTOR* NewDescriptor,
        BOOL IsDirectoryObject,
        HANDLE Token,
        PGENERIC_MAPPING GenericMapping )
{
    FIXME("%p %p %p %d %p %p - stub\n", ParentDescriptor, CreatorDescriptor,
          NewDescriptor, IsDirectoryObject, Token, GenericMapping);

    return FALSE;
}

BOOL WINAPI DestroyPrivateObjectSecurity( PSECURITY_DESCRIPTOR* ObjectDescriptor )
{
    FIXME("%p - stub\n", ObjectDescriptor);

    return TRUE;
}

BOOL WINAPI CreateProcessAsUserA(
        HANDLE hToken,
        LPCSTR lpApplicationName,
        LPSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        BOOL bInheritHandles,
        DWORD dwCreationFlags,
        LPVOID lpEnvironment,
        LPCSTR lpCurrentDirectory,
        LPSTARTUPINFOA lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation )
{
    FIXME("%p %s %s %p %p %d 0x%08lx %p %s %p %p - stub\n", hToken, debugstr_a(lpApplicationName),
          debugstr_a(lpCommandLine), lpProcessAttributes, lpThreadAttributes, bInheritHandles,
          dwCreationFlags, lpEnvironment, debugstr_a(lpCurrentDirectory), lpStartupInfo, lpProcessInformation);

    return FALSE;
}

BOOL WINAPI CreateProcessAsUserW(
        HANDLE hToken,
        LPCWSTR lpApplicationName,
        LPWSTR lpCommandLine,
        LPSECURITY_ATTRIBUTES lpProcessAttributes,
        LPSECURITY_ATTRIBUTES lpThreadAttributes,
        BOOL bInheritHandles,
        DWORD dwCreationFlags,
        LPVOID lpEnvironment,
        LPCWSTR lpCurrentDirectory,
        LPSTARTUPINFOW lpStartupInfo,
        LPPROCESS_INFORMATION lpProcessInformation )
{
    FIXME("%p %s %s %p %p %d 0x%08lx %p %s %p %p - semi- stub\n", hToken, 
          debugstr_w(lpApplicationName), debugstr_w(lpCommandLine), lpProcessAttributes,
          lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, 
          debugstr_w(lpCurrentDirectory), lpStartupInfo, lpProcessInformation);

    /* We should create the process with a suspended main thread */
    if (!CreateProcessW (lpApplicationName,
                         lpCommandLine,
                         lpProcessAttributes,
                         lpThreadAttributes,
                         bInheritHandles,
                         dwCreationFlags, /* CREATE_SUSPENDED */
                         lpEnvironment,
                         lpCurrentDirectory,
                         lpStartupInfo,
                         lpProcessInformation))
    {
      return FALSE;
    }

    return TRUE;
}

/******************************************************************************
 * ComputeStringSidSize
 */
static DWORD ComputeStringSidSize(LPCWSTR StringSid)
{
    int ctok = 0;
    DWORD size = sizeof(SID);

    while (*StringSid)
    {
        if (*StringSid == '-')
            ctok++;
        StringSid++;
    }

    if (ctok > 3)
        size += (ctok - 3) * sizeof(DWORD);

    return size;
}

BOOL WINAPI DuplicateTokenEx(
        HANDLE ExistingTokenHandle, DWORD dwDesiredAccess,
        LPSECURITY_ATTRIBUTES lpTokenAttributes,
        SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
        TOKEN_TYPE TokenType,
        PHANDLE DuplicateTokenHandle )
{
    OBJECT_ATTRIBUTES ObjectAttributes;

    TRACE("%p 0x%08lx 0x%08x 0x%08x %p\n", ExistingTokenHandle, dwDesiredAccess,
          ImpersonationLevel, TokenType, DuplicateTokenHandle);

    InitializeObjectAttributes(
        &ObjectAttributes,
        NULL,
        (lpTokenAttributes && lpTokenAttributes->bInheritHandle) ? OBJ_INHERIT : 0,
        NULL,
        lpTokenAttributes ? lpTokenAttributes->lpSecurityDescriptor : NULL );

    return set_ntstatus( NtDuplicateToken( ExistingTokenHandle,
                                           dwDesiredAccess,
                                           &ObjectAttributes,
                                           ImpersonationLevel,
                                           TokenType,
                                           DuplicateTokenHandle ) );
}

BOOL WINAPI DuplicateToken(
        HANDLE ExistingTokenHandle,
        SECURITY_IMPERSONATION_LEVEL ImpersonationLevel,
        PHANDLE DuplicateTokenHandle )
{
    return DuplicateTokenEx( ExistingTokenHandle, TOKEN_IMPERSONATE | TOKEN_QUERY,
                             NULL, ImpersonationLevel, TokenImpersonation,
                             DuplicateTokenHandle );
}

BOOL WINAPI EnumDependentServicesA(
        SC_HANDLE hService,
        DWORD dwServiceState,
        LPENUM_SERVICE_STATUSA lpServices,
        DWORD cbBufSize,
        LPDWORD pcbBytesNeeded,
        LPDWORD lpServicesReturned )
{
    FIXME("%p 0x%08lx %p 0x%08lx %p %p - stub\n", hService, dwServiceState,
          lpServices, cbBufSize, pcbBytesNeeded, lpServicesReturned);

    return FALSE;
}

BOOL WINAPI EnumDependentServicesW(
        SC_HANDLE hService,
        DWORD dwServiceState,
        LPENUM_SERVICE_STATUSW lpServices,
        DWORD cbBufSize,
        LPDWORD pcbBytesNeeded,
        LPDWORD lpServicesReturned )
{
    FIXME("%p 0x%08lx %p 0x%08lx %p %p - stub\n", hService, dwServiceState,
          lpServices, cbBufSize, pcbBytesNeeded, lpServicesReturned);

    return FALSE;
}

/******************************************************************************
 * ParseStringSidToSid
 */
static BOOL ParseStringSidToSid(LPCWSTR StringSid, PSID pSid, LPDWORD cBytes)
{
    BOOL bret = FALSE;
    SID* pisid=pSid;

    TRACE("%s, %p, %p\n", debugstr_w(StringSid), pSid, cBytes);
    if (!StringSid)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        TRACE("StringSid is NULL, returning FALSE\n");
	return FALSE;
    }

    *cBytes = ComputeStringSidSize(StringSid);
    if (!pisid) /* Simply compute the size */
    {
        TRACE("only size requested, returning TRUE\n");
        return TRUE;
    }

    if (StringSid[0] == 'S' && StringSid[1] == '-') /* S-R-I-S-S */
    {
        DWORD i = 0, identAuth;
	DWORD csubauth = ((*cBytes - sizeof(SID)) / sizeof(DWORD)) + 1;

        StringSid += 2; /* Advance to Revision */
        pisid->Revision = atoiW(StringSid);

        if (pisid->Revision != SDDL_REVISION)
        {
            TRACE("Revision %d is unknown\n", pisid->Revision);
            goto lend; /* ERROR_INVALID_SID */
        }
        if (csubauth == 0)
        {
            TRACE("SubAuthorityCount is 0\n");
            goto lend; /* ERROR_INVALID_SID */
        }

	pisid->SubAuthorityCount = csubauth;

        /* Advance to identifier authority */
	while (*StringSid && *StringSid != '-')
            StringSid++;
        if (*StringSid == '-')
            StringSid++;

        /* MS' implementation can't handle values greater than 2^32 - 1, so
         * we don't either; assume most significant bytes are always 0
         */
        pisid->IdentifierAuthority.Value[0] = 0;
        pisid->IdentifierAuthority.Value[1] = 0;
        identAuth = atoiW(StringSid);
        pisid->IdentifierAuthority.Value[5] = identAuth & 0xff;
        pisid->IdentifierAuthority.Value[4] = (identAuth & 0xff00) >> 8;
        pisid->IdentifierAuthority.Value[3] = (identAuth & 0xff0000) >> 16;
        pisid->IdentifierAuthority.Value[2] = (identAuth & 0xff000000) >> 24;

        /* Advance to first sub authority */
        while (*StringSid && *StringSid != '-')
            StringSid++;
        if (*StringSid == '-')
            StringSid++;

        while (*StringSid)
	{	
	    while (*StringSid && *StringSid != '-')
                StringSid++;

            pisid->SubAuthority[i++] = atoiW(StringSid);
        }

	if (i != pisid->SubAuthorityCount)
            goto lend; /* ERROR_INVALID_SID */

        bret = TRUE;
    }
    else /* String constant format  - Only available in winxp and above */
    {
        pisid->Revision = SDDL_REVISION;
	pisid->SubAuthorityCount = 1;

	FIXME("String constant not supported: %s\n", debugstr_wn(StringSid, 2));

	/* TODO: Lookup string of well-known SIDs in table */
	pisid->IdentifierAuthority.Value[5] = 0;
	pisid->SubAuthority[0] = 0;

        bret = TRUE;
    }

lend:
    if (!bret)
        SetLastError(ERROR_INVALID_SID);

    TRACE("returning %s\n", bret ? "TRUE" : "FALSE");
    return bret;
}

/******************************************************************************
 * GetNamedSecurityInfoA [ADVAPI32.@]
 */
DWORD WINAPI GetNamedSecurityInfoA(LPSTR pObjectName,
        SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
        PSID* ppsidOwner, PSID* ppsidGroup, PACL* ppDacl, PACL* ppSacl,
        PSECURITY_DESCRIPTOR* ppSecurityDescriptor)
{
    DWORD len;
    LPWSTR wstr = NULL;
    DWORD r;

    TRACE("%s %d %ld %p %p %p %p %p\n", pObjectName, ObjectType, SecurityInfo,
        ppsidOwner, ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor);

    if( pObjectName )
    {
        len = MultiByteToWideChar( CP_ACP, 0, pObjectName, -1, NULL, 0 );
        wstr = HeapAlloc( GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar( CP_ACP, 0, pObjectName, -1, wstr, len );
    }

    r = GetNamedSecurityInfoW( wstr, ObjectType, SecurityInfo, ppsidOwner,
                           ppsidGroup, ppDacl, ppSacl, ppSecurityDescriptor );

    HeapFree( GetProcessHeap(), 0, wstr );

    return r;
}

/******************************************************************************
 * GetNamedSecurityInfoW [ADVAPI32.@]
 */
DWORD WINAPI GetNamedSecurityInfoW( LPWSTR name, SE_OBJECT_TYPE type,
    SECURITY_INFORMATION info, PSID* owner, PSID* group, PACL* dacl,
    PACL* sacl, PSECURITY_DESCRIPTOR* descriptor )
{
    DWORD needed, offset;
    SECURITY_DESCRIPTOR_RELATIVE *relative;
    BYTE *buffer;

    TRACE( "%s %d %ld %p %p %p %p %p\n", debugstr_w(name), type, info, owner,
           group, dacl, sacl, descriptor );

    if (!name || !descriptor) return ERROR_INVALID_PARAMETER;

    needed = sizeof(SECURITY_DESCRIPTOR_RELATIVE);
    if (info & OWNER_SECURITY_INFORMATION)
        needed += sizeof(sidWorld);
    if (info & GROUP_SECURITY_INFORMATION)
        needed += sizeof(sidWorld);
    if (info & DACL_SECURITY_INFORMATION)
        needed += WINE_SIZE_OF_WORLD_ACCESS_ACL;
    if (info & SACL_SECURITY_INFORMATION)
        needed += WINE_SIZE_OF_WORLD_ACCESS_ACL;

    /* must be freed by caller */
    *descriptor = HeapAlloc( GetProcessHeap(), 0, needed );
    if (!*descriptor) return ERROR_NOT_ENOUGH_MEMORY;

    if (!InitializeSecurityDescriptor( *descriptor, SECURITY_DESCRIPTOR_REVISION ))
    {
        HeapFree( GetProcessHeap(), 0, *descriptor );
        return ERROR_INVALID_SECURITY_DESCR;
    }

    relative = (SECURITY_DESCRIPTOR_RELATIVE *)*descriptor;
    relative->Control |= SE_SELF_RELATIVE;
    buffer = (BYTE *)relative;
    offset = sizeof(SECURITY_DESCRIPTOR_RELATIVE);

    if (owner && (info & OWNER_SECURITY_INFORMATION))
    {
        memcpy( buffer + offset, &sidWorld, sizeof(sidWorld) );
        relative->Owner = offset;
        *owner = buffer + offset;
        offset += sizeof(sidWorld);
    }
    if (group && (info & GROUP_SECURITY_INFORMATION))
    {
        memcpy( buffer + offset, &sidWorld, sizeof(sidWorld) );
        relative->Group = offset;
        *group = buffer + offset;
        offset += sizeof(sidWorld);
    }
    if (dacl && (info & DACL_SECURITY_INFORMATION))
    {
        GetWorldAccessACL( (PACL)(buffer + offset) );
        relative->Dacl = offset;
        *dacl = (PACL)(buffer + offset);
        offset += WINE_SIZE_OF_WORLD_ACCESS_ACL;
    }
    if (sacl && (info & SACL_SECURITY_INFORMATION))
    {
        GetWorldAccessACL( (PACL)(buffer + offset) );
        relative->Sacl = offset;
        *sacl = (PACL)(buffer + offset);
    }
    return ERROR_SUCCESS;
}

/******************************************************************************
 * DecryptFileW [ADVAPI32.@]
 */
BOOL WINAPI DecryptFileW(LPCWSTR lpFileName, DWORD dwReserved)
{
    FIXME("%s %08lx\n", debugstr_w(lpFileName), dwReserved);
    return TRUE;
}

/******************************************************************************
 * DecryptFileA [ADVAPI32.@]
 */
BOOL WINAPI DecryptFileA(LPCSTR lpFileName, DWORD dwReserved)
{
    FIXME("%s %08lx\n", debugstr_a(lpFileName), dwReserved);
    return TRUE;
}

/******************************************************************************
 * EncryptFileW [ADVAPI32.@]
 */
BOOL WINAPI EncryptFileW(LPCWSTR lpFileName)
{
    FIXME("%s\n", debugstr_w(lpFileName));
    return TRUE;
}

/******************************************************************************
 * EncryptFileA [ADVAPI32.@]
 */
BOOL WINAPI EncryptFileA(LPCSTR lpFileName)
{
    FIXME("%s\n", debugstr_a(lpFileName));
    return TRUE;
}

/******************************************************************************
 * SetSecurityInfo [ADVAPI32.@]
 */
DWORD WINAPI SetSecurityInfo(HANDLE handle, SE_OBJECT_TYPE ObjectType, 
                      SECURITY_INFORMATION SecurityInfo, PSID psidOwner,
                      PSID psidGroup, PACL pDacl, PACL pSacl) {
    FIXME("stub\n");
    return ERROR_SUCCESS;
}
