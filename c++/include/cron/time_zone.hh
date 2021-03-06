#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "aslib/string.hh"
#include "cron/types.hh"
#include "cron/tzfile.hh"

namespace cron {

using namespace aslib;

//------------------------------------------------------------------------------

class TimeZone
{
public:

  TimeZone();
  TimeZone(TimeZone const&) = default;
  TimeZone(TimeZone&&) = default;
  TimeZone(TzFile const& tz_file, std::string const& name);
  TimeZone& operator=(TimeZone const&) = default;
  TimeZone& operator=(TimeZone&&) = default;

  std::string get_name() const { return name_; }

  TimeZoneParts get_parts(TimeOffset time) const;

  template<class TIME> 
  TimeZoneParts 
  get_parts(
    TIME time) 
    const
  {
    return get_parts(time.get_time_offset());
  }

  TimeZoneParts get_parts_local(TimeOffset, bool first=true) const;

  // FIXME: Take a LocalDatenumDaytick instead?
  TimeZoneParts get_parts_local(
    Datenum datenum, 
    Daytick daytick, 
    bool first=true) 
    const
   {
     return get_parts_local(
       ((long) datenum - DATENUM_UNIX_EPOCH) * SECS_PER_DAY 
         + (TimeOffset) (daytick / DAYTICK_PER_SEC),
       first);
   }

private:

  struct Entry
  {
    Entry(TimeOffset transition, TzFile::Type const& type);

    TimeOffset transition;
    TimeZoneParts parts;
  };

  std::string name_;
  std::vector<Entry> entries_;

};


//------------------------------------------------------------------------------

using TimeZone_ptr = std::shared_ptr<TimeZone const>;

/**
 * UTC time zone singleton.
 */
extern TimeZone_ptr const UTC;

/**
 * Returns the path to the current default zoneinfo directory.
 */
extern fs::Filename     get_zoneinfo_dir();

/**
 * Returns the path to the zoneinfo file for the time zone named 'name' in the
 * given zoneinfo directory.  If the time zone is not found, raises ValueError.
 */
extern fs::Filename     find_time_zone_file(std::string const& name, fs::Filename const& zoneinfo_dir);

/**
 * Returns the path to the zoneinfo file for the time zone named 'name' in the
 * default zoneinfo directory.
 */
extern inline fs::Filename
find_time_zone_file(
  std::string const& name)
{
  return find_time_zone_file(name, get_zoneinfo_dir());
}

/**
 * Returns a time zone named 'name' from the default zoneinfo directory.
 */
extern TimeZone_ptr     get_time_zone(std::string const& name);

/**
 * Returns a time zone named 'name' from the given zoneinfo directory.
 */
extern TimeZone         get_time_zone(std::string const& name, fs::Filename const& zoneinfo_dir);

extern std::string      get_system_time_zone_name();
extern TimeZone_ptr     get_system_time_zone();
extern TimeZone_ptr     get_display_time_zone();
extern void             set_display_time_zone(TimeZone_ptr tz);

extern inline void 
set_display_time_zone(
  std::string const& name)
{
  set_display_time_zone(get_time_zone(name));
}


//------------------------------------------------------------------------------

}  // namespace cron


