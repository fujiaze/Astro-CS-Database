#pragma once
// Aggregate public entry point.
//
// The scientific data model and free function signatures are owned by
// <acsd/model.hpp>; the fixed runtime declarations by <acsd/runtime/*.hpp>;
// reusable typed module functions by <acsd/modules/*.hpp>.  Every type and
// signature is defined exactly once; this header only aggregates them.
#include <acsd/model.hpp>

#include <acsd/runtime/types.hpp>
#include <acsd/runtime/task.hpp>
#include <acsd/runtime/memory.hpp>
#include <acsd/runtime/pipe.hpp>
#include <acsd/runtime/storage.hpp>
#include <acsd/runtime/process.hpp>
#include <acsd/runtime/diagnostics.hpp>

#include <acsd/modules/observation.hpp>
