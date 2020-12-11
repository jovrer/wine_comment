/* -*- tab-width: 8; c-basic-offset: 4 -*- */
/* Main file for CD-ROM support
 *
 * Copyright 1994 Martin Ayotte
 * Copyright 1999, 2001, 2003 Eric Pouech
 * Copyright 2000 Andreas Mohr
 * Copyright 2005 Ivan Leo Puoti
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

#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_IO_H
# include <io.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <fcntl.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <sys/types.h>

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_SCSI_SG_H
# include <scsi/sg.h>
#endif
#ifdef HAVE_SCSI_SCSI_H
# include <scsi/scsi.h>
# undef REASSIGN_BLOCKS  /* avoid conflict with winioctl.h */
#endif
#ifdef HAVE_SCSI_SCSI_IOCTL_H
# include <scsi/scsi_ioctl.h>
#endif
#ifdef HAVE_LINUX_MAJOR_H
# include <linux/major.h>
#endif
#ifdef HAVE_LINUX_HDREG_H
# include <linux/hdreg.h>
#endif
#ifdef HAVE_LINUX_PARAM_H
# include <linux/param.h>
#endif
#ifdef HAVE_LINUX_CDROM_H
# include <linux/cdrom.h>
#endif
#ifdef HAVE_LINUX_UCDROM_H
# include <linux/ucdrom.h>
#endif
#ifdef HAVE_SYS_CDIO_H
# include <sys/cdio.h>
#endif
#ifdef HAVE_SYS_SCSIIO_H
# include <sys/scsiio.h>
#endif

#ifdef HAVE_IOKIT_IOKITLIB_H
# ifndef SENSEBUFLEN
#  include <IOKit/IOKitLib.h>
#  include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#  define SENSEBUFLEN kSenseDefaultSize
# endif
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#include "windef.h"
#include "winternl.h"
#include "winioctl.h"
#include "ntddstor.h"
#include "ntddcdrm.h"
#include "ntddscsi.h"
#include "ntdll_misc.h"
#include "wine/server.h"
#include "wine/library.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(cdrom);

/* Non-Linux systems do not have linux/cdrom.h and the like, and thus
   lack the following constants. */

#ifndef CD_SECS
# define CD_SECS              60 /* seconds per minute */
#endif
#ifndef CD_FRAMES
# define CD_FRAMES            75 /* frames per second */
#endif

/* definitions taken from libdvdcss */

#define IOCTL_DVD_BASE                 FILE_DEVICE_DVD

#define IOCTL_DVD_START_SESSION     CTL_CODE(IOCTL_DVD_BASE, 0x0400, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_READ_KEY          CTL_CODE(IOCTL_DVD_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY          CTL_CODE(IOCTL_DVD_BASE, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_END_SESSION       CTL_CODE(IOCTL_DVD_BASE, 0x0403, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SET_READ_AHEAD    CTL_CODE(IOCTL_DVD_BASE, 0x0404, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_GET_REGION        CTL_CODE(IOCTL_DVD_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_DVD_SEND_KEY2         CTL_CODE(IOCTL_DVD_BASE, 0x0406, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define IOCTL_DVD_READ_STRUCTURE    CTL_CODE(IOCTL_DVD_BASE, 0x0450, METHOD_BUFFERED, FILE_READ_ACCESS)

typedef enum {
    DvdChallengeKey = 0x01,
    DvdBusKey1,
    DvdBusKey2,
    DvdTitleKey,
    DvdAsf,
    DvdSetRpcKey = 0x6,
    DvdGetRpcKey = 0x8,
    DvdDiskKey = 0x80,
    DvdInvalidateAGID = 0x3f
} DVD_KEY_TYPE;

typedef ULONG DVD_SESSION_ID, *PDVD_SESSION_ID;

typedef struct _DVD_COPY_PROTECT_KEY {
    ULONG KeyLength;
    DVD_SESSION_ID SessionId;
    DVD_KEY_TYPE KeyType;
    ULONG KeyFlags;
    union {
        struct {
            ULONG FileHandle;
            ULONG Reserved;   /* used for NT alignment */
        } s;
        LARGE_INTEGER TitleOffset;
    } Parameters;
    UCHAR KeyData[1];
} DVD_COPY_PROTECT_KEY, *PDVD_COPY_PROTECT_KEY;

typedef struct _DVD_RPC_KEY {
    UCHAR UserResetsAvailable:3;
    UCHAR ManufacturerResetsAvailable:3;
    UCHAR TypeCode:2;
    UCHAR RegionMask;
    UCHAR RpcScheme;
    UCHAR Reserved2[1];
} DVD_RPC_KEY, * PDVD_RPC_KEY;

typedef struct _DVD_ASF {
    UCHAR Reserved0[3];
    UCHAR SuccessFlag:1;
    UCHAR Reserved1:7;
} DVD_ASF, * PDVD_ASF;

typedef struct _DVD_REGION
{
        unsigned char copy_system;
        unsigned char region_data;              /* current media region (not playable when set) */
        unsigned char system_region;    /* current drive region (playable when set) */
        unsigned char reset_count;              /* number of resets available */
} DVD_REGION, * PDVD_REGION;

typedef struct _DVD_READ_STRUCTURE {
        /* Contains an offset to the logical block address of the descriptor to be retrieved. */
        LARGE_INTEGER block_byte_offset;

        /* 0:Physical descriptor, 1:Copyright descriptor, 2:Disk key descriptor
           3:BCA descriptor, 4:Manufacturer descriptor, 5:Max descriptor
         */
        long format;

        /* Session ID, that is obtained by IOCTL_DVD_START_SESSION */
        long session;

        /* From 0 to 4 */
        unsigned char layer_no;
}DVD_READ_STRUCTURE, * PDVD_READ_STRUCTURE;

typedef struct _DVD_LAYER_DESCRIPTOR
{
    unsigned short length;

        unsigned char book_version : 4;

        /* 0:DVD-ROM, 1:DVD-RAM, 2:DVD-R, 3:DVD-RW, 9:DVD-RW */
        unsigned char book_type : 4;

        unsigned char minimum_rate : 4;

        /* The physical size of the media. 0:120 mm, 1:80 mm. */
    unsigned char disk_size : 4;

    /* 1:Read-only layer, 2:Recordable layer, 4:Rewritable layer */
        unsigned char layer_type : 4;

        /* 0:parallel track path, 1:opposite track path */
    unsigned char track_path : 1;

        /* 0:one layers, 1:two layers, and so on */
    unsigned char num_of_layers : 2;

    unsigned char reserved1 : 1;

        /* 0:0.74 �m/track, 1:0.80 �m/track, 2:0.615 �m/track */
    unsigned char track_density : 4;

        /* 0:0.267 �m/bit, 1:0.293 �m/bit, 2:0.409 to 0.435 �m/bit, 4:0.280 to 0.291 �m/bit, 8:0.353 �m/bit */
    unsigned char linear_density : 4;

        /* Must be either 0x30000:DVD-ROM or DVD-R/-RW or 0x31000:DVD-RAM or DVD+RW */
    unsigned long starting_data_sector;

    unsigned long end_data_sector;
    unsigned long end_layer_zero_sector;
    unsigned char reserved5 : 7;

        /* 0 indicates no BCA data */
        unsigned char BCA_flag : 1;

        unsigned char reserved6;
}DVD_LAYER_DESCRIPTOR, * PDVD_LAYER_DESCRIPTOR;

typedef struct _DVD_COPYRIGHT_DESCRIPTOR
{
        unsigned char protection;
    unsigned char region;
    unsigned short reserved;
}DVD_COPYRIGHT_DESCRIPTOR, * PDVD_COPYRIGHT_DESCRIPTOR;

typedef struct _DVD_MANUFACTURER_DESCRIPTOR
{
        unsigned char manufacturing[2048];
}DVD_MANUFACTURER_DESCRIPTOR, * PDVD_MANUFACTURER_DESCRIPTOR;

#define DVD_CHALLENGE_KEY_LENGTH    (12 + sizeof(DVD_COPY_PROTECT_KEY) - sizeof(UCHAR))

#define DVD_DISK_KEY_LENGTH         (2048 + sizeof(DVD_COPY_PROTECT_KEY) - sizeof(UCHAR))

#define DVD_KEY_SIZE 5
#define DVD_CHALLENGE_SIZE 10
#define DVD_DISCKEY_SIZE 2048
#define DVD_SECTOR_PROTECTED            0x00000020

static const struct iocodexs
{
  DWORD code;
  const char *codex;
} iocodextable[] = {
#define X(x) { x, #x },
X(IOCTL_CDROM_CHECK_VERIFY)
X(IOCTL_CDROM_CURRENT_POSITION)
X(IOCTL_CDROM_DISK_TYPE)
X(IOCTL_CDROM_GET_CONTROL)
X(IOCTL_CDROM_GET_DRIVE_GEOMETRY)
X(IOCTL_CDROM_GET_VOLUME)
X(IOCTL_CDROM_LOAD_MEDIA)
X(IOCTL_CDROM_MEDIA_CATALOG)
X(IOCTL_CDROM_MEDIA_REMOVAL)
X(IOCTL_CDROM_PAUSE_AUDIO)
X(IOCTL_CDROM_PLAY_AUDIO_MSF)
X(IOCTL_CDROM_RAW_READ)
X(IOCTL_CDROM_READ_Q_CHANNEL)
X(IOCTL_CDROM_READ_TOC)
X(IOCTL_CDROM_RESUME_AUDIO)
X(IOCTL_CDROM_SEEK_AUDIO_MSF)
X(IOCTL_CDROM_SET_VOLUME)
X(IOCTL_CDROM_STOP_AUDIO)
X(IOCTL_CDROM_TRACK_ISRC)
X(IOCTL_DISK_MEDIA_REMOVAL)
X(IOCTL_DVD_END_SESSION)
X(IOCTL_DVD_GET_REGION)
X(IOCTL_DVD_READ_KEY)
X(IOCTL_DVD_READ_STRUCTURE)
X(IOCTL_DVD_SEND_KEY)
X(IOCTL_DVD_START_SESSION)
X(IOCTL_SCSI_GET_ADDRESS)
X(IOCTL_SCSI_GET_CAPABILITIES)
X(IOCTL_SCSI_GET_INQUIRY_DATA)
X(IOCTL_SCSI_PASS_THROUGH)
X(IOCTL_SCSI_PASS_THROUGH_DIRECT)
X(IOCTL_STORAGE_CHECK_VERIFY)
X(IOCTL_STORAGE_EJECTION_CONTROL)
X(IOCTL_STORAGE_EJECT_MEDIA)
X(IOCTL_STORAGE_GET_DEVICE_NUMBER)
X(IOCTL_STORAGE_LOAD_MEDIA)
X(IOCTL_STORAGE_MEDIA_REMOVAL)
X(IOCTL_STORAGE_RESET_DEVICE)
#undef X
};
static const char *iocodex(DWORD code)
{
   unsigned int i;
   static char buffer[25];
   for(i=0; i<sizeof(iocodextable)/sizeof(struct iocodexs); i++)
      if (code==iocodextable[i].code)
	 return iocodextable[i].codex;
   sprintf(buffer, "IOCTL_CODE_%x", (int)code);
   return buffer;
}

