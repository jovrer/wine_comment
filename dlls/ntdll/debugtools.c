/*
 * Debugging functions
 *
 * Copyright 2000 Alexandre Julliard
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <ctype.h>

#include "wine/debug.h"
#include "wine/exception.h"
#include "wine/library.h"
#include "wine/unicode.h"
#include "ntstatus.h"
#include "winnt.h"
#include "winternl.h"
#include "excpt.h"
#include "ntdll_misc.h"

WINE_DECLARE_DEBUG_CHANNEL(tid);

static struct __wine_debug_functions default_funcs;

/* ---------------------------------------------------------------------- */

/* filter for page-fault exceptions */
static WINE_EXCEPTION_FILTER(page_fault)
{
    if (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_EXECUTE_HANDLER;
    return EXCEPTION_CONTINUE_SEARCH;
}

/* get the debug info pointer for the current thread */
static inline struct debug_info *get_info(void)
{
    return ntdll_get_thread_data()->debug_info;
}

/* allocate some tmp space for a string */
static char *get_temp_buffer( size_t n )
{
    struct debug_info *info = get_info();
    char *res = info->str_pos;

    if (res + n >= &info->strings[sizeof(info->strings)]) res = info->strings;
    info->str_pos = res + n;
    return res;
}

/* release extra space that we requested in gimme1() */
static void release_temp_buffer( char *ptr, size_t size )
{
    struct debug_info *info = get_info();
    info->str_pos = ptr + size;
}

/***********************************************************************
 *		NTDLL_dbgstr_an
 */
static const char *NTDLL_dbgstr_an( const char *src, int n )
{
    const char *res;
    struct debug_info *info = get_info();
    /* save current position to restore it on exception */
    char *old_pos = info->str_pos;

    __TRY
    {
        res = default_funcs.dbgstr_an( src, n );
    }
    __EXCEPT(page_fault)
    {
        release_temp_buffer( old_pos, 0 );
        return "(invalid)";
    }
    __ENDTRY
    return res;
}

/***********************************************************************
 *		NTDLL_dbgstr_wn
 */
static const char *NTDLL_dbgstr_wn( const WCHAR *src, int n )
{
    const char *res;
    struct debug_info *info = get_info();
    /* save current position to restore it on exception */
    char *old_pos = info->str_pos;

    __TRY
    {
        res = default_funcs.dbgstr_wn( src, n );
    }
    __EXCEPT(page_fault)
    {
        release_temp_buffer( old_pos, 0 );
        return "(invalid)";
    }
    __ENDTRY
     return res;
}

/***********************************************************************
 *		NTDLL_dbg_vprintf
 */
static int NTDLL_dbg_vprintf( const char *format, va_list args )
{
    struct debug_info *info = get_info();
    char *p;

    int ret = vsnprintf( info->out_pos, sizeof(info->output) - (info->out_pos - info->output),
                         format, args );

    /* make sure we didn't exceed the buffer length
     * the two checks are due to glibc changes in vsnprintfs return value
     * the buffer size can be exceeded in case of a missing \n in
     * debug output */
    if ((ret == -1) || (ret >= sizeof(info->output) - (info->out_pos - info->output)))
    {
       fprintf( stderr, "wine_dbg_vprintf: debugstr buffer overflow (contents: '%s')\n",
                info->output);
       info->out_pos = info->output;
       abort();
    }

    p = strrchr( info->out_pos, '\n' );
    if (!p) info->out_pos += ret;
    else
    {
        char *pos = info->output;
        p++;
        write( 2, pos, p - pos );
        /* move beginning of next line to start of buffer */
        while ((*pos = *p++)) pos++;
        info->out_pos = pos;
    }
    return ret;
}

/***********************************************************************
 *		NTDLL_dbg_vlog
 */
static int NTDLL_dbg_vlog( enum __wine_debug_class cls, struct __wine_debug_channel *channel,
                           const char *function, const char *format, va_list args )
{
    static const char * const classes[] = { "fixme", "err", "warn", "trace" };
    struct debug_info *info = get_info();
    int ret = 0;

    /* only print header if we are at the beginning of the line */
    if (info->out_pos == info->output || info->out_pos[-1] == '\n')
    {
        if (TRACE_ON(tid))
            ret = wine_dbg_printf( "%04lx:", GetCurrentThreadId() );
        if (cls < sizeof(classes)/sizeof(classes[0]))
            ret += wine_dbg_printf( "%s:%s:%s ", classes[cls], channel->name, function );
    }
    if (format)
        ret += NTDLL_dbg_vprintf( format, args );
    return ret;
}


static const struct __wine_debug_functions funcs =
{
    get_temp_buffer,
    release_temp_buffer,
    NTDLL_dbgstr_an,
    NTDLL_dbgstr_wn,
    NTDLL_dbg_vprintf,
    NTDLL_dbg_vlog
};

/***********************************************************************
 *		debug_init
 */
void debug_init(void)
{
    __wine_dbg_set_functions( &funcs, &default_funcs, sizeof(funcs) );
}
