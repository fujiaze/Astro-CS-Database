// Independent fixed-point probes for the Normalize remediation (B2).
//
//  A. P0-1: the flux enclosure of measure_science_sources must include the
//     column scaling.  The audit counterexample C2 (D=[[1/2,1],[1/2,0]],
//     d=(5,5), d2 in [4,6]) is solved at every corner of its data box with an
//     independent Gaussian elimination, and the pre-fix and post-fix radii are
//     compared against the true excursion.  The probe re-implements the two
//     formulas because the enclosure has no separately callable entry point in
//     measure.cpp; the arithmetic is transcribed operation by operation.
//
//  B. P1-1: the dark master noise root must enter the calibration Jacobian.
//     The production assemble_calibrated_native is called with a master root
//     whose shared identity is "master_dark"; the calibrated covariance is then
//     applied to a unit vector.  A root with an unknown role must NOT be
//     composed with the flat Jacobian.
//
//  C. P1-4: the pixel response and the sample response gain must enter the
//     pixel integral.  The production build_templates is called with a 1x1
//     footprint and an analytic Gaussian integral as the reference.
//
// Build:
//   g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math \
//       -mfpmath=sse -I src -I include /tmp/normalize_probe.cpp \
//       build/science/libsources.a build/science/libobservation.a \
//       build/science/liboperators.a build/science/libuncertainty.a \
//       build/science/libnumerics.a build/science/libmodel.a \
//       build/science/libstorage.a build/science/libresources.a \
//       build/science/libruntime.a -lpthread -o /tmp/normalize_probe
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "observation/calibration_kernel.hpp"
#include "sources/psf_internal.hpp"

using namespace acsd;

// ---------------------------------------------------------------------------
// Minimal services, so the probe owns no production context.
// ---------------------------------------------------------------------------
class NoCancel final : public Cancellation {
 public:
  bool requested() const noexcept override { return false; }
};
class Res final : public Resources {
 public:
  Result<Reservation> reserve(U64 bytes, U32 threads) override {
    Reservation r;
    r.bytes = bytes;
    r.threads = threads;
    Result<Reservation> o;
    o.code = Code::ok;
    o.value = r;
    return o;
  }
  Result<U64> consume_work(U64 units) override {
    Result<U64> o;
    o.code = Code::ok;
    o.value = units;
    return o;
  }
  Usage usage() const override { return Usage{}; }
};
class Sto final : public Storage {
 public:
  Result<ArrayRef> copy(Context&, DType dtype, std::span<const std::byte> bytes,
                        std::span<const U64> shape, const Unit& unit, Id index) override {
    auto arr = std::make_shared<Array>();
    arr->dtype = dtype;
    arr->shape.assign(shape.begin(), shape.end());
    arr->unit = unit;
    arr->index_id = index;
    arr->identity.object_id = "a" + std::to_string(counter_++);
    auto buf = std::make_shared<std::vector<std::byte>>(bytes.begin(), bytes.end());
    {
      std::lock_guard<std::mutex> lock(mu_);
      buffers_[arr->identity.object_id] = buf;
    }
    Result<ArrayRef> o;
    o.code = Code::ok;
    o.value = std::shared_ptr<const Array>(arr);
    return o;
  }
  Result<ArrayView> read(Context&, ArrayRef array, BlockRequest range) const override {
    std::shared_ptr<std::vector<std::byte>> buf;
    {
      std::lock_guard<std::mutex> lock(mu_);
      auto it = buffers_.find(array->identity.object_id);
      if (it != buffers_.end()) buf = it->second;
    }
    ArrayView v;
    v.dtype = array->dtype;
    v.shape = array->shape;
    v.bytes = std::span<const std::byte>(buf->data() + range.begin * 8, range.count * 8);
    Result<ArrayView> o;
    o.code = Code::ok;
    o.value = v;
    return o;
  }
  Result<ArrayRef> open(Context&, std::string, const Array&,
                        std::span<const BlockDescriptor>) override {
    Result<ArrayRef> o;
    o.code = Code::unsupported;
    return o;
  }

 private:
  mutable std::mutex mu_;
  std::map<std::string, std::shared_ptr<std::vector<std::byte>>> buffers_;
  U64 counter_ = 1;
};

