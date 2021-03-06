#pragma once

#include <limits>
#include <string>
#include <time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
// I don't f***ing believe it.
#undef TRUE
#undef FALSE
#endif

#include "aslib/exc.hh"
#include "aslib/printable.hh"
#include "cron/date.hh"
#include "cron/daytime.hh"
#include "cron/time_zone.hh"

namespace cron {

using namespace aslib;

//------------------------------------------------------------------------------
// Local time structs
//------------------------------------------------------------------------------

// FIXME: In types.hh?

struct LocalDatenumDaytick
{
  LocalDatenumDaytick()
  : datenum(DATENUM_INVALID),
    daytick(DAYTICK_INVALID)
  {
  }

  LocalDatenumDaytick(
    Datenum const _datenum,
    Daytick const _daytick)
  : datenum(_datenum),
    daytick(_daytick)
  {
  }

  Datenum   datenum;
  Daytick   daytick;

};


template<class DATE, class DAYTIME>
struct LocalTime
{
  LocalTime(
    DATE const _date, 
    DAYTIME const _daytime)
  : date(_date),
    daytime(_daytime)
  {
  }

  DATE      date;
  DAYTIME   daytime;

};


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

namespace {

inline intmax_t
convert_offset(
  intmax_t  const offset0,
  intmax_t  const denominator0,
  Datenum   const base0,
  intmax_t  const denominator1,
  Datenum   const base1)
{
  return
      rescale_int(offset0, denominator0, denominator1)
    + ((intmax_t) base0 - base1) * SECS_PER_DAY * denominator1;
}
  

template<typename TIME>
inline LocalDatenumDaytick
to_local_datenum_daytick(
  TIME const time,
  TimeZone const& tz)
{
  using Offset = typename TIME::Offset;

  // Look up the time zone offset for this time.
  auto const tz_offset = tz.get_parts(time).offset;
  // Compute the local offset.
  auto const offset 
    = (Offset) (time.get_offset() + tz_offset * TIME::DENOMINATOR);

  // Establish the date and daytime parts, using division rounded toward -inf
  // and a positive remainder.
  Datenum const datenum   
    =   (int64_t) (offset / TIME::DENOMINATOR) / SECS_PER_DAY 
      + (offset < 0 ? -1 : 0)
      + TIME::BASE;
  Offset const day_offset 
    =   (int64_t) offset % (TIME::DENOMINATOR * SECS_PER_DAY) 
      + (offset < 0 ? TIME::DENOMINATOR * SECS_PER_DAY : 0);
  // FIXME: Not sure the types are right here.
  Daytick const daytick = rescale_int(
    (intmax_t) day_offset, 
    (intmax_t) TIME::DENOMINATOR, (intmax_t) DAYTICK_PER_SEC);

  return {datenum, daytick};
}


}  // anonymous namespace


/* 
 * Converts the time in a a 'struct timespec' to an offset for 'TIME'.
 */
template<typename TIME>
inline typename TIME::Offset
timespec_to_offset(
  struct timespec const& ts)
{
  using Offset = typename TIME::Offset;
  return 
      ((Offset) (DATENUM_UNIX_EPOCH - TIME::BASE) * SECS_PER_DAY + ts.tv_sec) 
    * TIME::DENOMINATOR
    + rescale_int<Offset, (Offset) 1000000000, TIME::DENOMINATOR>(ts.tv_nsec);
}


//------------------------------------------------------------------------------

/**
 *  This table shows some sample configurations for time representation.  The
 *  first four columns give the number of bits used for storage, 'u' for
 *  unsigned or 's' for signed, the denominator to convert the storage
 *  representation to seconds, and the base year.  The remaining columns show
 *  the total representable range in years, the range of representable years,
 *  and the approximate time resolution.
 *
 *  FIXME: Maybe NsecTime should be Time.  Who measures ns before 1900?
 *  - SmallTime -> Time32
 *  - LongTime and LongTime32 to span 1-9999
 *
 *    Bits  Sgn  Denom  Base     Years  Yr. Range  Resolution    Class
 *    ----  ---  -----  ----     -----  ---------  ----------    ------------
 *      32    u  1      1970       136  1970-2106      1  sec    SmallTime
 *      32    s  1      1970       136  1902-2038      1  sec    Unix32Time
 *      64    s  1      1970      many  0001-9999      1  sec    Unix64Time
 *      32    u  1<< 2  1990        34  1990-2024    250 msec
 *      64    u  1<<32  1970       136  1970-2106    230 psec 
 *      64    u  1<<30  1900       544  1900-2444    930 psec    NsecTime
 *      64    u  1<<28  1200      2179  1200-3379      4 nsec
 *      64    u  1<<26     1      8716     1-8717     15 nsec    Time
 *
 */

template<class TRAITS>
class TimeTemplate
{
public:

