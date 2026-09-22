/* psfsw.h - Phase1/V6 PSFSW 四分量 / 共同星集 / validity-depth 门 (IMPL-P1-PSFW-001)
 *
 * 合同锚 (主: docs/science/PSF_SIGNAL_WEIGHT.md;
 *         Phase1: docs/design/PHASE1_DETAILED_DESIGN.md;
 *         冻结: docs/algorithms/GATES_AND_TOLERANCES.md;
 *         eng/contracts/data/v6_clause_registry_v1.json#weight_vocabulary):
 *   - FZ-FIELD-PSFSW-4COMP  : signal/concentration/noise/background 四分量,
 *                             measurement_id 互异, p05<=p50<=p95, valid_area_fraction in [0,1]
 *   - FZ-COND-WHITENOISE    : A_NEA = 1 / Sum P^2  (由 dynamic_psf 提供)
 *   - FZ-FORMULA-PSFSW-COMPOSITE: Wt=C_norm S^a Conc^b /(N^g B^d);
 *                             W_psfsw = Wt / median_j(Wt_j); 组内 median=1 且全正
 *   - FZ-FIELD-PSFSW-UNIT   : weight_kind=relative_dimensionless; weight_units=1;
 *                             group_normalized=true; scope=group; median_target=1.0
 *   - FZ-GATE-PSFSW-FAILCLOSED: 5 项白名单; valid=false -> weight_value=null; 禁 median SNR 回退
 *   - FZ-GATE-PSFSW-COV     : covariance.method=propagated_from_composite_coefficients;
 *                             C_out = R C_in R^T; 禁 1/W_psfsw 与诊断量反推
 *   - FZ-GATE-PSFSW-EPSF    : effective_psf_id 非空 + 归一约定; 只给 FWHM 标量 = REJECT
 *   - FZ-DEGRADE-SCALAR     : 标量需同时过空间残差/趋势门与功率损失门
 *   - FZ-GATE-MEDIAN-SNR / FZ-GATE-SUPPORT-COVERAGE: 诊断别名不得进权重面
 *   - PSFSW-T-DEPTH/-K/-SPAN/-RHO, -NU, -NU-LOW, -TREND, -POWERLOSS, -FLUXBIAS,
 *     -NMIN/-NROBUST/-NPREF, -WRANGE, COMPOSITE-ALPHA..FLOOR (PENDING_OWNER_SIGNOFF,
 *     fail-closed 实现, 不放宽)
 *
 * 纪律: 不接线 session; psfsw 无量纲且组内 median=1, 禁止写成 ivar/Fisher;
 *       不把 median(SNR_F)/support/coverage/FWHM 当权重。纯 std + libm。
 */
