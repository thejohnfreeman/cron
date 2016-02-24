#pragma once

#include <cmath>
#include <experimental/optional>
#include <iostream>

#include "cron/format.hh"
#include "cron/time.hh"
#include "cron/time_zone.hh"
#include "py.hh"
#include "PyDate.hh"
#include "PyDaytime.hh"
#include "PyTime.hh"
#include "PyTimeZone.hh"

namespace alxs {

using namespace py;

using std::experimental::optional;
using std::make_unique;
using std::string;
using std::unique_ptr;

//------------------------------------------------------------------------------
// Declarations
//------------------------------------------------------------------------------

StructSequenceType* get_time_parts_type();

// template<typename TIME> optional<TIME> convert_object(Object*);
// template<typename TIME> optional<TIME> convert_time_object(Object*);

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

namespace {

inline cron::TimeZone const&
check_time_zone_arg(
  PyObject* const arg)
{
  if (!PyTimeZone::Check(arg))
    throw Exception(PyExc_TypeError, "tz not a TimeZone");
  return *cast<PyTimeZone>(arg)->tz_;
}


}  // anonymous namespace

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

  // Number methods.
  static PyNumberMethods tp_as_number_;

  // Methods.
  static ref<Object> method_from_date_daytime   (PyTypeObject*, Tuple*, Dict*);
  static ref<Object> method_get_parts           (PyTime*,       Tuple*, Dict*);
  static Methods<PyTime> tp_methods_;

  // Getsets.
  static GetSets<PyTime> tp_getsets_;

  /** Date format used to generate the repr.  */
  static unique_ptr<cron::TimeFormat> repr_format_;
  /** Date format used to generate the str.  */
  static unique_ptr<cron::TimeFormat> str_format_;

private:

  static void tp_init(PyTime* self, Tuple* args, Dict* kw_args);
  static void tp_dealloc(PyTime* self);
  static ref<Unicode> tp_repr(PyTime* self);
  static ref<Unicode> tp_str(PyTime* self);

  static Type build_type(string const& type_name);

};


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

  // Build the repr format.
  repr_format_ = make_unique<cron::TimeFormat>(
    name + "(%0Y, %0m, %0d, %H, %M, %S)",  // FIXME: Not a ctor.
    name + ".INVALID",
    name + ".MISSING");

  // Build the str format.  Choose precision for seconds that captures actual
  // precision of the time class.
  std::string pattern = "%Y-%m-%dT%H:%M:%";
  size_t const precision = (size_t) ceil(log10(Time::DENOMINATOR));
  if (precision > 0) {
    pattern += ".";
    pattern += std::to_string(precision);
  }
  pattern += "SZ";
  str_format_ = make_unique<cron::TimeFormat>(pattern);

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
  // FIXME
  typename Time::Offset offset;
  Arg::ParseTuple(args, "|k", &offset);

  new(self) PyTime(Time::from_offset(offset));
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
  return Unicode::from((*repr_format_)(self->time_, cron::UTC));
}


template<typename TIME>
ref<Unicode>
PyTime<TIME>::tp_str(
  PyTime* const self)
{
  // FIXME: Not UTC?
  return Unicode::from((*str_format_)(self->time_, cron::UTC));  
}


//------------------------------------------------------------------------------
// Number methods
//------------------------------------------------------------------------------

template<typename TIME>
PyNumberMethods
PyTime<TIME>::tp_as_number_ = {
  (binaryfunc)  nullptr,                        // nb_add
  (binaryfunc)  nullptr,                        // nb_subtract
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
/* FIXME: Python 2.5
  (binaryfunc)  nullptr,                        // nb_matrix_multiply
  (binaryfunc)  nullptr,                        // nb_inplace_matrix_multiply
*/
};


//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

template<typename TIME>
ref<Object>
PyTime<TIME>::method_from_date_daytime(
  PyTypeObject* const type,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] 
    = {"date", "time", "tz", "first", nullptr};
  Object* date;
  Object* time;
  Object* tz_arg;
  bool first = true;
  Arg::ParseTupleAndKeywords(
    args, kw_args, "OOO|p", arg_names, &date, &time, &tz_arg, &first); 
  
  // Interpret the date, to obtain a datenum.
  cron::Datenum datenum;
  // If the date looks like a long, interpret it as a datenum.
  auto datenum_val = date->maybe_long_value();
  if (datenum_val && cron::datenum_is_valid(*datenum_val))
    datenum = *datenum_val;
  else {
    // Otherwise, look for a datenum attribute or property.
    auto datenum_attr = date->maybe_get_attr("datenum");
    if (datenum_attr) 
      datenum = (*datenum_attr)->long_value();
    else
      throw Exception(PyExc_TypeError, "not a date or datneum");
  }

  // Interpret the time, to obtain a daytick.
  cron::Daytick daytick;
  // If the time looks like a long, interpret it as a daytick.
  // FIXME: Use SSM instead?
  auto daytick_val = time->maybe_long_value();
  if (daytick_val && cron::daytick_is_valid(*daytick_val))
    daytick = *daytick_val;
  else {
    // Otherwise, look for a daytick attribute or property.
    auto daytick_attr = time->maybe_get_attr("daytick");
    if (daytick_attr)
      daytick = (*daytick_attr)->long_value();
    else 
      throw Exception(PyExc_TypeError, "not a time or daytick");
  }

  auto tz = check_time_zone_arg(tz_arg);
                                        
  return create(Time(datenum, daytick, tz, first), type);
}


template<typename TIME>
ref<Object>
PyTime<TIME>::method_get_parts(
  PyTime* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] = {"tz", nullptr};
  Object* tz_arg;
  Arg::ParseTupleAndKeywords(args, kw_args, "O", arg_names, &tz_arg);

  auto tz = check_time_zone_arg(tz_arg);
  auto parts = self->time_.get_parts(tz);

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


template<typename TIME>
Methods<PyTime<TIME>>
PyTime<TIME>::tp_methods_
  = Methods<PyTime>()
    .template add_class<method_from_date_daytime>   ("from_date_daytime")
    .template add<method_get_parts>                 ("get_parts")
  ;


//------------------------------------------------------------------------------
// Getsets
//------------------------------------------------------------------------------

template<typename TIME>
GetSets<PyTime<TIME>>
PyTime<TIME>::tp_getsets_ 
  = GetSets<PyTime>()
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
    // FIXME: Hack!  We'd like to provide a way for any PyTime instance to
    // return its datenum, for efficient manipulation by other PyTime instances,
    // without virtual methods.  PyTypeObject doesn't provide any slot for us to
    // stash this, so we requisition the deprecated tp_print slot.  This may
    // break in future Python versions, if that slot is reused.
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
    (richcmpfunc)         nullptr,                        // tp_richcompare
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

}  // namespace alxs

