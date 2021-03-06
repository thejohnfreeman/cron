#pragma once

#include <cmath>
#include <experimental/optional>
#include <iostream>
#include <unordered_map>

#include <Python.h>
#include <datetime.h>

#include "aslib/math.hh"
#include "cron/format.hh"
#include "cron/time.hh"
#include "cron/time_zone.hh"
#include "py.hh"
#include "PyDate.hh"
#include "PyDaytime.hh"
#include "PyTime.hh"
#include "PyTimeZone.hh"
#include "util.hh"

namespace aslib {

using namespace py;
using namespace std::literals;

using std::experimental::optional;
using std::make_unique;
using std::string;
using std::unique_ptr;

//------------------------------------------------------------------------------
// Declarations
//------------------------------------------------------------------------------

StructSequenceType* get_time_parts_type();

/**
 * Helper for converting a (localtime, tz) sequence to a time.
 */
template<typename TIME> inline TIME localtime_to_time(Sequence*);

/**
 * Helper for converting a (date, daytime, tz) sequence to a time.
 */
template<typename TIME> inline TIME date_daytime_to_time(Sequence*);

/**
 * Helper for converting a (Y,m,d,H,M,S,tz) sequence to a time.
 */
template<typename TIME> inline TIME parts_to_time(Sequence*);

/**
 * Attempts to decode various time objects.  The following objects are
 * recognized:
 *
 *   - PyTime instances
 *   - datetime.datetime instances
 */
template<typename TIME> inline optional<TIME> maybe_time(Object*);

/**
 * Converts an object to a time.  Beyond 'maybe_time()', recognizes the 
 * following: 
 *
 *   - 
 *
 * If the argument cannot be converted, raises a Python exception.
 */
template<typename TIME> inline TIME convert_to_time(Object*);

//------------------------------------------------------------------------------
// Virtual API
//------------------------------------------------------------------------------

/*
 * Provides an API with dynamic dispatch to PyTime objects.
 * 
 * The PyTime class, since it is a Python type, cannot be a C++ virtual class.
 * This mechanism interfaces the C++ virtual method mechanism with the Python
 * type system by mapping the Python type to a stub virtual C++ class.
 */
class PyTimeAPI
{
public:

  /*
   * Registers a virtual API for a Python type.
   */
  static void add(PyTypeObject* const type, std::unique_ptr<PyTimeAPI>&& api) 
    { apis_.emplace(type, std::move(api)); }

  /*
   * Returns the API for a Python object, or nullptr if it isn't a PyTime.
   */
  static PyTimeAPI const*
  get(
    PyTypeObject* const type)
  {
    auto api = apis_.find(type);
    return api == apis_.end() ? nullptr : api->second.get();
  }

  static PyTimeAPI const* get(PyObject* const obj)
    { return get(obj->ob_type);  }

  // API methods.
  virtual ref<Object>               from_local_datenum_daytick(cron::Datenum, cron::Daytick, cron::TimeZone const&, bool) const = 0; 
  virtual cron::TimeOffset          get_time_offset(Object* time) const = 0;
  virtual cron::Timetick            get_timetick(Object* time) const = 0;
  virtual bool                      is_invalid(Object* time) const = 0;
  virtual bool                      is_missing(Object* time) const = 0;
  virtual ref<Object>               now() const = 0;
  virtual cron::LocalDatenumDaytick to_local_datenum_daytick(Object* time, cron::TimeZone const& tz) const = 0;

private:

