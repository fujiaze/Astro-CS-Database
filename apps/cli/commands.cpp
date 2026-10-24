// The three commands of the CLI and the diagnostics.
#include "cli.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>

#include <fitsio.h>

#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"

namespace acsd::cli {
namespace {

using model::fail;
using model::ok_result;

bool has(const json& node, const char* key) {
  return node.is_object() && node.contains(key) && !node.at(key).is_null();
}

std::string string_field(const json& node, const char* key, const std::string& fallback = {}) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_string() ? value.get<std::string>() : fallback;
}

U64 u64_field(const json& node, const char* key, U64 fallback = 0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  if (value.is_number_unsigned()) return value.get<U64>();
  if (value.is_number_integer()) {
    const std::int64_t signed_value = value.get<std::int64_t>();
    return signed_value < 0 ? fallback : static_cast<U64>(signed_value);
  }
  return fallback;
}

bool bool_field(const json& node, const char* key, bool fallback) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_boolean() ? value.get<bool>() : fallback;
}

const json& field(const json& node, const char* key) {
  static const json empty = json(nullptr);
  if (!node.is_object() || !node.contains(key)) return empty;
  return node.at(key);
}

double double_field(const json& node, const char* key, double fallback = 0.0) {
  if (!has(node, key)) return fallback;
  const json& value = node.at(key);
  return value.is_number() ? value.get<double>() : fallback;
}

Unit decode_unit(const json& node) {
  Unit unit;
  unit.scale = string_field(node, "scale", "1");
  unit.sr_power = static_cast<std::int32_t>(u64_field(node, "sr_power", 0));
  unit.pixel_power = static_cast<std::int32_t>(u64_field(node, "pixel_power", 0));
  unit.nm_power = static_cast<std::int32_t>(u64_field(node, "nm_power", 0));
  unit.multiplier = double_field(node, "multiplier", 1.0);
  unit.linear_power = static_cast<std::int32_t>(u64_field(node, "linear_power", 0));
  return unit;
}

Arithmetic decode_arithmetic(const json& node) {
  Arithmetic arithmetic;
  arithmetic.mode = string_field(node, "mode", "outward") == "approximate"
                        ? Arithmetic::Mode::approximate
                        : Arithmetic::Mode::outward;
  arithmetic.initial_bits = static_cast<U32>(u64_field(node, "initial_bits", 64));
  arithmetic.max_bits = static_cast<U32>(u64_field(node, "max_bits", 256));
  arithmetic.allow_target_fallback = has(node, "allow_target_fallback") &&
                                     node.at("allow_target_fallback").is_boolean() &&
                                     node.at("allow_target_fallback").get<bool>();
  return arithmetic;
}

NumericBudget decode_numeric_budget(const json& node) {
  NumericBudget budget;
  budget.tolerance.absolute = double_field(field(node, "tolerance"), "absolute", 1e-12);
  budget.tolerance.relative = double_field(field(node, "tolerance"), "relative", 0.0);
  budget.tolerance.absolute_floor = double_field(field(node, "tolerance"), "absolute_floor", 1e-12);
  budget.tolerance.unit = decode_unit(field(node, "tolerance"));
  budget.max_iterations = static_cast<U32>(u64_field(node, "max_iterations", 200));
  budget.arithmetic = decode_arithmetic(has(node, "arithmetic") ? node.at("arithmetic")
                                                                 : json::object());
  return budget;
}

GeometryBudget decode_budget(const json& node) {
  GeometryBudget budget;
  budget.area_absolute_sr = double_field(node, "area_absolute_sr", 0.0);
  budget.normalizer_min_sr = double_field(node, "normalizer_min_sr", 0.0);
  budget.max_work = u64_field(node, "max_work", 0);
  budget.max_bytes = u64_field(node, "max_bytes", 0);
  budget.max_depth = static_cast<U32>(u64_field(node, "max_depth", 64));
  budget.arithmetic = decode_arithmetic(has(node, "arithmetic") ? node.at("arithmetic") : json::object());
  const json& limits = node.contains("per_leaf_absolute_area") ? node.at("per_leaf_absolute_area")
                                                                : json::array();
  if (limits.is_array()) {
    for (const json& item : limits) {
      LeafAreaLimit limit;
      const json& leaf = item.contains("leaf") ? item.at("leaf") : json::object();
      limit.leaf.order = static_cast<U32>(u64_field(leaf, "order", 0));
      limit.leaf.ipix = u64_field(leaf, "ipix", 0);
      limit.absolute_sr = double_field(item, "absolute_sr", 0.0);
      budget.per_leaf_absolute_area.push_back(limit);
    }
  }
  return budget;
}

Polynomial2D decode_polynomial(const json& node) {
  Polynomial2D polynomial;
  polynomial.order = static_cast<U32>(u64_field(node, "order", 0));
  const json& terms = node.contains("terms") ? node.at("terms") : json::array();
  if (terms.is_array()) {
    for (const json& item : terms) {
      Term2 term;
      term.x_power = static_cast<U32>(u64_field(item, "x_power", 0));
      term.y_power = static_cast<U32>(u64_field(item, "y_power", 0));
      term.coefficient = double_field(item, "coefficient", 0.0);
      polynomial.terms.push_back(term);
    }
  }
  return polynomial;
}

Box decode_box(const json& node) {
  Box box;
  const json& lower = node.contains("lower") ? node.at("lower") : json::array();
  const json& upper = node.contains("upper") ? node.at("upper") : json::array();
  if (lower.is_array() && lower.size() == 2) {
    box.lower = {lower[0].get<double>(), lower[1].get<double>()};
  }
  if (upper.is_array() && upper.size() == 2) {
    box.upper = {upper[0].get<double>(), upper[1].get<double>()};
  }
  return box;
}

AstrometricContext decode_astrometric(const json& node) {
  AstrometricContext context;
  context.epoch_julian_year = double_field(node, "epoch_julian_year", 0.0);
  context.observation_jd_tdb = double_field(node, "observation_jd_tdb", 0.0);
  const json& omitted = node.contains("omitted") ? node.at("omitted") : json::array();
  if (omitted.is_array()) {
    for (const json& item : omitted) {
      if (item.is_string()) context.omitted.push_back(item.get<std::string>());
    }
  }
  return context;
}

// The WCS model of the export target.
Ref<WcsModel> decode_wcs(const json& node) {
  auto wcs = std::make_shared<WcsModel>();
  const json& crpix = node.contains("crpix_fits1") ? node.at("crpix_fits1") : json::array();
  if (crpix.is_array() && crpix.size() == 2) {
    wcs->crpix_fits1 = {crpix[0].get<double>(), crpix[1].get<double>()};
  }
  const json& cd = node.contains("cd_rad_per_pixel") ? node.at("cd_rad_per_pixel") : json::array();
  if (cd.is_array() && cd.size() == 4) {
    wcs->cd_rad_per_pixel = {cd[0].get<double>(), cd[1].get<double>(), cd[2].get<double>(),
                             cd[3].get<double>()};
  }
  const json& c = node.contains("c") ? node.at("c") : json::array();
  if (c.is_array() && c.size() == 3) {
    wcs->c = {c[0].get<double>(), c[1].get<double>(), c[2].get<double>()};
  }
  const json& e = node.contains("e") ? node.at("e") : json::array();
  if (e.is_array() && e.size() == 3) {
    wcs->e = {e[0].get<double>(), e[1].get<double>(), e[2].get<double>()};
  }
  const json& north = node.contains("north") ? node.at("north") : json::array();
  if (north.is_array() && north.size() == 3) {
    wcs->north = {north[0].get<double>(), north[1].get<double>(), north[2].get<double>()};
  }
  if (has(node, "sip_a")) wcs->sip_a = decode_polynomial(node.at("sip_a"));
  if (has(node, "sip_b")) wcs->sip_b = decode_polynomial(node.at("sip_b"));
  if (has(node, "domain")) wcs->domain = decode_box(node.at("domain"));
  if (has(node, "epoch_frame_observer")) {
    wcs->epoch_frame_observer = decode_astrometric(node.at("epoch_frame_observer"));
  }
  wcs->detector_frame = string_field(node, "detector_frame", "acsd.target");
  wcs->identity = model::make_identity("acsd.WcsModel", 2, 0,
                                        model::sha256_hex("acsd.cli.target.wcs"), "acsd");
  wcs->assessment = model::make_assessment(Code::ok);
  wcs->assessment.complete = true;
  return std::shared_ptr<const WcsModel>(wcs);
}

