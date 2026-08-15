// ============================================================================
// astro_sphere_sink.h - Drizzle TileAccumulator -> AIO HiPS 直写 Sink
// (Phase1 Final Closure V3, 05_HIPS_DIRECT_PRODUCTION)
//
// 生产数据流 (无 HISS 中转):
//   DrizzleEngine::drizzleTiled[F32/F64]
//   -> AstroSphereTileSink (本文件)
//   -> astro_image_io.dll aio_hips_product_begin/write_*/finalize
// ============================================================================

#ifndef ASTRO_SPHERE_SINK_H
#define ASTRO_SPHERE_SINK_H

#include "drizzle_engine.h"
#include "aio_hips.h"

#include <string>
#include <vector>

namespace drizzle {

// 将 Tile 级累加结果直接流式写入 HiPS 产品集
// (signal/support/snr; has_variance=1 时追加 variance/ivar, P1-003)。
// tiles: 必须按 depth=9 分组 (512x512 叶 tile, 与 HiPS NorderK tile 1:1)。
// has_variance: 1=累加器已含 sumVarNum, 写 variance/ivar 产品。
// 返回: true=成功 (句柄已 finalize); false=失败, err 含原因。
template <typename Scalar>
bool write_hips_direct(const std::vector<TileAccumulatorT<Scalar>>& tiles,
                       const DrizzleConfig& config,
                       const DrizzleMeta& meta,
                       const std::string& hips_dir,
                       const std::vector<AioHipsSnrPoint>& snr_pts,
                       int has_variance,
                       std::string& err);

} // namespace drizzle

#endif // ASTRO_SPHERE_SINK_H
