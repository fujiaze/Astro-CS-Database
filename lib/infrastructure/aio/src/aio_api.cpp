#include "../include/astro_image_io.h"
#include "aio_log.h"
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <exception>

#ifdef AIO_ENABLE_FITS
#include "aio_fits.h"      // AIOImageData 完整定义 (公共头仅前向声明)
#endif
#ifdef AIO_ENABLE_XISF
#include "aio_xisf.h"      // aio_xisf_read_file / aio_xisf_detect 声明
#endif

// ============================================================================
// P0-4: C ABI 边界统一异常屏障 (bughunt_p0_io)
// ----------------------------------------------------------------------------
// 本文件全部 AIO_EXPORT 入口均为 extern "C" 可达边界 (DLL 消费方含 C 调用方)。
// 此前 25 个入口 0 个有 try 保护: C++ 异常 (std::stod/stoi 抛 invalid_argument、
// std::vector 分配抛 bad_alloc、std::string 构造抛 length_error 等) 可跨
// extern "C" 传播, 在 MSVC/GCC 下均为 UB/立即终止。
// 统一口径: 异常一律拦在 C 边界内转既有错误返回, 逐入口按返回语义选择:
//   - 指针返回型        -> return nullptr
//   - int 返回型        -> return -1 (aio_is_* 布尔检测型 return 0)
//   - 浮点返回型        -> return 0.0
//   - 结构体值返回型    -> 零初始化返回 (与既有 !image 缺省分支一致)
//   - void 返回型       -> 仅记录日志
// 不改变任何正常路径行为: try 块内代码与修复前逐行等价。
// ============================================================================

// 修复: AIO 模块级精度模式全局变量
// PrecisionContext 单例在 DLL 边界不共享 (EXE 和 DLL 各有一份副本),
// 必须通过 aio_set_precision_mode 显式设置 AIO 模块的精度模式
static int g_aio_precision_mode_fp64 = 0;  // 0=FP32, 1=FP64

AIO_EXPORT void aio_set_precision_mode(int is_fp64) {
    try {
        g_aio_precision_mode_fp64 = is_fp64 ? 1 : 0;
        aio_log(AIO_LOG_INFO, "API", "Precision mode set to %s",
                is_fp64 ? "FP64" : "FP32");
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_set_precision_mode exception: %s", e.what());
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_set_precision_mode unknown exception");
    }
}

// 供 aio_fits.cpp / aio_xisf.cpp 查询的内部接口
extern "C" int aio_internal_is_fp64() {
    return g_aio_precision_mode_fp64;
}

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

    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr)
    try {
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
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read exception: %s", e.what());
        return nullptr;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read unknown exception");
        return nullptr;
    }
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT AIOImageData *aio_read_fits(const char *path) {
    if (!path) return nullptr;

    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr); 局部 img 移到 try 外
    // 便于 catch 释放, 不泄漏。
    AIOImageData *img = nullptr;
    try {
        auto t0 = std::chrono::high_resolution_clock::now();

        img = alloc_image_data();
        if (!img) return nullptr;

        if (fits_read_file(path, img) != 0) {
            aio_free_image_data(img);
            return nullptr;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        aio_log(AIO_LOG_INFO, "API", "aio_read_fits: %.3f s", elapsed);

        return img;
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_fits exception: %s", e.what());
        if (img) aio_free_image_data(img);
        return nullptr;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_fits unknown exception");
        if (img) aio_free_image_data(img);
        return nullptr;
    }
}
#endif  // AIO_ENABLE_FITS

#ifdef AIO_ENABLE_XISF
AIO_EXPORT AIOImageData *aio_read_xisf(const char *path) {
    if (!path) return nullptr;

    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr); 局部 img 移到 try 外
    // 便于 catch 释放, 不泄漏。
    AIOImageData *img = nullptr;
    try {
        auto t0 = std::chrono::high_resolution_clock::now();

        img = alloc_image_data();
        if (!img) return nullptr;

        if (xisf_read_file(path, img) != 0) {
            aio_free_image_data(img);
            return nullptr;
        }

        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed = std::chrono::duration<double>(t1 - t0).count();
        aio_log(AIO_LOG_INFO, "API", "aio_read_xisf: %.3f s", elapsed);

        return img;
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_xisf exception: %s", e.what());
        if (img) aio_free_image_data(img);
        return nullptr;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_xisf unknown exception");
        if (img) aio_free_image_data(img);
        return nullptr;
    }
}
#endif

