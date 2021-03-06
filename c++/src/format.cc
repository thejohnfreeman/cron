#include <algorithm>
#include <cassert>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "aslib/exc.hh"
#include "aslib/string_builder.hh"
#include "cron/format.hh"

namespace cron {

using namespace aslib;

using std::string;

//------------------------------------------------------------------------------
// Implementation helpers
//------------------------------------------------------------------------------

namespace {

/**
 * Helper class to hold modifier state in an escape sequence.
 */
struct Modifiers
{
  /**
   * Returns the numeric width, or a default value if it's not set.
   */
  int get_width(int def) const { return width == -1 ? def : width; }
  
  /**
   * Returns the pad character, or a default value if it's not set.
   */
  char get_pad(char def) const { return pad == 0 ? def : pad; }

  int width = -1;
  int precision = -1;
  char pad = 0;
  char str_case = 0;
  bool abbreviate = false;
  bool decimal = false;

};


bool
parse_modifiers(
  string const& pattern,
  size_t& pos,
  Modifiers& mods)
{
  switch (pattern[pos]) {
  case '.':
    if (mods.decimal)
      // Already saw a decimal point in this escape.
      throw ValueError("second decimal point in escape");
    else {
      mods.decimal = true;
      pos++;
    }
    break;

  case '0': case '1': case '2': case '3': case '4':
  case '5': case '6': case '7': case '8': case '9':
    {
      size_t const end = pattern.find_first_not_of("0123456789", pos);
      int const value = atoi(pattern.substr(pos, end).c_str());
      if (mods.decimal)
        mods.precision = value;
      else
        mods.width = value;
      pos = end;
    }
    break;

  case '#':
    mods.pad = pattern[++pos];
    if (pos++ == pattern.length())
      throw ValueError("unterminated escape in pattern");
    break;

  case '^':
  case '_':
    mods.str_case = pattern[pos];
    pos++;
    break;

  case '~':
    mods.abbreviate = true;
    pos++;
    break;

  case 'E':
    // FIXME: IMPLEMENT: Locale's alternative representation.
    throw TimeFormatError("not implemented: E");
    break;

  case 'O':
    // FIXME: IMPLEMENT: Locale's alternative numerical representation
    throw TimeFormatError("not implemented: O");
    break;

  default:
    // Did not match anything.
    return false;

  }

  // Matched something.
  return true;
}


void
format_string(
  StringBuilder& sb,
  Modifiers const& mods,
  std::string const& str)
{
  int const pad_length = mods.width - str.length();
  if (pad_length > 0)
    sb.pad(pad_length, mods.get_pad(' '));
  
  if (mods.str_case == '^' || mods.str_case == '_') {
    std::string formatted = str;
    std::transform(begin(formatted), end(formatted), begin(formatted), mods.str_case == '^' ? toupper : tolower);
    sb << formatted;
  }
  else
    sb << str;
}


bool
format_date(
  string const& pattern,
  size_t& pos,
  StringBuilder& sb,
  Modifiers const& mods,
  DateParts const& date)
{
  switch (pattern[pos]) {
  case 'b':
    format_string(sb, mods, mods.abbreviate ? get_month_abbr(date.month) : get_month_name(date.month));
    break;

  case 'd':
    sb.format(date.day + 1, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'D':
    // FIXME: Locale.
    throw TimeFormatError("not implemented: %D");
    break;

  case 'g':
    sb.format(date.week_year % 100, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'G':
    sb.format(date.week_year, mods.get_width(4), mods.get_pad('0'));
    break;

  case 'j':
    sb.format(date.ordinal + 1, mods.get_width(3), mods.get_pad('0'));
    break;

  case 'm':
    sb.format(date.month + 1, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'V':
    sb.format(date.week + 1, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'w':
    // FIXME: Generalize?
    sb.format((date.weekday + (7 - SUNDAY)) % 7, mods.get_width(1), mods.get_pad('0'));
    break;

  case 'W':
    format_string(sb, mods, mods.abbreviate ? get_weekday_abbr(date.weekday) : get_weekday_name(date.weekday));
    break;

  case 'y':
    sb.format(date.year % 100, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'Y':
    sb.format(date.year, mods.get_width(4), mods.get_pad('0'));
    break;

  default:
    // Did not match anything.
    return false;

  }

  // Matched an output character.
  pos++;
  return true;
}


bool
format_daytime(
  string const& pattern,
  size_t& pos,
  StringBuilder& sb,
  Modifiers const& mods,
  HmsDaytime const& daytime)
{
  switch (pattern[pos]) {
  case 'h':
    {
      unsigned const hour = daytime.hour % 12;
      sb.format(hour == 0 ? 12 : hour, mods.get_width(2), mods.get_pad('0'));
    }
    break;

  case 'H':
    sb.format(daytime.hour, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'k':
    {
      unsigned const msec = (daytime.second - (unsigned) daytime.second) * 1e+3;
      sb.format(msec, mods.get_width(3), mods.get_pad('0'));
    }
    break;

  case 'K':
    {
      unsigned const usec = (unsigned) ((daytime.second - (unsigned) daytime.second) * 1e+6) % 1000;
      sb.format(usec, mods.get_width(3), mods.get_pad('0'));
    }
    break;

  case 'l':
    {
      unsigned const nsec = (unsigned) ((daytime.second - (unsigned) daytime.second) * 1e+9) % 1000;
      sb.format(nsec, mods.get_width(3), mods.get_pad('0'));
    }
    break;

  case 'L':
    {
      unsigned const psec = (unsigned) ((daytime.second - (unsigned) daytime.second) * 1e+12) % 1000;
      sb.format(psec, mods.get_width(3), mods.get_pad('0'));
    }
    break;

  case 'M':
    sb.format(daytime.minute, mods.get_width(2), mods.get_pad('0'));
    break;

  case 'p':
    format_string(sb, mods, daytime.hour < 12 ? "AM" : "PM");
    break;

  case 'S':
    {
      unsigned const prec = std::max(0, mods.precision);
      unsigned long long const digits = daytime.second * pow10(prec) + 0.5;
      // Integer part.
      sb.format(digits / pow10(prec), mods.get_width(2), mods.get_pad('0'));
      if (mods.precision >= 0) {
        sb << '.';
        // Fractional part.
        if (mods.precision > 0) 
          sb.format(digits % pow10(prec), prec, '0');
      }
    }
    break;

  case 'T':
    // FIXME: Locale.
    throw TimeFormatError("not implemented: %T");
    break;

  default:
    // Did not match anything.
    return false;

  }

  // Matched an output character.
  pos++;
  return true;
}


bool
format_time_zone(
  string const& pattern,
  size_t& pos,
  StringBuilder& sb,
  Modifiers const& mods,
  TimeZoneParts const& time_zone)
{
  switch (pattern[pos]) {
  case 'o':
    sb << (time_zone.offset < 0 ? '-' : '+');
    sb.format(std::abs(time_zone.offset), mods.get_width(5), mods.get_pad('0'));
    break;

  case 'q':
    {
      unsigned const offset_min = std::abs(time_zone.offset) % SECS_PER_HOUR / SECS_PER_MIN;
      sb.format(offset_min, mods.get_width(2), mods.get_pad('0'));
    }
    break;

  case 'Q':
    {
      unsigned const offset_hour = std::abs(time_zone.offset) / SECS_PER_HOUR;
      sb.format(offset_hour, mods.get_width(2),mods.get_pad('0'));
    }
    break;

  case 'U':
    sb << (time_zone.offset < 0 ? '-' : '+');
    break;

  case 'Z':
    // FIXME: Time zone full name.
    if (mods.abbreviate)
      sb << time_zone.abbreviation;
    else
      throw TimeFormatError("not implemented: time zone full name");
    break;

  default:
    // Did not match anything.
    return false;

  }

  // Matched an output character.
  pos++;
  return true;
}


bool
format_time(
  string const& pattern,
  size_t& pos,
  StringBuilder& /*sb*/,
  Modifiers     const& /*mods*/,
  DateParts     const& /*date_parts*/,
  HmsDaytime  const& /*daytime_parts*/,
  TimeZoneParts const& /*time_zone_parts*/)
{
  switch (pattern[pos]) {
  case 'c':
    // FIXME: Locale.
    throw TimeFormatError("not implemented: %c");
    break;

  default:
    // Did not match anything.
    return false;

  }

  // Matched an output character.
  pos++;
  return true;
}


}  // anonymous namespace


//------------------------------------------------------------------------------

void 
Format::format(
  StringBuilder& sb,
  DateParts const* date_parts,
  HmsDaytime const* daytime_parts,
  TimeZoneParts const* time_zone_parts)
  const
{
  size_t pos = 0;
  while (true) {
    // Find the next escape character.
    size_t const next = pattern_.find('%', pos);
    if (next == std::string::npos) {
      // No next escape.  Copy the rest of the pattern, and we're done.
      sb << pattern_.substr(pos);
      break;
    }
    else if (next > pos)
      // Copy from the pattern until the next escape.
      sb << pattern_.substr(pos, next - pos);
    // Skip over the escape character.
    pos = next + 1;

    // Set up state for the escape sequence.
    Modifiers mods;

    // Scan characters in the escape sequence.
    for (bool done = false; ! done; ) {
      if (pos == pattern_.length())
        throw ValueError("unterminated escape in pattern");

      // Literal '%' escape.
      if (pattern_[pos] == '%') {
        sb << '%';
        pos++;
        break;
      }

      // Handle modifiers.
      if (parse_modifiers(pattern_, pos, mods))
        continue;

      // Handle escape codes for date components.
      if (date_parts != nullptr
          && format_date(pattern_, pos, sb, mods, *date_parts))
        break;
      if (daytime_parts != nullptr
          && format_daytime(pattern_, pos, sb, mods, *daytime_parts))
        break;
      if (time_zone_parts != nullptr
          && format_time_zone(pattern_, pos, sb, mods, *time_zone_parts))
        break;
      if (   date_parts      != nullptr
          && daytime_parts   != nullptr
          && time_zone_parts != nullptr
          && format_time(pattern_, pos, sb, mods, *date_parts, *daytime_parts, *time_zone_parts))
        break;

      // If we made it this far, it's not a valid character.
      throw TimeFormatError(
        std::string("unknown escape '") + pattern_[pos] + "'");
    }
  }
}



//------------------------------------------------------------------------------
// Class TimeFormat
//------------------------------------------------------------------------------

TimeFormat const TimeFormat::ISO_LOCAL_BASIC    = "%Y%m%dT%H%M%S";
TimeFormat const TimeFormat::ISO_LOCAL_EXTENDED = "%Y-%m-%dT%H:%M:%S";
TimeFormat const TimeFormat::ISO_UTC_BASIC      = "%Y%m%dT%H%M%SZ";
TimeFormat const TimeFormat::ISO_UTC_EXTENDED   = "%Y-%m-%dT%H:%M:%SZ";
TimeFormat const TimeFormat::ISO_ZONE_BASIC     = "%Y%m%dT%H%M%S%U%Q%q";
TimeFormat const TimeFormat::ISO_ZONE_EXTENDED  = "%Y-%m-%dT%H:%M:%S%U%Q:%q";

//------------------------------------------------------------------------------
// Class DateFormat
//------------------------------------------------------------------------------

DateFormat const DateFormat::ISO_CALENDAR_BASIC    = "%Y%m%d";
DateFormat const DateFormat::ISO_CALENDAR_EXTENDED = "%Y-%m-%d";
DateFormat const DateFormat::ISO_ORDINAL_BASIC     = "%Y%j";
DateFormat const DateFormat::ISO_ORDINAL_EXTENDED  = "%Y-%j";
DateFormat const DateFormat::ISO_WEEK_BASIC        = "%GW%V%^w";
DateFormat const DateFormat::ISO_WEEK_EXTENDED     = "%G-W%V-%^w";

//------------------------------------------------------------------------------
// Class DaytimeFormat
//------------------------------------------------------------------------------

DaytimeFormat const DaytimeFormat::ISO_BASIC("%H%M%S", "INVALD", "MISSNG");
DaytimeFormat const DaytimeFormat::ISO_EXTENDED         = "%H:%M:%S";
DaytimeFormat const DaytimeFormat::ISO_BASIC_MSEC       = "%H%M%.3S";
DaytimeFormat const DaytimeFormat::ISO_EXTENDED_MSEC    = "%H:%M:%.3S";
DaytimeFormat const DaytimeFormat::ISO_BASIC_USEC       = "%H%M%.6S";
DaytimeFormat const DaytimeFormat::ISO_EXTENDED_USEC    = "%H:%M:%.6S";
DaytimeFormat const DaytimeFormat::ISO_BASIC_NSEC       = "%H%M%.9S";
DaytimeFormat const DaytimeFormat::ISO_EXTENDED_NSEC    = "%H:%M:%.9S";

//------------------------------------------------------------------------------

// FIXME: Internationalize.

namespace {

string const
month_names[] = {
  "January",
  "February",
  "March",
  "April",
  "May",
  "June",
  "July",
  "August",
  "September",
  "October",
  "November",
  "December"
};


string const
month_abbrs[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


string const
weekday_names[] = {
  "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };


string const
weekday_abbrs[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };


}  // anonymous


inline string const& 
get_month_name(
  Month month)
{
  if (! month_is_valid(month))
    throw ValueError("bad month");
  return month_names[(int) month];
}


inline Month 
parse_month_name(
  string const& str)
{
  for (Month month = 0; month < 12; ++month)
    if (month_names[month] == str)
      return month;
  throw ValueError(string("bad month name: ") + str);
}


inline string const& 
get_month_abbr(
  Month month)
{
  if (! month_is_valid(month))
    throw ValueError("bad month");
  return month_abbrs[(int) month];
}


inline Month 
parse_month_abbr(
  string const& str)
{
  for (Month month = 0; month < 12; ++month)
    if (month_abbrs[month] == str)
      return month;
  throw ValueError(string("bad month abbr: ") + str);
}


inline string const& 
get_weekday_name(
  Weekday weekday)
{
  if (! weekday_is_valid(weekday))
    throw ValueError("bad weekday");
  return weekday_names[(int) weekday];
}


inline Weekday 
parse_weekday_name(
  string const& str)
{
  for (Weekday weekday = 0; weekday < 7; ++weekday)
    if (weekday_names[weekday] == str)
      return weekday;
  throw ValueError(string("bad weekday name: ") + str);
}


inline string const& 
get_weekday_abbr(
  Weekday weekday)
{
  if (! weekday_is_valid(weekday))
    throw ValueError("bad weekday");
  return weekday_abbrs[(int) weekday];
}


Weekday 
parse_weekday_abbr(
  string const& str)
{
  for (Weekday weekday = 0; weekday < 7; ++weekday)
    if (weekday_abbrs[weekday] == str)
      return weekday;
  throw ValueError(string("bad weekday abbr: ") + str);
}


//------------------------------------------------------------------------------

}  // namespace cron


