/*
 * Tokens
 *
 * Copyright (C) 1998 Alexandre Julliard
 * Copyright (C) 2003 Mike McCormack
 * Copyright (C) 2005 Robert Shearman
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "windef.h"

#include "handle.h"
#include "thread.h"
#include "process.h"
#include "request.h"
#include "security.h"

#include "wine/unicode.h"

#define MAX_SUBAUTH_COUNT 1

const LUID SeIncreaseQuotaPrivilege        = {  5, 0 };
const LUID SeSecurityPrivilege             = {  8, 0 };
const LUID SeTakeOwnershipPrivilege        = {  9, 0 };
const LUID SeLoadDriverPrivilege           = { 10, 0 };
const LUID SeSystemProfilePrivilege        = { 11, 0 };
const LUID SeSystemtimePrivilege           = { 12, 0 };
const LUID SeProfileSingleProcessPrivilege = { 13, 0 };
const LUID SeIncreaseBasePriorityPrivilege = { 14, 0 };
const LUID SeCreatePagefilePrivilege       = { 15, 0 };
const LUID SeBackupPrivilege               = { 17, 0 };
const LUID SeRestorePrivilege              = { 18, 0 };
const LUID SeShutdownPrivilege             = { 19, 0 };
const LUID SeDebugPrivilege                = { 20, 0 };
const LUID SeSystemEnvironmentPrivilege    = { 22, 0 };
const LUID SeChangeNotifyPrivilege         = { 23, 0 };
const LUID SeRemoteShutdownPrivilege       = { 24, 0 };
const LUID SeUndockPrivilege               = { 25, 0 };
const LUID SeManageVolumePrivilege         = { 28, 0 };
const LUID SeImpersonatePrivilege          = { 29, 0 };
const LUID SeCreateGlobalPrivilege         = { 30, 0 };

static const SID world_sid = { SID_REVISION, 1, { SECURITY_WORLD_SID_AUTHORITY }, { SECURITY_WORLD_RID } };
static const SID local_sid = { SID_REVISION, 1, { SECURITY_LOCAL_SID_AUTHORITY }, { SECURITY_LOCAL_RID } };
static const SID interactive_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_INTERACTIVE_RID } };
static const SID authenticated_user_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_AUTHENTICATED_USER_RID } };
static const SID local_system_sid = { SID_REVISION, 1, { SECURITY_NT_AUTHORITY }, { SECURITY_LOCAL_SYSTEM_RID } };
static const PSID security_world_sid = (PSID)&world_sid;
static const PSID security_local_sid = (PSID)&local_sid;
const PSID security_interactive_sid = (PSID)&interactive_sid;
static const PSID security_authenticated_user_sid = (PSID)&authenticated_user_sid;
static const PSID security_local_system_sid = (PSID)&local_system_sid;

struct token
{
    struct object  obj;             /* object header */
    struct list    privileges;      /* privileges available to the token */
    struct list    groups;          /* groups that the user of this token belongs to (sid_and_attributes) */
    SID           *user;            /* SID of user this token represents */
    unsigned       primary;         /* is this a primary or impersonation token? */
    ACL           *default_dacl;    /* the default DACL to assign to objects created by this user */
};

struct privilege
{
    struct list entry;
    LUID        luid;
    unsigned    enabled  : 1; /* is the privilege currently enabled? */
    unsigned    def      : 1; /* is the privilege enabled by default? */
};

struct sid_and_attributes
{
    struct list entry;
    unsigned    enabled  : 1; /* is the sid currently enabled? */
    unsigned    def      : 1; /* is the sid enabled by default? */
    unsigned    logon    : 1; /* is this a logon sid? */
    unsigned    mandatory: 1; /* is this sid always enabled? */
    unsigned    owner    : 1; /* can this sid be an owner of an object? */
    unsigned    resource : 1; /* is this a domain-local group? */
    unsigned    deny_only: 1; /* is this a sid that should be use for denying only? */
    SID         sid;
};

static void token_dump( struct object *obj, int verbose );
static void token_destroy( struct object *obj );

static const struct object_ops token_ops =
{
    sizeof(struct token),      /* size */
    token_dump,                /* dump */
    no_add_queue,              /* add_queue */
    NULL,                      /* remove_queue */
    NULL,                      /* signaled */
    NULL,                      /* satisfied */
    no_signal,                 /* signal */
    no_get_fd,                 /* get_fd */
    no_close_handle,           /* close_handle */
    token_destroy              /* destroy */
};


static void token_dump( struct object *obj, int verbose )
{
    fprintf( stderr, "Security token\n" );
    /* FIXME: dump token members */
}

