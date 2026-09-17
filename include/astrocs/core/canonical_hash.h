// include/astrocs/core/canonical_hash.h — 规范产品哈希（canonical product hash）
//
// 口径（唯一权威定义 = tools/canonical_product_hash.py, 本文件是其 C++ 同构实现;
// 两者必须逐字节同值, 由 tools/canonical_product_hash.py compare 与 CLI 侧
// manifest 字段的交叉核对守住）:
//
//   完整性面 integrity_sha256  = 整文件字节 sha256  -> 防改动/损坏
//   可复现面 canonical_sha256  = 数据单元原始字节 + 规范化科学元数据 -> 可复现性验收
//
// 排除清单（版本化, 见 spec 输出）:
//   FITS 头: CHECKSUM / DATASUM / DATE / RUNID
//   JSON 键: run_id / elapsed_sec / created_utc / started_utc / finished_utc /
//            ended_utc / generated_utc / timestamp_utc / hips_creation_date /
//            hips_update_date / hips_release_date / sha256 / product_sha256
//   HiPS properties: hips_creation_date / hips_update_date / hips_release_date
// 明确**不排除**: DATE-OBS(观测时刻)/BUNIT/CRPIX/CRVAL/CD/CTYPE/SIP/像素数据 等。
//
// 依据与外部标准引用见 spec 文档与工具头注释; 部署内既有裁决
// tools/quality/compare_products.py（P26 T3, 负责人裁决 4）的墙钟清单与本清单一致。
#pragma once

#include <string>
#include <vector>

namespace astrocs::core {

// spec 标识（进入哈希域分隔前缀, 改口径必须升主版本号）
extern const char* const kCanonicalProductHashSpec;

struct CanonicalHashResult {
  bool ok = false;
  std::string error;                 // ok=false 时的原因（读失败/结构非法）
  std::string format;                // fits | json | hips-properties | raw
  std::string canonical_sha256;      // 规范产品哈希（hex, 64 字符）
  std::string integrity_sha256;      // 整文件 sha256（hex, 64 字符）
  std::vector<std::string> excluded_keys_hit;  // 实际命中的排除项（审计面）
  std::vector<std::string> warnings;
};

// 计算单个产品文件的规范哈希。不抛异常; 失败经 result.ok/error 返回。
CanonicalHashResult canonical_product_hash_file(const std::string& u8path);

}  // namespace astrocs::core