#define INQ_REPLY_LEN 36
#define INQ_CMD_LEN 6

#define FRAME_OF_ADDR(a) (((int)(a)[1] * CD_SECS + (a)[2]) * CD_FRAMES + (a)[3])
#define FRAME_OF_MSF(a) (((int)(a).M * CD_SECS + (a).S) * CD_FRAMES + (a).F)
#define FRAME_OF_TOC(toc, idx)  FRAME_OF_ADDR((toc).TrackData[idx - (toc).FirstTrack].Address)
#define MSF_OF_FRAME(m,fr) {int f=(fr); ((UCHAR *)&(m))[2]=f%CD_FRAMES;f/=CD_FRAMES;((UCHAR *)&(m))[1]=f%CD_SECS;((UCHAR *)&(m))[0]=f/CD_SECS;}

static NTSTATUS CDROM_ReadTOC(int, int, CDROM_TOC*);
static NTSTATUS CDROM_GetStatusCode(int);


#ifdef linux

# ifndef IDE6_MAJOR
#  define IDE6_MAJOR 88
# endif
# ifndef IDE7_MAJOR
#  define IDE7_MAJOR 89
# endif

# ifdef CDROM_SEND_PACKET
/* structure for CDROM_PACKET_COMMAND ioctl */
/* not all Linux versions have all the fields, so we define the
 * structure ourselves to make sure */
struct linux_cdrom_generic_command
{
    unsigned char          cmd[CDROM_PACKET_SIZE];
    unsigned char         *buffer;
    unsigned int           buflen;
    int                    stat;
    struct request_sense  *sense;
    unsigned char          data_direction;
    int                    quiet;
    int                    timeout;
    void                  *reserved[1];
};
# endif  /* CDROM_SEND_PACKET */

#endif  /* linux */

/* FIXME: this is needed because we can't open simultaneously several times /dev/cdrom
 * this should be removed when a proper device interface is implemented
 * 
 * (WS) We need this to keep track of current position and to safely
 * detect media changes. Besides this should provide a great speed up
 * for toc inquiries.
 */
struct cdrom_cache {
    dev_t       device;
    ino_t       inode;
    char        toc_good; /* if false, will reread TOC from disk */
    CDROM_TOC   toc;
    SUB_Q_CURRENT_POSITION CurrentPosition;
};
/* who has more than 5 cdroms on his/her machine ?? */
/* FIXME: this should grow depending on the number of cdroms we install/configure 
 * at startup
 */
#define MAX_CACHE_ENTRIES       5
static struct cdrom_cache cdrom_cache[MAX_CACHE_ENTRIES];

static RTL_CRITICAL_SECTION cache_section;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &cache_section,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": cache_section") }
};
static RTL_CRITICAL_SECTION cache_section = { &critsect_debug, -1, 0, 0, 0, 0 };

/* Proposed media change function: not really needed at this time */
/* This is a 1 or 0 type of function */
#if 0
static int CDROM_MediaChanged(int dev)
{
   int i;

   struct cdrom_tochdr	hdr;
   struct cdrom_tocentry entry;

   if (dev < 0 || dev >= MAX_CACHE_ENTRIES)
      return 0;
   if ( ioctl(cdrom_cache[dev].fd, CDROMREADTOCHDR, &hdr) == -1 )
      return 0;

   if ( memcmp(&hdr, &cdrom_cache[dev].hdr, sizeof(struct cdrom_tochdr)) )
      return 1;

   for (i=hdr.cdth_trk0; i<=hdr.cdth_trk1+1; i++)
   {
      if (i == hdr.cdth_trk1 + 1)
      {
	 entry.cdte_track = CDROM_LEADOUT;
      } else {
         entry.cdte_track = i;
      }
      entry.cdte_format = CDROM_MSF;
      if ( ioctl(cdrom_cache[dev].fd, CDROMREADTOCENTRY, &entry) == -1)
	 return 0;
      if ( memcmp(&entry, cdrom_cache[dev].entry+i-hdr.cdth_trk0,
			      sizeof(struct cdrom_tocentry)) )
	 return 1;
   }
   return 0;
}
#endif

/******************************************************************
 *		CDROM_SyncCache                          [internal]
 *
 * Read the TOC in and store it in the cdrom_cache structure.
 * Further requests for the TOC will be copied from the cache
 * unless certain events like disk ejection is detected, in which
 * case the cache will be cleared, causing it to be resynced.
 * The cache section must be held by caller.
 */
static int CDROM_SyncCache(int dev, int fd)
{
   int i, io = 0, tsz;
#ifdef linux
   struct cdrom_tochdr		hdr;
   struct cdrom_tocentry	entry;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
   struct ioc_toc_header	hdr;
   struct ioc_read_toc_entry	entry;
   struct cd_toc_entry         toc_buffer;
#endif
   CDROM_TOC *toc = &cdrom_cache[dev].toc;
   cdrom_cache[dev].toc_good = 0;

#ifdef linux

   io = ioctl(fd, CDROMREADTOCHDR, &hdr);
   if (io == -1)
   {
      WARN("(%d) -- Error occurred (%s)!\n", dev, strerror(errno));
      goto end;
   }

   toc->FirstTrack = hdr.cdth_trk0;
   toc->LastTrack  = hdr.cdth_trk1;
   tsz = sizeof(toc->FirstTrack) + sizeof(toc->LastTrack)
       + sizeof(TRACK_DATA) * (toc->LastTrack-toc->FirstTrack+2);
   toc->Length[0] = tsz >> 8;
   toc->Length[1] = tsz;

   TRACE("caching toc from=%d to=%d\n", toc->FirstTrack, toc->LastTrack );

   for (i = toc->FirstTrack; i <= toc->LastTrack + 1; i++)
   {
     if (i == toc->LastTrack + 1)
       entry.cdte_track = CDROM_LEADOUT;
     else 
       entry.cdte_track = i;
     entry.cdte_format = CDROM_MSF;
     io = ioctl(fd, CDROMREADTOCENTRY, &entry);
     if (io == -1) {
       WARN("error read entry (%s)\n", strerror(errno));
       goto end;
     }
     toc->TrackData[i - toc->FirstTrack].Control = entry.cdte_ctrl;
     toc->TrackData[i - toc->FirstTrack].Adr = entry.cdte_adr;
     /* marking last track with leadout value as index */
     toc->TrackData[i - toc->FirstTrack].TrackNumber = entry.cdte_track;
     toc->TrackData[i - toc->FirstTrack].Address[0] = 0;
     toc->TrackData[i - toc->FirstTrack].Address[1] = entry.cdte_addr.msf.minute;
     toc->TrackData[i - toc->FirstTrack].Address[2] = entry.cdte_addr.msf.second;
     toc->TrackData[i - toc->FirstTrack].Address[3] = entry.cdte_addr.msf.frame;
    }
    cdrom_cache[dev].toc_good = 1;
    io = 0;
#elif defined(__FreeBSD__) || defined(__NetBSD__)

    io = ioctl(fd, CDIOREADTOCHEADER, &hdr);
    if (io == -1)
    {
        WARN("(%d) -- Error occurred (%s)!\n", dev, strerror(errno));
        goto end;
    }
    toc->FirstTrack = hdr.starting_track;
    toc->LastTrack  = hdr.ending_track;
    tsz = sizeof(toc->FirstTrack) + sizeof(toc->LastTrack)
        + sizeof(TRACK_DATA) * (toc->LastTrack-toc->FirstTrack+2);
    toc->Length[0] = tsz >> 8;
    toc->Length[1] = tsz;

    TRACE("caching toc from=%d to=%d\n", toc->FirstTrack, toc->LastTrack );

    for (i = toc->FirstTrack; i <= toc->LastTrack + 1; i++)
    {
	if (i == toc->LastTrack + 1)
        {
#define LEADOUT 0xaa
	    entry.starting_track = LEADOUT;
        } else {
            entry.starting_track = i;
        }
	memset((char *)&toc_buffer, 0, sizeof(toc_buffer));
	entry.address_format = CD_MSF_FORMAT;
	entry.data_len = sizeof(toc_buffer);
	entry.data = &toc_buffer;
	io = ioctl(fd, CDIOREADTOCENTRYS, &entry);
	if (io == -1) {
	    WARN("error read entry (%s)\n", strerror(errno));
	    goto end;
	}
        toc->TrackData[i - toc->FirstTrack].Control = toc_buffer.control;
        toc->TrackData[i - toc->FirstTrack].Adr = toc_buffer.addr_type;
        /* marking last track with leadout value as index */
        toc->TrackData[i - toc->FirstTrack].TrackNumber = entry.starting_track;
        toc->TrackData[i - toc->FirstTrack].Address[0] = 0;
        toc->TrackData[i - toc->FirstTrack].Address[1] = toc_buffer.addr.msf.minute;
        toc->TrackData[i - toc->FirstTrack].Address[2] = toc_buffer.addr.msf.second;
        toc->TrackData[i - toc->FirstTrack].Address[3] = toc_buffer.addr.msf.frame;
    }
    cdrom_cache[dev].toc_good = 1;
    io = 0;
#else
    return STATUS_NOT_SUPPORTED;
#endif
end:
    return CDROM_GetStatusCode(io);
}

static void CDROM_ClearCacheEntry(int dev)
{
    RtlEnterCriticalSection( &cache_section );
    cdrom_cache[dev].toc_good = 0;
    RtlLeaveCriticalSection( &cache_section );
}



/******************************************************************
 *		CDROM_GetInterfaceInfo
 *
 * Determines the ide interface (the number after the ide), and the
 * number of the device on that interface for ide cdroms (*iface <= 1).
 * Determines the scsi information for scsi cdroms (*iface >= 2).
 * Returns false if the info cannot not be obtained.
 */
