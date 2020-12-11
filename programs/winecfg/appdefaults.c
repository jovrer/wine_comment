/*
 * WineCfg app settings tabsheet
 *
 * Copyright 2004 Robert van Herk
 * Copyright 2004 Chris Morgan
 * Copyright 2004 Mike Hearn
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
 *
 */

#define NONAMELESSUNION
#include <windows.h>
#include <commdlg.h>
#include <wine/debug.h>
#include <stdio.h>
#include <assert.h>
#include "winecfg.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(winecfg);

static const struct
{
    const char *szVersion;
    const char *szDescription;
    DWORD       dwMajorVersion;
    DWORD       dwMinorVersion;
    DWORD       dwBuildNumber;
    DWORD       dwPlatformId;
    const char *szCSDVersion;
    WORD        wServicePackMajor;
    WORD        wServicePackMinor;
    const char *szProductType;
} win_versions[] =
{
    { "win2003", "Windows 2003",   5,  2, 0xECE, VER_PLATFORM_WIN32_NT, "Service Pack 1", 1, 0, "ServerNT"},
    { "winxp",   "Windows XP",     5,  1, 0xA28, VER_PLATFORM_WIN32_NT, "Service Pack 2", 2, 0, "WinNT"},
    { "win2k",   "Windows 2000",   5,  0, 0x893, VER_PLATFORM_WIN32_NT, "Service Pack 4", 4, 0, "WinNT"},
    { "winme",   "Windows ME",     4, 90, 0xBB8, VER_PLATFORM_WIN32_WINDOWS, " ", 0, 0, ""},
    { "win98",   "Windows 98",     4, 10, 0x8AE, VER_PLATFORM_WIN32_WINDOWS, " A ", 0, 0, ""},
    { "win95",   "Windows 95",     4,  0, 0x3B6, VER_PLATFORM_WIN32_WINDOWS, "", 0, 0, ""},
    { "nt40",    "Windows NT 4.0", 4,  0, 0x565, VER_PLATFORM_WIN32_NT, "Service Pack 6a", 6, 0, "WinNT"},
    { "nt351",   "Windows NT 3.5", 3, 51, 0x421, VER_PLATFORM_WIN32_NT, "Service Pack 2", 0, 0, "WinNT"},
    { "win31",   "Windows 3.1",    2, 10,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""},
    { "win30",   "Windows 3.0",    3,  0,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""},
    { "win20",   "Windows 2.0",    2,  0,     0, VER_PLATFORM_WIN32s, "Win32s 1.3", 0, 0, ""}
};

#define NB_VERSIONS (sizeof(win_versions)/sizeof(win_versions[0]))

static void update_comboboxes(HWND dialog)
{
    int i;

    char *winver;
  
    /* retrieve the registry values */
    winver = get_reg_key(config_key, keypath(""), "Version", "");

    if (*winver == '\0')
    {
        HeapFree(GetProcessHeap(), 0, winver);

        if (current_app) /* no explicit setting */
        {
            WINE_TRACE("setting winver combobox to default\n");
            SendDlgItemMessage (dialog, IDC_WINVER, CB_SETCURSEL, 0, 0);
            return;
        }
        winver = strdupA("win2k");
    }
    WINE_TRACE("winver is %s\n", winver);

    /* normalize the version strings */
    for (i = 0; i < NB_VERSIONS; i++)
    {
        if (!strcasecmp (win_versions[i].szVersion, winver))
        {
            SendDlgItemMessage (dialog, IDC_WINVER, CB_SETCURSEL,
                                (WPARAM) i + (current_app?1:0), 0);
            WINE_TRACE("match with %s\n", win_versions[i].szVersion);
            break;
	}
    }

    HeapFree(GetProcessHeap(), 0, winver);
}

static void
init_comboboxes (HWND dialog)
{
    int i;

    SendDlgItemMessage(dialog, IDC_WINVER, CB_RESETCONTENT, 0, 0);

    /* add the default entries (automatic) which correspond to no setting  */
    if (current_app)
        SendDlgItemMessage(dialog, IDC_WINVER, CB_ADDSTRING, 0, (LPARAM) "Use global settings");

    for (i = 0; i < NB_VERSIONS; i++)
    {
      SendDlgItemMessage (dialog, IDC_WINVER, CB_ADDSTRING,
                          0, (LPARAM) win_versions[i].szDescription);
    }
}

static void add_listview_item(HWND listview, const char *text, void *association)
{
  LVITEM item;

  ZeroMemory(&item, sizeof(LVITEM));

  item.mask = LVIF_TEXT | LVIF_PARAM;
  item.pszText = (char*) text;
  item.cchTextMax = strlen(text);
  item.lParam = (LPARAM) association;
  item.iItem = ListView_GetItemCount(listview);

  ListView_InsertItem(listview, &item);
}

