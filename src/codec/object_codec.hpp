#pragma once
// Object-table codec helpers for the generated typed Input/Output codecs.
//
// A field declared Ref<T> (or a span replaced by Ref<vector<T>>) never travels
// as JSON data: it travels as an object-table alias, because an array/model/
// provider reference is only valid against the WireObjects table of the current
// request.  This header is the single implementation of that aliasing rule.
//
// This header includes the runtime on purpose: apps/contract must keep using
// value_codec.hpp only.
#include "codec/value_codec.hpp"

#include <acsd/runtime/process.hpp>

namespace acsd::codec {

// An alias payload: {"$ref": "<alias>"} with no other fields.
template <class T>
Result<Ref<T>> decode_ref(rt::WireObjects& objects, const json& node,
                          std::string_view origin) {
  if (!node.is_object()) {
    return failure<Ref<T>>(Code::invalid_request, origin,
                           "a Ref field must be a {\"$ref\": alias} object");
  }
  for (auto entry = node.begin(); entry != node.end(); ++entry) {
    if (entry.key() != "$ref") {
      return failure<Ref<T>>(Code::invalid_request, origin,
                             "unexpected field '" + entry.key() +
                                 "' in a {\"$ref\": alias} object");
    }
  }
  const json* alias = member(node, "$ref");
  if (alias == nullptr || !alias->is_string()) {
    return failure<Ref<T>>(Code::invalid_request, origin,
                           "a Ref field needs a string \"$ref\" alias");
  }
  const std::string name = alias->get<std::string>();
  if (name.empty()) {
    return failure<Ref<T>>(Code::invalid_request, origin,
                           "the object alias is empty");
  }
  // WireObjects::resolve returns missing_dependency for an unknown alias; a
  // type-cookie mismatch is a runtime responsibility (FOUNDATION_API:
  // "按类型cookie安全resolve").
  return objects.resolve<T>(name);
}

template <class T>
Result<json> encode_ref(rt::WireObjects& objects, const Ref<T>& value,
                        std::string_view origin) {
  if (!value) {
    return failure<json>(Code::invalid_request, origin,
                         "a required Ref field is null; a missing object is "
                         "not encoded as an empty reference");
  }
  Result<Id> alias = objects.intern<T>(value);
  if (alias.code != Code::ok || !alias.value) {
    return remap<json>(alias);
  }
  json node = json::object();
  node["$ref"] = *alias.value;
  return ok<json>(std::move(node));
}

}  // namespace acsd::codec
