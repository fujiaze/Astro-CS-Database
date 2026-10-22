// The read only catalogue provider.
//
// The provider exposes the offline index through the declared CatalogueProvider
// interface: the metadata, the paged sky query with epoch propagation, the
// paged quad query and the identifier lookup.  A cursor binds the dataset
// hash and the complete request, so a cursor issued for one request cannot be
// replayed on another one.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "acsd/contracts.hpp"
#include "catalogue/catalogue_internal.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "uncertainty/covariance.hpp"

namespace acsd {
namespace {

using catalogue::StarRow;

constexpr U64 kNoNode = ~0ull;
constexpr const char* kCursorPrefix = "acsd.cat.v1";

// The cursor of a paged query.
struct Cursor {
  std::string dataset;
  std::string request;
  U64 offset = 0;
};

std::string encode_cursor(const std::string& dataset, const std::string& request, U64 offset) {
  std::ostringstream stream;
  stream << kCursorPrefix << '|' << dataset << '|' << request << '|' << offset;
  return stream.str();
}

bool decode_cursor(const std::string& text, Cursor& out) {
  if (text.rfind(kCursorPrefix, 0) != 0) return false;
  const std::string body = text.substr(std::string(kCursorPrefix).size() + 1);
  const std::size_t first = body.find('|');
  if (first == std::string::npos) return false;
  const std::size_t second = body.find('|', first + 1);
  if (second == std::string::npos) return false;
  out.dataset = body.substr(0, first);
  out.request = body.substr(first + 1, second - first - 1);
  const std::string tail = body.substr(second + 1);
  if (tail.empty()) return false;
  for (char character : tail) {
    if (character < '0' || character > '9') return false;
  }
  out.offset = std::stoull(tail);
  return true;
}

std::string hex_of(const Id& content_hash) {
  const std::string prefix = "sha256:";
  if (content_hash.rfind(prefix, 0) == 0) return content_hash.substr(prefix.size());
  return content_hash;
}

// The request hash binds the complete request except the cursor itself.
std::string hash_query(const SkySearchRegion& region, double epoch, U32 page_size) {
  model::Sha256 hash;
  hash.field("operation", "catalogue.query");
  hash.field_f64("centre_x", region.center.nx);
  hash.field_f64("centre_y", region.center.ny);
  hash.field_f64("centre_z", region.center.nz);
  hash.field_f64("radius", region.radius_rad);
  hash.field_f64("epoch", epoch);
  hash.field_u64("page_size", page_size);
  return hash.hex();
}

std::string hash_quad_query(const QuadQuery& request) {
  model::Sha256 hash;
  hash.field("operation", "catalogue.query_quads");
  for (std::size_t k = 0; k < 4; ++k) {
    hash.field_f64("descriptor_lower", request.descriptor[k].lower);
    hash.field_f64("descriptor_upper", request.descriptor[k].upper);
  }
  hash.field_f64("baseline_lower", request.baseline_rad.lower);
  hash.field_f64("baseline_upper", request.baseline_rad.upper);
  for (const SkySearchRegion& region : request.regions) {
    hash.field_f64("centre_x", region.center.nx);
    hash.field_f64("centre_y", region.center.ny);
    hash.field_f64("centre_z", region.center.nz);
    hash.field_f64("radius", region.radius_rad);
  }
  hash.field_u64("page_size", request.page_size);
  return hash.hex();
}

bool interval_valid(const Interval& value) {
  return std::isfinite(value.lower) && std::isfinite(value.upper) && value.lower <= value.upper;
}

class CatalogueProviderImpl final : public CatalogueProvider {
 public:
  CatalogueProviderImpl(Context& context, Ref<CatalogueIndex> index,
                        std::vector<StarRow> stars)
      : index_(std::move(index)), stars_(std::move(stars)) {
    dataset_hash_ = hex_of(index_->identity.content_hash);
    for (std::size_t i = 0; i < stars_.size(); ++i) {
      by_id_[stars_[i].source_id] = i;
    }
    load_index_arrays(context);
    // The noise of the catalogue is exposed as the covariance action built by
    // the single public constructor; a missing noise model stays null, which
    // means unknown and not zero.
    if (index_->columns->astrometric_noise) {
      Result<Ref<CovarianceAction>> covariance =
          make_covariance(context, index_->columns->astrometric_noise);
      if (ok(covariance)) astrometric_noise_ = val(covariance);
    }
    if (index_->columns->xp_noise) {
      Result<Ref<CovarianceAction>> covariance =
          make_covariance(context, index_->columns->xp_noise);
      if (ok(covariance)) xp_noise_ = val(covariance);
    }
  }