  using Offset = typename TRAITS::Offset;

  static bool         constexpr USE_INVALID = TRAITS::use_invalid;
  static Datenum      constexpr BASE        = TRAITS::base;
  static Offset       constexpr DENOMINATOR = TRAITS::denominator;
  static TimeTemplate const     MIN;
  static TimeTemplate const     MAX;
  static TimeTemplate const     INVALID;
  static TimeTemplate const     MISSING;
  static double       constexpr RESOLUTION  = 1.0 / TRAITS::denominator;

  // Constructors

  TimeTemplate() 
  : offset_(USE_INVALID ? INVALID.offset_ : MIN.offset_) 
  {
  }

  template<class TTRAITS> 
  TimeTemplate(
    TimeTemplate<TTRAITS> time)
  : TimeTemplate(convert_offset(time.get_offset(), TTRAITS::denominator, TTRAITS::base))
  {
  }

  TimeTemplate(
    Datenum datenum,
    Daytick daytick,
    TimeZone const& tz,
    bool first=true)
  : offset_(datenum_daytick_to_offset(datenum, daytick, tz, first))
  {
  }

  template<class DTRAITS, class YTRAITS>
  TimeTemplate(
    DateTemplate<DTRAITS> date,
    DaytimeTemplate<YTRAITS> daytime,
    TimeZone const& tz,
    bool first=true)
  : TimeTemplate(date.get_datenum(), daytime.get_daytick(), tz, first)
  {
  }

  TimeTemplate(
    Year year,
    Month month,
    Day day,
    Hour hour,
    Minute minute,
    Second second,
    TimeZone const& tz,
    bool first=true)
  : TimeTemplate(parts_to_offset(year, month, day, hour, minute, second, tz, first))
  {
  }

  static TimeTemplate 
  from_offset(
    Offset offset)
  {
    return TimeTemplate(
      in_range(MIN.get_offset(), offset, MAX.get_offset())
      ? offset
      : on_error<InvalidTimeError>());
  }

  static TimeTemplate
  from_timetick(
    Timetick const timetick)
  {
    return TimeTemplate(rescale_int<Timetick, TIMETICK_PER_SEC, DENOMINATOR>(
      timetick - TIMETICK_PER_SEC * SECS_PER_DAY * BASE));
  }

  // Comparisons

  bool is_valid()   const { return in_range(MIN.offset_, offset_, MAX.offset_); }
  bool is_invalid() const { return is(INVALID); }
  bool is_missing() const { return is(MISSING); }

  bool is(TimeTemplate const& o) const { return offset_ == o.offset_; }
  bool operator==(TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ == o.offset_; }
  bool operator!=(TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ != o.offset_; }
  bool operator< (TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ <  o.offset_; }
  bool operator<=(TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ <= o.offset_; }
  bool operator> (TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ >  o.offset_; }
  bool operator>=(TimeTemplate const& o) const { return is_valid() && o.is_valid() && offset_ >= o.offset_; }

  // Accessors

  Offset get_offset() const { return offset_; }

  Datenum
  get_utc_datenum() 
    const
  {
    return 
      is_valid() 
      ? offset_ / SECS_PER_DAY / TRAITS::denominator + TRAITS::base
      : DATENUM_INVALID;
  }

  Daytick 
  get_utc_daytick() 
    const
  {
    if (is_valid()) {
      Offset const day_offset = offset_ % (SECS_PER_DAY * TRAITS::denominator);
      return rescale_int<Daytick, TRAITS::denominator, DAYTICK_PER_SEC>(day_offset);
    }
    else
      return DAYTICK_INVALID;
  }

