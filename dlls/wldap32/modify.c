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
static LDAPMod *nullmods[] = { NULL };
#else
#define LDAP_NOT_SUPPORTED  0x5c
#endif

#include "winldap_private.h"
#include "wldap32.h"

WINE_DEFAULT_DEBUG_CHANNEL(wldap32);

ULONG ldap_modifyA( WLDAP32_LDAP *ld, PCHAR dn, LDAPModA *mods[] )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPModW **modsW = NULL;
    
    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_a(dn), mods );

    if (!ld) return ~0UL;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (mods) {
        modsW = modarrayAtoW( mods );
        if (!modsW) goto exit;
    }

    ret = ldap_modifyW( ld, dnW, modsW );

exit:
    strfreeW( dnW );
    modarrayfreeW( modsW );

#endif
    return ret;
}

ULONG ldap_modifyW( WLDAP32_LDAP *ld, PWCHAR dn, LDAPModW *mods[] )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPMod **modsU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_w(dn), mods );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (mods) {
        modsU = modarrayWtoU( mods );
        if (!modsU) goto exit;
    }

    ret = ldap_modify( ld, dn ? dnU : "", mods ? modsU : nullmods );

exit:
    strfreeU( dnU );
    modarrayfreeU( modsU );

#endif
    return ret;
}

ULONG ldap_modify_extA( WLDAP32_LDAP *ld, PCHAR dn, LDAPModA *mods[],
    PLDAPControlA *serverctrls, PLDAPControlA *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPModW **modsW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p)\n", ld, debugstr_a(dn), mods,
           serverctrls, clientctrls, message );

    if (!ld) return ~0UL;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (mods) {
        modsW = modarrayAtoW( mods );
        if (!modsW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_modify_extW( ld, dnW, modsW, serverctrlsW, clientctrlsW, message );

exit:
    strfreeW( dnW );
    modarrayfreeW( modsW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_modify_extW( WLDAP32_LDAP *ld, PWCHAR dn, LDAPModW *mods[],
    PLDAPControlW *serverctrls, PLDAPControlW *clientctrls, ULONG *message )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPMod **modsU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;
    int dummy;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p, %p)\n", ld, debugstr_w(dn), mods,
           serverctrls, clientctrls, message );

    if (!ld) return ~0UL;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (mods) {
        modsU = modarrayWtoU( mods );
        if (!modsU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_modify_ext( ld, dn ? dnU : "", mods ? modsU : nullmods, serverctrlsU,
                           clientctrlsU, message ? (int *)message : &dummy );

exit:
    strfreeU( dnU );
    modarrayfreeU( modsU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}

ULONG ldap_modify_ext_sA( WLDAP32_LDAP *ld, PCHAR dn, LDAPModA *mods[],
    PLDAPControlA *serverctrls, PLDAPControlA *clientctrls )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPModW **modsW = NULL;
    LDAPControlW **serverctrlsW = NULL, **clientctrlsW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p)\n", ld, debugstr_a(dn), mods,
           serverctrls, clientctrls );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (mods) {
        modsW = modarrayAtoW( mods );
        if (!modsW) goto exit;
    }
    if (serverctrls) {
        serverctrlsW = controlarrayAtoW( serverctrls );
        if (!serverctrlsW) goto exit;
    }
    if (clientctrls) {
        clientctrlsW = controlarrayAtoW( clientctrls );
        if (!clientctrlsW) goto exit;
    }

    ret = ldap_modify_ext_sW( ld, dnW, modsW, serverctrlsW, clientctrlsW );

exit:
    strfreeW( dnW );
    modarrayfreeW( modsW );
    controlarrayfreeW( serverctrlsW );
    controlarrayfreeW( clientctrlsW );

#endif
    return ret;
}

ULONG ldap_modify_ext_sW( WLDAP32_LDAP *ld, PWCHAR dn, LDAPModW *mods[],
    PLDAPControlW *serverctrls, PLDAPControlW *clientctrls )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPMod **modsU = NULL;
    LDAPControl **serverctrlsU = NULL, **clientctrlsU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p, %p, %p)\n", ld, debugstr_w(dn), mods,
           serverctrls, clientctrls );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (mods) {
        modsU = modarrayWtoU( mods );
        if (!modsU) goto exit;
    }
    if (serverctrls) {
        serverctrlsU = controlarrayWtoU( serverctrls );
        if (!serverctrlsU) goto exit;
    }
    if (clientctrls) {
        clientctrlsU = controlarrayWtoU( clientctrls );
        if (!clientctrlsU) goto exit;
    }

    ret = ldap_modify_ext_s( ld, dn ? dnU : "", mods ? modsU : nullmods,
                             serverctrlsU, clientctrlsU );

exit:
    strfreeU( dnU );
    modarrayfreeU( modsU );
    controlarrayfreeU( serverctrlsU );
    controlarrayfreeU( clientctrlsU );

#endif
    return ret;
}

ULONG ldap_modify_sA( WLDAP32_LDAP *ld, PCHAR dn, LDAPModA *mods[] )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    WCHAR *dnW = NULL;
    LDAPModW **modsW = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_a(dn), mods );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnW = strAtoW( dn );
        if (!dnW) goto exit;
    }
    if (mods) {
        modsW = modarrayAtoW( mods );
        if (!modsW) goto exit;
    }

    ret = ldap_modify_sW( ld, dnW, modsW );

exit:
    strfreeW( dnW );
    modarrayfreeW( modsW );

#endif
    return ret;
}

ULONG ldap_modify_sW( WLDAP32_LDAP *ld, PWCHAR dn, LDAPModW *mods[] )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP
    char *dnU = NULL;
    LDAPMod **modsU = NULL;

    ret = WLDAP32_LDAP_NO_MEMORY;

    TRACE( "(%p, %s, %p)\n", ld, debugstr_w(dn), mods );

    if (!ld) return WLDAP32_LDAP_PARAM_ERROR;

    if (dn) {
        dnU = strWtoU( dn );
        if (!dnU) goto exit;
    }
    if (mods) {
        modsU = modarrayWtoU( mods );
        if (!modsU) goto exit;
    }

    ret = ldap_modify_s( ld, dn ? dnU : "", mods ? modsU : nullmods );

exit:
    strfreeU( dnU );
    modarrayfreeU( modsU );

#endif
    return ret;
}
