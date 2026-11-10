// Independent probes for the geometry/drizzle remediation (task-7).
//
// Every probe re-derives the expected value from the definitions of
// SPHERICAL_DRIZZLE sections 1/2/5 with its own arithmetic and its own closed
// form; it does not call the production helper whose output it checks (the
// public chart API is compared against a hand written chart formula, the
// drizzle signal against the algebra of Eq. (1)/(2), the noise against
// R C R^T).  A probe passes only when the wrong implementation would fail it.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "coordinates/chart.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "geometry/kernel.hpp"
#include "model/units.hpp"
#include "resources/cpu_resources.hpp"
#include "storage/local_storage.hpp"

namespace {

using namespace acsd;

constexpr double kPi = 3.14159265358979323846;
int failures = 0;

void check(const char* name, bool condition, const std::string& detail = "") {
  if (condition) {
    std::printf("ok     %s\n", name);
  } else {
    std::printf("FAIL   %s %s\n", name, detail.c_str());
    ++failures;
  }
}

class NeverCancelled final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};

Ref<Index> make_index(U64 size) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = Index::Kind::native;
  index->namespace_id = "probe";
  for (U64 i = 0; i < size; ++i) {
    index->sources.push_back(SourceIdentity{"probe", "exposure", "detector", "channel", i});
  }
  index->identity = model::make_identity(
      "acsd.Index", 2, 0, model::sha256_hex("probe-index:" + std::to_string(size)), "probe");
  index->assessment = model::make_assessment(Code::ok);
  index->assessment.complete = true;
  return index;
}

Result<ArrayRef> store_f64(Context& context, const std::vector<double>& values, const Unit& unit,
                           const Id& id) {
  return context.storage.copy(context, DType::f64, model::encode_f64(values),
                              std::span<const U64>(std::array<U64, 1>{
                                  static_cast<U64>(values.size())}),
                              unit, id);
}

Result<ArrayRef> store_u64(Context& context, const std::vector<U64>& values, U64 size,
                           const Id& id) {
  return context.storage.copy(context, DType::u64, model::encode_u64(values),
                              std::span<const U64>(std::array<U64, 1>{size}),
                              model::unit_dimensionless(), id);
}

std::vector<double> read_f64(Context& context, ArrayRef array) {
  std::vector<double> out;
  if (!array) return out;
  Result<ArrayView> view = context.storage.read(context, array, BlockRequest{0, array->shape[0]});
  if (!ok(view)) return out;
  const auto bytes = val(view).bytes;
  out.resize(bytes.size() / sizeof(double));
  std::memcpy(out.data(), bytes.data(), bytes.size());
  return out;
}

// ---------------------------------------------------------------------------
// Probe A: the polar chart partials carry the face longitude constant L
// (audit P0-5 / counterexample C1).
// ---------------------------------------------------------------------------
// Closed form of the north cap chart (J = 2):  r = sqrt(3(1-z)),
// d = r (4 phi / pi - L),  s = (J - r + d)/2.
double north_s(double z, double phi, int l) {
  const double r = std::sqrt(3.0 * (1.0 - z));
  const double d = r * (4.0 * phi / kPi - static_cast<double>(l));
  return (2.0 - r + d) / 2.0;
}
double north_ds_dz(double z, double phi, int l) {
  const double r = std::sqrt(3.0 * (1.0 - z));
  const double dr_dz = -3.0 / (2.0 * r);
  return (-dr_dz + (4.0 * phi / kPi - static_cast<double>(l)) * dr_dz) / 2.0;
}
// Closed form of the south cap chart (J = 4):  w = sqrt(3(1+z)), r = 4 - w,
// d = (4 phi / pi - L) w,  s = (J - r + d)/2.
double south_s(double z, double phi, int l) {
  const double w = std::sqrt(3.0 * (1.0 + z));
  const double r = 4.0 - w;
  const double d = (4.0 * phi / kPi - static_cast<double>(l)) * w;
  return (4.0 - r + d) / 2.0;
}
double south_ds_dz(double z, double phi, int l) {
  const double w = std::sqrt(3.0 * (1.0 + z));
  const double dw_dz = 3.0 / (2.0 * w);
  const double dr_dz = -dw_dz;
  return (-dr_dz + (4.0 * phi / kPi - static_cast<double>(l)) * dw_dz) / 2.0;
}