ExportRequest decode_export_request(const json& node) {
  ExportRequest request;
  const json& target = node.contains("target") ? node.at("target") : json::object();
  const json& model = target.contains("model") ? target.at("model") : json::object();
  request.target.model = decode_wcs(model);
  request.target.width = u64_field(target, "width", 0);
  request.target.height = u64_field(target, "height", 0);
  const std::string quantity = string_field(node, "quantity", "supported_mean");
  if (quantity == "supported_flux") {
    request.quantity = ExportRequest::Quantity::supported_flux;
  } else if (quantity == "extrapolated_full_flux") {
    request.quantity = ExportRequest::Quantity::extrapolated_full_flux;
  } else {
    request.quantity = ExportRequest::Quantity::supported_mean;
  }
  const std::string policy = string_field(node, "support_policy", "dyadic_cells");
  if (policy == "primitives") {
    request.support_policy = SupportMap::Kind::primitives;
  } else if (policy == "full_leaf_approx") {
    request.support_policy = SupportMap::Kind::full_leaf_approx;
  } else if (policy == "effective_area_approx") {
    request.support_policy = SupportMap::Kind::effective_area_approx;
  } else {
    request.support_policy = SupportMap::Kind::dyadic_cells;
  }
  request.assume_uniform_uncovered = has(node, "assume_uniform_uncovered") &&
                                     node.at("assume_uniform_uncovered").is_boolean() &&
                                     node.at("assume_uniform_uncovered").get<bool>();
  const std::string dtype = string_field(node, "output_dtype", "f64");
  request.output_dtype = dtype == "f32" ? DType::f32 : DType::f64;
  if (has(node, "budget")) request.budget = decode_budget(node.at("budget"));
  return request;
}

// The exit code of a failed call.

// The stable lowercase name of a code: the event stream carries the specific
// code, not only "failed".
std::string code_name(Code code) {
  switch (code) {
    case Code::ok: return "ok";
    case Code::invalid_request: return "invalid_request";
    case Code::invalid_model: return "invalid_model";
    case Code::domain: return "domain";
    case Code::unsupported: return "unsupported";
    case Code::unavailable: return "unavailable";
    case Code::unsolved: return "unsolved";
    case Code::not_identifiable: return "not_identifiable";
    case Code::invalid_covariance: return "invalid_covariance";
    case Code::duplicate_source: return "duplicate_source";
    case Code::identity_conflict: return "identity_conflict";
    case Code::hash_mismatch: return "hash_mismatch";
    case Code::corrupt_product: return "corrupt_product";
    case Code::missing_dependency: return "missing_dependency";
    case Code::no_support: return "no_support";
    case Code::support_unresolved: return "support_unresolved";
    case Code::geometry_solve_unresolved: return "geometry_solve_unresolved";
    case Code::selection_unresolved: return "selection_unresolved";
    case Code::nonconverged: return "nonconverged";
    case Code::precision_exhausted: return "precision_exhausted";
    case Code::resource_exhausted: return "resource_exhausted";
    case Code::io_error: return "io_error";
    case Code::cancelled: return "cancelled";
    case Code::internal: return "internal";
  }
  return "internal";
}

int exit_code_of(Code code) {
  switch (code) {
    case Code::ok: return kSuccess;
    case Code::invalid_request:
    case Code::invalid_model:
    case Code::duplicate_source:
    case Code::identity_conflict: return kInvalidInput;
    case Code::unsupported:
    case Code::unavailable:
    case Code::not_identifiable:
    case Code::unsolved:
    case Code::no_support:
    case Code::support_unresolved: return kUnsupported;
    case Code::cancelled: return kCancelled;
    case Code::resource_exhausted:
    case Code::precision_exhausted:
    case Code::nonconverged:
    case Code::io_error:
    case Code::corrupt_product:
    case Code::hash_mismatch:
    case Code::missing_dependency:
    case Code::invalid_covariance:
    case Code::internal: return kFailure;
    case Code::domain:
    case Code::geometry_solve_unresolved:
    case Code::selection_unresolved: return kPartial;
  }
  return kFailure;
}

// The run summary of one command.

// The per frame status of the summary.  The element fields are fixed by the
// protocol: the frame identifier, the status and the reason.
json frame_status(const std::string& frame, const std::string& status,
                  const std::string& reason) {
  json entry;
  entry["frame"] = frame;
  entry["status"] = status;
  entry["code"] = status == "published" ? "ok" : status;
  entry["reason"] = reason;
  return entry;
}

json make_summary(U64 total, U64 published, U64 unsolved, U64 failed, U64 cancelled,
                  const json& frames) {
  json summary;
  summary["inputs_total"] = total;
  summary["published"] = published;
  summary["unsolved"] = unsolved;
  summary["failed"] = failed;
  summary["cancelled"] = cancelled;
  summary["frames"] = frames;
  return summary;
}

std::string hex_digest(const std::string& text) {
  return model::sha256_hex(text);
}

}  // namespace



// The astrometry request of the normalize command.
SolverRequest decode_solver_request(const json& node) {
  SolverRequest request;
  const json& regions = node.contains("regions") ? node.at("regions") : json::array();
  if (regions.is_array()) {
    for (const json& item : regions) {
      SkySearchRegion region;
      const json& center = item.contains("center") ? item.at("center") : json::object();
      region.center.nx = double_field(center, "nx", 0.0);
      region.center.ny = double_field(center, "ny", 0.0);
      region.center.nz = double_field(center, "nz", 1.0);
      region.radius_rad = double_field(item, "radius_rad", 0.0);
      request.regions.push_back(region);
    }
  }
  request.scale_rad_pixel.lower = double_field(field(node, "scale_rad_pixel"), "lower", 0.0);
  request.scale_rad_pixel.upper = double_field(field(node, "scale_rad_pixel"), "upper", 0.0);
  request.max_candidates = static_cast<U32>(u64_field(node, "max_candidates", 32));
  request.max_hypotheses = static_cast<U32>(u64_field(node, "max_hypotheses", 8));
  request.sip_order = static_cast<U32>(u64_field(node, "sip_order", 2));
  request.detect_snr = double_field(node, "detect_snr", 5.0);
  request.match_radius_pixel = double_field(node, "match_radius_pixel", 2.0);
  request.min_matches = static_cast<U32>(u64_field(node, "min_matches", 6));
  request.quad_descriptor_tolerance = double_field(node, "quad_descriptor_tolerance", 0.1);
  if (has(node, "budget")) request.budget = decode_numeric_budget(node.at("budget"));
  return request;
}

