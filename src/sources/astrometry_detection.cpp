// The candidate detection of the blind astrometric solver.
//
// The background is a fixed tile statistic (the median and the scaled median
// absolute deviation of a 64x64 tile) and is used for the detection only; it
// is never published as a science sky.  The candidates are the 5x5 local
// maxima above the SNR threshold, ordered by SNR and (y,x), reduced by a non
// maximum suppression of two pixels and capped per tile.  The position is
// initialised with the weighted centroid of the positive residual and refined
// with a pixel integrated Gaussian.
#include <algorithm>
#include <cmath>
#include <vector>

#include "acsd/contracts.hpp"
#include "astrometry_common.hpp"
#include "numerics/linalg.hpp"
#include "numerics/quadrature.hpp"

namespace acsd::sources {
namespace {

constexpr U64 kTile = 64;
constexpr double kSqrt2 = 1.4142135623730951;

// The DQ bits that disqualify a sample.
constexpr std::uint16_t kUnqualified = 1 | 2 | 4 | 8;

bool sample_usable(std::uint16_t dq) { return (dq & kUnqualified) == 0; }

double median_of(std::vector<double> values) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2;
  if (values.size() % 2 == 1) return values[middle];
  return 0.5 * (values[middle - 1] + values[middle]);
}

// The integral of the normalised Gaussian over the pixel [x-1/2, x+1/2].
// The error function comes from the numerics base, so the pixel integrated
// Gaussian has a single implementation of the error function.
double pixel_integral(double x, double x0, double sigma) {
  const double a = (x + 0.5 - x0) / (sigma * kSqrt2);
  const double b = (x - 0.5 - x0) / (sigma * kSqrt2);
  return 0.5 * (numerics::erf(a) - numerics::erf(b));
}

// The derivatives of the pixel integral with respect to the centre and the
// width.
void pixel_integral_derivatives(double x, double x0, double sigma, double& d_dx0, double& d_dsigma) {
  const double a = (x + 0.5 - x0) / (sigma * kSqrt2);
  const double b = (x - 0.5 - x0) / (sigma * kSqrt2);
  const double factor = 1.0 / (sigma * std::sqrt(2.0 * M_PI));
  d_dx0 = factor * (std::exp(-b * b) - std::exp(-a * a));
  d_dsigma = factor * ((-2.0 * b / sigma) * std::exp(-b * b) - (-2.0 * a / sigma) * std::exp(-a * a));
}

// The pixel integrated Gaussian model and its derivatives with respect to
// (x0, y0, log sigma x, log sigma y, log amplitude).
struct GaussianModel {
  double value = 0.0;
  double dx0 = 0.0;
  double dy0 = 0.0;
  double dlog_sigma_x = 0.0;
  double dlog_sigma_y = 0.0;
  double dlog_amplitude = 0.0;
};

GaussianModel gaussian_model(double x, double y, double x0, double y0, double sigma_x,
                             double sigma_y, double amplitude, double background) {
  GaussianModel model;
  const double px = pixel_integral(x, x0, sigma_x);
  const double py = pixel_integral(y, y0, sigma_y);
  double dpx = 0.0;
  double dpsx = 0.0;
  double dpy = 0.0;
  double dpsy = 0.0;
  pixel_integral_derivatives(x, x0, sigma_x, dpx, dpsx);
  pixel_integral_derivatives(y, y0, sigma_y, dpy, dpsy);
  model.value = amplitude * px * py + background;
  model.dx0 = amplitude * dpx * py;
  model.dy0 = amplitude * px * dpy;
  model.dlog_sigma_x = amplitude * dpsx * sigma_x * py;
  model.dlog_sigma_y = amplitude * px * dpsy * sigma_y;
  model.dlog_amplitude = amplitude * px * py;
  return model;
}

// Refines one candidate with the pixel integrated Gaussian.
void refine_detection(const DetectorImage& image, Detection& detection, double background) {
  constexpr int kWindow = 7;   // the half width of the refinement window
  constexpr int kHalf = kWindow / 2;
  const int centre_x = static_cast<int>(std::lround(detection.sample_x));
  const int centre_y = static_cast<int>(std::lround(detection.sample_y));
  const int width = static_cast<int>(image.width);
  const int height = static_cast<int>(image.height);

  std::vector<double> residuals;
  std::vector<double> jacobian_rows;
  const int columns = 5;
  std::vector<double> design;
  std::vector<double> observed;

  // The positive weighted centroid initialisation.
  double weight_sum = 0.0;
  double centroid_x = 0.0;
  double centroid_y = 0.0;
  double flux = 0.0;
  double second_x = 0.0;
  double second_y = 0.0;
  for (int dy = -kHalf; dy <= kHalf; ++dy) {
    const int y = centre_y + dy;
    if (y < 0 || y >= height) continue;
    for (int dx = -kHalf; dx <= kHalf; ++dx) {
      const int x = centre_x + dx;
      if (x < 0 || x >= width) continue;
      const std::size_t index = static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x);
      if (!sample_usable(image.dq[index])) continue;
      const double value = image.values[index];
      // A zero or negative background pixel never contributes a positive
      // photon weight.
      const double weight = value - background;
      if (!(weight > 0.0)) continue;
      weight_sum += weight;
      centroid_x += weight * (x + 0.0);
      centroid_y += weight * (y + 0.0);
      flux += weight;
      second_x += weight * dx * dx;
      second_y += weight * dy * dy;
    }
  }
  if (!(weight_sum > 0.0)) return;
  centroid_x /= weight_sum;
  centroid_y /= weight_sum;
  double sigma_x = std::sqrt(std::max(second_x / weight_sum, 0.25));
  double sigma_y = std::sqrt(std::max(second_y / weight_sum, 0.25));

