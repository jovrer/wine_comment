/*
 * WineCfg libraries tabsheet
 *
 * Copyright 2004 Robert van Herk
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

enum dllmode
{
	BUILTIN_NATIVE,
	NATIVE_BUILTIN,
	BUILTIN,
	NATIVE,
	DISABLE,
	UNKNOWN /* Special value indicating an erronous DLL override mode */
};

struct dll
{
        char *name;
        enum dllmode mode;
};

/* Convert a registry string to a dllmode */
static enum dllmode string_to_mode(char *in)
{
    int i, j, len;
    char *out;
    enum dllmode res;

    len = strlen(in);
    out = HeapAlloc(GetProcessHeap(), 0, len);

    /* remove the spaces */
    for (i = j = 0; i <= len; ++i) {
        if (in[i] != ' ') {
            out[j++] = in[i];
        }
    }

    /* parse the string */
    res = UNKNOWN;
    if (strcmp(out, "builtin,native") == 0) res = BUILTIN_NATIVE;
    if (strcmp(out, "native,builtin") == 0) res = NATIVE_BUILTIN;
    if (strcmp(out, "builtin") == 0) res = BUILTIN;
    if (strcmp(out, "native") == 0) res = NATIVE;
    if (strcmp(out, "") == 0) res = DISABLE;

    HeapFree(GetProcessHeap(), 0, out);
    return res;
}

/* Convert a dllmode to a registry string. */
static const char* mode_to_string(enum dllmode mode)
{
    switch( mode )
    {
        case NATIVE: return "native";
        case BUILTIN: return "builtin";
        case NATIVE_BUILTIN: return "native,builtin";
        case BUILTIN_NATIVE: return "builtin,native";
        case DISABLE: return "";
        default: assert(FALSE); return "";
    }
}

/* Convert a dllmode to a pretty string for display. TODO: use translations. */
static const char* mode_to_label(enum dllmode mode)
{
    WINE_FIXME("translate me\n");
    return mode_to_string(mode);
}

/* Convert a control id (IDC_ constant) to a dllmode */
static enum dllmode id_to_mode(DWORD id)
{
    switch( id )
    {
        case IDC_RAD_BUILTIN: return BUILTIN;
        case IDC_RAD_NATIVE: return NATIVE;
        case IDC_RAD_NATIVE_BUILTIN: return NATIVE_BUILTIN;
        case IDC_RAD_BUILTIN_NATIVE: return BUILTIN_NATIVE;
        case IDC_RAD_DISABLE: return DISABLE;
        default: assert( FALSE ); return 0; /* should not be reached  */
    }
}

/* Convert a dllmode to a control id (IDC_ constant) */
static DWORD mode_to_id(enum dllmode mode)
{
    switch( mode )
    {
        case BUILTIN: return IDC_RAD_BUILTIN;
        case NATIVE: return IDC_RAD_NATIVE;
        case NATIVE_BUILTIN: return IDC_RAD_NATIVE_BUILTIN;
        case BUILTIN_NATIVE: return IDC_RAD_BUILTIN_NATIVE;
        case DISABLE: return IDC_RAD_DISABLE;
        default: assert( FALSE ); return 0; /* should not be reached  */
    }
}

static void set_controls_from_selection(HWND dialog)
{
    /* FIXME: display/update some information about the selected dll (purpose, recommended loadorder) maybe? */
}

static void clear_settings(HWND dialog)
{
    int count = SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETCOUNT, 0, 0);
    int i;

    WINE_TRACE("count=%d\n", count);
    
    for (i = 0; i < count; i++)
    {
        struct dll *dll = (struct dll *) SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETITEMDATA, 0, 0);
        
        SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_DELETESTRING, 0, 0);
        
        HeapFree(GetProcessHeap(), 0, dll->name);
        HeapFree(GetProcessHeap(), 0, dll);
    }
}