// The point spread function request.
PsfRequest decode_psf_request(const json& node) {
  PsfRequest request;
  const std::string family = string_field(node, "family", "gaussian");
  request.family = family == "moffat" ? PsfRequest::Family::moffat
                                      : (family == "airy" ? PsfRequest::Family::airy
                                                          : PsfRequest::Family::gaussian);
  request.spatial_degree = static_cast<U32>(u64_field(node, "spatial_degree", 0));
  request.initial_width_pixel = double_field(node, "initial_width_pixel", 2.0);
  request.tail_absolute_flux = double_field(node, "tail_absolute_flux", 1e-6);
  request.window_radius_pixel = double_field(node, "window_radius_pixel", 10.0);
  if (has(node, "budget")) request.budget = decode_numeric_budget(node.at("budget"));
  return request;
}

// The drizzle request.
DrizzleRequest decode_drizzle_request(const json& node) {
  DrizzleRequest request;
  request.order = static_cast<U32>(u64_field(node, "order", 6));
  request.pixfrac = double_field(node, "pixfrac", 1.0);
  if (has(node, "budget")) request.budget = decode_budget(node.at("budget"));
  return request;
}

// The joint photometry request.  The responses of the frames are the ones of
// the configuration.
PhotometryRequest decode_photometry_request(const json& node,
                                            const std::vector<NormalizeFrame>& frames) {
  PhotometryRequest request;
  for (const NormalizeFrame& frame : frames) {
    if (frame.response) request.responses.push_back(frame.response);
    request.frames.push_back(frame.frame);
  }
  request.application_id = string_field(node, "application_id", "acsd.cli");
  request.gradient_tolerance = double_field(node, "gradient_tolerance", 1e-8);
  request.step_tolerance = double_field(node, "step_tolerance", 1e-8);
  request.max_outer_iterations = static_cast<U32>(u64_field(node, "max_outer_iterations", 50));
  if (has(node, "solve")) {
    const json& solve = node.at("solve");
    request.solve.relative_residual = double_field(solve, "relative_residual", 1e-10);
    request.solve.absolute_residual = double_field(solve, "absolute_residual", 1e-12);
    request.solve.rank_relative = double_field(solve, "rank_relative", 1e-12);
    request.solve.max_iterations = static_cast<U32>(u64_field(solve, "max_iterations", 200));
    request.solve.dense_max_bytes = u64_field(solve, "dense_max_bytes", 64ull * 1024 * 1024);
    request.solve.huber = bool_field(solve, "huber", false);
    request.solve.huber_delta = double_field(solve, "huber_delta", 1.345);
  }
  if (has(node, "basis")) {
    const json& basis = node.at("basis");
    const std::string kind = string_field(basis, "kind", "constant");
    request.basis.kind = kind == "plane" ? Basis::Kind::plane
                                         : (kind == "polynomial" ? Basis::Kind::polynomial
                                                                 : Basis::Kind::constant);
    request.basis.degree = static_cast<U32>(u64_field(basis, "degree", 0));
    const json& center = field(basis, "center");
    if (center.is_array() && center.size() == 2) {
      request.basis.center = {center[0].get<double>(), center[1].get<double>()};
    }
    const json& scale = field(basis, "scale");
    if (scale.is_array() && scale.size() == 2) {
      request.basis.scale = {scale[0].get<double>(), scale[1].get<double>()};
    }
    request.basis.normalization_domain = decode_box(field(basis, "normalization_domain"));
  }
  if (has(node, "likelihood")) {
    const json& likelihood = node.at("likelihood");
    request.likelihood.estimator = string_field(likelihood, "estimator", "joint_flux_gaussian");
    request.likelihood.kind = string_field(likelihood, "kind", "fixed_geometry");
  }
  return request;
}

// ---------------------------------------------------------------------------
// The inputs of the normalize command
// ---------------------------------------------------------------------------

// One FITS image read from the disk.
struct FitsImage {
  U64 width = 0;
  U64 height = 0;
  std::vector<double> values;
  std::vector<std::uint16_t> dq;
  HeaderFields header;
};

std::string fits_message(int status) {
  char text[128] = {0};
  fits_get_errstatus(status, text);
  return std::string(text);
}

// Reads the primary image of a FITS file and its header cards.
//
// The reader parses the standard FITS layout directly: the header blocks of
// 2880 bytes with the 80 character cards and the big endian float64 data
// unit.  The library reader of the installed CFITSIO 4.6.2 rejects the
// elements of the frames of the validation set with "bad first element
// number"; the probe is recorded in validation/apps/fits_cfitsio_probe.txt.
// Reads the primary image of a FITS file and its header cards.
//
// The reading uses the system CFITSIO 4.6.2: fits_open_file, fits_get_img_dim,
// fits_get_img_size, fits_read_img (the short name ffgpv) and
// fits_read_keyword.  Two facts of the API of the library are respected here:
// the axis lengths of ffgisz are written directly into the array (the first
// entry is the length of the first axis) and the element reader of the
// images is ffgpv, not ffgpxv (which reads the rows of a binary table).
// The writing path of the library is not usable (the data unit stays at
// zero), so the products are written by the explicit writer of the workflows
// module; see validation/apps/fits_cfitsio_probe.txt.
Result<FitsImage> read_fits_image(const std::filesystem::path& path) {
  int status = 0;
  fitsfile* file = nullptr;
  status = fits_open_file(&file, path.string().c_str(), READONLY, &status);
  if (status || !file) {
    return fail<FitsImage>(Code::invalid_request,
                           "the FITS file cannot be read: " + fits_message(status), "cli.fits",
                           "the frame cannot be processed");
  }
  FitsImage image;
  int naxis = 0;
  status = fits_get_img_dim(file, &naxis, &status);
  long naxes[4] = {1, 1, 1, 1};
  if (!status) status = fits_get_img_size(file, 4, naxes, &status);
  if (status || naxis < 2) {
    fits_close_file(file, &status);
    return fail<FitsImage>(Code::invalid_request,
                           "the FITS file has no two dimensional image: " + fits_message(status),
                           "cli.fits", "the frame cannot be processed");
  }
  image.width = static_cast<U64>(naxes[0]);
  image.height = static_cast<U64>(naxes[1]);
  const U64 count = image.width * image.height;
  image.values.assign(count, 0.0);
  image.dq.assign(count, 0);
  int any_null = 0;
  status = fits_read_img(file, TDOUBLE, 1, static_cast<long long>(count), nullptr,
                         image.values.data(), &any_null, &status);
  if (status) {
    fits_close_file(file, &status);
    return fail<FitsImage>(Code::invalid_request,
                           "the FITS pixels cannot be read: " + fits_message(status), "cli.fits",
                           "the frame cannot be processed");
  }
  // The header cards of the world coordinate solution and of the exposure.
  static const char* kKeys[] = {"CTYPE1", "CTYPE2", "CUNIT1", "CUNIT2", "CRPIX1", "CRPIX2",
                                "CRVAL1", "CRVAL2", "CD1_1",  "CD1_2",  "CD2_1",  "CD2_2",
                                "RADESYS", "MJD-OBS", "DATE-OBS", "EXPTIME", "BUNIT"};
  for (const char* key : kKeys) {
    char value[80] = {0};
    char comment[80] = {0};
    int read_status = 0;
    status = fits_read_keyword(file, key, value, comment, &read_status);
    if (status || read_status) continue;
    HeaderField field;
    field.key = key;
    field.value = std::string(value);
    image.header.push_back(field);
  }
  fits_close_file(file, &status);
  return ok_result(image);
}

// The numeric value of a header card.
bool header_double(const HeaderFields& header, const std::string& key, double& value) {
  for (const HeaderField& field : header) {
    if (field.key != key) continue;
    if (std::holds_alternative<double>(field.value)) {
      value = std::get<double>(field.value);
      return true;
    }
    if (std::holds_alternative<std::string>(field.value)) {
      try {
        value = std::stod(std::get<std::string>(field.value));
        return true;
      } catch (const std::exception&) {
        return false;
      }
    }
  }
  return false;
}