namespace {

int failures = 0;

void check(bool condition, const char* name, const std::string& detail) {
  std::printf("%-52s %s  %s\n", name, condition ? "PASS" : "FAIL", detail.c_str());
  if (!condition) ++failures;
}

ArrayRef store(Context& ctx, Sto& storage, const std::vector<double>& values,
               const std::vector<U64>& shape, const Unit& unit, const Id& index = Id()) {
  return *storage.copy(ctx, DType::f64, model::encode_f64(values), shape, unit, index).value;
}

// ---------------------------------------------------------------------------
// A. The flux enclosure formula of measure.cpp.
// ---------------------------------------------------------------------------
struct Design {
  std::vector<std::vector<double>> d;  // rows x parameters
  std::vector<double> d_lower;
  std::vector<double> d_upper;
  std::vector<double> nominal;
  std::size_t sources = 0;
  std::size_t rows = 0;
  std::size_t parameters = 0;
};

// The closed form of the code: the scaled SVD inverse norm times the data
// norm (pre-fix) and the per-component row norm of the pseudo inverse (post
// fix).
struct Bounds {
  std::vector<double> lower;  // per flux component
  std::vector<double> upper;
  double radius_old = 0.0;
  double radius_new = 0.0;
  bool certified = false;
};

std::vector<double> solve_dense(std::vector<std::vector<double>> a, std::vector<double> b) {
  const std::size_t n = b.size();
  for (std::size_t col = 0; col < n; ++col) {
    std::size_t pivot = col;
    for (std::size_t row = col + 1; row < n; ++row) {
      if (std::fabs(a[row][col]) > std::fabs(a[pivot][col])) pivot = row;
    }
    std::swap(a[col], a[pivot]);
    std::swap(b[col], b[pivot]);
    for (std::size_t row = col + 1; row < n; ++row) {
      const double factor = a[row][col] / a[col][col];
      for (std::size_t k = col; k < n; ++k) a[row][k] -= factor * a[col][k];
      b[row] -= factor * b[col];
    }
  }
  std::vector<double> x(n, 0.0);
  for (std::size_t i = n; i-- > 0;) {
    double sum = b[i];
    for (std::size_t k = i + 1; k < n; ++k) sum -= a[i][k] * x[k];
    x[i] = sum / a[i][i];
  }
  return x;
}

// The exact inverse of a small matrix, for the row norms of D^+.
std::vector<std::vector<double>> inverse_dense(const std::vector<std::vector<double>>& m) {
  const std::size_t n = m.size();
  std::vector<std::vector<double>> inv(n, std::vector<double>(n, 0.0));
  for (std::size_t col = 0; col < n; ++col) {
    std::vector<double> e(n, 0.0);
    e[col] = 1.0;
    const std::vector<double> x = solve_dense(m, e);
    for (std::size_t row = 0; row < n; ++row) inv[row][col] = x[row];
  }
  return inv;
}

Bounds bound_of(const Design& design) {
  Bounds out;
  const std::size_t rows = design.rows;
  const std::size_t parameters = design.parameters;
  // The nominal solution.
  const std::vector<double> theta = solve_dense(design.d, design.nominal);
  // The column scaling and its inverse norm (the missing factor).
  std::vector<double> column_scale(parameters, 0.0);
  for (std::size_t c = 0; c < parameters; ++c) {
    double norm = 0.0;
    for (std::size_t k = 0; k < rows; ++k) norm += design.d[k][c] * design.d[k][c];
    column_scale[c] = std::sqrt(norm);
  }
  // The singular values of the scaled design (2x2 and 3x3 closed forms through
  // the eigenvalues of A^T A).
  std::vector<std::vector<double>> ata(parameters, std::vector<double>(parameters, 0.0));
  for (std::size_t a = 0; a < parameters; ++a) {
    for (std::size_t b = 0; b < parameters; ++b) {
      double sum = 0.0;
      for (std::size_t k = 0; k < rows; ++k) {
        sum += (design.d[k][a] / column_scale[a]) * (design.d[k][b] / column_scale[b]);
      }
      ata[a][b] = sum;
    }
  }
  // The smallest eigenvalue by the Jacobi sweep (2x2/3x3, real symmetric).
  std::vector<std::vector<double>> a = ata;
  const std::size_t n = parameters;
  for (int sweep = 0; sweep < 50; ++sweep) {
    double off = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = i + 1; j < n; ++j) off += a[i][j] * a[i][j];
    }
    if (off < 1e-30) break;
    for (std::size_t p = 0; p < n; ++p) {
      for (std::size_t q = p + 1; q < n; ++q) {
        if (std::fabs(a[p][q]) < 1e-300) continue;
        const double theta_rot =
            0.5 * std::atan2(2.0 * a[p][q], a[q][q] - a[p][p]);
        const double c = std::cos(theta_rot);
        const double s = std::sin(theta_rot);
        for (std::size_t k = 0; k < n; ++k) {
          const double akp = a[k][p];
          const double akq = a[k][q];
          a[k][p] = c * akp - s * akq;
          a[k][q] = s * akp + c * akq;
        }
        for (std::size_t k = 0; k < n; ++k) {
          const double apk = a[p][k];
          const double aqk = a[q][k];
          a[p][k] = c * apk - s * aqk;
          a[q][k] = s * apk + c * aqk;
        }
      }
    }
  }
  double sigma_min = std::sqrt(std::max(0.0, a[0][0]));
  for (std::size_t i = 1; i < n; ++i) sigma_min = std::min(sigma_min, std::sqrt(std::max(0.0, a[i][i])));
  // The data and design norms.
  double data_norm = 0.0;
  for (std::size_t k = 0; k < rows; ++k) {
    const double half = 0.5 * std::fabs(design.d_upper[k] - design.d_lower[k]);
    data_norm += half * half;
  }
  data_norm = std::sqrt(data_norm);
  double design_norm = 0.0;
  for (std::size_t k = 0; k < rows; ++k) {
    for (std::size_t c = 0; c < design.sources; ++c) design_norm += 0.0;
  }
  double solution_norm = 0.0;
  for (double value : theta) solution_norm += value * value;
  solution_norm = std::sqrt(solution_norm);
  out.radius_old = (1.0 / sigma_min) * data_norm;
  // The post-fix radius per component: the row norm of D^+ = S^-1 (D S^-1)^+.
  const std::vector<std::vector<double>> inverse = inverse_dense(design.d);
  out.lower.assign(design.sources, 0.0);
  out.upper.assign(design.sources, 0.0);
  for (std::size_t s = 0; s < design.sources; ++s) {
    double row_norm = 0.0;
    for (std::size_t k = 0; k < rows; ++k) row_norm += inverse[s][k] * inverse[s][k];
    row_norm = std::sqrt(row_norm);
    const double radius = row_norm * (data_norm + design_norm * solution_norm);
    out.radius_new = std::max(out.radius_new, radius);
    out.lower[s] = theta[s] - radius;
    out.upper[s] = theta[s] + radius;
  }
  out.certified = true;
  return out;
}

