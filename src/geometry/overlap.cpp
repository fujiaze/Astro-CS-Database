#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/healpix.hpp"
#include "coordinates/chart.hpp"
#include "geometry/containment.hpp"
#include "geometry/kernel.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"

namespace acsd {
using acsd::coordinates::chart_inverse;
using acsd::coordinates::ChartPoint;
namespace {

using geometry::AreaBound;
using model::Rational;

constexpr U32 kMaxInlineContributions = 4096;

// ---------------------------------------------------------------------------
// Small array helpers (every output array goes through the storage lease)
// ---------------------------------------------------------------------------

Result<ArrayRef> store_array(Context& context, DType dtype, const std::vector<std::byte>& bytes,
                              std::span<const U64> shape, const Unit& unit, const Id& index_id) {
  return context.storage.copy(context, dtype, bytes, shape, unit, index_id);
}

Result<ArrayRef> store_u64(Context& context, const std::vector<U64>& values, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u64(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return store_array(context, DType::u64, bytes, shape, model::unit_dimensionless(), index_id);
}

Result<ArrayRef> store_u32(Context& context, const std::vector<U32>& values, const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u32(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return store_array(context, DType::u32, bytes, shape, model::unit_dimensionless(), index_id);
}

Result<ArrayRef> store_u8(Context& context, const std::vector<std::uint8_t>& values,
                          const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_u8(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return store_array(context, DType::u8, bytes, shape, model::unit_dimensionless(), index_id);
}

Result<ArrayRef> store_dyadic(Context& context, const std::vector<Dyadic>& values,
                              const Id& index_id) {
  const std::vector<std::byte> bytes = model::encode_dyadic(values);
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  return store_array(context, DType::dyadic, bytes, shape, model::unit_sr(), index_id);
}

ExactInterval exact_from_dyadic(const Dyadic& lower, const Dyadic& upper) {
  ExactInterval out;
  out.lower = lower;
  out.upper = upper;
  out.precision_bits = 53;
  return out;
}

SphericalAreaInterval area_interval(const Dyadic& lower, const Dyadic& upper,
                                    const Id& simultaneous) {
  SphericalAreaInterval area;
  area.lower_sr = geometry::dyadic_to_double(lower);
  area.upper_sr = geometry::dyadic_to_double(upper);
  area.simultaneous_bound = simultaneous;
  area.exact = exact_from_dyadic(lower, upper);
  return area;
}

// Reads an u64 array from the storage.
Result<std::vector<U64>> read_u64(Context& context, ArrayRef array, U64 count) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) return model::fail<std::vector<U64>>(view.code, "primitive_read_failed", "read_u64");
  return model::ok_result(model::decode_u64(val(view).bytes));
}

Result<std::vector<std::uint8_t>> read_u8(Context& context, ArrayRef array, U64 count) {
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
  if (!ok(view)) return model::fail<std::vector<std::uint8_t>>(view.code, "primitive_read_failed", "read_u8");
  const auto& bytes = val(view).bytes;
  std::vector<std::uint8_t> out(bytes.size());
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    out[i] = std::to_integer<std::uint8_t>(bytes[i]);
  }
  return model::ok_result(out);
}

// The rings of a detector footprint in the detector plane.
std::vector<std::vector<std::array<double, 2>>> footprint_rings(const DetectorFootprint& source) {
  std::vector<std::vector<std::array<double, 2>>> rings;
  for (const Ring& ring : source.detector_rings) {
    std::vector<std::array<double, 2>> points;
    for (const PixelEdge0& vertex : ring.vertices) {
      points.push_back({vertex.x, vertex.y});
    }
    rings.push_back(std::move(points));
  }
  return rings;
}

bool rings_are_axis_aligned_rectangle(const std::vector<std::vector<std::array<double, 2>>>& rings,
                                      Box& rectangle) {
  if (rings.size() != 1 || rings[0].size() != 4) return false;
  double x_lo = std::numeric_limits<double>::infinity();
  double x_hi = -std::numeric_limits<double>::infinity();
  double y_lo = std::numeric_limits<double>::infinity();
  double y_hi = -std::numeric_limits<double>::infinity();
  for (const auto& vertex : rings[0]) {
    x_lo = std::min(x_lo, vertex[0]);
    x_hi = std::max(x_hi, vertex[0]);
    y_lo = std::min(y_lo, vertex[1]);
    y_hi = std::max(y_hi, vertex[1]);
  }
  rectangle = Box{{x_lo, y_lo}, {x_hi, y_hi}};
  return true;
}

// The pixfrac contraction of the rings about the contraction centre.
std::vector<std::vector<std::array<double, 2>>> contract_rings(
    const std::vector<std::vector<std::array<double, 2>>>& rings, double pixfrac,
    const PixelCenter0& centre) {
  std::vector<std::vector<std::array<double, 2>>> out;
  for (const auto& ring : rings) {
    std::vector<std::array<double, 2>> points;
    for (const auto& vertex : ring) {
      points.push_back({centre.x + pixfrac * (vertex[0] - centre.x),
                        centre.y + pixfrac * (vertex[1] - centre.y)});
    }
    out.push_back(std::move(points));
  }
  return out;
}

}  // namespace

// ---------------------------------------------------------------------------
// detector_footprint
// ---------------------------------------------------------------------------

Result<Ref<DetectorFootprint>> detector_footprint(Context& context, Ref<FootprintSet> footprints,
                                                   U64 source_row) {
  using model::fail;
  using model::ok_result;
  if (!footprints) {
    return fail<Ref<DetectorFootprint>>(Code::invalid_model, "null_footprint_set", "detector_footprint");
  }
  const FootprintSet& set = *footprints;
  if (set.index && set.index->size > 0 && source_row >= set.index->size) {
    return fail<Ref<DetectorFootprint>>(Code::invalid_request, "source_row_out_of_range",
                                        "detector_footprint");
  }

  DetectorFootprint footprint;
  footprint.wcs = set.wcs;
  footprint.pixel_response = set.pixel_response;
  footprint.source = model::source_at(*set.index, source_row);
  footprint.identity = model::make_identity("acsd.DetectorFootprint", 2, 0,
                                             model::sha256_hex(std::to_string(source_row)), "acsd");
  footprint.assessment = model::make_assessment(Code::ok);
  footprint.assessment.complete = true;

  // The default rectangular collection domain of one sample.  The sample to
  // detector transform must be invertible, otherwise the collection domain
  // degenerates and the area ledger would silently collapse to zero.
  const double transform_det = set.detector_from_sample[0] * set.detector_from_sample[3] -
                               set.detector_from_sample[1] * set.detector_from_sample[2];
  if (!std::isfinite(transform_det) || transform_det == 0.0) {
    return fail<Ref<DetectorFootprint>>(Code::invalid_model, "degenerate_detector_from_sample",
                                        "detector_footprint");
  }
  const double column = static_cast<double>(source_row % std::max<U64>(1, set.width));
  const double row = static_cast<double>(source_row / std::max<U64>(1, set.width));
  const double local[2] = {column, row};
  const Mat2& m = set.detector_from_sample;
  const double centre_x = m[0] * local[0] + m[1] * local[1] + set.offset[0];
  const double centre_y = m[2] * local[0] + m[3] * local[1] + set.offset[1];
  footprint.contraction_center = PixelCenter0{centre_x, centre_y};

  bool has_exception = false;
  if (set.exceptions) {
    Result<std::vector<U64>> sample_rows =
        read_u64(context, set.exceptions->sample_rows, set.exceptions->sample_rows->shape[0]);
    if (!ok(sample_rows)) {
      return fail<Ref<DetectorFootprint>>(sample_rows.code, "exception_index_read_failed",
                                          "detector_footprint");
    }
    const auto& rows = val(sample_rows);
    const auto it = std::lower_bound(rows.begin(), rows.end(), source_row);
    has_exception = it != rows.end() && *it == source_row;
    if (has_exception) {
      const std::size_t position = static_cast<std::size_t>(it - rows.begin());
      Result<std::vector<U64>> ring_offsets =
          read_u64(context, set.exceptions->ring_offsets, set.exceptions->ring_offsets->shape[0]);
      if (!ok(ring_offsets)) {
        return fail<Ref<DetectorFootprint>>(ring_offsets.code, "exception_index_read_failed",
                                            "detector_footprint");
      }
      Result<std::vector<U64>> vertex_offsets =
          read_u64(context, set.exceptions->vertex_offsets, set.exceptions->vertex_offsets->shape[0]);
      if (!ok(vertex_offsets)) {
        return fail<Ref<DetectorFootprint>>(vertex_offsets.code, "exception_index_read_failed",
                                            "detector_footprint");
      }
      const auto& ring_off = val(ring_offsets);
      const auto& vertex_off = val(vertex_offsets);
      if (position + 1 >= ring_off.size()) {
        return fail<Ref<DetectorFootprint>>(Code::invalid_model, "exception_ring_offset_range",
                                            "detector_footprint");
      }
      Result<std::vector<std::uint8_t>> hole_flags =
          read_u8(context, set.exceptions->hole_flags, set.exceptions->hole_flags->shape[0]);
      if (!ok(hole_flags)) {
        return fail<Ref<DetectorFootprint>>(hole_flags.code, "exception_index_read_failed",
                                            "detector_footprint");
      }
      Result<ArrayView> vertices =
          context.storage.read(context, set.exceptions->vertices,
                                BlockRequest{0, set.exceptions->vertices->shape[0]});
      if (!ok(vertices)) {
        return fail<Ref<DetectorFootprint>>(vertices.code, "exception_vertex_read_failed",
                                            "detector_footprint");
      }
      const std::vector<double> vertex_data = model::decode_f64(val(vertices).bytes);
      for (U64 ring = ring_off[position]; ring < ring_off[position + 1]; ++ring) {
        if (ring + 1 >= vertex_off.size()) {
          return fail<Ref<DetectorFootprint>>(Code::invalid_model, "exception_vertex_offset_range",
                                              "detector_footprint");
        }
        Ring out;
        out.hole = ring < val(hole_flags).size() ? val(hole_flags)[ring] != 0 : false;
        for (U64 vertex = vertex_off[ring]; vertex < vertex_off[ring + 1]; ++vertex) {
          const std::size_t base = static_cast<std::size_t>(vertex) * 2;
          if (base + 1 >= vertex_data.size()) {
            return fail<Ref<DetectorFootprint>>(Code::invalid_model, "exception_vertex_range",
                                                "detector_footprint");
          }
          out.vertices.push_back(PixelEdge0{vertex_data[base], vertex_data[base + 1]});
        }
        footprint.detector_rings.push_back(std::move(out));
      }
    }
  }

  if (!has_exception) {
    Ring out;
    out.hole = false;
    const double hx = 0.5 * (set.local_cell.upper[0] - set.local_cell.lower[0]);
    const double hy = 0.5 * (set.local_cell.upper[1] - set.local_cell.lower[1]);
    const double cx = 0.5 * (set.local_cell.lower[0] + set.local_cell.upper[0]);
    const double cy = 0.5 * (set.local_cell.lower[1] + set.local_cell.upper[1]);
    const double corners[4][2] = {{cx - hx, cy - hy}, {cx + hx, cy - hy}, {cx + hx, cy + hy},
                                  {cx - hx, cy + hy}};
    for (const auto& corner : corners) {
      const double x = m[0] * corner[0] + m[1] * corner[1] + set.offset[0];
      const double y = m[2] * corner[0] + m[3] * corner[1] + set.offset[1];
      out.vertices.push_back(PixelEdge0{x, y});
    }
    footprint.detector_rings.push_back(std::move(out));
  }

  return ok_result(std::make_shared<const DetectorFootprint>(std::move(footprint)));
}

// ---------------------------------------------------------------------------
// footprint
// ---------------------------------------------------------------------------

Result<Ref<CurvedBoundary>> footprint(Context& context, const FootprintProvider& provider,
                                      Ref<DetectorFootprint> source, double pixfrac,
                                      const GeometryBudget& request) {
  using model::fail;
  using model::ok_result;
  (void)request;
  if (!source) {
    return fail<Ref<CurvedBoundary>>(Code::invalid_model, "null_detector_footprint", "footprint");
  }
  if (!(pixfrac > 0.0) || pixfrac > 1.0) {
    return fail<Ref<CurvedBoundary>>(Code::invalid_request, "invalid_pixfrac", "footprint");
  }
  if (context.cancellation.requested()) {
    return fail<Ref<CurvedBoundary>>(Code::cancelled, "cancelled", "footprint");
  }
  const auto rings = contract_rings(footprint_rings(*source), pixfrac, source->contraction_center);

  CurvedBoundary boundary;
  boundary.identity = model::make_identity("acsd.CurvedBoundary", 2, 0,
                                           model::sha256_hex(source->source.dataset), "acsd");
  boundary.assessment = model::make_assessment(Code::ok);
  boundary.assessment.complete = true;
  std::vector<CurvedEdge> edges;
  for (std::size_t ring_index = 0; ring_index < rings.size(); ++ring_index) {
    const auto& ring = rings[ring_index];
    if (ring.size() < 2) continue;
    for (std::size_t i = 0; i < ring.size(); ++i) {
      const auto& a = ring[i];
      const auto& b = ring[(i + 1) % ring.size()];
      CurvedEdge edge;
      // The Bernstein control points of one straight detector edge.
      edge.bernstein.push_back({model::exact_point(a[0]), model::exact_point(a[1])});
      edge.bernstein.push_back({model::exact_point(b[0]), model::exact_point(b[1])});
      edge.parameter = Interval(0.0, 1.0);
      edge.reversed = false;
      edge.common_edge = "edge:" + source->source.dataset + ":" + std::to_string(ring_index) + ":" +
                         std::to_string(i);
      edges.push_back(std::move(edge));
    }
  }
  boundary.rings.push_back(std::move(edges));

  // The provider must be able to enclose this boundary.
  for (const CurvedEdge& edge : boundary.rings.front()) {
    Result<EnclosedSkyRegion> enclosure = provider.boundary_interval(context, edge, edge.parameter);
    if (!ok(enclosure)) {
      return fail<Ref<CurvedBoundary>>(enclosure.code, "boundary_enclosure_unavailable",
                                       "footprint");
    }
  }
  return ok_result(std::make_shared<const CurvedBoundary>(std::move(boundary)));
}

// ---------------------------------------------------------------------------
// measure
// ---------------------------------------------------------------------------

Result<SphericalAreaInterval> measure(Context& context, const FootprintProvider& provider,
                                      Ref<DetectorFootprint> source, const GeometryBudget& request) {
  using model::fail;
  using model::ok_result;
  if (!source) {
    return fail<SphericalAreaInterval>(Code::invalid_model, "null_detector_footprint", "measure");
  }
  if (context.cancellation.requested()) {
    return fail<SphericalAreaInterval>(Code::cancelled, "cancelled", "measure");
  }
  Ref<WcsModel> model = provider.model();
  if (!model) {
    return fail<SphericalAreaInterval>(Code::invalid_model, "provider_without_model", "measure");
  }
  const AreaBound bound = geometry::footprint_area(*model, footprint_rings(*source));
  if (geometry::dyadic_to_double(bound.upper) <= 0.0 &&
      geometry::dyadic_to_double(bound.lower) == 0.0) {
    return fail<SphericalAreaInterval>(Code::unsupported, "footprint_area_not_representable",
                                       "measure");
  }
  return ok_result(area_interval(bound.lower, bound.upper, "measure:native_area"));
}

// ---------------------------------------------------------------------------
// compute_overlap
// ---------------------------------------------------------------------------

Result<Ref<Overlaps>> compute_overlap(Context& context, const FootprintProvider& provider,
                                       Ref<DetectorFootprint> source, U32 order, double pixfrac,
                                       const GeometryBudget& request) {
  using model::fail;
  using model::ok_result;
  if (!source) {
    return fail<Ref<Overlaps>>(Code::invalid_model, "null_detector_footprint", "compute_overlap");
  }
  if (order > coordinates::kMaxOrder) {
    return fail<Ref<Overlaps>>(Code::invalid_request, "order_out_of_range", "compute_overlap");
  }
  if (!(pixfrac > 0.0) || pixfrac > 1.0) {
    return fail<Ref<Overlaps>>(Code::invalid_request, "invalid_pixfrac", "compute_overlap");
  }
  if (context.cancellation.requested()) {
    return fail<Ref<Overlaps>>(Code::cancelled, "cancelled", "compute_overlap");
  }
  Ref<WcsModel> model = provider.model();
  if (!model) {
    return fail<Ref<Overlaps>>(Code::invalid_model, "provider_without_model", "compute_overlap");
  }

  const auto native_rings = footprint_rings(*source);
  const auto drop_rings = contract_rings(native_rings, pixfrac, source->contraction_center);
  Box drop_rectangle{};
  if (!rings_are_axis_aligned_rectangle(drop_rings, drop_rectangle)) {
    Result<Ref<Overlaps>> out = fail<Ref<Overlaps>>(
        Code::unsupported, "non_rectangular_footprint_not_supported", "compute_overlap:source_footprint",
        "the production cut cell kernel and the containment fallback require a rectangular "
        "collection domain, the overlap of this footprint is not published",
        "use a rectangular collection domain or a footprint without holes");
    model::add_issue(out.assessment, Code::unsupported, "non_rectangular_footprint_not_supported",
                     "compute_overlap:source_footprint",
                     "the overlap of this footprint is not published; the caller must use a "
                     "rectangular collection domain",
                     "use a rectangular collection domain or a footprint without holes");
    return out;
  }

  const AreaBound native = geometry::footprint_area(*model, native_rings);
  const AreaBound drop = geometry::footprint_area(*model, drop_rings);
  if (geometry::dyadic_to_double(native.upper) <= 0.0) {
    return fail<Ref<Overlaps>>(Code::unsupported, "footprint_area_not_representable",
                               "compute_overlap");
  }

  const geometry::KernelOutcome kernel = geometry::cut_cell_overlap(*model, drop_rectangle, order,
                                                                      request);
  if (kernel.leaves.size() > kMaxInlineContributions) {
    return fail<Ref<Overlaps>>(Code::resource_exhausted, "too_many_contributions", "compute_overlap");
  }

  // The second algorithm: the equal-area containment integral A (SPHERICAL_DRIZZLE
  // section 3).  It proves for every candidate target cell whether the whole
  // cell lies inside or outside the source region and keeps the whole cell
  // mass of an undecided cell as the error bound.  Its per leaf intervals are
  // an independent enclosure of the same overlap mass, so the published
  // interval is the intersection of the two algorithms.
  const geometry::ContainmentOutcome containment =
      geometry::target_containment(context, *model, model, drop_rectangle, order, request);
  std::map<std::pair<U32, U64>, geometry::ContainmentLeaf> containment_leaves;
  for (const geometry::ContainmentLeaf& leaf : containment.leaves) {
    containment_leaves[{leaf.leaf.order, leaf.leaf.ipix}] = leaf;
  }

  // --- the primitive table ------------------------------------------------
  std::vector<U64> primitive_source;
  std::vector<U32> primitive_orders;
  std::vector<U64> primitive_ipix;
  std::vector<std::uint8_t> primitive_kinds;
  std::vector<Dyadic> primitive_lower;
  std::vector<Dyadic> primitive_upper;

  auto push_primitive = [&](std::uint8_t kind, std::optional<HealpixLeaf> leaf, const Dyadic& lower,
                             const Dyadic& upper) {
    primitive_source.push_back(0);
    if (leaf.has_value()) {
      primitive_orders.push_back(leaf->order);
      primitive_ipix.push_back(leaf->ipix);
    } else {
      primitive_orders.push_back(UINT32_MAX);
      primitive_ipix.push_back(UINT64_MAX);
    }
    primitive_kinds.push_back(kind);
    primitive_lower.push_back(lower);
    primitive_upper.push_back(upper);
  };

  const U64 native_row = primitive_source.size();
  push_primitive(0, std::nullopt, native.lower, native.upper);
  const U64 drop_row = primitive_source.size();
  push_primitive(1, std::nullopt, drop.lower, drop.upper);

  std::vector<U64> overlap_source;
  std::vector<U32> overlap_orders;
  std::vector<U64> overlap_ipix;
  std::vector<U64> overlap_native;
  std::vector<U64> overlap_drop;
  std::vector<U64> overlap_overlap;

  std::vector<OverlapContribution> contributions;
  std::map<std::pair<U32, U64>, std::pair<Dyadic, Dyadic>> published;
  for (const geometry::LeafAreaBound& leaf : kernel.leaves) {
    published[{leaf.leaf.order, leaf.leaf.ipix}] = {leaf.lower, leaf.upper};
  }
  // The containment algorithm is the fallback for the leaves that the cut cell
  // kernel could not resolve.  It is only used for a leaf without any kernel
  // contribution: the two algorithms cover different cell sets for such a
  // leaf, so their masses add instead of intersecting.
  model::BigInt containment_added_lower(0);
  model::BigInt containment_added_upper(0);
  for (const auto& entry : containment_leaves) {
    if (published.find(entry.first) != published.end()) continue;
    published[entry.first] = {entry.second.lower, entry.second.upper};
    containment_added_lower =
        containment_added_lower + model::BigInt::from_string(entry.second.lower.signed_integer);
    containment_added_upper =
        containment_added_upper + model::BigInt::from_string(entry.second.upper.signed_integer);
  }
  for (const auto& entry : published) {
    const HealpixLeaf leaf{entry.first.first, entry.first.second};
    const Dyadic& lower = entry.second.first;
    const Dyadic& upper = entry.second.second;
    const U64 row = primitive_source.size();
    push_primitive(2, leaf, lower, upper);
    overlap_source.push_back(0);
    overlap_orders.push_back(leaf.order);
    overlap_ipix.push_back(leaf.ipix);
    overlap_native.push_back(native_row);
    overlap_drop.push_back(drop_row);
    overlap_overlap.push_back(row);
    OverlapContribution contribution;
    contribution.source = source->source;
    contribution.leaf = leaf;
    contribution.native_area = area_interval(native.lower, native.upper, "primitive:native");
    contribution.drop_area = area_interval(drop.lower, drop.upper, "primitive:drop");
    contribution.overlap_area = area_interval(lower, upper, "primitive:overlap");
    contribution.shared_geometry_constraints = "geometry:constraints";
    contributions.push_back(std::move(contribution));
  }

  const U64 coarse_row = primitive_source.size();
  Dyadic coarse_upper = kernel.unresolved_upper;
  {
    // The mass that the containment algorithm proved inside the source region
    // leaves the unresolved mass.  Only the proven part is removed, so the
    // coarse row stays an upper bound of the unresolved mass.
    const model::BigInt kernel_upper = model::BigInt::from_string(coarse_upper.signed_integer);
    const model::BigInt removed = containment_added_upper;
    const model::BigInt remaining = kernel_upper > removed ? kernel_upper - removed : model::BigInt(0);
    coarse_upper.signed_integer = remaining.to_string();
  }
  push_primitive(3, std::nullopt, Dyadic{"0", 0}, coarse_upper);

  const Id primitive_index = "acsd.geometry.primitives";
  Result<ArrayRef> source_rows_array = store_u64(context, primitive_source, primitive_index);
  if (!ok(source_rows_array)) {
    return fail<Ref<Overlaps>>(source_rows_array.code, "primitive_store_failed", "compute_overlap");
  }
  Result<ArrayRef> orders_array = store_u32(context, primitive_orders, primitive_index);
  if (!ok(orders_array)) {
    return fail<Ref<Overlaps>>(orders_array.code, "primitive_store_failed", "compute_overlap");
  }
  Result<ArrayRef> ipix_array = store_u64(context, primitive_ipix, primitive_index);
  if (!ok(ipix_array)) {
    return fail<Ref<Overlaps>>(ipix_array.code, "primitive_store_failed", "compute_overlap");
  }
  Result<ArrayRef> kinds_array = store_u8(context, primitive_kinds, primitive_index);
  if (!ok(kinds_array)) {
    return fail<Ref<Overlaps>>(kinds_array.code, "primitive_store_failed", "compute_overlap");
  }
  Result<ArrayRef> lower_array = store_dyadic(context, primitive_lower, primitive_index);
  if (!ok(lower_array)) {
    return fail<Ref<Overlaps>>(lower_array.code, "primitive_store_failed", "compute_overlap");
  }
  Result<ArrayRef> upper_array = store_dyadic(context, primitive_upper, primitive_index);
  if (!ok(upper_array)) {
    return fail<Ref<Overlaps>>(upper_array.code, "primitive_store_failed", "compute_overlap");
  }

  auto primitives = std::make_shared<GeometryPrimitiveTable>();
  primitives->source_index = std::make_shared<const Index>(
      model::make_native_index({source->source}, 1, "acsd.geometry.source"));
  primitives->source_rows = val(source_rows_array);
  primitives->leaf_orders = val(orders_array);
  primitives->leaf_ipix = val(ipix_array);
  primitives->kinds = val(kinds_array);
  primitives->lower = val(lower_array);
  primitives->upper = val(upper_array);
  primitives->precision_bits = 53;
  primitives->identity = model::make_identity("acsd.GeometryPrimitiveTable", 2, 0,
                                              model::sha256_hex(primitive_index), "acsd");
  primitives->assessment = model::make_assessment(Code::ok);
  primitives->assessment.complete = true;

  // --- the constraint matrix ----------------------------------------------
  // Rows:  a >= 0,  a - A <= 0,  A - Omega <= 0,  sum(a) + coarse - A = 0.
  std::vector<U64> offsets;
  std::vector<U64> indices;
  std::vector<double> values;
  std::vector<Dyadic> constraint_lower;
  std::vector<Dyadic> constraint_upper;
  const U64 primitive_count = primitive_source.size();
  const U64 leaf_count = contributions.size();
  auto add_row = [&](const std::vector<std::pair<U64, double>>& entries, const Dyadic& lower,
                     const Dyadic& upper) {
    offsets.push_back(static_cast<U64>(indices.size()));
    std::vector<std::pair<U64, double>> sorted = entries;
    std::sort(sorted.begin(), sorted.end());
    for (const auto& entry : sorted) {
      indices.push_back(entry.first);
      values.push_back(entry.second);
    }
    constraint_lower.push_back(lower);
    constraint_upper.push_back(upper);
  };
  const Dyadic drop_upper = drop.upper;
  const Dyadic native_upper = native.upper;
  for (U64 leaf = 0; leaf < leaf_count; ++leaf) {
    const U64 row = native_row + 2 + leaf;  // primitive row of this leaf
    add_row({{row, 1.0}}, Dyadic{"0", 0}, drop_upper);
    add_row({{row, 1.0}, {drop_row, -1.0}}, geometry::rational_to_dyadic(Rational(0, 1), false),
            geometry::rational_to_dyadic(Rational(0, 1), false));
  }
  add_row({{drop_row, 1.0}, {native_row, -1.0}},
          geometry::rational_to_dyadic(Rational(0, 1), false),
          geometry::rational_to_dyadic(Rational(0, 1), false));
  {
    std::vector<std::pair<U64, double>> entries;
    for (U64 leaf = 0; leaf < leaf_count; ++leaf) {
      entries.emplace_back(native_row + 2 + leaf, 1.0);
    }
    entries.emplace_back(coarse_row, 1.0);
    entries.emplace_back(drop_row, -1.0);
    add_row(entries, Dyadic{"0", 0}, Dyadic{"0", 0});
  }
  offsets.push_back(static_cast<U64>(indices.size()));

  Result<Ref<LinearAction>> matrix = operators::make_csr_action(
      context, static_cast<U64>(constraint_lower.size()), primitive_count, offsets, indices, values,
      primitive_index, "acsd.geometry.constraints", model::unit_sr());
  if (!ok(matrix)) {
    return fail<Ref<Overlaps>>(matrix.code, "constraint_matrix_failed", "compute_overlap");
  }
  Result<ArrayRef> constraint_lower_array = store_dyadic(context, constraint_lower, "acsd.geometry.constraints");
  if (!ok(constraint_lower_array)) {
    return fail<Ref<Overlaps>>(constraint_lower_array.code, "constraint_store_failed",
                               "compute_overlap");
  }
  Result<ArrayRef> constraint_upper_array = store_dyadic(context, constraint_upper, "acsd.geometry.constraints");
  if (!ok(constraint_upper_array)) {
    return fail<Ref<Overlaps>>(constraint_upper_array.code, "constraint_store_failed",
                               "compute_overlap");
  }
  auto constraints = std::make_shared<GeometryConstraints>();
  constraints->primitives = primitives;
  constraints->matrix = val(matrix);
  constraints->lower = val(constraint_lower_array);
  constraints->upper = val(constraint_upper_array);
  constraints->identity = model::make_identity("acsd.GeometryConstraints", 2, 0,
                                                model::sha256_hex("acsd.geometry.constraints"), "acsd");
  constraints->assessment = model::make_assessment(Code::ok);
  constraints->assessment.complete = true;

  // --- the overlap table ---------------------------------------------------
  const Id table_index = "acsd.geometry.overlap_table";
  Result<ArrayRef> table_source = store_u64(context, overlap_source, table_index);
  if (!ok(table_source)) {
    return fail<Ref<Overlaps>>(table_source.code, "table_store_failed", "compute_overlap");
  }
  Result<ArrayRef> table_orders = store_u32(context, overlap_orders, table_index);
  if (!ok(table_orders)) {
    return fail<Ref<Overlaps>>(table_orders.code, "table_store_failed", "compute_overlap");
  }
  Result<ArrayRef> table_ipix = store_u64(context, overlap_ipix, table_index);
  if (!ok(table_ipix)) {
    return fail<Ref<Overlaps>>(table_ipix.code, "table_store_failed", "compute_overlap");
  }
  Result<ArrayRef> table_native = store_u64(context, overlap_native, table_index);
  if (!ok(table_native)) {
    return fail<Ref<Overlaps>>(table_native.code, "table_store_failed", "compute_overlap");
  }
  Result<ArrayRef> table_drop = store_u64(context, overlap_drop, table_index);
  if (!ok(table_drop)) {
    return fail<Ref<Overlaps>>(table_drop.code, "table_store_failed", "compute_overlap");
  }
  Result<ArrayRef> table_overlap = store_u64(context, overlap_overlap, table_index);
  if (!ok(table_overlap)) {
    return fail<Ref<Overlaps>>(table_overlap.code, "table_store_failed", "compute_overlap");
  }
  auto table = std::make_shared<OverlapTable>();
  table->source_rows = val(table_source);
  table->leaf_orders = val(table_orders);
  table->leaf_ipix = val(table_ipix);
  table->native_primitive = val(table_native);
  table->drop_primitive = val(table_drop);
  table->overlap_primitive = val(table_overlap);
  table->identity = model::make_identity("acsd.OverlapTable", 2, 0, model::sha256_hex(table_index),
                                         "acsd");
  table->assessment = model::make_assessment(Code::ok);
  table->assessment.complete = true;

  // --- the recipe ----------------------------------------------------------
  auto footprint_set = std::make_shared<FootprintSet>();
  footprint_set->index = primitives->source_index;
  footprint_set->width = 1;
  footprint_set->height = 1;
  footprint_set->detector_from_sample = Mat2{1.0, 0.0, 0.0, 1.0};
  footprint_set->offset = Vec2{0.0, 0.0};
  footprint_set->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
  footprint_set->wcs = model;
  footprint_set->pixel_response = source->pixel_response;
  footprint_set->detector_frame = model->detector_frame;
  footprint_set->identity = model::make_identity("acsd.FootprintSet", 2, 0,
                                                 model::sha256_hex("acsd.geometry.footprint_set"),
                                                 "acsd");
  footprint_set->assessment = model::make_assessment(Code::ok);
  footprint_set->assessment.complete = true;

  Result<ArrayRef> recipe_rows = store_u64(context, {0ull}, "acsd.geometry.source_rows");
  if (!ok(recipe_rows)) {
    return fail<Ref<Overlaps>>(recipe_rows.code, "recipe_store_failed", "compute_overlap");
  }
  auto recipe = std::make_shared<GeometryRecipe>();
  recipe->footprints = footprint_set;
  recipe->source_index = primitives->source_index;
  recipe->source_rows = val(recipe_rows);
  recipe->order = order;
  recipe->pixfrac = pixfrac;
  recipe->initial_budget = request;
  recipe->arithmetic = request.arithmetic;
  recipe->algorithm = "affine_strip_cut_cell";
  recipe->version = 2;
  recipe->identity = model::make_identity("acsd.GeometryRecipe", 2, 0,
                                           model::sha256_hex("acsd.geometry.recipe"), "acsd");
  recipe->assessment = model::make_assessment(Code::ok);
  recipe->assessment.complete = true;

  // --- the result ----------------------------------------------------------
  auto overlaps = std::make_shared<Overlaps>();
  overlaps->contributions = std::move(contributions);
  overlaps->contribution_blocks = table;
  overlaps->coarse_candidates = kernel.unresolved;
  overlaps->recipe = recipe;
  overlaps->primitives = primitives;
  overlaps->constraints = constraints;
  overlaps->status.termination = kernel.termination;
  overlaps->status.bound_validity = Validity::certified;
  overlaps->status.support = kernel.unresolved.empty() ? SupportState::positive
                                                        : SupportState::bounded_undecided;
  overlaps->identity = model::make_identity("acsd.Overlaps", 2, 0, model::sha256_hex("acsd.overlaps"),
                                             "acsd");
  overlaps->assessment = model::make_assessment(Code::ok);
  overlaps->assessment.complete = kernel.termination == Termination::tolerance_met;
  overlaps->assessment.science = kernel.termination == Termination::tolerance_met
                                     ? ScienceState::bounded
                                     : ScienceState::unconverged;
  if (!kernel.reason.empty()) {
    model::add_issue(overlaps->assessment, Code::nonconverged, kernel.reason, "compute_overlap",
                     "the overlap bounds keep the unresolved mass instead of dropping it",
                     "refine with a larger budget");
  }
  model::set_included(overlaps->assessment, "affine_strip_cut_cell");
  model::set_included(overlaps->assessment, "exact_rational_clipping");
  model::set_included(overlaps->assessment, "shared_constraints");
  if (containment.complete) {
    // The proof strength of the containment fallback is recorded: the number
    // of boundary segments the adaptive refinement used, and how many target
    // cells were decided or kept undecided.
    model::set_included(
        overlaps->assessment,
        "target_domain_containment_fallback:boundary_segments=" +
            std::to_string(containment.boundary_segments) + ":cells_decided=" +
            std::to_string(containment.cells_decided) + ":cells_undecided=" +
            std::to_string(containment.cells_undecided));
    if (containment.cells_undecided > 0) {
      model::add_issue(overlaps->assessment, Code::support_unresolved,
                       "containment_cells_undecided", "compute_overlap:target_containment",
                       "the target cells that the containment proof could not decide keep their "
                       "whole cell mass as the error bound",
                       "raise the geometry budget for a finer boundary refinement");
    }
  }
  if (!containment.complete) {
    // The containment fallback could not prove the chart enclosure of the
    // source boundary: the boundary is named, never silent.
    model::add_issue(overlaps->assessment, Code::unsupported,
                     "containment_fallback_incomplete", "compute_overlap:target_containment",
                     "the equal-area containment proof of the target cells is unavailable, the "
                     "cut cell kernel bounds remain authoritative",
                     "refine the chart model or raise the geometry budget");
  }
  model::set_omitted(overlaps->assessment, "astrometric_parameter_error");

  return ok_result(std::static_pointer_cast<const Overlaps>(overlaps));
}

}  // namespace acsd
