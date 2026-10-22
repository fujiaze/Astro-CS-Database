// The performance profile of the geometry path.
//
// The driver reproduces the configuration that blocked the normalise
// workflow: a 128 by 128 pixel frame, the detector split into blocks of 16 by
// 16 pixels (64 source rows), the HEALPix order 6, pixfrac 1.0 and the work
// budget of two million units.
//
// For every source row the driver runs the exact overlap of the public API and
// records the wall clock of the whole call together with the stage counters
// of the two geometry algorithms (the cut cell kernel and the containment
// fallback).  The output is a JSON document; the driver never changes a
// scientific result, it only measures.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "geometry/profile.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/units.hpp"

namespace {

using Clock = std::chrono::steady_clock;

template <class T>
bool good(const acsd::Result<T>& result) {
  return result.code == acsd::Code::ok && result.value.has_value();
}
template <class T>
const T& value_of(const acsd::Result<T>& result) {
  return *result.value;
}

double seconds_since(Clock::time_point start) {
  return std::chrono::duration<double>(Clock::now() - start).count();
}

// The in-memory resources of the driver: the work ledger is real, everything
// else is a stub that the geometry path never touches.
class ProfileResources final : public acsd::Resources {
 public:
  explicit ProfileResources(const acsd::Limits& limits) : limits_(limits) {}
  acsd::Result<acsd::Reservation> reserve(acsd::U64 bytes, acsd::U32 threads) override {
    if (bytes > limits_.bytes || threads > limits_.threads) {
      acsd::Result<acsd::Reservation> out;
      out.code = acsd::Code::resource_exhausted;
      return out;
    }
    acsd::Result<acsd::Reservation> out;
    out.code = acsd::Code::ok;
    out.value = acsd::Reservation{};
    return out;
  }
  acsd::Result<acsd::U64> consume_work(acsd::U64 units) override {
    if (work_used_ + units > limits_.work) {
      acsd::Result<acsd::U64> out;
      out.code = acsd::Code::resource_exhausted;
      return out;
    }
    work_used_ += units;
    acsd::Result<acsd::U64> out;
    out.code = acsd::Code::ok;
    out.value = work_used_;
    return out;
  }
  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = work_used_;
    return usage;
  }
  acsd::U64 work_used() const { return work_used_; }

 private:
  acsd::Limits limits_;
  acsd::U64 work_used_ = 0;
};

// The in-memory storage of the driver: the geometry path publishes its
// primitive tables through it.
class ProfileStorage final : public acsd::Storage {
 public:
  acsd::Result<acsd::ArrayRef> copy(acsd::Context&, acsd::DType dtype,
                                     std::span<const std::byte> bytes,
                                     std::span<const acsd::U64> shape, const acsd::Unit& unit,
                                     acsd::Id index) override {
    acsd::Result<acsd::ArrayRef> out;
    auto owner = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<acsd::Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    array->block_ids.push_back(index);
    array->identity.object_id = index;
    array->identity.content_hash = index;
    array->identity.schema_id = "acsd.Array";
    array->identity.schema_major = 2;
    array->identity.schema_minor = 0;
    store_[index] = owner;
    out.code = acsd::Code::ok;
    out.value = array;
    return out;
  }
  acsd::Result<acsd::ArrayView> read(acsd::Context&, acsd::ArrayRef array,
                                      acsd::BlockRequest range) const override {
    acsd::Result<acsd::ArrayView> out;
    const auto it = store_.find(array ? array->index_id : acsd::Id());
    if (it == store_.end()) {
      out.code = acsd::Code::invalid_request;
      return out;
    }
    const std::size_t bytes_per_element =
        it->second->size() /
        std::max<std::size_t>(1, array->shape.empty() ? 1 : array->shape[0]);
    const std::size_t begin = static_cast<std::size_t>(range.begin) * bytes_per_element;
    const std::size_t count = static_cast<std::size_t>(range.count) * bytes_per_element;
    if (begin + count > it->second->size()) {
      out.code = acsd::Code::invalid_request;
      return out;
    }
    acsd::ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(it->second->data() + begin, count);
    auto owner = it->second;
    view.lease =
        acsd::Lease{std::shared_ptr<const void>(owner.get(), [owner](const void*) { (void)owner; })};
    out.code = acsd::Code::ok;
    out.value = view;
    return out;
  }
  acsd::Result<acsd::ArrayRef> open(acsd::Context&, std::string, const acsd::Array&,
                                     std::span<const acsd::BlockDescriptor>) override {
    acsd::Result<acsd::ArrayRef> out;
    out.code = acsd::Code::unsupported;
    return out;
  }

