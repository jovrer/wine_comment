/*
 * Win32 virtual memory functions
 *
 * Copyright 1997, 2002 Alexandre Julliard
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

#include <assert.h>
#include <errno.h>
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "ntstatus.h"
#include "windef.h"
#include "winternl.h"
#include "winioctl.h"
#include "wine/library.h"
#include "wine/server.h"
#include "wine/list.h"
#include "wine/debug.h"
#include "ntdll_misc.h"

WINE_DEFAULT_DEBUG_CHANNEL(virtual);
WINE_DECLARE_DEBUG_CHANNEL(module);

#ifndef MS_SYNC
#define MS_SYNC 0
#endif

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

/* File view */
typedef struct file_view
{
    struct list   entry;       /* Entry in global view list */
    void         *base;        /* Base address */
    size_t        size;        /* Size in bytes */
    HANDLE        mapping;     /* Handle to the file mapping */
    BYTE          flags;       /* Allocation flags (VFLAG_*) */
    BYTE          protect;     /* Protection for all pages at allocation time */
    BYTE          prot[1];     /* Protection byte for each page */
} FILE_VIEW;

/* Per-view flags */
#define VFLAG_SYSTEM     0x01  /* system view (underlying mmap not under our control) */
#define VFLAG_VALLOC     0x02  /* allocated by VirtualAlloc */

/* Conversion from VPROT_* to Win32 flags */
static const BYTE VIRTUAL_Win32Flags[16] =
{
    PAGE_NOACCESS,              /* 0 */
    PAGE_READONLY,              /* READ */
    PAGE_READWRITE,             /* WRITE */
    PAGE_READWRITE,             /* READ | WRITE */
    PAGE_EXECUTE,               /* EXEC */
    PAGE_EXECUTE_READ,          /* READ | EXEC */
    PAGE_EXECUTE_READWRITE,     /* WRITE | EXEC */
    PAGE_EXECUTE_READWRITE,     /* READ | WRITE | EXEC */
    PAGE_WRITECOPY,             /* WRITECOPY */
    PAGE_WRITECOPY,             /* READ | WRITECOPY */
    PAGE_WRITECOPY,             /* WRITE | WRITECOPY */
    PAGE_WRITECOPY,             /* READ | WRITE | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* READ | EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY,     /* WRITE | EXEC | WRITECOPY */
    PAGE_EXECUTE_WRITECOPY      /* READ | WRITE | EXEC | WRITECOPY */
};

static struct list views_list = LIST_INIT(views_list);

static RTL_CRITICAL_SECTION csVirtual;
static RTL_CRITICAL_SECTION_DEBUG critsect_debug =
{
    0, 0, &csVirtual,
    { &critsect_debug.ProcessLocksList, &critsect_debug.ProcessLocksList },
      0, 0, { (DWORD_PTR)(__FILE__ ": csVirtual") }
};
static RTL_CRITICAL_SECTION csVirtual = { &critsect_debug, -1, 0, 0, 0, 0 };

#ifdef __i386__
/* These are always the same on an i386, and it will be faster this way */
# define page_mask  0xfff
# define page_shift 12
# define page_size  0x1000
/* Note: these are Windows limits, you cannot change them. */
# define ADDRESS_SPACE_LIMIT  ((void *)0xc0000000)  /* top of the total available address space */
# define USER_SPACE_LIMIT     ((void *)0x80000000)  /* top of the user address space */
#else
static UINT page_shift;
static UINT page_size;
static UINT_PTR page_mask;
# define ADDRESS_SPACE_LIMIT  0   /* no limit needed on other platforms */
# define USER_SPACE_LIMIT     0   /* no limit needed on other platforms */
#endif  /* __i386__ */
static const UINT_PTR granularity_mask = 0xffff;  /* Allocation granularity (usually 64k) */

#define ROUND_ADDR(addr,mask) \
   ((void *)((UINT_PTR)(addr) & ~(UINT_PTR)(mask)))

#define ROUND_SIZE(addr,size) \
   (((UINT)(size) + ((UINT_PTR)(addr) & page_mask) + page_mask) & ~page_mask)

#define VIRTUAL_DEBUG_DUMP_VIEW(view) \
    do { if (TRACE_ON(virtual)) VIRTUAL_DumpView(view); } while (0)

static void *user_space_limit = USER_SPACE_LIMIT;


/***********************************************************************
 *           VIRTUAL_GetProtStr
 */
static const char *VIRTUAL_GetProtStr( BYTE prot )
{
    static char buffer[6];
    buffer[0] = (prot & VPROT_COMMITTED) ? 'c' : '-';
    buffer[1] = (prot & VPROT_GUARD) ? 'g' : '-';
    buffer[2] = (prot & VPROT_READ) ? 'r' : '-';
    buffer[3] = (prot & VPROT_WRITECOPY) ? 'W' : ((prot & VPROT_WRITE) ? 'w' : '-');
    buffer[4] = (prot & VPROT_EXEC) ? 'x' : '-';
    buffer[5] = 0;
    return buffer;
}


/***********************************************************************
 *           VIRTUAL_DumpView
 */
static void VIRTUAL_DumpView( FILE_VIEW *view )
{
    UINT i, count;
    char *addr = view->base;
    BYTE prot = view->prot[0];

    TRACE( "View: %p - %p", addr, addr + view->size - 1 );
    if (view->flags & VFLAG_SYSTEM)
        TRACE( " (system)\n" );
    else if (view->flags & VFLAG_VALLOC)
        TRACE( " (valloc)\n" );
    else if (view->mapping)
        TRACE( " %p\n", view->mapping );
    else
        TRACE( " (anonymous)\n");

    for (count = i = 1; i < view->size >> page_shift; i++, count++)
    {
        if (view->prot[i] == prot) continue;
        TRACE( "      %p - %p %s\n",
                 addr, addr + (count << page_shift) - 1, VIRTUAL_GetProtStr(prot) );
        addr += (count << page_shift);
        prot = view->prot[i];
        count = 0;
    }
    if (count)
        TRACE( "      %p - %p %s\n",
                 addr, addr + (count << page_shift) - 1, VIRTUAL_GetProtStr(prot) );
}


/***********************************************************************
 *           VIRTUAL_Dump
 */
void VIRTUAL_Dump(void)
{
    struct file_view *view;

    TRACE( "Dump of all virtual memory views:\n" );
    RtlEnterCriticalSection(&csVirtual);
    LIST_FOR_EACH_ENTRY( view, &views_list, FILE_VIEW, entry )
    {
        VIRTUAL_DumpView( view );
    }
    RtlLeaveCriticalSection(&csVirtual);
}


/***********************************************************************
 *           VIRTUAL_FindView
 *
 * Find the view containing a given address. The csVirtual section must be held by caller.
 *
 * PARAMS
 *      addr  [I] Address
 *
 * RETURNS
 *	View: Success
 *	NULL: Failure
 */
static struct file_view *VIRTUAL_FindView( const void *addr )
{
    struct file_view *view;

    LIST_FOR_EACH_ENTRY( view, &views_list, struct file_view, entry )
    {
        if (view->base > addr) break;
        if ((const char*)view->base + view->size > (const char*)addr) return view;
    }
    return NULL;
}


/***********************************************************************
 *           find_view_range
 *
 * Find the first view overlapping at least part of the specified range.
 * The csVirtual section must be held by caller.
 */
static struct file_view *find_view_range( const void *addr, size_t size )
{
    struct file_view *view;

    LIST_FOR_EACH_ENTRY( view, &views_list, struct file_view, entry )
    {
        if ((const char *)view->base >= (const char *)addr + size) break;
        if ((const char *)view->base + view->size > (const char *)addr) return view;
    }
    return NULL;
}


/***********************************************************************
 *           add_reserved_area
 *
 * Add a reserved area to the list maintained by libwine.
 * The csVirtual section must be held by caller.
 */