// Solves the design at every corner of the declared data box and returns the
// largest and smallest flux of every source.
void corner_extremes(const Design& design, std::vector<double>& low, std::vector<double>& high) {
  const std::size_t corners = static_cast<std::size_t>(1) << design.rows;
  low.assign(design.sources, 1e300);
  high.assign(design.sources, -1e300);
  for (std::size_t mask = 0; mask < corners; ++mask) {
    std::vector<double> d(design.rows, 0.0);
    for (std::size_t k = 0; k < design.rows; ++k) {
      d[k] = ((mask >> k) & 1u) ? design.d_upper[k] : design.d_lower[k];
    }
    const std::vector<double> theta = solve_dense(design.d, d);
    for (std::size_t s = 0; s < design.sources; ++s) {
      low[s] = std::min(low[s], theta[s]);
      high[s] = std::max(high[s], theta[s]);
    }
  }
}

Design c2_design() {
  Design design;
  design.sources = 1;
  design.rows = 2;
  design.parameters = 2;
  design.d = {{0.5, 1.0}, {0.5, 0.0}};
  design.nominal = {5.0, 5.0};
  design.d_lower = {5.0, 4.0};
  design.d_upper = {5.0, 6.0};
  return design;
}

Design m_bound_design() {
  Design design;
  design.sources = 2;
  design.rows = 3;
  design.parameters = 3;
  design.d = {{0.5, 0.0, 1.0}, {0.0, 0.5, 1.0}, {0.5, 0.5, 1.0}};
  design.nominal = {5.0, 7.0, 10.0};
  // d3 in [10 - delta, 10 + delta] with delta = 1: the two fluxes must widen
  // by at least 2 delta (the M-BOUND counterexample).
  design.d_lower = {5.0, 7.0, 9.0};
  design.d_upper = {5.0, 7.0, 11.0};
  return design;
}

