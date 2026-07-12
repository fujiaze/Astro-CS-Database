// ============================================================================
// aio_ahpx_api.cpp - .ahpx 简化 C API 实现
//
// 提供 4 个一次性调用的 C 函数, 供 Python (ctypes) 或其他 C 程序调用:
//   aio_ahpx_write        - 写入 .ahpx 文件 (一次性写入所有数据)
//   aio_ahpx_read_header  - 读取 .ahpx 文件元数据 (不读像素)
//   aio_ahpx_read_pixels  - 读取 .ahpx 文件像素数据
//   aio_ahpx_read_snr     - 读取 .ahpx 文件 SNR 数据
//
// 返回值: 0=成功, 非0=失败
// ============================================================================

#include "../include/astro_image_io.h"
#include "aio_ahpx_writer.h"
#include "aio_ahpx_reader.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ============================================================================
// aio_ahpx_write - 写入 .ahpx 文件 (一次性写入所有数据)
//
// path:          输出文件路径
// pixels:        像素数据 (float32, width×height×channels)
// width/height/channels: 图像几何
// snr:           SNR 图 (float32, snr_w×snr_h), 可为 nullptr
// snr_w/snr_h:   SNR 图几何
// weight_mode:   0=SCALAR, 1=GRID, 2=PIXEL
// weight_data:   权重数据 (float32), 可为 nullptr (使用默认标量 1.0)
// grid_w/grid_h: 仅 GRID 模式有效
// metadata_json: 元数据 JSON 字符串, 可为 nullptr
// zstd_level:    ZSTD 压缩级别 (1-22, 0=不压缩, 推荐 5)
// ============================================================================
AIO_EXPORT int aio_ahpx_write(const char *path,
                               const void *pixels, int width, int height, int channels,
                               const float *snr, int snr_w, int snr_h,
                               int weight_mode, const void *weight_data,
                               int grid_w, int grid_h,
                               const char *metadata_json,
                               int zstd_level) {
    if (!path || !pixels || width <= 0 || height <= 0 || channels <= 0) {
        fprintf(stderr, "[aio][ahpx][api] write: 无效参数 (path=%p pixels=%p w=%d h=%d c=%d)\n",
                path, pixels, width, height, channels);
        return 1;
    }

    aio::ahpx::AhpxWriter writer;

    // 设置元数据 (如果提供)
    if (metadata_json) {
        writer.setMetadata(std::string(metadata_json));
    }

    // 设置像素数据
    writer.setPixels(reinterpret_cast<const float*>(pixels), width, height, channels);

    // 设置 SNR (如果提供)
    if (snr && snr_w > 0 && snr_h > 0) {
        writer.setSnr(snr, snr_w, snr_h);
    }

    // 设置权重
    if (weight_mode == 0) {
        // SCALAR 模式
        if (weight_data) {
            writer.setWeightScalar(*reinterpret_cast<const float*>(weight_data));
        } else {
            // 默认标量权重 1.0
            writer.setWeightScalar(1.0f);
        }
    } else if (weight_mode == 1) {
        // GRID 模式
        if (!weight_data || grid_w <= 0 || grid_h <= 0) {
            fprintf(stderr, "[aio][ahpx][api] write: GRID 模式缺少权重数据或网格参数\n");
            return 2;
        }
        writer.setWeightGrid(reinterpret_cast<const float*>(weight_data),
                             (uint16_t)grid_w, (uint16_t)grid_h);
    } else if (weight_mode == 2) {
        // PIXEL 模式
        if (!weight_data) {
            fprintf(stderr, "[aio][ahpx][api] write: PIXEL 模式缺少权重数据\n");
            return 3;
        }
        writer.setWeightPixel(reinterpret_cast<const float*>(weight_data), width, height);
    } else {
        fprintf(stderr, "[aio][ahpx][api] write: 未知 weight_mode=%d\n", weight_mode);
        return 4;
    }

    // 写入文件
    aio::ahpx::AhpxWriteConfig config;
    config.zstdLevel = zstd_level;
    if (!writer.write(path, config)) {
        fprintf(stderr, "[aio][ahpx][api] write: 写入失败: %s\n", path);
        return 5;
    }

    return 0;
}

