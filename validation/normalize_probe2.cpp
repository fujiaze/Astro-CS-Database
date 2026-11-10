// Independent fixed-point probes for the SECOND round of the Normalize
// remediation (B2, review-normalize-fix.md).
//
//   A. R1 / R1b: the numerical radius of measure_science_sources.
//      A1  the audit counterexample C2 still encloses [8, 12];
//      A2  a large STATISTICAL data residual does not enter the radius while
//          the normal equation residual ||D^T r|| stays at the rounding level;
//      A3  the R1b counterexample D=[[1,0],[0,1],[0.9,0.9],[0.9,0.900001]] is
//          refused with the spectral norm bound even though the max row norm
//          rule would certify it;
//      A4  a non finite design error and a perturbation past the contraction
//          boundary are refused instead of certified.
//      The production function measure_flux_numerical_radius (defined in
//      src/sources/measure.cpp) is called; the inverse, the row norms, the
//      solution and the pre-fix radius are computed independently here with a
//      Gaussian elimination (the formula is transcribed only for the PRE-fix
//      comparison).
//
//   B. R2: the master root role of observation.calibrate.  The production
//      assemble_calibrated_native is called.  "gain" is not a flat, a declared
//      role whose array is absent is unknown (never the f=1 flat derivative and
//      never a silent zero), and a role written in the component field is
//      unknown.
//
//   C. R3: the counts covariance fallback.  run_joint_fit is called on a two
//      frame problem whose first frame publishes a correlated flux block and
//      whose second frame publishes none.  The fixed fallback must produce the
//      SAME fit as the all-diagonal problem; the matrix the old fallback left
//      behind (diagonal overwritten, off diagonal kept) is supplied explicitly
//      as a third configuration and produces a different chi square.
//
//   D. R4: two frames that share one master root must record the omitted cross
//      frame covariance; a single frame with the same root must not.
//
//   E. P1-2: a source without a measured XP covariance makes the parameter
//      covariance conditional: run_joint_fit reports covariance_state_known =
//      false and fit_photometry publishes the covariance NoiseModel with
//      state = unknown plus the explicit working prior term.
//
// Build:
//   g++ -std=c++20 -O2 -fno-fast-math -ffp-contract=off -frounding-math \
//       -mfpmath=sse -I src -I include validation/normalize_probe2.cpp \
//       -Wl,--start-group build/science/libsources.a build/science/libobservation.a \
//       build/science/libphotometry.a build/science/libresponse.a \
//       build/science/liboperators.a build/science/libuncertainty.a \
//       build/science/libnumerics.a build/science/libmodel.a \
//       build/science/libstorage.a build/science/libresources.a \
//       build/science/libruntime.a -Wl,--end-group -lpthread -o /tmp/normalize_probe2
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
#include "numerics/linalg.hpp"
#include "observation/calibration_kernel.hpp"
#include "photometry/joint_fit.hpp"
#include "photometry/spatial_basis.hpp"

using namespace acsd;

// The production enclosure (definition in src/sources/measure.cpp).  The
// probe does not redeclare the algorithm, it links the production one.
namespace acsd {
std::vector<double> measure_flux_numerical_radius(
    const numerics::Matrix& design, std::size_t sources, const std::vector<double>& measurement,
    const std::vector<double>& solution, const std::vector<double>& row_norm,
    const numerics::SvdResult& svd, const std::vector<double>& column_scale, double data_norm,
    double design_norm, double& amplification, double& spectral_norm,
    double& normal_residual_norm);
}

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
    const std::size_t element = array->dtype == DType::u16 ? 2 : 8;
    v.bytes = std::span<const std::byte>(buf->data() + range.begin * element,
                                         range.count * element);
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
  std::printf("%-56s %s  %s\n", name, condition ? "PASS" : "FAIL", detail.c_str());
  if (!condition) ++failures;
}

ArrayRef store(Context& ctx, Sto& storage, const std::vector<double>& values,
               const std::vector<U64>& shape, const Unit& unit, const Id& index = Id()) {
  return *storage.copy(ctx, DType::f64, model::encode_f64(values), shape, unit, index).value;
}
ArrayRef store_u64(Context& ctx, Sto& storage, const std::vector<U64>& values,
                   const std::vector<U64>& shape, const Unit& unit) {
  return *storage.copy(ctx, DType::u64, model::encode_u64(values), shape, unit, Id()).value;
}
std::vector<double> read_f64(Context& ctx, Sto& storage, ArrayRef array) {
  Result<ArrayView> view = storage.read(ctx, array, BlockRequest{0, array->shape[0]});
  if (view.code != Code::ok) return {};
  return model::decode_f64(view.value->bytes);
}

// ---------------------------------------------------------------------------
// Simple dense linear algebra owned by the probe (not the production code).
// ---------------------------------------------------------------------------
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

// (D^T D)^-1 D^T by an independent elimination: the pseudo inverse of a full
// rank design.
std::vector<std::vector<double>> pseudo_inverse_dense(const std::vector<std::vector<double>>& d) {
  const std::size_t rows = d.size();
  const std::size_t parameters = d[0].size();
  std::vector<std::vector<double>> gram(parameters, std::vector<double>(parameters, 0.0));
  for (std::size_t a = 0; a < parameters; ++a) {
    for (std::size_t b = 0; b < parameters; ++b) {
      double sum = 0.0;
      for (std::size_t k = 0; k < rows; ++k) sum += d[k][a] * d[k][b];
      gram[a][b] = sum;
    }
  }
  std::vector<std::vector<double>> inverse(parameters, std::vector<double>(parameters, 0.0));
  for (std::size_t col = 0; col < parameters; ++col) {
    std::vector<double> unit(parameters, 0.0);
    unit[col] = 1.0;
    const std::vector<double> x = solve_dense(gram, unit);
    for (std::size_t row = 0; row < parameters; ++row) inverse[row][col] = x[row];
  }
  // (D^T D)^-1 D^T: parameters x rows.
  std::vector<std::vector<double>> out(parameters, std::vector<double>(rows, 0.0));
  for (std::size_t c = 0; c < parameters; ++c) {
    for (std::size_t k = 0; k < rows; ++k) {
      double sum = 0.0;
      for (std::size_t j = 0; j < parameters; ++j) sum += inverse[c][j] * d[k][j];
      out[c][k] = sum;
    }
  }
  return out;
}

