// ============================================================================
// aio_hips.h - IVOA HiPS 写入器 C API (Phase1 Full Freeze v2, HiPS 迁移)
//
// 唯一 AIO: HiPS 全部由 astro_image_io.dll 写入。
// 标准: IVOA HiPS 2.0 (https://www.ivoa.net/documents/HiPS/), NESTED,
//       tile_width=512, NorderK/DirD/NpixN.fits, properties, MOC, Catalogue HiPS。
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

// HiPS 数据集类型
enum AioHipsDatasetType {
    AIO_HIPS_IMAGE_SIGNAL  = 0,  // signal = F/A (float32/64)
    AIO_HIPS_IMAGE_SUPPORT = 1,  // support = A/A_cell (uint8)
    AIO_HIPS_CATALOGUE_SNR = 2   // SNR catalogue (TSV)
};

// 单个 Tile 的叶级累加结果 (Drizzle 输出)
typedef struct {
    uint64_t parent_ipix;   // Tile 父级 ipix (NESTED)
    uint32_t depth;         // d = L - 9 (K = L - 9)
    const void* signal;     // signal = F/A (float* 或 double*, 按 signal_dtype)
    const uint8_t* support; // support = A/A_cell, [tile_width*tile_width]
} AioHipsTile;

// SNR catalogue 控制点
typedef struct {
    double ra_deg;
    double dec_deg;
    double snr;
    int64_t source_id;
} AioHipsSnrPoint;

// ============================================================================
// aio_hips_write - 写完整 HiPS 数据集 (signal + support + MOC + properties)
//
// 参数:
//   out_dir       - HiPS 根目录 (properties 与 NorderK/... 写入此处)
//   nside         - 叶级 NSIDE (2 的幂)
//   tile_width    - 标准 512
//   tiles         - Tile 数组 [n_tiles] (signal/support 叶级数据)
//   n_tiles       - Tile 数
//   signal_dtype  - 0=float32, 1=float64 (signal 存储精度)
//   snr_points    - SNR catalogue 控制点 (可为 NULL)
//   n_snr         - 控制点数
//   creator_did   - 数据集标识 (写入 properties creator_did)
//   obs_title     - 数据集标题
//   moc_order     - MOC 阶数 (<= 9), 0=auto (与 tile depth 相同)
//
// 返回: 0=成功, <0=失败
// ============================================================================
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
