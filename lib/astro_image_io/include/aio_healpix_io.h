#ifndef AIO_HEALPIX_IO_H
#define AIO_HEALPIX_IO_H

// ============================================================================
// aio_healpix_io.h - HEALPix I/O (合并自 healpix_io 模块)
//
// 本头文件原为 lib/healpix_db/healpix_io/include/healpix_io.h
// 2026-07-16 按 architecture-refactor spec G1 合并入 aio 模块
// 原 healpix_io/ 目录已归档到 lib/healpix_db/healpix_io/archive/
//
// 条件编译: 定义 AIO_ENABLE_HEALPIX 时启用 HEALPix I/O (默认 ON)
// 向后兼容: 末尾宏定义允许旧代码用 hiss_write 等旧名编译
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "astro_image_io.h"  // AIO_EXPORT 宏

#ifdef __cplusplus
extern "C" {
#endif

#ifdef AIO_ENABLE_HEALPIX

// ============================================================================
// .hiss 单帧存储格式 API
// ============================================================================
//
// SNR 通道格式 (JSON 头 snr_format 字段):
//   0 = 逐像素 float32[n_pix] (旧格式, 向后兼容)
//   1 = 稀疏控制点 (新格式, 见 HioSnrModel)
// 旧文件无 snr_format 字段, 默认按 0 处理 (若 has_snr=true)
// ============================================================================

// SNR 控制点 (球面坐标 + snr_psf 值, 20 字节, 用于序列化)
#pragma pack(push, 1)
typedef struct {
    double ra;       // 球面赤经 (度)
    double dec;      // 球面赤纬 (度)
    float  snr_psf;  // (A-B)/mad (无量纲)
} HioSnrControlPoint;
#pragma pack(pop)
static_assert(sizeof(HioSnrControlPoint) == 20, "HioSnrControlPoint must be 20 bytes");

// SNR 模型 (稀疏控制点 + 全局参数, 用于 I/O 序列化)
// 对应 snr_format=1 的二进制布局:
//   [n_points: uint32]
//   [points: n_points * 20B]
//   [snr_phot: f64][median_snr: f64][idw_power: f64]
typedef struct {
    uint32_t n_points;              // 控制点数
    HioSnrControlPoint* points;     // 控制点数组 (malloc 分配, 用 aio_hio_free_snr_model 释放)
    double   snr_phot;              // 1/(ln10×sigma_residual) 全局标量
    double   median_snr;            // median(snr_psf) 归一化基准
    double   idw_power;             // IDW 幂次 (默认 2.0)
} HioSnrModel;

// 写入 .hiss 文件 (snr_format=0, 逐像素 SNR)
// path - 文件路径 (UTF-8)
// nside - HEALPix nside 参数
// nested - 是否为 nested 排序 (0/1)
// n_pix - 像素数量
// ipix - ipix 索引数组 [n_pix] (uint64)
// pixel - 像素值数组 [n_pix] (float32)
// snr - SNR 数组 [n_pix] (float32), 可为 nullptr (无 snr 通道)
// meta_json - JSON 元数据字符串
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_write(const char* path, uint32_t nside, int nested,
                               uint64_t n_pix, const uint64_t* ipix,
                               const float* pixel, const float* snr,
                               const char* meta_json);

// 读取 .hiss 文件 (兼容 snr_format=0 和 1)
// path - 文件路径 (UTF-8)
// nside, nested, n_pix - 输出参数
// ipix, pixel, meta_json - 输出参数 (由 malloc 分配，调用者负责 free)
// snr - 逐像素 SNR 数组输出参数 [n_pix] (float32), 可为 nullptr (不读取 snr 通道);
//       文件 snr_format=0 且 has_snr=true 时填充, 否则 *snr = nullptr;
//       文件 snr_format=1 时 *snr = nullptr (稀疏格式不读为逐像素)
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_read(const char* path, uint32_t* nside, int* nested,
                              uint64_t* n_pix, uint64_t** ipix,
                              float** pixel, float** snr, char** meta_json);

// 写入 .hiss 文件 (snr_format=1, 稀疏控制点 SNR)
// snr_model - SNR 模型指针, 可为 nullptr (无 snr 通道, 等同 aio_hiss_write 的 snr=nullptr)
// 其他参数同 aio_hiss_write
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_write_snr_model(const char* path, uint32_t nside, int nested,
                                         uint64_t n_pix, const uint64_t* ipix,
                                         const float* pixel,
                                         const HioSnrModel* snr_model,
                                         const char* meta_json);

// 读取 .hiss 文件的稀疏 SNR 模型 (snr_format=1)
// snr_model - 输出参数 (malloc 分配, 用 aio_hio_free_snr_model 释放);
//             文件 snr_format=0 或无 snr 通道时 *snr_model = nullptr
// 其他参数同 aio_hiss_read (snr 参数省略, 因为稀疏格式不输出逐像素)
// 返回: 0=成功, <0=失败
// 注意: 若文件 snr_format=0 且 has_snr=true, 本函数不读取逐像素 snr (跳过)
AIO_EXPORT int aio_hiss_read_snr_model(const char* path, uint32_t* nside, int* nested,
                                        uint64_t* n_pix, uint64_t** ipix,
                                        float** pixel, HioSnrModel** snr_model,
                                        char** meta_json);

// 释放 HioSnrModel (aio_hiss_read_snr_model 分配的内存)
AIO_EXPORT void aio_hio_free_snr_model(HioSnrModel* model);

// ============================================================================
// .hcsd 天球数据库格式 API
// ============================================================================

// 写入 .hcsd 文件 (含子叶块索引构建)
AIO_EXPORT int aio_hcsd_write(const char* path, uint32_t nside, int nested,
                               uint64_t n_pix, const uint64_t* ipix,
                               const float* pixel, const char* meta_json);

// 读取 .hcsd 文件 (全量读取)
AIO_EXPORT int aio_hcsd_read(const char* path, uint32_t* nside, int* nested,
                              uint64_t* n_pix, uint64_t** ipix,
                              float** pixel, char** meta_json);

// 读取 .hcsd 文件中指定子叶的数据 (按需加载)
// leaf_ipix_at_nside64 - nside=64 层的子叶 ipix
// 返回该子叶内的所有像素数据
AIO_EXPORT int aio_hcsd_read_leaf(const char* path, uint64_t leaf_ipix_at_nside64,
                                   uint64_t* n_pix, uint64_t** ipix, float** pixel);

// 释放内存
AIO_EXPORT void aio_hio_free(void* ptr);

#endif // AIO_ENABLE_HEALPIX

#ifdef __cplusplus
}
#endif

// ============================================================================
// 向后兼容宏 (允许旧代码用 hiss_write/hiss_read 等旧名编译)
// 旧代码 #include "healpix_io.h" 改为 #include "aio_healpix_io.h" 后无需改其他代码
// ============================================================================
#ifdef AIO_ENABLE_HEALPIX
#define hiss_write          aio_hiss_write
#define hiss_read           aio_hiss_read
#define hiss_write_snr_model aio_hiss_write_snr_model
#define hiss_read_snr_model aio_hiss_read_snr_model
#define hcsd_write          aio_hcsd_write
#define hcsd_read           aio_hcsd_read
#define hcsd_read_leaf      aio_hcsd_read_leaf
#define hio_free            aio_hio_free
#define hio_free_snr_model  aio_hio_free_snr_model
#endif

#endif // AIO_HEALPIX_IO_H
