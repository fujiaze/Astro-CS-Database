// lib/algorithms/coverage/include/astro/phase2/sampler.h
//
// Phase2 W4：稀疏光度控制点采样器（control sampler）。
//
// 语义（冻结， 34A532A2...B2EB308 + wiki Phase2_Unified_Photometric_Model）：
// - 控制点 geometry 由 union 几何与目标角间距决定，**不由 SNR 决定**；
// - 每个覆盖控制节点的 frame 提供观测 y_ik/sigma_ik/snr_ik/support_ik/quality_ik，
// y_ik 必须从实际 Phase1 HiPS 数据读取；
// - patch estimator：control cell 附近小型 HEALPix patch，finite/support 过滤，
// robust median/biweight location，MAD/robust scale，保留负值；
// - SNR 只作为当前 frame 在当前 control 观测的可信度，来自 Phase1 SNR Catalogue
// （禁止重新检测星点）。
#pragma once

#include "astro/phase2/coverage.h"
#include "astro/phase2/upm.h"

#include <cstdint>
#include <cstddef>

#ifdef _WIN32
#define P2_API __declspec(dllexport)
#else
#define P2_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// 采样配置（默认值在 sampler.cpp 中定义）
typedef struct {
    int control_grid_per_tile;    // 每个 union tile 内的 control cell 网格数（默认 8）
    int patch_radius_leaf;        // 观测 patch 半径（叶级 leaf 数，默认 2 → 5×5）
    int min_samples;              // 有效样本最小数（默认 5）
    double snr_search_radius_deg; // SNR 星点检索半径（度，默认 0.05）
    // background-clean 采样（DBE-like；BACKGROUND_SAMPLER_SPEC.md）
    int    background_patch_radius;        // 背景 patch 半径（默认 8 → 17×17）
    double background_clip_sigma;          // 亮端迭代 clipping 阈值（MAD 单位，默认 3.0）
    int    background_clip_iters;          // 亮端 clipping 迭代次数（默认 3）
    double background_max_contamination;   // 亮像素占比上限（默认 0.20）
    double background_contamination_sigma; // 污染判定 sigma（默认 3.0）
    double background_min_retained_fraction; // clipping 后保留比例下限（默认 0.60）
    double background_tolerance;           // 局部 tolerance gate（MAD 单位，默认 3.0）
    int    background_neighbor_radius;     // 局部 baseline 邻域 cell 半径（默认 2）
    int    background_catalog_veto;        // 允许 SNR catalogue veto（默认 1）
    // control estimator 为
    // background-clean patch median；
    // control_variance = k_corr × (π/2) × sigma_bg² / N_retained；
    // k_corr 由当前 Drizzle synthetic noise/covariance MC 校准
    // （UPMW-005 control_median_mc_test，pixfrac=0.8 实证 1.3883），
    // 冻结保守值 1.4（>= 实证，余量 <1%）。<=0 时回退冻结默认。
    double control_k_corr;                // 默认 1.4
    // CON-004 并行采样 worker 数（0=auto：omp_get_max_threads/hardware_concurrency；1=串行默认）。
    // 仅 P2_ENABLE_OPENMP 且 >1 时启用并行第一遍；否则恒串行（默认行为不变）。
    int cpu_workers;                      // 来自 Runtime lease(p2_session 传 budget.max_workers); 1=串行 reference
} P2SamplerConfig;

// sampler 默认配置单一来源（null cfg 时使用；显式 cfg 覆盖）。
P2_API P2SamplerConfig p2_sampler_default_config(void);

// background-clean 采样统计（accepted/rejected 可追踪）
typedef struct {
    std::uint64_t candidate_observations;   // 候选观测总数（几何×覆盖帧）
    std::uint64_t accepted_observations;    // 进入 UPM 的 clean 观测
    std::uint64_t rejected_insufficient_support;   // support/finite 不足
    std::uint64_t rejected_insufficient_retained;  // clipping 后保留比例过低
    std::uint64_t rejected_bright_tolerance;       // 超过局部 tolerance
    std::uint64_t rejected_high_contamination;     // 亮像素占比过高
    std::uint64_t rejected_catalog_veto;           // 星表 veto
    std::uint64_t rejected_lt_two_clean_frames;    // clean 帧数 <2 未入拟合
    std::uint64_t accepted_controls;               // 有 ≥1 clean obs 的 control
    std::uint64_t overlap_controls;                // 有 ≥2 clean obs 的 control
} P2SampleStats;

// control 几何节点（全 coverage 网格；与观测解耦）
typedef struct P2ControlNode {
    std::uint64_t control_id;
    std::uint64_t tile_ipix;
    int  gx, gy;
    double ra_deg, dec_deg;
    std::uint64_t leaf_ipix;   // cell 中心叶级像素
} P2ControlNode;

