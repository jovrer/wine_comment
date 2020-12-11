/*
 * Unit test of the IShellFolder functions.
 *
 * Copyright 2004 Vitaliy Margolen
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

#include <stdarg.h>
#include <stdio.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "wtypes.h"
#include "shellapi.h"


#include "shlguid.h"
#include "shlobj.h"
#include "shobjidl.h"
#include "shlwapi.h"
#include "ocidl.h"
#include "oleauto.h"


#include "wine/unicode.h"
#include "wine/test.h"


static IMalloc *ppM;

static HRESULT (WINAPI *pSHBindToParent)(LPCITEMIDLIST, REFIID, LPVOID*, LPCITEMIDLIST*);
static BOOL (WINAPI *pSHGetSpecialFolderPathW)(HWND, LPWSTR, int, BOOL);
static HRESULT (WINAPI *pStrRetToBufW)(STRRET*,LPCITEMIDLIST,LPWSTR,UINT);
static LPITEMIDLIST (WINAPI *pILFindLastID)(LPCITEMIDLIST);

static void init_function_pointers(void)
{
    HMODULE hmod;
    HRESULT hr;

    hmod = GetModuleHandleA("shell32.dll");
    if(hmod)
    {
        pSHBindToParent = (void*)GetProcAddress(hmod, "SHBindToParent");
        pSHGetSpecialFolderPathW = (void*)GetProcAddress(hmod, "SHGetSpecialFolderPathW");
        pILFindLastID = (void *)GetProcAddress(hmod, (LPCSTR)16);

    }

    hmod = GetModuleHandleA("shlwapi.dll");
    if(hmod)
    {
        pStrRetToBufW = (void*)GetProcAddress(hmod, "StrRetToBufW");
    }

    hr = SHGetMalloc(&ppM);
    ok(hr == S_OK, "SHGetMalloc failed %08lx\n", hr);
}

static void test_ParseDisplayName(void)
{
    HRESULT hr;
    IShellFolder *IDesktopFolder;
    static const char *cNonExistDir1A = "c:\\nonexist_subdir";
    static const char *cNonExistDir2A = "c:\\\\nonexist_subdir";
    DWORD res;
    WCHAR cTestDirW [MAX_PATH] = {0};
    ITEMIDLIST *newPIDL;

    hr = SHGetDesktopFolder(&IDesktopFolder);
    if(hr != S_OK) return;

    res = GetFileAttributesA(cNonExistDir1A);
    if(res != INVALID_FILE_ATTRIBUTES) return;

    MultiByteToWideChar(CP_ACP, 0, cNonExistDir1A, -1, cTestDirW, MAX_PATH);
    hr = IShellFolder_ParseDisplayName(IDesktopFolder, 
        NULL, NULL, cTestDirW, NULL, &newPIDL, 0);
    ok((hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) || (hr == E_FAIL), 
        "ParseDisplayName returned %08lx, expected 80070002 or E_FAIL\n", hr);

    res = GetFileAttributesA(cNonExistDir2A);
    if(res != INVALID_FILE_ATTRIBUTES) return;

    MultiByteToWideChar(CP_ACP, 0, cNonExistDir2A, -1, cTestDirW, MAX_PATH);
    hr = IShellFolder_ParseDisplayName(IDesktopFolder, 
        NULL, NULL, cTestDirW, NULL, &newPIDL, 0);
    ok((hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) || (hr == E_FAIL) || (hr == E_INVALIDARG), 
        "ParseDisplayName returned %08lx, expected 80070002, E_FAIL or E_INVALIDARG\n", hr);
}

/* creates a file with the specified name for tests */
static void CreateTestFile(const CHAR *name)
{
    HANDLE file;
    DWORD written;

    file = CreateFileA(name, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
	WriteFile(file, name, strlen(name), &written, NULL);
	WriteFile(file, "\n", strlen("\n"), &written, NULL);
	CloseHandle(file);
    }
}


/* initializes the tests */
static void CreateFilesFolders(void)
{
    CreateDirectoryA(".\\testdir", NULL);
    CreateDirectoryA(".\\testdir\\test.txt", NULL);
    CreateTestFile  (".\\testdir\\test1.txt ");
    CreateTestFile  (".\\testdir\\test2.txt ");
    CreateTestFile  (".\\testdir\\test3.txt ");
    CreateDirectoryA(".\\testdir\\testdir2 ", NULL);
    CreateDirectoryA(".\\testdir\\testdir2\\subdir", NULL);
}

/* cleans after tests */
static void Cleanup(void)
{
    DeleteFileA(".\\testdir\\test1.txt");
    DeleteFileA(".\\testdir\\test2.txt");
    DeleteFileA(".\\testdir\\test3.txt");
    RemoveDirectoryA(".\\testdir\\test.txt");
    RemoveDirectoryA(".\\testdir\\testdir2\\subdir");
    RemoveDirectoryA(".\\testdir\\testdir2");
    RemoveDirectoryA(".\\testdir");
}


