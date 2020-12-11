/*
 * Debugger break-points handling
 *
 * Copyright 1994 Martin von Loewis
 * Copyright 1995 Alexandre Julliard
 * Copyright 1999,2000 Eric Pouech
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
#include "debugger.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg);

/***********************************************************************
 *           break_set_xpoints
 *
 * Set or remove all the breakpoints & watchpoints
 */
void  break_set_xpoints(BOOL set)
{
    static BOOL                 last; /* = 0 = FALSE */

    unsigned int                i, ret, size;
    void*                       addr;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    if (set == last) return;
    last = set;

    for (i = 0; i < dbg_curr_process->next_bp; i++)
    {
        if (!bp[i].refcount || !bp[i].enabled) continue;

        if (bp[i].xpoint_type == be_xpoint_break)
            size = 0;
        else
            size = bp[i].w.len + 1;
        addr = (void*)memory_to_linear_addr(&bp[i].addr);

        if (set)
            ret = be_cpu->insert_Xpoint(dbg_curr_process->handle,
                                        dbg_curr_process->process_io,
                                        &dbg_context, bp[i].xpoint_type, addr,
                                        &bp[i].info, size);
        else
            ret = be_cpu->remove_Xpoint(dbg_curr_process->handle, 
                                        dbg_curr_process->process_io,
                                        &dbg_context, bp[i].xpoint_type, addr,
                                        bp[i].info, size);
        if (!ret)
        {
            dbg_printf("Invalid address (");
            print_address(&bp[i].addr, FALSE);
            dbg_printf(") for breakpoint %d, disabling it\n", i);
            bp[i].enabled = FALSE;
        }
    }
}

/***********************************************************************
 *           find_xpoint
 *
 * Find the breakpoint for a given address. Return the breakpoint
 * number or -1 if none.
 */
static int find_xpoint(const ADDRESS* addr, enum be_xpoint_type type)
{
    int                         i;
    void*                       lin = memory_to_linear_addr(addr);
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    for (i = 0; i < dbg_curr_process->next_bp; i++)
    {
        if (bp[i].refcount && bp[i].enabled && bp[i].xpoint_type == type &&
            memory_to_linear_addr(&bp[i].addr) == lin)
            return i;
    }
    return -1;
}

/***********************************************************************
 *           init_xpoint
 *
 * Find an empty slot in BP table to add a new break/watch point
 */
static	int init_xpoint(int type, const ADDRESS* addr)
{
    int	                        num;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    for (num = (dbg_curr_process->next_bp < MAX_BREAKPOINTS) ? 
             dbg_curr_process->next_bp++ : 1;
         num < MAX_BREAKPOINTS; num++)
    {
        if (bp[num].refcount == 0)
        {
            bp[num].refcount    = 1;
            bp[num].enabled     = TRUE;
            bp[num].xpoint_type = type;
            bp[num].skipcount   = 0;
            bp[num].addr        = *addr;
            return num;
        }
    }

    dbg_printf("Too many bp. Please delete some.\n");
    return -1;
}

/***********************************************************************
 *           get_watched_value
 *
 * Returns the value watched by watch point 'num'.
 */
static	BOOL	get_watched_value(int num, LPDWORD val)
{
    BYTE        buf[4];

    if (!dbg_read_memory(memory_to_linear_addr(&dbg_curr_process->bp[num].addr),
                         buf, dbg_curr_process->bp[num].w.len + 1))
        return FALSE;

    switch (dbg_curr_process->bp[num].w.len + 1)
    {
    case 4:	*val = *(DWORD*)buf;	break;
    case 2:	*val = *(WORD*)buf;	break;
    case 1:	*val = *(BYTE*)buf;	break;
    default: RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
    }
    return TRUE;
}

/***********************************************************************
 *           break_add_break
 *
 * Add a breakpoint.
 */