  static std::unordered_map<PyTypeObject*, std::unique_ptr<PyTimeAPI>> apis_;

};


//------------------------------------------------------------------------------
// Type class
//------------------------------------------------------------------------------

template<typename TIME>
class PyTime
  : public ExtensionType
{
public:

  using Time = TIME;

  /** 
   * Readies the Python type and adds it to `module` as `name`.  
   *
   * Should only be called once; this is not checked.
   */
  static void add_to(Module& module, string const& name);

  static Type type_;

  /**
   * Creates an instance of the Python type.
   */
  static ref<PyTime> create(Time time, PyTypeObject* type=&type_);

  /**
   * Returns true if 'object' is an instance of this type.
   */
  static bool Check(PyObject* object);

  PyTime(Time time) : time_(time) {}

  /**
   * The wrapped date instance.
   *
   * This is the only non-static data member.
   */
  Time const time_;

  class API 
  : public PyTimeAPI 
  {
  public:

    virtual cron::TimeOffset get_time_offset(Object* const time) const
      { return ((PyTime*) time)->time_.get_time_offset(); }

    virtual cron::Timetick get_timetick(Object* const time) const
      { return ((PyTime*) time)->time_.get_timetick(); }

    virtual ref<Object> from_local_datenum_daytick(cron::Datenum const datenum, cron::Daytick const daytick, cron::TimeZone const& tz, bool first) const
      { return PyTime::create(cron::from_local<Time>(datenum, daytick, tz, first)); }

    virtual ref<Object> now() const
      { return PyTime::create(cron::now<Time>()); }

    virtual bool is_invalid(Object* const time) const
      { return ((PyTime*) time)->time_.is_invalid(); }

    virtual bool is_missing(Object* const time) const
      { return ((PyTime*) time)->time_.is_missing(); }

    virtual cron::LocalDatenumDaytick to_local_datenum_daytick(Object* const time, cron::TimeZone const& tz) const
      { return cron::to_local_datenum_daytick(((PyTime*) time)->time_, tz); }

  };

private:

  static void           tp_init(PyTime*, Tuple* args, Dict* kw_args);
  static void           tp_dealloc(PyTime*);
  static ref<Unicode>   tp_repr(PyTime*);
  static ref<Unicode>   tp_str(PyTime*);
  static ref<Object>    tp_richcompare(PyTime*, Object*, int);

  // Number methods.
  static ref<Object>    nb_add                      (PyTime*, Object*, bool);
  static ref<Object>    nb_subtract                 (PyTime*, Object*, bool);
  static PyNumberMethods tp_as_number_;

  // Methods.
  static ref<Object>    method_get_parts            (PyTime*, Tuple*, Dict*);
  static ref<Object>    method_is_same              (PyTime*, Tuple*, Dict*);
  static Methods<PyTime> tp_methods_;

  // Getsets.
  static ref<Object> get_invalid                    (PyTime*, void*);
  static ref<Object> get_missing                    (PyTime*, void*);
  static ref<Object> get_offset                     (PyTime*, void*);
  static ref<Object> get_timetick                   (PyTime*, void*);
  static ref<Object> get_valid                      (PyTime*, void*);
  static GetSets<PyTime> tp_getsets_;

  /** Date format used to generate the repr.  */
  static unique_ptr<cron::TimeFormat> repr_format_;
  /** Date format used to generate the str.  */
  static unique_ptr<cron::TimeFormat> str_format_;

  static Type build_type(string const& type_name);

};


//------------------------------------------------------------------------------

template<typename TIME>
void
PyTime<TIME>::add_to(
  Module& module,
  string const& name)
{
  // Construct the type struct.
  type_ = build_type(string{module.GetName()} + "." + name);
  // Hand it to Python.
  type_.Ready();

  PyTimeAPI::add(&type_, std::make_unique<API>());

  // Build the repr format.
  repr_format_ = make_unique<cron::TimeFormat>(
    name + "(%0Y, %0m, %0d, %0H, %0M, %0S, UTC)",
    name + ".INVALID",
    name + ".MISSING");

  // Build the str format.  Choose precision for seconds that captures actual
  // precision of the time class.
  std::string pattern = "%Y-%m-%dT%H:%M:%";
  size_t const precision = (size_t) log10(Time::DENOMINATOR);
  if (precision > 0) {
    pattern += ".";
    pattern += std::to_string(precision);
  }
  pattern += "SZ";
  str_format_ = make_unique<cron::TimeFormat>(pattern);

  // Add in static data members.
  Dict* const dict = (Dict*) type_.tp_dict;
  assert(dict != nullptr);
  dict->SetItemString("DENOMINATOR" , Long::FromUnsignedLong(Time::DENOMINATOR));
  dict->SetItemString("INVALID"     , create(Time::INVALID));
  dict->SetItemString("MAX"         , create(Time::MAX));
  dict->SetItemString("MIN"         , create(Time::MIN));
  dict->SetItemString("MISSING"     , create(Time::MISSING));
  dict->SetItemString("RESOLUTION"  , Float::FromDouble(1.0 / Time::DENOMINATOR));

  // Add the type to the module.
  module.add(&type_);
}


template<typename TIME>
ref<PyTime<TIME>>
PyTime<TIME>::create(
  Time const time,
  PyTypeObject* const type)
{
  auto obj = ref<PyTime>::take(check_not_null(PyTime::type_.tp_alloc(type, 0)));

  // time_ is const to indicate immutablity, but Python initialization is later
  // than C++ initialization, so we have to cast off const here.
  new(const_cast<Time*>(&obj->time_)) Time{time};
  return obj;
}


template<typename TIME>
Type
PyTime<TIME>::type_;


template<typename TIME>
bool
PyTime<TIME>::Check(
  PyObject* const other)
{
  return static_cast<Object*>(other)->IsInstance((PyObject*) &type_);
}


//------------------------------------------------------------------------------
// Standard type methods
//------------------------------------------------------------------------------

template<typename TIME>
void
PyTime<TIME>::tp_init(
  PyTime* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  if (kw_args != nullptr)
    throw TypeError("function takes no keyword arguments");
  auto const num_args = args->Length();
  Time time;
  if (num_args == 0)
    ;
  else if (num_args == 1)
    time = convert_to_time<Time>(args->GetItem(0));
  else if (num_args == 2)
    time = localtime_to_time<Time>(args);
  else if (num_args == 3)
    time = date_daytime_to_time<Time>(args);
  else if (num_args == 7)
    time = parts_to_time<Time>(args);
  else
    throw TypeError("function takes 0, 1, 2, 3, or 7 arguments");

  new(self) PyTime(time);
}


template<typename TIME>
void
PyTime<TIME>::tp_dealloc(
  PyTime* const self)
{
  self->time_.~TimeTemplate();
  self->ob_type->tp_free(self);
}


template<typename TIME>
ref<Unicode>
PyTime<TIME>::tp_repr(
  PyTime* const self)
{
  return Unicode::from((*repr_format_)(self->time_, *cron::UTC));
}


template<typename TIME>
ref<Unicode>
PyTime<TIME>::tp_str(
  PyTime* const self)
{
  // FIXME: Not UTC?
  return Unicode::from((*str_format_)(self->time_, *cron::UTC));  
}


template<typename TIME>
ref<Object>
PyTime<TIME>::tp_richcompare(
  PyTime* const self,
  Object* const other,
  int const comparison)
{
  auto const other_time = maybe_time<Time>(other);
  if (!other_time)
    return not_implemented_ref();

  Time const t0 = self->time_;
  Time const t1 = *other_time;

  bool result;
  switch (comparison) {
  case Py_EQ: result = t0 == t1; break;
  case Py_GE: result = t0 >= t1; break;
  case Py_GT: result = t0 >  t1; break;
  case Py_LE: result = t0 <= t1; break;
  case Py_LT: result = t0 <  t1; break;
  case Py_NE: result = t0 != t1; break;
  default:    result = false; assert(false);
  }
  return Bool::from(result);
}


//------------------------------------------------------------------------------
// Number methods
//------------------------------------------------------------------------------

template<typename TIME>
inline ref<Object>
PyTime<TIME>::nb_add(
  PyTime* const self,
  Object* const other,
  bool /* ignored */)
{
  auto offset = other->maybe_double_value();
  if (offset)
    return 
      *offset == 0 ? ref<PyTime>::of(self)
      : create(self->time_ + *offset, self->ob_type);
  else
    return not_implemented_ref();
}


template<typename TIME>
inline ref<Object>
PyTime<TIME>::nb_subtract(
  PyTime* const self,
  Object* const other,
  bool right)
{
  if (right) 
    return not_implemented_ref();

  auto const other_time = maybe_time<Time>(other);
  if (other_time)
    if (self->time_.is_valid() && other_time->is_valid())
      return Float::from(self->time_ - *other_time);
    else
      return none_ref();

  auto offset = other->maybe_double_value();
  if (offset)
    return 
      *offset == 0 
      ? ref<PyTime>::of(self)  // Optimization: same time.
      : create(self->time_ - *offset, self->ob_type);

  return not_implemented_ref();
}


template<typename TIME>
PyNumberMethods
PyTime<TIME>::tp_as_number_ = {
  (binaryfunc)  wrap<PyTime, nb_add>,           // nb_add
  (binaryfunc)  wrap<PyTime, nb_subtract>,      // nb_subtract
  (binaryfunc)  nullptr,                        // nb_multiply
  (binaryfunc)  nullptr,                        // nb_remainder
  (binaryfunc)  nullptr,                        // nb_divmod
  (ternaryfunc) nullptr,                        // nb_power
  (unaryfunc)   nullptr,                        // nb_negative
  (unaryfunc)   nullptr,                        // nb_positive
  (unaryfunc)   nullptr,                        // nb_absolute
  (inquiry)     nullptr,                        // nb_bool
  (unaryfunc)   nullptr,                        // nb_invert
  (binaryfunc)  nullptr,                        // nb_lshift
  (binaryfunc)  nullptr,                        // nb_rshift
  (binaryfunc)  nullptr,                        // nb_and
  (binaryfunc)  nullptr,                        // nb_xor
  (binaryfunc)  nullptr,                        // nb_or
  (unaryfunc)   nullptr,                        // nb_int
  (void*)       nullptr,                        // nb_reserved
  (unaryfunc)   nullptr,                        // nb_float
  (binaryfunc)  nullptr,                        // nb_inplace_add
  (binaryfunc)  nullptr,                        // nb_inplace_subtract
  (binaryfunc)  nullptr,                        // nb_inplace_multiply
  (binaryfunc)  nullptr,                        // nb_inplace_remainder
  (ternaryfunc) nullptr,                        // nb_inplace_power
  (binaryfunc)  nullptr,                        // nb_inplace_lshift
  (binaryfunc)  nullptr,                        // nb_inplace_rshift
  (binaryfunc)  nullptr,                        // nb_inplace_and
  (binaryfunc)  nullptr,                        // nb_inplace_xor
  (binaryfunc)  nullptr,                        // nb_inplace_or
  (binaryfunc)  nullptr,                        // nb_floor_divide
  (binaryfunc)  nullptr,                        // nb_true_divide
  (binaryfunc)  nullptr,                        // nb_inplace_floor_divide
  (binaryfunc)  nullptr,                        // nb_inplace_true_divide
  (unaryfunc)   nullptr,                        // nb_index
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 5
  (binaryfunc)  nullptr,                        // nb_matrix_multiply
  (binaryfunc)  nullptr,                        // nb_inplace_matrix_multiply
#endif
};


//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

template<typename TIME>
ref<Object>
PyTime<TIME>::method_get_parts(
  PyTime* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] = {"time_zone", nullptr};
  Object* tz;
  Arg::ParseTupleAndKeywords(args, kw_args, "O", arg_names, &tz);

