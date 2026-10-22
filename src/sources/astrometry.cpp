// The blind astrometric solver: solve_astrometry.
//
// The solver consumes a detector only observation and the offline catalogue
// and produces the astrometric solution.  The stages are the ones of SYSTEM
// "N2 无 WCS 盲解":
//   * the detection of the candidates on the sample grid with a fixed tile
//     background that is used for the detection only;
//   * the enumeration of the quads in the detection order;
//   * the query of the catalogue index in the descriptor tolerance box and in
//     the baseline range implied by the scale interval;
//   * the coarse affine verification of every candidate against all detected
//     sources and the epoch propagated Gaia sources;
//   * the nonlinear fit of the plane plus SIP with the escalation of the SIP
//     order and the held out validation;
//   * the admission of the model over the complete detector domain.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "astrometry_common.hpp"
#include "model/array.hpp"
#include "model/hash.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "numerics/linalg.hpp"

namespace acsd {
namespace {

using sources::Detection;
using sources::DetectorImage;
using sources::MatchPair;
using sources::WcsParameters;

constexpr double kPi = 3.14159265358979323846;

// The number of neighbours of an anchor used for the quad enumeration.
constexpr std::size_t kQuadNeighbours = 8;
// The work charged for one examined quad.
constexpr U64 kQuadWork = 400;

// The local basis and the gnomonic projection used by the solver.
void local_frame(const Vec3& centre, Vec3& east, Vec3& north) {
  double ra = std::atan2(centre[1], centre[0]);
  double dec = std::asin(std::clamp(centre[2], -1.0, 1.0));
  if (ra < 0.0) ra += 2.0 * kPi;
  east = Vec3{-std::sin(ra), std::cos(ra), 0.0};
  north = Vec3{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
}

double separation(const Vec3& left, const Vec3& right) {
  const double dot = left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
  const Vec3 cross{left[1] * right[2] - left[2] * right[1], left[2] * right[0] - left[0] * right[2],
                   left[0] * right[1] - left[1] * right[0]};
  const double s = std::sqrt(cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
  return std::atan2(s, std::clamp(dot, -1.0, 1.0));
}

// The detector domain of the footprint set.
Box detector_domain(const FootprintSet& footprints) {
  const double width = static_cast<double>(footprints.width);
  const double height = static_cast<double>(footprints.height);
  const Mat2& m = footprints.detector_from_sample;
  const Vec2& offset = footprints.offset;
  const std::array<std::array<double, 2>, 4> corners{
      std::array<double, 2>{-0.5, -0.5}, std::array<double, 2>{width - 0.5, -0.5},
      std::array<double, 2>{-0.5, height - 0.5}, std::array<double, 2>{width - 0.5, height - 0.5}};
  double lower_x = 0.0;
  double lower_y = 0.0;
  double upper_x = 0.0;
  double upper_y = 0.0;
  for (std::size_t k = 0; k < 4; ++k) {
    const double x = m[0] * corners[k][0] + m[1] * corners[k][1] + offset[0];
    const double y = m[2] * corners[k][0] + m[3] * corners[k][1] + offset[1];
    if (k == 0) {
      lower_x = upper_x = x;
      lower_y = upper_y = y;
    } else {
      lower_x = std::min(lower_x, x);
      upper_x = std::max(upper_x, x);
      lower_y = std::min(lower_y, y);
      upper_y = std::max(upper_y, y);
    }
  }
  // The collection cell of a sample enlarges the domain.
  const double cell = std::max(std::abs(footprints.local_cell.lower[0]),
                               std::max(std::abs(footprints.local_cell.upper[0]),
                                        std::max(std::abs(footprints.local_cell.lower[1]),
                                                 std::abs(footprints.local_cell.upper[1]))));
  Box domain;
  domain.lower = Vec2{lower_x - cell, lower_y - cell};
  domain.upper = Vec2{upper_x + cell, upper_y + cell};
  return domain;
}

// Reads the image of the observation.
Result<DetectorImage> read_image(Context& context, const NativeObservation& observation) {
  if (!observation.footprints) {
    return model::fail<DetectorImage>(Code::invalid_model, "the observation has no footprint set",
                                      "solve_astrometry", "the image cannot be read");
  }
  if (!observation.integrated_values || !observation.dq) {
    return model::fail<DetectorImage>(Code::invalid_model, "the observation has no image",
                                      "solve_astrometry", "the image cannot be read");
  }
  const FootprintSet& footprints = *observation.footprints;
  const U64 width = footprints.width;
  const U64 height = footprints.height;
  if (width == 0 || height == 0) {
    return model::fail<DetectorImage>(Code::invalid_model, "the footprint set has no samples",
                                      "solve_astrometry", "the image cannot be read");
  }
  const U64 total = width * height;
  if (observation.integrated_values->shape.size() != 1 ||
      observation.integrated_values->shape[0] != total) {
    return model::fail<DetectorImage>(Code::invalid_model,
                                      "the image does not match the footprint set",
                                      "solve_astrometry", "the image cannot be read");
  }
  Result<ArrayView> values = context.storage.read(context, observation.integrated_values,
                                                   BlockRequest{0, total});
  if (!ok(values)) {
    return model::fail<DetectorImage>(values.code, "the image cannot be read", "solve_astrometry",
                                      "the image cannot be read");
  }
  Result<ArrayView> dq = context.storage.read(context, observation.dq, BlockRequest{0, total});
  if (!ok(dq)) {
    return model::fail<DetectorImage>(dq.code, "the quality flags cannot be read", "solve_astrometry",
                                      "the image cannot be read");
  }
  DetectorImage image;
  image.width = width;
  image.height = height;
  image.values = model::decode_f64(val(values).bytes);
  image.dq.resize(total);
  const std::vector<std::byte> dq_bytes(val(dq).bytes.begin(), val(dq).bytes.end());
  for (U64 i = 0; i < total && 2 * i + 1 < dq_bytes.size(); ++i) {
    image.dq[i] = static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(dq_bytes[2 * i]) |
        (static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(dq_bytes[2 * i + 1])) << 8));
  }
  image.detector_from_sample = footprints.detector_from_sample;
  image.offset = footprints.offset;
  image.domain = detector_domain(footprints);
  return model::ok_result(std::move(image));
}

// The observer of the observation.
struct ObserverState {
  bool barycentric_available = false;
  Vec3 barycentric_au{};
  bool geodetic_available = false;
  Vec3 geodetic{};
};

ObserverState observer_of(const AstrometricContext& context) {
  ObserverState observer;
  if (context.barycentric_observer_au) {
    observer.barycentric_available = true;
    observer.barycentric_au = *context.barycentric_observer_au;
  }
  if (context.geodetic_lon_lat_height_rad_m) {
    observer.geodetic_available = true;
    observer.geodetic = *context.geodetic_lon_lat_height_rad_m;
  }
  return observer;
}

// The topocentric correction of a catalogue direction.
Vec3 topocentric(const Vec3& barycentric, double parallax_rad, const ObserverState& observer,
                 bool& applied) {
  applied = false;
  if (!observer.barycentric_available || !(parallax_rad > 0.0)) return barycentric;
  const double distance = 1.0 / parallax_rad;
  const Vec3 star{distance * barycentric[0], distance * barycentric[1], distance * barycentric[2]};
  const Vec3 relative{star[0] - observer.barycentric_au[0], star[1] - observer.barycentric_au[1],
                      star[2] - observer.barycentric_au[2]};
  const double norm = std::sqrt(relative[0] * relative[0] + relative[1] * relative[1] +
                                relative[2] * relative[2]);
  if (!(norm > 0.0)) return barycentric;
  applied = true;
  return Vec3{relative[0] / norm, relative[1] / norm, relative[2] / norm};
}

// The catalogue stars of a region, paged over the whole region.
Result<std::vector<CatalogueStar>> query_region(Context& context, const CatalogueProvider& catalogue,
                                                 const SkyICRS& centre, double radius,
                                                 double epoch, U32 page_size) {
  std::vector<CatalogueStar> stars;
  CatalogueQuery request;
  request.region.center = centre;
  request.region.radius_rad = radius;
  request.epoch_julian_year = epoch;
  request.page_size = page_size;
  request.cursor = "";
  for (U32 page = 0; page < 64; ++page) {
    Result<CataloguePage> result = catalogue.query(context, request);
    if (!ok(result)) {
      return model::fail<std::vector<CatalogueStar>>(result.code,
                                                     "the catalogue region cannot be read",
                                                     "solve_astrometry", "the field is unknown");
    }
    for (const CatalogueStar& star : val(result).stars) stars.push_back(star);
    if (val(result).next_cursor.empty()) break;
    request.cursor = val(result).next_cursor;
  }
  return model::ok_result(std::move(stars));
}

// Builds a CSR action from a dense matrix.
Result<Ref<LinearAction>> make_dense_action(Context& context, const numerics::Matrix& matrix,
                                            const Ref<Index>& domain, const Ref<Index>& codomain,
                                            const Id& index_id) {
  std::vector<U64> offsets(matrix.rows + 1, 0);
  std::vector<U64> indices;
  std::vector<double> values;
  for (std::size_t row = 0; row < matrix.rows; ++row) {
    for (std::size_t column = 0; column < matrix.cols; ++column) {
      const double value = matrix(row, column);
      if (value == 0.0) continue;
      indices.push_back(column);
      values.push_back(value);
    }
    offsets[row + 1] = indices.size();
  }
  const std::vector<U64> offset_shape{offsets.size()};
  const std::vector<U64> index_shape{indices.size()};
  const std::vector<U64> value_shape{values.size()};
  Result<ArrayRef> offset_array =
      context.storage.copy(context, DType::u64, model::encode_u64(offsets), offset_shape,
                           model::unit_dimensionless(), index_id);
  if (!ok(offset_array)) {
    return model::fail<Ref<LinearAction>>(offset_array.code, "the action cannot be stored",
                                          "solve_astrometry", "the action cannot be built");
  }
  Result<ArrayRef> index_array =
      context.storage.copy(context, DType::u64, model::encode_u64(indices), index_shape,
                           model::unit_dimensionless(), index_id);
  if (!ok(index_array)) {
    return model::fail<Ref<LinearAction>>(index_array.code, "the action cannot be stored",
                                          "solve_astrometry", "the action cannot be built");
  }
  Result<ArrayRef> value_array =
      context.storage.copy(context, DType::f64, model::encode_f64(values), value_shape,
                           model::unit_dimensionless(), index_id);
  if (!ok(value_array)) {
    return model::fail<Ref<LinearAction>>(value_array.code, "the action cannot be stored",
                                          "solve_astrometry", "the action cannot be built");
  }
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::csr;
  recipe.algorithm = "canonical_csr_v1";
  recipe.version = 1;
  recipe.spaces.domain = domain;
  recipe.spaces.codomain = codomain;
  const std::vector<Ref<LinearAction>> children;
  const std::vector<ArrayRef> arrays{val(offset_array), val(index_array), val(value_array)};
  recipe.arrays.clear();
  for (const ArrayRef& array : arrays) recipe.arrays.push_back(array->identity.object_id);
  return make_action(context, recipe, children, arrays);
}

// Builds the parameter covariance and the same data influence of the fit.
struct ParameterNoise {
  Ref<CovarianceAction> covariance;
  Ref<LinearAction> influence;
};

Result<ParameterNoise> build_parameter_noise(Context& context, const sources::FitDiagnostics& diagnostics,
                                             const std::vector<MatchPair>& pairs,
                                             const WcsParameters& parameters,
                                             const Index& parameter_index) {
  const std::size_t columns = diagnostics.jacobian.cols;
  const std::size_t rows = diagnostics.jacobian.rows;
  ParameterNoise noise;
  if (!diagnostics.covariance_valid || columns == 0 || pairs.empty()) {
    return model::fail<ParameterNoise>(Code::not_identifiable,
                                       "the formal covariance of the fit is not available",
                                       "solve_astrometry", "the parameter noise is not published");
  }

  // The Cholesky factor of the formal covariance is the factor of the root.
  numerics::Matrix factor(columns, columns);
  double minimum_diagonal = 0.0;
  if (!numerics::cholesky_factor(diagnostics.covariance, factor, minimum_diagonal)) {
    return model::fail<ParameterNoise>(Code::invalid_covariance,
                                       "the formal covariance is not positive definite",
                                       "solve_astrometry", "the parameter noise is not published");
  }

  const auto parameter_ref = std::make_shared<const Index>(parameter_index);
  Result<Ref<LinearAction>> factor_action =
      make_dense_action(context, factor, parameter_ref, parameter_ref, parameter_index.identity.object_id);
  if (!ok(factor_action)) {
    return model::fail<ParameterNoise>(factor_action.code, "the parameter factor cannot be built",
                                       "solve_astrometry", "the parameter noise is not published");
  }

  // The influence from the root coordinates to the output is the identity.
  ActionRecipe identity_recipe;
  identity_recipe.kind = ActionRecipe::Kind::identity;
  identity_recipe.algorithm = "canonical_identity_v1";
  identity_recipe.version = 1;
  identity_recipe.spaces.domain = parameter_ref;
  identity_recipe.spaces.codomain = parameter_ref;
  Result<Ref<LinearAction>> identity_action =
      make_action(context, identity_recipe, std::span<const Ref<LinearAction>>{},
                  std::span<const ArrayRef>{});
  if (!ok(identity_action)) {
    return model::fail<ParameterNoise>(identity_action.code, "the root influence cannot be built",
                                       "solve_astrometry", "the parameter noise is not published");
  }

  auto model = std::make_shared<NoiseModel>();
  model->identity.object_id = "acsd.NoiseModel:astrometric";
  model->identity.content_hash = "astrometric";
  model->identity.run_id = "acsd";
  model->identity.revision = 1;
  model->identity.schema_id = "acsd.NoiseModel";
  model->identity.schema_major = 2;
  model->identity.schema_minor = 0;
  model->variance_unit = model::unit_variance("astrometric_parameter");
  model->condition = model::make_condition("astrometric_parameter", "formal_covariance");
  model->condition.assumptions.push_back(
      "the covariance is the formal covariance of the fit; the pixel noise model is not available");
  auto root = std::make_shared<NoiseRoot>();
  root->identity.object_id = "acsd.NoiseRoot:astrometric";
  root->identity.content_hash = "astrometric";
  root->identity.run_id = "acsd";
  root->identity.revision = 1;
  root->identity.schema_id = "acsd.NoiseRoot";
  root->identity.schema_major = 2;
  root->identity.schema_minor = 0;
  root->shared_id = SharedId{"acsd", "astrometric_parameters", "fit_v1", "parameters"};
  root->coordinates = parameter_ref;
  root->unit = model->variance_unit;
  root->factor = val(factor_action);
  model->innovations.push_back(root);
  model->influence.push_back(val(identity_action));
  model->state = NoiseState::conditional;

  Result<Ref<CovarianceAction>> covariance = make_covariance(context, model);
  if (!ok(covariance)) {
    return model::fail<ParameterNoise>(covariance.code, "the parameter covariance cannot be built",
                                       "solve_astrometry", "the parameter noise is not published");
  }
  noise.covariance = val(covariance);

  // The same data influence: the derivative of the parameters with respect to
  // the matched pixel positions.
  numerics::Matrix derivative(rows, rows);
  for (std::size_t star = 0; star < pairs.size(); ++star) {
    const MatchPair& pair = pairs[star];
    const Vec3 predicted = sources::wcs_forward(parameters, pair.pixel_x, pair.pixel_y);
    const Vec3& catalogue = pair.sky;
    double ra = std::atan2(catalogue[1], catalogue[0]);
    double dec = std::asin(std::clamp(catalogue[2], -1.0, 1.0));
    const Vec3 east{-std::sin(ra), std::cos(ra), 0.0};
    const Vec3 north{-std::sin(dec) * std::cos(ra), -std::sin(dec) * std::sin(ra), std::cos(dec)};
    const double difference[3] = {predicted[0] - catalogue[0], predicted[1] - catalogue[1],
                                  predicted[2] - catalogue[2]};
    const double denominator = catalogue[0] * predicted[0] + catalogue[1] * predicted[1] +
                               catalogue[2] * predicted[2];
    if (!(std::abs(denominator) > 1e-9)) continue;
    const double delta_ra = (difference[0] * east[0] + difference[1] * east[1] +
                             difference[2] * east[2]) / denominator;
    const double delta_dec = (difference[0] * north[0] + difference[1] * north[1] +
                              difference[2] * north[2]) / denominator;
    double pixel_jacobian[3][2] = {};
    sources::wcs_pixel_jacobian(parameters, pair.pixel_x, pair.pixel_y, pixel_jacobian);
    // The derivative of the two residuals with respect to the predicted
    // direction.
    const double b[2][3] = {{(east[0] - delta_ra * catalogue[0]) / denominator,
                             (east[1] - delta_ra * catalogue[1]) / denominator,
                             (east[2] - delta_ra * catalogue[2]) / denominator},
                            {(north[0] - delta_dec * catalogue[0]) / denominator,
                             (north[1] - delta_dec * catalogue[1]) / denominator,
                             (north[2] - delta_dec * catalogue[2]) / denominator}};
    for (std::size_t r = 0; r < 2; ++r) {
      for (std::size_t c = 0; c < 2; ++c) {
        double sum = 0.0;
        for (std::size_t k = 0; k < 3; ++k) sum += b[r][k] * pixel_jacobian[k][c];
        derivative(2 * star + r, 2 * star + c) = -sum;
      }
    }
  }
  // The chain rule: d theta / d pixels = (J^T J)^-1 J^T (d r / d pixels).
  numerics::Matrix chain(columns, rows);
  for (std::size_t i = 0; i < columns; ++i) {
    for (std::size_t k = 0; k < rows; ++k) {
      double sum = 0.0;
      for (std::size_t j = 0; j < rows; ++j) {
        sum += diagnostics.jacobian(j, i) * derivative(j, k);
      }
      chain(i, k) = sum;
    }
  }
  numerics::Matrix influence(columns, rows);
  for (std::size_t i = 0; i < columns; ++i) {
    for (std::size_t k = 0; k < rows; ++k) {
      double sum = 0.0;
      for (std::size_t j = 0; j < columns; ++j) {
        sum += diagnostics.normal_inverse(i, j) * chain(j, k);
      }
      influence(i, k) = sum;
    }
  }
  const std::vector<Id> pixel_ids = [&] {
    std::vector<Id> ids;
    for (std::size_t k = 0; k < pairs.size(); ++k) {
      ids.push_back("matched_pixel:" + std::to_string(k));
    }
    return ids;
  }();
  const Index pixel_index = model::make_parameter_index(pixel_ids);
  const auto pixel_ref = std::make_shared<const Index>(pixel_index);
  Result<Ref<LinearAction>> influence_action =
      make_dense_action(context, influence, pixel_ref, parameter_ref, parameter_index.identity.object_id);
  if (!ok(influence_action)) {
    return model::fail<ParameterNoise>(influence_action.code, "the influence cannot be built",
                                       "solve_astrometry", "the influence is not published");
  }
  noise.influence = val(influence_action);
  return model::ok_result(std::move(noise));
}

}  // namespace

Result<Ref<AstrometricSolution>> solve_astrometry(Context& context, Ref<NativeObservation> observation,
                                                   const CatalogueProvider& catalogue,
                                                   const SolverRequest& request) {
  if (!observation) {
    return model::fail<Ref<AstrometricSolution>>(Code::invalid_request, "no observation supplied",
                                                 "solve_astrometry", "nothing can be solved");
  }
  if (request.min_matches == 0 || request.max_candidates == 0) {
    return model::fail<Ref<AstrometricSolution>>(Code::invalid_request,
                                                 "the match and the candidate budgets must be positive",
                                                 "solve_astrometry", "nothing can be solved");
  }
  if (!(request.detect_snr > 0.0) || !(request.match_radius_pixel > 0.0)) {
    return model::fail<Ref<AstrometricSolution>>(Code::invalid_request,
                                                 "the detection and the match thresholds must be positive",
                                                 "solve_astrometry", "nothing can be solved");
  }
  if (!(request.scale_rad_pixel.lower > 0.0) || !(request.scale_rad_pixel.upper > 0.0) ||
      request.scale_rad_pixel.upper < request.scale_rad_pixel.lower) {
    return model::fail<Ref<AstrometricSolution>>(Code::invalid_request, "the scale interval is invalid",
                                                 "solve_astrometry", "nothing can be solved");
  }

  Result<DetectorImage> image = read_image(context, *observation);
  if (!ok(image)) {
    return model::fail<Ref<AstrometricSolution>>(image.code, "the image cannot be read",
                                                 "solve_astrometry", "nothing can be solved");
  }
  const DetectorImage& detector = val(image);

  const double epoch = observation->context.epoch_julian_year;
  const ObserverState observer = observer_of(observation->context);

  // The detection.
  const std::vector<Detection> detections =
      sources::detect_candidates(detector, request.detect_snr, request.max_candidates);
  if (detections.size() < request.min_matches) {
    return model::fail<Ref<AstrometricSolution>>(
        Code::unsolved, "the detection produced fewer sources than the required minimum",
        "solve_astrometry", "no astrometric solution is published");
  }

  // The observed quads in the detection order.
  struct ObservedQuad {
    std::array<std::size_t, 4> indices{};
    std::array<std::array<double, 2>, 4> points{};
    std::array<double, 4> descriptor{};
    double baseline_pixel = 0.0;
  };

  // The quad selection of the solver must follow the selection of the index,
  // otherwise the observed quadruple would not be one of the registered ones.
  // The parameters are read from the metadata of the catalogue.
  U32 neighbours_per_anchor = sources::kQuadNeighbours;
  U32 quads_per_anchor = 16;
  {
    Result<Ref<CatalogueMetadata>> described = catalogue.describe(context);
    if (ok(described)) {
      const CatalogueMetadata& metadata = *val(described);
      if (metadata.neighbors_per_anchor >= 3) neighbours_per_anchor = metadata.neighbors_per_anchor;
      if (metadata.quads_per_anchor > 0) quads_per_anchor = metadata.quads_per_anchor;
    }
  }

  std::vector<ObservedQuad> observed_quads;
  for (std::size_t anchor = 0; anchor < detections.size(); ++anchor) {
    // The nearest neighbours of the anchor.
    std::vector<std::pair<double, std::size_t>> neighbours;
    for (std::size_t other = 0; other < detections.size(); ++other) {
      if (other == anchor) continue;
      const double dx = detections[other].x - detections[anchor].x;
      const double dy = detections[other].y - detections[anchor].y;
      neighbours.emplace_back(std::sqrt(dx * dx + dy * dy), other);
    }
    std::sort(neighbours.begin(), neighbours.end(),
              [&](const auto& left, const auto& right) {
                if (left.first != right.first) return left.first < right.first;
                return detections[left.second].order < detections[right.second].order;
              });
    const std::size_t count = std::min(neighbours.size(), static_cast<std::size_t>(neighbours_per_anchor));
    if (count < 3) continue;
    // The candidate quadruples of the anchor, ranked by the smallest triangle
    // area as the index does.
    struct Candidate {
      double area = 0.0;
      std::array<std::size_t, 4> indices{};
    };
    std::vector<Candidate> candidates;
    for (std::size_t i = 0; i < count; ++i) {
      for (std::size_t j = i + 1; j < count; ++j) {
        for (std::size_t k = j + 1; k < count; ++k) {
          Candidate candidate;
          candidate.indices = {anchor, neighbours[i].second, neighbours[j].second, neighbours[k].second};
          double longest = 0.0;
          for (std::size_t m = 0; m < 4; ++m) {
            for (std::size_t n = m + 1; n < 4; ++n) {
              const double dx = detections[candidate.indices[n]].x - detections[candidate.indices[m]].x;
              const double dy = detections[candidate.indices[n]].y - detections[candidate.indices[m]].y;
              longest = std::max(longest, std::sqrt(dx * dx + dy * dy));
            }
          }
          double minimum_area = std::numeric_limits<double>::infinity();
          for (std::size_t m = 0; m < 4; ++m) {
            for (std::size_t n = m + 1; n < 4; ++n) {
              for (std::size_t p = n + 1; p < 4; ++p) {
                const double ax = detections[candidate.indices[n]].x - detections[candidate.indices[m]].x;
                const double ay = detections[candidate.indices[n]].y - detections[candidate.indices[m]].y;
                const double bx = detections[candidate.indices[p]].x - detections[candidate.indices[m]].x;
                const double by = detections[candidate.indices[p]].y - detections[candidate.indices[m]].y;
                minimum_area = std::min(minimum_area, 0.5 * std::abs(ax * by - ay * bx));
              }
            }
          }
          // A collinear configuration is discarded.
          if (!(minimum_area > 1e-12 * longest * longest)) continue;
          candidate.area = minimum_area;
          candidates.push_back(candidate);
        }
      }
    }
    std::sort(candidates.begin(), candidates.end(),
              [&](const Candidate& left, const Candidate& right) {
                if (left.area != right.area) return left.area > right.area;
                return left.indices < right.indices;
              });
    if (candidates.size() > quads_per_anchor) candidates.resize(quads_per_anchor);
    for (const Candidate& candidate : candidates) {
      ObservedQuad quad;
      quad.indices = candidate.indices;
      for (std::size_t m = 0; m < 4; ++m) {
        quad.points[m] = {detections[quad.indices[m]].x, detections[quad.indices[m]].y};
      }
      quad.descriptor = sources::observed_descriptor(quad.points);
      double longest = 0.0;
      for (std::size_t m = 0; m < 4; ++m) {
        for (std::size_t n = m + 1; n < 4; ++n) {
          const double dx = quad.points[n][0] - quad.points[m][0];
          const double dy = quad.points[n][1] - quad.points[m][1];
          longest = std::max(longest, std::sqrt(dx * dx + dy * dy));
        }
      }
      quad.baseline_pixel = longest;
      observed_quads.push_back(quad);
    }
  }

  const double half_diagonal = 0.5 * std::sqrt(
      std::pow(detector.domain.upper[0] - detector.domain.lower[0], 2.0) +
      std::pow(detector.domain.upper[1] - detector.domain.lower[1], 2.0));

  std::vector<std::string> reasons;
  std::string best_reason = "no quad of the detection matched the catalogue";
  double best_residual = std::numeric_limits<double>::infinity();
  U32 verified = 0;
  bool search_exhausted = false;
  U32 examined_quads = 0;

  for (const ObservedQuad& observed : observed_quads) {
    Result<U64> work = context.resources.consume_work(kQuadWork);
    if (!ok(work)) {
      search_exhausted = true;
      break;
    }
    ++examined_quads;

    // The catalogue baseline range implied by the scale interval.
    const double baseline_lower = observed.baseline_pixel * request.scale_rad_pixel.lower;
    const double baseline_upper = observed.baseline_pixel * request.scale_rad_pixel.upper;
    if (!(baseline_upper > 0.0)) continue;

    // The descriptor tolerance box.
    QuadQuery query;
    for (std::size_t k = 0; k < 4; ++k) {
      query.descriptor[k] = Interval{observed.descriptor[k] - request.quad_descriptor_tolerance,
                                     observed.descriptor[k] + request.quad_descriptor_tolerance};
    }
    query.baseline_rad = Interval{baseline_lower, baseline_upper};
    query.regions = request.regions;
    query.page_size = 4096;
    Result<QuadPage> candidates = catalogue.query_quads(context, query);

    for (const QuadMatch& match : val(candidates).matches) {
      // The catalogue positions of the quadruple.
      Result<std::vector<CatalogueStar>> stars =
          catalogue.lookup(context, std::span<const U64>(match.source_ids.data(), 4));
      if (!ok(stars) || val(stars).size() != 4) continue;
      std::array<Vec3, 4> catalogue_sky{};
      for (std::size_t k = 0; k < 4; ++k) {
        const CatalogueStar& star = val(stars)[k];
        catalogue_sky[k] = Vec3{std::cos(star.dec_rad) * std::cos(star.ra_rad),
                                std::cos(star.dec_rad) * std::sin(star.ra_rad),
                                std::sin(star.dec_rad)};
      }
      const Vec3 centre{match.center.nx, match.center.ny, match.center.nz};
      const sources::AffineHypothesis hypothesis =
          sources::fit_affine(catalogue_sky, centre, observed.points);
      if (!hypothesis.valid) continue;
      if (hypothesis.residual_pixel > request.match_radius_pixel) {
        best_residual = std::min(best_residual, hypothesis.residual_pixel);
        continue;
      }

      // The verification against all the detected sources and the epoch
      // propagated Gaia sources.
      const double field_radius = half_diagonal * hypothesis.scale_rad_pixel * 1.2 + 1e-9;
      Result<std::vector<CatalogueStar>> field = query_region(
          context, catalogue, SkyICRS{centre[0], centre[1], centre[2]}, field_radius, epoch, 4096);
      if (!ok(field)) continue;

      Vec3 east{};
      Vec3 north{};
      local_frame(centre, east, north);
      // The affine maps the tangent plane of the quadruple onto the detector:
      // px = m0 + m1 xi + m2 eta and py = m3 + m4 xi + m5 eta.
      const double det00 = hypothesis.matrix[1];
      const double det01 = hypothesis.matrix[2];
      const double det10 = hypothesis.matrix[4];
      const double det11 = hypothesis.matrix[5];
      const double affine_det = det00 * det11 - det01 * det10;
      if (!(std::abs(affine_det) > 1e-30)) continue;
      // The inverse affine maps the detector onto the tangent plane; it is the
      // linear part of the initial model of the WCS.
      const double inverse[4] = {det11 / affine_det, -det01 / affine_det, -det10 / affine_det,
                                 det00 / affine_det};

      struct Predicted {
        Vec3 sky{};
        double pixel_x = 0.0;
        double pixel_y = 0.0;
        U64 source_id = 0;
      };
      std::vector<Predicted> predicted;
      for (const CatalogueStar& star : val(field)) {
        const Vec3 barycentric{std::cos(star.dec_rad) * std::cos(star.ra_rad),
                               std::cos(star.dec_rad) * std::sin(star.ra_rad), std::sin(star.dec_rad)};
        bool applied = false;
        const Vec3 sky = topocentric(barycentric, star.parallax_rad, observer, applied);
        const double denominator = centre[0] * sky[0] + centre[1] * sky[1] + centre[2] * sky[2];
        if (!(std::abs(denominator) > 1e-9)) continue;
        const double xi = (east[0] * sky[0] + east[1] * sky[1] + east[2] * sky[2]) / denominator;
        const double eta = (north[0] * sky[0] + north[1] * sky[1] + north[2] * sky[2]) / denominator;
        Predicted item;
        item.pixel_x = hypothesis.matrix[0] + hypothesis.matrix[1] * xi + hypothesis.matrix[2] * eta;
        item.pixel_y = hypothesis.matrix[3] + hypothesis.matrix[4] * xi + hypothesis.matrix[5] * eta;
        item.sky = sky;
        item.source_id = star.source_id;
        predicted.push_back(item);
      }
      if (predicted.empty()) continue;

      // The mutual nearest neighbours.
      std::vector<std::size_t> detection_match(detections.size(), predicted.size());
      std::vector<std::size_t> predicted_match(predicted.size(), detections.size());
      for (std::size_t d = 0; d < detections.size(); ++d) {
        double best = std::numeric_limits<double>::infinity();
        std::size_t best_index = predicted.size();
        for (std::size_t p = 0; p < predicted.size(); ++p) {
          const double dx = detections[d].x - predicted[p].pixel_x;
          const double dy = detections[d].y - predicted[p].pixel_y;
          const double distance = std::sqrt(dx * dx + dy * dy);
          if (distance < best) {
            best = distance;
            best_index = p;
          }
        }
        if (best <= request.match_radius_pixel) detection_match[d] = best_index;
      }
      for (std::size_t p = 0; p < predicted.size(); ++p) {
        double best = std::numeric_limits<double>::infinity();
        std::size_t best_index = detections.size();
        for (std::size_t d = 0; d < detections.size(); ++d) {
          const double dx = detections[d].x - predicted[p].pixel_x;
          const double dy = detections[d].y - predicted[p].pixel_y;
          const double distance = std::sqrt(dx * dx + dy * dy);
          if (distance < best) {
            best = distance;
            best_index = d;
          }
        }
        if (best <= request.match_radius_pixel) predicted_match[p] = best_index;
      }

      std::vector<MatchPair> pairs;
      for (std::size_t p = 0; p < predicted.size(); ++p) {
        if (predicted_match[p] >= detections.size()) continue;
        const std::size_t d = predicted_match[p];
        if (detection_match[d] != p) continue;
        MatchPair pair;
        pair.pixel_x = detections[d].x;
        pair.pixel_y = detections[d].y;
        pair.sky = predicted[p].sky;
        pair.source_id = predicted[p].source_id;
        pairs.push_back(pair);
      }
      if (pairs.size() < request.min_matches) {
        best_reason = "the candidate matched fewer sources than the required minimum";
        continue;
      }
      ++verified;
      if (request.max_hypotheses > 0 && verified > request.max_hypotheses) {
        search_exhausted = true;
        break;
      }

      // The initial model of the fit.
      WcsParameters initial;
      initial.ra0 = std::atan2(centre[1], centre[0]);
      if (initial.ra0 < 0.0) initial.ra0 += 2.0 * kPi;
      initial.dec0 = std::asin(std::clamp(centre[2], -1.0, 1.0));
      initial.crpix_x = hypothesis.matrix[0] + 1.0;
      initial.crpix_y = hypothesis.matrix[3] + 1.0;
      initial.cd[0] = inverse[0];
      initial.cd[1] = inverse[1];
      initial.cd[2] = inverse[2];
      initial.cd[3] = inverse[3];
      initial.sip_order = 0;

      const U32 iterations = request.budget.max_iterations > 0 ? request.budget.max_iterations : 50;
      const sources::FitOutcome fit =
          sources::fit_wcs(pairs, initial, request.sip_order, iterations);
      if (!fit.converged) {
        reasons.push_back("the fit did not converge: " + fit.reason);
        continue;
      }
      if (fit.rms_rad > best_residual) best_residual = fit.rms_rad;

      // The admission of the model over the complete detector domain.
      const std::vector<HeaderField> header = sources::wcs_header_fields(fit.parameters);
      Result<Ref<WcsModel>> wcs =
          make_wcs(context, header, detector.domain, observation->context,
                   observation->footprints->detector_frame, request.budget);
      if (!ok(wcs)) {
        reasons.push_back("the model was not admitted: " + std::string(wcs.assessment.issues.empty()
                                                                          ? "the domain check failed"
                                                                          : wcs.assessment.issues.front().reason));
        continue;
      }

      // The published solution.
      auto solution = std::make_shared<AstrometricSolution>();
      solution->wcs = val(wcs);
      solution->identity.object_id = "acsd.AstrometricSolution:" + val(wcs)->identity.object_id;
      solution->identity.content_hash = val(wcs)->identity.content_hash;
      solution->identity.run_id = "acsd";
      solution->identity.revision = 1;
      solution->identity.schema_id = "acsd.AstrometricSolution";
      solution->identity.schema_major = 2;
      solution->identity.schema_minor = 0;
      const std::vector<double> parameter_values{
          fit.parameters.crpix_x, fit.parameters.crpix_y, fit.parameters.ra0, fit.parameters.dec0,
          fit.parameters.cd[0],   fit.parameters.cd[1],   fit.parameters.cd[2], fit.parameters.cd[3]};
      std::vector<double> published = parameter_values;
      published.insert(published.end(), fit.parameters.sip_a.begin(), fit.parameters.sip_a.end());
      published.insert(published.end(), fit.parameters.sip_b.begin(), fit.parameters.sip_b.end());
      const std::vector<Id> parameter_ids = [&] {
        std::vector<Id> ids;
        for (std::size_t k = 0; k < published.size(); ++k) {
          ids.push_back("astrometric:" + std::to_string(k));
        }
        return ids;
      }();
      const Index parameter_index = model::make_parameter_index(parameter_ids);
      const std::vector<U64> parameter_shape{published.size()};
      Result<ArrayRef> parameter_array =
          context.storage.copy(context, DType::f64, model::encode_f64(published), parameter_shape,
                               model::unit_dimensionless(), parameter_index.identity.object_id);
      if (!ok(parameter_array)) {
        reasons.push_back("the fitted parameters cannot be stored");
        continue;
      }
      solution->parameters = val(parameter_array);

      // The detected sources.
      std::vector<double> detection_values;
      for (const Detection& detection : detections) {
        detection_values.push_back(detection.x);
        detection_values.push_back(detection.y);
      }
      const std::vector<U64> detection_shape{detections.size(), 2};

      Result<ArrayRef> detection_array =
          context.storage.copy(context, DType::f64, model::encode_f64(detection_values),
                               detection_shape, model::unit_dimensionless(),
                               parameter_index.identity.object_id);
      if (!ok(detection_array)) {
        reasons.push_back("the detected sources cannot be stored");
        continue;
      }
      solution->solver_detections = val(detection_array);

      // The support of the solution.
      double lower_x = detector.domain.lower[0];
      double lower_y = detector.domain.lower[1];
      double upper_x = detector.domain.upper[0];
      double upper_y = detector.domain.upper[1];
      for (const MatchPair& pair : pairs) {
        lower_x = std::min(lower_x, pair.pixel_x);
        lower_y = std::min(lower_y, pair.pixel_y);
        upper_x = std::max(upper_x, pair.pixel_x);
        upper_y = std::max(upper_y, pair.pixel_y);
      }
      solution->support = Box{{lower_x, lower_y}, {upper_x, upper_y}};

      // The parameter noise: the formal covariance of the fit, conditional on
      // the absence of a pixel noise model.
      solution->assessment = model::make_assessment(Code::ok);
      solution->assessment.complete = true;
      solution->assessment.validity = Validity::certified;
      solution->assessment.science = ScienceState::bounded;
      solution->assessment.noise = NoiseState::conditional;
      solution->assessment.condition = model::make_condition("astrometric_parameters", "wcs_fit");
      solution->assessment.condition.assumptions.push_back(
          "the parameter covariance is the formal covariance of the fit; the pixel noise model is not "
          "available so the absolute scale of the covariance is conditional");
      model::set_included(solution->assessment, "plane_fit");
      model::set_included(solution->assessment, "sip_fit_order_" + std::to_string(fit.sip_order));
      model::set_included(solution->assessment, "held_out_validation");
      model::set_omitted(solution->assessment, "pixel_noise_model:the_absolute_parameter_covariance");
      model::set_omitted(solution->assessment, "atmospheric_refraction_and_dcr");
      if (!observer.barycentric_available) {
        model::set_omitted(solution->assessment,
                           "observer_position:the_topocentric_correction_was_not_applied");
      }
      solution->model_bias_rad = std::nullopt;

      // The parameter noise and the same data influence.
      const sources::FitDiagnostics diagnostics = sources::fit_diagnostics(pairs, fit.parameters);
      Result<ParameterNoise> noise =
          build_parameter_noise(context, diagnostics, pairs, fit.parameters, parameter_index);
      if (!ok(noise)) {
      }
      if (ok(noise)) {
        solution->parameter_noise = val(noise).covariance;
        solution->influence.from_measurements = val(noise).influence;
        std::vector<double> linearization;
        for (const MatchPair& pair : pairs) {
          linearization.push_back(pair.pixel_x);
          linearization.push_back(pair.pixel_y);
        }
        const std::vector<U64> linearization_shape{linearization.size()};
        Result<ArrayRef> linearization_array =
            context.storage.copy(context, DType::f64, model::encode_f64(linearization),
                                 linearization_shape, model::unit_dimensionless(),
                                 parameter_index.identity.object_id);
        if (ok(linearization_array)) solution->influence.linearization_point = val(linearization_array);
        solution->influence.condition = model::make_condition("astrometric_parameters", "wcs_fit");
        solution->influence.condition.assumptions.push_back(
            "the influence is the derivative of the optimal condition with respect to the matched "
            "pixel positions");
        solution->influence.roots.push_back("acsd.astrometric_parameters.fit_v1");
      } else {
        reasons.push_back("the parameter noise is not identifiable");
        model::set_omitted(solution->assessment, "parameter_covariance");
      }

      Result<Ref<AstrometricSolution>> out =
          model::ok_result(std::shared_ptr<const AstrometricSolution>(solution));
      out.usage = context.resources.usage();
      return out;
    }
    if (search_exhausted) break;
  }

  std::string reason = best_reason;
  if (search_exhausted) {
    reason = "the search was exhausted at the hypothesis limit";
  } else if (!reasons.empty()) {
    reason = reasons.front();
  }
  Result<Ref<AstrometricSolution>> out = model::fail<Ref<AstrometricSolution>>(
      Code::unsolved, reason, "solve_astrometry",
      "no astrometric solution is published; the exhausted search does not prove that no sky "
      "solution exists");
  out.assessment = model::make_assessment(Code::unsolved);
  out.assessment.complete = false;
  out.assessment.validity = Validity::invalid;
  out.assessment.science = ScienceState::unconverged;
  out.assessment.noise = NoiseState::unknown;
  out.assessment.condition = model::make_condition("astrometric_parameters", "wcs_fit");
  for (const std::string& item : reasons) {
    model::add_issue(out.assessment, Code::unsolved, item, "solve_astrometry",
                     "the candidate was rejected");
  }
  model::add_issue(out.assessment, Code::unsolved, reason, "solve_astrometry",
                   "no astrometric solution is published");
  out.usage = context.resources.usage();
  return out;
}

}  // namespace acsd