// The raw observation of one frame.
Result<Ref<RawObservation>> build_raw_observation(Context& context, const FitsImage& image,
                                                   const std::string& frame_id) {
  const U64 count = image.width * image.height;
  const std::vector<std::byte> adu_bytes = model::encode_f64(image.values);
  const std::vector<std::byte> dq_bytes = model::encode_u16(image.dq);
  const std::vector<U64> shape{count};
  Result<ArrayRef> adu = context.storage.copy(context, DType::f64, adu_bytes, shape,
                                               model::unit_adu("detector:" + frame_id),
                                               "acsd.raw.adu");
  if (!ok(adu)) {
    return fail<Ref<RawObservation>>(adu.code, "the image cannot be stored", "cli.fits",
                                     "the frame cannot be processed");
  }
  Result<ArrayRef> dq = context.storage.copy(context, DType::u16, dq_bytes, shape,
                                              model::unit_dimensionless(), "acsd.raw.dq");
  if (!ok(dq)) {
    return fail<Ref<RawObservation>>(dq.code, "the data quality cannot be stored", "cli.fits",
                                     "the frame cannot be processed");
  }
  // The native index of the frame: one partition over the whole detector.
  Index index;
  index.size = count;
  index.kind = Index::Kind::native;
  index.order = 0;
  index.namespace_id = "acsd.native:" + frame_id;
  SourcePartition partition;
  partition.dataset = "acsd.cli";
  partition.exposure = frame_id;
  partition.detector = "detector";
  partition.channel = "adu";
  partition.index_begin = 0;
  partition.count = count;
  partition.first_sample = 0;
  partition.sample_stride = 1;
  index.partitions.push_back(partition);
  index.identity = model::make_identity("acsd.Index", 2, 0,
                                        model::sha256_hex("acsd.cli.index:" + frame_id), "acsd");
  index.assessment = model::make_assessment(Code::ok);
  index.assessment.complete = true;
  auto native_index = std::make_shared<Index>(index);

  // The footprint set: a native rectangle without a world coordinate
  // solution, the solver binds it later.
  auto footprints = std::make_shared<FootprintSet>();
  footprints->index = std::shared_ptr<const Index>(native_index);
  footprints->width = image.width;
  footprints->height = image.height;
  footprints->detector_from_sample = {1.0, 0.0, 0.0, 1.0};
  footprints->offset = {0.0, 0.0};
  footprints->local_cell = {{-0.5, -0.5}, {0.5, 0.5}};
  footprints->detector_frame = frame_id;
  footprints->identity = model::make_identity("acsd.FootprintSet", 2, 0,
                                              model::sha256_hex("acsd.cli.footprints:" + frame_id),
                                              "acsd");
  footprints->assessment = model::make_assessment(Code::ok);
  footprints->assessment.complete = true;

  // The astrometric context of the exposure.
  AstrometricContext astrometric;
  double mjd = 0.0;
  if (header_double(image.header, "MJD-OBS", mjd)) {
    astrometric.observation_jd_tdb = mjd + 2400000.5;
    astrometric.epoch_julian_year = 2000.0 + (mjd - 51544.5) / 365.25;
  }

  auto raw = std::make_shared<RawObservation>();
  raw->adu = val(adu);
  raw->dq = val(dq);
  raw->width = image.width;
  raw->height = image.height;
  raw->context = astrometric;
  raw->source_index = std::shared_ptr<const Index>(native_index);
  raw->footprints = std::shared_ptr<const FootprintSet>(footprints);
  raw->frame = frame_id;
  raw->identity = model::make_identity("acsd.RawObservation", 2, 0,
                                       model::sha256_hex("acsd.cli.raw:" + frame_id), "acsd");
  raw->assessment = model::make_assessment(Code::ok);
  raw->assessment.complete = true;
  return ok_result(std::shared_ptr<const RawObservation>(raw));
}


// The response of a frame: the wavelength grid and the throughput.
Result<Ref<Response>> load_response(Context& context, const std::filesystem::path& path,
                                     const std::string& frame_id) {
  std::ifstream stream(path);
  if (!stream) {
    return fail<Ref<Response>>(Code::invalid_request, "the response cannot be read: " + path.string(),
                               "cli.response", "the frame cannot be processed");
  }
  json document;
  try {
    stream >> document;
  } catch (const std::exception& failure) {
    return fail<Ref<Response>>(Code::invalid_request,
                               std::string("the response is not valid JSON: ") + failure.what(),
                               "cli.response", "the frame cannot be processed");
  }
  const json& wavelengths = document.contains("wavelength_nm") ? document.at("wavelength_nm")
                                                               : json::array();
  const json& throughput = document.contains("throughput") ? document.at("throughput")
                                                           : json::array();
  if (!wavelengths.is_array() || !throughput.is_array() || wavelengths.size() != throughput.size() ||
      wavelengths.empty()) {
    return fail<Ref<Response>>(Code::invalid_request,
                               "the response needs a wavelength grid and a throughput of the same size",
                               "cli.response", "the frame cannot be processed");
  }
  std::vector<double> wavelength_values;
  std::vector<double> throughput_values;
  for (const json& item : wavelengths) wavelength_values.push_back(item.get<double>());
  for (const json& item : throughput) throughput_values.push_back(item.get<double>());
  const std::vector<U64> shape{static_cast<U64>(wavelength_values.size())};
  Result<ArrayRef> grid = context.storage.copy(context, DType::f64,
                                                model::encode_f64(wavelength_values), shape,
                                                model::unit_nm(), "acsd.response.wavelength");
  Result<ArrayRef> values = context.storage.copy(context, DType::f64,
                                                  model::encode_f64(throughput_values), shape,
                                                  model::unit_dimensionless(),
                                                  "acsd.response.throughput");
  if (!ok(grid) || !ok(values)) {
    return fail<Ref<Response>>(Code::io_error, "the response cannot be stored", "cli.response",
                               "the frame cannot be processed");
  }
  auto response = std::make_shared<Response>();
  response->convention = string_field(document, "convention", "photon_throughput") == "energy"
                              ? Response::Convention::includes_photon_conversion
                              : Response::Convention::photon_throughput;
  response->wavelength_nm = val(grid);
  response->throughput = val(values);
  response->provenance = string_field(document, "provenance", "cli:" + frame_id);
  response->normalization = string_field(document, "normalization", "unity");
  if (has(document, "support_nm")) {
    const json& support = document.at("support_nm");
    response->support_nm.lower = double_field(support, "lower", 0.0);
    response->support_nm.upper = double_field(support, "upper", 0.0);
  } else {
    response->support_nm.lower = wavelength_values.front();
    response->support_nm.upper = wavelength_values.back();
  }
  response->identity = model::make_identity("acsd.Response", 2, 0,
                                            model::sha256_hex("acsd.cli.response:" + frame_id),
                                            "acsd");
  response->assessment = model::make_assessment(Code::ok);
  response->assessment.complete = true;
  return ok_result(std::shared_ptr<const Response>(response));
}