struct DesignCase {
  std::vector<std::vector<double>> d;
  std::vector<double> measurement;
  std::vector<double> data_error;      // per row, |nominal - farther endpoint|
  std::vector<double> template_error;  // per row, per source
  std::size_t sources = 1;
  std::size_t parameters = 2;
};

struct RadiusCase {
  std::vector<double> radius;  // empty when the production code refuses
  std::vector<double> solution;
  std::vector<double> row_norm;  // row norms of D^+, independent
  double old_radius = 0.0;       // transcribed pre-fix radius (largest component)
  double new_radius = 0.0;       // largest production radius
  double amplification = 0.0;
  double spectral_norm = 0.0;
  double normal_residual = 0.0;
  double residual_l2 = 0.0;
};

RadiusCase radius_case(const DesignCase& design) {
  RadiusCase out;
  const std::size_t rows = design.d.size();
  const std::size_t parameters = design.parameters;
  // The independent inverse and the nominal solution.
  const std::vector<std::vector<double>> pseudo = pseudo_inverse_dense(design.d);
  out.solution.assign(parameters, 0.0);
  for (std::size_t c = 0; c < parameters; ++c) {
    double sum = 0.0;
    for (std::size_t k = 0; k < rows; ++k) sum += pseudo[c][k] * design.measurement[k];
    out.solution[c] = sum;
  }
  // The row norms of D^+ (one per parameter; the flux rows are the first
  // `sources`).
  out.row_norm.assign(parameters, 0.0);
  for (std::size_t c = 0; c < parameters; ++c) {
    double sum = 0.0;
    for (std::size_t k = 0; k < rows; ++k) sum += pseudo[c][k] * pseudo[c][k];
    out.row_norm[c] = std::sqrt(sum);
  }
  // The design norms as measure uses them.
  double data_norm = 0.0;
  for (double error : design.data_error) data_norm += error * error;
  data_norm = std::sqrt(data_norm);
  double design_norm = 0.0;
  for (std::size_t k = 0; k < rows; ++k) {
    for (std::size_t c = 0; c < design.sources; ++c) {
      design_norm += design.template_error[k * design.sources + c] *
                     design.template_error[k * design.sources + c];
    }
  }
  design_norm = std::sqrt(design_norm);
  // The statistical residual norm ||r|| and the normal equation residual.
  double residual = 0.0;
  std::vector<double> gradient(parameters, 0.0);
  for (std::size_t k = 0; k < rows; ++k) {
    double model = 0.0;
    for (std::size_t c = 0; c < parameters; ++c) model += design.d[k][c] * out.solution[c];
    const double r = design.measurement[k] - model;
    residual += r * r;
    for (std::size_t c = 0; c < parameters; ++c) gradient[c] += design.d[k][c] * r;
  }
  out.residual_l2 = std::sqrt(residual);
  double gradient_norm = 0.0;
  for (double value : gradient) gradient_norm += value * value;
  out.normal_residual = std::sqrt(gradient_norm);
  // The PRE-fix radius, transcribed from the code at 48eaf0612:
  //   1/(1 - max_row_norm * ||dD||) * row_norm_s * (||dd|| + ||dD|| ||theta|| + ||r||)
  double solution_norm = 0.0;
  for (double value : out.solution) solution_norm += value * value;
  solution_norm = std::sqrt(solution_norm);
  double largest_row = 0.0;
  for (std::size_t s = 0; s < design.sources; ++s) {
    largest_row = std::max(largest_row, out.row_norm[s]);
  }
  const double old_contraction = largest_row * design_norm;
  if (old_contraction < 1.0) {
    out.old_radius = (1.0 / (1.0 - old_contraction)) * out.row_norm[0] *
                     (data_norm + design_norm * solution_norm + out.residual_l2);
  } else {
    out.old_radius = std::numeric_limits<double>::infinity();
  }
  // The production enclosure.  The design of measure is the COLUMN SCALED one
  // D S^-1; its SVD is what the production helper consumes.
  numerics::Matrix matrix(rows, parameters);
  for (std::size_t k = 0; k < rows; ++k) {
    for (std::size_t c = 0; c < parameters; ++c) matrix(k, c) = design.d[k][c];
  }
  std::vector<double> column_scale(parameters, 0.0);
  for (std::size_t c = 0; c < parameters; ++c) {
    double sum = 0.0;
    for (std::size_t k = 0; k < rows; ++k) sum += design.d[k][c] * design.d[k][c];
    column_scale[c] = std::sqrt(sum);
  }
  numerics::Matrix scaled(rows, parameters);
  for (std::size_t k = 0; k < rows; ++k) {
    for (std::size_t c = 0; c < parameters; ++c) {
      scaled(k, c) = column_scale[c] > 0.0 ? design.d[k][c] / column_scale[c] : 0.0;
    }
  }
  const numerics::SvdResult svd = numerics::jacobi_svd(scaled, 1.0e-12);
  out.radius = measure_flux_numerical_radius(matrix, design.sources, design.measurement,
                                             out.solution, out.row_norm, svd, column_scale,
                                             data_norm, design_norm, out.amplification,
                                             out.spectral_norm, out.normal_residual);
  for (double value : out.radius) out.new_radius = std::max(out.new_radius, value);
  return out;
}

