// W01 driver: the coaddition weights.
//
// The driver implements the public FramesProvider of the contract with a
// frozen fixture of two frames whose joint covariance is
// Gamma = [[1, 2], [2, 9]] by construction (two shared noise roots, the
// Cholesky factor of Gamma).  Every field is moved into the public API; no
// science is computed here: the BLUE and the non-negative QP solutions are
// computed by the independent truth side with exact rational arithmetic.

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "validation_support.hpp"

namespace {

using acsd::Code;
using acsd::Context;
using acsd::Result;
using acsd::U64;
using json = nlohmann::json;

template <typename T>
bool ok(const Result<T>& result) {
  return result.code == Code::ok;
}

template <typename T>
const T& val(const Result<T>& result) {
  return *result.value;
}

const char* code_name(Code code) { return valsupport::code_name(static_cast<int>(code)); }

std::vector<std::byte> as_bytes(const std::vector<double>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(double));
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> as_bytes(const std::vector<U64>& values) {
  std::vector<std::byte> bytes(values.size() * sizeof(U64));
  if (!values.empty()) std::memcpy(bytes.data(), values.data(), bytes.size());
  return bytes;
}

std::vector<std::byte> as_bytes(const std::vector<std::uint8_t>& values) {
  std::vector<std::byte> bytes(values.size());
  for (std::size_t index = 0; index < values.size(); ++index) {
    bytes[index] = static_cast<std::byte>(values[index]);
  }
  return bytes;
}

Result<acsd::ArrayRef> store_f64(Context& context, const std::vector<double>& values,
                                 const std::string& id) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, acsd::DType::f64, as_bytes(values), shape,
                              valsupport::unit_dimensionless(), id);
}

Result<acsd::ArrayRef> store_u64(Context& context, const std::vector<U64>& values,
                                 const std::string& id) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, acsd::DType::u64, as_bytes(values), shape,
                              valsupport::unit_dimensionless(), id);
}

Result<acsd::ArrayRef> store_u16(Context& context, const std::vector<std::uint16_t>& values,
                                 const std::string& id) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  std::vector<std::byte> bytes(values.size() * sizeof(std::uint16_t));
  if (!values.empty()) {
    std::memcpy(bytes.data(), values.data(), bytes.size());
  }
  return context.storage.copy(context, acsd::DType::u16, bytes, shape,
                              valsupport::unit_dimensionless(), id);
}

Result<acsd::ArrayRef> store_u8(Context& context, const std::vector<std::uint8_t>& values,
                                const std::string& id) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, acsd::DType::u8, as_bytes(values), shape,
                              valsupport::unit_dimensionless(), id);
}

std::vector<double> read_f64(Context& context, acsd::ArrayRef array) {
  std::vector<double> values;
  if (!array) return values;
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<acsd::ArrayView> view =
      context.storage.read(context, array, acsd::BlockRequest{0, count});
  if (!ok(view)) return values;
  const std::span<const std::byte> bytes = val(view).bytes;
  const std::size_t count_values = bytes.size() / sizeof(double);
  values.resize(count_values);
  if (count_values > 0) {
    std::memcpy(values.data(), reinterpret_cast<const void*>(bytes.data()),
                count_values * sizeof(double));
  }
  return values;
}

// A dense linear action of the public contract.
class DenseAction final : public acsd::LinearAction {
 public:
  DenseAction(std::size_t rows, std::size_t columns, std::vector<double> values)
      : rows_(rows), columns_(columns), values_(std::move(values)) {
    auto domain = std::make_shared<acsd::Index>();
    domain->size = static_cast<U64>(columns);
    domain->kind = acsd::Index::Kind::parameter;
    auto codomain = std::make_shared<acsd::Index>();
    codomain->size = static_cast<U64>(rows);
    codomain->kind = acsd::Index::Kind::parameter;
    spaces.domain = domain;
    spaces.codomain = codomain;
  }

  acsd::Result<acsd::ArrayRef> apply(acsd::Context& context, acsd::ArrayRef values) const override {
    return multiply(context, values, false);
  }

  acsd::Result<acsd::ArrayRef> transpose_apply(acsd::Context& context,
                                                acsd::ArrayRef values) const override {
    return multiply(context, values, true);
  }

  acsd::Result<std::vector<acsd::Id>> dependency_blocks(acsd::BlockRequest, bool) const override {
    acsd::Result<std::vector<acsd::Id>> out;
    out.code = Code::ok;
    out.value = std::vector<acsd::Id>();
    return out;
  }

  acsd::Result<acsd::ActionRecipe> serialize_recipe() const override {
    acsd::Result<acsd::ActionRecipe> out;
    out.code = Code::unsupported;
    return out;
  }