  template<class DATE> DATE get_utc_date() const { return DATE::from_datenum(get_utc_datenum()); }
  template<class DAYTIME> DAYTIME get_utc_daytime() const { return DAYTIME::from_daytick(get_utc_daytick()); }

  TimeParts 
  get_parts(
    TimeZone const& tz) 
    const
  {
    if (! is_valid()) 
      return TimeParts::get_invalid();

    TimeParts parts;

    // Look up the time zone.
    parts.time_zone = tz.get_parts(*this);
    Offset const offset = offset_ + parts.time_zone.offset * TRAITS::denominator;

    // Establish the date and daytime parts, using division rounded toward -inf
    // and a positive remainder.
    Datenum const datenum   
      = (int64_t) (offset / TRAITS::denominator) / SECS_PER_DAY 
        + (offset < 0 ? -1 : 0)
        + BASE;
    Offset const day_offset 
      = (int64_t) offset % (TRAITS::denominator * SECS_PER_DAY) 
        + (offset < 0 ? TRAITS::denominator * SECS_PER_DAY : 0);

    parts.date            = datenum_to_parts(datenum);
    parts.daytime.second  = (Second) (day_offset % (SECS_PER_MIN * TRAITS::denominator)) / TRAITS::denominator;
    Offset const minutes  = day_offset / (SECS_PER_MIN * TRAITS::denominator);
    parts.daytime.minute  = minutes % MINS_PER_HOUR;
    parts.daytime.hour    = minutes / MINS_PER_HOUR;
    return parts;
  }

  TimeParts get_parts(std::string const& tz_name) const { return get_parts(*get_time_zone(tz_name)); }
  TimeParts get_parts() const { return get_parts(*get_display_time_zone()); }

  TimeOffset 
  get_time_offset()
    const
  {
    return cron::convert_offset(get_offset(), DENOMINATOR, BASE, 1, DATENUM_UNIX_EPOCH);
  }

  Timetick
  get_timetick()
    const
  {
    return 
      is_valid() 
      ?   rescale_int<Timetick, DENOMINATOR, TIMETICK_PER_SEC>(offset_)
        + TIMETICK_PER_SEC * SECS_PER_DAY * BASE
      : TIMETICK_INVALID;
  }

private:

  template<class EXC>
  static Offset
  on_error()
  {
    if (TRAITS::use_invalid)
      return INVALID.get_offset();
    else
      throw EXC();
  }

  static Offset 
  datenum_daytick_to_offset(
    Datenum datenum,
    Daytick daytick,
    TimeZone const& tz,
    bool first)
  {
    if (! datenum_is_valid(datenum)) 
      return on_error<InvalidDateError>();
    if (! daytick_is_valid(daytick))
      return on_error<InvalidDaytimeError>();

    Offset tz_offset;
    try {
      tz_offset = tz.get_parts_local(datenum, daytick, first).offset;
    }
    catch (NonexistentLocalTime) {
      // FIXME: Don't catch and rethrow...
      return on_error<NonexistentLocalTime>();
    }

    // Below, we compute this expression with overflow checking:
    //
    //     DENOMINATOR * SECS_PER_DAY * (datenum - BASE)
    //   + (Offset) rescale_int<Daytick, DAYTICK_PER_SEC, DENOMINATOR>(daytick)
    //   - DENOMINATOR * tz_offset;

    Offset const off = 
      (Offset) rescale_int<Daytick, DAYTICK_PER_SEC, DENOMINATOR>(daytick)
      - DENOMINATOR * tz_offset;
    Offset r;
    if (   mul_overflow(DENOMINATOR * SECS_PER_DAY, (Offset) datenum - BASE, r)
        || add_overflow(r, off, r))
      return on_error<InvalidTimeError>();
    else
      return r;
  }

