// Independent validation of the conservative leave one out rejection of the
// coadd module.
//
// The case follows the frozen truth of W01 (validation/w01/truth_w01.py and
// validation/w01/truth/w01_truth.json): the same external Gamma families, the
// same alpha, the same decision rule and the same statistics.  The random
// stream is independent, so the rates are compared with the exact binomial
// interval of the frozen reference instead of with the same samples.
//
//   * the clean family must not reject a frame more often than the reference
//     Monte-Carlo does (the rate has to lie inside the reference interval);
//   * the coverage of the common mean at 68% and at 95% must agree with the
//     reference within one percentage point;
//   * N < 4 must never reject a frame;
//   * a frame that is shifted by twelve sigma must be rejected for N >= 4.

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"

#include "harness.hpp"

using namespace acsd_validation;

namespace {

int failures = 0;

void check(bool condition, const std::string& what) {
  std::printf("%s  %s\n", condition ? "[ ok ]" : "[FAIL]", what.c_str());
  if (!condition) ++failures;
}

// The frozen external Gamma families of W01.
std::vector<std::vector<double>> gamma_family(int n) {
  if (n == 1) return {{4.0}};
  if (n == 2) return {{1.0, 2.0}, {2.0, 9.0}};
  std::vector<std::vector<double>> gamma(static_cast<std::size_t>(n),
                                         std::vector<double>(static_cast<std::size_t>(n), 0.5));
  for (int i = 0; i < n; ++i) gamma[static_cast<std::size_t>(i)][static_cast<std::size_t>(i)] = 1.0;
  return gamma;
}

// The quantile of the standard normal distribution, by the bisection on the
// error function of the reference.
double normal_quantile(double probability) {
  auto erf_approx = [](double x) {
    // The Abramowitz and Stegun 7.1.26 approximation.
    const double sign = x < 0.0 ? -1.0 : 1.0;
    const double t = 1.0 / (1.0 + 0.3275911 * std::fabs(x));
    const double y = 1.0 - (((((1.061405429 * t - 1.453152027) * t) + 1.421413741) * t -
                              0.284496736) * t + 0.254829592) *
                                 t * std::exp(-x * x);
    return sign * y;
  };
  double low = -40.0;
  double high = 40.0;
  for (int step = 0; step < 200; ++step) {
    const double middle = 0.5 * (low + high);
    const double value = 0.5 * (1.0 + erf_approx(middle / std::sqrt(2.0)));
    if (value < probability) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return 0.5 * (low + high);
}

// The Cholesky factor of a positive definite matrix.
std::vector<std::vector<double>> cholesky(const std::vector<std::vector<double>>& matrix) {
  const std::size_t n = matrix.size();
  std::vector<std::vector<double>> lower(n, std::vector<double>(n, 0.0));
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      double sum = matrix[i][j];
      for (std::size_t k = 0; k < j; ++k) sum -= lower[i][k] * lower[j][k];
      if (i == j) {
        lower[i][j] = std::sqrt(sum);
      } else {
        lower[i][j] = sum / lower[j][j];
      }
    }
  }
  return lower;
}

}  // namespace

