/*
 *	Security functions
 *
 *	Copyright 1996-1998 Marcus Meissner
 * 	Copyright 2003 CodeWeavers Inc. (Ulrich Czekalla)
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "windef.h"
#include "wine/exception.h"
#include "ntdll_misc.h"
#include "excpt.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntdll);

#define NT_SUCCESS(status) (status == STATUS_SUCCESS)

/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

/* helper function to copy an ACL */
static BOOLEAN copy_acl(DWORD nDestinationAclLength, PACL pDestinationAcl, PACL pSourceAcl)
{
    DWORD size;

    if (!pSourceAcl || !RtlValidAcl(pSourceAcl))
        return FALSE;
        
    size = ((ACL *)pSourceAcl)->AclSize;
    if (nDestinationAclLength < size)
        return FALSE;

    memmove(pDestinationAcl, pSourceAcl, size);
    return TRUE;
}

/* generically adds an ACE to an ACL */
static NTSTATUS add_access_ace(PACL pAcl, DWORD dwAceRevision, DWORD dwAceFlags,
                               DWORD dwAccessMask, PSID pSid, DWORD dwAceType)
{
    ACE_HEADER *pAceHeader;
    DWORD dwLengthSid;
    DWORD dwAceSize;
    DWORD *pAccessMask;
    DWORD *pSidStart;

    if (!RtlValidSid(pSid))
        return STATUS_INVALID_SID;

    if (pAcl->AclRevision > MAX_ACL_REVISION || dwAceRevision > MAX_ACL_REVISION)
        return STATUS_REVISION_MISMATCH;

    if (!RtlValidAcl(pAcl))
        return STATUS_INVALID_ACL;

    if (!RtlFirstFreeAce(pAcl, &pAceHeader))
        return STATUS_INVALID_ACL;

    if (!pAceHeader)
        return STATUS_ALLOTTED_SPACE_EXCEEDED;

    /* calculate generic size of the ACE */
    dwLengthSid = RtlLengthSid(pSid);
    dwAceSize = sizeof(ACE_HEADER) + sizeof(DWORD) + dwLengthSid;
    if ((char *)pAceHeader + dwAceSize > (char *)pAcl + pAcl->AclSize)
        return STATUS_ALLOTTED_SPACE_EXCEEDED;

    /* fill the new ACE */
    pAceHeader->AceType = dwAceType;
    pAceHeader->AceFlags = dwAceFlags;
    pAceHeader->AceSize = dwAceSize;

    /* skip past the ACE_HEADER of the ACE */
    pAccessMask = (DWORD *)(pAceHeader + 1);
    *pAccessMask = dwAccessMask;

    /* skip past ACE->Mask */
    pSidStart = pAccessMask + 1;
    RtlCopySid(dwLengthSid, (PSID)pSidStart, pSid);

    pAcl->AclRevision = max(pAcl->AclRevision, dwAceRevision);
    pAcl->AceCount++;

    return STATUS_SUCCESS;
}

/*
 *	SID FUNCTIONS
 */

/******************************************************************************
 *  RtlAllocateAndInitializeSid		[NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlAllocateAndInitializeSid (
	PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
	BYTE nSubAuthorityCount,
	DWORD nSubAuthority0, DWORD nSubAuthority1,
	DWORD nSubAuthority2, DWORD nSubAuthority3,
	DWORD nSubAuthority4, DWORD nSubAuthority5,
	DWORD nSubAuthority6, DWORD nSubAuthority7,
	PSID *pSid )
{

	TRACE("(%p, 0x%04x,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,0x%08lx,%p)\n",
		pIdentifierAuthority,nSubAuthorityCount,
		nSubAuthority0, nSubAuthority1,	nSubAuthority2, nSubAuthority3,
		nSubAuthority4, nSubAuthority5,	nSubAuthority6, nSubAuthority7, pSid);

	if (!(*pSid = RtlAllocateHeap( GetProcessHeap(), 0,
                                       RtlLengthRequiredSid(nSubAuthorityCount))))
	    return STATUS_NO_MEMORY;

	((SID*)*pSid)->Revision = SID_REVISION;

	if (pIdentifierAuthority)
	  memcpy(&((SID*)*pSid)->IdentifierAuthority, pIdentifierAuthority, sizeof (SID_IDENTIFIER_AUTHORITY));
	*RtlSubAuthorityCountSid(*pSid) = nSubAuthorityCount;

	if (nSubAuthorityCount > 0)
          *RtlSubAuthoritySid(*pSid, 0) = nSubAuthority0;
	if (nSubAuthorityCount > 1)
          *RtlSubAuthoritySid(*pSid, 1) = nSubAuthority1;
	if (nSubAuthorityCount > 2)
          *RtlSubAuthoritySid(*pSid, 2) = nSubAuthority2;
	if (nSubAuthorityCount > 3)
          *RtlSubAuthoritySid(*pSid, 3) = nSubAuthority3;
	if (nSubAuthorityCount > 4)
          *RtlSubAuthoritySid(*pSid, 4) = nSubAuthority4;
	if (nSubAuthorityCount > 5)
          *RtlSubAuthoritySid(*pSid, 5) = nSubAuthority5;
        if (nSubAuthorityCount > 6)
	  *RtlSubAuthoritySid(*pSid, 6) = nSubAuthority6;
	if (nSubAuthorityCount > 7)
          *RtlSubAuthoritySid(*pSid, 7) = nSubAuthority7;

	return STATUS_SUCCESS;
}
/******************************************************************************
 *  RtlEqualSid		[NTDLL.@]
 *
 * Determine if two SIDs are equal.
 *
 * PARAMS
 *  pSid1 [I] Source SID
 *  pSid2 [I] SID to compare with
 *
 * RETURNS
 *  TRUE, if pSid1 is equal to pSid2,
 *  FALSE otherwise.
 */
BOOL WINAPI RtlEqualSid( PSID pSid1, PSID pSid2 )
{
    if (!RtlValidSid(pSid1) || !RtlValidSid(pSid2))
        return FALSE;

    if (*RtlSubAuthorityCountSid(pSid1) != *RtlSubAuthorityCountSid(pSid2))
        return FALSE;

    if (memcmp(pSid1, pSid2, RtlLengthSid(pSid1)) != 0)
        return FALSE;

    return TRUE;
}

/******************************************************************************
 * RtlEqualPrefixSid	[NTDLL.@]
 */
BOOL WINAPI RtlEqualPrefixSid (PSID pSid1, PSID pSid2)
{
    if (!RtlValidSid(pSid1) || !RtlValidSid(pSid2))
        return FALSE;

    if (*RtlSubAuthorityCountSid(pSid1) != *RtlSubAuthorityCountSid(pSid2))
        return FALSE;

    if (memcmp(pSid1, pSid2, RtlLengthRequiredSid(((SID*)pSid1)->SubAuthorityCount - 1)) != 0)
        return FALSE;

    return TRUE;
}


/******************************************************************************
 *  RtlFreeSid		[NTDLL.@]
 *
 * Free the resources used by a SID.
 *
 * PARAMS
 *  pSid [I] SID to Free.
 *
 * RETURNS
 *  STATUS_SUCCESS.
 */
