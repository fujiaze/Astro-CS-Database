#include "../include/astro_image_io.h"
#include "aio_log.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>

#ifdef AIO_ENABLE_FITS
#include "aio_fits.h"
#endif
#ifdef AIO_ENABLE_XISF
#include "aio_xisf.h"
#endif

static AIOImageData *alloc_image_data() {
    AIOImageData *img = (AIOImageData *)calloc(1, sizeof(AIOImageData));
    if (!img) {
        aio_log(AIO_LOG_ERROR, "API", "Failed to allocate AIOImageData");
        return nullptr;
    }
    // 显式初始化双精度 ABI 字段 (calloc 已清零, 此处显式赋值以明示意图形与防御性编程)
    img->data = nullptr;
    img->data_f64 = nullptr;
    img->dtype = 0;  // 默认 FP32 (与 AstroScalarType::FP32 一致)
    return img;
}

AIO_EXPORT AIOImageData *aio_read(const char *path) {
    if (!path) return nullptr;

#ifdef AIO_ENABLE_XISF
    if (xisf_detect(path)) {
        return aio_read_xisf(path);
    }
#endif
#ifdef AIO_ENABLE_FITS
    return aio_read_fits(path);
#else
    return nullptr;
#endif
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT AIOImageData *aio_read_fits(const char *path) {
    if (!path) return nullptr;

    auto t0 = std::chrono::high_resolution_clock::now();

    AIOImageData *img = alloc_image_data();
    if (!img) return nullptr;

    if (fits_read_file(path, img) != 0) {
        free(img);
        return nullptr;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    aio_log(AIO_LOG_INFO, "API", "aio_read_fits: %.3f s", elapsed);

    return img;
}
#endif  // AIO_ENABLE_FITS

#ifdef AIO_ENABLE_XISF
AIO_EXPORT AIOImageData *aio_read_xisf(const char *path) {
    if (!path) return nullptr;

    auto t0 = std::chrono::high_resolution_clock::now();

    AIOImageData *img = alloc_image_data();
    if (!img) return nullptr;

    if (xisf_read_file(path, img) != 0) {
        free(img);
        return nullptr;
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    aio_log(AIO_LOG_INFO, "API", "aio_read_xisf: %.3f s", elapsed);

    return img;
}
#endif

AIO_EXPORT AIOImageData *aio_read_header_only(const char *path) {
    if (!path) return nullptr;

    AIOImageData *img = alloc_image_data();
    if (!img) return nullptr;

#ifdef AIO_ENABLE_XISF
    if (xisf_detect(path)) {
        if (xisf_read_header_only(path, img) != 0) {
            free(img);
            return nullptr;
        }
    } else
#endif
#ifdef AIO_ENABLE_FITS
    {
        if (fits_read_header_only(path, img) != 0) {
            free(img);
            return nullptr;
        }
    }
#else
    {
        free(img);
        return nullptr;
    }
#endif

    return img;
}

AIO_EXPORT AIOImageMetadata aio_read_metadata(const char *path) {
    AIOImageMetadata meta;
    memset(&meta, 0, sizeof(meta));

    AIOImageData *img = aio_read_header_only(path);
    if (!img) return meta;

    meta = img->metadata;
    aio_free_image_data(img);
    return meta;
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT int aio_write_fits(const AIOImageData *image, const char *path) {
    if (!image || !path) return -1;
    return fits_write_file(image, path);
}
#endif

AIO_EXPORT float *aio_get_pixel_data(const AIOImageData *image) {
    if (!image) return nullptr;
    return image->data;
}

// 双精度 ABI: 获取 FP64 像素数据 (FP64 模式下 data_f64 非空, FP32 模式返回 nullptr)
AIO_EXPORT double *aio_get_pixel_data_f64(const AIOImageData *image) {
    if (!image) return nullptr;
    return image->data_f64;
}

// 双精度 ABI: 获取 dtype (0=FP32, 1=FP64, 与 AstroScalarType 一致)
AIO_EXPORT uint8_t aio_get_dtype(const AIOImageData *image) {
    if (!image) return 0;
    return image->dtype;
}

AIO_EXPORT int aio_get_width(const AIOImageData *image) {
    if (!image) return 0;
    return image->width;
}

AIO_EXPORT int aio_get_height(const AIOImageData *image) {
    if (!image) return 0;
    return image->height;
}

AIO_EXPORT int aio_get_channels(const AIOImageData *image) {
    if (!image) return 0;
    return image->channels;
}

AIO_EXPORT AIOImageGeometry aio_get_geometry(const AIOImageData *image) {
    AIOImageGeometry g = {0, 0, 0};
    if (!image) return g;
    return image->metadata.geometry;
}

AIO_EXPORT AIOImageOptions aio_get_options(const AIOImageData *image) {
    AIOImageOptions o = {0, 0};
    if (!image) return o;
    return image->metadata.options;
}

AIO_EXPORT AIOImageMetadata aio_get_metadata(const AIOImageData *image) {
    AIOImageMetadata m;
    memset(&m, 0, sizeof(m));
    if (!image) return m;
    return image->metadata;
}

AIO_EXPORT int aio_get_keyword_count(const AIOImageData *image) {
    if (!image) return 0;
    return image->keyword_count;
}

AIO_EXPORT AIOFITSKeyword aio_get_keyword(const AIOImageData *image, int index) {
    AIOFITSKeyword kw;
    memset(&kw, 0, sizeof(kw));
    if (!image || !image->keywords || index < 0 || index >= image->keyword_count) return kw;
    return image->keywords[index];
}

AIO_EXPORT const char *aio_get_source_format(const AIOImageData *image) {
    if (!image) return "";
    return image->source_format;
}

AIO_EXPORT const char *aio_get_source_path(const AIOImageData *image) {
    if (!image) return "";
    return image->source_path;
}

AIO_EXPORT double aio_wcs_pixel_scale(const AIOWCSKeywords *wcs) {
    if (!wcs) return 0.0;
    double det = wcs->cd1_1 * wcs->cd2_2 - wcs->cd1_2 * wcs->cd2_1;
    if (std::abs(det) < 1e-30) return 0.0;
    return std::sqrt(std::abs(det)) * 3600.0;
}

AIO_EXPORT double aio_wcs_rotation_deg(const AIOWCSKeywords *wcs) {
    if (!wcs) return 0.0;
    double scale_x = std::sqrt(wcs->cd1_1 * wcs->cd1_1 + wcs->cd2_1 * wcs->cd2_1);
    if (scale_x < 1e-15) return 0.0;
    return std::atan2(wcs->cd2_1, wcs->cd1_1) * 180.0 / 3.14159265358979323846;
}

AIO_EXPORT void aio_free_image_data(AIOImageData *image) {
    if (!image) return;
    // 双精度 ABI: 同时释放 data 与 data_f64 (二者互斥, 但均需检查)
    if (image->data) free(image->data);
    if (image->data_f64) free(image->data_f64);
    if (image->keywords) free(image->keywords);
    free(image);
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT int aio_is_fits(const char *path) {
    if (!path) return 0;
    return fits_detect(path);
}
#endif

#ifdef AIO_ENABLE_XISF
AIO_EXPORT int aio_is_xisf(const char *path) {
    if (!path) return 0;
    return xisf_detect(path);
}
#endif
