// Minimal numerical counterexample for the x01 photometry.fit failure.
//
// The program calls the module-internal joint fit (the same code as
// fit_photometry) with hand-computable inputs.  It is the minimal form of
// x01-00: one star, one frame, a 1e16 measured flux and an unknown variance
// (the joint covariance of sources.measure is not a flux-domain covariance, so
// the fit falls back to equal weights).  The expected results:
//
//   A  counts 1e16, variance unknown (1)      -> precision_exhausted
//   B  counts 1e16, variance 1e16             -> converged
//   C  100 counts, variance 100 (normal)      -> converged
//   D  2 frames, one star, no Gaia information, identical design -> not
//      identifiable with a non-empty null space (no ridge)
//
// Case A is the arithmetic of the x01 trace: v = 2.31879e20, theta0 =
// log(1e16 / 2.31879e20) = -10.06, chi2 ~ O(1), score ~ 1.9e17,
// reduced jacobian ~ 1.5e16 and step ~ -8.2e-16, which is below
// ulp(10.06) = 1.78e-15: no representable candidate exists.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "photometry/joint_fit.hpp"

namespace {

using namespace acsd;

class NeverCancelled final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

class MemResources final : public Resources {
 public:
  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    Reservation reservation;
    reservation.bytes = bytes;
    reservation.threads = threads;
    reservation.lease.owner = std::shared_ptr<const void>(&owner_, [](const void*) {});
    Result<Reservation> out;
    out.code = Code::ok;
    out.value = reservation;
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }
  Result<U64> consume_work(U64 units) override {
    work_ += units;
    Result<U64> out;
    out.code = Code::ok;
    out.value = units;
    out.assessment.complete = true;
    out.assessment.execution = ExecutionState::complete;
    return out;
  }
  Usage usage() const override {
    Usage usage;
    usage.work = work_;
    return usage;
  }

 private:
  int owner_ = 0;
  U64 work_ = 0;
};

class NullStorage final : public Storage {
 public:
  Result<ArrayRef> copy(Context&, DType, std::span<const std::byte>, std::span<const U64>,
                        const Unit&, Id) override {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }
  Result<ArrayView> read(Context&, ArrayRef, BlockRequest) const override {
    Result<ArrayView> out;
    out.code = Code::unsupported;
    return out;
  }
  Result<ArrayRef> open(Context&, std::string, const Array&,
                        std::span<const BlockDescriptor>) override {
    Result<ArrayRef> out;
    out.code = Code::unsupported;
    return out;
  }
};

int g_failures = 0;

void check(const char* name, bool pass, const std::string& actual) {
  std::printf("{\"check\":\"%s\",\"pass\":%s,\"actual\":\"%s\"}\n", name,
              pass ? "true" : "false", actual.c_str());
  if (!pass) ++g_failures;
}

photometry::FitSettings settings() {
  photometry::FitSettings settings;
  settings.gradient_tolerance = 1e-10;  // x01 normalize.scientific.json
  settings.step_tolerance = 1e-12;
  settings.max_outer_iterations = 32;
  settings.rank_relative = 1e-10;
  settings.max_backtracks = 32;
  return settings;
}

Basis constant_basis() {
  Basis basis;
  basis.kind = Basis::Kind::constant;
  basis.degree = 0;
  basis.normalization_domain.lower = Vec2{0.0, 0.0};
  basis.normalization_domain.upper = Vec2{1.0, 1.0};
  basis.center = Vec2{0.0, 0.0};
  basis.scale = Vec2{1.0, 1.0};
  return basis;
}

// The x01-00 magnitudes: v = 2.31879e20 (response basis integral of the first
// XP basis, photon convention) and a measured flux at the 1e16 level.
photometry::FrameInput frame(const char* id, double counts, double variance, bool variance_known,
                             const std::vector<double>& v) {
  photometry::FrameInput frame;
  frame.frame = id;
  frame.v = v;
  frame.dv = {8.00657e6, 4.05598e6, 2.73264e6};
  frame.counts = {counts};
  frame.variance = {variance};
  frame.variance_known = variance_known;
  frame.flux_low = {counts};
  frame.flux_high = {counts};
  frame.position_x = {63.5};
  frame.position_y = {63.5};
  frame.star = {0};
  return frame;
}

