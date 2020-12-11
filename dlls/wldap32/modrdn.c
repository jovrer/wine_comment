/*
 * WLDAP32 - LDAP support for Wine
 *
 * Copyright 2005 Hans Leidekker
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
#include "wine/debug.h"

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winnls.h"

#ifdef HAVE_LDAP_H
#include <ldap.h>
#else
#define LDAP_NOT_SUPPORTED  0x5c
#endif

#include "winldap_private.h"
#include "wldap32.h"

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);

ULONG ldap_modrdnA( WLDAP32_LDAP *ld, PCHAR dn, PCHAR newdn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL, *newdnW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %s)\n", ld, debugstr_a(dn), debugstr_a(newdn) );

    if (!ld || !newdn) return ~0UL;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }

    newdnW = strAtoW( newdn );
    if (!newdnW) goto exit;

    ret = ldap_modrdnW( ld, dnW, newdnW );

exit:
    strfreeW( dnW );
    strfreeW( newdnW );

#endif
    return ret;
}

ULONG ldap_modrdnW( WLDAP32_LDAP *ld, PWCHAR dn, PWCHAR newdn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL, *newdnU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %s)\n", ld, debugstr_w(dn), debugstr_w(newdn) );

    if (!ld || !newdn) return ~0UL;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }

    newdnU = strWtoU( newdn );
    if (!newdnU) goto exit;

    ret = ldap_modrdn( ld, dn ? dnU : "", newdnU );

exit:
    strfreeU( dnU );
    strfreeU( newdnU );

#endif
    return ret;
}

ULONG ldap_modrdn2A( WLDAP32_LDAP *ld, PCHAR dn, PCHAR newdn, INT delete )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL, *newdnW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, 0x%02x)\n", ld, debugstr_a(dn), newdn, delete );

    if (!ld || !newdn) return ~0UL;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }

    newdnW = strAtoW( newdn );
    if (!newdnW) goto exit;

    ret = ldap_modrdn2W( ld, dnW, newdnW, delete );

exit:
    strfreeW( dnW );
    strfreeW( newdnW );

#endif
    return ret;
}

ULONG ldap_modrdn2W( WLDAP32_LDAP *ld, PWCHAR dn, PWCHAR newdn, INT delete )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL, *newdnU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, 0x%02x)\n", ld, debugstr_w(dn), newdn, delete );

    if (!ld || !newdn) return ~0UL;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }

    newdnU = strWtoU( newdn );
    if (!newdnU) goto exit;

    ret = ldap_modrdn2( ld, dn ? dnU : "", newdnU, delete );

exit:
    strfreeU( dnU );
    strfreeU( newdnU );

#endif
    return ret;
}

ULONG ldap_modrdn2_sA( WLDAP32_LDAP *ld, PCHAR dn, PCHAR newdn, INT delete )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL, *newdnW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, 0x%02x)\n", ld, debugstr_a(dn), newdn, delete );

    if (!ld || !newdn) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }

    newdnW = strAtoW( newdn );
    if (!newdnW) goto exit;

    ret = ldap_modrdn2_sW( ld, dnW, newdnW, delete );

exit:
    strfreeW( dnW );
    strfreeW( newdnW );

#endif
    return ret;
}

ULONG ldap_modrdn2_sW( WLDAP32_LDAP *ld, PWCHAR dn, PWCHAR newdn, INT delete )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL, *newdnU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, 0x%02x)\n", ld, debugstr_w(dn), newdn, delete );

    if (!ld || !newdn) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }

    newdnU = strWtoU( newdn );
    if (!newdnU) goto exit;

    ret = ldap_modrdn2_s( ld, dn ? dnU : "", newdnU, delete );

exit:
    strfreeU( dnU );
    strfreeU( newdnU );

#endif
    return ret;
}

ULONG ldap_modrdn_sA( WLDAP32_LDAP *ld, PCHAR dn, PCHAR newdn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL, *newdnW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_a(dn), newdn );

    if (!ld || !newdn) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }

    newdnW = strAtoW( newdn );
    if (!newdnW) goto exit;

    ret = ldap_modrdn_sW( ld, dnW, newdnW );

exit:
    strfreeW( dnW );
    strfreeW( newdnW );

#endif
    return ret;
}

ULONG ldap_modrdn_sW( WLDAP32_LDAP *ld, PWCHAR dn, PWCHAR newdn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL, *newdnU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_w(dn), newdn );

    if (!ld || !newdn) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }

    newdnU = strWtoU( newdn );
    if (!newdnU) goto exit;

    ret = ldap_modrdn_s( ld, dn ? dnU : "", newdnU );

exit:
    strfreeU( dnU );
    strfreeU( newdnU );

#endif
    return ret;
}
