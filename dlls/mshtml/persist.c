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
#define NONAMELESSUNION
#define NONAMELESSSTRUCT

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlguid.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

struct BindStatusCallback {
    const IBindStatusCallbackVtbl *lpBindStatusCallbackVtbl;

    LONG ref;

    HTMLDocument *doc;
    IBinding *binding;
    IStream *stream;
    LPOLESTR url;
};

#define STATUSCLB_THIS(iface) DEFINE_THIS(BindStatusCallback, BindStatusCallback, iface)

static HRESULT WINAPI BindStatusCallback_QueryInterface(IBindStatusCallback *iface,
        REFIID riid, void **ppv)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);

    *ppv = NULL;
    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown, %p)\n", This, ppv);
        *ppv = STATUSCLB(This);
    }else if(IsEqualGUID(&IID_IBindStatusCallback, riid)) {
        TRACE("(%p)->(IID_IBindStatusCallback, %p)\n", This, ppv);
        *ppv = STATUSCLB(This);
    }

    if(*ppv) {
        IBindStatusCallback_AddRef(STATUSCLB(This));
        return S_OK;
    }

    TRACE("Unsupported riid = %s\n", debugstr_guid(riid));
    return E_NOINTERFACE;
}

static ULONG WINAPI BindStatusCallback_AddRef(IBindStatusCallback *iface)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    LONG ref = InterlockedIncrement(&This->ref);

    TRACE("(%p) ref = %ld\n", This, ref);

    return ref;
}

static ULONG WINAPI BindStatusCallback_Release(IBindStatusCallback *iface)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    LONG ref = InterlockedDecrement(&This->ref);

    TRACE("(%p) ref = %ld\n", This, ref);

    if(!ref) {
        if(This->doc->status_callback == This)
            This->doc->status_callback = NULL;
        IHTMLDocument2_Release(HTMLDOC(This->doc));
        if(This->stream)
            IStream_Release(This->stream);
        CoTaskMemFree(This->url);
        HeapFree(GetProcessHeap(), 0, This);
    }

    return ref;
}

static HRESULT WINAPI BindStatusCallback_OnStartBinding(IBindStatusCallback *iface,
        DWORD dwReserved, IBinding *pbind)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);

    TRACE("(%p)->(%ld %p)\n", This, dwReserved, pbind);

    This->binding = pbind;
    IBinding_AddRef(pbind);

    if(This->doc->nscontainer && This->doc->nscontainer->stream) {
        nsACString *strTextHtml;
        nsresult nsres;
        nsIURI *uri = get_nsIURI(This->url);

        strTextHtml = nsACString_Create();
        /* FIXME: Set it correctly */
        nsACString_SetData(strTextHtml, "text/html");

        nsres = nsIWebBrowserStream_OpenStream(This->doc->nscontainer->stream, uri, strTextHtml);
        if(NS_FAILED(nsres))
            ERR("OpenStream failed: %08lx\n", nsres);

        nsIURI_Release(uri);
    }

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_GetPriority(IBindStatusCallback *iface, LONG *pnPriority)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pnPriority);
    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnLowResource(IBindStatusCallback *iface, DWORD reserved)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    FIXME("(%p)->(%ld)\n", This, reserved);
    return E_NOTIMPL;
}

static HRESULT WINAPI BindStatusCallback_OnProgress(IBindStatusCallback *iface, ULONG ulProgress,
        ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    TRACE("%p)->(%lu %lu %lu %s)\n", This, ulProgress, ulProgressMax, ulStatusCode,
            debugstr_w(szStatusText));
    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnStopBinding(IBindStatusCallback *iface,
        HRESULT hresult, LPCWSTR szError)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);

    TRACE("(%p)->(%08lx %s)\n", This, hresult, debugstr_w(szError));

    if(This->doc->nscontainer && This->doc->nscontainer->stream)
        nsIWebBrowserStream_CloseStream(This->doc->nscontainer->stream);

    IBinding_Release(This->binding);
    This->binding = NULL;
    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_GetBindInfo(IBindStatusCallback *iface,
        DWORD *grfBINDF, BINDINFO *pbindinfo)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    DWORD size;

    TRACE("(%p)->(%p %p)\n", This, grfBINDF, pbindinfo);

    *grfBINDF = BINDF_ASYNCHRONOUS | BINDF_ASYNCSTORAGE | BINDF_PULLDATA;
    size = pbindinfo->cbSize;
    memset(pbindinfo, 0, size);
    pbindinfo->cbSize = size;

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnDataAvailable(IBindStatusCallback *iface,
        DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);

    TRACE("(%p)->(%08lx %ld %p %p)\n", This, grfBSCF, dwSize, pformatetc, pstgmed);

    if(!This->stream) {
        This->stream = pstgmed->u.pstm;
        IStream_AddRef(This->stream);
    }

    if(This->doc->nscontainer && This->doc->nscontainer->stream) {
        BYTE buf[1024];
        DWORD size;
        HRESULT hres;

        do {
            size = sizeof(buf);
            hres = IStream_Read(This->stream, buf, size, &size);
            nsIWebBrowserStream_AppendToStream(This->doc->nscontainer->stream, buf, size);
        }while(hres == S_OK);
    }

    return S_OK;
}