/* perform test */
static void test_EnumObjects(IShellFolder *iFolder)
{
    IEnumIDList *iEnumList;
    LPITEMIDLIST newPIDL, idlArr[10];
    ULONG NumPIDLs;
    int i=0, j;
    HRESULT hr;

    static const WORD iResults [5][5] =
    {
	{ 0,-1,-1,-1,-1},
	{ 1, 0,-1,-1,-1},
	{ 1, 1, 0,-1,-1},
	{ 1, 1, 1, 0,-1},
	{ 1, 1, 1, 1, 0}
    };

#define SFGAO_testfor SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_FILESYSANCESTOR | SFGAO_CAPABILITYMASK
    /* Don't test for SFGAO_HASSUBFOLDER since we return real state and native cached */
    static const ULONG attrs[5] =
    {
        SFGAO_CAPABILITYMASK | SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_FILESYSANCESTOR,
        SFGAO_CAPABILITYMASK | SFGAO_FILESYSTEM | SFGAO_FOLDER | SFGAO_FILESYSANCESTOR,
        SFGAO_CAPABILITYMASK | SFGAO_FILESYSTEM,
        SFGAO_CAPABILITYMASK | SFGAO_FILESYSTEM,
        SFGAO_CAPABILITYMASK | SFGAO_FILESYSTEM,
    };

    hr = IShellFolder_EnumObjects(iFolder, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS | SHCONTF_INCLUDEHIDDEN, &iEnumList);
    ok(hr == S_OK, "EnumObjects failed %08lx\n", hr);

    /* This is to show that, contrary to what is said on MSDN, on IEnumIDList::Next,
     * the filesystem shellfolders return S_OK even if less than 'celt' items are
     * returned (in contrast to S_FALSE). We have to do it in a loop since WinXP
     * only ever returns a single entry per call. */
    while (IEnumIDList_Next(iEnumList, 10-i, &idlArr[i], &NumPIDLs) == S_OK) 
        i += NumPIDLs;
    ok (i == 5, "i: %d\n", i);

    hr = IEnumIDList_Release(iEnumList);
    ok(hr == S_OK, "IEnumIDList_Release failed %08lx\n", hr);
    
    /* Sort them first in case of wrong order from system */
    for (i=0;i<5;i++) for (j=0;j<5;j++)
        if ((SHORT)IShellFolder_CompareIDs(iFolder, 0, idlArr[i], idlArr[j]) < 0)
	{
            newPIDL = idlArr[i];
            idlArr[i] = idlArr[j];
            idlArr[j] = newPIDL;
        }
	    
    for (i=0;i<5;i++) for (j=0;j<5;j++)
    {
        hr = IShellFolder_CompareIDs(iFolder, 0, idlArr[i], idlArr[j]);
        ok(hr == iResults[i][j], "Got %lx expected [%d]-[%d]=%x\n", hr, i, j, iResults[i][j]);
    }


    for (i = 0; i < 5; i++)
    {
        SFGAOF flags;
        /* Native returns all flags no matter what we ask for */
        flags = SFGAO_CANCOPY;
        hr = IShellFolder_GetAttributesOf(iFolder, 1, (LPCITEMIDLIST*)(idlArr + i), &flags);
        flags &= SFGAO_testfor;
        ok(hr == S_OK, "GetAttributesOf returns %08lx\n", hr);
        ok(flags == (attrs[i]), "GetAttributesOf[%i] got %08lx, expected %08lx\n", i, flags, attrs[i]);

        flags = SFGAO_testfor;
        hr = IShellFolder_GetAttributesOf(iFolder, 1, (LPCITEMIDLIST*)(idlArr + i), &flags);
        flags &= SFGAO_testfor;
        ok(hr == S_OK, "GetAttributesOf returns %08lx\n", hr);
        ok(flags == attrs[i], "GetAttributesOf[%i] got %08lx, expected %08lx\n", i, flags, attrs[i]);
    }

    for (i=0;i<5;i++)
        IMalloc_Free(ppM, idlArr[i]);
}

