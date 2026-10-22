#pragma once
// Uniform helpers for the public Result<T> type.  The declaration contract in
// include/acsd/contracts.hpp is left untouched; these helpers only give the
// modules a consistent way to test and to unwrap a result.
#include "acsd/contracts.hpp"

namespace acsd {

template <class T>
bool ok(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& val(const Result<T>& result) {
  return *result.value;
}

}  // namespace acsd