int main() {
  CountingResources resources;
  NoCancellation cancellation;
  MemoryStorage storage;
  acsd::Context context{resources, cancellation, storage};

  const double alpha = 0.001;
  const double mu = 3.0;

  // The frozen reference rates of validation/w01/truth/w01_truth.json.
  struct Reference {
    int n;
    bool contaminated;
    double false_rejection_rate;
    double coverage_68;
    double coverage_95;
  };
  const std::vector<Reference> references{
      {1, false, 0.0, 0.68185, 0.95016}, {1, true, 0.0, 0.0, 0.0},
      {2, false, 0.0, 0.68152, 0.95002}, {2, true, 0.0, 0.0, 0.0},
      {3, false, 0.0, 0.68479, 0.95047}, {3, true, 0.0, 0.00007, 0.00156},
      {4, false, 0.00089, 0.68215, 0.94875}, {4, true, 1.0, 0.68186, 0.94964},
      {5, false, 0.00096, 0.68358, 0.94950}, {5, true, 1.0, 0.68519, 0.95075},
      {6, false, 0.00101, 0.68381, 0.94983}, {6, true, 1.0, 0.68133, 0.94944},
      {7, false, 0.00095, 0.68273, 0.95064}, {7, true, 1.0, 0.68122, 0.94886},
  };

  std::printf("the leave one out Monte-Carlo of the coadd module\n");
  std::printf("  n  clean  trials  rejections  rate        reference  68%% coverage  95%% coverage\n");

  for (const Reference& reference : references) {
    const int n = reference.n;
    const std::size_t frame_count = static_cast<std::size_t>(n);
    const std::vector<std::vector<double>> gamma = gamma_family(n);
    const std::vector<std::vector<double>> lower = cholesky(gamma);

    // The external fixed Gamma and the frame noises that produce it.
    std::vector<std::vector<double>> identity(frame_count,
                                              std::vector<double>(frame_count, 0.0));
    for (std::size_t i = 0; i < frame_count; ++i) identity[i][i] = 1.0;
    // The external fixed Gamma is the product L L^T of its Cholesky factor.
    acsd::Result<acsd::Ref<acsd::CovarianceAction>> external =
        make_covariance(context, identity, lower, "external_gamma");

    std::vector<acsd::Result<acsd::Ref<acsd::CovarianceAction>>> noises;
    for (std::size_t i = 0; i < frame_count; ++i) {
      noises.push_back(make_covariance(context, identity, {lower[i]}, "frame_noise", "shared", true));
    }

    acsd::CoaddRequest request;
    request.target.id = "validation";
    request.target.quantity = "brightness";
    request.mixed_observation = false;
    request.weights = acsd::WeightPolicy::blue;
    request.rejection.kind = acsd::RejectionPolicy::Kind::single_max_loo;
    request.rejection.alpha = alpha;
    request.rejection.external_fixed_covariance = value_of(external);
    request.rejection.common_mean_proof = "validation_common_mean";
    request.solve.relative_residual = 1e-10;
    request.solve.absolute_residual = 1e-12;
    request.solve.rank_relative = 1e-12;
    request.solve.max_iterations = 64;
    request.solve.dense_max_bytes = 1u << 22;

    const std::size_t trials = n >= 4 ? 20000u : 4000u;
    std::mt19937_64 generator(20261022ull + static_cast<unsigned long long>(n) * 1000ull +
                              (reference.contaminated ? 4ull : 0ull));
    std::normal_distribution<double> normal(0.0, 1.0);

    std::size_t rejections = 0;
    std::size_t covered68 = 0;
    std::size_t covered95 = 0;
    std::size_t skipped = 0;
    std::size_t independent_rejections = 0;
    std::size_t weight_violations = 0;
    for (std::size_t trial = 0; trial < trials; ++trial) {
      // x = mu + L z with the independent normal draws of the trial.
      std::vector<double> z(frame_count, 0.0);
      for (std::size_t k = 0; k < frame_count; ++k) z[k] = normal(generator);
      std::vector<double> samples(frame_count, 0.0);
      for (std::size_t i = 0; i < frame_count; ++i) {
        double shift = 0.0;
        for (std::size_t k = 0; k < frame_count; ++k) shift += lower[i][k] * z[k];
        samples[i] = mu + shift;
      }
      if (reference.contaminated) {
        samples[frame_count - 1] += 12.0 * std::sqrt(gamma[frame_count - 1][frame_count - 1]);
      }

      std::vector<TestFrame> frames;
      for (std::size_t i = 0; i < frame_count; ++i) {
        TestFrame frame;
        frame.frame = "frame_" + std::to_string(i);
        frame.order = 1;
        frame.leaves = {acsd::HealpixLeaf{1, 0}};
        frame.signal = {samples[i]};
        frame.dq = {0};
        frame.state = {1};
        frame.noise = value_of(noises[i]);
        frames.push_back(frame);
      }
      TestFramesProvider provider(frames);

      acsd::Result<acsd::Ref<acsd::Selection>> selection =
          acsd::select_samples(context, provider, request);
      if (!is_ok(selection)) {
        check(false, "the selection of the Monte-Carlo trial is available");
        break;
      }
      acsd::Result<acsd::Ref<acsd::WeightSolution>> weights =
          acsd::solve_weights(context, provider, value_of(selection), request);
      if (!is_ok(weights)) {
        check(false, "the weights of the Monte-Carlo trial are available");
        break;
      }
      // The accepted samples and the weights of the solution.
      const std::size_t accepted_count = value_of(selection)->accepted->shape[0] *
                                         value_of(selection)->accepted->shape[1];
      acsd::Result<acsd::ArrayView> accepted_view = context.storage.read(
          context, value_of(selection)->accepted, acsd::BlockRequest{0, accepted_count});
      const std::vector<std::uint8_t> accepted = decode_u8(value_of(accepted_view).bytes);
      acsd::Result<acsd::ArrayView> coefficient_view = context.storage.read(
          context, value_of(weights)->coefficients,
          acsd::BlockRequest{0, static_cast<acsd::U64>(frame_count)});
      const std::vector<double> coefficients = decode_f64(value_of(coefficient_view).bytes);

      // The leave one out statistic, computed independently of the module.
      double worst_q = 0.0;
      for (std::size_t k = 0; k < frame_count; ++k) {
        std::vector<std::size_t> rest;
        for (std::size_t j = 0; j < frame_count; ++j) {
          if (j != k) rest.push_back(j);
        }
        // The GLS weights of the sub block.
        const std::size_t m = rest.size();
        std::vector<std::vector<double>> sub(m, std::vector<double>(m, 0.0));
        for (std::size_t a = 0; a < m; ++a) {
          for (std::size_t b = 0; b < m; ++b) sub[a][b] = gamma[rest[a]][rest[b]];
        }
        std::vector<double> weights_rest(m, 0.0);
        {
          // Solve sub t = 1 / (1^T t) by the Cholesky factor.
          const std::vector<std::vector<double>> factor = cholesky(sub);
          std::vector<double> y(m, 0.0);
          for (std::size_t a = 0; a < m; ++a) {
            double sum = 1.0;
            for (std::size_t b = 0; b < a; ++b) sum -= factor[a][b] * y[b];
            y[a] = sum / factor[a][a];
          }
          std::vector<double> t(m, 0.0);
          for (std::size_t a = m; a-- > 0;) {
            double sum = y[a];
            for (std::size_t b = a + 1; b < m; ++b) sum -= factor[b][a] * t[b];
            t[a] = sum / factor[a][a];
          }
          double denominator = 0.0;
          for (const double value : t) denominator += value;
          for (std::size_t a = 0; a < m; ++a) weights_rest[a] = t[a] / denominator;
        }
        double mean = 0.0;
        for (std::size_t a = 0; a < m; ++a) mean += weights_rest[a] * samples[rest[a]];
        double quadratic = 0.0;
        double cross = 0.0;
        for (std::size_t a = 0; a < m; ++a) {
          for (std::size_t b = 0; b < m; ++b) {
            quadratic += weights_rest[a] * sub[a][b] * weights_rest[b];
          }
          cross += gamma[k][rest[a]] * weights_rest[a];
        }
        const double variance = gamma[k][k] + quadratic - 2.0 * cross;
        const double statistic = (samples[k] - mean) / std::sqrt(variance);
        if (std::fabs(statistic) > worst_q) worst_q = std::fabs(statistic);
      }
      if (n >= 4) {
        // The quantile of the reference: Phi^-1(1 - alpha / (2 N)).
        const double independent_threshold = normal_quantile(1.0 - alpha / (2.0 * static_cast<double>(n)));
        if (worst_q > independent_threshold) ++independent_rejections;
      }

      bool rejected = false;
      double estimate = 0.0;
      double variance = 0.0;
      double weight_sum = 0.0;
      for (std::size_t i = 0; i < frame_count; ++i) {
        if (accepted[i] == 1) weight_sum += coefficients[i];
        if (accepted[i] != 1 && std::fabs(coefficients[i]) > 1e-12) ++weight_violations;
      }
      if (std::fabs(weight_sum - 1.0) > 1e-9) ++weight_violations;
      for (std::size_t i = 0; i < frame_count; ++i) {
        if (accepted[i] != 1) {
          rejected = true;
          continue;
        }
        estimate += coefficients[i] * samples[i];
        for (std::size_t j = 0; j < frame_count; ++j) {
          if (accepted[j] != 1) continue;
          variance += coefficients[i] * gamma[i][j] * coefficients[j];
        }
      }
      if (rejected) ++rejections;
      const double sigma = std::sqrt(std::max(variance, 0.0));
      if (std::fabs(estimate - mu) <= sigma) ++covered68;
      if (std::fabs(estimate - mu) <= 1.959963984540054 * sigma) ++covered95;
      // The frames of a small family are never rejected.
      if (n < 4 && rejected) ++skipped;
    }

    const double rate = static_cast<double>(rejections) / static_cast<double>(trials);
    const double coverage68 = static_cast<double>(covered68) / static_cast<double>(trials);
    const double coverage95 = static_cast<double>(covered95) / static_cast<double>(trials);
    std::printf("  %d  %-5s  %6zu  %9zu  %-10.6g %-10.6g %-14.6g %-14.6g  independent %zu\n", n,
                reference.contaminated ? "dirty" : "clean", trials, rejections, rate,
                reference.false_rejection_rate, coverage68, coverage95, independent_rejections);

    const std::string label = "N = " + std::to_string(n) + (reference.contaminated ? " (contaminated)"
                                                                                     : " (clean)");
    check(weight_violations == 0,
          "the weights of " + label + " are recomputed on the accepted samples");
    // The two computations agree up to the last digits of the statistic, so
    // only a trial that sits exactly on the threshold may differ.
    const std::size_t tolerance = std::max<std::size_t>(2, trials / 100);
    const std::size_t difference = independent_rejections > rejections
                                       ? independent_rejections - rejections
                                       : rejections - independent_rejections;
    check(difference <= tolerance,
          "the module rejects the same trials as the independent statistic of " + label);
    if (n < 4) {
      check(skipped == 0, "no frame is rejected for " + label);
      check(rejections == 0, "the rejection count stays zero for " + label);
    } else if (reference.contaminated) {
      check(rate > 0.99, "the contaminated frame is rejected for " + label);
      check(std::fabs(coverage68 - reference.coverage_68) <= 0.02,
            "the 68% coverage of " + label + " agrees with the reference");
      check(std::fabs(coverage95 - reference.coverage_95) <= 0.02,
            "the 95% coverage of " + label + " agrees with the reference");
    } else {
      // The clean family must not reject more often than the reference.  The
      // interval of the reference is widened by the binomial noise of the
      // smaller run of this validation.
      const std::pair<double, double> interval = clopper_pearson(rejections, trials);
      const double upper = std::max(interval.second, 3.0 * reference.false_rejection_rate);
      check(rate <= upper, "the clean false rejection rate of " + label + " is inside the interval");
      check(std::fabs(coverage68 - reference.coverage_68) <= 0.02,
            "the 68% coverage of " + label + " agrees with the reference");
      check(std::fabs(coverage95 - reference.coverage_95) <= 0.02,
            "the 95% coverage of " + label + " agrees with the reference");
    }
  }

  std::printf("%d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