// ---------------------------------------------------------------------------
// A. R1 / R1b
// ---------------------------------------------------------------------------
void probe_r1() {
  std::printf("\n== A. R1 the radius is a numerical enclosure, not a statistical interval ==\n");
  {
    // A1: the audit counterexample C2 must still be enclosed.
    DesignCase design;
    design.d = {{0.5, 1.0}, {0.5, 0.0}};
    design.measurement = {5.0, 5.0};
    design.data_error = {0.0, 1.0};
    design.template_error = {0.0, 0.0};
    design.sources = 1;
    design.parameters = 2;
    const RadiusCase out = radius_case(design);
    char detail[256];
    std::snprintf(detail, sizeof(detail),
                  "radius %.9f interval [%.9f, %.9f]; pre-fix radius %.6f",
                  out.new_radius, out.solution[0] - out.new_radius,
                  out.solution[0] + out.new_radius, out.old_radius);
    check(out.new_radius >= 2.0 - 1e-12 && out.solution[0] - out.new_radius <= 8.0 + 1e-9 &&
              out.solution[0] + out.new_radius >= 12.0 - 1e-9,
          "R1/C2 the enclosure still contains the corner truths [8,12]", detail);
  }
  {
    // A2: a realistic design with a large statistical residual.
    const std::size_t rows = 160;
    DesignCase design;
    design.parameters = 2;
    design.sources = 1;
    std::vector<double> profile(rows, 0.0);
    double sum = 0.0;
    for (std::size_t k = 0; k < rows; ++k) {
      const double x = static_cast<double>(k) - 0.5 * static_cast<double>(rows - 1);
      profile[k] = std::exp(-0.5 * (x / 6.0) * (x / 6.0));
      sum += profile[k];
    }
    for (double& value : profile) value /= sum;  // the template integrates to one
    std::uint64_t lcg = 12345;
    for (std::size_t k = 0; k < rows; ++k) {
      lcg = lcg * 6364136223846793005ull + 1442695040888963407ull;
      const double noise =
          30.0 * (static_cast<double>((lcg >> 33) % 10000) / 10000.0 - 0.5) * 2.0;
      const double sky = 300.0;
      design.d.push_back({profile[k], 1.0});
      design.measurement.push_back(sky + 1000.0 * profile[k] + noise);
      design.data_error.push_back(1e-12 * std::fabs(design.measurement.back()));
      design.template_error.push_back(1e-15 * std::fabs(profile[k]));
    }
    const RadiusCase out = radius_case(design);
    const double flux = out.solution[0];
    char detail[512];
    std::snprintf(detail, sizeof(detail),
                  "F=%.6f ||r||=%.3f ||D^T r||=%.3e pre-fix radius=%.3f new radius=%.3e "
                  "(new/F=%.2e) spectral=%.3f",
                  flux, out.residual_l2, out.normal_residual, out.old_radius, out.new_radius,
                  out.new_radius / std::fabs(flux), out.spectral_norm);
    check(!out.radius.empty(), "R1/A2 the statistical residual case is certified", detail);
    check(out.residual_l2 > 100.0,
          "R1/A2 the data residual is large (statistical, not numerical)",
          "||r|| = " + std::to_string(out.residual_l2));
    check(out.new_radius < 1e-6,
          "R1/A2 the new radius is a floating point enclosure, not ||r||",
          "new radius " + std::to_string(out.new_radius) + " ADU, pre-fix " +
              std::to_string(out.old_radius) + " ADU");
    check(out.old_radius > 100.0 * out.new_radius,
          "R1/A2 the pre-fix radius was dominated by ||r||",
          "pre-fix/new = " + std::to_string(out.old_radius / out.new_radius));
    // ||D^T r|| must sit at the rounding level relative to ||D|| ||r||: the
    // exact least squares solution has D^T r = 0 identically.
    double design_frobenius = 0.0;
    for (const auto& row : design.d) {
      for (double value : row) design_frobenius += value * value;
    }
    design_frobenius = std::sqrt(design_frobenius);
    const double cosine = out.normal_residual / (design_frobenius * out.residual_l2);
    char cos_detail[256];
    std::snprintf(cos_detail, sizeof(cos_detail),
                  "||D^T r||/(||D||_F ||r||) = %.3e (exact least squares has 0)", cosine);
    check(cosine < 1e-8, "R1/A2 the normal equation residual is a rounding defect", cos_detail);
  }
  {
    // A3: the R1b counterexample.  The max row norm rule certifies; the
    // spectral norm must refuse.
    DesignCase design;
    design.d = {{1.0, 0.0}, {0.0, 1.0}, {0.9, 0.9}, {0.9, 0.900001}};
    design.measurement = {5.0, 5.0, 9.0, 9.000002};
    design.data_error = {0.0, 0.0, 0.0, 0.0};
    design.template_error = {0.0, 0.0, 1.001, 0.0};  // ||dD||_F = 1.001
    design.sources = 1;
    design.parameters = 2;
    const RadiusCase out = radius_case(design);
    char detail[512];
    std::snprintf(detail, sizeof(detail),
                  "max row norm=%.6f, pre-fix contraction=%.6f (certified), ||D^+||_2=%.6f, "
                  "spectral contraction=%.6f",
                  out.row_norm[0], out.row_norm[0] * 1.001, out.spectral_norm,
                  out.spectral_norm * 1.001);
    check(out.old_radius < std::numeric_limits<double>::infinity(),
          "R1b/A3 the max row norm rule would certify the counterexample", detail);
    check(out.radius.empty(),
          "R1b/A3 the spectral norm bound refuses the counterexample", detail);
    check(out.spectral_norm > out.row_norm[0] * 1.25,
          "R1b/A3 the spectral norm is materially above the max row norm",
          "spectral " + std::to_string(out.spectral_norm) + " vs row " +
              std::to_string(out.row_norm[0]));
  }
  {
    // A4: non finite input is refused.
    DesignCase design;
    design.d = {{0.5, 1.0}, {0.5, 0.0}};
    design.measurement = {5.0, 5.0};
    design.data_error = {0.0, 1.0};
    design.template_error = {0.0, 0.0};
    design.sources = 1;
    design.parameters = 2;
    numerics::Matrix matrix(2, 2);
    matrix(0, 0) = 0.5;
    matrix(0, 1) = 1.0;
    matrix(1, 0) = 0.5;
    matrix(1, 1) = 0.0;
    const numerics::SvdResult svd = numerics::jacobi_svd(matrix, 1.0e-12);
    const std::vector<double> column_scale{0.7071067811865476, 1.0};
    const std::vector<double> solution{10.0, 0.0};
    const std::vector<double> row_norm{2.0};
    const std::vector<double> measurement{5.0, 5.0};
    double amplification = 0.0;
    double spectral = 0.0;
    double normal = 0.0;
    const std::vector<double> nan_radius = measure_flux_numerical_radius(
        matrix, 1, measurement, solution, row_norm, svd, column_scale,
        std::numeric_limits<double>::quiet_NaN(), 0.0, amplification, spectral, normal);
    check(nan_radius.empty(), "R1/A4 a non finite data norm is refused",
          "radius count " + std::to_string(nan_radius.size()));
    const std::vector<double> huge_radius = measure_flux_numerical_radius(
        matrix, 1, measurement, solution, row_norm, svd, column_scale, 0.0, 1.0e300,
        amplification, spectral, normal);
    check(huge_radius.empty(), "R1/A4 a perturbation past the contraction boundary is refused",
          "radius count " + std::to_string(huge_radius.size()));
  }
}

