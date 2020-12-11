/*
 * Locale definitions
 *
 * Copyright 2000 Francois Gouget.
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
#ifndef __WINE_LOCALE_H
#define __WINE_LOCALE_H
#ifndef __WINE_USE_MSVCRT
#define __WINE_USE_MSVCRT
#endif

#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
#ifndef __cplusplus
typedef unsigned short wchar_t;
#endif
#endif

#define LC_ALL                 0
#define LC_COLLATE             1
#define LC_CTYPE               2
#define LC_MONETARY            3
#define LC_NUMERIC             4
#define LC_TIME                5
#define LC_MIN                 LC_ALL
#define LC_MAX                 LC_TIME

#ifndef _LCONV_DEFINED
#define _LCONV_DEFINED
struct lconv
{
    char* decimal_point;
    char* thousands_sep;
    char* grouping;
    char* int_curr_symbol;
    char* currency_symbol;
    char* mon_decimal_point;
    char* mon_thousands_sep;
    char* mon_grouping;
    char* positive_sign;
    char* negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
};
#endif /* _LCONV_DEFINED */


#ifdef __cplusplus
extern "C" {
#endif

char*       setlocale(int,const char*);
struct lconv* localeconv(void);

#ifndef _WLOCALE_DEFINED
#define _WLOCALE_DEFINED
wchar_t* _wsetlocale(int,const wchar_t*);
#endif /* _WLOCALE_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* __WINE_LOCALE_H */