  double x0 = centroid_x;
  double y0 = centroid_y;
  double amplitude = flux;

  auto cost = [&](double cx, double cy, double sx, double sy, double a) {
    double total = 0.0;
    for (int dy = -kHalf; dy <= kHalf; ++dy) {
      const int y = centre_y + dy;
      if (y < 0 || y >= height) continue;
      for (int dx = -kHalf; dx <= kHalf; ++dx) {
        const int x = centre_x + dx;
        if (x < 0 || x >= width) continue;
        const std::size_t index =
            static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x);
        if (!sample_usable(image.dq[index])) continue;
        const double model =
            gaussian_model(x + 0.0, y + 0.0, cx, cy, sx, sy, a, background).value;
        const double difference = image.values[index] - model;
        total += difference * difference;
      }
    }
    return total;
  };

  double current = cost(x0, y0, sigma_x, sigma_y, amplitude);
  for (int iteration = 0; iteration < 12; ++iteration) {
    design.clear();
    observed.clear();
    for (int dy = -kHalf; dy <= kHalf; ++dy) {
      const int y = centre_y + dy;
      if (y < 0 || y >= height) continue;
      for (int dx = -kHalf; dx <= kHalf; ++dx) {
        const int x = centre_x + dx;
        if (x < 0 || x >= width) continue;
        const std::size_t index =
            static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x);
        if (!sample_usable(image.dq[index])) continue;
        const GaussianModel model =
            gaussian_model(x + 0.0, y + 0.0, x0, y0, sigma_x, sigma_y, amplitude, background);
        design.push_back(model.dx0);
        design.push_back(model.dy0);
        design.push_back(model.dlog_sigma_x);
        design.push_back(model.dlog_sigma_y);
        design.push_back(model.dlog_amplitude);
        observed.push_back(image.values[index] - model.value);
      }
    }
    if (design.empty()) break;
    numerics::Matrix jacobian(observed.size(), static_cast<std::size_t>(columns));
    for (std::size_t row = 0; row < observed.size(); ++row) {
      for (std::size_t column = 0; column < static_cast<std::size_t>(columns); ++column) {
        jacobian(row, column) = design[row * static_cast<std::size_t>(columns) + column];
      }
    }
    numerics::LeastSquaresResult solution = numerics::least_squares(jacobian, observed, 1e-12);
    if (!solution.converged || solution.solution.size() != static_cast<std::size_t>(columns)) break;

    // A line search on the step length.
    bool improved = false;
    for (int attempt = 0; attempt < 12; ++attempt) {
      const double scale = std::pow(0.5, attempt);
      const double candidate_x0 = x0 + scale * solution.solution[0];
      const double candidate_y0 = y0 + scale * solution.solution[1];
      const double candidate_sx = sigma_x * std::exp(scale * solution.solution[2]);
      const double candidate_sy = sigma_y * std::exp(scale * solution.solution[3]);
      const double candidate_a = amplitude * std::exp(scale * solution.solution[4]);
      if (!(candidate_sx > 0.0) || !(candidate_sy > 0.0) || !(candidate_a > 0.0)) continue;
      const double candidate_cost =
          cost(candidate_x0, candidate_y0, candidate_sx, candidate_sy, candidate_a);
      if (candidate_cost < current) {
        const double change = std::abs(candidate_x0 - x0) + std::abs(candidate_y0 - y0);
        x0 = candidate_x0;
        y0 = candidate_y0;
        sigma_x = candidate_sx;
        sigma_y = candidate_sy;
        amplitude = candidate_a;
        current = candidate_cost;
        improved = true;
        if (change < 1e-4) iteration = 100;
        break;
      }
    }
    if (!improved) break;
  }

  detection.sample_x = x0;
  detection.sample_y = y0;
  detection.flux = amplitude;
  detection.sigma_x = sigma_x;
  detection.sigma_y = sigma_y;
  detection.x = image.detector_from_sample[0] * x0 + image.detector_from_sample[1] * y0 +
                image.offset[0];
  detection.y = image.detector_from_sample[2] * x0 + image.detector_from_sample[3] * y0 +
                image.offset[1];
}

}  // namespace