// ---------------------------------------------------------------------------
// B. R2: the master root role
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

Ref<LinearAction> identity_action(Context& ctx, Ref<Index> index, const Unit& unit) {
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::identity;
  recipe.algorithm = "probe_identity";
  recipe.version = 1;
  recipe.spaces.domain = index;
  recipe.spaces.codomain = index;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  Result<Ref<LinearAction>> action =
      make_action(ctx, recipe, std::span<const Ref<LinearAction>>{}, std::span<const ArrayRef>{});
  if (action.code != Code::ok) return Ref<LinearAction>();
  return *action.value;
}

// A dense sparse row action: y = M x.  Same construction as the production
// make_dense_action (offsets, indices, values).
Ref<LinearAction> dense_action(Context& ctx, Sto& storage, const numerics::Matrix& matrix,
                               Ref<Index> domain, Ref<Index> codomain, const Unit& unit) {
  std::vector<U64> offsets(matrix.rows + 1, 0);
  std::vector<U64> indices;
  std::vector<double> values;
  for (std::size_t row = 0; row < matrix.rows; ++row) {
    for (std::size_t column = 0; column < matrix.cols; ++column) {
      const double value = matrix(row, column);
      if (value != 0.0) {
        indices.push_back(static_cast<U64>(column));
        values.push_back(value);
      }
    }
    offsets[row + 1] = static_cast<U64>(values.size());
  }
  ArrayRef offset_array = store_u64(ctx, storage, offsets, {static_cast<U64>(offsets.size())}, unit);
  ArrayRef index_array = store_u64(ctx, storage, indices, {static_cast<U64>(indices.size())}, unit);
  ArrayRef value_array = store(ctx, storage, values, {static_cast<U64>(values.size())}, unit);
  ActionRecipe recipe;
  recipe.kind = ActionRecipe::Kind::csr;
  recipe.algorithm = "probe_csr";
  recipe.version = 1;
  recipe.arrays = {"offsets", "indices", "values"};
  recipe.spaces.domain = domain;
  recipe.spaces.codomain = codomain;
  recipe.spaces.input = unit;
  recipe.spaces.output = unit;
  const std::vector<ArrayRef> arrays{offset_array, index_array, value_array};
  Result<Ref<LinearAction>> action =
      make_action(ctx, recipe, std::span<const Ref<LinearAction>>{}, arrays);
  if (action.code != Code::ok) return Ref<LinearAction>();
  return *action.value;
}

Ref<NoiseModel> master_noise(Context& ctx, Sto& storage, Ref<Index> index, const Unit& adu,
                             const SharedId& shared_id, double sigma, bool shared) {
  Ref<LinearAction> factor = diagonal_action(ctx, storage, {sigma}, index, adu, adu);
  Ref<LinearAction> identity = identity_action(ctx, index, adu);
  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = shared_id;
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
  noise->influence.push_back(identity);
  noise->state = NoiseState::known;
  noise->assessment = model::make_assessment(Code::ok);
  return noise;
}