static void add_reserved_area( void *addr, size_t size )
{
    TRACE( "adding %p-%p\n", addr, (char *)addr + size );

    if (addr < user_space_limit)
    {
        /* unmap the part of the area that is below the limit */
        assert( (char *)addr + size > (char *)user_space_limit );
        munmap( addr, (char *)user_space_limit - (char *)addr );
        size -= (char *)user_space_limit - (char *)addr;
        addr = user_space_limit;
    }
    /* blow away existing mappings */
    wine_anon_mmap( addr, size, PROT_NONE, MAP_NORESERVE | MAP_FIXED );
    wine_mmap_add_reserved_area( addr, size );
}


/***********************************************************************
 *           remove_reserved_area
 *
 * Remove a reserved area from the list maintained by libwine.
 * The csVirtual section must be held by caller.
 */
static void remove_reserved_area( void *addr, size_t size )
{
    struct file_view *view;

    LIST_FOR_EACH_ENTRY( view, &views_list, struct file_view, entry )
    {
        if ((char *)view->base >= (char *)addr + size) break;
        if ((char *)view->base + view->size <= (char *)addr) continue;
        /* now we have an overlapping view */
        if (view->base > addr)
        {
            wine_mmap_remove_reserved_area( addr, (char *)view->base - (char *)addr, TRUE );
            size -= (char *)view->base - (char *)addr;
            addr = view->base;
        }
        if ((char *)view->base + view->size >= (char *)addr + size)
        {
            /* view covers all the remaining area */
            wine_mmap_remove_reserved_area( addr, size, FALSE );
            size = 0;
            break;
        }
        else  /* view covers only part of the area */
        {
            wine_mmap_remove_reserved_area( addr, (char *)view->base + view->size - (char *)addr, FALSE );
            size -= (char *)view->base + view->size - (char *)addr;
            addr = (char *)view->base + view->size;
        }
    }
    /* remove remaining space */
    if (size) wine_mmap_remove_reserved_area( addr, size, TRUE );
}


/***********************************************************************
 *           is_beyond_limit
 *
 * Check if an address range goes beyond a given limit.
 */
static inline int is_beyond_limit( void *addr, size_t size, void *limit )
{
    return (limit && (addr >= limit || (char *)addr + size > (char *)limit));
}


/***********************************************************************
 *           unmap_area
 *
 * Unmap an area, or simply replace it by an empty mapping if it is
 * in a reserved area. The csVirtual section must be held by caller.
 */
static inline void unmap_area( void *addr, size_t size )
{
    if (wine_mmap_is_in_reserved_area( addr, size ))
        wine_anon_mmap( addr, size, PROT_NONE, MAP_NORESERVE | MAP_FIXED );
    else
        munmap( addr, size );
}


/***********************************************************************
 *           delete_view
 *
 * Deletes a view. The csVirtual section must be held by caller.
 */
static void delete_view( struct file_view *view ) /* [in] View */
{
    if (!(view->flags & VFLAG_SYSTEM)) unmap_area( view->base, view->size );
    list_remove( &view->entry );
    if (view->mapping) NtClose( view->mapping );
    free( view );
}


/***********************************************************************
 *           create_view
 *
 * Create a view. The csVirtual section must be held by caller.
 */
static NTSTATUS create_view( struct file_view **view_ret, void *base, size_t size, BYTE vprot )
{
    struct file_view *view;
    struct list *ptr;

    assert( !((UINT_PTR)base & page_mask) );
    assert( !(size & page_mask) );

    /* Create the view structure */

    if (!(view = malloc( sizeof(*view) + (size >> page_shift) - 1 ))) return STATUS_NO_MEMORY;

    view->base    = base;
    view->size    = size;
    view->flags   = 0;
    view->mapping = 0;
    view->protect = vprot;
    memset( view->prot, vprot, size >> page_shift );

    /* Insert it in the linked list */

    LIST_FOR_EACH( ptr, &views_list )
    {
        struct file_view *next = LIST_ENTRY( ptr, struct file_view, entry );
        if (next->base > base) break;
    }
    list_add_before( ptr, &view->entry );

    /* Check for overlapping views. This can happen if the previous view
     * was a system view that got unmapped behind our back. In that case
     * we recover by simply deleting it. */

    if ((ptr = list_prev( &views_list, &view->entry )) != NULL)
    {
        struct file_view *prev = LIST_ENTRY( ptr, struct file_view, entry );
        if ((char *)prev->base + prev->size > (char *)base)
        {
            TRACE( "overlapping prev view %p-%p for %p-%p\n",
                   prev->base, (char *)prev->base + prev->size,
                   base, (char *)base + view->size );
            assert( prev->flags & VFLAG_SYSTEM );
            delete_view( prev );
        }
    }
    if ((ptr = list_next( &views_list, &view->entry )) != NULL)
    {
        struct file_view *next = LIST_ENTRY( ptr, struct file_view, entry );
        if ((char *)base + view->size > (char *)next->base)
        {
            TRACE( "overlapping next view %p-%p for %p-%p\n",
                   next->base, (char *)next->base + next->size,
                   base, (char *)base + view->size );
            assert( next->flags & VFLAG_SYSTEM );
            delete_view( next );
        }
    }

    *view_ret = view;
    VIRTUAL_DEBUG_DUMP_VIEW( view );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           VIRTUAL_GetUnixProt
 *
 * Convert page protections to protection for mmap/mprotect.
 */
static int VIRTUAL_GetUnixProt( BYTE vprot )
{
    int prot = 0;
    if ((vprot & VPROT_COMMITTED) && !(vprot & VPROT_GUARD))
    {
        if (vprot & VPROT_READ) prot |= PROT_READ;
        if (vprot & VPROT_WRITE) prot |= PROT_WRITE;
        if (vprot & VPROT_WRITECOPY) prot |= PROT_WRITE;
        if (vprot & VPROT_EXEC) prot |= PROT_EXEC;
    }
    return prot;
}


/***********************************************************************
 *           VIRTUAL_GetWin32Prot
 *
 * Convert page protections to Win32 flags.
 *
 * RETURNS
 *	None
 */
static void VIRTUAL_GetWin32Prot(
            BYTE vprot,     /* [in] Page protection flags */
            DWORD *protect, /* [out] Location to store Win32 protection flags */
            DWORD *state )  /* [out] Location to store mem state flag */
{
    if (protect) {
        *protect = VIRTUAL_Win32Flags[vprot & 0x0f];
        if (vprot & VPROT_NOCACHE) *protect |= PAGE_NOCACHE;
        if (vprot & VPROT_GUARD) *protect = PAGE_NOACCESS | PAGE_GUARD;
    }

    if (state) *state = (vprot & VPROT_COMMITTED) ? MEM_COMMIT : MEM_RESERVE;
}


/***********************************************************************
 *           VIRTUAL_GetProt
 *
 * Build page protections from Win32 flags.
 *
 * PARAMS
 *      protect [I] Win32 protection flags
 *
 * RETURNS
 *	Value of page protection flags
 */
static BYTE VIRTUAL_GetProt( DWORD protect )
{
    BYTE vprot;

    switch(protect & 0xff)
    {
    case PAGE_READONLY:
        vprot = VPROT_READ;
        break;
    case PAGE_READWRITE:
        vprot = VPROT_READ | VPROT_WRITE;
        break;
    case PAGE_WRITECOPY:
        /* MSDN CreateFileMapping() states that if PAGE_WRITECOPY is given,
	 * that the hFile must have been opened with GENERIC_READ and
	 * GENERIC_WRITE access.  This is WRONG as tests show that you
	 * only need GENERIC_READ access (at least for Win9x,
	 * FIXME: what about NT?).  Thus, we don't put VPROT_WRITE in
	 * PAGE_WRITECOPY and PAGE_EXECUTE_WRITECOPY.
	 */
        vprot = VPROT_READ | VPROT_WRITECOPY;
        break;
    case PAGE_EXECUTE:
        vprot = VPROT_EXEC;
        break;
    case PAGE_EXECUTE_READ:
        vprot = VPROT_EXEC | VPROT_READ;
        break;
    case PAGE_EXECUTE_READWRITE:
        vprot = VPROT_EXEC | VPROT_READ | VPROT_WRITE;
        break;
    case PAGE_EXECUTE_WRITECOPY:
        /* See comment for PAGE_WRITECOPY above */
        vprot = VPROT_EXEC | VPROT_READ | VPROT_WRITECOPY;
        break;
    case PAGE_NOACCESS:
    default:
        vprot = 0;
        break;
    }
    if (protect & PAGE_GUARD) vprot |= VPROT_GUARD;
    if (protect & PAGE_NOCACHE) vprot |= VPROT_NOCACHE;
    return vprot;
}


/***********************************************************************
 *           VIRTUAL_SetProt
 *
 * Change the protection of a range of pages.
 *
 * RETURNS
 *	TRUE: Success
 *	FALSE: Failure
 */
static BOOL VIRTUAL_SetProt( FILE_VIEW *view, /* [in] Pointer to view */
                             void *base,      /* [in] Starting address */
                             size_t size,     /* [in] Size in bytes */
                             BYTE vprot )     /* [in] Protections to use */
{
    TRACE("%p-%p %s\n",
          base, (char *)base + size - 1, VIRTUAL_GetProtStr( vprot ) );

    if (mprotect( base, size, VIRTUAL_GetUnixProt(vprot) ))
        return FALSE;  /* FIXME: last error */

    memset( view->prot + (((char *)base - (char *)view->base) >> page_shift),
            vprot, size >> page_shift );
    VIRTUAL_DEBUG_DUMP_VIEW( view );
    return TRUE;
}


/***********************************************************************
 *           unmap_extra_space
 *
 * Release the extra memory while keeping the range starting on the granularity boundary.
 */
static inline void *unmap_extra_space( void *ptr, size_t total_size, size_t wanted_size, size_t mask )
{
    if ((ULONG_PTR)ptr & mask)
    {
        size_t extra = mask + 1 - ((ULONG_PTR)ptr & mask);
        munmap( ptr, extra );
        ptr = (char *)ptr + extra;
        total_size -= extra;
    }
    if (total_size > wanted_size)
        munmap( (char *)ptr + wanted_size, total_size - wanted_size );
    return ptr;
}


/***********************************************************************
 *           map_view
 *
 * Create a view and mmap the corresponding memory area.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS map_view( struct file_view **view_ret, void *base, size_t size, BYTE vprot )
{
    void *ptr;
    NTSTATUS status;

    if (base)
    {
        if (is_beyond_limit( base, size, ADDRESS_SPACE_LIMIT ))
            return STATUS_WORKING_SET_LIMIT_RANGE;

        switch (wine_mmap_is_in_reserved_area( base, size ))
        {
        case -1: /* partially in a reserved area */
            return STATUS_CONFLICTING_ADDRESSES;

        case 0:  /* not in a reserved area, do a normal allocation */
            if ((ptr = wine_anon_mmap( base, size, VIRTUAL_GetUnixProt(vprot), 0 )) == (void *)-1)
            {
                if (errno == ENOMEM) return STATUS_NO_MEMORY;
                return STATUS_INVALID_PARAMETER;
            }
            if (ptr != base)
            {
                /* We couldn't get the address we wanted */
                if (is_beyond_limit( ptr, size, user_space_limit )) add_reserved_area( ptr, size );
                else munmap( ptr, size );
                return STATUS_CONFLICTING_ADDRESSES;
            }
            break;

        default:
        case 1:  /* in a reserved area, make sure the address is available */
            if (find_view_range( base, size )) return STATUS_CONFLICTING_ADDRESSES;
            /* replace the reserved area by our mapping */
            if ((ptr = wine_anon_mmap( base, size, VIRTUAL_GetUnixProt(vprot), MAP_FIXED )) != base)
                return STATUS_INVALID_PARAMETER;
            break;
        }
    }
    else
    {
        size_t view_size = size + granularity_mask + 1;

        for (;;)
        {
            if ((ptr = wine_anon_mmap( NULL, view_size, VIRTUAL_GetUnixProt(vprot), 0 )) == (void *)-1)
            {
                if (errno == ENOMEM) return STATUS_NO_MEMORY;
                return STATUS_INVALID_PARAMETER;
            }
            /* if we got something beyond the user limit, unmap it and retry */
            if (is_beyond_limit( ptr, view_size, user_space_limit )) add_reserved_area( ptr, view_size );
            else break;
        }
        ptr = unmap_extra_space( ptr, view_size, size, granularity_mask );
    }

    status = create_view( view_ret, ptr, size, vprot );
    if (status != STATUS_SUCCESS) unmap_area( ptr, size );
    return status;
}