  Result<Ref<CatalogueMetadata>> describe(Context& context) const override {
    (void)context;
    return model::ok_result(std::make_shared<const CatalogueMetadata>(index_->metadata));
  }

  Result<CataloguePage> query(Context& context, const CatalogueQuery& request) const override;

  Result<QuadPage> query_quads(Context& context, const QuadQuery& request) const override;

  Result<std::vector<CatalogueStar>> lookup(Context& context,
                                             std::span<const U64> source_ids) const override;

  Id dataset_hash() const override { return dataset_hash_; }

 private:
  void load_index_arrays(Context& context);
  Result<U64> resolve_cursor(const std::string& cursor, const std::string& request_hash,
                              std::string& reason) const;
  CatalogueStar make_star(Context& context, const StarRow& row, double epoch,
                          double parallax_rad) const;

  static U64 element_count(const ArrayRef& array) {
    if (!array) return 0;
    U64 total = 1;
    for (U64 extent : array->shape) total *= extent;
    return total;
  }
  U64 quad_source_ids_size() const { return element_count(index_->quad_source_ids); }
  U64 quad_descriptors_size() const { return element_count(index_->quad_descriptors); }
  U64 quad_baselines_size() const { return element_count(index_->quad_baselines); }
  U64 quad_centers_size() const { return element_count(index_->quad_centers); }
  U64 kd_size() const { return element_count(index_->kd_left); }

  Ref<CatalogueIndex> index_;
  std::vector<StarRow> stars_;
  std::unordered_map<U64, std::size_t> by_id_;
  std::string dataset_hash_;
  Ref<CovarianceAction> astrometric_noise_;
  Ref<CovarianceAction> xp_noise_;

  // The exported index arrays, decoded once.
  std::vector<std::array<U64, 4>> quad_source_ids_;
  std::vector<std::array<double, 4>> quad_descriptors_;
  std::vector<double> quad_baselines_;
  std::vector<Vec3> quad_centers_;
  std::vector<U64> kd_left_;
  std::vector<U64> kd_right_;
  std::vector<std::uint8_t> kd_axis_;
  std::vector<double> kd_split_;
  std::vector<std::array<double, 8>> kd_bounds_;  // four (lower, upper) pairs
  U64 kd_root_ = kNoNode;
};

void CatalogueProviderImpl::load_index_arrays(Context& context) {
  const auto read = [&](const ArrayRef& array, U64 count, DType) {
    Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, count});
    if (!ok(view)) return std::vector<std::byte>{};
    return std::vector<std::byte>(val(view).bytes.begin(), val(view).bytes.end());
  };

  const std::vector<std::byte> source_bytes =
      read(index_->quad_source_ids, quad_source_ids_size(), DType::u64);
  const std::vector<U64> source_values = model::decode_u64(source_bytes);
  for (std::size_t i = 0; i + 3 < source_values.size(); i += 4) {
    quad_source_ids_.push_back({source_values[i], source_values[i + 1], source_values[i + 2],
                                source_values[i + 3]});
  }

  const std::vector<std::byte> descriptor_bytes =
      read(index_->quad_descriptors, quad_descriptors_size(), DType::f64);
  const std::vector<double> descriptor_values = model::decode_f64(descriptor_bytes);
  for (std::size_t i = 0; i + 3 < descriptor_values.size(); i += 4) {
    quad_descriptors_.push_back({descriptor_values[i], descriptor_values[i + 1],
                                 descriptor_values[i + 2], descriptor_values[i + 3]});
  }

  const std::vector<std::byte> baseline_bytes =
      read(index_->quad_baselines, quad_baselines_size(), DType::f64);
  quad_baselines_ = model::decode_f64(baseline_bytes);

  const std::vector<std::byte> center_bytes = read(index_->quad_centers, quad_centers_size(), DType::f64);
  const std::vector<double> center_values = model::decode_f64(center_bytes);
  for (std::size_t i = 0; i + 2 < center_values.size(); i += 3) {
    quad_centers_.push_back(Vec3{center_values[i], center_values[i + 1], center_values[i + 2]});
  }

