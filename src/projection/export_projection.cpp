// The Export target operator: build_export.
//
// The target pixel footprints are handed to the production geometry kernel
// through the public compute_overlap entry point with pixfrac = 1, so the
// target pixel is never contracted a second time.  The support of the input
// product decides how much of every touched leaf is real support:
//
//   b_ip = Omega(E_i intersect H_p intersect U_p)
//
// with three request variants that all use the same row Q:
//
//   supported_mean        Q_ip = b_ip / sum_p b_ip          (dimensionless)
//   supported_flux        Q_ip = b_ip                        (steradian)
//   extrapolated_full_flux Q_ip = Omega(E_i) * b_ip / sum_p b_ip (steradian)
//
// The extrapolation is only allowed when the request states the uniform
// uncovered assumption explicitly.
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "geometry/pixel_grid.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

// An issue collected before the assessment of the exported estimate exists.  The
// assessment is initialised once, before the issues are added: the previous
// order added the reason first and cleared it with the default assessment, so a
// covariance that could not be pushed through Q was silently missing.
struct PendingIssue {
  Code code;
  std::string reason;
  std::string origin;
  std::string impact;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Reads the leaf identifiers of an index.  The leaf index of a product stores
// the (order, ipix) pairs in its ids array.
Result<std::vector<HealpixLeaf>> read_leaves(Context& context, const Index& index) {
  std::vector<HealpixLeaf> leaves;
  if (index.kind != Index::Kind::leaf) {
    return fail<std::vector<HealpixLeaf>>(Code::invalid_model, "the index is not a leaf index",
                                          "build_export", "the support cannot be resolved");
  }
  if (!index.ids) {
    return fail<std::vector<HealpixLeaf>>(Code::missing_dependency,
                                          "the leaf index does not carry its identifiers",
                                          "build_export", "the support cannot be resolved");
  }
  Result<ArrayView> view = context.storage.read(context, index.ids, BlockRequest{0, index.size * 2});
  if (!ok(view)) {
    return fail<std::vector<HealpixLeaf>>(view.code, "the leaf identifiers cannot be read",
                                          "build_export", "the support cannot be resolved");
  }
  const std::vector<U64> ids = model::decode_u64(val(view).bytes);
  for (std::size_t i = 0; i + 1 < ids.size(); i += 2) {
    HealpixLeaf leaf;
    leaf.order = static_cast<U32>(ids[i]);
    leaf.ipix = ids[i + 1];
    leaves.push_back(leaf);
  }
  return ok_result(leaves);
}

std::map<std::pair<U32, U64>, U64> leaf_row_map(const std::vector<HealpixLeaf>& leaves) {
  std::map<std::pair<U32, U64>, U64> rows;
  for (std::size_t i = 0; i < leaves.size(); ++i) {
    rows[{leaves[i].order, leaves[i].ipix}] = static_cast<U64>(i);
  }
  return rows;
}

double leaf_area_sr(U32 order) { return 4.0 * M_PI / (12.0 * std::pow(4.0, static_cast<double>(order))); }

// The state array of the support map holds 0 absent, 1 positive and
// 2 bounded undecided per leaf.
Result<std::vector<std::uint8_t>> read_states(Context& context, const ArrayRef& array,
                                               U64 count) {
  std::vector<std::uint8_t> states;
  if (!array) {
    return model::fail<std::vector<std::uint8_t>>(
        Code::unavailable, "the support map carries no states array", "build_export",
        "the support of the product is unknown");
  }
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) {
    // An unreadable support state is not "all supported": silently returning an
    // empty vector would export every geometrically overlapping pixel.
    return model::fail<std::vector<std::uint8_t>>(
        view.code, "the support states cannot be read", "build_export",
        "the support of the product is unknown");
  }
  return model::ok_result(model::decode_u8(val(view).bytes));
}

std::vector<double> read_doubles(Context& context, const ArrayRef& array, U64 count) {
  std::vector<double> values;
  if (!array) return values;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) return values;
  return model::decode_f64(val(view).bytes);
}

