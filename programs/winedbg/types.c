/*
 * File types.c - datatype handling stuff for internal debugger.
 *
 * Copyright (C) 1997, Eric Youngdale.
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
 * Note: This really doesn't do much at the moment, but it forms the framework
 * upon which full support for datatype handling will eventually be built.
 */

#include "config.h"
#include <stdlib.h>

#include "debugger.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedbg);

/******************************************************************
 *		types_extract_as_integer
 *
 * Given a lvalue, try to get an integral (or pointer/address) value
 * out of it
 */
long int types_extract_as_integer(const struct dbg_lvalue* lvalue)
{
    long int            rtn = 0;
    DWORD               tag, size, bt;

    if (lvalue->type.id == dbg_itype_none ||
        !types_get_info(&lvalue->type, TI_GET_SYMTAG, &tag))
        return 0;

    switch (tag)
    {
    case SymTagBaseType:
        if (!types_get_info(&lvalue->type, TI_GET_LENGTH, &size) ||
            !types_get_info(&lvalue->type, TI_GET_BASETYPE, &bt))
        {
            WINE_ERR("Couldn't get information\n");
            RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        }
        if (size > sizeof(rtn))
        {
            WINE_ERR("Size too large (%lu)\n", size);
            return 0;
        }
        /* FIXME: we have an ugly & non portable thing here !!! */
        if (!memory_read_value(lvalue, size, &rtn)) return 0;

        /* now let's do some promotions !! */
        switch (bt)
        {
        case btInt:
            /* propagate sign information */
            if (((size & 3) != 0) && (rtn >> (size * 8 - 1)) != 0)
                rtn |= (-1) << (size * 8);
            break;
        case btUInt:
        case btChar:
            break;
        case btFloat:
            RaiseException(DEBUG_STATUS_NOT_AN_INTEGER, 0, 0, NULL);
        }
        break;
    case SymTagPointerType:
        if (!memory_read_value(lvalue, sizeof(void*), &rtn)) return 0;
        break;
    case SymTagArrayType:
    case SymTagUDT:
        assert(lvalue->cookie == DLV_TARGET);
        if (!memory_read_value(lvalue, sizeof(rtn), &rtn)) return 0;
        break;
    case SymTagEnum:
        assert(lvalue->cookie == DLV_TARGET);
        if (!memory_read_value(lvalue, sizeof(rtn), &rtn)) return 0;
        break;
    default:
        WINE_FIXME("Unsupported tag %lu\n", tag);
        rtn = 0;
        break;
    }

    return rtn;
}

/******************************************************************
 *		types_deref
 *
 */
BOOL types_deref(const struct dbg_lvalue* lvalue, struct dbg_lvalue* result)
{
    DWORD       tag;

    memset(result, 0, sizeof(*result));
    result->type.id = dbg_itype_none;
    result->type.module = 0;

    /*
     * Make sure that this really makes sense.
     */
    if (!types_get_info(&lvalue->type, TI_GET_SYMTAG, &tag) ||
        tag != SymTagPointerType ||
        !memory_read_value(lvalue, sizeof(result->addr.Offset), &result->addr.Offset) ||
        !types_get_info(&lvalue->type, TI_GET_TYPE, &result->type.id))
        return FALSE;
    result->type.module = lvalue->type.module;
    result->cookie = DLV_TARGET;
    /* FIXME: this is currently buggy.
     * there is no way to tell were the deref:ed value is...
     * for example:
     *	x is a pointer to struct s, x being on the stack
     *		=> lvalue is in debuggee, result is in debugger
     *	x is a pointer to struct s, x being optimized into a reg
     *		=> lvalue is debugger, result is debuggee
     *	x is a pointer to internal variable x
     *	       	=> lvalue is debugger, result is debuggee
     * so we force debuggee address space, because dereferencing pointers to
     * internal variables is very unlikely. A correct fix would be
     * rather large.
     */
    result->addr.Mode = AddrModeFlat;
    return TRUE;
}

/******************************************************************
 *		types_get_udt_element_lvalue
 *
 * Implement a structure derefencement
 */
