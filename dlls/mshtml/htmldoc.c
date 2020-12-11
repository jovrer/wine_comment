/*
 * Copyright 2005 Jacek Caban
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

#include <stdarg.h>
#include <stdio.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

#define HTMLDOC_THIS(iface) DEFINE_THIS(HTMLDocument, HTMLDocument2, iface)

static HRESULT WINAPI HTMLDocument_QueryInterface(IHTMLDocument2 *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = HTMLDOC_THIS(iface);

    *ppvObject = NULL;
    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown, %p)\n", This, ppvObject);
        *ppvObject = HTMLDOC(This);
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch, %p)\n", This, ppvObject);
        *ppvObject = HTMLDOC(This);
    }else if(IsEqualGUID(&IID_IHTMLDocument, riid)) {
        TRACE("(%p)->(IID_IHTMLDocument, %p)\n", This, ppvObject);
        *ppvObject = HTMLDOC(This);
    }else if(IsEqualGUID(&IID_IHTMLDocument2, riid)) {
        TRACE("(%p)->(IID_IHTMLDocument2, %p)\n", This, ppvObject);
        *ppvObject = HTMLDOC(This);
    }else if(IsEqualGUID(&IID_IPersist, riid)) {
        TRACE("(%p)->(IID_IPersist, %p)\n", This, ppvObject);
        *ppvObject = PERSIST(This);
    }else if(IsEqualGUID(&IID_IPersistMoniker, riid)) {
        TRACE("(%p)->(IID_IPersistMoniker, %p)\n", This, ppvObject);
        *ppvObject = PERSISTMON(This);
    }else if(IsEqualGUID(&IID_IPersistFile, riid)) {
        TRACE("(%p)->(IID_IPersistFile, %p)\n", This, ppvObject);
        *ppvObject = PERSISTFILE(This);
    }else if(IsEqualGUID(&IID_IMonikerProp, riid)) {
        TRACE("(%p)->(IID_IMonikerProp, %p)\n", This, ppvObject);
        *ppvObject = MONPROP(This);
    }else if(IsEqualGUID(&IID_IOleObject, riid)) {
        TRACE("(%p)->(IID_IOleObject, %p)\n", This, ppvObject);
        *ppvObject = OLEOBJ(This);
    }else if(IsEqualGUID(&IID_IOleDocument, riid)) {
        TRACE("(%p)->(IID_IOleDocument, %p)\n", This, ppvObject);
        *ppvObject = OLEDOC(This);
    }else if(IsEqualGUID(&IID_IOleDocumentView, riid)) {
        TRACE("(%p)->(IID_IOleDocumentView, %p)\n", This, ppvObject);
        *ppvObject = DOCVIEW(This);
    }else if(IsEqualGUID(&IID_IOleInPlaceActiveObject, riid)) {
        TRACE("(%p)->(IID_IOleInPlaceActiveObject, %p)\n", This, ppvObject);
        *ppvObject = ACTOBJ(This);
    }else if(IsEqualGUID(&IID_IViewObject, riid)) {
        TRACE("(%p)->(IID_IViewObject, %p)\n", This, ppvObject);
        *ppvObject = VIEWOBJ(This);
    }else if(IsEqualGUID(&IID_IViewObject2, riid)) {
        TRACE("(%p)->(IID_IViewObject2, %p)\n", This, ppvObject);
        *ppvObject = VIEWOBJ2(This);
    }else if(IsEqualGUID(&IID_IOleWindow, riid)) {
        TRACE("(%p)->(IID_IOleWindow, %p)\n", This, ppvObject);
        *ppvObject = OLEWIN(This);
    }else if(IsEqualGUID(&IID_IOleInPlaceObject, riid)) {
        TRACE("(%p)->(IID_IOleInPlaceObject, %p)\n", This, ppvObject);
        *ppvObject = INPLACEOBJ(This);
    }else if(IsEqualGUID(&IID_IOleInPlaceObjectWindowless, riid)) {
        TRACE("(%p)->(IID_IOleInPlaceObjectWindowless, %p)\n", This, ppvObject);
        *ppvObject = INPLACEWIN(This);
    }else if(IsEqualGUID(&IID_IServiceProvider, riid)) {
        TRACE("(%p)->(IID_IServiceProvider, %p)\n", This, ppvObject);
        *ppvObject = SERVPROV(This);
    }else if(IsEqualGUID(&IID_IOleCommandTarget, riid)) {
        TRACE("(%p)->(IID_IOleCommandTarget, %p)\n", This, ppvObject);
        *ppvObject = CMDTARGET(This);
    }else if(IsEqualGUID(&IID_IOleControl, riid)) {
        TRACE("(%p)->(IID_IOleControl, %p)\n", This, ppvObject);
        *ppvObject = CONTROL(This);
    }else if(IsEqualGUID(&IID_IHlinkTarget, riid)) {
        TRACE("(%p)->(IID_IHlinkTarget, %p)\n", This, ppvObject);
        *ppvObject = HLNKTARGET(This);
    }

    if(*ppvObject) {
        IHTMLDocument2_AddRef(iface);
        return S_OK;
    }

    FIXME("(%p)->(%s %p) interface not supported\n", This, debugstr_guid(riid), ppvObject);
    return E_NOINTERFACE;
}

static ULONG WINAPI HTMLDocument_AddRef(IHTMLDocument2 *iface)
{
    HTMLDocument *This = HTMLDOC_THIS(iface);
    ULONG ref = InterlockedIncrement(&This->ref);
    TRACE("(%p) ref = %lu\n", This, ref);
    return ref;
}

static ULONG WINAPI HTMLDocument_Release(IHTMLDocument2 *iface)
{
    HTMLDocument *This = HTMLDOC_THIS(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %lu\n", This, ref);

    if(!ref) {
        if(This->client)
            IOleObject_SetClientSite(OLEOBJ(This), NULL);
        if(This->in_place_active)
            IOleInPlaceObjectWindowless_InPlaceDeactivate(INPLACEWIN(This));
        if(This->ipsite)
            IOleDocumentView_SetInPlaceSite(DOCVIEW(This), NULL);
        if(This->hwnd)
            DestroyWindow(This->hwnd);
        if(This->nscontainer)
            HTMLDocument_NSContainer_Destroy(This);
        HeapFree(GetProcessHeap(), 0, This);

        UNLOCK_MODULE();
    }

    return ref;
}

static HRESULT WINAPI HTMLDocument_GetTypeInfoCount(IHTMLDocument2 *iface, UINT *pctinfo)
{
    FIXME("(%p)->(%p)\n", iface, pctinfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_GetTypeInfo(IHTMLDocument2 *iface, UINT iTInfo,
                                                LCID lcid, ITypeInfo **ppTInfo)
{
    FIXME("(%p)->(%u %lu %p)\n", iface, iTInfo, lcid, ppTInfo);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_GetIDsOfNames(IHTMLDocument2 *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    FIXME("(%p)->(%s %p %u %lu %p)\n", iface, debugstr_guid(riid), rgszNames, cNames,
                                        lcid, rgDispId);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_Invoke(IHTMLDocument2 *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    FIXME("(%p)->(%ld %s %ld %d %p %p %p %p)\n", iface, dispIdMember, debugstr_guid(riid),
            lcid, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_Script(IHTMLDocument2 *iface, IDispatch **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_all(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_body(IHTMLDocument2 *iface, IHTMLElement **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_activeElement(IHTMLDocument2 *iface, IHTMLElement **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_images(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_applets(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_links(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_forms(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_anchors(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_title(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_title(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_scripts(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_designMode(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_designMode(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_selection(IHTMLDocument2 *iface, IHTMLSelectionObject **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_readyState(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_frames(IHTMLDocument2 *iface, IHTMLFramesCollection2 **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_embeds(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_plugins(IHTMLDocument2 *iface, IHTMLElementCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_alinkColor(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_alinkColor(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_bgColor(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_bgColor(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_fgColor(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_fgColor(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_linkColor(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)->()\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_linkColor(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_vlinkColor(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_vlinkColor(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_referrer(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_location(IHTMLDocument2 *iface, IHTMLLocation **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_lastModified(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_URL(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_URL(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_domain(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_domain(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_cookie(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_cookie(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_expando(IHTMLDocument2 *iface, VARIANT_BOOL v)
{
    FIXME("(%p)->(%x)\n", iface, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_expando(IHTMLDocument2 *iface, VARIANT_BOOL *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_charset(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_charset(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_defaultCharset(IHTMLDocument2 *iface, BSTR v)
{
    FIXME("(%p)->(%s)\n", iface, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_defaultCharset(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_mimeType(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_fileSize(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_fileCreatedDate(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_fileModifiedDate(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_fileUpdatedDate(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_security(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_protocol(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_nameProp(IHTMLDocument2 *iface, BSTR *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_write(IHTMLDocument2 *iface, SAFEARRAY *psarray)
{
    FIXME("(%p)->(%p)\n", iface, psarray);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_writeln(IHTMLDocument2 *iface, SAFEARRAY *psarray)
{
    FIXME("(%p)->(%p)\n", iface, psarray);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_open(IHTMLDocument2 *iface, BSTR url, VARIANT name,
                        VARIANT features, VARIANT replace, IDispatch **pomWindowResult)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(url), pomWindowResult);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_close(IHTMLDocument2 *iface)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_clear(IHTMLDocument2 *iface)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandSupported(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandEnabled(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandState(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandIndeterm(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandText(IHTMLDocument2 *iface, BSTR cmdID,
                                                        BSTR *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_queryCommandValue(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_execCommand(IHTMLDocument2 *iface, BSTR cmdID,
                                VARIANT_BOOL showUI, VARIANT value, VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %x %p)\n", iface, debugstr_w(cmdID), showUI, pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_execCommandShowHelp(IHTMLDocument2 *iface, BSTR cmdID,
                                                        VARIANT_BOOL *pfRet)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(cmdID), pfRet);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_createElement(IHTMLDocument2 *iface, BSTR eTag,
                                                    IHTMLElement **newElem)
{
    FIXME("(%p)->(%s %p)\n", iface, debugstr_w(eTag), newElem);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onhelp(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onhelp(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onclick(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onclick(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_ondblclick(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_ondblclick(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onkeyup(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onkeyup(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onkeydown(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onkeydown(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onkeypress(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onkeypress(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onmouseup(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onmouseup(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onmousedown(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onmousedown(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onmousemove(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onmousemove(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onmouseout(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onmouseout(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onmouseover(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onmouseover(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onreadystatechange(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onreadystatechange(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onafterupdate(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onafterupdate(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onrowexit(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onrowexit(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onrowenter(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onrowenter(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_ondragstart(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_ondragstart(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onselectstart(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onselectstart(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_elementFromPoint(IHTMLDocument2 *iface, long x, long y,
                                                        IHTMLElement **elementHit)
{
    FIXME("(%p)->(%ld %ld %p)\n", iface, x, y, elementHit);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_parentWindow(IHTMLDocument2 *iface, IHTMLWindow2 **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_styleSheets(IHTMLDocument2 *iface,
                                                    IHTMLStyleSheetsCollection **p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onbeforeupdate(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onbeforeupdate(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_put_onerrorupdate(IHTMLDocument2 *iface, VARIANT v)
{
    FIXME("(%p)\n", iface);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_get_onerrorupdate(IHTMLDocument2 *iface, VARIANT *p)
{
    FIXME("(%p)->(%p)\n", iface, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_toString(IHTMLDocument2 *iface, BSTR *String)
{
    FIXME("(%p)->(%p)\n", iface, String);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLDocument_createStyleSheet(IHTMLDocument2 *iface, BSTR bstrHref,
                                            long lIndex, IHTMLStyleSheet **ppnewStyleSheet)
{
    FIXME("(%p)->(%s %ld %p)\n", iface, debugstr_w(bstrHref), lIndex, ppnewStyleSheet);
    return E_NOTIMPL;
}

static const IHTMLDocument2Vtbl HTMLDocumentVtbl = {
    HTMLDocument_QueryInterface,
    HTMLDocument_AddRef,
    HTMLDocument_Release,
    HTMLDocument_GetTypeInfoCount,
    HTMLDocument_GetTypeInfo,
    HTMLDocument_GetIDsOfNames,
    HTMLDocument_Invoke,
    HTMLDocument_get_Script,
    HTMLDocument_get_all,
    HTMLDocument_get_body,
    HTMLDocument_get_activeElement,
    HTMLDocument_get_images,
    HTMLDocument_get_applets,
    HTMLDocument_get_links,
    HTMLDocument_get_forms,
    HTMLDocument_get_anchors,
    HTMLDocument_put_title,
    HTMLDocument_get_title,
    HTMLDocument_get_scripts,
    HTMLDocument_put_designMode,
    HTMLDocument_get_designMode,
    HTMLDocument_get_selection,
    HTMLDocument_get_readyState,
    HTMLDocument_get_frames,
    HTMLDocument_get_embeds,
    HTMLDocument_get_plugins,
    HTMLDocument_put_alinkColor,
    HTMLDocument_get_alinkColor,
    HTMLDocument_put_bgColor,
    HTMLDocument_get_bgColor,
    HTMLDocument_put_fgColor,
    HTMLDocument_get_fgColor,
    HTMLDocument_put_linkColor,
    HTMLDocument_get_linkColor,
    HTMLDocument_put_vlinkColor,
    HTMLDocument_get_vlinkColor,
    HTMLDocument_get_referrer,
    HTMLDocument_get_location,
    HTMLDocument_get_lastModified,
    HTMLDocument_put_URL,
    HTMLDocument_get_URL,
    HTMLDocument_put_domain,
    HTMLDocument_get_domain,
    HTMLDocument_put_cookie,
    HTMLDocument_get_cookie,
    HTMLDocument_put_expando,
    HTMLDocument_get_expando,
    HTMLDocument_put_charset,
    HTMLDocument_get_charset,
    HTMLDocument_put_defaultCharset,
    HTMLDocument_get_defaultCharset,
    HTMLDocument_get_mimeType,
    HTMLDocument_get_fileSize,
    HTMLDocument_get_fileCreatedDate,
    HTMLDocument_get_fileModifiedDate,
    HTMLDocument_get_fileUpdatedDate,
    HTMLDocument_get_security,
    HTMLDocument_get_protocol,
    HTMLDocument_get_nameProp,
    HTMLDocument_write,
    HTMLDocument_writeln,
    HTMLDocument_open,
    HTMLDocument_close,
    HTMLDocument_clear,
    HTMLDocument_queryCommandSupported,
    HTMLDocument_queryCommandEnabled,
    HTMLDocument_queryCommandState,
    HTMLDocument_queryCommandIndeterm,
    HTMLDocument_queryCommandText,
    HTMLDocument_queryCommandValue,
    HTMLDocument_execCommand,
    HTMLDocument_execCommandShowHelp,
    HTMLDocument_createElement,
    HTMLDocument_put_onhelp,
    HTMLDocument_get_onhelp,
    HTMLDocument_put_onclick,
    HTMLDocument_get_onclick,
    HTMLDocument_put_ondblclick,
    HTMLDocument_get_ondblclick,
    HTMLDocument_put_onkeyup,
    HTMLDocument_get_onkeyup,
    HTMLDocument_put_onkeydown,
    HTMLDocument_get_onkeydown,
    HTMLDocument_put_onkeypress,
    HTMLDocument_get_onkeypress,
    HTMLDocument_put_onmouseup,
    HTMLDocument_get_onmouseup,
    HTMLDocument_put_onmousedown,
    HTMLDocument_get_onmousedown,
    HTMLDocument_put_onmousemove,
    HTMLDocument_get_onmousemove,
    HTMLDocument_put_onmouseout,
    HTMLDocument_get_onmouseout,
    HTMLDocument_put_onmouseover,
    HTMLDocument_get_onmouseover,
    HTMLDocument_put_onreadystatechange,
    HTMLDocument_get_onreadystatechange,
    HTMLDocument_put_onafterupdate,
    HTMLDocument_get_onafterupdate,
    HTMLDocument_put_onrowexit,
    HTMLDocument_get_onrowexit,
    HTMLDocument_put_onrowenter,
    HTMLDocument_get_onrowenter,
    HTMLDocument_put_ondragstart,
    HTMLDocument_get_ondragstart,
    HTMLDocument_put_onselectstart,
    HTMLDocument_get_onselectstart,
    HTMLDocument_elementFromPoint,
    HTMLDocument_get_parentWindow,
    HTMLDocument_get_styleSheets,
    HTMLDocument_put_onbeforeupdate,
    HTMLDocument_get_onbeforeupdate,
    HTMLDocument_put_onerrorupdate,
    HTMLDocument_get_onerrorupdate,
    HTMLDocument_toString,
    HTMLDocument_createStyleSheet
};

HRESULT HTMLDocument_Create(IUnknown *pUnkOuter, REFIID riid, void** ppvObject)
{
    HTMLDocument *ret;
    HRESULT hres;

    TRACE("(%p %s %p)\n", pUnkOuter, debugstr_guid(riid), ppvObject);

    ret = HeapAlloc(GetProcessHeap(), 0, sizeof(HTMLDocument));
    ret->lpHTMLDocument2Vtbl = &HTMLDocumentVtbl;
    ret->ref = 0;

    hres = IHTMLDocument_QueryInterface(HTMLDOC(ret), riid, ppvObject);
    if(FAILED(hres)) {
        HeapFree(GetProcessHeap(), 0, ret);
        return hres;
    }

    LOCK_MODULE();

    HTMLDocument_Persist_Init(ret);
    HTMLDocument_OleObj_Init(ret);
    HTMLDocument_View_Init(ret);
    HTMLDocument_Window_Init(ret);
    HTMLDocument_Service_Init(ret);
    HTMLDocument_Hlink_Init(ret);
    HTMLDocument_NSContainer_Init(ret);

    return hres;
}
