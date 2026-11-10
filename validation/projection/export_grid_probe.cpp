#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <vector>
#include "acsd/contracts.hpp"
#include "coordinates/healpix.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
#include "operators/linear_action.hpp"
#include "storage/local_storage.hpp"
#include "validation/stats/harness.hpp"

using namespace acsd;
using namespace acsd::model;

static int failures = 0;
static void check(bool ok, const char* what) {
  std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++failures;
}

int main(int argc, char** argv) {
  std::filesystem::remove_all("/tmp/expr");
  acsd_validation::CountingResources resources;
  acsd_validation::NoCancellation cancellation;
  storage::LocalStorage storage;
  Context context{resources, cancellation, storage};

  const double b0 = 1.0e6;
  const U32 order = 6;
  const U64 ipix = 0;
  const HealpixLeaf leaf{order, ipix};
  const SkyICRS sky = coordinates::healpix_center(order, ipix);

  auto index = std::make_shared<Index>();
  index->size = 1; index->kind = Index::Kind::leaf; index->order = order;
  index->identity = make_identity("acsd.Index", 2, 0, sha256_hex(std::string("exp-leaf")), "exp");
  auto ids_values = std::vector<U64>{order, ipix};
  auto ids_bytes = encode_u64(ids_values);
  Result<ArrayRef> ids = storage.copy(context, DType::u64, ids_bytes, std::vector<U64>{2},
                                      unit_dimensionless(), index->identity.object_id);
  index->ids = *ids.value;

  auto signal_values = std::vector<double>{b0};
  Result<ArrayRef> signal = storage.copy(context, DType::f64, encode_f64(signal_values),
                                         std::vector<U64>{1}, unit_dimensionless(),
                                         index->identity.object_id);
  auto states_bytes = std::vector<std::byte>{std::byte{1}};
  Result<ArrayRef> states = storage.copy(context, DType::u8, states_bytes, std::vector<U64>{1},
                                         unit_dimensionless(), index->identity.object_id);

  auto support = std::make_shared<SupportMap>();
  support->kind = SupportMap::Kind::full_leaf_approx;
  support->index = std::shared_ptr<const Index>(index);
  support->states = *states.value;
  support->identity = make_identity("acsd.SupportMap", 2, 0, sha256_hex(std::string("exp-support")), "exp");

  Result<ArrayRef> bound_low = storage.copy(context, DType::f64, encode_f64(std::vector<double>{b0 - 1.0}),
                                            std::vector<U64>{1}, unit_dimensionless(),
                                            index->identity.object_id);
  Result<ArrayRef> bound_high = storage.copy(context, DType::f64, encode_f64(std::vector<double>{b0 + 1.0}),
                                             std::vector<U64>{1}, unit_dimensionless(),
                                             index->identity.object_id);
  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = *signal.value;
  estimate->numerical_lower = *bound_low.value;
  estimate->numerical_upper = *bound_high.value;
  estimate->unit = unit_dimensionless();
  estimate->support = support;
  estimate->identity = make_identity("acsd.SignalEstimate", 2, 0, sha256_hex(std::string("exp-est")), "exp");
  estimate->assessment = make_assessment(Code::ok);
  estimate->assessment.complete = true;

  auto graph = std::make_shared<ProductGraph>();
  graph->stage = ProductGraph::Stage::mosaic;
  graph->signal = estimate;
  graph->profile = "signal_only";
  graph->identity = make_identity("acsd.ProductGraph", 2, 0, sha256_hex(std::string("exp-graph")), "exp");
  graph->assessment = make_assessment(Code::ok);
  graph->assessment.complete = true;

  PublishRequest publish;
  publish.destination = "/tmp/expr";
  publish.signal_dtype = DType::f64;
  publish.profile = "signal_only";
  publish.tile_log2_width = 6;
  publish.durable = false;
  Result<ProductLocation> published = publish_product(context, graph, publish);
  check(published.code == Code::ok, "publish constant-surface-brightness product");
  if (published.code != Code::ok) return 1;

  std::ifstream manifest("/tmp/expr/manifest.json", std::ios::binary);
  std::string text((std::istreambuf_iterator<char>(manifest)), std::istreambuf_iterator<char>());
  ProductLocation location;
  location.directory = "/tmp/expr";
  location.manifest_hash = "sha256:" + sha256_hex(std::span<const std::byte>(
      reinterpret_cast<const std::byte*>(text.data()), text.size()));
  Result<Ref<ProductProvider>> opened = open_product(context, location);
  check(opened.code == Code::ok, "reopen product");
  if (opened.code != Code::ok) return 1;

  const U64 width = argc > 1 ? std::stoull(argv[1]) : 1;
  const U64 height = argc > 2 ? std::stoull(argv[2]) : 1;
  const double crpix1 = argc > 3 ? std::stod(argv[3]) : 0.5;
  const double crpix2 = argc > 4 ? std::stod(argv[4]) : 0.5;
  const double scale = 0.0002777777777777778 * M_PI / 180.0;
  const double angle = argc > 5 ? std::stod(argv[5]) : 0.0;
  const double ca = std::cos(angle), sa = std::sin(angle);
  auto wcs = std::make_shared<WcsModel>();
  wcs->crpix_fits1 = Vec2{crpix1, crpix2};
  wcs->cd_rad_per_pixel = Mat2{-scale * ca, scale * sa, scale * sa, scale * ca};
  const double sd = sky.nz;
  const double cd = std::sqrt(std::max(0.0, 1.0 - sd * sd));
  const double cr = cd > 0.0 ? sky.nx / cd : 1.0;
  const double sr = cd > 0.0 ? sky.ny / cd : 0.0;
  wcs->c = Vec3{sky.nx, sky.ny, sky.nz};
  wcs->e = Vec3{-sr, cr, 0.0};
  wcs->north = Vec3{-sd * cr, -sd * sr, cd};
  wcs->domain = Box{-0.5, -0.5, static_cast<double>(width) - 0.5, static_cast<double>(height) - 0.5};
  wcs->identity = make_identity("acsd.WcsModel", 2, 0, sha256_hex(std::string("exp-wcs")), "exp");

  ExportRequest request;
  request.target.model = std::shared_ptr<const WcsModel>(wcs);
  request.target.width = width;
  request.target.height = height;
  request.quantity = ExportRequest::Quantity::supported_flux;
  request.support_policy = SupportMap::Kind::full_leaf_approx;
  request.assume_uniform_uncovered = false;
  request.output_dtype = DType::f64;
  request.budget.area_absolute_sr = 1e-12;
  Result<ExportResult> flux = build_export(context, **opened.value, request);
  if (flux.code != Code::ok) { std::printf("build failed %d\n", (int)flux.code); return 1; }
  Result<std::vector<double>> values = operators::read_f64(
      context, (*flux.value).estimate->signal, 0, width * height);
  if (!ok(values)) return 1;
  double sum = 0.0; bool any_nan = false;
  for (double v : val(values)) { if (std::isnan(v)) any_nan = true; else sum += v; }
  std::printf("sky=%.17g,%.17g,%.17g\n", sky.nx, sky.ny, sky.nz);
  std::printf("sum=%.17g nan=%d width=%llu height=%llu\n", sum, any_nan ? 1 : 0,
              (unsigned long long)width, (unsigned long long)height);
  return 0;
  return failures == 0 ? 0 : 1;
}
