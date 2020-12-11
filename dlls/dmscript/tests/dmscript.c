/*
 * Copyright 2014 Michael Stefaniuc
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <stdarg.h>
#include <windef.h>
#include <initguid.h>
#include <wine/test.h>
#include <ole2.h>
#include <dmusici.h>
#include <dmusicf.h>

static BOOL missing_dmscript(void)
{
    IDirectMusicScript *dms;
    HRESULT hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicScript, (void**)&dms);

    if (hr == S_OK && dms)
    {
        IDirectMusicScript_Release(dms);
        return FALSE;
    }
    return TRUE;
}

/* Outer IUnknown for COM aggregation tests */
struct unk_impl {
    IUnknown IUnknown_iface;
    LONG ref;
    IUnknown *inner_unk;
};

static inline struct unk_impl *impl_from_IUnknown(IUnknown *iface)
{
    return CONTAINING_RECORD(iface, struct unk_impl, IUnknown_iface);
}

static HRESULT WINAPI unk_QueryInterface(IUnknown *iface, REFIID riid, void **ppv)
{
    struct unk_impl *This = impl_from_IUnknown(iface);

    return IUnknown_QueryInterface(This->inner_unk, riid, ppv);
}

static ULONG WINAPI unk_AddRef(IUnknown *iface)
{
    struct unk_impl *This = impl_from_IUnknown(iface);

    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI unk_Release(IUnknown *iface)
{
    struct unk_impl *This = impl_from_IUnknown(iface);

    return InterlockedDecrement(&This->ref);
}

static const IUnknownVtbl unk_vtbl =
{
    unk_QueryInterface,
    unk_AddRef,
    unk_Release
};

static void test_COM(void)
{
    IDirectMusicScript *dms = NULL;
    IDirectMusicObject *dmo;
    IPersistStream *ps;
    IUnknown *unk;
    struct unk_impl unk_obj = {{&unk_vtbl}, 19, NULL};
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation. An invalid non-NULL outer IUnknown crashes newer Windows versions */
    hr = CoCreateInstance(&CLSID_DirectMusicScript, &unk_obj.IUnknown_iface, CLSCTX_INPROC_SERVER,
            &IID_IUnknown, (void**)&unk_obj.inner_unk);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicScript create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!unk_obj.inner_unk, "unk_obj.inner_unk = %p\n", unk_obj.inner_unk);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER, &IID_IClassFactory,
            (void**)&dms);
    ok(hr == E_NOINTERFACE, "DirectMusicScript create failed: %08x, expected E_NOINTERFACE\n", hr);

    /* Same refcount for all DirectMusicScript interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicScript, (void**)&dms);
    ok(hr == S_OK, "DirectMusicScript create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicScript_AddRef(dms);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicScript_QueryInterface(dms, &IID_IDirectMusicObject, (void**)&dmo);
    ok(hr == S_OK, "QueryInterface for IID_IDirectMusicObject failed: %08x\n", hr);
    refcount = IDirectMusicObject_AddRef(dmo);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    refcount = IDirectMusicObject_Release(dmo);

    hr = IDirectMusicScript_QueryInterface(dms, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IPersistStream_Release(ps);

    hr = IDirectMusicScript_QueryInterface(dms, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 6, "refcount == %u, expected 6\n", refcount);
    refcount = IUnknown_Release(unk);

    while (IDirectMusicScript_Release(dms));
}

static void test_COM_scripttrack(void)
{
    IDirectMusicTrack *dmt;
    IPersistStream *ps;
    IUnknown *unk;
    struct unk_impl unk_obj = {{&unk_vtbl}, 19, NULL};
    ULONG refcount;
    HRESULT hr;

    /* COM aggregation. An invalid non-NULL outer IUnknown crashes newer Windows versions */
    hr = CoCreateInstance(&CLSID_DirectMusicScriptTrack, &unk_obj.IUnknown_iface,
            CLSCTX_INPROC_SERVER, &IID_IUnknown, (void**)&unk_obj.inner_unk);
    ok(hr == CLASS_E_NOAGGREGATION,
            "DirectMusicScriptTrack create failed: %08x, expected CLASS_E_NOAGGREGATION\n", hr);
    ok(!unk_obj.inner_unk, "unk_obj.inner_unk = %p\n", unk_obj.inner_unk);

    /* Invalid RIID */
    hr = CoCreateInstance(&CLSID_DirectMusicScriptTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void**)&dmt);
    ok(hr == E_NOINTERFACE, "DirectMusicScriptTrack create failed: %08x, expected E_NOINTERFACE\n",
            hr);

    /* Same refcount for all DirectMusicScriptTrack interfaces */
    hr = CoCreateInstance(&CLSID_DirectMusicScriptTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicTrack, (void**)&dmt);
    ok(hr == S_OK, "DirectMusicScriptTrack create failed: %08x, expected S_OK\n", hr);
    refcount = IDirectMusicTrack_AddRef(dmt);
    ok(refcount == 2, "refcount == %u, expected 2\n", refcount);

    hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    refcount = IPersistStream_AddRef(ps);
    ok(refcount == 4, "refcount == %u, expected 4\n", refcount);
    IPersistStream_Release(ps);

    hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IUnknown, (void**)&unk);
    ok(hr == S_OK, "QueryInterface for IID_IUnknown failed: %08x\n", hr);
    refcount = IUnknown_AddRef(unk);
    ok(refcount == 5, "refcount == %u, expected 5\n", refcount);
    refcount = IUnknown_Release(unk);

    while (IDirectMusicTrack_Release(dmt));
}

