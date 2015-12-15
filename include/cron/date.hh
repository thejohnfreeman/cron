#pragma once

#include <limits>
#include <string>

#include "cron/math.hh"
#include "cron/types.hh"
#include "exc.hh"

namespace alxs {
namespace cron {

//------------------------------------------------------------------------------
// Functions
//------------------------------------------------------------------------------

extern inline bool constexpr
is_leap_year(
  Year year)
{
  return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}


extern inline Day constexpr
days_per_month(
  Year year,
  Month month)
{
  return 
    month ==  3 || month ==  5 || month ==  8 || month == 10 ? 30
    : month == 1 ? (is_leap_year(year) ? 29 : 28)
    : 31;
}


extern inline bool constexpr
ymd_is_valid(
  Year year,
  Month month,
  Day day)
{
  return 
    month_is_valid(month)
    && year_is_valid(year)
    && in_interval(DAY_MIN, day, days_per_month(year, month));
}


extern inline Weekday constexpr
get_weekday(
  Datenum datenum)
{
  return 
    datenum_is_valid(datenum) 
    // 1200 March 1 is a Wednesday.
    ? ((WEDNESDAY + datenum) % 7) 
    : WEEKDAY_INVALID;
}


namespace impl {

inline Datenum constexpr
get_month_datenum(
  Month month)
{
  // The offset of the first day of each month relative to the most recent 
  // March 1 is the same for ordinary and leap years.  

  // The cumbersome construction is required for a constexpr function.
  // FIXME: It's probably not the most efficient at runtime.  Consider options.
  return 
    (month ==  0) ? 306 :
    (month ==  1) ? 337 :
    (month ==  2) ?   0 :
    (month ==  3) ?  31 :
    (month ==  4) ?  61 :
    (month ==  5) ?  92 :
    (month ==  6) ? 122 :
    (month ==  7) ? 153 :
    (month ==  8) ? 184 :
    (month ==  9) ? 214 :
    (month == 10) ? 245 :
    (month == 11) ? 275 :
    DATENUM_INVALID;
}


inline Datenum constexpr
ymd_to_datenum(
  Year year,
  Month month,
  Day day)
{
  return
    365 * year    // An ordinary year has 365 days.
    + year /   4  // Add a leap day every four years, 
    - year / 100  // ... but century years are not leap years,
    + year / 400  // ... but multiples of 400 are.
    + get_month_datenum(month)
    + day;
}


}  // namespace impl


extern inline Datenum constexpr
ymd_to_datenum(
  Year year,
  Month month,
  Day day)
{
  // Count years from 1200-03-01.
  return
    ymd_is_valid(year, month, day) 
    ? impl::ymd_to_datenum(year - 1200 - (month < 2 ? 1 : 0), month, day)
    : DATENUM_INVALID;
}


extern DateParts datenum_to_parts(Datenum datenum);

//------------------------------------------------------------------------------
// Generic date type.
//------------------------------------------------------------------------------

/**
 * This class represents a date as a datenum stored as an offset of type
 * `TRAITS::Offset` plus a fixed `TRAITS::base`.  
 */
template<class TRAITS>
class DateTemplate
{
public:

  typedef typename TRAITS::Offset Offset;

  static DateTemplate constexpr MIN         = TRAITS::min;
  static DateTemplate constexpr LAST        = (Offset) (TRAITS::max - 1);
  static DateTemplate constexpr MAX         = TRAITS::max;
  static DateTemplate constexpr INVALID     = TRAITS::invalid;
  static DateTemplate constexpr MISSING     = TRAITS::missing;
  static bool constexpr         USE_INVALID = TRAITS::use_invalid;

  // Constructors.

  constexpr DateTemplate()
    : offset_(TRAITS::use_invalid ? TRAITS::invalid : TRAITS::min)
  {
  }

  DateTemplate(
    DateTemplate const& date)
    : offset_(date.offset_)
  {
  }

  constexpr DateTemplate(
    Year year, 
    Month month, 
    Day day) 
    : DateTemplate(ymd_to_offset(year, month, day)) 
  {
  }

  DateTemplate(
    DateParts const& parts) 
    : DateTemplate(parts.year, parts.month, parts.day) 
  {
  }

  template<class OTHER_TRAITS> 
  DateTemplate(
    DateTemplate<OTHER_TRAITS> date)
    : DateTemplate(
          USE_INVALID && date.is_invalid() ? INVALID.get_offset()
        : USE_INVALID && date.is_missing() ? MISSING.get_offset()
        : datenum_to_offset(date.get_datenum()))
  {
  }

