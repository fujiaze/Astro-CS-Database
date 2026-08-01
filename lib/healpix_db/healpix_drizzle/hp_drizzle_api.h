#ifndef HP_DRIZZLE_API_H
#define HP_DRIZZLE_API_H

#ifdef _WIN32
#  ifdef HP_DRIZZLE_EXPORTS
#    define HP_DRIZZLE_API __declspec(dllexport)
#  else
#    define HP_DRIZZLE_API __declspec(dllimport)
#  endif
#else
#  define HP_DRIZZLE_API __attribute__((visibility("default")))
#endif

#include <cstdint>
#include "aio_pipeline.h"   // PipelineFrame 定义

#ifdef __cplusplus
extern "C" {
#endif

// Drizzle 结果
typedef struct {
    int64_t n_healpix_pixels;   // 有效 HEALPix 像素数
    int64_t n_source_pixels;    // 源图像像素数
    int     nside;
    int     nested;             // 1=NESTED, 0=RING
    double  pixfrac;
    double  elapsed_sec;
    char    error_msg[512];     // 错误信息
} HpDrizzleResult;

// 执行 Drizzle: FITS → .hiss
// fits_path: 输入 FITS 文件路径 (UTF-8)
// output_path: 输出 .hiss 文件路径 (UTF-8, 若以 .ahpx 结尾会自动改为 .hiss)
// nside: HEALPix nside (默认 32768)
// nested: 1=NESTED, 0=RING
// pixfrac: 像素收缩因子 (0.0~1.0, 默认 1.0 避免源像素固有缝隙)
// snr_path: 可选 SNR FITS 文件路径 (nullptr 则不用)
// weight_path: 可选权重 FITS 文件路径 (nullptr 则不用)
// result: 输出结果
// 返回: 0=成功, 非0=失败
HP_DRIZZLE_API int hp_drizzle_fits_to_ahpx(
    const char* fits_path,
    const char* output_path,
    int nside,
    int nested,
    double pixfrac,
    const char* snr_path,
    const char* weight_path,
    HpDrizzleResult* result
);

// Drizzle 阶段: 从 PipelineFrame 命名块直通调用 DrizzleEngine (不经临时 FITS 文件)
// frame: 输入帧 (需含 "data" 块 [H,W] float32 + "header" KV 块含 WCS/SIP 字段)
// nside: HEALPix nside
// nested: 1=NESTED, 0=RING
// pixfrac: 像素收缩因子 (0.0~1.0)
// output_path: 输出 .hiss 文件路径 (nullptr 则不写文件, 仅返回统计; 若以 .ahpx 结尾会自动改为 .hiss)
// result: 输出结果统计与错误信息
// 返回: 0=成功, 非0=失败
HP_DRIZZLE_API int hp_drizzle_run(PipelineFrame* frame,
                                   int nside, int nested, double pixfrac,
                                   const char* output_path,
                                   HpDrizzleResult* result);

// R05-B03: 自动 NSIDE 计算 (从 PipelineFrame 的 WCS/SIP 局部 Jacobian)
// 正式 Stage1 入口必须调用此函数计算自动 NSIDE, 禁止在 orchestrator 端用 CD 矩阵平均.
// frame: 输入帧 (需含 "data" 块 [H,W] float32 + "header" KV 块含 WCS/SIP 字段)
// out_nside: 输出 NSIDE (2 的幂, 范围 [16, 4194304])
// 返回: 0=成功, 非0=失败 (WCS 无效/图像尺寸非法等)
HP_DRIZZLE_API int hp_drizzle_compute_auto_nside(PipelineFrame* frame, int* out_nside);

#ifdef __cplusplus
}
#endif

#endif // HP_DRIZZLE_API_H
