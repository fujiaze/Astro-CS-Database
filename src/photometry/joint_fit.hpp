#pragma once
// Internal joint photometric fit of the photometry module.
//
// The public entry point is `fit_photometry` declared in
// include/acsd/contracts.hpp.  This header carries the prepared inputs and
// the outcome of the flux domain joint fit described in
// docs/science/foundation/GAIA_RESPONSE.md §3 and
// docs/engineering/rebuild/SYSTEM.md N3.
//
// Model (per frame k and star s):
//
//   A_sk = exp(theta[k] . h(u_s))          h_0 = 1, spatial terms mean zero
//   m_sk = A_sk * (v_k . c_s)              v_k = response basis integrals
//   C_sk = m_sk + noise                    counts / flux domain
//   c_Gaia = c_s + eta                     shared XP latent, one per star
//
// The latent spectral coefficients c_s are nuisance parameters that are
// shared by every frame in which the star appears.  The objective is the
// profile Gaussian likelihood of the fixed joint covariance:
//
//   chi2(theta, c) = || W_c (C - m) ||^2 + sum_s || W_s (c_Gaia,s - c_s) ||^2
//
// Because the joint covariance does not depend on theta, the elimination of
// the latent block carries no log determinant term and the profile objective
// is exactly the Gaussian likelihood up to a constant.
#include <cstddef>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"

namespace acsd::photometry {

// One frame of the joint fit.
struct FrameInput {
  Id frame;
  const SourceMeasurements* measurements = nullptr;
  const SpectralPrediction* prediction = nullptr;
  std::vector<double> v;            // d response basis integrals
  std::vector<double> dv;           // absolute integration error of v
  std::vector<double> counts;       // measured integrated flux per star
  std::vector<double> variance;     // measurement variance per star
  std::vector<double> flux_low;     // numerical enclosure of the measured flux
  std::vector<double> flux_high;
  std::vector<double> position_x;   // detector position of the measurement
  std::vector<double> position_y;
  std::vector<std::size_t> star;    // index into the star table
  bool variance_known = true;       // false when the measurement variance is unknown
  bool enclosure_missing = false;   // true when the numerical flux enclosure is absent
};

// One star with its shared XP latent.
struct StarInput {
  U64 source_id = 0;
  std::vector<double> gaia;         // d measured XP coefficients
  Ref<CovarianceAction> noise;      // covariance of the measured XP coefficients
  bool variance_known = true;
};

struct FitSettings {
  double gradient_tolerance = 1e-8;
  double step_tolerance = 1e-10;
  U32 max_outer_iterations = 100;
  double armijo = 1e-4;
  int max_backtracks = 32;
  double rank_relative = 1e-12;
  bool huber = false;
  double huber_delta = 1.345;
  U64 max_dense_elements = 4000000;
};

struct FitResult {
  bool converged = false;
  bool identifiable = true;
  std::vector<double> theta;            // frames * terms
  std::vector<double> parameter_lower;  // frames * terms
  std::vector<double> parameter_upper;  // frames * terms
  bool parameter_box_certified = false;
  numerics::RankReport rank;
  std::vector<double> covariance;       // (frames*terms)^2, row major
  std::vector<double> latent;           // stars * latent_dim
  double chi_square = 0.0;
  int iterations = 0;
  // The condition number of the profiled design and the degenerate flags: a
  // scale is published only when the profile is a normal number.
  double reduction_condition = 0.0;
  bool reduction_degenerate = false;
  bool precision_exhausted = false;
  std::string failure;
  std::vector<std::string> notes;
  std::vector<std::string> unknown_terms;
};

// Runs the joint fit.  `weighted_mean` holds the training weighted mean of
// the spatial terms (one entry per basis term, the constant term ignored).
Result<FitResult> run_joint_fit(Context& context, const std::vector<FrameInput>& frames,
                                 const std::vector<StarInput>& stars, const Basis& basis,
                                 const std::vector<double>& weighted_mean,
                                 const FitSettings& settings);

}  // namespace acsd::photometry