  static Offset 
  parts_to_offset(
    Year year,
    Month month,
    Day day,
    Hour hour,
    Minute minute,
    Second second,
    TimeZone const& tz,
    bool first=true)
  {
    if (! ymd_is_valid(year, month, day))
      return on_error<InvalidDateError>();
    if (! hms_is_valid(hour, minute, second))
      return on_error<InvalidTimeError>();

    Datenum const datenum = ymd_to_datenum(year, month, day);
    Daytick const daytick = hms_to_daytick(hour, minute, second);
    return datenum_daytick_to_offset(datenum, daytick, tz, first);
  }

  static Offset
  convert_offset(
    intmax_t offset0,
    intmax_t denominator0,
    Datenum base0)
  {
    intmax_t const offset = 
      cron::convert_offset(offset0, denominator0, base0, DENOMINATOR, BASE);
    return
      in_range((intmax_t) MIN.get_offset(), offset, (intmax_t) MAX.get_offset())
      ? offset
      : on_error<InvalidTimeError>();
  }

  constexpr TimeTemplate(Offset offset) : offset_(offset) {}

  Offset offset_;

};


template<class TRAITS>
bool constexpr
TimeTemplate<TRAITS>::USE_INVALID;

template<class TRAITS>
Datenum constexpr
TimeTemplate<TRAITS>::BASE;

template<class TRAITS>
typename TimeTemplate<TRAITS>::Offset constexpr
TimeTemplate<TRAITS>::DENOMINATOR;

template<class TRAITS>
TimeTemplate<TRAITS> constexpr
TimeTemplate<TRAITS>::MIN{TRAITS::min};

template<class TRAITS>
TimeTemplate<TRAITS> constexpr
TimeTemplate<TRAITS>::MAX{TRAITS::max};

template<class TRAITS>
TimeTemplate<TRAITS> constexpr
TimeTemplate<TRAITS>::INVALID{TRAITS::invalid};

template<class TRAITS>
TimeTemplate<TRAITS> constexpr
TimeTemplate<TRAITS>::MISSING{TRAITS::missing};

template<class TRAITS>
double constexpr
TimeTemplate<TRAITS>::RESOLUTION;


//------------------------------------------------------------------------------
// Concrete time classes
//------------------------------------------------------------------------------

struct TimeTraits
{
  using Offset = uint64_t;

  static Datenum constexpr base         = 0; 
  static Offset  constexpr denominator  = (Offset) 1 << 26;
  static Offset  constexpr invalid      = std::numeric_limits<Offset>::max();
  static Offset  constexpr missing      = std::numeric_limits<Offset>::max() - 1;
  static Offset  constexpr min          = 0;
  static Offset  constexpr max          = std::numeric_limits<Offset>::max() - 2;

  static bool    constexpr use_invalid  = true;
};

extern template class TimeTemplate<TimeTraits>;
using Time =  TimeTemplate<TimeTraits>;


struct SmallTimeTraits
{
  using Offset = uint32_t;

  static Datenum constexpr base         = DATENUM_UNIX_EPOCH; 
  static Offset  constexpr denominator  = 1;
  static Offset  constexpr invalid      = std::numeric_limits<Offset>::max();
  static Offset  constexpr missing      = std::numeric_limits<Offset>::max() - 1;
  static Offset  constexpr min          = 0;
  static Offset  constexpr max          = std::numeric_limits<Offset>::max() - 2;

  static bool    constexpr use_invalid  = true;
};

extern template class TimeTemplate<SmallTimeTraits>;
using SmallTime = TimeTemplate<SmallTimeTraits>;


struct NsecTimeTraits
{
  using Offset = uint64_t;

  static Datenum constexpr base         = 693595;  // 1900-01-01
  static Offset  constexpr denominator  = (Offset) 1 << 30;
  static Offset  constexpr invalid      = std::numeric_limits<Offset>::max();
  static Offset  constexpr missing      = std::numeric_limits<Offset>::max() - 1;
  static Offset  constexpr min          = 0;
  static Offset  constexpr max          = std::numeric_limits<Offset>::max() - 2;

  static bool    constexpr use_invalid  = true;
};

extern template class TimeTemplate<NsecTimeTraits>;
using NsecTime = TimeTemplate<NsecTimeTraits>;


struct Unix32TimeTraits
{
  using Offset = int32_t;

