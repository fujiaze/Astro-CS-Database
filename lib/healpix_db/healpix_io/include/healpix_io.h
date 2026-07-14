#ifndef HEALPIX_IO_H
#define HEALPIX_IO_H

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#define HIO_API __declspec(dllexport)
#else
#define HIO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// .hiss 单帧存储格式 API
// ============================================================================

// 写入 .hiss 文件
// path - 文件路径 (UTF-8)
// nside - HEALPix nside 参数
// nested - 是否为 nested 排序 (0/1)
// n_pix - 像素数量
// ipix - ipix 索引数组 [n_pix] (uint64)
// pixel - 像素值数组 [n_pix] (float32)
// meta_json - JSON 元数据字符串
// 返回: 0=成功, <0=失败
HIO_API int hiss_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const char* meta_json);

// 读取 .hiss 文件
// path - 文件路径 (UTF-8)
// nside, nested, n_pix - 输出参数
// ipix, pixel, meta_json - 输出参数 (由 malloc 分配，调用者负责 free)
// 返回: 0=成功, <0=失败
HIO_API int hiss_read(const char* path, uint32_t* nside, int* nested,
                      uint64_t* n_pix, uint64_t** ipix,
                      float** pixel, char** meta_json);

// ============================================================================
// .hcsd 天球数据库格式 API
// ============================================================================

// 写入 .hcsd 文件 (含子叶块索引构建)
HIO_API int hcsd_write(const char* path, uint32_t nside, int nested,
                       uint64_t n_pix, const uint64_t* ipix,
                       const float* pixel, const char* meta_json);

// 读取 .hcsd 文件 (全量读取)
HIO_API int hcsd_read(const char* path, uint32_t* nside, int* nested,
                      uint64_t* n_pix, uint64_t** ipix,
                      float** pixel, char** meta_json);

// 读取 .hcsd 文件中指定子叶的数据 (按需加载)
// leaf_ipix_at_nside64 - nside=64 层的子叶 ipix
// 返回该子叶内的所有像素数据
HIO_API int hcsd_read_leaf(const char* path, uint64_t leaf_ipix_at_nside64,
                           uint64_t* n_pix, uint64_t** ipix, float** pixel);

// 释放内存
HIO_API void hio_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // HEALPIX_IO_H