BOOL break_add_break(const ADDRESS* addr, BOOL verbose)
{
    int                         num;
    BYTE                        ch;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    if ((num = find_xpoint(addr, be_xpoint_break)) >= 1)
    {
        bp[num].refcount++;
        dbg_printf("Breakpoint %d at ", num);
        print_address(&bp[num].addr, TRUE);
        dbg_printf(" (refcount=%d)\n", bp[num].refcount);
        return TRUE;
    }

    if (!dbg_read_memory(memory_to_linear_addr(addr), &ch, sizeof(ch)))
    {
        if (verbose)
        {
            dbg_printf("Invalid address ");
            print_bare_address(addr);
            dbg_printf(", can't set breakpoint\n");
        }
        return FALSE;
    }

    if ((num = init_xpoint(be_xpoint_break, addr)) == -1)
        return FALSE;

    dbg_printf("Breakpoint %d at ", num);
    print_address(&bp[num].addr, TRUE);
    dbg_printf("\n");

    return TRUE;
}

/***********************************************************************
 *           break_add_break_from_lvalue
 *
 * Add a breakpoint.
 */
BOOL break_add_break_from_lvalue(const struct dbg_lvalue* lvalue)
{
    ADDRESS     addr;

    addr.Mode = AddrModeFlat;
    addr.Offset = types_extract_as_integer(lvalue);

    if (!break_add_break(&addr, TRUE))
    {
        if (!DBG_IVAR(CanDeferOnBPByAddr))
        {
            dbg_printf("Invalid address, can't set breakpoint\n"
                       "You can turn on deferring bp by address by setting $CanDeferOnBPByAddr to 1\n");
            return FALSE;
        }
        dbg_printf("Unable to add breakpoint, will check again any time a new DLL is loaded\n");
        dbg_curr_process->delayed_bp = 
            dbg_heap_realloc(dbg_curr_process->delayed_bp,
                             sizeof(struct dbg_delayed_bp) * ++dbg_curr_process->num_delayed_bp);

        dbg_curr_process->delayed_bp[dbg_curr_process->num_delayed_bp - 1].is_symbol = FALSE;
        dbg_curr_process->delayed_bp[dbg_curr_process->num_delayed_bp - 1].u.addr    = addr;
        return TRUE;
    }
    return FALSE;
}

/***********************************************************************
 *           break_add_break_from_id
 *
 * Add a breakpoint from a function name (and eventually a line #)
 */
void	break_add_break_from_id(const char *name, int lineno)
{
    struct dbg_lvalue 	lvalue;
    int		        i;

    switch (symbol_get_lvalue(name, lineno, &lvalue, TRUE))
    {
    case sglv_found:
        break_add_break(&lvalue.addr, TRUE);
        return;
    case sglv_unknown:
        break;
    case sglv_aborted: /* user aborted symbol lookup */
        return;
    }

    dbg_printf("Unable to add breakpoint, will check again when a new DLL is loaded\n");
    for (i = 0; i < dbg_curr_process->num_delayed_bp; i++)
    {
        if (dbg_curr_process->delayed_bp[i].is_symbol &&
            !strcmp(name, dbg_curr_process->delayed_bp[i].u.symbol.name) &&
            lineno == dbg_curr_process->delayed_bp[i].u.symbol.lineno)
            return;
    }
    dbg_curr_process->delayed_bp = dbg_heap_realloc(dbg_curr_process->delayed_bp,
                                                    sizeof(struct dbg_delayed_bp) * ++dbg_curr_process->num_delayed_bp);

    dbg_curr_process->delayed_bp[dbg_curr_process->num_delayed_bp - 1].is_symbol = TRUE;
    dbg_curr_process->delayed_bp[dbg_curr_process->num_delayed_bp - 1].u.symbol.name = strcpy(HeapAlloc(GetProcessHeap(), 0, strlen(name) + 1), name);
    dbg_curr_process->delayed_bp[dbg_curr_process->num_delayed_bp - 1].u.symbol.lineno = lineno;
}