static void load_library_settings(HWND dialog)
{
    char **overrides = enumerate_values(config_key, keypath("DllOverrides"));
    char **p;
    int sel, count = 0;

    sel = SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETCURSEL, 0, 0);

    WINE_TRACE("sel=%d\n", sel);

    clear_settings(dialog);
    
    if (!overrides || *overrides == NULL)
    {
        set_controls_from_selection(dialog);
        disable(IDC_DLLS_EDITDLL);
        disable(IDC_DLLS_REMOVEDLL);
        HeapFree(GetProcessHeap(), 0, overrides);
        return;
    }

    enable(IDC_DLLS_EDITDLL);
    enable(IDC_DLLS_REMOVEDLL);
    
    for (p = overrides; *p != NULL; p++)
    {
        int index;
        char *str, *value;
        const char *label;
        struct dll *dll;

        value = get_reg_key(config_key, keypath("DllOverrides"), *p, NULL);

        label = mode_to_label(string_to_mode(value));
        
        str = HeapAlloc(GetProcessHeap(), 0, strlen(*p) + 2 + strlen(label) + 2);
        strcpy(str, *p);
        strcat(str, " (");
        strcat(str, label);
        strcat(str, ")");

        dll = HeapAlloc(GetProcessHeap(), 0, sizeof(struct dll));
        dll->name = *p;
        dll->mode = string_to_mode(value);

        index = SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_ADDSTRING, (WPARAM) -1, (LPARAM) str);
        SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_SETITEMDATA, index, (LPARAM) dll);

        HeapFree(GetProcessHeap(), 0, str);

        count++;
    }

    HeapFree(GetProcessHeap(), 0, overrides);

    /* restore the previous selection, if possible  */
    if (sel >= count - 1) sel = count - 1;
    else if (sel == -1) sel = 0;
    
    SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_SETCURSEL, sel, 0);

    set_controls_from_selection(dialog);
}

/* Called when the application is initialized (cannot reinit!)  */
static void init_libsheet(HWND dialog)
{
    /* clear the add dll controls  */
    SendDlgItemMessage(dialog, IDC_DLLCOMBO, WM_SETTEXT, 1, (LPARAM) "");
    disable(IDC_DLLS_ADDDLL);
}

static void on_add_combo_change(HWND dialog)
{
    char buffer[1024];

    SendDlgItemMessage(dialog, IDC_DLLCOMBO, WM_GETTEXT, sizeof(buffer), (LPARAM) buffer);

    if (strlen(buffer))
        enable(IDC_DLLS_ADDDLL)
    else
        disable(IDC_DLLS_ADDDLL);
}

static void set_dllmode(HWND dialog, DWORD id)
{
    enum dllmode mode;
    struct dll *dll;
    int sel;
    const char *str;

    mode = id_to_mode(id);

    sel = SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETCURSEL, 0, 0);
    if (sel == -1) return;
    
    dll = (struct dll *) SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETITEMDATA, sel, 0);

    str = mode_to_string(mode);
    WINE_TRACE("Setting %s to %s\n", dll->name, str);
    
    SendMessage(GetParent(dialog), PSM_CHANGED, 0, 0);
    set_reg_key(config_key, keypath("DllOverrides"), dll->name, str);

    load_library_settings(dialog);  /* ... and refresh  */
}

static void on_add_click(HWND dialog)
{
    static const char dotDll[] = ".dll";
    char buffer[1024], *ptr;

    ZeroMemory(buffer, sizeof(buffer));

    SendDlgItemMessage(dialog, IDC_DLLCOMBO, WM_GETTEXT, sizeof(buffer), (LPARAM) buffer);
    if (lstrlenA(buffer) >= sizeof(dotDll))
    {
        ptr = buffer + lstrlenA(buffer) - sizeof(dotDll) + 1;
        if (!lstrcmpiA(ptr, dotDll))
        {
            WINE_TRACE("Stripping dll extension\n");
            *ptr = '\0';
        }
    }
    SendDlgItemMessage(dialog, IDC_DLLCOMBO, WM_SETTEXT, 0, (LPARAM) "");
    disable(IDC_DLLS_ADDDLL);
    
    WINE_TRACE("Adding %s as native, builtin", buffer);
    
    SendMessage(GetParent(dialog), PSM_CHANGED, 0, 0);
    set_reg_key(config_key, keypath("DllOverrides"), buffer, "native,builtin");

    load_library_settings(dialog);

    SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_SELECTSTRING, (WPARAM) 0, (LPARAM) buffer);

    set_controls_from_selection(dialog);
}

