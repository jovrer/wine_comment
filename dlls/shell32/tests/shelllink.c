/*
 * Unit tests for shelllinks
 *
 * Copyright 2004 Mike McCormack
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
 * This is a test program for the SHGet{Special}Folder{Path|Location} functions
 * of shell32, that get either a filesytem path or a LPITEMIDLIST (shell
 * namespace) path for a given folder (CSIDL value).
 *
 */

#define COBJMACROS

#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "shlguid.h"
#include "shobjidl.h"
#include "shlobj.h"
#include "wine/test.h"

#include "shell32_test.h"

static const WCHAR lnkfile[]= { 'C',':','\\','t','e','s','t','.','l','n','k',0 };
static const WCHAR notafile[]= { 'C',':','\\','n','o','n','e','x','i','s','t','e','n','t','\\','f','i','l','e',0 };


/* For some reason SHILCreateFromPath does not work on Win98 and
 * SHSimpleIDListFromPathA does not work on NT4. But if we call both we
 * get what we want on all platforms.
 */
static LPITEMIDLIST (WINAPI *pSHSimpleIDListFromPathA)(LPCSTR)=NULL;

static LPITEMIDLIST path_to_pidl(const char* path)
{
    LPITEMIDLIST pidl;

    if (!pSHSimpleIDListFromPathA)
    {
        HMODULE hdll=LoadLibraryA("shell32.dll");
        pSHSimpleIDListFromPathA=(void*)GetProcAddress(hdll, (char*)162);
        if (!pSHSimpleIDListFromPathA)
            trace("SHSimpleIDListFromPathA not found in shell32.dll\n");
    }

    pidl=NULL;
    if (pSHSimpleIDListFromPathA)
        pidl=pSHSimpleIDListFromPathA(path);

    if (!pidl)
    {
        WCHAR* pathW;
        HRESULT r;
        int len;

        len=MultiByteToWideChar(CP_ACP, 0, path, -1, NULL, 0);
        pathW=HeapAlloc(GetProcessHeap(), 0, len*sizeof(WCHAR));
        MultiByteToWideChar(CP_ACP, 0, path, -1, pathW, len);

        r=SHILCreateFromPath(pathW, &pidl, NULL);
        todo_wine {
        ok(SUCCEEDED(r), "SHILCreateFromPath failed (0x%08lx)\n", r);
        }
        HeapFree(GetProcessHeap(), 0, pathW);
    }
    return pidl;
}


/*
 * Test manipulation of an IShellLink's properties.
 */

