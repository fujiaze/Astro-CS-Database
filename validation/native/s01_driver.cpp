// S01 native in-memory API driver: the real relative sky path
// fit_relative_sky -> apply_sky.
//
// The driver moves the frozen fields into the public API and reports what
// came back.  It includes only <acsd/contracts.hpp>; the linear actions are
// implemented here as the public interface requires, and they only move
// numbers (a dense matrix times a vector).

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "validation_support.hpp"

namespace {

using acsd::ArrayRef;
using acsd::Code;
using acsd::Context;
using acsd::Id;
using acsd::Ref;
using acsd::Result;
using acsd::U64;
using json = nlohmann::json;
using valsupport::code_name;
using valsupport::make_identity;
using valsupport::make_assessment;
using valsupport::NeverCancelled;
using valsupport::store_doubles;
using valsupport::unit_dimensionless;
using valsupport::unit_luminosity;
using valsupport::unit_variance_luminosity;
using valsupport::ValidationResources;
using valsupport::ValidationStorage;

template <class T>
bool ok(const Result<T>& result) {
  return result.code == Code::ok && result.value.has_value();
}

template <class T>
const T& val(const Result<T>& result) {
  return *result.value;
}

// A dense matrix action: the public interface, no science.
class DenseAction final : public acsd::LinearAction {
 public:
  DenseAction(std::shared_ptr<const acsd::Array> array, std::size_t rows, std::size_t columns,
              std::vector<double> values)
      : array_(std::move(array)), rows_(rows), columns_(columns), values_(std::move(values)) {}

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

  const std::vector<double>& values() const { return values_; }
  std::size_t rows() const { return rows_; }
  std::size_t columns() const { return columns_; }

 private:
  acsd::Result<acsd::ArrayRef> multiply(acsd::Context& context, acsd::ArrayRef input,
                                        bool transpose) const {
    if (!input) {
      acsd::Result<acsd::ArrayRef> out;
      out.code = Code::invalid_request;
      return out;
    }
    acsd::U64 count = 1;
    for (acsd::U64 extent : input->shape) count *= extent;
    acsd::Result<acsd::ArrayView> read =
        context.storage.read(context, input, acsd::BlockRequest{0, count});
    acsd::Result<acsd::ArrayRef> failed;
    failed.code = read.code;
    if (!ok(read)) {
      return failed;
    }
    std::vector<double> source(count);
    const acsd::ArrayView& view_value = *read.value;
    if (!view_value.bytes.empty()) {
      std::memcpy(source.data(), view_value.bytes.data(),
                  std::min(source.size() * sizeof(double), view_value.bytes.size()));
    }
    // the row major matrix maps a column vector of the columns onto a row
    // vector of the rows; the transpose does the opposite
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
    acsd::Result<acsd::ArrayRef> stored =
        store_doubles(context, result, {static_cast<acsd::U64>(outer)}, unit_luminosity(),
                      "s01:action:out");
    return stored;
  }