/***********************************************************************
 *           unaligned_mmap
 *
 * Linux kernels before 2.4.x can support non page-aligned offsets, as
 * long as the offset is aligned to the filesystem block size. This is
 * a big performance gain so we want to take advantage of it.
 *
 * However, when we use 64-bit file support this doesn't work because
 * glibc rejects unaligned offsets. Also glibc 2.1.3 mmap64 is broken
 * in that it rounds unaligned offsets down to a page boundary. For
 * these reasons we do a direct system call here.
 */
static void *unaligned_mmap( void *addr, size_t length, unsigned int prot,
                             unsigned int flags, int fd, off_t offset )
{
#if defined(linux) && defined(__i386__) && defined(__GNUC__)
    if (!(offset >> 32) && (offset & page_mask))
    {
        int ret;

        struct
        {
            void        *addr;
            unsigned int length;
            unsigned int prot;
            unsigned int flags;
            unsigned int fd;
            unsigned int offset;
        } args;

        args.addr   = addr;
        args.length = length;
        args.prot   = prot;
        args.flags  = flags;
        args.fd     = fd;
        args.offset = offset;

        __asm__ __volatile__("push %%ebx\n\t"
                             "movl %2,%%ebx\n\t"
                             "int $0x80\n\t"
                             "popl %%ebx"
                             : "=a" (ret)
                             : "0" (90), /* SYS_mmap */
                               "q" (&args)
                             : "memory" );
        if (ret < 0 && ret > -4096)
        {
            errno = -ret;
            ret = -1;
        }
        return (void *)ret;
    }
#endif
    return mmap( addr, length, prot, flags, fd, offset );
}


/***********************************************************************
 *           map_file_into_view
 *
 * Wrapper for mmap() to map a file into a view, falling back to read if mmap fails.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS map_file_into_view( struct file_view *view, int fd, size_t start, size_t size,
                                    off_t offset, BYTE vprot, BOOL removable )
{
    void *ptr;
    int prot = VIRTUAL_GetUnixProt( vprot );
    BOOL shared_write = (vprot & VPROT_WRITE) != 0;

    assert( start < view->size );
    assert( start + size <= view->size );

    /* only try mmap if media is not removable (or if we require write access) */
    if (!removable || shared_write)
    {
        int flags = MAP_FIXED | (shared_write ? MAP_SHARED : MAP_PRIVATE);

        if (unaligned_mmap( (char *)view->base + start, size, prot, flags, fd, offset ) != (void *)-1)
            goto done;

        /* mmap() failed; if this is because the file offset is not    */
        /* page-aligned (EINVAL), or because the underlying filesystem */
        /* does not support mmap() (ENOEXEC,ENODEV), we do it by hand. */
        if ((errno != ENOEXEC) && (errno != EINVAL) && (errno != ENODEV)) return FILE_GetNtStatus();
        if (shared_write) return FILE_GetNtStatus();  /* we cannot fake shared write mappings */
    }

    /* Reserve the memory with an anonymous mmap */
    ptr = wine_anon_mmap( (char *)view->base + start, size, PROT_READ | PROT_WRITE, MAP_FIXED );
    if (ptr == (void *)-1) return FILE_GetNtStatus();
    /* Now read in the file */
    pread( fd, ptr, size, offset );
    if (prot != (PROT_READ|PROT_WRITE)) mprotect( ptr, size, prot );  /* Set the right protection */
done:
    memset( view->prot + (start >> page_shift), vprot, size >> page_shift );
    return STATUS_SUCCESS;
}


/***********************************************************************
 *           decommit_view
 *
 * Decommit some pages of a given view.
 * The csVirtual section must be held by caller.
 */
static NTSTATUS decommit_pages( struct file_view *view, size_t start, size_t size )
{
    if (wine_anon_mmap( (char *)view->base + start, size, PROT_NONE, MAP_FIXED ) != (void *)-1)
    {
        BYTE *p = view->prot + (start >> page_shift);
        size >>= page_shift;
        while (size--) *p++ &= ~VPROT_COMMITTED;
        return STATUS_SUCCESS;
    }
    return FILE_GetNtStatus();
}


