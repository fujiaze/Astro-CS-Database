// V01 driver: the sparse actual-scene variance reconstruction.
//
// The driver only includes the public contract and only moves frozen fields:
// the control grid, the control values, the per-control error bound, the
// support map and the queries.  Every scientific quantity (the log V truth,
// the Taylor remainder, the barycentric interpolation) is computed by the
// independent truth side, not here.

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "validation_support.hpp"

namespace {

using acsd::Code;
using acsd::Context;
using acsd::Result;
using acsd::U64;
using json = nlohmann::json;

constexpr double kPi = 3.14159265358979323846;

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

Result<acsd::ArrayRef> store_u8(Context& context, const std::vector<std::uint8_t>& values,
                                const std::string& id) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return context.storage.copy(context, acsd::DType::u8, as_bytes(values), shape,
                              valsupport::unit_dimensionless(), id);
}

std::vector<std::uint8_t> read_u8(Context& context, acsd::ArrayRef array) {
  std::vector<std::uint8_t> values;
  if (!array) return values;
  U64 count = 1;
  for (U64 extent : array->shape) count *= extent;
  Result<acsd::ArrayView> view =
      context.storage.read(context, array, acsd::BlockRequest{0, count});
  if (!ok(view)) return values;
  const std::span<const std::byte> bytes = val(view).bytes;
  values.resize(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index) {
    values[index] = static_cast<std::uint8_t>(bytes[index]);
  }
  return values;
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

// The chart of the fixture: the gnomonic tangent plane at (RA 0, Dec 0).
struct ChartBasis {
  acsd::SkyICRS center{1.0, 0.0, 0.0};
  acsd::Vec3 east{0.0, 1.0, 0.0};
  acsd::Vec3 north{0.0, 0.0, 1.0};
};

// The sky direction of a chart coordinate in radians.
acsd::SkyICRS direction_of(const ChartBasis& basis, double x, double y) {
  const double dx = basis.center.nx + x * basis.east[0] + y * basis.north[0];
  const double dy = basis.center.ny + x * basis.east[1] + y * basis.north[1];
  const double dz = basis.center.nz + x * basis.east[2] + y * basis.north[2];
  const double norm = std::sqrt(dx * dx + dy * dy + dz * dz);
  return acsd::SkyICRS{dx / norm, dy / norm, dz / norm};
}

// The frozen control grid: four by four points of the chart, in radians.
std::vector<std::pair<double, double>> control_grid() {
  std::vector<std::pair<double, double>> grid;
  const double start = -0.6 * kPi / 180.0;
  const double step = 0.4 * kPi / 180.0;
  for (int i = 0; i < 4; ++i) {
    for (int j = 0; j < 4; ++j) {
      grid.emplace_back(start + step * i, start + step * j);
    }
  }
  return grid;
}

// The affine truth of log V on the chart, in radians.
double log_v_plane(double x, double y) { return 0.7 + 130.0 * x - 90.0 * y; }

// The quadratic truth of log V on the chart, in radians.
double log_v_quadratic(double x, double y) {
  return 0.7 + 130.0 * x - 90.0 * y + 2.0 * x * x - 1.1 * y * y;
}

}  // namespace