  std::shared_ptr<const acsd::Array> array_;
  std::size_t rows_;
  std::size_t columns_;
  std::vector<double> values_;
};

std::shared_ptr<DenseAction> make_dense(acsd::Context& context, const std::string& name,
                                         const std::vector<std::vector<double>>& matrix) {
  const std::size_t rows = matrix.size();
  const std::size_t columns = rows > 0 ? matrix[0].size() : 0;
  std::vector<double> flat;
  for (const std::vector<double>& row : matrix) {
    for (double value : row) flat.push_back(value);
  }
  acsd::Result<acsd::ArrayRef> stored =
      store_doubles(context, flat, {static_cast<acsd::U64>(rows),
                                     static_cast<acsd::U64>(columns)},
                    unit_dimensionless(), "s01:" + name);
  if (!ok(stored)) return nullptr;
  auto array = std::static_pointer_cast<const acsd::Array>(*stored.value);
  auto action = std::make_shared<DenseAction>(array, rows, columns, flat);
  action->spaces.input = unit_luminosity();
  action->spaces.output = unit_luminosity();
  // the action maps a column vector of the columns onto a row vector of the
  // rows: the domain is the input, the codomain the output
  action->spaces.domain = valsupport::make_index("s01:" + name + ":domain",
                                                  static_cast<acsd::U64>(columns),
                                                  acsd::Index::Kind::native);
  action->spaces.codomain = valsupport::make_index("s01:" + name + ":codomain",
                                                   static_cast<acsd::U64>(rows),
                                                   acsd::Index::Kind::native);
  action->identity = make_identity("s01:" + name, "acsd.LinearAction");
  action->assessment = valsupport::make_assessment();
  return action;
}

std::vector<double> read_doubles(acsd::Context& context, acsd::ArrayRef array);

// The matrix of a linear action, recovered by applying it to the unit
// vectors of its domain.
std::vector<std::vector<double>> action_matrix(acsd::Context& context,
                                                acsd::Ref<acsd::LinearAction> action,
                                                std::size_t columns) {
  std::vector<std::vector<double>> matrix;
  if (!action) return matrix;
  for (std::size_t column = 0; column < columns; ++column) {
    std::vector<double> unit(columns, 0.0);
    unit[column] = 1.0;
    acsd::Result<acsd::ArrayRef> output;
    // the action may expect a flat vector or a column vector
    for (int variant = 0; variant < 2 && !ok(output); ++variant) {
      std::vector<acsd::U64> shape{static_cast<acsd::U64>(columns)};
      if (variant == 1) shape.push_back(1);
      acsd::Result<acsd::ArrayRef> input =
          store_doubles(context, unit, shape, unit_luminosity(), "s01:probe");
      if (!ok(input)) return matrix;
      output = acsd::apply(context, action, val(input));
    }
    if (!ok(output)) return matrix;
    matrix.push_back(read_doubles(context, val(output)));
  }
  return matrix;
}

std::vector<double> read_doubles(acsd::Context& context, acsd::ArrayRef array) {
  std::vector<double> values;
  if (!array) return values;
  acsd::U64 count = 1;
  for (acsd::U64 extent : array->shape) count *= extent;
  acsd::Result<acsd::ArrayView> view =
      context.storage.read(context, array, acsd::BlockRequest{0, count});
  if (!ok(view)) return values;
  values.resize(count);
  const acsd::ArrayView& view_value = *view.value;
  if (!view_value.bytes.empty()) {
    std::memcpy(values.data(), view_value.bytes.data(),
                std::min(values.size() * sizeof(double), view_value.bytes.size()));
  }
  return values;
}

}  // namespace

