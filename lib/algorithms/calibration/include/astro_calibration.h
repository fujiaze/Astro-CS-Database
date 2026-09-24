#ifndef ASTRO_CALIBRATION_H
#define ASTRO_CALIBRATION_H

#ifdef __cplusplus
extern "C" {
#endif

/* AC_API 可被构建方预定义覆盖: astrocs_p1_calibration DLL target (P1-CAL-IMPL)
 * 以 -DAC_API= 将 12 个 legacy 符号本地化, 导出面仅 astrocs_module_query_v1
 * (ABI-006; 对齐 GAIA_EXPORT= 先例)。未定义时保持原语义, 既有构建零变化。 */
#ifndef AC_API
#ifdef _WIN32
#define AC_API __declspec(dllexport)
#else
#define AC_API __attribute__((visibility("default")))
#endif
#endif

/* ========== 常量 ========== */
#define AC_COMBINE_MEAN   0
#define AC_COMBINE_MEDIAN 1

#define AC_METHOD_MEDIAN  0
#define AC_METHOD_BILINEAR 1

#define AC_OK             0
#define AC_ERR_PARAM     -1
#define AC_ERR_MEMORY    -2
#define AC_ERR_INTERNAL  -3

/* 坏列（linear defect）路径状态位（按位或；0 = 无降级）。
 * 与 ac_correct_frame 的坏点路径完全独立：不改动其判据、阈值与语义。 */
#define AC_COLSTAT_OK                 0
#define AC_COLSTAT_SCALE_DEGENERATE   1   /* MAD(dev)==0 => 检测不可用，不判任何列 */
#define AC_COLSTAT_FRAME_TOO_SMALL    2   /* 列数不足 => 无列可判 */
#define AC_COLSTAT_EDGE_ONE_SIDED     4   /* 坏列段贴边，仅单侧锚点 => 复制该锚列 */
#define AC_COLSTAT_NO_ANCHOR          8   /* 坏列段无任何好列锚点 => 不修复，保留原值 */
#define AC_COLSTAT_EDGE_EXCLUDED      16  /* 适用域声明：首尾 k 列无双侧邻域，未参与判定
                                           * （单侧基准对横向梯度系统性有偏 => 必然假阳性；
                                           *  该位恒置，非故障。实际排除列数见 manifest 的
                                           *  bad_column_edge_columns_excluded） */
#define AC_COLSTAT_WIDE_DEFECT        32  /* 检出宽度 > max_seg_len 的缺陷段：**未修复**，
                                           * 仅在 col_mask 里以值 2 标记（负责人 2026-09-24
                                           * 裁决：只修单列；相邻多列不属本任务）。
                                           * 该位只声明"存在被跳过的宽段"，不静默。 */

/* col_mask 的取值语义（不是布尔！） */
#define AC_COLSTAT_MASK_CLEAN  0   /* 未检出 */
#define AC_COLSTAT_MASK_REPAIRED 1 /* 单列缺陷，已按隔壁列插值修复 */
#define AC_COLSTAT_MASK_WIDE   2   /* 宽缺陷段（> max_seg_len），仅标记、未修复 */

/* ========== 主帧生成 ========== */

/* 生成 Master Bias：sigma-clip + median/mean 合并
 * stack: [n_frames * height * width] float32 行优先
 * out: [height * width] float32
 * combine: AC_COMBINE_MEAN 或 AC_COMBINE_MEDIAN
 */
AC_API int ac_generate_master_bias(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Dark：sigma-clip + median/mean 合并（不减Bias，Dark已含Bias）
 * 同 ac_generate_master_bias
 */
AC_API int ac_generate_master_dark(
    const float* stack, int n_frames, int width, int height,
    float* out,
    float sigma_low, float sigma_high, int max_iterations,
    int combine);

/* 生成 Master Flat：减Bias + 逐帧归一化 + sigma-clip + mean + 再归一化
 * flat_stack: [n_frames * height * width]
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 */
AC_API int ac_generate_master_flat(
    const float* flat_stack, int n_frames, int width, int height,
    const float* master_bias,
    float* out,
    float sigma_low, float sigma_high, int max_iterations);

/* ========== 图像校准 ========== */

/* 校准单帧 Light
 * 无暗场优化: (Light - Dark) / Flat
 * 有暗场优化: (Light - Bias - K*(Dark - Bias)) / Flat
 *
 * light: [height * width]
 * master_dark: [height * width] 或 NULL
 * master_flat: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * dark_optimization: 0=关闭, 1=开启
 * dark_scale_factor: K初始值（如 Light曝光/Dark曝光）
 * actual_k: 输出实际使用的K值（可选，NULL则不输出）
 */
AC_API int ac_calibrate_frame(
    const float* light, int width, int height,
    const float* master_dark, const float* master_flat, const float* master_bias,
    float* out,
    int dark_optimization, float dark_scale_factor,
    float* actual_k);

/* ========== 坏点修复 ========== */

/* 校正单帧图像的坏点
 * 检测方法：Dark全局统计检测热像素 + Bias全局统计检测冷像素
 *
 * data: [height * width] 校准后Light
 * master_dark: [height * width] 或 NULL
 * master_bias: [height * width] 或 NULL
 * out: [height * width]
 * hot_sigma: Dark热像素检测sigma倍数
 * cold_sigma: Bias冷像素检测sigma倍数
 * method: AC_METHOD_MEDIAN 或 AC_METHOD_BILINEAR
 * max_structure_size: 连通区域大小阈值（>=此值视为星点）
 * out_hot: 输出热像素数（可选）
 * out_cold: 输出冷像素数（可选）
 */
AC_API int ac_correct_frame(
    const float* data, int width, int height,
    const float* master_dark, const float* master_bias,
    float* out,
    float hot_sigma, float cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold);

/* ========== 坏列（linear defect, 单列）检测与修复 ==========
 *
 * 【与 ac_correct_frame 的关系】完全独立的新路径。ac_correct_frame 的检测
 * 判据、阈值、插值语义与产物（out/out_hot/out_cold）在本合同下**零改动**；
 * 本函数不调用它，它也不调用本函数。两者可同时施加（先坏列、后坏点），
 * 互不覆盖：坏点按像素判定，坏列按列判定。
 *
 * data: 检测与修复源帧 [height*width] float32（生产 = cal 节点产物）
 * out:  修复帧 [height*width] float32（调用方分配；非坏列逐像素恒等）
 * column_sigma: 判据阈值（帧内 MAD 倍数，无量纲）；<=0 = 显式禁用本路径
 * neighbor_k:   横向邻域半径（每侧列数）；<=0 归一到 3
 * col_mask:     输出 [width] unsigned char，1 = 该列被判为坏列（可 NULL）
 * out_n_cols:   输出检出坏列数（可 NULL）
 * out_px_repaired: 输出被插值覆盖的像素数 = Σ(坏列段长度)*height（可 NULL）
 * out_sigma_col:输出帧内稳健尺度 1.482602218505602*MAD(dev) [ADU]（可 NULL，供审计）
 * out_status:   输出状态位（AC_COLSTAT_* 的按位或；可 NULL）
 *
 * 检测（列统计量的跳变配对分段）：colstat[x]=median_y data；
 *   d[x]=colstat[x]-colstat[x-1]；sigma_col=1.482602218505602*MAD(d)；
 *   显著跳变 J={x: |d[x]-median(d)| >= column_sigma*sigma_col}；
 *   边界 {0}∪J∪{width} 把列轴切成电平段；段长 <= 2*neighbor_k-1 且非全宽
 *   ⇒ 判为坏列段。判据建在一阶差分上，对缓变结构、段内互相印证、坏列污染
 *   邻居基准三种失败模式同时免疫；且与修复算子（段外锚点线性插值）同构。
 *   sigma_col==0 时显式降级（AC_COLSTAT_SCALE_DEGENERATE），不判任何列。
 * 修复：隔壁列插值。单列缺陷 = 左右两邻算术平均；连续坏列段 = 段两端最近
 *   好列之间的线性插值。插值一律取自输入 data（不级联）=> 闭式解析预期。
 *   段贴边（仅单侧锚点）=> 复制该锚列并置位；无锚点 => 保留原值并置位。
 * 返回：AC_OK；data/out 空指针或 width/height 非正 => AC_ERR_PARAM（不写 out）。
 */
AC_API int ac_correct_columns(
    const float* data, int width, int height,
    float* out,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, int* out_px_repaired,
    float* out_sigma_col, int* out_status);

/* ========== 坏列定位：dark / bias 母版路径 + 三路径仲裁 ==========
 *
 * 【为什么要有母版路径】整列缺陷的**物理来源**是探测器本身，而 dark/bias 母版
 * 是这一缺陷最直接的观测面：母版上没有天体结构，坏列表现为该列电平的系统性
 * 偏离，判据比科学帧自身统计干净得多（成熟实现同向：Siril 的缺陷图从 dark
 * 生成；IRAF ccdmask 的列向中值窗默认 7；见 run/LINDEF-IMPL-01/LIT-methods.md）。
 *
 * 【标度无关性】三条路径各自**帧内自校准**（判据尺度取本帧 MAD），且判据只
 * 依赖列统计量的**横向相对**关系 ⇒ 对检测源的正标度变换严格不变（与
 * detect_hot_pixels 的同类不变性同性质，ALG-COS-001 §1）。因此母版与科学帧
 * 标度不一致**不影响**坏列检出集合，无需标度声明即可安全并用。
 *
 * 判据与 ac_correct_columns 完全同构（跳变配对分段），阈值语义同：
 *   column_sigma <= 0 显式禁用；neighbor_k <= 0 归一到 3；段长上限 2k-1。
 */
AC_API int ac_detect_bad_columns_from_master(
    const float* master, int width, int height,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, float* out_sigma_col, int* out_status);

/* 三路径仲裁来源位（out_source_mask[x] 的按位或） */
#define AC_COLSTAT_SRC_SCIENCE 1u   /* 该列在科学帧自身被判坏列 */
#define AC_COLSTAT_SRC_DARK    2u   /* 该列在 master_dark 被判坏列 */
#define AC_COLSTAT_SRC_BIAS    4u   /* 该列在 master_bias 被判坏列 */
/* 列级置信度（由来源组合导出，供下游筛选；不改判据本身） */
#define AC_COLSTAT_CONF_HIGH   3u   /* dark 与 bias 一致判坏（物理来源双重印证） */
#define AC_COLSTAT_CONF_MEDIUM 2u   /* 仅一个物理来源判坏，或科学帧+任一母版 */
#define AC_COLSTAT_CONF_LOW    1u   /* 仅科学帧自身判坏（生产上母版不可得时的主路径） */

/* 三路径仲裁 + 修复（坏列的完整入口）。
 * science 必需；master_dark / master_bias 可为 NULL（该路径不参与）。
 * 仲裁规则 = **并集**并逐列记录来源（不用交集：生产两条路径都不接线母版，
 * 科学帧路径必须能独立工作；交集会漏掉只在一处可见的缺陷）。每列的来源与
 * 置信度写入 out_source_mask / out_conf_mask，供下游筛选与审计——不静默丢弃
 * 任何一路的证据。若某列仅由母版判出而科学帧未判，仍修复（该缺陷经校准会
 * 进入科学帧，只是单帧统计未必显形），置信度标记为 MEDIUM。
 */
AC_API int ac_correct_columns_ex(
    const float* data, const float* master_dark, const float* master_bias,
    int width, int height, float* out,
    float column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    unsigned char* out_source_mask,
    unsigned char* out_conf_mask,
    int* out_n_cols, int* out_n_science, int* out_n_dark, int* out_n_bias,
    int* out_px_repaired, float* out_sigma_col, int* out_status);

/* ========== 双精度 ABI (FP64) ==========
 *
 * 双精度 ABI 改造 (R10): FP64 模式下全链路使用 double, 不降级到 float32。
 *   - ac_calibrate_frame_f64: 像素级算术 (light-bias-K*(dark-bias))/flat 在 double 上运行,
 *     不降级 (精度关键路径)。
 *   - ac_generate_master_bias_f64 / dark_f64 / flat_f64 / ac_correct_frame_f64:
 *     统计/mask 操作, 内部将 double 输入转 float 调用 f32 实现, 输出转回 double。
 *     (这些函数用于 master 帧预生成与坏点修复, orchestrator 的 run_stage_calibrate
 *      不调用它们, 因此不影响 FP64 全链路精度。)
 * 向后兼容: 原有 float32 API 保留不变。
 */

AC_API int ac_generate_master_bias_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine);

AC_API int ac_generate_master_dark_f64(
    const double* stack, int n_frames, int width, int height,
    double* out,
    double sigma_low, double sigma_high, int max_iterations,
    int combine);

AC_API int ac_generate_master_flat_f64(
    const double* flat_stack, int n_frames, int width, int height,
    const double* master_bias,
    double* out,
    double sigma_low, double sigma_high, int max_iterations);

AC_API int ac_calibrate_frame_f64(
    const double* light, int width, int height,
    const double* master_dark, const double* master_flat, const double* master_bias,
    double* out,
    int dark_optimization, double dark_scale_factor,
    double* actual_k);

AC_API int ac_correct_frame_f64(
    const double* data, int width, int height,
    const double* master_dark, const double* master_bias,
    double* out,
    double hot_sigma, double cold_sigma,
    int method, int max_structure_size,
    int* out_hot, int* out_cold);

/* ac_correct_columns 的 double 变体：与 ac_correct_frame_f64 同款降级语义
 * （double 输入转 float32 执行、输出回转 double；统计/mask/插值全程 f32）。 */
AC_API int ac_correct_columns_f64(
    const double* data, int width, int height,
    double* out,
    double column_sigma, int neighbor_k, int max_seg_len,
    unsigned char* col_mask,
    int* out_n_cols, int* out_px_repaired,
    double* out_sigma_col, int* out_status);

/* ========== 工具 ========== */

/* 设置OpenMP线程数 */
AC_API void ac_set_num_threads(int n);

/* 获取版本号 */
AC_API const char* ac_version();

#ifdef __cplusplus
}
#endif

/* ========== C++ 接口（最优 Dark 系数估计）========== */
/* 纯 C++ 接口，使用 hiss::Stage1Diagnostics 结构化诊断，仅供 C++ 调用方使用。
 * 对应实现见 src/dark_optimizer.cpp。
 */
#ifdef __cplusplus
#include "hiss_format.h"  // hiss::Stage1Diagnostics (02_FROZEN §2.3)
namespace ac {
/* 最优 Dark 系数估计
 * 模型: L - B = c + k*(D - B)
 * 算法: 背景提取(sigma-clip) + 8x8 分区抽样 + 鲁棒线性回归(MAD 离群抑制, 5 轮迭代)
 * 失败处理: 输出结构化诊断后自动回退曝光时间比例 k_init，
 *           设置 diagnostics.fell_back=1, fallback_from="OPTIMAL", fallback_to="EXPOSURE_RATIO"
 * 返回: 最优 k 值；失败时返回 k_init
 */
float optimize_dark_k(const float* light, const float* bias, const float* dark,
                      const float* flat, int w, int h, float k_init,
                      hiss::Stage1Diagnostics& diagnostics);
}
#endif

#endif /* ASTRO_CALIBRATION_H */
