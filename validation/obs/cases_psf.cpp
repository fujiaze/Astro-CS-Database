// PSF cases: the pixel integral against the analytic error function integral,
// the finite window tail, the output row selection and the Moffat and Airy
// patterns against an independent quadrature.
#include <cmath>
#include <cstdio>
#include <vector>

#include "obs_validation.hpp"

namespace acsd_validation {
namespace {

// The Bessel function J1 from its integral representation, an independent
// method for the reference value.
double reference_bessel_j1(double x) {
  const int steps = 2000;
  const double h = M_PI / steps;
  double sum = 0.0;
  for (int i = 0; i < steps; ++i) {
    const double theta = (i + 0.5) * h;
    sum += std::cos(theta - x * std::sin(theta)) * h;
  }
  return sum / M_PI;
}

// An independent midpoint quadrature of the Moffat and Airy density over one
// pixel.  The density is written out again here, so the comparison does not
// reuse the implementation under test.
double reference_pixel_integral(acsd::PsfRequest::Family family, double px, double py, double x,
                                double y, double p0, double p1, double p2, double p3) {
  const int steps = 400;
  const double hx = 1.0 / steps;
  const double hy = 1.0 / steps;
  double sum = 0.0;
  for (int i = 0; i < steps; ++i) {
    const double px_i = x - 0.5 + (i + 0.5) * hx;
    for (int j = 0; j < steps; ++j) {
      const double py_j = y - 0.5 + (j + 0.5) * hy;
      const double dx = px_i - px;
      const double dy = py_j - py;
      double density = 0.0;
      if (family == acsd::PsfRequest::Family::moffat) {
        const double sx = std::exp(p0);
        const double sy = std::exp(p1);
        const double rho = std::tanh(p2);
        const double beta = 1.0 + std::exp(p3);
        const double cross = rho * sx * sy;
        const double determinant = sx * sx * sy * sy - cross * cross;
        const double ixx = sy * sy / determinant;
        const double iyy = sx * sx / determinant;
        const double ixy = -cross / determinant;
        const double m2 = ixx * dx * dx + 2.0 * ixy * dx * dy + iyy * dy * dy;
        density =
            (beta - 1.0) * std::pow(1.0 + m2, -beta) / (M_PI * std::sqrt(determinant));
      } else {
        const double r0 = std::exp(p0);
        const double k = 3.8317059702075125 / r0;  // the first positive zero of J1
        const double r = std::sqrt(dx * dx + dy * dy);
        const double kr = k * r;
        double ratio = 1.0;
        if (kr > 0.0) {
          ratio = 2.0 * reference_bessel_j1(kr) / kr;
        }
        density = k * k / (4.0 * M_PI) * ratio * ratio;
      }
      sum += density * hx * hy;
    }
  }
  return sum;
}

struct Fixture {
  MemoryStorage storage;
  CountingResources resources{100000000};
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

std::shared_ptr<acsd::EffectivePSF> make_psf(Fixture& fixture, acsd::PsfRequest::Family family,
                                              const std::vector<double>& parameters,
                                              std::shared_ptr<acsd::FootprintSet> footprints,
                                              U64 width, U64 height) {
  auto psf = std::make_shared<acsd::EffectivePSF>();
  psf->family = family;
  psf->parameters = make_f64_array(fixture.context, parameters, dimensionless(), "psf-parameters");
  psf->spatial_basis.kind = acsd::Basis::Kind::constant;
  psf->spatial_basis.degree = 0;
  psf->spatial_basis.center = {0.5 * static_cast<double>(width), 0.5 * static_cast<double>(height)};
  psf->spatial_basis.scale = {0.5 * static_cast<double>(width), 0.5 * static_cast<double>(height)};
  psf->spatial_domain.lower = {-0.5, -0.5};
  psf->spatial_domain.upper = {static_cast<double>(width) - 0.5,
                                static_cast<double>(height) - 0.5};
  psf->achromatic = true;
  psf->detector_samples = footprints;
  psf->regroup_scale_project = nullptr;
  psf->frame = "frame0";
  psf->tail_mass_upper = 0.0;
  psf->identity.object_id = "psf:test";
  psf->identity.content_hash = "sha256:" + std::string(64, '0');
  psf->identity.schema_id = "acsd.EffectivePSF";
  psf->identity.schema_major = 2;
  psf->identity.schema_minor = 0;
  psf->assessment.complete = true;
  return psf;
}

}  // namespace

int run_psf_cases() {
  std::printf("[psf]\n");
  Fixture fixture;
  const U64 width = 16;
  const U64 height = 16;
  const U64 count = width * height;
  std::shared_ptr<const acsd::Index> index = make_index(count, "detector0");
  std::shared_ptr<acsd::FootprintSet> footprints = make_footprints(index, width, height);

  const double sigma = 1.5;
  const double px = 7.4;
  const double py = 6.6;
  auto psf = make_psf(fixture, acsd::PsfRequest::Family::gaussian,
                      {std::log(sigma), std::log(sigma), 0.0}, footprints, width, height);

  acsd::PsfEvaluationRequest request{};
  request.source_positions = {acsd::PixelCenter0{px, py}};
  request.budget.tolerance.absolute = 1.0e-13;

  Result<acsd::Ref<acsd::PsfEvaluation>> evaluation =
      acsd::evaluate_psf(fixture.context, psf, request);
  check(evaluation.code == acsd::Code::ok, "the pixel integral is built");
  if (!evaluation.value.has_value()) return 1;
  const acsd::Ref<acsd::PsfEvaluation> result = *evaluation.value;
  check(result->values->shape.size() == 2, "the template has two axes");
  check(result->values->shape[0] == count && result->values->shape[1] == 1,
        "the template covers every output sample and the source");
  check(result->output_index && result->output_index->size == count, "the output index is complete");

  acsd::ArrayView values =
      *fixture.storage.read(fixture.context, result->values, acsd::BlockRequest{0, count}).value;
  const std::vector<double> template_values = decode_f64(values);

  // The pixel integral against the analytic error function integral.
  double worst = 0.0;
  for (U64 row = 0; row < count; ++row) {
    const double x = static_cast<double>(row % width);
    const double y = static_cast<double>(row / width);
    const double analytic = gaussian_pixel_integral(px, py, x, y, sigma);
    worst = std::max(worst, std::fabs(template_values[static_cast<std::size_t>(row)] - analytic));
  }
  check(worst < 1.0e-13, "the Gaussian pixel integral matches the analytic erf integral");
  std::printf("  max |pixel integral - analytic erf| = %.3g\n", worst);

  // The finite window is not renormalised: the collected mass is below one.
  double collected = 0.0;
  for (double value : template_values) collected += value;
  check(collected < 1.0, "a finite window does not collect the total flux");
  check(collected > 0.99, "a wide window collects nearly the total flux");
  std::printf("  collected mass over the whole frame = %.12f\n", collected);

  acsd::ArrayView tails =
      *fixture.storage.read(fixture.context, result->native_tail_upper, acsd::BlockRequest{0, 1})
           .value;
  const std::vector<double> tail_values = decode_f64(tails);
  check(std::fabs(tail_values[0] - (1.0 - collected)) < 1.0e-12,
        "the native tail bound is the mass outside the integrated region");

  // The output row selection returns the same rows as the full evaluation.
  {
    acsd::PsfEvaluationRequest subset;
    subset.source_positions = request.source_positions;
    subset.budget = request.budget;
    subset.output_rows = {3, 17, 200};
    Result<acsd::Ref<acsd::PsfEvaluation>> partial =
        acsd::evaluate_psf(fixture.context, psf, subset);
    check(partial.code == acsd::Code::ok, "the subset evaluation succeeds");
    if (partial.value.has_value()) {
      check((*partial.value)->values->shape[0] == 3 && (*partial.value)->values->shape[1] == 1,
            "the subset template has the requested rows");
      check((*partial.value)->output_index && (*partial.value)->output_index->size == 3,
            "the subset output index is the exact sub index");
      acsd::ArrayView subset_values = *fixture.storage.read(fixture.context,
                                                            (*partial.value)->values,
                                                            acsd::BlockRequest{0, 3})
                                          .value;
      const std::vector<double> subset = decode_f64(subset_values);
      check_close(subset[0], template_values[3], 0.0, "the first selected row is unchanged");
      check_close(subset[1], template_values[17], 0.0, "the second selected row is unchanged");
      check_close(subset[2], template_values[200], 0.0, "the third selected row is unchanged");
    }
  }

  // The invalid row selections.
  {
    acsd::PsfEvaluationRequest bad;
    bad.source_positions = request.source_positions;
    bad.budget = request.budget;
    bad.output_rows = {5, 3};
    Result<acsd::Ref<acsd::PsfEvaluation>> decreasing =
        acsd::evaluate_psf(fixture.context, psf, bad);
    check(decreasing.code == acsd::Code::invalid_request,
          "rows that are not strictly increasing are an invalid request");
    bad.output_rows = {5, 5};
    Result<acsd::Ref<acsd::PsfEvaluation>> repeated =
        acsd::evaluate_psf(fixture.context, psf, bad);
    check(repeated.code == acsd::Code::invalid_request, "a repeated row is an invalid request");
    bad.output_rows = {count + 1};
    Result<acsd::Ref<acsd::PsfEvaluation>> outside = acsd::evaluate_psf(fixture.context, psf, bad);
    check(outside.code == acsd::Code::invalid_request, "a row outside the output is invalid");
  }

  // The Moffat pattern against an independent quadrature.
  {
    auto moffat = make_psf(fixture, acsd::PsfRequest::Family::moffat,
                           {std::log(2.0), std::log(2.0), 0.0, std::log(2.0)}, footprints, width,
                           height);
    acsd::PsfEvaluationRequest moffat_request;
    moffat_request.source_positions = {acsd::PixelCenter0{px, py}};
    moffat_request.budget.tolerance.absolute = 1.0e-13;
    moffat_request.output_rows = {7, 8, 23};
    Result<acsd::Ref<acsd::PsfEvaluation>> moffat_result =
        acsd::evaluate_psf(fixture.context, moffat, moffat_request);
    check(moffat_result.code == acsd::Code::ok, "the Moffat integral is built");
    if (moffat_result.value.has_value()) {
      acsd::ArrayView moffat_values = *fixture.storage.read(fixture.context,
                                                            (*moffat_result.value)->values,
                                                            acsd::BlockRequest{0, 3})
                                          .value;
      const std::vector<double> values_moffat = decode_f64(moffat_values);
      double worst_moffat = 0.0;
      const std::uint64_t moffat_rows[3] = {7, 8, 23};
      for (std::size_t k = 0; k < 3; ++k) {
        const double x = static_cast<double>(moffat_rows[k] % width);
        const double y = static_cast<double>(moffat_rows[k] / width);
        const double reference = reference_pixel_integral(acsd::PsfRequest::Family::moffat, px, py, x,
                                                           y, std::log(2.0), std::log(2.0), 0.0,
                                                           std::log(2.0));
        std::printf("    Moffat row %llu: template %.12g reference %.12g\n",
                    static_cast<unsigned long long>(moffat_rows[k]), values_moffat[k], reference);
        worst_moffat = std::max(worst_moffat, std::fabs(values_moffat[k] - reference));
      }
      check(worst_moffat < 1.0e-4, "the Moffat pixel integral matches the independent quadrature");
      std::printf("  max |Moffat pixel integral - reference| = %.3g\n", worst_moffat);
    }
  }

  // The Airy pattern against an independent quadrature.
  {
    auto airy = make_psf(fixture, acsd::PsfRequest::Family::airy, {std::log(3.0)}, footprints, width,
                          height);
    acsd::PsfEvaluationRequest airy_request;
    airy_request.source_positions = {acsd::PixelCenter0{px, py}};
    airy_request.budget.tolerance.absolute = 1.0e-13;
    airy_request.output_rows = {7, 8, 23};
    Result<acsd::Ref<acsd::PsfEvaluation>> airy_result =
        acsd::evaluate_psf(fixture.context, airy, airy_request);
    check(airy_result.code == acsd::Code::ok, "the Airy integral is built");
    if (airy_result.value.has_value()) {
      acsd::ArrayView airy_values = *fixture.storage.read(fixture.context,
                                                          (*airy_result.value)->values,
                                                          acsd::BlockRequest{0, 3})
                                        .value;
      const std::vector<double> values_airy = decode_f64(airy_values);
      double worst_airy = 0.0;
      const std::uint64_t airy_rows[3] = {7, 8, 23};
      for (std::size_t k = 0; k < 3; ++k) {
        const double x = static_cast<double>(airy_rows[k] % width);
        const double y = static_cast<double>(airy_rows[k] / width);
        const double reference = reference_pixel_integral(acsd::PsfRequest::Family::airy, px, py, x,
                                                           y, std::log(3.0), 0.0, 0.0, 0.0);
        std::printf("    Airy row %llu: template %.12g reference %.12g\n",
                    static_cast<unsigned long long>(airy_rows[k]), values_airy[k], reference);
        worst_airy = std::max(worst_airy, std::fabs(values_airy[k] - reference));
      }
      check(worst_airy < 1.0e-4, "the Airy pixel integral matches the independent quadrature");
      std::printf("  max |Airy pixel integral - reference| = %.3g\n", worst_airy);
    }
  }

  // A signed regrouping (a demosaic like combination) is a linear view: the
  // template is mapped through the signed matrix, the output rows are the
  // regrouped samples and the error of a signed transfer is an absolute
  // bound, so the evaluation is marked conditional.
  {
    // A two by two native frame and one regrouped sample.
    const U64 native_count = 4;
    std::shared_ptr<const Index> native_index = make_index(native_count, "detector0");
    std::shared_ptr<const Index> grouped_index = make_index(1, "detector0");
    std::shared_ptr<acsd::FootprintSet> native_footprints = make_footprints(native_index, 2, 2);
    std::shared_ptr<acsd::FootprintSet> grouped_footprints = make_footprints(grouped_index, 1, 1);
    // The classic signed quarter sum: (a - b + c + d) / 2.
    const std::vector<U64> offsets{0, 4};
    const std::vector<U64> indices{0, 1, 2, 3};
    const std::vector<double> signed_values{0.5, -0.5, 0.5, 0.5};
    Result<acsd::Ref<acsd::LinearAction>> signed_action = make_csr(
        fixture.context, 1, native_count, offsets, indices, signed_values, native_index,
        grouped_index, adu_unit("detector0"));
    check(signed_action.code == acsd::Code::ok, "the signed regrouping matrix is built");
    if (signed_action.value.has_value()) {
      const double small_px = 0.5;
      const double small_py = 0.5;
      const double at00 = gaussian_pixel_integral(small_px, small_py, 0.0, 0.0, sigma);
      const double at10 = gaussian_pixel_integral(small_px, small_py, 1.0, 0.0, sigma);
      const double at01 = gaussian_pixel_integral(small_px, small_py, 0.0, 1.0, sigma);
      const double at11 = gaussian_pixel_integral(small_px, small_py, 1.0, 1.0, sigma);
      const double expected = 0.5 * at00 - 0.5 * at10 + 0.5 * at01 + 0.5 * at11;
      auto signed_psf = make_psf(fixture, acsd::PsfRequest::Family::gaussian,
                                 {std::log(sigma), std::log(sigma), 0.0}, native_footprints, 2, 2);
      signed_psf->regroup_scale_project = *signed_action.value;
      // The kernel is locked to the native samples; the transfer maps them onto the
      // regrouped sample.
      signed_psf->detector_samples = native_footprints;

      acsd::PsfEvaluationRequest signed_request{};
      signed_request.source_positions = {acsd::PixelCenter0{small_px, small_py}};
      signed_request.budget = request.budget;
      Result<acsd::Ref<acsd::PsfEvaluation>> signed_evaluation =
          acsd::evaluate_psf(fixture.context, signed_psf, signed_request);
      check(signed_evaluation.code == acsd::Code::ok, "the signed transfer is evaluated");
      if (signed_evaluation.value.has_value()) {
        const acsd::Ref<acsd::PsfEvaluation> signed_result = *signed_evaluation.value;
        check(signed_result->values->shape[0] == 1 && signed_result->values->shape[1] == 1,
              "the signed transfer produces the regrouped sample");
        acsd::ArrayView signed_values_read =
            *fixture.storage.read(fixture.context, signed_result->values, acsd::BlockRequest{0, 1})
                 .value;
        const std::vector<double> mapped = decode_f64(signed_values_read);
        // The regrouped sample is the signed combination of the four native
        // samples; the reference is the analytic pixel integral, so the check
        // does not use the implementation of the transfer.
        check_close(mapped[0], expected, 1.0e-13, "the signed transfer maps the template");
        // The error of a signed transfer is an absolute bound, so the
        // evaluation cannot be certified and says so.
        bool approximate = false;
        for (const acsd::Issue& issue : signed_result->assessment.issues) {
          if (issue.reason.find("absolute") != std::string::npos ||
              issue.reason.find("signed") != std::string::npos) {
            approximate = true;
          }
        }
        check(approximate,
              "the evaluation of a signed transfer states that the error is an absolute bound");
        check(signed_result->assessment.validity == acsd::Validity::approximate,
              "the evaluation of a signed transfer is marked approximate");
        check(signed_result->assessment.science == acsd::ScienceState::conditional,
              "the evaluation of a signed transfer is conditional");
      }

      // The sparse selection under the signed transfer: the requested row is
      // the only output row and the omitted rows are not renormalised.
      acsd::PsfEvaluationRequest sparse{};
      sparse.source_positions = signed_request.source_positions;
      sparse.budget = request.budget;
      sparse.output_rows = {0};
      Result<acsd::Ref<acsd::PsfEvaluation>> sparse_evaluation =
          acsd::evaluate_psf(fixture.context, signed_psf, sparse);
      check(sparse_evaluation.code == acsd::Code::ok,
            "the sparse selection under a signed transfer succeeds");
      if (sparse_evaluation.value.has_value()) {
        check((*sparse_evaluation.value)->values->shape[0] == 1,
              "the sparse selection keeps the requested row");
        acsd::ArrayView sparse_read =
            *fixture.storage.read(fixture.context, (*sparse_evaluation.value)->values,
                                  acsd::BlockRequest{0, 1})
                 .value;
        const std::vector<double> sparse_values = decode_f64(sparse_read);
        check_close(sparse_values[0], expected, 1.0e-13,
                    "the sparse selection under a signed transfer keeps the value");
      }
    }
  }

  std::printf("  psf cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