DWORD WINAPI RtlFreeSid(PSID pSid)
{
	TRACE("(%p)\n", pSid);
	RtlFreeHeap( GetProcessHeap(), 0, pSid );
	return STATUS_SUCCESS;
}

/**************************************************************************
 * RtlLengthRequiredSid	[NTDLL.@]
 *
 * Determine the amount of memory a SID will use
 *
 * PARAMS
 *   nrofsubauths [I] Number of Sub Authorities in the SID.
 *
 * RETURNS
 *   The size, in bytes, of a SID with nrofsubauths Sub Authorities.
 */
DWORD WINAPI RtlLengthRequiredSid(DWORD nrofsubauths)
{
	return (nrofsubauths-1)*sizeof(DWORD) + sizeof(SID);
}

/**************************************************************************
 *                 RtlLengthSid				[NTDLL.@]
 *
 * Determine the amount of memory a SID is using
 *
 * PARAMS
 *  pSid [I] SID to get the size of.
 *
 * RETURNS
 *  The size, in bytes, of pSid.
 */
DWORD WINAPI RtlLengthSid(PSID pSid)
{
	TRACE("sid=%p\n",pSid);
	if (!pSid) return 0;
	return RtlLengthRequiredSid(*RtlSubAuthorityCountSid(pSid));
}

/**************************************************************************
 *                 RtlInitializeSid			[NTDLL.@]
 *
 * Initialise a SID.
 *
 * PARAMS
 *  pSid                 [I] SID to initialise
 *  pIdentifierAuthority [I] Identifier Authority
 *  nSubAuthorityCount   [I] Number of Sub Authorities
 *
 * RETURNS
 *  Success: TRUE. pSid is initialised with the details given.
 *  Failure: FALSE, if nSubAuthorityCount is >= SID_MAX_SUB_AUTHORITIES.
 */
BOOL WINAPI RtlInitializeSid(
	PSID pSid,
	PSID_IDENTIFIER_AUTHORITY pIdentifierAuthority,
	BYTE nSubAuthorityCount)
{
	int i;
	SID* pisid=pSid;

	if (nSubAuthorityCount >= SID_MAX_SUB_AUTHORITIES)
	  return FALSE;

	pisid->Revision = SID_REVISION;
	pisid->SubAuthorityCount = nSubAuthorityCount;
	if (pIdentifierAuthority)
	  memcpy(&pisid->IdentifierAuthority, pIdentifierAuthority, sizeof (SID_IDENTIFIER_AUTHORITY));

	for (i = 0; i < nSubAuthorityCount; i++)
	  *RtlSubAuthoritySid(pSid, i) = 0;

	return TRUE;
}

/**************************************************************************
 *                 RtlSubAuthoritySid			[NTDLL.@]
 *
 * Return the Sub Authority of a SID
 *
 * PARAMS
 *   pSid          [I] SID to get the Sub Authority from.
 *   nSubAuthority [I] Sub Authority number.
 *
 * RETURNS
 *   A pointer to The Sub Authority value of pSid.
 */
LPDWORD WINAPI RtlSubAuthoritySid( PSID pSid, DWORD nSubAuthority )
{
    return &(((SID*)pSid)->SubAuthority[nSubAuthority]);
}

/**************************************************************************
 * RtlIdentifierAuthoritySid	[NTDLL.@]
 *
 * Return the Identifier Authority of a SID.
 *
 * PARAMS
 *   pSid [I] SID to get the Identifier Authority from.
 *
 * RETURNS
 *   A pointer to the Identifier Authority value of pSid.
 */
PSID_IDENTIFIER_AUTHORITY WINAPI RtlIdentifierAuthoritySid( PSID pSid )
{
    return &(((SID*)pSid)->IdentifierAuthority);
}

/**************************************************************************
 *                 RtlSubAuthorityCountSid		[NTDLL.@]
 *
 * Get the number of Sub Authorities in a SID.
 *
 * PARAMS
 *   pSid [I] SID to get the count from.
 *
 * RETURNS
 *  A pointer to the Sub Authority count of pSid.
 */
LPBYTE WINAPI RtlSubAuthorityCountSid(PSID pSid)
{
    return &(((SID*)pSid)->SubAuthorityCount);
}

/**************************************************************************
 *                 RtlCopySid				[NTDLL.@]
 */
BOOLEAN WINAPI RtlCopySid( DWORD nDestinationSidLength, PSID pDestinationSid, PSID pSourceSid )
{
	if (!pSourceSid || !RtlValidSid(pSourceSid) ||
	    (nDestinationSidLength < RtlLengthSid(pSourceSid)))
          return FALSE;

	if (nDestinationSidLength < (((SID*)pSourceSid)->SubAuthorityCount*4+8))
	  return FALSE;

	memmove(pDestinationSid, pSourceSid, ((SID*)pSourceSid)->SubAuthorityCount*4+8);
	return TRUE;
}
/******************************************************************************
 * RtlValidSid [NTDLL.@]
 *
 * Determine if a SID is valid.
 *
 * PARAMS
 *   pSid [I] SID to check
 *
 * RETURNS
 *   TRUE if pSid is valid,
 *   FALSE otherwise.
 */
BOOLEAN WINAPI RtlValidSid( PSID pSid )
{
    BOOL ret;
    __TRY
    {
        ret = TRUE;
        if (!pSid || ((SID*)pSid)->Revision != SID_REVISION ||
            ((SID*)pSid)->SubAuthorityCount > SID_MAX_SUB_AUTHORITIES)
        {
            ret = FALSE;
        }
    }
    __EXCEPT(page_fault)
    {
        WARN("(%p): invalid pointer!\n", pSid);
        return FALSE;
    }
    __ENDTRY
    return ret;
}


/*
 *	security descriptor functions
 */

/**************************************************************************
 * RtlCreateSecurityDescriptor			[NTDLL.@]
 *
 * Initialise a SECURITY_DESCRIPTOR.
 *
 * PARAMS
 *  lpsd [O] Descriptor to initialise.
 *  rev  [I] Revision, must be set to SECURITY_DESCRIPTOR_REVISION.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: STATUS_UNKNOWN_REVISION if rev is incorrect.
 */
NTSTATUS WINAPI RtlCreateSecurityDescriptor(
	PSECURITY_DESCRIPTOR lpsd,
	DWORD rev)
{
	if (rev!=SECURITY_DESCRIPTOR_REVISION)
		return STATUS_UNKNOWN_REVISION;
	memset(lpsd,'\0',sizeof(SECURITY_DESCRIPTOR));
	((SECURITY_DESCRIPTOR*)lpsd)->Revision = SECURITY_DESCRIPTOR_REVISION;
	return STATUS_SUCCESS;
}

/**************************************************************************
 * RtlCopySecurityDescriptor            [NTDLL.@]
 *
 * Copies an absolute or sefl-relative SECURITY_DESCRIPTOR.
 *
 * PARAMS
 *  pSourceSD      [O] SD to copy from.
 *  pDestinationSD [I] Destination SD.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: STATUS_UNKNOWN_REVISION if rev is incorrect.
 */