#ifndef ASTROCS_V6_P1PSFW_PSFSW_H
#define ASTROCS_V6_P1PSFW_PSFSW_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {
namespace v6 {
namespace p1psfw {

/* ------------------------------------------------------------------------- */
/* 冻结阈值 (全部 PENDING_OWNER_SIGNOFF -> fail-closed 实现, 不放宽)            */
/* ------------------------------------------------------------------------- */
constexpr double kMadToSigma = 1.482602218505602;
constexpr double kDepthMaxRelDev = 0.05;         /* PSFSW-T-DEPTH */
constexpr int    kDepthMinScanPoints = 5;        /* PSFSW-T-DEPTH-K */
constexpr double kDepthMinSpanMag = 1.0;         /* PSFSW-T-DEPTH-SPAN */
constexpr double kDepthMaxAbsRho = 0.8;          /* PSFSW-T-DEPTH-RHO */
constexpr int    kNCommonMin = 3;                /* PSFSW-T-NMIN */
constexpr int    kNCommonRobust = 10;            /* PSFSW-T-NROBUST */
constexpr int    kNCommonPreferred = 30;         /* PSFSW-T-NPREF */
constexpr double kNonuniformityMax = 0.30;       /* PSFSW-T-NU */
constexpr double kNonuniformityMaxLow = 0.20;    /* PSFSW-T-NU-LOW */
constexpr double kSpatialTrendMax = 0.10;        /* PSFSW-T-TREND */
constexpr double kPowerLossMax = 0.05;           /* PSFSW-T-POWERLOSS */
constexpr double kFluxBiasMax = 0.01;            /* PSFSW-T-FLUXBIAS */
constexpr double kWeightRangeMax = 100.0;        /* PSFSW-T-WRANGE */
constexpr double kComponentFloor = 1e-12;        /* PSFSW-COMPOSITE-FLOOR */
constexpr double kCompositeAlpha = 2.0;          /* PSFSW-COMPOSITE-ALPHA */
constexpr double kCompositeBeta = 1.0;           /* PSFSW-COMPOSITE-BETA */
constexpr double kCompositeGamma = 2.0;          /* PSFSW-COMPOSITE-GAMMA */
constexpr double kCompositeDelta = 1.0;          /* PSFSW-COMPOSITE-DELTA */
constexpr double kCompositeCNorm = 1.0;          /* PSFSW-COMPOSITE-CNORM */
constexpr double kCompositeMedianRtol = 1e-9;    /* 组内 median == 1 */
constexpr double kGroupMedianTarget = 1.0;
constexpr const char* kCompositeVersion = "PSFSW-COMPOSITE-V1";

/* ------------------------------------------------------------------------- */
/* fail-closed 原因白名单 (FZ-GATE-PSFSW-FAILCLOSED; 不得增删)                 */
/* ------------------------------------------------------------------------- */
enum class PsfswReason {
    none = 0,
    no_common_star_set,
    background_nonpositive_undefined_transform,
    insufficient_valid_stars,
    selection_bias_gate_failed,
    spatial_nonuniformity_gate_failed,
};

const char* to_string(PsfswReason r);
bool is_whitelisted_reason(PsfswReason r);
bool parse_reason(const std::string& s, PsfswReason* out);

/* n_common 分档 (PSFSW-T-NMIN/-NROBUST/-NPREF) */
enum class NCommonTier { hard_fail, low, standard, preferred };
const char* to_string(NCommonTier t);
NCommonTier n_common_tier(int n_common);

/* ------------------------------------------------------------------------- */
/* 稳健统计 (独立实现; Oracle 另有独立实现以互证)                              */
/* ------------------------------------------------------------------------- */
double median_of(std::vector<double> v);
double robust_scale_mad(const double* x, std::size_t n, double* out_median = nullptr);
double quantile_of(std::vector<double> v, double p);
double spearman_rho(const std::vector<double>& x, const std::vector<double>& y);

/* ------------------------------------------------------------------------- */
/* 四分量 (FZ-FIELD-PSFSW-4COMP)                                              */
/* ------------------------------------------------------------------------- */
struct ComponentMeasure {
    std::string name;                 /* "signal" | "concentration" | "noise" | "background" */
    std::string measurement_id;       /* 四者互异 */
    std::string unit;
    std::string estimator;
    std::string estimator_version;
    double value = 0.0;               /* 帧级标量 (在 validity 门上) */
    double p05 = 0.0, p50 = 0.0, p95 = 0.0;
    double valid_area_fraction = 1.0;
    double spatial_spread_rel = 0.0;  /* (p95-p05)/p50 */
    double linear_trend_max_rel = 0.0;/* max |线性拟合值| / p50 */
    bool has_spatial_map = false;
    bool has_model = false;
    bool has_control_points = false;
    std::vector<double> samples;      /* 空间样本 (有序), 供分位/趋势复算 */
};

struct FrameComponentInput {
    double a_nea = 0.0;                        /* px^2, 由 PSF 归一算得 */
    std::vector<double> fhat;                  /* 逐共同星 PSF 测光通量 [component_flux_unit] */
    std::vector<char> fhat_valid;              /* 与 fhat 等长; 空=全有效 */
    double background_robust_mean = 0.0;       /* bbar_k */
    double a_ref = 0.0;                        /* 参考面积 [px^2]; 0 => a_nea */
    std::string component_flux_unit = "ADU";
    std::string estimator_version = "psfsw-est-v1";
    std::vector<double> signal_samples;
    std::vector<double> concentration_samples;
    std::vector<double> noise_samples;
    std::vector<double> background_samples;
};

struct PsfswFrameComponents {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;              /* 机器可读原因 */
    double s = 0.0, conc = 0.0, n = 0.0, b = 0.0;
    ComponentMeasure signal, concentration, noise, background;
};

/* S_k = Sum fhat; Conc_k = mean(fhat)/A_NEA; N_k = 1.482602218505602*MAD(fhat);
 * B_k = bbar_k * A_ref,k。样本不足 (<3) / A_NEA 非正 -> insufficient_valid_stars。 */
PsfswFrameComponents extract_psfsw_components(const FrameComponentInput& in);

/* ------------------------------------------------------------------------- */
/* 复合权重与组内归一 (FZ-FORMULA-PSFSW-COMPOSITE)                             */
/* ------------------------------------------------------------------------- */
struct CompositeParams {
    std::string version = kCompositeVersion;
    double alpha = kCompositeAlpha;
    double beta = kCompositeBeta;
    double gamma = kCompositeGamma;
    double delta = kCompositeDelta;
    double c_norm = kCompositeCNorm;
    double floor = kComponentFloor;
};

struct ComponentValues {
    double s = 0.0, conc = 0.0, n = 0.0, b = 0.0;
};

struct CompositeResult {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;
    std::vector<double> wt;        /* 未归一复合 (尺度简并) */
    std::vector<double> w_psfsw;   /* 组内 median = 1, 全正 */
    double median_wt = 0.0;
    double median_w_psfsw = 0.0;
};

/* 截断顺序固定: (1) fail-closed (B<=0 -> background_nonpositive_undefined_transform;
 * N/S/Conc<=0 -> insufficient_valid_stars) (2) 分量下限 (3) Wt (4) 组内 median 归一。
 * 组内 median == 1 且全正; 禁止后归一化裁剪。 */
CompositeResult compute_psfsw_weights(const std::vector<ComponentValues>& frames,
                                      const CompositeParams& params = CompositeParams());

/* 组内中值归一 (供 C_norm 尺度简并复算) */
std::vector<double> group_normalize_by_median(const std::vector<double>& wt, double* out_median);

/* 指数合法性 (PSFSW-G14): (alpha>0 or beta>0) and (gamma>0 or delta>0), 非全零。 */
bool composite_exponents_valid(const CompositeParams& p);

/* C_norm 尺度简并: 返回 max |W(c_norm) - W(1)| (应为 0, 逐位/容差)。 */
double cnorm_invariance_deviation(const std::vector<ComponentValues>& frames, double c_norm);

/* ------------------------------------------------------------------------- */
/* 共同星集与 selection function (SC-ADJ-P203.6 / PSFSW-G08/G09)              */
/* ------------------------------------------------------------------------- */
struct ExclusionFlags {
    bool saturated = false;
    bool blended = false;
    bool trailed = false;
    bool moving = false;
    bool psf_mismatch = false;
    bool edge_truncated = false;
    bool any() const;
};

struct CommonStarMember {
    int64_t star_id = 0;
    ExclusionFlags flags;               /* 组级排除旗标 */
    std::vector<char> valid_in_frame;   /* 逐帧检测/匹配有效 (长度 = n_frames) */
};

struct SelectionFunction {
    std::string selection_function_id;
    std::string reference_catalog_id;
    std::string reference_catalog_version_hash;
    double mag_min = 0.0, mag_max = 0.0;
    double detection_threshold_sigma = 0.0;
    double matching_radius_arcsec = 0.0;
    std::string epoch_pm_handling;
    std::string applied_at;
    std::string independence_proof;   /* external_reference_catalog | reference_stack_single_threshold */
};

struct CommonStarSet {
    std::string common_star_set_id;
    SelectionFunction selection;
    std::vector<CommonStarMember> members;
};

struct CommonStarValidation {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;
    int n_common = 0;                          /* |S| (组级共同星集) */
    std::vector<int> per_frame_valid;          /* 每帧 |S_k| */
    std::vector<int64_t> valid_star_ids;       /* 组级有效成员 */
};

/* 缺 id/selection 字段 -> no_common_star_set;
 * independence_proof 非法 -> selection_bias_gate_failed;
 * n_common<3 或任一帧 |S_k|<3 -> insufficient_valid_stars。 */
CommonStarValidation validate_common_star_set(const CommonStarSet& set, int n_frames);

/* ------------------------------------------------------------------------- */
/* validity / depth 门                                                        */
/* ------------------------------------------------------------------------- */
struct DepthScanPoint {
    double mag_limit = 0.0;
    int n_common = 0;
    std::vector<double> w_psfsw;   /* 组内归一权重 (与帧对齐) */
};

struct DepthGateResult {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;
    int k = 0;
    double span_mag = 0.0;
    double max_rel_dev = 0.0;
    double max_abs_rho = 0.0;
    bool n_common_span_ok = false;
};

/* K>=5 且 (跨度>=1.0 mag 或 n_common 变化>=2 倍); 每点 n_common>=3;
 * max_t |W_{k,t} - median_t(W_{k,t})| / median_t(W_{k,t}) <= 0.05;
 * |Spearman(scan index, W_{k,t})| <= 0.8。 */
DepthGateResult depth_stability_gate(const std::vector<DepthScanPoint>& scans);

struct ComponentMeasure;
struct NonuniformityResult {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;
    const ComponentMeasure* failing_component = nullptr;
    double spread = 0.0;
    double trend = 0.0;
    double spread_limit = kNonuniformityMax;
};

/* 任一 (p95-p05)/p50 > limit (LOW 档 0.20) 或趋势 > 0.10
 * -> spatial_nonuniformity_gate_failed (须拆 tile / 存 map/model)。 */
NonuniformityResult spatial_nonuniformity_gate(
    const std::vector<ComponentMeasure>& components, NCommonTier tier);

struct ScalarDegradationResult {
    bool ok = false;
    PsfswReason reason = PsfswReason::none;
    const char* reject = nullptr;
};

/* 同时过 (a) 空间门 与 (b) 标量功率损失门: power_loss<=0.05 且 flux_bias<=0.01。 */
ScalarDegradationResult scalar_degradation_gate(double power_loss, double flux_bias);

struct WeightRangeGuard {
    bool exceeded = false;         /* >100 仅强制旗标, 不判 unavailable */
    double range = 0.0;
    double concentration = 0.0;    /* sum a_k^2 / (sum a_k)^2 */
};

WeightRangeGuard weight_dynamic_range_guard(const std::vector<double>& w_psfsw);

/* ------------------------------------------------------------------------- */
/* conventional coadd / covariance / effective PSF (FZ-FORMULA-COV-PROP)       */
/* ------------------------------------------------------------------------- */
struct CoaddResult {
    bool ok = false;
    const char* reject = nullptr;
    std::vector<double> i_out;
    std::vector<char> defined;                       /* 每像素是否有有效帧 */
    std::vector<std::vector<double>> alpha;          /* [K][P] 组合系数 */
};

/* alpha_k(p) = W_k v_k(p) / Sum_j W_j v_j(p); I_out(p) = Sum_k alpha_k(p) d_k(p)。 */
CoaddResult conventional_coadd(const std::vector<std::vector<double>>& d,
                               const std::vector<std::vector<char>>& validity,
                               const std::vector<double>& w_psfsw);

struct CovariancePropagation {
    bool ok = false;
    const char* reject = nullptr;
    std::string method = "propagated_from_composite_coefficients";
    bool variance_from_weight = false;
    bool uses_relative_weight_as_ivar = false;
    std::vector<double> var_out;                     /* Var(I_out(p)) */
};

/* Var(I_out(p)) = Sum_{k,l} alpha_k(p) alpha_l(p) C_in(k,l)。C_in 为 K*K 行主序。 */
CovariancePropagation propagate_covariance(
    const std::vector<std::vector<double>>& alpha,
    const std::vector<double>& c_in);

/* ------------------------------------------------------------------------- */
/* psfsw 记录门 (PSFSW-G01..G25 的 Phase1 切片)                                */
/* ------------------------------------------------------------------------- */
const std::vector<std::string>& forbidden_psfsw_product_keys();
const std::vector<std::string>& forbidden_weight_source_aliases();
/* FZ-MODE-RETIRED（PSFSW-RETIRE-03 口径统一）：psfsw_robust 是**退役对象**
 * psfsw_robust_weight 的声明 token；权重只有一个口径（FZ-WEIGHT-SINGLE-PATH：
 * 阶段1 稀疏 SNR 控制点 → 阶段2 重建稠密 SNR 面 → 逆方差定权 → 叠加），
 * 因此不存在任何"生产权重口径"集合。识别/拒绝面保留：命中即调用方必须显式拒绝 +
 * 迁移提示，不得静默接受。 */
bool is_retired_weight_mode_token(const std::string& mode);
/* 拒绝说明（含被拒 token、现行允许面与迁移提示）；未命中返回空串。 */
std::string retired_weight_mode_reject_reason(const std::string& mode);

struct GateFinding {
    std::string gate;      /* "PSFSW-G05" ... */
    std::string detail;
};

struct PsfswRecord {
    /* 身份：本记录族 = 退役对象 psfsw_robust_weight 的历史/诊断声明面。
     * 唯一合法取值 = 退役 token "psfsw_robust"；声明任何其它值 ⇒ PSFSW-G01。 */
    std::string weight_mode = "psfsw_robust";
    std::string weight_kind = "relative_dimensionless";
    std::string weight_units = "1";
    bool group_normalized = true;
    std::string normalization_scope = "group";
    double normalization_median_target = kGroupMedianTarget;
    std::string component_flux_unit = "ADU";
    /* 共同星集 */
    std::string common_star_set_id;
    std::string selection_function_id;
    std::string independence_proof;
    int n_common = 0;
    /* depth 稳定性门 (PSFSW-G11) */
    bool depth_gate_evaluated = false;
    bool depth_gate_ok = false;
    double depth_max_rel_dev = 0.0;
    double depth_max_abs_rho = 0.0;
    int depth_k = 0;
    /* 复合/版本 */
    CompositeParams composite;
    std::vector<ComponentMeasure> components;    /* 恰 4 项 */
    std::vector<double> w_psfsw;
    /* validity */
    bool valid = true;
    bool has_weight_value = true;
    double weight_value = 0.0;
    PsfswReason reason = PsfswReason::none;
    std::string weight_value_source;
    /* covariance / effective PSF */
    std::string covariance_method = "propagated_from_composite_coefficients";
    bool variance_from_weight = false;
    bool uses_relative_weight_as_ivar = false;
    std::vector<std::string> combination_coefficient_ids;
    std::string variance_from;
    std::string effective_psf_id;
    bool effective_psf_only_fwhm = false;
    bool effective_psf_normalization_declared = true;
    /* 标量降级门 */
    double power_loss = 0.0;
    double flux_bias = 0.0;
    /* 诊断来源与禁止键 */
    std::vector<std::string> weight_sources;
    std::vector<std::string> produced_keys;      /* 点分路径, 任意层 */
    /* 样本分离 */
    std::string calibration_sample_id;
    std::string acceptance_sample_id;
    /* 基线声明 (可空) */
    std::string baseline_claim;                  /* better_than | non_inferior_to | "" */
    int baseline_bootstrap_resamples = 0;
    double baseline_confidence = 0.0;
};

struct RecordValidation {
    bool accept = true;
    std::vector<GateFinding> findings;
};

RecordValidation validate_psfsw_record(const PsfswRecord& rec);

}  /* namespace p1psfw */
}  /* namespace v6 */
}  /* namespace astrocs */

#endif /* ASTROCS_V6_P1PSFW_PSFSW_H */
