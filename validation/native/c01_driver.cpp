// C01 native in-memory API driver: WCS forward / inverse / derivative.
//
// Independence rules honoured by this file:
//   * it includes ONLY the public declaration contract <acsd/contracts.hpp>
//     (plus a third party JSON reader for the frozen fixture file);
//   * it does not include any production internal header and does not
//     re-implement any science algorithm;
//   * it reads the frozen *input* fixtures and moves fields into the public
//     API; the truth values live in a separate file that this driver never
//     opens.

#include <acsd/contracts.hpp>

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
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
Result<T> failure(Code code, const std::string& reason) {
  Result<T> r;
  r.code = code;
  return r;
}

// The stable lower-case wire names of the public Code enumeration.
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

template <class T>
std::string reason_of(const Result<T>& r) {
  if (!r.assessment.issues.empty()) return r.assessment.issues.front().reason;
  return "";
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
    if (bytes.size() != static_cast<std::size_t>(count) * item) {
      return failure<ArrayRef>(Code::invalid_request, "byte count does not match the shape");
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
    identity.run_id = "validation-c01";
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
    if (!array) return failure<ArrayView>(Code::invalid_request, "no array supplied");
    auto it = entries_.find(array->identity.content_hash);
    if (it == entries_.end()) return failure<ArrayView>(Code::invalid_request, "unknown array");
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    if (range.begin + range.count > total) {
      return failure<ArrayView>(Code::invalid_request, "the range is out of bounds");
    }
    const std::size_t item = item_size(array->dtype);
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
    return failure<ArrayRef>(Code::unsupported, "the validation storage does not open products");
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
    // A stable, non-cryptographic identity is enough for the driver: the
    // canonical content hash of a product is produced by the production code.
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
    if (value.is_string()) {
      field.value = value.get<std::string>();
    } else if (value.is_number_integer()) {
      field.value = value.get<std::int64_t>();
    } else {
      field.value = value.get<double>();
    }
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

acsd::NumericBudget read_budget(const json& node) {
  acsd::NumericBudget budget;
  budget.tolerance.absolute = node.value("absolute", 1e-12);
  budget.tolerance.relative = node.value("relative", 1e-12);
  budget.tolerance.absolute_floor = node.value("absolute_floor", 0.0);
  budget.tolerance.unit.scale = node.value("unit_scale", "rad");
  budget.tolerance.unit.sr_power = 0;
  budget.tolerance.unit.pixel_power = 0;
  budget.tolerance.unit.nm_power = 0;
  budget.tolerance.unit.multiplier = 1.0;
  budget.tolerance.unit.linear_power = 0;
  budget.max_iterations = node.value("max_iterations", 64u);
  budget.arithmetic.mode = node.value("outward", true) ? acsd::Arithmetic::Mode::outward
                                                        : acsd::Arithmetic::Mode::approximate;
  budget.arithmetic.initial_bits = node.value("initial_bits", 64u);
  budget.arithmetic.max_bits = node.value("max_bits", 256u);
  budget.arithmetic.allow_target_fallback = node.value("allow_target_fallback", false);
  return budget;
}

acsd::AstrometricContext read_context(const json& node) {
  acsd::AstrometricContext context;
  context.epoch_julian_year = node.value("epoch_julian_year", 2016.0);
  context.observation_jd_tdb = node.value("observation_jd_tdb", 2457600.0);
  if (node.contains("barycentric_observer_au")) {
    const auto& observer = node.at("barycentric_observer_au");
    context.barycentric_observer_au = acsd::Vec3{observer.at(0).get<double>(),
                                                  observer.at(1).get<double>(),
                                                  observer.at(2).get<double>()};
  }
  return context;
}

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
    std::cerr << "usage: c01_driver --fixtures <file> [--output <file>]\n";
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
  report["driver"] = "c01_native";
  report["route"] = "in_memory_public_api";
  report["cases"] = json::array();

  for (const auto& fixture : fixtures.at("cases")) {
    json entry;
    entry["name"] = fixture.at("name");
    const acsd::HeaderFields header = read_header(fixture.at("header"));
    const acsd::Box domain = read_box(fixture.at("domain"));
    const acsd::NumericBudget budget = read_budget(fixture.at("budget"));
    const acsd::AstrometricContext astro = read_context(fixture.at("astrometric_context"));

    Result<Ref<acsd::WcsModel>> wcs = acsd::make_wcs(context, header, domain, astro,
                                                      fixture.value("detector_frame", "detector"),
                                                      budget);
    if (!ok(wcs)) {
      entry["status"] = "wcs_failed";
      entry["code"] = code_name(static_cast<int>(wcs.code));
      entry["code_value"] = static_cast<int>(wcs.code);
      entry["reason"] = reason_of(wcs);
      report["cases"].push_back(entry);
      continue;
    }
    entry["status"] = "wcs_built";
    entry["crpix_fits1"] = {val(wcs)->crpix_fits1[0], val(wcs)->crpix_fits1[1]};
    entry["cd_rad_per_pixel"] = {val(wcs)->cd_rad_per_pixel[0], val(wcs)->cd_rad_per_pixel[1],
                                  val(wcs)->cd_rad_per_pixel[2], val(wcs)->cd_rad_per_pixel[3]};
    entry["sip_a_terms"] = val(wcs)->sip_a.terms.size();
    entry["sip_b_terms"] = val(wcs)->sip_b.terms.size();
    entry["sip_a_order"] = val(wcs)->sip_a.order;
    entry["sip_b_order"] = val(wcs)->sip_b.order;
    entry["c"] = {val(wcs)->c[0], val(wcs)->c[1], val(wcs)->c[2]};
    entry["e"] = {val(wcs)->e[0], val(wcs)->e[1], val(wcs)->e[2]};
    entry["north"] = {val(wcs)->north[0], val(wcs)->north[1], val(wcs)->north[2]};

    json forward = json::array();
    for (const auto& pixel : fixture.at("forward_pixels")) {
      acsd::PixelCenter0 point;
      point.x = pixel.at(0).get<double>();
      point.y = pixel.at(1).get<double>();
      Result<acsd::SkyICRS> sky = acsd::forward(context, val(wcs), point, budget);
      json item;
      item["pixel"] = {point.x, point.y};
      item["code"] = code_name(static_cast<int>(sky.code));
      if (ok(sky)) {
        item["nx"] = val(sky).nx;
        item["ny"] = val(sky).ny;
        item["nz"] = val(sky).nz;
      } else {
        item["reason"] = reason_of(sky);
      }
      forward.push_back(item);
    }
    entry["forward"] = forward;

    json inverse = json::array();
    if (fixture.contains("inverse_sky")) {
      for (const auto& sky_node : fixture.at("inverse_sky")) {
        acsd::SkyICRS sky;
        sky.nx = sky_node.at(0).get<double>();
        sky.ny = sky_node.at(1).get<double>();
        sky.nz = sky_node.at(2).get<double>();
        Result<acsd::InverseResult> back = acsd::inverse(context, val(wcs), sky, budget);
        json item;
        item["sky"] = {sky.nx, sky.ny, sky.nz};
        item["code"] = code_name(static_cast<int>(back.code));
        if (ok(back)) {
          item["x"] = val(back).point.x;
          item["y"] = val(back).point.y;
          item["polynomial_residual_lower"] = val(back).polynomial_residual_pixel.lower;
          item["polynomial_residual_upper"] = val(back).polynomial_residual_pixel.upper;
          item["sky_residual_lower"] = val(back).sky_residual_rad.lower;
          item["sky_residual_upper"] = val(back).sky_residual_rad.upper;
          item["root_box_lower"] = {val(back).root_box.lower[0], val(back).root_box.lower[1]};
          item["root_box_upper"] = {val(back).root_box.upper[0], val(back).root_box.upper[1]};
        } else {
          item["reason"] = reason_of(back);
        }
        inverse.push_back(item);
      }
    }
    entry["inverse"] = inverse;

    json derivatives = json::array();
    if (fixture.contains("derivative_pixels")) {
      for (const auto& pixel : fixture.at("derivative_pixels")) {
        acsd::PixelCenter0 point;
        point.x = pixel.at(0).get<double>();
        point.y = pixel.at(1).get<double>();
        Result<acsd::DerivativeResult> jacobian =
            acsd::derivative(context, val(wcs), point, budget);
        json item;
        item["pixel"] = {point.x, point.y};
        item["code"] = code_name(static_cast<int>(jacobian.code));
        if (ok(jacobian)) {
          item["jacobian"] = val(jacobian).jacobian;
          for (const auto& enclosure : val(jacobian).enclosure) {
            item["enclosure"].push_back({enclosure.lower, enclosure.upper});
          }
        } else {
          item["reason"] = reason_of(jacobian);
        }
        derivatives.push_back(item);
      }
    }
    entry["derivative"] = derivatives;

    if (fixture.contains("jacobian_box")) {
      const acsd::Box box = read_box(fixture.at("jacobian_box"));
      Result<acsd::Interval> solid = acsd::solid_angle_jacobian(context, val(wcs), box, budget);
      json item;
      item["code"] = code_name(static_cast<int>(solid.code));
      if (ok(solid)) {
        item["lower"] = val(solid).lower;
        item["upper"] = val(solid).upper;
      } else {
        item["reason"] = reason_of(solid);
      }
      entry["solid_angle_jacobian"] = item;
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