AIO_EXPORT AIOImageData *aio_read_header_only(const char *path) {
    if (!path) return nullptr;

    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr); 局部 img 移到 try 外
    // 便于 catch 释放, 不泄漏。
    AIOImageData *img = nullptr;
    try {
        img = alloc_image_data();
        if (!img) return nullptr;

#ifdef AIO_ENABLE_XISF
        if (xisf_detect(path)) {
            if (xisf_read_header_only(path, img) != 0) {
                aio_free_image_data(img);
                return nullptr;
            }
        } else
#endif
#ifdef AIO_ENABLE_FITS
        {
            if (fits_read_header_only(path, img) != 0) {
                aio_free_image_data(img);
                return nullptr;
            }
        }
#else
        {
            aio_free_image_data(img);
            return nullptr;
        }
#endif

        return img;
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_header_only exception: %s", e.what());
        if (img) aio_free_image_data(img);
        return nullptr;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_header_only unknown exception");
        if (img) aio_free_image_data(img);
        return nullptr;
    }
}

AIO_EXPORT AIOImageMetadata aio_read_metadata(const char *path) {
    // P0-4: C 边界异常屏障 (结构体值返回型 -> 零初始化, 与既有缺省分支一致)
    AIOImageMetadata meta;
    memset(&meta, 0, sizeof(meta));
    try {
        AIOImageData *img = aio_read_header_only(path);
        if (!img) return meta;

        meta = img->metadata;
        aio_free_image_data(img);
        return meta;
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_metadata exception: %s", e.what());
        memset(&meta, 0, sizeof(meta));
        return meta;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_read_metadata unknown exception");
        memset(&meta, 0, sizeof(meta));
        return meta;
    }
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT int aio_write_fits(const AIOImageData *image, const char *path) {
    if (!image || !path) return -1;
    // P0-4: C 边界异常屏障 (int 返回型 -> 既有错误码 -1)
    try {
        return fits_write_file(image, path);
    } catch (const std::exception &e) {
        aio_log(AIO_LOG_ERROR, "API", "aio_write_fits exception: %s", e.what());
        return -1;
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_write_fits unknown exception");
        return -1;
    }
}
#endif

AIO_EXPORT float *aio_get_pixel_data(const AIOImageData *image) {
    if (!image) return nullptr;
    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr)
    try {
        return image->data;
    } catch (...) {
        return nullptr;
    }
}

// 双精度 ABI: 获取 FP64 像素数据 (FP64 模式下 data_f64 非空, FP32 模式返回 nullptr)
AIO_EXPORT double *aio_get_pixel_data_f64(const AIOImageData *image) {
    if (!image) return nullptr;
    // P0-4: C 边界异常屏障 (指针返回型 -> nullptr)
    try {
        return image->data_f64;
    } catch (...) {
        return nullptr;
    }
}

// 双精度 ABI: 获取 dtype (0=FP32, 1=FP64, 与 AstroScalarType 一致)
AIO_EXPORT uint8_t aio_get_dtype(const AIOImageData *image) {
    if (!image) return 0;
    // P0-4: C 边界异常屏障 (整型返回型 -> 0, 与既有缺省分支一致)
    try {
        return image->dtype;
    } catch (...) {
        return 0;
    }
}

AIO_EXPORT int aio_get_width(const AIOImageData *image) {
    if (!image) return 0;
    // P0-4: C 边界异常屏障 (int 返回型 -> 0, 与既有缺省分支一致)
    try {
        return image->width;
    } catch (...) {
        return 0;
    }
}

AIO_EXPORT int aio_get_height(const AIOImageData *image) {
    if (!image) return 0;
    // P0-4: C 边界异常屏障 (int 返回型 -> 0, 与既有缺省分支一致)
    try {
        return image->height;
    } catch (...) {
        return 0;
    }
}

AIO_EXPORT int aio_get_channels(const AIOImageData *image) {
    if (!image) return 0;
    // P0-4: C 边界异常屏障 (int 返回型 -> 0, 与既有缺省分支一致)
    try {
        return image->channels;
    } catch (...) {
        return 0;
    }
}

AIO_EXPORT AIOImageGeometry aio_get_geometry(const AIOImageData *image) {
    AIOImageGeometry g = {0, 0, 0};
    if (!image) return g;
    // P0-4: C 边界异常屏障 (结构体值返回型 -> 零初始化, 与既有缺省分支一致)
    try {
        return image->metadata.geometry;
    } catch (...) {
        return g;
    }
}