static int CDROM_GetInterfaceInfo(int fd, UCHAR* iface, UCHAR* port, UCHAR* device, UCHAR* lun)
{
#if defined(linux)
    struct stat st;
    if ( fstat(fd, &st) == -1 || ! S_ISBLK(st.st_mode)) return 0;
    *port = 0;
    *iface = 0;
    *device = 0;
    *lun = 0;
    switch (major(st.st_rdev)) {
    case IDE0_MAJOR: *iface = 0; break;
    case IDE1_MAJOR: *iface = 1; break;
    case IDE2_MAJOR: *iface = 2; break;
    case IDE3_MAJOR: *iface = 3; break;
    case IDE4_MAJOR: *iface = 4; break;
    case IDE5_MAJOR: *iface = 5; break;
    case IDE6_MAJOR: *iface = 6; break;
    case IDE7_MAJOR: *iface = 7; break;
    default: *port = 1; break;
    }

    if (*port == 0)
        *device = (minor(st.st_rdev) >> 6);
    else
    {
#ifdef SCSI_IOCTL_GET_IDLUN
        UINT32 idlun[2];
        if (ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun) != -1)
        {
            *port = (idlun[0] >> 24) & 0xff;
            *iface = ((idlun[0] >> 16) & 0xff) + 2;
            *device = idlun[0] & 0xff;
            *lun = (idlun[0] >> 8) & 0xff;
        }
        else
#endif
        {
            WARN("CD-ROM device (%d, %d) not supported\n", major(st.st_rdev), minor(st.st_rdev));
            return 0;
        }
    }
    return 1;
#elif defined(__NetBSD__)
    struct scsi_addr addr;
    if (ioctl(fd, SCIOCIDENTIFY, &addr) != -1)
    {
        switch (addr.type) 
        {
        case TYPE_SCSI:  *port = 1;
            *iface = addr.addr.scsi.scbus;
            *device = addr.addr.scsi.target;
            *lun = addr.addr.scsi.lun;
            break;
        case TYPE_ATAPI: *port = 0;
            *iface = addr.addr.atapi.atbus;
            *device = addr.addr.atapi.drive;
            *lun = 0;
            break;
        }
        return 1;
    }
    return 0;
#elif defined(__FreeBSD__)
    FIXME("not implemented for BSD\n");
    return 0;
#else
    FIXME("not implemented for nonlinux\n");
    return 0;
#endif
}


/******************************************************************
 *		CDROM_Open
 *
 */
static NTSTATUS CDROM_Open(int fd, int* dev)
{
    struct stat st;
    NTSTATUS ret = STATUS_SUCCESS;
    int         empty = -1;

    fstat(fd, &st);

    RtlEnterCriticalSection( &cache_section );
    for (*dev = 0; *dev < MAX_CACHE_ENTRIES; (*dev)++)
    {
        if (empty == -1 &&
            cdrom_cache[*dev].device == 0 &&
            cdrom_cache[*dev].inode == 0)
            empty = *dev;
        else if (cdrom_cache[*dev].device == st.st_dev &&
                 cdrom_cache[*dev].inode == st.st_ino)
            break;
    }
    if (*dev == MAX_CACHE_ENTRIES)
    {
        if (empty == -1) ret = STATUS_NOT_IMPLEMENTED;
        else
        {
            *dev = empty;
            cdrom_cache[*dev].device  = st.st_dev;
            cdrom_cache[*dev].inode   = st.st_ino;
        }
    }
    RtlLeaveCriticalSection( &cache_section );

    TRACE("%d, %d\n", *dev, fd);
    return ret;
}

/******************************************************************
 *		CDROM_GetStatusCode
 *
 *
 */
static NTSTATUS CDROM_GetStatusCode(int io)
{
    if (io == 0) return STATUS_SUCCESS;
    return FILE_GetNtStatus();
}

/******************************************************************
 *		CDROM_GetControl
 *
 */
static NTSTATUS CDROM_GetControl(int dev, CDROM_AUDIO_CONTROL* cac)
{
    cac->LbaFormat = 0; /* FIXME */
    cac->LogicalBlocksPerSecond = 1; /* FIXME */
    return  STATUS_NOT_SUPPORTED;
}

/******************************************************************
 *		CDROM_GetDeviceNumber
 *
 */
static NTSTATUS CDROM_GetDeviceNumber(int dev, STORAGE_DEVICE_NUMBER* devnum)
{
    return STATUS_NOT_SUPPORTED;
}

/******************************************************************
 *		CDROM_GetDriveGeometry
 *
 */
static NTSTATUS CDROM_GetDriveGeometry(int dev, int fd, DISK_GEOMETRY* dg)
{
  CDROM_TOC     toc;
  NTSTATUS      ret = 0;
  int           fsize = 0;

  if ((ret = CDROM_ReadTOC(dev, fd, &toc)) != 0) return ret;

  fsize = FRAME_OF_TOC(toc, toc.LastTrack+1)
        - FRAME_OF_TOC(toc, 1); /* Total size in frames */
  
  dg->Cylinders.u.LowPart = fsize / (64 * 32); 
  dg->Cylinders.u.HighPart = 0; 
  dg->MediaType = RemovableMedia;  
  dg->TracksPerCylinder = 64; 
  dg->SectorsPerTrack = 32;  
  dg->BytesPerSector= 2048; 
  return ret;
}

/**************************************************************************
 *                              CDROM_Reset                     [internal]
 */
static NTSTATUS CDROM_ResetAudio(int fd)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, CDROMRESET));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode(ioctl(fd, CDIOCRESET, NULL));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_SetTray
 *
 *
 */
static NTSTATUS CDROM_SetTray(int fd, BOOL doEject)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, doEject ? CDROMEJECT : CDROMCLOSETRAY));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode((ioctl(fd, CDIOCALLOW, NULL)) ||
                               (ioctl(fd, doEject ? CDIOCEJECT : CDIOCCLOSE, NULL)) ||
                               (ioctl(fd, CDIOCPREVENT, NULL)));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_ControlEjection
 *
 *
 */
static NTSTATUS CDROM_ControlEjection(int fd, const PREVENT_MEDIA_REMOVAL* rmv)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, CDROM_LOCKDOOR, rmv->PreventMediaRemoval));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode(ioctl(fd, (rmv->PreventMediaRemoval) ? CDIOCPREVENT : CDIOCALLOW, NULL));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_ReadTOC
 *
 *
 */
static NTSTATUS CDROM_ReadTOC(int dev, int fd, CDROM_TOC* toc)
{
    NTSTATUS       ret = STATUS_NOT_SUPPORTED;

    if (dev < 0 || dev >= MAX_CACHE_ENTRIES)
        return STATUS_INVALID_PARAMETER;

    RtlEnterCriticalSection( &cache_section );
    if (cdrom_cache[dev].toc_good || !(ret = CDROM_SyncCache(dev, fd)))
    {
        *toc = cdrom_cache[dev].toc;
        ret = STATUS_SUCCESS;
    }
    RtlLeaveCriticalSection( &cache_section );
    return ret;
}

/******************************************************************
 *		CDROM_GetDiskData
 *
 *
 */
static NTSTATUS CDROM_GetDiskData(int dev, int fd, CDROM_DISK_DATA* data)
{
    CDROM_TOC   toc;
    NTSTATUS    ret;
    int         i;

    if ((ret = CDROM_ReadTOC(dev, fd, &toc)) != 0) return ret;
    data->DiskData = 0;
    for (i = toc.FirstTrack; i <= toc.LastTrack; i++) {
        if (toc.TrackData[i-toc.FirstTrack].Control & 0x04)
            data->DiskData |= CDROM_DISK_DATA_TRACK;
        else
            data->DiskData |= CDROM_DISK_AUDIO_TRACK;
    }
    return STATUS_SUCCESS;
}

/******************************************************************
 *		CDROM_ReadQChannel
 *
 *
 */