std::vector<Detection> detect_candidates(const DetectorImage& image, double detect_snr,
                                         U32 max_candidates) {
  std::vector<Detection> detections;
  if (image.width == 0 || image.height == 0) return detections;
  const U64 tiles_x = (image.width + kTile - 1) / kTile;
  const U64 tiles_y = (image.height + kTile - 1) / kTile;

  struct Candidate {
    double x = 0.0;
    double y = 0.0;
    double snr = 0.0;
    double value = 0.0;
  };

  std::vector<Candidate> selected;
  for (U64 tile_y = 0; tile_y < tiles_y; ++tile_y) {
    for (U64 tile_x = 0; tile_x < tiles_x; ++tile_x) {
      const U64 x0 = tile_x * kTile;
      const U64 y0 = tile_y * kTile;
      const U64 x1 = std::min(x0 + kTile, image.width);
      const U64 y1 = std::min(y0 + kTile, image.height);

      // The detection background of the tile: the median and the scaled
      // median absolute deviation of the usable samples.
      std::vector<double> samples;
      samples.reserve(static_cast<std::size_t>((x1 - x0) * (y1 - y0)));
      for (U64 y = y0; y < y1; ++y) {
        for (U64 x = x0; x < x1; ++x) {
          const std::size_t index = static_cast<std::size_t>(y) * image.width + x;
          if (!sample_usable(image.dq[index])) continue;
          samples.push_back(image.values[index]);
        }
      }
      if (samples.size() < 8) continue;
      const double background = median_of(samples);
      std::vector<double> deviations;
      deviations.reserve(samples.size());
      for (double value : samples) deviations.push_back(std::abs(value - background));
      const double sigma = 1.4826 * median_of(deviations);
      if (!(sigma > 0.0)) continue;

      std::vector<Candidate> tile_candidates;
      for (U64 y = y0; y < y1; ++y) {
        for (U64 x = x0; x < x1; ++x) {
          const std::size_t index = static_cast<std::size_t>(y) * image.width + x;
          if (!sample_usable(image.dq[index])) continue;
          const double value = image.values[index];
          if (!std::isfinite(value)) continue;
          const double snr = (value - background) / sigma;
          if (snr < detect_snr) continue;
          // The 5x5 local maximum; the first maximum in raster order wins a
          // plateau.
          bool maximum = true;
          for (int dy = -2; dy <= 2 && maximum; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
              if (dx == 0 && dy == 0) continue;
              const long nx = static_cast<long>(x) + dx;
              const long ny = static_cast<long>(y) + dy;
              if (nx < 0 || ny < 0 || nx >= static_cast<long>(image.width) ||
                  ny >= static_cast<long>(image.height)) {
                continue;
              }
              const std::size_t neighbour =
                  static_cast<std::size_t>(ny) * image.width + static_cast<std::size_t>(nx);
              if (!sample_usable(image.dq[neighbour])) continue;
              const double neighbour_value = image.values[neighbour];
              if (!std::isfinite(neighbour_value)) continue;
              if (neighbour_value > value) {
                maximum = false;
                break;
              }
              if (neighbour_value == value) {
                // The earlier sample in raster order owns the plateau.
                if (dy < 0 || (dy == 0 && dx < 0)) {
                  maximum = false;
                  break;
                }
              }
            }
          }
          if (!maximum) continue;
          Candidate candidate;
          candidate.x = static_cast<double>(x);
          candidate.y = static_cast<double>(y);
          candidate.snr = snr;
          candidate.value = value;
          tile_candidates.push_back(candidate);
        }
      }
      if (tile_candidates.empty()) continue;
      std::sort(tile_candidates.begin(), tile_candidates.end(),
                [](const Candidate& left, const Candidate& right) {
                  if (left.snr != right.snr) return left.snr > right.snr;
                  if (left.y != right.y) return left.y < right.y;
                  return left.x < right.x;
                });

      // The non maximum suppression of two pixels.
      std::vector<Candidate> kept;
      for (const Candidate& candidate : tile_candidates) {
        bool suppressed = false;
        for (const Candidate& previous : kept) {
          const double dx = candidate.x - previous.x;
          const double dy = candidate.y - previous.y;
          if (dx * dx + dy * dy <= 4.0) {
            suppressed = true;
            break;
          }
        }
        if (suppressed) continue;
        kept.push_back(candidate);
        if (kept.size() >= 8) break;
      }
      selected.insert(selected.end(), kept.begin(), kept.end());
    }
  }

  // The global order: the SNR descending and then (y, x) ascending.
  std::sort(selected.begin(), selected.end(), [](const Candidate& left, const Candidate& right) {
    if (left.snr != right.snr) return left.snr > right.snr;
    if (left.y != right.y) return left.y < right.y;
    return left.x < right.x;
  });
  if (max_candidates > 0 && selected.size() > max_candidates) {
    selected.resize(max_candidates);
  }

  detections.reserve(selected.size());
  for (std::size_t index = 0; index < selected.size(); ++index) {
    Detection detection;
    detection.sample_x = selected[index].x;
    detection.sample_y = selected[index].y;
    detection.snr = selected[index].snr;
    detection.x = image.detector_from_sample[0] * detection.sample_x +
                  image.detector_from_sample[1] * detection.sample_y + image.offset[0];
    detection.y = image.detector_from_sample[2] * detection.sample_x +
                  image.detector_from_sample[3] * detection.sample_y + image.offset[1];
    detection.order = index;
    detections.push_back(detection);
  }
  return detections;
}

}  // namespace acsd::sources
