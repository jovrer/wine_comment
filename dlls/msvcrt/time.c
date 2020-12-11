/*
 * msvcrt.dll date/time functions
 *
 * Copyright 1996,1998 Marcus Meissner
 * Copyright 1996 Jukka Iivonen
 * Copyright 1997,2000 Uwe Bonnes
 * Copyright 2000 Jon Griffiths
 * Copyright 2004 Hans Leidekker
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

#include <time.h>
#ifdef HAVE_SYS_TIMES_H
# include <sys/times.h>
#endif
#include <limits.h>

#include "msvcrt.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvcrt);

static const int MonthLengths[2][12] =
{
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
    { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static inline int IsLeapYear(int Year)
{
    return Year % 4 == 0 && (Year % 100 != 0 || Year % 400 == 0);
}

#define SECSPERDAY        86400
/* 1601 to 1970 is 369 years plus 89 leap days */
#define SECS_1601_TO_1970  ((369 * 365 + 89) * (ULONGLONG)SECSPERDAY)
#define TICKSPERSEC       10000000
#define TICKSPERMSEC      10000
#define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)

/* native uses a single static buffer for localtime/gmtime/mktime */
static struct MSVCRT_tm tm;

/**********************************************************************
 *		mktime (MSVCRT.@)
 */
MSVCRT_time_t MSVCRT_mktime(struct MSVCRT_tm *t)
{
    MSVCRT_time_t secs;
    FILETIME lft, uft;
    ULONGLONG time;
    struct MSVCRT_tm ts;
    int cleaps, day;

    ts=*t;
    /* to prevent arithmetic overflows put constraints on some fields */
    /* whether the effective date falls in the 1970-2038 time period */
    /* will be tested later */
    /* BTW, I have no idea what limits native msvcrt has. */
    if ( ts.tm_year < 0 || ts.tm_year > 140  ||
            ts.tm_mon < -840 || ts.tm_mon > 840 ||
            ts.tm_mday < -20160 || ts.tm_mday > 20160 ||
            ts.tm_hour < -484000 || ts.tm_hour > 484000 ||
            ts.tm_min < -29000000 || ts.tm_min > 29000000 )
        return -1;
           
    /* normalize the tm month fields */
    if( ts.tm_mon > 11) { ts.tm_year += ts.tm_mon / 12; ts.tm_mon %= 12; }
    if( ts.tm_mon < 0) {
        int dy = (11 - ts.tm_mon) / 12;
        ts.tm_year -= dy;
        ts.tm_mon += dy * 12;
    }
    /* now calculate a day count from the date
     * First start counting years from March. This way the leap days
     * are added at the end of the year, not somewhere in the middle.
     * Formula's become so much less complicate that way.
     * To convert: add 12 to the month numbers of Jan and Feb, and 
     * take 1 from the year */
    if(ts.tm_mon < 2) {
        ts.tm_mon += 14;
        ts.tm_year += 1899;
    } else {
        ts.tm_mon += 2;
        ts.tm_year += 1900;
    }
    cleaps = (3 * (ts.tm_year / 100) + 3) / 4;   /* nr of "century leap years"*/
    day =  (36525 * ts.tm_year) / 100 - cleaps + /* year * dayperyr, corrected*/
             (1959 * ts.tm_mon) / 64 +    /* months * daypermonth */
             ts.tm_mday -                 /* day of the month */
             584817 ;                     /* zero that on 1601-01-01 */
    /* done */

    /* convert to 100 ns ticks */
    time = ((((ULONGLONG) day * 24 +
            ts.tm_hour) * 60 +
            ts.tm_min) * 60 +
            ts.tm_sec ) * TICKSPERSEC;
    
    lft.dwHighDateTime = (DWORD) (time >> 32);
    lft.dwLowDateTime = (DWORD) time;

    LocalFileTimeToFileTime(&lft, &uft);

    time = ((ULONGLONG)uft.dwHighDateTime << 32) | uft.dwLowDateTime;
    time /= TICKSPERSEC;
    if( time < SECS_1601_TO_1970 || time > (SECS_1601_TO_1970 + INT_MAX))
        return -1;
    secs = time - SECS_1601_TO_1970;
    /* compute tm_wday, tm_yday and renormalize the other fields of the
     * tm structure */
    if( MSVCRT_localtime( &secs)) *t = tm;

    return secs; 
}

/*********************************************************************
 *      localtime (MSVCRT.@)
 */
struct MSVCRT_tm* MSVCRT_localtime(const MSVCRT_time_t* secs)
{
  int i;

