#include <memory>
#include <string>

#include "py.hh"
#include "PyTime.hh"
#include "PyTimeZone.hh"

namespace aslib {

using std::string;
using namespace std::literals;

using namespace py;

//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

StructSequenceType*
get_time_zone_parts_type()
{
  static StructSequenceType type;

  if (type.tp_name == nullptr) {
    // Lazy one-time initialization.
    static PyStructSequence_Field fields[] = {
      {(char*) "offset"         , nullptr},
      {(char*) "abbreviation"   , nullptr},
      {(char*) "is_dst"         , nullptr},
      {nullptr, nullptr}
    };
    static PyStructSequence_Desc desc{
      (char*) "TimeZoneParts",                              // name
      nullptr,                                              // doc
      fields,                                               // fields
      3                                                     // n_in_sequence
    };

    StructSequenceType::InitType(&type, &desc);
  }

  return &type;
}


inline ref<Object>
make_time_zone_parts(
  cron::TimeZoneParts const& parts)
{
  auto parts_obj = get_time_zone_parts_type()->New();
  parts_obj->initialize(0, Long::from(parts.offset));
  parts_obj->initialize(1, Unicode::from(parts.abbreviation));
  parts_obj->initialize(2, Bool::from(parts.is_dst));
  return std::move(parts_obj);
}


/**
 * Interprets 'obj' as a time zone.
 */
cron::TimeZone_ptr
maybe_time_zone(
  Object* const obj)
{
  if (PyTimeZone::Check(obj))
    return cast<PyTimeZone>(obj)->tz_;

  // If it has a 'zone' attribute, as pytz time zone objects, interpret that as
  // a time zone name.
  auto zone_attr = obj->GetAttrString("zone", false);
  if (zone_attr != nullptr) {
    auto const tz_name = zone_attr->Str()->as_utf8_string();
    try {
      return cron::get_time_zone(tz_name);
    }
    catch (aslib::ValueError) {
      throw py::ValueError(string("not a time zone: ") + tz_name);
    }
  }
    
  // Not a time zone object.
  return nullptr;
}


/**
 * Attempts to convert 'obj' to a time zone.
 */
cron::TimeZone_ptr
convert_to_time_zone(
  Object* const obj)
{
  auto const tz = maybe_time_zone(obj);
  if (tz != nullptr)
    return tz;

  // If it's a string, interpret it as a time zone name.
  if (Unicode::Check(obj)) {
    auto const tz_name = cast<Unicode>(obj)->as_utf8_string();
    try {
      return cron::get_time_zone(tz_name);
    }
    catch (aslib::ValueError) {
      throw py::ValueError(string("not a time zone: ") + tz_name);
    }
  }

  // Can't convert to a time zone.
  throw py::TypeError("can't convert to a time zone: "s + *obj->Repr());
}


// FIXME: This is a hack to translate C++ into Python exceptions.  Instead, wrap
// excpetions comprehensively.

inline cron::TimeZoneParts 
get_parts_local(
  cron::TimeZone_ptr const tz,
  cron::Datenum const datenum,
  cron::Daytick const daytick,
  bool const first)
{
  try {
    return tz->get_parts_local(datenum, daytick, first);
  }
  catch (cron::NonexistentLocalTime) {
    // FIXME: Use a custom exception class.
    throw py::ValueError("nonexistent local time");
  }
}


//------------------------------------------------------------------------------

void
PyTimeZone::add_to(
  Module& module,
  string const& name)
{
  // Construct the type struct.
  type_ = build_type(string{module.GetName()} + "." + name);
  // Hand it to Python.
  type_.Ready();

  // Add the type to the module.
  module.add(&type_);
}


//------------------------------------------------------------------------------
// Data members
//------------------------------------------------------------------------------

Type
PyTimeZone::type_;

//------------------------------------------------------------------------------
// Standard type methods
//------------------------------------------------------------------------------

void
PyTimeZone::tp_dealloc(
  PyTimeZone* const self)
{
  self->ob_type->tp_free(self);
}


ref<Unicode>
PyTimeZone::tp_repr(
  PyTimeZone* const self)
{
  string full_name{self->ob_type->tp_name};
  string type_name = full_name.substr(full_name.rfind('.') + 1);
  auto const repr = type_name + "('" + self->tz_->get_name() + "')";
  return Unicode::from(repr);
}


ref<Object>
PyTimeZone::tp_call(
  PyTimeZone* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  // We accept:
  //   tz(time)             == tz.at(time)
  //   tz((date, daytime)   == tz.at_local(date, daytime)
  //   tz(date, daytime)    == tz.at_local(date, daytime)

  static char const* const arg_names[] = {"date", "daytime", "first", nullptr};
  Object* arg;
  Object* daytime = nullptr;
  int first = true;
  Arg::ParseTupleAndKeywords(
    args, kw_args, "O|O$p", arg_names, &arg, &daytime, &first);

  if (daytime == nullptr) {
    // One arg.  Is it a local time?
    if (Sequence::Check(arg)) {
      auto const local = cast<Sequence>(arg);
      if (local->Length() == 2) {
        auto const datenum = to_datenum(local->GetItem(0));
        auto const daytick = to_daytick(local->GetItem(1));
        auto const parts = get_parts_local(self->tz_, datenum, daytick, first);
        return make_time_zone_parts(parts);
      }
      else
        throw TypeError("local time arg must be (date, daytime)");
    }

    // Is it a time object?
    auto const api = PyTimeAPI::get(arg);
    if (api != nullptr)
      return make_time_zone_parts(
        self->tz_->get_parts(api->get_time_offset(arg)));

    throw TypeError("arg not a time or local time");
  }    

  else {
    auto const datenum = to_datenum(arg);
    auto const daytick = to_daytick(daytime);
    auto const parts = get_parts_local(self->tz_, datenum, daytick, first);
    return make_time_zone_parts(parts);
  }
}


ref<Unicode>
PyTimeZone::tp_str(
  PyTimeZone* const self)
{
  return Unicode::from(self->tz_->get_name());  
}


void
PyTimeZone::tp_init(
  PyTimeZone* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  Object* obj = nullptr;
  Arg::ParseTuple(args, "O", &obj);

  new(self) PyTimeZone(convert_to_time_zone(obj));
}


//------------------------------------------------------------------------------
// Number methods
//------------------------------------------------------------------------------

inline ref<Object>
PyTimeZone::nb_matrix_multiply(
  PyTimeZone* const self,
  Object* const other,
  bool const right)
{
  // The time zone should be the RHS.
  if (!right)
    return not_implemented_ref();

  auto const api = PyTimeAPI::get(other);
  if (api != nullptr) 
    // The LHS is a time.  Localize it.
    return make_local(api->to_local_datenum_daytick(other, *self->tz_));

  if (Sequence::Check(other)) {
    auto const local = cast<Sequence>(other);
    if (local->Length() == 2) {
      auto const datenum = to_datenum(local->GetItem(0));
      auto const daytick = to_daytick(local->GetItem(1));
      return PyTimeDefault::create(
        PyTimeDefault::Time(datenum, daytick, *self->tz_));
    }
  }

  return not_implemented_ref();
}


PyNumberMethods
PyTimeZone::tp_as_number_ = {
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
#if PY_MAJOR_VERSION >= 3 && PY_MINOR_VERSION >= 5
  (binaryfunc)  wrap<PyTimeZone, nb_matrix_multiply>, // nb_matrix_multiply
  (binaryfunc)  nullptr,                        // nb_inplace_matrix_multiply
#endif
};


//------------------------------------------------------------------------------
// Methods
//------------------------------------------------------------------------------

ref<Object>
PyTimeZone::method_at(
  PyTimeZone* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] = {"time", nullptr};
  Object* time;
  Arg::ParseTupleAndKeywords(args, kw_args, "O", arg_names, &time);

