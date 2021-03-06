#pragma once

#include "aslib/math.hh"
#include "cron/date.hh"
#include "cron/types.hh"

namespace cron {

//------------------------------------------------------------------------------
// Construction functions
//------------------------------------------------------------------------------

/*
 * Creates a date from its (date class-specific) offset.
 *
 * Returns an invalid date if the offset is not valid.
 */
template<class DATE>
inline DATE
from_offset(
  typename DATE::Offset const offset)
{
  return 
      DATE::offset_is_valid(offset)
    ? DATE::from_offset(offset)
    : DATE::INVALID;
}


template<class DATE>
inline DATE
from_datenum(
  Datenum const datenum)
{
  return from_offset<DATE>(DATE::datenum_to_offset(datenum));
}


template<class DATE>
inline DATE
from_ordinal_date(
  Year const year,
  Ordinal const ordinal)
{
  return 
      ordinal_date_is_valid(year, ordinal)
    ? from_datenum<DATE>(ordinal_date_to_datenum(year, ordinal))
    : DATE::INVALID;
}


template<class DATE>
inline DATE
from_ymd(
  Year const year,
  Month const month,
  Day const day)
{
  return 
      ymd_is_valid(year, month, day) 
    ? from_datenum<DATE>(ymd_to_datenum(year, month, day))
    : DATE::INVALID;
}


template<class DATE>
inline DATE
from_ymdi(
  int const ymdi)
{
  return 
      ymdi_is_valid(ymdi)
    ? from_datenum<DATE>(ymdi_to_datenum(ymdi))
    : DATE::INVALID;
}


template<class DATE>
inline DATE
from_week_date(
  Year const week_year,
  Week const week,
  Weekday const weekday)
{
  return
      week_date_is_valid(week_year, week, weekday)
    ? from_datenum<DATE>(week_date_to_datenum(week_year, week, weekday))
    : DATE::INVALID;
}


//------------------------------------------------------------------------------
// Accessors
//------------------------------------------------------------------------------

template<class DATE>
inline Datenum
get_datenum(
  DATE const date)
{
  return date.is_valid() ? date.get_datenum() : DATENUM_INVALID;
}


template<typename DATE>
inline cron::Day
get_day(
  DATE const date)
{
  return date.is_valid() ? date.get_ymd().day + 1 : cron::DAY_INVALID;
}


template<typename DATE>
inline cron::Month
get_month(
  DATE const date)
{
  return date.is_valid() ? date.get_ymd().month + 1 : cron::MONTH_INVALID;
}


template<class DATE>
inline OrdinalDate
get_ordinal_date(
  DATE const date)
{
  return date.is_valid() ? date.get_ordinal_date() : OrdinalDate::get_invalid();
}


template<typename DATE>
inline cron::Year
get_year(
  DATE const date)
{
  return date.is_valid() ? date.get_ymd().year : cron::YEAR_INVALID;
}


template<class DATE>
inline WeekDate
get_week_date(
  DATE const date)
{
  return date.is_valid() ? date.get_week_date() : WeekDate::get_invalid();
}


template<class DATE>
inline YmdDate
get_ymd(
  DATE const date)
{
  return date.is_valid() ? date.get_ymd() : YmdDate::get_invalid();
}


template<class DATE>
inline int
get_ymdi(
  DATE const date)
{
  return date.is_valid() ? date.get_ymdi() : YMDI_INVALID;
}


template<class DATE>
inline Weekday
get_weekday(
  DATE const date)
{
  return date.is_valid() ? date.get_weekday() : WEEKDAY_INVALID;
}


//------------------------------------------------------------------------------
// Arithmetic
//------------------------------------------------------------------------------

template<class DATE>
inline DATE
add(
  DATE const date,
  int shift)
{
  return 
      date.is_valid()
    ? from_offset<DATE>(date.get_offset() + shift)
    : date;
}  


template<class DATE>
inline DATE
subtract(
  DATE const date,
  int shift)
{
  return add(date, -shift);
}  


template<class DATE>
inline int
subtract(
  DATE const date0,
  DATE const date1)
{
  return
      date0.is_valid() && date1.is_valid()
    ? (int) date0.get_offset() - date1.get_offset()
    : std::numeric_limits<int>::min();
}


//------------------------------------------------------------------------------

}  // namespace cron


