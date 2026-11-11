#include <cstdio>
#include <vector>
#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "observation/observation.hpp"
#include "operators/linear_action.hpp"
#include "storage/local_storage.hpp"
#include "validation/stats/harness.hpp"

using namespace acsd;
using namespace acsd::model;

int main() {
  acsd_validation::CountingResources resources;
  acsd_validation::NoCancellation cancellation;
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  auto index = std::make_shared<Index>();
  index->size = 2; index->kind = Index::Kind::native;
  index->identity = make_identity("acsd.Index", 2, 0, sha256_hex(std::string("info-index")), "info");
  auto h = storage.copy(context, DType::f64, encode_f64(std::vector<double>{1.0, 0.0}),
                        std::vector<U64>{2}, unit_dimensionless(), index->identity.object_id);

  auto factor = observation::make_diagonal_action(
      context, std::vector<double>{2.0, 3.0}, index, unit_dimensionless(), unit_dimensionless());
  operators::CsrMatrix identity;
  identity.rows = 2; identity.cols = 2; identity.offsets = {0, 1, 2};
  identity.indices = {0, 1}; identity.values = {1.0, 1.0};
  auto influence = operators::make_csr_action(context, 2, 2, identity.offsets, identity.indices,
                                              identity.values, index->identity.object_id,
                                              index->identity.object_id, unit_dimensionless());
  auto root = std::make_shared<NoiseRoot>();
  root->coordinates = index;
  root->factor = *factor.value;
  root->unit = unit_dimensionless();
  auto model = std::make_shared<NoiseModel>();
  model->innovations = {root};
  model->influence = {*influence.value};
  model->variance_unit = unit_variance("adu");
  model->state = NoiseState::known;
  model->assessment = make_assessment(Code::ok);
  model->identity = make_identity("acsd.NoiseModel", 2, 0, sha256_hex(std::string("info-noise")), "info");
  Result<Ref<CovarianceAction>> cov = make_covariance(context, Ref<NoiseModel>(model));
  std::printf("cov code=%d\n", (int)cov.code);
  if (!ok(cov)) return 1;

  InformationRequest request;
  request.h = *h.value;
  request.covariance = *cov.value;
  request.reference = Estimator{"flux"};
  request.solve.rank_relative = 1e-12;
  Result<Information> info = estimate_flux_information(context, request);
  std::printf("info code=%d state=%d value=%s\n", (int)info.code,
              (int)(*info.value).flux_variance.state,
              (*info.value).flux_variance.value ? std::to_string(*(*info.value).flux_variance.value).c_str()
                                                : "null");
  // C = diag(4,9), h = [1,0]  =>  V_F = 4.
  bool first = ok(info) && (*info.value).flux_variance.value.has_value() &&
               std::fabs(*(*info.value).flux_variance.value - 4.0) <= 1e-12;

  auto unknown_model = std::make_shared<NoiseModel>(*model);
  unknown_model->state = NoiseState::unknown;
  unknown_model->identity = make_identity("acsd.NoiseModel", 2, 0, sha256_hex(std::string("info-noise-unknown")), "info");
  Result<Ref<CovarianceAction>> cov_unknown =
      make_covariance(context, Ref<NoiseModel>(unknown_model));
  request.covariance = *cov_unknown.value;
  Result<Information> unknown = estimate_flux_information(context, request);
  bool second = ok(unknown) && !(*unknown.value).flux_variance.value.has_value() &&
                (*unknown.value).flux_variance.state == NoiseState::unknown;
  std::printf("known V_F=4: %s; unknown propagated: %s\n", first ? "ok" : "FAIL",
              second ? "ok" : "FAIL");
  return first && second ? 0 : 1;
}
