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
#define LDAP_SUCCESS        0x00
#define LDAP_NOT_SUPPORTED  0x5c
#endif

#include "winldap_private.h"
#include "wldap32.h"

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);

ULONG ldap_close_extended_op( WLDAP32_LDAP *ld, ULONG msgid )
{
    TRACE( "(%p, 0x%08lx)\n", ld, msgid );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;
    return LDAP_SUCCESS;
}

ULONG ldap_extended_operationA( WLDAP32_LDAP *ld, PCHAR oid, struct WLDAP32_berval *data,
    PLDAPControlA *serverctrls, PLDAPControlA *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *oidW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p)\n", ld, debugstr_a(oid), data, serverctrls,
           clientctrls, message );

    if (!ld || !message) return WLDAP32_LDAP_PARAM_ERROR;

    if (oid) {
        oidW = strAtoW( oid );
        if (!oidW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_extended_operationW( ld, oidW, data, serverctrlsW, clientctrlsW, message );

exit:
    strfreeW( oidW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_extended_operationW( WLDAP32_LDAP *ld, PWCHAR oid, struct WLDAP32_berval *data,
    PLDAPControlW *serverctrls, PLDAPControlW *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *oidU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p)\n", ld, debugstr_w(oid), data, serverctrls,
           clientctrls, message );

    if (!ld || !message) return WLDAP32_LDAP_PARAM_ERROR;

    if (oid) {
        oidU = strWtoU( oid );
        if (!oidU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_extended_operation( ld, oid ? oidU : "", (struct berval *)data,
                                   serverctrlsU, clientctrlsU, (int *)message );

exit:
    strfreeU( oidU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}

ULONG ldap_extended_operation_sA( WLDAP32_LDAP *ld, PCHAR oid, struct WLDAP32_berval *data,
    PLDAPControlA *serverctrls, PLDAPControlA *clientctrls, PCHAR *retoid,
    struct WLDAP32_berval **retdata )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *oidW = NULL, *retoidW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p, %p)\n", ld, debugstr_a(oid), data, serverctrls,
           clientctrls, retoid, retdata );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (oid) {
        oidW = strAtoW( oid );
        if (!oidW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_extended_operation_sW( ld, oidW, data, serverctrlsW, clientctrlsW,
                                      &retoidW, retdata );

    if (retoid && retoidW) {
        *retoid = strWtoA( retoidW );
        if (!*retoid) ret = WLDAP32_LDAP_NO_MEMORY;
        ldap_memfreeW( retoidW );
    }

exit:
    strfreeW( oidW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_extended_operation_sW( WLDAP32_LDAP *ld, PWCHAR oid, struct WLDAP32_berval *data,
    PLDAPControlW *serverctrls, PLDAPControlW *clientctrls, PWCHAR *retoid,
    struct WLDAP32_berval **retdata )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *oidU = NULL, *retoidU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p, %p)\n", ld, debugstr_w(oid), data, serverctrls,
           clientctrls, retoid, retdata );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (oid) {
        oidU = strWtoU( oid );
        if (!oidU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_extended_operation_s( ld, oid ? oidU : "", (struct berval *)data, serverctrlsU,
                                     clientctrlsU, &retoidU, (struct berval **)retdata );

    if (retoid && retoidU) {
        *retoid = strUtoW( retoidU );
        if (!*retoid) ret = WLDAP32_LDAP_NO_MEMORY;
        ldap_memfree( retoidU );
    }

exit:
    strfreeU( oidU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}
