#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "storage/local_storage.hpp"
#include "validation/stats/harness.hpp"
using namespace acsd;
using namespace acsd::model;
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
  if (!ok(opened)) { std::printf("open failed %d\n", (int)opened.code); return 1; }
  Ref<ProductGraph> graph = (*opened.value)->graph();
  auto estimate = graph->signal;
  std::printf("profile=%s noise=%d\n", graph->profile.c_str(), estimate && estimate->noise ? 1 : 0);
  if (!estimate || !estimate->noise) return 1;
  const U64 n = estimate->noise->input_output.domain ? estimate->noise->input_output.domain->size : 0;
  std::printf("cov domain=%llu state=%d\n", (unsigned long long)n,
              (int)estimate->noise->assessment.noise);
  std::vector<double> ones(static_cast<std::size_t>(n), 1.0);
  Result<ArrayRef> input = context.storage.copy(context, DType::f64, encode_f64(ones),
                                               std::vector<U64>{n}, unit_dimensionless(), Id());
  Result<ArrayRef> applied = covariance_apply(context, estimate->noise, *input.value);
  std::printf("apply code=%d\n", (int)applied.code);
  if (!ok(applied)) return 1;
  Result<std::vector<double>> values = operators::read_f64(context, *applied.value, 0, n);
  if (!ok(values)) return 1;
  double maxv = 0.0; bool finite = true;
  for (double v : val(values)) { if (!std::isfinite(v)) finite = false; maxv = std::max(maxv, std::fabs(v)); }
  std::printf("C*1 finite=%d maxabs=%.6g\n", finite ? 1 : 0, maxv);
  return finite ? 0 : 1;
}