struct cb_break_lineno
{
    int         lineno;
    ADDRESS     addr;
};

static BOOL CALLBACK line_cb(SRCCODEINFO* sci, void* user)
{
    struct cb_break_lineno*      bkln = user;

    if (bkln->lineno == sci->LineNumber)
    {
        bkln->addr.Mode = AddrModeFlat;
        bkln->addr.Offset = sci->Address;
        return FALSE;
    }
    return TRUE;
}

/***********************************************************************
 *           break_add_break_from_lineno
 *
 * Add a breakpoint from a line number in current file
 */
void	break_add_break_from_lineno(int lineno)
{
    struct cb_break_lineno      bkln;

    memory_get_current_pc(&bkln.addr);

    if (lineno != -1)
    {
        IMAGEHLP_LINE   il;


        DWORD           disp;
        DWORD           linear = (DWORD)memory_to_linear_addr(&bkln.addr);

        il.SizeOfStruct = sizeof(il);
        if (!SymGetLineFromAddr(dbg_curr_process->handle, linear, &disp, &il))

        {
            dbg_printf("Unable to add breakpoint (unknown address %lx)\n", linear);
            return;
        }
        bkln.addr.Offset = 0;
        bkln.lineno = lineno;
        SymEnumLines(dbg_curr_process->handle, linear, NULL, il.FileName, line_cb, &bkln);
        if (!bkln.addr.Offset)
        {
            dbg_printf("Unknown line number\n"
                       "(either out of file, or no code at given line number)\n");
            return;
        }
    }

    break_add_break(&bkln.addr, TRUE);
}

/***********************************************************************
 *           break_check_delayed_bp
 *
 * Check is a registered delayed BP is now available.
 */
void break_check_delayed_bp(void)
{
    struct dbg_lvalue	        lvalue;
    int			        i;
    struct dbg_delayed_bp*	dbp = dbg_curr_process->delayed_bp;

    for (i = 0; i < dbg_curr_process->num_delayed_bp; i++)
    {
        if (dbp[i].is_symbol)
        {
            if (symbol_get_lvalue(dbp[i].u.symbol.name, dbp[i].u.symbol.lineno,
                                  &lvalue, TRUE) != sglv_found)
                continue;
            if (lvalue.cookie != DLV_TARGET) continue;
        }
        else
            lvalue.addr = dbp[i].u.addr;
        WINE_TRACE("trying to add delayed %s-bp\n", dbp[i].is_symbol ? "S" : "A");
        if (!dbp[i].is_symbol)
            WINE_TRACE("\t%04x:%08lx\n", 
                       dbp[i].u.addr.Segment, dbp[i].u.addr.Offset);
        else
            WINE_TRACE("\t'%s' @ %d\n", 
                       dbp[i].u.symbol.name, dbp[i].u.symbol.lineno);

        if (break_add_break(&lvalue.addr, FALSE))
            memmove(&dbp[i], &dbp[i+1], (--dbg_curr_process->num_delayed_bp - i) * sizeof(*dbp));
    }
}

/***********************************************************************
 *           break_add_watch
 *
 * Add a watchpoint.
 */
static void break_add_watch(const struct dbg_lvalue* lvalue, BOOL is_write)
{
    int         num;
    DWORD       l = 4;

    num = init_xpoint((is_write) ? be_xpoint_watch_write : be_xpoint_watch_read,
                      &lvalue->addr);
    if (num == -1) return;

    if (lvalue->type.id != dbg_itype_none)
    {
        if (types_get_info(&lvalue->type, TI_GET_LENGTH, &l))
        {
            switch (l)
            {
            case 4: case 2: case 1: break;
            default:
                dbg_printf("Unsupported length (%lu) for watch-points, defaulting to 4\n", l);
                break;
            }
        }
        else dbg_printf("Cannot get watch size, defaulting to 4\n");
    }
    dbg_curr_process->bp[num].w.len = l - 1;

    if (!get_watched_value(num, &dbg_curr_process->bp[num].w.oldval))
    {
        dbg_printf("Bad address. Watchpoint not set\n");
        dbg_curr_process->bp[num].refcount = 0;
        return;
    }
    dbg_printf("Watchpoint %d at ", num);
    print_address(&dbg_curr_process->bp[num].addr, TRUE);
    dbg_printf("\n");
}

