// G01 native in-memory API driver: the real spherical area and the overlap
// candidates.
//
// Independence rules honoured by this file:
//   * it includes ONLY the public declaration contract <acsd/contracts.hpp>;
//   * it does not include any production internal header and does not
//     re-implement any science algorithm;
//   * it reads the frozen *input* fixtures and moves fields into the public
//     API; the truth values live in a separate file that this driver never
//     opens.
//
// The driver also exercises the named negative configurations: a starved work
// budget and a capped precision must not be reported as a satisfied tolerance.

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

using acsd::Array;
using acsd::ArrayRef;
using acsd::ArrayView;
using acsd::BlockRequest;
using acsd::Code;
using acsd::Context;
using acsd::DType;
using acsd::Id;
using acsd::Identity;
using acsd::Index;
using acsd::LinearAction;
using acsd::Ref;
using acsd::Result;
using acsd::Unit;
using acsd::U32;
using acsd::U64;
using json = nlohmann::json;

template <class T>
bool ok(const Result<T>& r) {
  return r.code == Code::ok && r.value.has_value();
}

template <class T>
const T& val(const Result<T>& r) {
  return *r.value;
}

template <class T>
Result<T> failure(Code code) {
  Result<T> r;
  r.code = code;
  return r;
}

template <class T>
std::string reason_of(const Result<T>& r) {
  if (!r.assessment.issues.empty()) return r.assessment.issues.front().reason;
  return "";
}

const char* code_name(int code) {
  static const char* names[] = {
      "ok", "invalid_request", "invalid_model", "domain", "unsupported", "unavailable",
      "unsolved", "not_identifiable", "invalid_covariance", "duplicate_source",
      "identity_conflict", "hash_mismatch", "corrupt_product", "missing_dependency",
      "no_support", "support_unresolved", "geometry_solve_unresolved", "selection_unresolved",
      "nonconverged", "precision_exhausted", "resource_exhausted", "io_error", "cancelled",
      "internal"};
  if (code < 0 || code >= static_cast<int>(sizeof(names) / sizeof(names[0]))) return "unknown";
  return names[code];
}

const char* termination_name(int value) {
  static const char* names[] = {"tolerance_met", "work", "depth", "precision", "unsupported"};
  if (value < 0 || value >= 5) return "unknown";
  return names[value];
}

const char* validity_name(int value) {
  static const char* names[] = {"certified", "approximate", "invalid"};
  if (value < 0 || value >= 3) return "unknown";
  return names[value];
}

const char* support_name(int value) {
  static const char* names[] = {"absent", "positive", "bounded_undecided"};
  if (value < 0 || value >= 3) return "unknown";
  return names[value];
}

// ---------------------------------------------------------------------------
// public service interfaces implemented by this independent caller
// ---------------------------------------------------------------------------

class ValidationStorage final : public acsd::Storage {
 public:
  struct Entry {
    std::shared_ptr<std::vector<std::byte>> bytes;
    std::vector<U64> shape;
    Unit unit;
  };

