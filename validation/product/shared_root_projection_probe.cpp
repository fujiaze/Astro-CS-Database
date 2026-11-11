// Independent multi-leaf / shared-root projection check.
//
// Two leaves, Q = [[1,0],[1,1]].  The shared root has total influence
// L = [1,2]^T, so C = L L^T and Q C Q^T = [[1,3],[3,9]].  The same total
// influence is also built from two roots with the same SharedId, each acting
// on one leaf; make_covariance must merge them before squaring, keeping the
// cross term.
#include <cmath>
#include <cstdio>
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

namespace {

Result<Ref<LinearAction>> csr(Context& c, U64 rows, U64 cols, std::vector<U64> off,
                              std::vector<U64> idx, std::vector<double> val) {
  return operators::make_csr_action(c, rows, cols, off, idx, val, Id(), Id(),
                                     unit_dimensionless());
}

Result<Ref<CovarianceAction>> model_with_roots(Context& c, bool split) {
  auto factor = csr(c, 1, 1, {0, 1}, {0}, {1.0});
  if (!ok(factor)) return model::fail<Ref<CovarianceAction>>(factor.code, "factor", "probe", "no");
  auto make_root = [&]() {
    auto root = std::make_shared<NoiseRoot>();
    root->shared_id = SharedId{"ns", "entity", "rev", "comp"};
    root->unit = unit_dimensionless();
    root->factor = *factor.value;
    return root;
  };
  auto model = std::make_shared<NoiseModel>();
  model->variance_unit = unit_variance("test");
  model->state = NoiseState::known;
  if (split) {
    auto l1 = csr(c, 2, 1, {0, 1, 1}, {0}, {1.0});  // [1,0]^T
    auto l2 = csr(c, 2, 1, {0, 0, 1}, {0}, {2.0});  // [0,2]^T
    if (!ok(l1) || !ok(l2)) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model, "influences", "probe", "no");
    }
    model->shared_factors = {make_root(), make_root()};
    model->influence = {*l1.value, *l2.value};
  } else {
    auto l = csr(c, 2, 1, {0, 1, 2}, {0, 0}, {1.0, 2.0});  // [1,2]^T
    if (!ok(l)) {
      return model::fail<Ref<CovarianceAction>>(Code::invalid_model, "influence", "probe", "no");
    }
    model->shared_factors = {make_root()};
    model->influence = {*l.value};
  }
  model->identity = make_identity("acsd.NoiseModel", 2, 0,
                                  sha256_hex(std::string("shared-root:") + (split ? "split" : "one")),
                                  "test");
  return make_covariance(c, Ref<NoiseModel>(model));
}

bool check(Context& c, bool split) {
  Result<Ref<CovarianceAction>> cov = model_with_roots(c, split);
  if (!ok(cov)) {
    std::printf("%s: make_covariance failed %d\n", split ? "split" : "single", (int)cov.code);
    return false;
  }
  auto q = csr(c, 2, 2, {0, 1, 3}, {0, 0, 1}, {1.0, 1.0, 1.0});
  if (!ok(q)) return false;
  Result<Ref<CovarianceAction>> projected = control_covariance(c, *cov.value, *q.value);
  if (!ok(projected)) {
    std::printf("%s: projection failed %d\n", split ? "split" : "single", (int)projected.code);
    return false;
  }
  const double expected[2][2] = {{1.0, 3.0}, {3.0, 9.0}};
  double got[2][2] = {{0, 0}, {0, 0}};
  for (int k = 0; k < 2; ++k) {
    std::vector<double> e{0.0, 0.0};
    e[k] = 1.0;
    Result<ArrayRef> input = c.storage.copy(c, DType::f64, encode_f64(e), std::vector<U64>{2},
                                             unit_dimensionless(), Id());
    Result<ArrayRef> out = covariance_apply(c, *projected.value, *input.value);
    if (!ok(out)) return false;
    Result<std::vector<double>> vals = operators::read_f64(c, *out.value, 0, 2);
    for (int i = 0; i < 2; ++i) got[i][k] = val(vals)[i];
  }
  double err = 0.0;
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j) err = std::max(err, std::fabs(got[i][j] - expected[i][j]));
  std::printf("%s: C_out=[%.3f %.3f; %.3f %.3f] max_abs_err=%.3g\n", split ? "split" : "single",
              got[0][0], got[0][1], got[1][0], got[1][1], err);
  return err <= 1e-12;
}

}  // namespace

int main() {
  acsd_validation::CountingResources resources;
  acsd_validation::NoCancellation cancellation;
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};
  const bool single = check(context, false);
  const bool split = check(context, true);
  std::printf("shared-root projection: single=%d split=%d\n", single ? 1 : 0, split ? 1 : 0);
  return single && split ? 0 : 1;
}