// The calibration of a frame: the optional master bias, dark and flat.
Result<Calibration> load_calibration(Context& context, const std::filesystem::path& path,
                                     const std::vector<U64>& shape) {
  Calibration calibration;
  calibration.dark_contains_bias = true;
  if (path.empty()) return ok_result(calibration);
  std::ifstream stream(path);
  if (!stream) {
    return fail<Calibration>(Code::invalid_request,
                             "the calibration cannot be read: " + path.string(), "cli.calibration",
                             "the frame cannot be processed");
  }
  json document;
  try {
    stream >> document;
  } catch (const std::exception& failure) {
    return fail<Calibration>(Code::invalid_request,
                             std::string("the calibration is not valid JSON: ") + failure.what(),
                             "cli.calibration", "the frame cannot be processed");
  }
  auto load_plane = [&](const char* key) -> Result<ArrayRef> {
    if (!has(document, key)) {
      Result<ArrayRef> out;
      out.code = Code::ok;
      return out;
    }
    const json& values = document.at(key);
    if (!values.is_array()) {
      return fail<ArrayRef>(Code::invalid_request, "the calibration plane is not a list",
                            "cli.calibration", "the frame cannot be processed");
    }
    std::vector<double> plane;
    for (const json& item : values) plane.push_back(item.get<double>());
    return context.storage.copy(context, DType::f64, model::encode_f64(plane), shape,
                                 model::unit_adu("detector"), key);
  };
  Result<ArrayRef> bias = load_plane("bias");
  Result<ArrayRef> dark = load_plane("dark");
  Result<ArrayRef> flat = load_plane("flat");
  if (!ok(bias) || !ok(dark) || !ok(flat)) {
    return fail<Calibration>(bias.code == Code::ok ? dark.code : bias.code,
                             "the calibration cannot be stored", "cli.calibration",
                             "the frame cannot be processed");
  }
  calibration.bias = val(bias);
  calibration.dark = val(dark);
  calibration.flat = val(flat);
  calibration.dark_contains_bias = bool_field(document, "dark_contains_bias", true);
  calibration.exposure_seconds = double_field(document, "exposure_seconds", 1.0);
  calibration.dark_exposure_seconds = double_field(document, "dark_exposure_seconds", 1.0);
  calibration.flat_normalization = double_field(document, "flat_normalization", 1.0);
  if (document.contains("gain_e_per_adu"))
    calibration.gain_e_per_adu = document.at("gain_e_per_adu").get<double>();
  if (document.contains("read_rms_e"))
    calibration.read_rms_e = document.at("read_rms_e").get<double>();
  return ok_result(calibration);
}

// The catalogue of the run: the columns and the index.
Result<Ref<CatalogueProvider>> load_catalogue(Context& context, const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    return fail<Ref<CatalogueProvider>>(Code::invalid_request,
                                        "the catalogue cannot be read: " + path.string(),
                                        "cli.catalogue", "the command cannot run");
  }
  json document;
  try {
    stream >> document;
  } catch (const std::exception& failure) {
    return fail<Ref<CatalogueProvider>>(Code::invalid_request,
                                        std::string("the catalogue is not valid JSON: ") +
                                            failure.what(),
                                        "cli.catalogue", "the command cannot run");
  }
  const json& source_ids = document.contains("source_ids") ? document.at("source_ids")
                                                            : json::array();
  const json& astrometry = document.contains("astrometry") ? document.at("astrometry")
                                                           : json::array();
  const json& brightness = document.contains("brightness_mag") ? document.at("brightness_mag")
                                                              : json::array();
  const json& coefficients = document.contains("xp_coefficients") ? document.at("xp_coefficients")
                                                                  : json::array();
  if (!source_ids.is_array() || !astrometry.is_array() || !brightness.is_array() ||
      !coefficients.is_array() || source_ids.empty()) {
    return fail<Ref<CatalogueProvider>>(Code::invalid_request, "the catalogue columns are missing",
                                        "cli.catalogue", "the command cannot run");
  }
  std::vector<U64> ids;
  std::vector<double> positions;
  std::vector<double> magnitudes;
  std::vector<double> xp;
  for (const json& item : source_ids) ids.push_back(item.get<U64>());
  for (const json& row : astrometry) {
    if (!row.is_array() || row.size() != 8) {
      return fail<Ref<CatalogueProvider>>(Code::invalid_request,
                                          "the astrometry rows need eight columns", "cli.catalogue",
                                          "the command cannot run");
    }
    for (const json& item : row) positions.push_back(item.get<double>());
  }
  for (const json& item : brightness) magnitudes.push_back(item.get<double>());
  U64 coefficient_count = 0;
  for (const json& row : coefficients) {
    if (!row.is_array()) {
      return fail<Ref<CatalogueProvider>>(Code::invalid_request, "the xp coefficients are not rows",
                                          "cli.catalogue", "the command cannot run");
    }
    coefficient_count = row.size();
    for (const json& item : row) xp.push_back(item.get<double>());
  }
  const U64 count = ids.size();
  Result<ArrayRef> id_array = context.storage.copy(context, DType::u64, model::encode_u64(ids),
                                                    std::vector<U64>{count},
                                                    model::unit_dimensionless(),
                                                    "acsd.catalogue.source_ids");
  Result<ArrayRef> astrometry_array =
      context.storage.copy(context, DType::f64, model::encode_f64(positions),
                           std::vector<U64>{count, 8}, model::unit_dimensionless(),
                           "acsd.catalogue.astrometry");
  Result<ArrayRef> magnitude_array =
      context.storage.copy(context, DType::f64, model::encode_f64(magnitudes),
                           std::vector<U64>{count}, model::unit_dimensionless(),
                           "acsd.catalogue.brightness");
  Result<ArrayRef> coefficient_array =
      context.storage.copy(context, DType::f64, model::encode_f64(xp),
                           std::vector<U64>{count, coefficient_count},
                           model::unit_dimensionless(), "acsd.catalogue.xp");
  if (!ok(id_array) || !ok(astrometry_array) || !ok(magnitude_array) || !ok(coefficient_array)) {
    return fail<Ref<CatalogueProvider>>(Code::io_error, "the catalogue columns cannot be stored",
                                        "cli.catalogue", "the command cannot run");
  }
  // The spectral basis of the catalogue.
  const json& basis = document.contains("xp_basis") ? document.at("xp_basis") : json::object();
  const json& knots = basis.contains("knots_nm") ? basis.at("knots_nm") : json::array();
  const json& basis_coefficients = basis.contains("basis_coefficients")
                                       ? basis.at("basis_coefficients")
                                       : json::array();
  std::vector<double> knot_values;
  std::vector<double> coefficient_values;
  U64 basis_width = 0;
  for (const json& item : knots) knot_values.push_back(item.get<double>());
  for (const json& row : basis_coefficients) {
    basis_width = row.is_array() ? row.size() : 0;
    for (const json& item : row) coefficient_values.push_back(item.get<double>());
  }
  // OBJECTS:117 stores basis_coefficients[d,K,degree+1] (one polynomial per
  // knot segment).  A catalogue may give one [degree+1] row per basis function;
  // expand it across the K segments instead of rejecting or silently
  // reinterpreting the layout.
  const U64 basis_degree = u64_field(basis, "degree", 0);
  const U64 basis_functions =
      std::max<U64>(1, u64_field(basis, "coefficient_count",
                                 basis_coefficients.size()));
  const U64 segments = knot_values.size() > 1 ? knot_values.size() - 1 : 1;
  const U64 expected = basis_functions * segments * (basis_degree + 1);
  if (coefficient_values.size() != expected &&
      basis_coefficients.size() == basis_functions &&
      basis_width == basis_degree + 1 && segments > 1) {
    std::vector<double> expanded;
    expanded.reserve(expected);
    for (const json& row : basis_coefficients)
      for (U64 segment = 0; segment < segments; ++segment)
        for (const json& item : row) expanded.push_back(item.get<double>());
    coefficient_values = std::move(expanded);
    basis_width = basis_degree + 1;
  }
  Result<ArrayRef> knot_array =
      context.storage.copy(context, DType::f64, model::encode_f64(knot_values),
                           std::vector<U64>{static_cast<U64>(knot_values.size())}, model::unit_nm(),
                           "acsd.catalogue.knots");
  Result<ArrayRef> coefficient_basis =
      context.storage.copy(
          context, DType::f64, model::encode_f64(coefficient_values),
          coefficient_values.size() == expected
              ? std::vector<U64>{basis_functions, segments, basis_degree + 1}
              : std::vector<U64>{static_cast<U64>(coefficient_values.size() /
                                                  std::max<U64>(1, basis_width)),
                                 basis_width},
          model::unit_dimensionless(), "acsd.catalogue.basis");
  if (!ok(knot_array) || !ok(coefficient_basis)) {
    return fail<Ref<CatalogueProvider>>(Code::io_error, "the spectral basis cannot be stored",
                                        "cli.catalogue", "the command cannot run");
  }
  auto spectral = std::make_shared<SpectralBasis>();
  spectral->knots_nm = val(knot_array);
  spectral->basis_coefficients = val(coefficient_basis);
  spectral->degree = static_cast<U32>(u64_field(basis, "degree", 0));
  spectral->coefficient_count = static_cast<U32>(u64_field(basis, "coefficient_count", basis_width));
  spectral->support_nm.lower = double_field(field(basis, "support_nm"), "lower", 0.0);
  spectral->support_nm.upper = double_field(field(basis, "support_nm"), "upper", 0.0);
  spectral->identity = model::make_identity("acsd.SpectralBasis", 2, 0,
                                            model::sha256_hex("acsd.cli.catalogue.basis"), "acsd");
  spectral->assessment = model::make_assessment(Code::ok);

  CatalogueColumns columns;
  columns.source_ids = val(id_array);
  columns.astrometry = val(astrometry_array);
  columns.brightness_mag = val(magnitude_array);
  columns.xp_coefficients = val(coefficient_array);
  columns.xp_basis = spectral;
  columns.identity = model::make_identity("acsd.CatalogueColumns", 2, 0,
                                          model::sha256_hex("acsd.cli.catalogue.columns"), "acsd");
  columns.assessment = model::make_assessment(Code::ok);

  const json& build = document.contains("build") ? document.at("build") : json::object();
  CatalogueBuildRequest request;
  request.columns = std::make_shared<const CatalogueColumns>(columns);
  request.sky_order = static_cast<U32>(u64_field(build, "sky_order", 3));
  request.max_stars_per_cell = static_cast<U32>(u64_field(build, "max_stars_per_cell", 64));
  request.neighbors_per_anchor = static_cast<U32>(u64_field(build, "neighbors_per_anchor", 12));
  request.quads_per_anchor = static_cast<U32>(u64_field(build, "quads_per_anchor", 8));
  request.quad_baseline_rad.lower = double_field(field(build, "quad_baseline_rad"), "lower", 0.0);
  request.quad_baseline_rad.upper = double_field(field(build, "quad_baseline_rad"), "upper", 0.0);
  request.release = string_field(build, "release", "cli");
  request.calibration = string_field(build, "calibration", "none");
  Result<Ref<CatalogueIndex>> index = build_catalogue_index(context, request);
  if (!ok(index)) {
    return fail<Ref<CatalogueProvider>>(index.code, "the catalogue index cannot be built",
                                        "cli.catalogue", "the command cannot run");
  }
  Result<Ref<CatalogueProvider>> provider = make_catalogue_provider(context, val(index));
  if (!ok(provider)) {
    return fail<Ref<CatalogueProvider>>(provider.code, "the catalogue provider cannot be built",
                                        "cli.catalogue", "the command cannot run");
  }
  return ok_result(val(provider));
}