  static Datenum constexpr base         = DATENUM_UNIX_EPOCH;
  static Offset  constexpr denominator  = 1;
  static Offset  constexpr invalid      = std::numeric_limits<Offset>::max();
  static Offset  constexpr missing      = std::numeric_limits<Offset>::max() - 1;
  static Offset  constexpr min          = std::numeric_limits<Offset>::min();
  static Offset  constexpr max          = std::numeric_limits<Offset>::max() - 2;

  static bool    constexpr use_invalid  = true;
};

extern template class TimeTemplate<Unix32TimeTraits>;
using Unix32Time = TimeTemplate<Unix32TimeTraits>;


struct Unix64TimeTraits
{
  using Offset = int64_t;

  static Datenum constexpr base         = DATENUM_UNIX_EPOCH;
  static Offset  constexpr denominator  = 1;
  static Offset  constexpr min          = -62135596800l;    // 0001-01-01
  static Offset  constexpr max          = 253402300800l;    // 9999-12-31
  static Offset  constexpr invalid      = 253402300802l;
  static Offset  constexpr missing      = 253402300801l;

  static bool    constexpr use_invalid  = true;
};

extern template class TimeTemplate<Unix64TimeTraits>;
using Unix64Time = TimeTemplate<Unix64TimeTraits>;


//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

template<typename TRAITS>
inline TimeTemplate<TRAITS>
operator+(
  TimeTemplate<TRAITS> time,
  double shift)
{
  using Time = TimeTemplate<TRAITS>;
  using Offset = typename Time::Offset;
  return
      time.is_invalid() || time.is_missing() ? time
    : Time::from_offset(
        time.get_offset() + (Offset) (shift * Time::DENOMINATOR));
}


template<typename TRAITS>
inline TimeTemplate<TRAITS>
operator-(
  TimeTemplate<TRAITS> time,
  double shift)
{
  using Time = TimeTemplate<TRAITS>;
  using Offset = typename Time::Offset;
  return
      time.is_invalid() || time.is_missing() ? time
    : Time::from_offset(
        time.get_offset() - (Offset) (shift * Time::DENOMINATOR));
}


template<typename TRAITS>
inline double
operator-(
  TimeTemplate<TRAITS> time0,
  TimeTemplate<TRAITS> time1)
{
  using Time = TimeTemplate<TRAITS>;

  if (time0.is_valid() && time1.is_valid())
    return 
        (double) time0.get_offset() / Time::DENOMINATOR
      - (double) time1.get_offset() / Time::DENOMINATOR;
  else if (Time::USE_INVALID)
    // FIXME: What do we do with invalid/missing values?
    return 0;
  else
    throw cron::ValueError("can't subtract invalid times");
}


template<typename TIME>
inline TIME
from_local(
  Datenum const datenum,
  Daytick const daytick,
  TimeZone const& time_zone,
  bool const first=true)
{
  // FIXME: Move the logic here, instead of delegating.
  return {datenum, daytick, time_zone, first};
}


template<typename TIME>
inline TIME
now()
{
  struct timespec ts;
  bool success;

#ifdef __MACH__
  clock_serv_t cclock;
  mach_timespec_t mts;
  success = 
       host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock) == 0
    && clock_get_time(cclock, &mts) == 0;
  mach_port_deallocate(mach_task_self(), cclock);
  ts.tv_sec = mts.tv_sec;
  ts.tv_nsec = mts.tv_nsec;
#else
  success = clock_gettime(CLOCK_REALTIME, &ts) == 0;
#endif

  return 
      success
    ? TIME::from_offset(cron::timespec_to_offset<Time>(ts)) 
    : TIME::INVALID;
}


template<typename TIME, typename DATE, typename DAYTIME>
inline LocalTime<DATE, DAYTIME>
to_local(
  TIME const time,
  TimeZone const& tz)
{
  if (time.is_valid()) {
    auto dd = to_local_datenum_daytick(time, tz);
    return {DATE::from_datenum(dd.datenum), TIME::from_daytick(dd.daytick)};
  }
  else
    // FIXME: LocalTime::INVALID?
    return {DATE::INVALID, DAYTIME::INVALID};
}


//------------------------------------------------------------------------------

}  // namespace cron