AIO_EXPORT AIOImageOptions aio_get_options(const AIOImageData *image) {
    AIOImageOptions o = {0, 0};
    if (!image) return o;
    // P0-4: C 边界异常屏障 (结构体值返回型 -> 零初始化, 与既有缺省分支一致)
    try {
        return image->metadata.options;
    } catch (...) {
        return o;
    }
}

AIO_EXPORT AIOImageMetadata aio_get_metadata(const AIOImageData *image) {
    AIOImageMetadata m;
    memset(&m, 0, sizeof(m));
    if (!image) return m;
    // P0-4: C 边界异常屏障 (结构体值返回型 -> 零初始化, 与既有缺省分支一致)
    try {
        return image->metadata;
    } catch (...) {
        return m;
    }
}

AIO_EXPORT int aio_get_keyword_count(const AIOImageData *image) {
    if (!image) return 0;
    // P0-4: C 边界异常屏障 (int 返回型 -> 0, 与既有缺省分支一致)
    try {
        return image->keyword_count;
    } catch (...) {
        return 0;
    }
}

AIO_EXPORT AIOFITSKeyword aio_get_keyword(const AIOImageData *image, int index) {
    AIOFITSKeyword kw;
    memset(&kw, 0, sizeof(kw));
    // P0-4: C 边界异常屏障 (结构体值返回型 -> 零初始化, 与既有缺省分支一致)
    try {
        if (!image || !image->keywords || index < 0 || index >= image->keyword_count) return kw;
        return image->keywords[index];
    } catch (...) {
        return kw;
    }
}

AIO_EXPORT const char *aio_get_source_format(const AIOImageData *image) {
    if (!image) return "";
    // P0-4: C 边界异常屏障 (指针返回型 -> 既有缺省空串)
    try {
        return image->source_format;
    } catch (...) {
        return "";
    }
}

AIO_EXPORT const char *aio_get_source_path(const AIOImageData *image) {
    if (!image) return "";
    // P0-4: C 边界异常屏障 (指针返回型 -> 既有缺省空串)
    try {
        return image->source_path;
    } catch (...) {
        return "";
    }
}

AIO_EXPORT double aio_wcs_pixel_scale(const AIOWCSKeywords *wcs) {
    if (!wcs) return 0.0;
    // P0-4: C 边界异常屏障 (浮点返回型 -> 0.0, 与既有缺省分支一致)
    try {
        double det = wcs->cd1_1 * wcs->cd2_2 - wcs->cd1_2 * wcs->cd2_1;
        if (std::abs(det) < 1e-30) return 0.0;
        return std::sqrt(std::abs(det)) * 3600.0;
    } catch (...) {
        return 0.0;
    }
}

AIO_EXPORT double aio_wcs_rotation_deg(const AIOWCSKeywords *wcs) {
    if (!wcs) return 0.0;
    // P0-4: C 边界异常屏障 (浮点返回型 -> 0.0, 与既有缺省分支一致)
    try {
        double scale_x = std::sqrt(wcs->cd1_1 * wcs->cd1_1 + wcs->cd2_1 * wcs->cd2_1);
        if (scale_x < 1e-15) return 0.0;
        return std::atan2(wcs->cd2_1, wcs->cd1_1) * 180.0 / 3.14159265358979323846;
    } catch (...) {
        return 0.0;
    }
}

AIO_EXPORT void aio_free_image_data(AIOImageData *image) {
    // P0-4: C 边界异常屏障 (void 返回型 -> 仅记录, 保证不跨 C 边界传播)
    try {
        if (!image) return;
        // 双精度 ABI: 同时释放 data 与 data_f64 (二者互斥, 但均需检查)
        if (image->data) free(image->data);
        if (image->data_f64) free(image->data_f64);
        if (image->keywords) free(image->keywords);
        free(image);
    } catch (...) {
        aio_log(AIO_LOG_ERROR, "API", "aio_free_image_data exception (ignored)");
    }
}

#ifdef AIO_ENABLE_FITS
AIO_EXPORT int aio_is_fits(const char *path) {
    if (!path) return 0;
    // P0-4: C 边界异常屏障 (布尔检测型 -> 0=否, 与既有缺省分支一致)
    try {
        return fits_detect(path);
    } catch (...) {
        return 0;
    }
}
#endif

#ifdef AIO_ENABLE_XISF
AIO_EXPORT int aio_is_xisf(const char *path) {
    if (!path) return 0;
    // P0-4: C 边界异常屏障 (布尔检测型 -> 0=否, 与既有缺省分支一致)
    try {
        return xisf_detect(path);
    } catch (...) {
        return 0;
    }
}
#endif
