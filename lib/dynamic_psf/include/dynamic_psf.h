#ifndef DYNAMIC_PSF_H
#define DYNAMIC_PSF_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#define DPSF_EXPORT __declspec(dllexport)
#else
#define DPSF_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int status;
    double B;
    double A;
    double cx;
    double cy;
    double sx;
    double sy;
    double theta;
    double fwhm_x;
    double fwhm_y;
    double mad;
    double flux;
    double eccentricity;
} DPSFFitResult;

#define DPSF_FIT_OK              0
#define DPSF_FIT_NO_CONVERGENCE  1
#define DPSF_FIT_INVALID_PARAMS  2
#define DPSF_FIT_ITERATION_LIMIT 3

typedef struct {
    int fitRadius;
    int maxIter;
    double tolerance;
} DPSFFitParams;

DPSF_EXPORT int dpsf_fit(const uint16_t *image, int width, int height,
                          double cx, double cy,
                          const DPSFFitParams *params,
                          DPSFFitResult *result);

DPSF_EXPORT int dpsf_fit_batch(const uint16_t *image, int width, int height,
                                const double *cx_array, const double *cy_array, int count,
                                const DPSFFitParams *params,
                                DPSFFitResult **out_results);

DPSF_EXPORT void dpsf_free_results(DPSFFitResult *results);

// ============================================================================
// float32 PSF 拟合 API (v1.1 新增, P02-005)
//
// 直接接收 float32 图像, 内部拟合使用 float/double, 不做 0-65535 clip,
// 不创建整张 uint16 图像。消费 star_det v1 (FLOAT64 [N,6]) 格式的检测结果,
// 不调用 sdet_detect_ex。
//
// 输入:
//   image         - float32 图像数据 [height*width], 行主序 (row-major)
//   width         - 图像宽度 (像素)
//   height        - 图像高度 (像素)
//   detections    - star_det v1 检测结果, FLOAT64 [N,6]
//                   列定义: [0]=x_px, [1]=y_px, [2]=flux, [3]=mag,
//                           [4]=saturated(0/1), [5]=has_saturated(0/1)
//   n_detections  - 检测到的星点数 N
//   params        - 拟合参数 (fitRadius/maxIter/tolerance), 可为 NULL 使用默认值
//
// 输出:
//   out_psf_params - PSF 参数缓冲区, 调用者分配, 大小 = N * 9 * sizeof(double)
//                    每行 9 个字段 (FLOAT64):
//                      [0]=B        (背景)
//                      [1]=A        (振幅)
//                      [2]=cx       (中心 x, 图像坐标系)
//                      [3]=cy       (中心 y, 图像坐标系)
//                      [4]=sx       (sigma x)
//                      [5]=sy       (sigma y)
//                      [6]=theta    (旋转角, 弧度)
//                      [7]=fwhm_x   (X 方向 FWHM, 像素)
//                      [8]=fwhm_y   (Y 方向 FWHM, 像素)
//                    失败的拟合所有字段置为 NaN。
//   out_n_valid    - 成功拟合的星点数 (status == DPSF_FIT_OK)
//
// 返回值:
//   0  - 成功完成批量拟合 (不要求每颗星都成功, 看 out_n_valid)
//   -1 - 参数错误 (空指针/尺寸非法)
//
// 旧 uint16 API (dpsf_fit_batch) 保留兼容, 不删除不修改。
// ============================================================================
#define DPSF_STAR_DET_SCHEMA_V1 "star_det_v1:FLOAT64[N,6]"
#define DPSF_PSF_PARAMS_SCHEMA  "psf_params:FLOAT64[N,9]"

DPSF_EXPORT int dpsf_fit_batch_f32(
    const float *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid
);

// ============================================================================
// double PSF 拟合 API (双精度 ABI, R10 新增)
//
// 双精度 ABI 改造: FP64 模式下全链路使用 double, 不降级到 float32。
// 与 dpsf_fit_batch_f32 逻辑一致, 仅 image 数据类型从 float 改为 double。
// 内部 moffat4_fit_d 直接在 double 上采样像素, 拟合使用 double (不降级)。
//
// 输入:
//   image         - float64 图像数据 [height*width], 行主序 (row-major)
//   width         - 图像宽度 (像素)
//   height        - 图像高度 (像素)
//   detections    - star_det v1 检测结果, FLOAT64 [N,6]
//   n_detections  - 检测到的星点数 N
//   params        - 拟合参数 (fitRadius/maxIter/tolerance), 可为 NULL 使用默认值
//
// 输出:
//   out_psf_params - PSF 参数缓冲区, 调用者分配, 大小 = N * 9 * sizeof(double)
//                    失败的拟合所有字段置为 NaN。
//   out_n_valid    - 成功拟合的星点数 (status == DPSF_FIT_OK)
//
// 返回值:
//   0  - 成功完成批量拟合 (不要求每颗星都成功, 看 out_n_valid)
//   -1 - 参数错误 (空指针/尺寸非法)
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_f64(
    const double *image,
    int width,
    int height,
    const double *detections,
    int n_detections,
    const DPSFFitParams *params,
    double *out_psf_params,
    int *out_n_valid
);

// ============================================================================
// double PSF 批量拟合 API (返回完整 DPSFFitResult 结构体, 双精度 ABI R10 新增)
//
// 与 dpsf_fit_batch (uint16) 接口形态一致: 接收独立 cx/cy 数组, 返回
// DPSFFitResult* (含全部字段: status/B/A/cx/cy/sx/sy/theta/fwhm_x/fwhm_y/
// mad/flux/eccentricity), 调用者用 dpsf_free_results 释放。
//
// 用途: orchestrator FP64 路径。FP64 模式下 data 块为 FLOAT64, 直接传入 double
// 图像 (不降级到 uint16/float32), 内部 moffat4_fit_d 在 double 上裁剪 patch
// 并拟合。返回完整结构体使 orchestrator 能写出与 FP32 路径完全一致的 psf 块
// 布局 (status,B,flux,cx,cy,fwhm,A,mad,eccentricity), 不破坏下游 SNR/PHOTOMETRIC。
//
// 输入:
//   image    - float64 图像数据 [height*width], 行主序 (row-major)
//   width    - 图像宽度 (像素)
//   height   - 图像高度 (像素)
//   cx_array - 各星点中心 x 坐标 (图像坐标系)
//   cy_array - 各星点中心 y 坐标 (图像坐标系)
//   count    - 星点数 N
//   params   - 拟合参数 (fitRadius/maxIter/tolerance), 可为 NULL 使用默认值
//
// 输出:
//   out_results - DPSFFitResult 数组 (调用者用 dpsf_free_results 释放)
//
// 返回值:
//   0  - 成功完成批量拟合 (不要求每颗星都成功, 看 results[i].status)
//   -1 - 参数错误 (空指针/尺寸非法)
// ============================================================================
DPSF_EXPORT int dpsf_fit_batch_d(
    const double *image,
    int width,
    int height,
    const double *cx_array,
    const double *cy_array,
    int count,
    const DPSFFitParams *params,
    DPSFFitResult **out_results
);

#ifdef __cplusplus
}
#endif

#endif
