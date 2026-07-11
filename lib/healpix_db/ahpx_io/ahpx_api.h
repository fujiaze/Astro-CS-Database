#ifndef AHPX_API_H
#define AHPX_API_H

// ============================================================================
// .ahpx 单帧存储格式 C API 导出层
// 供 Python (ctypes) 或其他 C 程序调用
// 所有返回指针的函数: 调用方负责 free()
// ============================================================================

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AHPX_EXPORT __declspec(dllexport)
#else
#define AHPX_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 不透明类型 (隐藏 C++ 实现)
typedef struct AhpxReader AhpxReader;
typedef struct AhpxWriter AhpxWriter;

// ============================================================================
// 读取 API
// ============================================================================

// 打开 .ahpx 文件, 解析头
// 返回: 读取器指针, 失败返回 NULL
AHPX_EXPORT AhpxReader* ahpx_open(const char* path);

// 获取 JSON 头 (已解压)
// 返回: JSON 字符串指针 (内部管理, 不需调用方 free, reader 关闭后失效)
// 失败返回 NULL
AHPX_EXPORT const char* ahpx_get_header_json(AhpxReader* reader);

// 获取图像几何信息
// w/h/c 输出宽/高/通道数
// 返回: 1=成功, 0=失败
AHPX_EXPORT int ahpx_get_image_info(AhpxReader* reader, int* w, int* h, int* c);

// 读取像素数据 (float32)
// 返回: malloc 分配的 float 数组 (调用方 free()), 失败返回 NULL
// 数组长度 = w * h * c
AHPX_EXPORT float* ahpx_read_pixels(AhpxReader* reader);

// 读取 SNR 图 (float32)
// 返回: malloc 分配的 float 数组 (调用方 free()), 失败返回 NULL
// 数组长度 = w * h
AHPX_EXPORT float* ahpx_read_snr(AhpxReader* reader);

// 读取权重 (float32)
// mode:  输出权重模式 (0=SCALAR, 1=GRID, 2=PIXEL)
// gw/gh: 输出网格宽高 (仅 GRID 模式有效)
// count: 输出 float 数量
// 返回: malloc 分配的 float 数组 (调用方 free()), 失败返回 NULL
AHPX_EXPORT float* ahpx_read_weight(AhpxReader* reader, int* mode, int* gw, int* gh, int* count);

// 关闭读取器, 释放资源
AHPX_EXPORT void ahpx_close(AhpxReader* reader);

// ============================================================================
// 写入 API
// ============================================================================

// 创建写入器
// 返回: 写入器指针, 失败返回 NULL
AHPX_EXPORT AhpxWriter* ahpx_writer_new();

// 设置元数据 JSON (调用方构建完整 JSON 字符串)
AHPX_EXPORT void ahpx_writer_set_metadata(AhpxWriter* w, const char* json);

// 设置图像数据 (float32)
AHPX_EXPORT void ahpx_writer_set_pixels(AhpxWriter* w, const float* data,
                                         int width, int height, int channels);

// 设置 SNR 图 (float32, W×H)
AHPX_EXPORT void ahpx_writer_set_snr(AhpxWriter* w, const float* data,
                                      int width, int height);

// 设置权重 - 标量模式
AHPX_EXPORT void ahpx_writer_set_weight_scalar(AhpxWriter* w, float scalar);

// 设置权重 - 网格模式
AHPX_EXPORT void ahpx_writer_set_weight_grid(AhpxWriter* w, const float* grid,
                                              int gw, int gh);

// 设置权重 - 逐像素模式
AHPX_EXPORT void ahpx_writer_set_weight_pixel(AhpxWriter* w, const float* data,
                                               int width, int height);

// 写入文件
// zstd_level: ZSTD 压缩级别 (1-22, 0=不压缩, 推荐 5)
// 返回: 1=成功, 0=失败
AHPX_EXPORT int ahpx_writer_write(AhpxWriter* w, const char* path, int zstd_level);

// 释放写入器
AHPX_EXPORT void ahpx_writer_free(AhpxWriter* w);

// ============================================================================
// 工具函数
// ============================================================================

// 检查文件是否为 .ahpx 格式 (检查 Magic)
// 返回: 1=是, 0=否
AHPX_EXPORT int ahpx_is_ahpx(const char* path);

// 释放由 ahpx_read_pixels/snr/weight 返回的 malloc 分配内存
// (调用方必须用此函数释放, 不能用其他 C runtime 的 free)
AHPX_EXPORT void ahpx_free(void* ptr);

#ifdef __cplusplus
}
#endif

#endif // AHPX_API_H