 private:
  mutable std::map<acsd::Id, std::shared_ptr<std::vector<std::byte>>> store_;
};

class NoCancellation final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// The provider of the driver: the geometry path asks the provider for the
// model, which the exact WCS provides; the region enclosures are never used
// by the overlap path.
class ExactProvider final : public acsd::FootprintProvider {
 public:
  explicit ExactProvider(acsd::Ref<acsd::WcsModel> wcs) : wcs_(std::move(wcs)) {}
  acsd::Ref<acsd::WcsModel> model() const override { return wcs_; }
  acsd::Result<acsd::EnclosedSkyRegion> forward_interval(acsd::Context&, acsd::Box) const override {
    acsd::Result<acsd::EnclosedSkyRegion> out;
    out.code = acsd::Code::unsupported;
    return out;
  }
  acsd::Result<acsd::DerivativeResult> derivative_interval(acsd::Context&, acsd::Box) const override {
    acsd::Result<acsd::DerivativeResult> out;
    out.code = acsd::Code::unsupported;
    return out;
  }
  acsd::Result<acsd::EnclosedSkyRegion> boundary_interval(acsd::Context&, const acsd::CurvedEdge&,
                                                          acsd::Interval) const override {
    acsd::Result<acsd::EnclosedSkyRegion> out;
    out.code = acsd::Code::unsupported;
    return out;
  }
  acsd::Result<acsd::SphericalAreaInterval> measure(acsd::Context&, const acsd::DetectorFootprint&,
                                                     const acsd::GeometryBudget&) const override {
    acsd::Result<acsd::SphericalAreaInterval> out;
    out.code = acsd::Code::unsupported;
    return out;
  }

 private:
  acsd::Ref<acsd::WcsModel> wcs_;
};

acsd::HeaderFields frame_header() {
  acsd::HeaderFields header;
  header.push_back({"CTYPE1", std::string("RA---TAN")});
  header.push_back({"CTYPE2", std::string("DEC--TAN")});
  header.push_back({"CRPIX1", 64.0});
  header.push_back({"CRPIX2", 64.0});
  header.push_back({"CRVAL1", 83.82208});
  header.push_back({"CRVAL2", -5.39111});
  header.push_back({"CDELT1", -0.0002777778});
  header.push_back({"CDELT2", 0.0002777778});
  header.push_back({"RADESYS", std::string("ICRS")});
  return header;
}

std::string json_timings(const acsd::geometry::StageTimings& timings) {
  std::string out = "{";
  out += "\"chart_model_ms\": " + std::to_string(timings.chart_model_ns / 1000000);
  out += ", \"candidate_enumeration_ms\": " + std::to_string(timings.candidate_enumeration_ns / 1000000);
  out += ", \"rational_clip_ms\": " + std::to_string(timings.rational_clip_ns / 1000000);
  out += ", \"boundary_segmentation_ms\": " + std::to_string(timings.boundary_segmentation_ns / 1000000);
  out += ", \"separation_test_ms\": " + std::to_string(timings.separation_test_ns / 1000000);
  out += ", \"membership_test_ms\": " + std::to_string(timings.membership_test_ns / 1000000);
  out += ", \"work_metering_ms\": " + std::to_string(timings.work_metering_ns / 1000000);
  out += ", \"dyadic_merge_ms\": " + std::to_string(timings.dyadic_merge_ns / 1000000);
  out += "}";
  return out;
}