/******************************************************************
 *		break_add_watch_from_lvalue
 *
 * Adds a watch point from an address (stored in a lvalue)
 */
void break_add_watch_from_lvalue(const struct dbg_lvalue* lvalue)
{
    struct dbg_lvalue   lval;

    lval.addr.Mode = AddrModeFlat;
    lval.addr.Offset = types_extract_as_integer(lvalue);
    lval.type.id = dbg_itype_none;

    break_add_watch(&lval, TRUE);
}

/***********************************************************************
 *           break_add_watch_from_id
 *
 * Add a watchpoint from a symbol name
 */
void	break_add_watch_from_id(const char *name)
{
    struct dbg_lvalue    lvalue;

    switch (symbol_get_lvalue(name, -1, &lvalue, TRUE))
    {
    case sglv_found:
        break_add_watch(&lvalue, 1);
        break;
    case sglv_unknown:
        dbg_printf("Unable to add watchpoint\n");
        break;
    case sglv_aborted: /* user aborted symbol lookup */
        break;
    }
}

/***********************************************************************
 *           break_delete_xpoint
 *
 * Delete a breakpoint.
 */
void break_delete_xpoint(int num)
{
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    if ((num <= 0) || (num >= dbg_curr_process->next_bp) ||
        bp[num].refcount == 0)
    {
        dbg_printf("Invalid breakpoint number %d\n", num);
        return;
    }

    if (--bp[num].refcount > 0)
        return;

    if (bp[num].condition != NULL)
    {
        expr_free(bp[num].condition);
        bp[num].condition = NULL;
    }

    bp[num].enabled = FALSE;
    bp[num].refcount = 0;
    bp[num].skipcount = 0;
}

static inline BOOL module_is_container(const IMAGEHLP_MODULE* wmod_cntnr,
                                     const IMAGEHLP_MODULE* wmod_child)
{
    return wmod_cntnr->BaseOfImage <= wmod_child->BaseOfImage &&
        (DWORD)wmod_cntnr->BaseOfImage + wmod_cntnr->ImageSize >=
        (DWORD)wmod_child->BaseOfImage + wmod_child->ImageSize;
}

/******************************************************************
 *		break_delete_xpoints_from_module
 *
 * Remove all Xpoints from module which base is 'base'
 */
void break_delete_xpoints_from_module(unsigned long base)
{
    IMAGEHLP_MODULE             im, im_elf;
    int                         i;
    DWORD                       linear;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    /* FIXME: should do it also on the ELF sibbling if any */
    im.SizeOfStruct = sizeof(im);
    im_elf.SizeOfStruct = sizeof(im_elf);
    if (!SymGetModuleInfo(dbg_curr_process->handle, base, &im)) return;

    /* try to get in fact the underlying ELF module (if any) */
    if (SymGetModuleInfo(dbg_curr_process->handle, im.BaseOfImage - 1, &im_elf) &&
        im_elf.BaseOfImage <= im.BaseOfImage &&
        (DWORD)im_elf.BaseOfImage + im_elf.ImageSize >= (DWORD)im.BaseOfImage + im.ImageSize)
        im = im_elf;

    for (i = 0; i < dbg_curr_process->next_bp; i++)
    {
        linear = (DWORD)memory_to_linear_addr(&bp[i].addr);
        if (bp[i].refcount && bp[i].enabled &&
            im.BaseOfImage <= linear && linear < im.BaseOfImage + im.ImageSize)
        {
            break_delete_xpoint(i);
        }
    }
}