  auto parts = self->time_.get_parts(*convert_to_time_zone(tz));

  auto date_parts = get_date_parts_type()->New();
  date_parts->initialize(0, Long::FromLong(parts.date.year));
  date_parts->initialize(1, get_month_obj(parts.date.month + 1));
  date_parts->initialize(2, Long::FromLong(parts.date.day + 1));
  date_parts->initialize(3, Long::FromLong(parts.date.ordinal + 1));
  date_parts->initialize(4, Long::FromLong(parts.date.week_year));
  date_parts->initialize(5, Long::FromLong(parts.date.week + 1));
  date_parts->initialize(6, get_weekday_obj(parts.date.weekday));

  auto daytime_parts = get_daytime_parts_type()->New();
  daytime_parts->initialize(0, Long::FromLong(parts.daytime.hour));
  daytime_parts->initialize(1, Long::FromLong(parts.daytime.minute));
  daytime_parts->initialize(2, Float::FromDouble(parts.daytime.second));

  auto time_zone_parts = get_time_zone_parts_type()->New();
  time_zone_parts->initialize(0, Long::FromLong(parts.time_zone.offset));
  time_zone_parts->initialize(1, Unicode::from(parts.time_zone.abbreviation));
  time_zone_parts->initialize(2, Bool::from(parts.time_zone.is_dst));