/***********************************************************************
 *           do_relocations
 *
 * Apply the relocations to a mapped PE image
 */
static int do_relocations( char *base, const IMAGE_DATA_DIRECTORY *dir,
                           int delta, SIZE_T total_size )
{
    IMAGE_BASE_RELOCATION *rel;

    TRACE_(module)( "relocating from %p-%p to %p-%p\n",
                    base - delta, base - delta + total_size, base, base + total_size );

    for (rel = (IMAGE_BASE_RELOCATION *)(base + dir->VirtualAddress);
         ((char *)rel < base + dir->VirtualAddress + dir->Size) && rel->SizeOfBlock;
         rel = (IMAGE_BASE_RELOCATION*)((char*)rel + rel->SizeOfBlock) )
    {
        char *page = base + rel->VirtualAddress;
        WORD *TypeOffset = (WORD *)(rel + 1);
        int i, count = (rel->SizeOfBlock - sizeof(*rel)) / sizeof(*TypeOffset);

        if (!count) continue;

        /* sanity checks */
        if ((char *)rel + rel->SizeOfBlock > base + dir->VirtualAddress + dir->Size)
        {
            ERR_(module)("invalid relocation %p,%lx,%ld at %p,%lx,%lx\n",
                         rel, rel->VirtualAddress, rel->SizeOfBlock,
                         base, dir->VirtualAddress, dir->Size );
            return 0;
        }

        if (page > base + total_size)
        {
            WARN_(module)("skipping %d relocations for page %p beyond module %p-%p\n",
                          count, page, base, base + total_size );
            continue;
        }

        TRACE_(module)("%d relocations for page %lx\n", count, rel->VirtualAddress);

        /* patching in reverse order */
        for (i = 0 ; i < count; i++)
        {
            int offset = TypeOffset[i] & 0xFFF;
            int type = TypeOffset[i] >> 12;
            switch(type)
            {
            case IMAGE_REL_BASED_ABSOLUTE:
                break;
            case IMAGE_REL_BASED_HIGH:
                *(short*)(page+offset) += HIWORD(delta);
                break;
            case IMAGE_REL_BASED_LOW:
                *(short*)(page+offset) += LOWORD(delta);
                break;
            case IMAGE_REL_BASED_HIGHLOW:
                *(int*)(page+offset) += delta;
                /* FIXME: if this is an exported address, fire up enhanced logic */
                break;
            default:
                FIXME_(module)("Unknown/unsupported fixup type %d.\n", type);
                break;
            }
        }
    }
    return 1;
}


/***********************************************************************
 *           map_image
 *
 * Map an executable (PE format) image into memory.
 */
static NTSTATUS map_image( HANDLE hmapping, int fd, char *base, SIZE_T total_size,
                           SIZE_T header_size, int shared_fd, BOOL removable, PVOID *addr_ptr )
{
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS *nt;
    IMAGE_SECTION_HEADER *sec;
    IMAGE_DATA_DIRECTORY *imports;
    NTSTATUS status = STATUS_CONFLICTING_ADDRESSES;
    int i;
    off_t pos;
    struct file_view *view = NULL;
    char *ptr;

    /* zero-map the whole range */

    RtlEnterCriticalSection( &csVirtual );

    if (base >= (char *)0x110000)  /* make sure the DOS area remains free */
        status = map_view( &view, base, total_size,
                           VPROT_COMMITTED | VPROT_READ | VPROT_EXEC | VPROT_WRITECOPY | VPROT_IMAGE );

    if (status == STATUS_CONFLICTING_ADDRESSES)
        status = map_view( &view, NULL, total_size,
                           VPROT_COMMITTED | VPROT_READ | VPROT_EXEC | VPROT_WRITECOPY | VPROT_IMAGE );

    if (status != STATUS_SUCCESS) goto error;

    ptr = view->base;
    TRACE_(module)( "mapped PE file at %p-%p\n", ptr, ptr + total_size );

    /* map the header */

    status = STATUS_INVALID_IMAGE_FORMAT;  /* generic error */
    if (map_file_into_view( view, fd, 0, header_size, 0, VPROT_COMMITTED | VPROT_READ,
                            removable ) != STATUS_SUCCESS) goto error;
    dos = (IMAGE_DOS_HEADER *)ptr;
    nt = (IMAGE_NT_HEADERS *)(ptr + dos->e_lfanew);
    if ((char *)(nt + 1) > ptr + header_size) goto error;

    sec = (IMAGE_SECTION_HEADER*)((char*)&nt->OptionalHeader+nt->FileHeader.SizeOfOptionalHeader);
    if ((char *)(sec + nt->FileHeader.NumberOfSections) > ptr + header_size) goto error;

    imports = nt->OptionalHeader.DataDirectory + IMAGE_DIRECTORY_ENTRY_IMPORT;
    if (!imports->Size || !imports->VirtualAddress) imports = NULL;

    /* check the architecture */

    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
    {
        MESSAGE("Trying to load PE image for unsupported architecture (");
        switch (nt->FileHeader.Machine)
        {
        case IMAGE_FILE_MACHINE_UNKNOWN: MESSAGE("Unknown"); break;
        case IMAGE_FILE_MACHINE_I860:    MESSAGE("I860"); break;
        case IMAGE_FILE_MACHINE_R3000:   MESSAGE("R3000"); break;
        case IMAGE_FILE_MACHINE_R4000:   MESSAGE("R4000"); break;
        case IMAGE_FILE_MACHINE_R10000:  MESSAGE("R10000"); break;
        case IMAGE_FILE_MACHINE_ALPHA:   MESSAGE("Alpha"); break;
        case IMAGE_FILE_MACHINE_POWERPC: MESSAGE("PowerPC"); break;
        case IMAGE_FILE_MACHINE_IA64:    MESSAGE("IA-64"); break;
        case IMAGE_FILE_MACHINE_ALPHA64: MESSAGE("Alpha-64"); break;
        case IMAGE_FILE_MACHINE_AMD64:   MESSAGE("AMD-64"); break;
        default: MESSAGE("Unknown-%04x", nt->FileHeader.Machine); break;
        }
        MESSAGE(")\n");
        goto error;
    }

    /* check for non page-aligned binary */

    if (nt->OptionalHeader.SectionAlignment <= page_mask)
    {
        /* unaligned sections, this happens for native subsystem binaries */
        /* in that case Windows simply maps in the whole file */

        if (map_file_into_view( view, fd, 0, total_size, 0, VPROT_COMMITTED | VPROT_READ,
                                removable ) != STATUS_SUCCESS) goto error;

        /* check that all sections are loaded at the right offset */
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
        {
            if (sec[i].VirtualAddress != sec[i].PointerToRawData)
                goto error;  /* Windows refuses to load in that case too */
        }

        /* set the image protections */
        VIRTUAL_SetProt( view, ptr, total_size,
                         VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY | VPROT_EXEC );

        /* perform relocations if necessary */
        /* FIXME: not 100% compatible, Windows doesn't do this for non page-aligned binaries */
        if (ptr != base)
        {
            const IMAGE_DATA_DIRECTORY *relocs;
            relocs = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
            if (relocs->VirtualAddress && relocs->Size)
                do_relocations( ptr, relocs, ptr - base, total_size );
        }

        goto done;
    }


    /* map all the sections */

    for (i = pos = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        SIZE_T map_size, file_size, end;

        if (!sec->Misc.VirtualSize)
        {
            file_size = sec->SizeOfRawData;
            map_size  = ROUND_SIZE( 0, file_size );
        }
        else
        {
            map_size = ROUND_SIZE( 0, sec->Misc.VirtualSize );
            file_size = min( sec->SizeOfRawData, map_size );
        }

        /* a few sanity checks */
        end = sec->VirtualAddress + ROUND_SIZE( sec->VirtualAddress, map_size );
        if (sec->VirtualAddress > total_size || end > total_size || end < sec->VirtualAddress)
        {
            ERR_(module)( "Section %.8s too large (%lx+%lx/%lx)\n",
                          sec->Name, sec->VirtualAddress, map_size, total_size );
            goto error;
        }

        if ((sec->Characteristics & IMAGE_SCN_MEM_SHARED) &&
            (sec->Characteristics & IMAGE_SCN_MEM_WRITE))
        {
            TRACE_(module)( "mapping shared section %.8s at %p off %lx (%x) size %lx (%lx) flags %lx\n",
                            sec->Name, ptr + sec->VirtualAddress,
                            sec->PointerToRawData, (int)pos, file_size, map_size,
                            sec->Characteristics );
            if (map_file_into_view( view, shared_fd, sec->VirtualAddress, map_size, pos,
                                    VPROT_COMMITTED | VPROT_READ | PROT_WRITE,
                                    FALSE ) != STATUS_SUCCESS)
            {
                ERR_(module)( "Could not map shared section %.8s\n", sec->Name );
                goto error;
            }

            /* check if the import directory falls inside this section */
            if (imports && imports->VirtualAddress >= sec->VirtualAddress &&
                imports->VirtualAddress < sec->VirtualAddress + map_size)
            {
                UINT_PTR base = imports->VirtualAddress & ~page_mask;
                UINT_PTR end = base + ROUND_SIZE( imports->VirtualAddress, imports->Size );
                if (end > sec->VirtualAddress + map_size) end = sec->VirtualAddress + map_size;
                if (end > base)
                    map_file_into_view( view, shared_fd, base, end - base,
                                        pos + (base - sec->VirtualAddress),
                                        VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY,
                                        FALSE );
            }
            pos += map_size;
            continue;
        }

        TRACE_(module)( "mapping section %.8s at %p off %lx size %lx virt %lx flags %lx\n",
                        sec->Name, ptr + sec->VirtualAddress,
                        sec->PointerToRawData, sec->SizeOfRawData,
                        sec->Misc.VirtualSize, sec->Characteristics );

        if (!sec->PointerToRawData || !file_size) continue;

        /* Note: if the section is not aligned properly map_file_into_view will magically
         *       fall back to read(), so we don't need to check anything here.
         */
        if (map_file_into_view( view, fd, sec->VirtualAddress, file_size, sec->PointerToRawData,
                                VPROT_COMMITTED | VPROT_READ | VPROT_WRITECOPY,
                                removable ) != STATUS_SUCCESS)
        {
            ERR_(module)( "Could not map section %.8s, file probably truncated\n", sec->Name );
            goto error;
        }

        if (file_size & page_mask)
        {
            end = ROUND_SIZE( 0, file_size );
            if (end > map_size) end = map_size;
            TRACE_(module)("clearing %p - %p\n",
                           ptr + sec->VirtualAddress + file_size,
                           ptr + sec->VirtualAddress + end );
            memset( ptr + sec->VirtualAddress + file_size, 0, end - file_size );
        }
    }


    /* perform base relocation, if necessary */

    if (ptr != base)
    {
        const IMAGE_DATA_DIRECTORY *relocs;

        relocs = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        if (!relocs->VirtualAddress || !relocs->Size)
        {
            if (nt->OptionalHeader.ImageBase == 0x400000) {
                ERR("Image was mapped at %p: standard load address for a Win32 program (0x00400000) not available\n", ptr);
                ERR("Do you have exec-shield or prelink active?\n");
            } else
                ERR( "FATAL: Need to relocate module from addr %lx, but there are no relocation records\n",
                     nt->OptionalHeader.ImageBase );
            goto error;
        }

        /* FIXME: If we need to relocate a system DLL (base > 2GB) we should
         *        really make sure that the *new* base address is also > 2GB.
         *        Some DLLs really check the MSB of the module handle :-/
         */
        if ((nt->OptionalHeader.ImageBase & 0x80000000) && !((ULONG_PTR)base & 0x80000000))
            ERR( "Forced to relocate system DLL (base > 2GB). This is not good.\n" );

        if (!do_relocations( ptr, relocs, ptr - base, total_size ))
        {
            goto error;
        }
    }

    /* set the image protections */

    sec = (IMAGE_SECTION_HEADER*)((char *)&nt->OptionalHeader+nt->FileHeader.SizeOfOptionalHeader);
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, sec++)
    {
        SIZE_T size = ROUND_SIZE( sec->VirtualAddress, sec->Misc.VirtualSize );
        BYTE vprot = VPROT_COMMITTED;
        if (sec->Characteristics & IMAGE_SCN_MEM_READ)    vprot |= VPROT_READ;
        if (sec->Characteristics & IMAGE_SCN_MEM_WRITE)   vprot |= VPROT_READ|VPROT_WRITECOPY;
        if (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) vprot |= VPROT_EXEC;
        VIRTUAL_SetProt( view, ptr + sec->VirtualAddress, size, vprot );
    }

 done:
    if (!removable)  /* don't keep handle open on removable media */
        NtDuplicateObject( NtCurrentProcess(), hmapping,
                           NtCurrentProcess(), &view->mapping,
                           0, 0, DUPLICATE_SAME_ACCESS );

    RtlLeaveCriticalSection( &csVirtual );

    *addr_ptr = ptr;
    return STATUS_SUCCESS;

 error:
    if (view) delete_view( view );
    RtlLeaveCriticalSection( &csVirtual );
    return status;
}