static NTSTATUS CDROM_ReadQChannel(int dev, int fd, const CDROM_SUB_Q_DATA_FORMAT* fmt,
                                   SUB_Q_CHANNEL_DATA* data)
{
    NTSTATUS            ret = STATUS_NOT_SUPPORTED;
#ifdef linux
    unsigned            size;
    SUB_Q_HEADER*       hdr = (SUB_Q_HEADER*)data;
    int                 io;
    struct cdrom_subchnl	sc;
    sc.cdsc_format = CDROM_MSF;

    io = ioctl(fd, CDROMSUBCHNL, &sc);
    if (io == -1)
    {
	TRACE("opened or no_media (%s)!\n", strerror(errno));
	hdr->AudioStatus = AUDIO_STATUS_NO_STATUS;
	CDROM_ClearCacheEntry(dev);
	goto end;
    }

    hdr->AudioStatus = AUDIO_STATUS_NOT_SUPPORTED;

    switch (sc.cdsc_audiostatus) {
    case CDROM_AUDIO_INVALID:
	CDROM_ClearCacheEntry(dev);
	hdr->AudioStatus = AUDIO_STATUS_NOT_SUPPORTED;
	break;
    case CDROM_AUDIO_NO_STATUS:
	CDROM_ClearCacheEntry(dev);
	hdr->AudioStatus = AUDIO_STATUS_NO_STATUS;
	break;
    case CDROM_AUDIO_PLAY:
        hdr->AudioStatus = AUDIO_STATUS_IN_PROGRESS;
        break;
    case CDROM_AUDIO_PAUSED:
        hdr->AudioStatus = AUDIO_STATUS_PAUSED;
        break;
    case CDROM_AUDIO_COMPLETED:
        hdr->AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
        break;
    case CDROM_AUDIO_ERROR:
        hdr->AudioStatus = AUDIO_STATUS_PLAY_ERROR;
        break;
    default:
	TRACE("status=%02X !\n", sc.cdsc_audiostatus);
        break;
    }
    switch (fmt->Format)
    {
    case IOCTL_CDROM_CURRENT_POSITION:
        size = sizeof(SUB_Q_CURRENT_POSITION);
        RtlEnterCriticalSection( &cache_section );
	if (hdr->AudioStatus==AUDIO_STATUS_IN_PROGRESS) {
          data->CurrentPosition.FormatCode = IOCTL_CDROM_CURRENT_POSITION;
          data->CurrentPosition.Control = sc.cdsc_ctrl; 
          data->CurrentPosition.ADR = sc.cdsc_adr; 
          data->CurrentPosition.TrackNumber = sc.cdsc_trk; 
          data->CurrentPosition.IndexNumber = sc.cdsc_ind; 

          data->CurrentPosition.AbsoluteAddress[0] = 0; 
          data->CurrentPosition.AbsoluteAddress[1] = sc.cdsc_absaddr.msf.minute; 
          data->CurrentPosition.AbsoluteAddress[2] = sc.cdsc_absaddr.msf.second;
          data->CurrentPosition.AbsoluteAddress[3] = sc.cdsc_absaddr.msf.frame;
 
          data->CurrentPosition.TrackRelativeAddress[0] = 0; 
          data->CurrentPosition.TrackRelativeAddress[1] = sc.cdsc_reladdr.msf.minute; 
          data->CurrentPosition.TrackRelativeAddress[2] = sc.cdsc_reladdr.msf.second;
          data->CurrentPosition.TrackRelativeAddress[3] = sc.cdsc_reladdr.msf.frame;

	  cdrom_cache[dev].CurrentPosition = data->CurrentPosition;
	}
	else /* not playing */
	{
	  cdrom_cache[dev].CurrentPosition.Header = *hdr; /* Preserve header info */
	  data->CurrentPosition = cdrom_cache[dev].CurrentPosition;
	}
        RtlLeaveCriticalSection( &cache_section );
        break;
    case IOCTL_CDROM_MEDIA_CATALOG:
        size = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
        data->MediaCatalog.FormatCode = IOCTL_CDROM_MEDIA_CATALOG;
        {
            struct cdrom_mcn mcn;
            if ((io = ioctl(fd, CDROM_GET_MCN, &mcn)) == -1) goto end;

            data->MediaCatalog.FormatCode = IOCTL_CDROM_MEDIA_CATALOG;
            data->MediaCatalog.Mcval = 0; /* FIXME */
            memcpy(data->MediaCatalog.MediaCatalog, mcn.medium_catalog_number, 14);
            data->MediaCatalog.MediaCatalog[14] = 0;
        }
        break;
    case IOCTL_CDROM_TRACK_ISRC:
        size = sizeof(SUB_Q_CURRENT_POSITION);
        FIXME("TrackIsrc: NIY on linux\n");
        data->TrackIsrc.FormatCode = IOCTL_CDROM_TRACK_ISRC;
        data->TrackIsrc.Tcval = 0;
        io = 0;
        break;
    }

 end:
    ret = CDROM_GetStatusCode(io);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    unsigned            size;
    SUB_Q_HEADER*       hdr = (SUB_Q_HEADER*)data;
    int                 io;
    struct ioc_read_subchannel	read_sc;
    struct cd_sub_channel_info	sc;

    read_sc.address_format = CD_MSF_FORMAT;
    read_sc.track          = 0;
    read_sc.data_len       = sizeof(sc);
    read_sc.data           = &sc;
    switch (fmt->Format)
    {
    case IOCTL_CDROM_CURRENT_POSITION:
        read_sc.data_format    = CD_CURRENT_POSITION;
        break;
    case IOCTL_CDROM_MEDIA_CATALOG:
        read_sc.data_format    = CD_MEDIA_CATALOG;
        break;
    case IOCTL_CDROM_TRACK_ISRC:
        read_sc.data_format    = CD_TRACK_INFO;
        sc.what.track_info.track_number = data->TrackIsrc.Track;
        break;
    }
    io = ioctl(fd, CDIOCREADSUBCHANNEL, &read_sc);
    if (io == -1)
    {
	TRACE("opened or no_media (%s)!\n", strerror(errno));
	CDROM_ClearCacheEntry(dev);
	hdr->AudioStatus = AUDIO_STATUS_NO_STATUS;
	goto end;
    }

    hdr->AudioStatus = AUDIO_STATUS_NOT_SUPPORTED;

    switch (sc.header.audio_status) {
    case CD_AS_AUDIO_INVALID:
	CDROM_ClearCacheEntry(dev);
	hdr->AudioStatus = AUDIO_STATUS_NOT_SUPPORTED;
	break;
    case CD_AS_NO_STATUS:
	CDROM_ClearCacheEntry(dev);
	hdr->AudioStatus = AUDIO_STATUS_NO_STATUS;
        break;
    case CD_AS_PLAY_IN_PROGRESS:
        hdr->AudioStatus = AUDIO_STATUS_IN_PROGRESS;
        break;
    case CD_AS_PLAY_PAUSED:
        hdr->AudioStatus = AUDIO_STATUS_PAUSED;
        break;
    case CD_AS_PLAY_COMPLETED:
        hdr->AudioStatus = AUDIO_STATUS_PLAY_COMPLETE;
        break;
    case CD_AS_PLAY_ERROR:
        hdr->AudioStatus = AUDIO_STATUS_PLAY_ERROR;
        break;
    default:
	TRACE("status=%02X !\n", sc.header.audio_status);
    }
    switch (fmt->Format)
    {
    case IOCTL_CDROM_CURRENT_POSITION:
        size = sizeof(SUB_Q_CURRENT_POSITION);
        RtlEnterCriticalSection( &cache_section );
	if (hdr->AudioStatus==AUDIO_STATUS_IN_PROGRESS) {
          data->CurrentPosition.FormatCode = IOCTL_CDROM_CURRENT_POSITION;
          data->CurrentPosition.Control = sc.what.position.control;
          data->CurrentPosition.ADR = sc.what.position.addr_type;
          data->CurrentPosition.TrackNumber = sc.what.position.track_number;
          data->CurrentPosition.IndexNumber = sc.what.position.index_number;

          data->CurrentPosition.AbsoluteAddress[0] = 0;
          data->CurrentPosition.AbsoluteAddress[1] = sc.what.position.absaddr.msf.minute;
          data->CurrentPosition.AbsoluteAddress[2] = sc.what.position.absaddr.msf.second;
          data->CurrentPosition.AbsoluteAddress[3] = sc.what.position.absaddr.msf.frame;
          data->CurrentPosition.TrackRelativeAddress[0] = 0;
          data->CurrentPosition.TrackRelativeAddress[1] = sc.what.position.reladdr.msf.minute;
          data->CurrentPosition.TrackRelativeAddress[2] = sc.what.position.reladdr.msf.second;
          data->CurrentPosition.TrackRelativeAddress[3] = sc.what.position.reladdr.msf.frame;
	  cdrom_cache[dev].CurrentPosition = data->CurrentPosition;
	}
	else { /* not playing */
	  cdrom_cache[dev].CurrentPosition.Header = *hdr; /* Preserve header info */
	  data->CurrentPosition = cdrom_cache[dev].CurrentPosition;
	}
        RtlLeaveCriticalSection( &cache_section );
        break;
    case IOCTL_CDROM_MEDIA_CATALOG:
        size = sizeof(SUB_Q_MEDIA_CATALOG_NUMBER);
        data->MediaCatalog.FormatCode = IOCTL_CDROM_MEDIA_CATALOG;
        data->MediaCatalog.Mcval = sc.what.media_catalog.mc_valid;
        memcpy(data->MediaCatalog.MediaCatalog, sc.what.media_catalog.mc_number, 15);
        break;
    case IOCTL_CDROM_TRACK_ISRC:
        size = sizeof(SUB_Q_CURRENT_POSITION);
        data->TrackIsrc.FormatCode = IOCTL_CDROM_TRACK_ISRC;
        data->TrackIsrc.Tcval = sc.what.track_info.ti_valid;
        memcpy(data->TrackIsrc.TrackIsrc, sc.what.track_info.ti_number, 15);
        break;
    }

 end:
    ret = CDROM_GetStatusCode(io);
#endif
    return ret;
}

/******************************************************************
 *		CDROM_Verify
 *  Implements: IOCTL_STORAGE_CHECK_VERIFY
 *              IOCTL_CDROM_CHECK_VERIFY
 *
 */
static NTSTATUS CDROM_Verify(int dev, int fd)
{
#if defined(linux)
    int ret;

    ret = ioctl(fd,  CDROM_DRIVE_STATUS, NULL);
    if(ret == -1) {
        TRACE("ioctl CDROM_DRIVE_STATUS failed(%s)!\n", strerror(errno));
        return CDROM_GetStatusCode(ret);
    }

    if(ret == CDS_DISC_OK)
        return STATUS_SUCCESS;
    else
        return STATUS_NO_MEDIA_IN_DEVICE;
#endif
    FIXME("not implemented for non-linux\n");
    return STATUS_NOT_SUPPORTED;
}

/******************************************************************
 *		CDROM_PlayAudioMSF
 *
 *
 */
static NTSTATUS CDROM_PlayAudioMSF(int fd, const CDROM_PLAY_AUDIO_MSF* audio_msf)
{
    NTSTATUS       ret = STATUS_NOT_SUPPORTED;
#ifdef linux
    struct 	cdrom_msf	msf;
    int         io;

    msf.cdmsf_min0   = audio_msf->StartingM;
    msf.cdmsf_sec0   = audio_msf->StartingS;
    msf.cdmsf_frame0 = audio_msf->StartingF;
    msf.cdmsf_min1   = audio_msf->EndingM;
    msf.cdmsf_sec1   = audio_msf->EndingS;
    msf.cdmsf_frame1 = audio_msf->EndingF;

    io = ioctl(fd, CDROMSTART);
    if (io == -1)
    {
	WARN("motor doesn't start !\n");
	goto end;
    }
    io = ioctl(fd, CDROMPLAYMSF, &msf);
    if (io == -1)
    {
	WARN("device doesn't play !\n");
	goto end;
    }
    TRACE("msf = %d:%d:%d %d:%d:%d\n",
	  msf.cdmsf_min0, msf.cdmsf_sec0, msf.cdmsf_frame0,
	  msf.cdmsf_min1, msf.cdmsf_sec1, msf.cdmsf_frame1);
 end:
    ret = CDROM_GetStatusCode(io);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    struct	ioc_play_msf	msf;
    int         io;

    msf.start_m      = audio_msf->StartingM;
    msf.start_s      = audio_msf->StartingS;
    msf.start_f      = audio_msf->StartingF;
    msf.end_m        = audio_msf->EndingM;
    msf.end_s        = audio_msf->EndingS;
    msf.end_f        = audio_msf->EndingF;

    io = ioctl(fd, CDIOCSTART, NULL);
    if (io == -1)
    {
	WARN("motor doesn't start !\n");
	goto end;
    }
    io = ioctl(fd, CDIOCPLAYMSF, &msf);
    if (io == -1)
    {
	WARN("device doesn't play !\n");
	goto end;
    }
    TRACE("msf = %d:%d:%d %d:%d:%d\n",
	  msf.start_m, msf.start_s, msf.start_f,
	  msf.end_m,   msf.end_s,   msf.end_f);
end:
    ret = CDROM_GetStatusCode(io);
#endif
    return ret;
}

/******************************************************************
 *		CDROM_SeekAudioMSF
 *
 *
 */
