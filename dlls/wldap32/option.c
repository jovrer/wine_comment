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

ULONG ldap_get_optionA( WLDAP32_LDAP *ld, int option, void *value )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP

    TRACE( "(%p, 0x%08x, %p)\n", ld, option, value );

    if (!ld || !value) return WLDAP32_LDAP_PARAM_ERROR;

    switch (option)
    {
    case LDAP_OPT_API_FEATURE_INFO:
    {
        LDAPAPIFeatureInfoW featureW;
        LDAPAPIFeatureInfoA *featureA = value;

        if (!featureA->ldapaif_name) return WLDAP32_LDAP_PARAM_ERROR;

        featureW.ldapaif_info_version = featureA->ldapaif_info_version;
        featureW.ldapaif_name = strAtoW( featureA->ldapaif_name );
        featureW.ldapaif_version = 0;

        if (!featureW.ldapaif_name) return WLDAP32_LDAP_NO_MEMORY;

        ret = ldap_get_optionW( ld, option, &featureW );

        featureA->ldapaif_version = featureW.ldapaif_version;
        strfreeW( featureW.ldapaif_name );
        return ret;
    }
    case LDAP_OPT_API_INFO:
    {
        LDAPAPIInfoW infoW;
        LDAPAPIInfoA *infoA = value;

        memset( &infoW, 0, sizeof(LDAPAPIInfoW) );
        infoW.ldapai_info_version = infoA->ldapai_info_version;

        ret = ldap_get_optionW( ld, option, &infoW );

        infoA->ldapai_api_version = infoW.ldapai_api_version;
        infoA->ldapai_protocol_version = infoW.ldapai_protocol_version;

        if (infoW.ldapai_extensions)
        {
            infoA->ldapai_extensions = strarrayWtoA( infoW.ldapai_extensions );
            if (!infoA->ldapai_extensions) return WLDAP32_LDAP_NO_MEMORY;
        }
        if (infoW.ldapai_vendor_name)
        {
            infoA->ldapai_vendor_name = strWtoA( infoW.ldapai_vendor_name );
            if (!infoA->ldapai_vendor_name)
            {
                ldap_value_freeW( infoW.ldapai_extensions );
                return WLDAP32_LDAP_NO_MEMORY;
            }
        }
        infoA->ldapai_vendor_version = infoW.ldapai_vendor_version;

        ldap_value_freeW( infoW.ldapai_extensions );
        ldap_memfreeW( infoW.ldapai_vendor_name );
        return ret;
    }

    case LDAP_OPT_DEREF:
    case LDAP_OPT_DESC:
    case LDAP_OPT_ERROR_NUMBER:
    case LDAP_OPT_PROTOCOL_VERSION:
    case LDAP_OPT_REFERRALS:
    case LDAP_OPT_SIZELIMIT:
    case LDAP_OPT_TIMELIMIT:
        return ldap_get_optionW( ld, option, value );

    case LDAP_OPT_CACHE_ENABLE:
    case LDAP_OPT_CACHE_FN_PTRS:
    case LDAP_OPT_CACHE_STRATEGY:
    case LDAP_OPT_IO_FN_PTRS:
    case LDAP_OPT_REBIND_ARG:
    case LDAP_OPT_REBIND_FN:
    case LDAP_OPT_RESTART:
    case LDAP_OPT_THREAD_FN_PTRS:
        return LDAP_LOCAL_ERROR;

    case LDAP_OPT_AREC_EXCLUSIVE:
    case LDAP_OPT_AUTO_RECONNECT:
    case LDAP_OPT_CLIENT_CERTIFICATE:
    case LDAP_OPT_DNSDOMAIN_NAME:
    case LDAP_OPT_ENCRYPT:
    case LDAP_OPT_ERROR_STRING:
    case LDAP_OPT_FAST_CONCURRENT_BIND:
    case LDAP_OPT_GETDSNAME_FLAGS:
    case LDAP_OPT_HOST_NAME:
    case LDAP_OPT_HOST_REACHABLE:
    case LDAP_OPT_PING_KEEP_ALIVE:
    case LDAP_OPT_PING_LIMIT:
    case LDAP_OPT_PING_WAIT_TIME:
    case LDAP_OPT_PROMPT_CREDENTIALS:
    case LDAP_OPT_REF_DEREF_CONN_PER_MSG:
    case LDAP_OPT_REFERRAL_CALLBACK:
    case LDAP_OPT_REFERRAL_HOP_LIMIT:
    case LDAP_OPT_ROOTDSE_CACHE:
    case LDAP_OPT_SASL_METHOD:
    case LDAP_OPT_SECURITY_CONTEXT:
    case LDAP_OPT_SEND_TIMEOUT:
    case LDAP_OPT_SERVER_CERTIFICATE:
    case LDAP_OPT_SERVER_ERROR:
    case LDAP_OPT_SERVER_EXT_ERROR:
    case LDAP_OPT_SIGN:
    case LDAP_OPT_SSL:
    case LDAP_OPT_SSL_INFO:
    case LDAP_OPT_SSPI_FLAGS:
    case LDAP_OPT_TCP_KEEPALIVE:
        FIXME( "Unsupported option: 0x%02x\n", option );
        return LDAP_NOT_SUPPORTED;

    default:
        FIXME( "Unknown option: 0x%02x\n", option );
        return LDAP_LOCAL_ERROR;
    }