// ---------------------------------------------------------------------------
// export
// ---------------------------------------------------------------------------

int run_export(Config& config, EventWriter& events, CancellationSource& cancellation) {
  // A missing key or a wrong type in the scientific file is a named failure:
  // the command answers invalid_request and writes the event stream.
  try {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(1, 0, 0, 1, 0, json::array()));
    return kInvalidInput;
  }
  const json& projection = val(scientific).request.at("projection");
  const ExportRequest request = decode_export_request(projection);
  if (request.target.width == 0 || request.target.height == 0) {
    events.issue("invalid_request", "the export target is empty");
    events.run_finished(kInvalidInput, make_summary(1, 0, 0, 1, 0, json::array()));
    return kInvalidInput;
  }

  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  PublishRequest publish;
  publish.destination = (config.base / config.output).string();
  publish.signal_dtype = config.signal_dtype == "f32" ? DType::f32 : DType::f64;
  publish.profile = config.product_profile;
  publish.tile_log2_width = config.tile_log2_width;
  publish.durable = config.durable;

  ExportWorkflowRequest workflow;
  workflow.input.directory = (config.base / config.inputs[0].path).string();
  workflow.input.manifest_hash = config.inputs[0].expected_hash;
  workflow.projection = request;
  workflow.output = publish;

  events.frame_started("export", 1);
  Result<ProductLocation> location = export_image(context, workflow);
  if (!ok(location)) {
    const std::string reason = location.assessment.issues.empty()
                                   ? "the export failed"
                                   : location.assessment.issues.front().reason;
    const bool cancelled = location.code == Code::cancelled;
    events.frame_finished("export", code_name(location.code), 0, 1, reason);
    json frames = json::array();
    frames.push_back(frame_status("export", cancelled ? "cancelled" : "failed", reason));
    events.run_finished(exit_code_of(location.code),
                        make_summary(1, 0, 0, cancelled ? 0 : 1, cancelled ? 1 : 0, frames));
    return exit_code_of(location.code);
  }
  events.product_published("export", val(location).directory);
  events.frame_finished("export", "ok", 1, 1, "the export was published");
  json frames = json::array();
  frames.push_back(frame_status("export", "published", "the export was published"));
  events.run_finished(kSuccess, make_summary(1, 1, 0, 0, 0, frames));
  return kSuccess;

  } catch (const json::exception& failure) {
    events.issue("invalid_request",
                 std::string("the scientific file is invalid: ") + failure.what());
    events.run_finished(kInvalidInput,
                        make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                    json::array()));
    return kInvalidInput;
  }}

// ---------------------------------------------------------------------------
// normalize
// ---------------------------------------------------------------------------

