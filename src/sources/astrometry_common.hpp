#pragma once
// Internal helpers of the blind astrometric solver.
//
// The public entry point is solve_astrometry, declared in
// include/acsd/contracts.hpp.  Everything below is private to src/sources and
// is never included by another module.
//
// Science references:
//   * SYSTEM.md "N2 无 WCS 盲解" - the detection background (tile median and
//     MAD, detection only), the 5x5 local maximum, the SNR threshold, the
//     ordering by SNR and (y,x), the non maximum suppression radius, the
//     per tile candidate cap, the centroid initialisation and the pixel
//     integrated Gaussian refinement, the quad enumeration, the descriptor
//     tolerance box, the coarse affine verification and the hard limit on the
//     number of verified hypotheses.
//   * OBSERVATION_COORDINATES.md §3 - the TAN and SIP model used by the fit:
//     u = p0 + 1 - CRPIX, the SIP polynomials A and B in pixel units, the
//     linear part applied after SIP and the gnomonic normalisation.
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"

namespace acsd::sources {

// ---------------------------------------------------------------------------
// The detector image consumed by the solver
// ---------------------------------------------------------------------------

// The sampled image in sample grid coordinates together with the mapping into
// the detector frame of the footprint set.
struct DetectorImage {
  U64 width = 0;
  U64 height = 0;
  std::vector<double> values;             // sample grid, row major
  std::vector<std::uint16_t> dq;           // sample grid, row major
  Mat2 detector_from_sample{};             // detector = M * sample + offset
  Vec2 offset{};
  Box domain{};                           // the complete detector domain
  double background_level = 0.0;           // the detection background only
  double background_sigma = 0.0;
};

// A detected candidate.
struct Detection {
  double x = 0.0;          // detector coordinates
  double y = 0.0;
  double sample_x = 0.0;   // sample grid coordinates
  double sample_y = 0.0;
  double flux = 0.0;       // the fitted Gaussian amplitude
  double sigma_x = 0.0;
  double sigma_y = 0.0;
  double snr = 0.0;
  U64 order = 0;          // the detection order (SNR descending, then y, x)
};

// The detection stage.  Returns the candidates in the detection order.
std::vector<Detection> detect_candidates(const DetectorImage& image, double detect_snr,
                                         U32 max_candidates);

// ---------------------------------------------------------------------------
// The TAN and SIP model of the fit
// ---------------------------------------------------------------------------

// The fitted model parameters.
struct WcsParameters {
  double crpix_x = 0.0;  // FITS one based
  double crpix_y = 0.0;
  double ra0 = 0.0;       // rad
  double dec0 = 0.0;     // rad
  double cd[4] = {0.0, 0.0, 0.0, 0.0};  // rad/pixel, row major
  U32 sip_order = 0;
  std::vector<double> sip_a;  // the coefficients of A, ordered by (total order, x power)
  std::vector<double> sip_b;
};

// The number of SIP terms of an order (the terms with 2 <= i + j <= order).
U32 sip_term_count(U32 order);

// The (i, j) powers of the SIP terms of an order, in the canonical order.
std::vector<std::array<U32, 2>> sip_term_powers(U32 order);

// The number of fitted parameters of a SIP order.
U32 wcs_parameter_count(U32 sip_order);

// The forward projection of the model: the pixel position maps to the ICRS
// unit vector.
Vec3 wcs_forward(const WcsParameters& parameters, double pixel_x, double pixel_y);

// The forward projection with the analytic Jacobian with respect to the fitted
// parameters.  The Jacobian has three rows (the unit vector components).
void wcs_forward_jacobian(const WcsParameters& parameters, double pixel_x, double pixel_y,
                          double jacobian[3][16]);

// The SIP polynomials and their derivatives with respect to the pixel
// coordinates.
double sip_value(const std::vector<double>& coefficients, std::size_t index,
                 const std::array<U32, 2>& powers, double u, double v);
void sip_derivatives(const std::vector<double>& coefficients, std::size_t index,
                     const std::array<U32, 2>& powers, double u, double v, double& d_du,
                     double& d_dv);

// The FITS header of a fitted model.
std::vector<HeaderField> wcs_header_fields(const WcsParameters& parameters);

// ---------------------------------------------------------------------------
// The quad matching
// ---------------------------------------------------------------------------

// One matched pair of a detected source and a catalogue source.
struct MatchPair {
  double pixel_x = 0.0;
  double pixel_y = 0.0;
  Vec3 sky{};             // the catalogue direction at the observation epoch
  U64 source_id = 0;
};

// The affine hypothesis that links the catalogue tangent plane to the detector.
struct AffineHypothesis {
  bool valid = false;
  double matrix[6] = {0, 0, 0, 0, 0, 0};  // px = m0 + m1 xi + m2 eta, py = m3 + m4 xi + m5 eta
  double residual_pixel = 0.0;
  std::array<int, 4> assignment{};  // observed index for the catalogue roles A, B, C, D
  Vec3 centre{};                    // the sky direction of the tangent plane origin
  double scale_rad_pixel = 0.0;     // the median plate scale of the hypothesis
};

// The catalogue side of a candidate quadruple.
struct CatalogueQuad {
  std::array<U64, 4> source_ids{};
  std::array<Vec3, 4> sky{};   // the directions at the observation epoch, in the role order
  Vec3 centre{};
  double baseline_rad = 0.0;
};

// Fits the affine hypothesis between a catalogue quadruple and an observed
// quadruple.  All 24 correspondences are tried; the best residual wins.
AffineHypothesis fit_affine(const std::array<Vec3, 4>& catalogue_sky, const Vec3& centre,
                             const std::array<std::array<double, 2>, 4>& observed);

// The similarity invariant descriptor of four points given in a two dimensional
// frame; the value is the mirror invariant canonical descriptor.
std::array<double, 4> observed_descriptor(const std::array<std::array<double, 2>, 4>& points);

// The determinant of an affine matrix stored as (m0..m5).
double affine_determinant(const double matrix[6]);

// The Jacobian of the forward projection with respect to the pixel
// coordinates (three rows, two columns).
void wcs_pixel_jacobian(const WcsParameters& parameters, double pixel_x, double pixel_y,
                        double jacobian[3][2]);

// The number of neighbours of an anchor used for the quad enumeration.
inline constexpr std::size_t kQuadNeighbours = 8;

// The result of the nonlinear fit.
struct FitOutcome {
  bool converged = false;
  bool estimable = false;
  WcsParameters parameters;
  double rms_rad = 0.0;
  double holdout_rms_rad = 0.0;
  U32 sip_order = 0;
  U32 rank = 0;
  std::string reason;
};

// The diagnostics of the fit at the solution: the Jacobian of the residuals,
// the normal matrix and the formal covariance.
struct FitDiagnostics {
  numerics::Matrix jacobian;             // the residuals (2M) against the parameters (n)
  std::vector<double> residuals;         // 2M
  numerics::Matrix covariance;           // n against n, the formal covariance
  numerics::Matrix normal_inverse;       // n against n, the inverse normal matrix
  bool covariance_valid = false;
  double reduced_chi_square = 0.0;
  double pixel_jacobian_scale = 1.0;     // the radian per pixel of the model
};

// The diagnostics of a fitted model.
FitDiagnostics fit_diagnostics(const std::vector<MatchPair>& pairs, const WcsParameters& parameters);

// The fit of the model with the escalation of the SIP order.
FitOutcome fit_wcs(const std::vector<MatchPair>& pairs, const WcsParameters& initial,
                    U32 requested_sip_order, U32 max_iterations);

}  // namespace acsd::sources
