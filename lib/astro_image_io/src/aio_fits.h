#pragma once

#include "../include/astro_image_io.h"
#include <vector>
#include <string>

// AIOImageData 双精度 ABI (FP32/FP64 双模式):
//   - data      : FP32 模式像素数据 (向后兼容, 历史路径不变)
//   - data_f64  : FP64 模式像素数据 (FP64 模式下 FITS/XISF 不再降级到 float32)
//   - dtype     : 0=FP32 (与 AstroScalarType::FP32 一致), 1=FP64 (与 AstroScalarType::FP64 一致)
// 约束:
//   - 同一 image 中, data 与 data_f64 至多其一非空 (互斥)
//   - aio_free_image_data 必须同时释放 data 与 data_f64
//   - FP64 模式下, 读取器直接存储 double 到 data_f64, data 保持 nullptr
struct AIOImageData {
    float *data;            // FP32 模式像素数据 (向后兼容)
    double *data_f64;       // FP64 模式像素数据 (FP64 模式, 与 data 互斥)
    uint8_t dtype;          // 0=FP32, 1=FP64 (与 AstroScalarType 一致)
    int width;
    int height;
    int channels;
    int bits_per_sample;
    int float_sample;
    char source_format[16];
    char source_path[AIO_PATH_MAX];
    AIOFITSKeyword *keywords;
    int keyword_count;
    AIOImageMetadata metadata;
};

struct FITSHeader {
    std::vector<AIOFITSKeyword> keywords;
    int bitpix;
    int naxis;
    int naxis1, naxis2, naxis3;
    double bscale;
    double bzero;
    size_t data_offset;
    size_t data_size;
};

int fits_read_file(const char *path, AIOImageData *out);
int fits_read_header_only(const char *path, AIOImageData *out);
int fits_write_file(const AIOImageData *image, const char *path);
int fits_detect(const char *path);