  auto time_parts = get_time_parts_type()->New();
  time_parts->initialize(0, std::move(date_parts));
  time_parts->initialize(1, std::move(daytime_parts));
  time_parts->initialize(2, std::move(time_zone_parts));

  return std::move(time_parts);
}


// We call this method "is_same" because "is" is a keyword in Python.
template<typename TIME>
ref<Object>
PyTime<TIME>::method_is_same(
  PyTime* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] = {"other", nullptr};
  Object* other;
  Arg::ParseTupleAndKeywords(args, kw_args, "O", arg_names, &other);

  auto const other_time = maybe_time<Time>(other);
  return Bool::from(other_time && self->time_.is(*other_time));
}


template<typename TIME>
Methods<PyTime<TIME>>
PyTime<TIME>::tp_methods_
  = Methods<PyTime>()
    .template add<method_get_parts>                     ("get_parts")
    .template add<method_is_same>                       ("is_same")
  ;


//------------------------------------------------------------------------------
// Getsets
//------------------------------------------------------------------------------

template<typename TIME>
ref<Object>
PyTime<TIME>::get_invalid(
  PyTime* const self,
  void* /* closure */)
{
  return Bool::from(self->time_.is_invalid());
}


template<typename TIME>
ref<Object>
PyTime<TIME>::get_missing(
  PyTime* const self,
  void* /* closure */)
{
  return Bool::from(self->time_.is_missing());
}