static void test_dmscript(void)
{
    IDirectMusicScript *dms;
    IPersistStream *ps;
    CLSID class;
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicScript, (void**)&dms);
    ok(hr == S_OK, "DirectMusicScript create failed: %08x, expected S_OK\n", hr);

    /* Unimplemented IPersistStream methods */
    hr = IDirectMusicScript_QueryInterface(dms, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);
    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == E_NOTIMPL, "IPersistStream_GetClassID failed: %08x\n", hr);
    hr = IPersistStream_IsDirty(ps);
    ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicScript_Release(dms));
}

static void test_scripttrack(void)
{
    IDirectMusicTrack *dmt;
    IPersistStream *ps;
    CLSID class;
    ULARGE_INTEGER size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectMusicScriptTrack, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicTrack, (void**)&dmt);
    ok(hr == S_OK, "DirectMusicScriptTrack create failed: %08x, expected S_OK\n", hr);

    /* IPersistStream */
    hr = IDirectMusicTrack_QueryInterface(dmt, &IID_IPersistStream, (void**)&ps);
    ok(hr == S_OK, "QueryInterface for IID_IPersistStream failed: %08x\n", hr);

    hr = IPersistStream_GetClassID(ps, NULL);
    ok(hr == E_POINTER, "got 0x%08x\n", hr);

    hr = IPersistStream_GetClassID(ps, &class);
    ok(hr == S_OK, "IPersistStream_GetClassID failed: %08x\n", hr);
    ok(IsEqualGUID(&class, &CLSID_DirectMusicScriptTrack),
            "Expected class CLSID_DirectMusicScriptTrack got %s\n", wine_dbgstr_guid(&class));

    /* Unimplemented IPersistStream methods */
    hr = IPersistStream_IsDirty(ps);
    todo_wine ok(hr == S_FALSE, "IPersistStream_IsDirty failed: %08x\n", hr);
    hr = IPersistStream_GetSizeMax(ps, &size);
    ok(hr == E_NOTIMPL, "IPersistStream_GetSizeMax failed: %08x\n", hr);
    hr = IPersistStream_Save(ps, NULL, TRUE);
    ok(hr == E_NOTIMPL, "IPersistStream_Save failed: %08x\n", hr);

    while (IDirectMusicTrack_Release(dmt));
}

struct chunk {
    FOURCC id;
    DWORD size;
    FOURCC type;
};

#define CHUNK_HDR_SIZE (sizeof(FOURCC) + sizeof(DWORD))

/* Generate a RIFF file format stream from an array of FOURCC ids.
   RIFF and LIST need to be followed by the form type respectively list type,
   followed by the chunks of the list and terminated with 0. */
