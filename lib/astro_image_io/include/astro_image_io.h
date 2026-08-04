#ifndef ASTRO_IMAGE_IO_H
#define ASTRO_IMAGE_IO_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define AIO_EXPORT __declspec(dllexport)
#else
#define AIO_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define AIO_KEYWORD_NAME_MAX    72
#define AIO_KEYWORD_VALUE_MAX   72
#define AIO_KEYWORD_COMMENT_MAX 72
#define AIO_PATH_MAX            512
#define AIO_CTYPE_MAX           32
#define AIO_RADESYS_MAX         32
#define AIO_FILTER_MAX          64
#define AIO_FRAME_TYPE_MAX      32
#define AIO_BUNIT_MAX           32
#define AIO_OBJECT_MAX          128
#define AIO_OBSERVAT_MAX        64
#define AIO_DATE_MAX            64

typedef struct {
    char name[AIO_KEYWORD_NAME_MAX];
    char value[AIO_KEYWORD_VALUE_MAX];
    char comment[AIO_KEYWORD_COMMENT_MAX];
} AIOFITSKeyword;

typedef struct {
    int width;
    int height;
    int channels;
} AIOImageGeometry;

typedef struct {
    int bits_per_sample;
    int float_sample;
} AIOImageOptions;

typedef struct {
    double crpix1, crpix2;
    double crval1, crval2;
    char ctype1[AIO_CTYPE_MAX];
    char ctype2[AIO_CTYPE_MAX];
    double cd1_1, cd1_2, cd2_1, cd2_2;
    double cdelt1, cdelt2;
    int has_cdelt1, has_cdelt2;
    char radesys[AIO_RADESYS_MAX];
    double equinox;
    int has_equinox;
    double lonpole, latpole;
    int has_lonpole, has_latpole;
    int has_wcs;
} AIOWCSKeywords;

typedef struct {
    char date_obs[AIO_DATE_MAX];
    char date_end[AIO_DATE_MAX];
    double jd_obs;
    int has_jd_obs;
    double longobs, latobs, altobs;
    int has_longobs, has_latobs, has_altobs;
    char observat[AIO_OBSERVAT_MAX];
    double focallen, xpixsz, aperture, focal_ratio;
    int has_focallen, has_xpixsz, has_aperture, has_focal_ratio;
    char object_name[AIO_OBJECT_MAX];
} AIOObservationMetadata;

typedef struct {
    double exptime;
    char filter_name[AIO_FILTER_MAX];
    double gain;
    double ccd_temp;
    int has_ccd_temp;
    char frame_type[AIO_FRAME_TYPE_MAX];
    char bunit[AIO_BUNIT_MAX];
} AIOCalibrationMetadata;

typedef struct {
    AIOImageGeometry geometry;
    AIOImageOptions options;
    AIOWCSKeywords wcs;
    AIOObservationMetadata observation;
    AIOCalibrationMetadata calibration;
} AIOImageMetadata;

typedef struct AIOImageData AIOImageData;

AIO_EXPORT AIOImageData *aio_read(const char *path);
AIO_EXPORT AIOImageData *aio_read_fits(const char *path);
AIO_EXPORT AIOImageData *aio_read_xisf(const char *path);
AIO_EXPORT AIOImageData *aio_read_header_only(const char *path);
AIO_EXPORT AIOImageMetadata aio_read_metadata(const char *path);

AIO_EXPORT int aio_write_fits(const AIOImageData *image, const char *path);

AIO_EXPORT float *aio_get_pixel_data(const AIOImageData *image);
// 双精度 ABI: 获取 FP64 像素数据 (FP64 模式下返回 data_f64, FP32 模式返回 nullptr)
AIO_EXPORT double *aio_get_pixel_data_f64(const AIOImageData *image);
// 双精度 ABI: 获取 dtype (0=FP32, 1=FP64, 与 AstroScalarType 一致)
AIO_EXPORT uint8_t aio_get_dtype(const AIOImageData *image);
AIO_EXPORT int aio_get_width(const AIOImageData *image);
AIO_EXPORT int aio_get_height(const AIOImageData *image);
AIO_EXPORT int aio_get_channels(const AIOImageData *image);
AIO_EXPORT AIOImageGeometry aio_get_geometry(const AIOImageData *image);
AIO_EXPORT AIOImageOptions aio_get_options(const AIOImageData *image);
AIO_EXPORT AIOImageMetadata aio_get_metadata(const AIOImageData *image);

AIO_EXPORT int aio_get_keyword_count(const AIOImageData *image);
AIO_EXPORT AIOFITSKeyword aio_get_keyword(const AIOImageData *image, int index);
AIO_EXPORT const char *aio_get_source_format(const AIOImageData *image);
AIO_EXPORT const char *aio_get_source_path(const AIOImageData *image);

AIO_EXPORT double aio_wcs_pixel_scale(const AIOWCSKeywords *wcs);
AIO_EXPORT double aio_wcs_rotation_deg(const AIOWCSKeywords *wcs);

AIO_EXPORT void aio_free_image_data(AIOImageData *image);

AIO_EXPORT int aio_is_fits(const char *path);
AIO_EXPORT int aio_is_xisf(const char *path);

// ============================================================================
// 压缩 API (codec: 0=NONE, 1=ZSTD, 2=LZ4; level: zstd 1-22, lz4 忽略)
// ============================================================================
AIO_EXPORT size_t aio_compress(const void *src, size_t srcSize,
                                void *dst, size_t dstCapacity,
                                int codec, int level);
AIO_EXPORT size_t aio_decompress(const void *src, size_t srcSize,
                                  void *dst, size_t dstCapacity,
                                  int codec);
AIO_EXPORT size_t aio_compress_bound(size_t srcSize, int codec);

// ============================================================================
// .ahpx 读写 API (管线入口/出口的文件 I/O)
// ============================================================================
AIO_EXPORT int aio_ahpx_write(const char *path,
                               const void *pixels, int width, int height, int channels,
                               const float *snr, int snr_w, int snr_h,
                               int weight_mode, const void *weight_data,
                               int grid_w, int grid_h,
                               const char *metadata_json,
                               int zstd_level);
AIO_EXPORT int aio_ahpx_read_header(const char *path,
                                     char *metadata_json, int metadata_capacity);
AIO_EXPORT int aio_ahpx_read_pixels(const char *path,
                                     float *pixels, int capacity,
                                     int *width, int *height, int *channels);
AIO_EXPORT int aio_ahpx_read_snr(const char *path,
                                  float *snr, int capacity,
                                  int *width, int *height);

#ifdef __cplusplus
}
#endif

#include "aio_pipeline.h"
#include "aio_pipeline_engine.h"

#endif