// x01-00 exactly: seven measurements in one frame, flux magnitudes and the
// response integrals copied from the trace.  Four fluxes are positive, three
// negative; the initial log amplitude is the median of the positive ratios.
photometry::FrameInput x01_frame(const std::vector<double>& counts, double variance,
                                 bool variance_known, const std::vector<double>& v) {
  photometry::FrameInput frame;
  frame.frame = "x01-00";
  frame.v = v;
  frame.dv = {8.00657e6, 4.05598e6, 2.73264e6};
  for (std::size_t i = 0; i < counts.size(); ++i) {
    frame.counts.push_back(counts[i]);
    frame.variance.push_back(variance);
    frame.flux_low.push_back(counts[i]);
    frame.flux_high.push_back(counts[i]);
    frame.position_x.push_back(40.0 + static_cast<double>(i));
    frame.position_y.push_back(60.0);
    frame.star.push_back(i);
  }
  frame.variance_known = variance_known;
  return frame;
}

photometry::StarInput star(const std::vector<double>& gaia) {
  photometry::StarInput star;
  star.source_id = 1000;
  star.gaia = gaia;
  return star;
}

void run_case(const char* label, std::vector<photometry::FrameInput> frames,
              std::vector<photometry::StarInput> stars, Context& context,
              const std::vector<double>& weighted_mean, bool expect_converged,
              bool expect_precision_exhausted, bool expect_identifiable) {
  Result<photometry::FitResult> fit = photometry::run_joint_fit(
      context, frames, stars, constant_basis(), weighted_mean, settings());
  const std::string actual =
      std::string("code=") + std::to_string(static_cast<int>(fit.code)) +
      " converged=" + (fit.value && fit.value->converged ? "true" : "false") +
      " precision=" + (fit.value && fit.value->precision_exhausted ? "true" : "false") +
      " identifiable=" + (fit.value && fit.value->identifiable ? "true" : "false") +
      " iterations=" + (fit.value ? std::to_string(fit.value->iterations) : "-") +
      " chi2=" + (fit.value ? std::to_string(fit.value->chi_square) : "-") +
      " nullspace=" + (fit.value ? std::to_string(fit.value->rank.nullspace.size()) : "-") +
      " failure=" + (fit.value ? fit.value->failure : "no-result");
  check(label, fit.code == Code::ok && fit.value &&
                   fit.value->converged == expect_converged &&
                   (!fit.value->precision_exhausted || expect_precision_exhausted) &&
                   fit.value->identifiable == expect_identifiable,
        actual);
}

}  // namespace

int main() {
  NeverCancelled cancellation;
  MemResources resources;
  NullStorage storage;
  Context context{resources, cancellation, storage};
  const std::vector<double> v{2.31879e20, 1.17463e20, 7.91382e19};
  const std::vector<double> gaia{1.0, 0.0, 0.0};
  const std::vector<double> weighted_mean{};

  const std::vector<double> x01_counts{-2.43161e16, 7.58085e15, 8.05308e15,
                                       9.20909e15,  9.29698e15, -5.43299e15,
                                       -4.30042e15};
  std::vector<photometry::StarInput> x01_stars;
  for (int i = 0; i < 7; ++i) x01_stars.push_back(star(gaia));

  // A: x01-00, unknown variance -> equal weights, step below the resolution.
  run_case("A_x01_scale_unknown_variance",
           {x01_frame(x01_counts, 1.0, false, v)}, x01_stars, context,
           weighted_mean, false, true, true);

  // B: the same counts with a variance of the order of the squared flux
  // (sigma = 1e8): the step is representable and the fit converges.
  run_case("B_same_counts_known_variance",
           {x01_frame(x01_counts, 1.0e16, true, v)}, x01_stars, context,
           weighted_mean, true, false, true);

  // C: the normal magnitude control.
  run_case("C_normal_magnitude", {frame("f0", 100.0, 100.0, true, v)}, {star(gaia)},
           context, weighted_mean, true, false, true);

  // D: two frames, one star, no Gaia information and an identical design:
  // the two scale columns are collinear and the null space is reported.
  {
    std::vector<photometry::FrameInput> frames{
        frame("f0", 100.0, 100.0, true, v), frame("f1", 100.0, 100.0, true, v)};
    std::vector<photometry::StarInput> stars{star({0.0, 0.0, 0.0})};
    run_case("D_collinear_scales", frames, stars, context, weighted_mean, false, false,
             false);
  }

  std::printf("{\"case\":\"x01_minimal_repro\",\"failures\":%d}\n", g_failures);
  return g_failures == 0 ? 0 : 1;
}
