#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "storage/local_storage.hpp"
#include "validation/stats/harness.hpp"
using namespace acsd;
using namespace acsd::model;

static double dot(const std::vector<double>& a, const std::vector<double>& b) {
  double s = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
  return s;
}
static std::vector<double> apply(Context& ctx, Ref<CovarianceAction> cov,
                                 const std::vector<double>& x) {
  Result<ArrayRef> a = ctx.storage.copy(ctx, DType::f64, encode_f64(x),
                                        std::vector<U64>{x.size()}, unit_dimensionless(), Id());
  Result<ArrayRef> out = covariance_apply(ctx, cov, *a.value);
  if (!ok(out)) return {};
  Result<std::vector<double>> v = operators::read_f64(ctx, *out.value, 0, x.size());
  return ok(v) ? val(v) : std::vector<double>{};
}

int main(int argc, char** argv) {
  if (argc < 2) return 2;
  acsd_validation::CountingResources resources;
  acsd_validation::NoCancellation cancellation;
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};
  std::ifstream manifest(std::string(argv[1]) + "/manifest.json", std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
  ProductLocation location;
  location.directory = argv[1];
  location.manifest_hash = "sha256:" + sha256_hex(std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(text.data()), text.size()));
  Result<Ref<ProductProvider>> opened = open_product(context, location);
  if (!ok(opened)) return 1;
  auto estimate = (*opened.value)->graph()->signal;
  if (!estimate || !estimate->noise) { std::printf("no noise\n"); return 1; }
  Ref<CovarianceAction> cov = estimate->noise;
  const U64 n = cov->input_output.domain ? cov->input_output.domain->size : 0;
  std::printf("domain=%llu state=%d\n", (unsigned long long)n, (int)cov->assessment.noise);
  const Unit sig = estimate->unit;
  const Unit var = model::unit_multiply(sig, sig);
  std::printf("signal_unit=%s(sr=%d,L=%d) var_unit=%s(sr=%d,L=%d) cov_unit=%s(sr=%d,L=%d)\n",
              sig.scale.c_str(), sig.sr_power, sig.linear_power, var.scale.c_str(), var.sr_power,
              var.linear_power, cov->model->variance_unit.scale.c_str(),
              cov->model->variance_unit.sr_power, cov->model->variance_unit.linear_power);
  std::vector<double> x(static_cast<std::size_t>(n)), y(static_cast<std::size_t>(n));
  for (U64 i = 0; i < n; ++i) {
    x[i] = std::sin(0.7 * static_cast<double>(i) + 0.1);
    y[i] = std::cos(1.3 * static_cast<double>(i) + 0.4);
  }
  std::vector<double> cx = apply(context, cov, x);
  std::vector<double> cy = apply(context, cov, y);
  if (cx.empty() || cy.empty()) { std::printf("apply failed\n"); return 1; }
  const double xcy = dot(x, cy);
  const double ycx = dot(y, cx);
  const double xcx = dot(x, cx);
  std::printf("xCy=%.12g yCx=%.12g sym_rel=%.3g xCx=%.12g\n", xcy, ycx,
              std::fabs(xcy - ycx) / (std::fabs(xcy) + 1e-300), xcx);
  const bool sym = std::fabs(xcy - ycx) <= 1e-9 * (std::fabs(xcy) + std::fabs(ycx) + 1e-300);
  const bool psd = xcx >= -1e-9 * (std::fabs(xcx) + 1e-300);
  std::printf("sym=%d psd=%d\n", sym ? 1 : 0, psd ? 1 : 0);
  return sym && psd ? 0 : 1;
}