void probe_r2() {
  std::printf("\n== B. R2 the master root role is never guessed ==\n");
  const Unit adu{"detector0", 0, 0, 0, 1.0, 1};
  const Unit variance_unit{"detector0", 0, 0, 0, 1.0, 2};
  auto index = std::make_shared<Index>();
  index->size = 1;
  index->kind = Index::Kind::native;
  index->identity.object_id = "probe-native";
  index->assessment.complete = true;
  Sto storage;
  Res resources;
  NoCancel cancel;
  Context ctx{resources, cancel, storage};
  auto raw_observation = std::make_shared<RawObservation>();
  raw_observation->width = 1;
  raw_observation->height = 1;
  raw_observation->context.epoch_julian_year = 2016.0;
  raw_observation->source_index = index;
  raw_observation->frame = "probe-frame";
  raw_observation->identity = model::make_identity("acsd.RawObservation", 2, 0, "probe", "acsd");
  Calibration request;
  request.exposure_seconds = 200.0;
  request.dark_exposure_seconds = 100.0;
  request.flat_normalization = 1.0;
  request.dark_contains_bias = false;
  request.bias = store(ctx, storage, {0.0}, {1}, adu);
  request.dark = store(ctx, storage, {1.0}, {1}, adu);
  request.flat = store(ctx, storage, {2.0}, {1}, adu);
  request.raw_noise = master_noise(ctx, storage, index, adu,
                                   SharedId{"acsd.native", "input", "1", "amplitude"}, 0.1, false);
  const std::vector<double> values{4.0};
  const std::vector<double> lowers{4.0 - 1e-12};
  const std::vector<double> uppers{4.0 + 1e-12};
  const std::vector<std::uint16_t> flags{0};
  ArrayRef value_array = store(ctx, storage, values, {1}, variance_unit);
  ArrayRef lower_array = store(ctx, storage, lowers, {1}, variance_unit);
  ArrayRef upper_array = store(ctx, storage, uppers, {1}, variance_unit);
  std::vector<std::byte> dq_bytes(2);
  std::memcpy(dq_bytes.data(), flags.data(), 2);
  const std::vector<U64> dq_shape{1};
  ArrayRef dq_array = *storage.copy(ctx, DType::u16, std::span<const std::byte>(dq_bytes), dq_shape,
                                    model::unit_dimensionless(), Id()).value;
  observation::CalibrationAssemblyInputs inputs;
  const std::vector<double> raw_vector{10.0};
  const std::vector<double> bias_vector{0.0};
  const std::vector<double> flat_vector{2.0};
  inputs.raw = std::span<const double>(raw_vector);
  inputs.bias = std::span<const double>(bias_vector);
  inputs.flat = std::span<const double>(flat_vector);
  inputs.values = std::span<const double>(values);
  inputs.flags = std::span<const std::uint16_t>(flags);
  modules::CalibrationSampleStats stats;

  const auto assemble = [&](const Calibration& calibration) {
    return observation::assemble_calibrated_native(ctx, raw_observation, calibration, inputs,
                                                   value_array, lower_array, upper_array,
                                                   dq_array, stats);
  };
  const auto has_unknown_role = [](const NoiseModel& noise) {
    for (const UnknownTerm& term : noise.unknown_terms) {
      if (term.id.rfind("master_root_role_unknown", 0) == 0) return true;
    }
    return false;
  };
  const auto variance_of = [&](Ref<NoiseModel> noise) {
    Result<Ref<CovarianceAction>> covariance = make_covariance(ctx, noise);
    if (covariance.code != Code::ok) return -1.0;
    ArrayRef unit = store(ctx, storage, {1.0}, {1}, model::unit_dimensionless());
    Result<ArrayRef> applied = covariance_apply(ctx, *covariance.value, unit);
    if (applied.code != Code::ok) return -1.0;
    Result<ArrayView> view = storage.read(ctx, *applied.value, BlockRequest{0, 1});
    if (view.code != Code::ok) return -1.0;
    double value = -1.0;
    std::memcpy(&value, view.value->bytes.data(), 8);
    return value;
  };

  {
    // The valid flat root still enters the Jacobian: (0.1/2)^2 + (4/2)^2 sigma_f^2
    // with sigma_f = 0.1 -> 0.0025 + 0.04.
    Calibration flat_request = request;
    flat_request.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master_flat", "1", "amplitude"}, 0.1,
        true);
    Result<Ref<NativeObservation>> native = assemble(flat_request);
    double variance = -1.0;
    if (native.code == Code::ok) variance = variance_of((*native.value)->noise);
    char detail[256];
    std::snprintf(detail, sizeof(detail), "variance %.9f expected %.9f", variance,
                  0.0025 + 0.04);
    check(native.code == Code::ok && std::fabs(variance - 0.0425) < 1e-12,
          "R2 the flat role still carries (-x/flat)^2 sigma_f^2", detail);
  }
  {
    // "gain" is not a flat.
    Calibration gain_request = request;
    gain_request.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master_gain", "1", "amplitude"}, 1.0,
        true);
    Result<Ref<NativeObservation>> native = assemble(gain_request);
    const NoiseModel* noise = native.code == Code::ok ? (*native.value)->noise.get() : nullptr;
    check(noise != nullptr && noise->shared_factors.empty() && has_unknown_role(*noise),
          "R2 entity gain is unknown, not the flat derivative",
          noise == nullptr ? std::string("assembly failed")
                           : "shared roots " + std::to_string(noise->shared_factors.size()) +
                                 " unknown terms " + std::to_string(noise->unknown_terms.size()));
  }
  {
    // A flat root with no flat array.
    Calibration no_flat = request;
    no_flat.flat = nullptr;
    no_flat.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master_flat", "1", "amplitude"}, 1.0,
        true);
    observation::CalibrationAssemblyInputs no_flat_inputs = inputs;
    no_flat_inputs.flat = std::span<const double>();
    Result<Ref<NativeObservation>> native = observation::assemble_calibrated_native(
        ctx, raw_observation, no_flat, no_flat_inputs, value_array, lower_array, upper_array,
        dq_array, stats);
    const NoiseModel* noise = native.code == Code::ok ? (*native.value)->noise.get() : nullptr;
    check(noise != nullptr && noise->shared_factors.empty() && has_unknown_role(*noise),
          "R2 a flat root without a flat map is unknown, not -x",
          noise == nullptr ? std::string("assembly failed")
                           : "shared roots " + std::to_string(noise->shared_factors.size()));
  }
  {
    // A dark root with no dark array; a bias root with no bias array.
    Calibration no_dark = request;
    no_dark.dark = nullptr;
    no_dark.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master_dark", "1", "amplitude"}, 1.0,
        true);
    Result<Ref<NativeObservation>> dark_native = assemble(no_dark);
    const NoiseModel* dark_noise =
        dark_native.code == Code::ok ? (*dark_native.value)->noise.get() : nullptr;
    check(dark_noise != nullptr && dark_noise->shared_factors.empty() &&
              has_unknown_role(*dark_noise),
          "R2 a dark root without a dark map is unknown, not a silent zero",
          dark_noise == nullptr ? std::string("assembly failed")
                                : "shared roots " +
                                      std::to_string(dark_noise->shared_factors.size()));
    Calibration no_bias = request;
    no_bias.bias = nullptr;
    no_bias.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master_bias", "1", "amplitude"}, 1.0,
        true);
    Result<Ref<NativeObservation>> bias_native = assemble(no_bias);
    const NoiseModel* bias_noise =
        bias_native.code == Code::ok ? (*bias_native.value)->noise.get() : nullptr;
    check(bias_noise != nullptr && bias_noise->shared_factors.empty() &&
              has_unknown_role(*bias_noise),
          "R2 a bias root without a bias map is unknown, not a silent zero",
          bias_noise == nullptr ? std::string("assembly failed")
                                : "shared roots " +
                                      std::to_string(bias_noise->shared_factors.size()));
  }
  {
    // The role written in the component field is not read.
    Calibration component_request = request;
    component_request.master_noise = master_noise(
        ctx, storage, index, adu, SharedId{"acsd.native", "master", "1", "flat"}, 1.0, true);
    Result<Ref<NativeObservation>> native = assemble(component_request);
    const NoiseModel* noise = native.code == Code::ok ? (*native.value)->noise.get() : nullptr;
    bool mentions_component = false;
    if (noise != nullptr) {
      for (const UnknownTerm& term : noise->unknown_terms) {
        if (term.reason.find("component field") != std::string::npos) mentions_component = true;
      }
    }
    check(noise != nullptr && noise->shared_factors.empty() && has_unknown_role(*noise) &&
              mentions_component,
          "R2 a role in the component field is unknown and says so",
          noise == nullptr ? std::string("assembly failed")
                           : "unknown terms " + std::to_string(noise->unknown_terms.size()));
  }
}

// ---------------------------------------------------------------------------
// C / D / E: the joint fit
// ---------------------------------------------------------------------------
Ref<Index> make_index(U64 size, const std::string& name, Index::Kind kind) {
  auto index = std::make_shared<Index>();
  index->size = size;
  index->kind = kind;
  index->identity.object_id = name;
  index->assessment.complete = true;
  return index;
}

std::vector<double> lower_chol(const numerics::Matrix& covariance) {
  // A lower triangular factor L with L L^T = covariance (Cholesky).
  const std::size_t n = covariance.rows;
  numerics::Matrix l(n, n);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j <= i; ++j) {
      double sum = covariance(i, j);
      for (std::size_t k = 0; k < j; ++k) sum -= l(i, k) * l(j, k);
      if (i == j) {
        l(i, j) = std::sqrt(std::max(0.0, sum));
      } else {
        l(i, j) = l(j, j) > 0.0 ? sum / l(j, j) : 0.0;
      }
    }
  }
  std::vector<double> out(n * n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    for (std::size_t j = 0; j < n; ++j) out[i * n + j] = l(i, j);
  }
  return out;
}