static HRESULT WINAPI BindStatusCallback_OnObjectAvailable(IBindStatusCallback *iface,
        REFIID riid, IUnknown *punk)
{
    BindStatusCallback *This = STATUSCLB_THIS(iface);
    FIXME("(%p)->(%s %p)\n", This, debugstr_guid(riid), punk);
    return E_NOTIMPL;
}

static const IBindStatusCallbackVtbl BindStatusCallbackVtbl = {
    BindStatusCallback_QueryInterface,
    BindStatusCallback_AddRef,
    BindStatusCallback_Release,
    BindStatusCallback_OnStartBinding,
    BindStatusCallback_GetPriority,
    BindStatusCallback_OnLowResource,
    BindStatusCallback_OnProgress,
    BindStatusCallback_OnStopBinding,
    BindStatusCallback_GetBindInfo,
    BindStatusCallback_OnDataAvailable,
    BindStatusCallback_OnObjectAvailable
};

static BindStatusCallback *BindStatusCallback_Create(HTMLDocument *doc, LPOLESTR url)
{
    BindStatusCallback *ret = HeapAlloc(GetProcessHeap(), 0, sizeof(BindStatusCallback));
    ret->lpBindStatusCallbackVtbl = &BindStatusCallbackVtbl;
    ret->ref = 0;
    ret->url = url;
    ret->doc = doc;
    ret->stream = NULL;
    IHTMLDocument2_AddRef(HTMLDOC(doc));
    return ret;
}

/**********************************************************
 * IPersistMoniker implementation
 */

#define PERSISTMON_THIS(iface) DEFINE_THIS(HTMLDocument, PersistMoniker, iface)