static void test_BindToObject(void)
{
    HRESULT hr;
    UINT cChars;
    IShellFolder *psfDesktop, *psfChild, *psfMyComputer, *psfSystemDir;
    SHITEMID emptyitem = { 0, { 0 } };
    LPITEMIDLIST pidlMyComputer, pidlSystemDir, pidlEmpty = (LPITEMIDLIST)&emptyitem;
    WCHAR wszSystemDir[MAX_PATH];
    char szSystemDir[MAX_PATH];
    WCHAR wszMyComputer[] = { 
        ':',':','{','2','0','D','0','4','F','E','0','-','3','A','E','A','-','1','0','6','9','-',
        'A','2','D','8','-','0','8','0','0','2','B','3','0','3','0','9','D','}',0 };

    /* The following tests shows that BindToObject should fail with E_INVALIDARG if called
     * with an empty pidl. This is tested for Desktop, MyComputer and the FS ShellFolder
     */
    hr = SHGetDesktopFolder(&psfDesktop);
    ok (SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;
    
    hr = IShellFolder_BindToObject(psfDesktop, pidlEmpty, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, "Desktop's BindToObject should fail, when called with empty pidl! hr = %08lx\n", hr);

    hr = IShellFolder_BindToObject(psfDesktop, NULL, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, "Desktop's BindToObject should fail, when called with NULL pidl! hr = %08lx\n", hr);

    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszMyComputer, NULL, &pidlMyComputer, NULL);
    ok (SUCCEEDED(hr), "Desktop's ParseDisplayName failed to parse MyComputer's CLSID! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }
    
    hr = IShellFolder_BindToObject(psfDesktop, pidlMyComputer, NULL, &IID_IShellFolder, (LPVOID*)&psfMyComputer);
    ok (SUCCEEDED(hr), "Desktop failed to bind to MyComputer object! hr = %08lx\n", hr);
    IShellFolder_Release(psfDesktop);
    IMalloc_Free(ppM, pidlMyComputer);
    if (FAILED(hr)) return;

    hr = IShellFolder_BindToObject(psfMyComputer, pidlEmpty, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, "MyComputers's BindToObject should fail, when called with empty pidl! hr = %08lx\n", hr);

#if 0
    /* this call segfaults on 98SE */
    hr = IShellFolder_BindToObject(psfMyComputer, NULL, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, "MyComputers's BindToObject should fail, when called with NULL pidl! hr = %08lx\n", hr);
#endif

    cChars = GetSystemDirectoryA(szSystemDir, MAX_PATH);
    ok (cChars > 0 && cChars < MAX_PATH, "GetSystemDirectoryA failed! LastError: %08lx\n", GetLastError());
    if (cChars == 0 || cChars >= MAX_PATH) {
        IShellFolder_Release(psfMyComputer);
        return;
    }
    MultiByteToWideChar(CP_ACP, 0, szSystemDir, -1, wszSystemDir, MAX_PATH);
    
    hr = IShellFolder_ParseDisplayName(psfMyComputer, NULL, NULL, wszSystemDir, NULL, &pidlSystemDir, NULL);
    ok (SUCCEEDED(hr), "MyComputers's ParseDisplayName failed to parse the SystemDirectory! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfMyComputer);
        return;
    }

    hr = IShellFolder_BindToObject(psfMyComputer, pidlSystemDir, NULL, &IID_IShellFolder, (LPVOID*)&psfSystemDir);
    ok (SUCCEEDED(hr), "MyComputer failed to bind to a FileSystem ShellFolder! hr = %08lx\n", hr);
    IShellFolder_Release(psfMyComputer);
    IMalloc_Free(ppM, pidlSystemDir);
    if (FAILED(hr)) return;

    hr = IShellFolder_BindToObject(psfSystemDir, pidlEmpty, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, 
        "FileSystem ShellFolder's BindToObject should fail, when called with empty pidl! hr = %08lx\n", hr);
    
#if 0
    /* this call segfaults on 98SE */
    hr = IShellFolder_BindToObject(psfSystemDir, NULL, NULL, &IID_IShellFolder, (LPVOID*)&psfChild);
    ok (hr == E_INVALIDARG, 
        "FileSystem ShellFolder's BindToObject should fail, when called with NULL pidl! hr = %08lx\n", hr);
#endif

    IShellFolder_Release(psfSystemDir);
}
  
static void test_GetDisplayName(void)
{
    BOOL result;
    HRESULT hr;
    HANDLE hTestFile;
    WCHAR wszTestFile[MAX_PATH], wszTestFile2[MAX_PATH], wszTestDir[MAX_PATH];
    char szTestFile[MAX_PATH], szTestDir[MAX_PATH];
    STRRET strret;
    LPSHELLFOLDER psfDesktop, psfPersonal;
    IUnknown *psfFile;
    SHITEMID emptyitem = { 0, { 0 } };
    LPITEMIDLIST pidlTestFile, pidlEmpty = (LPITEMIDLIST)&emptyitem;
    LPCITEMIDLIST pidlLast;
    static const WCHAR wszFileName[] = { 'w','i','n','e','t','e','s','t','.','f','o','o',0 };
    static const WCHAR wszDirName[] = { 'w','i','n','e','t','e','s','t',0 };

    /* I'm trying to figure if there is a functional difference between calling
     * SHGetPathFromIDList and calling GetDisplayNameOf(SHGDN_FORPARSING) after
     * binding to the shellfolder. One thing I thought of was that perhaps 
     * SHGetPathFromIDList would be able to get the path to a file, which does
     * not exist anymore, while the other method would'nt. It turns out there's
     * no functional difference in this respect.
     */

    if(!pSHGetSpecialFolderPathW) return;

    /* First creating a directory in MyDocuments and a file in this directory. */
    result = pSHGetSpecialFolderPathW(NULL, wszTestDir, CSIDL_PERSONAL, FALSE);
    ok(result, "SHGetSpecialFolderPathW failed! Last error: %08lx\n", GetLastError());
    if (!result) return;

    PathAddBackslashW(wszTestDir);
    lstrcatW(wszTestDir, wszDirName);
    WideCharToMultiByte(CP_ACP, 0, wszTestDir, -1, szTestDir, MAX_PATH, 0, 0);
    result = CreateDirectoryA(szTestDir, NULL);
    ok(result, "CreateDirectoryA failed! Last error: %08lx\n", GetLastError());
    if (!result) return;

    lstrcpyW(wszTestFile, wszTestDir);
    PathAddBackslashW(wszTestFile);
    lstrcatW(wszTestFile, wszFileName);
    WideCharToMultiByte(CP_ACP, 0, wszTestFile, -1, szTestFile, MAX_PATH, 0, 0);

    hTestFile = CreateFileA(szTestFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    ok((hTestFile != INVALID_HANDLE_VALUE), "CreateFileA failed! Last error: %08lx\n", GetLastError());
    if (hTestFile == INVALID_HANDLE_VALUE) return;
    CloseHandle(hTestFile);

    /* Getting an itemidlist for the file. */
    hr = SHGetDesktopFolder(&psfDesktop);
    ok(SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszTestFile, NULL, &pidlTestFile, NULL);
    ok(SUCCEEDED(hr), "Desktop->ParseDisplayName failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    /* It seems as if we cannot bind to regular files on windows, but only directories. 
     */
    hr = IShellFolder_BindToObject(psfDesktop, pidlTestFile, NULL, &IID_IUnknown, (VOID**)&psfFile);
    todo_wine { ok (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), "hr = %08lx\n", hr); }
    if (SUCCEEDED(hr)) {
        IShellFolder_Release(psfFile);
    }
    
    /* Deleting the file and the directory */
    DeleteFileA(szTestFile);
    RemoveDirectoryA(szTestDir);

    /* SHGetPathFromIDListW still works, although the file is not present anymore. */
    result = SHGetPathFromIDListW(pidlTestFile, wszTestFile2);
    ok (result, "SHGetPathFromIDListW failed! Last error: %08lx\n", GetLastError());
    ok (!lstrcmpiW(wszTestFile, wszTestFile2), "SHGetPathFromIDListW returns incorrect path!\n");

    if(!pSHBindToParent) return;

    /* SHBindToParent fails, if called with a NULL PIDL. */
    hr = pSHBindToParent(NULL, &IID_IShellFolder, (VOID**)&psfPersonal, &pidlLast);
    ok (FAILED(hr), "SHBindToParent(NULL) should fail!\n");

    /* But it succeeds with an empty PIDL. */
    hr = pSHBindToParent(pidlEmpty, &IID_IShellFolder, (VOID**)&psfPersonal, &pidlLast);
    ok (SUCCEEDED(hr), "SHBindToParent(empty PIDL) should succeed! hr = %08lx\n", hr);
    ok (pidlLast == pidlEmpty, "The last element of an empty PIDL should be the PIDL itself!\n");
    if (SUCCEEDED(hr)) 
        IShellFolder_Release(psfPersonal);
    
    /* Binding to the folder and querying the display name of the file also works. */
    hr = pSHBindToParent(pidlTestFile, &IID_IShellFolder, (VOID**)&psfPersonal, &pidlLast); 
    ok (SUCCEEDED(hr), "SHBindToParent failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    /* This test shows that Windows doesn't allocate a new pidlLast, but returns a pointer into 
     * pidlTestFile (In accordance with MSDN). */
    ok (pILFindLastID(pidlTestFile) == pidlLast, 
                                "SHBindToParent doesn't return the last id of the pidl param!\n");
    
    hr = IShellFolder_GetDisplayNameOf(psfPersonal, pidlLast, SHGDN_FORPARSING, &strret);
    ok (SUCCEEDED(hr), "Personal->GetDisplayNameOf failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        IShellFolder_Release(psfPersonal);
        return;
    }

    if (pStrRetToBufW)
    {
        hr = pStrRetToBufW(&strret, pidlLast, wszTestFile2, MAX_PATH);
        ok (SUCCEEDED(hr), "StrRetToBufW failed! hr = %08lx\n", hr);
        ok (!lstrcmpiW(wszTestFile, wszTestFile2), "GetDisplayNameOf returns incorrect path!\n");
    }
    
    IShellFolder_Release(psfDesktop);
    IShellFolder_Release(psfPersonal);
}

static void test_CallForAttributes(void)
{
    HKEY hKey;
    LONG lResult;
    HRESULT hr;
    DWORD dwSize;
    LPSHELLFOLDER psfDesktop;
    LPITEMIDLIST pidlMyDocuments;
    DWORD dwAttributes, dwCallForAttributes, dwOrigAttributes, dwOrigCallForAttributes;
    static const WCHAR wszAttributes[] = { 'A','t','t','r','i','b','u','t','e','s',0 };
    static const WCHAR wszCallForAttributes[] = { 
        'C','a','l','l','F','o','r','A','t','t','r','i','b','u','t','e','s',0 };
    static const WCHAR wszMyDocumentsKey[] = {
        'C','L','S','I','D','\\','{','4','5','0','D','8','F','B','A','-','A','D','2','5','-',
        '1','1','D','0','-','9','8','A','8','-','0','8','0','0','3','6','1','B','1','1','0','3','}',
        '\\','S','h','e','l','l','F','o','l','d','e','r',0 };
    WCHAR wszMyDocuments[] = {
        ':',':','{','4','5','0','D','8','F','B','A','-','A','D','2','5','-','1','1','D','0','-',
        '9','8','A','8','-','0','8','0','0','3','6','1','B','1','1','0','3','}',0 };
    
    /* For the root of a namespace extension, the attributes are not queried by binding
     * to the object and calling GetAttributesOf. Instead, the attributes are read from 
     * the registry value HKCR/CLSID/{...}/ShellFolder/Attributes. This is documented on MSDN.
     *
     * The MyDocuments shellfolder on WinXP has a HKCR/CLSID/{...}/ShellFolder/CallForAttributes
     * value. It seems that if the folder is queried for one of the flags set in CallForAttributes,
     * the shell does bind to the folder object and calls GetAttributesOf. This is not documented
     * on MSDN. This test is meant to document the observed behaviour on WinXP SP2.
     */
    hr = SHGetDesktopFolder(&psfDesktop);
    ok (SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;
    
    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszMyDocuments, NULL, 
                                       &pidlMyDocuments, NULL);
    ok (SUCCEEDED(hr), 
        "Desktop's ParseDisplayName failed to parse MyDocuments's CLSID! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    dwAttributes = 0xffffffff;
    hr = IShellFolder_GetAttributesOf(psfDesktop, 1, 
                                      (LPCITEMIDLIST*)&pidlMyDocuments, &dwAttributes);
    ok (SUCCEEDED(hr), "Desktop->GetAttributesOf(MyDocuments) failed! hr = %08lx\n", hr);

    /* We need the following setup (as observed on WinXP SP2), for the tests to make sense. */
    ok (dwAttributes & SFGAO_FILESYSTEM, "SFGAO_FILESYSTEM attribute is not set for MyDocuments!\n");
    ok (!(dwAttributes & SFGAO_ISSLOW), "SFGAO_ISSLOW attribute is set for MyDocuments!\n");
    ok (!(dwAttributes & SFGAO_GHOSTED), "SFGAO_GHOSTED attribute is set for MyDocuments!\n");

    /* We don't have the MyDocuments shellfolder in wine yet, and thus we don't have the registry
     * key. So the test will return at this point, if run on wine. 
     */
    lResult = RegOpenKeyExW(HKEY_CLASSES_ROOT, wszMyDocumentsKey, 0, KEY_WRITE|KEY_READ, &hKey);
    ok (lResult == ERROR_SUCCESS, "RegOpenKeyEx failed! result: %08lx\n", lResult);
    if (lResult != ERROR_SUCCESS) {
        IMalloc_Free(ppM, pidlMyDocuments);
        IShellFolder_Release(psfDesktop);
        return;
    }
    
    /* Query MyDocuments' Attributes value, to be able to restore it later. */
    dwSize = sizeof(DWORD);
    lResult = RegQueryValueExW(hKey, wszAttributes, NULL, NULL, (LPBYTE)&dwOrigAttributes, &dwSize);
    ok (lResult == ERROR_SUCCESS, "RegQueryValueEx failed! result: %08lx\n", lResult);
    if (lResult != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        IMalloc_Free(ppM, pidlMyDocuments);
        IShellFolder_Release(psfDesktop);
        return;
    }

    /* Query MyDocuments' CallForAttributes value, to be able to restore it later. */
    dwSize = sizeof(DWORD);
    lResult = RegQueryValueExW(hKey, wszCallForAttributes, NULL, NULL, 
                              (LPBYTE)&dwOrigCallForAttributes, &dwSize);
    ok (lResult == ERROR_SUCCESS, "RegQueryValueEx failed! result: %08lx\n", lResult);
    if (lResult != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        IMalloc_Free(ppM, pidlMyDocuments);
        IShellFolder_Release(psfDesktop);
        return;
    }
    
    /* Define via the Attributes value that MyDocuments attributes are SFGAO_ISSLOW and 
     * SFGAO_GHOSTED and that MyDocuments should be called for the SFGAO_ISSLOW and
     * SFGAO_FILESYSTEM attributes. */
    dwAttributes = SFGAO_ISSLOW|SFGAO_GHOSTED;
    RegSetValueExW(hKey, wszAttributes, 0, REG_DWORD, (LPBYTE)&dwAttributes, sizeof(DWORD));
    dwCallForAttributes = SFGAO_ISSLOW|SFGAO_FILESYSTEM;
    RegSetValueExW(hKey, wszCallForAttributes, 0, REG_DWORD, 
                   (LPBYTE)&dwCallForAttributes, sizeof(DWORD));

    /* Although it is not set in CallForAttributes, the SFGAO_GHOSTED flag is reset by 
     * GetAttributesOf. It seems that once there is a single attribute queried, for which
     * CallForAttributes is set, all flags are taken from the GetAttributesOf call and
     * the flags in Attributes are ignored. 
     */
    dwAttributes = SFGAO_ISSLOW|SFGAO_GHOSTED|SFGAO_FILESYSTEM;
    hr = IShellFolder_GetAttributesOf(psfDesktop, 1, 
                                      (LPCITEMIDLIST*)&pidlMyDocuments, &dwAttributes);
    ok (SUCCEEDED(hr), "Desktop->GetAttributesOf(MyDocuments) failed! hr = %08lx\n", hr);
    if (SUCCEEDED(hr)) 
        ok (dwAttributes == SFGAO_FILESYSTEM, 
            "Desktop->GetAttributes(MyDocuments) returned unexpected attributes: %08lx\n", 
            dwAttributes);

    /* Restore MyDocuments' original Attributes and CallForAttributes registry values */
    RegSetValueExW(hKey, wszAttributes, 0, REG_DWORD, (LPBYTE)&dwOrigAttributes, sizeof(DWORD));
    RegSetValueExW(hKey, wszCallForAttributes, 0, REG_DWORD, 
                   (LPBYTE)&dwOrigCallForAttributes, sizeof(DWORD));
    RegCloseKey(hKey);
    IMalloc_Free(ppM, pidlMyDocuments);
    IShellFolder_Release(psfDesktop);
}

static void test_GetAttributesOf(void) 
{
    HRESULT hr;
    LPSHELLFOLDER psfDesktop, psfMyComputer;
    SHITEMID emptyitem = { 0, { 0 } };
    LPCITEMIDLIST pidlEmpty = (LPCITEMIDLIST)&emptyitem;
    LPITEMIDLIST pidlMyComputer;
    DWORD dwFlags;
    static const DWORD dwDesktopFlags = /* As observed on WinXP SP2 */
        SFGAO_STORAGE | SFGAO_HASPROPSHEET | SFGAO_STORAGEANCESTOR |
        SFGAO_FILESYSANCESTOR | SFGAO_FOLDER | SFGAO_FILESYSTEM | SFGAO_HASSUBFOLDER;
    static const DWORD dwMyComputerFlags = /* As observed on WinXP SP2 */
        SFGAO_CANRENAME | SFGAO_CANDELETE | SFGAO_HASPROPSHEET |
        SFGAO_DROPTARGET | SFGAO_FILESYSANCESTOR | SFGAO_FOLDER | SFGAO_HASSUBFOLDER;
    WCHAR wszMyComputer[] = { 
        ':',':','{','2','0','D','0','4','F','E','0','-','3','A','E','A','-','1','0','6','9','-',
        'A','2','D','8','-','0','8','0','0','2','B','3','0','3','0','9','D','}',0 };

    hr = SHGetDesktopFolder(&psfDesktop);
    ok (SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    /* The Desktop attributes can be queried with a single empty itemidlist, .. */
    dwFlags = 0xffffffff;
    hr = IShellFolder_GetAttributesOf(psfDesktop, 1, &pidlEmpty, &dwFlags);
    ok (SUCCEEDED(hr), "Desktop->GetAttributesOf(empty pidl) failed! hr = %08lx\n", hr);
    ok (dwFlags == dwDesktopFlags, "Wrong Desktop attributes: %08lx, expected: %08lx\n", 
        dwFlags, dwDesktopFlags);

    /* .. or with no itemidlist at all. */
    dwFlags = 0xffffffff;
    hr = IShellFolder_GetAttributesOf(psfDesktop, 0, NULL, &dwFlags);
    ok (SUCCEEDED(hr), "Desktop->GetAttributesOf(NULL) failed! hr = %08lx\n", hr);
    ok (dwFlags == dwDesktopFlags, "Wrong Desktop attributes: %08lx, expected: %08lx\n", 
        dwFlags, dwDesktopFlags);
   
    /* Testing the attributes of the MyComputer shellfolder */
    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszMyComputer, NULL, &pidlMyComputer, NULL);
    ok (SUCCEEDED(hr), "Desktop's ParseDisplayName failed to parse MyComputer's CLSID! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    /* WinXP SP2 sets the SFGAO_CANLINK flag, when MyComputer is queried via the Desktop 
     * folder object. It doesn't do this, if MyComputer is queried directly (see below).
     * SFGAO_CANLINK is the same as DROPEFFECT_LINK, which MSDN says means: "Drag source
     * should create a link to the original data". You can't create links on MyComputer on
     * Windows, so this flag shouldn't be set. Seems like a bug in Windows. As long as nobody
     * depends on this bug, we probably shouldn't imitate it.
     */
    dwFlags = 0xffffffff;
    hr = IShellFolder_GetAttributesOf(psfDesktop, 1, (LPCITEMIDLIST*)&pidlMyComputer, &dwFlags);
    ok (SUCCEEDED(hr), "Desktop->GetAttributesOf(MyComputer) failed! hr = %08lx\n", hr);
    todo_wine { ok ((dwFlags & ~(DWORD)SFGAO_CANLINK) == dwMyComputerFlags, 
                    "Wrong MyComputer attributes: %08lx, expected: %08lx\n", dwFlags, dwMyComputerFlags); }

    hr = IShellFolder_BindToObject(psfDesktop, pidlMyComputer, NULL, &IID_IShellFolder, (LPVOID*)&psfMyComputer);
    ok (SUCCEEDED(hr), "Desktop failed to bind to MyComputer object! hr = %08lx\n", hr);
    IShellFolder_Release(psfDesktop);
    IMalloc_Free(ppM, pidlMyComputer);
    if (FAILED(hr)) return;

    hr = IShellFolder_GetAttributesOf(psfMyComputer, 1, &pidlEmpty, &dwFlags);
    todo_wine {ok (hr == E_INVALIDARG, "MyComputer->GetAttributesOf(emtpy pidl) should fail! hr = %08lx\n", hr); }

    dwFlags = 0xffffffff;
    hr = IShellFolder_GetAttributesOf(psfMyComputer, 0, NULL, &dwFlags);
    ok (SUCCEEDED(hr), "MyComputer->GetAttributesOf(NULL) failed! hr = %08lx\n", hr); 
    todo_wine { ok (dwFlags == dwMyComputerFlags, 
                    "Wrong MyComputer attributes: %08lx, expected: %08lx\n", dwFlags, dwMyComputerFlags); }

    IShellFolder_Release(psfMyComputer);
}    

static void test_SHGetPathFromIDList(void)
{
    SHITEMID emptyitem = { 0, { 0 } };
    LPCITEMIDLIST pidlEmpty = (LPCITEMIDLIST)&emptyitem;
    LPITEMIDLIST pidlMyComputer;
    WCHAR wszPath[MAX_PATH], wszDesktop[MAX_PATH];
    BOOL result;
    HRESULT hr;
    LPSHELLFOLDER psfDesktop;
    WCHAR wszMyComputer[] = { 
        ':',':','{','2','0','D','0','4','F','E','0','-','3','A','E','A','-','1','0','6','9','-',
        'A','2','D','8','-','0','8','0','0','2','B','3','0','3','0','9','D','}',0 };
    WCHAR wszFileName[MAX_PATH];
    LPITEMIDLIST pidlTestFile;
    HANDLE hTestFile;
    STRRET strret;
    static WCHAR wszTestFile[] = {
        'w','i','n','e','t','e','s','t','.','f','o','o',0 };

    if(!pSHGetSpecialFolderPathW) return;

    /* Calling SHGetPathFromIDList with an empty pidl should return the desktop folder's path. */
    result = pSHGetSpecialFolderPathW(NULL, wszDesktop, CSIDL_DESKTOP, FALSE);
    ok(result, "SHGetSpecialFolderPathW(CSIDL_DESKTOP) failed! Last error: %08lx\n", GetLastError());
    if (!result) return;
    
    result = SHGetPathFromIDListW(pidlEmpty, wszPath);
    ok(result, "SHGetPathFromIDListW failed! Last error: %08lx\n", GetLastError());
    if (!result) return;
    ok(!lstrcmpiW(wszDesktop, wszPath), "SHGetPathFromIDList didn't return desktop path for empty pidl!\n");

    /* MyComputer does not map to a filesystem path. SHGetPathFromIDList should fail. */
    hr = SHGetDesktopFolder(&psfDesktop);
    ok (SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszMyComputer, NULL, &pidlMyComputer, NULL);
    ok (SUCCEEDED(hr), "Desktop's ParseDisplayName failed to parse MyComputer's CLSID! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    SetLastError(0xdeadbeef);
    result = SHGetPathFromIDListW(pidlMyComputer, wszPath);
    ok (!result, "SHGetPathFromIDList succeeded where it shouldn't!\n");
    ok (GetLastError()==0xdeadbeef, "SHGetPathFromIDList shouldn't set last error! Last error: %08lx\n", GetLastError());
    if (result) {
        IShellFolder_Release(psfDesktop);
        return;
    }

    IMalloc_Free(ppM, pidlMyComputer);

    result = pSHGetSpecialFolderPathW(NULL, wszFileName, CSIDL_DESKTOPDIRECTORY, FALSE);
    ok(result, "SHGetSpecialFolderPathW failed! Last error: %08lx\n", GetLastError());
    if (!result) {
        IShellFolder_Release(psfDesktop);
        return;
    }
    PathAddBackslashW(wszFileName);
    lstrcatW(wszFileName, wszTestFile);
    hTestFile = CreateFileW(wszFileName, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
    ok(hTestFile != INVALID_HANDLE_VALUE, "CreateFileW failed! Last error: %08lx\n", GetLastError());
    if (hTestFile == INVALID_HANDLE_VALUE) {
        IShellFolder_Release(psfDesktop);
        return;
    }
    CloseHandle(hTestFile);

    hr = IShellFolder_ParseDisplayName(psfDesktop, NULL, NULL, wszTestFile, NULL, &pidlTestFile, NULL);
    ok (SUCCEEDED(hr), "Desktop's ParseDisplayName failed to parse filename hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(psfDesktop);
        DeleteFileW(wszFileName);
        IMalloc_Free(ppM, pidlTestFile);
        return;
    }

    /* This test is to show that the Desktop shellfolder prepends the CSIDL_DESKTOPDIRECTORY
     * path for files placed on the desktop, if called with SHGDN_FORPARSING. */
    hr = IShellFolder_GetDisplayNameOf(psfDesktop, pidlTestFile, SHGDN_FORPARSING, &strret);
    ok (SUCCEEDED(hr), "Desktop's GetDisplayNamfOf failed! hr = %08lx\n", hr);
    IShellFolder_Release(psfDesktop);
    DeleteFileW(wszFileName);
    if (FAILED(hr)) {
        IMalloc_Free(ppM, pidlTestFile);
        return;
    }
    if (pStrRetToBufW)
    {
        pStrRetToBufW(&strret, pidlTestFile, wszPath, MAX_PATH);
        ok(0 == lstrcmpW(wszFileName, wszPath), 
           "Desktop->GetDisplayNameOf(pidlTestFile, SHGDN_FORPARSING) "
           "returned incorrect path for file placed on desktop\n");
    }

    result = SHGetPathFromIDListW(pidlTestFile, wszPath);
    ok(result, "SHGetPathFromIDListW failed! Last error: %08lx\n", GetLastError());
    IMalloc_Free(ppM, pidlTestFile);
    if (!result) return;
    ok(0 == lstrcmpW(wszFileName, wszPath), "SHGetPathFromIDListW returned incorrect path for file placed on desktop\n");
}

static void test_EnumObjects_and_CompareIDs(void)
{
    ITEMIDLIST *newPIDL;
    IShellFolder *IDesktopFolder, *testIShellFolder;
    char  cCurrDirA [MAX_PATH] = {0};
    WCHAR cCurrDirW [MAX_PATH];
    static const WCHAR cTestDirW[] = {'\\','t','e','s','t','d','i','r',0};
    int len;
    HRESULT hr;

    GetCurrentDirectoryA(MAX_PATH, cCurrDirA);
    len = lstrlenA(cCurrDirA);

    if(len == 0) {
        trace("GetCurrentDirectoryA returned empty string. Skipping test_EnumObjects_and_CompareIDs\n");
        return;
    }
    if(cCurrDirA[len-1] == '\\')
        cCurrDirA[len-1] = 0;

    MultiByteToWideChar(CP_ACP, 0, cCurrDirA, -1, cCurrDirW, MAX_PATH);
    strcatW(cCurrDirW, cTestDirW);

    hr = SHGetDesktopFolder(&IDesktopFolder);
    ok(hr == S_OK, "SHGetDesktopfolder failed %08lx\n", hr);

    CreateFilesFolders();

    hr = IShellFolder_ParseDisplayName(IDesktopFolder, NULL, NULL, cCurrDirW, NULL, &newPIDL, 0);
    ok(hr == S_OK, "ParseDisplayName failed %08lx\n", hr);

    hr = IShellFolder_BindToObject(IDesktopFolder, newPIDL, NULL, (REFIID)&IID_IShellFolder, (LPVOID *)&testIShellFolder);
    ok(hr == S_OK, "BindToObject failed %08lx\n", hr);

    test_EnumObjects(testIShellFolder);

    hr = IShellFolder_Release(testIShellFolder);
    ok(hr == S_OK, "IShellFolder_Release failed %08lx\n", hr);

    Cleanup();

    IMalloc_Free(ppM, newPIDL);
}

/* A simple implementation of an IPropertyBag, which returns fixed values for
 * 'Target' and 'Attributes' properties.
 */
static HRESULT WINAPI InitPropertyBag_IPropertyBag_QueryInterface(IPropertyBag *iface, REFIID riid,
    void **ppvObject) 
{
    if (!ppvObject)
        return E_INVALIDARG;

    if (IsEqualIID(&IID_IUnknown, riid) || IsEqualIID(&IID_IPropertyBag, riid)) {
        *ppvObject = iface;
    } else {
        ok (FALSE, "InitPropertyBag asked for unknown interface!\n");
        return E_NOINTERFACE;
    }

    IPropertyBag_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI InitPropertyBag_IPropertyBag_AddRef(IPropertyBag *iface) {
    return 2;
}

static ULONG WINAPI InitPropertyBag_IPropertyBag_Release(IPropertyBag *iface) {
    return 1;
}

static HRESULT WINAPI InitPropertyBag_IPropertyBag_Read(IPropertyBag *iface, LPCOLESTR pszPropName,
    VARIANT *pVar, IErrorLog *pErrorLog)
{
    static const WCHAR wszTargetSpecialFolder[] = {
        'T','a','r','g','e','t','S','p','e','c','i','a','l','F','o','l','d','e','r',0 };
    static const WCHAR wszTarget[] = {
        'T','a','r','g','e','t',0 };
    static const WCHAR wszAttributes[] = {
        'A','t','t','r','i','b','u','t','e','s',0 };
    static const WCHAR wszResolveLinkFlags[] = {
        'R','e','s','o','l','v','e','L','i','n','k','F','l','a','g','s',0 };
       
    if (!lstrcmpW(pszPropName, wszTargetSpecialFolder)) {
        ok(V_VT(pVar) == VT_I4, "Wrong variant type for 'TargetSpecialFolder' property!\n");
        return E_INVALIDARG;
    }
    
    if (!lstrcmpW(pszPropName, wszResolveLinkFlags)) 
    {
        ok(V_VT(pVar) == VT_UI4, "Wrong variant type for 'ResolveLinkFlags' property!\n");
        return E_INVALIDARG;
    }

    if (!lstrcmpW(pszPropName, wszTarget)) {
        WCHAR wszPath[MAX_PATH];
        BOOL result;
        
        ok(V_VT(pVar) == VT_BSTR, "Wrong variant type for 'Target' property!\n");
        if (V_VT(pVar) != VT_BSTR) return E_INVALIDARG;

        result = pSHGetSpecialFolderPathW(NULL, wszPath, CSIDL_DESKTOPDIRECTORY, FALSE);
        ok(result, "SHGetSpecialFolderPathW(DESKTOPDIRECTORY) failed! x%08lx\n", GetLastError());
        if (!result) return E_INVALIDARG;

        V_BSTR(pVar) = SysAllocString(wszPath);
        return S_OK;
    }

    if (!lstrcmpW(pszPropName, wszAttributes)) {
        ok(V_VT(pVar) == VT_UI4, "Wrong variant type for 'Attributes' property!\n");
        if (V_VT(pVar) != VT_UI4) return E_INVALIDARG;
        V_UI4(pVar) = SFGAO_FOLDER|SFGAO_HASSUBFOLDER|SFGAO_FILESYSANCESTOR|
                      SFGAO_CANRENAME|SFGAO_FILESYSTEM;
        return S_OK;
    }

    ok(FALSE, "PropertyBag was asked for unknown property (vt=%d)!\n", V_VT(pVar));
    return E_INVALIDARG;
}

static HRESULT WINAPI InitPropertyBag_IPropertyBag_Write(IPropertyBag *iface, LPCOLESTR pszPropName,
    VARIANT *pVar)
{
    ok(FALSE, "Unexpected call to IPropertyBag_Write\n");
    return E_NOTIMPL;
}
    
static const IPropertyBagVtbl InitPropertyBag_IPropertyBagVtbl = {
    InitPropertyBag_IPropertyBag_QueryInterface,
    InitPropertyBag_IPropertyBag_AddRef,
    InitPropertyBag_IPropertyBag_Release,
    InitPropertyBag_IPropertyBag_Read,
    InitPropertyBag_IPropertyBag_Write
};

struct IPropertyBag InitPropertyBag = {
    &InitPropertyBag_IPropertyBagVtbl
};

void test_FolderShortcut(void) {
    IPersistPropertyBag *pPersistPropertyBag;
    IShellFolder *pShellFolder, *pDesktopFolder;
    IPersistFolder3 *pPersistFolder3;
    HRESULT hr;
    STRRET strret;
    WCHAR wszDesktopPath[MAX_PATH], wszBuffer[MAX_PATH];
    BOOL result;
    CLSID clsid;
    LPITEMIDLIST pidlCurrentFolder, pidlWineTestFolder, pidlSubFolder;
    WCHAR wszWineTestFolder[] = {
        ':',':','{','9','B','3','5','2','E','B','F','-','2','7','6','5','-','4','5','C','1','-',
        'B','4','C','6','-','8','5','C','C','7','F','7','A','B','C','6','4','}',0 };
    WCHAR wszSomeSubFolder[] = { 'S','u','b','F','o','l','d','e','r', 0};
    static const GUID CLSID_UnixDosFolder = 
        {0x9d20aae8, 0x0625, 0x44b0, {0x9c, 0xa7, 0x71, 0x88, 0x9c, 0x22, 0x54, 0xd9}};

    if (!pSHGetSpecialFolderPathW || !pStrRetToBufW) return;
   
    /* These tests basically show, that CLSID_FolderShortcuts are initialized
     * via their IPersistPropertyBag interface. And that the target folder
     * is taken from the IPropertyBag's 'Target' property.
     */
    hr = CoCreateInstance(&CLSID_FolderShortcut, NULL, CLSCTX_INPROC_SERVER, 
                          &IID_IPersistPropertyBag, (LPVOID*)&pPersistPropertyBag);
    ok (SUCCEEDED(hr), "CoCreateInstance failed! hr = 0x%08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IPersistPropertyBag_Load(pPersistPropertyBag, &InitPropertyBag, NULL);
    ok(SUCCEEDED(hr), "IPersistPropertyBag_Load failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IPersistPropertyBag_Release(pPersistPropertyBag);
        return;
    }
    
    hr = IPersistPropertyBag_QueryInterface(pPersistPropertyBag, &IID_IShellFolder, 
                                            (LPVOID*)&pShellFolder);
    IPersistPropertyBag_Release(pPersistPropertyBag);
    ok(SUCCEEDED(hr), "IPersistPropertyBag_QueryInterface(IShellFolder) failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IShellFolder_GetDisplayNameOf(pShellFolder, NULL, SHGDN_FORPARSING, &strret);
    ok(SUCCEEDED(hr), "IShellFolder_GetDisplayNameOf(NULL) failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(pShellFolder);
        return;
    }

    result = pSHGetSpecialFolderPathW(NULL, wszDesktopPath, CSIDL_DESKTOPDIRECTORY, FALSE);
    ok(result, "SHGetSpecialFolderPathW(CSIDL_DESKTOPDIRECTORY) failed! 0x%08lx\n", GetLastError());
    if (!result) return;

    pStrRetToBufW(&strret, NULL, wszBuffer, MAX_PATH);
    ok(!lstrcmpiW(wszDesktopPath, wszBuffer), "FolderShortcut returned incorrect folder!\n");

    hr = IShellFolder_QueryInterface(pShellFolder, &IID_IPersistFolder3, (LPVOID*)&pPersistFolder3);
    IShellFolder_Release(pShellFolder);
    ok(SUCCEEDED(hr), "IShellFolder_QueryInterface(IID_IPersistFolder3 failed! hr = 0x%08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IPersistFolder3_GetClassID(pPersistFolder3, &clsid);
    ok(SUCCEEDED(hr), "IPersistFolder3_GetClassID failed! hr=0x%08lx\n", hr);
    ok(IsEqualCLSID(&clsid, &CLSID_FolderShortcut), "Unexpected CLSID!\n");

    hr = IPersistFolder3_GetCurFolder(pPersistFolder3, &pidlCurrentFolder);
    ok(SUCCEEDED(hr), "IPersistFolder3_GetCurFolder failed! hr=0x%08lx\n", hr);
    ok(!pidlCurrentFolder, "IPersistFolder3_GetCurFolder should return a NULL pidl!\n");
                    
    /* For FolderShortcut objects, the Initialize method initialized the folder's position in the
     * shell namespace. The target folder, read from the property bag above, remains untouched. 
     * The following tests show this: The itemidlist for some imaginary shellfolder object
     * is created and the FolderShortcut is initialized with it. GetCurFolder now returns this
     * itemidlist, but GetDisplayNameOf still returns the path from above.
     */
    hr = SHGetDesktopFolder(&pDesktopFolder);
    ok (SUCCEEDED(hr), "SHGetDesktopFolder failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IShellFolder_ParseDisplayName(pDesktopFolder, NULL, NULL, wszWineTestFolder, NULL,
                                       &pidlWineTestFolder, NULL);
    IShellFolder_Release(pDesktopFolder);
    ok (SUCCEEDED(hr), "IShellFolder::ParseDisplayName failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IPersistFolder3_Initialize(pPersistFolder3, pidlWineTestFolder);
    ok (SUCCEEDED(hr), "IPersistFolder3::Initialize failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IPersistFolder3_Release(pPersistFolder3);
        ILFree(pidlWineTestFolder);
        return;
    }
    
    hr = IPersistFolder3_GetCurFolder(pPersistFolder3, &pidlCurrentFolder);
    ok(SUCCEEDED(hr), "IPersistFolder3_GetCurFolder failed! hr=0x%08lx\n", hr);
    ok(ILIsEqual(pidlCurrentFolder, pidlWineTestFolder), 
        "IPersistFolder3_GetCurFolder should return pidlWineTestFolder!\n");
    ILFree(pidlCurrentFolder);
    ILFree(pidlWineTestFolder);
 
    hr = IPersistFolder3_QueryInterface(pPersistFolder3, &IID_IShellFolder, (LPVOID*)&pShellFolder);
    IPersistFolder3_Release(pPersistFolder3);
    ok(SUCCEEDED(hr), "IPersistFolder3_QueryInterface(IShellFolder) failed! hr = %08lx\n", hr);
    if (FAILED(hr)) return;

    hr = IShellFolder_GetDisplayNameOf(pShellFolder, NULL, SHGDN_FORPARSING, &strret);
    ok(SUCCEEDED(hr), "IShellFolder_GetDisplayNameOf(NULL) failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(pShellFolder);
        return;
    }

    pStrRetToBufW(&strret, NULL, wszBuffer, MAX_PATH);
    ok(!lstrcmpiW(wszDesktopPath, wszBuffer), "FolderShortcut returned incorrect folder!\n");

    /* Next few lines are meant to show that children of FolderShortcuts are not FolderShortcuts,
     * but ShellFSFolders. */
    PathAddBackslashW(wszDesktopPath);
    lstrcatW(wszDesktopPath, wszSomeSubFolder);
    if (!CreateDirectoryW(wszDesktopPath, NULL)) {
        IShellFolder_Release(pShellFolder);
        return;
    }
    
    hr = IShellFolder_ParseDisplayName(pShellFolder, NULL, NULL, wszSomeSubFolder, NULL, 
                                       &pidlSubFolder, NULL);
    RemoveDirectoryW(wszDesktopPath);
    ok (SUCCEEDED(hr), "IShellFolder::ParseDisplayName failed! hr = %08lx\n", hr);
    if (FAILED(hr)) {
        IShellFolder_Release(pShellFolder);
        return;
    }

    hr = IShellFolder_BindToObject(pShellFolder, pidlSubFolder, NULL, &IID_IPersistFolder3, 
                                   (LPVOID*)&pPersistFolder3);
    IShellFolder_Release(pShellFolder);
    ILFree(pidlSubFolder);
    ok (SUCCEEDED(hr), "IShellFolder::BindToObject failed! hr = %08lx\n", hr);
    if (FAILED(hr)) 
        return;

    /* On windows, we expect CLSID_ShellFSFolder. On wine we relax this constraint
     * a little bit and also allow CLSID_UnixDosFolder. */
    hr = IPersistFolder3_GetClassID(pPersistFolder3, &clsid);
    ok(SUCCEEDED(hr), "IPersistFolder3_GetClassID failed! hr=0x%08lx\n", hr);
    ok(IsEqualCLSID(&clsid, &CLSID_ShellFSFolder) || IsEqualCLSID(&clsid, &CLSID_UnixDosFolder),
        "IPersistFolder3::GetClassID returned unexpected CLSID!\n");

    IPersistFolder3_Release(pPersistFolder3);
}

START_TEST(shlfolder)
{
    init_function_pointers();
    /* if OleInitialize doesn't get called, ParseDisplayName returns
       CO_E_NOTINITIALIZED for malformed directory names on win2k. */
    OleInitialize(NULL);

    test_ParseDisplayName();
    test_BindToObject();
    test_EnumObjects_and_CompareIDs();
    test_GetDisplayName();
    test_GetAttributesOf();
    test_SHGetPathFromIDList();
    test_CallForAttributes();
    test_FolderShortcut();

    OleUninitialize();
}