// The covariance action the measurements of one frame publish: C = I C_latent
// I^T with the frame influence I and one latent root.  The latent root factor
// is the lower Cholesky factor of the requested flux covariance.
Ref<CovarianceAction> flux_covariance(Context& ctx, Sto& storage, const numerics::Matrix& c,
                                      const std::vector<SharedId>& shared_ids) {
  const std::size_t n = c.rows;
  auto index = make_index(n, "probe-flux", Index::Kind::native);
  const Unit unit = model::unit_variance("probe");
  auto noise = std::make_shared<NoiseModel>();
  noise->variance_unit = unit;
  noise->condition = model::make_condition("probe_flux", "probe");
  Ref<LinearAction> identity = identity_action(ctx, index, unit);
  if (shared_ids.empty()) {
    // One innovation root carrying the whole covariance.
    numerics::Matrix factor(n, n);
    const std::vector<double> chol = lower_chol(c);
    for (std::size_t i = 0; i < n; ++i) {
      for (std::size_t j = 0; j < n; ++j) factor(i, j) = chol[i * n + j];
    }
    auto root = std::make_shared<NoiseRoot>();
    root->shared_id = SharedId{"acsd.native", "probe", "1", "amplitude"};
    root->coordinates = index;
    root->unit = unit;
    root->factor = dense_action(ctx, storage, factor, index, index, unit);
    root->assessment = model::make_assessment(Code::ok);
    noise->innovations.push_back(root);
    noise->influence.push_back(identity);
  } else {
    // One shared root per requested identity, each diagonal; used to exercise
    // the cross frame detection.
    for (const SharedId& id : shared_ids) {
      auto root = std::make_shared<NoiseRoot>();
      root->shared_id = id;
      root->coordinates = index;
      root->unit = unit;
      root->factor = diagonal_action(ctx, storage, {10.0, 1.0}, index, unit, unit);
      root->assessment = model::make_assessment(Code::ok);
      noise->shared_factors.push_back(root);
      noise->influence.push_back(identity);
    }
  }
  noise->state = NoiseState::known;
  noise->assessment = model::make_assessment(Code::ok);
  auto covariance = std::make_shared<CovarianceAction>();
  covariance->model = noise;
  covariance->influence = nullptr;
  covariance->input_output.domain = index;
  covariance->input_output.codomain = index;
  covariance->input_output.input = unit;
  covariance->input_output.output = unit;
  covariance->assessment = model::make_assessment(Code::ok);
  return std::shared_ptr<const CovarianceAction>(covariance);
}

struct FitCase {
  photometry::FitResult result;
  Code code = Code::ok;
  std::vector<Ref<SourceMeasurements>> measurements;
};

// Two frames, one star, the first frame measures the star twice with a
// correlated flux block, the second frame does not publish a joint covariance.
FitCase run_case(Context& ctx, Sto& storage, bool frame0_covariance, bool frame1_covariance,
                 bool frame0_old_residual) {
  FitCase out;
  Basis basis;
  basis.kind = Basis::Kind::constant;
  basis.degree = 0;
  std::vector<photometry::StarInput> stars(1);
  stars[0].source_id = 7;
  stars[0].gaia = {1.0};
  std::vector<photometry::FrameInput> frames(2);
  frames[0].frame = "frame-0";
  frames[0].v = {1.0};
  frames[0].dv = {0.0};
  frames[0].counts = {100.0, 110.0};
  frames[0].variance = {4.0, 4.0};
  frames[0].flux_low = {100.0 - 1e-9, 110.0 - 1e-9};
  frames[0].flux_high = {100.0 + 1e-9, 110.0 + 1e-9};
  frames[0].position_x = {0.0, 1.0};
  frames[0].position_y = {0.0, 0.0};
  frames[0].star = {0, 0};
  frames[1].frame = "frame-1";
  frames[1].v = {1.0};
  frames[1].dv = {0.0};
  frames[1].counts = {105.0};
  frames[1].variance = {4.0};
  frames[1].flux_low = {105.0 - 1e-9};
  frames[1].flux_high = {105.0 + 1e-9};
  frames[1].position_x = {0.5};
  frames[1].position_y = {0.0};
  frames[1].star = {0};
  auto measurement0 = std::make_shared<SourceMeasurements>();
  measurement0->frame = "frame-0";
  auto measurement1 = std::make_shared<SourceMeasurements>();
  measurement1->frame = "frame-1";
  if (frame0_covariance) {
    numerics::Matrix c(3, 3);
    c(0, 0) = 4.0;
    c(0, 1) = 3.0;
    c(1, 0) = 3.0;
    c(1, 1) = 4.0;
    c(2, 2) = 4.0;
    if (frame0_old_residual) {
      // The matrix the pre-fix fallback left behind: the diagonal was
      // overwritten by the declared variance while the off diagonal block of
      // frame 0 stayed.
      c(0, 0) = 4.0;
      c(1, 1) = 4.0;
    }
    measurement0->joint_noise = flux_covariance(ctx, storage, c, {});
  }
  if (frame1_covariance) {
    numerics::Matrix c(1, 1);
    c(0, 0) = 4.0;
    measurement1->joint_noise = flux_covariance(ctx, storage, c, {});
  }
  frames[0].measurements = measurement0.get();
  frames[1].measurements = measurement1.get();
  out.measurements = {measurement0, measurement1};
  std::vector<double> weighted_mean{0.0};
  Result<photometry::FitResult> fit =
      photometry::run_joint_fit(ctx, frames, stars, basis, weighted_mean,
                                photometry::FitSettings{});
  out.code = fit.code;
  if (fit.code == Code::ok) out.result = *fit.value;
  return out;
}