NTSTATUS WINAPI RtlCopySecurityDescriptor(PSECURITY_DESCRIPTOR pSourceSD, PSECURITY_DESCRIPTOR pDestinationSD)
{
    SECURITY_DESCRIPTOR *srcSD = (SECURITY_DESCRIPTOR *)pSourceSD;
    SECURITY_DESCRIPTOR *destSD = (SECURITY_DESCRIPTOR *)pDestinationSD;
    PSID Owner, Group;
    PACL Dacl, Sacl;
    BOOLEAN defaulted, present;
    DWORD length;
    BOOL isSelfRelative = srcSD->Control & SE_SELF_RELATIVE;
    
    if (srcSD->Revision != SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    /* copy initial data */
    destSD->Revision = srcSD->Revision;
    destSD->Sbz1 = srcSD->Sbz1;
    destSD->Control = srcSD->Control;

    /* copy Owner */
    RtlGetOwnerSecurityDescriptor(pSourceSD, &Owner, &defaulted);
    length = RtlLengthSid(Owner);

    if (isSelfRelative)
    {
        destSD->Owner = srcSD->Owner;
        RtlCopySid(length, (LPBYTE)destSD + (DWORD)destSD->Owner, Owner);
    }
    else
    {
        destSD->Owner = RtlAllocateHeap(GetProcessHeap(), 0, length);
        RtlCopySid(length, destSD->Owner, Owner);
    }

    /* copy Group */
    RtlGetGroupSecurityDescriptor(pSourceSD, &Group, &defaulted);
    length = RtlLengthSid(Group);

    if (isSelfRelative)
    {
        destSD->Group = srcSD->Group;
        RtlCopySid(length, (LPBYTE)destSD + (DWORD)destSD->Group, Group);
    }
    else
    {
        destSD->Group = RtlAllocateHeap(GetProcessHeap(), 0, length);
        RtlCopySid(length, destSD->Group, Group);
    }

    /* copy Dacl */
    if (srcSD->Control & SE_DACL_PRESENT)
    {
        RtlGetDaclSecurityDescriptor(pSourceSD, &present, &Dacl, &defaulted);
        length = Dacl->AclSize;

        if (isSelfRelative)
        {
            destSD->Dacl = srcSD->Dacl;
            copy_acl(length, (PACL)((LPBYTE)destSD + (DWORD)destSD->Dacl), Dacl);
        }
        else
        {
            destSD->Dacl = RtlAllocateHeap(GetProcessHeap(), 0, length);
            copy_acl(length, destSD->Dacl, Dacl);
        }
    }

    /* copy Sacl */
    if (srcSD->Control & SE_SACL_PRESENT)
    {
        RtlGetSaclSecurityDescriptor(pSourceSD, &present, &Sacl, &defaulted);
        length = Sacl->AclSize;

        if (isSelfRelative)
        {
            destSD->Sacl = srcSD->Sacl;
            copy_acl(length, (PACL)((LPBYTE)destSD + (DWORD)destSD->Sacl), Sacl);
        }
        else
        {
            destSD->Sacl = RtlAllocateHeap(GetProcessHeap(), 0, length);
            copy_acl(length, destSD->Sacl, Sacl);
        }
    }

    return STATUS_SUCCESS;
}

/**************************************************************************
 * RtlValidSecurityDescriptor			[NTDLL.@]
 *
 * Determine if a SECURITY_DESCRIPTOR is valid.
 *
 * PARAMS
 *  SecurityDescriptor [I] Descriptor to check.
 *
 * RETURNS
 *   Success: STATUS_SUCCESS.
 *   Failure: STATUS_INVALID_SECURITY_DESCR or STATUS_UNKNOWN_REVISION.
 */
NTSTATUS WINAPI RtlValidSecurityDescriptor(
	PSECURITY_DESCRIPTOR SecurityDescriptor)
{
	if ( ! SecurityDescriptor )
		return STATUS_INVALID_SECURITY_DESCR;
	if ( ((SECURITY_DESCRIPTOR*)SecurityDescriptor)->Revision != SECURITY_DESCRIPTOR_REVISION )
		return STATUS_UNKNOWN_REVISION;

	return STATUS_SUCCESS;
}

/**************************************************************************
 *  RtlLengthSecurityDescriptor			[NTDLL.@]
 */
ULONG WINAPI RtlLengthSecurityDescriptor(
	PSECURITY_DESCRIPTOR pSecurityDescriptor)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;
	ULONG offset = 0;
	ULONG Size = SECURITY_DESCRIPTOR_MIN_LENGTH;

	if ( lpsd == NULL )
		return 0;

	if ( lpsd->Control & SE_SELF_RELATIVE)
		offset = (ULONG) lpsd;

	if ( lpsd->Owner != NULL )
		Size += RtlLengthSid((PSID)((LPBYTE)lpsd->Owner + offset));

	if ( lpsd->Group != NULL )
		Size += RtlLengthSid((PSID)((LPBYTE)lpsd->Group + offset));

	if ( (lpsd->Control & SE_SACL_PRESENT) &&
	      lpsd->Sacl != NULL )
		Size += ((PACL)((LPBYTE)lpsd->Sacl + offset))->AclSize;

	if ( (lpsd->Control & SE_DACL_PRESENT) &&
	      lpsd->Dacl != NULL )
		Size += ((PACL)((LPBYTE)lpsd->Dacl + offset))->AclSize;

	return Size;
}

/******************************************************************************
 *  RtlGetDaclSecurityDescriptor		[NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlGetDaclSecurityDescriptor(
	IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
	OUT PBOOLEAN lpbDaclPresent,
	OUT PACL *pDacl,
	OUT PBOOLEAN lpbDaclDefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	TRACE("(%p,%p,%p,%p)\n",
	pSecurityDescriptor, lpbDaclPresent, pDacl, lpbDaclDefaulted);

	if (lpsd->Revision != SECURITY_DESCRIPTOR_REVISION)
	  return STATUS_UNKNOWN_REVISION ;

	if ( (*lpbDaclPresent = (SE_DACL_PRESENT & lpsd->Control) ? 1 : 0) )
	{
	  if ( SE_SELF_RELATIVE & lpsd->Control)
	    *pDacl = (PACL) ((LPBYTE)lpsd + (DWORD)lpsd->Dacl);
	  else
	    *pDacl = lpsd->Dacl;

	  *lpbDaclDefaulted = (( SE_DACL_DEFAULTED & lpsd->Control ) ? 1 : 0);
	}

	return STATUS_SUCCESS;
}

/**************************************************************************
 *  RtlSetDaclSecurityDescriptor		[NTDLL.@]
 */
NTSTATUS WINAPI RtlSetDaclSecurityDescriptor (
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	BOOLEAN daclpresent,
	PACL dacl,
	BOOLEAN dacldefaulted )
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if (lpsd->Revision!=SECURITY_DESCRIPTOR_REVISION)
		return STATUS_UNKNOWN_REVISION;
	if (lpsd->Control & SE_SELF_RELATIVE)
		return STATUS_INVALID_SECURITY_DESCR;

	if (!daclpresent)
	{	lpsd->Control &= ~SE_DACL_PRESENT;
		return TRUE;
	}

	lpsd->Control |= SE_DACL_PRESENT;
	lpsd->Dacl = dacl;

	if (dacldefaulted)
		lpsd->Control |= SE_DACL_DEFAULTED;
	else
		lpsd->Control &= ~SE_DACL_DEFAULTED;

	return STATUS_SUCCESS;
}

