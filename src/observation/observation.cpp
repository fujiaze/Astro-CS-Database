#include "observation/observation.hpp"

#include <cmath>
#include <limits>

#include "model/hash.hpp"

namespace acsd::observation {

bool dq_usable(std::uint16_t dq, bool has_influence) {
  if ((dq & kDqDisqualifying) != 0u) return false;
  if ((dq & kDqRepaired) != 0u) return has_influence;
  return true;
}

U64 element_count(const std::vector<U64>& shape) {
  U64 total = 1;
  for (U64 extent : shape) {
    if (extent == 0) return 0;
    if (total > (~0ull) / extent) return 0;
    total *= extent;
  }
  return total;
}

bool array_has_count(const Array& array, U64 count) {
  return element_count(array.shape) == count;
}

namespace {

template <class T, class Decoder>
Result<std::vector<T>> read_typed(Context& context, ArrayRef array, Decoder decoder,
                                   const char* label) {
  if (!array) {
    return model::fail<std::vector<T>>(Code::invalid_model,
                                       std::string("the ") + label + " array is missing",
                                       "observation.read", "the array cannot be read");
  }
  const U64 count = element_count(array->shape);
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    return model::fail<std::vector<T>>(view.code, std::string("the ") + label + " array cannot be read",
                                       "observation.read", "the array cannot be read");
  }
  return model::ok_result(decoder(val(view).bytes));
}

}  // namespace

Result<std::vector<double>> read_f64_values(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the f64 array is missing",
                                            "observation.read", "the array cannot be read");
  }
  if (array->dtype != DType::f64) {
    return model::fail<std::vector<double>>(Code::invalid_model, "the array is not f64",
                                            "observation.read", "the array cannot be read");
  }
  return read_typed<double>(context, array,
                            [](std::span<const std::byte> bytes) {
                              return model::decode_f64(bytes);
                            },
                            "f64");
}

Result<std::vector<std::uint16_t>> read_u16_values(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_model, "the u16 array is missing",
                                                   "observation.read", "the array cannot be read");
  }
  if (array->dtype != DType::u16) {
    return model::fail<std::vector<std::uint16_t>>(Code::invalid_model, "the array is not u16",
                                                   "observation.read", "the array cannot be read");
  }
  return read_typed<std::uint16_t>(context, array,
                                   [](std::span<const std::byte> bytes) {
                                     return model::decode_u16(bytes);
                                   },
                                   "u16");
}

Result<std::vector<U64>> read_u64_values(Context& context, ArrayRef array) {
  if (!array) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the u64 array is missing",
                                         "observation.read", "the array cannot be read");
  }
  if (array->dtype != DType::u64) {
    return model::fail<std::vector<U64>>(Code::invalid_model, "the array is not u64",
                                         "observation.read", "the array cannot be read");
  }
  return read_typed<U64>(context, array,
                         [](std::span<const std::byte> bytes) { return model::decode_u64(bytes); },
                         "u64");
}

Result<ArrayRef> write_f64_values(Context& context, const std::vector<double>& values,
                                  std::span<const U64> shape, const Unit& unit,
                                  const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_f64(values);
  return context.storage.copy(context, DType::f64, bytes, shape, unit, index_id);
}

Result<ArrayRef> write_u16_values(Context& context, const std::vector<std::uint16_t>& values,
                                  std::span<const U64> shape, const Unit& unit,
                                  const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u16(values);
  return context.storage.copy(context, DType::u16, bytes, shape, unit, index_id);
}

std::string detector_identity(const Index& index) {
  if (!index.sources.empty()) {
    const SourceIdentity& first = index.sources.front();
    if (!first.detector.empty()) return first.detector;
  }
  if (!index.namespace_id.empty()) return index.namespace_id;
  return "native";
}

std::string payload_hash(const std::string& schema, const std::string& discriminator,
                         const std::vector<std::string>& parts) {
  model::Sha256 hash;
  hash.field("schema", schema);
  hash.field("discriminator", discriminator);
  for (const std::string& part : parts) hash.field("part", part);
  return hash.hex();
}

void note_calibration_jacobian(Assessment& assessment) {
  model::set_included(assessment, "calibration_jacobian(d,b,dark,flat)");
}

Result<Ref<LinearAction>> make_diagonal_action(Context& context,
                                                const std::vector<double>& weights,
                                                Ref<Index> index, const Unit& in_unit,
                                                const Unit& out_unit) {
  const std::vector<U64> shape{static_cast<U64>(weights.size())};
  Result<ArrayRef> array =
      write_f64_values(context, weights, shape, in_unit,
                       index ? index->identity.object_id : Id());
  if (!ok(array)) {
    return model::fail<Ref<LinearAction>>(array.code, "the diagonal weights cannot be stored",
                                          "observation.action",
                                          "the action cannot be built");
  }
  const std::vector<ArrayRef> arrays{val(array)};
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "diagonal_v1";
  recipe.version = 1;
  recipe.arrays.push_back(val(array)->identity.object_id);
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = in_unit;
  recipe.spaces.output = out_unit;
  return make_action(context, recipe, std::span<const Ref<LinearAction>>{},
                     std::span<const ArrayRef>{arrays.data(), arrays.size()});
}

}  // namespace acsd::observation