  const std::vector<std::byte> left_bytes = read(index_->kd_left, kd_size(), DType::u64);
  kd_left_ = model::decode_u64(left_bytes);
  const std::vector<std::byte> right_bytes = read(index_->kd_right, kd_size(), DType::u64);
  kd_right_ = model::decode_u64(right_bytes);
  const std::vector<std::byte> axis_bytes = read(index_->kd_axis, kd_size(), DType::u8);
  for (std::byte value : axis_bytes) {
    kd_axis_.push_back(static_cast<std::uint8_t>(value));
  }
  const std::vector<std::byte> split_bytes = read(index_->kd_split, kd_size(), DType::f64);
  kd_split_ = model::decode_f64(split_bytes);
  const std::vector<std::byte> bounds_bytes = read(index_->kd_bounds, kd_size() * 8, DType::f64);
  const std::vector<double> bounds_values = model::decode_f64(bounds_bytes);
  for (std::size_t i = 0; i + 7 < bounds_values.size(); i += 8) {
    kd_bounds_.push_back({bounds_values[i], bounds_values[i + 1], bounds_values[i + 2],
                          bounds_values[i + 3], bounds_values[i + 4], bounds_values[i + 5],
                          bounds_values[i + 6], bounds_values[i + 7]});
  }

  // The root of the tree is the node that no other node points at.
  if (!kd_left_.empty()) {
    std::vector<bool> is_child(kd_left_.size(), false);
    for (std::size_t i = 0; i < kd_left_.size(); ++i) {
      if (kd_left_[i] < kd_left_.size()) is_child[kd_left_[i]] = true;
      if (kd_right_[i] < kd_right_.size()) is_child[kd_right_[i]] = true;
    }
    for (std::size_t i = 0; i < is_child.size(); ++i) {
      if (!is_child[i]) {
        kd_root_ = static_cast<U64>(i);
        break;
      }
    }
  }
}

Result<U64> CatalogueProviderImpl::resolve_cursor(const std::string& cursor,
                                                   const std::string& request_hash,
                                                   std::string& reason) const {
  if (cursor.empty()) return model::ok_result(U64(0));
  Cursor decoded;
  if (!decode_cursor(cursor, decoded)) {
    reason = "the cursor is not a valid catalogue cursor";
    return model::fail<U64>(Code::invalid_request, reason, "catalogue.provider",
                             "the page cannot be resolved");
  }
  if (decoded.dataset != dataset_hash_) {
    reason = "the cursor belongs to a different catalogue dataset";
    return model::fail<U64>(Code::invalid_request, reason, "catalogue.provider",
                             "the page cannot be resolved");
  }
  if (decoded.request != request_hash) {
    reason = "the cursor was issued for a different request";
    return model::fail<U64>(Code::invalid_request, reason, "catalogue.provider",
                             "the page cannot be resolved");
  }
  return model::ok_result(decoded.offset);
}

CatalogueStar CatalogueProviderImpl::make_star(Context& context, const StarRow& row, double epoch,
                                               double parallax_rad) const {
  CatalogueStar star;
  star.source_id = row.source_id;
  const catalogue::PropagatedPosition propagated = catalogue::propagate_star(row, epoch);
  star.ra_rad = propagated.ra_rad;
  star.dec_rad = propagated.dec_rad;
  star.epoch_year = epoch;
  star.pmra_cosdec_rad_year = row.pmra_cosdec_rad_year;
  star.pmdec_rad_year = row.pmdec_rad_year;
  star.parallax_rad = parallax_rad;
  if (row.rv_present) star.radial_velocity_m_s = row.rv_m_s;
  star.brightness_mag = row.brightness_mag;
  star.xp_basis = index_->columns->xp_basis;
  star.astrometric_noise = astrometric_noise_;
  star.xp_noise = xp_noise_;

  // The spectral coefficients of this source are exported as their own array.
  const U64 coefficients = catalogue::spectral_coefficient_count(*index_->columns);
  if (coefficients > 0 && index_->columns->xp_coefficients) {
    const ArrayRef& source = index_->columns->xp_coefficients;
    const std::size_t row_index = static_cast<std::size_t>(by_id_.at(row.source_id));
    Result<ArrayView> view = context.storage.read(
        context, source, BlockRequest{row_index * coefficients, coefficients});
    if (ok(view)) {
      const std::vector<std::byte> bytes(val(view).bytes.begin(), val(view).bytes.end());
      const std::vector<U64> shape{coefficients};
      Result<ArrayRef> copy = context.storage.copy(context, DType::f64, bytes, shape, source->unit,
                                                    source->index_id);
      if (ok(copy)) star.xp_coefficients = val(copy);
    }
  }
  return star;
}