  Result<ArrayRef> copy(acsd::Context& context, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    (void)context;
    U64 count = 1;
    for (U64 extent : shape) count *= extent;
    const std::size_t item = item_size(dtype);
    // A dyadic array carries a variable length encoding, so it has no fixed
    // element size; this mirrors the production storage rule.
    if (dtype != DType::dyadic && bytes.size() != static_cast<std::size_t>(count) * item) {
      return failure<ArrayRef>(Code::invalid_request);
    }
    auto buffer = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    auto array = std::make_shared<Array>();
    array->dtype = dtype;
    array->shape.assign(shape.begin(), shape.end());
    array->unit = unit;
    array->index_id = index;
    Identity identity;
    identity.object_id = index;
    identity.content_hash = "sha256:" + hash_hex(std::string(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
    identity.run_id = "validation-g01";
    identity.revision = 0;
    identity.schema_id = "acsd.Array";
    identity.schema_major = 2;
    identity.schema_minor = 0;
    array->identity = identity;
    Entry entry;
    entry.bytes = buffer;
    entry.shape = array->shape;
    entry.unit = unit;
    entries_[identity.content_hash] = entry;
    Result<ArrayRef> out;
    out.code = Code::ok;
    out.value = ArrayRef(array);
    return out;
  }

  Result<ArrayView> read(acsd::Context& context, ArrayRef array, BlockRequest range) const override {
    (void)context;
    if (!array) return failure<ArrayView>(Code::invalid_request);
    auto it = entries_.find(array->identity.content_hash);
    if (it == entries_.end()) return failure<ArrayView>(Code::invalid_request);
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    const std::size_t item = item_size(array->dtype);
    if (array->dtype == DType::dyadic) {
      // a variable length encoding has no element offset
      ArrayView view;
      view.dtype = array->dtype;
      view.shape = array->shape;
      view.bytes = std::span<const std::byte>(it->second.bytes->data(),
                                              it->second.bytes->size());
      view.lease.owner = it->second.bytes;
      Result<ArrayView> out;
      out.code = Code::ok;
      out.value = view;
      return out;
    }
    if (range.begin + range.count > total) return failure<ArrayView>(Code::invalid_request);
    ArrayView view;
    view.dtype = array->dtype;
    view.shape = array->shape;
    view.bytes = std::span<const std::byte>(
        it->second.bytes->data() + static_cast<std::size_t>(range.begin) * item,
        static_cast<std::size_t>(range.count) * item);
    view.lease.owner = it->second.bytes;
    Result<ArrayView> out;
    out.code = Code::ok;
    out.value = view;
    return out;
  }

  Result<ArrayRef> open(acsd::Context& context, std::string root, const Array& description,
                         std::span<const acsd::BlockDescriptor> blocks) override {
    (void)context; (void)root; (void)description; (void)blocks;
    return failure<ArrayRef>(Code::unsupported);
  }

 private:
  static std::size_t item_size(DType dtype) {
    switch (dtype) {
      case DType::f64: return 8;
      case DType::f32: return 4;
      case DType::u64: return 8;
      case DType::u32: return 4;
      case DType::u16: return 2;
      case DType::u8: return 1;
      default: return 0;
    }
  }

  static std::string hash_hex(const std::string& payload) {
    std::size_t hash = 1469598103934665603ull;
    for (unsigned char c : payload) {
      hash ^= c;
      hash *= 1099511628211ull;
    }
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
  }

  std::map<std::string, Entry> entries_;
};

class ValidationResources final : public acsd::Resources {
 public:
  acsd::Result<acsd::Reservation> reserve(U64 bytes, U32 threads) override {
    if (bytes > budget_bytes_ || threads > threads_) {
      acsd::Result<acsd::Reservation> out;
      out.code = Code::resource_exhausted;
      return out;
    }
    acsd::Reservation reservation;
    reservation.lease.owner = std::make_shared<std::vector<std::byte>>(1);
    reservation.bytes = bytes;
    reservation.threads = threads;
    acsd::Result<acsd::Reservation> out;
    out.code = Code::ok;
    out.value = reservation;
    return out;
  }

  acsd::Result<U64> consume_work(U64 units) override {
    work_ += units;
    acsd::Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    return out;
  }

  acsd::Usage usage() const override {
    acsd::Usage usage;
    usage.work = work_;
    return usage;
  }

 private:
  U64 budget_bytes_ = 1ull << 30;
  U32 threads_ = 8;
  U64 work_ = 0;
};

class NeverCancelled final : public acsd::Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

// ---------------------------------------------------------------------------

acsd::HeaderFields read_header(const json& header) {
  acsd::HeaderFields fields;
  for (const auto& item : header) {
    acsd::HeaderField field;
    field.key = item.at("key").get<std::string>();
    const json& value = item.at("value");
    if (value.is_string()) field.value = value.get<std::string>();
    else if (value.is_number_integer()) field.value = value.get<std::int64_t>();
    else field.value = value.get<double>();
    fields.push_back(field);
  }
  return fields;
}

acsd::Box read_box(const json& box) {
  acsd::Box out;
  out.lower = {box.at("lower").at(0).get<double>(), box.at("lower").at(1).get<double>()};
  out.upper = {box.at("upper").at(0).get<double>(), box.at("upper").at(1).get<double>()};
  return out;
}

acsd::AstrometricContext read_context(const json& node) {
  acsd::AstrometricContext context;
  context.epoch_julian_year = node.value("epoch_julian_year", 2016.0);
  context.observation_jd_tdb = node.value("observation_jd_tdb", 2457600.5);
  return context;
}

acsd::GeometryBudget read_budget(const json& node, const std::string& functional_id) {
  acsd::GeometryBudget budget;
  acsd::QuantityRequest functional;
  functional.id = functional_id;
  functional.kind = acsd::QuantityRequest::Kind::area;
  functional.tolerance.absolute = node.value("area_absolute_sr", 1e-12);
  functional.tolerance.relative = 0.0;
  functional.tolerance.absolute_floor = 0.0;
  functional.tolerance.unit.scale = "sr";
  functional.tolerance.unit.sr_power = 1;
  functional.tolerance.unit.pixel_power = 0;
  functional.tolerance.unit.nm_power = 0;
  functional.tolerance.unit.multiplier = 1.0;
  functional.tolerance.unit.linear_power = 0;
  functional.condition.estimator = "fixture-g01";
  functional.condition.kind = "fixed_geometry";
  budget.requested_functionals.push_back(functional);
  budget.area_absolute_sr = node.value("area_absolute_sr", 1e-12);
  budget.normalizer_min_sr = node.value("normalizer_min_sr", 0.0);
  budget.max_work = node.value("max_work", 1000000ull);
  budget.max_bytes = node.value("max_bytes", 16777216ull);
  budget.max_depth = node.value("max_depth", 32u);
  budget.arithmetic.mode = node.value("outward", true) ? acsd::Arithmetic::Mode::outward
                                                        : acsd::Arithmetic::Mode::approximate;
  budget.arithmetic.initial_bits = node.value("initial_bits", 64u);
  budget.arithmetic.max_bits = node.value("max_bits", 256u);
  budget.arithmetic.allow_target_fallback = node.value("allow_target_fallback", false);
  return budget;
}

// A minimal RefinableGeometryProvider so that the public refine_geometry
// transaction can be exercised; the provider itself carries no science.
class SnapshotProvider final : public acsd::RefinableGeometryProvider {
 public:
  explicit SnapshotProvider(std::shared_ptr<const acsd::GeometrySnapshot> snapshot)
      : snapshot_(std::move(snapshot)) {}

  Result<Ref<acsd::GeometrySnapshot>> open(acsd::Context& context, Id snapshot) const override {
    (void)context; (void)snapshot;
    Result<Ref<acsd::GeometrySnapshot>> out;
    out.code = Code::ok;
    out.value = Ref<acsd::GeometrySnapshot>(snapshot_);
    return out;
  }

  Result<Ref<acsd::GeometryRevision>> refine(acsd::Context& context,
                                             Ref<acsd::GeometrySnapshot> snapshot,
                                             const acsd::GeometryBudget& request) const override {
    (void)context; (void)request;
    auto revision = std::make_shared<acsd::GeometryRevision>();
    revision->base = snapshot;
    revision->next = snapshot;
    revision->status.termination = acsd::Termination::tolerance_met;
    revision->status.bound_validity = acsd::Validity::certified;
    revision->status.support = acsd::SupportState::positive;
    Result<Ref<acsd::GeometryRevision>> out;
    out.code = Code::ok;
    out.value = Ref<acsd::GeometryRevision>(revision);
    return out;
  }

 private:
  std::shared_ptr<const acsd::GeometrySnapshot> snapshot_;
};

}  // namespace

int main(int argc, char** argv) {
  std::string fixture_path;
  std::string output_path;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--fixtures" && i + 1 < argc) fixture_path = argv[++i];
    if (arg == "--output" && i + 1 < argc) output_path = argv[++i];
  }
  if (fixture_path.empty()) {
    std::cerr << "usage: g01_driver --fixtures <file> [--output <file>]\n";
    return 2;
  }

