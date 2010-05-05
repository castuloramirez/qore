/* -*- mode: c++; indent-tabs-mode: nil -*- */
/*
  qore_date_private.h

  DateTime private implementation

  Qore Programming Language

  Copyright 2003 - 2010 David Nichols

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef QORE_QORE_DATE_PRIVATE_H
#define QORE_QORE_DATE_PRIVATE_H 

#include <math.h>

// note: this implementation does not yet take into account leap seconds,
//       even if this information is available in the zoneinfo data

#define SECS_PER_MINUTE          60
// 3600
#define SECS_PER_HOUR            (SECS_PER_MINUTE * 60)
// number of seconds in a normal day (no DST) = 86400
#define SECS_PER_DAY             (SECS_PER_HOUR * 24)
// number of seconds in a normal year (no leap day)
#define SECS_PER_YEAR            (SECS_PER_DAY * 365ll)
// number of seconds in a leap year
#define SECS_PER_LEAP_YEAR       (SECS_PER_YEAR + SECS_PER_DAY)

#define MICROSECS_PER_SEC        1000000ll
#define MICROSECS_PER_MINUTE     (MICROSECS_PER_SEC * 60)
#define MICROSECS_PER_HOUR       (MICROSECS_PER_MINUTE * 60)
// number of microseconds in a day (no DST)
#define MICROSECS_PER_DAY        (MICROSECS_PER_HOUR * 24)
// number of microseconds in an average month (30 days)
#define MICROSECS_PER_AVG_MONTH  (MICROSECS_PER_HOUR * 24)
// number of microseconds in a year
#define MICROSECS_PER_YEAR       (MICROSECS_PER_DAY * 365)

// number of seconds from 1970-01-01 to 2000-01-01, 30 years with 7 leap days: 1972, 1976, 1980, 1984, 1988, 1992, 1996
#define SECS_TO_2K               (SECS_PER_YEAR * 30 + SECS_PER_DAY * 7ll)

// number of seconds from 1970-01-01 to 2000-03-01, with 8 leap days: 1972, 1976, 1980, 1984, 1988, 1992, 1996, 2000
#define SECS_TO_2KLD             (SECS_PER_YEAR * 30 + SECS_PER_DAY * (7ll + 60ll))

// there are 97 leap days every 400 years
#define SECS_IN_400_YEARS        (SECS_PER_YEAR * 400 + SECS_PER_DAY * 97ll)
// there are 24 leap days every 100 years
#define SECS_IN_100_YEARS        (SECS_PER_YEAR * 100 + SECS_PER_DAY * 24ll)
// there is 1 leap day every 4 years
#define SECS_IN_4_YEARS          (SECS_PER_YEAR * 4 + SECS_PER_DAY)

// second offset in year for start of leap day (either 03-01 or 02-29)
#define LEAPDAY_OFFSET           (SECS_PER_DAY * 59)

#define SECS_AFTER_LD            (SECS_PER_DAY * 306)

template <typename T1, typename T2>
DLLLOCAL void normalize_units(T1 &bigger, T2 &smaller, int ratio) {
   if (smaller <= -ratio || smaller >= ratio) {
      int64 units = smaller / ratio;
      bigger += units;
      smaller -= units * ratio;
   }

   // perform further sign normalization; ensure signs are the same
   if (bigger > 0) {
      if (smaller < 0) {
         smaller += ratio;
         --bigger;
      }
   }
   else if (bigger < 0 && smaller > 0) {
      smaller -= ratio;
      ++bigger;
   }
}

// normalize so that the smaller units are always positive
template <typename T1, typename T2>
DLLLOCAL void normalize_units2(T1 &bigger, T2 &smaller, int ratio) {
   if (smaller <= -ratio || smaller >= ratio) {
      int64 units = smaller / ratio;
      bigger += units;
      smaller -= units * ratio;
   }

   // perform further sign normalization
   if (smaller < 0) {
      smaller += ratio;
      --bigger;
   }
}

// normalize with second unsigned
template <typename T1>
DLLLOCAL void normalize_units3(T1 &bigger, unsigned &smaller, unsigned ratio) {
   if (smaller >= ratio) {
      int64 units = smaller / ratio;
      bigger += units;
      smaller -= units * ratio;
   }
}

struct qore_date_info {
   // static constants
   DLLLOCAL static const int month_lengths[];
   // for calculating the days passed in a year
   DLLLOCAL static const int positive_months[];
   DLLLOCAL static const int negative_months[];

   DLLLOCAL static bool isLeapYear(int year);

   // returns the year and the positive number of seconds from the beginning
   // of the year (even for dates before 1970)
   // we calculate the based on an offset from a known date, 2000-03-01,
   // because the leap day calculations are regular from that point, as
   // this date marks the start of a 400-year cycle, being right after the
   // last leap day of the previous 400-year cycle
   DLLLOCAL static void get_epoch_year(int64 &epoch, int &year, bool &ly) {
      // get second offset from 2000-03-01
      epoch -= SECS_TO_2KLD;

      // how many 400-year periods are we off of 2000-03-01
      int64 mult = epoch / SECS_IN_400_YEARS;
      // remaining seconds
      epoch %= SECS_IN_400_YEARS;

      // if year is an even multiple of 400
      if (!epoch) {
         epoch = LEAPDAY_OFFSET + SECS_PER_DAY;
         year = mult * 400 + 2000;
         ly = true;
         return;
      }

      // make sure second offset is positive
      if (epoch < 0) {
         --mult; 
         epoch += SECS_IN_400_YEARS;
      }

      // year offset
      int yo = 0;

      // get the number of 100-year remaining periods (24 leap days each)
      int64 d = epoch / SECS_IN_100_YEARS;
      if (d) {
         // there can be max 3 100-year periods
         // if the time is in the extra leap day for the 400=year cycle, 
         // then 4 could be returned
         if (d == 4)
            d = 3;
         epoch -= d * SECS_IN_100_YEARS;
         yo = 100 * d;
      }

      //printd(5, "qore_date_info::get_epoch_year() after 100: epoch: %d (%d from %d) year base: %d\n", epoch, d, SECS_IN_100_YEARS, mult * 400 + 2000 + yo);

      // get the number of 4-year periods remaining (1 leap day each)
      d = epoch / SECS_IN_4_YEARS;
      if (d) {
         epoch %= SECS_IN_4_YEARS;
         yo += d * 4;
      }

      // target date/time is in a leap year if the second offset from the 4-year period 
      // is less than the number of seconds after a leap day
      // or greater than the number of seconds in 4 regular years
      ly = epoch < SECS_AFTER_LD || epoch >= (SECS_PER_YEAR * 4);

      //printd(5, "qore_date_info::get_epoch_year() after 4: epoch: %d (%d from %d) year base: %d (ily=%d)\n", epoch, d, SECS_IN_4_YEARS, mult * 400 + 2000 + yo, ly);

      // get the number of 1-year periods
      d = epoch / SECS_PER_YEAR;
      if (d) {
         // maximum of 3 years
         if (d == 4)
            d = 3;
         epoch -= d * SECS_PER_YEAR;
         yo += d;
      }

      year = mult * 400 + 2000 + yo;

      //printd(5, "qore_date_info::get_epoch_year() after 1: epoch: %d (%d from %d) year base: %d\n", epoch, d, SECS_PER_YEAR, year);

      // check if we are in the current year or the next and align with year start
      // r is currently the offset from YEAR-03-01
      if (epoch >= SECS_AFTER_LD) {
         ++year;
         epoch -= SECS_AFTER_LD;
      }
      else {
         // move offset start of current year
         epoch += LEAPDAY_OFFSET;
         if (ly)
            epoch += SECS_PER_DAY;
      }

      //printd(5, "qore_date_info::get_epoch_year() after adj: epoch: %d year: %d\n", epoch, year);
   }

   // number of leap days from 1970-01-01Z to a certain month and year
   DLLLOCAL static int leap_days_from_epoch(int year, int month) {
      // 1968 was the 477th leap year from year 0 assuming a proleptic gregorian calendar
      int d;
      if (year >= 1970) {
         d = year/4 - year/100 + year/400 - 477;
         if (month < 3 && isLeapYear(year))
            --d;
      }
      else {
         --year;
         d = year/4 - year/100 + year/400 - 477;
         // first leap year before 1970 is 1968
         // adjust for negative leap days
         if (month > 2 && isLeapYear(year + 1))
            ++d;
      }

      return d;
   }
   DLLLOCAL static int getLastDayOfMonth(int month, int year) {
      if (month != 2)
         return qore_date_info::month_lengths[month];
      return qore_date_info::isLeapYear(year) ? 29 : 28;
   }

   DLLLOCAL static int getDayOfWeek(int year, int month, int day) {
      int a = (14 - month) / 12;
      int y = year - a;
      int m = month + 12 * a - 2;
      return (day + y + y / 4 - y / 100 + y / 400 + (31 * m / 12)) % 7;
   }

   // assumes UTC, returns seconds from 1970-01-01Z
   DLLLOCAL static int64 getEpochSeconds(int year, int month, int day) {
      //printd(5, "qore_date_info::getEpochSeconds(year=%d, month=%d, day=%d) leap days to %d: %d\n", year, month, day, year, leap_days_from_epoch(year, month));
      assert(month > 0 && month < 13);

      // calculate seconds
      int64 epoch = (year - 1970) * SECS_PER_YEAR + (positive_months[month - 1] + day - 1 + leap_days_from_epoch(year, month)) * SECS_PER_DAY;

      //printd(5, "qore_date_info::getEpochSeconds(year=%d, month=%d, day=%d) epoch=%lld\n", year, month, day, epoch);
      return epoch;
   }

   // assumes UTC, returns seconds from 1970-01-01Z
   DLLLOCAL static int64 getEpochSeconds(int year, int month, int day, int hour, int minute, int second) {
      int64 secs = getEpochSeconds(year, month, day);

      return secs
         + (int64)hour * 3600
         + (int64)minute * 60
         + (int64)second;
   }

   DLLLOCAL static int getDayNumber(int year, int month, int day) {
      return positive_months[(month < 13 ? month : 12) - 1] + day + (month > 2 && qore_date_info::isLeapYear(year) ? 1 : 0);
   }
};

// normalize the given date to the last day of the month
DLLLOCAL void normalize_dm(int &year, int &month, int &day);

// normalize to the correct day, month, and year
DLLLOCAL void normalize_day(int &year, int &month, int &day);

class qore_relative_time;

DLLLOCAL extern const char *STATIC_UTC;

struct qore_simple_tm {
protected:

public:
   int year;    // year
   int month;   // month
   int day;     // day
   int hour;    // hours
   int minute;  // minutes
   int second;  // seconds
   int us;      // microseconds

   DLLLOCAL void zero() {
      year = 0;
      month = 0;
      day = 0;
      hour = 0;
      minute = 0;
      second = 0;
      us = 0;
   }
   
   DLLLOCAL void set(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
      year = n_year;
      month = n_month;
      day = n_day;
      hour = n_hour;
      minute = n_minute;
      second = n_second;
      us = n_us;
   }

   DLLLOCAL void set(int64 seconds, unsigned my_us) {
      normalize_units3<int64>(seconds, my_us, 1000000);
      us = my_us;

      // leap year flag
      bool ly;

      //printd(5, "qore_simple_tm::set(seconds=%lld, my_us=%d)\n", seconds, my_us);
      qore_date_info::get_epoch_year(seconds, year, ly);

      //printd(5, "qore_simple_tm::set() seconds=%lld year=%d (day=%d, new secs=%lld)\n", seconds, year, seconds / 86400, seconds % 86400);

      day = seconds / SECS_PER_DAY;
      seconds %= SECS_PER_DAY;

      for (month = 1; month < 12; ++month) {
         int ml = qore_date_info::month_lengths[month];
         if (ly && month == 2)
            ml = 29;

         if (ml > day)
            break;

         day -= ml;
      }

      ++day;

      second = seconds;
      hour = second / SECS_PER_HOUR;
      second %= SECS_PER_HOUR;
      minute = second / SECS_PER_MINUTE;
      second %= SECS_PER_MINUTE;
   }

   DLLLOCAL bool hasValue() const {
      return year || month || day || hour || minute || second || us;
   }
};

// for time info
struct qore_time_info : public qore_simple_tm {
   const char *zname;
   int utcoffset;
   bool isdst;
   const AbstractQoreZoneInfo *zone;

   DLLLOCAL void set(int64 epoch, unsigned us, int n_utcoffset, bool n_isdst, const char *n_zname, const AbstractQoreZoneInfo *n_zone) {
      zname = n_zname ? n_zname : STATIC_UTC;
      utcoffset = n_utcoffset;
      isdst = n_isdst;
      zone = n_zone;
      qore_simple_tm::set(epoch + utcoffset, us);
   }

   DLLLOCAL qore_time_info &operator=(const qore_simple_tm &t) {
      zname = STATIC_UTC;
      utcoffset = 0;
      isdst = false;
      zone = 0;
      year = t.year;
      month = t.month;
      day = t.day;
      hour = t.hour;
      minute = t.minute;
      second = t.second;
      us = t.us;

      return *this;
   }

   DLLLOCAL void copyTo(qore_tm &info) {
      info.year          = year;
      info.month         = month;
      info.day           = day;
      info.hour          = hour;
      info.minute        = minute;
      info.second        = second;
      info.us            = us;
      info.zone_name     = zname;
      info.utc_secs_east = utcoffset;
      info.dst           = isdst;
      info.zone          = zone;
   }
};

// with constructors, for use with absolute dates
struct qore_simple_tm2 : public qore_simple_tm {
   DLLLOCAL qore_simple_tm2() {
   }
   DLLLOCAL qore_simple_tm2(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
      set(year, month, day, hour, minute, second, us);
   }
   DLLLOCAL qore_simple_tm2(int64 secs, unsigned my_us) {
      set(secs, my_us);
   }
   DLLLOCAL void setLiteral(int64 date, int usecs) {
      //printd(5, "qore_simple_tm2::setLiteral(date=%lld, usecs=%d)\n", date, usecs);

      year = date / 10000000000ll;
      date -= year * 10000000000ll;
      month = date / 100000000ll;
      date -= month * 100000000ll;
      day = date / 1000000ll;
      date -= day * 1000000ll;
      hour = date / 10000ll; 
      date -= hour * 10000ll;
      minute = date / 100ll;
      second = date - minute * 100ll;
      us = usecs;

      normalize_units2<int, int>(second, us, 1000000);
      normalize_units2<int, int>(minute, second, 60);
      normalize_units2<int, int>(hour, minute, 60);
      normalize_units2<int, int>(day, hour, 24);

      // adjust month and year
      if (month > 12) {
         --month;
         normalize_units2<int, int>(year, month, 12);
         ++month;
      }
      else if (!month)
         month = 1;

      if (!day)
         day = 1;

      // now normalize day
      normalize_day(year, month, day);

      //printd(5, "qore_simple_tm2::setLiteral() %04d-%02d-%02d %02d:%02d:%02d.%06d\n", year, month, day, hour, minute, second, us);
   }
   DLLLOCAL void getISOWeek(int &yr, int &week, int &wday) const;
};

DLLLOCAL void concatOffset(int utcoffset, QoreString &str);

class qore_absolute_time {
   friend class qore_relative_time;
protected:
   int64 epoch;                       // offset in seconds from the epoch (1970-01-01Z)
   unsigned us;                       // microseconds
   const AbstractQoreZoneInfo *zone;  // time zone region

   // epoch is set to local time; needs to be converted to UTC
   DLLLOCAL void setLocalIntern(int n_us) {
      // normalize units in case us > 1000000 or < 0
      normalize_units2<int64>(epoch, n_us, 1000000);
      us = n_us;

      // get standard time UTC offset
      int off = zone->getUTCOffset();

      //printd(5, "qore_absolute_time::setLocalIntern() epoch: %lld -> %lld (%d)\n", epoch, epoch + off, off);
      epoch -= off;

      // now get actual UTC offset
      int aoff = zone->getUTCOffset(epoch);
      //printd(5, "qore_absolute_time::setLocalIntern() epoch: %lld -> %lld (aoff=%d diff=%d)\n", epoch, epoch + aoff - off, aoff, aoff - off);
      if (aoff != off)
         epoch -= (aoff - off);
   }

   DLLLOCAL void setTM(qore_simple_tm2 &tm, struct tm &tms, bool dst = false) const {
      tms.tm_year = tm.year - 1900;
      tms.tm_mon = tm.month - 1;
      tms.tm_mday = tm.day;
      tms.tm_hour = tm.hour;
      tms.tm_min = tm.minute;
      tms.tm_sec = tm.second;
      tms.tm_isdst = 0;
      tms.tm_wday = qore_date_info::getDayOfWeek(tm.year, tm.month, tm.day);
      tms.tm_yday = qore_date_info::getDayNumber(tm.year, tm.month, tm.day) - 1;
      tms.tm_isdst = dst;
   }

public:
   DLLLOCAL void set(const AbstractQoreZoneInfo *n_zone, int64 n_epoch, int n_us) {
      zone = n_zone;
      epoch = n_epoch;
      normalize_units2<int64, int>(epoch, n_us, 1000000);
      us = n_us;      
   }

   DLLLOCAL void setLocal(const AbstractQoreZoneInfo *n_zone, int64 n_epoch, int n_us) {
      epoch = n_epoch;
      zone = n_zone;
      normalize_units2<int64, int>(epoch, n_us, 1000000);
      setLocalIntern(n_us);
   }

   DLLLOCAL void set(const AbstractQoreZoneInfo *n_zone, int year, int month, int day, int hour, int minute, int second, int n_us) {
      zone = n_zone;
      epoch = qore_date_info::getEpochSeconds(year, month, day, hour, minute, second);
      //printd(0, "zone=%p (%s) %04d-%02d-%02d %02d:%02d:%02d.%06d epoch=%lld\n", zone, zone->getRegionName(), year, month, day, hour, minute, second, n_us, epoch);

      setLocalIntern(n_us);
   }

   DLLLOCAL void set(const qore_absolute_time &p) {
      epoch = p.epoch;
      us = p.us;
      zone = p.zone;
   }

   DLLLOCAL void setZone(const AbstractQoreZoneInfo *n_zone) {
      zone = n_zone;
   }

   DLLLOCAL void setTime(int h, int m, int s, int usecs) {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      //printd(5, "qore_absolute_time::setTime(h=%d, m=%d, s=%d, usecs=%d) %04d-%02d-%02d\n", h, m, s, usecs, tm.year, tm.month, tm.day);

      normalize_units2<int, int>(s, usecs, 1000000);
      normalize_units2<int, int>(m, s, 60);
      normalize_units2<int, int>(h, m, 60);

      if (h < 0)
         h = 0;
      else if (h > 23)
         h = 23;

      epoch = qore_date_info::getEpochSeconds(tm.year, tm.month, tm.day, h, m, s);
      setLocalIntern(usecs);
   }

   DLLLOCAL void setLiteral(int64 date, int usecs = 0) {
      // reset time zone to current time zone
      zone = currentTZ();

      // get broken down date from literal representation
      qore_simple_tm2 tm;
      tm.setLiteral(date, usecs);

      // set local time from date
      epoch = qore_date_info::getEpochSeconds(tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
      //printd(5, "qore_absolute_date::setLiteral(date=%lld, usecs=%d) epoch=%lld %04d-%02d-%02d %02d:%02d:%02d\n", date, usecs, epoch, tm.year, tm.month, tm.day, tm.hour, tm.minute, tm.second);
      setLocalIntern(usecs);
   }

   DLLLOCAL void getISOWeek(int &yr, int &week, int &wday) const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      tm.getISOWeek(yr, week, wday);
   }

   DLLLOCAL void get(qore_time_info &info) const {
      const char *zname;
      bool isdst;
      int offset = zone->getUTCOffset(epoch, isdst, zname);
      info.set(epoch, us, offset, isdst, zname, zone);
   }

   DLLLOCAL void get(const AbstractQoreZoneInfo *z, qore_time_info &info) const {
      const char *zname;
      bool isdst;
      int offset = zone->getUTCOffset(epoch, isdst, zname);
      info.set(epoch, us, offset, isdst, zname, zone);
   }

   DLLLOCAL void getDate(qore_simple_tm &tm) const {
      int off = zone->getUTCOffset(epoch);

      tm.set(epoch + off, us);
   }

   DLLLOCAL short getYear() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.year;
   }

   DLLLOCAL int getMonth() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.month;
   }

   DLLLOCAL int getDay() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.day;
   }

   DLLLOCAL int getHour() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.hour;
   }

   DLLLOCAL int getMinute() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.minute;
   }

   DLLLOCAL int getSecond() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.second;
   }

   DLLLOCAL int getMillisecond() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.us / 1000;
   }

   DLLLOCAL int getMicrosecond() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return tm.us;
   }

   DLLLOCAL int64 getRelativeSeconds() const {
      return getRelativeMicroseconds() / 1000000;
   }

   DLLLOCAL int64 getRelativeMilliseconds() const {
      return getRelativeMicroseconds() / 1000;
   }

   DLLLOCAL int64 getRelativeMicroseconds() const;

   DLLLOCAL void localtime(struct tm &tms) const {
      bool isdst;
      int offset = zone->getUTCOffset(epoch, isdst);
      qore_simple_tm2 tm(epoch + offset, us);

      setTM(tm, tms, isdst);
   }

   DLLLOCAL void gmtime(struct tm &tms) const {
      qore_simple_tm2 tm(epoch, us);
      setTM(tm, tms, false);
   }

   DLLLOCAL int compare(const qore_absolute_time &r) const {
      if (epoch > r.epoch)
         return 1;
      if (epoch < r.epoch)
         return -1;
      if (us > r.us)
         return 1;
      if (us < r.us)
         return -1;
      return 0;
   }

   DLLLOCAL int getDayOfWeek() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return qore_date_info::getDayOfWeek(tm.year, tm.month, tm.day);
   }

   DLLLOCAL int getDayNumber() const {
      qore_simple_tm2 tm(epoch + zone->getUTCOffset(epoch), us);
      return qore_date_info::getDayNumber(tm.year, tm.month, tm.day);
   }

   DLLLOCAL bool hasValue() const {
      return epoch || us ? true : false;
   }

   DLLLOCAL int64 getEpochSeconds() const {
      return epoch + zone->getUTCOffset(epoch);
   }

   DLLLOCAL int64 getEpochMilliseconds() const {
      return getEpochSeconds() * 1000 + (us / 1000);
   }

   DLLLOCAL int64 getEpochMicroseconds() const {
      return getEpochSeconds() * 1000000 + us;
   }

   DLLLOCAL int64 getEpochSecondsUTC() const {
      return epoch;
   }

   DLLLOCAL int64 getEpochMicrosecondsUTC() const {
      return epoch * 1000000 + us;
   }

   DLLLOCAL int64 getEpochMillisecondsUTC() const {
      return epoch * 1000 + (us / 1000);
   }

   DLLLOCAL qore_absolute_time &operator+=(const qore_relative_time &dt);
   DLLLOCAL qore_absolute_time &operator-=(const qore_relative_time &dt);

   DLLLOCAL void getAsString(QoreString &str) const;

   DLLLOCAL void unaryMinus() {
      epoch = -epoch;
      us = -us;
   }
};
   
class qore_relative_time : protected qore_simple_tm {
   friend class qore_absolute_time;
protected:
   DLLLOCAL void normalize() {
      //printd(5, "DT:cD() sec=%lld ms=%d\n", sec, ms);

      // normalize years from months      
      normalize_units<int, int>(year, month, 12);

      // normalize seconds from microseconds
      normalize_units<int, int>(second, us, 1000000);

      // no longer normalize days, as with DST not all days are 24 hours

      // normalize hours from seconds
      normalize_units<int, int>(hour, second, 3600);

      // normalize minutes from seconds
      normalize_units<int, int>(minute, second, 60);
   }

public:
   DLLLOCAL void set(int n_year, int n_month, int n_day, int n_hour, int n_minute, int n_second, int n_us) {
      qore_simple_tm::set(n_year, n_month, n_day, n_hour, n_minute, n_second, n_us);
      normalize();
   }

   DLLLOCAL void set(const qore_relative_time &p) {
      year = p.year;
      month = p.month;
      day = p.day;
      hour = p.hour;
      minute = p.minute;
      second = p.second;
      us = p.us;
   }

   // takes the different between seconds.micros - dt and sets this to the relative date/time difference
   DLLLOCAL void setDifference(int64 seconds, int micros, const qore_absolute_time &dt) {
      int64 sec = seconds - dt.epoch;
      us = micros - dt.us;      

      year = month = day = hour = minute = 0;

      // normalize seconds from microseconds
      normalize_units<int64, int>(sec, us, 1000000);

      // do not normalize days, as with DST not all days are 24 hours

      // normalize hours from seconds
      normalize_units<int, int64>(hour, sec, 3600);

      // normalize minutes from seconds
      normalize_units<int, int64>(minute, sec, 60);

      second = sec;
   }

   DLLLOCAL void setLiteral(int64 date, int usecs = 0) {
      year = date / 10000000000ll;
      date -= year * 10000000000ll;
      month = date / 100000000ll;
      date -= month * 100000000ll;
      day = date / 1000000ll;
      date -= day * 1000000ll;
      hour = date / 10000ll; 
      date -= hour * 10000ll;
      minute = date / 100ll;
      second = date - minute * 100ll;
      us = usecs;

      normalize();
   }

   DLLLOCAL void setTime(int h, int m, int s, int usecs) {
      hour = h;
      minute = m;
      second = s;
      us = usecs;
   }

   DLLLOCAL short getYear() const {
      return year;
   }

   DLLLOCAL int getMonth() const {
      return month;
   }

   DLLLOCAL int getDay() const {
      return day;
   }

   DLLLOCAL int getHour() const {
      return hour;
   }

   DLLLOCAL int getMinute() const {
      return minute;
   }

   DLLLOCAL int getSecond() const {
      return second;
   }

   DLLLOCAL int getMillisecond() const {
      return us / 1000;
   }

   DLLLOCAL int getMicrosecond() const {
      return us;
   }

   DLLLOCAL int compare(const qore_relative_time &r) const {      
      if (year > r.year)
         return 1;
      if (year < r.year)
         return -1;
      if (month > r.month)
         return 1;
      if (month < r.month)
         return -1;
      if (day > r.day)
         return 1;
      if (day < r.day)
         return -1;
      if (hour > r.hour)
         return 1;
      if (hour < r.hour)
         return -1;
      if (minute > r.minute)
         return 1;
      if (minute < r.minute)
         return -1;
      if (second > r.second)
         return 1;
      if (second < r.second)
         return -1;
      if (us > r.us)
         return 1;
      if (us < r.us)
         return -1;
      return 0;
   }

   DLLLOCAL void unaryMinus() {
      year = -year;
      month = -month;
      day = -day;
      hour = -hour;
      minute = -minute;
      second = -second;
      us = -us;
   }

   DLLLOCAL int64 getRelativeSeconds() const {
      return getRelativeMicroseconds() / 1000000;
   }

   DLLLOCAL int64 getRelativeMilliseconds() const {
      return getRelativeMicroseconds() / 1000;
   }

   DLLLOCAL int64 getRelativeMicroseconds() const {
      return (int64)us + (int64)second * MICROSECS_PER_SEC
         + (int64)minute * MICROSECS_PER_MINUTE 
         + (int64)hour * MICROSECS_PER_HOUR 
         + (int64)day * MICROSECS_PER_DAY 
         + (month ? (int64)month * MICROSECS_PER_AVG_MONTH : 0ll)
         + (year ? (int64)year * MICROSECS_PER_YEAR : 0ll);
   }

   DLLLOCAL qore_relative_time &operator+=(const qore_relative_time &dt);
   DLLLOCAL qore_relative_time &operator-=(const qore_relative_time &dt);

   DLLLOCAL void getAsString(QoreString &str) const {
      int f = 0;
      str.concat("<time:");

#define PL(n) (n == 1 ? "" : "s")

      if (year)
         str.sprintf(" %d year%s", year, PL(year)), f++;
      if (month)
         str.sprintf(" %d month%s", month, PL(month)), f++;
      if (day)
         str.sprintf(" %d day%s", day, PL(day)), f++;
      if (hour)
         str.sprintf(" %d hour%s", hour, PL(hour)), f++;
      if (minute)
         str.sprintf(" %d minute%s", minute, PL(minute)), f++;
      if (second || (!f && !us))
         str.sprintf(" %d second%s", second, PL(second));

      if (us) {
         int ms = us / 1000;
         if (ms * 1000 == us)
            str.sprintf(" %d millisecond%s", ms, PL(ms));
         else
            str.sprintf(" %d microsecond%s", us, PL(us));
      }

#undef PL

      str.concat('>');
   }

   DLLLOCAL void getTM(struct tm &tms) const {
      tms.tm_year = year;
      tms.tm_mon = month;
      tms.tm_mday = day;
      tms.tm_hour = hour;
      tms.tm_min = minute;
      tms.tm_sec = second;
      tms.tm_isdst = 0;
      tms.tm_wday = 0;
      tms.tm_yday = 0;
      tms.tm_isdst = -1;
   }

   DLLLOCAL void get(qore_time_info &info) const {
      info = *this;
   }

   DLLLOCAL void zero() {
      qore_simple_tm::zero();
   }

   DLLLOCAL bool hasValue() const {
      return qore_simple_tm::hasValue();
   }
};

static inline void zero_tm(struct tm &tms) {
   tms.tm_year = 70;
   tms.tm_mon = 0;
   tms.tm_mday = 1;
   tms.tm_hour = 0;
   tms.tm_min = 0;
   tms.tm_sec = 0;
   tms.tm_isdst = 0;
   tms.tm_wday = 0;
   tms.tm_yday = 0;
   tms.tm_isdst = -1;
}

class qore_date_private {
protected: 
   // actual data is held in the following union
   // unfortunately the original API was designed such that the object must be
   // able to change from absolute to relative, so we have a union
   union {
      qore_absolute_time abs;
      qore_relative_time rel;
   } d;
   bool relative;

public:
   DLLLOCAL qore_date_private(bool r = false) : relative(r) {
      if (r)
         d.rel.zero();
      else
         d.abs.set(currentTZ(), 0, 0);
   }

   DLLLOCAL qore_date_private(const AbstractQoreZoneInfo *zone, int64 seconds, int us = 0) : relative(false) {
      d.abs.set(zone, seconds, us);
   }

   DLLLOCAL qore_date_private(const AbstractQoreZoneInfo *zone, int y, int mo, int dy, int h, int mi, int s, int us) : relative(false) {
      d.abs.set(zone, y, mo, dy, h, mi, s, us);
   }

   // this constructor assumes local time
   DLLLOCAL qore_date_private(int y, int mo, int dy, int h, int mi, int s, int us, bool r) : relative(r) {
      if (r)
         d.rel.set(y, mo, dy, h, mi, s, us);
      else
         d.abs.set(currentTZ(), y, mo, dy, h, mi, s, us);
   }

   DLLLOCAL static int compare(const qore_date_private &left, const qore_date_private &right) {
      // absolute dates are always larger than relative dates, no matter the value
      if (left.relative)
         return right.relative ? left.d.rel.compare(right.d.rel) : -1;

      return right.relative ? 1 : left.d.abs.compare(right.d.abs);
   }

   DLLLOCAL qore_date_private &operator=(const qore_date_private &p) {
      if (p.relative)
         d.rel.set(p.d.rel);
      else
         d.abs.set(p.d.abs);

      relative = p.relative;
      return *this;
   }

   DLLLOCAL void setDate(const qore_date_private &p) {
      *this = p;
   }

   // assumes local time zone
   DLLLOCAL void setDate(const struct tm &tms, int us) {
      relative = false;

      d.abs.set(currentTZ(), 1900 + tms.tm_year, tms.tm_mon + 1, tms.tm_mday, tms.tm_hour, tms.tm_min, tms.tm_sec, us);
   }

   DLLLOCAL void setDate(const char *str) {
#ifdef HAVE_STRTOLL
      int64 date = strtoll(str, 0, 10);
#else
      int64 date = atoll(str);
#endif
      const char *p = strchr(str, '.');

      int l = p ? p - str : strlen(str);
      // for date-only strings, move the date up to the right position
      if (l == 8)
         date *= 1000000;
      else if (l == 6 || l == 10) // for time-only strings
         date += 19700101000000LL;

      relative = false;

      int us = p ? atoi(p + 1) : 0;
      if (us) {
         l = strlen(p + 1);
         assert(l < 7);
         us *= (int)pow(10, 6 - l);
      }         

      d.abs.setLiteral(date, us);
   }

   DLLLOCAL void setRelativeDate(const char *str) {
#ifdef HAVE_STRTOLL
      int64 date = strtoll(str, 0, 10);
#else
      int64 date = atoll(str);
#endif
      const char *p = strchr(str, '.');

      int l = p ? p - str : strlen(str);
      // for date-only strings, move the date up to the right position
      if (l == 8)
         date *= 1000000;

      relative = true;
      
      int us = p ? atoi(p + 1) : 0;
      if (us) {
         l = strlen(p + 1);
         assert(l < 7);
         us *= (int)pow(10, 6 - l);
      }         

      d.rel.setLiteral(date, us);
   }

   DLLLOCAL bool isRelative() const {
      return relative;
   }

   DLLLOCAL void setZone(const AbstractQoreZoneInfo *n_zone) {
      if (!relative)
         d.abs.setZone(n_zone);
   }

   DLLLOCAL short getYear() const {
      return relative ? d.rel.getYear() : d.abs.getYear();
   }

   DLLLOCAL int getMonth() const {
      return relative ? d.rel.getMonth() : d.abs.getMonth();
   }

   DLLLOCAL int getDay() const {
      return relative ? d.rel.getDay() : d.abs.getDay();
   }

   DLLLOCAL int getHour() const {
      return relative ? d.rel.getHour() : d.abs.getHour();
   }

   DLLLOCAL int getMinute() const {
      return relative ? d.rel.getMinute() : d.abs.getMinute();
   }

   DLLLOCAL int getSecond() const {
      return relative ? d.rel.getSecond() : d.abs.getSecond();
   }

   DLLLOCAL int getMillisecond() const {
      return relative ? d.rel.getMillisecond() : d.abs.getMillisecond();
   }

   DLLLOCAL int getMicrosecond() const {
      return relative ? d.rel.getMicrosecond() : d.abs.getMicrosecond();
   }

   DLLLOCAL bool hasValue() const {
      return relative ? d.rel.hasValue() : d.abs.hasValue();
   }

   DLLLOCAL int64 getEpochSeconds() const {
      return relative ? d.rel.getRelativeSeconds() : d.abs.getEpochSeconds();
   }

   DLLLOCAL int64 getEpochSecondsUTC() const {
      return relative ? d.rel.getRelativeSeconds() : d.abs.getEpochSecondsUTC();
   }

   DLLLOCAL int64 getEpochMillisecondsUTC() const {
      return relative ? d.rel.getRelativeMilliseconds() : d.abs.getEpochMillisecondsUTC();
   }

   DLLLOCAL int64 getEpochMicrosecondsUTC() const {
      return relative ? d.rel.getRelativeMicroseconds() : d.abs.getEpochMicrosecondsUTC();
   }

   DLLLOCAL int getDayNumber() const {
      return relative ? 0 : d.abs.getDayNumber();
   }

   // it's not legal to call with this=relative and dt=absolute
   DLLLOCAL void add(const qore_date_private &dt) {
      if (!relative) {
         if (dt.relative)
            d.abs += dt.d.rel;
         else
            setDate(getEpochSecondsUTC() + dt.d.abs.getEpochSecondsUTC(), d.abs.getMicrosecond() + dt.d.abs.getMicrosecond());
         return;
      }

      assert(dt.relative);
      d.rel += dt.d.rel;
   }

   DLLLOCAL void unaryMinus() {
      if (relative)
         d.rel.unaryMinus();
      else
         d.abs.unaryMinus();
   }

   // it's not legal to call with this=relative and dt=absolute
   DLLLOCAL void subtractBy(const qore_date_private &dt) {
      if (!relative) {
         if (dt.relative)
            d.abs -= dt.d.rel;
         else {
            int64 secs = d.abs.getEpochSecondsUTC();
            int us = d.abs.getMicrosecond();
            relative = true;
            d.rel.setDifference(secs, us, dt.d.abs);
         }
         return;
      }

      assert(dt.relative);
      d.rel -= dt.d.rel;
   }

   DLLLOCAL void setTime(int h, int m, int s, int us) {
      if (relative)
         d.rel.setTime(h, m, s, us);
      else
         d.abs.setTime(h, m, s, us);
   }

   DLLLOCAL void setDate(const AbstractQoreZoneInfo *n_zone, int year, int month, int day, int hour, int minute, int second, int n_us) {
      relative = false;
      d.abs.set(n_zone, year, month, day, hour, minute, second, n_us);
   }

   DLLLOCAL void setDate(const AbstractQoreZoneInfo *zone, int64 seconds, int us = 0) {
      relative = false;
      d.abs.set(zone, seconds, us);
   }

   DLLLOCAL void setDate(int64 seconds, int us = 0) {
      relative = false;
      d.abs.set(currentTZ(), seconds, us);
   }

   DLLLOCAL void setLocalDate(int64 seconds, int us) {
      relative = false;
      d.abs.setLocal(currentTZ(), seconds, us);
   }

   DLLLOCAL void setLocalDate(const AbstractQoreZoneInfo *zone, int64 seconds, int us) {
      relative = false;
      d.abs.setLocal(zone, seconds, us);
   }

   DLLLOCAL void setDateLiteral(int64 date, int us = 0) {
      relative = false;
      d.abs.setLiteral(date, us);
   }

   DLLLOCAL void setRelativeDateLiteral(int64 date, int us = 0) {
      relative = true;
      d.rel.setLiteral(date, us);
   }

   DLLLOCAL int64 getRelativeSeconds() const {
      return relative ? d.rel.getRelativeSeconds() : d.abs.getRelativeSeconds();
   }

   DLLLOCAL int64 getRelativeMilliseconds() const {
      return relative ? d.rel.getRelativeMilliseconds() : d.abs.getRelativeMilliseconds();
   }

   DLLLOCAL int64 getRelativeMicroseconds() const {
      return relative ? d.rel.getRelativeMicroseconds() : d.abs.getRelativeMicroseconds();
   }

   DLLLOCAL int getDayOfWeek() const {
      return relative ? 0 : d.abs.getDayOfWeek();
   }

   DLLLOCAL void getISOWeek(int &yr, int &week, int &wday) const {
      if (relative) {
         yr = 1970;
         week = wday = 1;
         return;
      }
      return d.abs.getISOWeek(yr, week, wday);
   }

   DLLLOCAL bool isEqual(const qore_date_private &dt) const {
      return !compare(*this, dt);
   }

   DLLLOCAL void localtime(struct tm &tms) const {
      if (relative)
         d.rel.getTM(tms);
      else
         d.abs.localtime(tms);
   }

   DLLLOCAL void gmtime(struct tm &tms) const {
      if (relative) {
         zero_tm(tms);
         return;
      }
      d.abs.gmtime(tms);
   }

   DLLLOCAL void get(qore_time_info &info) const {
      if (relative)
         d.rel.get(info);
      else
         d.abs.get(info);
   }

   DLLLOCAL void get(const AbstractQoreZoneInfo *zone, qore_time_info &info) const {
      if (relative)
         d.rel.get(info);
      else
         d.abs.get(zone, info);
   }

   DLLLOCAL void format(QoreString &str, const char *fmt) const;

   DLLLOCAL void getAsString(QoreString &str) const {
      if (!relative)
         d.abs.getAsString(str);
      else
         d.rel.getAsString(str);
   }

   // note that ISO-8601 week days go from 1 - 7 = Mon - Sun
   // return value: 0 = an exception was raised, not 0 = OK
   DLLLOCAL static qore_date_private *getDateFromISOWeek(qore_date_private &result, int year, int week, int day, ExceptionSink *xsink) {
      if (week <= 0) {
         xsink->raiseException("ISO-8601-INVALID-WEEK", "week numbers must be positive (value passed: %d)", week);
         return 0;
      }

      // get day of week of jan 1 of this year
      int jan1 = qore_date_info::getDayOfWeek(year, 1, 1);

      if (week > 52) {
         // get maximum week number in this year
         int mw = 52 + ((jan1 == 4 && !qore_date_info::isLeapYear(year)) || (jan1 == 3 && qore_date_info::isLeapYear(year)));
         if (week > mw) {
            xsink->raiseException("ISO-8601-INVALID-WEEK", "there are only %d calendar weeks in year %d (week value passed: %d)", mw, year, week);
            return 0;
         }
      }
   
      if (day < 1 || day > 7) {
         xsink->raiseException("ISO-8601-INVALID-DAY", "calendar week days must be between 1 and 7 for Mon - Sun (day value passed: %f)", day);
         return 0;
      }

      // get year, month, day for start of iso-8601 calendar year
      int y, m, d;
      // if jan1 is mon, then the iso-8601 year starts with the normal year
      if (jan1 == 1) {
         y = year;
         m = 1;
         d = 1;
      }
      // if jan1 is tue - thurs, iso-8601 year starts in dec of previous real year
      else if (jan1 > 1 && jan1 < 5) {
         y = year - 1;
         m = 12;
         d = 33 - jan1;
      }
      else {
         y = year;
         m = 1;
         // jan1 is fri or saturday
         if (jan1)
            d = 9 - jan1;
         else // jan1 is sunday
            d = 2;
      }
   
      // get seconds for date of start of iso-8601 calendar year, add seconds for day offset and create new time
      result.setLocalDate(qore_date_info::getEpochSeconds(y, m, d) + ((week - 1) * 7 + (day - 1)) * 86400, 0);
      return 0;
   }
};

#endif
