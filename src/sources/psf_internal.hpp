#pragma once
// Internal PSF model of the sources module.
//
// The first release is achromatic: one Gaussian, Moffat or Airy shape whose
// parameters may vary over the detector as a polynomial of degree at most two
// in the normalised detector coordinates.  The density is defined in the
// original detector plane (1/pixel^2); the world density is p_det/g.
//
// The pixel integral is implemented exactly once, here.  The Gaussian without
// correlation is separable and uses the error function; the correlated
// Gaussian, the Moffat and the Airy pattern are integrated by an adaptive
// two dimensional block quadrature over the real footprint.  A finite window is
// never renormalised to the total flux.
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

#include "acsd/contracts.hpp"
#include "numerics/linalg.hpp"

namespace acsd::sources::psfmodel {

// The number of family parameters: Gaussian (log sigma x, log sigma y,
// atanh rho), Moffat (log alpha x, log alpha y, atanh rho, log(beta-1)) and
// Airy (log r0).
std::size_t family_parameter_count(PsfRequest::Family family);

// The number of spatial basis terms for the given degree (1, 3 or 6).
std::size_t basis_term_count(U32 degree);

// The basis values in the OBJECTS term order (1, x, y, x^2, xy, y^2) of the
// normalised detector coordinates.
std::vector<double> basis_values(const Basis& basis, U32 degree, double x, double y);

// The local family parameters of one source, obtained from the polynomial
// coefficients and the basis values at the source position.
std::vector<double> local_parameters(PsfRequest::Family family,
                                     const std::vector<double>& coefficients,
                                     const std::vector<double>& basis_vector);

// The density in the detector plane at the offset (dx, dy) from the source.
double density(PsfRequest::Family family, const std::vector<double>& local, double dx, double dy);

// The largest eigenvalue of the inverse shape matrix.  It converts a Euclidean
// radius into the elliptical radius of the shape metric.
double max_metric_eigenvalue(PsfRequest::Family family, const std::vector<double>& local);

// The analytic tail mass of the positive model beyond the given radius.  For
// the Gaussian and the Moffat the radius is the elliptical one, for the Airy
// pattern it is the Euclidean one.
double tail_mass(PsfRequest::Family family, const std::vector<double>& local, double radius,
                 bool euclidean_radius);

// The result of a pixel integral.
struct Integral {
  double value = 0.0;
  double error = 0.0;
  bool converged = false;
  int panels = 0;
};

// The integral of the model density over an axis aligned rectangle.  The
// source sits at (px, py) in detector coordinates.
Integral integrate_rectangle(PsfRequest::Family family, const std::vector<double>& local,
                              double px, double py, double x0, double x1, double y0, double y1,
                              double absolute_tolerance);

// The integral over a polygon footprint given by its rings.  The even-odd rule
// decides the membership, so a hole removes the mass of its area.
Integral integrate_polygon(PsfRequest::Family family, const std::vector<double>& local,
                           double px, double py, const std::vector<std::vector<PixelEdge0>>& rings,
                           double absolute_tolerance);

// The real collection footprint of one native sample in detector coordinates.
struct SampleFootprint {
  bool valid = false;
  double x0 = 0.0;
  double x1 = 0.0;
  double y0 = 0.0;
  double y1 = 0.0;
  std::vector<std::vector<PixelEdge0>> rings;
};

// The footprint of a native sample.  The default footprint is the local cell of
// the sample; the exceptions replace it by their rings.
SampleFootprint sample_footprint(const FootprintSet& footprints, U64 row);

// The radius that carries the mass of the model: ten times the width of the
// profile.  Beyond it the analytic tail of the positive model is negligible,
// so the dependency scan of a transfer never has to touch the whole frame.
double support_radius(PsfRequest::Family family,
                       const std::vector<std::vector<double>>& locals);

// The native samples whose footprint can carry the mass of the sources.
std::vector<U64> candidate_rows(const FootprintSet& footprints,
                                const std::vector<PixelCenter0>& positions, double radius);

// The valid domain of the local parameters of the model.  A parameter vector
// outside of it does not describe a density: the Gaussian and the Moffat need
// a positive width and a correlation with an absolute value below one, the
// Airy needs a positive first dark ring radius.
bool local_parameters_valid(PsfRequest::Family family, const std::vector<double>& local,
                            std::string& reason);

// The largest change of a local parameter that keeps the parameter vector
// inside the valid domain.  It is used to constrain the Gauss-Newton step, so
// the step can never walk into an invalid model.
double local_parameter_margin(PsfRequest::Family family, const std::vector<double>& local,
                              std::size_t parameter);

// The template block of a set of sources over a set of native samples.
struct TemplateBlock {
  std::vector<U64> rows;
  std::vector<double> values;   // [rows.size() * sources.size()]
  std::vector<double> errors;   // the same layout
  std::vector<double> tails;    // [sources]
};

// Builds the pixel integral template of the given sources over the given
// native samples.
Result<TemplateBlock> build_templates(Context& context, PsfRequest::Family family,
                                       const std::vector<double>& coefficients, const Basis& basis,
                                       const std::vector<PixelCenter0>& positions,
                                       const FootprintSet& footprints,
                                       const std::vector<U64>& rows, double absolute_tolerance);

// Builds the pixel integral template from explicit local parameters.  The
// non linear fit perturbs the local shape parameters, so the template has to
// be rebuildable without going through the polynomial coefficients.
Result<TemplateBlock> build_templates_from_local(Context& context, PsfRequest::Family family,
                                                  const std::vector<std::vector<double>>& locals,
                                                  const std::vector<PixelCenter0>& positions,
                                                  const FootprintSet& footprints,
                                                  const std::vector<U64>& rows,
                                                  double absolute_tolerance);

// The least squares solution of a scaled design matrix: x = A^+ b, where the
// columns of A were divided by their two norm.  The result is mapped back to
// the original units of the parameters.
std::vector<double> solve_scaled(const numerics::SvdResult& svd, const std::vector<double>& rhs,
                                  const std::vector<double>& column_scale);

// The damped Gauss-Newton step: the singular values enter as
// 1/(sigma^2 + damping).  It is the fallback of the fit when the pure
// Gauss-Newton step cannot decrease the objective.
std::vector<double> solve_damped(const numerics::SvdResult& svd, const std::vector<double>& rhs,
                                  const std::vector<double>& column_scale, double damping);


// The detector position of a catalogue star.
Result<PixelCenter0> detector_position(Context& context, Ref<WcsModel> wcs,
                                        const CatalogueStar& star,
                                        const AstrometricContext& context_epoch,
                                        const NumericBudget& budget);

// The weighted mean of the samples that are usable for a fit.
double weighted_mean(const std::vector<double>& values, const std::vector<double>& weights);

}  // namespace acsd::sources::psfmodel
