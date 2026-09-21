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
    // ── FIX-405 G3-5 / DATA-002 §2a（rule_id NAN-SAMPLE-MASK-COVERAGE-NAN）──
    // 样本级掩膜强制计数（禁静默剔除）。**只增不改**：新字段一律追加在尾部，
    // 既有字段偏移不变（已编译的 DLL/ctypes 消费者不受影响）。
    // 语义: 不合格样本 = ¬isfinite(x_j) ∨ ¬isfinite(V_j) ∨ V_j ≤ 0；
    //       被剔除样本从 F_p（分子）、D_p（分母）、Var_p（方差）三项一并剔除
    //       并重新归一；仅 D_p=0 时输出 signal=NaN ∧ support≤0。
    // n_rejected_nonfinite = value + variance + nonpositive_weight（可加、互斥）。
    int64_t n_rejected_nonfinite;            // 合计（按原因分类见下三字段）
    int64_t n_rejected_nonfinite_value;      // 值非有限 (NaN/Inf)
    int64_t n_rejected_nonfinite_variance;   // 方差面非有限 (NaN/Inf)
    int64_t n_rejected_nonpositive_weight;   // 权重非有限或 ≤0
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
// precision_mode: 精度模式 (0=FP32, 1=FP64; -1=未指定: header KV "PRECISION" 优先, 无 KV 缺省 FP64/宪章 §5.3, 未知值显式拒绝)
// 返回: 0=成功, 非0=失败
HP_DRIZZLE_API int hp_drizzle_run(PipelineFrame* frame,
                                   int nside, int nested, double pixfrac,
                                   const char* output_path,
                                   HpDrizzleResult* result,
                                   int precision_mode);

// Phase1 Final Closure: Drizzle -> AIO HiPS 直写 (无 HISS 中转)
// hips_dir: HiPS 产品集根目录; legacy_hiss_path: 可选 legacy .hiss (validation 用)
HP_DRIZZLE_API int hp_drizzle_run_hips(PipelineFrame* frame,
                                       int nside, int nested, double pixfrac,
                                       const char* hips_dir,
                                       const char* legacy_hiss_path,
                                       HpDrizzleResult* result,
                                       int precision_mode);

// Phase1 生产末端正式 C ABI: Drizzle -> 标准 HiPS 直写 (无容器中转)。
// 产物与旧 writer 节点 (读中间容器后写 HiPS) 逐字节等价:
// signal+support 子产品恒写 (support 按 uint8 面积比量化); 输入帧含
// "variance" 块时按 DATA-P1-HIPS §12.1/§12.2 追加 variance/ivar 子产品,
// 不含时保持两产品面不变; 不写 snr, 不写 drizzle provenance;
// product_begin 元数据与旧 writer 一致。
// hips_dir: HiPS 产品集根目录 (signal/ support/ Moc.fits metadata.fits properties)
// filter_passband: obs_filter 透传 (可 NULL 或空串)
// 返回: 0=成功, 非 0=失败 (result->error_msg 给出原因)
HP_DRIZZLE_API int hp_drizzle_run_phase1_hips(PipelineFrame* frame,
                                              int nside, int nested, double pixfrac,
                                              const char* hips_dir,
                                              const char* filter_passband,
                                              HpDrizzleResult* result,
                                              int precision_mode);