static NTSTATUS CDROM_SeekAudioMSF(int dev, int fd, const CDROM_SEEK_AUDIO_MSF* audio_msf)
{
    CDROM_TOC toc;
    int i, io, frame;
    SUB_Q_CURRENT_POSITION *cp;
#if defined(linux)
    struct cdrom_msf0	msf;
    struct cdrom_subchnl sc;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    struct ioc_play_msf	msf;
    struct ioc_read_subchannel	read_sc;
    struct cd_sub_channel_info	sc;
    int final_frame;
#endif

    /* Use the information on the TOC to compute the new current
     * position, which is shadowed on the cache. [Portable]. */
    frame = FRAME_OF_MSF(*audio_msf);

    if ((io = CDROM_ReadTOC(dev, fd, &toc)) != 0) return io;

    for(i=toc.FirstTrack;i<=toc.LastTrack+1;i++)
      if (FRAME_OF_TOC(toc,i)>frame) break;
    if (i <= toc.FirstTrack || i > toc.LastTrack+1)
      return STATUS_INVALID_PARAMETER;
    i--;
    RtlEnterCriticalSection( &cache_section );
    cp = &cdrom_cache[dev].CurrentPosition;
    cp->FormatCode = IOCTL_CDROM_CURRENT_POSITION; 
    cp->Control = toc.TrackData[i-toc.FirstTrack].Control; 
    cp->ADR = toc.TrackData[i-toc.FirstTrack].Adr; 
    cp->TrackNumber = toc.TrackData[i-toc.FirstTrack].TrackNumber;
    cp->IndexNumber = 0; /* FIXME: where do they keep these? */
    cp->AbsoluteAddress[0] = 0; 
    cp->AbsoluteAddress[1] = toc.TrackData[i-toc.FirstTrack].Address[1];
    cp->AbsoluteAddress[2] = toc.TrackData[i-toc.FirstTrack].Address[2];
    cp->AbsoluteAddress[3] = toc.TrackData[i-toc.FirstTrack].Address[3];
    frame -= FRAME_OF_TOC(toc,i);
    cp->TrackRelativeAddress[0] = 0;
    MSF_OF_FRAME(cp->TrackRelativeAddress[1], frame); 
    RtlLeaveCriticalSection( &cache_section );

    /* If playing, then issue a seek command, otherwise do nothing */
#ifdef linux
    sc.cdsc_format = CDROM_MSF;

    io = ioctl(fd, CDROMSUBCHNL, &sc);
    if (io == -1)
    {
	TRACE("opened or no_media (%s)!\n", strerror(errno));
	CDROM_ClearCacheEntry(dev);
        return CDROM_GetStatusCode(io);
    }
    if (sc.cdsc_audiostatus==CDROM_AUDIO_PLAY)
    {
      msf.minute = audio_msf->M;
      msf.second = audio_msf->S;
      msf.frame  = audio_msf->F;
      return CDROM_GetStatusCode(ioctl(fd, CDROMSEEK, &msf));
    }
    return STATUS_SUCCESS;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    read_sc.address_format = CD_MSF_FORMAT;
    read_sc.track          = 0;
    read_sc.data_len       = sizeof(sc);
    read_sc.data           = &sc;
    read_sc.data_format    = CD_CURRENT_POSITION;

    io = ioctl(fd, CDIOCREADSUBCHANNEL, &read_sc);
    if (io == -1)
    {
	TRACE("opened or no_media (%s)!\n", strerror(errno));
	CDROM_ClearCacheEntry(dev);
        return CDROM_GetStatusCode(io);
    }
    if (sc.header.audio_status==CD_AS_PLAY_IN_PROGRESS) 
    {

      msf.start_m      = audio_msf->M;
      msf.start_s      = audio_msf->S;
      msf.start_f      = audio_msf->F;
      final_frame = FRAME_OF_TOC(toc,toc.LastTrack+1)-1;
      MSF_OF_FRAME(msf.end_m, final_frame);

      return CDROM_GetStatusCode(ioctl(fd, CDIOCPLAYMSF, &msf));
    }
    return STATUS_SUCCESS;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_PauseAudio
 *
 *
 */
static NTSTATUS CDROM_PauseAudio(int fd)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, CDROMPAUSE));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode(ioctl(fd, CDIOCPAUSE, NULL));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_ResumeAudio
 *
 *
 */
static NTSTATUS CDROM_ResumeAudio(int fd)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, CDROMRESUME));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode(ioctl(fd, CDIOCRESUME, NULL));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_StopAudio
 *
 *
 */
