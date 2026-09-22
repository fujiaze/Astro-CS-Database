#ifndef STAR_DETECTOR_H
#define STAR_DETECTOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define SDET_EXPORT __declspec(dllexport)
#else
#define SDET_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int structureLayers;
    int hotPixelFilterRadius;
    float iterativeClipSigma;
    int iterativeMaxRounds;
    int medianFilterDetail;
    int maxStars;
    int fitRadius;
    float fwhmClipSigma;
    float maxAxisRatio;
} SDetParams;

typedef struct StarDetectorHandle_s *StarDetectorHandle;

SDET_EXPORT StarDetectorHandle sdet_create(const SDetParams *params);
SDET_EXPORT void sdet_destroy(StarDetectorHandle handle);

SDET_EXPORT int sdet_detect(StarDetectorHandle handle,
                             const uint16_t *image, int width, int height,
                             double **out_x, double **out_y, int *out_count);

SDET_EXPORT void sdet_free_coords(double *coords);

SDET_EXPORT int sdet_detect_debug(StarDetectorHandle handle,
                                   const uint16_t *image, int width, int height,
                                   double **out_x, double **out_y, int *out_count,
                                   float **out_mag, int **out_has_saturated,
                                   float **out_detail, float **out_smap, float **out_binary,
                                   const char **extra_names, int extra_count, float ***out_extras);

SDET_EXPORT void sdet_free_debug_maps(float *maps);

SDET_EXPORT int sdet_detect_ex(StarDetectorHandle handle,
                                const uint16_t *image, int width, int height,
                                double **out_x, double **out_y, float **out_flux, int **out_saturated,
                                float **out_mag, int **out_has_saturated,
                                int *out_count,
                                const char **extra_names, int extra_count, float ***out_extras);

// FP64 星点检测 (double 图像, 全程 double 不降级 float32)
// 输出布局与 sdet_detect_ex 完全一致; 返回 0=成功, -1=参数错误
SDET_EXPORT int sdet_detect_ex_f64(StarDetectorHandle handle,
                                    const double *image, int width, int height,
                                    double **out_x, double **out_y, float **out_flux, int **out_saturated,
                                    float **out_mag, int **out_has_saturated,
                                    int *out_count,
                                    const char **extra_names, int extra_count, float ***out_extras);

SDET_EXPORT void sdet_free_detect_ex(double *x, double *y, float *flux, int *saturated,
                                       float *mag, int *has_saturated,
                                       float **extras, int extra_count);

// ── 星表引导检测（权威路径，ASTROCS_DESIGN.md §4.2 / §2.1）──────────────────
// 检测定义域 = 星表逆投影到像素域的预测位置 pred_x/pred_y[0..n_pred)：
// 只在这些位置做质心/椭圆高斯 PSF 拟合；拟合或质量门失败的位置**直接丢弃**
// （不计虚警、不报错）。全图盲检测（sdet_detect_ex[_f64]）保留为**诊断/初值**
// 路径，不是权威路径。
// 预测位置 (pred_x, pred_y) 为 0-based 像素坐标（像素中心 = 索引 + 0.5），
// 由调用方用本帧近似 WCS 把星表逆投影得到；本入口不做任何星表/投影运算。
// 输出十数组布局与 sdet_detect_ex_f64 完全一致，同样经 sdet_free_detect_ex 释放。
// 返回 0=成功（含 0 星：输出指针全 NULL + *out_count=0，不是错误）；
//      -1=参数无效/内存分配失败。
// out_stats（可 NULL）逐项报告定义域计数，用于"不计虚警"语义的可观测性：
//   n_predicted = 输入预测位置数
//   n_dropped   = 丢弃的位置数（非有限 / 距边界 <2px / 拟合盒越界）
//   n_fit_failed= 拟合未收敛或参数非法的位置数
//   n_rejected  = 拟合成功但未过质量门（maxAxisRatio / reject_star）的位置数
//   n_fit_ok    = 拟合成功且过质量门的星点数（去重与 maxStars 截断前）
//   n_output    = 最终输出星点数（去重 + maxStars 截断后）
typedef struct {
    int n_predicted;
    int n_dropped;
    int n_fit_failed;
    int n_rejected;
    int n_fit_ok;
    int n_output;
} SDetGuidedStats;

SDET_EXPORT int sdet_detect_guided_ex_f64(StarDetectorHandle handle,
                                           const double *image, int width, int height,
                                           const double *pred_x, const double *pred_y,
                                           int n_pred,
                                           double **out_x, double **out_y, float **out_flux,
                                           int **out_saturated, float **out_mag,
                                           int **out_has_saturated, int *out_count,
                                           const char **extra_names, int extra_count,
                                           float ***out_extras,
                                           SDetGuidedStats *out_stats);

#ifdef __cplusplus
}
#endif

#endif
