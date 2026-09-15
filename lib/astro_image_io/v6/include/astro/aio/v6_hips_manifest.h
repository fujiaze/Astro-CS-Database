/* v6_hips_manifest.h — HiPS properties / 产品 manifest 渲染与 fail-closed 校验
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/astro_image_io/v6/。
 * 语义锚: ALG-P3-008 §7 (流式原子 FITS + 层完整性)、FZ-PROV-MINIMAL-SET
 * (output_hash)、FZ-P3-BUNIT-QUADRATIC (逐层 BUNIT 二次律)。
 * HiPS 属性文件键集遵循 IVOA HiPS 1.0 通用键；本模块只做序列化/结构校验，
 * 不实现科学投影（属 IMPL-P3-PROJ-001）。
 */
#ifndef ASTROCS_V6_AIO_HIPS_MANIFEST_H
#define ASTROCS_V6_AIO_HIPS_MANIFEST_H

#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "astro/aio/v6_validation.h"

namespace astrocs {
namespace aio {

struct HipsProperties {
  std::string creator_did;
  std::string obs_collection;
  std::string dataproduct_type = "image";
  std::string hips_version = "1.4";
  std::string release_date;
  std::string hips_status = "public master clonableOnce";
  std::string frame = "icrs";
  int order = 0;
  int order_min = -1;  // <0 => 不输出
  int tile_width = 512;
  std::string tile_format = "fits";
  bool has_initial_position = false;
  double initial_ra = 0.0;
  double initial_dec = 0.0;
  double initial_fov = 0.0;
};

std::string render_hips_properties(const HipsProperties& p);
ValidationReport validate_hips_properties(const std::string& text);

struct HipsFileRecord {
  std::string relative_path;
  std::uint64_t size_bytes = 0;
  std::string sha256_hex;
  std::string role;         // signal|variance|ivar|coverage|validity|support|...
  std::string hdu_extname;  // 可选
};

struct HipsManifest {
  std::string manifest_schema = "astrocs.v6.hips_manifest/v1";
  int schema_version = 1;
  std::string product_type_id;
  int order = 0;
  int order_min = -1;
  std::string tile_format = "fits";
  std::string frame = "icrs";
  std::uint64_t tile_count = 0;
  std::vector<HipsFileRecord> files;
  std::string provenance_sha256;
  std::string output_hash;
  std::string generated_utc;
};

nlohmann::json hips_manifest_to_json(const HipsManifest& m);
std::string render_hips_manifest_json(const HipsManifest& m);
ValidationReport validate_hips_manifest_json(const nlohmann::json& j);

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_HIPS_MANIFEST_H