static NTSTATUS CDROM_StopAudio(int fd)
{
#if defined(linux)
    return CDROM_GetStatusCode(ioctl(fd, CDROMSTOP));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return CDROM_GetStatusCode(ioctl(fd, CDIOCSTOP, NULL));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_GetVolume
 *
 *
 */
static NTSTATUS CDROM_GetVolume(int fd, VOLUME_CONTROL* vc)
{
#if defined(linux)
    struct cdrom_volctrl volc;
    int io;

    io = ioctl(fd, CDROMVOLREAD, &volc);
    if (io != -1)
    {
        vc->PortVolume[0] = volc.channel0;
        vc->PortVolume[1] = volc.channel1;
        vc->PortVolume[2] = volc.channel2;
        vc->PortVolume[3] = volc.channel3;
    }
    return CDROM_GetStatusCode(io);
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    struct  ioc_vol     volc;
    int io;

    io = ioctl(fd, CDIOCGETVOL, &volc);
    if (io != -1)
    {
        vc->PortVolume[0] = volc.vol[0];
        vc->PortVolume[1] = volc.vol[1];
        vc->PortVolume[2] = volc.vol[2];
        vc->PortVolume[3] = volc.vol[3];
    }
    return CDROM_GetStatusCode(io);
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_SetVolume
 *
 *
 */
static NTSTATUS CDROM_SetVolume(int fd, const VOLUME_CONTROL* vc)
{
#if defined(linux)
    struct cdrom_volctrl volc;

    volc.channel0 = vc->PortVolume[0];
    volc.channel1 = vc->PortVolume[1];
    volc.channel2 = vc->PortVolume[2];
    volc.channel3 = vc->PortVolume[3];

    return CDROM_GetStatusCode(ioctl(fd, CDROMVOLCTRL, &volc));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    struct  ioc_vol     volc;

    volc.vol[0] = vc->PortVolume[0];
    volc.vol[1] = vc->PortVolume[1];
    volc.vol[2] = vc->PortVolume[2];
    volc.vol[3] = vc->PortVolume[3];

    return CDROM_GetStatusCode(ioctl(fd, CDIOCSETVOL, &volc));
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_RawRead
 *
 *
 */
static NTSTATUS CDROM_RawRead(int fd, const RAW_READ_INFO* raw, void* buffer, DWORD len, DWORD* sz)
{
    int	        ret = STATUS_NOT_SUPPORTED;
    int         io = -1;
    DWORD       sectSize;

    TRACE("RAW_READ_INFO: DiskOffset=%li,%li SectorCount=%li TrackMode=%i\n buffer=%p len=%li sz=%p\n",
          raw->DiskOffset.u.HighPart, raw->DiskOffset.u.LowPart, raw->SectorCount, raw->TrackMode, buffer, len, sz);

    switch (raw->TrackMode)
    {
    case YellowMode2:   sectSize = 2336;        break;
    case XAForm2:       sectSize = 2328;        break;
    case CDDA:          sectSize = 2352;        break;
    default:    return STATUS_INVALID_PARAMETER;
    }
    if (len < raw->SectorCount * sectSize) return STATUS_BUFFER_TOO_SMALL;
    /* strangely enough, it seems that sector offsets are always indicated with a size of 2048,
     * even if a larger size if read...
     */
#if defined(linux)
    {
        struct cdrom_read_audio cdra;
        struct cdrom_msf*       msf;
        int i;
        LONGLONG t = ((LONGLONG)raw->DiskOffset.u.HighPart << 32) +
                                raw->DiskOffset.u.LowPart + CD_MSF_OFFSET;

        switch (raw->TrackMode)
        {
        case YellowMode2:
            /* Linux reads only one sector at a time.
             * ioctl CDROMREADMODE2 takes struct cdrom_msf as an argument
             * on the contrary to what header comments state.
             */
            for (i = 0; i < raw->SectorCount; i++, t += sectSize)
            {
                msf = (struct cdrom_msf*)buffer + i * sectSize;
                msf->cdmsf_min0   = t / CD_FRAMES / CD_SECS;
                msf->cdmsf_sec0   = t / CD_FRAMES % CD_SECS;
                msf->cdmsf_frame0 = t % CD_FRAMES;
                io = ioctl(fd, CDROMREADMODE2, msf);
                if (io != 0)
                {
                    *sz = sectSize * i;
                    return CDROM_GetStatusCode(io);
                }
            }
            break;
        case XAForm2:
            FIXME("XAForm2: NIY\n");
            return ret;
        case CDDA:
            /* FIXME: the output doesn't seem 100% correct... in fact output is shifted
             * between by NT2K box and this... should check on the same drive...
             * otherwise, I fear a 2352/2368 mismatch somewhere in one of the drivers
             * (linux/NT).
             * Anyway, that's not critical at all. We're talking of 16/32 bytes, we're
             * talking of 0.2 ms of sound
             */
            /* 2048 = 2 ** 11 */
            if (raw->DiskOffset.u.HighPart & ~2047) FIXME("Unsupported value\n");
            cdra.addr.lba = ((raw->DiskOffset.u.LowPart >> 11) |
                (raw->DiskOffset.u.HighPart << (32 - 11))) - 1;
            FIXME("reading at %u\n", cdra.addr.lba);
            cdra.addr_format = CDROM_LBA;
            cdra.nframes = raw->SectorCount;
            cdra.buf = buffer;
            io = ioctl(fd, CDROMREADAUDIO, &cdra);
            break;
        default:
            FIXME("NIY: %d\n", raw->TrackMode);
            return ret;
        }
    }
#else
    {
        switch (raw->TrackMode)
        {
        case YellowMode2:
            FIXME("YellowMode2: NIY\n");
            return ret;
        case XAForm2:
            FIXME("XAForm2: NIY\n");
            return ret;
        case CDDA:
            FIXME("CDDA: NIY\n");
            return ret;
        }
    }
#endif

    *sz = sectSize * raw->SectorCount;
    ret = CDROM_GetStatusCode(io);
    return ret;
}

/******************************************************************
 *        CDROM_ScsiPassThroughDirect
 *        Implements IOCTL_SCSI_PASS_THROUGH_DIRECT
 *
 */
static NTSTATUS CDROM_ScsiPassThroughDirect(int fd, PSCSI_PASS_THROUGH_DIRECT pPacket)
{
    int ret = STATUS_NOT_SUPPORTED;
#ifdef HAVE_SG_IO_HDR_T_INTERFACE_ID
    sg_io_hdr_t cmd;
    int io;
#elif defined HAVE_SCSIREQ_T_CMD
    scsireq_t cmd;
    int io;
#endif

    if (pPacket->Length < sizeof(SCSI_PASS_THROUGH_DIRECT))
        return STATUS_BUFFER_TOO_SMALL;

    if (pPacket->CdbLength > 16)
        return STATUS_INVALID_PARAMETER;

#ifdef SENSEBUFLEN
    if (pPacket->SenseInfoLength > SENSEBUFLEN)
        return STATUS_INVALID_PARAMETER;
#elif defined HAVE_REQUEST_SENSE
    if (pPacket->SenseInfoLength > sizeof(struct request_sense))
        return STATUS_INVALID_PARAMETER;
#endif

    if (pPacket->DataTransferLength > 0 && !pPacket->DataBuffer)
        return STATUS_INVALID_PARAMETER;

#ifdef HAVE_SG_IO_HDR_T_INTERFACE_ID
    RtlZeroMemory(&cmd, sizeof(cmd));

    cmd.interface_id   = 'S';
    cmd.cmd_len        = pPacket->CdbLength;
    cmd.mx_sb_len      = pPacket->SenseInfoLength;
    cmd.dxfer_len      = pPacket->DataTransferLength;
    cmd.dxferp         = pPacket->DataBuffer;
    cmd.cmdp           = pPacket->Cdb;
    cmd.sbp            = (unsigned char*)pPacket + pPacket->SenseInfoOffset;
    cmd.timeout        = pPacket->TimeOutValue*1000;

    switch (pPacket->DataIn)
    {
    case SCSI_IOCTL_DATA_IN:
        cmd.dxfer_direction = SG_DXFER_FROM_DEV;
        break;
    case SCSI_IOCTL_DATA_OUT:
        cmd.dxfer_direction = SG_DXFER_TO_DEV;
        break;
    case SCSI_IOCTL_DATA_UNSPECIFIED:
        cmd.dxfer_direction = SG_DXFER_NONE;
        break;
    default:
       return STATUS_INVALID_PARAMETER;
    }

    io = ioctl(fd, SG_IO, &cmd);

    pPacket->ScsiStatus         = cmd.status;
    pPacket->DataTransferLength = cmd.resid;
    pPacket->SenseInfoLength    = cmd.sb_len_wr;

    ret = CDROM_GetStatusCode(io);

#elif defined HAVE_SCSIREQ_T_CMD

    memset(&cmd, 0, sizeof(cmd));
    memcpy(&(cmd.cmd), &(pPacket->Cdb), pPacket->CdbLength);

    cmd.cmdlen         = pPacket->CdbLength;
    cmd.databuf        = pPacket->DataBuffer;
    cmd.datalen        = pPacket->DataTransferLength;
    cmd.senselen       = pPacket->SenseInfoLength;
    cmd.timeout        = pPacket->TimeOutValue*1000; /* in milliseconds */

    switch (pPacket->DataIn)
    {
    case SCSI_IOCTL_DATA_OUT:
        cmd.flags |= SCCMD_WRITE;
	break;
    case SCSI_IOCTL_DATA_IN:
        cmd.flags |= SCCMD_READ;
	break;
    case SCSI_IOCTL_DATA_UNSPECIFIED:
        cmd.flags = 0;
	break;
    default:
       return STATUS_INVALID_PARAMETER;
    }

    io = ioctl(fd, SCIOCCOMMAND, &cmd);

    switch (cmd.retsts)
    {
    case SCCMD_OK:         break;
    case SCCMD_TIMEOUT:    return STATUS_TIMEOUT;
                           break;
    case SCCMD_BUSY:       return STATUS_DEVICE_BUSY;
                           break;
    case SCCMD_SENSE:      break;
    case SCCMD_UNKNOWN:    return STATUS_UNSUCCESSFUL;
                           break;
    }

    if (pPacket->SenseInfoLength != 0)
    {
        memcpy((char*)pPacket + pPacket->SenseInfoOffset,
	       cmd.sense, pPacket->SenseInfoLength);
    }

    pPacket->ScsiStatus = cmd.status;

    ret = CDROM_GetStatusCode(io);
#endif
    return ret;
}

/******************************************************************
 *              CDROM_ScsiPassThrough
 *              Implements IOCTL_SCSI_PASS_THROUGH
 *
 */
static NTSTATUS CDROM_ScsiPassThrough(int fd, PSCSI_PASS_THROUGH pPacket)
{
    int ret = STATUS_NOT_SUPPORTED;
#ifdef HAVE_SG_IO_HDR_T_INTERFACE_ID
    sg_io_hdr_t cmd;
    int io;
#elif defined HAVE_SCSIREQ_T_CMD
    scsireq_t cmd;
    int io;
#endif

    if (pPacket->Length < sizeof(SCSI_PASS_THROUGH))
        return STATUS_BUFFER_TOO_SMALL;

    if (pPacket->CdbLength > 16)
        return STATUS_INVALID_PARAMETER;

#ifdef SENSEBUFLEN
    if (pPacket->SenseInfoLength > SENSEBUFLEN)
        return STATUS_INVALID_PARAMETER;
#elif defined HAVE_REQUEST_SENSE
    if (pPacket->SenseInfoLength > sizeof(struct request_sense))
        return STATUS_INVALID_PARAMETER;
#endif

    if (pPacket->DataTransferLength > 0 && pPacket->DataBufferOffset < sizeof(SCSI_PASS_THROUGH))
        return STATUS_INVALID_PARAMETER;

#ifdef HAVE_SG_IO_HDR_T_INTERFACE_ID
    RtlZeroMemory(&cmd, sizeof(cmd));

    cmd.interface_id   = 'S';
    cmd.dxfer_len      = pPacket->DataTransferLength;
    cmd.dxferp         = (char*)pPacket + pPacket->DataBufferOffset;
    cmd.cmd_len        = pPacket->CdbLength;
    cmd.cmdp           = pPacket->Cdb;
    cmd.mx_sb_len      = pPacket->SenseInfoLength;
    cmd.timeout        = pPacket->TimeOutValue*1000;

    if(cmd.mx_sb_len > 0)
        cmd.sbp = (unsigned char*)pPacket + pPacket->SenseInfoOffset;

    switch (pPacket->DataIn)
    {
    case SCSI_IOCTL_DATA_IN:
        cmd.dxfer_direction = SG_DXFER_FROM_DEV;
                             break;
    case SCSI_IOCTL_DATA_OUT:
        cmd.dxfer_direction = SG_DXFER_TO_DEV;
                             break;
    case SCSI_IOCTL_DATA_UNSPECIFIED:
        cmd.dxfer_direction = SG_DXFER_NONE;
                             break;
    default:
       return STATUS_INVALID_PARAMETER;
    }

    io = ioctl(fd, SG_IO, &cmd);

    pPacket->ScsiStatus         = cmd.status;
    pPacket->DataTransferLength = cmd.resid;
    pPacket->SenseInfoLength    = cmd.sb_len_wr;

    ret = CDROM_GetStatusCode(io);

#elif defined HAVE_SCSIREQ_T_CMD

    memset(&cmd, 0, sizeof(cmd));
    memcpy(&(cmd.cmd), &(pPacket->Cdb), pPacket->CdbLength);

    if ( pPacket->DataBufferOffset > 0x1000 )
    {
        cmd.databuf     = (void*)pPacket->DataBufferOffset;
    }
    else
    {
        cmd.databuf     = (char*)pPacket + pPacket->DataBufferOffset;
    }

    cmd.cmdlen         = pPacket->CdbLength;
    cmd.datalen        = pPacket->DataTransferLength;
    cmd.senselen       = pPacket->SenseInfoLength;
    cmd.timeout        = pPacket->TimeOutValue*1000; /* in milliseconds */

    switch (pPacket->DataIn)
    {
    case SCSI_IOCTL_DATA_OUT:
        cmd.flags |= SCCMD_WRITE;
	break;
    case SCSI_IOCTL_DATA_IN:
        cmd.flags |= SCCMD_READ;
	break;
    case SCSI_IOCTL_DATA_UNSPECIFIED:
        cmd.flags = 0;
	break;
    default:
       return STATUS_INVALID_PARAMETER;
    }

    io = ioctl(fd, SCIOCCOMMAND, &cmd);

    switch (cmd.retsts)
    {
    case SCCMD_OK:         break;
    case SCCMD_TIMEOUT:    return STATUS_TIMEOUT;
                           break;
    case SCCMD_BUSY:       return STATUS_DEVICE_BUSY;
                           break;
    case SCCMD_SENSE:      break;
    case SCCMD_UNKNOWN:    return STATUS_UNSUCCESSFUL;
                           break;
    }

    if (pPacket->SenseInfoLength != 0)
    {
        memcpy((char*)pPacket + pPacket->SenseInfoOffset,
	       cmd.sense, pPacket->SenseInfoLength);
    }

    pPacket->ScsiStatus = cmd.status;

    ret = CDROM_GetStatusCode(io);
#endif
    return ret;
}

/******************************************************************
 *		CDROM_ScsiGetCaps
 *
 *
 */
static NTSTATUS CDROM_ScsiGetCaps(PIO_SCSI_CAPABILITIES caps)
{
    NTSTATUS    ret = STATUS_NOT_IMPLEMENTED;

    caps->Length = sizeof(*caps);
#ifdef SG_SCATTER_SZ
    caps->MaximumTransferLength = SG_SCATTER_SZ; /* FIXME */
    caps->MaximumPhysicalPages = SG_SCATTER_SZ / getpagesize();
    caps->SupportedAsynchronousEvents = TRUE;
    caps->AlignmentMask = getpagesize();
    caps->TaggedQueuing = FALSE; /* we could check that it works and answer TRUE */
    caps->AdapterScansDown = FALSE; /* FIXME ? */
    caps->AdapterUsesPio = FALSE; /* FIXME ? */
    ret = STATUS_SUCCESS;
#else
    FIXME("Unimplemented\n");
#endif
    return ret;
}

/******************************************************************
 *		CDROM_GetAddress
 *
 * implements IOCTL_SCSI_GET_ADDRESS
 */
static NTSTATUS CDROM_GetAddress(int fd, SCSI_ADDRESS* address)
{
    UCHAR portnum, busid, targetid, lun;

    address->Length = sizeof(SCSI_ADDRESS);
    if ( ! CDROM_GetInterfaceInfo(fd, &portnum, &busid, &targetid, &lun))
        return STATUS_NOT_SUPPORTED;

    address->PortNumber = portnum; /* primary=0 secondary=1 for ide */
    address->PathId = busid;       /* always 0 for ide */
    address->TargetId = targetid;  /* master=0 slave=1 for ide */
    address->Lun = lun;
    return STATUS_SUCCESS;
}

/******************************************************************
 *		DVD_StartSession
 *
 *
 */
static NTSTATUS DVD_StartSession(int fd, PDVD_SESSION_ID sid_in, PDVD_SESSION_ID sid_out)
{
#if defined(linux)
    NTSTATUS ret = STATUS_NOT_SUPPORTED;
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_LU_SEND_AGID;
    if (sid_in) auth_info.lsa.agid = *(int*)sid_in; /* ?*/

    TRACE("fd 0x%08x\n",fd);
    ret =CDROM_GetStatusCode(ioctl(fd, DVD_AUTH, &auth_info));
    *sid_out = auth_info.lsa.agid;
    return ret;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return STATUS_NOT_SUPPORTED;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		DVD_EndSession
 *
 *
 */
static NTSTATUS DVD_EndSession(int fd, PDVD_SESSION_ID sid)
{
#if defined(linux)
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    auth_info.type = DVD_INVALIDATE_AGID;
    auth_info.lsa.agid = *(int*)sid;

    TRACE("\n");
    return CDROM_GetStatusCode(ioctl(fd, DVD_AUTH, &auth_info));
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    return STATUS_NOT_SUPPORTED;
#else
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		DVD_SendKey
 *
 *
 */
static NTSTATUS DVD_SendKey(int fd, PDVD_COPY_PROTECT_KEY key)
{
#if defined(linux)
    NTSTATUS ret = STATUS_NOT_SUPPORTED;
    dvd_authinfo auth_info;

    memset( &auth_info, 0, sizeof( auth_info ) );
    switch (key->KeyType)
    {
    case DvdChallengeKey:
	auth_info.type = DVD_HOST_SEND_CHALLENGE;
	auth_info.hsc.agid = (int)key->SessionId;
	TRACE("DvdChallengeKey ioc 0x%x\n", DVD_AUTH );
	memcpy( auth_info.hsc.chal, key->KeyData, DVD_CHALLENGE_SIZE );
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));
	break;
    case DvdBusKey2:
	auth_info.type = DVD_HOST_SEND_KEY2;
	auth_info.hsk.agid = (int)key->SessionId;

	memcpy( auth_info.hsk.key, key->KeyData, DVD_KEY_SIZE );
	
	TRACE("DvdBusKey2\n");
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));
	break;

    default:
	FIXME("Unknown Keytype 0x%x\n",key->KeyType);
    }
    return ret;
#else
    FIXME("unsupported on this platform\n");
    return STATUS_NOT_SUPPORTED;
#endif    
}

/******************************************************************
 *		DVD_ReadKey
 *
 *
 */
static NTSTATUS DVD_ReadKey(int fd, PDVD_COPY_PROTECT_KEY key)
{
#if defined(linux)
    NTSTATUS ret = STATUS_NOT_SUPPORTED;
    dvd_struct dvd;
    dvd_authinfo auth_info;

    memset( &dvd, 0, sizeof( dvd_struct ) );
    memset( &auth_info, 0, sizeof( auth_info ) );
    switch (key->KeyType)
    {
    case DvdDiskKey:
	
	dvd.type = DVD_STRUCT_DISCKEY;
	dvd.disckey.agid = (int)key->SessionId;
	memset( dvd.disckey.value, 0, DVD_DISCKEY_SIZE );
	
	TRACE("DvdDiskKey\n");
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_READ_STRUCT, &dvd ));
	if (ret == STATUS_SUCCESS)
	    memcpy(key->KeyData,dvd.disckey.value,DVD_DISCKEY_SIZE);
	break;
    case DvdTitleKey:
	auth_info.type = DVD_LU_SEND_TITLE_KEY;
	auth_info.lstk.agid = (int)key->SessionId;
	auth_info.lstk.lba = (int)(key->Parameters.TitleOffset.QuadPart>>11);
	TRACE("DvdTitleKey session %d Quadpart 0x%08lx offset 0x%08x\n",
	      (int)key->SessionId, (long)key->Parameters.TitleOffset.QuadPart, 
	      auth_info.lstk.lba);
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));
	if (ret == STATUS_SUCCESS)
	    memcpy(key->KeyData, auth_info.lstk.title_key, DVD_KEY_SIZE );
	break;
    case DvdChallengeKey:

	auth_info.type = DVD_LU_SEND_CHALLENGE;
	auth_info.lsc.agid = (int)key->SessionId;

	TRACE("DvdChallengeKey\n");
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));
	if (ret == STATUS_SUCCESS)
	    memcpy( key->KeyData, auth_info.lsc.chal, DVD_CHALLENGE_SIZE );
	break;
    case DvdAsf:
	auth_info.type = DVD_LU_SEND_ASF;
	TRACE("DvdAsf\n");
	auth_info.lsasf.asf=((PDVD_ASF)key->KeyData)->SuccessFlag;
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));
	((PDVD_ASF)key->KeyData)->SuccessFlag = auth_info.lsasf.asf;
	break;
    case DvdBusKey1:
	auth_info.type = DVD_LU_SEND_KEY1;
	auth_info.lsk.agid = (int)key->SessionId;

	TRACE("DvdBusKey1\n");
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));

	if (ret == STATUS_SUCCESS)
	    memcpy( key->KeyData, auth_info.lsk.key, DVD_KEY_SIZE );
	break;
    case DvdGetRpcKey:
	auth_info.type = DVD_LU_SEND_RPC_STATE;

	TRACE("DvdGetRpcKey\n");
	ret = CDROM_GetStatusCode(ioctl( fd, DVD_AUTH, &auth_info ));

	if (ret == STATUS_SUCCESS)
	{
	    ((PDVD_RPC_KEY)key->KeyData)->TypeCode = auth_info.lrpcs.type;
	    ((PDVD_RPC_KEY)key->KeyData)->RegionMask = auth_info.lrpcs.region_mask;
	    ((PDVD_RPC_KEY)key->KeyData)->RpcScheme = auth_info.lrpcs.rpc_scheme;
	}
	break;
    default:
	FIXME("Unknown keytype 0x%x\n",key->KeyType);
    }
    return ret;