#endif
    return ret;
}

ULONG ldap_get_optionW( WLDAP32_LDAP *ld, int option, void *value )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP

    TRACE( "(%p, 0x%08x, %p)\n", ld, option, value );

    if (!ld || !value) return WLDAP32_LDAP_PARAM_ERROR;

    switch (option)
    {
    case LDAP_OPT_API_FEATURE_INFO:
    {
        LDAPAPIFeatureInfo featureU;
        LDAPAPIFeatureInfoW *featureW = value;

        if (!featureW->ldapaif_name) return WLDAP32_LDAP_PARAM_ERROR;

        featureU.ldapaif_info_version = featureW->ldapaif_info_version;
        featureU.ldapaif_name = strWtoU( featureW->ldapaif_name );
        featureU.ldapaif_version = 0;

        if (!featureU.ldapaif_name) return WLDAP32_LDAP_NO_MEMORY;

        ret = ldap_get_option( ld, option, &featureU );

        featureW->ldapaif_version = featureU.ldapaif_version;
        strfreeU( featureU.ldapaif_name );
        return ret;
    }
    case LDAP_OPT_API_INFO:
    {
        LDAPAPIInfo infoU;
        LDAPAPIInfoW *infoW = value;

        memset( &infoU, 0, sizeof(LDAPAPIInfo) );
        infoU.ldapai_info_version = infoW->ldapai_info_version;

        ret = ldap_get_option( ld, option, &infoU );

        infoW->ldapai_api_version = infoU.ldapai_api_version;
        infoW->ldapai_protocol_version = infoU.ldapai_protocol_version;

        if (infoU.ldapai_extensions)
        {
            infoW->ldapai_extensions = strarrayUtoW( infoU.ldapai_extensions );
            if (!infoW->ldapai_extensions) return WLDAP32_LDAP_NO_MEMORY;
        }
        if (infoU.ldapai_vendor_name)
        {
            infoW->ldapai_vendor_name = strUtoW( infoU.ldapai_vendor_name );
            if (!infoW->ldapai_vendor_name)
            {
                ldap_value_free( infoU.ldapai_extensions );
                return WLDAP32_LDAP_NO_MEMORY;
            }
        }
        infoW->ldapai_vendor_version = infoU.ldapai_vendor_version;

        ldap_value_free( infoU.ldapai_extensions );
        ldap_memfree( infoU.ldapai_vendor_name );
        return ret;
    }

    case LDAP_OPT_DEREF:
    case LDAP_OPT_DESC:
    case LDAP_OPT_ERROR_NUMBER:
    case LDAP_OPT_PROTOCOL_VERSION:
    case LDAP_OPT_REFERRALS:
    case LDAP_OPT_SIZELIMIT:
    case LDAP_OPT_TIMELIMIT:
        return ldap_get_option( ld, option, value );

    case LDAP_OPT_CACHE_ENABLE:
    case LDAP_OPT_CACHE_FN_PTRS:
    case LDAP_OPT_CACHE_STRATEGY:
    case LDAP_OPT_IO_FN_PTRS:
    case LDAP_OPT_REBIND_ARG:
    case LDAP_OPT_REBIND_FN:
    case LDAP_OPT_RESTART:
    case LDAP_OPT_THREAD_FN_PTRS:
        return LDAP_LOCAL_ERROR;

    case LDAP_OPT_AREC_EXCLUSIVE:
    case LDAP_OPT_AUTO_RECONNECT:
    case LDAP_OPT_CLIENT_CERTIFICATE:
    case LDAP_OPT_DNSDOMAIN_NAME:
    case LDAP_OPT_ENCRYPT:
    case LDAP_OPT_ERROR_STRING:
    case LDAP_OPT_FAST_CONCURRENT_BIND:
    case LDAP_OPT_GETDSNAME_FLAGS:
    case LDAP_OPT_HOST_NAME:
    case LDAP_OPT_HOST_REACHABLE:
    case LDAP_OPT_PING_KEEP_ALIVE:
    case LDAP_OPT_PING_LIMIT:
    case LDAP_OPT_PING_WAIT_TIME:
    case LDAP_OPT_PROMPT_CREDENTIALS:
    case LDAP_OPT_REF_DEREF_CONN_PER_MSG:
    case LDAP_OPT_REFERRAL_CALLBACK:
    case LDAP_OPT_REFERRAL_HOP_LIMIT:
    case LDAP_OPT_ROOTDSE_CACHE:
    case LDAP_OPT_SASL_METHOD:
    case LDAP_OPT_SECURITY_CONTEXT:
    case LDAP_OPT_SEND_TIMEOUT:
    case LDAP_OPT_SERVER_CERTIFICATE:
    case LDAP_OPT_SERVER_ERROR:
    case LDAP_OPT_SERVER_EXT_ERROR:
    case LDAP_OPT_SIGN:
    case LDAP_OPT_SSL:
    case LDAP_OPT_SSL_INFO:
    case LDAP_OPT_SSPI_FLAGS:
    case LDAP_OPT_TCP_KEEPALIVE:
        FIXME( "Unsupported option: 0x%02x\n", option );
        return LDAP_NOT_SUPPORTED;

    default:
        FIXME( "Unknown option: 0x%02x\n", option );
        return LDAP_LOCAL_ERROR;
    }

