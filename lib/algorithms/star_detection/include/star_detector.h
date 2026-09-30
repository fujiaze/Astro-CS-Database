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

// ── 字段默认成员初始化（NSDMI）= SDetParams 的「类型层默认值」───────────────
// 取值依据（逐字段查证消费点得出, 非按「0 看着安全」推定）:
//   ① 有行为读点的字段 → 取 sdet_create(NULL) 的同一组默认（sdet_api.cpp:1119-1133）;
//   ② 语义为「<=0 ⇒ 用内置默认」的字段 → 取 0（本组的 fitRadius 与四个
//      psfFwhm*/maxPeakFraction/minQuarterMaxPixels）, 使 sdet_create(nullptr)
//      与「SDetParams x;」两条路径**行为**等价; 两条路径的**字面取值**在 ② 类
//      字段上不相同（NULL 分支直接写内置默认, NSDMI 分支写 0 哨兵）, 消费点
//      用 `>0 ? 取值 : 内置默认` 归一, 故不影响任何判定;
//   ③ 无任何行为读点的字段（只被赋值 / 只进日志）→ 取同一组默认以保持自文档, 不改判定。
// 作用: 消除「SDetParams x;」未初始化即使用这一未定义行为（读到栈垃圾 ⇒ 形状门用
// 随机阈值判星 ⇒ 误杀）。对既有「先 memset(0) 再逐字段赋」的调用方**零影响**:
// memset 在其后覆盖全部字段, 本组初值不参与任何判定。
typedef struct {
    int   structureLayers      = 5;     // ③ 无行为读点; 取 sdet_create 默认
    int   hotPixelFilterRadius = 1;     // ③ 同上
    float iterativeClipSigma   = 9.0f;  // ③ 同上
    int   iterativeMaxRounds   = 5;     // ③ 同上
    int   medianFilterDetail   = 1;     // ③ 同上
    int   maxStars             = 2000;  // ① 读点 sdet_emit_records: 仅 >0 时截断
                                          //   ⇒ 0 会静默退化为「不截断」, 故取 2000
    int   fitRadius            = 0;     // ② 读点 sdet_api.cpp 形状门窗半径:
                                          //   fitRadius>0 ? fitRadius : 6 ⇒ 0 即「自动」,
                                          //   与生产三处（fitRadius=0，注释「0 = 自动」）一致
                                          //   ⚠ 字段名是 fitRadius 但它**不**驱动 LM 拟合盒
                                          //   半径（那来自候选 R = ceil(3.7172*Sr)）;
                                          //   唯一消费点是 O13b 点源形状门的量测窗半径。
    float fwhmClipSigma        = 3.0f;  // ③ 仅日志读点; 取 sdet_create 默认
    float maxAxisRatio         = 2.0f;  // ① 读点 sdet_api.cpp:2092/2337: 仅 >0 时启门
                                          //   ⇒ 0 会静默关闭细长门, 故取 2.0
    // ── R-58-1 点源形状门（盲检测路径 O13b; 0 = 用内置默认, <0 = 显式关闭该子门）──
    // 依据: run/FINAL-07/审核包/端到端/五帧越闸定性报告.md §6.1（R-58 裁决 1）。
    // 语义（判据相对本帧点源参考, 不用全局常数）:
    //   ① 半高宽比例窗: f0 = 本帧过既有 O13 门的候选 fwhm 中位数;
    //      fwhm = 0.5*(fwhm_x+fwhm_y); 拒 fwhm < psfFwhmLoRatio*f0
    //      或 > psfFwhmHiRatio*f0（默认 0.5 / 2.5）
    //   ② 峰占比上限: pf = (峰值像素 - 拟合背景)/(拟合盒内正通量和);
    //      拒 pf > maxPeakFraction（默认 0.35; 真实 PSF 中位 0.08-0.14,
    //      单像素尖峰 0.5-0.99）
    //   ③ 最小像素数: n_quarter = #{pixel > B + 0.25*(peak-B)}（1/4 峰）;
    //      拒 n_quarter < minQuarterMaxPixels（默认 4）。
    //      实测依据: 本采样下“半高”判据对 sigma 0.8 px 的真实窄星只给 1
    //      （半高等高线半径 1.177*sigma = 0.94 px < 1 px，无相邻像素过线）,
    //      与单像素尖峰不可分 ⇒ 改用 1/4 峰（半径 1.665*sigma）:
    //      真实窄星 5, 生产帧实测尖峰族 1-2, 阈值 4 落在中间且两侧都有裕度。
    // 适用面: 只作用于全图盲检测 sdet_detect_ex[_f64]（WCS 解算的检测输入）;
    // 星表引导路径 sdet_detect_guided_ex_f64（权威测光路径）不经本门, 其
    // 检测定义域由星表位置给定, 语义不受影响。
    // ④ 四个子门默认取 0（= 用内置默认）, 与生产 memset(0) 后逐位一致;
    //    只有负值才关闭对应子门（0 与生产行为相同, 不改形状门默认开/关语义）。
    float psfFwhmLoRatio      = 0.0f;   // ② 读点 sdet_api.cpp:437/442: <0 关 / 0 内置默认 0.5
    float psfFwhmHiRatio      = 0.0f;   // ② 读点 sdet_api.cpp:438/444: <0 关 / 0 内置默认 2.5
    float maxPeakFraction     = 0.0f;   // ② 读点 sdet_api.cpp:439/446: <0 关 / 0 内置默认 0.35
    int   minQuarterMaxPixels = 0;      // ② 读点 sdet_api.cpp:440/448: <0 关 / 0 内置默认 4
} SDetParams;

typedef struct StarDetectorHandle_s *StarDetectorHandle;

SDET_EXPORT StarDetectorHandle sdet_create(const SDetParams *params);
SDET_EXPORT void sdet_destroy(StarDetectorHandle handle);





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

// ── 星表引导检测（权威路径，docs/ASTROCS_DESIGN.md §4.2 / §2.1）──────────────────
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
//   n_dropped   = 丢弃的位置数（非有限 / 距边界 <2px / 拟合盒越界；
//                 语义映射与 sdet_api.cpp sdet_detect_guided_impl 注释 ① 一致）
//   n_fit_failed= 拟合未收敛或参数非法、或 O4b 拟合前置判据失败（3x3 邻域
//                 高像素计数 <3，sdet_detect_guided_impl 注释 ②）的位置数
//   n_rejected  = 拟合前 O10 对称门拒绝（非饱和），或拟合成功但未过质量门
//                 （maxAxisRatio / reject_star）的位置数
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