Result<CataloguePage> CatalogueProviderImpl::query(Context& context,
                                                    const CatalogueQuery& request) const {
  if (request.page_size == 0 || request.page_size > catalogue::kMaxPageSize) {
    return model::fail<CataloguePage>(Code::invalid_request,
                                      "the page size must be between 1 and 4096",
                                      "catalogue.query", "the page cannot be produced");
  }
  if (!(request.region.radius_rad > 0.0) || !std::isfinite(request.region.radius_rad)) {
    return model::fail<CataloguePage>(Code::invalid_request, "the search radius must be positive",
                                      "catalogue.query", "the page cannot be produced");
  }
  if (!std::isfinite(request.epoch_julian_year)) {
    return model::fail<CataloguePage>(Code::invalid_request, "the epoch must be finite",
                                      "catalogue.query", "the page cannot be produced");
  }
  const double norm = std::sqrt(request.region.center.nx * request.region.center.nx +
                                request.region.center.ny * request.region.center.ny +
                                request.region.center.nz * request.region.center.nz);
  if (!(norm > 0.0)) {
    return model::fail<CataloguePage>(Code::invalid_request, "the search centre is not a direction",
                                      "catalogue.query", "the page cannot be produced");
  }
  const Vec3 centre{request.region.center.nx / norm, request.region.center.ny / norm,
                    request.region.center.nz / norm};

  const std::string request_hash = hash_query(request.region, request.epoch_julian_year,
                                               request.page_size);
  std::string reason;
  Result<U64> offset = resolve_cursor(request.cursor, request_hash, reason);
  if (!ok(offset)) {
    return model::fail<CataloguePage>(offset.code, reason, "catalogue.query",
                                      "the page cannot be produced");
  }

  // The members of the region at the requested epoch, ordered by identifier.
  std::vector<std::size_t> members;
  for (std::size_t i = 0; i < stars_.size(); ++i) {
    const catalogue::PropagatedPosition propagated =
        catalogue::propagate_star(stars_[i], request.epoch_julian_year);
    if (catalogue::angular_separation(propagated.unit, centre) <= request.region.radius_rad) {
      members.push_back(i);
    }
  }
  std::sort(members.begin(), members.end(), [&](std::size_t left, std::size_t right) {
    return stars_[left].source_id < stars_[right].source_id;
  });

  CataloguePage page;
  const U64 start = val(offset);
  for (U64 i = start; i < members.size() && page.stars.size() < request.page_size; ++i) {
    const StarRow& row = stars_[members[i]];
    const catalogue::PropagatedPosition propagated =
        catalogue::propagate_star(row, request.epoch_julian_year);
    page.stars.push_back(make_star(context, row, request.epoch_julian_year, propagated.parallax_rad));
  }
  const U64 next = start + page.stars.size();
  if (next < members.size()) {
    page.next_cursor = encode_cursor(dataset_hash_, request_hash, next);
  }
  Result<CataloguePage> out = model::ok_result(std::move(page));
  // The catalogue carries no observer: the published positions are
  // barycentric and the topocentric correction is omitted.  The omission is
  // published with the page; its bound is the parallax of the sources and it
  // is not finite when a source has no known distance.
  out.assessment = model::make_assessment(Code::ok);
  out.assessment.complete = true;
  out.assessment.omitted.push_back("topocentric_correction");
  out.assessment.included.push_back("barycentric_position");
  double bound = 0.0;
  bool unbounded = false;
  for (std::size_t i = start; i < members.size() && i - start < page.stars.size(); ++i) {
    const std::optional<double> source_bound =
        catalogue::observer_omission_bound_rad(stars_[members[i]]);
    if (!source_bound.has_value()) {
      // The omitted correction is unbounded: the result carries no bound and
      // the error state says that the noise is not known.  An omitted error
      // is never published as zero.
      unbounded = true;
      break;
    }
    bound = std::max(bound, *source_bound);
  }
  if (unbounded) {
    out.assessment.noise = NoiseState::unknown;
    out.assessment.omitted_position_bound_rad = std::nullopt;
  } else {
    out.assessment.omitted_position_bound_rad = bound;
  }
  out.usage = context.resources.usage();
  return out;
}

