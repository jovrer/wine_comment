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

ULONG ldap_deleteA( WLDAP32_LDAP *ld, PCHAR dn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;

    TRACE( "(%p, %s)\n", ld, debugstr_a(dn) );

    if (!ld) return ~0UL;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) return WLDAP32_LDAP_NO_MEMORY;
    }

    ret = ldap_deleteW( ld, dnW );
    strfreeW( dnW );

#endif
    return ret;
}

ULONG ldap_deleteW( WLDAP32_LDAP *ld, PWCHAR dn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;

    TRACE( "(%p, %s)\n", ld, debugstr_w(dn) );

    if (!ld) return ~0UL;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) return WLDAP32_LDAP_NO_MEMORY;
    }

    ret = ldap_delete( ld, dn ? dnU : "" );
    strfreeU( dnU );

#endif
    return ret;
}

ULONG ldap_delete_extA( WLDAP32_LDAP *ld, PCHAR dn, PLDAPControlA *serverctrls,
    PLDAPControlA *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    TRACE( "(%p, %s, %p, %p, %p)\n", ld, debugstr_a(dn), serverctrls,
           clientctrls, message );

    ret = WLDAP32_LDAP_NO_MEMORY;

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_delete_extW( ld, dnW, serverctrlsW, clientctrlsW, message );

exit:
    strfreeW( dnW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_delete_extW( WLDAP32_LDAP *ld, PWCHAR dn, PLDAPControlW *serverctrls,
    PLDAPControlW *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;
    int dummy;

    TRACE( "(%p, %s, %p, %p, %p)\n", ld, debugstr_w(dn), serverctrls,
           clientctrls, message );

    ret = WLDAP32_LDAP_NO_MEMORY;

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_delete_ext( ld, dn ? dnU : "", serverctrlsU, clientctrlsU,
                           message ? (int *)message : &dummy );

exit:
    strfreeU( dnU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}

ULONG ldap_delete_ext_sA( WLDAP32_LDAP *ld, PCHAR dn, PLDAPControlA *serverctrls,
    PLDAPControlA *clientctrls )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    TRACE( "(%p, %s, %p, %p)\n", ld, debugstr_a(dn), serverctrls,
           clientctrls );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_delete_ext_sW( ld, dnW, serverctrlsW, clientctrlsW );

exit:
    strfreeW( dnW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_delete_ext_sW( WLDAP32_LDAP *ld, PWCHAR dn, PLDAPControlW *serverctrls,
    PLDAPControlW *clientctrls )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;

    TRACE( "(%p, %s, %p, %p)\n", ld, debugstr_w(dn), serverctrls,
           clientctrls );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_delete_ext_s( ld, dn ? dnU : "", serverctrlsU, clientctrlsU );

exit:
    strfreeU( dnU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}
 
ULONG ldap_delete_sA( WLDAP32_LDAP *ld, PCHAR dn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;

    TRACE( "(%p, %s)\n", ld, debugstr_a(dn) );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) return WLDAP32_LDAP_NO_MEMORY;
    }

    ret = ldap_delete_sW( ld, dnW );
    strfreeW( dnW );

#endif
    return ret;
}

ULONG ldap_delete_sW( WLDAP32_LDAP *ld, PWCHAR dn )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;

    TRACE( "(%p, %s)\n", ld, debugstr_w(dn) );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) return WLDAP32_LDAP_NO_MEMORY;
    }

    ret = ldap_delete_s( ld, dn ? dnU : "" );
    strfreeU( dnU );

#endif
    return ret;
}