/***********************************************************************
 *           is_current_process
 *
 * Check whether a process handle is for the current process.
 */
BOOL is_current_process( HANDLE handle )
{
    BOOL ret = FALSE;

    if (handle == NtCurrentProcess()) return TRUE;
    SERVER_START_REQ( get_process_info )
    {
        req->handle = handle;
        if (!wine_server_call( req ))
            ret = ((DWORD)reply->pid == GetCurrentProcessId());
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *           virtual_init
 */
static inline void virtual_init(void)
{
#ifndef page_mask
    page_size = getpagesize();
    page_mask = page_size - 1;
    /* Make sure we have a power of 2 */
    assert( !(page_size & page_mask) );
    page_shift = 0;
    while ((1 << page_shift) != page_size) page_shift++;
#endif  /* page_mask */
}


/***********************************************************************
 *           VIRTUAL_alloc_teb
 *
 * Allocate a memory view for a new TEB, properly aligned to a multiple of the size.
 */
NTSTATUS VIRTUAL_alloc_teb( void **ret, size_t size, BOOL first )
{
    void *ptr;
    NTSTATUS status;
    struct file_view *view;
    size_t align_size;
    BYTE vprot = VPROT_READ | VPROT_WRITE | VPROT_COMMITTED;

    if (first) virtual_init();

    *ret = NULL;
    size = ROUND_SIZE( 0, size );
    align_size = page_size;
    while (align_size < size) align_size *= 2;

    for (;;)
    {
        if ((ptr = wine_anon_mmap( NULL, 2 * align_size, VIRTUAL_GetUnixProt(vprot), 0 )) == (void *)-1)
        {
            if (errno == ENOMEM) return STATUS_NO_MEMORY;
            return STATUS_INVALID_PARAMETER;
        }
        if (!is_beyond_limit( ptr, 2 * align_size, user_space_limit ))
        {
            ptr = unmap_extra_space( ptr, 2 * align_size, align_size, align_size - 1 );
            break;
        }
        /* if we got something beyond the user limit, unmap it and retry */
        add_reserved_area( ptr, 2 * align_size );
    }

    if (!first) RtlEnterCriticalSection( &csVirtual );

    status = create_view( &view, ptr, size, vprot );
    if (status == STATUS_SUCCESS)
    {
        view->flags |= VFLAG_VALLOC;
        *ret = ptr;
    }
    else unmap_area( ptr, size );

    if (!first) RtlLeaveCriticalSection( &csVirtual );

    return status;
}


/***********************************************************************
 *           VIRTUAL_HandleFault
 */
NTSTATUS VIRTUAL_HandleFault( LPCVOID addr )
{
    FILE_VIEW *view;
    NTSTATUS ret = STATUS_ACCESS_VIOLATION;

    RtlEnterCriticalSection( &csVirtual );
    if ((view = VIRTUAL_FindView( addr )))
    {
        BYTE vprot = view->prot[((const char *)addr - (const char *)view->base) >> page_shift];
        void *page = (void *)((UINT_PTR)addr & ~page_mask);
        char *stack = NtCurrentTeb()->Tib.StackLimit;
        if (vprot & VPROT_GUARD)
        {
            VIRTUAL_SetProt( view, page, page_mask + 1, vprot & ~VPROT_GUARD );
            ret = STATUS_GUARD_PAGE_VIOLATION;
        }
        /* is it inside the stack guard page? */
        if (((const char *)addr >= stack) && ((const char *)addr < stack + (page_mask+1)))
            ret = STATUS_STACK_OVERFLOW;
    }
    RtlLeaveCriticalSection( &csVirtual );
    return ret;
}

/***********************************************************************
 *           VIRTUAL_HasMapping
 *
 * Check if the specified view has an associated file mapping.
 */
BOOL VIRTUAL_HasMapping( LPCVOID addr )
{
    FILE_VIEW *view;
    BOOL ret = FALSE;

    RtlEnterCriticalSection( &csVirtual );
    if ((view = VIRTUAL_FindView( addr ))) ret = (view->mapping != 0);
    RtlLeaveCriticalSection( &csVirtual );
    return ret;
}


/***********************************************************************
 *           VIRTUAL_UseLargeAddressSpace
 *
 * Increase the address space size for apps that support it.
 */
void VIRTUAL_UseLargeAddressSpace(void)
{
    if (user_space_limit >= ADDRESS_SPACE_LIMIT) return;
    RtlEnterCriticalSection( &csVirtual );
    remove_reserved_area( user_space_limit, (char *)ADDRESS_SPACE_LIMIT - (char *)user_space_limit );
    user_space_limit = ADDRESS_SPACE_LIMIT;
    RtlLeaveCriticalSection( &csVirtual );
}


/***********************************************************************
 *             NtAllocateVirtualMemory   (NTDLL.@)
 *             ZwAllocateVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtAllocateVirtualMemory( HANDLE process, PVOID *ret, ULONG zero_bits,
                                         SIZE_T *size_ptr, ULONG type, ULONG protect )
{
    void *base;
    BYTE vprot;
    SIZE_T size = *size_ptr;
    NTSTATUS status = STATUS_SUCCESS;
    struct file_view *view;

    TRACE("%p %p %08lx %lx %08lx\n", process, *ret, size, type, protect );

    if (!size) return STATUS_INVALID_PARAMETER;

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Round parameters to a page boundary */

    if (size > 0x7fc00000) return STATUS_WORKING_SET_LIMIT_RANGE; /* 2Gb - 4Mb */

    if (*ret)
    {
        if (type & MEM_RESERVE) /* Round down to 64k boundary */
            base = ROUND_ADDR( *ret, granularity_mask );
        else
            base = ROUND_ADDR( *ret, page_mask );
        size = (((UINT_PTR)*ret + size + page_mask) & ~page_mask) - (UINT_PTR)base;

        /* disallow low 64k, wrap-around and kernel space */
        if (((char *)base <= (char *)granularity_mask) ||
            ((char *)base + size < (char *)base) ||
            is_beyond_limit( base, size, ADDRESS_SPACE_LIMIT ))
            return STATUS_INVALID_PARAMETER;
    }
    else
    {
        base = NULL;
        size = (size + page_mask) & ~page_mask;
    }

    if (type & MEM_TOP_DOWN) {
        /* FIXME: MEM_TOP_DOWN allocates the largest possible address. */
        WARN("MEM_TOP_DOWN ignored\n");
        type &= ~MEM_TOP_DOWN;
    }

    if (zero_bits)
        WARN("zero_bits %lu ignored\n", zero_bits);

    /* Compute the alloc type flags */

    if (!(type & MEM_SYSTEM))
    {
        if (!(type & (MEM_COMMIT | MEM_RESERVE)) || (type & ~(MEM_COMMIT | MEM_RESERVE)))
        {
            WARN("called with wrong alloc type flags (%08lx) !\n", type);
            return STATUS_INVALID_PARAMETER;
        }
    }
    vprot = VIRTUAL_GetProt( protect );
    if (type & MEM_COMMIT) vprot |= VPROT_COMMITTED;

    /* Reserve the memory */

    RtlEnterCriticalSection( &csVirtual );

    if (type & MEM_SYSTEM)
    {
        if (type & MEM_IMAGE) vprot |= VPROT_IMAGE;
        status = create_view( &view, base, size, vprot | VPROT_COMMITTED );
        if (status == STATUS_SUCCESS)
        {
            view->flags |= VFLAG_VALLOC | VFLAG_SYSTEM;
            base = view->base;
        }
    }
    else if ((type & MEM_RESERVE) || !base)
    {
        status = map_view( &view, base, size, vprot );
        if (status == STATUS_SUCCESS)
        {
            view->flags |= VFLAG_VALLOC;
            base = view->base;
        }
    }
    else  /* commit the pages */
    {
        if (!(view = VIRTUAL_FindView( base )) ||
            ((char *)base + size > (char *)view->base + view->size)) status = STATUS_NOT_MAPPED_VIEW;
        else if (!VIRTUAL_SetProt( view, base, size, vprot )) status = STATUS_ACCESS_DENIED;
    }

    RtlLeaveCriticalSection( &csVirtual );

    if (status == STATUS_SUCCESS)
    {
        *ret = base;
        *size_ptr = size;
    }
    return status;
}


/***********************************************************************
 *             NtFreeVirtualMemory   (NTDLL.@)
 *             ZwFreeVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtFreeVirtualMemory( HANDLE process, PVOID *addr_ptr, SIZE_T *size_ptr, ULONG type )
{
    FILE_VIEW *view;
    char *base;
    NTSTATUS status = STATUS_SUCCESS;
    LPVOID addr = *addr_ptr;
    SIZE_T size = *size_ptr;

    TRACE("%p %p %08lx %lx\n", process, addr, size, type );

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Fix the parameters */

    size = ROUND_SIZE( addr, size );
    base = ROUND_ADDR( addr, page_mask );

    RtlEnterCriticalSection(&csVirtual);

    if (!(view = VIRTUAL_FindView( base )) ||
        (base + size > (char *)view->base + view->size) ||
        !(view->flags & VFLAG_VALLOC))
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else if (type & MEM_SYSTEM)
    {
        /* return the values that the caller should use to unmap the area */
        *addr_ptr = view->base;
        *size_ptr = view->size;
        view->flags |= VFLAG_SYSTEM;
        delete_view( view );
    }
    else if (type == MEM_RELEASE)
    {
        /* Free the pages */

        if (size || (base != view->base)) status = STATUS_INVALID_PARAMETER;
        else
        {
            delete_view( view );
            *addr_ptr = base;
            *size_ptr = size;
        }
    }
    else if (type == MEM_DECOMMIT)
    {
        status = decommit_pages( view, base - (char *)view->base, size );
        if (status == STATUS_SUCCESS)
        {
            *addr_ptr = base;
            *size_ptr = size;
        }
    }
    else
    {
        WARN("called with wrong free type flags (%08lx) !\n", type);
        status = STATUS_INVALID_PARAMETER;
    }

    RtlLeaveCriticalSection(&csVirtual);
    return status;
}


