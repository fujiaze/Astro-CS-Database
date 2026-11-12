// The streaming drizzle of one detector pixel grid.
//
// Purpose: a 4096x4096 frame at HEALPix order 13 has 16.7 million source rows.
// Building one OverlapContribution per (pixel, leaf) pair before the drizzle
// costs ~592 bytes per record plus a dozen heap strings (measured: 19-28 GiB
// for a full frame), which cannot fit the 12 GiB process budget.  This entry
// point computes the same a_jp/A_j terms with the same certified chart model
// and feeds them directly into the single DrizzleAccumulator; only the per
// leaf aggregates and the compact CSR rows are kept.
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <memory>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/footprint_provider.hpp"
#include "geometry/drizzle_core.hpp"
#include "geometry/drizzle_package.hpp"
#include "geometry/kernel.hpp"
#include "geometry/pixel_grid.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "observation/observation.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
namespace {

using model::fail;
using model::ok_result;

}  // namespace

Result<DrizzleResult> drizzle_grid(Context& context, Ref<FootprintProvider> provider,
                                   Ref<NativeObservation> observation, const Box& window,
                                   const DrizzleRequest& request) {
  if (!provider) {
    return fail<DrizzleResult>(Code::invalid_model, "null_footprint_provider", "drizzle_grid");
  }
  if (!observation) {
    return fail<DrizzleResult>(Code::invalid_model, "null_observation", "drizzle_grid");
  }
  const Ref<WcsModel> wcs = provider->model();
  if (!wcs) {
    return fail<DrizzleResult>(Code::invalid_model, "provider_without_model", "drizzle_grid");
  }
  if (!observation->footprints || observation->footprints->width == 0 ||
      observation->footprints->height == 0) {
    return fail<DrizzleResult>(Code::invalid_request, "no_pixel_grid", "drizzle_grid");
  }
  const U64 width = observation->footprints->width;
  const U64 height = observation->footprints->height;
  // The window is a pixel range in detector coordinates: [lower, upper) with
  // integer pixel indices, 0-based.  It is the streaming unit of the caller.
  const double x0 = std::floor(window.lower[0] + 0.5);
  const double y0 = std::floor(window.lower[1] + 0.5);
  const double x1 = std::floor(window.upper[0] + 0.5);
  const double y1 = std::floor(window.upper[1] + 0.5);
  if (!(x0 >= 0.0) || !(y0 >= 0.0) || !(x1 <= static_cast<double>(width)) ||
      !(y1 <= static_cast<double>(height)) || !(x1 > x0) || !(y1 > y0)) {
    return fail<DrizzleResult>(Code::invalid_request, "window_outside_the_detector", "drizzle_grid",
                               "the pixel window is not a non-empty sub-rectangle of the frame");
  }
  const U64 first_row = static_cast<U64>(y0);
  const U64 row_count = static_cast<U64>(y1 - y0);
  const U64 first_col = static_cast<U64>(x0);
  const U64 last_col = static_cast<U64>(x1);

  Result<std::vector<double>> values = operators::read_f64(
      context, observation->integrated_values, 0, observation->integrated_values->shape[0]);
  if (!ok(values)) {
    return fail<DrizzleResult>(values.code, "native_values_unavailable", "drizzle_grid");
  }
  const std::vector<double>& x = val(values);
  const U64 source_count = static_cast<U64>(x.size());

  // Saturated samples carry no usable signal: integrating 65535 as if it were a
  // measurement published a saturation plateau (the S1 review measured 22k-33k
  // such pixels per frame and an output maximum of 2.98e15, exactly the ADC
  // ceiling).  They are excluded from both the numerator and the normaliser and
  // the removed count is named on the estimate.
  std::vector<std::uint16_t> quality;
  if (observation->dq) {
    Result<std::vector<std::uint16_t>> flags =
        acsd::observation::read_u16_values(context, observation->dq);
    if (ok(flags)) quality = val(flags);
  }
  U64 saturated_samples = 0;      // (pixel, leaf) terms excluded
  U64 saturated_pixels = 0;       // distinct native samples at the ADC ceiling
  std::set<std::pair<U32, U64>> saturated_leaves;
  std::vector<std::uint8_t> saturated_row(0);
  const U64 window_samples = row_count * width;

  geometry::DrizzleAccumulator accumulator;
  bool failed = false;
  Result<rt::Done> failure;
  const geometry::PixelTermSink sink = [&](U64 source_row,
                                           const geometry::PixelTerm& term) -> Result<rt::Done> {
    if (source_row >= source_count) {
      return fail<rt::Done>(Code::invalid_model, "source_row_out_of_range", "drizzle_grid",
                            "a pixel row of the window is outside the native values");
    }
    if (source_row < quality.size() &&
        (quality[source_row] & acsd::observation::kDqSaturated) != 0u) {
      ++saturated_samples;
      if (saturated_row.size() < quality.size()) saturated_row.resize(quality.size(), 0);
      if (saturated_row[static_cast<std::size_t>(source_row)] == 0) {
        saturated_row[static_cast<std::size_t>(source_row)] = 1;
        ++saturated_pixels;
      }
      // The saturated sample contributes nothing to the drizzle operator: its
      // weight is not a measurement.  The leaf is remembered only so that an
      // all-saturated leaf can be appended to the index as absent after the
      // walk.  A zero-overlap accumulator term would be given weight_nominal
      // 0.5 and would enter the CSR as R = 0.5/N (measured: 20k saturated
      // (pixel,leaf) entries and 113 leaves with sum(Rx) != signal); the term
      // must stay outside the operator.
      saturated_leaves.insert({term.order, term.ipix});
      return model::ok_result(rt::Done{});
    }
    geometry::DrizzleTerm drizzle_term;
    drizzle_term.source_row = source_row;
    drizzle_term.order = term.order;
    drizzle_term.ipix = term.ipix;
    drizzle_term.overlap_lower = term.overlap_lower;
    drizzle_term.overlap_upper = term.overlap_upper;
    drizzle_term.drop_lower = term.drop_lower;
    drizzle_term.drop_upper = term.drop_upper;
    drizzle_term.omega_lower = term.native_lower;
    drizzle_term.omega_upper = term.native_upper;
    drizzle_term.value = x[source_row];
    return accumulator.add(drizzle_term);
  };

  // The row window is walked in complete detector rows; the column range is
  // handled by the pixel grid (a whole row is the streaming unit).
  (void)first_col;
  (void)last_col;
  // The pixel grid always walks whole rows: a column window is expressed by
  // the detector footprint width, not by a shifted grid.  A partial column
  // range is therefore refused rather than silently resampled.
  if (first_col != 0 || last_col != width) {
    return fail<DrizzleResult>(Code::unsupported, "partial_column_window", "drizzle_grid",
                               "the streaming drizzle walks whole detector rows; a partial "
                               "column window would change the source row identity");
  }
  Result<rt::Done> walked = geometry::pixel_grid_overlap(
      context, *wcs, width, height, first_row, row_count, request.order, request.pixfrac,
      request.budget, [&](U64 source_row, const geometry::PixelTerm& term) -> Result<rt::Done> {
        if (failed) {
          return failure;
        }
        Result<rt::Done> added = sink(source_row, term);
        if (!ok(added)) {
          failed = true;
          failure = added;
        }
        return added;
      });
  if (!ok(walked)) {
    return fail<DrizzleResult>(walked.code, "pixel_grid_failed", "drizzle_grid",
                               walked.assessment.issues.empty()
                                   ? "the pixel grid overlap failed"
                                   : walked.assessment.issues.front().reason);
  }
  geometry::DrizzleLeafOutput leaf_output = accumulator.finish();
  // Append the all-saturated leaves that the walk touched but that have no
  // usable sample: they stay in the index as absent (NaN signal, zero area)
  // and the export's NaN pixels can be attributed to them.  They never enter
  // the operator.
  if (!saturated_leaves.empty()) {
    std::set<std::pair<U32, U64>> present;
    for (const HealpixLeaf& leaf : leaf_output.leaves) present.insert({leaf.order, leaf.ipix});
    std::vector<HealpixLeaf> appended;
    for (const auto& id : saturated_leaves) {
      if (present.count(id) != 0) continue;
      appended.push_back(HealpixLeaf{id.first, id.second});
    }
    if (!appended.empty()) {
      // Merge by (order, ipix), keeping the operator rows of the present
      // leaves in the same relative order and giving every absent leaf an
      // empty CSR row (offsets monotone, length leaves+1).
      std::vector<HealpixLeaf> all = leaf_output.leaves;
      all.insert(all.end(), appended.begin(), appended.end());
      std::vector<std::size_t> order(all.size());
      for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
      std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (all[a].order != all[b].order) return all[a].order < all[b].order;
        return all[a].ipix < all[b].ipix;
      });
      const std::size_t old_count = leaf_output.leaves.size();
      std::vector<HealpixLeaf> leaves;
      std::vector<double> normalizer, signal, signal_lower, signal_upper, area_lower, area_upper;
      std::vector<std::uint8_t> states, normalization, observational;
      std::vector<U64> merged_offsets{0};
      std::vector<U64> merged_indices;
      std::vector<double> merged_values;
      for (std::size_t index : order) {
        if (index < old_count) {
          leaves.push_back(leaf_output.leaves[index]);
          normalizer.push_back(leaf_output.normalizer[index]);
          signal.push_back(leaf_output.signal[index]);
          signal_lower.push_back(leaf_output.signal_lower[index]);
          signal_upper.push_back(leaf_output.signal_upper[index]);
          states.push_back(leaf_output.support_states[index]);
          normalization.push_back(leaf_output.normalization_states[index]);
          observational.push_back(leaf_output.observational_states[index]);
          area_lower.push_back(leaf_output.area_lower[index]);
          area_upper.push_back(leaf_output.area_upper[index]);
          const U64 begin = leaf_output.row_offsets[index];
          const U64 finish = leaf_output.row_offsets[index + 1];
          for (U64 k = begin; k < finish; ++k) {
            merged_indices.push_back(leaf_output.row_indices[k]);
            merged_values.push_back(leaf_output.row_values[k]);
          }
          merged_offsets.push_back(static_cast<U64>(merged_indices.size()));
        } else {
          leaves.push_back(all[index]);
          normalizer.push_back(0.0);
          signal.push_back(std::numeric_limits<double>::quiet_NaN());
          signal_lower.push_back(std::numeric_limits<double>::quiet_NaN());
          signal_upper.push_back(std::numeric_limits<double>::quiet_NaN());
          states.push_back(0);
          normalization.push_back(0);
          observational.push_back(1);
          area_lower.push_back(0.0);
          area_upper.push_back(0.0);
          merged_offsets.push_back(static_cast<U64>(merged_indices.size()));
        }
      }
      leaf_output.leaves = std::move(leaves);
      leaf_output.normalizer = std::move(normalizer);
      leaf_output.signal = std::move(signal);
      leaf_output.signal_lower = std::move(signal_lower);
      leaf_output.signal_upper = std::move(signal_upper);
      leaf_output.support_states = std::move(states);
      leaf_output.normalization_states = std::move(normalization);
      leaf_output.observational_states = std::move(observational);
      leaf_output.area_lower = std::move(area_lower);
      leaf_output.area_upper = std::move(area_upper);
      leaf_output.row_offsets = std::move(merged_offsets);
      leaf_output.row_indices = std::move(merged_indices);
      leaf_output.row_values = std::move(merged_values);
      leaf_output.any_absent = true;
    }
  }
  if (leaf_output.any_absent && !leaf_output.any_present) {
    return fail<DrizzleResult>(Code::no_support, "no_support_rows_present", "drizzle_grid",
                               "no target leaf has a positive support");
  }

  DrizzleGeometrySource source;
  source.overlaps = nullptr;
  source.primitives_id = Id{};
  source.recipe_identity_proven = true;
  source.overlaps_certified = true;
  source.geometry_termination_met = true;
  geometry::ContentDigest digest;
  digest.add("schema", "acsd.drizzle.pixel_grid");
  digest.add("wcs", wcs->identity.object_id);
  digest.add_u64("width", width);
  digest.add_u64("height", height);
  digest.add_u64("first_row", first_row);
  digest.add_u64("row_count", row_count);
  digest.add_u64("order", request.order);
  digest.add_f64("pixfrac", request.pixfrac);
  source.geometry_id = "acsd.drizzle.grid:" + digest.hex();

  Result<DrizzleResult> packaged =
      package_drizzle(context, leaf_output, observation, request, source_count, source);
  if (!ok(packaged)) {
    return packaged;
  }
  if (saturated_samples > 0 && packaged.value->estimate) {
    auto copy = std::make_shared<SignalEstimate>(*packaged.value->estimate);
    char text[256];
    std::snprintf(text, sizeof(text),
                  "saturated_pixel_count=%llu (raw==65535); %llu (pixel,leaf) "
                  "terms excluded from the drizzle",
                  static_cast<unsigned long long>(saturated_pixels),
                  static_cast<unsigned long long>(saturated_samples));
    model::add_issue(copy->assessment, Code::unsupported, text, "drizzle_grid",
                     "the saturated samples carry no usable flux; the photometry of the "
                     "saturated core is undefined",
                     "use unsaturated frames or a shorter exposure");
    model::set_included(copy->assessment,
                        "saturated_pixel_count=" + std::to_string(saturated_pixels) +
                            ";saturated_pixel_leaf_terms=" + std::to_string(saturated_samples));
    model::set_omitted(copy->assessment,
                       "no_nonlinearity_flag_below_the_adc_ceiling");
    packaged.value->estimate = copy;
  }
  return packaged;
}

}  // namespace acsd