  FILETIME ft, lft;
  SYSTEMTIME st;
  DWORD tzid;
  TIME_ZONE_INFORMATION tzinfo;

  ULONGLONG time = *secs * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;

  ft.dwHighDateTime = (UINT)(time >> 32);
  ft.dwLowDateTime  = (UINT)time;

  FileTimeToLocalFileTime(&ft, &lft);
  FileTimeToSystemTime(&lft, &st);

  if (st.wYear < 1970) return NULL;

  tm.tm_sec  = st.wSecond;
  tm.tm_min  = st.wMinute;
  tm.tm_hour = st.wHour;
  tm.tm_mday = st.wDay;
  tm.tm_year = st.wYear - 1900;
  tm.tm_mon  = st.wMonth  - 1;
  tm.tm_wday = st.wDayOfWeek;

  for (i = tm.tm_yday = 0; i < st.wMonth - 1; i++) {
    tm.tm_yday += MonthLengths[IsLeapYear(st.wYear)][i];
  }

  tm.tm_yday += st.wDay - 1;
 
  tzid = GetTimeZoneInformation(&tzinfo);

  if (tzid == TIME_ZONE_ID_INVALID)
    tm.tm_isdst = -1;
  else 
    tm.tm_isdst = (tzid == TIME_ZONE_ID_DAYLIGHT?1:0);

  return &tm;
}

struct MSVCRT_tm* MSVCRT_gmtime(const MSVCRT_time_t* secs)
{
  int i;

  FILETIME ft;
  SYSTEMTIME st;

  ULONGLONG time = *secs * (ULONGLONG)TICKSPERSEC + TICKS_1601_TO_1970;

  ft.dwHighDateTime = (UINT)(time >> 32);
  ft.dwLowDateTime  = (UINT)time;

  FileTimeToSystemTime(&ft, &st);

  if (st.wYear < 1970) return NULL;

  tm.tm_sec  = st.wSecond;
  tm.tm_min  = st.wMinute;
  tm.tm_hour = st.wHour;
  tm.tm_mday = st.wDay;
  tm.tm_year = st.wYear - 1900;
  tm.tm_mon  = st.wMonth - 1;
  tm.tm_wday = st.wDayOfWeek;
  for (i = tm.tm_yday = 0; i < st.wMonth - 1; i++) {
    tm.tm_yday += MonthLengths[IsLeapYear(st.wYear)][i];
  }

  tm.tm_yday += st.wDay - 1;
  tm.tm_isdst = 0;

  return &tm;
}

/**********************************************************************
 *		_strdate (MSVCRT.@)
 */
char* _strdate(char* date)
{
  LPCSTR format = "MM'/'dd'/'yy";

  GetDateFormatA(LOCALE_NEUTRAL, 0, NULL, format, date, 9);

  return date;
}

/**********************************************************************
 *		_wstrdate (MSVCRT.@)
 */
MSVCRT_wchar_t* _wstrdate(MSVCRT_wchar_t* date)
{
  static const WCHAR format[] = { 'M','M','\'','/','\'','d','d','\'','/','\'','y','y',0 };

  GetDateFormatW(LOCALE_NEUTRAL, 0, NULL, format, (LPWSTR)date, 9);

  return date;
}

/*********************************************************************
 *		_strtime (MSVCRT.@)
 */
char* _strtime(char* time)
{
  LPCSTR format = "HH':'mm':'ss";

  GetTimeFormatA(LOCALE_NEUTRAL, 0, NULL, format, time, 9); 

  return time;
}

/*********************************************************************
 *		_wstrtime (MSVCRT.@)
 */
MSVCRT_wchar_t* _wstrtime(MSVCRT_wchar_t* time)
{
  static const WCHAR format[] = { 'H','H','\'',':','\'','m','m','\'',':','\'','s','s',0 };

  GetTimeFormatW(LOCALE_NEUTRAL, 0, NULL, format, (LPWSTR)time, 9);

  return time;
}

/*********************************************************************
 *		clock (MSVCRT.@)
 */
MSVCRT_clock_t MSVCRT_clock(void)
{
  FILETIME ftc, fte, ftk, ftu;
  ULONGLONG utime, ktime;
 
  MSVCRT_clock_t clock;

  GetProcessTimes(GetCurrentProcess(), &ftc, &fte, &ftk, &ftu);

  ktime = ((ULONGLONG)ftk.dwHighDateTime << 32) | ftk.dwLowDateTime;
  utime = ((ULONGLONG)ftu.dwHighDateTime << 32) | ftu.dwLowDateTime;

  clock = (utime + ktime) / (TICKSPERSEC / MSVCRT_CLOCKS_PER_SEC);

  return clock;
}