/***********************************************************************
 *             NtProtectVirtualMemory   (NTDLL.@)
 *             ZwProtectVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtProtectVirtualMemory( HANDLE process, PVOID *addr_ptr, SIZE_T *size_ptr,
                                        ULONG new_prot, ULONG *old_prot )
{
    FILE_VIEW *view;
    NTSTATUS status = STATUS_SUCCESS;
    char *base;
    UINT i;
    BYTE vprot, *p;
    ULONG prot;
    SIZE_T size = *size_ptr;
    LPVOID addr = *addr_ptr;

    TRACE("%p %p %08lx %08lx\n", process, addr, size, new_prot );

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Fix the parameters */

    size = ROUND_SIZE( addr, size );
    base = ROUND_ADDR( addr, page_mask );

    RtlEnterCriticalSection( &csVirtual );

    if (!(view = VIRTUAL_FindView( base )) || (base + size > (char *)view->base + view->size))
    {
        status = STATUS_INVALID_PARAMETER;
    }
    else
    {
        /* Make sure all the pages are committed */

        p = view->prot + ((base - (char *)view->base) >> page_shift);
        VIRTUAL_GetWin32Prot( *p, &prot, NULL );
        for (i = size >> page_shift; i; i--, p++)
        {
            if (!(*p & VPROT_COMMITTED))
            {
                status = STATUS_NOT_COMMITTED;
                break;
            }
        }
        if (!i)
        {
            if (old_prot) *old_prot = prot;
            vprot = VIRTUAL_GetProt( new_prot ) | VPROT_COMMITTED;
            if (!VIRTUAL_SetProt( view, base, size, vprot )) status = STATUS_ACCESS_DENIED;
        }
    }
    RtlLeaveCriticalSection( &csVirtual );

    if (status == STATUS_SUCCESS)
    {
        *addr_ptr = base;
        *size_ptr = size;
    }
    return status;
}