// Builds the pixel footprint of the target image.  The first pixel centre is
// (0, 0) and the pixel covers [-0.5, 0.5] in both axes.
Ref<DetectorFootprint> make_target_footprint(Ref<WcsModel> wcs, U64 x, U64 y) {
  auto footprint = std::make_shared<DetectorFootprint>();
  footprint->wcs = wcs;
  Ring ring;
  ring.hole = false;
  ring.vertices = {PixelEdge0{static_cast<double>(x) - 0.5, static_cast<double>(y) - 0.5},
                   PixelEdge0{static_cast<double>(x) + 0.5, static_cast<double>(y) - 0.5},
                   PixelEdge0{static_cast<double>(x) + 0.5, static_cast<double>(y) + 0.5},
                   PixelEdge0{static_cast<double>(x) - 0.5, static_cast<double>(y) + 0.5}};
  footprint->detector_rings.push_back(ring);
  footprint->source.dataset = "acsd.export";
  footprint->source.exposure = "acsd.export";
  footprint->source.detector = "acsd.target";
  footprint->source.channel = "acsd.signal";
  footprint->source.sample = y * 1000000ull + x;
  footprint->contraction_center = PixelCenter0{static_cast<double>(x), static_cast<double>(y)};
  footprint->identity = model::make_identity("acsd.DetectorFootprint", 2, 0,
                                             model::sha256_hex("export:target:" +
                                                               std::to_string(x) + ":" +
                                                               std::to_string(y)),
                                             "acsd");
  footprint->assessment = model::make_assessment(Code::ok);
  footprint->assessment.complete = true;
  return footprint;
}

}  // namespace