int run_normalize(Config& config, EventWriter& events, CancellationSource& cancellation) {
  // A missing key or a wrong type in the scientific file is a named failure:
  // the command answers invalid_request and writes the event stream.
  try {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }
  // The frame options are joined with the configured inputs by the frame
  // identifier; a duplicate or unknown frame is a configuration error.
  std::map<std::string, const json*> options;
  for (const json& item : val(scientific).frame_options) {
    const std::string frame = string_field(item, "frame");
    if (frame.empty()) {
      events.issue("invalid_request", "a frame option has no frame identifier");
      events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                      json::array()));
      return kInvalidInput;
    }
    if (options.count(frame) != 0) {
      events.issue("invalid_request", "the frame " + frame + " has two option sets");
      events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                      json::array()));
      return kInvalidInput;
    }
    options[frame] = &item;
  }
  for (const InputEntry& input : config.inputs) {
    if (options.count(input.frame_id) == 0) continue;
  }

  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  // The catalogue of the run.
  const std::filesystem::path catalogue_path = config.base / config.catalogue;
  Result<Ref<CatalogueProvider>> catalogue = load_catalogue(context, catalogue_path);
  if (!ok(catalogue)) {
    const std::string reason = catalogue.assessment.issues.empty()
                                   ? "the catalogue cannot be read"
                                   : catalogue.assessment.issues.front().reason;
    events.issue("invalid_request", reason);
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }

  // The frames: the image, the calibration, the response and the optional
  // supplied world coordinate solution.
  NormalizeRequest request;
  json frames = json::array();
  U64 cancelled = 0;
  U64 failed = 0;
  for (const InputEntry& input : config.inputs) {
    // The cancellation is checked at the frame boundary of the command.
    if (cancellation.requested()) {
      ++cancelled;
      events.frame_finished(input.frame_id, "cancelled", 0, config.inputs.size(),
                            "the run was cancelled");
      frames.push_back(json{{"frame", input.frame_id}, {"status", "cancelled"},
                            {"code", "cancelled"}});
      continue;
    }
    events.frame_started(input.frame_id, config.inputs.size());
    const std::filesystem::path image_path = config.base / input.path;
    Result<FitsImage> image = read_fits_image(image_path);
    if (!ok(image)) {
      events.frame_finished(input.frame_id, "failed", 0, config.inputs.size(),
                            image.assessment.issues.front().reason);
      frames.push_back(json{{"frame", input.frame_id}, {"status", "failed"},
                            {"code", "invalid_request"}});
      ++failed;
      continue;
    }
    Result<Ref<RawObservation>> raw = build_raw_observation(context, val(image), input.frame_id);
    if (!ok(raw)) {
      events.frame_finished(input.frame_id, "failed", 0, config.inputs.size(),
                            raw.assessment.issues.front().reason);
      frames.push_back(json{{"frame", input.frame_id}, {"status", "failed"},
                            {"code", "invalid_request"}});
      ++failed;
      continue;
    }
    Result<Calibration> calibration =
        load_calibration(context, input.calibration.empty()
                                        ? std::filesystem::path()
                                        : config.base / input.calibration,
                          std::vector<U64>{val(image).width * val(image).height});
    if (!ok(calibration)) {
      events.frame_finished(input.frame_id, "failed", 0, config.inputs.size(),
                            calibration.assessment.issues.front().reason);
      frames.push_back(json{{"frame", input.frame_id}, {"status", "failed"},
                            {"code", "invalid_request"}});
      ++failed;
      continue;
    }
    Result<Ref<Response>> response =
        load_response(context, config.base / input.response, input.frame_id);
    if (!ok(response)) {
      events.frame_finished(input.frame_id, "failed", 0, config.inputs.size(),
                            response.assessment.issues.front().reason);
      frames.push_back(json{{"frame", input.frame_id}, {"status", "failed"},
                            {"code", "invalid_request"}});
      ++failed;
      continue;
    }
    NormalizeFrame frame;
    frame.frame = input.frame_id;
    frame.raw = val(raw);
    frame.calibration = val(calibration);
    frame.response = val(response);
    // The optional supplied world coordinate solution of the frame.
    for (const json& item : val(scientific).frame_options) {
      if (string_field(item, "frame") != input.frame_id) continue;
      if (has(item, "supplied_wcs")) {
        frame.supplied_wcs = decode_wcs(item.at("supplied_wcs"));
      }
    }
    request.frames.push_back(frame);
  }

  // The scientific request of the normalize command.
  const json& scientific_request = val(scientific).request;
  if (has(scientific_request, "astrometry")) {
    request.astrometry = decode_solver_request(scientific_request.at("astrometry"));
  }
  if (has(scientific_request, "psf")) {
    request.psf = decode_psf_request(scientific_request.at("psf"));
  }
  if (has(scientific_request, "drizzle")) {
    request.drizzle = decode_drizzle_request(scientific_request.at("drizzle"));
  }
  if (has(scientific_request, "photometry")) {
    request.photometry = decode_photometry_request(scientific_request.at("photometry"), request.frames);
  }
  request.allow_relative_uncalibrated =
      has(scientific_request, "allow_relative_uncalibrated") &&
      scientific_request.at("allow_relative_uncalibrated").is_boolean() &&
      scientific_request.at("allow_relative_uncalibrated").get<bool>();
  // The output of every frame.
  for (const NormalizeFrame& frame : request.frames) {
    PublishRequest publish;
    publish.destination = (config.base / config.output / frame.frame).string();
    publish.signal_dtype = config.signal_dtype == "f32" ? DType::f32 : DType::f64;
    publish.profile = config.product_profile;
    publish.tile_log2_width = config.tile_log2_width;
    publish.durable = config.durable;
    request.outputs.push_back(publish);
  }
  if (request.frames.size() != request.outputs.size()) {
    events.issue("invalid_request", "the frames and the outputs do not match");
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    frames));
    return kInvalidInput;
  }

  Result<NormalizeResult> result = normalize(context, *val(catalogue), request);
  if (!ok(result)) {
    const std::string reason = result.assessment.issues.empty()
                                   ? "the normalize command failed"
                                   : result.assessment.issues.front().reason;
    events.issue(code_name(result.code), reason);
    events.run_finished(exit_code_of(result.code),
                        make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0, frames));
    return exit_code_of(result.code);
  }

  // The per frame status of the result.
  U64 published = 0;
  U64 unsolved = 0;
  for (const auto& entry : val(result).frames) {
    const std::string code = entry.second.issues.empty()
                                 ? "ok"
                                 : (entry.second.issues.back().code == Code::ok ? "ok" : "failed");
    const bool solved = entry.second.complete && code == "ok";
    if (solved) {
      ++published;
      events.frame_finished(entry.first, "ok", published, config.inputs.size(),
                            "the frame was published");
      frames.push_back(json{{"frame", entry.first}, {"status", "published"}, {"code", "ok"}});
      continue;
    }
    // A cancelled frame is counted as cancelled, not as unsolved or failed.
    const Code status = entry.second.issues.empty() ? Code::unsolved : entry.second.issues.back().code;
    const std::string reason = entry.second.issues.empty()
                                   ? "the frame was not solved"
                                   : entry.second.issues.back().reason;
    if (status == Code::cancelled) {
      ++cancelled;
      events.frame_finished(entry.first, "cancelled", published, config.inputs.size(), reason);
      frames.push_back(json{{"frame", entry.first}, {"status", "cancelled"},
                            {"code", "cancelled"}, {"reason", reason}});
      continue;
    }
    ++unsolved;
    events.frame_finished(entry.first, "failed", published, config.inputs.size(), reason);
    frames.push_back(json{{"frame", entry.first},
                          {"status", status == Code::unsolved ? "unsolved" : "failed"},
                          {"code", status == Code::unsolved ? "unsolved" : "failed"},
                          {"reason", reason}});
  }
  for (const ProductLocation& location : val(result).published) {
    events.product_published("normalize", location.directory);
  }
  // The exit code: the priority is cancellation, then the integrity and the
  // input errors, then the capability, then the partial outcome.
  int code = kSuccess;
  if (cancelled > 0) {
    code = kCancelled;
  } else if (published == 0 && (failed > 0 || unsolved > 0)) {
    code = kFailure;
  } else if (published < config.inputs.size()) {
    code = kPartial;
  }
  events.run_finished(code, make_summary(config.inputs.size(), published, unsolved, failed, cancelled,
                                         frames));
  return code;

  } catch (const json::exception& failure) {
    events.issue("invalid_request",
                 std::string("the scientific file is invalid: ") + failure.what());
    events.run_finished(kInvalidInput,
                        make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                    json::array()));
    return kInvalidInput;
  }}

// ---------------------------------------------------------------------------
// mosaic
// ---------------------------------------------------------------------------

