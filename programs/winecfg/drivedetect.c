/*
 * Drive autodetection code
 *
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

#include "config.h"
#include "wine/port.h"

#include <wine/debug.h>
#include <wine/library.h>

#include "winecfg.h"

#include <stdio.h>
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <stdlib.h>
#include <errno.h>

#include <sys/stat.h>

#include <winbase.h>

WINE_DEFAULT_DEBUG_CHANNEL(winecfg);

BOOL gui_mode = TRUE;
static long working_mask = 0;

#ifdef HAVE_MNTENT_H

static const DEV_NODES sDeviceNodes[] = {
  {"/dev/fd", DRIVE_REMOVABLE},
  {"/dev/cdrom", DRIVE_CDROM},
  {"",0}
};

static const char * const ignored_fstypes[] = {
    "devpts",
    "tmpfs",
    "proc",
    "sysfs",
    "swap",
    "usbdevfs",
    "rpc_pipefs",
    "binfmt_misc",
    NULL
};

static const char * const ignored_mnt_dirs[] = {
    "/boot",
    NULL
};

static int try_dev_node(char *dev)
{
    const DEV_NODES *pDevNodes = sDeviceNodes;
    
    while(pDevNodes->szNode[0])
    {
        if(!strncmp(dev,pDevNodes->szNode,strlen(pDevNodes->szNode)))
            return pDevNodes->nType;
        ++pDevNodes;
    }
    
    return DRIVE_FIXED;
}

static BOOL should_ignore_fstype(char *type)
{
    const char * const *s;
    
    for (s = ignored_fstypes; *s; s++)
        if (!strcmp(*s, type)) return TRUE;

    return FALSE;
}

static BOOL should_ignore_mnt_dir(char *dir)
{
    const char * const *s;

    for (s = ignored_mnt_dirs; *s; s++)
        if (!strcmp(*s, dir)) return TRUE;

    return FALSE;
}

static BOOL is_drive_defined(char *path)
{
    int i;
    
    for (i = 0; i < 26; i++)
        if (drives[i].in_use && !strcmp(drives[i].unixpath, path)) return TRUE;

    return FALSE;
}

/* returns Z + 1 if there are no more available letters */
static char allocate_letter(void)
{
    char letter;

    for (letter = 'C'; letter <= 'Z'; letter++)
        if ((DRIVE_MASK_BIT(letter) & working_mask) != 0) break;

    return letter;
}
#endif

#define FSTAB_OPEN 1
#define NO_MORE_LETTERS 2
#define NO_ROOT 3
#define NO_DRIVE_C 4
#define NO_HOME 5

static void report_error(int code)
{
    char *buffer;
    int len;
    
    switch (code)
    {
        case FSTAB_OPEN:
            if (gui_mode)
            {
                static const char s[] = "Could not open your mountpoint description table.\n\nOpening of /etc/fstab failed: %s";
                len = snprintf(NULL, 0, s, strerror(errno));
                buffer = HeapAlloc(GetProcessHeap(), 0, len + 1);
                snprintf(buffer, len, s, strerror(errno));
                MessageBox(NULL, s, "", MB_OK | MB_ICONEXCLAMATION);
                HeapFree(GetProcessHeap(), 0, buffer);
            }
            else
            {
                fprintf(stderr, "winecfg: could not open fstab: %s\n", strerror(errno));
            }
            break;

        case NO_MORE_LETTERS:
            if (gui_mode) MessageBox(NULL, "No more letters are available to auto-detect available drives with.", "", MB_OK | MB_ICONEXCLAMATION);
            fprintf(stderr, "winecfg: no more available letters while scanning /etc/fstab\n");
            break;

        case NO_ROOT:
            if (gui_mode) MessageBox(NULL, "Could not ensure that the root directory was mapped.\n\n"
                                     "This can happen if you run out of drive letters. "
                                     "It's important to have the root directory mapped, otherwise Wine"
                                     "will not be able to always find the programs you want to run. "
                                     "Try unmapping a drive letter then trying again.", "",
                                     MB_OK | MB_ICONEXCLAMATION);
            else fprintf(stderr, "winecfg: unable to map root drive\n");
            break;

        case NO_DRIVE_C:
            if (gui_mode)
                MessageBox(NULL, "No virtual drive C mapped\n\nTry running wineprefixcreate", "", MB_OK | MB_ICONEXCLAMATION);
            else
                fprintf(stderr, "winecfg: no drive_c directory\n");

        case NO_HOME:
            if (gui_mode)
                MessageBox(NULL, "Could not ensure that your home directory was mapped.\n\n"
                                 "This can happen if you run out of drive letters. "
                                 "Try unmapping a drive letter then try again.", "",
                                 MB_OK | MB_ICONEXCLAMATION);
            else 
                fprintf(stderr, "winecfg: unable to map home drive\n");
            break;
    }
}