Result<QuadPage> CatalogueProviderImpl::query_quads(Context& context,
                                                      const QuadQuery& request) const {
  if (request.page_size == 0 || request.page_size > catalogue::kMaxPageSize) {
    return model::fail<QuadPage>(Code::invalid_request,
                                  "the page size must be between 1 and 4096", "catalogue.query_quads",
                                  "the page cannot be produced");
  }
  for (const Interval& component : request.descriptor) {
    if (!interval_valid(component)) {
      return model::fail<QuadPage>(Code::invalid_request, "the descriptor interval is invalid",
                                    "catalogue.query_quads", "the page cannot be produced");
    }
  }
  if (!interval_valid(request.baseline_rad)) {
    return model::fail<QuadPage>(Code::invalid_request, "the baseline interval is invalid",
                                  "catalogue.query_quads", "the page cannot be produced");
  }
  std::vector<Vec3> region_centres;
  for (const SkySearchRegion& region : request.regions) {
    if (!(region.radius_rad > 0.0) || !std::isfinite(region.radius_rad)) {
      return model::fail<QuadPage>(Code::invalid_request, "the search radius must be positive",
                                    "catalogue.query_quads", "the page cannot be produced");
    }
    const double norm = std::sqrt(region.center.nx * region.center.nx +
                                  region.center.ny * region.center.ny +
                                  region.center.nz * region.center.nz);
    if (!(norm > 0.0)) {
      return model::fail<QuadPage>(Code::invalid_request, "the search centre is not a direction",
                                    "catalogue.query_quads", "the page cannot be produced");
    }
    region_centres.push_back(
        Vec3{region.center.nx / norm, region.center.ny / norm, region.center.nz / norm});
  }

  const std::string request_hash = hash_quad_query(request);
  std::string reason;
  Result<U64> offset = resolve_cursor(request.cursor, request_hash, reason);
  if (!ok(offset)) {
    return model::fail<QuadPage>(offset.code, reason, "catalogue.query_quads",
                                  "the page cannot be produced");
  }

  // The KD tree box query.
  const auto inside_box = [&](U64 row) {
    const auto& descriptor = quad_descriptors_[row];
    for (std::size_t k = 0; k < 4; ++k) {
      if (descriptor[k] < request.descriptor[k].lower || descriptor[k] > request.descriptor[k].upper) {
        return false;
      }
    }
    return true;
  };
  const auto box_overlaps = [&](U64 node) {
    if (node == kNoNode || node >= kd_bounds_.size()) return false;
    const auto& bounds = kd_bounds_[node];
    for (std::size_t k = 0; k < 4; ++k) {
      const double lower = bounds[2 * k];
      const double upper = bounds[2 * k + 1];
      if (upper < request.descriptor[k].lower || lower > request.descriptor[k].upper) return false;
    }
    return true;
  };

  std::vector<U64> rows;
  if (!quad_descriptors_.empty() && kd_root_ != kNoNode) {
    std::vector<U64> stack{kd_root_};
    while (!stack.empty()) {
      const U64 node = stack.back();
      stack.pop_back();
      if (node == kNoNode || node >= kd_left_.size()) continue;
      if (inside_box(node)) rows.push_back(node);
      if (box_overlaps(kd_left_[node])) stack.push_back(kd_left_[node]);
      if (box_overlaps(kd_right_[node])) stack.push_back(kd_right_[node]);
    }
  }

  // The baseline and the region filters, then the source tuple order.
  std::vector<U64> selected;
  selected.reserve(rows.size());
  for (U64 row : rows) {
    const double baseline = quad_baselines_[row];
    if (baseline < request.baseline_rad.lower || baseline > request.baseline_rad.upper) continue;
    if (!region_centres.empty()) {
      bool inside = false;
      for (std::size_t k = 0; k < region_centres.size() && !inside; ++k) {
        if (catalogue::angular_separation(quad_centers_[row], region_centres[k]) <=
            request.regions[k].radius_rad) {
          inside = true;
        }
      }
      if (!inside) continue;
    }
    selected.push_back(row);
  }
  std::sort(selected.begin(), selected.end(), [&](U64 left, U64 right) {
    std::array<U64, 4> left_ids = quad_source_ids_[left];
    std::array<U64, 4> right_ids = quad_source_ids_[right];
    std::sort(left_ids.begin(), left_ids.end());
    std::sort(right_ids.begin(), right_ids.end());
    if (left_ids != right_ids) return left_ids < right_ids;
    return left < right;
  });

  QuadPage page;
  const U64 start = val(offset);
  for (U64 i = start; i < selected.size() && page.matches.size() < request.page_size; ++i) {
    const U64 row = selected[i];
    QuadMatch match;
    match.source_ids = quad_source_ids_[row];
    match.descriptor = quad_descriptors_[row];
    match.baseline_rad = quad_baselines_[row];
    const Vec3& centre = quad_centers_[row];
    match.center = SkyICRS{centre[0], centre[1], centre[2]};
    page.matches.push_back(match);
  }
  const U64 next = start + page.matches.size();
  if (next < selected.size()) {
    page.next_cursor = encode_cursor(dataset_hash_, request_hash, next);
  }
  Result<QuadPage> out = model::ok_result(std::move(page));
  out.usage = context.resources.usage();
  return out;
}

