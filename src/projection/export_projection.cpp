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
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

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
std::vector<std::uint8_t> read_states(Context& context, const ArrayRef& array, U64 count) {
  std::vector<std::uint8_t> states;
  if (!array) return states;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) return states;
  return model::decode_u8(val(view).bytes);
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
  const std::vector<std::uint8_t> states = read_states(context, support.states, leaf_count);
  const std::vector<double> area_lower = read_doubles(context, support.area_lower_sr, leaf_count);
  const std::vector<double> area_upper = read_doubles(context, support.area_upper_sr, leaf_count);

  // The geometry: the target pixel against the leaves of the product order.
  const U32 order = leaf_index->order;
  Result<Ref<FootprintProvider>> provider =
      make_footprint_provider(context, request.target.model);
  if (!ok(provider)) {
    return fail<ExportResult>(provider.code, "the target provider cannot be built", "build_export",
                              "the export cannot be built");
  }
  const U64 pixel_count = request.target.width * request.target.height;

  std::vector<std::vector<std::pair<U64, double>>> rows_of_pixel(pixel_count);
  std::vector<double> pixel_area(pixel_count, 0.0);
  std::vector<char> pixel_supported(pixel_count, 0);

  for (U64 y = 0; y < request.target.height; ++y) {
    for (U64 x = 0; x < request.target.width; ++x) {
      const U64 pixel = y * request.target.width + x;
      if (context.cancellation.requested()) {
        return fail<ExportResult>(Code::cancelled, "the export was cancelled", "build_export",
                                  "the export cannot be built");
      }
      Ref<DetectorFootprint> footprint = make_target_footprint(request.target.model, x, y);
      // pixfrac = 1: the target pixel is never contracted a second time.
      Result<Ref<Overlaps>> overlaps = compute_overlap(context, *val(provider), footprint, order, 1.0,
                                                        request.budget);
      if (!ok(overlaps)) {
        return fail<ExportResult>(overlaps.code,
                                  overlaps.assessment.issues.empty()
                                      ? "the target overlap cannot be computed"
                                      : overlaps.assessment.issues.front().reason,
                                  "build_export", "the export cannot be built");
      }
      // The same leaf may be touched by several pieces; the pieces are merged
      // before the row is formed.
      std::map<U64, double> merged;
      double native_area = 0.0;
      for (const OverlapContribution& contribution : val(overlaps)->contributions) {
        const auto key = std::make_pair(contribution.leaf.order, contribution.leaf.ipix);
        auto row = rows.find(key);
        if (row == rows.end()) continue;  // a leaf outside the product support
        if (!states.empty() && states[row->second] == 0) continue;  // no support at all
        const double area = contribution.overlap_area.lower_sr;
        if (!(area > 0.0)) continue;
        merged[row->second] += area;
        native_area = std::max(native_area, contribution.native_area.lower_sr);
      }
      double total = 0.0;
      for (const auto& entry : merged) total += entry.second;
      if (!(total > 0.0)) continue;  // no support: the pixel stays NaN
      pixel_supported[pixel] = 1;
      pixel_area[pixel] = native_area;
      for (const auto& entry : merged) {
        double coefficient = 0.0;
        switch (request.quantity) {
          case ExportRequest::Quantity::supported_mean:
            coefficient = entry.second / total;
            break;
          case ExportRequest::Quantity::supported_flux:
            coefficient = entry.second;
            break;
          case ExportRequest::Quantity::extrapolated_full_flux:
            coefficient = native_area * entry.second / total;
            break;
        }
        if (request.support_policy == SupportMap::Kind::effective_area_approx) {
          // The effective area approximation replaces the true sub leaf shape
          // by the supported fraction of the leaf.
          const double fraction = area_lower.empty() || area_lower[entry.first] <= 0.0
                                     ? 0.0
                                     : area_lower[entry.first] /
                                           leaf_area_sr(val(leaves)[entry.first].order);
          coefficient = native_area * fraction;
        }
        if (coefficient != 0.0) rows_of_pixel[pixel].emplace_back(entry.first, coefficient);
      }
    }
  }

  // The row Q as a sparse action: rows are the target pixels, columns the
  // leaves of the product.
  std::vector<U64> offsets;
  std::vector<U64> indices;
  std::vector<double> values;
  offsets.push_back(0);
  U64 last_column = 0;
  for (U64 pixel = 0; pixel < pixel_count; ++pixel) {
    std::vector<std::pair<U64, double>>& row = rows_of_pixel[pixel];
    std::sort(row.begin(), row.end());
    for (const auto& entry : row) {
      indices.push_back(entry.first);
      values.push_back(entry.second);
      last_column = std::max(last_column, entry.first + 1);
    }
    offsets.push_back(indices.size());
  }
  if (indices.empty()) {
    // No pixel has support: the action stays empty but well formed.
    offsets.assign(2, 0);
  }
  // The sparse action derives its column count from the largest index, so the
  // trailing leaves are padded with explicit zero entries.
  if (!indices.empty() && last_column < leaf_count) {
    for (U64 column = last_column; column < leaf_count; ++column) {
      indices.push_back(column);
      values.push_back(0.0);
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

  // The numerical bounds travel through the same row: every coefficient of the
  // row is non negative, so the leaf bounds map onto the target bounds.
  ArrayRef lower_bound;
  ArrayRef upper_bound;
  if (estimate.numerical_lower && estimate.numerical_upper) {
    Result<ArrayRef> lower = apply(context, val(row_action), estimate.numerical_lower);
    Result<ArrayRef> upper = apply(context, val(row_action), estimate.numerical_upper);
    if (ok(lower) && ok(upper)) {
      lower_bound = val(lower);
      upper_bound = val(upper);
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
  export_estimate->noise = estimate.noise;
  export_estimate->error = estimate.error;
  export_estimate->identity = model::make_identity(
      "acsd.SignalEstimate", 2, 0,
      model::sha256_hex("export:signal:" + std::to_string(pixel_count) + ":" +
                         std::to_string(static_cast<int>(request.quantity))),
      "acsd");
  export_estimate->assessment = model::make_assessment(Code::ok);
  export_estimate->assessment.complete = true;
  export_estimate->assessment.science = ScienceState::bounded;
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