static void ensure_root_is_mapped(void)
{
    int i;
    BOOL mapped = FALSE;

    for (i = 0; i < 26; i++)
        if (drives[i].in_use && !strcmp(drives[i].unixpath, "/")) mapped = TRUE;

    if (!mapped)
    {
        /* work backwards from Z, trying to map it */
        char letter;

        for (letter = 'Z'; letter >= 'A'; letter--)
        {
            if (!drives[letter - 'A'].in_use) 
            {
                add_drive(letter, "/", "System", "0", DRIVE_FIXED);
                WINE_TRACE("allocated drive %c as the root drive\n", letter);
                break;
            }
        }

        if (letter == ('A' - 1)) report_error(NO_ROOT);
    }
}

static void ensure_home_is_mapped(void)
{
    int i;
    BOOL mapped = FALSE;
    char *home = getenv("HOME");

    if (!home) return;

    for (i = 0; i < 26; i++)
        if (drives[i].in_use && !strcmp(drives[i].unixpath, home)) mapped = TRUE;

    if (!mapped)
    {
        char letter;

        for (letter = 'H'; letter <= 'Z'; letter++)
        {
            if (!drives[letter - 'A'].in_use)
            {
                add_drive(letter, home, "Home", "0", DRIVE_FIXED);
                WINE_TRACE("allocated drive %c as the user's home directory\n", letter);
                break;
            }
        }
        if (letter == ('Z' + 1)) report_error(NO_HOME);
    }        
}

static void ensure_drive_c_is_mapped(void)
{
    struct stat buf;
    const char *configdir = wine_get_config_dir();
    int len;
    char *drive_c_dir;
    
    if (drives[2].in_use) return;

    len = snprintf(NULL, 0, "%s/../drive_c", configdir);
    drive_c_dir = HeapAlloc(GetProcessHeap(), 0, len);
    snprintf(drive_c_dir, len, "%s/../drive_c", configdir);
    HeapFree(GetProcessHeap(), 0, drive_c_dir);

    if (stat(drive_c_dir, &buf) == 0)
    {
        add_drive('C', "../drive_c", "Virtual Windows Drive", "0", DRIVE_FIXED);
    }
    else
    {
        report_error(NO_DRIVE_C);
    }
}

int autodetect_drives()
{
#ifdef HAVE_MNTENT_H
    struct mntent *ent;
    FILE *fstab;
#endif

    /* we want to build a list of autodetected drives, then ensure each entry
       exists in the users setup. so, we superimpose the autodetected drives
       onto whatever is pre-existing.

       for now let's just rummage around inside the fstab.
     */

    load_drives();

    working_mask = drive_available_mask('\0');

#ifdef HAVE_MNTENT_H
    fstab = fopen("/etc/fstab", "r");
    if (!fstab)
    {
        report_error(FSTAB_OPEN);
        return FALSE;
    }

    while ((ent = getmntent(fstab)))
    {
        char letter;
        char label[256];
        int type;
        
        WINE_TRACE("ent->mnt_dir=%s\n", ent->mnt_dir);

        if (should_ignore_fstype(ent->mnt_type)) continue;
        if (should_ignore_mnt_dir(ent->mnt_dir)) continue;
        if (is_drive_defined(ent->mnt_dir)) continue;

        /* allocate a drive for it */
        letter = allocate_letter();
        if (letter == ']')
        {
            report_error(NO_MORE_LETTERS);
            fclose(fstab);
            return FALSE;
        }
        
        strcpy(label, "Drive X");
        label[6] = letter;
        
        WINE_TRACE("adding drive %c for %s, type %s with label %s\n", letter, ent->mnt_dir, ent->mnt_type,label);

        if (!strcmp(ent->mnt_type, "nfs")) type = DRIVE_REMOTE;
        else if (!strcmp(ent->mnt_type, "nfs4")) type = DRIVE_REMOTE;
        else if (!strcmp(ent->mnt_type, "smbfs")) type = DRIVE_REMOTE;
        else if (!strcmp(ent->mnt_type, "cifs")) type = DRIVE_REMOTE;
        else if (!strcmp(ent->mnt_type, "coda")) type = DRIVE_REMOTE;
        else if (!strcmp(ent->mnt_type, "iso9660")) type = DRIVE_CDROM;
        else if (!strcmp(ent->mnt_type, "ramfs")) type = DRIVE_RAMDISK;
        else type = try_dev_node(ent->mnt_fsname);
        
        add_drive(letter, ent->mnt_dir, label, "0", type);
        
        /* working_mask is a map of the drive letters still available. */
        working_mask &= ~DRIVE_MASK_BIT(letter);
    }

    fclose(fstab);
#endif

    ensure_root_is_mapped();

    ensure_drive_c_is_mapped();

    ensure_home_is_mapped();

    return TRUE;
}