#endif
    return ret;
}

ULONG ldap_set_optionA( WLDAP32_LDAP *ld, int option, void *value )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP

    TRACE( "(%p, 0x%08x, %p)\n", ld, option, value );

    if (!ld || !value) return WLDAP32_LDAP_PARAM_ERROR;

    switch (option)
    {
    case LDAP_OPT_DEREF:
    case LDAP_OPT_DESC:
    case LDAP_OPT_ERROR_NUMBER:
    case LDAP_OPT_PROTOCOL_VERSION:
    case LDAP_OPT_REFERRALS:
    case LDAP_OPT_SIZELIMIT:
    case LDAP_OPT_TIMELIMIT:
        return ldap_set_optionW( ld, option, value );

    case LDAP_OPT_CACHE_ENABLE:
    case LDAP_OPT_CACHE_FN_PTRS:
    case LDAP_OPT_CACHE_STRATEGY:
    case LDAP_OPT_IO_FN_PTRS:
    case LDAP_OPT_REBIND_ARG:
    case LDAP_OPT_REBIND_FN:
    case LDAP_OPT_RESTART:
    case LDAP_OPT_THREAD_FN_PTRS:
        return LDAP_LOCAL_ERROR;

    case LDAP_OPT_API_FEATURE_INFO:
    case LDAP_OPT_API_INFO:
        return LDAP_UNWILLING_TO_PERFORM;

    case LDAP_OPT_AREC_EXCLUSIVE:
    case LDAP_OPT_AUTO_RECONNECT:
    case LDAP_OPT_CLIENT_CERTIFICATE:
    case LDAP_OPT_DNSDOMAIN_NAME:
    case LDAP_OPT_ENCRYPT:
    case LDAP_OPT_ERROR_STRING:
    case LDAP_OPT_FAST_CONCURRENT_BIND:
    case LDAP_OPT_GETDSNAME_FLAGS:
    case LDAP_OPT_HOST_NAME:
    case LDAP_OPT_HOST_REACHABLE:
    case LDAP_OPT_PING_KEEP_ALIVE:
    case LDAP_OPT_PING_LIMIT:
    case LDAP_OPT_PING_WAIT_TIME:
    case LDAP_OPT_PROMPT_CREDENTIALS:
    case LDAP_OPT_REF_DEREF_CONN_PER_MSG:
    case LDAP_OPT_REFERRAL_CALLBACK:
    case LDAP_OPT_REFERRAL_HOP_LIMIT:
    case LDAP_OPT_ROOTDSE_CACHE:
    case LDAP_OPT_SASL_METHOD:
    case LDAP_OPT_SECURITY_CONTEXT:
    case LDAP_OPT_SEND_TIMEOUT:
    case LDAP_OPT_SERVER_CERTIFICATE:
    case LDAP_OPT_SERVER_ERROR:
    case LDAP_OPT_SERVER_EXT_ERROR:
    case LDAP_OPT_SIGN:
    case LDAP_OPT_SSL:
    case LDAP_OPT_SSL_INFO:
    case LDAP_OPT_SSPI_FLAGS:
    case LDAP_OPT_TCP_KEEPALIVE:
        FIXME( "Unsupported option: 0x%02x\n", option );
        return LDAP_NOT_SUPPORTED;

    default:
        FIXME( "Unknown option: 0x%02x\n", option );
        return LDAP_LOCAL_ERROR;
    }