static void test_get_set(void)
{
    HRESULT r;
    IShellLinkA *sl;
    char mypath[MAX_PATH];
    char buffer[INFOTIPSIZE];
    LPITEMIDLIST pidl, tmp_pidl;
    const char * str;
    int i;
    WORD w;

    r = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IShellLinkA, (LPVOID*)&sl);
    ok(SUCCEEDED(r), "no IID_IShellLinkA (0x%08lx)\n", r);
    if (!SUCCEEDED(r))
        return;

    /* Test Getting / Setting the description */
    strcpy(buffer,"garbage");
    r = IShellLinkA_GetDescription(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetDescription failed (0x%08lx)\n", r);
    ok(*buffer=='\0', "GetDescription returned '%s'\n", buffer);

    str="Some description";
    r = IShellLinkA_SetDescription(sl, str);
    ok(SUCCEEDED(r), "SetDescription failed (0x%08lx)\n", r);

    strcpy(buffer,"garbage");
    r = IShellLinkA_GetDescription(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetDescription failed (0x%08lx)\n", r);
    ok(lstrcmp(buffer,str)==0, "GetDescription returned '%s'\n", buffer);

    /* Test Getting / Setting the work directory */
    strcpy(buffer,"garbage");
    r = IShellLinkA_GetWorkingDirectory(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetWorkingDirectory failed (0x%08lx)\n", r);
    ok(*buffer=='\0', "GetWorkingDirectory returned '%s'\n", buffer);

    str="c:\\nonexistent\\directory";
    r = IShellLinkA_SetWorkingDirectory(sl, str);
    ok(SUCCEEDED(r), "SetWorkingDirectory failed (0x%08lx)\n", r);

    strcpy(buffer,"garbage");
    r = IShellLinkA_GetWorkingDirectory(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetWorkingDirectory failed (0x%08lx)\n", r);
    ok(lstrcmpi(buffer,str)==0, "GetWorkingDirectory returned '%s'\n", buffer);

    /* Test Getting / Setting the work directory */
    strcpy(buffer,"garbage");
    r = IShellLinkA_GetPath(sl, buffer, sizeof(buffer), NULL, SLGP_RAWPATH);
    ok(SUCCEEDED(r), "GetPath failed (0x%08lx)\n", r);
    ok(*buffer=='\0', "GetPath returned '%s'\n", buffer);

    r = IShellLinkA_SetPath(sl, "");
    ok(r==S_OK, "SetPath failed (0x%08lx)\n", r);

    strcpy(buffer,"garbage");
    r = IShellLinkA_GetPath(sl, buffer, sizeof(buffer), NULL, SLGP_RAWPATH);
    ok(SUCCEEDED(r), "GetPath failed (0x%08lx)\n", r);
    ok(*buffer=='\0', "GetPath returned '%s'\n", buffer);

    str="c:\\nonexistent\\file";
    r = IShellLinkA_SetPath(sl, str);
    ok(r==S_FALSE, "SetPath failed (0x%08lx)\n", r);

    strcpy(buffer,"garbage");
    r = IShellLinkA_GetPath(sl, buffer, sizeof(buffer), NULL, SLGP_RAWPATH);
    ok(SUCCEEDED(r), "GetPath failed (0x%08lx)\n", r);
    ok(lstrcmpi(buffer,str)==0, "GetPath returned '%s'\n", buffer);

    /* Get some a real path to play with */
    r=GetModuleFileName(NULL, mypath, sizeof(mypath));
    ok(r>=0 && r<sizeof(mypath), "GetModuleFileName failed (%ld)\n", r);

    /* Test the interaction of SetPath and SetIDList */
    tmp_pidl=NULL;
    r = IShellLinkA_GetIDList(sl, &tmp_pidl);
    ok(SUCCEEDED(r), "GetIDList failed (0x%08lx)\n", r);
    if (SUCCEEDED(r))
    {
        strcpy(buffer,"garbage");
        r=SHGetPathFromIDListA(tmp_pidl, buffer);
        todo_wine {
        ok(r, "SHGetPathFromIDListA failed\n");
        }
        if (r)
            ok(lstrcmpi(buffer,str)==0, "GetIDList returned '%s'\n", buffer);
    }

    pidl=path_to_pidl(mypath);
    todo_wine {
    ok(pidl!=NULL, "path_to_pidl returned a NULL pidl\n");
    }

    if (pidl)
    {
        r = IShellLinkA_SetIDList(sl, pidl);
        ok(SUCCEEDED(r), "SetIDList failed (0x%08lx)\n", r);

        tmp_pidl=NULL;
        r = IShellLinkA_GetIDList(sl, &tmp_pidl);
        ok(SUCCEEDED(r), "GetIDList failed (0x%08lx)\n", r);
        ok(tmp_pidl && ILIsEqual(pidl, tmp_pidl),
           "GetIDList returned an incorrect pidl\n");

        /* tmp_pidl is owned by IShellLink so we don't free it */
        ILFree(pidl);

        strcpy(buffer,"garbage");
        r = IShellLinkA_GetPath(sl, buffer, sizeof(buffer), NULL, SLGP_RAWPATH);
        ok(SUCCEEDED(r), "GetPath failed (0x%08lx)\n", r);
        ok(lstrcmpi(buffer, mypath)==0, "GetPath returned '%s'\n", buffer);
    }

    /* Test Getting / Setting the arguments */
    strcpy(buffer,"garbage");
    r = IShellLinkA_GetArguments(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetArguments failed (0x%08lx)\n", r);
    ok(*buffer=='\0', "GetArguments returned '%s'\n", buffer);

    str="param1 \"spaced param2\"";
    r = IShellLinkA_SetArguments(sl, str);
    ok(SUCCEEDED(r), "SetArguments failed (0x%08lx)\n", r);

    strcpy(buffer,"garbage");
    r = IShellLinkA_GetArguments(sl, buffer, sizeof(buffer));
    ok(SUCCEEDED(r), "GetArguments failed (0x%08lx)\n", r);
    ok(lstrcmp(buffer,str)==0, "GetArguments returned '%s'\n", buffer);

    /* Test Getting / Setting showcmd */
    i=0xdeadbeef;
    r = IShellLinkA_GetShowCmd(sl, &i);
    ok(SUCCEEDED(r), "GetShowCmd failed (0x%08lx)\n", r);
    ok(i==SW_SHOWNORMAL, "GetShowCmd returned %d\n", i);

    r = IShellLinkA_SetShowCmd(sl, SW_SHOWMAXIMIZED);
    ok(SUCCEEDED(r), "SetShowCmd failed (0x%08lx)\n", r);

    i=0xdeadbeef;
    r = IShellLinkA_GetShowCmd(sl, &i);
    ok(SUCCEEDED(r), "GetShowCmd failed (0x%08lx)\n", r);
    ok(i==SW_SHOWMAXIMIZED, "GetShowCmd returned %d'\n", i);

    /* Test Getting / Setting the icon */
    i=0xdeadbeef;
    strcpy(buffer,"garbage");
    r = IShellLinkA_GetIconLocation(sl, buffer, sizeof(buffer), &i);
    todo_wine {
    ok(SUCCEEDED(r), "GetIconLocation failed (0x%08lx)\n", r);
    }
    ok(*buffer=='\0', "GetIconLocation returned '%s'\n", buffer);
    ok(i==0, "GetIconLocation returned %d\n", i);

    str="c:\\nonexistent\\file";
    r = IShellLinkA_SetIconLocation(sl, str, 0xbabecafe);
    ok(SUCCEEDED(r), "SetIconLocation failed (0x%08lx)\n", r);

    i=0xdeadbeef;
    r = IShellLinkA_GetIconLocation(sl, buffer, sizeof(buffer), &i);
    ok(SUCCEEDED(r), "GetIconLocation failed (0x%08lx)\n", r);
    ok(lstrcmpi(buffer,str)==0, "GetArguments returned '%s'\n", buffer);
    ok(i==0xbabecafe, "GetIconLocation returned %d'\n", i);

    /* Test Getting / Setting the hot key */
    w=0xbeef;
    r = IShellLinkA_GetHotkey(sl, &w);
    ok(SUCCEEDED(r), "GetHotkey failed (0x%08lx)\n", r);
    ok(w==0, "GetHotkey returned %d\n", w);

    r = IShellLinkA_SetHotkey(sl, 0x5678);
    ok(SUCCEEDED(r), "SetHotkey failed (0x%08lx)\n", r);

    w=0xbeef;
    r = IShellLinkA_GetHotkey(sl, &w);
    ok(SUCCEEDED(r), "GetHotkey failed (0x%08lx)\n", r);
    ok(w==0x5678, "GetHotkey returned %d'\n", w);

    IShellLinkA_Release(sl);
}


/*
 * Test saving and loading .lnk files
 */

#define lok                   ok_(__FILE__, line)
#define check_lnk(a,b)        check_lnk_(__LINE__, (a), (b))

void create_lnk_(int line, const WCHAR* path, lnk_desc_t* desc, int save_fails)
{
    HRESULT r;
    IShellLinkA *sl;
    IPersistFile *pf;

    r = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IShellLinkA, (LPVOID*)&sl);
    lok(SUCCEEDED(r), "no IID_IShellLinkA (0x%08lx)\n", r);
    if (!SUCCEEDED(r))
        return;

    if (desc->description)
    {
        r = IShellLinkA_SetDescription(sl, desc->description);
        lok(SUCCEEDED(r), "SetDescription failed (0x%08lx)\n", r);
    }
    if (desc->workdir)
    {
        r = IShellLinkA_SetWorkingDirectory(sl, desc->workdir);
        lok(SUCCEEDED(r), "SetWorkingDirectory failed (0x%08lx)\n", r);
    }
    if (desc->path)
    {
        r = IShellLinkA_SetPath(sl, desc->path);
        lok(SUCCEEDED(r), "SetPath failed (0x%08lx)\n", r);
    }
    if (desc->pidl)
    {
        r = IShellLinkA_SetIDList(sl, desc->pidl);
        lok(SUCCEEDED(r), "SetIDList failed (0x%08lx)\n", r);
    }
    if (desc->arguments)
    {
        r = IShellLinkA_SetArguments(sl, desc->arguments);
        lok(SUCCEEDED(r), "SetArguments failed (0x%08lx)\n", r);
    }
    if (desc->showcmd)
    {
        r = IShellLinkA_SetShowCmd(sl, desc->showcmd);
        lok(SUCCEEDED(r), "SetShowCmd failed (0x%08lx)\n", r);
    }
    if (desc->icon)
    {
        r = IShellLinkA_SetIconLocation(sl, desc->icon, desc->icon_id);
        lok(SUCCEEDED(r), "SetIconLocation failed (0x%08lx)\n", r);
    }
    if (desc->hotkey)
    {
        r = IShellLinkA_SetHotkey(sl, desc->hotkey);
        lok(SUCCEEDED(r), "SetHotkey failed (0x%08lx)\n", r);
    }

    r = IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (LPVOID*)&pf);
    lok(SUCCEEDED(r), "no IID_IPersistFile (0x%08lx)\n", r);
    if (SUCCEEDED(r))
    {
        r = IPersistFile_Save(pf, path, TRUE);
        if (save_fails)
        {
            todo_wine {
            lok(SUCCEEDED(r), "save failed (0x%08lx)\n", r);
            }
        }
        else
        {
            lok(SUCCEEDED(r), "save failed (0x%08lx)\n", r);
        }
        IPersistFile_Release(pf);
    }

    IShellLinkA_Release(sl);
}

static void check_lnk_(int line, const WCHAR* path, lnk_desc_t* desc)
{
    HRESULT r;
    IShellLinkA *sl;
    IPersistFile *pf;
    char buffer[INFOTIPSIZE];

    r = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                         &IID_IShellLinkA, (LPVOID*)&sl);
    lok(SUCCEEDED(r), "no IID_IShellLinkA (0x%08lx)\n", r);
    if (!SUCCEEDED(r))
        return;

    r = IShellLinkA_QueryInterface(sl, &IID_IPersistFile, (LPVOID*)&pf);
    lok(SUCCEEDED(r), "no IID_IPersistFile (0x%08lx)\n", r);
    if (!SUCCEEDED(r))
    {
        IShellLinkA_Release(sl);
        return;
    }

    r = IPersistFile_Load(pf, path, STGM_READ);
    lok(SUCCEEDED(r), "load failed (0x%08lx)\n", r);
    IPersistFile_Release(pf);
    if (!SUCCEEDED(r))
    {
        IShellLinkA_Release(sl);
        return;
    }

    if (desc->description)
    {
        strcpy(buffer,"garbage");
        r = IShellLinkA_GetDescription(sl, buffer, sizeof(buffer));
        lok(SUCCEEDED(r), "GetDescription failed (0x%08lx)\n", r);
        lok(lstrcmp(buffer, desc->description)==0,
           "GetDescription returned '%s' instead of '%s'\n",
           buffer, desc->description);
    }
    if (desc->workdir)
    {
        strcpy(buffer,"garbage");
        r = IShellLinkA_GetWorkingDirectory(sl, buffer, sizeof(buffer));
        lok(SUCCEEDED(r), "GetWorkingDirectory failed (0x%08lx)\n", r);
        lok(lstrcmpi(buffer, desc->workdir)==0,
           "GetWorkingDirectory returned '%s' instead of '%s'\n",
           buffer, desc->workdir);
    }
    if (desc->path)
    {
        strcpy(buffer,"garbage");
        r = IShellLinkA_GetPath(sl, buffer, sizeof(buffer), NULL, SLGP_RAWPATH);
        lok(SUCCEEDED(r), "GetPath failed (0x%08lx)\n", r);
        lok(lstrcmpi(buffer, desc->path)==0,
           "GetPath returned '%s' instead of '%s'\n",
           buffer, desc->path);
    }
    if (desc->pidl)
    {
        LPITEMIDLIST pidl=NULL;
        r = IShellLinkA_GetIDList(sl, &pidl);
        lok(SUCCEEDED(r), "GetIDList failed (0x%08lx)\n", r);
        lok(ILIsEqual(pidl, desc->pidl),
           "GetIDList returned an incorrect pidl\n");
    }
    if (desc->showcmd)
    {
        int i=0xdeadbeef;
        r = IShellLinkA_GetShowCmd(sl, &i);
        lok(SUCCEEDED(r), "GetShowCmd failed (0x%08lx)\n", r);
        lok(i==desc->showcmd,
           "GetShowCmd returned 0x%0x instead of 0x%0x\n",
           i, desc->showcmd);
    }
    if (desc->icon)
    {
        int i=0xdeadbeef;
        strcpy(buffer,"garbage");
        r = IShellLinkA_GetIconLocation(sl, buffer, sizeof(buffer), &i);
        lok(SUCCEEDED(r), "GetIconLocation failed (0x%08lx)\n", r);
        lok(lstrcmpi(buffer, desc->icon)==0,
           "GetIconLocation returned '%s' instead of '%s'\n",
           buffer, desc->icon);
        lok(i==desc->icon_id,
           "GetIconLocation returned 0x%0x instead of 0x%0x\n",
           i, desc->icon_id);
    }
    if (desc->hotkey)
    {
        WORD i=0xbeef;
        r = IShellLinkA_GetHotkey(sl, &i);
        lok(SUCCEEDED(r), "GetHotkey failed (0x%08lx)\n", r);
        lok(i==desc->hotkey,
           "GetHotkey returned 0x%04x instead of 0x%04x\n",
           i, desc->hotkey);
    }

    IShellLinkA_Release(sl);
}

static void test_load_save(void)
{
    lnk_desc_t desc;
    char mypath[MAX_PATH];
    char mydir[MAX_PATH];
    char* p;
    DWORD r;

    /* Save an empty .lnk file */
    memset(&desc, 0, sizeof(desc));
    create_lnk(lnkfile, &desc, 0);

    /* It should come back as a bunch of empty strings */
    desc.description="";
    desc.workdir="";
    desc.path="";
    desc.arguments="";
    desc.icon="";
    check_lnk(lnkfile, &desc);


    /* Point a .lnk file to nonexistent files */
    desc.description="";
    desc.workdir="c:\\Nonexitent\\work\\directory";
    desc.path="c:\\nonexistent\\path";
    desc.pidl=NULL;
    desc.arguments="";
    desc.showcmd=0;
    desc.icon="c:\\nonexistent\\icon\\file";
    desc.icon_id=1234;
    desc.hotkey=0;
    create_lnk(lnkfile, &desc, 0);
    check_lnk(lnkfile, &desc);

    r=GetModuleFileName(NULL, mypath, sizeof(mypath));
    ok(r>=0 && r<sizeof(mypath), "GetModuleFileName failed (%ld)\n", r);
    strcpy(mydir, mypath);
    p=strrchr(mydir, '\\');
    if (p)
        *p='\0';


    /* Overwrite the existing lnk file and point it to existing files */
    desc.description="test 2";
    desc.workdir=mydir;
    desc.path=mypath;
    desc.pidl=NULL;
    desc.arguments="/option1 /option2 \"Some string\"";
    desc.showcmd=SW_SHOWNORMAL;
    desc.icon=mypath;
    desc.icon_id=0;
    desc.hotkey=0x1234;
    create_lnk(lnkfile, &desc, 0);
    check_lnk(lnkfile, &desc);

    /* FIXME: Also test saving a .lnk pointing to a pidl that cannot be
     * represented as a path.
     */

    /* DeleteFileW is not implemented on Win9x */
    r=DeleteFileA("c:\\test.lnk");
    ok(r, "failed to delete link (%ld)\n", GetLastError());
}

static void test_datalink(void)
{
    static const WCHAR lnk[] = {
      ':',':','{','9','d','b','1','1','8','6','f','-','4','0','d','f','-','1',
      '1','d','1','-','a','a','8','c','-','0','0','c','0','4','f','b','6','7',
      '8','6','3','}',':','{','0','0','0','1','0','4','0','9','-','7','8','E',
      '1','-','1','1','D','2','-','B','6','0','F','-','0','0','6','0','9','7',
      'C','9','9','8','E','7','}',':',':','{','9','d','b','1','1','8','6','e',
      '-','4','0','d','f','-','1','1','d','1','-','a','a','8','c','-','0','0',
      'c','0','4','f','b','6','7','8','6','3','}',':','2','6',',','!','!','g',
      'x','s','f','(','N','g',']','q','F','`','H','{','L','s','A','C','C','E',
      'S','S','F','i','l','e','s','>','p','l','T',']','j','I','{','j','f','(',
      '=','1','&','L','[','-','8','1','-',']',':',':',0 };
    static const WCHAR comp[] = {
      '2','6',',','!','!','g','x','s','f','(','N','g',']','q','F','`','H','{',
      'L','s','A','C','C','E','S','S','F','i','l','e','s','>','p','l','T',']',
      'j','I','{','j','f','(','=','1','&','L','[','-','8','1','-',']',0 };
    IShellLinkDataList *dl = NULL;
    IShellLinkW *sl = NULL;
    HRESULT r;
    DWORD flags = 0;
    EXP_DARWIN_LINK *dar;

    r = CoCreateInstance( &CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                            &IID_IShellLinkW, (LPVOID*)&sl );
    ok( r == S_OK, "no shelllink\n");
    if (!sl)
        return;

    r = IShellLinkW_QueryInterface( sl, &IID_IShellLinkDataList, (LPVOID*) &dl );
    ok(r == S_OK, "no datalink interface\n");

    if (!dl)
        return;

    flags = 0;
    r = dl->lpVtbl->GetFlags( dl, &flags );
    ok( r == S_OK, "GetFlags failed\n");
    ok( flags == 0, "GetFlags returned wrong flags\n");

    dar = (void*)-1;
    r = dl->lpVtbl->CopyDataBlock( dl, EXP_DARWIN_ID_SIG, (LPVOID*) &dar );
    ok( r == E_FAIL, "CopyDataBlock failed\n");
    ok( dar == NULL, "should be null\n");

    r = IShellLinkW_SetPath(sl, lnk);
    ok(r == S_OK, "set path failed\n");

    /*
     * The following crashes:
     * r = dl->lpVtbl->GetFlags( dl, NULL );
     */

    flags = 0;
    r = dl->lpVtbl->GetFlags( dl, &flags );
    ok( r == S_OK, "GetFlags failed\n");
    ok( flags == (SLDF_HAS_DARWINID|SLDF_HAS_LOGO3ID),
        "GetFlags returned wrong flags\n");

    dar = NULL;
    r = dl->lpVtbl->CopyDataBlock( dl, EXP_DARWIN_ID_SIG, (LPVOID*) &dar );
    ok( r == S_OK, "CopyDataBlock failed\n");

    ok( dar && dar->dbh.dwSignature == EXP_DARWIN_ID_SIG, "signature wrong\n");
    ok( dar && 0==lstrcmpW(dar->szwDarwinID, comp ), "signature wrong\n");

    LocalFree( dar );

    IUnknown_Release( dl );
    IShellLinkW_Release( sl );
}

START_TEST(shelllink)
{
    HRESULT r;

    r = CoInitialize(NULL);
    ok(SUCCEEDED(r), "CoInitialize failed (0x%08lx)\n", r);
    if (!SUCCEEDED(r))
        return;

    test_get_set();
    test_load_save();
    test_datalink();

    CoUninitialize();
}