static SID *security_sid_alloc( const SID_IDENTIFIER_AUTHORITY *idauthority, int subauthcount, const unsigned int subauth[] )
{
    int i;
    SID *sid = mem_alloc( FIELD_OFFSET(SID, SubAuthority[subauthcount]) );
    if (!sid) return NULL;
    sid->Revision = SID_REVISION;
    sid->SubAuthorityCount = subauthcount;
    sid->IdentifierAuthority = *idauthority;
    for (i = 0; i < subauthcount; i++)
        sid->SubAuthority[i] = subauth[i];
    return sid;
}

static inline int security_equal_sid( const SID *sid1, const SID *sid2 )
{
    return ((sid1->SubAuthorityCount == sid2->SubAuthorityCount) &&
        !memcmp( sid1, sid2, FIELD_OFFSET(SID, SubAuthority[sid1->SubAuthorityCount]) ));
}

void security_set_thread_token( struct thread *thread, obj_handle_t handle )
{
    if (!handle)
    {
        if (thread->token)
            release_object( thread->token );
        thread->token = NULL;
    }
    else
    {
        struct token *token = (struct token *)get_handle_obj( current->process,
                                                              handle,
                                                              TOKEN_IMPERSONATE,
                                                              &token_ops );
        if (token)
        {
            if (thread->token)
                release_object( thread->token );
            thread->token = token;
        }
    }
}

static const ACE_HEADER *ace_next( const ACE_HEADER *ace )
{
    return (const ACE_HEADER *)((const char *)ace + ace->AceSize);
}