/******************************************************************************
 *  RtlGetSaclSecurityDescriptor		[NTDLL.@]
 *
 */
NTSTATUS WINAPI RtlGetSaclSecurityDescriptor(
	IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
	OUT PBOOLEAN lpbSaclPresent,
	OUT PACL *pSacl,
	OUT PBOOLEAN lpbSaclDefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	TRACE("(%p,%p,%p,%p)\n",
	pSecurityDescriptor, lpbSaclPresent, *pSacl, lpbSaclDefaulted);

	if (lpsd->Revision != SECURITY_DESCRIPTOR_REVISION)
	  return STATUS_UNKNOWN_REVISION;

	if ( (*lpbSaclPresent = (SE_SACL_PRESENT & lpsd->Control) ? 1 : 0) )
	{
	  if (SE_SELF_RELATIVE & lpsd->Control)
	    *pSacl = (PACL) ((LPBYTE)lpsd + (DWORD)lpsd->Sacl);
	  else
	    *pSacl = lpsd->Sacl;

	  *lpbSaclDefaulted = (( SE_SACL_DEFAULTED & lpsd->Control ) ? 1 : 0);
	}

	return STATUS_SUCCESS;
}

/**************************************************************************
 * RtlSetSaclSecurityDescriptor			[NTDLL.@]
 */
NTSTATUS WINAPI RtlSetSaclSecurityDescriptor (
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	BOOLEAN saclpresent,
	PACL sacl,
	BOOLEAN sacldefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if (lpsd->Revision!=SECURITY_DESCRIPTOR_REVISION)
		return STATUS_UNKNOWN_REVISION;
	if (lpsd->Control & SE_SELF_RELATIVE)
		return STATUS_INVALID_SECURITY_DESCR;
	if (!saclpresent) {
		lpsd->Control &= ~SE_SACL_PRESENT;
		return 0;
	}
	lpsd->Control |= SE_SACL_PRESENT;
	lpsd->Sacl = sacl;
	if (sacldefaulted)
		lpsd->Control |= SE_SACL_DEFAULTED;
	else
		lpsd->Control &= ~SE_SACL_DEFAULTED;
	return STATUS_SUCCESS;
}

/**************************************************************************
 * RtlGetOwnerSecurityDescriptor		[NTDLL.@]
 */
