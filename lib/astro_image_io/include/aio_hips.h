// ============================================================================
// aio_hips.h - IVOA HiPS 生产链 C API (Phase1 Final Closure V3, HiPS 直写)
//
// 唯一 AIO: HiPS 全部由 astro_image_io.dll 写入/读取。
// 标准: IVOA HiPS 1.4 (https://www.ivoa.net/documents/HiPS/), NESTED,
//       tile_width=512, NorderK/DirD/NpixN.fits, properties, MOC, Catalogue HiPS。
// FITS 一律由 vendored CFITSIO 4.6.4 写入 (不再手写 header/checksum)。
//
// 生产数据流 (无 HISS 中转):
//   Drizzle TileAccumulator -> AstroSphereTileView -> aio_hips_* (本文件)
//
// 三个独立标准 HiPS 数据集 (子产品):
//   <out_dir>/signal/   Image HiPS, signal  = flux_sum / covered_area
//   <out_dir>/support/  Image HiPS, support = covered_area / A_cell
//   <out_dir>/snr/      Catalogue HiPS (TSV tiles)
// ============================================================================

#ifndef AIO_HIPS_H
#define AIO_HIPS_H

#include <stdint.h>

#ifdef _WIN32
#define AIO_HIPS_EXPORT __declspec(dllexport)
#else
#define AIO_HIPS_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// HiPS 子产品标志
enum AioHipsProductFlag {
    AIO_HIPS_PRODUCT_SIGNAL  = 1,
    AIO_HIPS_PRODUCT_SUPPORT = 2,
    AIO_HIPS_PRODUCT_SNR     = 4,
    AIO_HIPS_PRODUCT_ALL     = 7
};

// 数据类型
enum AioHipsDataType {
    AIO_HIPS_FLOAT32 = 0,
    AIO_HIPS_FLOAT64 = 1
};

// Drizzle Tile 直写视图 (控制包 05_HIPS_DIRECT_PRODUCTION §2)
//   parent_ipix : NESTED, 叶级 tile 父单元 (Norder K = log2(nside)-9)
//   leaf_order  : 叶级 Norder L (= log2(nside))
//   width       : 512 (HiPS 标准 tile)
//   data_type   : AIO_HIPS_FLOAT32 / AIO_HIPS_FLOAT64
//   flux_sum    : [width*width] 累计通量 (LeafAccumulator.sumFlux)
//   covered_area: [width*width] 球面覆盖面积 sr (LeafAccumulator.sumArea)
//   valid_mask  : [width*width] 1=有效 (covered_area>0), 可 NULL (全部有效)
typedef struct {
    uint64_t parent_ipix;
    uint32_t leaf_order;
    uint32_t width;
    int32_t  data_type;
    const void* flux_sum;
    const void* covered_area;
    const uint8_t* valid_mask;
} AstroSphereTileView;

// SNR catalogue 控制点 (V4: 携带 stable star_id 与真实状态字段)
typedef struct {
    double ra_deg;
    double dec_deg;
    double snr;
    int64_t star_id;            // PSF 阶段 stable star_id (禁止重新编号)
    uint32_t quality_flags;     // 位标志: 1=PSF_OK 2=saturated 4=has_saturated
                                // 8=photo_matched 16=photo_rejected
    uint32_t photometric_status; // 0=unmatched 1=used 2=rejected
} AioHipsSnrPoint;

typedef struct AioHipsProductSet AioHipsProductSet;

// ============================================================================
// aio_hips_product_begin - 开始一个 HiPS 产品集 (流式, 不要求 tiles 全在 RAM)
//
// 参数:
//   out_dir      - 输出根目录 (内含 signal/ support/ snr/ 三个子产品)
//   nside        - 叶级 NSIDE (2 的幂, >= 512)
//   tile_width   - 标准 512
//   data_type    - AIO_HIPS_FLOAT32 / AIO_HIPS_FLOAT64 (signal/support 存储精度)
//   flags        - 子产品位或 (AIO_HIPS_PRODUCT_*)
//   creator_did  - 数据集标识 (properties creator_did)
//   obs_title    - 数据集标题
//   obs_filter   - 滤光片 (可 NULL)
//   exposure_s   - 曝光时间秒
//   obs_date     - 观测日期 ISO (可 NULL)
//   moc_order    - MOC 阶 (<= tile order K), 0=auto(=K)
//
// 返回: 句柄 (失败返回 NULL, 用 aio_hips_last_error 获取原因)
// ============================================================================
AIO_HIPS_EXPORT AioHipsProductSet* aio_hips_product_begin(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    int32_t data_type,
    int flags,
    const char* creator_did,
    const char* obs_title,
    const char* obs_filter,
    double exposure_s,
    const char* obs_date,
    uint32_t moc_order);

// 写一个 signal/support 叶级 Tile (写 signal 与 support 两个 Image HiPS 的
// NorderK/DirD/NpixN.fits, 由 CFITSIO 写入含 checksum 的标准 FITS)
// 语义: signal = flux_sum/covered_area, support = covered_area/A_cell
//       covered_area<=0 -> signal=NaN, support=0
AIO_HIPS_EXPORT int aio_hips_write_signal_support_tile(
    AioHipsProductSet* ps,
    const AstroSphereTileView* view);

// 写 SNR Catalogue HiPS 控制点 (finalize 时按 tile 分组写出 NorderK/.../NpixN.tsv)
AIO_HIPS_EXPORT int aio_hips_write_snr_points(
    AioHipsProductSet* ps,
    const AioHipsSnrPoint* pts,
    int n);

// 结束产品集: 写 properties/MOC/低阶 hierarchy (从磁盘 leaf tiles 聚合),
// 释放句柄。返回 0=成功。
AIO_HIPS_EXPORT int aio_hips_finalize(AioHipsProductSet* ps);

// 中止: 清理已写部分 (尽力) 并释放句柄
AIO_HIPS_EXPORT int aio_hips_abort(AioHipsProductSet* ps);

// 旧 Tile 结构 (兼容声明, 仅 aio_hips_write 使用)
typedef struct {
    uint64_t parent_ipix;
    uint32_t depth;
    const void* signal;
    const uint8_t* support;
} AioHipsTile;

// 兼容旧接口 (HISS 中转验证用): 全量写信号+support+SNR
AIO_HIPS_EXPORT int aio_hips_write(
    const char* out_dir,
    uint32_t nside,
    uint32_t tile_width,
    const AioHipsTile* tiles,
    int n_tiles,
    int signal_dtype,
    const AioHipsSnrPoint* snr_points,
    int n_snr,
    const char* creator_did,
    const char* obs_title,
    int moc_order);

// 获取最后错误信息 (线程局部)
AIO_HIPS_EXPORT const char* aio_hips_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // AIO_HIPS_H