std::string json_counts(const acsd::geometry::StageCounts& counts) {
  std::string out = "{";
  out += "\"chart_models\": " + std::to_string(counts.chart_models);
  out += ", \"candidate_cells\": " + std::to_string(counts.candidate_cells);
  out += ", \"rational_clips\": " + std::to_string(counts.rational_clips);
  out += ", \"blocks\": " + std::to_string(counts.blocks);
  out += ", \"boundary_segments\": " + std::to_string(counts.boundary_segments);
  out += ", \"cells_decided\": " + std::to_string(counts.cells_decided);
  out += ", \"cells_undecided\": " + std::to_string(counts.cells_undecided);
  out += ", \"inverse_calls\": " + std::to_string(counts.inverse_calls);
  out += ", \"work_charges\": " + std::to_string(counts.work_charges);
  out += ", \"work_units\": " + std::to_string(counts.work_units);
  out += ", \"leaves\": " + std::to_string(counts.leaves);
  out += "}";
  return out;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string directory = argc > 1 ? argv[1] : ".";
  const acsd::U64 order = argc > 2 ? static_cast<acsd::U64>(std::atoll(argv[2])) : 6;
  const acsd::U64 block_size = argc > 3 ? static_cast<acsd::U64>(std::atoll(argv[3])) : 16;
  const acsd::U64 width = argc > 4 ? static_cast<acsd::U64>(std::atoll(argv[4])) : 128;
  const acsd::U64 height = argc > 5 ? static_cast<acsd::U64>(std::atoll(argv[5])) : 128;
  const acsd::U64 max_work = argc > 6 ? static_cast<acsd::U64>(std::atoll(argv[6])) : 2000000;
  const acsd::U64 row_limit = argc > 7 ? static_cast<acsd::U64>(std::atoll(argv[7])) : 0;

  acsd::Limits limits;
  limits.bytes = 1ull << 30;
  limits.work = max_work;
  limits.threads = 1;
  ProfileResources resources(limits);
  NoCancellation cancellation;
  ProfileStorage storage;
  acsd::Context context{resources, cancellation, storage};

  const acsd::Box domain{{0.0, 0.0}, {static_cast<double>(width), static_cast<double>(height)}};
  acsd::AstrometricContext astrometric;
  acsd::NumericBudget budget;
  auto model = acsd::make_wcs(context, frame_header(), domain, astrometric, "detector", budget);
  if (!good(model)) {
    std::fprintf(stderr, "the frame model failed\n");
    return 1;
  }
  const acsd::Ref<acsd::WcsModel>& wcs = value_of(model);
  ExactProvider provider(wcs);

  // The index of the source rows: one row per block of the detector.
  const acsd::U64 blocks_per_row = (width + block_size - 1) / block_size;
  const acsd::U64 block_count = blocks_per_row * ((height + block_size - 1) / block_size);
  auto index = std::make_shared<acsd::Index>();
  index->size = block_count;
  index->kind = acsd::Index::Kind::native;
  index->namespace_id = "acsd.validation.profile";

  acsd::GeometryBudget geometry_budget;
  geometry_budget.max_work = max_work;
  geometry_budget.max_depth = max_depth;
  geometry_budget.area_absolute_sr = 1e-12;
  geometry_budget.arithmetic.initial_bits = 64;
  geometry_budget.arithmetic.max_bits = 256;

  acsd::geometry::reset_stage_counts();
  acsd::geometry::set_stage_timing(true);
  std::map<std::pair<acsd::U32, acsd::U64>, double> leaf_areas;
  std::vector<double> row_times;
  acsd::U64 completed = 0;
  acsd::U64 failed = 0;
  double total_seconds = 0.0;
  double slowest = 0.0;
  std::string first_failure;
  const Clock::time_point start = Clock::now();

  const acsd::U64 rows = row_limit > 0 && row_limit < block_count ? row_limit : block_count;
  for (acsd::U64 block = 0; block < rows; ++block) {
    const acsd::U64 first_x = (block % blocks_per_row) * block_size;
    const acsd::U64 first_y = (block / blocks_per_row) * block_size;
    const acsd::U64 last_x = std::min(first_x + block_size, width);
    const acsd::U64 last_y = std::min(first_y + block_size, height);

    acsd::DetectorFootprint footprint;
    footprint.wcs = wcs;
    footprint.source = acsd::model::source_at(*index, block);
    footprint.source.sample = block;
    footprint.identity = acsd::model::make_identity("acsd.DetectorFootprint", 2, 0,
                                                    acsd::model::sha256_hex("acsd.profile.block"),
                                                    "acsd");
    footprint.assessment = acsd::model::make_assessment(acsd::Code::ok);
    footprint.assessment.complete = true;
    const double x0 = static_cast<double>(first_x) - 0.5;
    const double y0 = static_cast<double>(first_y) - 0.5;
    const double x1 = static_cast<double>(last_x) - 0.5;
    const double y1 = static_cast<double>(last_y) - 0.5;
    footprint.contraction_center = acsd::PixelCenter0{0.5 * (x0 + x1), 0.5 * (y0 + y1)};
    acsd::Ring ring;
    ring.hole = false;
    const double corners[4][2] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    for (const auto& corner : corners) {
      acsd::PixelEdge0 edge;
      edge.x = corner[0];
      edge.y = corner[1];
      ring.vertices.push_back(edge);
    }
    footprint.detector_rings.push_back(ring);
    const acsd::Ref<acsd::DetectorFootprint> footprint_ref =
        std::make_shared<acsd::DetectorFootprint>(footprint);

    const Clock::time_point row_start = Clock::now();
    auto overlaps = acsd::compute_overlap(context, provider, footprint_ref,
                                          static_cast<acsd::U32>(order), 1.0, geometry_budget);
    const double row_seconds = seconds_since(row_start);
    row_times.push_back(row_seconds);
    total_seconds += row_seconds;
    slowest = std::max(slowest, row_seconds);
    if (!good(overlaps)) {
      ++failed;
      if (first_failure.empty()) {
        first_failure = std::to_string(static_cast<int>(overlaps.code));
        for (const acsd::Issue& issue : overlaps.assessment.issues) {
          first_failure += " | " + issue.reason + " @ " + issue.origin;
        }
      }
      continue;
    }
    ++completed;
    for (const acsd::OverlapContribution& contribution : value_of(overlaps)->contributions) {
      leaf_areas[{contribution.leaf.order, contribution.leaf.ipix}] +=
          contribution.overlap_area.lower_sr;
    }
  }
  const double wall_seconds = seconds_since(start);

  double total_area = 0.0;
  for (const auto& entry : leaf_areas) total_area += entry.second;

  std::vector<double> sorted = row_times;
  std::sort(sorted.begin(), sorted.end());
  const double median = sorted.empty() ? 0.0 : sorted[sorted.size() / 2];

  std::ofstream out(directory + "/geo_performance.json");
  out << "{\n";
  out << "  \"configuration\": {\"width\": " << width << ", \"height\": " << height
      << ", \"block_size\": " << block_size << ", \"source_rows\": " << rows
      << ", \"order\": " << order << ", \"pixfrac\": 1.0, \"max_work\": " << max_work << "},\n";
  out << "  \"wall_seconds\": " << wall_seconds << ",\n";
  out << "  \"sum_row_seconds\": " << total_seconds << ",\n";
  out << "  \"mean_row_seconds\": " << (rows > 0 ? total_seconds / static_cast<double>(rows) : 0.0)
      << ",\n";
  out << "  \"median_row_seconds\": " << median << ",\n";
  out << "  \"slowest_row_seconds\": " << slowest << ",\n";
  out << "  \"rows_completed\": " << completed << ",\n";
  out << "  \"rows_failed\": " << failed << ",\n";
  out << "  \"first_failure\": \"" << first_failure << "\",\n";
  out << "  \"leaves\": " << leaf_areas.size() << ",\n";
  out << "  \"total_area_sr\": " << total_area << ",\n";
  out << "  \"work_used\": " << resources.work_used() << ",\n";
  out << "  \"kernel_counts\": " << json_counts(acsd::geometry::kernel_counts()) << ",\n";
  out << "  \"containment_counts\": " << json_counts(acsd::geometry::containment_counts()) << ",\n";
  out << "  \"kernel_timings\": " << json_timings(acsd::geometry::kernel_timings()) << ",\n";
  out << "  \"containment_timings\": " << json_timings(acsd::geometry::containment_timings()) << "\n";
  out << "}\n";
  std::printf("profile: %llu rows in %.2f s (mean %.3f s, median %.3f s, slowest %.3f s)\n",
              (unsigned long long)rows, wall_seconds,
              rows > 0 ? total_seconds / static_cast<double>(rows) : 0.0, median, slowest);
  return 0;
}