  acsd::Result<std::vector<acsd::ExactInterval>> row_enclosure(acsd::Context&, acsd::U64) const override {
    acsd::Result<std::vector<acsd::ExactInterval>> out;
    out.code = Code::unsupported;
    return out;
  }

 private:
  acsd::Result<acsd::ArrayRef> multiply(acsd::Context& context, acsd::ArrayRef input,
                                        bool transpose) const {
    if (!input) {
      acsd::Result<acsd::ArrayRef> out;
      out.code = Code::invalid_request;
      return out;
    }
    U64 count = 1;
    for (U64 extent : input->shape) count *= extent;
    Result<acsd::ArrayView> read =
        context.storage.read(context, input, acsd::BlockRequest{0, count});
    if (!ok(read)) {
      acsd::Result<acsd::ArrayRef> out;
      out.code = read.code;
      return out;
    }
    std::vector<double> source(count);
    if (!val(read).bytes.empty()) {
      std::memcpy(source.data(), val(read).bytes.data(),
                  std::min(source.size() * sizeof(double), val(read).bytes.size()));
    }
    const std::size_t inner = transpose ? rows_ : columns_;
    const std::size_t outer = transpose ? columns_ : rows_;
    if (source.size() != inner) {
      acsd::Result<acsd::ArrayRef> out;
      out.code = Code::invalid_request;
      return out;
    }
    std::vector<double> result(outer, 0.0);
    for (std::size_t row = 0; row < outer; ++row) {
      double sum = 0.0;
      for (std::size_t column = 0; column < inner; ++column) {
        const std::size_t index = transpose ? column * rows_ + row : row * columns_ + column;
        sum += values_[index] * source[column];
      }
      result[row] = sum;
    }
    const std::vector<U64> shape{static_cast<U64>(result.size())};
    return context.storage.copy(context, acsd::DType::f64, as_bytes(result), shape,
                                valsupport::unit_dimensionless(), "w01:action");
  }

  std::size_t rows_;
  std::size_t columns_;
  std::vector<double> values_;
};

// The minimal public frames provider of the fixture.
class FixtureFrames final : public acsd::FramesProvider {
 public:
  FixtureFrames(std::vector<acsd::Ref<acsd::FrameDescription>> descriptions)
      : descriptions_(std::move(descriptions)) {}

  acsd::Result<std::vector<acsd::Id>> list_frames(Context&) const override {
    acsd::Result<std::vector<acsd::Id>> out;
    out.code = Code::ok;
    out.value = std::vector<acsd::Id>();
    for (const auto& description : descriptions_) out.value->push_back(description->frame);
    return out;
  }

  acsd::Result<acsd::Ref<acsd::FrameDescription>> describe_frame(Context&, acsd::Id frame) const override {
    for (const auto& description : descriptions_) {
      if (description->frame == frame) {
        acsd::Result<acsd::Ref<acsd::FrameDescription>> out;
        out.code = Code::ok;
        out.value = description;
        return out;
      }
    }
    acsd::Result<acsd::Ref<acsd::FrameDescription>> out;
    out.code = Code::unavailable;
    return out;
  }

  acsd::Result<acsd::LeafBlock> read_leaf_block(Context& context,
                                                 const acsd::FrameReadRequest& request) const override {
    acsd::Result<acsd::LeafBlock> out;
    for (const auto& description : descriptions_) {
      if (description->frame != request.frame) continue;
      acsd::LeafBlock block;
      block.description = description;
      block.index = description->index;
      const auto signal_found = signal_.find(description->frame);
      const auto dq_found = dq_.find(description->frame);
      block.signal = signal_found == signal_.end() ? nullptr : signal_found->second;
      block.dq = dq_found == dq_.end() ? nullptr : dq_found->second;
      out.code = Code::ok;
      out.value = block;
      return out;
    }
    out.code = Code::unavailable;
    return out;
  }

  acsd::Result<std::vector<acsd::ArrayRef>> read_native_dependencies(
      Context&, std::span<const acsd::SourceIdentity>) const override {
    acsd::Result<std::vector<acsd::ArrayRef>> out;
    out.code = Code::ok;
    out.value = std::vector<acsd::ArrayRef>();
    return out;
  }

  acsd::Result<acsd::Ref<acsd::LinearAction>> open_action(Context&, acsd::Id frame) const override {
    for (const auto& description : descriptions_) {
      if (description->frame == frame) {
        acsd::Result<acsd::Ref<acsd::LinearAction>> out;
        out.code = Code::ok;
        out.value = description->native_to_leaf;
        return out;
      }
    }
    acsd::Result<acsd::Ref<acsd::LinearAction>> out;
    out.code = Code::unavailable;
    return out;
  }