static IStream *gen_riff_stream(const FOURCC *ids)
{
    static const LARGE_INTEGER zero;
    int level = -1;
    DWORD *sizes[4];    /* Stack for the sizes of RIFF and LIST chunks */
    char riff[1024];
    char *p = riff;
    struct chunk *ck;
    IStream *stream;

    do {
        ck = (struct chunk *)p;
        ck->id = *ids++;
        switch (ck->id) {
            case 0:
                *sizes[level] = p - (char *)sizes[level] - sizeof(DWORD);
                level--;
                break;
            case FOURCC_RIFF:
                level++;
                sizes[level] = &ck->size;
                ck->type = *ids++;
                p += sizeof(*ck);
                break;
        }
    } while (level >= 0);

    ck = (struct chunk *)riff;
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    IStream_Write(stream, riff, ck->size + CHUNK_HDR_SIZE, NULL);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);

    return stream;
}

static void test_parsedescriptor(void)
{
    IDirectMusicObject *dmo;
    IStream *stream;
    DMUS_OBJECTDESC desc = {0};
    HRESULT hr;
    FOURCC empty[] = {FOURCC_RIFF, DMUS_FOURCC_SCRIPT_FORM, 0};

    hr = CoCreateInstance(&CLSID_DirectMusicScript, NULL, CLSCTX_INPROC_SERVER,
            &IID_IDirectMusicObject, (void **)&dmo);
    ok(hr == S_OK, "DirectMusicScript create failed: %08x, expected S_OK\n", hr);

    /* Nothing loaded */
    hr = IDirectMusicObject_GetDescriptor(dmo, &desc);
    ok(hr == S_OK, "GetDescriptor failed: %08x, expected S_OK\n", hr);
    todo_wine ok(desc.dwValidData == (DMUS_OBJ_CLASS | DMUS_OBJ_VERSION),
            "Got valid data %#x, expected DMUS_OBJ_OBJECT | DMUS_OBJ_VERSION\n", desc.dwValidData);
    ok(IsEqualGUID(&desc.guidClass, &CLSID_DirectMusicScript),
            "Got class guid %s, expected CLSID_DirectMusicScript\n",
            wine_dbgstr_guid(&desc.guidClass));
    ok(!desc.vVersion.dwVersionMS && !desc.vVersion.dwVersionLS,
            "Got DMUS_VERSION %u.%u, expected 0.0\n", desc.vVersion.dwVersionMS,
            desc.vVersion.dwVersionLS);

    /* Empty RIFF stream */
    stream = gen_riff_stream(empty);
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == S_OK, "ParseDescriptor failed: %08x, expected S_OK\n", hr);
    ok(!desc.dwValidData, "Got valid data %#x, expected 0\n", desc.dwValidData);
    IStream_Release(stream);

    /* NULL pointers */
    memset(&desc, 0, sizeof(desc));
    hr = IDirectMusicObject_ParseDescriptor(dmo, NULL, &desc);
    ok(hr == E_POINTER, "ParseDescriptor failed: %08x, expected E_POINTER\n", hr);
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, NULL);
    ok(hr == E_POINTER, "ParseDescriptor failed: %08x, expected E_POINTER\n", hr);

    /* Wrong form */
    empty[1] = DMUS_FOURCC_CONTAINER_FORM;
    stream = gen_riff_stream(empty);
    hr = IDirectMusicObject_ParseDescriptor(dmo, stream, &desc);
    ok(hr == DMUS_E_SCRIPT_INVALID_FILE,
            "ParseDescriptor failed: %08x, expected DMUS_E_SCRIPT_INVALID_FILE\n", hr);

    IDirectMusicObject_Release(dmo);
}

START_TEST(dmscript)
{
    CoInitialize(NULL);

    if (missing_dmscript())
    {
        skip("dmscript not available\n");
        CoUninitialize();
        return;
    }
    test_COM();
    test_COM_scripttrack();
    test_dmscript();
    test_scripttrack();
    test_parsedescriptor();

    CoUninitialize();
}