  static DateTemplate from_datenum(Datenum datenum) { return DateTemplate(datenum_to_offset(datenum)); }
  static DateTemplate from_offset(Offset offset) { return DateTemplate(validate_offset(offset)); }

  // Accessors.

  bool      is_valid()      const { return in_interval(MIN.offset_, offset_, MAX.offset_); }
  bool      is_invalid()    const { return offset_ == TRAITS::invalid; }
  bool      is_missing()    const { return offset_ == TRAITS::missing; }

  Offset    get_offset()    const { return offset_; }
  Datenum   get_datenum()   const { return is_valid() ? (TRAITS::base + offset_) : DATENUM_INVALID; }
  DateParts get_parts()     const { return datenum_to_parts(get_datenum()); }
  Weekday   get_weekday()   const { return is_valid() ? alxs::cron::get_weekday(get_datenum()) : WEEKDAY_INVALID; }

  bool is(DateTemplate const& o) const { return offset_ == o.offset_; }
  bool operator==(DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ == o.offset_; }
  bool operator!=(DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ != o.offset_; }
  bool operator< (DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ <  o.offset_; }
  bool operator<=(DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ <= o.offset_; }
  bool operator> (DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ >  o.offset_; }
  bool operator>=(DateTemplate const& o) const { return is_valid() && o.is_valid() && offset_ >= o.offset_; }

protected:

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
  validate_offset(
    Offset offset)
  {
    return 
      in_interval(TRAITS::min, offset, TRAITS::max) 
      ? offset 
      : on_error<DateRangeError>();
  }

  /**
   * Does not handle DATENUM_INVALID.
   */
  static Offset 
  datenum_to_offset(
    Datenum datenum)
  {
    if (! datenum_is_valid(datenum))
      return on_error<InvalidDateError>();
    Datenum const offset = datenum - (Datenum) TRAITS::base;
    return 
      in_interval<Datenum>(TRAITS::min, offset, TRAITS::max) 
      ? offset
      : on_error<DateRangeError>();
  }

  static Offset 
  ymd_to_offset(
    Year year, 
    Month month, 
    Day day)
  {
    return 
      ymd_is_valid(year, month, day)
      ? datenum_to_offset(ymd_to_datenum(year, month, day))
      : on_error<InvalidDateError>();
  }

private:

  constexpr DateTemplate(Offset offset) : offset_(offset) {}

  Offset offset_;

};


//------------------------------------------------------------------------------
// Static attributes
//------------------------------------------------------------------------------

template<class TRAITS>
DateTemplate<TRAITS> constexpr
DateTemplate<TRAITS>::MIN;

template<class TRAITS>
DateTemplate<TRAITS> constexpr
DateTemplate<TRAITS>::LAST;

template<class TRAITS>
DateTemplate<TRAITS> constexpr
DateTemplate<TRAITS>::MAX;

template<class TRAITS>
DateTemplate<TRAITS> constexpr
DateTemplate<TRAITS>::INVALID;

template<class TRAITS>
DateTemplate<TRAITS> constexpr
DateTemplate<TRAITS>::MISSING;

//------------------------------------------------------------------------------
// Concrete date types
//------------------------------------------------------------------------------

struct DateTraits
{
  typedef uint32_t Offset;

  static Datenum constexpr base     = -437985;
  static Offset  constexpr invalid  = 3652059;
  static Offset  constexpr missing  = 3652060;
  static Offset  constexpr min      =       0;   //  0001-01-01.
  static Offset  constexpr max      = 3652059;   // 10000-01-01.
  static bool    constexpr use_invalid = true;
};

typedef DateTemplate<DateTraits> Date;


// FIXME: Use a better name.

struct SafeDateTraits
{
  typedef uint32_t Offset;

  static Datenum constexpr base     = -437985;
  static Offset  constexpr invalid  = 3652059;
  static Offset  constexpr missing  = 3652060;
  static Offset  constexpr min      =       0;   //  0001-01-01.
  static Offset  constexpr max      = 3652059;   // 10000-01-01.
  static bool    constexpr use_invalid = false;
};

typedef DateTemplate<SafeDateTraits> SafeDate;

struct SmallDateTraits
{
  typedef uint16_t Offset;