// Frames that carry a shared master root on their measurement covariance.
FitCase run_shared_root_case(Context& ctx, Sto& storage, bool two_frames) {
  FitCase out;
  Basis basis;
  basis.kind = Basis::Kind::constant;
  basis.degree = 0;
  std::vector<photometry::StarInput> stars(1);
  stars[0].source_id = 7;
  stars[0].gaia = {1.0};
  std::vector<photometry::FrameInput> frames(two_frames ? 2 : 1);
  for (std::size_t k = 0; k < frames.size(); ++k) {
    frames[k].frame = "frame-" + std::to_string(k);
    frames[k].v = {1.0};
    frames[k].dv = {0.0};
    frames[k].counts = {1000.0};
    frames[k].variance = {100.0};
    frames[k].flux_low = {1000.0 - 1e-9};
    frames[k].flux_high = {1000.0 + 1e-9};
    frames[k].position_x = {0.0};
    frames[k].position_y = {0.0};
    frames[k].star = {0};
  }
  auto measurement0 = std::make_shared<SourceMeasurements>();
  measurement0->frame = "frame-0";
  numerics::Matrix c(2, 2);
  c(0, 0) = 100.0;
  c(1, 1) = 1.0;
  measurement0->joint_noise =
      flux_covariance(ctx, storage, c, {SharedId{"acsd.native", "master_flat", "1", "amplitude"}});
  auto measurement1 = std::make_shared<SourceMeasurements>();
  measurement1->frame = "frame-1";
  if (two_frames) measurement1->joint_noise = measurement0->joint_noise;
  frames[0].measurements = measurement0.get();
  if (two_frames) frames[1].measurements = measurement1.get();
  out.measurements = {measurement0};
  if (two_frames) out.measurements.push_back(measurement1);
  std::vector<double> weighted_mean{0.0};
  Result<photometry::FitResult> fit =
      photometry::run_joint_fit(ctx, frames, stars, basis, weighted_mean,
                                photometry::FitSettings{});
  out.code = fit.code;
  if (fit.code == Code::ok) out.result = *fit.value;
  return out;
}

bool contains(const std::vector<std::string>& terms, const std::string& needle) {
  for (const std::string& term : terms) {
    if (term.find(needle) != std::string::npos) return true;
  }
  return false;
}

void probe_r3(Context& ctx, Sto& storage) {
  std::printf("\n== C. R3 the counts covariance fallback is a pure diagonal ==\n");
  const FitCase none = run_case(ctx, storage, false, false, false);
  const FitCase partial = run_case(ctx, storage, true, false, false);
  const FitCase old_matrix = run_case(ctx, storage, true, true, true);
  char detail[512];
  std::snprintf(detail, sizeof(detail),
                "none chi2=%.9f theta[0]=%.9f, partial chi2=%.9f theta[0]=%.9f",
                none.result.chi_square, none.result.theta.empty() ? 0.0 : none.result.theta[0],
                partial.result.chi_square,
                partial.result.theta.empty() ? 0.0 : partial.result.theta[0]);
  check(partial.code == Code::ok && none.code == Code::ok,
          "R3/R3a both configurations fit", detail);
  check(std::fabs(partial.result.chi_square - none.result.chi_square) < 1e-9,
          "R3 a partial covariance falls back to the SAME diagonal fit", detail);
  check(partial.result.theta.size() == none.result.theta.size() &&
            std::fabs(partial.result.theta[0] - none.result.theta[0]) < 1e-9,
          "R3 the scale estimate of the partial case equals the diagonal case", detail);
  check(std::fabs(none.result.chi_square - 12.5) < 1e-6,
          "R3 the hand computed diagonal chi2 is 12.5", detail);
  check(contains(partial.result.unknown_terms, "off diagonal terms are unknown"),
          "R3 the fallback records the unknown off diagonal terms",
          "unknown terms " + std::to_string(partial.result.unknown_terms.size()));
  check(contains(partial.result.unknown_terms, "were discarded, not partially kept"),
          "R3 the record says the earlier blocks were discarded",
          "unknown terms " + std::to_string(partial.result.unknown_terms.size()));
  char old_detail[512];
  std::snprintf(old_detail, sizeof(old_detail),
                "matrix the old fallback left behind: chi2=%.9f (diagonal %.9f, retained "
                "correlation %.6f)",
                old_matrix.result.chi_square, none.result.chi_square,
                std::fabs(old_matrix.result.chi_square - none.result.chi_square) /
                    none.result.chi_square);
  check(old_matrix.result.chi_square > 3.9 * none.result.chi_square,
          "R3 the pre-fix residual matrix changes chi2 by a factor four", old_detail);
}

void probe_r4(Context& ctx, Sto& storage) {
  std::printf("\n== D. R4 a cross frame shared master root is an explicit unknown ==\n");
  const FitCase two = run_shared_root_case(ctx, storage, true);
  const FitCase one = run_shared_root_case(ctx, storage, false);
  check(two.code == Code::ok && one.code == Code::ok, "R4 both shared root cases fit",
        "code " + std::to_string((int)two.code) + "/" + std::to_string((int)one.code));
  const bool recorded = contains(two.result.unknown_terms, "master root") &&
                        contains(two.result.unknown_terms, "shared by the frames");
  check(recorded, "R4 the shared root of two frames is recorded as an unknown",
        "unknown terms " + std::to_string(two.result.unknown_terms.size()));
  check(contains(two.result.notes, "Cov(F_k,F_l)"),
        "R4 the record names the omitted cross covariance", "");
  check(!contains(one.result.unknown_terms, "shared by the frames"),
        "R4 a single frame with the same root records nothing", "");
  // The exact small example of the review: F = 1000 ADU, a relative flat
  // amplitude sigma_f = 0.01 gives a per frame flux move of 10 ADU and a cross
  // covariance of 100 ADU^2, while the block diagonal assembly holds zero.
  const double flux = 1000.0;
  const double sigma = 0.01;
  const double cross = flux * sigma * flux * sigma;
  const double block_diagonal = 0.0;
  char detail[256];
  std::snprintf(detail, sizeof(detail),
                "Cov(F0,F1) = F^2 sigma_f^2 = %.3f ADU^2; block diagonal assembly = %.1f", cross,
                block_diagonal);
  check(std::fabs(cross - 100.0) < 1e-9 && block_diagonal == 0.0,
          "R4 the worked example: current 0 vs correct 100 ADU^2", detail);
  check(contains(two.result.notes, "100 ADU"),
          "R4 the published note carries the worked example number", "");
}

