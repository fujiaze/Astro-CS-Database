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

int main() {
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

  auto estimate = std::make_shared<SignalEstimate>();
  estimate->signal = *signal.value;
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

  const double scale_deg = 0.0002777777777777778;
  const double scale = scale_deg * M_PI / 180.0;
  auto wcs = std::make_shared<WcsModel>();
  wcs->crpix_fits1 = Vec2{0.5, 0.5};
  wcs->cd_rad_per_pixel = Mat2{-scale, 0.0, 0.0, scale};
  const double sd = sky.nz;
  const double cd = std::sqrt(std::max(0.0, 1.0 - sd * sd));
  const double cr = cd > 0.0 ? sky.nx / cd : 1.0;
  const double sr = cd > 0.0 ? sky.ny / cd : 0.0;
  wcs->c = Vec3{sky.nx, sky.ny, sky.nz};
  wcs->e = Vec3{-sr, cr, 0.0};
  wcs->north = Vec3{-sd * cr, -sd * sr, cd};
  wcs->domain = Box{0.0, 0.0, 1.0, 1.0};
  wcs->identity = make_identity("acsd.WcsModel", 2, 0, sha256_hex(std::string("exp-wcs")), "exp");

  ExportRequest request;
  request.target.model = std::shared_ptr<const WcsModel>(wcs);
  request.target.width = 1;
  request.target.height = 1;
  request.quantity = ExportRequest::Quantity::supported_flux;
  request.support_policy = SupportMap::Kind::full_leaf_approx;
  request.assume_uniform_uncovered = false;
  request.output_dtype = DType::f64;
  request.budget.area_absolute_sr = 1e-12;
  Result<ExportResult> flux = build_export(context, **opened.value, request);
  check(flux.code == Code::ok, "build supported_flux export");
  if (flux.code == Code::ok) {
    auto values = operators::read_f64(context, (*flux.value).estimate->signal, 0, 1);
    const double expected = b0 * scale * scale;
    const double actual = val(values)[0];
    std::printf("  flux actual=%.12g expected=%.12g rel=%.3g\n", actual, expected,
                std::fabs(actual - expected) / expected);
    check(std::fabs(actual - expected) <= 1e-6 * expected,
          "supported_flux equals b0 * pixel solid angle");
  }

  request.quantity = ExportRequest::Quantity::supported_mean;
  Result<ExportResult> mean = build_export(context, **opened.value, request);
  check(mean.code == Code::ok, "build supported_mean export");
  if (mean.code == Code::ok) {
    auto values = operators::read_f64(context, (*mean.value).estimate->signal, 0, 1);
    const double actual = val(values)[0];
    std::printf("  mean actual=%.12g expected=%.12g\n", actual, b0);
    check(std::fabs(actual - b0) <= 1e-9 * b0, "supported_mean equals b0");
  }
  // A target pixel far from the supported leaf must be NaN, not zero.
  auto away = std::make_shared<WcsModel>(*wcs);
  const double d = 0.2;  // rad
  away->c = Vec3{std::cos(d) * sky.nx - std::sin(d) * sr,
                 std::cos(d) * sky.ny + std::sin(d) * cr,
                 sky.nz};
  away->e = Vec3{-away->c[1], away->c[0], 0.0};
  const double len = std::sqrt(away->e[0] * away->e[0] + away->e[1] * away->e[1]);
  away->e = Vec3{away->e[0] / len, away->e[1] / len, 0.0};
  away->north = Vec3{away->c[1] * away->e[2] - away->c[2] * away->e[1],
                     away->c[2] * away->e[0] - away->c[0] * away->e[2],
                     away->c[0] * away->e[1] - away->c[1] * away->e[0]};
  away->identity = make_identity("acsd.WcsModel", 2, 0, sha256_hex(std::string("exp-wcs-away")), "exp");
  request.target.model = std::shared_ptr<const WcsModel>(away);
  request.quantity = ExportRequest::Quantity::supported_flux;
  Result<ExportResult> unsupported = build_export(context, **opened.value, request);
  check(unsupported.code == Code::ok, "build export for an unsupported pixel");
  if (unsupported.code == Code::ok) {
    auto values = operators::read_f64(context, (*unsupported.value).estimate->signal, 0, 1);
    check(std::isnan(val(values)[0]), "unsupported pixel is NaN, not zero");
  }

  std::printf("EXPORT PROBE: %d failure(s)\n", failures);
  return failures == 0 ? 0 : 1;
}