  json fixtures;
  {
    std::ifstream input(fixture_path);
    if (!input) {
      std::cerr << "cannot read " << fixture_path << "\n";
      return 2;
    }
    input >> fixtures;
  }

  ValidationStorage storage;
  ValidationResources resources;
  NeverCancelled cancellation;
  acsd::Context context{resources, cancellation, storage};

  json report;
  report["driver"] = "g01_native";
  report["route"] = "in_memory_public_api";
  report["cases"] = json::array();

  for (const auto& fixture : fixtures.at("cases")) {
    json entry;
    entry["name"] = fixture.at("name");

    // ---- the WCS of the fixture -----------------------------------------
    acsd::HeaderFields header;
    header.push_back({"CTYPE1", std::string("RA---TAN")});
    header.push_back({"CTYPE2", std::string("DEC--TAN")});
    header.push_back({"CRPIX1", fixture.at("crpix").at(0).get<double>()});
    header.push_back({"CRPIX2", fixture.at("crpix").at(1).get<double>()});
    header.push_back({"CRVAL1", fixture.at("crval").at(0).get<double>()});
    header.push_back({"CRVAL2", fixture.at("crval").at(1).get<double>()});
    const json& cd = fixture.at("cd");
    header.push_back({"CD1_1", cd.at(0).at(0).get<double>()});
    header.push_back({"CD1_2", cd.at(0).at(1).get<double>()});
    header.push_back({"CD2_1", cd.at(1).at(0).get<double>()});
    header.push_back({"CD2_2", cd.at(1).at(1).get<double>()});
    header.push_back({"RADESYS", std::string("ICRS")});

    const double width = fixture.at("corners").at(1).at(0).get<double>()
                       - fixture.at("corners").at(0).at(0).get<double>();
    const double height = fixture.at("corners").at(1).at(1).get<double>()
                        - fixture.at("corners").at(0).at(1).get<double>();
    acsd::Box domain;
    domain.lower = {fixture.at("corners").at(0).at(0).get<double>(),
                    fixture.at("corners").at(0).at(1).get<double>()};
    domain.upper = {fixture.at("corners").at(1).at(0).get<double>(),
                    fixture.at("corners").at(1).at(1).get<double>()};

    acsd::NumericBudget numeric;
    numeric.tolerance.absolute = 1e-12;
    numeric.tolerance.relative = 0.0;
    numeric.tolerance.absolute_floor = 0.0;
    numeric.tolerance.unit.scale = "rad";
    numeric.tolerance.unit.sr_power = 0;
    numeric.tolerance.unit.pixel_power = 0;
    numeric.tolerance.unit.nm_power = 0;
    numeric.tolerance.unit.multiplier = 1.0;
    numeric.tolerance.unit.linear_power = 0;
    numeric.max_iterations = 128;
    numeric.arithmetic.mode = acsd::Arithmetic::Mode::outward;
    numeric.arithmetic.initial_bits = 128;
    numeric.arithmetic.max_bits = 256;
    numeric.arithmetic.allow_target_fallback = false;

    Result<Ref<acsd::WcsModel>> wcs =
        acsd::make_wcs(context, header, domain, read_context(fixture.at("astrometric_context")),
                       fixture.value("detector_frame", "detector"), numeric);
    if (!ok(wcs)) {
      entry["status"] = "wcs_failed";
      entry["code"] = code_name(static_cast<int>(wcs.code));
      entry["reason"] = reason_of(wcs);
      report["cases"].push_back(entry);
      continue;
    }

    Result<Ref<acsd::FootprintProvider>> provider =
        acsd::make_footprint_provider(context, val(wcs));
    if (!ok(provider)) {
      entry["status"] = "provider_failed";
      entry["code"] = code_name(static_cast<int>(provider.code));
      entry["reason"] = reason_of(provider);
      report["cases"].push_back(entry);
      continue;
    }

    // ---- the detector footprint of the fixture cell ----------------------
    auto source = std::make_shared<acsd::DetectorFootprint>();
    source->wcs = val(wcs);
    source->source.dataset = "fixture";
    source->source.exposure = "exposure1";
    source->source.detector = "det1";
    source->source.channel = "L";
    source->source.sample = 0;
    source->contraction_center = {0.5 * (domain.lower[0] + domain.upper[0]),
                                  0.5 * (domain.lower[1] + domain.upper[1])};
    acsd::Ring ring;
    const double x0 = domain.lower[0], y0 = domain.lower[1];
    const double x1 = domain.upper[0], y1 = domain.upper[1];
    ring.vertices = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    ring.hole = false;
    source->detector_rings.push_back(ring);

    const U32 order = fixture.value("order", 5u);
    const double pixfrac = fixture.value("pixfrac", 1.0);

    // ---- the drop area of the cell ---------------------------------------
    {
      acsd::GeometryBudget budget = read_budget(fixture.at("budget"), "drop_area");
      Result<acsd::SphericalAreaInterval> area =
          acsd::measure(context, *val(provider), Ref<acsd::DetectorFootprint>(source), budget);
      json item;
      item["code"] = code_name(static_cast<int>(area.code));
      if (ok(area)) {
        item["lower_sr"] = val(area).lower_sr;
        item["upper_sr"] = val(area).upper_sr;
      } else {
        item["reason"] = reason_of(area);
      }
      entry["measure"] = item;
    }

    // ---- the overlap candidates -------------------------------------------
    {
      acsd::GeometryBudget budget = read_budget(fixture.at("budget"), "overlap_area");
      const U64 work_before = resources.usage().work;
      Result<Ref<acsd::Overlaps>> overlaps = acsd::compute_overlap(
          context, *val(provider), Ref<acsd::DetectorFootprint>(source), order, pixfrac, budget);
      const U64 work_used = resources.usage().work - work_before;
      json item;
      item["code"] = code_name(static_cast<int>(overlaps.code));
      item["work_units"] = work_used;
      item["budget_max_work"] = budget.max_work;
      item["issues"] = json::array();
      for (const auto& issue : overlaps.assessment.issues) {
        item["issues"].push_back({ {"code", code_name(static_cast<int>(issue.code))},
                                   {"origin", issue.origin}, {"reason", issue.reason},
                                   {"impact", issue.impact} });
      }
      item["execution"] = static_cast<int>(overlaps.assessment.execution);
      if (ok(overlaps)) {
        item["termination"] = termination_name(static_cast<int>(val(overlaps)->status.termination));
        item["bound_validity"] = validity_name(static_cast<int>(val(overlaps)->status.bound_validity));
        item["support"] = support_name(static_cast<int>(val(overlaps)->status.support));
        item["unresolved"] = val(overlaps)->status.unresolved.size();
        item["coarse_candidates"] = val(overlaps)->coarse_candidates.size();
        json contributions = json::array();
        for (const auto& contribution : val(overlaps)->contributions) {
          contributions.push_back({
              {"order", contribution.leaf.order},
              {"ipix", contribution.leaf.ipix},
              {"native_area_lower", contribution.native_area.lower_sr},
              {"native_area_upper", contribution.native_area.upper_sr},
              {"drop_area_lower", contribution.drop_area.lower_sr},
              {"drop_area_upper", contribution.drop_area.upper_sr},
              {"overlap_area_lower", contribution.overlap_area.lower_sr},
              {"overlap_area_upper", contribution.overlap_area.upper_sr},
          });
        }
        item["contributions"] = contributions;
        entry["overlaps"] = item;

        // ---- the drizzle operator ---------------------------------------
        auto native = std::make_shared<acsd::NativeObservation>();
        auto index = std::make_shared<Index>();
        index->size = 1;
        index->kind = Index::Kind::native;
        index->namespace_id = "fixture:g01";
        index->identity.object_id = "fixture:g01";
        index->identity.content_hash = "sha256:" + std::string(64, '0');
        index->identity.run_id = "validation-g01";
        index->identity.schema_id = "acsd.Index";
        index->identity.schema_major = 2;
        index->identity.schema_minor = 0;
        native->native_index = Ref<Index>(index);
        native->frame = "exposure1";
        native->unit.scale = "fixture:L";
        native->unit.linear_power = 1;
        native->unit.sr_power = 0;
        native->unit.pixel_power = 0;
        native->unit.nm_power = 0;
        native->unit.multiplier = 1.0;
        std::vector<double> value{1.0};
        std::vector<std::byte> bytes(value.size() * sizeof(double));
        std::memcpy(bytes.data(), value.data(), bytes.size());
        const std::vector<U64> shape{1};
        Result<ArrayRef> array = context.storage.copy(context, DType::f64, bytes, shape,
                                                      native->unit, "fixture:g01:value");
        if (ok(array)) native->integrated_values = val(array);

        acsd::DrizzleRequest drizzle;
        drizzle.order = order;
        drizzle.pixfrac = pixfrac;
        drizzle.budget = read_budget(fixture.at("budget"), "drizzle");
        Result<acsd::DrizzleResult> built =
            acsd::build_drizzle(context, val(overlaps), Ref<acsd::NativeObservation>(native),
                                 drizzle);
        json drizzle_item;
        drizzle_item["code"] = code_name(static_cast<int>(built.code));
        if (ok(built)) {
          drizzle_item["has_operator"] = static_cast<bool>(val(built).op);
          if (val(built).op) {
            drizzle_item["termination"] =
                termination_name(static_cast<int>(val(built).op->status.termination));
            drizzle_item["bound_validity"] =
                validity_name(static_cast<int>(val(built).op->status.bound_validity));
            drizzle_item["support"] =
                support_name(static_cast<int>(val(built).op->status.support));
          }
          drizzle_item["has_estimate"] = static_cast<bool>(val(built).estimate);
        } else {
          drizzle_item["reason"] = reason_of(built);
        }
        entry["drizzle"] = drizzle_item;

        // ---- the refinement transaction ----------------------------------
        auto snapshot = std::make_shared<acsd::GeometrySnapshot>();
        snapshot->overlaps.push_back(val(overlaps));
        if (val(built).op) snapshot->operators.push_back(val(built).op);
        Ref<acsd::RefinableGeometryProvider> snapshot_provider =
            std::make_shared<SnapshotProvider>(Ref<acsd::GeometrySnapshot>(snapshot));
        acsd::GeometryBudget refine_budget = read_budget(fixture.at("budget"), "refine");
        Result<Ref<acsd::GeometryRevision>> revision = acsd::refine_geometry(
            context, *snapshot_provider, Ref<acsd::GeometrySnapshot>(snapshot), refine_budget);
        json refine_item;
        refine_item["code"] = code_name(static_cast<int>(revision.code));
        if (ok(revision)) {
          refine_item["termination"] =
              termination_name(static_cast<int>(val(revision)->status.termination));
          refine_item["bound_validity"] =
              validity_name(static_cast<int>(val(revision)->status.bound_validity));
          refine_item["support"] = support_name(static_cast<int>(val(revision)->status.support));
          refine_item["affected_sources"] = val(revision)->affected_sources.size();
          refine_item["affected_leaves"] = val(revision)->affected_leaves.size();
          refine_item["replacements"] = val(revision)->replacements.size();
        } else {
          refine_item["reason"] = reason_of(revision);
        }
        entry["refine"] = refine_item;
      } else {
        entry["overlaps"] = item;
      }
    }

    // ---- the starved budget negative configurations -----------------------
    {
      json negatives = json::array();
      for (const auto& variant : fixture.at("budget").value("negative_variants", json::array())) {
        acsd::GeometryBudget budget = read_budget(fixture.at("budget"), "negative");
        budget.max_work = variant.value("max_work", budget.max_work);
        budget.arithmetic.max_bits = variant.value("max_bits", budget.arithmetic.max_bits);
        budget.arithmetic.initial_bits = variant.value("initial_bits",
                                                       budget.arithmetic.initial_bits);
        const U64 work_before = resources.usage().work;
        Result<Ref<acsd::Overlaps>> overlaps = acsd::compute_overlap(
            context, *val(provider), Ref<acsd::DetectorFootprint>(source), order, pixfrac, budget);
        const U64 work_used = resources.usage().work - work_before;
        json item;
        item["name"] = variant.value("name", "negative");
        item["code"] = code_name(static_cast<int>(overlaps.code));
        item["work_units"] = work_used;
        item["budget_max_work"] = budget.max_work;
        item["budget_max_bits"] = budget.arithmetic.max_bits;
        item["issues"] = json::array();
        for (const auto& issue : overlaps.assessment.issues) {
          item["issues"].push_back({ {"code", code_name(static_cast<int>(issue.code))},
                                     {"origin", issue.origin}, {"reason", issue.reason},
                                     {"impact", issue.impact} });
        }
        item["fake_pass"] = false;
        if (ok(overlaps)) {
          item["termination"] = termination_name(static_cast<int>(val(overlaps)->status.termination));
          item["bound_validity"] = validity_name(static_cast<int>(val(overlaps)->status.bound_validity));
          item["support"] = support_name(static_cast<int>(val(overlaps)->status.support));
          // A fake pass is a certified, tolerance-met result that was produced
          // while the declared budget was exceeded.
          const bool budget_exceeded = work_used > budget.max_work;
          const bool precision_exceeded =
              budget.arithmetic.max_bits < 64 && val(overlaps)->status.termination ==
                                                  acsd::Termination::tolerance_met &&
              val(overlaps)->status.bound_validity == acsd::Validity::certified;
          item["budget_exceeded"] = budget_exceeded;
          item["fake_pass"] = (budget_exceeded || precision_exceeded) &&
                              val(overlaps)->status.termination == acsd::Termination::tolerance_met &&
                              val(overlaps)->status.bound_validity == acsd::Validity::certified;
        } else {
          item["reason"] = reason_of(overlaps);
        }
        negatives.push_back(item);
      }
      entry["negatives"] = negatives;
    }

    report["cases"].push_back(entry);
  }

  const std::string text = report.dump(1);
  if (!output_path.empty()) {
    std::ofstream out(output_path);
    out << text << "\n";
  }
  std::cout << text;
  return 0;
}