#define UNIMPLEMENTED_INFO_CLASS(c) \
    case c: \
        FIXME("(process=%p,addr=%p) Unimplemented information class: " #c "\n", process, addr); \
        return STATUS_INVALID_INFO_CLASS

/***********************************************************************
 *             NtQueryVirtualMemory   (NTDLL.@)
 *             ZwQueryVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtQueryVirtualMemory( HANDLE process, LPCVOID addr,
                                      MEMORY_INFORMATION_CLASS info_class, PVOID buffer,
                                      SIZE_T len, SIZE_T *res_len )
{
    FILE_VIEW *view;
    char *base, *alloc_base = 0;
    struct list *ptr;
    SIZE_T size = 0;
    MEMORY_BASIC_INFORMATION *info = buffer;

    if (info_class != MemoryBasicInformation)
    {
        switch(info_class)
        {
            UNIMPLEMENTED_INFO_CLASS(MemoryWorkingSetList);
            UNIMPLEMENTED_INFO_CLASS(MemorySectionName);
            UNIMPLEMENTED_INFO_CLASS(MemoryBasicVlmInformation);

            default:
                FIXME("(%p,%p,info_class=%d,%p,%ld,%p) Unknown information class\n", 
                      process, addr, info_class, buffer, len, res_len);
                return STATUS_INVALID_INFO_CLASS;
        }
    }
    if (ADDRESS_SPACE_LIMIT && addr >= ADDRESS_SPACE_LIMIT)
        return STATUS_WORKING_SET_LIMIT_RANGE;

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    base = ROUND_ADDR( addr, page_mask );

    /* Find the view containing the address */

    RtlEnterCriticalSection(&csVirtual);
    ptr = list_head( &views_list );
    for (;;)
    {
        if (!ptr)
        {
            /* make the address space end at the user limit, except if
             * the last view was mapped beyond that */
            if (alloc_base <= (char *)user_space_limit)
            {
                if (user_space_limit && base >= (char *)user_space_limit)
                {
                    RtlLeaveCriticalSection( &csVirtual );
                    return STATUS_WORKING_SET_LIMIT_RANGE;
                }
                size = (char *)user_space_limit - alloc_base;
            }
            else size = (char *)ADDRESS_SPACE_LIMIT - alloc_base;
            view = NULL;
            break;
        }
        view = LIST_ENTRY( ptr, struct file_view, entry );
        if ((char *)view->base > base)
        {
            size = (char *)view->base - alloc_base;
            view = NULL;
            break;
        }
        if ((char *)view->base + view->size > base)
        {
            alloc_base = view->base;
            size = view->size;
            break;
        }
        alloc_base = (char *)view->base + view->size;
        ptr = list_next( &views_list, ptr );
    }

    /* Fill the info structure */

    if (!view)
    {
        info->State             = MEM_FREE;
        info->Protect           = 0;
        info->AllocationProtect = 0;
        info->Type              = 0;
    }
    else
    {
        BYTE vprot = view->prot[(base - alloc_base) >> page_shift];
        VIRTUAL_GetWin32Prot( vprot, &info->Protect, &info->State );
        for (size = base - alloc_base; size < view->size; size += page_mask+1)
            if (view->prot[size >> page_shift] != vprot) break;
        VIRTUAL_GetWin32Prot( view->protect, &info->AllocationProtect, NULL );
        if (view->protect & VPROT_IMAGE) info->Type = MEM_IMAGE;
        else if (view->flags & VFLAG_VALLOC) info->Type = MEM_PRIVATE;
        else info->Type = MEM_MAPPED;
    }
    RtlLeaveCriticalSection(&csVirtual);

    info->BaseAddress    = (LPVOID)base;
    info->AllocationBase = (LPVOID)alloc_base;
    info->RegionSize     = size - (base - alloc_base);
    if (res_len) *res_len = sizeof(*info);
    return STATUS_SUCCESS;
}


