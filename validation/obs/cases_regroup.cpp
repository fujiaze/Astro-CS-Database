// Regrouping cases: the G dimension rules, the physical sum/mean modes and
// the linear view of regroup_observation.
#include <cmath>
#include <cstdio>
#include <vector>

#include "obs_validation.hpp"

namespace acsd_validation {
namespace {

struct Fixture {
  MemoryStorage storage;
  CountingResources resources{1000000};
  NeverCancelled cancellation;
  Context context{resources, cancellation, storage};
};

std::shared_ptr<acsd::RawObservation> make_raw(Fixture& fixture, const std::vector<double>& adu,
                                                U64 width, U64 height) {
  std::shared_ptr<const Index> index = make_index(static_cast<U64>(adu.size()), "detector0");
  auto footprints = make_footprints(index, width, height);
  auto raw = std::make_shared<acsd::RawObservation>();
  raw->adu = make_f64_array(fixture.context, adu, adu_unit("detector0"),
                             index->identity.object_id);
  raw->dq = make_u16_array(fixture.context, std::vector<std::uint16_t>(adu.size(), 0),
                           index->identity.object_id);
  raw->width = width;
  raw->height = height;
  raw->source_index = index;
  raw->footprints = footprints;
  raw->frame = "frame0";
  raw->identity.object_id = "raw:frame0";
  raw->identity.content_hash = "sha256:" + std::string(64, '0');
  raw->identity.schema_id = "acsd.RawObservation";
  raw->identity.schema_major = 2;
  raw->identity.schema_minor = 0;
  raw->assessment.complete = true;
  return raw;
}

}  // namespace

int run_regroup_cases() {
  std::printf("[regroup]\n");
  Fixture fixture;
  const U64 width = 4;
  const U64 height = 4;
  const U64 count = width * height;

  std::vector<double> adu(count);
  for (U64 i = 0; i < count; ++i) adu[i] = 100.0 + static_cast<double>(i);

  // A calibrated parent with a known dark and flat.
  auto raw = make_raw(fixture, adu, width, height);
  acsd::Calibration calibration;
  calibration.exposure_seconds = 1.0;
  calibration.dark_exposure_seconds = 1.0;
  calibration.flat_normalization = 1.0;
  calibration.gain_e_per_adu = 1.0;
  calibration.read_rms_e = 1.0;
  Result<acsd::Ref<acsd::NativeObservation>> calibrated =
      acsd::calibrate_observation(fixture.context, raw, calibration);
  check(calibrated.code == Code::ok, "the parent observation is calibrated");
  if (!calibrated.value.has_value()) return 1;
  const acsd::Ref<acsd::NativeObservation> parent = *calibrated.value;
  {
    acsd::ArrayView native_view =
        *fixture.storage.read(fixture.context, parent->integrated_values, acsd::BlockRequest{0, count})
             .value;
    const std::vector<double> native_values = decode_f64(native_view);
    check(native_values.size() == count, "the calibrated parent carries the native samples");
    check_close(native_values[0], 100.0, 1e-12, "the calibrated parent keeps the calibrated mean");
  }

  std::shared_ptr<const Index> native_index = make_index(count, "detector0");
  std::shared_ptr<const Index> output_index = make_index(4, "detector0-binned");
  auto output_footprints = make_footprints(output_index, 2, 2);

  // A 2x2 sum binning: four native samples per output sample.
  // The native samples are flattened as y*width + x, so a 2x2 block of a four
  // wide frame collects the indices {0,1,4,5} and {2,3,6,7}.
  const std::vector<U64> offsets{0, 4, 8, 12, 16};
  const std::vector<U64> indices{0, 1, 4, 5, 2, 3, 6, 7, 8, 9, 12, 13, 10, 11, 14, 15};
  const std::vector<double> sum_values(16, 1.0);
  Result<acsd::Ref<acsd::LinearAction>> g_sum = make_csr(
      fixture.context, 4, 16, offsets, indices, sum_values, native_index, output_index,
      adu_unit("detector0"));
  check(g_sum.code == Code::ok, "the regrouping matrix is built");
  if (g_sum.value.has_value()) {
    // The public member of the action carries the declared domain; the
    // implementation must not have to read the recipe to find it.
    const acsd::DomainUnits& spaces = (*g_sum.value)->spaces;
    check(spaces.domain != nullptr && spaces.codomain != nullptr,
          "the action object carries the inherited domain member");
  }
  if (!g_sum.value.has_value()) return 1;

  acsd::RegroupRequest request{};
  request.G = *g_sum.value;
  request.output_footprints = output_footprints;
  request.old_from_new = {2.0, 0.0, 0.0, 2.0};
  request.offset = {0.5, 0.5};
  request.mode = acsd::RegroupRequest::Mode::physical_sum;

  Result<acsd::Ref<acsd::NativeObservation>> binned =
      acsd::regroup_observation(fixture.context, parent, request);
  check(binned.code == Code::ok, "the physical sum regrouping succeeds");
  if (binned.code != Code::ok) {
    std::printf("  regroup code=%d", static_cast<int>(binned.code));
    for (const auto& issue : binned.assessment.issues) {
      std::printf(" | %s: %s", issue.origin.c_str(), issue.reason.c_str());
    }
    std::printf("\n");
  }
  if (binned.value.has_value()) {
    const acsd::Ref<acsd::NativeObservation> result = *binned.value;
    acsd::ArrayView values =
        *fixture.storage.read(fixture.context, result->integrated_values, acsd::BlockRequest{0, 4}).value;
    const std::vector<double> regrouped = decode_f64(values);
    // Native rows are flattened as y*width + x, so the first output sample
    // collects the native rows 0 and 1 of the first column pair.
    check_close(regrouped[0], adu[0] + adu[1] + adu[4] + adu[5], 1e-9,
                "the sum regrouping collects the four members");
    check_close(regrouped[1], adu[2] + adu[3] + adu[6] + adu[7], 1e-9,
                "the second sum regrouping collects the four members");
    check(result->physical_collection, "the sum regrouping is a physical collection");
    check(result->footprints != nullptr, "the physical regrouping carries a footprint set");
    check(result->native_parent == parent, "the regrouping keeps the parent");
    check(result->frame == parent->frame, "the regrouping keeps the frame");
    check(result->noise && result->noise->influence.size() == parent->noise->influence.size(),
          "the noise roots are kept and only the influence is regrouped");
    std::printf("  binned[0] = %.17g (expected %.17g)\n", regrouped[0],
                adu[0] + adu[1] + adu[4] + adu[5]);
  }

  // The mean binning: the coefficient is one over the member count.
  const std::vector<double> mean_values(16, 0.25);
  Result<acsd::Ref<acsd::LinearAction>> g_mean = make_csr(
      fixture.context, 4, 16, offsets, indices, mean_values, native_index, output_index,
      adu_unit("detector0"));
  acsd::RegroupRequest mean_request = request;
  mean_request.G = *g_mean.value;
  mean_request.mode = acsd::RegroupRequest::Mode::physical_mean;
  mean_request.mean_member_count = 4;
  Result<acsd::Ref<acsd::NativeObservation>> averaged =
      acsd::regroup_observation(fixture.context, parent, mean_request);
  check(averaged.code == Code::ok, "the physical mean regrouping succeeds");
  if (averaged.value.has_value()) {
    acsd::ArrayView values = *fixture.storage.read(fixture.context, (*averaged.value)->integrated_values,
                                            acsd::BlockRequest{0, 4}).value;
    const std::vector<double> mean = decode_f64(values);
    check_close(mean[0], (adu[0] + adu[1] + adu[4] + adu[5]) / 4.0, 1e-9,
                "the mean regrouping averages the four members");
    check_close(mean[0] * 4.0, adu[0] + adu[1] + adu[4] + adu[5], 1e-9,
                "the mean is restored to the sum by the member count");
  }

  // A mean regrouping whose member count does not match the matrix is refused.
  {
    acsd::RegroupRequest wrong = mean_request;
    wrong.mean_member_count = 2;
    Result<acsd::Ref<acsd::NativeObservation>> refused =
        acsd::regroup_observation(fixture.context, parent, wrong);
    check(refused.code == Code::invalid_model,
          "a mean regrouping with a wrong member count is an invalid model");
  }

  // A physical sum request with coefficients that are not one is refused.
  {
    Result<acsd::Ref<acsd::LinearAction>> g_half = make_csr(
        fixture.context, 4, 16, offsets, indices, std::vector<double>(16, 0.5), native_index,
        output_index, adu_unit("detector0"));
    acsd::RegroupRequest half = request;
    half.G = *g_half.value;
    Result<acsd::Ref<acsd::NativeObservation>> refused =
        acsd::regroup_observation(fixture.context, parent, half);
    check(refused.code == Code::invalid_model,
          "a physical regrouping with non-unit coefficients is an invalid model");
  }

  // A signed matrix is a linear view: no physical collection and no footprint.
  {
    Result<acsd::Ref<acsd::LinearAction>> g_signed = make_csr(
        fixture.context, 4, 16, offsets, indices,
        std::vector<double>{1.0, -1.0, 0.5, 0.5, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                            1.0, 1.0},
        native_index, output_index, adu_unit("detector0"));
    acsd::RegroupRequest view = request;
    view.G = *g_signed.value;
    view.mode = acsd::RegroupRequest::Mode::linear_view;
    Result<acsd::Ref<acsd::NativeObservation>> linear =
        acsd::regroup_observation(fixture.context, parent, view);
    check(linear.code == Code::ok, "the linear view succeeds");
    if (linear.value.has_value()) {
      check(!(*linear.value)->physical_collection, "the linear view is not a physical collection");
      check((*linear.value)->footprints == nullptr, "the linear view has no physical footprint");
      check((*linear.value)->native_parent == parent, "the linear view keeps the parent");
      acsd::ArrayView values = *fixture.storage.read(fixture.context, (*linear.value)->integrated_values,
                                              acsd::BlockRequest{0, 4}).value;
      const std::vector<double> view_values = decode_f64(values);
      check_close(view_values[0], adu[0] - adu[1] + 0.5 * adu[4] + 0.5 * adu[5], 1e-9,
                  "the signed linear view is applied as it is");
    }
  }

  // A regrouping matrix with the wrong domain is refused.
  {
    std::shared_ptr<const Index> wrong_index = make_index(7, "detector0");
    Result<acsd::Ref<acsd::LinearAction>> g_wrong = make_csr(
        fixture.context, 4, 16, offsets, indices, sum_values, wrong_index, output_index,
        adu_unit("detector0"));
    acsd::RegroupRequest wrong = request;
    wrong.G = *g_wrong.value;
    Result<acsd::Ref<acsd::NativeObservation>> refused =
        acsd::regroup_observation(fixture.context, parent, wrong);
    check(refused.code == Code::invalid_model,
          "a regrouping matrix on the wrong domain is an invalid model");
  }

  std::printf("  regroup cases done\n");
  return g_failures == 0 ? 0 : 1;
}

}  // namespace acsd_validation