  auto const api = PyTimeAPI::get(time);
  if (api == nullptr)
    throw py::TypeError("not a time: "s + *time->Repr());

  return make_time_zone_parts(self->tz_->get_parts(api->get_time_offset(time)));
}


ref<Object>
PyTimeZone::method_at_local(
  PyTimeZone* const self,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* const arg_names[] = {"date", "daytime", "first", nullptr};
  Object* arg;
  Object* daytime = nullptr;
  int first = true;
  Arg::ParseTupleAndKeywords(
    args, kw_args, "O|O$p", arg_names, &arg, &daytime, &first);

  cron::Datenum datenum;
  cron::Daytick daytick;

  if (daytime == nullptr) 
    // One arg.  Is it a local time?
    if (Sequence::Check(arg)) {
      auto const local = cast<Sequence>(arg);
      if (local->Length() == 2) {
        datenum = to_datenum(local->GetItem(0));
        daytick = to_daytick(local->GetItem(1));
      }
      else
        throw TypeError("local time arg must be (date, daytime)");
    }
    else
      throw TypeError("arg is not a local time");

  else {
    datenum = to_datenum(arg);
    daytick = to_daytick(daytime);
  }

  auto const parts = get_parts_local(self->tz_, datenum, daytick, first);
  return make_time_zone_parts(parts);
}


Methods<PyTimeZone>
PyTimeZone::tp_methods_
  = Methods<PyTimeZone>()
    .template add<method_at>                    ("at")
    .template add<method_at_local>              ("at_local")
  ;


//------------------------------------------------------------------------------
// Getsets
//------------------------------------------------------------------------------

ref<Object>
PyTimeZone::get_name(
  PyTimeZone* const self,
  void* /* closure */)
{
  return Unicode::from(self->tz_->get_name());
}


GetSets<PyTimeZone>
PyTimeZone::tp_getsets_ 
  = GetSets<PyTimeZone>()
    .template add_get<get_name>         ("name")
  ;


//------------------------------------------------------------------------------
// Type object
//------------------------------------------------------------------------------

Type
PyTimeZone::build_type(
  string const& type_name)
{
  return PyTypeObject{
    PyVarObject_HEAD_INIT(nullptr, 0)
    (char const*)         strdup(type_name.c_str()),      // tp_name
    (Py_ssize_t)          sizeof(PyTimeZone),             // tp_basicsize
    (Py_ssize_t)          0,                              // tp_itemsize
    (destructor)          wrap<PyTimeZone, tp_dealloc>,   // tp_dealloc
    (printfunc)           nullptr,                        // tp_print
    (getattrfunc)         nullptr,                        // tp_getattr
    (setattrfunc)         nullptr,                        // tp_setattr
                          nullptr,                        // tp_reserved
    (reprfunc)            wrap<PyTimeZone, tp_repr>,      // tp_repr
    (PyNumberMethods*)    &tp_as_number_,                 // tp_as_number
    (PySequenceMethods*)  nullptr,                        // tp_as_sequence
    (PyMappingMethods*)   nullptr,                        // tp_as_mapping
    (hashfunc)            nullptr,                        // tp_hash
    (ternaryfunc)         wrap<PyTimeZone, tp_call>,      // tp_call
    (reprfunc)            wrap<PyTimeZone, tp_str>,       // tp_str
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
    (initproc)            wrap<PyTimeZone, tp_init>,      // tp_init
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

}  // namespace aslib