/*********************************************************************
 *		difftime (MSVCRT.@)
 */
double MSVCRT_difftime(MSVCRT_time_t time1, MSVCRT_time_t time2)
{
  return (double)(time1 - time2);
}

/*********************************************************************
 *		_ftime (MSVCRT.@)
 */
void _ftime(struct MSVCRT__timeb *buf)
{
  TIME_ZONE_INFORMATION tzinfo;
  FILETIME ft;
  ULONGLONG time;

  DWORD tzid = GetTimeZoneInformation(&tzinfo);
  GetSystemTimeAsFileTime(&ft);

  time = ((ULONGLONG)ft.dwHighDateTime << 32) | ft.dwLowDateTime;

  buf->time = time / TICKSPERSEC - SECS_1601_TO_1970;
  buf->millitm = (time % TICKSPERSEC) / TICKSPERMSEC;
  buf->timezone = tzinfo.Bias +
      ( tzid == TIME_ZONE_ID_STANDARD ? tzinfo.StandardBias :
      ( tzid == TIME_ZONE_ID_DAYLIGHT ? tzinfo.DaylightBias : 0 ));
  buf->dstflag = (tzid == TIME_ZONE_ID_DAYLIGHT?1:0);
}

/*********************************************************************
 *		time (MSVCRT.@)
 */
MSVCRT_time_t MSVCRT_time(MSVCRT_time_t* buf)
{
  MSVCRT_time_t curtime;
  struct MSVCRT__timeb tb;

  _ftime(&tb);

  curtime = tb.time;
  return buf ? *buf = curtime : curtime;
}

/*********************************************************************
 *		_daylight (MSVCRT.@)
 */
int MSVCRT___daylight = 0;

/*********************************************************************
 *		__p_daylight (MSVCRT.@)
 */
int *MSVCRT___p__daylight(void)
{
	return &MSVCRT___daylight;
}

/*********************************************************************
 *		_dstbias (MSVCRT.@)
 */
int MSVCRT__dstbias = 0;

/*********************************************************************
 *		__p_dstbias (MSVCRT.@)
 */
int *__p__dstbias(void)
{
    return &MSVCRT__dstbias;
}

/*********************************************************************
 *		_timezone (MSVCRT.@)
 */
long MSVCRT___timezone = 0;

/*********************************************************************
 *		__p_timezone (MSVCRT.@)
 */
long *MSVCRT___p__timezone(void)
{
	return &MSVCRT___timezone;
}

/*********************************************************************
 *		_tzname (MSVCRT.@)
 * NOTES
 *  Some apps (notably Mozilla) insist on writing to these, so the buffer
 *  must be large enough.  The size is picked based on observation of
 *  Windows XP.
 */
static char tzname_std[64] = "";
static char tzname_dst[64] = "";
char *MSVCRT__tzname[2] = { tzname_std, tzname_dst };

/*********************************************************************
 *		__p_tzname (MSVCRT.@)
 */
char **__p__tzname(void)
{
	return MSVCRT__tzname;
}

/*********************************************************************
 *		_tzset (MSVCRT.@)
 */
void MSVCRT__tzset(void)
{
    tzset();
#if defined(HAVE_TIMEZONE) && defined(HAVE_DAYLIGHT)
    MSVCRT___daylight = daylight;
    MSVCRT___timezone = timezone;
#else
    {
        static const time_t seconds_in_year = (365 * 24 + 6) * 3600;
        time_t t;
        struct tm *tmp;
        long zone_january, zone_july;

        t = (time((time_t *)0) / seconds_in_year) * seconds_in_year;
        tmp = localtime(&t);
        zone_january = -tmp->tm_gmtoff;
        t += seconds_in_year / 2;
        tmp = localtime(&t);
        zone_july = -tmp->tm_gmtoff;
        MSVCRT___daylight = (zone_january != zone_july);
        MSVCRT___timezone = max(zone_january, zone_july);
    }
#endif
    lstrcpynA(tzname_std, tzname[0], sizeof(tzname_std));
    tzname_std[sizeof(tzname_std) - 1] = '\0';
    lstrcpynA(tzname_dst, tzname[1], sizeof(tzname_dst));
    tzname_dst[sizeof(tzname_dst) - 1] = '\0';
}