// ============================================================================
// aio_ahpx_read_header - 读取 .ahpx 文件元数据 (不读像素)
//
// path:             输入文件路径
// metadata_json:    输出缓冲区, 拷贝 JSON 头 (可为 nullptr, 仅校验文件)
// metadata_capacity: 缓冲区容量 (字节)
// ============================================================================
AIO_EXPORT int aio_ahpx_read_header(const char *path,
                                     char *metadata_json, int metadata_capacity) {
    if (!path) {
        fprintf(stderr, "[aio][ahpx][api] read_header: path 为空\n");
        return 1;
    }

    aio::ahpx::AhpxReader reader;
    if (!reader.open(path)) {
        fprintf(stderr, "[aio][ahpx][api] read_header: 打开文件失败: %s\n", path);
        return 2;
    }

    // 获取 JSON 头
    const std::string& headerJson = reader.getHeaderJson();

    // 如果调用方提供了缓冲区, 拷贝 JSON 头
    if (metadata_json && metadata_capacity > 0) {
        size_t copyLen = headerJson.size();
        if (copyLen >= (size_t)metadata_capacity) {
            // 缓冲区不足, 截断
            copyLen = (size_t)metadata_capacity - 1;
            fprintf(stderr, "[aio][ahpx][api] read_header: 缓冲区不足 (需要 %zu, 容量 %d), 已截断\n",
                    headerJson.size() + 1, metadata_capacity);
        }
        std::memcpy(metadata_json, headerJson.data(), copyLen);
        metadata_json[copyLen] = '\0';
    }

    return 0;
}

// ============================================================================
// aio_ahpx_read_pixels - 读取 .ahpx 文件像素数据
//
// path:     输入文件路径
// pixels:   输出缓冲区 (float32, 调用方分配)
// capacity: 缓冲区容量 (float 数量)
// width/height/channels: 输出图像几何
// ============================================================================
AIO_EXPORT int aio_ahpx_read_pixels(const char *path,
                                     float *pixels, int capacity,
                                     int *width, int *height, int *channels) {
    if (!path) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: path 为空\n");
        return 1;
    }
    if (!pixels || capacity <= 0) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 无效缓冲区 (pixels=%p capacity=%d)\n",
                pixels, capacity);
        return 2;
    }

    aio::ahpx::AhpxReader reader;
    if (!reader.open(path)) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 打开文件失败: %s\n", path);
        return 3;
    }

    // 获取图像几何信息
    int w = 0, h = 0, c = 0;
    if (!reader.getImageInfo(&w, &h, &c)) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 无法获取图像几何信息\n");
        return 4;
    }
    if (w <= 0 || h <= 0 || c <= 0) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 无效几何 (w=%d h=%d c=%d)\n", w, h, c);
        return 5;
    }

    // 检查缓冲区容量
    size_t requiredCount = (size_t)w * h * c;
    if ((size_t)capacity < requiredCount) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 缓冲区不足 (需要 %zu, 容量 %d)\n",
                requiredCount, capacity);
        return 6;
    }

    // 读取像素数据
    std::vector<float> pixelData = reader.readPixels();
    if (pixelData.empty()) {
        fprintf(stderr, "[aio][ahpx][api] read_pixels: 读取像素数据失败\n");
        return 7;
    }

    // 拷贝到调用方缓冲区
    std::memcpy(pixels, pixelData.data(), pixelData.size() * sizeof(float));

    // 输出几何信息
    if (width)    *width = w;
    if (height)   *height = h;
    if (channels) *channels = c;

    return 0;
}

// ============================================================================
// aio_ahpx_read_snr - 读取 .ahpx 文件 SNR 数据
//
// path:     输入文件路径
// snr:      输出缓冲区 (float32, 调用方分配)
// capacity: 缓冲区容量 (float 数量)
// width/height: 输出 SNR 图几何
// ============================================================================
AIO_EXPORT int aio_ahpx_read_snr(const char *path,
                                  float *snr, int capacity,
                                  int *width, int *height) {
    if (!path) {
        fprintf(stderr, "[aio][ahpx][api] read_snr: path 为空\n");
        return 1;
    }
    if (!snr || capacity <= 0) {
        fprintf(stderr, "[aio][ahpx][api] read_snr: 无效缓冲区 (snr=%p capacity=%d)\n",
                snr, capacity);
        return 2;
    }

    aio::ahpx::AhpxReader reader;
    if (!reader.open(path)) {
        fprintf(stderr, "[aio][ahpx][api] read_snr: 打开文件失败: %s\n", path);
        return 3;
    }

    // 读取 SNR 数据
    std::vector<float> snrData = reader.readSnr();
    if (snrData.empty()) {
        fprintf(stderr, "[aio][ahpx][api] read_snr: 读取 SNR 数据失败 (文件可能不包含 SNR)\n");
        return 4;
    }

    // 检查缓冲区容量
    if ((size_t)capacity < snrData.size()) {
        fprintf(stderr, "[aio][ahpx][api] read_snr: 缓冲区不足 (需要 %zu, 容量 %d)\n",
                snrData.size(), capacity);
        return 5;
    }

    // 拷贝到调用方缓冲区
    std::memcpy(snr, snrData.data(), snrData.size() * sizeof(float));

    // 输出 SNR 图几何 (从图像几何推断, SNR 与图像同尺寸)
    int w = 0, h = 0, c = 0;
    if (reader.getImageInfo(&w, &h, &c)) {
        if (width)  *width = w;
        if (height) *height = h;
    }

    return 0;
}