static HRESULT WINAPI PersistMoniker_QueryInterface(IPersistMoniker *iface, REFIID riid,
                                                            void **ppvObject)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistMoniker_AddRef(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistMoniker_Release(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistMoniker_GetClassID(IPersistMoniker *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    return IPersist_GetClassID(PERSIST(This), pClassID);
}

static HRESULT WINAPI PersistMoniker_IsDirty(IPersistMoniker *iface)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_Load(IPersistMoniker *iface, BOOL fFullyAvailable,
        IMoniker *pimkName, LPBC pibc, DWORD grfMode)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    IBindCtx *pbind;
    BindStatusCallback *callback;
    IStream *str = NULL;
    LPOLESTR url;
    HRESULT hres;
    nsresult nsres;

    TRACE("(%p)->(%x %p %p %08lx)\n", This, fFullyAvailable, pimkName, pibc, grfMode);

    if(pibc) {
        IUnknown *unk = NULL;

        static WCHAR wszClientSiteParam[] = {'{','d','4','d','b','6','8','5','0','-',
            '5','3','8','5','-','1','1','d','0','-','8','9','e','9','-','0','0','a',
            '0','c','9','0','a','9','0','a','c','}',0};

        /* FIXME:
         * Use params:
         * "__PrecreatedObject"
         * "BIND_CONTEXT_PARAM"
         * "__HTMLLOADOPTIONS"
         * "__DWNBINDINFO"
         * "URL Context"
         * "CBinding Context"
         * "_ITransData_Object_"
         * "_EnumFORMATETC_"
         */

        IBindCtx_GetObjectParam(pibc, wszClientSiteParam, &unk);
        if(unk) {
            IOleClientSite *client = NULL;

            hres = IUnknown_QueryInterface(unk, &IID_IOleClientSite, (void**)&client);
            if(SUCCEEDED(hres)) {
                TRACE("Got client site %p\n", client);
                IOleObject_SetClientSite(OLEOBJ(This), client);
                IOleClientSite_Release(client);
            }

            IUnknown_Release(unk);
        }
    }

    HTMLDocument_LockContainer(This, TRUE);
    
    hres = IMoniker_GetDisplayName(pimkName, pibc, NULL, &url);
    if(FAILED(hres)) {
        WARN("GetDiaplayName failed: %08lx\n", hres);
        return hres;
    }

    TRACE("got url: %s\n", debugstr_w(url));

    if(This->client) {
        IOleCommandTarget *cmdtrg = NULL;

        hres = IOleClientSite_QueryInterface(This->client, &IID_IOleCommandTarget,
                (void**)&cmdtrg);
        if(SUCCEEDED(hres)) {
            VARIANT var;

            V_VT(&var) = VT_I4;
            V_I4(&var) = 0;
            IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 37, 0, &var, NULL);
        }
    }

    if(This->nscontainer && !This->nscontainer->stream) {
        /*
         * This is a workaround for older Gecko that doesn't support nsIWebBrowserStream.
         * It uses Gecko's LoadURI instead of IMoniker's BindToStorage. Should we improve
         * it (to do so we'd have to use not frozen interfaces)?
         */
        LPOLESTR old_url = This->nscontainer->url;

        This->nscontainer->url = url;
        nsres = nsIWebNavigation_LoadURI(This->nscontainer->navigation, url,
                LOAD_FLAGS_NONE, NULL, NULL, NULL);
        if(NS_SUCCEEDED(nsres)) {
            CoTaskMemFree(old_url);
            return S_OK;
        }else {
            WARN("LoadURI failed: %08lx\n", nsres);
            This->nscontainer->url = old_url;
        }
    }    

    /* FIXME: Use grfMode */

    if(fFullyAvailable)
        FIXME("not supported fFullyAvailable\n");

    if(This->status_callback && This->status_callback->binding)
        IBinding_Abort(This->status_callback->binding);

    callback = This->status_callback = BindStatusCallback_Create(This, url);

    if(pibc) {
        pbind = pibc;
        RegisterBindStatusCallback(pbind, STATUSCLB(callback), NULL, 0);
    }else {
        CreateAsyncBindCtx(0, STATUSCLB(callback), NULL, &pbind);
    }

    hres = IMoniker_BindToStorage(pimkName, pbind, NULL, &IID_IStream, (void**)&str);

    if(!pibc)
        IBindCtx_Release(pbind);
    if(str)
        IStream_Release(str);
    if(FAILED(hres)) {
        WARN("BindToStorage failed: %08lx\n", hres);
        return hres;
    }

    return S_OK;
}

static HRESULT WINAPI PersistMoniker_Save(IPersistMoniker *iface, IMoniker *pimkName,
        LPBC pbc, BOOL fRemember)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p %x)\n", This, pimkName, pbc, fRemember);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_SaveCompleted(IPersistMoniker *iface, IMoniker *pimkName, LPBC pibc)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p %p)\n", This, pimkName, pibc);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistMoniker_GetCurMoniker(IPersistMoniker *iface, IMoniker **ppimkName)
{
    HTMLDocument *This = PERSISTMON_THIS(iface);
    FIXME("(%p)->(%p)\n", This, ppimkName);
    return E_NOTIMPL;
}

static const IPersistMonikerVtbl PersistMonikerVtbl = {
    PersistMoniker_QueryInterface,
    PersistMoniker_AddRef,
    PersistMoniker_Release,
    PersistMoniker_GetClassID,
    PersistMoniker_IsDirty,
    PersistMoniker_Load,
    PersistMoniker_Save,
    PersistMoniker_SaveCompleted,
    PersistMoniker_GetCurMoniker
};

/**********************************************************
 * IMonikerProp implementation
 */

#define MONPROP_THIS(iface) DEFINE_THIS(HTMLDocument, MonikerProp, iface)