static BOOL types_get_udt_element_lvalue(struct dbg_lvalue* lvalue, 
                                         const struct dbg_type* type, long int* tmpbuf)
{
    DWORD       offset, length, bitoffset;
    DWORD       bt;
    unsigned    mask;

    types_get_info(type, TI_GET_TYPE, &lvalue->type.id);
    lvalue->type.module = type->module;
    if (!types_get_info(type, TI_GET_OFFSET, &offset)) return FALSE;
    lvalue->addr.Offset += offset;

    if (types_get_info(type, TI_GET_BITPOSITION, &bitoffset))
    {
        types_get_info(type, TI_GET_LENGTH, &length);
        /* FIXME: this test isn't sufficient, depending on start of bitfield
         * (ie a 32 bit field can spread across 5 bytes)
         */
        if (length > 8 * sizeof(*tmpbuf)) return FALSE;
        lvalue->addr.Offset += bitoffset >> 3;
        /*
         * Bitfield operation.  We have to extract the field and store
         * it in a temporary buffer so that we get it all right.
         */
        if (!memory_read_value(lvalue, sizeof(*tmpbuf), tmpbuf)) return FALSE;
        mask = 0xffffffff << length;
        *tmpbuf >>= bitoffset & 7;
        *tmpbuf &= ~mask;

        lvalue->cookie      = DLV_HOST;
        lvalue->addr.Offset = (DWORD)tmpbuf;

        /*
         * OK, now we have the correct part of the number.
         * Check to see whether the basic type is signed or not, and if so,
         * we need to sign extend the number.
         */
        if (types_get_info(&lvalue->type, TI_GET_BASETYPE, &bt) && 
            bt == btInt && (*tmpbuf & (1 << (length - 1))))
        {
            *tmpbuf |= mask;
        }
    }
    else
    {
        if (!memory_read_value(lvalue, sizeof(*tmpbuf), tmpbuf)) return FALSE;

    }
    return TRUE;
}

/******************************************************************
 *		types_udt_find_element
 *
 */
BOOL types_udt_find_element(struct dbg_lvalue* lvalue, const char* name, long int* tmpbuf)
{
    DWORD                       tag, count;
    char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
    TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
    WCHAR*                      ptr;
    char                        tmp[256];
    int                         i;
    struct dbg_type             type;

    if (!types_get_info(&lvalue->type, TI_GET_SYMTAG, &tag) ||
        tag != SymTagUDT)
        return FALSE;

    if (types_get_info(&lvalue->type, TI_GET_CHILDRENCOUNT, &count))
    {
        fcp->Start = 0;
        while (count)
        {
            fcp->Count = min(count, 256);
            if (types_get_info(&lvalue->type, TI_FINDCHILDREN, fcp))
            {
                type.module = lvalue->type.module;
                for (i = 0; i < min(fcp->Count, count); i++)
                {
                    ptr = NULL;
                    type.id = fcp->ChildId[i];
                    types_get_info(&type, TI_GET_SYMNAME, &ptr);
                    if (!ptr) continue;
                    WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                    HeapFree(GetProcessHeap(), 0, ptr);
                    if (strcmp(tmp, name)) continue;

                    return types_get_udt_element_lvalue(lvalue, &type, tmpbuf);
                }
            }
            count -= min(count, 256);
            fcp->Start += 256;
        }
    }
    return FALSE;
}

/******************************************************************
 *		types_array_index
 *
 * Grab an element from an array
 */
BOOL types_array_index(const struct dbg_lvalue* lvalue, int index, 
                       struct dbg_lvalue* result)
{
    DWORD       tag, length, count;

    if (!types_get_info(&lvalue->type, TI_GET_SYMTAG, &tag))
        return FALSE;
    switch (tag)
    {
    case SymTagArrayType:
        types_get_info(&lvalue->type, TI_GET_COUNT, &count);
        if (index < 0 || index >= count) return FALSE;
        /* fall through */
    case SymTagPointerType:
        /* Contents of array share same data (addr mode, module...) */
        *result = *lvalue;
        /*
         * Get the base type, so we know how much to index by.
         */
        types_get_info(&lvalue->type, TI_GET_TYPE, &result->type.id);
        types_get_info(&result->type, TI_GET_LENGTH, &length);
        memory_read_value(lvalue, sizeof(result->addr.Offset), &result->addr.Offset);
        result->addr.Offset += index * length;
        break;
    default:
        assert(FALSE);
    }
    return TRUE;
}