// 内容稳定帧标识（DATA-FRAME-ID-001， 冻结）：
// frame_id = truncated-64(canonical SHA-256 of science payload identity)
// - 输入：关键 properties 白名单 + signal tile 像素 + support tile 像素 +
// SNR catalogue 内容（实现见 sampler.cpp p2_frame_id）；
// - 路径/重命名/换根目录不变；任何科学 payload 变化 → 改变；
// - 取 SHA-256 前 16 hex 字符（大端序截断）为 uint64；
// - 与输入顺序无关；UPM 参考帧 = 每分量最小 frame_id。
// 禁止描述为 FNV-1a / 路径派生（旧文档已修正）。
P2_API std::uint64_t p2_frame_id(const char* hips_path);

// 统一统计量（sampler patch estimator / MAD / SNR 邻域共用同一实现）。
// median：偶数 n 取上下中位数平均；NaN 自动过滤（全部 NaN → 0）。
P2_API double p2_stats_median(const double* vals, std::uint64_t n);
P2_API double p2_stats_mad(const double* vals, std::uint64_t n,
                           double* out_median = nullptr);

// 采样控制观测。调用方先以 out_n_obs=0 查询所需数量，再分配后二次调用；
// 或直接传入足够大的 out_capacity。
P2_API int p2_sample_controls(
    const P2CoverageResult* coverage,
    const char* const* hips_paths,
    const P2SamplerConfig* cfg,
    P2ControlObservation* out_obs,   // 可空（查询容量）
    std::uint64_t out_capacity,
    std::uint64_t* out_n_obs,        // 实际观测数
    std::uint64_t* out_n_controls,   // 控制节点数
    P2SampleStats* out_stats,        // 可空（ 统计）
    P2ControlNode* out_controls,     // 可空（ 全几何节点）
    std::uint64_t ctrl_capacity,
    char* err, std::size_t err_size);

// 含 frame_id 缓存的重载（性能：stage2 已算 frame_id 时透传，避免二次 500MB payload 哈希）。
// @param frame_ids 长度 n_inputs，与 hips_paths 同序；0 视为非法（p2_frame_id 失败哨兵），实现将直接拒绝。
// @note out_n_controls = n_geometry_controls (= n_union * grid*grid，含空覆盖占位)，
// 与 out_stats.accepted_controls / overlap_controls (≥1/≥2 clean) 区分；日志应并列表述。
P2_API int p2_sample_controls_cached(
    const P2CoverageResult* coverage,
    const char* const* hips_paths,
    const std::uint64_t* frame_ids,  // n_inputs 长度，可空则内部计算；0 非法
    const P2SamplerConfig* cfg,
    P2ControlObservation* out_obs,
    std::uint64_t out_capacity,
    std::uint64_t* out_n_obs,
    std::uint64_t* out_n_controls,
    P2SampleStats* out_stats,
    P2ControlNode* out_controls,
    std::uint64_t ctrl_capacity,
    char* err, std::size_t err_size);

// ===========================================================================
// V6 目标态：空间模型求值、空间摘要与帧级标量降级门
// ---------------------------------------------------------------------------
// 冻结锚：
//   DESIGN-P2 §7 / UNIFIED §8 : PSF/背景/variance/photometric response/
//       point information 默认是空间量，Phase2 必须在输出位置求值；压成
//       帧级标量必须同时过 (a) 空间残差/趋势门 与 (b) 功率损失门，并输出
//       p05/p50/p95 + 最大系统偏差 + 采样覆盖 + 模型误差 + 适用域；否则
//       保留 map/model/control points；
//   FZ-DEGRADE-SCALAR        : 缺分位数/未过门即 REJECT；
//   FZ-GATE-SUPPORT-COVERAGE : support/coverage 只作门，不得冒充权重；
//   FZ-COND-WHITENOISE       : 白噪声近似为条件式（本求值器不产生权重）；
//   SO-07（PENDING_OWNER_SIGNOFF）: 残差/趋势与功率损失阈值数值待签，
//       本实现不自行定值；调用方未显式声明阈值即 fail-closed。
// ---------------------------------------------------------------------------
// 单位（冻结表，不在此重定义）：signal_sb=ADU/px^2、sb_variance_out=
// ADU^2/px^4、W_info=ADU^-2、Q=ADU^-1、flux=ADU、psfsw_robust_weight=1。
// 求值器按调用方声明的量求值，不改变单位、不发明权重、不把值当 variance。
// 值必须来自 Phase1 模型；缺失/NaN 一律 fail-closed，禁止零填。
// ===========================================================================

typedef enum {
    P2_SPATIAL_OK            = 0,  // 求值成功
    P2_SPATIAL_OUT_OF_DOMAIN = 1,  // 输出位置在模型节点域外：禁止外插
    P2_SPATIAL_MISSING_NODE  = 2,  // 双线性支撑节点缺失/NaN：禁止零填/退化
    P2_SPATIAL_INVALID_MODEL = 3   // 模型非法（空/尺寸<2/步长<=0/非有限）
} P2SpatialEvalStatus;