#elif defined(__FreeBSD__) || defined(__NetBSD__)
    TRACE("bsd\n");
    return STATUS_NOT_SUPPORTED;
#else
    TRACE("outside\n");
    return STATUS_NOT_SUPPORTED;
#endif
    TRACE("not reached\n");
}

/******************************************************************
 *		DVD_GetRegion
 *
 *
 */
static NTSTATUS DVD_GetRegion(int dev, PDVD_REGION region)
{
    FIXME("\n");
    return STATUS_SUCCESS;

}

/******************************************************************
 *		DVD_GetRegion
 *
 *
 */
static NTSTATUS DVD_ReadStructure(int dev, PDVD_READ_STRUCTURE structure, PDVD_LAYER_DESCRIPTOR layer)
{
    FIXME("\n");
    return STATUS_SUCCESS;

}

/******************************************************************
 *        GetInquiryData
 *        Implements the IOCTL_GET_INQUIRY_DATA ioctl.
 *        Returns Inquiry data for all devices on the specified scsi bus
 *        Returns STATUS_BUFFER_TOO_SMALL if the output buffer is too small, 
 *        STATUS_INVALID_DEVICE_REQUEST if the given handle isn't to a SCSI device,
 *        or STATUS_NOT_SUPPORTED if the OS driver is too old
 */
static NTSTATUS GetInquiryData(int fd, PSCSI_ADAPTER_BUS_INFO BufferOut, DWORD OutBufferSize)
{
#ifdef HAVE_SG_IO_HDR_T_INTERFACE_ID
    PSCSI_INQUIRY_DATA pInquiryData = NULL;
    UCHAR sense_buffer[32];
    int iochk, version;
    sg_io_hdr_t iocmd;
    UCHAR inquiry[INQ_CMD_LEN] = {INQUIRY, 0, 0, 0, INQ_REPLY_LEN, 0};

    /* Check we have a SCSI device and a supported driver */
    if(ioctl(fd, SG_GET_VERSION_NUM, &version) != 0)
    {
        WARN("IOCTL_SCSI_GET_INQUIRY_DATA sg driver is not loaded\n");
        return STATUS_INVALID_DEVICE_REQUEST;
    }
    if(version < 30000 )
        return STATUS_NOT_SUPPORTED;

    /* FIXME: Enumerate devices on the bus */
    BufferOut->NumberOfBuses = 1;
    BufferOut->BusData[0].NumberOfLogicalUnits = 1;
    BufferOut->BusData[0].InquiryDataOffset = sizeof(SCSI_ADAPTER_BUS_INFO);

    pInquiryData = (PSCSI_INQUIRY_DATA)(BufferOut + 1);

    RtlZeroMemory(&iocmd, sizeof(iocmd));
    iocmd.interface_id = 'S';
    iocmd.cmd_len = sizeof(inquiry);
    iocmd.mx_sb_len = sizeof(sense_buffer);
    iocmd.dxfer_direction = SG_DXFER_FROM_DEV;
    iocmd.dxfer_len = INQ_REPLY_LEN;
    iocmd.dxferp = pInquiryData->InquiryData;
    iocmd.cmdp = inquiry;
    iocmd.sbp = sense_buffer;
    iocmd.timeout = 1000;

    iochk = ioctl(fd, SG_IO, &iocmd);
    if(iochk != 0)
        WARN("ioctl SG_IO returned %d, error (%s)\n", iochk, strerror(errno));

    CDROM_GetInterfaceInfo(fd, &BufferOut->BusData[0].InitiatorBusId, &pInquiryData->PathId, &pInquiryData->TargetId, &pInquiryData->Lun);
    pInquiryData->DeviceClaimed = TRUE;
    pInquiryData->InquiryDataLength = INQ_REPLY_LEN;
    pInquiryData->NextInquiryDataOffset = 0;
    return STATUS_SUCCESS;
#else
    FIXME("not implemented for nonlinux\n");
    return STATUS_NOT_SUPPORTED;
#endif
}

/******************************************************************
 *		CDROM_DeviceIoControl
 *
 *
 */