  static Datenum constexpr base     = 281177;
  static Offset  constexpr invalid  = std::numeric_limits<Offset>::max() - 1;
  static Offset  constexpr missing  = invalid + 1;
  static Offset  constexpr min      = 0;         // 1970-01-01.
  static Offset  constexpr max      = invalid;   // 2149-05-05.
  static bool    constexpr use_invalid = true;
};

typedef DateTemplate<SmallDateTraits> SmallDate;

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Functions.
//------------------------------------------------------------------------------

template<class TRAITS> 
extern inline DateTemplate<TRAITS> 
shift(
  DateTemplate<TRAITS> date, 
  ssize_t shift)
{
  return 
      date.is_invalid() ? DateTemplate<TRAITS>::INVALID
    : date.is_missing() ? DateTemplate<TRAITS>::MISSING
    : DateTemplate<TRAITS>::from_offset(date.get_offset() + shift);
}


template<class TRAITS>
extern inline ssize_t
operator-(
  DateTemplate<TRAITS> date0,
  DateTemplate<TRAITS> date1)
{
  if (date0.is_valid() && date1.is_valid())
    return date0.get_offset() - date1.get_offset();
  else if (DateTemplate<TRAITS>::USE_INVALID)
    // FIXME: What do we do with invalid/missing values?
    return 0;
  else
    throw ValueError("can't subtract invalid dates");
}


template<class TRAITS> DateTemplate<TRAITS> operator+ (DateTemplate<TRAITS> date, ssize_t days) { return shift(date,  days); }
template<class TRAITS> DateTemplate<TRAITS> operator+=(DateTemplate<TRAITS>& date, ssize_t days) { return date = shift(date,  days); }
template<class TRAITS> DateTemplate<TRAITS> operator++(DateTemplate<TRAITS>& date) { return date = shift(date, 1); }
template<class TRAITS> DateTemplate<TRAITS> operator++(DateTemplate<TRAITS>& date, int) { auto old = date; date = shift(date, 1); return old; }
template<class TRAITS> DateTemplate<TRAITS> operator- (DateTemplate<TRAITS> date, ssize_t days) { return shift(date, -days); }
template<class TRAITS> DateTemplate<TRAITS> operator-=(DateTemplate<TRAITS>& date, ssize_t days) { return date = shift(date, -days); }
template<class TRAITS> DateTemplate<TRAITS> operator--(DateTemplate<TRAITS>& date) { return date = shift(date, -1); }
template<class TRAITS> DateTemplate<TRAITS> operator--(DateTemplate<TRAITS>& date, int) { auto old = date; date = shift(date, -1); return old; }

// FIXME: Use DateFormat.
extern DateParts iso_parse(std::string const& text);  

//------------------------------------------------------------------------------
// Syntactic sugar for date literals.

// FIXME: Is this even a good idea?

namespace ez {

namespace {

class MonthLiteral
{
private:

  class YearMonthLiteral
  {
  public:

    constexpr YearMonthLiteral(Year year, Month month) : year_(year), month_(month) {}
    Date operator/(Day day) const { return Date(year_, month_ - 1, day - 1); }

  private:

    Year year_;
    Month month_;

  };

public:

  constexpr MonthLiteral(Month month) : month_(month) {}
  constexpr YearMonthLiteral with_year(Year year) const { return YearMonthLiteral(year, month_); }
  
private:

  Month month_;

  friend YearMonthLiteral operator/(Year year, MonthLiteral const&);

};


inline
MonthLiteral::YearMonthLiteral
operator/(
  Year year,
  MonthLiteral const& month)
{
  return month.with_year(year);
}


}  // anonymous namespace

MonthLiteral constexpr JAN = MonthLiteral( 1);
MonthLiteral constexpr FEB = MonthLiteral( 2);
MonthLiteral constexpr MAR = MonthLiteral( 3);
MonthLiteral constexpr APR = MonthLiteral( 4);
MonthLiteral constexpr MAY = MonthLiteral( 5);
MonthLiteral constexpr JUN = MonthLiteral( 6);
MonthLiteral constexpr JUL = MonthLiteral( 7);
MonthLiteral constexpr AUG = MonthLiteral( 8);
MonthLiteral constexpr SEP = MonthLiteral( 9);
MonthLiteral constexpr OCT = MonthLiteral(10);
MonthLiteral constexpr NOV = MonthLiteral(11);
MonthLiteral constexpr DEC = MonthLiteral(12);


}  // namespace ez

//------------------------------------------------------------------------------

}  // namespace cron
}  // namespace alxs

