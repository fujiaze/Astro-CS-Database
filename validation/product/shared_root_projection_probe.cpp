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

int main() {
  acsd_validation::CountingResources r; acsd_validation::NoCancellation n;
  storage::LocalStorage s; Context c{r, n, s};

  auto make_action = [&](U64 rows, U64 cols, std::vector<U64> off, std::vector<U64> idx,
                         std::vector<double> val) {
    return operators::make_csr_action(c, rows, cols, off, idx, val, Id(), Id(),
                                      unit_dimensionless());
  };
  // Two shared roots with the same identity, acting on different leaves.
  auto factor = make_action(1, 1, {0, 1}, {0}, {1.0});
  auto make_shared_root = [&]() {
    auto root = std::make_shared<NoiseRoot>();
    root->shared_id = SharedId{"ns", "entity", "rev", "comp"};
    root->unit = unit_dimensionless();
    root->factor = *factor.value;
    return root;
  };
  auto shared_a = make_shared_root();
  auto shared_b = make_shared_root();

  auto l1 = make_action(2, 1, {0, 1, 2}, {0, 0}, {1.0, 2.0});
  auto model = std::make_shared<NoiseModel>();
  model->shared_factors = {shared_a};
  model->influence = {*l1.value};
  model->variance_unit = unit_variance("test");
  model->state = NoiseState::known;
  model->identity = make_identity("acsd.NoiseModel", 2, 0, sha256_hex(std::string("shared-root")), "test");
  Result<Ref<CovarianceAction>> cov = make_covariance(c, Ref<NoiseModel>(model));
  if (!ok(cov)) return 1;
  if (!ok(cov)) return 1;

  // Q = [[1,0],[1,1]]: rows are target pixels, columns leaves.
  auto q = make_action(2, 2, {0, 1, 3}, {0, 0, 1}, {1.0, 1.0, 1.0});
  if (!ok(q)) return 1;
  Result<Ref<CovarianceAction>> projected = control_covariance(c, *cov.value, *q.value);
  if (!ok(projected)) { std::printf("projection failed %d\n", (int)projected.code); return 1; }

  // Expected C_out = Q C Q^T, C = [1,2]^T [1,2] = [[1,2],[2,4]].
  const double expected[2][2] = {{1.0, 3.0}, {3.0, 9.0}};
  double got[2][2] = {{0, 0}, {0, 0}};
  for (int k = 0; k < 2; ++k) {
    std::vector<double> e{0.0, 0.0};
    e[k] = 1.0;
    Result<ArrayRef> input = c.storage.copy(c, DType::f64, encode_f64(e), std::vector<U64>{2},
                                             unit_dimensionless(), Id());
    Result<ArrayRef> out = covariance_apply(c, *projected.value, *input.value);
    if (!ok(out)) { std::printf("apply failed %d\n", (int)out.code); return 1; }
    Result<std::vector<double>> vals = operators::read_f64(c, *out.value, 0, 2);
    for (int i = 0; i < 2; ++i) got[i][k] = val(vals)[i];
  }
  double err = 0.0;
  for (int i = 0; i < 2; ++i)
    for (int j = 0; j < 2; ++j) err = std::max(err, std::fabs(got[i][j] - expected[i][j]));
  std::printf("C_out=[%.3f %.3f; %.3f %.3f] max_abs_err=%.3g\n", got[0][0], got[0][1], got[1][0],
              got[1][1], err);
  return err <= 1e-12 ? 0 : 1;
}
