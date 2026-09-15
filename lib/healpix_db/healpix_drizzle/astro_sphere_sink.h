// ============================================================================
// astro_sphere_sink.h - Drizzle TileAccumulator -> AIO HiPS 直写 Sink
// (Phase1 Final Closure , 05_HIPS_DIRECT_PRODUCTION)
//
// 生产数据流 (无 legacy 单文件容器中转):
// DrizzleEngine::drizzleTiled[F32/F64]
// -> AstroSphereTileSink (本文件)
// -> astro_image_io.dll aio_hips_product_begin/write_*/finalize
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

// Phase1 生产末端: 与旧 writer 节点 (读取 legacy 单文件容器后写 HiPS)
// 逐字节等价的直写 sink。产物 = 标准 HiPS 树 (signal/ + support/ + Moc.fits +
// metadata.fits + properties), 不落任何中间容器。
//
// 逐字节等价要点 (与 lib/core/src/module_adapters.cpp 旧 p1_op_writer 同口径):
//   * 只写 signal + support 两个子产品 (flags = SIGNAL|SUPPORT), 不写
//     variance/ivar/snr, 不写 drizzle provenance;
//   * covered_area 先按容器 support 的 uint8 面积比量化
//     (u8 = lround(255*clamp(sumArea/A_cell,0,1)), area = (u8/255)*A_cell),
//     再交给 aio_hips 计算 signal = flux/area, support = area/A_cell;
//   * signal 窄化为 float32 (HiPS 产品位深固定 AIO_HIPS_FLOAT32);
//   * valid_mask = 本 tile 实际触及叶;
//   * tile 按 parent_ipix 升序写出 (hierarchy/覆盖面积的累加顺序 = 旧 writer
//     的 parent 升序), 保证 finalize 产出的 NorderK hierarchy 与 properties
//     数值逐位一致;
//   * product_begin 的 creator/title/filter/exposure/obs_date/moc_order 与旧
//     writer 完全一致。
// tiles: 必须按 depth=9 分组 (512x512 叶 tile, 与 HiPS Norder9 tile 1:1)。
// filter_passband: 观测 passband 身份 (可为空串, 与旧 writer 同口径透传)。
template <typename Scalar>
bool write_hips_phase1(const std::vector<TileAccumulatorT<Scalar>>& tiles,
                       const DrizzleConfig& config,
                       const std::string& hips_dir,
                       const std::string& filter_passband,
                       std::string& err);

} // namespace drizzle

#endif // ASTRO_SPHERE_SINK_H