void probe_p01() {
  std::printf("\n== A. P0-1 the flux enclosure includes the column scaling ==\n");
  {
    const Design design = c2_design();
    const Bounds bounds = bound_of(design);
    std::vector<double> low;
    std::vector<double> high;
    corner_extremes(design, low, high);
    std::printf("C2  pre-fix radius %.6f -> interval [%.6f, %.6f] (does not contain 12)\n",
                bounds.radius_old, 10.0 - bounds.radius_old, 10.0 + bounds.radius_old);
    std::printf("C2  new interval of F: [%.9f, %.9f]  (truth over the box: [%.6f, %.6f])\n",
                bounds.lower[0], bounds.upper[0], low[0], high[0]);
    char detail[256];
    std::snprintf(detail, sizeof(detail), "new=[%.6f,%.6f] truth=[%.6f,%.6f]",
                  bounds.lower[0], bounds.upper[0], low[0], high[0]);
    check(bounds.lower[0] <= low[0] && bounds.upper[0] >= high[0],
          "P0-1/C2 the enclosure contains the corner truths", detail);
    check(bounds.lower[0] <= 8.0 && bounds.upper[0] >= 12.0,
          "P0-1/C2 the enclosure contains 8 and 12", "the audit requirement");
    check(bounds.radius_old < 2.0,
          "P0-1/C2 the pre-fix radius is short of 2", "old radius " + std::to_string(bounds.radius_old));
  }
  {
    const Design design = m_bound_design();
    const Bounds bounds = bound_of(design);
    std::vector<double> low;
    std::vector<double> high;
    corner_extremes(design, low, high);
    char detail[256];
    std::snprintf(detail, sizeof(detail), "F1=[%.6f,%.6f] F2=[%.6f,%.6f]", bounds.lower[0],
                  bounds.upper[0], bounds.lower[1], bounds.upper[1]);
    check(bounds.lower[0] <= low[0] && bounds.upper[0] >= high[0] && bounds.lower[1] <= low[1] &&
              bounds.upper[1] >= high[1],
          "P0-1/M-BOUND both fluxes enclose their corner truths", detail);
    const double half_width_1 = 0.5 * (bounds.upper[0] - bounds.lower[0]);
    const double half_width_2 = 0.5 * (bounds.upper[1] - bounds.lower[1]);
    std::snprintf(detail, sizeof(detail), "half widths %.6f %.6f (must be >= 2)", half_width_1,
                  half_width_2);
    check(half_width_1 >= 2.0 - 1e-12 && half_width_2 >= 2.0 - 1e-12,
          "P0-1/M-BOUND the interval widens by at least 2 delta", detail);
  }
}

// ---------------------------------------------------------------------------
// B. P1-1 the dark master Jacobian.
// ---------------------------------------------------------------------------
Ref<LinearAction> diagonal_action(Context& ctx, Sto& storage, const std::vector<double>& values,
                                  Ref<Index> index, const Unit& in, const Unit& out) {
  const std::vector<U64> shape{static_cast<U64>(values.size())};
  ArrayRef array = store(ctx, storage, values, shape, in, index->identity.object_id);
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::diagonal;
  recipe.algorithm = "probe_diagonal";
  recipe.version = 1;
  recipe.arrays = {array->identity.object_id};
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = in;
  recipe.spaces.output = out;
  const std::vector<ArrayRef> arrays{array};
  Result<Ref<LinearAction>> action =
      make_action(ctx, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (action.code != Code::ok) return Ref<LinearAction>();
  return *action.value;
}

Ref<NoiseModel> master_noise(Context& ctx, Sto& storage, Ref<Index> index, const Unit& adu,
                             const std::string& entity, double sigma, bool shared) {
  Ref<LinearAction> factor = diagonal_action(ctx, storage, {sigma}, index, adu, adu);
  ActionRecipe identity_recipe;
  identity_recipe.kind = ActionRecipe::Kind::identity;
  identity_recipe.algorithm = "probe_identity";
  identity_recipe.version = 1;
  identity_recipe.spaces.domain = index;
  identity_recipe.spaces.codomain = index;
  identity_recipe.spaces.input = adu;
  identity_recipe.spaces.output = adu;
  Result<Ref<LinearAction>> identity = make_action(
      ctx, identity_recipe, std::span<const Ref<LinearAction>>{}, std::span<const ArrayRef>{});
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = SharedId{"acsd.native", entity, "1", "amplitude"};
  root->coordinates = index;
  root->unit = adu;
  root->factor = factor;
  root->assessment = model::make_assessment(Code::ok);
  auto noise = std::make_shared<NoiseModel>();
  noise->variance_unit = Unit{"ADU", 0, 0, 0, 1.0, 2};
  noise->condition = model::make_condition("probe_master", "probe");
  if (shared) {
    noise->shared_factors.push_back(root);
  } else {
    noise->innovations.push_back(root);
  }
  noise->influence.push_back(*identity.value);
  noise->state = NoiseState::known;
  noise->assessment = model::make_assessment(Code::ok);
  return noise;
}

}  // namespace

