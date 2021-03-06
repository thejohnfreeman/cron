#include <cassert>

#include <Python.h>

// Note: Order is important here!
//
// In this, and only this, compilation unit, we need to #include the numpy
// headers without NO_IMPORT_ARRAY #defined.  In all other compilation units,
// this macro is defined, to make sure a single shared copy of the API is used.
// 
// See http://docs.scipy.org/doc/numpy/reference/c-api.array.html#importing-the-api.
//
// FIXME: Encapsulate this so that no human ever ever has to deal with it again.
#define PY_ARRAY_UNIQUE_SYMBOL cron_numpy
#define NPY_NO_DEPRECATED_API NPY_API_VERSION
#include <numpy/arrayobject.h>
#include <numpy/npy_math.h>
#include <numpy/ufuncobject.h>
#include <numpy/npy_3kcompat.h>

#include "py.hh"
#include "np_date.hh"
#include "numpy.hh"

using namespace py;
using namespace aslib;

//------------------------------------------------------------------------------

namespace {

ref<Object>
date_from_ymdi(
  Module* /* module */,
  Tuple* const args,
  Dict* const kw_args)
{
  static char const* arg_names[] = {"ymdi", "dtype", nullptr};
  PyObject* ymdi_arg;
  PyArray_Descr* dtype = DateDtype<PyDateDefault>::get();
  Arg::ParseTupleAndKeywords(
    args, kw_args, "O|$O!", arg_names,
    &ymdi_arg, &PyArrayDescr_Type, &dtype);
  auto ymdi_arr
    = Array::FromAny(ymdi_arg, NPY_INT32, 1, 1, NPY_ARRAY_CARRAY_RO);
  // OK, we have an aligned 1D int32 array.
  // FIXME: Encapsulate this, and check that it is a cron date dtype.
  auto api = (DateDtypeAPI*) dtype->c_metadata;
  assert(api != nullptr);

  return api->function_date_from_ymdi(ymdi_arr);
}


auto
functions 
  = Methods<Module>()
    .add<date_from_ymdi>            ("date_from_ymdi")
  ;
  

}  // anonymous namespace

//------------------------------------------------------------------------------

ref<Object>
set_up_numpy(
  Module* const module,
  Tuple* const args,
  Dict* kw_args)
{
  static char const* const arg_names[] = {nullptr};
  Arg::ParseTupleAndKeywords(args, kw_args, "", arg_names);

  // Import numpy stuff.
  if (_import_array() < 0) 
    throw ImportError("failed to import numpy.core.multiarray"); 
  if (_import_umath() < 0) 
    throw ImportError("failed to import numpy.core.umath");

  DateDtype<PyDate<cron::Date>>::add(module);
  DateDtype<PyDate<cron::Date16>>::add(module);

  module->AddFunctions(functions);

  return none_ref();
}