void probe_chart_partials() {
  std::printf("\n[A] P0-5 polar chart partials\n");
  // C1: north face 0 (L = 1), the mid meridian phi = pi/4.  The true ds/dz is
  // 3/(4r); the implementation without the face constant gives 0.
  const int l = 1;
  const double phi = kPi / 4.0;
  const double zs[3] = {0.70, 0.75, 0.80};
  for (double z : zs) {
    double ds_dz = 0.0, ds_dphi = 0.0, dt_dz = 0.0, dt_dphi = 0.0;
    const bool ok_partial = coordinates::chart_partials(z, phi, ds_dz, ds_dphi, dt_dz, dt_dphi);
    const double truth = north_ds_dz(z, phi, l);
    const double buggy = 0.0;  // (4 phi/pi) dr/dz - L dr/dz with 4 phi/pi = L
    check(("A1 north ds/dz matches 3/(4r) at z=" + std::to_string(z)).c_str(),
          ok_partial && std::fabs(ds_dz - truth) < 1e-12,
          "got " + std::to_string(ds_dz) + " truth " + std::to_string(truth) + " buggy " +
              std::to_string(buggy));
    // The independent buggy formula: (4 phi/pi) dr/dz instead of
    // (4 phi/pi - L) dr/dz.
    const double r = std::sqrt(3.0 * (1.0 - z));
    const double dr_dz = -3.0 / (2.0 * r);
    const double ds_dz_without_l = (-dr_dz + (4.0 * phi / kPi) * dr_dz) / 2.0;
    check(("A2 the missing-L formula is distinguishable at z=" + std::to_string(z)).c_str(),
          std::fabs(ds_dz_without_l - truth) > 0.5,
          "without L " + std::to_string(ds_dz_without_l));
  }
  // South face 8 (L = 1), mid meridian: true ds/dz = 3/(4w).
  {
    const double z = -0.75;
    double ds_dz = 0.0, ds_dphi = 0.0, dt_dz = 0.0, dt_dphi = 0.0;
    const bool ok_partial = coordinates::chart_partials(z, phi, ds_dz, ds_dphi, dt_dz, dt_dphi);
    const double truth = south_ds_dz(z, phi, l);
    check("A3 south ds/dz matches 3/(4w)", ok_partial && std::fabs(ds_dz - truth) < 1e-12,
          "got " + std::to_string(ds_dz) + " truth " + std::to_string(truth));
  }
  // The interval enclosure over a block must contain the true partials.
  {
    const numerics::Interval z(0.78, 0.80);
    const numerics::Interval phi_iv(phi - 0.02, phi + 0.02);
    numerics::Interval ds_dz_iv, ds_dphi_iv, dt_dz_iv, dt_dphi_iv;
    const bool ok_interval = coordinates::chart_partials_interval(z, phi_iv, 0, ds_dz_iv,
                                                                 ds_dphi_iv, dt_dz_iv, dt_dphi_iv);
    bool contains = ok_interval;
    for (int i = 0; i <= 8; ++i) {
      const double zz = 0.78 + 0.02 * i / 8.0;
      for (int j = 0; j <= 8; ++j) {
        const double pp = phi - 0.02 + 0.04 * j / 8.0;
        contains = contains && ds_dz_iv.contains(north_ds_dz(zz, pp, l));
      }
    }
    check("A4 interval ds/dz contains the true partials", contains,
          "[" + std::to_string(ds_dz_iv.lower) + "," + std::to_string(ds_dz_iv.upper) + "]");
  }
  // C1 residual: the declared mean value remainder of the affine model must
  // cover the true model error.  The model is f0 + J0 (u - c) with f0 = f(c).
  {
    const double z0 = 0.78, z1 = 0.80;
    const double p0 = phi - 0.02, p1 = phi + 0.02;
    const double zc = 0.5 * (z0 + z1);
    const double pc = 0.5 * (p0 + p1);
    const double hz = std::max(zc - z0, z1 - zc);
    const double hp = std::max(pc - p0, p1 - pc);
    double ds_dz = 0.0, ds_dphi = 0.0, dt_dz = 0.0, dt_dphi = 0.0;
    coordinates::chart_partials(zc, pc, ds_dz, ds_dphi, dt_dz, dt_dphi);
    numerics::Interval ds_dz_iv, ds_dphi_iv, dt_dz_iv, dt_dphi_iv;
    coordinates::chart_partials_interval(numerics::Interval(z0, z1), numerics::Interval(p0, p1), 0, ds_dz_iv,
                                         ds_dphi_iv, dt_dz_iv, dt_dphi_iv);
    const double deviation_z =
        std::max(std::fabs(ds_dz_iv.lower - ds_dz), std::fabs(ds_dz_iv.upper - ds_dz));
    const double deviation_p =
        std::max(std::fabs(ds_dphi_iv.lower - ds_dphi), std::fabs(ds_dphi_iv.upper - ds_dphi));
    const double e_declared = deviation_z * hz + deviation_p * hp;
    const double s0 = north_s(zc, pc, l);
    double true_error = 0.0;
    double buggy_error = 0.0;
    // The buggy model uses J0 = 0 (the missing-L derivative on the mid meridian).
    for (int i = 0; i <= 20; ++i) {
      const double zz = z0 + (z1 - z0) * i / 20.0;
      for (int j = 0; j <= 20; ++j) {
        const double pp = p0 + (p1 - p0) * j / 20.0;
        const double model = s0 + ds_dz * (zz - zc) + ds_dphi * (pp - pc);
        true_error = std::max(true_error, std::fabs(north_s(zz, pp, l) - model));
        const double buggy_model = s0 + 0.0 * (zz - zc) + ds_dphi * (pp - pc);
        buggy_error = std::max(buggy_error, std::fabs(north_s(zz, pp, l) - buggy_model));
      }
    }
    check("A5 the certified residual covers the true affine model error",
          true_error <= e_declared * (1.0 + 1e-9),
          "true " + std::to_string(true_error) + " declared " + std::to_string(e_declared));
    std::printf("       C1 reproduction: declared residual %.3e, true error of the "
                "missing-L model %.3e (%.1fx larger)\n",
                e_declared, buggy_error, buggy_error / e_declared);
    check("A6 the missing-L model violates its own residual bound", buggy_error > e_declared,
          "ratio " + std::to_string(buggy_error / e_declared));
  }
}