struct type_find_t
{
    unsigned long       result; /* out: the found type */
    enum SymTagEnum     tag;    /* in: the tag to look for */
    union
    {
        unsigned long           typeid; /* when tag is SymTagUDT */
        const char*             name;   /* when tag is SymTagPointerType */
    } u;
};

static BOOL CALLBACK types_cb(PSYMBOL_INFO sym, ULONG size, void* _user)
{
    struct type_find_t* user = (struct type_find_t*)_user;
    BOOL                ret = TRUE;
    struct dbg_type     type;
    DWORD               type_id;

    if (sym->Tag == user->tag)
    {
        switch (user->tag)
        {
        case SymTagUDT:
            if (!strcmp(user->u.name, sym->Name))
            {
                user->result = sym->TypeIndex;
                ret = FALSE;
            }
            break;
        case SymTagPointerType:
            type.module = sym->ModBase;
            type.id = sym->TypeIndex;
            if (types_get_info(&type, TI_GET_TYPE, &type_id) && type_id == user->u.typeid)
            {
                user->result = sym->TypeIndex;
                ret = FALSE;
            }
            break;
        default: break;
        }
    }
    return ret;
}

/******************************************************************
 *		types_find_pointer
 *
 * Should look up in module based at linear whether (typeid*) exists
 * Otherwise, we could create it locally
 */
struct dbg_type types_find_pointer(const struct dbg_type* type)
{
    struct type_find_t  f;
    struct dbg_type     ret;

    f.result = dbg_itype_none;
    f.tag = SymTagPointerType;
    f.u.typeid = type->id;
    SymEnumTypes(dbg_curr_process->handle, type->module, types_cb, &f);
    ret.module = type->module;
    ret.id = f.result;
    return ret;
}

/******************************************************************
 *		types_find_type
 *
 * Should look up in the module based at linear address whether a type
 * named 'name' and with the correct tag exists
 */
struct dbg_type types_find_type(unsigned long linear, const char* name, enum SymTagEnum tag)

{
    struct type_find_t  f;
    struct dbg_type     ret;

    f.result = dbg_itype_none;
    f.tag = tag;
    f.u.name = name;
    SymEnumTypes(dbg_curr_process->handle, linear, types_cb, &f);
    ret.module = linear;
    ret.id = f.result;
    return ret;
}

/***********************************************************************
 *           print_value
 *
 * Implementation of the 'print' command.
 */
