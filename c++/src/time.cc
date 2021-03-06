#include "cron/time.hh"

namespace cron {

//------------------------------------------------------------------------------

template class TimeTemplate<TimeTraits>;
template class TimeTemplate<SmallTimeTraits>;
template class TimeTemplate<NsecTimeTraits>;
template class TimeTemplate<Unix32TimeTraits>;
template class TimeTemplate<Unix64TimeTraits>;

//------------------------------------------------------------------------------

}  // namespace cron


