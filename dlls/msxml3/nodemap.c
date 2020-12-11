/*
 *    Node map implementation
 *
 * Copyright 2005 Mike McCormack
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

#define COBJMACROS

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winnls.h"
#include "ole2.h"
#include "msxml2.h"

#include "msxml_private.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msxml);

#ifdef HAVE_LIBXML2

typedef struct _xmlnodemap
{
    const struct IXMLDOMNamedNodeMapVtbl *lpVtbl;
    LONG ref;
    IXMLDOMNode *node;
} xmlnodemap;

static inline xmlnodemap *impl_from_IXMLDOMNamedNodeMap( IXMLDOMNamedNodeMap *iface )
{
    return (xmlnodemap *)((char*)iface - FIELD_OFFSET(xmlnodemap, lpVtbl));
}

static HRESULT WINAPI xmlnodemap_QueryInterface(
    IXMLDOMNamedNodeMap *iface,
    REFIID riid, void** ppvObject )
{
    TRACE("%p %s %p\n", iface, debugstr_guid(riid), ppvObject);

    if( IsEqualGUID( riid, &IID_IXMLDOMElement ) ||
        IsEqualGUID( riid, &IID_IUnknown ) ||
        IsEqualGUID( riid, &IID_IDispatch ) ||
        IsEqualGUID( riid, &IID_IXMLDOMNode ) )
    {
        *ppvObject = iface;
    }
    else
        return E_NOINTERFACE;

    IXMLDOMElement_AddRef( iface );

    return S_OK;
}

static ULONG WINAPI xmlnodemap_AddRef(
    IXMLDOMNamedNodeMap *iface )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    return InterlockedIncrement( &This->ref );
}

static ULONG WINAPI xmlnodemap_Release(
    IXMLDOMNamedNodeMap *iface )
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    ULONG ref;

    ref = InterlockedDecrement( &This->ref );
    if ( ref == 0 )
    {
        IXMLDOMNode_Release( This->node );
        HeapFree( GetProcessHeap(), 0, This );
    }

    return ref;
}

static HRESULT WINAPI xmlnodemap_GetTypeInfoCount(
    IXMLDOMNamedNodeMap *iface,
    UINT* pctinfo )
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_GetTypeInfo(
    IXMLDOMNamedNodeMap *iface,
    UINT iTInfo, LCID lcid,
    ITypeInfo** ppTInfo )
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_GetIDsOfNames(
    IXMLDOMNamedNodeMap *iface,
    REFIID riid, LPOLESTR* rgszNames,
    UINT cNames, LCID lcid, DISPID* rgDispId )
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_Invoke(
    IXMLDOMNamedNodeMap *iface,
    DISPID dispIdMember, REFIID riid, LCID lcid,
    WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult,
    EXCEPINFO* pExcepInfo, UINT* puArgErr )
{
    FIXME("\n");
    return E_NOTIMPL;
}

xmlChar *xmlChar_from_wchar( LPWSTR str )
{
    DWORD len;
    xmlChar *xmlstr;

    len = WideCharToMultiByte( CP_UTF8, 0, str, -1, NULL, 0, NULL, NULL );
    xmlstr = (xmlChar*) HeapAlloc( GetProcessHeap(), 0, len );
    if ( xmlstr )
        WideCharToMultiByte( CP_UTF8, 0, str, -1, (LPSTR) xmlstr, len, NULL, NULL );
    return xmlstr;
}

static HRESULT WINAPI xmlnodemap_getNamedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR name,
    IXMLDOMNode** namedItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlChar *element_name;
    xmlAttrPtr attr;
    xmlNodePtr node;

    TRACE("%p %s\n", This, debugstr_w(name) );

    node = xmlNodePtr_from_domnode( This->node, 0 );
    if ( !node )
        return E_FAIL;

    element_name = xmlChar_from_wchar( name );
    attr = xmlHasNsProp( node, element_name, NULL );
    HeapFree( GetProcessHeap(), 0, element_name );

    if ( !attr )
        return E_FAIL;

    *namedItem = create_node( (xmlNodePtr) attr );

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_setNamedItem(
    IXMLDOMNamedNodeMap *iface,
    IXMLDOMNode* newItem,
    IXMLDOMNode** namedItem)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_removeNamedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR name,
    IXMLDOMNode** namedItem)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_get_item(
    IXMLDOMNamedNodeMap *iface,
    long index,
    IXMLDOMNode** listItem)
{
    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );
    xmlNodePtr node;
    xmlAttrPtr curr;
    long attrIndex;

    TRACE("%p %ld\n", This, index);

    *listItem = NULL;

    if (index < 0)
        return S_FALSE;

    node = xmlNodePtr_from_domnode( This->node, 0 );
    curr = node->properties;

    for (attrIndex = 0; attrIndex < index; attrIndex++) {
        if (curr->next == NULL)
            return S_FALSE;
        else
            curr = curr->next;
    }
    
    *listItem = create_node( (xmlNodePtr) curr );

    return S_OK;
}

static HRESULT WINAPI xmlnodemap_get_length(
    IXMLDOMNamedNodeMap *iface,
    long* listLength)
{
    xmlNodePtr node;
    xmlAttrPtr first;
    xmlAttrPtr curr;
    long attrCount;

    xmlnodemap *This = impl_from_IXMLDOMNamedNodeMap( iface );

    TRACE("%p\n", This);

    node = xmlNodePtr_from_domnode( This->node, 0 );
    if ( !node )
        return E_FAIL;

    first = node->properties;
    if (first == NULL) {
	*listLength = 0;
	return S_OK;
    }

    curr = first;
    attrCount = 1;
    while (curr->next != NULL) {
        attrCount++;
        curr = curr->next;
    }
    *listLength = attrCount;
 
    return S_OK;
}

static HRESULT WINAPI xmlnodemap_getQualifiedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR baseName,
    BSTR namespaceURI,
    IXMLDOMNode** qualifiedItem)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_removeQualifiedItem(
    IXMLDOMNamedNodeMap *iface,
    BSTR baseName,
    BSTR namespaceURI,
    IXMLDOMNode** qualifiedItem)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_nextNode(
    IXMLDOMNamedNodeMap *iface,
    IXMLDOMNode** nextItem)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap_reset(
    IXMLDOMNamedNodeMap *iface )
{
    FIXME("\n");
    return E_NOTIMPL;
}

static HRESULT WINAPI xmlnodemap__newEnum(
    IXMLDOMNamedNodeMap *iface,
    IUnknown** ppUnk)
{
    FIXME("\n");
    return E_NOTIMPL;
}

static const struct IXMLDOMNamedNodeMapVtbl xmlnodemap_vtbl =
{
    xmlnodemap_QueryInterface,
    xmlnodemap_AddRef,
    xmlnodemap_Release,
    xmlnodemap_GetTypeInfoCount,
    xmlnodemap_GetTypeInfo,
    xmlnodemap_GetIDsOfNames,
    xmlnodemap_Invoke,
    xmlnodemap_getNamedItem,
    xmlnodemap_setNamedItem,
    xmlnodemap_removeNamedItem,
    xmlnodemap_get_item,
    xmlnodemap_get_length,
    xmlnodemap_getQualifiedItem,
    xmlnodemap_removeQualifiedItem,
    xmlnodemap_nextNode,
    xmlnodemap_reset,
    xmlnodemap__newEnum,
};

IXMLDOMNamedNodeMap *create_nodemap( IXMLDOMNode *node )
{
    xmlnodemap *nodemap;

    nodemap = HeapAlloc( GetProcessHeap(), 0, sizeof *nodemap );
    if ( !nodemap )
        return NULL;

    nodemap->lpVtbl = &xmlnodemap_vtbl;
    nodemap->node = node;
    nodemap->ref = 1;

    IXMLDOMNode_AddRef( node );

    return (IXMLDOMNamedNodeMap*) &nodemap->lpVtbl;
}

#endif