static INT_PTR CALLBACK loadorder_dlgproc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static WORD sel;

    switch(uMsg) 
    {
    case WM_INITDIALOG:
        CheckRadioButton(hwndDlg, IDC_RAD_BUILTIN, IDC_RAD_DISABLE, lParam);
        sel = lParam;
        return TRUE;

    case WM_COMMAND:
        if(HIWORD(wParam) != BN_CLICKED) break;
        switch (LOWORD(wParam))
        {
        case IDC_RAD_BUILTIN:
        case IDC_RAD_NATIVE:
        case IDC_RAD_BUILTIN_NATIVE:
        case IDC_RAD_NATIVE_BUILTIN:
        case IDC_RAD_DISABLE:
            sel = LOWORD(wParam);
            return TRUE;
        case IDOK:
            EndDialog(hwndDlg, sel);
            return TRUE;
        case IDCANCEL:
            EndDialog(hwndDlg, wParam);
            return TRUE;
        }
    }
    return FALSE;
}

static void on_edit_click(HWND hwnd)
{
    INT_PTR ret; 
    int index = SendDlgItemMessage(hwnd, IDC_DLLS_LIST, LB_GETCURSEL, 0, 0);
    struct dll *dll;
    DWORD id;

    /* if no override is selected the edit button should be disabled... */
    assert(index != -1);

    dll = (struct dll *) SendDlgItemMessage(hwnd, IDC_DLLS_LIST, LB_GETITEMDATA, index, 0);
    id = mode_to_id(dll->mode);
    
    ret = DialogBoxParam(0, MAKEINTRESOURCE(IDD_LOADORDER), hwnd, loadorder_dlgproc, id);
    
    if(ret != IDCANCEL)
        set_dllmode(hwnd, ret);
}

static void on_remove_click(HWND dialog)
{
    int sel = SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETCURSEL, 0, 0);
    struct dll *dll;

    if (sel == LB_ERR) return;
    
    dll = (struct dll *) SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETITEMDATA, sel, 0);
    
    SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_DELETESTRING, sel, 0);

    SendMessage(GetParent(dialog), PSM_CHANGED, 0, 0);
    set_reg_key(config_key, keypath("DllOverrides"), dll->name, NULL);

    HeapFree(GetProcessHeap(), 0, dll->name);
    HeapFree(GetProcessHeap(), 0, dll);

    if (SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_GETCOUNT, 0, 0) > 0)
        SendDlgItemMessage(dialog, IDC_DLLS_LIST, LB_SETCURSEL, max(sel - 1, 0), 0);
    else
    {
        disable(IDC_DLLS_EDITDLL);
        disable(IDC_DLLS_REMOVEDLL);
    }

    set_controls_from_selection(dialog);
}

INT_PTR CALLBACK
LibrariesDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		init_libsheet(hDlg);
		break;
        case WM_SHOWWINDOW:
                set_window_title(hDlg);
                break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
                case PSN_SETACTIVE:
                    load_library_settings(hDlg);
                    break;
		}
		break;
	case WM_COMMAND:
		switch(HIWORD(wParam)) {

                    /* FIXME: when the user hits enter in the DLL combo box we should invoke the add
                     * add button, rather than the propsheet OK button. But I don't know how to do that!
                     */
                    
                case CBN_EDITCHANGE:
                        if(LOWORD(wParam) == IDC_DLLCOMBO)
                        {
                            on_add_combo_change(hDlg);
                            break;
                        }

		case BN_CLICKED:
			switch(LOWORD(wParam)) {
			case IDC_DLLS_ADDDLL:
                            on_add_click(hDlg);
                            break;
			case IDC_DLLS_EDITDLL:
			    on_edit_click(hDlg);
			    break;
			case IDC_DLLS_REMOVEDLL:
                            on_remove_click(hDlg);
                            break;
			}
			break;
                case LBN_SELCHANGE:
                        set_controls_from_selection(hDlg);
                        break;
		}
		break;
	}

	return 0;
}