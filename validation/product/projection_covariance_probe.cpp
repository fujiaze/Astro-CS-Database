#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
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
static Result<Ref<ProductProvider>> openp(Context& c, const std::string& dir) {
  std::ifstream f(dir + "/manifest.json", std::ios::binary);
  std::string t((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  ProductLocation l; l.directory = dir;
  l.manifest_hash = "sha256:" + sha256_hex(std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(t.data()), t.size()));
  return open_product(c, l);
}
static std::vector<double> read(Context& c, ArrayRef a, U64 n) {
  Result<std::vector<double>> v = operators::read_f64(c, a, 0, n);
  return ok(v) ? val(v) : std::vector<double>{};
}
int main(int argc, char** argv) {
  if (argc < 3) return 2;
  acsd_validation::CountingResources r; acsd_validation::NoCancellation n;
  storage::LocalStorage s; Context c{r, n, s};
  auto exp = openp(c, argv[1]); auto mos = openp(c, argv[2]);
  if (!ok(exp) || !ok(mos)) return 1;
  Ref<ProductGraph> eg = (*exp.value)->graph();
  Ref<CovarianceAction> cout = eg->signal->noise;
  Ref<LinearAction> q;
  for (const Ref<Model>& m : eg->closure) {
    if (m && m->identity.schema_id == "acsd.LinearAction") {
      q = std::static_pointer_cast<const LinearAction>(m);
      break;
    }
  }
  if (!q) { std::printf("no Q in closure\n"); return 1; }
  Ref<CovarianceAction> cleaf = (*mos.value)->graph()->signal->noise;
  const U64 np = q->spaces.codomain ? q->spaces.codomain->size : 0;
  const U64 nl = q->spaces.domain ? q->spaces.domain->size : 0;
  std::printf("npix=%llu nleaf=%llu\n", (unsigned long long)np, (unsigned long long)nl);
  std::vector<double> x(static_cast<std::size_t>(np));
  for (U64 i = 0; i < np; ++i) x[i] = std::sin(0.3 * static_cast<double>(i) + 0.2);
  Result<ArrayRef> xa = c.storage.copy(c, DType::f64, encode_f64(x), std::vector<U64>{np},
                                       unit_dimensionless(), Id());
  Result<ArrayRef> cx = covariance_apply(c, cout, *xa.value);
  Result<ArrayRef> u = q->transpose_apply(c, *xa.value);
  if (!ok(cx) || !ok(u)) {
    if (cout && cout->model && cout->model->state != NoiseState::known) {
      std::printf("C_out state is unknown; apply correctly refused\n");
      return 0;
    }
    std::printf("step failed\n");
    return 1;
  }
  Result<ArrayRef> v = covariance_apply(c, cleaf, *u.value);
  if (!ok(v)) { std::printf("leaf cov failed\n"); return 1; }
  Result<ArrayRef> w = apply(c, q, *v.value);
  if (!ok(w)) { std::printf("Q apply failed\n"); return 1; }
  auto l = read(c, *cx.value, np); auto rr = read(c, *w.value, np);
  double num = 0, den = 0;
  for (U64 i = 0; i < np; ++i) { num = std::max(num, std::fabs(l[i] - rr[i])); den = std::max(den, std::fabs(l[i])); }
  std::printf("max_abs_diff=%.6g max_abs=%.6g rel=%.3g\n", num, den, num / (den + 1e-300));
  return num <= 1e-9 * (den + 1e-300) ? 0 : 1;
}