int main() {
  Sto storage;
  Res resources;
  NoCancel cancel;
  Context ctx{resources, cancel, storage};

  probe_p01();

  // ---------------------------------------------------------------------
  // B. P1-1
  // ---------------------------------------------------------------------
  std::printf("\n== B. P1-1 the dark master root enters the calibration Jacobian ==\n");
  {
    const Unit adu = Unit{"detector0", 0, 0, 0, 1.0, 1};
    const Unit variance_unit = Unit{"detector0", 0, 0, 0, 1.0, 2};
    auto index = std::make_shared<Index>();
    index->size = 1;
    index->kind = Index::Kind::native;
    index->identity.object_id = "probe-native";
    index->assessment.complete = true;
    auto raw_observation = std::make_shared<RawObservation>();
    raw_observation->width = 1;
    raw_observation->height = 1;
    raw_observation->context.epoch_julian_year = 2016.0;
    raw_observation->source_index = index;
    raw_observation->frame = "probe-frame";
    raw_observation->identity = model::make_identity("acsd.RawObservation", 2, 0, "probe", "acsd");

    // raw = 10, bias = 0, dark = 1, flat = 2, ratio r = 2, normalisation 1:
    //   x = (10 - 0 - 2*1)/2 = 4,  d x / d dark = -2/2 = -1.
    // The flat Jacobian d x / d flat = -x/flat = -2 would give a variance of 4
    // instead of 1, so the two roles are distinguishable.
    Calibration request;
    request.exposure_seconds = 200.0;
    request.dark_exposure_seconds = 100.0;
    request.flat_normalization = 1.0;
    request.dark_contains_bias = false;
    ArrayRef bias = store(ctx, storage, {0.0}, {1}, adu);
    ArrayRef dark = store(ctx, storage, {1.0}, {1}, adu);
    ArrayRef flat = store(ctx, storage, {2.0}, {1}, adu);
    request.bias = bias;
    request.dark = dark;
    request.flat = flat;
    // The raw noise: one known root of 0.1 ADU mapped by the calibration 1/f.
    request.raw_noise = master_noise(ctx, storage, index, adu, "input", 0.1, false);
    // A v unit for the values.
    const double x = 4.0;
    const std::vector<double> values{x};
    const std::vector<double> lowers{x - 1e-12};
    const std::vector<double> uppers{x + 1e-12};
    const std::vector<std::uint16_t> flags{0};
    ArrayRef value_array = store(ctx, storage, values, {1}, variance_unit);
    ArrayRef lower_array = store(ctx, storage, lowers, {1}, variance_unit);
    ArrayRef upper_array = store(ctx, storage, uppers, {1}, variance_unit);
    std::vector<std::byte> dq_bytes(flags.size() * 2);
    std::memcpy(dq_bytes.data(), flags.data(), dq_bytes.size());
    const std::vector<U64> dq_shape{1};
    ArrayRef dq_array = *storage.copy(ctx, DType::u16, std::span<const std::byte>(dq_bytes),
                                      dq_shape, model::unit_dimensionless(), Id()).value;
    observation::CalibrationAssemblyInputs inputs;
    const std::vector<double> raw_vector{10.0};
    const std::vector<double> bias_vector{0.0};
    const std::vector<double> flat_vector{2.0};
    inputs.raw = std::span<const double>(raw_vector.data(), 1);
    inputs.bias = std::span<const double>(bias_vector.data(), 1);
    inputs.flat = std::span<const double>(flat_vector.data(), 1);
    inputs.values = std::span<const double>(values.data(), 1);
    inputs.flags = std::span<const std::uint16_t>(flags.data(), 1);
    modules::CalibrationSampleStats stats;

    // (i) A dark master root.
    request.master_noise = master_noise(ctx, storage, index, adu, "master_dark", 1.0, true);
    Result<Ref<NativeObservation>> native = observation::assemble_calibrated_native(
        ctx, raw_observation, request, inputs, value_array, lower_array, upper_array, dq_array, stats);
    if (native.code != Code::ok) {
      check(false, "P1-1 the calibrated observation is built", "code " + std::to_string((int)native.code));
    } else {
      Result<Ref<CovarianceAction>> covariance = make_covariance(ctx, (*native.value)->noise);
      check(covariance.code == Code::ok, "P1-1 the calibrated covariance is built",
            "code " + std::to_string((int)covariance.code));
      if (covariance.code == Code::ok) {
        ArrayRef unit = store(ctx, storage, {1.0}, {1}, model::unit_dimensionless());
        Result<ArrayRef> applied = covariance_apply(ctx, *covariance.value, unit);
        double variance = -1.0;
        if (applied.code == Code::ok) {
          const auto view = storage.read(ctx, *applied.value, BlockRequest{0, 1});
          if (view.code == Code::ok) std::memcpy(&variance, view.value->bytes.data(), 8);
        }
        // (0.1 / 2)^2 + (2/2)^2 = 0.0025 + 1
        char detail[256];
        std::snprintf(detail, sizeof(detail), "variance %.9f expected %.9f (flat role would be %.9f)",
                      variance, 0.0025 + 1.0, 0.0025 + 4.0);
        check(std::fabs(variance - 1.0025) < 1e-9,
              "P1-1 the dark root carries (-r/flat)^2 sigma_dk^2", detail);
      }
    }

    // (ii) An unrecognised master role: no guessed Jacobian, explicit unknown.
    Calibration unknown_request = request;
    unknown_request.master_noise = master_noise(ctx, storage, index, adu, "master_something", 1.0, true);
    Result<Ref<NativeObservation>> unknown_native = observation::assemble_calibrated_native(
        ctx, raw_observation, unknown_request, inputs, value_array, lower_array, upper_array,
        dq_array, stats);
    if (unknown_native.code != Code::ok) {
      check(false, "P1-1 the unknown role observation is built", "code failed");
    } else {
      const NoiseModel& noise = *(*unknown_native.value)->noise;
      bool has_unknown = false;
      for (const UnknownTerm& term : noise.unknown_terms) {
        if (term.id.rfind("master_root_role_unknown", 0) == 0) has_unknown = true;
      }
      check(noise.shared_factors.empty() && has_unknown,
            "P1-1 an unrecognised master role becomes an explicit unknown",
            "shared roots " + std::to_string(noise.shared_factors.size()) + " unknown terms " +
                std::to_string(noise.unknown_terms.size()));
    }
  }

  // ---------------------------------------------------------------------
  // C. P1-4 the PRF and the sample gain enter the pixel integral
  // ---------------------------------------------------------------------
  std::printf("\n== C. P1-4 the pixel response and the sample gain enter the integral ==\n");
  {
    auto index = std::make_shared<Index>();
    index->size = 1;
    index->kind = Index::Kind::native;
    index->identity.object_id = "probe-prf";
    index->assessment.complete = true;
    auto footprints = std::make_shared<FootprintSet>();
    footprints->index = index;
    footprints->width = 1;
    footprints->height = 1;
    footprints->detector_from_sample = Mat2{1.0, 0.0, 0.0, 1.0};
    footprints->offset = Vec2{0.0, 0.0};
    footprints->local_cell = Box{Vec2{-0.5, -0.5}, Vec2{0.5, 0.5}};
    footprints->detector_frame = "probe";
    footprints->assessment.complete = true;

    Basis basis;
    basis.kind = Basis::Kind::constant;
    basis.degree = 0;
    const std::vector<double> coefficients{0.0, 0.0, 0.0};  // sigma = 1, rho = 0
    const std::vector<PixelCenter0> positions{PixelCenter0{0.0, 0.0}};
    const std::vector<U64> rows{0};
    const Unit dimensionless = model::unit_dimensionless();

    // The reference: (erf(0.5/sqrt 2))^2.
    const double half = 0.5 / std::sqrt(2.0);
    const double erf_half = std::erf(half);
    const double reference = erf_half * erf_half;

    const auto build = [&](const FootprintSet& set) {
      return sources::psfmodel::build_templates(ctx, PsfRequest::Family::gaussian, coefficients,
                                                basis, positions, set, rows, 1e-12);
    };
    Result<sources::psfmodel::TemplateBlock> plain = build(*footprints);
    check(plain.code == Code::ok && std::fabs(plain.value->values[0] - reference) < 1e-10,
          "P1-4 the analytic Gaussian pixel integral is unchanged",
          "value " + std::to_string(plain.code == Code::ok ? plain.value->values[0] : -1.0));

    // (i) The sample response gain.
    auto gained = std::make_shared<FootprintSet>(*footprints);
    gained->sample_response_gain = store(ctx, storage, {2.0}, {1}, dimensionless);
    Result<sources::psfmodel::TemplateBlock> with_gain = build(*gained);
    check(with_gain.code == Code::ok &&
              std::fabs(with_gain.value->values[0] - 2.0 * reference) < 1e-10,
          "P1-4 the sample response gain multiplies the integral",
          "value " + std::to_string(with_gain.code == Code::ok ? with_gain.value->values[0] : -1.0));

    // (ii) The intra-pixel PRF.  A uniform two-cell response with
    // relative_mean_one must reproduce the plain integral exactly; then the
    // gains (3,1) must give 3/2 + 1/2 halves.
    auto prf = std::make_shared<FootprintSet>(*footprints);
    auto response = std::make_shared<PixelResponse>();
    response->cells = {Box{Vec2{-0.5, -0.5}, Vec2{0.0, 0.5}},
                       Box{Vec2{0.0, -0.5}, Vec2{0.5, 0.5}}};
    response->gains = store(ctx, storage, {1.0, 1.0}, {2}, dimensionless);
    response->relative_mean_one = true;
    prf->pixel_response = response;
    Result<sources::psfmodel::TemplateBlock> uniform_prf = build(*prf);
    check(uniform_prf.code == Code::ok &&
              std::fabs(uniform_prf.value->values[0] - reference) < 1e-10,
          "P1-4 a relative_mean_one PRF reproduces the uniform integral",
          "value " + std::to_string(uniform_prf.code == Code::ok ? uniform_prf.value->values[0] : -1.0));

    auto graded = std::make_shared<FootprintSet>(*footprints);
    auto graded_response = std::make_shared<PixelResponse>(*response);
    graded_response->gains = store(ctx, storage, {3.0, 1.0}, {2}, dimensionless);
    graded->pixel_response = graded_response;
    Result<sources::psfmodel::TemplateBlock> with_prf = build(*graded);
    // Each half carries half of the x integral: 0.5 * erf_half per cell, so the
    // weighted sum is (3*0.5 + 1*0.5) * erf_half^2 = 2 * reference.
    const double expected_prf = 2.0 * reference;
    check(with_prf.code == Code::ok &&
              std::fabs(with_prf.value->values[0] - expected_prf) < 1e-10,
          "P1-4 the PRF cell gains weight the pixel integral",
          "value " + std::to_string(with_prf.code == Code::ok ? with_prf.value->values[0] : -1.0) +
              " expected " + std::to_string(expected_prf));
    check(with_prf.code == Code::ok &&
              std::fabs(with_prf.value->tails[0] - (1.0 - reference)) < 1e-10,
          "P1-4 the tail stays the unweighted positive model mass",
          "tail " + std::to_string(with_prf.code == Code::ok ? with_prf.value->tails[0] : -1.0));

    // (iii) A colour dependent PRF is refused by name, not by picking a band.
    auto multi = std::make_shared<FootprintSet>(*footprints);
    auto multi_response = std::make_shared<PixelResponse>(*response);
    multi_response->gains = store(ctx, storage, {2.0, 0.0, 1.0, 1.0}, {2, 2}, dimensionless);
    multi->pixel_response = multi_response;
    Result<sources::psfmodel::TemplateBlock> with_multi = build(*multi);
    check(with_multi.code == Code::unsupported,
          "P1-4 a multi-band PRF is refused as unsupported",
          "code " + std::to_string((int)with_multi.code));
  }

  std::printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL PROBES PASS" : "PROBES FAILED", failures);
  return failures == 0 ? 0 : 1;
}