void print_value(const struct dbg_lvalue* lvalue, char format, int level)
{
    struct dbg_lvalue   lvalue_field;
    int		        i;
    DWORD               tag;
    DWORD               count;
    DWORD               size;

    if (lvalue->type.id == dbg_itype_none)
    {
        /* No type, just print the addr value */
        print_bare_address(&lvalue->addr);
        goto leave;
    }

    if (format == 'i' || format == 's' || format == 'w' || format == 'b' || format == 'g')
    {
        dbg_printf("Format specifier '%c' is meaningless in 'print' command\n", format);
        format = '\0';
    }

    if (!types_get_info(&lvalue->type, TI_GET_SYMTAG, &tag))
    {
        WINE_FIXME("---error\n");
        return;
    }
    switch (tag)
    {
    case SymTagBaseType:
    case SymTagEnum:
    case SymTagPointerType:
        print_basic(lvalue, 1, format);
        break;
    case SymTagUDT:
        if (types_get_info(&lvalue->type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            WCHAR*                      ptr;
            char                        tmp[256];
            long int                    tmpbuf;
            struct dbg_type             type;

            dbg_printf("{");
            fcp->Start = 0;
            while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(&lvalue->type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        ptr = NULL;
                        type.module = lvalue->type.module;
                        type.id = fcp->ChildId[i];
                        types_get_info(&type, TI_GET_SYMNAME, &ptr);
                        if (!ptr) continue;
                        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                        dbg_printf("%s=", tmp);
                        HeapFree(GetProcessHeap(), 0, ptr);
                        lvalue_field = *lvalue;
                        if (types_get_udt_element_lvalue(&lvalue_field, &type, &tmpbuf))
                        {
                            print_value(&lvalue_field, format, level + 1);
                        }
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
            dbg_printf("}");
        }
        break;
    case SymTagArrayType:
        /*
         * Loop over all of the entries, printing stuff as we go.
         */
        count = 1; size = 1;
        types_get_info(&lvalue->type, TI_GET_COUNT, &count);
        types_get_info(&lvalue->type, TI_GET_LENGTH, &size);

        if (size == count)
	{
            unsigned    len;
            char        buffer[256];
            /*
             * Special handling for character arrays.
             */
            /* FIXME should check basic type here (should be a char!!!!)... */
            len = min(count, sizeof(buffer));
            memory_get_string(dbg_curr_process,
                              memory_to_linear_addr(&lvalue->addr),
                              lvalue->cookie == DLV_TARGET, TRUE, buffer, len);
            dbg_printf("\"%s%s\"", buffer, (len < count) ? "..." : "");
            break;
        }
        lvalue_field = *lvalue;
        types_get_info(&lvalue->type, TI_GET_TYPE, &lvalue_field.type.id);
        dbg_printf("{");
        for (i = 0; i < count; i++)
	{
            print_value(&lvalue_field, format, level + 1);
            lvalue_field.addr.Offset += size / count;
            dbg_printf((i == count - 1) ? "}" : ", ");
	}
        break;
    case SymTagFunctionType:
        dbg_printf("Function ");
        print_bare_address(&lvalue->addr);
        dbg_printf(": ");
        types_print_type(&lvalue->type, FALSE);
        break;
    default:
        WINE_FIXME("Unknown tag (%lu)\n", tag);
        RaiseException(DEBUG_STATUS_INTERNAL_ERROR, 0, 0, NULL);
        break;
    }

leave:

    if (level == 0) dbg_printf("\n");
}

static BOOL CALLBACK print_types_cb(PSYMBOL_INFO sym, ULONG size, void* ctx)
{
    struct dbg_type     type;
    type.module = sym->ModBase;
    type.id = sym->TypeIndex;
    dbg_printf("Mod: %08lx ID: %08lx \n", type.module, type.id);
    types_print_type(&type, TRUE);
    dbg_printf("\n");
    return TRUE;
}

static BOOL CALLBACK print_types_mod_cb(PSTR mod_name, DWORD base, void* ctx)
{
    return SymEnumTypes(dbg_curr_process->handle, base, print_types_cb, ctx);
}

int print_types(void)
{
    SymEnumerateModules(dbg_curr_process->handle, print_types_mod_cb, NULL);
    return 0;
}

int types_print_type(const struct dbg_type* type, BOOL details)
{
    WCHAR*              ptr;
    char                tmp[256];
    const char*         name;
    DWORD               tag, udt, count;
    struct dbg_type     subtype;

    if (type->id == dbg_itype_none || !types_get_info(type, TI_GET_SYMTAG, &tag))
    {
        dbg_printf("--invalid--<%lxh>--", type->id);
        return FALSE;
    }

    if (types_get_info(type, TI_GET_SYMNAME, &ptr) && ptr)
    {
        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
        name = tmp;
        HeapFree(GetProcessHeap(), 0, ptr);
    }
    else name = "--none--";

    switch (tag)
    {
    case SymTagBaseType:
        if (details) dbg_printf("Basic<%s>", name); else dbg_printf("%s", name);
        break;
    case SymTagPointerType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        subtype.module = type->module;
        types_print_type(&subtype, FALSE);
        dbg_printf("*");
        break;
    case SymTagUDT:
        types_get_info(type, TI_GET_UDTKIND, &udt);
        switch (udt)
        {
        case UdtStruct: dbg_printf("struct %s", name); break;
        case UdtUnion:  dbg_printf("union %s", name); break;
        case UdtClass:  dbg_printf("class %s", name); break;
        default:        WINE_ERR("Unsupported UDT type (%ld) for %s", udt, name); break;
        }
        if (details &&
            types_get_info(type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            WCHAR*                      ptr;
            char                        tmp[256];
            int                         i;
            struct dbg_type             type_elt;
            dbg_printf(" {");

            fcp->Start = 0;
            while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        ptr = NULL;
                        type_elt.module = type->module;
                        type_elt.id = fcp->ChildId[i];
                        types_get_info(&type_elt, TI_GET_SYMNAME, &ptr);
                        if (!ptr) continue;
                        WideCharToMultiByte(CP_ACP, 0, ptr, -1, tmp, sizeof(tmp), NULL, NULL);
                        HeapFree(GetProcessHeap(), 0, ptr);
                        dbg_printf("%s", tmp);
                        if (types_get_info(&type_elt, TI_GET_TYPE, &type_elt.id))
                        {
                            dbg_printf(":");
                            types_print_type(&type_elt, details);
                        }
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
            dbg_printf("}");
        }
        break;
    case SymTagArrayType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        subtype.module = type->module;
        types_print_type(&subtype, details);
        dbg_printf(" %s[]", name);
        break;
    case SymTagEnum:
        dbg_printf("enum %s", name);
        break;
    case SymTagFunctionType:
        types_get_info(type, TI_GET_TYPE, &subtype.id);
        subtype.module = type->module;
        types_print_type(&subtype, FALSE);
        dbg_printf(" (*%s)(", name);
        if (types_get_info(type, TI_GET_CHILDRENCOUNT, &count))
        {
            char                        buffer[sizeof(TI_FINDCHILDREN_PARAMS) + 256 * sizeof(DWORD)];
            TI_FINDCHILDREN_PARAMS*     fcp = (TI_FINDCHILDREN_PARAMS*)buffer;
            int                         i;

            fcp->Start = 0;
            while (count)
            {
                fcp->Count = min(count, 256);
                if (types_get_info(type, TI_FINDCHILDREN, fcp))
                {
                    for (i = 0; i < min(fcp->Count, count); i++)
                    {
                        subtype.id = fcp->ChildId[i];
                        types_print_type(&subtype, FALSE);
                        if (i < min(fcp->Count, count) - 1 || count > 256) dbg_printf(", ");
                    }
                }
                count -= min(count, 256);
                fcp->Start += 256;
            }
        }
        dbg_printf(")");
        break;
    default:
        WINE_ERR("Unknown type %lu for %s\n", tag, name);
        break;
    }
    
    return TRUE;
}

BOOL types_get_info(const struct dbg_type* type, IMAGEHLP_SYMBOL_TYPE_INFO ti, void* pInfo)
{
    if (type->id == dbg_itype_none) return FALSE;
    if (type->module != 0)
        return SymGetTypeInfo(dbg_curr_process->handle, type->module, type->id, ti, pInfo);

    assert(type->id >= dbg_itype_first);
/* helper to typecast pInfo to its expected type (_t) */
#define X(_t) (*((_t*)pInfo))

    switch (type->id)
    {
    case dbg_itype_unsigned_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD) = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 4; break;
        case TI_GET_BASETYPE:   X(DWORD) = btInt; break;
        default: WINE_FIXME("unsupported %u for s-int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_unsigned_short_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 2; break;
        case TI_GET_BASETYPE:   X(DWORD) = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-short int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_short_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 2; break;
        case TI_GET_BASETYPE:   X(DWORD) = btInt; break;
        default: WINE_FIXME("unsupported %u for s-short int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_unsigned_char_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD) = btUInt; break;
        default: WINE_FIXME("unsupported %u for u-char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_signed_char_int:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD) = btInt; break;
        default: WINE_FIXME("unsupported %u for s-char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_char:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagBaseType; break;
        case TI_GET_LENGTH:     X(DWORD) = 1; break;
        case TI_GET_BASETYPE:   X(DWORD) = btChar; break;
        default: WINE_FIXME("unsupported %u for char int\n", ti); return FALSE;
        }
        break;
    case dbg_itype_astring:
        switch (ti)
        {
        case TI_GET_SYMTAG:     X(DWORD) = SymTagPointerType; break;
        case TI_GET_LENGTH:     X(DWORD) = 4; break;
        case TI_GET_TYPE:       X(DWORD) = dbg_itype_char; break;
        default: WINE_FIXME("unsupported %u for a string\n", ti); return FALSE;
        }
        break;
    default: WINE_FIXME("unsupported type id 0x%lx\n", type->id);
    }

#undef X
    return TRUE;
}