int main(int argc, char** argv) {
  std::string output_path = "/tmp/v01_out.json";
  for (int i = 1; i < argc; ++i) {
    const std::string argument = argv[i];
    if (argument == "--output" && i + 1 < argc) output_path = argv[++i];
  }

  ChartBasis basis;

  valsupport::ValidationStorage storage;
  valsupport::ValidationResources resources;
  valsupport::NeverCancelled cancellation;
  Context context{resources, cancellation, storage};

  json report;
  report["driver"] = "v01_native";
  report["route"] = "in_memory_public_api";
  report["cases"] = json::array();

  const auto grid = control_grid();

  // ---- the support: the twelve order zero leaves, all of them positive ---
  std::vector<U64> leaf_ids;
  for (U64 ipix = 0; ipix < 12; ++ipix) {
    leaf_ids.push_back(0);
    leaf_ids.push_back(ipix);
  }
  Result<acsd::ArrayRef> stored_ids = store_u64(context, leaf_ids, "v01:leaf_ids");
  Result<acsd::ArrayRef> stored_states =
      store_u8(context, std::vector<std::uint8_t>(12, 1), "v01:states");
  if (!ok(stored_ids) || !ok(stored_states)) {
    std::cerr << "the support cannot be stored\n";
    return 2;
  }
  auto support_index = std::make_shared<acsd::Index>();
  support_index->size = 12;
  support_index->order = 0;
  support_index->namespace_id = "v01:leaves";
  support_index->kind = acsd::Index::Kind::leaf;
  support_index->ids = *stored_ids.value;

  auto support = std::make_shared<acsd::SupportMap>();
  support->kind = acsd::SupportMap::Kind::primitives;
  support->index = support_index;
  support->states = *stored_states.value;
  support->geometry_snapshot = "v01:fixture";

  acsd::Chart chart;
  chart.center = basis.center;
  chart.maximum_radius_rad = 2.0 * kPi / 180.0;

  // The queries: four points of the chart and one far outside the mesh.
  const std::vector<std::pair<double, double>> query_points = {
      {-0.5 * kPi / 180.0, -0.5 * kPi / 180.0},
      {0.1 * kPi / 180.0, 0.2 * kPi / 180.0},
      {0.55 * kPi / 180.0, -0.45 * kPi / 180.0},
      {0.0, 0.0},
      {5.0 * kPi / 180.0, 5.0 * kPi / 180.0},
  };
  std::vector<acsd::SkyICRS> queries;
  for (const auto& point : query_points) queries.push_back(direction_of(basis, point.first, point.second));

  struct SceneCase {
    const char* name;
    bool quadratic;
    bool with_curvature;
  };
  const std::vector<SceneCase> scenes = {
      {"plane", false, true},
      {"quadratic", true, true},
      {"quadratic_without_curvature", true, false},
  };

  for (const SceneCase& scene : scenes) {
    json entry;
    entry["name"] = scene.name;

    // ---- the controls ----------------------------------------------------
    std::vector<double> positions;
    std::vector<double> values;
    for (const auto& point : grid) {
      positions.push_back(point.first);
      positions.push_back(point.second);
      // The control values are the variances themselves; the reconstruction
      // interpolates their logarithm.
      const double log_v = scene.quadratic ? log_v_quadratic(point.first, point.second)
                                           : log_v_plane(point.first, point.second);
      values.push_back(std::exp(log_v));
    }
    // The per control bound on the logarithm of the variance.
    const std::vector<double> errors(values.size(), 0.01);

    auto controls = std::make_shared<acsd::PrecisionControls>();
    controls->kind = acsd::PrecisionControls::Kind::actual_scene;
    Result<acsd::ArrayRef> stored_values = store_f64(context, values, "v01:values");
    Result<acsd::ArrayRef> stored_errors = store_f64(context, errors, "v01:errors");
    Result<acsd::ArrayRef> stored_positions = store_f64(context, positions, "v01:positions");
    if (!ok(stored_values) || !ok(stored_errors) || !ok(stored_positions)) {
      std::cerr << "the controls cannot be stored\n";
      return 2;
    }
    controls->values = *stored_values.value;
    controls->log_errors = *stored_errors.value;
    controls->positions = *stored_positions.value;
    controls->estimator.id = "v01:fixture";
    controls->estimator.quantity = "variance";
    controls->estimator.unit = valsupport::unit_dimensionless();
    controls->support = support;
    if (scene.with_curvature) {
      // the spectral norm of the Hessian of the quadratic truth
      controls->hessian_norm_upper = 4.0;
    }

    // The independent check that the storage of the controls is readable.
    {
      const std::vector<double> probe_values = read_f64(context, controls->values);
      const std::vector<double> probe_errors = read_f64(context, controls->log_errors);
      const std::vector<double> probe_positions = read_f64(context, controls->positions);
      entry["control_values_readable"] = static_cast<double>(probe_values.size());
    }

    // ---- the mesh --------------------------------------------------------
    Result<acsd::Ref<acsd::SupportedTriangulation>> mesh =
        acsd::build_supported_triangulation(context, controls, chart, support);
    if (!ok(mesh)) {
      entry["code"] = code_name(mesh.code);
      entry["reason"] = valsupport::first_issue(mesh.assessment);
      report["cases"].push_back(entry);
      continue;
    }
    const acsd::Ref<acsd::SupportedTriangulation>& triangulation = *mesh.value;
    entry["triangulation_code"] = "ok";
    entry["vertex_count"] =
        triangulation->vertices->shape.empty() ? 0 : triangulation->vertices->shape[0];
    entry["triangle_count"] =
        triangulation->triangles->shape.empty() ? 0 : triangulation->triangles->shape[0];

    // ---- the reconstruction ----------------------------------------------
    Result<acsd::VariancePrediction> prediction =
        acsd::reconstruct_scene_variance(context, controls, triangulation, queries);
    if (!ok(prediction)) {
      entry["code"] = code_name(prediction.code);
      entry["reason"] = valsupport::first_issue(prediction.assessment);
      report["cases"].push_back(entry);
      continue;
    }
    entry["code"] = "ok";
    const std::vector<double> predicted = read_f64(context, prediction.value->values);
    const std::vector<double> lower = read_f64(context, prediction.value->lower);
    const std::vector<double> upper = read_f64(context, prediction.value->upper);
    json rows = json::array();
    for (std::size_t index = 0; index < predicted.size() && index < query_points.size(); ++index) {
      json row;
      row["query_chart_rad"] = {query_points[index].first, query_points[index].second};
      row["value"] = predicted[index];
      row["lower"] = lower[index];
      row["upper"] = upper[index];
      row["conditional"] = prediction.value->conditional;
      const std::vector<std::uint8_t> states =
          read_u8(context, prediction.value->support_states);
      row["state"] = index < states.size() ? states[index] : 0;
      const double truth = scene.quadratic ? log_v_quadratic(query_points[index].first,
                                                            query_points[index].second)
                                           : log_v_plane(query_points[index].first,
                                                         query_points[index].second);
      row["truth_log_v"] = truth;
      row["observed_log_value"] = predicted[index] > 0.0 ? std::log(predicted[index]) : 0.0;
      row["observed_log_lower"] = lower[index] > 0.0 ? std::log(lower[index]) : 0.0;
      row["observed_log_upper"] = upper[index] > 0.0 ? std::log(upper[index]) : 0.0;
      row["truth_inside"] = lower[index] > 0.0 && upper[index] > 0.0
                                ? (std::log(lower[index]) <= truth && truth <= std::log(upper[index]))
                                : false;
      rows.push_back(row);
    }
    entry["queries"] = rows;
    entry["estimator_id"] = prediction.value->estimator.id;
    report["cases"].push_back(entry);
  }

  std::ofstream stream(output_path);
  stream << report.dump(2) << "\n";
  return 0;
}