#endif
    return ret;
}

ULONG ldap_set_optionW( WLDAP32_LDAP *ld, int option, void *value )
{
    ULONG ret = LDAP_NOT_SUPPORTED;
#ifdef HAVE_LDAP

    TRACE( "(%p, 0x%08x, %p)\n", ld, option, value );

    if (!ld || !value) return WLDAP32_LDAP_PARAM_ERROR;

    switch (option)
    {
    case LDAP_OPT_DEREF:
    case LDAP_OPT_DESC:
    case LDAP_OPT_ERROR_NUMBER:
    case LDAP_OPT_PROTOCOL_VERSION:
    case LDAP_OPT_REFERRALS:
    case LDAP_OPT_SIZELIMIT:
    case LDAP_OPT_TIMELIMIT:
        return ldap_set_option( ld, option, value );

    case LDAP_OPT_CACHE_ENABLE:
    case LDAP_OPT_CACHE_FN_PTRS:
    case LDAP_OPT_CACHE_STRATEGY:
    case LDAP_OPT_IO_FN_PTRS:
    case LDAP_OPT_REBIND_ARG:
    case LDAP_OPT_REBIND_FN:
    case LDAP_OPT_RESTART:
    case LDAP_OPT_THREAD_FN_PTRS:
        return LDAP_LOCAL_ERROR;

    case LDAP_OPT_API_FEATURE_INFO:
    case LDAP_OPT_API_INFO:
        return LDAP_UNWILLING_TO_PERFORM;

    case LDAP_OPT_AREC_EXCLUSIVE:
    case LDAP_OPT_AUTO_RECONNECT:
    case LDAP_OPT_CLIENT_CERTIFICATE:
    case LDAP_OPT_DNSDOMAIN_NAME:
    case LDAP_OPT_ENCRYPT:
    case LDAP_OPT_ERROR_STRING:
    case LDAP_OPT_FAST_CONCURRENT_BIND:
    case LDAP_OPT_GETDSNAME_FLAGS:
    case LDAP_OPT_HOST_NAME:
    case LDAP_OPT_HOST_REACHABLE:
    case LDAP_OPT_PING_KEEP_ALIVE:
    case LDAP_OPT_PING_LIMIT:
    case LDAP_OPT_PING_WAIT_TIME:
    case LDAP_OPT_PROMPT_CREDENTIALS:
    case LDAP_OPT_REF_DEREF_CONN_PER_MSG:
    case LDAP_OPT_REFERRAL_CALLBACK:
    case LDAP_OPT_REFERRAL_HOP_LIMIT:
    case LDAP_OPT_ROOTDSE_CACHE:
    case LDAP_OPT_SASL_METHOD:
    case LDAP_OPT_SECURITY_CONTEXT:
    case LDAP_OPT_SEND_TIMEOUT:
    case LDAP_OPT_SERVER_CERTIFICATE:
    case LDAP_OPT_SERVER_ERROR:
    case LDAP_OPT_SERVER_EXT_ERROR:
    case LDAP_OPT_SIGN:
    case LDAP_OPT_SSL:
    case LDAP_OPT_SSL_INFO:
    case LDAP_OPT_SSPI_FLAGS:
    case LDAP_OPT_TCP_KEEPALIVE:
        FIXME( "Unsupported option: 0x%02x\n", option );
        return LDAP_NOT_SUPPORTED;

    default:
        FIXME( "Unknown option: 0x%02x\n", option );
        return LDAP_LOCAL_ERROR;
    }

#endif
    return ret;
}
