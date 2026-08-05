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

// WP-H 步骤14: 只读 HISS Header (不加载 Tile 数据)
// 用于 CLI 诊断输出和 Browser 首次打开 (只读 NSIDE/Tile数/元数据)
// path - 文件路径 (UTF-8)
// nside, tile_nside, depth, n_leaf_per_tile, n_tiles, n_pix_total - 输出参数
// meta_json - 输出参数 (malloc 分配, 调用者负责用 aio_hio_free 释放)
// tile_ipix_list - 输出参数 (malloc 分配 uint64 数组, 含 *n_tiles 个 parent_ipix;
//                  可传 nullptr 跳过; 非 nullptr 时调用者负责用 aio_hio_free 释放)
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_inspect(const char* path,
                                  uint32_t* nside,
                                  uint32_t* tile_nside,
                                  uint32_t* depth,
                                  uint32_t* n_leaf_per_tile,
                                  uint64_t* n_tiles,
                                  uint64_t* n_pix_total,
                                  char** meta_json,
                                  uint64_t** tile_ipix_list);

// WP-H 步骤14: 按 Tile 父 ipix 读取 signal (float32 数组, 已展开到 n_leaf_per_tile)
// path - 文件路径 (UTF-8)
// parent_ipix - Tile 父像素 NESTED ipix
// signal - 输出参数 (malloc 分配 float32 数组, 调用者负责用 aio_hio_free 释放)
// n_signal - 输出参数, signal 数组长度
// 返回: 0=成功, <0=失败
// 注: 仅适用于 FP32 模式文件 (signal_dtype=0); FP64 文件会返回错误
AIO_EXPORT int aio_hiss_read_tile_signal(const char* path, uint64_t parent_ipix,
                                           float** signal, uint32_t* n_signal);

// R10: 按 Tile 父 ipix 读取 signal (float64 数组, FP64 模式专用)
// 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
// signal - 输出参数 (malloc 分配 double 数组, 调用者负责用 aio_hio_free 释放)
AIO_EXPORT int aio_hiss_read_tile_signal_f64(const char* path, uint64_t parent_ipix,
                                               double** signal, uint32_t* n_signal);

// WP-H 步骤14: 按 Tile 父 ipix 读取 support (uint8 数组, 已展开到 n_leaf_per_tile)
AIO_EXPORT int aio_hiss_read_tile_support(const char* path, uint64_t parent_ipix,
                                            uint8_t** support, uint32_t* n_support);

// WP-H 步骤14: 按 Tile 父 ipix 读取 SNR 控制点 (稀疏, local_ipix + snr)
// snr_out - 输出参数 (malloc 分配, 每点 8 字节: local_ipix(uint32) + snr(float32))
// n_points - 输出参数, 控制点数
AIO_EXPORT int aio_hiss_read_tile_snr(const char* path, uint64_t parent_ipix,
                                        uint8_t** snr_out, uint32_t* n_points);

// R11: 按 Tile 读取 SNR 控制点 (FP64, snr_dtype=1 文件专用)
// snr_out - 输出参数 (malloc 分配, 每点 12 字节: local_ipix uint32 LE + snr float64 LE)
// 仅适用于 snr_dtype=1 文件; f32 文件返回错误 (禁止静默转换)
AIO_EXPORT int aio_hiss_read_tile_snr_f64(const char* path, uint64_t parent_ipix,
                                            uint8_t** snr_out, uint32_t* n_points);

// WP-H 步骤14: 通过 ra/dec 查询像素值 (与 HissReader::query_pixel 一致)
// ra, dec - 度
// signal, support - 输出参数 (单个值)
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_query_pixel(const char* path, double ra, double dec,
                                      float* signal, uint8_t* support);

// R10: 通过 ra/dec 查询像素值 (FP64 版本, 与 HissReader::query_pixel_f64 一致)
// 仅适用于 FP64 模式文件 (signal_dtype=1); FP32 文件会返回错误 (禁止静默转换)
// ra, dec - 度
// signal - 输出参数 (单个 double 值, 调用者负责分配 sizeof(double))
// support - 输出参数 (单个 uint8_t 值)
// 返回: 0=成功, <0=失败
AIO_EXPORT int aio_hiss_query_pixel_f64(const char* path, double ra, double dec,
                                          double* signal, uint8_t* support);

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