/***********************************************************************
 *           break_enable_xpoint
 *
 * Enable or disable a break point.
 */
void break_enable_xpoint(int num, BOOL enable)
{
    if ((num <= 0) || (num >= dbg_curr_process->next_bp) || 
        dbg_curr_process->bp[num].refcount == 0)
    {
        dbg_printf("Invalid breakpoint number %d\n", num);
        return;
    }
    dbg_curr_process->bp[num].enabled = (enable) ? TRUE : FALSE;
    dbg_curr_process->bp[num].skipcount = 0;
}


/***********************************************************************
 *           find_triggered_watch
 *
 * Lookup the watchpoints to see if one has been triggered
 * Return >= (watch point index) if one is found and *oldval is set to
 * 	the value watched before the TRAP
 * Return -1 if none found (*oldval is undetermined)
 *
 * Unfortunately, Linux does *NOT* (A REAL PITA) report with ptrace
 * the DR6 register value, so we have to look with our own need the
 * cause of the TRAP.
 * -EP
 */
static int find_triggered_watch(LPDWORD oldval)
{
    int                         found = -1;
    int                         i;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    /* Method 1 => get triggered watchpoint from context (doesn't work on Linux
     * 2.2.x). This should be fixed in >= 2.2.16
     */
    for (i = 0; i < dbg_curr_process->next_bp; i++)
    {
        DWORD val = 0;

        if (bp[i].refcount && bp[i].enabled && bp[i].xpoint_type != be_xpoint_break &&
            (be_cpu->is_watchpoint_set(&dbg_context, bp[i].info)))
        {
            be_cpu->clear_watchpoint(&dbg_context, bp[i].info);

            *oldval = bp[i].w.oldval;
            if (get_watched_value(i, &val))
            {
                bp[i].w.oldval = val;
                return i;
            }
        }
    }

    /* Method 1 failed, trying method 2 */

    /* Method 2 => check if value has changed among registered watchpoints
     * this really sucks, but this is how gdb 4.18 works on my linux box
     * -EP
     */
    for (i = 0; i < dbg_curr_process->next_bp; i++)
    {
        DWORD val = 0;

        if (bp[i].refcount && bp[i].enabled && bp[i].xpoint_type != be_xpoint_break &&
            get_watched_value(i, &val))
        {
            *oldval = bp[i].w.oldval;
            if (val != *oldval)
            {
                be_cpu->clear_watchpoint(&dbg_context, bp[i].info);
                bp[i].w.oldval = val;
                found = i;
                /* cannot break, because two watch points may have been triggered on
                 * the same access
                 * only one will be reported to the user (FIXME ?)
                 */
            }
        }
    }
    return found;
}

/***********************************************************************
 *           break_info
 *
 * Display break & watch points information.
 */