// ============================================================================
// P17-NSIDE: 自动 NSIDE 决策 (采样率等价 drizzle 1x-2x) 的正式 C ABI。
//
// 语义 (负责人裁定): nside 缺省 (0/空/未给出) => 自动; 依据帧内 WCS/SIP 的
// 最细局部输入像素尺度 finest (自适应四叉树 + 3D 切向量 Jacobian), 取最小
// 2 次幂 NSIDE 使 HEALPix 特征尺度 hp_res = 211034.6/nside <= finest
// => 线性过采样倍率 finest/hp_res ∈ [1, 2); nside 钳位 [16, 2^22]。
//
// frame: 输入帧 (需含 "data" 块 [H,W] 与 "header" KV WCS/SIP; 与 hp_drizzle_run
//        同源解析, 保证决策输入 == drizzle 输入)
// result: 决策依据输出 (nside/finest/hp_res/oversample/clamped)
// 返回: 0=成功; 非 0=失败 (result->error_msg 给出原因)
// ============================================================================
typedef struct {
    int     nside;            // 推荐 NSIDE (2 的幂); 失败时 0
    double  finest_arcsec;    // 最细局部输入像素尺度 (角秒/像素)
    double  hp_res_arcsec;    // 所选 NSIDE 的 HEALPix 特征尺度 (角秒)
    double  oversample;       // hp_res/finest (引擎口径; 0.5~1 = 1~2 倍过采样)
    int     clamped;          // 1 = nside 被 [16, 2^22] 钳位 (决策不自由)
    char    error_msg[512];   // 失败原因
} HpAutoNsideResult;

HP_DRIZZLE_API int hp_drizzle_compute_auto_nside(PipelineFrame* frame,
                                                 HpAutoNsideResult* result);

// ============================================================================
// 反向 Drizzle (Sphere -> Plane, 球面面积语义) — 签字修正 REV-101 正式 C ABI
// ============================================================================

// 反向 Drizzle 输入 (扁平 WCS/SIP)
typedef struct {
    int32_t  nside;              // HEALPix NSIDE (2 的幂, 1..2^22)
    int32_t  nested;             // 必须 1 (NESTED)
    int32_t  target_width;       // 输出平面宽 (>0)
    int32_t  target_height;      // 输出平面高 (>0)
    double   pixfrac;            // source leaf 球面收缩 (0, 1]
    int32_t  output_fp64;        // 1=FP64 输出, 0=FP32 输出
    // WCS/SIP
    double   crval[2];           // 度
    double   crpix[2];           // 1-based
    double   cd[4];              // [cd1_1, cd1_2, cd2_1, cd2_2]
    int32_t  sip_order;          // 0..4
    int32_t  sip_ap_order;
    double   sip_a[36];
    double   sip_b[36];
    double   sip_ap[36];
    double   sip_bp[36];
    // source leaf 数据
    const uint64_t* leaf_ipix;       // NESTED ipix
    int64_t  n_leaf;
    const float*   leaf_signal_f32;  // 与 leaf_signal_f64 二选一 (非空即使用)
    const double*  leaf_signal_f64;
    const double*  leaf_support;     // [0,1], 可选 (NULL=全 1.0)
    int32_t  no_data_as_zero;        // 1=无覆盖输出 0, 0=NaN
} HpReverseDrizzleInput;

// 反向 Drizzle 结果/统计
typedef struct {
    int64_t n_source_leaf;
    int64_t n_target_pixel_touched;
    int64_t n_candidates;
    int64_t n_overlaps;
    double  total_signal_in;
    double  total_signal_out;
    double  total_covered_area_in;
    double  total_covered_area_out;
    int64_t n_invalid_ipix;
    int64_t n_nonfinite;
    int64_t n_skipped_outside;
    char    error_msg[512];
} HpReverseDrizzleResult;

// 执行反向 Drizzle。
// in: 输入 (严格校验, 非法返回非 0)
// signal_out / coverage_out: 输出缓冲区, 尺寸 width*height;
// output_fp64=1 时按 double 数组, 否则按 float 数组。
// result: 统计与错误信息。
// 返回 0=成功, 非 0=失败。
HP_DRIZZLE_API int hp_drizzle_reverse_run(
    const HpReverseDrizzleInput* in,
    void* signal_out,
    void* coverage_out,
    HpReverseDrizzleResult* result);

// 反向 Drizzle 能力/版本 (capability bit):
// 0x01 球面面积权重 | 0x02 FP32 数据面 | 0x04 FP64 数据面 |
// 0x08 support 语义 | 0x10 partial support (均匀假设) | 0x20 严格输入校验
HP_DRIZZLE_API uint32_t hp_drizzle_reverse_capability(void);
HP_DRIZZLE_API const char* hp_drizzle_reverse_version(void);

#ifdef __cplusplus
}
#endif

#endif // HP_DRIZZLE_API_H