static HRESULT WINAPI MonikerProp_QueryInterface(IMonikerProp *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI MonikerProp_AddRef(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI MonikerProp_Release(IMonikerProp *iface)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    return IHTMLDocument_Release(HTMLDOC(This));
}

static HRESULT WINAPI MonikerProp_PutProperty(IMonikerProp *iface, MONIKERPROPERTY mkp, LPCWSTR val)
{
    HTMLDocument *This = MONPROP_THIS(iface);
    FIXME("(%p)->(%d %s)\n", This, mkp, debugstr_w(val));
    return E_NOTIMPL;
}

static const IMonikerPropVtbl MonikerPropVtbl = {
    MonikerProp_QueryInterface,
    MonikerProp_AddRef,
    MonikerProp_Release,
    MonikerProp_PutProperty
};

/**********************************************************
 * IPersistFile implementation
 */

#define PERSISTFILE_THIS(iface) DEFINE_THIS(HTMLDocument, PersistFile, iface)

static HRESULT WINAPI PersistFile_QueryInterface(IPersistFile *iface, REFIID riid, void **ppvObject)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_QueryInterface(HTMLDOC(This), riid, ppvObject);
}

static ULONG WINAPI PersistFile_AddRef(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_AddRef(HTMLDOC(This));
}

static ULONG WINAPI PersistFile_Release(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    return IHTMLDocument2_Release(HTMLDOC(This));
}

static HRESULT WINAPI PersistFile_GetClassID(IPersistFile *iface, CLSID *pClassID)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);

    TRACE("(%p)->(%p)\n", This, pClassID);

    if(!pClassID)
        return E_INVALIDARG;

    memcpy(pClassID, &CLSID_HTMLDocument, sizeof(CLSID));
    return S_OK;
}

static HRESULT WINAPI PersistFile_IsDirty(IPersistFile *iface)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)\n", This);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Load(IPersistFile *iface, LPCOLESTR pszFileName, DWORD dwMode)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s %08lx)\n", This, debugstr_w(pszFileName), dwMode);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_Save(IPersistFile *iface, LPCOLESTR pszFileName, BOOL fRemember)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s %x)\n", This, debugstr_w(pszFileName), fRemember);
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_SaveCompleted(IPersistFile *iface, LPCOLESTR pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(pszFileName));
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistFile_GetCurFile(IPersistFile *iface, LPOLESTR *pszFileName)
{
    HTMLDocument *This = PERSISTFILE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, pszFileName);
    return E_NOTIMPL;
}

static const IPersistFileVtbl PersistFileVtbl = {
    PersistFile_QueryInterface,
    PersistFile_AddRef,
    PersistFile_Release,
    PersistFile_GetClassID,
    PersistFile_IsDirty,
    PersistFile_Load,
    PersistFile_Save,
    PersistFile_SaveCompleted,
    PersistFile_GetCurFile
};

BOOL HTMLDocument_OnLoad(HTMLDocument *This, LPCWSTR url)
{
    IOleCommandTarget *cmdtrg = NULL;
    HRESULT hres;

    TRACE("(%p)->(%s)\n", This, debugstr_w(url));

    hres = IOleClientSite_QueryInterface(This->client, &IID_IOleCommandTarget, (void**)&cmdtrg);
    if(SUCCEEDED(hres)) {
        VARIANT varUrl, varRes;

        V_VT(&varUrl) = VT_BSTR;
        V_BSTR(&varUrl) = SysAllocString(url);
        V_VT(&varRes) = VT_BOOL;

        hres = IOleCommandTarget_Exec(cmdtrg, &CGID_ShellDocView, 67, 0, &varUrl, &varRes);
        SysFreeString(V_BSTR(&varUrl));

        if(SUCCEEDED(hres) && !V_BOOL(&varRes)) {
            TRACE("got VARIANT_FALSE, do not load\n");
            IOleCommandTarget_Release(cmdtrg);
            return FALSE;
        }
    }

    return TRUE;
}

void HTMLDocument_Persist_Init(HTMLDocument *This)
{
    This->lpPersistMonikerVtbl = &PersistMonikerVtbl;
    This->lpPersistFileVtbl = &PersistFileVtbl;
    This->lpMonikerPropVtbl = &MonikerPropVtbl;

    This->status_callback = NULL;
}