void break_info(void)
{
    int                         i;
    int                         nbp = 0, nwp = 0;
    struct dbg_delayed_bp*	dbp = dbg_curr_process->delayed_bp;
    struct dbg_breakpoint*      bp = dbg_curr_process->bp;

    for (i = 1; i < dbg_curr_process->next_bp; i++)
    {
        if (bp[i].refcount)
        {
            if (bp[i].xpoint_type == be_xpoint_break) nbp++; else nwp++;
        }
    }

    if (nbp)
    {
        dbg_printf("Breakpoints:\n");
        for (i = 1; i < dbg_curr_process->next_bp; i++)
        {
            if (!bp[i].refcount || bp[i].xpoint_type != be_xpoint_break)
                continue;
            dbg_printf("%d: %c ", i, bp[i].enabled ? 'y' : 'n');
            print_address(&bp[i].addr, TRUE);
            dbg_printf(" (%u)\n", bp[i].refcount);
	    if (bp[i].condition != NULL)
	    {
	        dbg_printf("\t\tstop when  ");
 		expr_print(bp[i].condition);
		dbg_printf("\n");
	    }
        }
    }
    else dbg_printf("No breakpoints\n");
    if (nwp)
    {
        dbg_printf("Watchpoints:\n");
        for (i = 1; i < dbg_curr_process->next_bp; i++)
        {
            if (!bp[i].refcount || bp[i].xpoint_type == be_xpoint_break)
                continue;
            dbg_printf("%d: %c ", i, bp[i].enabled ? 'y' : 'n');
            print_address(&bp[i].addr, TRUE);
            dbg_printf(" on %d byte%s (%c)\n",
                       bp[i].w.len + 1, bp[i].w.len > 0 ? "s" : "",
                       bp[i].xpoint_type == be_xpoint_watch_write ? 'W' : 'R');
	    if (bp[i].condition != NULL)
	    {
	        dbg_printf("\t\tstop when ");
 		expr_print(bp[i].condition);
		dbg_printf("\n");
	    }
        }
    }
    else dbg_printf("No watchpoints\n");
    if (dbg_curr_process->num_delayed_bp)
    {
        dbg_printf("Delayed breakpoints:\n");
        for (i = 0; i < dbg_curr_process->num_delayed_bp; i++)
        {
            if (dbp[i].is_symbol)
            {
                dbg_printf("%d: %s", i, dbp[i].u.symbol.name);
                if (dbp[i].u.symbol.lineno != -1)
                    dbg_printf(" at line %u", dbp[i].u.symbol.lineno);
            }
            else
            {
                dbg_printf("%d: ", i);
                print_address(&dbp[i].u.addr, FALSE);
            }
            dbg_printf("\n");
        }
    }
}

/***********************************************************************
 *           should_stop
 *
 * Check whether or not the condition (bp / skipcount) of a break/watch
 * point are met.
 */
static	BOOL should_stop(int bpnum)
{
    struct dbg_breakpoint*      bp = &dbg_curr_process->bp[bpnum];

    if (bp->condition != NULL)
    {
        struct dbg_lvalue lvalue = expr_eval(bp->condition);

        if (lvalue.type.id == dbg_itype_none)
        {
	    /*
	     * Something wrong - unable to evaluate this expression.
	     */
	    dbg_printf("Unable to evaluate expression ");
	    expr_print(bp->condition);
	    dbg_printf("\nTurning off condition\n");
	    break_add_condition(bpnum, NULL);
        }
        else if (!types_extract_as_integer(&lvalue))
        {
	    return FALSE;
        }
    }

    if (bp->skipcount > 0) bp->skipcount--;
    return bp->skipcount == 0;
}

/***********************************************************************
 *           break_should_continue
 *
 * Determine if we should continue execution after a SIGTRAP signal when
 * executing in the given mode.
 */
