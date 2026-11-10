#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "acsd/contracts.hpp"
#include "storage/local_storage.hpp"
#include "model/array.hpp"
#include "observation/observation.hpp"
#include "model/units.hpp"
#include "validation/stats/harness.hpp"

using namespace acsd;
using namespace acsd;
using namespace acsd::model;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int main() {
  std::filesystem::remove_all("/tmp/rtprod");
  acsd_validation::CountingResources resources;
  acsd_validation::NoCancellation cancellation;
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  auto index = std::make_shared<Index>();
  index->size = 4;
  index->kind = Index::Kind::leaf;
  index->order = 6;
  index->identity = make_identity("acsd.Index", 2, 0, sha256_hex(std::string("rt-index")), "rt");

  auto signal_values = std::vector<double>{1.0, 2.0, 3.0, 4.0};
  auto signal_bytes = model::encode_f64(signal_values);
  Result<ArrayRef> signal = storage.copy(context, DType::f64, signal_bytes, std::vector<U64>{4},
                                         unit_dimensionless(), index->identity.object_id);
  if (!signal.value) { std::printf("signal array failed\n"); return 1; }

  auto factors = std::vector<double>{2.0, 2.0, 2.0, 2.0};
  Result<Ref<LinearAction>> factor = observation::make_diagonal_action(
      context, factors, std::shared_ptr<const Index>(index), unit_dimensionless(),
      unit_dimensionless());
  if (!factor.value) { std::printf("factor failed\n"); return 1; }

  auto root = std::make_shared<NoiseRoot>();
  root->shared_id = SharedId{"rt", "rt-root", "1", "rt"};
  root->coordinates = std::shared_ptr<const Index>(index);
  root->unit = unit_dimensionless();
  root->factor = *factor.value;
  root->identity = make_identity("acsd.NoiseRoot", 2, 0, sha256_hex(std::string("rt-root")), "rt");

  auto noise = std::make_shared<NoiseModel>();
  noise->variance_unit = unit_dimensionless();
  noise->condition = make_condition("rt", "exact");
  noise->innovations.push_back(root);
  auto ones_factors = std::vector<double>{1.0, 1.0, 1.0, 1.0};
  Result<Ref<LinearAction>> influence = observation::make_diagonal_action(
      context, ones_factors, std::shared_ptr<const Index>(index), unit_dimensionless(),
      unit_dimensionless());
  noise->influence.push_back(*influence.value);
  noise->state = NoiseState::known;
  noise->identity = make_identity("acsd.NoiseModel", 2, 0, sha256_hex(std::string("rt-noise")), "rt");

  auto covariance = std::make_shared<CovarianceAction>();
  covariance->model = noise;
  covariance->influence = nullptr;
  covariance->input_output.domain = std::shared_ptr<const Index>(index);
  covariance->input_output.codomain = std::shared_ptr<const Index>(index);
  covariance->input_output.input = unit_dimensionless();
  covariance->input_output.output = unit_dimensionless();
  covariance->identity = make_identity("acsd.CovarianceAction", 2, 0,
                                       sha256_hex(std::string("rt-cov")), "rt");


  auto states_bytes = std::vector<std::byte>{std::byte{1}, std::byte{1}, std::byte{1}, std::byte{1}};
  Result<ArrayRef> states = storage.copy(context, DType::u8, states_bytes, std::vector<U64>{4},
                                         unit_dimensionless(), index->identity.object_id);
  auto support = std::make_shared<SupportMap>();
  support->kind = SupportMap::Kind::full_leaf_approx;
  support->index = std::shared_ptr<const Index>(index);
  support->states = *states.value;
  support->identity = make_identity("acsd.SupportMap", 2, 0, sha256_hex(std::string("rt-support")), "rt");

  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = *signal.value;
  estimate->support = support;
  estimate->unit = unit_dimensionless();
  estimate->noise = covariance;
  estimate->identity = make_identity("acsd.SignalEstimate", 2, 0,
                                     sha256_hex(std::string("rt-estimate")), "rt");
  estimate->assessment = make_assessment(Code::ok);
  estimate->assessment.complete = true;
  estimate->assessment.noise = NoiseState::known;

  auto graph = std::make_shared<ProductGraph>();
  graph->stage = ProductGraph::Stage::mosaic;
  graph->signal = estimate;
  graph->profile = "full";
  graph->identity = make_identity("acsd.ProductGraph", 2, 0,
                                  sha256_hex(std::string("rt-graph")), "rt");
  graph->assessment = make_assessment(Code::ok);
  graph->assessment.complete = true;

  PublishRequest request;
  request.destination = "/tmp/rtprod";
  request.signal_dtype = DType::f64;
  request.profile = "full";
  request.tile_log2_width = 6;
  request.durable = false;
  Result<ProductLocation> published = publish_product(context, graph, request);
  check(published.code == Code::ok, "publish full with a covariance");
  if (published.code != Code::ok) {
    for (const auto& issue : published.assessment.issues) std::printf("  issue: %s\n", issue.reason.c_str());
    return 1;
  }
  std::ifstream manifest("/tmp/rtprod/manifest.json", std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
  const std::string hash = "sha256:" + sha256_hex(std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(text.data()), text.size()));
  ProductLocation location;
  location.directory = "/tmp/rtprod";
  location.manifest_hash = hash;
  Result<Ref<ProductProvider>> opened = open_product(context, location);
  check(opened.code == Code::ok, "reopen the product");
  if (opened.code != Code::ok) {
    for (const auto& issue : opened.assessment.issues) std::printf("  issue: %s\n", issue.reason.c_str());
    return 1;
  }
  Ref<ProductGraph> reopened = (*opened.value)->graph();
  check(reopened && reopened->signal, "the reopened graph has a signal");
  check(reopened && reopened->signal && reopened->signal->noise, "the reopened noise is not null");
  if (reopened && reopened->signal && reopened->signal->noise) {
    auto ones = std::vector<double>{1.0, 1.0, 1.0, 1.0};
    auto ones_bytes = model::encode_f64(ones);
    Result<ArrayRef> input = storage.copy(context, DType::f64, ones_bytes, std::vector<U64>{4},
                                          unit_dimensionless(), index->identity.object_id);
    Result<ArrayRef> applied = covariance_apply(context, reopened->signal->noise, *input.value);
    auto cov = reopened->signal->noise;
    std::printf("debug roots=%zu infl=%zu model_infl=%zu root0=%d factor0=%d\n",
                cov->model->innovations.size(), cov->influence ? 1 : 0,
                cov->model->influence.size(),
                cov->model->innovations.empty() ? -1 : (cov->model->innovations[0] ? 1 : 0),
                (cov->model->innovations.empty() || !cov->model->innovations[0]) ? -1 :
                  (cov->model->innovations[0]->factor ? 1 : 0));
    check(applied.code == Code::ok, "apply the reopened covariance");
    if (applied.code != Code::ok) {
      for (const auto& issue : applied.assessment.issues) std::printf("  issue: %s (%s)\n", issue.reason.c_str(), issue.origin.c_str());
    }
    if (applied.code == Code::ok) {
      Result<ArrayView> view = storage.read(context, *applied.value, BlockRequest{0, 4});
      std::vector<double> out = acsd_validation::decode_f64(acsd_validation::value_of(view).bytes);
      check(out.size() == 4 && out[0] == 4.0 && out[3] == 4.0,
            "C * ones = 4 * ones (root factors 2)");
    }
  }
  std::printf("RT PROBE: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