int main(int argc, char** argv) {
  std::string fixture_path;
  std::string output_path;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--fixtures" && index + 1 < argc) fixture_path = argv[++index];
    if (argument == "--output" && index + 1 < argc) output_path = argv[++index];
  }
  if (fixture_path.empty()) {
    std::cerr << "usage: s01_driver --fixtures <file> [--output <file>]\n";
    return 2;
  }
  json fixture;
  {
    std::ifstream input(fixture_path);
    if (!input) {
      std::cerr << "cannot read " << fixture_path << "\n";
      return 2;
    }
    input >> fixture;
  }

  ValidationStorage storage;
  ValidationResources resources;
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};

  json report;
  report["driver"] = "s01_native";
  report["route"] = "in_memory_public_api";

  auto matrix_of = [&](const char* key) {
    std::vector<std::vector<double>> matrix;
    for (const auto& row : fixture.at(key)) {
      std::vector<double> values;
      for (const auto& value : row) values.push_back(value.get<double>());
      matrix.push_back(values);
    }
    return matrix;
  };

  std::shared_ptr<DenseAction> t_action = make_dense(context, "T", matrix_of("T"));
  std::shared_ptr<DenseAction> h_action = make_dense(context, "H", matrix_of("H"));
  // The difference mode is an action from the parameter space (one mode) onto
  // the control space (nine controls): its matrix has one column and nine
  // rows.
  std::shared_ptr<DenseAction> a_action = make_dense(context, "A", matrix_of("A"));
  std::shared_ptr<DenseAction> e_action = make_dense(context, "E", matrix_of("E"));
  std::shared_ptr<DenseAction> native_action =
      make_dense(context, "native", matrix_of("native_cholesky"));
  std::shared_ptr<DenseAction> control_action =
      make_dense(context, "control", matrix_of("control_cholesky"));
  if (!t_action || !h_action || !a_action || !e_action || !native_action || !control_action) {
    std::cerr << "the frozen matrices could not be stored\n";
    return 2;
  }

  // The noise model of the control statistic: its covariance is the control
  // Cholesky factor times its transpose.
  auto noise_root = std::make_shared<acsd::NoiseRoot>();
  noise_root->coordinates = valsupport::make_index("s01:control", control_action->rows(),
                                                    acsd::Index::Kind::control);
  noise_root->unit = unit_luminosity();
  noise_root->factor = Ref<acsd::LinearAction>(control_action);
  noise_root->shared_id.component = "all";
  noise_root->shared_id.entity = "control-root";
  noise_root->shared_id.external_namespace = "fixture";
  noise_root->shared_id.model_revision = "1";
  auto noise = std::make_shared<acsd::NoiseModel>();
  noise->variance_unit = unit_variance_luminosity();
  noise->condition.kind = "fixed_geometry";
  noise->condition.estimator = "fixture-S01";
  noise->condition.assumptions.push_back("independent standard normal latent");
  noise->state = acsd::NoiseState::known;
  noise->innovations.push_back(Ref<acsd::NoiseRoot>(noise_root));
  noise->influence.push_back(Ref<acsd::LinearAction>(control_action));
  noise->identity = make_identity("s01:noise", "acsd.NoiseModel");
  noise->assessment = valsupport::make_assessment();

  // The controls of the fit.
  std::vector<double> data;
  for (const auto& value : fixture.at("data")) data.push_back(value.get<double>());
  acsd::Result<acsd::ArrayRef> data_array =
      store_doubles(context, data, {data.size()}, unit_luminosity(), "s01:data");
  if (!ok(data_array)) {
    std::cerr << "the control statistic could not be stored\n";
    return 2;
  }

  auto controls = std::make_shared<acsd::SkyControls>();
  controls->c = val(data_array);
  controls->T = Ref<acsd::LinearAction>(t_action);
  controls->H = Ref<acsd::LinearAction>(h_action);
  controls->A = Ref<acsd::LinearAction>(a_action);
  controls->E = Ref<acsd::LinearAction>(e_action);
  controls->noise = Ref<acsd::CovarianceAction>([&]() {
    auto covariance = std::make_shared<acsd::CovarianceAction>();
    covariance->model = Ref<acsd::NoiseModel>(noise);
    covariance->influence = Ref<acsd::LinearAction>(control_action);
    covariance->input_output.input = unit_luminosity();
    covariance->input_output.output = unit_luminosity();
    covariance->input_output.domain = valsupport::make_index("s01:control", control_action->rows(),
                                                             acsd::Index::Kind::control);
    covariance->input_output.codomain = valsupport::make_index("s01:control",
                                                               control_action->rows(),
                                                               acsd::Index::Kind::control);
    covariance->identity = make_identity("s01:control-covariance", "acsd.CovarianceAction");
    covariance->assessment = valsupport::make_assessment();
    return Ref<acsd::CovarianceAction>(covariance);
  }());
  controls->positions = val(data_array);
  // the component identities: one per control
  std::vector<acsd::U64> component_ids(data.size());
  for (std::size_t index = 0; index < data.size(); ++index) component_ids[index] = index;
  std::vector<std::byte> component_bytes(component_ids.size() * sizeof(acsd::U64));
  if (!component_ids.empty()) {
    std::memcpy(component_bytes.data(), component_ids.data(), component_bytes.size());
  }
  const std::vector<acsd::U64> component_shape{static_cast<acsd::U64>(component_ids.size())};
  acsd::Result<acsd::ArrayRef> component_array =
      context.storage.copy(context, acsd::DType::u64, component_bytes, component_shape,
                           valsupport::unit_dimensionless(), "s01:components");
  if (!ok(component_array)) {
    std::cerr << "the component identities could not be stored\n";
    return 2;
  }
  controls->component_ids = val(component_array);

  acsd::SkyRequest request;
  // the frames of the run: three frames of the fixture
  acsd::Comparability group;
  group.frames = {"frame-0", "frame-1", "frame-2"};
  group.response_proof = "fixture:response";
  group.spatial_kernel_proof = "fixture:psf";
  group.assumptions = {"identical response", "identical spatial kernel"};
  request.groups.push_back(group);
  request.basis.kind = acsd::Basis::Kind::constant;
  request.basis.degree = 0;
  request.basis.center = {0.0, 0.0};
  request.basis.scale = {1.0, 1.0};
  request.basis.normalization_domain = {{0.0, 0.0}, {1.0, 1.0}};
  request.gauge.kind = acsd::Gauge::Kind::reference_frame;
  request.gauge.reference = "frame-0";
  request.T = Ref<acsd::LinearAction>(t_action);
  request.H = Ref<acsd::LinearAction>(h_action);
  request.control_noise = controls->noise;
  request.solve.relative_residual = 1e-10;
  request.solve.absolute_residual = 1e-12;
  request.solve.rank_relative = 1e-10;
  request.solve.max_iterations = 100;
  request.solve.dense_max_bytes = 16777216;
  request.solve.huber = false;
  request.allow_prediction = false;

  Result<Ref<acsd::SkyFit>> fit =
      acsd::fit_relative_sky(context, Ref<acsd::SkyControls>(controls), request);
  report["fit_code"] = code_name(static_cast<int>(fit.code));
  if (!ok(fit)) {
    report["fit_reason"] = fit.assessment.issues.empty()
                               ? std::string("no issue was reported")
                               : fit.assessment.issues.front().reason;
  } else {
    const acsd::SkyFit& value = *(*fit.value);
    json item;
    item["beta"] = read_doubles(context, value.beta);
    // K and P are linear actions: their matrices are recovered by applying
    // them to the unit vectors.
    item["K"] = action_matrix(context, value.K, 9);
    // The correction operator may live in the control space or in the native
    // space; both shapes are tried and the one that applies is reported.
    item["P"] = action_matrix(context, value.P, 12);
    if (item["P"].empty()) item["P"] = action_matrix(context, value.P, 9);
    item["K_rows"] = item["K"].size();
    item["K_columns"] = item["K"].empty() ? 0 : item["K"][0].size();
    item["P_rows"] = item["P"].size();
    item["P_columns"] = item["P"].empty() ? 0 : item["P"][0].size();
    item["has_parameter_covariance"] = static_cast<bool>(value.parameter_cov);
    item["rank"] = {{"columns", value.rank.columns}, {"rank", value.rank.rank},
                    {"certified", value.rank.certified}};
    item["support"] = value.support ? static_cast<int>(value.support->kind) : -1;
    report["fit"] = item;

    // The derived criteria of the case, computed from the returned operators:
    // K H must vanish, so that the common sky is protected.
    if (value.K) {
      std::vector<std::vector<double>> k_matrix = action_matrix(context, value.K, 9);
      const std::vector<double>& h_flat = h_action->values();
      const std::size_t h_rows = h_action->rows();
      const std::size_t h_columns = h_action->columns();
      std::vector<std::vector<double>> h_matrix(h_rows, std::vector<double>(h_columns, 0.0));
      for (std::size_t row = 0; row < h_rows; ++row) {
        for (std::size_t column = 0; column < h_columns; ++column) {
          h_matrix[row][column] = h_flat[row * h_columns + column];
        }
      }
      double worst = 0.0;
      json per_column = json::array();
      for (std::size_t column = 0; column < h_matrix[0].size(); ++column) {
        double column_worst = 0.0;
        for (std::size_t row = 0; row < k_matrix.size(); ++row) {
          double sum = 0.0;
          for (std::size_t inner = 0; inner < h_matrix[column].size(); ++inner) {
            sum += k_matrix[row][inner] * h_matrix[column][inner];
          }
          column_worst = std::max(column_worst, std::fabs(sum));
        }
        per_column.push_back(column_worst);
        worst = std::max(worst, column_worst);
      }
      report["KH_max_abs"] = worst;
      report["KH_per_column"] = per_column;
    }
    report["fit_has_K"] = static_cast<bool>(value.K);
    report["fit_has_P"] = static_cast<bool>(value.P);
  }

  const std::string text = report.dump(1);
  if (!output_path.empty()) {
    std::ofstream out(output_path);
    out << text << "\n";
  }
  std::cout << text;
  return 0;
}