static int acl_is_valid( const ACL *acl, size_t size )
{
    ULONG i;
    const ACE_HEADER *ace;

    if (size < sizeof(ACL))
        return FALSE;

    size = min(size, MAX_ACL_LEN);

    size -= sizeof(ACL);

    ace = (const ACE_HEADER *)(acl + 1);
    for (i = 0; i < acl->AceCount; i++)
    {
        const SID *sid;
        size_t sid_size;

        if (size < sizeof(ACE_HEADER))
            return FALSE;
        if (size < ace->AceSize)
            return FALSE;
        size -= ace->AceSize;
        switch (ace->AceType)
        {
        case ACCESS_DENIED_ACE_TYPE:
            sid = (const SID *)&((const ACCESS_DENIED_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(ACCESS_DENIED_ACE, SidStart);
            break;
        case ACCESS_ALLOWED_ACE_TYPE:
            sid = (const SID *)&((const ACCESS_ALLOWED_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart);
            break;
        case SYSTEM_AUDIT_ACE_TYPE:
            sid = (const SID *)&((const SYSTEM_AUDIT_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(SYSTEM_AUDIT_ACE, SidStart);
            break;
        case SYSTEM_ALARM_ACE_TYPE:
            sid = (const SID *)&((const SYSTEM_ALARM_ACE *)ace)->SidStart;
            sid_size = ace->AceSize - FIELD_OFFSET(SYSTEM_ALARM_ACE, SidStart);
            break;
        default:
            return FALSE;
        }
        if (sid_size < FIELD_OFFSET(SID, SubAuthority[0]) ||
            sid_size < FIELD_OFFSET(SID, SubAuthority[sid->SubAuthorityCount]))
            return FALSE;
        ace = ace_next( ace );
    }
    return TRUE;
}

/* gets the discretionary access control list from a security descriptor */
static inline const ACL *sd_get_dacl( const struct security_descriptor *sd, int *present )
{
    *present = (sd->control & SE_DACL_PRESENT ? TRUE : FALSE);

    if (sd->dacl_len)
        return (const ACL *)((const char *)(sd + 1) +
            sd->owner_len + sd->group_len + sd->sacl_len);
    else
        return NULL;
}

/* gets the system access control list from a security descriptor */
static inline const ACL *sd_get_sacl( const struct security_descriptor *sd, int *present )
{
    *present = (sd->control & SE_SACL_PRESENT ? TRUE : FALSE);

    if (sd->sacl_len)
        return (const ACL *)((const char *)(sd + 1) +
            sd->owner_len + sd->group_len);
    else
        return NULL;
}

/* gets the owner from a security descriptor */
static inline const SID *sd_get_owner( const struct security_descriptor *sd )
{
    if (sd->owner_len)
        return (const SID *)(sd + 1);
    else
        return NULL;
}

/* gets the primary group from a security descriptor */
static inline const SID *sd_get_group( const struct security_descriptor *sd )
{
    if (sd->group_len)
        return (const SID *)((const char *)(sd + 1) + sd->owner_len);
    else
        return NULL;
}

/* checks whether all members of a security descriptor fit inside the size
 * of memory specified */
static int sd_is_valid( const struct security_descriptor *sd, size_t size )
{
    size_t offset = sizeof(struct security_descriptor);
    const SID *group;
    const SID *owner;
    const ACL *sacl;
    const ACL *dacl;
    int dummy;

    if (size < offset)
        return FALSE;

    if ((sd->owner_len >= FIELD_OFFSET(SID, SubAuthority[255])) ||
        (offset + sd->owner_len > size))
        return FALSE;
    owner = sd_get_owner( sd );
    if (owner)
    {
        size_t needed_size = FIELD_OFFSET(SID, SubAuthority[owner->SubAuthorityCount]);
        if ((sd->owner_len < sizeof(SID)) || (needed_size > sd->owner_len))
            return FALSE;
    }
    offset += sd->owner_len;

    if ((sd->group_len >= FIELD_OFFSET(SID, SubAuthority[255])) ||
        (offset + sd->group_len > size))
        return FALSE;
    group = sd_get_group( sd );
    if (group)
    {
        size_t needed_size = FIELD_OFFSET(SID, SubAuthority[group->SubAuthorityCount]);
        if ((sd->owner_len < sizeof(SID)) || (needed_size > sd->owner_len))
            return FALSE;
    }
    offset += sd->group_len;

    if ((sd->sacl_len >= MAX_ACL_LEN) || (offset + sd->sacl_len > size))
        return FALSE;
    sacl = sd_get_sacl( sd, &dummy );
    if (sacl && !acl_is_valid( sacl, sd->sacl_len ))
        return FALSE;
    offset += sd->sacl_len;

    if ((sd->dacl_len >= MAX_ACL_LEN) || (offset + sd->dacl_len > size))
        return FALSE;
    dacl = sd_get_dacl( sd, &dummy );
    if (dacl && !acl_is_valid( dacl, sd->dacl_len ))
        return FALSE;
    offset += sd->dacl_len;

    return TRUE;
}

/* maps from generic rights to specific rights as given by a mapping */
static inline void map_generic_mask(unsigned int *mask, const GENERIC_MAPPING *mapping)
{
    if (*mask & GENERIC_READ) *mask |= mapping->GenericRead;
    if (*mask & GENERIC_WRITE) *mask |= mapping->GenericWrite;
    if (*mask & GENERIC_EXECUTE) *mask |= mapping->GenericExecute;
    if (*mask & GENERIC_ALL) *mask |= mapping->GenericAll;
    *mask &= 0x0FFFFFFF;
}

static inline int is_equal_luid( const LUID *luid1, const LUID *luid2 )
{
    return (luid1->LowPart == luid2->LowPart && luid1->HighPart == luid2->HighPart);
}

static inline void luid_and_attr_from_privilege( LUID_AND_ATTRIBUTES *out, const struct privilege *in)
{
    out->Luid = in->luid;
    out->Attributes =
        (in->enabled ? SE_PRIVILEGE_ENABLED : 0) |
        (in->def ? SE_PRIVILEGE_ENABLED_BY_DEFAULT : 0);
}

static struct privilege *privilege_add( struct token *token, const LUID *luid, int enabled )
{
    struct privilege *privilege = mem_alloc( sizeof(*privilege) );
    if (privilege)
    {
        privilege->luid = *luid;
        privilege->def = privilege->enabled = (enabled != 0);
        list_add_tail( &token->privileges, &privilege->entry );
    }
    return privilege;
}

static inline void privilege_remove( struct privilege *privilege )
{
    list_remove( &privilege->entry );
    free( privilege );
}

static void token_destroy( struct object *obj )
{
    struct token* token;
    struct list *cursor, *cursor_next;

    assert( obj->ops == &token_ops );
    token = (struct token *)obj;

    free( token->user );

    LIST_FOR_EACH_SAFE( cursor, cursor_next, &token->privileges )
    {
        struct privilege *privilege = LIST_ENTRY( cursor, struct privilege, entry );
        privilege_remove( privilege );
    }

    LIST_FOR_EACH_SAFE( cursor, cursor_next, &token->groups )
    {
        struct sid_and_attributes *group = LIST_ENTRY( cursor, struct sid_and_attributes, entry );
        list_remove( &group->entry );
        free( group );
    }

    free( token->default_dacl );
}

/* creates a new token.
 *  groups may be NULL if group_count is 0.
 *  privs may be NULL if priv_count is 0.
 *  default_dacl may be NULL, indicating that all objects created by the user
 *   are unsecured.
 */
static struct token *create_token( unsigned primary, const SID *user,
                                   const SID_AND_ATTRIBUTES *groups, unsigned int group_count,
                                   const LUID_AND_ATTRIBUTES *privs, unsigned int priv_count,
                                   const ACL *default_dacl )
{
    struct token *token = alloc_object( &token_ops );
    if (token)
    {
        int i;

        list_init( &token->privileges );
        list_init( &token->groups );
        token->primary = primary;

        /* copy user */
        token->user = memdup( user, FIELD_OFFSET(SID, SubAuthority[user->SubAuthorityCount]) );
        if (!token->user)
        {
            release_object( token );
            return NULL;
        }

        /* copy groups */
        for (i = 0; i < group_count; i++)
        {
            size_t size = FIELD_OFFSET( struct sid_and_attributes, sid.SubAuthority[((const SID *)groups[i].Sid)->SubAuthorityCount] );
            struct sid_and_attributes *group = mem_alloc( size );
            memcpy( &group->sid, groups[i].Sid, FIELD_OFFSET( SID, SubAuthority[((const SID *)groups[i].Sid)->SubAuthorityCount] ) );
            group->enabled = TRUE;
            group->def = TRUE;
            group->logon = FALSE;
            group->mandatory = (groups[i].Attributes & SE_GROUP_MANDATORY) ? TRUE : FALSE;
            group->owner = FALSE;
            group->resource = FALSE;
            group->deny_only = FALSE;
            list_add_tail( &token->groups, &group->entry );
        }

        /* copy privileges */
        for (i = 0; i < priv_count; i++)
        {
            /* note: we don't check uniqueness: the caller must make sure
             * privs doesn't contain any duplicate luids */
            if (!privilege_add( token, &privs[i].Luid,
                                privs[i].Attributes & SE_PRIVILEGE_ENABLED ))
            {
                release_object( token );
                return NULL;
            }
        }

        if (default_dacl)
        {
            token->default_dacl = memdup( default_dacl, default_dacl->AclSize );
            if (!token->default_dacl)
            {
                release_object( token );
                return NULL;
            }
        }
        else
            token->default_dacl = NULL;
    }
    return token;
}

static ACL *create_default_dacl( const SID *user )
{
    ACCESS_ALLOWED_ACE *aaa;
    ACL *default_dacl;
    SID *sid;
    size_t default_dacl_size = sizeof(ACL) +
                               2*(sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                               sizeof(local_system_sid) +
                               FIELD_OFFSET(SID, SubAuthority[user->SubAuthorityCount]);

    default_dacl = mem_alloc( default_dacl_size );
    if (!default_dacl) return NULL;

    default_dacl->AclRevision = MAX_ACL_REVISION;
    default_dacl->Sbz1 = 0;
    default_dacl->AclSize = default_dacl_size;
    default_dacl->AceCount = 2;
    default_dacl->Sbz2 = 0;

    /* GENERIC_ALL for Local System */
    aaa = (ACCESS_ALLOWED_ACE *)(default_dacl + 1);
    aaa->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    aaa->Header.AceFlags = 0;
    aaa->Header.AceSize = (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                          sizeof(local_system_sid);
    aaa->Mask = GENERIC_ALL;
    sid = (SID *)&aaa->SidStart;
    memcpy( sid, &local_system_sid, sizeof(local_system_sid) );

    /* GENERIC_ALL for specified user */
    aaa = (ACCESS_ALLOWED_ACE *)((const char *)aaa + aaa->Header.AceSize);
    aaa->Header.AceType = ACCESS_ALLOWED_ACE_TYPE;
    aaa->Header.AceFlags = 0;
    aaa->Header.AceSize = (sizeof(ACCESS_ALLOWED_ACE) - sizeof(DWORD)) +
                          FIELD_OFFSET( SID, SubAuthority[user->SubAuthorityCount] );
    aaa->Mask = GENERIC_ALL;
    sid = (SID *)&aaa->SidStart;
    memcpy( sid, user, FIELD_OFFSET(SID, SubAuthority[user->SubAuthorityCount]) );

    return default_dacl;
}

struct sid_data
{
    SID_IDENTIFIER_AUTHORITY idauth;
    int count;
    unsigned int subauth[MAX_SUBAUTH_COUNT];
};

struct token *token_create_admin( void )
{
    struct token *token = NULL;
    static const SID_IDENTIFIER_AUTHORITY nt_authority = { SECURITY_NT_AUTHORITY };
    static const unsigned int alias_admins_subauth[] = { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS };
    static const unsigned int alias_users_subauth[] = { SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_USERS };
    PSID alias_admins_sid;
    PSID alias_users_sid;
    ACL *default_dacl = create_default_dacl( &local_system_sid );

    alias_admins_sid = security_sid_alloc( &nt_authority, sizeof(alias_admins_subauth)/sizeof(alias_admins_subauth[0]),
                                           alias_admins_subauth );
    alias_users_sid = security_sid_alloc( &nt_authority, sizeof(alias_users_subauth)/sizeof(alias_users_subauth[0]),
                                          alias_users_subauth );

    if (alias_admins_sid && alias_users_sid && default_dacl)
    {
        const LUID_AND_ATTRIBUTES admin_privs[] =
        {
            { SeChangeNotifyPrivilege        , SE_PRIVILEGE_ENABLED },
            { SeSecurityPrivilege            , 0                    },
            { SeBackupPrivilege              , 0                    },
            { SeRestorePrivilege             , 0                    },
            { SeSystemtimePrivilege          , 0                    },
            { SeShutdownPrivilege            , 0                    },
            { SeRemoteShutdownPrivilege      , 0                    },
            { SeTakeOwnershipPrivilege       , 0                    },
            { SeDebugPrivilege               , 0                    },
            { SeSystemEnvironmentPrivilege   , 0                    },
            { SeSystemProfilePrivilege       , 0                    },
            { SeProfileSingleProcessPrivilege, 0                    },
            { SeIncreaseBasePriorityPrivilege, 0                    },
            { SeLoadDriverPrivilege          , 0                    },
            { SeCreatePagefilePrivilege      , 0                    },
            { SeIncreaseQuotaPrivilege       , 0                    },
            { SeUndockPrivilege              , 0                    },
            { SeManageVolumePrivilege        , 0                    },
            { SeImpersonatePrivilege         , SE_PRIVILEGE_ENABLED },
            { SeCreateGlobalPrivilege        , SE_PRIVILEGE_ENABLED },
        };
        /* note: we don't include non-builtin groups here for the user -
         * telling us these is the job of a client-side program */
        const SID_AND_ATTRIBUTES admin_groups[] =
        {
            { security_world_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
            { security_local_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
            { security_interactive_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
            { security_authenticated_user_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
            { alias_admins_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
            { alias_users_sid, SE_GROUP_ENABLED|SE_GROUP_ENABLED_BY_DEFAULT|SE_GROUP_MANDATORY },
        };
        /* note: we just set the user sid to be the interactive builtin sid -
         * we should really translate the UNIX user id to a sid */
        token = create_token( TRUE, &interactive_sid,
                            admin_groups, sizeof(admin_groups)/sizeof(admin_groups[0]),
                            admin_privs, sizeof(admin_privs)/sizeof(admin_privs[0]),
                            default_dacl );
    }

    if (alias_admins_sid)
        free( alias_admins_sid );
    if (alias_users_sid)
        free( alias_users_sid );
    if (default_dacl)
        free( default_dacl );

    return token;
}

static struct privilege *token_find_privilege( struct token *token, const LUID *luid, int enabled_only )
{
    struct privilege *privilege;
    LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
    {
        if (is_equal_luid( luid, &privilege->luid ))
        {
            if (enabled_only && !privilege->enabled)
                return NULL;
            return privilege;
        }
    }
    return NULL;
}

static unsigned int token_adjust_privileges( struct token *token, const LUID_AND_ATTRIBUTES *privs,
                                             unsigned int count, LUID_AND_ATTRIBUTES *mod_privs,
                                             unsigned int mod_privs_count )
{
    int i;
    unsigned int modified_count = 0;

    for (i = 0; i < count; i++)
    {
        struct privilege *privilege =
            token_find_privilege( token, &privs[i].Luid, FALSE );
        if (!privilege)
        {
            set_error( STATUS_NOT_ALL_ASSIGNED );
            continue;
        }

        if (privs[i].Attributes & SE_PRIVILEGE_REMOVE)
            privilege_remove( privilege );
        else
        {
            /* save previous state for caller */
            if (mod_privs_count)
            {
                luid_and_attr_from_privilege(mod_privs, privilege);
                mod_privs++;
                mod_privs_count--;
                modified_count++;
            }

            if (privs[i].Attributes & SE_PRIVILEGE_ENABLED)
                privilege->enabled = TRUE;
            else
                privilege->enabled = FALSE;
        }
    }
    return modified_count;
}

static void token_disable_privileges( struct token *token )
{
    struct privilege *privilege;
    LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
        privilege->enabled = FALSE;
}

int token_check_privileges( struct token *token, int all_required,
                            const LUID_AND_ATTRIBUTES *reqprivs,
                            unsigned int count, LUID_AND_ATTRIBUTES *usedprivs)
{
    int i;
    unsigned int enabled_count = 0;

    for (i = 0; i < count; i++)
    {
        struct privilege *privilege = 
            token_find_privilege( token, &reqprivs[i].Luid, TRUE );

        if (usedprivs)
            usedprivs[i] = reqprivs[i];

        if (privilege && privilege->enabled)
        {
            enabled_count++;
            if (usedprivs)
                usedprivs[i].Attributes |= SE_PRIVILEGE_USED_FOR_ACCESS;
        }
    }

    if (all_required)
        return (enabled_count == count);
    else
        return (enabled_count > 0);
}

static int token_sid_present( struct token *token, const SID *sid, int deny )
{
    struct sid_and_attributes *group;

    if (security_equal_sid( token->user, sid )) return TRUE;

    LIST_FOR_EACH_ENTRY( group, &token->groups, struct sid_and_attributes, entry )
    {
        if (!group->enabled) continue;
        if (group->deny_only && !deny) continue;

        if (security_equal_sid( &group->sid, sid )) return TRUE;
    }

    return FALSE;
}

/* checks access to a security descriptor. sd must have been validated by caller.
 * it returns STATUS_SUCCESS if access was granted to the object, or an error
 * status code if not, giving the reason. errors not relating to giving access
 * to the object are returned in the status parameter. granted_access and
 * status always have a valid value stored in them on return. */
static unsigned int token_access_check( struct token *token,
                                 const struct security_descriptor *sd,
                                 unsigned int desired_access,
                                 LUID_AND_ATTRIBUTES *privs,
                                 unsigned int *priv_count,
                                 const GENERIC_MAPPING *mapping,
                                 unsigned int *granted_access,
                                 unsigned int *status )
{
    unsigned int current_access = 0;
    unsigned int denied_access = 0;
    ULONG i;
    const ACL *dacl;
    int dacl_present;
    const ACE_HEADER *ace;
    const SID *owner;

    /* assume success, but no access rights */
    *status = STATUS_SUCCESS;
    *granted_access = 0;

    /* fail if desired_access contains generic rights */
    if (desired_access & (GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE|GENERIC_ALL))
    {
        *priv_count = 0;
        *status = STATUS_GENERIC_NOT_MAPPED;
        return STATUS_ACCESS_DENIED;
    }

    dacl = sd_get_dacl( sd, &dacl_present );
    owner = sd_get_owner( sd );
    if (!owner || !sd_get_group( sd ))
    {
        *priv_count = 0;
        *status = STATUS_INVALID_SECURITY_DESCR;
        return STATUS_ACCESS_DENIED;
    }

    /* 1: Grant desired access if the object is unprotected */
    if (!dacl_present)
    {
        *priv_count = 0;
        *granted_access = desired_access;
        return STATUS_SUCCESS;
    }
    if (!dacl)
    {
        *priv_count = 0;
        return STATUS_ACCESS_DENIED;
    }

    /* 2: Check if caller wants access to system security part. Note: access
     * is only granted if specifically asked for */
    if (desired_access & ACCESS_SYSTEM_SECURITY)
    {
        const LUID_AND_ATTRIBUTES security_priv = { SeSecurityPrivilege, 0 };
        LUID_AND_ATTRIBUTES retpriv = security_priv;
        if (token_check_privileges( token, TRUE, &security_priv, 1, &retpriv ))
        {
            if (priv_count)
            {
                /* assumes that there will only be one privilege to return */
                if (*priv_count >= 1)
                {
                    *priv_count = 1;
                    *privs = retpriv;
                }
                else
                {
                    *priv_count = 1;
                    return STATUS_BUFFER_TOO_SMALL;
                }
            }
            current_access |= ACCESS_SYSTEM_SECURITY;
            if (desired_access == current_access)
            {
                *granted_access = current_access;
                return STATUS_SUCCESS;
            }
        }
        else
        {
            *priv_count = 0;
            return STATUS_PRIVILEGE_NOT_HELD;
        }
    }
    else if (priv_count) *priv_count = 0;

    /* 3: Check whether the token is the owner */
    /* NOTE: SeTakeOwnershipPrivilege is not checked for here - it is instead
     * checked when a "set owner" call is made, overriding the access rights
     * determined here. */
    if (token_sid_present( token, owner, FALSE ))
    {
        current_access |= (READ_CONTROL | WRITE_DAC);
        if (desired_access == current_access)
        {
            *granted_access = current_access;
            return STATUS_SUCCESS;
        }
    }

    /* 4: Grant rights according to the DACL */
    ace = (const ACE_HEADER *)(dacl + 1);
    for (i = 0; i < dacl->AceCount; i++)
    {
        const ACCESS_ALLOWED_ACE *aa_ace;
        const ACCESS_DENIED_ACE *ad_ace;
        const SID *sid;
        switch (ace->AceType)
        {
        case ACCESS_DENIED_ACE_TYPE:
            ad_ace = (const ACCESS_DENIED_ACE *)ace;
            sid = (const SID *)&ad_ace->SidStart;
            if (token_sid_present( token, sid, TRUE ))
            {
                unsigned int access = ad_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    denied_access |= access;
                else
                {
                    denied_access |= (access & ~current_access);
                    if (desired_access & access)
                    {
                        *granted_access = 0;
                        return STATUS_SUCCESS;
                    }
                }
            }
            break;
        case ACCESS_ALLOWED_ACE_TYPE:
            aa_ace = (const ACCESS_ALLOWED_ACE *)ace;
            sid = (const SID *)&aa_ace->SidStart;
            if (token_sid_present( token, sid, FALSE ))
            {
                unsigned int access = aa_ace->Mask;
                map_generic_mask(&access, mapping);
                if (desired_access & MAXIMUM_ALLOWED)
                    current_access |= access;
                else
                    current_access |= (access & ~denied_access);
            }
            break;
        }

        /* don't bother carrying on checking if we've already got all of
            * rights we need */
        if (desired_access == *granted_access)
            break;

        ace = ace_next( ace );
    }

    if (desired_access & MAXIMUM_ALLOWED)
    {
        *granted_access = current_access & ~denied_access;
        if (*granted_access)
            return STATUS_SUCCESS;
        else
            return STATUS_ACCESS_DENIED;
    }
    else
    {
        if ((current_access & desired_access) == desired_access)
        {
            *granted_access = current_access & desired_access;
            return STATUS_SUCCESS;
        }
        else
            return STATUS_ACCESS_DENIED;
    }
}

const ACL *token_get_default_dacl( struct token *token )
{
    return token->default_dacl;
}

/* open a security token */
DECL_HANDLER(open_token)
{
    if (req->flags & OPEN_TOKEN_THREAD)
    {
        struct thread *thread = get_thread_from_handle( req->handle, 0 );
        if (thread)
        {
            if (thread->token)
                reply->token = alloc_handle( current->process, thread->token, TOKEN_ALL_ACCESS, 0);
            else
                set_error(STATUS_NO_TOKEN);
            release_object( thread );
        }
    }
    else
    {
        struct process *process = get_process_from_handle( req->handle, 0 );
        if (process)
        {
            if (process->token)
                reply->token = alloc_handle( current->process, process->token, TOKEN_ALL_ACCESS, 0);
            else
                set_error(STATUS_NO_TOKEN);
            release_object( process );
        }
    }
}

/* adjust the privileges held by a token */
DECL_HANDLER(adjust_token_privileges)
{
    struct token *token;
    unsigned int access = TOKEN_ADJUST_PRIVILEGES;

    if (req->get_modified_state) access |= TOKEN_QUERY;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 access, &token_ops )))
    {
        const LUID_AND_ATTRIBUTES *privs = get_req_data();
        LUID_AND_ATTRIBUTES *modified_privs = NULL;
        unsigned int priv_count = get_req_data_size() / sizeof(LUID_AND_ATTRIBUTES);
        unsigned int modified_priv_count = 0;

        if (req->get_modified_state && !req->disable_all)
        {
            int i;
            /* count modified privs */
            for (i = 0; i < priv_count; i++)
            {
                struct privilege *privilege =
                    token_find_privilege( token, &privs[i].Luid, FALSE );
                if (privilege && req->get_modified_state)
                    modified_priv_count++;
            }
            reply->len = modified_priv_count;
            modified_priv_count = min( modified_priv_count, get_reply_max_size() / sizeof(*modified_privs) );
            if (modified_priv_count)
                modified_privs = set_reply_data_size( modified_priv_count * sizeof(*modified_privs) );
        }
        reply->len = modified_priv_count * sizeof(*modified_privs);

        if (req->disable_all)
            token_disable_privileges( token );
        else
            modified_priv_count = token_adjust_privileges( token, privs,
                priv_count, modified_privs, modified_priv_count );

        release_object( token );
    }
}

/* retrieves the list of privileges that may be held be the token */
DECL_HANDLER(get_token_privileges)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        int priv_count = 0;
        LUID_AND_ATTRIBUTES *privs;
        struct privilege *privilege;

        LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
            priv_count++;

        reply->len = priv_count * sizeof(*privs);
        if (reply->len <= get_reply_max_size())
        {
            privs = set_reply_data_size( priv_count * sizeof(*privs) );
            if (privs)
            {
                int i = 0;
                LIST_FOR_EACH_ENTRY( privilege, &token->privileges, struct privilege, entry )
                {
                    luid_and_attr_from_privilege( &privs[i], privilege );
                    i++;
                }
            }
        }
        else
            set_error(STATUS_BUFFER_TOO_SMALL);

        release_object( token );
    }
}

/* creates a duplicate of the token */
DECL_HANDLER(duplicate_token)
{
    struct token *src_token;
    if ((src_token = (struct token *)get_handle_obj( current->process, req->handle,
                                                     TOKEN_DUPLICATE,
                                                     &token_ops )))
    {
        /* FIXME: use req->impersonation_level */
        struct token *token = create_token( req->primary, src_token->user, NULL, 0, NULL, 0, src_token->default_dacl );
        if (token)
        {
            struct privilege *privilege;
            struct sid_and_attributes *group;
            unsigned int access;

            /* copy groups */
            LIST_FOR_EACH_ENTRY( group, &src_token->groups, struct sid_and_attributes, entry )
            {
                size_t size = FIELD_OFFSET( struct sid_and_attributes, sid.SubAuthority[group->sid.SubAuthorityCount] );
                struct sid_and_attributes *newgroup = mem_alloc( size );
                memcpy( newgroup, group, size );
                list_add_tail( &token->groups, &newgroup->entry );
            }

            /* copy privileges */
            LIST_FOR_EACH_ENTRY( privilege, &src_token->privileges, struct privilege, entry )
                privilege_add( token, &privilege->luid, privilege->enabled );

            access = req->access;
            if (access & MAXIMUM_ALLOWED) access = TOKEN_ALL_ACCESS; /* FIXME: needs general solution */
            reply->new_handle = alloc_handle( current->process, token, access, req->inherit);
            release_object( token );
        }
        release_object( src_token );
    }
}

/* checks the specified privileges are held by the token */
DECL_HANDLER(check_token_privileges)
{
    struct token *token;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        unsigned int count = get_req_data_size() / sizeof(LUID_AND_ATTRIBUTES);
        if (get_reply_max_size() >= count * sizeof(LUID_AND_ATTRIBUTES))
        {
            LUID_AND_ATTRIBUTES *usedprivs = set_reply_data_size( count * sizeof(*usedprivs) );
            reply->has_privileges = token_check_privileges( token, req->all_required, get_req_data(), count, usedprivs );
        }
        else
            set_error( STATUS_BUFFER_OVERFLOW );
        release_object( token );
    }
}

/* checks that a user represented by a token is allowed to access an object
 * represented by a security descriptor */
DECL_HANDLER(access_check)
{
    size_t sd_size = get_req_data_size();
    const struct security_descriptor *sd = get_req_data();
    struct token *token;

    if (!sd_is_valid( sd, sd_size ))
    {
        set_error( STATUS_ACCESS_VIOLATION );
        return;
    }

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        GENERIC_MAPPING mapping;
        unsigned int status;
        LUID_AND_ATTRIBUTES priv;
        unsigned int priv_count = 1;

        memset(&priv, 0, sizeof(priv));

        /* only impersonation tokens may be used with this function */
        if (token->primary)
        {
            set_error( STATUS_NO_IMPERSONATION_TOKEN );
            release_object( token );
            return;
        }

        mapping.GenericRead = req->mapping_read;
        mapping.GenericWrite = req->mapping_write;
        mapping.GenericExecute = req->mapping_execute;
        mapping.GenericAll = req->mapping_all;

        reply->access_status = token_access_check(
            token, sd, req->desired_access, &priv, &priv_count, &mapping,
            &reply->access_granted, &status );

        reply->privileges_len = priv_count*sizeof(LUID_AND_ATTRIBUTES);

        if ((priv_count > 0) && (reply->privileges_len <= get_reply_max_size()))
        {
            LUID_AND_ATTRIBUTES *privs = set_reply_data_size( priv_count * sizeof(*privs) );
            memcpy( privs, &priv, sizeof(priv) );
        }

        if (status != STATUS_SUCCESS)
            set_error( status );

        release_object( token );
    }
}

/* */
DECL_HANDLER(get_token_user)
{
    struct token *token;

    reply->user_len = 0;

    if ((token = (struct token *)get_handle_obj( current->process, req->handle,
                                                 TOKEN_QUERY,
                                                 &token_ops )))
    {
        const SID *user = token->user;

        reply->user_len = FIELD_OFFSET(SID, SubAuthority[user->SubAuthorityCount]);
        if (reply->user_len <= get_reply_max_size())
        {
            SID *user_reply = set_reply_data_size( reply->user_len );
            if (user_reply)
                memcpy( user_reply, user, reply->user_len );
        }
        else set_error( STATUS_BUFFER_TOO_SMALL );

        release_object( token );
    }
}