NTSTATUS WINAPI RtlGetOwnerSecurityDescriptor(
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	PSID *Owner,
	PBOOLEAN OwnerDefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if ( !lpsd  || !Owner || !OwnerDefaulted )
		return STATUS_INVALID_PARAMETER;

	if (lpsd->Owner != NULL)
	{
            if (lpsd->Control & SE_SELF_RELATIVE)
                *Owner = (PSID)((LPBYTE)lpsd +
                                (ULONG)lpsd->Owner);
            else
                *Owner = lpsd->Owner;

            if ( lpsd->Control & SE_OWNER_DEFAULTED )
                *OwnerDefaulted = TRUE;
            else
                *OwnerDefaulted = FALSE;
        }
	else
	    *Owner = NULL;

	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlSetOwnerSecurityDescriptor		[NTDLL.@]
 */
NTSTATUS WINAPI RtlSetOwnerSecurityDescriptor(
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	PSID owner,
	BOOLEAN ownerdefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if (lpsd->Revision!=SECURITY_DESCRIPTOR_REVISION)
		return STATUS_UNKNOWN_REVISION;
	if (lpsd->Control & SE_SELF_RELATIVE)
		return STATUS_INVALID_SECURITY_DESCR;

	lpsd->Owner = owner;
	if (ownerdefaulted)
		lpsd->Control |= SE_OWNER_DEFAULTED;
	else
		lpsd->Control &= ~SE_OWNER_DEFAULTED;
	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlSetGroupSecurityDescriptor		[NTDLL.@]
 */
NTSTATUS WINAPI RtlSetGroupSecurityDescriptor (
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	PSID group,
	BOOLEAN groupdefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if (lpsd->Revision!=SECURITY_DESCRIPTOR_REVISION)
		return STATUS_UNKNOWN_REVISION;
	if (lpsd->Control & SE_SELF_RELATIVE)
		return STATUS_INVALID_SECURITY_DESCR;

	lpsd->Group = group;
	if (groupdefaulted)
		lpsd->Control |= SE_GROUP_DEFAULTED;
	else
		lpsd->Control &= ~SE_GROUP_DEFAULTED;
	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlGetGroupSecurityDescriptor		[NTDLL.@]
 */
NTSTATUS WINAPI RtlGetGroupSecurityDescriptor(
	PSECURITY_DESCRIPTOR pSecurityDescriptor,
	PSID *Group,
	PBOOLEAN GroupDefaulted)
{
	SECURITY_DESCRIPTOR* lpsd=pSecurityDescriptor;

	if ( !lpsd || !Group || !GroupDefaulted )
		return STATUS_INVALID_PARAMETER;

	if (lpsd->Group != NULL)
	{
            if (lpsd->Control & SE_SELF_RELATIVE)
                *Group = (PSID)((LPBYTE)lpsd +
                                (ULONG)lpsd->Group);
            else
                *Group = lpsd->Group;

            if ( lpsd->Control & SE_GROUP_DEFAULTED )
                *GroupDefaulted = TRUE;
            else
                *GroupDefaulted = FALSE;
	}
	else
	    *Group = NULL;

	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlMakeSelfRelativeSD		[NTDLL.@]
 */
NTSTATUS WINAPI RtlMakeSelfRelativeSD(
	IN PSECURITY_DESCRIPTOR pAbsoluteSecurityDescriptor,
	IN PSECURITY_DESCRIPTOR pSelfRelativeSecurityDescriptor,
	IN OUT LPDWORD lpdwBufferLength)
{
    ULONG offsetRel;
    ULONG length;
    SECURITY_DESCRIPTOR* pAbs = pAbsoluteSecurityDescriptor;
    SECURITY_DESCRIPTOR* pRel = pSelfRelativeSecurityDescriptor;

    TRACE(" %p %p %p(%ld)\n", pAbs, pRel, lpdwBufferLength,
        lpdwBufferLength ? *lpdwBufferLength: -1);

    if (!lpdwBufferLength || !pAbs)
        return STATUS_INVALID_PARAMETER;

    length = RtlLengthSecurityDescriptor(pAbs);
    if (*lpdwBufferLength < length)
    {
        *lpdwBufferLength = length;
        return STATUS_BUFFER_TOO_SMALL;
    }

    if (!pRel)
        return STATUS_INVALID_PARAMETER;

    if (pAbs->Control & SE_SELF_RELATIVE)
    {
        memcpy(pRel, pAbs, length);
        return STATUS_SUCCESS;
    }

    pRel->Revision = pAbs->Revision;
    pRel->Sbz1 = pAbs->Sbz1;
    pRel->Control = pAbs->Control | SE_SELF_RELATIVE;

    offsetRel = sizeof(SECURITY_DESCRIPTOR);
    pRel->Owner = (PSID) offsetRel;
    length = RtlLengthSid(pAbs->Owner);
    memcpy((LPBYTE)pRel + offsetRel, pAbs->Owner, length);

    offsetRel += length;
    pRel->Group = (PSID) offsetRel;
    length = RtlLengthSid(pAbs->Group);
    memcpy((LPBYTE)pRel + offsetRel, pAbs->Group, length);

    if (pRel->Control & SE_SACL_PRESENT)
    {
        offsetRel += length;
        pRel->Sacl = (PACL) offsetRel;
        length = pAbs->Sacl->AclSize;
        memcpy((LPBYTE)pRel + offsetRel, pAbs->Sacl, length);
    }
    else
    {
        pRel->Sacl = NULL;
    }

    if (pRel->Control & SE_DACL_PRESENT)
    {
        offsetRel += length;
        pRel->Dacl = (PACL) offsetRel;
        length = pAbs->Dacl->AclSize;
        memcpy((LPBYTE)pRel + offsetRel, pAbs->Dacl, length);
    }
    else
    {
        pRel->Dacl = NULL;
    }

    return STATUS_SUCCESS;
}


/**************************************************************************
 *                 RtlSelfRelativeToAbsoluteSD [NTDLL.@]
 */
NTSTATUS WINAPI RtlSelfRelativeToAbsoluteSD(
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
    NTSTATUS status = STATUS_SUCCESS;
    SECURITY_DESCRIPTOR* pAbs = pAbsoluteSecurityDescriptor;
    SECURITY_DESCRIPTOR* pRel = pSelfRelativeSecurityDescriptor;

    if (!pRel ||
        !lpdwAbsoluteSecurityDescriptorSize ||
        !lpdwDaclSize ||
        !lpdwSaclSize ||
        !lpdwOwnerSize ||
        !lpdwPrimaryGroupSize ||
        ~pRel->Control & SE_SELF_RELATIVE)
        return STATUS_INVALID_PARAMETER;

    /* Confirm buffers are sufficiently large */
    if (*lpdwAbsoluteSecurityDescriptorSize < sizeof(SECURITY_DESCRIPTOR))
    {
        *lpdwAbsoluteSecurityDescriptorSize = sizeof(SECURITY_DESCRIPTOR);
        status = STATUS_BUFFER_TOO_SMALL;
    }

    if (pRel->Control & SE_DACL_PRESENT &&
        *lpdwDaclSize  < ((PACL)((LPBYTE)pRel->Dacl + (ULONG)pRel))->AclSize)
    {
        *lpdwDaclSize = ((PACL)((LPBYTE)pRel->Dacl + (ULONG)pRel))->AclSize;
        status = STATUS_BUFFER_TOO_SMALL;
    }

    if (pRel->Control & SE_SACL_PRESENT &&
        *lpdwSaclSize  < ((PACL)((LPBYTE)pRel->Sacl + (ULONG)pRel))->AclSize)
    {
        *lpdwSaclSize = ((PACL)((LPBYTE)pRel->Sacl + (ULONG)pRel))->AclSize;
        status = STATUS_BUFFER_TOO_SMALL;
    }

    if (pRel->Owner &&
        *lpdwOwnerSize < RtlLengthSid((PSID)((LPBYTE)pRel->Owner + (ULONG)pRel)))
    {
        *lpdwOwnerSize = RtlLengthSid((PSID)((LPBYTE)pRel->Owner + (ULONG)pRel));
        status = STATUS_BUFFER_TOO_SMALL;
    }

    if (pRel->Group &&
        *lpdwPrimaryGroupSize < RtlLengthSid((PSID)((LPBYTE)pRel->Group + (ULONG)pRel)))
    {
        *lpdwPrimaryGroupSize = RtlLengthSid((PSID)((LPBYTE)pRel->Group + (ULONG)pRel));
        status = STATUS_BUFFER_TOO_SMALL;
    }

    if (status != STATUS_SUCCESS)
        return status;

    /* Copy structures */
    pAbs->Revision = pRel->Revision;
    pAbs->Control = pRel->Control & ~SE_SELF_RELATIVE;

    if (pRel->Control & SE_SACL_PRESENT)
    {
        PACL pAcl = (PACL)((LPBYTE)pRel->Sacl + (ULONG)pRel);

        memcpy(pSacl, pAcl, pAcl->AclSize);
        pAbs->Sacl = pSacl;
    }

    if (pRel->Control & SE_DACL_PRESENT)
    {
        PACL pAcl = (PACL)((LPBYTE)pRel->Dacl + (ULONG)pRel);
        memcpy(pDacl, pAcl, pAcl->AclSize);
        pAbs->Dacl = pDacl;
    }

    if (pRel->Owner)
    {
        PSID psid = (PSID)((LPBYTE)pRel->Owner + (ULONG)pRel);
        memcpy(pOwner, psid, RtlLengthSid(psid));
        pAbs->Owner = pOwner;
    }

    if (pRel->Group)
    {
        PSID psid = (PSID)((LPBYTE)pRel->Group + (ULONG)pRel);
        memcpy(pPrimaryGroup, psid, RtlLengthSid(psid));
        pAbs->Group = pPrimaryGroup;
    }

    return status;
}

/******************************************************************************
 * RtlGetControlSecurityDescriptor (NTDLL.@)
 */
NTSTATUS WINAPI RtlGetControlSecurityDescriptor(
    PSECURITY_DESCRIPTOR pSecurityDescriptor,
    PSECURITY_DESCRIPTOR_CONTROL pControl,
    LPDWORD lpdwRevision)
{
    SECURITY_DESCRIPTOR *lpsd = pSecurityDescriptor;

    TRACE("(%p,%p,%p)\n",pSecurityDescriptor,pControl,lpdwRevision);

    *lpdwRevision = lpsd->Revision;

    if (*lpdwRevision != SECURITY_DESCRIPTOR_REVISION)
        return STATUS_UNKNOWN_REVISION;

    *pControl = lpsd->Control;

    return STATUS_SUCCESS;
}


/**************************************************************************
 *                 RtlAbsoluteToSelfRelativeSD [NTDLL.@]
 */
NTSTATUS WINAPI RtlAbsoluteToSelfRelativeSD(
    PSECURITY_DESCRIPTOR AbsoluteSecurityDescriptor,
    PSECURITY_DESCRIPTOR SelfRelativeSecurityDescriptor,
    PULONG BufferLength)
{
    SECURITY_DESCRIPTOR *abs = AbsoluteSecurityDescriptor;

    TRACE("%p %p %p\n", AbsoluteSecurityDescriptor,
          SelfRelativeSecurityDescriptor, BufferLength);

    if (abs->Control & SE_SELF_RELATIVE)
        return STATUS_BAD_DESCRIPTOR_FORMAT;

    return RtlMakeSelfRelativeSD(AbsoluteSecurityDescriptor, 
        SelfRelativeSecurityDescriptor, BufferLength);
}


/*
 *	access control list's
 */

/**************************************************************************
 *                 RtlCreateAcl				[NTDLL.@]
 *
 * NOTES
 *    This should return NTSTATUS
 */
NTSTATUS WINAPI RtlCreateAcl(PACL acl,DWORD size,DWORD rev)
{
	TRACE("%p 0x%08lx 0x%08lx\n", acl, size, rev);

	if (rev!=ACL_REVISION)
		return STATUS_INVALID_PARAMETER;
	if (size<sizeof(ACL))
		return STATUS_BUFFER_TOO_SMALL;
	if (size>0xFFFF)
		return STATUS_INVALID_PARAMETER;

	memset(acl,'\0',sizeof(ACL));
	acl->AclRevision	= rev;
	acl->AclSize		= size;
	acl->AceCount		= 0;
	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlFirstFreeAce			[NTDLL.@]
 * looks for the AceCount+1 ACE, and if it is still within the alloced
 * ACL, return a pointer to it
 */
BOOLEAN WINAPI RtlFirstFreeAce(
	PACL acl,
	PACE_HEADER *x)
{
	PACE_HEADER	ace;
	int		i;

	*x = 0;
	ace = (PACE_HEADER)(acl+1);
	for (i=0;i<acl->AceCount;i++) {
		if ((BYTE *)ace >= (BYTE *)acl + acl->AclSize)
			return 0;
		ace = (PACE_HEADER)(((BYTE*)ace)+ace->AceSize);
	}
	if ((BYTE *)ace >= (BYTE *)acl + acl->AclSize)
		return 0;
	*x = ace;
	return 1;
}

/**************************************************************************
 *                 RtlAddAce				[NTDLL.@]
 */
NTSTATUS WINAPI RtlAddAce(
	PACL acl,
	DWORD rev,
	DWORD xnrofaces,
	PACE_HEADER acestart,
	DWORD acelen)
{
	PACE_HEADER	ace,targetace;
	int		nrofaces;

	if (acl->AclRevision != ACL_REVISION)
		return STATUS_INVALID_PARAMETER;
	if (!RtlFirstFreeAce(acl,&targetace))
		return STATUS_INVALID_PARAMETER;
	nrofaces=0;ace=acestart;
	while (((BYTE *)ace - (BYTE *)acestart) < acelen) {
		nrofaces++;
		ace = (PACE_HEADER)(((BYTE*)ace)+ace->AceSize);
	}
	if ((BYTE *)targetace + acelen > (BYTE *)acl + acl->AclSize) /* too much aces */
		return STATUS_INVALID_PARAMETER;
	memcpy((LPBYTE)targetace,acestart,acelen);
	acl->AceCount+=nrofaces;
	return STATUS_SUCCESS;
}

/**************************************************************************
 *                 RtlDeleteAce				[NTDLL.@]
 */
NTSTATUS  WINAPI RtlDeleteAce(PACL pAcl, DWORD dwAceIndex)
{
	NTSTATUS status;
	PACE_HEADER pAce;

	status = RtlGetAce(pAcl,dwAceIndex,(LPVOID*)&pAce);

	if (STATUS_SUCCESS == status)
	{
		PACE_HEADER pcAce;
		DWORD len = 0;

		pcAce = (PACE_HEADER)(((BYTE*)pAce)+pAce->AceSize);
		for (; dwAceIndex < pAcl->AceCount; dwAceIndex++)
		{
			len += pcAce->AceSize;
			pcAce = (PACE_HEADER)(((BYTE*)pcAce) + pcAce->AceSize);
		}

		memcpy(pAce, ((BYTE*)pAce)+pAce->AceSize, len);
                pAcl->AceCount--;
	}

	TRACE("pAcl=%p dwAceIndex=%ld status=0x%08lx\n", pAcl, dwAceIndex, status);

	return status;
}

/******************************************************************************
 *  RtlAddAccessAllowedAce		[NTDLL.@]
 */
NTSTATUS WINAPI RtlAddAccessAllowedAce(
	IN OUT PACL pAcl,
	IN DWORD dwAceRevision,
	IN DWORD AccessMask,
	IN PSID pSid)
{
	return RtlAddAccessAllowedAceEx( pAcl, dwAceRevision, 0, AccessMask, pSid);
}
 
/******************************************************************************
 *  RtlAddAccessAllowedAceEx		[NTDLL.@]
 */
NTSTATUS WINAPI RtlAddAccessAllowedAceEx(
	IN OUT PACL pAcl,
	IN DWORD dwAceRevision,
	IN DWORD AceFlags,
	IN DWORD AccessMask,
	IN PSID pSid)
{
   TRACE("(%p,0x%08lx,0x%08lx,%p)\n", pAcl, dwAceRevision, AccessMask, pSid);

    return add_access_ace(pAcl, dwAceRevision, AceFlags,
                          AccessMask, pSid, ACCESS_ALLOWED_ACE_TYPE);
}

/******************************************************************************
 *  RtlAddAccessDeniedAce		[NTDLL.@]
 */
NTSTATUS WINAPI RtlAddAccessDeniedAce(
	IN OUT PACL pAcl,
	IN DWORD dwAceRevision,
	IN DWORD AccessMask,
	IN PSID pSid)
{
	return RtlAddAccessDeniedAceEx( pAcl, dwAceRevision, 0, AccessMask, pSid);
}

/******************************************************************************
 *  RtlAddAccessDeniedAceEx		[NTDLL.@]
 */
NTSTATUS WINAPI RtlAddAccessDeniedAceEx(
	IN OUT PACL pAcl,
	IN DWORD dwAceRevision,
	IN DWORD AceFlags,
	IN DWORD AccessMask,
	IN PSID pSid)
{
   TRACE("(%p,0x%08lx,0x%08lx,%p)\n", pAcl, dwAceRevision, AccessMask, pSid);

    return add_access_ace(pAcl, dwAceRevision, AceFlags,
                          AccessMask, pSid, ACCESS_DENIED_ACE_TYPE);
}

/************************************************************************** 
 *  RtlAddAuditAccessAce     [NTDLL.@] 
 */ 
NTSTATUS WINAPI RtlAddAuditAccessAce( 
    IN OUT PACL pAcl, 
    IN DWORD dwAceRevision, 
    IN DWORD dwAccessMask, 
    IN PSID pSid, 
    IN BOOL bAuditSuccess, 
    IN BOOL bAuditFailure) 
{ 
    DWORD dwAceFlags = 0;

    TRACE("(%p,%ld,%ld,%p,%u,%u)\n",pAcl,dwAceRevision,dwAccessMask,
          pSid,bAuditSuccess,bAuditFailure);

    if (bAuditSuccess)
        dwAceFlags |= SUCCESSFUL_ACCESS_ACE_FLAG;

    if (bAuditFailure)
        dwAceFlags |= FAILED_ACCESS_ACE_FLAG;

    return add_access_ace(pAcl, dwAceRevision, dwAceFlags,
                          dwAccessMask, pSid, SYSTEM_AUDIT_ACE_TYPE);
} 
 
/******************************************************************************
 *  RtlValidAcl		[NTDLL.@]
 */
BOOLEAN WINAPI RtlValidAcl(PACL pAcl)
{
        BOOLEAN ret;
	TRACE("(%p)\n", pAcl);

	__TRY
	{
		PACE_HEADER	ace;
		int		i;

                if (pAcl->AclRevision != ACL_REVISION)
                    ret = FALSE;
                else
                {
                    ace = (PACE_HEADER)(pAcl+1);
                    ret = TRUE;
                    for (i=0;i<=pAcl->AceCount;i++)
                    {
                        if ((char *)ace > (char *)pAcl + pAcl->AclSize)
                        {
                            ret = FALSE;
                            break;
                        }
                        ace = (PACE_HEADER)(((BYTE*)ace)+ace->AceSize);
                    }
                }
	}
	__EXCEPT(page_fault)
	{
		WARN("(%p): invalid pointer!\n", pAcl);
		return 0;
	}
	__ENDTRY
        return ret;
}

/******************************************************************************
 *  RtlGetAce		[NTDLL.@]
 */
NTSTATUS WINAPI RtlGetAce(PACL pAcl,DWORD dwAceIndex,LPVOID *pAce )
{
	PACE_HEADER ace;

	TRACE("(%p,%ld,%p)\n",pAcl,dwAceIndex,pAce);

	if ((dwAceIndex < 0) || (dwAceIndex > pAcl->AceCount))
		return STATUS_INVALID_PARAMETER;

	ace = (PACE_HEADER)(pAcl + 1);
	for (;dwAceIndex;dwAceIndex--)
		ace = (PACE_HEADER)(((BYTE*)ace)+ace->AceSize);

	*pAce = (LPVOID) ace;

	return STATUS_SUCCESS;
}

/*
 *	misc
 */

/******************************************************************************
 *  RtlAdjustPrivilege		[NTDLL.@]
 *
 * Enables or disables a privilege from the calling thread or process.
 *
 * PARAMS
 *  Privilege     [I] Privilege index to change.
 *  Enable        [I] If TRUE, then enable the privilege otherwise disable.
 *  CurrentThread [I] If TRUE, then enable in calling thread, otherwise process.
 *  Enabled       [O] Whether privilege was previously enabled or disabled.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: NTSTATUS code.
 *
 * SEE ALSO
 *  NtAdjustPrivilegesToken, NtOpenThreadToken, NtOpenProcessToken.
 *
 */
NTSTATUS WINAPI
RtlAdjustPrivilege(ULONG Privilege,
                   BOOLEAN Enable,
                   BOOLEAN CurrentThread,
                   PBOOLEAN Enabled)
{
    TOKEN_PRIVILEGES NewState;
    TOKEN_PRIVILEGES OldState;
    ULONG ReturnLength;
    HANDLE TokenHandle;
    NTSTATUS Status;

    TRACE("(%ld, %s, %s, %p)\n", Privilege, Enable ? "TRUE" : "FALSE",
        CurrentThread ? "TRUE" : "FALSE", Enabled);

    if (CurrentThread)
    {
        Status = NtOpenThreadToken(GetCurrentThread(),
                                   TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                   FALSE,
                                   &TokenHandle);
    }
    else
    {
        Status = NtOpenProcessToken(GetCurrentProcess(),
                                    TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                                    &TokenHandle);
    }

    if (!NT_SUCCESS(Status))
    {
        WARN("Retrieving token handle failed (Status %lx)\n", Status);
        return Status;
    }

    OldState.PrivilegeCount = 1;

    NewState.PrivilegeCount = 1;
    NewState.Privileges[0].Luid.LowPart = Privilege;
    NewState.Privileges[0].Luid.HighPart = 0;
    NewState.Privileges[0].Attributes = (Enable) ? SE_PRIVILEGE_ENABLED : 0;

    Status = NtAdjustPrivilegesToken(TokenHandle,
                                     FALSE,
                                     &NewState,
                                     sizeof(TOKEN_PRIVILEGES),
                                     &OldState,
                                     &ReturnLength);
    NtClose (TokenHandle);
    if (Status == STATUS_NOT_ALL_ASSIGNED)
    {
        TRACE("Failed to assign all privileges\n");
        return STATUS_PRIVILEGE_NOT_HELD;
    }
    if (!NT_SUCCESS(Status))
    {
        WARN("NtAdjustPrivilegesToken() failed (Status %lx)\n", Status);
        return Status;
    }

    if (OldState.PrivilegeCount == 0)
        *Enabled = Enable;
    else
        *Enabled = (OldState.Privileges[0].Attributes & SE_PRIVILEGE_ENABLED);

    return STATUS_SUCCESS;
}

/******************************************************************************
 *  RtlImpersonateSelf		[NTDLL.@]
 *
 * Makes an impersonation token that represents the process user and assigns
 * to the current thread.
 *
 * PARAMS
 *  ImpersonationLevel [I] Level at which to impersonate.
 *
 * RETURNS
 *  Success: STATUS_SUCCESS.
 *  Failure: NTSTATUS code.
 */
NTSTATUS WINAPI
RtlImpersonateSelf(SECURITY_IMPERSONATION_LEVEL ImpersonationLevel)
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    HANDLE ProcessToken;
    HANDLE ImpersonationToken;

    TRACE("(%08x)\n", ImpersonationLevel);

    Status = NtOpenProcessToken( NtCurrentProcess(), TOKEN_DUPLICATE,
                                 &ProcessToken);
    if (Status != STATUS_SUCCESS)
        return Status;

    InitializeObjectAttributes( &ObjectAttributes, NULL, 0, NULL, NULL );

    Status = NtDuplicateToken( ProcessToken,
                               TOKEN_IMPERSONATE,
                               &ObjectAttributes,
                               ImpersonationLevel,
                               TokenImpersonation,
                               &ImpersonationToken );
    if (Status != STATUS_SUCCESS)
    {
        NtClose( ProcessToken );
        return Status;
    }

    Status = NtSetInformationThread( GetCurrentThread(),
                                     ThreadImpersonationToken,
                                     &ImpersonationToken,
                                     sizeof(ImpersonationToken) );

    NtClose( ImpersonationToken );
    NtClose( ProcessToken );

    return Status;
}

/******************************************************************************
 *  NtAccessCheck		[NTDLL.@]
 *  ZwAccessCheck		[NTDLL.@]
 *
 * Checks that a user represented by a token is allowed to access an object
 * represented by a security descriptor.
 *
 * PARAMS
 *  SecurityDescriptor [I] The security descriptor of the object to check.
 *  ClientToken        [I] Token of the user accessing the object.
 *  DesiredAccess      [I] The desired access to the object.
 *  GenericMapping     [I] Mapping used to transform access rights in the SD to their specific forms.
 *  PrivilegeSet       [I/O] Privileges used during the access check.
 *  ReturnLength       [O] Number of bytes stored into PrivilegeSet.
 *  GrantedAccess      [O] The actual access rights granted.
 *  AccessStatus       [O] The status of the access check.
 *
 * RETURNS
 *  NTSTATUS code.
 *
 * NOTES
 *  DesiredAccess may be MAXIMUM_ALLOWED, in which case the function determines
 *  the maximum access rights allowed by the SD and returns them in
 *  GrantedAccess.
 *  The SecurityDescriptor must have a valid owner and groups present,
 *  otherwise the function will fail.
 */
NTSTATUS WINAPI
NtAccessCheck(
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    HANDLE ClientToken,
    ACCESS_MASK DesiredAccess,
    PGENERIC_MAPPING GenericMapping,
    PPRIVILEGE_SET PrivilegeSet,
    PULONG ReturnLength,
    PULONG GrantedAccess,
    NTSTATUS *AccessStatus)
{
    NTSTATUS status;

    TRACE("(%p, %p, %08lx, %p, %p, %p, %p, %p), stub\n",
        SecurityDescriptor, ClientToken, DesiredAccess, GenericMapping,
        PrivilegeSet, ReturnLength, GrantedAccess, AccessStatus);

    SERVER_START_REQ( access_check )
    {
        struct security_descriptor sd;
        PSID owner;
        PSID group;
        PACL sacl;
        PACL dacl;
        BOOLEAN defaulted, present;
        DWORD revision;
        SECURITY_DESCRIPTOR_CONTROL control;

        req->handle = ClientToken;
        req->desired_access = DesiredAccess;
        req->mapping_read = GenericMapping->GenericRead;
        req->mapping_write = GenericMapping->GenericWrite;
        req->mapping_execute = GenericMapping->GenericExecute;
        req->mapping_all = GenericMapping->GenericAll;

        /* marshal security descriptor */
        RtlGetControlSecurityDescriptor( SecurityDescriptor, &control, &revision );
        sd.control = control & ~SE_SELF_RELATIVE;
        RtlGetOwnerSecurityDescriptor( SecurityDescriptor, &owner, &defaulted );
        sd.owner_len = RtlLengthSid( owner );
        RtlGetGroupSecurityDescriptor( SecurityDescriptor, &group, &defaulted );
        sd.group_len = RtlLengthSid( group );
        RtlGetSaclSecurityDescriptor( SecurityDescriptor, &present, &sacl, &defaulted );
        sd.sacl_len = (present ? sacl->AclSize : 0);
        RtlGetDaclSecurityDescriptor( SecurityDescriptor, &present, &dacl, &defaulted );
        sd.dacl_len = (present ? dacl->AclSize : 0);

        wine_server_add_data( req, &sd, sizeof(sd) );
        wine_server_add_data( req, owner, sd.owner_len );
        wine_server_add_data( req, group, sd.group_len );
        wine_server_add_data( req, sacl, sd.sacl_len );
        wine_server_add_data( req, dacl, sd.dacl_len );

        wine_server_set_reply( req, &PrivilegeSet->Privilege, *ReturnLength - FIELD_OFFSET( PRIVILEGE_SET, Privilege ) );

        status = wine_server_call( req );

        *ReturnLength = FIELD_OFFSET( PRIVILEGE_SET, Privilege ) + reply->privileges_len;
        PrivilegeSet->PrivilegeCount = reply->privileges_len / sizeof(LUID_AND_ATTRIBUTES);

        if (status == STATUS_SUCCESS)
            *AccessStatus = reply->access_status;
        *GrantedAccess = reply->access_granted;
    }
    SERVER_END_REQ;

    return status;
}

/******************************************************************************
 *  NtSetSecurityObject		[NTDLL.@]
 */
NTSTATUS WINAPI
NtSetSecurityObject(
        IN HANDLE Handle,
        IN SECURITY_INFORMATION SecurityInformation,
        IN PSECURITY_DESCRIPTOR SecurityDescriptor)
{
	FIXME("%p 0x%08lx %p\n", Handle, SecurityInformation, SecurityDescriptor);
	return STATUS_SUCCESS;
}

/******************************************************************************
 * RtlConvertSidToUnicodeString (NTDLL.@)
 *
 * The returned SID is used to access the USER registry hive usually
 *
 * the native function returns something like
 * "S-1-5-21-0000000000-000000000-0000000000-500";
 */
NTSTATUS WINAPI RtlConvertSidToUnicodeString(
       PUNICODE_STRING String,
       PSID pSid,
       BOOLEAN AllocateString)
{
    static const WCHAR formatW[] = {'-','%','u',0};
    WCHAR buffer[2 + 10 + 10 + 10 * SID_MAX_SUB_AUTHORITIES];
    WCHAR *p = buffer;
    const SID *sid = (const SID *)pSid;
    DWORD i, len;

    *p++ = 'S';
    p += sprintfW( p, formatW, sid->Revision );
    p += sprintfW( p, formatW, MAKELONG( MAKEWORD( sid->IdentifierAuthority.Value[5],
                                                   sid->IdentifierAuthority.Value[4] ),
                                         MAKEWORD( sid->IdentifierAuthority.Value[3],
                                                   sid->IdentifierAuthority.Value[2] )));
    for (i = 0; i < sid->SubAuthorityCount; i++)
        p += sprintfW( p, formatW, sid->SubAuthority[i] );

    len = (p + 1 - buffer) * sizeof(WCHAR);

    String->Length = len - sizeof(WCHAR);
    if (AllocateString)
    {
        String->MaximumLength = len;
        if (!(String->Buffer = RtlAllocateHeap( GetProcessHeap(), 0, len )))
            return STATUS_NO_MEMORY;
    }
    else if (len > String->MaximumLength) return STATUS_BUFFER_OVERFLOW;

    memcpy( String->Buffer, buffer, len );
    return STATUS_SUCCESS;
}

/******************************************************************************
 * RtlQueryInformationAcl (NTDLL.@)
 */
NTSTATUS WINAPI RtlQueryInformationAcl(
    PACL pAcl,
    LPVOID pAclInformation,
    DWORD nAclInformationLength,
    ACL_INFORMATION_CLASS dwAclInformationClass)
{
    NTSTATUS status = STATUS_SUCCESS;

    TRACE("pAcl=%p pAclInfo=%p len=%ld, class=%d\n", 
        pAcl, pAclInformation, nAclInformationLength, dwAclInformationClass);

    switch (dwAclInformationClass)
    {
        case AclRevisionInformation:
        {
            PACL_REVISION_INFORMATION paclrev = (PACL_REVISION_INFORMATION) pAclInformation;

            if (nAclInformationLength < sizeof(ACL_REVISION_INFORMATION))
                status = STATUS_INVALID_PARAMETER;
            else
                paclrev->AclRevision = pAcl->AclRevision;

            break;
        }

        case AclSizeInformation:
        {
            PACL_SIZE_INFORMATION paclsize = (PACL_SIZE_INFORMATION) pAclInformation;

            if (nAclInformationLength < sizeof(ACL_SIZE_INFORMATION))
                status = STATUS_INVALID_PARAMETER;
            else
            {
                INT i;
                PACE_HEADER ace;

                paclsize->AceCount = pAcl->AceCount;

                paclsize->AclBytesInUse = 0;
		ace = (PACE_HEADER) (pAcl + 1);

                for (i = 0; i < pAcl->AceCount; i++)
                {
                    paclsize->AclBytesInUse += ace->AceSize;
		    ace = (PACE_HEADER)(((BYTE*)ace)+ace->AceSize);
                }

		if (pAcl->AclSize < paclsize->AclBytesInUse)
                {
                    WARN("Acl has %ld bytes free\n", paclsize->AclBytesFree);
                    paclsize->AclBytesFree = 0;
                    paclsize->AclBytesInUse = pAcl->AclSize;
                }
                else
                    paclsize->AclBytesFree = pAcl->AclSize - paclsize->AclBytesInUse;
            }

            break;
        }

        default:
            WARN("Unknown AclInformationClass value: %d\n", dwAclInformationClass);
            status = STATUS_INVALID_PARAMETER;
    }

    return status;
}