Result<ExportResult> build_export(Context& context, const ProductProvider& product,
                                   const ExportRequest& request) {
  const ProductGraph* graph = product.graph().get();
  if (!graph || !graph->signal) {
    return fail<ExportResult>(Code::missing_dependency, "the product has no signal estimate",
                              "build_export", "the export cannot be built");
  }
  const SignalEstimate& estimate = *graph->signal;
  std::vector<PendingIssue> pending_issues;
  if (!estimate.support) {
    return fail<ExportResult>(Code::missing_dependency, "the product has no support map",
                              "build_export", "the export cannot be built");
  }
  if (!request.target.model) {
    return fail<ExportResult>(Code::invalid_request, "the target has no wcs", "build_export",
                              "the export cannot be built");
  }
  if (request.target.width == 0 || request.target.height == 0) {
    return fail<ExportResult>(Code::invalid_request, "the target size is empty", "build_export",
                              "the export cannot be built");
  }
  if (request.quantity == ExportRequest::Quantity::extrapolated_full_flux &&
      !request.assume_uniform_uncovered) {
    return fail<ExportResult>(Code::invalid_request,
                              "the extrapolated flux needs the explicit uniform uncovered "
                              "assumption",
                              "build_export", "the export cannot be built");
  }
  if (request.output_dtype != DType::f64 && request.output_dtype != DType::f32) {
    return fail<ExportResult>(Code::unsupported, "the export supports f32 and f64 only",
                              "build_export", "the export cannot be built");
  }

  const SupportMap& support = *estimate.support;
  const bool strict_request =
      request.support_policy == SupportMap::Kind::primitives ||
      request.support_policy == SupportMap::Kind::dyadic_cells;
  if (strict_request && support.kind != request.support_policy) {
    return fail<ExportResult>(Code::unavailable,
                              "the product does not carry the requested support model",
                              "build_export", "the export cannot be built");
  }
  const bool approximate = request.support_policy == SupportMap::Kind::full_leaf_approx ||
                           request.support_policy == SupportMap::Kind::effective_area_approx;

  // The leaf index of the product.
  Ref<Index> leaf_index = support.index;
  if (!leaf_index && estimate.signal) leaf_index = estimate.signal->index_id.empty()
                                                          ? nullptr
                                                          : nullptr;
  if (!leaf_index) {
    return fail<ExportResult>(Code::missing_dependency, "the support map has no leaf index",
                              "build_export", "the export cannot be built");
  }
  Result<std::vector<HealpixLeaf>> leaves = read_leaves(context, *leaf_index);
  if (!ok(leaves)) return fail<ExportResult>(leaves.code, leaves.assessment.issues.front().reason,
                                             "build_export", "the export cannot be built");
  const std::map<std::pair<U32, U64>, U64> rows = leaf_row_map(val(leaves));
  const U64 leaf_count = val(leaves).size();

  // The leaf signal and the per leaf support.
  Result<std::vector<double>> leaf_signal =
      operators::read_f64(context, estimate.signal, 0, leaf_count);
  if (!ok(leaf_signal)) {
    return fail<ExportResult>(leaf_signal.code, "the leaf signal cannot be read", "build_export",
                              "the export cannot be built");
  }
  Result<std::vector<std::uint8_t>> states_result =
      read_states(context, support.states, leaf_count);
  if (!ok(states_result)) {
    return fail<ExportResult>(states_result.code,
                              states_result.assessment.issues.empty()
                                  ? "the support states cannot be read"
                                  : states_result.assessment.issues.front().reason,
                              "build_export", "the export cannot be built");
  }
  const std::vector<std::uint8_t> states = val(states_result);
  const std::vector<double> area_lower = read_doubles(context, support.area_lower_sr, leaf_count);
  const std::vector<double> area_upper = read_doubles(context, support.area_upper_sr, leaf_count);

  // The geometry: the target pixel against the leaves of the product order.
  const U32 order = leaf_index->order;
  if (context.cancellation.requested()) {
    return fail<ExportResult>(Code::cancelled, "the export was cancelled", "build_export",
                              "the export cannot be built");
  }
  Result<Ref<FootprintProvider>> provider =
      make_footprint_provider(context, request.target.model);
  if (!ok(provider)) {
    return fail<ExportResult>(provider.code, "the target provider cannot be built", "build_export",
                              "the export cannot be built");
  }
  const U64 pixel_count = request.target.width * request.target.height;

  // The row Q as a sparse action, built by the streaming pixel grid.  The
  // former loop invoked the certified overlap kernel once per target pixel
  // (4096x4096 = 16.7 million host invocations, hours of wall time and one
  // per-pixel vector pair); the grid emits the same (pixel, leaf, a, A) terms
  // in pixel order and the row is accumulated here.
  std::vector<U64> offsets;
  offsets.reserve(static_cast<std::size_t>(pixel_count) + 1);
  offsets.push_back(0);
  std::vector<U64> indices;
  std::vector<double> values;
  std::vector<double> interval_lower;
  std::vector<double> interval_upper;
  indices.reserve(static_cast<std::size_t>(pixel_count) * 2);
  values.reserve(static_cast<std::size_t>(pixel_count) * 2);
  interval_lower.reserve(static_cast<std::size_t>(pixel_count) * 2);
  interval_upper.reserve(static_cast<std::size_t>(pixel_count) * 2);
  std::vector<double> pixel_area(pixel_count, 0.0);
  std::vector<char> pixel_supported(pixel_count, 0);
  bool undecided_support = false;
  bool normalizer_unresolved = false;
  U64 current_pixel = std::numeric_limits<U64>::max();
  std::map<U64, double> merged;
  std::map<U64, double> merged_lower;
  std::map<U64, double> merged_upper;
  double native_area = 0.0;
  double native_area_lower = 0.0;
  double native_area_upper = 0.0;
  const std::map<std::pair<U32, U64>, U64>& leaf_rows = rows;
  auto flush_pixel = [&]() {
    if (current_pixel == std::numeric_limits<U64>::max()) return;
    double total = 0.0;
    double total_lower = 0.0;
    double total_upper = 0.0;
    for (const auto& entry : merged) total += entry.second;
    for (const auto& entry : merged_lower) total_lower += entry.second;
    for (const auto& entry : merged_upper) total_upper += entry.second;
    if (merged.empty() || !(total > 0.0)) {
      offsets.push_back(indices.size());
      merged.clear();
      merged_lower.clear();
      merged_upper.clear();
      native_area = native_area_lower = native_area_upper = 0.0;
      return;
    }
    pixel_supported[current_pixel] = 1;
    pixel_area[current_pixel] = native_area;
    for (const auto& entry : merged) {
      double coefficient = 0.0;
      double coefficient_lower = 0.0;
      double coefficient_upper = 0.0;
      const double entry_lower =
          merged_lower.count(entry.first) ? merged_lower[entry.first] : entry.second;
      const double entry_upper =
          merged_upper.count(entry.first) ? merged_upper[entry.first] : entry.second;
      switch (request.quantity) {
        case ExportRequest::Quantity::supported_mean:
          coefficient = entry.second / total;
          coefficient_lower = total_upper > 0.0 ? entry_lower / total_upper : 0.0;
          coefficient_upper = total_lower > 0.0 ? entry_upper / total_lower
                                                : std::numeric_limits<double>::infinity();
          if (!(total_lower > 0.0)) normalizer_unresolved = true;
          break;
        case ExportRequest::Quantity::supported_flux:
          coefficient = entry.second;
          coefficient_lower = entry_lower;
          coefficient_upper = entry_upper;
          break;
        case ExportRequest::Quantity::extrapolated_full_flux:
          coefficient = native_area * entry.second / total;
          coefficient_lower =
              total_upper > 0.0
                  ? (native_area_lower > 0.0 ? native_area_lower : native_area) * entry_lower /
                        total_upper
                  : 0.0;
          coefficient_upper =
              total_lower > 0.0
                  ? (native_area_upper > 0.0 ? native_area_upper : native_area) * entry_upper /
                        total_lower
                  : std::numeric_limits<double>::infinity();
          if (!(total_lower > 0.0)) normalizer_unresolved = true;
          break;
      }
      if (request.support_policy == SupportMap::Kind::effective_area_approx) {
        const double fraction = area_lower.empty() || area_lower[entry.first] <= 0.0
                                   ? 0.0
                                   : area_lower[entry.first] /
                                         leaf_area_sr(val(leaves)[entry.first].order);
        coefficient = native_area * fraction;
        coefficient_lower =
            (native_area_lower > 0.0 ? native_area_lower : native_area) * fraction;
        coefficient_upper =
            (native_area_upper > 0.0 ? native_area_upper : native_area) * fraction;
      }
      if (coefficient != 0.0) {
        indices.push_back(entry.first);
        values.push_back(coefficient);
        interval_lower.push_back(coefficient_lower);
        interval_upper.push_back(coefficient_upper);
      }
    }
    offsets.push_back(indices.size());
    merged.clear();
    merged_lower.clear();
    merged_upper.clear();
    native_area = native_area_lower = native_area_upper = 0.0;
  };
  const geometry::PixelTermSink sink =
      [&](U64 source_row, const geometry::PixelTerm& term) -> Result<rt::Done> {
        if (source_row != current_pixel) {
          flush_pixel();
          current_pixel = source_row;
          // A pixel without support emits no term: its row is empty and the
          // offset array must still have one entry per pixel.
          while (offsets.size() < source_row + 1) offsets.push_back(indices.size());
        }
        const auto key = std::make_pair(term.order, term.ipix);
        auto row = leaf_rows.find(key);
        if (row == leaf_rows.end()) return model::ok_result(rt::Done{});
        if (!states.empty() && states[row->second] != 1) {
          if (states[row->second] > 1) undecided_support = true;
          return model::ok_result(rt::Done{});
        }
        const double area = 0.5 * (term.overlap_lower + term.overlap_upper);
        if (!(area > 0.0) || !std::isfinite(area)) return model::ok_result(rt::Done{});
        merged[row->second] += area;
        merged_lower[row->second] += term.overlap_lower;
        merged_upper[row->second] += term.overlap_upper;
        const double native = 0.5 * (term.native_lower + term.native_upper);
        native_area = std::max(native_area, native);
        if (term.native_lower > 0.0) {
          native_area_lower = native_area_lower > 0.0
                                  ? std::min(native_area_lower, term.native_lower)
                                  : term.native_lower;
          native_area_upper = std::max(native_area_upper, term.native_upper);
        }
        return model::ok_result(rt::Done{});
      };
  Result<rt::Done> grid = geometry::pixel_grid_overlap(
      context, *request.target.model, request.target.width, request.target.height, 0,
      request.target.height, order, 1.0, request.budget, sink);
  flush_pixel();
  while (offsets.size() < pixel_count + 1) offsets.push_back(indices.size());
  if (!ok(grid)) {
    return fail<ExportResult>(grid.code,
                              grid.assessment.issues.empty()
                                  ? "the target pixel grid overlap failed"
                                  : grid.assessment.issues.front().reason,
                              "build_export", "the export cannot be built");
  }
  if (indices.empty()) {
    offsets.assign(2, 0);
  }
  U64 last_column = 0;
  for (const U64 column : indices) last_column = std::max(last_column, column + 1);
  // The sparse action derives its column count from the largest index, so the
  // trailing leaves are padded with explicit zero entries.
  if (!indices.empty() && last_column < leaf_count) {
    for (U64 column = last_column; column < leaf_count; ++column) {
      indices.push_back(column);
      values.push_back(0.0);
      interval_lower.push_back(0.0);
      interval_upper.push_back(0.0);
    }
    offsets[offsets.size() - 1] = indices.size();
  }

  Id leaf_index_id = leaf_index->identity.object_id;
  auto store_u64 = [&](const std::vector<U64>& data, const std::string& index) -> Result<ArrayRef> {
    const std::vector<std::byte> bytes = model::encode_u64(data);
    const std::vector<U64> shape{static_cast<U64>(data.size())};
    return context.storage.copy(context, DType::u64, bytes, shape, model::unit_dimensionless(),
                                 index);
  };
  auto store_f64 = [&](const std::vector<double>& data, const std::string& index) -> Result<ArrayRef> {
    const std::vector<std::byte> bytes = model::encode_f64(data);
    const std::vector<U64> shape{static_cast<U64>(data.size())};
    return context.storage.copy(context, DType::f64, bytes, shape, model::unit_dimensionless(),
                                 index);
  };
  Result<ArrayRef> offsets_block = store_u64(offsets, "acsd.export.q.offsets");
  if (!ok(offsets_block)) {
    return fail<ExportResult>(offsets_block.code, "the export row cannot be stored",
                              "build_export", "the export cannot be built");
  }
  Result<ArrayRef> indices_block = store_u64(indices, "acsd.export.q.indices");
  if (!ok(indices_block)) {
    return fail<ExportResult>(indices_block.code, "the export row cannot be stored",
                              "build_export", "the export cannot be built");
  }
  Result<ArrayRef> values_block = store_f64(values, "acsd.export.q.values");
  if (!ok(values_block)) {
    return fail<ExportResult>(values_block.code, "the export row cannot be stored",
                              "build_export", "the export cannot be built");
  }

  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::csr;
  recipe.algorithm = "export_target_row_v1";
  recipe.version = 1;
  recipe.spaces.domain = leaf_index;
  recipe.spaces.codomain = std::make_shared<const Index>(
      model::make_pixel_index(pixel_count, "acsd.export.pixels"));
  recipe.spaces.input = model::unit_dimensionless();
  recipe.spaces.output = model::unit_dimensionless();
  const std::vector<ArrayRef> arrays{val(offsets_block), val(indices_block), val(values_block)};
  // The recipe names the arrays of the action: the closure of the row is
  // the offsets, the indices and the values.
  recipe.arrays = {val(offsets_block)->identity.object_id, val(indices_block)->identity.object_id,
                   val(values_block)->identity.object_id};
  Result<Ref<LinearAction>> row_action =
      make_action(context, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (!ok(row_action)) {
    return fail<ExportResult>(row_action.code,
                              std::string("the export row cannot be built: ") +
                                  (row_action.assessment.issues.empty()
                                       ? std::string("no reason")
                                       : row_action.assessment.issues.front().reason),
                              "build_export",
                              "the export cannot be built");
  }

  // The signal of the target: the same row acts on the leaf signal.
  Result<ArrayRef> mapped = apply(context, val(row_action), estimate.signal);
  if (!ok(mapped)) {
    return fail<ExportResult>(mapped.code, "the export row cannot be applied", "build_export",
                              "the export cannot be built");
  }
  Result<std::vector<double>> values_out =
      operators::read_f64(context, val(mapped), 0, pixel_count);
  if (!ok(values_out)) {
    return fail<ExportResult>(values_out.code, "the export signal cannot be read", "build_export",
                              "the export cannot be built");
  }
  // A pixel without support keeps NaN; negative and finite zero values stay.
  std::vector<double> pixel_values = val(values_out);
  for (U64 pixel = 0; pixel < pixel_count; ++pixel) {
    if (!pixel_supported[pixel]) pixel_values[pixel] = std::nan("");
  }
  // The target carries its own signal array: the NaN of a pixel without
  // support has to be part of the published product.
  const std::vector<std::byte> pixel_bytes = model::encode_f64(pixel_values);
  const std::vector<U64> pixel_shape{pixel_count};
  Result<ArrayRef> pixel_array = context.storage.copy(
      context, DType::f64, pixel_bytes, pixel_shape, estimate.unit, "acsd.export.pixels");
  if (!ok(pixel_array)) {
    return fail<ExportResult>(pixel_array.code, "the export signal cannot be stored",
                              "build_export", "the export cannot be built");
  }

  // The numerical bound must include BOTH the leaf value interval and the
  // coefficient interval: output = sum c_i x_i with c_i in [cL,cU] and
  // x_i in [xL,xU].  Using only the leaf bounds (or only the midpoint Q)
  // would understate the exported uncertainty.
  ArrayRef lower_bound;
  ArrayRef upper_bound;
  if (estimate.numerical_lower && estimate.numerical_upper) {
    Result<std::vector<double>> leaf_low =
        operators::read_f64(context, estimate.numerical_lower, 0, leaf_count);
    Result<std::vector<double>> leaf_high =
        operators::read_f64(context, estimate.numerical_upper, 0, leaf_count);
    if (ok(leaf_low) && ok(leaf_high)) {
      std::vector<double> low(pixel_count, std::nan(""));
      std::vector<double> high(pixel_count, std::nan(""));
      for (U64 pixel = 0; pixel < pixel_count; ++pixel) {
        if (!pixel_supported[pixel]) continue;
        double lo = 0.0;
        double hi = 0.0;
        for (U64 k = offsets[pixel]; k < offsets[pixel + 1]; ++k) {
          const U64 row = indices[k];
          const double c_lo = interval_lower[k];
          const double c_hi = interval_upper[k];
          const double x_lo = val(leaf_low)[row];
          const double x_hi = val(leaf_high)[row];
          const double p1 = c_lo * x_lo;
          const double p2 = c_lo * x_hi;
          const double p3 = c_hi * x_lo;
          const double p4 = c_hi * x_hi;
          lo += std::min(std::min(p1, p2), std::min(p3, p4));
          hi += std::max(std::max(p1, p2), std::max(p3, p4));
        }
        low[pixel] = lo;
        high[pixel] = hi;
      }
      Result<ArrayRef> low_array =
          context.storage.copy(context, DType::f64, model::encode_f64(low),
                               std::vector<U64>{pixel_count}, estimate.unit,
                               "acsd.export.pixels");
      Result<ArrayRef> high_array =
          context.storage.copy(context, DType::f64, model::encode_f64(high),
                               std::vector<U64>{pixel_count}, estimate.unit,
                               "acsd.export.pixels");
      if (ok(low_array) && ok(high_array)) {
        lower_bound = val(low_array);
        upper_bound = val(high_array);
      }
    }
  }

  Unit signal_unit = estimate.unit;
  if (request.quantity == ExportRequest::Quantity::supported_mean) {
    signal_unit = model::unit_signal(estimate.unit.scale);
    signal_unit.sr_power = -1;
  } else {
    signal_unit = model::unit_signal(estimate.unit.scale);
    signal_unit.sr_power = 0;
  }

  auto export_estimate = std::make_shared<SignalEstimate>();
  export_estimate->signal = val(pixel_array);
  export_estimate->unit = signal_unit;
  export_estimate->estimator = estimate.estimator;
  export_estimate->numerical_lower = lower_bound;
  export_estimate->numerical_upper = upper_bound;
  export_estimate->support = estimate.support;
  export_estimate->effective_response = estimate.effective_response;
  // The exported signal is Q times the leaf signal, so its covariance is
  // Q C Q^T, not the leaf covariance.  The influence of the roots is folded
  // into Q (C_out = sum_k (Q L_k) F_k F_k^T (Q L_k)^T), so every shared root
  // and every cross leaf term survives the projection; keeping the leaf
  // influence would report the variance of a different estimand.
  const Unit variance_unit = model::unit_multiply(signal_unit, signal_unit);
  auto pixel_index = std::make_shared<Index>();
  pixel_index->size = pixel_count;
  pixel_index->kind = Index::Kind::pixel;
  pixel_index->namespace_id = "acsd.export.pixels";
  pixel_index->identity = model::make_identity(
      "acsd.Index", 2, 0,
      model::sha256_hex("export:pixels:" + std::to_string(pixel_count)), "acsd");
  export_estimate->noise = nullptr;  // null means unknown, never the leaf C
  if (estimate.noise && estimate.noise->model) {
    // The input covariance may itself carry an outer action: C_in = I0 C I0^T,
    // so the total outer action is Q I0.
    Ref<LinearAction> outer = val(row_action);
    bool usable = true;
    if (estimate.noise->influence) {
      Result<Ref<LinearAction>> composed =
          compose(context, val(row_action), estimate.noise->influence);
      if (!ok(composed)) {
        usable = false;
        pending_issues.push_back(
            {composed.code,
             composed.assessment.issues.empty()
                 ? "the export covariance cannot be pushed through Q"
                 : "the export covariance cannot be pushed through Q: " +
                       composed.assessment.issues.front().reason,
             "build_export", "the exported noise stays unknown"});
      } else {
        outer = val(composed);
      }
    }
    if (usable) {
      auto noise_model = std::make_shared<NoiseModel>();
      const NoiseModel& source = *estimate.noise->model;
      noise_model->variance_unit = variance_unit;
      noise_model->condition = source.condition;
      noise_model->state = source.state;
      noise_model->innovations = source.innovations;
      noise_model->shared_factors = source.shared_factors;
      noise_model->influence.reserve(source.influence.size());
      for (const Ref<LinearAction>& influence : source.influence) {
        Result<Ref<LinearAction>> composed = compose(context, outer, influence);
        if (!ok(composed)) {
          usable = false;
          pending_issues.push_back(
              {composed.code,
               composed.assessment.issues.empty()
                   ? "the noise influence cannot be composed with Q"
                   : "the noise influence cannot be composed with Q: " +
                         composed.assessment.issues.front().reason,
               "build_export", "the exported noise stays unknown"});
          break;
        }
        noise_model->influence.push_back(val(composed));
      }
      if (usable) {
        for (const UnknownTerm& term : source.unknown_terms) {
          UnknownTerm copy = term;
          if (term.influence) {
            Result<Ref<LinearAction>> composed = compose(context, outer, term.influence);
            if (!ok(composed)) {
              usable = false;
              break;
            }
            copy.influence = val(composed);
          }
          noise_model->unknown_terms.push_back(copy);
        }
      }
      if (usable) {
        noise_model->identity = model::make_identity(
            "acsd.NoiseModel", 2, 0,
            model::sha256_hex(
                "export:noise:" + val(pixel_array)->identity.object_id + ":" +
                std::to_string(static_cast<int>(request.quantity)) + ":" +
                std::to_string(static_cast<int>(noise_model->state)) + ":" +
                std::to_string(noise_model->unknown_terms.size()) + ":" +
                std::to_string(noise_model->variance_unit.sr_power) + ":" +
                std::to_string(noise_model->variance_unit.linear_power)),
            "acsd");
        noise_model->assessment = model::make_assessment(Code::ok);
        noise_model->assessment.complete =
            noise_model->state == NoiseState::known && noise_model->unknown_terms.empty();
        noise_model->assessment.noise = noise_model->state;
        if (!noise_model->assessment.complete) {
          noise_model->assessment.science = ScienceState::conditional;
          model::set_omitted(noise_model->assessment, "known_complete_variance");
        }
        Result<Ref<CovarianceAction>> covariance = make_covariance(
            context, Ref<NoiseModel>(std::shared_ptr<const NoiseModel>(noise_model)));
        if (ok(covariance)) {
          auto wrapped = std::make_shared<CovarianceAction>(*val(covariance));
          wrapped->input_output.domain = std::shared_ptr<const Index>(pixel_index);
          wrapped->input_output.codomain = wrapped->input_output.domain;
          wrapped->input_output.input = variance_unit;
          wrapped->input_output.output = variance_unit;
          wrapped->identity = model::make_identity(
              "acsd.CovarianceAction", 2, 0,
              model::sha256_hex("export:covariance:" + std::to_string(pixel_count) + ":" +
                                 std::to_string(static_cast<int>(request.quantity))),
              "acsd");
          export_estimate->noise = std::shared_ptr<const CovarianceAction>(wrapped);
        } else {
          usable = false;
          pending_issues.push_back(
              {covariance.code,
               covariance.assessment.issues.empty()
                   ? "the export covariance cannot be built"
                   : "the export covariance cannot be built: " +
                         covariance.assessment.issues.front().reason,
               "build_export", "the exported noise stays unknown"});
        }
      }
    }
    if (!usable) {
      // The assessment is initialised below, before any issue is added, so the
      // reason survives instead of being cleared by the default assessment.
      pending_issues.push_back(
          {Code::unavailable, "the export covariance cannot be pushed through Q",
           "build_export", "the exported noise stays unknown"});
    }
  } else {
    pending_issues.push_back({Code::unavailable,
                              "the leaf estimate publishes no covariance, so the exported "
                              "covariance is unknown",
                              "build_export", "the exported noise stays unknown"});
  }

  export_estimate->error = estimate.error;
  export_estimate->identity = model::make_identity(
      "acsd.SignalEstimate", 2, 0,
      model::sha256_hex(
          "export:signal:" + val(pixel_array)->identity.object_id + ":" +
          std::to_string(static_cast<int>(request.quantity)) + ":" +
          std::to_string(signal_unit.sr_power) + ":" + std::to_string(signal_unit.linear_power)),
      "acsd");
  export_estimate->assessment = model::make_assessment(Code::ok);
  export_estimate->assessment.complete = true;
  export_estimate->assessment.science = ScienceState::bounded;
  // The export is a linear map of the source estimate, so a conditional or
  // incomplete source cannot become bounded/complete here.
  for (const std::string& item : estimate.assessment.omitted) {
    if (std::find(export_estimate->assessment.omitted.begin(),
                  export_estimate->assessment.omitted.end(),
                  item) == export_estimate->assessment.omitted.end()) {
      export_estimate->assessment.omitted.push_back(item);
    }
  }
  for (const Issue& issue : estimate.assessment.issues) {
    export_estimate->assessment.issues.push_back(issue);
  }
  if (estimate.assessment.science == ScienceState::conditional) {
    export_estimate->assessment.science = ScienceState::conditional;
  }
  if (!estimate.assessment.complete) export_estimate->assessment.complete = false;
  if (undecided_support) {
    export_estimate->assessment.science = ScienceState::conditional;
    export_estimate->assessment.complete = false;
    model::set_omitted(export_estimate->assessment,
                       "at_least_one_leaf_has_an_undecided_support_state");
    model::add_issue(export_estimate->assessment, Code::unavailable,
                     "a leaf support state is undecided", "build_export",
                     "the support is conditional on the undecided state");
  }
  if (normalizer_unresolved) {
    export_estimate->assessment.science = ScienceState::conditional;
    export_estimate->assessment.complete = false;
    model::set_omitted(export_estimate->assessment,
                       "the_normalisation_denominator_is_not_bounded_away_from_zero");
    model::add_issue(export_estimate->assessment, Code::unavailable,
                     "the mean normalisation denominator has a zero lower bound",
                     "build_export", "the supported mean has an unbounded numerical interval");
  }
  // The noise state of the published estimate is the state of its covariance:
  // a known covariance is not reported as unknown, and an absent one is.
  export_estimate->assessment.noise =
      export_estimate->noise && export_estimate->noise->model
          ? export_estimate->noise->model->state
          : NoiseState::unknown;
  for (const PendingIssue& pending : pending_issues) {
    model::add_issue(export_estimate->assessment, pending.code, pending.reason, pending.origin,
                     pending.impact);
    if (export_estimate->assessment.science == ScienceState::bounded) {
      export_estimate->assessment.science = ScienceState::conditional;
    }
  }

  // The coverage of the export is recorded: the size of the target, the
  // number of the leaves it touches and the number of the supported pixels.
  // A reader can tell a constant image from a degenerate projection.
  {
    U64 supported = 0;
    for (char supported_pixel : pixel_supported) {
      if (supported_pixel != 0) ++supported;
    }
    export_estimate->assessment.included.push_back(
        "the target covers " + std::to_string(supported) + " of " +
        std::to_string(pixel_count) + " pixels and touches " +
        std::to_string(leaf_count) + " leaves");
  }

  if (approximate) {
    export_estimate->assessment.validity = Validity::approximate;
    model::add_issue(export_estimate->assessment, Code::ok,
                      "the requested support policy uses a named approximation",
                      "build_export", "the sub leaf support shape is approximated",
                      "request the primitives or dyadic_cells policy for the exact shape");
    model::set_included(export_estimate->assessment, "support_approximation");
  }

  ExportResult result;
  result.Q = val(row_action);
  result.estimate = export_estimate;
  result.target = request.target;
  return ok_result(result);
}

}  // namespace acsd
