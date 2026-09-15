/* v6_product_io.h — 科学产品 I/O 顶层组装 (FITS 层 + provenance + manifest + 原子发布)
 *
 * 任务: IMPL-AIO-001 (Wave 5)。写域: lib/astro_image_io/v6/。
 * 组装语义锚: ALG-P3-008 §7 (1..5 步原子发布) + FZ-PROV-MINIMAL-SET +
 * FZ-BUNIT-SEMANTICS + FZ-P3-BUNIT-QUADRATIC + ALG-P3-008 §7.4 provenance 最小集。
 * 本模块不接线任何 Phase session（任务正文要求"不接线"）。
 */
#ifndef ASTROCS_V6_AIO_PRODUCT_IO_H
#define ASTROCS_V6_AIO_PRODUCT_IO_H

#include <cstdint>
#include <string>
#include <vector>

#include "astro/aio/v6_atomic_publish.h"
#include "astro/aio/v6_fits.h"
#include "astro/aio/v6_hips_manifest.h"
#include "astro/aio/v6_provenance.h"
#include "astro/aio/v6_validation.h"

namespace astrocs {
namespace aio {

// 一层 HDU：规格 + 原始数据（big-endian 由写出器负责？否：调用方提供已按
// FITS 字节序排列的数据；本模块只做透传/补齐/校验和）。
struct FitsLayer {
  FitsHduSpec spec;
  std::vector<std::uint8_t> data;
};

std::vector<ExpectedHdu> expected_from_layers(const std::vector<FitsLayer>& layers);

// 原子发布一个多 HDU FITS 产品（ALG-P3-008 1..5）。
PublishResult publish_fits_product(const std::string& target,
                                   const std::vector<FitsLayer>& layers,
                                   const std::vector<ExpectedHdu>& expected,
                                   const PublishOptions& opts,
                                   const CancelFn& cancel);

// 产品记录级 fail-closed 汇总门。
ValidationReport validate_product_record(const Provenance& prov,
                                         const HipsManifest& manifest,
                                         const HipsProperties& props);

}  // namespace aio
}  // namespace astrocs

#endif  // ASTROCS_V6_AIO_PRODUCT_IO_H