template<typename TIME>
ref<Object>
PyTime<TIME>::get_offset(
  PyTime* const self,
  void* /* closure */)
{
  return Long::FromUnsignedLong(self->time_.get_offset());
}


template<typename TIME>
ref<Object>
PyTime<TIME>::get_timetick(
  PyTime* const self,
  void* /* closure */)
{
  return Long::from(self->time_.get_timetick());
}


template<typename TIME>
ref<Object>
PyTime<TIME>::get_valid(
  PyTime* const self,
  void* /* closure */)
{
  return Bool::from(self->time_.is_valid());
}


template<typename TIME>
GetSets<PyTime<TIME>>
PyTime<TIME>::tp_getsets_ 
  = GetSets<PyTime>()
    .template add_get<get_invalid>      ("invalid")
    .template add_get<get_missing>      ("missing")
    .template add_get<get_offset>       ("offset")
    .template add_get<get_timetick>     ("timetick")
    .template add_get<get_valid>        ("valid")
  ;


//------------------------------------------------------------------------------
// Other members
//------------------------------------------------------------------------------

template<typename TIME>
unique_ptr<cron::TimeFormat>
PyTime<TIME>::repr_format_;


template<typename TIME>
unique_ptr<cron::TimeFormat>
PyTime<TIME>::str_format_;


//------------------------------------------------------------------------------
// Type object
//------------------------------------------------------------------------------

