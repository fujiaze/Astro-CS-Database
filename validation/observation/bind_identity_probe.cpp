#include <cstdio>
#include <vector>
#include "acsd/contracts.hpp"
#include "model/array.hpp"
#include "model/result.hpp"
#include "model/units.hpp"
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
  index->identity = make_identity("acsd.Index", 2, 0, sha256_hex(std::string("bind-index")), "bind");

  auto make_obs = [&](double v) {
    Result<ArrayRef> values = storage.copy(context, DType::f64, encode_f64(std::vector<double>{v, v + 1.0}),
                                           std::vector<U64>{2}, unit_dimensionless(),
                                           index->identity.object_id);
    auto footprint = std::make_shared<FootprintSet>();
    footprint->width = 2; footprint->height = 1;
    footprint->detector_from_sample = Mat2{1, 0, 0, 1};
    footprint->offset = Vec2{0, 0};
    footprint->local_cell = Box{0, 0, 1, 1};
    footprint->detector_frame = "det";
    footprint->identity = make_identity("acsd.FootprintSet", 2, 0,
                                        sha256_hex(std::string("bind-foot")), "bind");
    auto obs = std::make_shared<NativeObservation>();
    obs->integrated_values = *values.value;
    obs->native_index = std::shared_ptr<const Index>(index);
    obs->footprints = footprint;
    obs->frame = "frame";
    obs->unit = unit_dimensionless();
    obs->identity = make_identity("acsd.NativeObservation", 2, 0,
                                  sha256_hex(std::string("bind-parent")), "bind");
    return obs;
  };

  auto wcs = std::make_shared<WcsModel>();
  wcs->crpix_fits1 = Vec2{1.0, 1.0};
  wcs->cd_rad_per_pixel = Mat2{1e-6, 0, 0, 1e-6};
  wcs->c = Vec3{1, 0, 0}; wcs->e = Vec3{0, 1, 0}; wcs->north = Vec3{0, 0, 1};
  wcs->domain = Box{-0.5, -0.5, 1.5, 0.5};
  wcs->detector_frame = "det";
  wcs->identity = make_identity("acsd.WcsModel", 2, 0, sha256_hex(std::string("bind-wcs")), "bind");

  auto a = make_obs(10.0);
  auto b = make_obs(20.0);
  Result<Ref<NativeObservation>> ba = bind_wcs(context, a, wcs, nullptr);
  Result<Ref<NativeObservation>> bb = bind_wcs(context, b, wcs, nullptr);
  std::printf("a code=%d b code=%d\n", (int)ba.code, (int)bb.code);
  if (!ok(ba) || !ok(bb)) return 1;
  const Id ha = (*ba.value)->identity.content_hash;
  const Id hb = (*bb.value)->identity.content_hash;
  std::printf("a=%s\nb=%s\n", ha.c_str(), hb.c_str());
  std::printf("different=%d\n", ha != hb);
  return ha != hb ? 0 : 1;
}