BOOL break_should_continue(ADDRESS* addr, DWORD code, int* count, BOOL* is_break)
{
    int 	        bpnum;
    DWORD	        oldval = 0;
    int 	        wpnum;
    enum dbg_exec_mode  mode = dbg_curr_thread->exec_mode;

    *is_break = FALSE;
    /* If not single-stepping, back up to the break instruction */
    if (code == EXCEPTION_BREAKPOINT)
        addr->Offset += be_cpu->adjust_pc_for_break(&dbg_context, TRUE);

    bpnum = find_xpoint(addr, be_xpoint_break);
    dbg_curr_process->bp[0].enabled = FALSE;  /* disable the step-over breakpoint */

    if (bpnum > 0)
    {
        if (!should_stop(bpnum)) return TRUE;

        dbg_printf("Stopped on breakpoint %d at ", bpnum);
        print_address(&dbg_curr_process->bp[bpnum].addr, TRUE);
        dbg_printf("\n");
        return FALSE;
    }

    wpnum = find_triggered_watch(&oldval);
    if (wpnum > 0)
    {
        /* If not single-stepping, do not back up over the break instruction */
        if (code == EXCEPTION_BREAKPOINT)
            addr->Offset += be_cpu->adjust_pc_for_break(&dbg_context, FALSE);

        if (!should_stop(wpnum)) return TRUE;

        dbg_printf("Stopped on watchpoint %d at ", wpnum);
        print_address(addr, TRUE);
        dbg_printf(" values: old=%lu new=%lu\n",
                     oldval, dbg_curr_process->bp[wpnum].w.oldval);
        return FALSE;
    }

    /*
     * If our mode indicates that we are stepping line numbers,
     * get the current function, and figure out if we are exactly
     * on a line number or not.
     */
    if (mode == dbg_exec_step_over_line || mode == dbg_exec_step_into_line)
    {
	if (symbol_get_function_line_status(addr) == dbg_on_a_line_number)
	{
	    (*count)--;
	}
    }
    else if (mode == dbg_exec_step_over_insn || mode == dbg_exec_step_into_insn)
    {
	(*count)--;
    }

    if (*count > 0 || mode == dbg_exec_finish)
    {
	/*
	 * We still need to execute more instructions.
	 */
	return TRUE;
    }

    /* If there's no breakpoint and we are not single-stepping, then
     * either we must have encountered a break insn in the Windows program
     * or someone is trying to stop us
     */
    if (bpnum == -1 && code == EXCEPTION_BREAKPOINT)
    {
        *is_break = TRUE;
        addr->Offset += be_cpu->adjust_pc_for_break(&dbg_context, FALSE);
        return FALSE;
    }

    /* no breakpoint, continue if in continuous mode */
    return mode == dbg_exec_cont || mode == dbg_exec_finish;
}

/***********************************************************************
 *           break_suspend_execution
 *
 * Remove all bp before entering the debug loop
 */
void	break_suspend_execution(void)
{
    break_set_xpoints(FALSE);
    dbg_curr_process->bp[0] = dbg_curr_thread->step_over_bp;
}

/***********************************************************************
 *           break_restart_execution
 *
 * Set the bp to the correct state to restart execution
 * in the given mode.
 */