  acsd::Result<acsd::Ref<acsd::SupportMap>> query_support(Context&, acsd::Id,
                                                            acsd::Ref<acsd::Index>) const override {
    acsd::Result<acsd::Ref<acsd::SupportMap>> out;
    out.code = Code::unsupported;
    return out;
  }

  acsd::Result<acsd::Ref<acsd::RefinableGeometryProvider>> open_refinable_geometry(
      Context&, acsd::Id) const override {
    acsd::Result<acsd::Ref<acsd::RefinableGeometryProvider>> out;
    out.code = Code::unsupported;
    return out;
  }

  void set_signal(const acsd::Id& frame, acsd::ArrayRef array) { signal_[frame] = array; }
  void set_dq(const acsd::Id& frame, acsd::ArrayRef array) { dq_[frame] = array; }

 private:
  std::vector<acsd::Ref<acsd::FrameDescription>> descriptions_;
  std::map<acsd::Id, acsd::ArrayRef> signal_;
  std::map<acsd::Id, acsd::ArrayRef> dq_;
};

}  // namespace

int main(int argc, char** argv) {
  std::string output_path = "/tmp/w01_out.json";
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--output" && i + 1 < argc) output_path = argv[++i];
  }

  valsupport::ValidationStorage storage;
  valsupport::ValidationResources resources;
  valsupport::NeverCancelled cancellation;
  Context context{resources, cancellation, storage};

  json report;
  report["driver"] = "w01_native";
  report["route"] = "in_memory_public_api";
  report["cases"] = json::array();

  // ---- the shared noise roots -------------------------------------------
  // Gamma = [[1, 2], [2, 9]] is the Cholesky factor L = [[1, 0], [2, sqrt(5)]]
  // times its transpose: the first root carries the column (1, 2) and the
  // second root the column (0, sqrt(5)).
  const double root_one = 1.0;
  const double root_two = std::sqrt(5.0);
  const double influence_one[2] = {1.0, 2.0};
  const double influence_two[2] = {0.0, root_two};

  auto make_root = [&](const std::string& name) {
    auto root = std::make_shared<acsd::NoiseRoot>();
    root->shared_id.external_namespace = "w01";
    root->shared_id.entity = name;
    root->shared_id.model_revision = "1";
    root->shared_id.component = "shared";
    root->unit = valsupport::unit_variance("L^2");
    // the factor is the identity: the variance of the root is one
    root->factor = std::make_shared<DenseAction>(1, 1, std::vector<double>{1.0});
    // the coordinates of the root: one latent coordinate per root
    auto coordinates = std::make_shared<acsd::Index>();
    coordinates->size = 1;
    coordinates->kind = acsd::Index::Kind::latent;
    coordinates->namespace_id = "w01:latent";
    root->coordinates = coordinates;
    return root;
  };

  auto build_frames = [&](bool singular_second_root) {
    std::vector<acsd::Ref<acsd::FrameDescription>> descriptions;
    for (int frame_index = 0; frame_index < 2; ++frame_index) {
      auto description = std::make_shared<acsd::FrameDescription>();
      const acsd::Id frame = frame_index == 0 ? "frame-0" : "frame-1";
      description->frame = frame;
      description->order = 0;
      description->estimator.id = "w01:fixture";
      description->estimator.quantity = "signal";
      description->estimator.unit = valsupport::unit_dimensionless();

      // the index of the frame: one leaf
      auto index = std::make_shared<acsd::Index>();
      index->size = 1;
      index->order = 0;
      index->kind = acsd::Index::Kind::leaf;
      index->namespace_id = "w01:leaves";
      const std::vector<U64> ids{0, static_cast<U64>(frame_index)};
      Result<acsd::ArrayRef> stored_ids = store_u64(context, ids, "w01:ids");
      if (!ok(stored_ids)) return descriptions;
      index->ids = *stored_ids.value;
      description->index = index;

      // the noise model: two shared roots, the influences of this frame
      auto noise = std::make_shared<acsd::NoiseModel>();
      noise->variance_unit = valsupport::unit_variance("L^2");
      const std::shared_ptr<acsd::NoiseRoot> root_one_handle = make_root("root-one");
      const std::shared_ptr<acsd::NoiseRoot> root_two_handle = make_root("root-two");
      noise->shared_factors = {root_one_handle, root_two_handle};
      const double second = singular_second_root ? 0.0 : influence_two[frame_index];
      noise->influence = {
          std::make_shared<DenseAction>(1, 1, std::vector<double>{influence_one[frame_index]}),
          std::make_shared<DenseAction>(1, 1, std::vector<double>{second})};
      auto covariance = std::make_shared<acsd::CovarianceAction>();
      covariance->model = noise;
      description->noise = covariance;
      description->native_to_leaf = std::make_shared<DenseAction>(1, 1, std::vector<double>{1.0});
      descriptions.push_back(description);
    }
    return descriptions;
  };

  // ---- the selection ----------------------------------------------------
  auto make_selection = [&](const std::string& name) {
    auto selection = std::make_shared<acsd::Selection>();
    auto index = std::make_shared<acsd::Index>();
    index->size = 2;
    index->kind = acsd::Index::Kind::leaf;
    index->namespace_id = "w01:selection";
    // the leaf identities of the selection: the same two leaves as the frames
    const std::vector<U64> leaf_ids{0, 0, 0, 1};
    Result<acsd::ArrayRef> stored_leaf_ids = store_u64(context, leaf_ids, name + ":leaves");
    if (!ok(stored_leaf_ids)) return selection;
    index->ids = *stored_leaf_ids.value;
    selection->index = index;
    Result<acsd::ArrayRef> accepted = store_u8(context, std::vector<std::uint8_t>{1, 1}, name);
    Result<acsd::ArrayRef> eligibility =
        store_u8(context, std::vector<std::uint8_t>{1, 1}, name + ":eligibility");
    if (!ok(accepted) || !ok(eligibility)) return selection;
    selection->accepted = *accepted.value;
    selection->eligibility = *eligibility.value;
    selection->reasons = *accepted.value;
    selection->policy.kind = acsd::RejectionPolicy::Kind::single_max_loo;
    selection->policy.alpha = 0.01;
    return selection;
  };

  struct Case {
    const char* name;
    acsd::WeightPolicy policy;
    bool singular;
  };
  const std::vector<Case> cases = {
      {"blue", acsd::WeightPolicy::blue, false},
      {"nonnegative_qp", acsd::WeightPolicy::nonnegative_qp, false},
      {"automatic", acsd::WeightPolicy::automatic, false},
      {"blue_singular", acsd::WeightPolicy::blue, true},
  };

  for (const Case& entry : cases) {
    json item;
    item["name"] = entry.name;
    item["policy"] = static_cast<int>(entry.policy);

    std::vector<acsd::Ref<acsd::FrameDescription>> descriptions = build_frames(entry.singular);
    if (descriptions.size() != 2) {
      item["code"] = "the fixture cannot be built";
      report["cases"].push_back(item);
      continue;
    }
    FixtureFrames frames(descriptions);
    for (const auto& description : descriptions) {
      Result<acsd::ArrayRef> signal = store_f64(context, std::vector<double>{1.0}, "w01:signal");
      Result<acsd::ArrayRef> dq =
          store_u16(context, std::vector<std::uint16_t>{0}, "w01:dq");
      if (!ok(signal) || !ok(dq)) continue;
      frames.set_signal(description->frame, *signal.value);
      frames.set_dq(description->frame, *dq.value);
    }

    acsd::Ref<acsd::Selection> selection = make_selection(std::string("w01:") + entry.name);
    acsd::CoaddRequest request;
    request.target.id = "w01:fixture";
    request.target.quantity = "signal";
    request.target.unit = valsupport::unit_dimensionless();
    request.mixed_observation = false;
    request.weights = entry.policy;
    request.rejection.kind = acsd::RejectionPolicy::Kind::single_max_loo;
    request.rejection.alpha = 0.01;

    Result<acsd::Ref<acsd::WeightSolution>> solution =
        acsd::solve_weights(context, frames, selection, request);
    if (!ok(solution)) {
      item["code"] = code_name(solution.code);
      item["reason"] = valsupport::first_issue(solution.assessment);
      json issues = json::array();
      for (const acsd::Issue& issue : solution.assessment.issues) {
        issues.push_back({{"origin", issue.origin}, {"reason", issue.reason},
                          {"impact", issue.impact}});
      }
      item["issues"] = issues;
      report["cases"].push_back(item);
      continue;
    }
    item["code"] = "ok";
    const acsd::Ref<acsd::WeightSolution> solved = *solution.value;
    const std::vector<double> coefficients = read_f64(context, solved->coefficients);
    item["coefficients"] = coefficients;
    item["policy_used"] = static_cast<int>(solved->policy);
    item["condition_estimator"] = solved->condition.estimator;
    report["cases"].push_back(item);
  }

  std::ofstream stream(output_path);
  stream << report.dump(2) << "\n";
  return 0;
}