int run_mosaic(Config& config, EventWriter& events, CancellationSource& cancellation) {
  // A missing key or a wrong type in the scientific file is a named failure:
  // the command answers invalid_request and writes the event stream.
  try {
  Result<ScientificFile> scientific = load_scientific(config);
  if (!ok(scientific)) {
    events.issue("invalid_request", scientific.assessment.issues.front().reason);
    events.run_finished(kInvalidInput, make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                                    json::array()));
    return kInvalidInput;
  }
  resources::CpuResources resources(config.resources);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  PublishRequest publish;
  publish.destination = (config.base / config.output).string();
  publish.signal_dtype = config.signal_dtype == "f32" ? DType::f32 : DType::f64;
  publish.profile = config.product_profile;
  publish.tile_log2_width = config.tile_log2_width;
  publish.durable = config.durable;

  MosaicRequest request;
  {
    const json& sci = val(scientific).request;
    if (sci.contains("coaddition") && sci.at("coaddition").is_object()) {
      const json& coadd = sci.at("coaddition");
      if (coadd.contains("target") && coadd.at("target").is_object()) {
        const json& target = coadd.at("target");
        request.coaddition.target.id = string_field(target, "id", "coadd");
        request.coaddition.target.quantity = string_field(target, "quantity", "brightness");
        if (target.contains("unit")) request.coaddition.target.unit = decode_unit(target.at("unit"));
        request.coaddition.target.condition =
            model::make_condition(request.coaddition.target.id, "fixed_geometry");
      }
      request.coaddition.mixed_observation = bool_field(coadd, "mixed_observation", false);
      const std::string weights = string_field(coadd, "weights", "automatic");
      request.coaddition.weights = weights == "equal" ? WeightPolicy::equal
                                    : weights == "blue" ? WeightPolicy::blue
                                    : weights == "nonnegative_qp" ? WeightPolicy::nonnegative_qp
                                                                  : WeightPolicy::automatic;
      if (coadd.contains("rejection") && coadd.at("rejection").is_object()) {
        const json& rejection = coadd.at("rejection");
        const std::string kind = string_field(rejection, "kind", "dq_only");
        request.coaddition.rejection.kind =
            kind == "single_max_loo" ? RejectionPolicy::Kind::single_max_loo
                                     : RejectionPolicy::Kind::dq_only;
        request.coaddition.rejection.alpha = double_field(rejection, "alpha", 0.0);
      }
    }
    if (sci.contains("sky") && sci.at("sky").is_object()) {
      const json& sky = sci.at("sky");
      if (sky.contains("basis") && sky.at("basis").is_object()) {
        const json& basis = sky.at("basis");
        const std::string kind = string_field(basis, "kind", "constant");
        request.sky.basis.kind = kind == "plane" ? Basis::Kind::plane
                                  : kind == "polynomial" ? Basis::Kind::polynomial
                                                         : Basis::Kind::constant;
        request.sky.basis.degree = static_cast<U32>(u64_field(basis, "degree", 0));
        if (basis.contains("center") && basis.at("center").is_array() &&
            basis.at("center").size() == 2) {
          request.sky.basis.center = {basis.at("center")[0].get<double>(),
                                      basis.at("center")[1].get<double>()};
        }
        if (basis.contains("scale") && basis.at("scale").is_array() &&
            basis.at("scale").size() == 2) {
          request.sky.basis.scale = {basis.at("scale")[0].get<double>(),
                                     basis.at("scale")[1].get<double>()};
        }
        if (basis.contains("normalization_domain"))
          request.sky.basis.normalization_domain =
              decode_box(basis.at("normalization_domain"));
      }
    }
  }
  for (const InputEntry& input : config.inputs) {
    ProductLocation location;
    location.directory = (config.base / input.path).string();
    location.manifest_hash = input.expected_hash;
    request.inputs.push_back(location);
  }
  request.output = publish;

  events.frame_started("mosaic", config.inputs.size());
  Result<ProductLocation> location = mosaic(context, request);
  if (!ok(location)) {
    const std::string reason = location.assessment.issues.empty()
                                   ? "the mosaic failed"
                                   : location.assessment.issues.front().reason;
    const bool cancelled = location.code == Code::cancelled;
    events.frame_finished("mosaic", code_name(location.code), 0, config.inputs.size(), reason);
    json frames = json::array();
    frames.push_back(frame_status("mosaic", cancelled ? "cancelled" : "failed", reason));
    events.run_finished(exit_code_of(location.code),
                        make_summary(config.inputs.size(), 0, 0, cancelled ? 0 : config.inputs.size(),
                                     cancelled ? 1 : 0, frames));
    return exit_code_of(location.code);
  }
  events.product_published("mosaic", val(location).directory);
  events.frame_finished("mosaic", "ok", config.inputs.size(), config.inputs.size(),
                        "the mosaic was published");
  json frames = json::array();
  frames.push_back(frame_status("mosaic", "published", "the mosaic was published"));
  events.run_finished(kSuccess, make_summary(config.inputs.size(), 1, 0, 0, 0, frames));
  return kSuccess;

  } catch (const json::exception& failure) {
    events.issue("invalid_request",
                 std::string("the scientific file is invalid: ") + failure.what());
    events.run_finished(kInvalidInput,
                        make_summary(config.inputs.size(), 0, 0, config.inputs.size(), 0,
                                    json::array()));
    return kInvalidInput;
  }}

// ---------------------------------------------------------------------------
// doctor and templates
// ---------------------------------------------------------------------------

int run_doctor(const Config* config, EventWriter& events) {
  json report;
  report["build"] = "acsd 2.0.0";
  report["protocol"] = "acsd.events/2";
  report["config_schema"] = "acsd.config/2";
  report["scientific_schema"] = "acsd.cli-science/2";
  report["product_format"] = "acsd.product/2";
  report["cfitsio"] = "4.6.2";
  report["json"] = "nlohmann_json";
  if (config) {
    report["command"] = config->command;
    report["inputs"] = config->inputs.size();
    report["product_profile"] = config->product_profile;
  }
  // The diagnostics are one JSON object per line: the stream stays JSONL and
  // a reader that parses line by line never fails.
  std::cout << report.dump() << "\n";
  std::cout.flush();
  events.run_finished(kSuccess, make_summary(config ? config->inputs.size() : 0, 0, 0, 0, 0, json::array()));
  return kSuccess;
}

void print_template(const std::string& command) {
  if (command == "normalize") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "normalize",
  "inputs": [
    {"path": "frame.fits", "frame_id": "frame-01", "response": "response.json"}
  ],
  "catalogue": "catalogue.json",
  "scientific": "normalize.scientific.json",
  "output": "products",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full",
  "publication": {"signal_dtype": "f64", "tile_log2_width": 9, "durable": true}
}
)";
    return;
  }
  if (command == "mosaic") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "mosaic",
  "inputs": [
    {"path": "products/frame-01", "expected_hash": "sha256:0000000000000000000000000000000000000000000000000000000000000000"}
  ],
  "scientific": "mosaic.scientific.json",
  "output": "products/mosaic",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full"
}
)";
    return;
  }
  if (command == "export") {
    std::cout << R"({
  "schema": "acsd.config/2",
  "command": "export",
  "inputs": [
    {"path": "products/frame-01", "expected_hash": "sha256:0000000000000000000000000000000000000000000000000000000000000000"}
  ],
  "scientific": "export.scientific.json",
  "output": "products/export",
  "resources": {
    "bytes": 1073741824,
    "work": 10000000,
    "spill_bytes": 0,
    "threads": 4,
    "max_depth": 64,
    "max_precision_bits": 256,
    "scratch_directory": ".work"
  },
  "product_profile": "full"
}
)";
    return;
  }
  std::cerr << "acsd: unknown template " << command << "\n";
}

}  // namespace acsd::cli