void break_restart_execution(int count)
{
    ADDRESS                     addr;
    int                         bp;
    enum dbg_line_status        status;
    enum dbg_exec_mode          mode, ret_mode;
    ADDRESS                     callee;
    void*                       linear;

    memory_get_current_pc(&addr);
    linear = memory_to_linear_addr(&addr);

    /*
     * This is the mode we will be running in after we finish.  We would like
     * to be able to modify this in certain cases.
     */
    ret_mode = mode = dbg_curr_thread->exec_mode;

    bp = find_xpoint(&addr, be_xpoint_break);
    if (bp != -1 && bp != 0)
    {
	/*
	 * If we have set a new value, then save it in the BP number.
	 */
	if (count != 0 && mode == dbg_exec_cont)
        {
	    dbg_curr_process->bp[bp].skipcount = count;
        }
        mode = dbg_exec_step_into_insn;  /* If there's a breakpoint, skip it */
    }
    else if (mode == dbg_exec_cont && count > 1)
    {
        dbg_printf("Not stopped at any breakpoint; argument ignored.\n");
    }

    if (mode == dbg_exec_finish && be_cpu->is_function_return(linear))
    {
	mode = ret_mode = dbg_exec_step_into_insn;
    }

    /*
     * See if the function we are stepping into has debug info
     * and line numbers.  If not, then we step over it instead.
     * FIXME - we need to check for things like thunks or trampolines,
     * as the actual function may in fact have debug info.
     */
    if (be_cpu->is_function_call(linear, &callee))
    {
	status = symbol_get_function_line_status(&callee);
#if 0
        /* FIXME: we need to get the thunk type */
	/*
	 * Anytime we have a trampoline, step over it.
	 */
	if ((mode == EXEC_STEP_OVER || mode == EXEC_STEPI_OVER)
	    && status == dbg_in_a_thunk)
        {
            WINE_WARN("Not stepping into trampoline at %p (no lines)\n", 
                      memory_to_linear_addr(&callee));
	    mode = EXEC_STEP_OVER_TRAMPOLINE;
        }
#endif
	if (mode == dbg_exec_step_into_line && status == dbg_no_line_info)
        {
            WINE_WARN("Not stepping into function at %p (no lines)\n",
                      memory_to_linear_addr(&callee));
	    mode = dbg_exec_step_over_line;
        }
    }

    if (mode == dbg_exec_step_into_line && 
        symbol_get_function_line_status(&addr) == dbg_no_line_info)
    {
        dbg_printf("Single stepping until exit from function, \n"
                   "which has no line number information.\n");
        ret_mode = mode = dbg_exec_finish;
    }

    switch (mode)
    {
    case dbg_exec_cont: /* Continuous execution */
        be_cpu->single_step(&dbg_context, FALSE);
        break_set_xpoints(TRUE);
        break;

#if 0
    case EXEC_STEP_OVER_TRAMPOLINE:
        /*
         * This is the means by which we step over our conversion stubs
         * in callfrom*.s and callto*.s.  We dig the appropriate address
         * off the stack, and we set the breakpoint there instead of the
         * address just after the call.
         */
        be_cpu->get_addr(dbg_curr_thread->handle, &dbg_context,
                         be_cpu_addr_stack, &addr);
        /* FIXME: we assume stack grows as on an i386 */
        addr.Offset += 2 * sizeof(unsigned int);
        dbg_read_memory(memory_to_linear_addr(&addr),
                        &addr.Offset, sizeof(addr.Offset));
        dbg_curr_process->bp[0].addr = addr;
        dbg_curr_process->bp[0].enabled = TRUE;
        dbg_curr_process->bp[0].refcount = 1;
        dbg_curr_process->bp[0].skipcount = 0;
        dbg_curr_process->bp[0].xpoint_type = be_xpoint_break;
        dbg_curr_process->bp[0].condition = NULL;
        be_cpu->single_step(&dbg_context, FALSE);
        break_set_xpoints(TRUE);
        break;
#endif

    case dbg_exec_finish:
    case dbg_exec_step_over_insn:  /* Stepping over a call */
    case dbg_exec_step_over_line:  /* Stepping over a call */
        if (be_cpu->is_step_over_insn(linear))
        {
            be_cpu->disasm_one_insn(&addr, FALSE);
            dbg_curr_process->bp[0].addr = addr;
            dbg_curr_process->bp[0].enabled = TRUE;
            dbg_curr_process->bp[0].refcount = 1;
	    dbg_curr_process->bp[0].skipcount = 0;
            dbg_curr_process->bp[0].xpoint_type = be_xpoint_break;
            dbg_curr_process->bp[0].condition = NULL;
            be_cpu->single_step(&dbg_context, FALSE);
            break_set_xpoints(TRUE);
            break;
        }
        /* else fall through to single-stepping */

    case dbg_exec_step_into_line: /* Single-stepping a line */
    case dbg_exec_step_into_insn: /* Single-stepping an instruction */
        be_cpu->single_step(&dbg_context, TRUE);
        break;
    default: RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
    }
    dbg_curr_thread->step_over_bp = dbg_curr_process->bp[0];
    dbg_curr_thread->exec_mode = ret_mode;
}

int break_add_condition(int num, struct expr* exp)
{
    if (num <= 0 || num >= dbg_curr_process->next_bp || 
        !dbg_curr_process->bp[num].refcount)
    {
        dbg_printf("Invalid breakpoint number %d\n", num);
        return FALSE;
    }

    if (dbg_curr_process->bp[num].condition != NULL)
    {
	expr_free(dbg_curr_process->bp[num].condition);
	dbg_curr_process->bp[num].condition = NULL;
    }

    if (exp != NULL) dbg_curr_process->bp[num].condition = expr_clone(exp, NULL);

    return TRUE;
}
