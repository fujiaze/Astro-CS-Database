#include "response/spectral.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace acsd::response {

double basis_value(const BasisData& basis, std::size_t function, double lambda_nm) {
  if (basis.segments == 0 || function >= basis.functions) {
    return 0.0;
  }
  // Locate the segment; the last knot belongs to the last segment.
  std::size_t segment = 0;
  if (lambda_nm >= basis.knots[basis.segments]) {
    segment = basis.segments - 1;
  } else {
    const auto it = std::upper_bound(basis.knots.begin(), basis.knots.end(), lambda_nm);
    segment = static_cast<std::size_t>(it - basis.knots.begin());
    if (segment > 0) {
      --segment;
    }
    if (segment >= basis.segments) {
      segment = basis.segments - 1;
    }
  }
  const double low = basis.knots[segment];
  const double high = basis.knots[segment + 1];
  const double span = high - low;
  const double t = (span > 0.0) ? (lambda_nm - low) / span : 0.0;
  const std::size_t stride = basis.degree + 1;
  const std::size_t offset = (function * basis.segments + segment) * stride;
  // Horner over ascending powers.
  double value = basis.coefficients[offset + basis.degree];
  for (std::size_t j = basis.degree; j > 0; --j) {
    value = value * t + basis.coefficients[offset + j - 1];
  }
  return value;
}

double response_value(const ResponseData& response, double lambda_nm) {
  if (response.top_hat) {
    const double half = 0.5 * response.top_hat_width_nm;
    const double distance = std::fabs(lambda_nm - response.top_hat_center_nm);
    return (distance <= half) ? 1.0 : 0.0;
  }
  const std::size_t count = response.wavelength_nm.size();
  if (count == 0) {
    return 0.0;
  }
  if (lambda_nm <= response.wavelength_nm.front()) {
    return response.throughput.front();
  }
  if (lambda_nm >= response.wavelength_nm.back()) {
    return response.throughput.back();
  }
  const auto it = std::upper_bound(response.wavelength_nm.begin(), response.wavelength_nm.end(),
                                   lambda_nm);
  const std::size_t upper = static_cast<std::size_t>(it - response.wavelength_nm.begin());
  const std::size_t lower = upper - 1;
  const double span = response.wavelength_nm[upper] - response.wavelength_nm[lower];
  const double t = (span > 0.0) ? (lambda_nm - response.wavelength_nm[lower]) / span : 0.0;
  return response.throughput[lower] + t * (response.throughput[upper] - response.throughput[lower]);
}

double photon_weight(double lambda_nm) { return lambda_nm * kPhotonConversion; }

bool provenance_declares_zero_tail(const std::string& provenance) {
  // The token is matched case insensitively on non alphanumeric boundaries so
  // that "zero_tail" inside a longer word does not accidentally enable it.
  const std::string needle = "zero_tail";
  const std::string haystack = [&] {
    std::string lowered;
    lowered.reserve(provenance.size());
    for (char c : provenance) {
      lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered;
  }();
  std::size_t at = haystack.find(needle);
  while (at != std::string::npos) {
    const bool left_ok =
        at == 0 || !(std::isalnum(static_cast<unsigned char>(haystack[at - 1])) != 0);
    const std::size_t after = at + needle.size();
    const bool right_ok = after >= haystack.size() ||
                          !(std::isalnum(static_cast<unsigned char>(haystack[after])) != 0);
    if (left_ok && right_ok) {
      return true;
    }
    at = haystack.find(needle, at + 1);
  }
  return false;
}

}  // namespace acsd::response