NTSTATUS CDROM_DeviceIoControl(HANDLE hDevice, 
                               HANDLE hEvent, PIO_APC_ROUTINE UserApcRoutine,
                               PVOID UserApcContext, 
                               PIO_STATUS_BLOCK piosb, 
                               ULONG dwIoControlCode,
                               LPVOID lpInBuffer, DWORD nInBufferSize,
                               LPVOID lpOutBuffer, DWORD nOutBufferSize)
{
    DWORD       sz = 0;
    NTSTATUS    status = STATUS_SUCCESS;
    int fd, dev;

    TRACE("%p %s %p %ld %p %ld %p\n",
          hDevice, iocodex(dwIoControlCode), lpInBuffer, nInBufferSize,
          lpOutBuffer, nOutBufferSize, piosb);

    piosb->Information = 0;

    if ((status = wine_server_handle_to_fd( hDevice, 0, &fd, NULL ))) goto error;
    if ((status = CDROM_Open(fd, &dev)))
    {
        wine_server_release_fd( hDevice, fd );
        goto error;
    }

    switch (dwIoControlCode)
    {
    case IOCTL_STORAGE_CHECK_VERIFY:
    case IOCTL_CDROM_CHECK_VERIFY:
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_Verify(dev, fd);
        break;

/* EPP     case IOCTL_STORAGE_CHECK_VERIFY2: */

/* EPP     case IOCTL_STORAGE_FIND_NEW_DEVICES: */
/* EPP     case IOCTL_CDROM_FIND_NEW_DEVICES: */

    case IOCTL_STORAGE_LOAD_MEDIA:
    case IOCTL_CDROM_LOAD_MEDIA:
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_SetTray(fd, FALSE);
        break;
     case IOCTL_STORAGE_EJECT_MEDIA:
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_SetTray(fd, TRUE);
        break;

    case IOCTL_CDROM_MEDIA_REMOVAL:
    case IOCTL_DISK_MEDIA_REMOVAL:
    case IOCTL_STORAGE_MEDIA_REMOVAL:
    case IOCTL_STORAGE_EJECTION_CONTROL:
        /* FIXME the last ioctl:s is not the same as the two others...
         * lockcount/owner should be handled */
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpOutBuffer != NULL || nOutBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nInBufferSize < sizeof(PREVENT_MEDIA_REMOVAL)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ControlEjection(fd, (const PREVENT_MEDIA_REMOVAL*)lpInBuffer);
        break;

/* EPP     case IOCTL_STORAGE_GET_MEDIA_TYPES: */

    case IOCTL_STORAGE_GET_DEVICE_NUMBER:
        sz = sizeof(STORAGE_DEVICE_NUMBER);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetDeviceNumber(dev, (STORAGE_DEVICE_NUMBER*)lpOutBuffer);
        break;

    case IOCTL_STORAGE_RESET_DEVICE:
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_ResetAudio(fd);
        break;

    case IOCTL_CDROM_GET_CONTROL:
        sz = sizeof(CDROM_AUDIO_CONTROL);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetControl(dev, (CDROM_AUDIO_CONTROL*)lpOutBuffer);
        break;

    case IOCTL_CDROM_GET_DRIVE_GEOMETRY:
        sz = sizeof(DISK_GEOMETRY);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetDriveGeometry(dev, fd, (DISK_GEOMETRY*)lpOutBuffer);
        break;

    case IOCTL_CDROM_DISK_TYPE:
        sz = sizeof(CDROM_DISK_DATA);
	/* CDROM_ClearCacheEntry(dev); */
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetDiskData(dev, fd, (CDROM_DISK_DATA*)lpOutBuffer);
        break;

/* EPP     case IOCTL_CDROM_GET_LAST_SESSION: */

    case IOCTL_CDROM_READ_Q_CHANNEL:
        sz = sizeof(SUB_Q_CHANNEL_DATA);
        if (lpInBuffer == NULL || nInBufferSize < sizeof(CDROM_SUB_Q_DATA_FORMAT))
            status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ReadQChannel(dev, fd, (const CDROM_SUB_Q_DATA_FORMAT*)lpInBuffer,
                                        (SUB_Q_CHANNEL_DATA*)lpOutBuffer);
        break;

    case IOCTL_CDROM_READ_TOC:
        sz = sizeof(CDROM_TOC);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ReadTOC(dev, fd, (CDROM_TOC*)lpOutBuffer);
        break;

/* EPP     case IOCTL_CDROM_READ_TOC_EX: */

    case IOCTL_CDROM_PAUSE_AUDIO:
        sz = 0;
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_PauseAudio(fd);
        break;
    case IOCTL_CDROM_PLAY_AUDIO_MSF:
        sz = 0;
        if (lpOutBuffer != NULL || nOutBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nInBufferSize < sizeof(CDROM_PLAY_AUDIO_MSF)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_PlayAudioMSF(fd, (const CDROM_PLAY_AUDIO_MSF*)lpInBuffer);
        break;
    case IOCTL_CDROM_RESUME_AUDIO:
        sz = 0;
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_ResumeAudio(fd);
        break;
    case IOCTL_CDROM_SEEK_AUDIO_MSF:
        sz = 0;
        if (lpOutBuffer != NULL || nOutBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nInBufferSize < sizeof(CDROM_SEEK_AUDIO_MSF)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_SeekAudioMSF(dev, fd, (const CDROM_SEEK_AUDIO_MSF*)lpInBuffer);
        break;
    case IOCTL_CDROM_STOP_AUDIO:
        sz = 0;
	CDROM_ClearCacheEntry(dev); /* Maybe intention is to change media */
        if (lpInBuffer != NULL || nInBufferSize != 0 || lpOutBuffer != NULL || nOutBufferSize != 0)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_StopAudio(fd);
        break;
    case IOCTL_CDROM_GET_VOLUME:
        sz = sizeof(VOLUME_CONTROL);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetVolume(fd, (VOLUME_CONTROL*)lpOutBuffer);
        break;
    case IOCTL_CDROM_SET_VOLUME:
        sz = 0;
	CDROM_ClearCacheEntry(dev);
        if (lpInBuffer == NULL || nInBufferSize < sizeof(VOLUME_CONTROL) || lpOutBuffer != NULL)
            status = STATUS_INVALID_PARAMETER;
        else status = CDROM_SetVolume(fd, (const VOLUME_CONTROL*)lpInBuffer);
        break;
    case IOCTL_CDROM_RAW_READ:
        sz = 0;
        if (nInBufferSize < sizeof(RAW_READ_INFO)) status = STATUS_INVALID_PARAMETER;
        else if (lpOutBuffer == NULL) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_RawRead(fd, (const RAW_READ_INFO*)lpInBuffer,
                                   lpOutBuffer, nOutBufferSize, &sz);
        break;
    case IOCTL_SCSI_GET_ADDRESS:
        sz = sizeof(SCSI_ADDRESS);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_GetAddress(fd, (SCSI_ADDRESS*)lpOutBuffer);
        break;
    case IOCTL_SCSI_PASS_THROUGH_DIRECT:
        sz = sizeof(SCSI_PASS_THROUGH_DIRECT);
        if (lpOutBuffer == NULL) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sizeof(SCSI_PASS_THROUGH_DIRECT)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ScsiPassThroughDirect(fd, (PSCSI_PASS_THROUGH_DIRECT)lpOutBuffer);
        break;
    case IOCTL_SCSI_PASS_THROUGH:
        sz = sizeof(SCSI_PASS_THROUGH);
        if (lpOutBuffer == NULL) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sizeof(SCSI_PASS_THROUGH)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ScsiPassThrough(fd, (PSCSI_PASS_THROUGH)lpOutBuffer);
        break;
    case IOCTL_SCSI_GET_CAPABILITIES:
        sz = sizeof(IO_SCSI_CAPABILITIES);
        if (lpOutBuffer == NULL) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sizeof(IO_SCSI_CAPABILITIES)) status = STATUS_BUFFER_TOO_SMALL;
        else status = CDROM_ScsiGetCaps((PIO_SCSI_CAPABILITIES)lpOutBuffer);
        break;
    case IOCTL_DVD_START_SESSION:
        sz = sizeof(DVD_SESSION_ID);
        if (lpOutBuffer == NULL) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            TRACE("before in 0x%08lx out 0x%08lx\n",(lpInBuffer)?*(PDVD_SESSION_ID)lpInBuffer:0,
                  *(PDVD_SESSION_ID)lpOutBuffer);
            status = DVD_StartSession(fd, (PDVD_SESSION_ID)lpInBuffer, (PDVD_SESSION_ID)lpOutBuffer);
            TRACE("before in 0x%08lx out 0x%08lx\n",(lpInBuffer)?*(PDVD_SESSION_ID)lpInBuffer:0,
                  *(PDVD_SESSION_ID)lpOutBuffer);
        }
        break;
    case IOCTL_DVD_END_SESSION:
        sz = sizeof(DVD_SESSION_ID);
        if ((lpInBuffer == NULL) ||  (nInBufferSize < sz))status = STATUS_INVALID_PARAMETER;
        else status = DVD_EndSession(fd, (PDVD_SESSION_ID)lpInBuffer);
        break;
    case IOCTL_DVD_SEND_KEY:
        sz = 0;
        if (!lpInBuffer ||
            (((PDVD_COPY_PROTECT_KEY)lpInBuffer)->KeyLength != nInBufferSize))
            status = STATUS_INVALID_PARAMETER;
        else
        {
            TRACE("doing DVD_SendKey\n");
            status = DVD_SendKey(fd, (PDVD_COPY_PROTECT_KEY)lpInBuffer);
        }
        break;
    case IOCTL_DVD_READ_KEY:
        if (!lpInBuffer ||
            (((PDVD_COPY_PROTECT_KEY)lpInBuffer)->KeyLength != nInBufferSize))
            status = STATUS_INVALID_PARAMETER;
        else if (lpInBuffer !=lpOutBuffer) status = STATUS_BUFFER_TOO_SMALL;
        else
        {
            TRACE("doing DVD_READ_KEY\n");
            sz = ((PDVD_COPY_PROTECT_KEY)lpInBuffer)->KeyLength;
            status = DVD_ReadKey(fd, (PDVD_COPY_PROTECT_KEY)lpInBuffer);
        }
        break;
    case IOCTL_DVD_GET_REGION:
        sz = sizeof(DVD_REGION);
        if (lpInBuffer != NULL || nInBufferSize != 0) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        TRACE("doing DVD_Get_REGION\n");
        status = DVD_GetRegion(fd, (PDVD_REGION)lpOutBuffer);
        break;
    case IOCTL_DVD_READ_STRUCTURE:
        sz = sizeof(DVD_LAYER_DESCRIPTOR);
        if (lpInBuffer == NULL || nInBufferSize != sizeof(DVD_READ_STRUCTURE)) status = STATUS_INVALID_PARAMETER;
        else if (nOutBufferSize < sz) status = STATUS_BUFFER_TOO_SMALL;
        TRACE("doing DVD_READ_STRUCTURE\n");
        status = DVD_ReadStructure(fd, (PDVD_READ_STRUCTURE)lpInBuffer, (PDVD_LAYER_DESCRIPTOR)lpOutBuffer);
        break;

    case IOCTL_SCSI_GET_INQUIRY_DATA:
        sz = INQ_REPLY_LEN;
        status = GetInquiryData(fd, (PSCSI_ADAPTER_BUS_INFO)lpOutBuffer, nOutBufferSize);
        break;

    default:
        FIXME("Unsupported IOCTL %lx (type=%lx access=%lx func=%lx meth=%lx)\n", 
              dwIoControlCode, dwIoControlCode >> 16, (dwIoControlCode >> 14) & 3,
              (dwIoControlCode >> 2) & 0xFFF, dwIoControlCode & 3);
        sz = 0;
        status = STATUS_INVALID_PARAMETER;
        break;
    }
    wine_server_release_fd( hDevice, fd );
 error:
    piosb->u.Status = status;
    piosb->Information = sz;
    if (hEvent) NtSetEvent(hEvent, NULL);
    return status;
}
