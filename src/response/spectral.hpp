#pragma once
// Internal spectral helpers of the response module.
//
// The public entry point is `integrate_response` declared in
// include/acsd/contracts.hpp.  This header only carries the internal data
// structures that mirror the declared `SpectralBasis` and `Response` payloads
// after they have been read from storage and validated.
//
// Scientific contract (docs/science/foundation/GAIA_RESPONSE.md §1-§2):
//   q_ks = INT f_s(lambda) R_k(lambda) lambda_nm 1e-9 / (h c) dlambda_nm
// with the photon conversion dropped when the response itself already carries
// it.  The basis integrals are
//   v_kl = INT b_l(lambda) R_k(lambda) lambda_nm 1e-9 / (h c) dlambda_nm
// and q_ks = v_k^T c_s.
#include <cstddef>
#include <vector>

#include "acsd/contracts.hpp"

namespace acsd::response {

// CODATA 2018 exact values: h in J s, c in m/s.
inline constexpr double kPlanckJouleSecond = 6.62607015e-34;
inline constexpr double kSpeedOfLightMeterPerSecond = 299792458.0;
// lambda_nm -> photon number factor: lambda_nm * 1e-9 / (h c) [m/J per nm].
inline constexpr double kPhotonConversion =
    1e-9 / (kPlanckJouleSecond * kSpeedOfLightMeterPerSecond);

// A validated copy of the declared `SpectralBasis` payload.
struct BasisData {
  std::vector<double> knots;         // K+1 strictly increasing knots in nm
  std::vector<double> coefficients;   // d * K * (degree+1), row major [l][k][j]
  std::size_t functions = 0;          // d
  std::size_t segments = 0;           // K
  std::size_t degree = 0;             // polynomial degree in the local variable
};

// A validated copy of the declared `Response` payload.
struct ResponseData {
  bool top_hat = false;
  double top_hat_center_nm = 0.0;
  double top_hat_width_nm = 0.0;
  std::vector<double> wavelength_nm;  // strictly increasing
  std::vector<double> throughput;     // non negative, piecewise linear
  bool zero_tail = false;             // provenance declares a zero tail outside the sampled range
  double known_lower_nm = 0.0;        // region where the response is defined
  double known_upper_nm = 0.0;
  double band_lower_nm = 0.0;         // region where the response can be non zero
  double band_upper_nm = 0.0;
};

// Value of basis function `function` at `lambda_nm` (local polynomial in the
// segment that contains the wavelength, ascending powers in t).
double basis_value(const BasisData& basis, std::size_t function, double lambda_nm);

// Value of the response at `lambda_nm`.  The caller must restrict the
// evaluation to the known range of the response.
double response_value(const ResponseData& response, double lambda_nm);

// Photon conversion weight of the integrand: lambda_nm * 1e-9 / (h c).
double photon_weight(double lambda_nm);

// True when the provenance string explicitly declares a zero tail outside the
// sampled wavelength range ("... zero_tail ...").  The convention is a token of
// the provenance string; any other provenance leaves the tail unknown.
bool provenance_declares_zero_tail(const std::string& provenance);

}  // namespace acsd::response
