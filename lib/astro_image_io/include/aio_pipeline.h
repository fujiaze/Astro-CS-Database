#ifndef AIO_PIPELINE_H
#define AIO_PIPELINE_H

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

typedef enum {
    STAGE_CALIBRATE    = 0,
    STAGE_PLATESOLVE   = 1,
    STAGE_PHOTOMETRIC  = 2,
    STAGE_DRIZZLE      = 3,
    STAGE_STACK        = 4,
} PipelineStage;

typedef struct {
    /* ---- 图像数据 (平面像素, 校准/platesolve/光度阶段使用) ---- */
    float* pixel_data;        /* 像素数据 (HWC), frame 拥有, 按需分配/释放 */
    int    width;
    int    height;
    int    channels;

    /* ---- WCS 数据 (platesolve 阶段写入) ---- */
    double cd[4];
    double crval[2];
    double crpix[2];
    char   ctype1[16];
    char   ctype2[16];
    double sip_a[36];         /* 前向 SIP */
    double sip_b[36];
    double sip_ap[36];        /* 逆向 SIP */
    double sip_bp[36];
    int    sip_order;
    int    sip_ap_order;

    /* ---- 辅助数据 ---- */
    float* snr_data;          /* SNR 图 (nullable, frame 拥有, 按需分配/释放) */
    float* weight_data;       /* 权重图 (nullable, frame 拥有, 按需分配/释放) */

    /* ---- HEALPix 数据 (drizzle 阶段写入, stack 阶段读取) ---- */
    float*   healpix_pixels;  /* HEALPix 像素值 (归一化亮度) */
    float*   healpix_snr;     /* HEALPix SNR */
    int64_t* healpix_ipix;    /* HEALPix 像素号 */
    int64_t  n_healpix;       /* HEALPix 像素数 */
    int      nside;
    int      nested;
    double   pixfrac;         /* drizzle 使用的 pixfrac */

    /* ---- 元数据 ---- */
    char     source_path[512];  /* 源 FITS 文件路径 */
    char     object_name[128];
    double   exptime;
    char     filter_name[64];
    double   jd_obs;
    double   rms_arcsec;        /* platesolve RMS */
    int      n_pairs;           /* platesolve 匹配对数 */

    /* ---- 状态标记 ---- */
    int      stages_completed;  /* 位掩码: bit0=calibrated, bit1=solved, bit2=photometric, bit3=drizzled */
    int      has_wcs;
    int      has_sip;
} PipelineFrame;

typedef int (*PipelineStageFn)(const PipelineFrame* input, PipelineFrame* output, const void* params, char* error_msg, int error_capacity);

AIO_EXPORT PipelineFrame* aio_pipeline_frame_create(void);
AIO_EXPORT void aio_pipeline_frame_destroy(PipelineFrame* frame);
AIO_EXPORT int aio_pipeline_frame_alloc_pixels(PipelineFrame* frame, int width, int height, int channels);
AIO_EXPORT int aio_pipeline_frame_alloc_snr(PipelineFrame* frame, int width, int height);
AIO_EXPORT int aio_pipeline_frame_alloc_weight(PipelineFrame* frame, int width, int height);
AIO_EXPORT int aio_pipeline_frame_alloc_healpix(PipelineFrame* frame, int64_t n_pixels);
AIO_EXPORT void aio_pipeline_frame_free_pixels(PipelineFrame* frame);
AIO_EXPORT void aio_pipeline_frame_free_snr(PipelineFrame* frame);
AIO_EXPORT void aio_pipeline_frame_free_weight(PipelineFrame* frame);
AIO_EXPORT void aio_pipeline_frame_free_healpix(PipelineFrame* frame);
AIO_EXPORT size_t aio_pipeline_frame_memory_usage(const PipelineFrame* frame);

AIO_EXPORT int aio_pipeline_export_xml(const PipelineFrame* frame, const char* path, const char* comment);

#ifdef __cplusplus
}
#endif

#endif /* AIO_PIPELINE_H */

/* ===========================================================================
 * 内存生命周期约定表
 * ---------------------------------------------------------------------------
 * | 阶段          | 操作                                                       |
 * |---------------|------------------------------------------------------------|
 * | 校准入口      | 分配 pixel_data / weight_data                               |
 * | Platesolve 后 | 可释放 weight_data                                          |
 * | Drizzle 后    | 分配 healpix_*，可释放 pixel_data / snr_data / weight_data  |
 * | Stack 后      | 可释放 healpix_*                                            |
 * ---------------------------------------------------------------------------
 * 说明: PipelineFrame 中所有带指针的字段由 frame 拥有，按需分配/释放。
 *       调用者通过 aio_pipeline_frame_alloc_* / aio_pipeline_frame_free_*
 *       接口显式管理；frame 销毁时由 aio_pipeline_frame_destroy 统一回收
 *       尚未释放的缓冲区，避免内存泄漏。
 * ===========================================================================
 *
 * ===========================================================================
 * 模块与管线字段映射表
 * ---------------------------------------------------------------------------
 * | 模块        | 读取字段                    | 写入字段                                                       |
 * |-------------|----------------------------|----------------------------------------------------------------|
 * | 校准        | pixel_data, weight_data    | pixel_data                                                     |
 * | Platesolve  | pixel_data                 | cd, crval, crpix, ctype, sip_*, rms_arcsec, n_pairs, has_wcs, has_sip |
 * | 光度标定    | pixel_data, wcs            | pixel_data, snr_data                                           |
 * | Drizzle     | pixel_data, snr_data, wcs  | healpix_*, nside, nested, pixfrac                              |
 * | Stack       | healpix_*                  | (输出到 .ahps)                                                 |
 * ---------------------------------------------------------------------------
 * 注: "wcs" 代表 cd / crval / crpix / ctype / sip_* 等 WCS 相关字段的集合。
 * ===========================================================================
 */