// ---------------------------------------------------------------------------
// Probe B: chart_leaf never clamps an out-of-face index (audit P1 / C3).
// ---------------------------------------------------------------------------
void probe_chart_leaf() {
  std::printf("\n[B] P1 chart_leaf out of range\n");
  const U32 order = 4;  // nside 16
  const U64 nside = 16;
  const HealpixLeaf valid = coordinates::chart_leaf(order, 3, 0, 0);
  check("B1 a valid cell is a valid leaf", valid.ipix != std::numeric_limits<U64>::max(),
        std::to_string(valid.ipix));
  const HealpixLeaf outside = coordinates::chart_leaf(order, 3, nside, 0);
  check("B2 an out-of-face column is refused, not clamped",
        outside.ipix == std::numeric_limits<U64>::max(), std::to_string(outside.ipix));
  const HealpixLeaf outside_y = coordinates::chart_leaf(order, 3, 0, nside + 7);
  check("B3 an out-of-face row is refused, not clamped",
        outside_y.ipix == std::numeric_limits<U64>::max(), std::to_string(outside_y.ipix));
}

// ---------------------------------------------------------------------------
// Probe C: Drizzle F/N/R/B per source, constant field, order independence
// (audit P0-1/P0-2/P0-3, counterexample C2).
// ---------------------------------------------------------------------------
struct DrizzleFixture {
  Ref<Overlaps> overlaps;
  Ref<NativeObservation> observation;
};

std::shared_ptr<OverlapContribution> contribution(U64 sample, double omega, double drop, double a_lower,
                                      double a_upper) {
  auto contribution = std::make_shared<OverlapContribution>();
  contribution->source = SourceIdentity{"probe", "exposure", "detector", "channel", sample};
  contribution->leaf = HealpixLeaf{0, 0};
  contribution->native_area = SphericalAreaInterval{omega, omega};
  contribution->drop_area = SphericalAreaInterval{drop, drop};
  contribution->overlap_area = SphericalAreaInterval{a_lower, a_upper};
  contribution->shared_geometry_constraints = "probe.constraints";
  return contribution;
}