template<typename TIME>
Type
PyTime<TIME>::build_type(
  string const& type_name)
{
  return PyTypeObject{
    PyVarObject_HEAD_INIT(nullptr, 0)
    (char const*)         strdup(type_name.c_str()),      // tp_name
    (Py_ssize_t)          sizeof(PyTime),                 // tp_basicsize
    (Py_ssize_t)          0,                              // tp_itemsize
    (destructor)          wrap<PyTime, tp_dealloc>,       // tp_dealloc
    (printfunc)           nullptr,                        // tp_print
    (getattrfunc)         nullptr,                        // tp_getattr
    (setattrfunc)         nullptr,                        // tp_setattr
                          nullptr,                        // tp_reserved
    (reprfunc)            wrap<PyTime, tp_repr>,          // tp_repr
    (PyNumberMethods*)    &tp_as_number_,                 // tp_as_number
    (PySequenceMethods*)  nullptr,                        // tp_as_sequence
    (PyMappingMethods*)   nullptr,                        // tp_as_mapping
    (hashfunc)            nullptr,                        // tp_hash
    (ternaryfunc)         nullptr,                        // tp_call
    (reprfunc)            wrap<PyTime, tp_str>,           // tp_str
    (getattrofunc)        nullptr,                        // tp_getattro
    (setattrofunc)        nullptr,                        // tp_setattro
    (PyBufferProcs*)      nullptr,                        // tp_as_buffer
    (unsigned long)       Py_TPFLAGS_DEFAULT
                          | Py_TPFLAGS_BASETYPE,          // tp_flags
    (char const*)         nullptr,                        // tp_doc
    (traverseproc)        nullptr,                        // tp_traverse
    (inquiry)             nullptr,                        // tp_clear
    (richcmpfunc)         wrap<PyTime, tp_richcompare>,   // tp_richcompare
    (Py_ssize_t)          0,                              // tp_weaklistoffset
    (getiterfunc)         nullptr,                        // tp_iter
    (iternextfunc)        nullptr,                        // tp_iternext
    (PyMethodDef*)        tp_methods_,                    // tp_methods
    (PyMemberDef*)        nullptr,                        // tp_members
    (PyGetSetDef*)        tp_getsets_,                    // tp_getset
    (_typeobject*)        nullptr,                        // tp_base
    (PyObject*)           nullptr,                        // tp_dict
    (descrgetfunc)        nullptr,                        // tp_descr_get
    (descrsetfunc)        nullptr,                        // tp_descr_set
    (Py_ssize_t)          0,                              // tp_dictoffset
    (initproc)            wrap<PyTime, tp_init>,          // tp_init
    (allocfunc)           nullptr,                        // tp_alloc
    (newfunc)             PyType_GenericNew,              // tp_new
    (freefunc)            nullptr,                        // tp_free
    (inquiry)             nullptr,                        // tp_is_gc
    (PyObject*)           nullptr,                        // tp_bases
    (PyObject*)           nullptr,                        // tp_mro
    (PyObject*)           nullptr,                        // tp_cache
    (PyObject*)           nullptr,                        // tp_subclasses
    (PyObject*)           nullptr,                        // tp_weaklist
    (destructor)          nullptr,                        // tp_del
    (unsigned int)        0,                              // tp_version_tag
    (destructor)          nullptr,                        // tp_finalize
  };
}


//------------------------------------------------------------------------------
// Helpers

using PyTimeDefault = PyTime<cron::Time>;

template<typename TIME>
inline TIME
localtime_to_time(
  Sequence* const parts)
{
  assert(parts->Length() == 2);
  auto const localtime  = cast<Sequence>(parts->GetItem(0));
  if (localtime->Length() != 2)
    throw TypeError("not a localtime: "s + *localtime->Repr());
  auto const datenum    = to_datenum(localtime->GetItem(0));
  auto const daytick    = to_daytick(localtime->GetItem(1));
  auto const tz         = convert_to_time_zone(parts->GetItem(1));
  return TIME(datenum, daytick, *tz);
}