// 规则网格空间模型：节点 (ix,iy) 位于
//   (ra0_deg + ix*step_deg, dec0_deg + iy*step_deg)，
// value/valid row-major = [iy*nx + ix]。valid==0 与 value=NaN 同义（缺失）。
typedef struct {
    double ra0_deg;
    double dec0_deg;
    double step_deg;
    std::uint64_t nx;
    std::uint64_t ny;
    const double* value;         // 长度 nx*ny，NaN=缺失
    const std::uint8_t* valid;   // 长度 nx*ny，0=缺失
} P2SpatialGridModel;

typedef struct {
    P2SpatialEvalStatus status;
    double value;                // status==P2_SPATIAL_OK 时有效
    std::uint64_t n_used_nodes;  // 参与插值的有效节点数（成功时=4）
    char err[256];
} P2SpatialEval;

// 确定性双线性求值（DESIGN-P2 §7）。域内 0<=fx<=nx-1 且 0<=fy<=ny-1；
// 边界（恰在节点/右/上边）精确取该边节点，不做外插。
// 四角任一 valid==0 或值非有限 → P2_SPATIAL_MISSING_NODE（禁止跳过/零填）。
// 返回 0=status 为 P2_SPATIAL_OK；1=未成功（status 已写，err 已写）。
P2_API int p2_spatial_model_eval(const P2SpatialGridModel* model,
                                 double ra_deg, double dec_deg,
                                 P2SpatialEval* out);

// 空间摘要（ADJ-GEN-04 摘要面：分位数 + 最大系统偏差 + 采样覆盖 + 模型误差）。
// 只统计有效节点；有效节点值必须全部有限（任一非有限 → rc=1 fail-closed）。
// 无有效节点 → rc=1。分位数为最近秩法 index=floor(p*(n_valid-1))（确定性）。
// sampling_coverage = n_valid / n_nodes ∈ [0,1]；
// max_systematic_deviation = max|v - p50|；model_error = RMS(v - p50)。
typedef struct {
    std::uint64_t n_nodes;
    std::uint64_t n_valid;
    double p05;
    double p50;
    double p95;
    double max_systematic_deviation;
    double model_error;
    double sampling_coverage;
} P2SpatialSummary;

P2_API int p2_spatial_model_summary(const P2SpatialGridModel* model,
                                    P2SpatialSummary* out,
                                    char* err, std::size_t err_size);

// 帧级标量降级门阈值（FZ-DEGRADE-SCALAR）。数值属 SO-07
// PENDING_OWNER_SIGNOFF：thresholds_declared=0 或阈值非正/非有限 →
// fail-closed（P2_SCALAR_UNAVAILABLE），不得自行定值、不得放宽。
typedef struct {
    int thresholds_declared;   // 1 = 调用方显式声明以下冻结阈值
    double residual_trend_max; // (a) 空间残差/趋势门上限（相对量）
    double power_loss_max;     // (b) 功率损失门上限（detection power 相对损失）
} P2ScalarGateThresholds;

// 标量摘要输入（压缩前必须完整；summary_complete=0 或缺键 → fail-closed）。
typedef struct {
    int summary_complete;      // 1 = 以下 p05..适用域全部声明
    double spatial_residual_p95;      // (a) 空间残差 p95
    double spatial_trend;             // (a) 空间趋势 max|线性拟合值|/p50
    double power_loss;                // (b) 功率损失（detection power 相对损失）
    double p05;
    double p50;
    double p95;
    double max_systematic_deviation;
    double sampling_coverage;
    double model_error;
    char applicability_domain[128];
} P2ScalarSummaryInput;

typedef enum {
    P2_SCALAR_ALLOWED        = 0,  // 双门通过 + 摘要完整 → 允许帧级标量
    P2_SCALAR_RETAIN_SPATIAL = 1,  // 门未过 → 必须保留 map/model/control points
    P2_SCALAR_UNAVAILABLE    = 2   // 阈值未声明 / 摘要缺键 / 非法值 → fail-closed
} P2ScalarDegradeVerdict;

// 帧级标量降级门（FZ-DEGRADE-SCALAR）。verdict 始终写出（out 非空时）。
// 返回 0=ALLOWED（可标量）；1=RETAIN_SPATIAL 或 UNAVAILABLE（fail-closed，
// err 写原因）；2=参数错误（th/s/out 为空）。
P2_API int p2_scalar_degrade_gate(const P2ScalarGateThresholds* th,
                                  const P2ScalarSummaryInput* s,
                                  P2ScalarDegradeVerdict* out_verdict,
                                  char* err, std::size_t err_size);

#ifdef __cplusplus
}
#endif