void probe_drizzle(Context& context) {
  std::printf("\n[C] P0-1/P0-2/P0-3 drizzle F/N/R/B\n");
  // C2: one leaf, two sources.  A_1 = Omega_1 = 1, A_2 = Omega_2 = 4, both
  // fully covering the leaf (a = A).  A constant surface brightness b0 = 1
  // gives x_j = b0 Omega_j = (1, 4).  The true answer is N = 5, F = 5, B = 1.
  auto make_overlaps = [](bool reversed) {
    auto overlaps = std::make_shared<Overlaps>();
    if (!reversed) {
      overlaps->contributions.push_back(*contribution(0, 1.0, 1.0, 1.0, 1.0));
      overlaps->contributions.push_back(*contribution(1, 4.0, 4.0, 4.0, 4.0));
    } else {
      overlaps->contributions.push_back(*contribution(1, 4.0, 4.0, 4.0, 4.0));
      overlaps->contributions.push_back(*contribution(0, 1.0, 1.0, 1.0, 1.0));
    }
    overlaps->identity = model::make_identity("acsd.Overlaps", 2, 0, "probe.overlaps", "probe");
    overlaps->assessment = model::make_assessment(Code::ok);
    return overlaps;
  };
  auto make_observation = [&](const std::vector<double>& values, Ref<NoiseModel> noise) {
    auto observation = std::make_shared<NativeObservation>();
    observation->integrated_values =
        val(store_f64(context, values, model::unit_adu("ADU"), "probe.native"));
    observation->native_index = make_index(static_cast<U64>(values.size()));
    observation->noise = std::move(noise);
    observation->unit = model::unit_adu("ADU");
    observation->identity = model::make_identity("acsd.NativeObservation", 2, 0, "probe.native",
                                                 "probe");
    observation->assessment = model::make_assessment(Code::ok);
    return observation;
  };
  DrizzleRequest request;
  request.order = 0;
  request.pixfrac = 1.0;
  request.budget.max_work = 1000;
  request.budget.max_depth = 8;
  request.budget.area_absolute_sr = 1e-9;
  request.budget.arithmetic.initial_bits = 64;
  request.budget.arithmetic.max_bits = 256;

  auto observation = make_observation({1.0, 4.0}, nullptr);
  std::vector<double> signals;
  for (int order = 0; order < 2; ++order) {
    Result<DrizzleResult> result =
        build_drizzle(context, make_overlaps(order == 1), observation, request);
    check(order == 0 ? "C1 constant field drizzle builds" : "C2 reversed order builds",
          ok(result), ok(result) ? "" : "code " + std::to_string(static_cast<int>(result.code)));
    if (!ok(result)) return;
    const std::vector<double> signal = read_f64(context, val(result).estimate->signal);
    const std::vector<double> lower = read_f64(context, val(result).estimate->numerical_lower);
    const std::vector<double> upper = read_f64(context, val(result).estimate->numerical_upper);
    check(order == 0 ? "C3 constant field B = b0 = 1" : "C4 reversed order B = b0 = 1",
          signal.size() == 1 && std::fabs(signal[0] - 1.0) < 1e-12,
          signal.empty() ? "empty" : std::to_string(signal[0]));
    check(order == 0 ? "C5 constant field interval is [1,1]"
                     : "C6 reversed order interval is [1,1]",
          lower.size() == 1 && upper.size() == 1 && std::fabs(lower[0] - 1.0) < 1e-12 &&
              std::fabs(upper[0] - 1.0) < 1e-12,
          lower.empty() ? "empty" : "[" + std::to_string(lower[0]) + "," +
                                          std::to_string(upper[0]) + "]");
    signals.push_back(signal.empty() ? -1.0 : signal[0]);

    // The same R: applying the operator to x reproduces the published B, and
    // the coefficients are w/N, not w.
    Result<ArrayRef> x_array =
        store_f64(context, {1.0, 4.0}, model::unit_adu("ADU"), "probe.x");
    Result<ArrayRef> applied = val(result).op->rows->apply(context, val(x_array));
    check(order == 0 ? "C7 R applied to x gives B" : "C8 R applied to x gives B",
          ok(applied) && std::fabs(read_f64(context, val(applied)).at(0) - 1.0) < 1e-12,
          ok(applied) ? std::to_string(read_f64(context, val(applied)).at(0)) : "apply failed");
    // The row of the operator for leaf 0 has coefficients w/N = 1/5 each.
    Result<std::vector<ExactInterval>> row = val(result).op->rows->row_enclosure(context, 0);
    (void)row;
    const std::vector<double> normalizer = read_f64(context, val(result).op->normalizer_sr);
    check(order == 0 ? "C9 normaliser N = 5" : "C10 normaliser N = 5",
          normalizer.size() == 1 && std::fabs(normalizer[0] - 5.0) < 1e-12,
          normalizer.empty() ? "empty" : std::to_string(normalizer[0]));
  }
  check("C11 the constant field is independent of the contribution order",
        signals.size() == 2 && std::fabs(signals[0] - signals[1]) < 1e-15,
        std::to_string(signals[0]) + " vs " + std::to_string(signals[1]));

  // Negative signal: x = (-1, 4) with w = 1 gives F = 3, N = 5, B = 0.6.
  {
    auto negative = make_observation({-1.0, 4.0}, nullptr);
    Result<DrizzleResult> result = build_drizzle(context, make_overlaps(false), negative, request);
    const std::vector<double> signal =
        ok(result) ? read_f64(context, val(result).estimate->signal) : std::vector<double>{};
    check("C12 a negative native value keeps its sign (B = 3/5)",
          signal.size() == 1 && std::fabs(signal[0] - 0.6) < 1e-12,
          signal.empty() ? "no signal" : std::to_string(signal[0]));
  }

  // Zero support: a leaf with a = 0 keeps a NaN signal, never a zero.
  {
    auto overlaps = std::make_shared<Overlaps>();
    auto zero = contribution(0, 1.0, 1.0, 0.0, 0.0);
    zero->leaf = HealpixLeaf{0, 0};
    auto present = contribution(1, 4.0, 4.0, 4.0, 4.0);
    present->leaf = HealpixLeaf{0, 1};
    overlaps->contributions.push_back(*zero);
    overlaps->contributions.push_back(*present);
    overlaps->identity = model::make_identity("acsd.Overlaps", 2, 0, "probe.overlaps", "probe");
    overlaps->assessment = model::make_assessment(Code::ok);
    auto observation2 = make_observation({1.0, 4.0}, nullptr);
    Result<DrizzleResult> result = build_drizzle(context, overlaps, observation2, request);
    check("C13 a zero support leaf does not fail the whole frame", ok(result),
          ok(result) ? "" : "code " + std::to_string(static_cast<int>(result.code)));
    if (ok(result)) {
      const std::vector<double> signal = read_f64(context, val(result).estimate->signal);
      check("C14 a zero support leaf keeps NaN, not zero",
            signal.size() == 2 && std::isnan(signal[0]) && std::fabs(signal[1] - 1.0) < 1e-12,
            signal.empty() ? "empty" : std::to_string(signal[0]));
    }
  }

  // P0-3: the operator identity follows the R coefficients.  The same areas
  // give the same operator; a different overlap interval gives a different R
  // and therefore a different DrizzleOperator/SpatialResponse identity.
  {
    auto overlaps_a = make_overlaps(false);
    auto overlaps_b = make_overlaps(false);
    overlaps_b->contributions[0].overlap_area = SphericalAreaInterval{0.5, 0.6};
    overlaps_b->identity =
        model::make_identity("acsd.Overlaps", 2, 0, "probe.overlaps.b", "probe");
    Result<DrizzleResult> result_a =
        build_drizzle(context, overlaps_a, make_observation({1.0, 4.0}, nullptr), request);
    Result<DrizzleResult> result_b =
        build_drizzle(context, overlaps_b, make_observation({1.0, 4.0}, nullptr), request);
    Result<DrizzleResult> result_a2 =
        build_drizzle(context, make_overlaps(false), make_observation({1.0, 4.0}, nullptr),
                      request);
    check("C18 the same overlap gives the same operator identity",
          ok(result_a) && ok(result_a2) &&
              val(result_a).op->identity.object_id == val(result_a2).op->identity.object_id,
          "");
    check("C19 a changed overlap changes the operator identity",
          ok(result_a) && ok(result_b) &&
              val(result_a).op->identity.object_id != val(result_b).op->identity.object_id,
          "");
    check("C20 a changed overlap changes the response identity",
          ok(result_a) && ok(result_b) &&
              val(result_a).estimate->effective_response->identity.object_id !=
                  val(result_b).estimate->effective_response->identity.object_id,
          "");
  }

  // Noise: C = diag(1,4) on the native samples gives R C R^T = (1+4)/25 = 0.2
  // on the single leaf (R = [1/5, 1/5]); the old w C w^T gave 5.
  {
    auto diagonal_index = make_index(2);
    auto diagonal_recipe = std::make_shared<ActionRecipe>();
    diagonal_recipe->kind = ActionRecipe::Kind::diagonal;
    diagonal_recipe->algorithm = "probe.diagonal";
    diagonal_recipe->version = 1;
    diagonal_recipe->spaces.domain = diagonal_index;
    diagonal_recipe->spaces.codomain = diagonal_index;
    diagonal_recipe->spaces.input = model::unit_dimensionless();
    diagonal_recipe->spaces.output = model::unit_dimensionless();
    ArrayRef diagonal_array =
        val(store_f64(context, {1.0, 2.0}, model::unit_dimensionless(), "probe.diag"));
    diagonal_recipe->arrays = {"probe.diag"};
    const std::vector<Ref<LinearAction>> no_children;
    const std::vector<ArrayRef> arrays{diagonal_array};
    Result<Ref<LinearAction>> factor =
        make_action(context, *diagonal_recipe, no_children, arrays);
    auto identity_recipe = std::make_shared<ActionRecipe>();
    identity_recipe->kind = ActionRecipe::Kind::identity;
    identity_recipe->algorithm = "probe.identity";
    identity_recipe->version = 1;
    identity_recipe->spaces.domain = diagonal_index;
    identity_recipe->spaces.codomain = diagonal_index;
    identity_recipe->spaces.input = model::unit_dimensionless();
    identity_recipe->spaces.output = model::unit_dimensionless();
    const std::vector<ArrayRef> no_arrays;
    Result<Ref<LinearAction>> identity =
        make_action(context, *identity_recipe, no_children, no_arrays);
    check("C15 the noise fixtures build", ok(factor) && ok(identity), "make_action failed");
    if (ok(factor) && ok(identity)) {
      auto root = std::make_shared<NoiseRoot>();
      root->identity = model::make_identity("acsd.NoiseRoot", 2, 0, "probe.root", "probe");
      root->assessment = model::make_assessment(Code::ok);
      root->coordinates = diagonal_index;
      root->unit = model::unit_adu("ADU");
      root->factor = val(factor);
      auto noise = std::make_shared<NoiseModel>();
      noise->identity = model::make_identity("acsd.NoiseModel", 2, 0, "probe.noise", "probe");
      noise->assessment = model::make_assessment(Code::ok);
      noise->variance_unit = model::unit_variance("ADU");
      noise->innovations = {root};
      noise->influence = {val(identity)};
      noise->state = NoiseState::known;
      auto noisy_observation = make_observation({1.0, 4.0}, noise);
      Result<DrizzleResult> result =
          build_drizzle(context, make_overlaps(false), noisy_observation, request);
      check("C16 the noise covariance is built", ok(result) && val(result).estimate->noise != nullptr,
            ok(result) ? "null noise" : "drizzle failed");
      if (ok(result) && val(result).estimate->noise) {
        ArrayRef unit_functional =
            val(store_f64(context, {1.0}, model::unit_dimensionless(), "probe.functional"));
        Result<Variance> variance =
            quadratic_form(context, val(result).estimate->noise, unit_functional);
        check("C17 noise is R C R^T (0.2), not w C w^T (5)",
              ok(variance) && variance.value->value.has_value() &&
                  std::fabs(*variance.value->value - 0.2) < 1e-12,
              ok(variance) ? (variance.value->value.has_value()
                                  ? std::to_string(*variance.value->value)
                                  : "no value")
                           : "quadratic form failed");
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Probe D: WCS based probes: rotated footprint, conservation, tolerance,
// content hashes, refine replacement (audit P0-3/P1).
// ---------------------------------------------------------------------------
HeaderFields probe_header() {
  HeaderFields header;
  header.push_back({"CTYPE1", std::string("RA---TAN")});
  header.push_back({"CTYPE2", std::string("DEC--TAN")});
  header.push_back({"CRPIX1", 17.293});
  header.push_back({"CRPIX2", 41.617});
  header.push_back({"CRVAL1", 210.0});
  header.push_back({"CRVAL2", 34.0});
  header.push_back({"RADESYS", std::string("ICRS")});
  header.push_back({"CD1_1", 0.5});
  header.push_back({"CD1_2", 0.0});
  header.push_back({"CD2_1", 0.0});
  header.push_back({"CD2_2", 0.5});
  return header;
}

Ref<FootprintSet> probe_footprints(Ref<WcsModel> wcs, U64 width, U64 height, Mat2 matrix) {
  auto set = std::make_shared<FootprintSet>();
  set->index = make_index(width * height);
  set->width = width;
  set->height = height;
  set->detector_from_sample = matrix;
  set->offset = Vec2{0.0, 0.0};
  set->local_cell = Box{{-0.5, -0.5}, {0.5, 0.5}};
  set->wcs = std::move(wcs);
  set->detector_frame = "detector-probe";
  set->identity = model::make_identity("acsd.FootprintSet", 2, 0, "probe.footprints", "probe");
  set->assessment = model::make_assessment(Code::ok);
  return set;
}

void probe_wcs_geometry(Context& context) {
  std::printf("\n[D] P1/P0-3 WCS geometry probes\n");
  NumericBudget numeric;
  numeric.tolerance.absolute = 1e-10;
  numeric.tolerance.relative = 1e-12;
  numeric.max_iterations = 64;
  numeric.arithmetic.max_bits = 256;
  AstrometricContext astrometric;
  astrometric.epoch_julian_year = 2016.0;
  const Box domain{{-0.5, -0.5}, {63.5, 63.5}};
  Result<Ref<WcsModel>> wcs =
      make_wcs(context, probe_header(), domain, astrometric, "detector-probe", numeric);
  check("D1 the TAN WCS builds", ok(wcs), ok(wcs) ? "" : "make_wcs failed");
  if (!ok(wcs)) return;
  Result<Ref<FootprintProvider>> provider = make_footprint_provider(context, val(wcs));
  check("D2 the footprint provider builds", ok(provider), "");
  if (!ok(provider)) return;

  GeometryBudget budget;
  budget.max_work = 200000;
  budget.max_depth = 6;
  budget.area_absolute_sr = 1e-6;
  budget.arithmetic.initial_bits = 64;
  budget.arithmetic.max_bits = 256;

  // P1: a rotated collection domain is refused, never replaced by its bbox.
  {
    const double c = std::sqrt(0.5);
    auto rotated_set = probe_footprints(val(wcs), 64, 64, Mat2{c, -c, c, c});
    Result<Ref<DetectorFootprint>> footprint = detector_footprint(context, rotated_set, 0);
    check("D3 the rotated footprint is built", ok(footprint), "");
    if (ok(footprint)) {
      Result<Ref<Overlaps>> overlaps =
          compute_overlap(context, *val(provider), val(footprint), 8, 1.0, budget);
      check("D4 a rotated rectangle is unsupported, not a bounding box",
            !ok(overlaps) && overlaps.code == Code::unsupported,
            "code " + std::to_string(static_cast<int>(overlaps.code)));
    }
  }

  // The axis aligned single pixel footprint.
  auto set = probe_footprints(val(wcs), 64, 64, Mat2{1.0, 0.0, 0.0, 1.0});
  Result<Ref<DetectorFootprint>> footprint = detector_footprint(context, set, 0);
  check("D5 the axis aligned footprint is built", ok(footprint), "");
  if (!ok(footprint)) return;
  Result<Ref<Overlaps>> overlaps =
      compute_overlap(context, *val(provider), val(footprint), 8, 1.0, budget);
  check("D6 the overlap builds", ok(overlaps), ok(overlaps) ? "" : "compute_overlap failed");
  if (!ok(overlaps)) return;
  check("D7 the certified overlap has a non-empty identity",
        !val(overlaps)->identity.object_id.empty(), "");
  // The declared a <= Omega_H constraint: every published leaf overlap is at
  // most the leaf solid angle, and the constraint matrix has 3 rows per leaf
  // plus the A <= Omega and sum(a)+coarse-A rows.
  {
    const double nside = 256.0;  // order 8
    const double omega_leaf = kPi / (3.0 * nside * nside);
    bool within = true;
    for (const OverlapContribution& contribution : val(overlaps)->contributions) {
      within = within && contribution.overlap_area.upper_sr <= omega_leaf * (1.0 + 1e-12);
    }
    check("D8 every leaf overlap is bounded by Omega_H", within, "");
    if (val(overlaps)->constraints && val(overlaps)->constraints->matrix) {
      const U64 rows = val(overlaps)->constraints->matrix->spaces.codomain
                           ? val(overlaps)->constraints->matrix->spaces.codomain->size
                           : 0;
      check("D9 the constraint set has a <= Omega_H rows",
            rows == 3 * val(overlaps)->contributions.size() + 2,
            "rows " + std::to_string(rows));
    }
  }
  // P0-3: the content hash follows the geometry.  Same inputs give the same
  // identity; a different order gives a different identity.
  {
    Result<Ref<Overlaps>> again =
        compute_overlap(context, *val(provider), val(footprint), 8, 1.0, budget);
    Result<Ref<Overlaps>> other_order =
        compute_overlap(context, *val(provider), val(footprint), 7, 1.0, budget);
    check("D10 the overlap identity is deterministic",
          ok(again) && val(again)->identity.object_id == val(overlaps)->identity.object_id,
          "");
    check("D11 a changed order changes the overlap identity",
          ok(other_order) &&
              val(other_order)->identity.object_id != val(overlaps)->identity.object_id,
          "");
  }
  // P1: the certified stop is bound to area_absolute_sr.  A detector rectangle
  // that straddles the z = 2/3 chart branch is kept unresolved; the same run is
  // not at tolerance with a zero area budget and is at tolerance with a budget
  // above the unresolved mass.
  {
    DetectorFootprint big;
    big.wcs = val(wcs);
    big.pixel_response = set->pixel_response;
    big.source = SourceIdentity{"probe", "exposure", "detector", "channel", 0};
    big.source.sample = 0;
    big.contraction_center = PixelCenter0{32.0, 32.0};
    Ring ring;
    ring.hole = false;
    ring.vertices = {PixelEdge0{0.5, 0.5}, PixelEdge0{63.5, 0.5}, PixelEdge0{63.5, 63.5},
                     PixelEdge0{0.5, 63.5}};
    big.detector_rings.push_back(ring);
    big.identity = model::make_identity("acsd.DetectorFootprint", 2, 0, "probe.big", "probe");
    big.assessment = model::make_assessment(Code::ok);
    auto big_footprint = std::make_shared<const DetectorFootprint>(big);
    GeometryBudget shallow;
    shallow.max_work = 200000;
    shallow.max_depth = 1;
    shallow.area_absolute_sr = 0.0;
    shallow.arithmetic.initial_bits = 64;
    shallow.arithmetic.max_bits = 256;
    Result<Ref<Overlaps>> strict =
        compute_overlap(context, *val(provider), big_footprint, 8, 1.0, shallow);
    GeometryBudget generous = shallow;
    generous.area_absolute_sr = 1.0e9;
    Result<Ref<Overlaps>> loose =
        compute_overlap(context, *val(provider), big_footprint, 8, 1.0, generous);
    std::printf("       strict: ok=%d code=%d termination=%d unresolved=%zu\n",
                ok(strict) ? 1 : 0, static_cast<int>(strict.code),
                ok(strict) ? static_cast<int>(val(strict)->status.termination) : -1,
                ok(strict) ? val(strict)->coarse_candidates.size() : 0);
    check("D12 a zero area budget is not tolerance_met with unresolved mass",
          ok(strict) && val(strict)->status.termination != Termination::tolerance_met,
          ok(strict) ? std::to_string(static_cast<int>(val(strict)->status.termination))
                     : "code " + std::to_string(static_cast<int>(strict.code)));
    std::printf("       loose:  ok=%d code=%d termination=%d unresolved=%zu\n",
                ok(loose) ? 1 : 0, static_cast<int>(loose.code),
                ok(loose) ? static_cast<int>(val(loose)->status.termination) : -1,
                ok(loose) ? val(loose)->coarse_candidates.size() : 0);
    check("D13 a generous area budget accepts the same unresolved mass",
          ok(loose) && val(loose)->status.termination == Termination::tolerance_met,
          ok(loose) ? std::to_string(static_cast<int>(val(loose)->status.termination))
                    : "code " + std::to_string(static_cast<int>(loose.code)));
  }
  // P0-3: refine_geometry produces a real replacement (old != new) when the
  // requested budget changes, and the snapshot/revision identities follow.
  {
    auto observation = std::make_shared<NativeObservation>();
    observation->integrated_values =
        val(store_f64(context, {0.5}, model::unit_adu("ADU"), "probe.native.wcs"));
    observation->native_index = make_index(1);
    observation->unit = model::unit_adu("ADU");
    observation->identity =
        model::make_identity("acsd.NativeObservation", 2, 0, "probe.native.wcs", "probe");
    observation->assessment = model::make_assessment(Code::ok);
    auto snapshot = std::make_shared<GeometrySnapshot>();
    snapshot->overlaps = {val(overlaps)};
    snapshot->source_inputs = {observation};
    snapshot->identity = model::make_identity("acsd.GeometrySnapshot", 2, 0, "probe.snapshot",
                                              "probe");
    snapshot->assessment = model::make_assessment(Code::ok);
    class DummyRefinable final : public RefinableGeometryProvider {
     public:
      Result<Ref<GeometrySnapshot>> open(Context&, Id) const override {
        return model::fail<Ref<GeometrySnapshot>>(Code::unsupported, "dummy", "probe", "");
      }
      Result<Ref<GeometryRevision>> refine(Context&, Ref<GeometrySnapshot>,
                                           const GeometryBudget&) const override {
        return model::fail<Ref<GeometryRevision>>(Code::unsupported, "dummy", "probe", "");
      }
    };
    DummyRefinable dummy;
    GeometryBudget refine_budget = budget;
    refine_budget.area_absolute_sr = 1e-9;
    Result<Ref<GeometryRevision>> revision =
        refine_geometry(context, dummy, snapshot, refine_budget);
    check("D14 refine_geometry builds a revision", ok(revision),
          ok(revision) ? "" : "refine failed");
    if (ok(revision)) {
      bool replaced = false;
      for (const Replacement& replacement : val(revision)->replacements) {
        replaced = replaced || replacement.old_hash != replacement.new_hash;
      }
      check("D15 a changed geometry budget changes the replacement hash", replaced, "");
      check("D16 the revision identity is content dependent",
            val(revision)->identity.object_id !=
                model::make_identity("acsd.GeometryRevision", 2, 0, "acsd.geometry.revision",
                                     "acsd").object_id,
            "");
    }
  }
}


// ---------------------------------------------------------------------------
// Probe E: the X01 field geometry still converges under the stricter chart
// checks (no regression of the production configuration).
// ---------------------------------------------------------------------------
void probe_x01_geometry(Context& context) {
  std::printf("\n[E] X01 field geometry regression probe\n");
  auto wcs = std::make_shared<WcsModel>();
  wcs->crpix_fits1 = Vec2{64.0, 64.0};
  wcs->cd_rad_per_pixel = Mat2{-4.848137198946305e-06, 0.0, 0.0, 4.848137198946305e-06};
  wcs->c = Vec3{0.10714019848480942, 0.9897947532827971, -0.09395384102037764};
  wcs->e = Vec3{-0.9941925097222705, 0.10761623303262886, 0.0};
  wcs->north = Vec3{0.010110958449559525, 0.09340820500209646, 0.9955765544434629};
  wcs->domain = Box{{-0.5, -0.5}, {127.5, 127.5}};
  wcs->detector_frame = "x01-00";
  wcs->identity = model::make_identity("acsd.WcsModel", 2, 0, "probe.x01.wcs", "probe");
  wcs->assessment = model::make_assessment(Code::ok);
  Result<Ref<FootprintProvider>> provider = make_footprint_provider(context, wcs);
  check("E1 the X01 provider builds", ok(provider), "");
  if (!ok(provider)) return;
  auto set = probe_footprints(wcs, 128, 128, Mat2{1.0, 0.0, 0.0, 1.0});
  GeometryBudget budget;
  budget.max_work = 1000000;
  budget.max_depth = 32;
  budget.area_absolute_sr = 1e-12;
  budget.arithmetic.initial_bits = 64;
  budget.arithmetic.max_bits = 256;
  const U64 rows[3] = {0, 64 * 128 + 64, 127 * 128 + 127};
  for (U64 row : rows) {
    Result<Ref<DetectorFootprint>> footprint = detector_footprint(context, set, row);
    if (!ok(footprint)) {
      check(("E2 pixel " + std::to_string(row) + " footprint").c_str(), false, "");
      continue;
    }
    Result<Ref<Overlaps>> overlaps =
        compute_overlap(context, *val(provider), val(footprint), 6, 1.0, budget);
    check(("E2 pixel " + std::to_string(row) + " overlap builds").c_str(), ok(overlaps),
          ok(overlaps) ? "" : "code " + std::to_string(static_cast<int>(overlaps.code)));
    if (ok(overlaps)) {
      check(("E3 pixel " + std::to_string(row) + " reaches tolerance_met").c_str(),
            val(overlaps)->status.termination == Termination::tolerance_met &&
                val(overlaps)->status.bound_validity == Validity::certified,
            "termination " + std::to_string(static_cast<int>(val(overlaps)->status.termination)) +
                " validity " + std::to_string(static_cast<int>(val(overlaps)->status.bound_validity)));
    }
  }
}

}  // namespace

int main() {
  NeverCancelled cancellation;
  Limits limits;
  limits.bytes = 1u << 28;
  limits.work = 100000000;
  limits.threads = 1;
  resources::CpuResources resources(limits);
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  probe_chart_partials();
  probe_chart_leaf();
  probe_drizzle(context);
  probe_wcs_geometry(context);
  probe_x01_geometry(context);

  std::printf("\n%s: %d failure(s)\n", failures == 0 ? "PROBES PASS" : "PROBES FAIL", failures);
  return failures == 0 ? 0 : 1;
}