/***********************************************************************
 *             NtLockVirtualMemory   (NTDLL.@)
 *             ZwLockVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtLockVirtualMemory( HANDLE process, PVOID *addr, SIZE_T *size, ULONG unknown )
{
    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *             NtUnlockVirtualMemory   (NTDLL.@)
 *             ZwUnlockVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtUnlockVirtualMemory( HANDLE process, PVOID *addr, SIZE_T *size, ULONG unknown )
{
    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }
    return STATUS_SUCCESS;
}


/***********************************************************************
 *             NtCreateSection   (NTDLL.@)
 *             ZwCreateSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtCreateSection( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr,
                                 const LARGE_INTEGER *size, ULONG protect,
                                 ULONG sec_flags, HANDLE file )
{
    NTSTATUS ret;
    BYTE vprot;
    DWORD len = (attr && attr->ObjectName) ? attr->ObjectName->Length : 0;

    /* Check parameters */

    if (len > MAX_PATH*sizeof(WCHAR)) return STATUS_NAME_TOO_LONG;

    vprot = VIRTUAL_GetProt( protect );
    if (sec_flags & SEC_RESERVE)
    {
        if (file) return STATUS_INVALID_PARAMETER;
    }
    else vprot |= VPROT_COMMITTED;
    if (sec_flags & SEC_NOCACHE) vprot |= VPROT_NOCACHE;
    if (sec_flags & SEC_IMAGE) vprot |= VPROT_IMAGE;

    /* Create the server object */

    SERVER_START_REQ( create_mapping )
    {
        req->access      = access;
        req->attributes  = (attr) ? attr->Attributes : 0;
        req->file_handle = file;
        req->size_high   = size ? size->u.HighPart : 0;
        req->size_low    = size ? size->u.LowPart : 0;
        req->protect     = vprot;
        if (len) wine_server_add_data( req, attr->ObjectName->Buffer, len );
        ret = wine_server_call( req );
        *handle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *             NtOpenSection   (NTDLL.@)
 *             ZwOpenSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtOpenSection( HANDLE *handle, ACCESS_MASK access, const OBJECT_ATTRIBUTES *attr )
{
    NTSTATUS ret;
    DWORD len = attr->ObjectName->Length;

    if (len > MAX_PATH*sizeof(WCHAR)) return STATUS_NAME_TOO_LONG;

    SERVER_START_REQ( open_mapping )
    {
        req->access  = access;
        req->attributes = (attr) ? attr->Attributes : 0;
        wine_server_add_data( req, attr->ObjectName->Buffer, len );
        if (!(ret = wine_server_call( req ))) *handle = reply->handle;
    }
    SERVER_END_REQ;
    return ret;
}


/***********************************************************************
 *             NtMapViewOfSection   (NTDLL.@)
 *             ZwMapViewOfSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtMapViewOfSection( HANDLE handle, HANDLE process, PVOID *addr_ptr, ULONG zero_bits,
                                    SIZE_T commit_size, const LARGE_INTEGER *offset_ptr, SIZE_T *size_ptr,
                                    SECTION_INHERIT inherit, ULONG alloc_type, ULONG protect )
{
    FILE_FS_DEVICE_INFORMATION device_info;
    NTSTATUS res;
    SIZE_T size = 0;
    int unix_handle = -1;
    int prot;
    void *base;
    struct file_view *view;
    DWORD size_low, size_high, header_size, shared_size;
    HANDLE shared_file;
    BOOL removable = FALSE;
    LARGE_INTEGER offset;

    offset.QuadPart = offset_ptr ? offset_ptr->QuadPart : 0;

    TRACE("handle=%p process=%p addr=%p off=%lx%08lx size=%lx access=%lx\n",
          handle, process, *addr_ptr, offset.u.HighPart, offset.u.LowPart, size, protect );

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }

    /* Check parameters */

    if ((offset.u.LowPart & granularity_mask) ||
        (*addr_ptr && ((UINT_PTR)*addr_ptr & granularity_mask)))
        return STATUS_INVALID_PARAMETER;

    SERVER_START_REQ( get_mapping_info )
    {
        req->handle = handle;
        res = wine_server_call( req );
        prot        = reply->protect;
        base        = reply->base;
        size_low    = reply->size_low;
        size_high   = reply->size_high;
        header_size = reply->header_size;
        shared_file = reply->shared_file;
        shared_size = reply->shared_size;
    }
    SERVER_END_REQ;
    if (res) return res;

    if ((res = wine_server_handle_to_fd( handle, 0, &unix_handle, NULL ))) return res;

    if (FILE_GetDeviceInfo( unix_handle, &device_info ) == STATUS_SUCCESS)
        removable = device_info.Characteristics & FILE_REMOVABLE_MEDIA;

    if (prot & VPROT_IMAGE)
    {
        if (shared_file)
        {
            int shared_fd;

            if ((res = wine_server_handle_to_fd( shared_file, GENERIC_READ, &shared_fd,
                                                 NULL ))) goto done;
            res = map_image( handle, unix_handle, base, size_low, header_size,
                             shared_fd, removable, addr_ptr );
            wine_server_release_fd( shared_file, shared_fd );
            NtClose( shared_file );
        }
        else
        {
            res = map_image( handle, unix_handle, base, size_low, header_size,
                             -1, removable, addr_ptr );
        }
        wine_server_release_fd( handle, unix_handle );
        if (!res) *size_ptr = size_low;
        return res;
    }

    if (size_high)
        ERR("Sizes larger than 4Gb not supported\n");

    if ((offset.u.LowPart >= size_low) ||
        (*size_ptr > size_low - offset.u.LowPart))
    {
        res = STATUS_INVALID_PARAMETER;
        goto done;
    }
    if (*size_ptr) size = ROUND_SIZE( offset.u.LowPart, *size_ptr );
    else size = size_low - offset.u.LowPart;

    switch(protect)
    {
    case PAGE_NOACCESS:
        break;
    case PAGE_READWRITE:
    case PAGE_EXECUTE_READWRITE:
        if (!(prot & VPROT_WRITE))
        {
            res = STATUS_INVALID_PARAMETER;
            goto done;
        }
        removable = FALSE;
        /* fall through */
    case PAGE_READONLY:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_WRITECOPY:
        if (prot & VPROT_READ) break;
        /* fall through */
    default:
        res = STATUS_INVALID_PARAMETER;
        goto done;
    }

    /* FIXME: If a mapping is created with SEC_RESERVE and a process,
     * which has a view of this mapping commits some pages, they will
     * appear commited in all other processes, which have the same
     * view created. Since we don`t support this yet, we create the
     * whole mapping commited.
     */
    prot |= VPROT_COMMITTED;

    /* Reserve a properly aligned area */

    RtlEnterCriticalSection( &csVirtual );

    res = map_view( &view, *addr_ptr, size, prot );
    if (res)
    {
        RtlLeaveCriticalSection( &csVirtual );
        goto done;
    }

    /* Map the file */

    TRACE("handle=%p size=%lx offset=%lx%08lx\n",
          handle, size, offset.u.HighPart, offset.u.LowPart );

    res = map_file_into_view( view, unix_handle, 0, size, offset.QuadPart, prot, removable );
    if (res == STATUS_SUCCESS)
    {
        if (!removable)  /* don't keep handle open on removable media */
            NtDuplicateObject( NtCurrentProcess(), handle,
                               NtCurrentProcess(), &view->mapping,
                               0, 0, DUPLICATE_SAME_ACCESS );

        *addr_ptr = view->base;
        *size_ptr = size;
    }
    else
    {
        ERR( "map_file_into_view %p %lx %lx%08lx failed\n",
             view->base, size, offset.u.HighPart, offset.u.LowPart );
        delete_view( view );
    }

    RtlLeaveCriticalSection( &csVirtual );

done:
    wine_server_release_fd( handle, unix_handle );
    return res;
}


/***********************************************************************
 *             NtUnmapViewOfSection   (NTDLL.@)
 *             ZwUnmapViewOfSection   (NTDLL.@)
 */
NTSTATUS WINAPI NtUnmapViewOfSection( HANDLE process, PVOID addr )
{
    FILE_VIEW *view;
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    void *base = ROUND_ADDR( addr, page_mask );

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }
    RtlEnterCriticalSection( &csVirtual );
    if ((view = VIRTUAL_FindView( base )) && (base == view->base))
    {
        delete_view( view );
        status = STATUS_SUCCESS;
    }
    RtlLeaveCriticalSection( &csVirtual );
    return status;
}


/***********************************************************************
 *             NtFlushVirtualMemory   (NTDLL.@)
 *             ZwFlushVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtFlushVirtualMemory( HANDLE process, LPCVOID *addr_ptr,
                                      SIZE_T *size_ptr, ULONG unknown )
{
    FILE_VIEW *view;
    NTSTATUS status = STATUS_SUCCESS;
    void *addr = ROUND_ADDR( *addr_ptr, page_mask );

    if (!is_current_process( process ))
    {
        ERR("Unsupported on other process\n");
        return STATUS_ACCESS_DENIED;
    }
    RtlEnterCriticalSection( &csVirtual );
    if (!(view = VIRTUAL_FindView( addr ))) status = STATUS_INVALID_PARAMETER;
    else
    {
        if (!*size_ptr) *size_ptr = view->size;
        *addr_ptr = addr;
        if (msync( addr, *size_ptr, MS_SYNC )) status = STATUS_NOT_MAPPED_DATA;
    }
    RtlLeaveCriticalSection( &csVirtual );
    return status;
}


/***********************************************************************
 *             NtReadVirtualMemory   (NTDLL.@)
 *             ZwReadVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtReadVirtualMemory( HANDLE process, const void *addr, void *buffer,
                                     SIZE_T size, SIZE_T *bytes_read )
{
    NTSTATUS status;

    SERVER_START_REQ( read_process_memory )
    {
        req->handle = process;
        req->addr   = (void *)addr;
        wine_server_set_reply( req, buffer, size );
        if ((status = wine_server_call( req ))) size = 0;
    }
    SERVER_END_REQ;
    if (bytes_read) *bytes_read = size;
    return status;
}


/***********************************************************************
 *             NtWriteVirtualMemory   (NTDLL.@)
 *             ZwWriteVirtualMemory   (NTDLL.@)
 */
NTSTATUS WINAPI NtWriteVirtualMemory( HANDLE process, void *addr, const void *buffer,
                                      SIZE_T size, SIZE_T *bytes_written )
{
    static const unsigned int zero;
    SIZE_T first_offset, last_offset, first_mask, last_mask;
    NTSTATUS status;

    if (!size) return STATUS_INVALID_PARAMETER;

    /* compute the mask for the first int */
    first_mask = ~0;
    first_offset = (ULONG_PTR)addr % sizeof(int);
    memset( &first_mask, 0, first_offset );

    /* compute the mask for the last int */
    last_offset = (size + first_offset) % sizeof(int);
    last_mask = 0;
    memset( &last_mask, 0xff, last_offset ? last_offset : sizeof(int) );

    SERVER_START_REQ( write_process_memory )
    {
        req->handle     = process;
        req->addr       = (char *)addr - first_offset;
        req->first_mask = first_mask;
        req->last_mask  = last_mask;
        if (first_offset) wine_server_add_data( req, &zero, first_offset );
        wine_server_add_data( req, buffer, size );
        if (last_offset) wine_server_add_data( req, &zero, sizeof(int) - last_offset );

        if ((status = wine_server_call( req ))) size = 0;
    }
    SERVER_END_REQ;
    if (bytes_written) *bytes_written = size;
    return status;
}