Result<std::vector<CatalogueStar>> CatalogueProviderImpl::lookup(
    Context& context, std::span<const U64> source_ids) const {
  std::vector<CatalogueStar> stars;
  stars.reserve(source_ids.size());
  Assessment assessment = model::make_assessment(Code::ok);
  assessment.complete = true;
  // The catalogue carries no observer: the published positions are
  // barycentric and the topocentric correction is omitted.  The omission is
  // published with the result; its bound is the parallax of the sources and
  // it is not finite when a source has no known distance.
  assessment.omitted.push_back("topocentric_correction");
  assessment.included.push_back("barycentric_position");
  double bound = 0.0;
  bool unbounded = false;
  for (U64 identifier : source_ids) {
    const auto found = by_id_.find(identifier);
    if (found == by_id_.end()) {
      model::add_issue(assessment, Code::no_support,
                       "the source identifier is not present in the catalogue",
                       "catalogue.lookup", "the identifier is missing from the result");
      continue;
    }
    const StarRow& row = stars_[found->second];
    const std::optional<double> source_bound = catalogue::observer_omission_bound_rad(row);
    if (!source_bound.has_value()) {
      // The omitted correction is unbounded: the result carries no bound and
      // the error state says that the noise is not known.  An omitted error
      // is never published as zero.
      unbounded = true;
    } else {
      bound = std::max(bound, *source_bound);
    }
    const catalogue::PropagatedPosition propagated =
        catalogue::propagate_star(row, row.epoch_year);
    stars.push_back(make_star(context, row, row.epoch_year, propagated.parallax_rad));
  }
  if (unbounded) {
    assessment.noise = NoiseState::unknown;
    assessment.omitted_position_bound_rad = std::nullopt;
  } else {
    assessment.omitted_position_bound_rad = bound;
  }
  Result<std::vector<CatalogueStar>> out = model::ok_result(std::move(stars));
  out.assessment = assessment;
  out.usage = context.resources.usage();
  return out;
}

}  // namespace

Result<Ref<CatalogueProvider>> make_catalogue_provider(Context& context, Ref<CatalogueIndex> index) {
  if (!index) {
    return model::fail<Ref<CatalogueProvider>>(Code::invalid_request, "no catalogue index supplied",
                                               "make_catalogue_provider",
                                               "the provider cannot be created");
  }
  if (!index->columns) {
    return model::fail<Ref<CatalogueProvider>>(Code::invalid_model,
                                               "the catalogue index carries no columns",
                                               "make_catalogue_provider",
                                               "the provider cannot be created");
  }
  std::string reason;
  Result<std::vector<StarRow>> stars = catalogue::parse_columns(context, *index->columns, reason);
  if (!ok(stars)) {
    return model::fail<Ref<CatalogueProvider>>(stars.code, reason, "make_catalogue_provider",
                                               "the provider cannot be created");
  }
  auto provider = std::make_shared<CatalogueProviderImpl>(context, index, val(stars));
  Result<Ref<CatalogueProvider>> out =
      model::ok_result(std::shared_ptr<const CatalogueProvider>(provider));
  out.usage = context.resources.usage();
  return out;
}

}  // namespace acsd