template<typename TIME>
inline TIME
date_daytime_to_time(
  Sequence* const parts)
{
  assert(parts->Length() == 3);
  auto const datenum    = to_datenum(parts->GetItem(0));
  auto const daytick    = to_daytick(parts->GetItem(1));
  auto const tz         = convert_to_time_zone(parts->GetItem(2));
  return TIME(datenum, daytick, *tz);
}


template<typename TIME>
inline TIME
parts_to_time(
  Sequence* const parts)
{
  assert(parts->Length() == 7);
  auto const year   = parts->GetItem(0)->long_value();
  auto const month  = parts->GetItem(1)->long_value() - 1;
  auto const day    = parts->GetItem(2)->long_value() - 1;
  auto const hour   = parts->GetItem(3)->long_value();
  auto const minute = parts->GetItem(4)->long_value();
  auto const second = parts->GetItem(5)->double_value();
  auto const tz     = convert_to_time_zone(parts->GetItem(6));
  return TIME(year, month, day, hour, minute, second, *tz);
}


template<typename TIME>
optional<TIME>
maybe_time(
  Object* const obj)
{
  // An object of the same type?
  if (PyTime<TIME>::Check(obj))
    return cast<PyTime<TIME>>(obj)->time_;

  // A different instance of the time class?
  auto const api = PyTimeAPI::get(obj);
  if (api != nullptr) 
    return 
        api->is_invalid(obj) ? TIME::INVALID
      : api->is_missing(obj) ? TIME::MISSING
      : TIME::from_timetick(api->get_timetick(obj));

  // A 'datetime.datetime'?
  if (PyDateTimeAPI == nullptr)
    PyDateTime_IMPORT;
  if (PyDateTime_Check(obj)) {
    // First, make sure it's localized.
    auto const tzinfo = obj->GetAttrString("tzinfo", false);
    if (tzinfo == None)
      throw py::ValueError("unlocalized datetime doesn't represent a time");
    auto const tz = maybe_time_zone(tzinfo);
    if (tz == nullptr)
      throw py::ValueError(
        string("unknown tzinfo: ") + tzinfo->Repr()->as_utf8_string());
    
    // FIXME: Provide a all-integer ctor with (sec, usec).
    return TIME(
      PyDateTime_GET_YEAR(obj),
      PyDateTime_GET_MONTH(obj) - 1,
      PyDateTime_GET_DAY(obj) - 1,
      PyDateTime_DATE_GET_HOUR(obj),
      PyDateTime_DATE_GET_MINUTE(obj),
      PyDateTime_DATE_GET_SECOND(obj) 
      + PyDateTime_DATE_GET_MICROSECOND(obj) * 1e-6,
      *tz,
      true);
  }

  // No type match.
  return {};
}


template<typename TIME>
TIME
convert_to_time(
  Object* const obj)
{
  if (obj == None)
    // Use the default value.
    return TIME{};

  auto time = maybe_time<TIME>(obj);
  if (time)
    return *time;

  if (Sequence::Check(obj)) {
    auto const parts = cast<Sequence>(obj);
    auto const length = parts->Length();
    if (length == 2)
      return localtime_to_time<TIME>(parts);
    else if (length == 3)
      return date_daytime_to_time<TIME>(parts);
    else if (length == 7)
      return parts_to_time<TIME>(parts);
  }

  // FIXME: Parse strings.

  throw py::TypeError("can't convert to a time: "s + *obj->Repr());
}


//------------------------------------------------------------------------------

#ifdef __clang__
// Use explicit instantiation for the main instances.
// FIXME: GCC 5.2.1 generates PyTime<>::type_ in BSS, which breaks linking.
extern template class PyTime<cron::Time>;
extern template class PyTime<cron::SmallTime>;
extern template class PyTime<cron::NsecTime>;
extern template class PyTime<cron::Unix32Time>;
extern template class PyTime<cron::Unix64Time>;
#endif

//------------------------------------------------------------------------------

}  // namespace aslib