/* Called when the application is initialized (cannot reinit!)  */
static void init_appsheet(HWND dialog)
{
  HWND listview;
  HKEY key;
  int i;
  DWORD size;
  char appname[1024];

  WINE_TRACE("()\n");

  listview = GetDlgItem(dialog, IDC_APP_LISTVIEW);

  /* we use the lparam field of the item so we can alter the presentation later and not change code
   * for instance, to use the tile view or to display the EXEs embedded 'display name' */
  add_listview_item(listview, "Default Settings", NULL);

  /* because this list is only populated once, it's safe to bypass the settings list here  */
  if (RegOpenKey(config_key, "AppDefaults", &key) == ERROR_SUCCESS)
  {
      i = 0;
      size = sizeof(appname);
      while (RegEnumKeyEx(key, i, appname, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
      {
          add_listview_item(listview, appname, strdupA(appname));

          i++;
          size = sizeof(appname);
      }

      RegCloseKey(key);
  }

  init_comboboxes(dialog);
  
  /* Select the default settings listview item  */
  {
      LVITEM item;
      
      ZeroMemory(&item, sizeof(item));
      
      item.mask = LVIF_STATE;
      item.iItem = 0;
      item.state = LVIS_SELECTED | LVIS_FOCUSED;
      item.stateMask = LVIS_SELECTED | LVIS_FOCUSED;

      ListView_SetItem(listview, &item);
  }
  
}

/* there has to be an easier way than this  */
static int get_listview_selection(HWND listview)
{
  int count = ListView_GetItemCount(listview);
  int i;
  
  for (i = 0; i < count; i++)
  {
    if (ListView_GetItemState(listview, i, LVIS_SELECTED)) return i;
  }

  return -1;
}


/* called when the user selects a different application */
static void on_selection_change(HWND dialog, HWND listview)
{
  LVITEM item;
  char *oldapp = current_app;

  WINE_TRACE("()\n");

  item.iItem = get_listview_selection(listview);
  item.mask = LVIF_PARAM;

  WINE_TRACE("item.iItem=%d\n", item.iItem);
  
  if (item.iItem == -1) return;
  
  ListView_GetItem(listview, &item);

  current_app = (char *) item.lParam;

  if (current_app)
  {
      WINE_TRACE("current_app is now %s\n", current_app);
      enable(IDC_APP_REMOVEAPP);
  }
  else
  {
      WINE_TRACE("current_app=NULL, editing global settings\n");
      /* focus will never be on the button in this callback so it's safe  */
      disable(IDC_APP_REMOVEAPP);
  }

  /* reset the combo boxes if we changed from/to global/app-specific  */

  if ((oldapp && !current_app) || (!oldapp && current_app))
      init_comboboxes(dialog);
  
  update_comboboxes(dialog);

  set_window_title(dialog);
}

static BOOL list_contains_file(HWND listview, char *filename)
{
  LVFINDINFO find_info = { LVFI_STRING, filename, 0, {0, 0}, 0 };
  int index;

  index = ListView_FindItem(listview, -1, &find_info);

  return (index != -1);
}

static void on_add_app_click(HWND dialog)
{
  char filetitle[MAX_PATH];
  char file[MAX_PATH];

  OPENFILENAME ofn = { sizeof(OPENFILENAME),
		       0, /*hInst*/0, "Wine Programs (*.exe,*.exe.so)\0*.exe;*.exe.so\0", NULL, 0, 0, NULL,
		       0, NULL, 0, "c:\\", "Select a Windows executable file",
		       OFN_SHOWHELP | OFN_HIDEREADONLY, 0, 0, NULL, 0, NULL };

  ofn.lpstrFileTitle = filetitle;
  ofn.lpstrFileTitle[0] = '\0';
  ofn.nMaxFileTitle = sizeof(filetitle);
  ofn.lpstrFile = file;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(file);

  if (GetOpenFileName(&ofn))
  {
      HWND listview = GetDlgItem(dialog, IDC_APP_LISTVIEW);
      int count = ListView_GetItemCount(listview);
      char* new_app;
      
      new_app = strdupA(filetitle);

      if (list_contains_file(listview, new_app))
          return;
      
      WINE_TRACE("adding %s\n", new_app);
      
      add_listview_item(listview, new_app, new_app);

      ListView_SetItemState(listview, count, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

      SetFocus(listview);
  }
  else WINE_TRACE("user cancelled\n");
}

static void on_remove_app_click(HWND dialog)
{
    HWND listview = GetDlgItem(dialog, IDC_APP_LISTVIEW);
    int selection = get_listview_selection(listview);
    char *section = keypath(""); /* AppDefaults\\whatever.exe\\ */

    WINE_TRACE("selection=%d, section=%s\n", selection, section);
    
    assert( selection != 0 ); /* user cannot click this button when "default settings" is selected  */

    section[strlen(section)] = '\0'; /* remove last backslash  */
    set_reg_key(config_key, section, NULL, NULL); /* delete the section  */
    ListView_DeleteItem(listview, selection);
    ListView_SetItemState(listview, selection - 1, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

    SetFocus(listview);
    
    SendMessage(GetParent(dialog), PSM_CHANGED, (WPARAM) dialog, 0);        
}

static void on_winver_change(HWND dialog)
{
    int selection = SendDlgItemMessage(dialog, IDC_WINVER, CB_GETCURSEL, 0, 0);

    if (!selection && current_app)
    {
        WINE_TRACE("default selected so removing current setting\n");
        set_reg_key(config_key, keypath(""), "Version", NULL);
    }
    else
    {
        if (current_app) selection--;
        WINE_TRACE("setting Version key to value '%s'\n", win_versions[selection].szVersion);
        set_reg_key(config_key, keypath(""), "Version", win_versions[selection].szVersion);
    }
    /* global version only */
    if (!current_app)
    {
        static const char szKey9x[] = "Software\\Microsoft\\Windows\\CurrentVersion";
        static const char szKeyNT[] = "Software\\Microsoft\\Windows NT\\CurrentVersion";
        static const char szKeyProdNT[] = "System\\CurrentControlSet\\Control\\ProductOptions";
        static const char szKeyWindNT[] = "System\\CurrentControlSet\\Control\\Windows";
        static const char szKeyEnvNT[]  = "System\\CurrentControlSet\\Control\\Session Manager\\Environment";
        char Buffer[40];

        switch (win_versions[selection].dwPlatformId)
        {
        case VER_PLATFORM_WIN32_WINDOWS:
            snprintf(Buffer, sizeof(Buffer), "%ld.%ld.%ld", win_versions[selection].dwMajorVersion,
                     win_versions[selection].dwMinorVersion, win_versions[selection].dwBuildNumber);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", win_versions[selection].szCSDVersion);

            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", NULL);
            break;

        case VER_PLATFORM_WIN32_NT:
            snprintf(Buffer, sizeof(Buffer), "%ld.%ld", win_versions[selection].dwMajorVersion,
                     win_versions[selection].dwMinorVersion);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", win_versions[selection].szCSDVersion);
            snprintf(Buffer, sizeof(Buffer), "%ld", win_versions[selection].dwBuildNumber);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", Buffer);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", win_versions[selection].szProductType);
            set_reg_key_dword(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion",
                              MAKEWORD( win_versions[selection].wServicePackMinor,
                                        win_versions[selection].wServicePackMajor ));
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", "Windows_NT");

            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", NULL);
            break;

        case VER_PLATFORM_WIN32s:
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyNT, "CurrentBuildNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyProdNT, "ProductType", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyWindNT, "CSDVersion", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKeyEnvNT, "OS", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "VersionNumber", NULL);
            set_reg_key(HKEY_LOCAL_MACHINE, szKey9x, "SubVersionNumber", NULL);
            break;
        }
    }

    /* enable the apply button  */
    SendMessage(GetParent(dialog), PSM_CHANGED, (WPARAM) dialog, 0);
}

INT_PTR CALLBACK
AppDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_INITDIALOG:
        init_appsheet(hDlg);
        break;

    case WM_SHOWWINDOW:
        set_window_title(hDlg);
        break;

    case WM_NOTIFY:
      switch (((LPNMHDR)lParam)->code)
      {
        case LVN_ITEMCHANGED:
            on_selection_change(hDlg, GetDlgItem(hDlg, IDC_APP_LISTVIEW));
            break;
        case PSN_APPLY:
            apply();
            SetWindowLongPtr(hDlg, DWLP_MSGRESULT, PSNRET_NOERROR);
            break;
      }
      
      break;
    
    case WM_COMMAND:
      switch(HIWORD(wParam))
      {
        case CBN_SELCHANGE:
          switch(LOWORD(wParam))
          {
            case IDC_WINVER:
              on_winver_change(hDlg);
              break;
          }
        case BN_CLICKED:
          switch(LOWORD(wParam))
          {
            case IDC_APP_ADDAPP:
              on_add_app_click(hDlg);
              break;
            case IDC_APP_REMOVEAPP:
              on_remove_app_click(hDlg);
              break;
          }
          break;
      }

      break;
  }
  
  return 0;
}