// ---------------------------------------------------------------------------
// E. P1-2
// ---------------------------------------------------------------------------
void probe_p12(Context& ctx, Sto& storage) {
  std::printf("\n== E. P1-2 the parameter covariance of a working prior is unknown ==\n");
  const FitCase working = run_case(ctx, storage, false, false, false);
  check(working.result.covariance_state_known == false,
          "P1-2 the working prior marks the covariance conditional", "");

  // A measured XP covariance keeps the covariance known.
  {
    Basis basis;
    basis.kind = Basis::Kind::constant;
    basis.degree = 0;
    std::vector<photometry::StarInput> stars(1);
    stars[0].source_id = 7;
    stars[0].gaia = {1.0};
    numerics::Matrix c(1, 1);
    c(0, 0) = 1e-4;
    stars[0].noise = flux_covariance(ctx, storage, c, {});
    std::vector<photometry::FrameInput> frames(1);
    frames[0].frame = "frame-0";
    frames[0].v = {1.0};
    frames[0].dv = {0.0};
    frames[0].counts = {100.0};
    frames[0].variance = {4.0};
    frames[0].flux_low = {100.0 - 1e-9};
    frames[0].flux_high = {100.0 + 1e-9};
    frames[0].position_x = {0.0};
    frames[0].position_y = {0.0};
    frames[0].star = {0};
    auto measurement = std::make_shared<SourceMeasurements>();
    measurement->frame = "frame-0";
    frames[0].measurements = measurement.get();
    std::vector<double> weighted_mean{0.0};
    Result<photometry::FitResult> fit = photometry::run_joint_fit(
        ctx, frames, stars, basis, weighted_mean, photometry::FitSettings{});
    check(fit.code == Code::ok && (*fit.value).covariance_state_known,
          "P1-2 a measured XP covariance keeps the covariance known",
          fit.code == Code::ok ? std::string("") : std::string("fit failed"));
  }

  // The published covariance of fit_photometry: state unknown, an explicit
  // working prior term, and the covariance is still applicable (the known
  // directions are not deleted).
  {
    auto spectral_basis = std::make_shared<SpectralBasis>();
    spectral_basis->identity = model::make_identity("acsd.SpectralBasis", 2, 0, "probe-basis", "acsd");
    auto response = std::make_shared<Response>();
    response->identity = model::make_identity("acsd.Response", 2, 0, "probe-response", "acsd");
    response->convention = Response::Convention::photon_throughput;
    response->support_nm = Interval{300.0, 900.0};
    auto prediction = std::make_shared<SpectralPrediction>();
    prediction->identity =
        model::make_identity("acsd.SpectralPrediction", 2, 0, "probe-prediction", "acsd");
    prediction->basis = spectral_basis;
    prediction->response = response;
    prediction->support_nm = Interval{300.0, 900.0};
    prediction->basis_integrals = store(ctx, storage, {2.0}, {1}, model::unit_dimensionless());
    prediction->absolute_errors = store(ctx, storage, {0.0}, {1}, model::unit_dimensionless());

    CatalogueStar star{};
    star.source_id = 7;
    star.ra_rad = 0.1;
    star.dec_rad = 0.2;
    star.epoch_year = 2016.0;
    star.brightness_mag = 12.0;
    star.xp_basis = spectral_basis;
    star.xp_coefficients = store(ctx, storage, {1.0}, {1}, model::unit_dimensionless());

    auto measurement = std::make_shared<SourceMeasurements>();
    measurement->frame = "frame-0";
    StarFlux flux;
    flux.gaia_source_id = 7;
    flux.position = PixelCenter0{0.0, 0.0};
    flux.integrated_flux = 100.0;
    flux.variance.value = 4.0;
    flux.variance.unit = model::unit_variance("probe");
    flux.variance.state = NoiseState::known;
    measurement->stars.push_back(flux);
    measurement->flux_numerical_lower =
        store(ctx, storage, {100.0 - 1e-9}, {1}, model::unit_variance("probe"));
    measurement->flux_numerical_upper =
        store(ctx, storage, {100.0 + 1e-9}, {1}, model::unit_variance("probe"));

    PhotometryRequest request;
    request.responses = {response};
    request.basis = Basis{};
    request.basis.kind = Basis::Kind::constant;
    request.basis.degree = 0;
    request.frames = {"frame-0"};
    request.application_id = "probe-application";
    request.likelihood = model::make_condition("photometric_scale", "joint_flux_gaussian");
    const std::vector<Ref<SourceMeasurements>> measurements{measurement};
    const std::vector<CatalogueStar> star_list{star};
    const std::vector<Ref<SpectralPrediction>> predictions{prediction};
    Result<Ref<PhotoSolution>> solution = fit_photometry(
        ctx, std::span<const Ref<SourceMeasurements>>(measurements),
        std::span<const CatalogueStar>(star_list),
        std::span<const Ref<SpectralPrediction>>(predictions), request);
    if (solution.code != Code::ok) {
      check(false, "P1-2 fit_photometry publishes the covariance",
            "code " + std::to_string((int)solution.code));
    } else {
      const PhotoSolution& published = **solution.value;
      bool state_unknown = false;
      bool prior_term = false;
      if (published.covariance && published.covariance->model) {
        state_unknown = published.covariance->model->state == NoiseState::unknown;
        for (const UnknownTerm& term : published.covariance->model->unknown_terms) {
          if (term.id == "parameter_covariance_uses_a_working_prior") prior_term = true;
        }
      }
      check(state_unknown, "P1-2 the published covariance model state is unknown",
            published.covariance && published.covariance->model
                ? std::string("state ") +
                      std::to_string((int)published.covariance->model->state)
                : std::string("no covariance"));
      check(prior_term, "P1-2 the working prior is an explicit unknown term", "");
      // The known directions stay applicable: the unknown carries no influence.
      bool applicable = false;
      if (published.covariance) {
        ArrayRef unit = store(ctx, storage, {1.0}, {1}, model::unit_dimensionless());
        Result<ArrayRef> applied = covariance_apply(ctx, published.covariance, unit);
        applicable = applied.code == Code::ok;
      }
      check(applicable, "P1-2 the covariance is still applicable on the measured directions", "");
      check(published.assessment.science == ScienceState::conditional,
            "P1-2 the published science state is conditional",
            "science " + std::to_string((int)published.assessment.science));
    }
  }
}

}  // namespace

int main() {
  probe_r1();
  probe_r2();
  Sto storage;
  Res resources;
  NoCancel cancel;
  Context ctx{resources, cancel, storage};
  probe_r3(ctx, storage);
  probe_r4(ctx, storage);
  probe_p12(ctx, storage);
  std::printf("\n%s (%d failure(s))\n", failures == 0 ? "ALL PROBES PASS" : "PROBES FAILED",
              failures);
  return failures == 0 ? 0 : 1;
}
