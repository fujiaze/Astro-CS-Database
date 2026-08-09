// ============================================================================
// aio_hips_reader.h - IVOA HiPS 读取器 C API (Phase1 Final Closure V3)
//
// Browser / HIPS_VERIFY 唯一后端: 不允许直接解析 properties/CFITSIO。
// ============================================================================

#ifndef AIO_HIPS_READER_H
#define AIO_HIPS_READER_H

#include <stdint.h>

#ifdef _WIN32
#define AIO_HIPS_RD_EXPORT __declspec(dllexport)
#else
#define AIO_HIPS_RD_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum AioHipsProduct {
    AIO_HIPS_RD_SIGNAL  = 0,
    AIO_HIPS_RD_SUPPORT = 1,
    AIO_HIPS_RD_SNR     = 2
};

typedef struct AioHipsDataset AioHipsDataset;

// 打开一个 HiPS 子产品 (signal/support/snr), 读取 properties。
// 返回句柄或 NULL (aio_hips_reader_last_error)。
AIO_HIPS_RD_EXPORT AioHipsDataset* aio_hips_open(
    const char* out_dir,      // 产品集根目录
    int product);             // AIO_HIPS_RD_*

// 读取 properties 关键字段 (hips_version/hips_order/...), 写入 buf。
AIO_HIPS_RD_EXPORT int aio_hips_get_properties(
    AioHipsDataset* d, char* buf, int buf_size);

// 获取叶级 tile 数
AIO_HIPS_RD_EXPORT int aio_hips_tile_count(AioHipsDataset* d);

// 获取第 i 个叶级 tile 的 NESTED ipix
AIO_HIPS_RD_EXPORT int aio_hips_tile_ipix(AioHipsDataset* d, int i, uint64_t* out_ipix);

// 读取一个叶级 tile 为 float32 (signal/support)。out 至少 512*512*sizeof(float)。
// 返回 0=成功。
// V5 (HIPS-IMG-001 §5): out 为 standard HiPS row-major —— out[y*512+x] 中
// (x,y) 是 tile 二维图像坐标 (FITS NAXIS1=x, NAXIS2=y 原样), 不是 NESTED local。
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f32(AioHipsDataset* d, uint64_t ipix, float* out);

// 读取一个叶级 tile 为 float64 (standard HiPS row-major, 同上)
AIO_HIPS_RD_EXPORT int aio_hips_read_tile_f64(AioHipsDataset* d, uint64_t ipix, double* out);

// V5 (HIPS-IMG-001 §5): 按 NESTED leaf ipix 查询单像素值。
// 内部执行 leaf_ipix -> local nested -> 标准 tile xy -> FITS row-major。
AIO_HIPS_RD_EXPORT int aio_hips_read_leaf_f32(AioHipsDataset* d, uint64_t leaf_ipix, float* out);
AIO_HIPS_RD_EXPORT int aio_hips_read_leaf_f64(AioHipsDataset* d, uint64_t leaf_ipix, double* out);

// SNR catalogue: 返回点数与数组 (ra[], dec[], snr[], star_id[],
// quality_flags[], photometric_status[])。调用方分配; 返回实际点数 (0=无)。
// quality_flags/photometric_status 可为 NULL (旧调用方不读取)。
AIO_HIPS_RD_EXPORT int aio_hips_read_snr_catalog(
    AioHipsDataset* d, double* ra, double* dec, double* snr, int64_t* star_id,
    uint32_t* quality_flags, uint32_t* photometric_status, int max);

AIO_HIPS_RD_EXPORT void aio_hips_close(AioHipsDataset* d);

AIO_HIPS_RD_EXPORT const char* aio_hips_reader_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // AIO_HIPS_READER_H

