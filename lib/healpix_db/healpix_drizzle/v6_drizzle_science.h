#ifndef ASTROCS_v6_DRIZZLE_SCIENCE_H
#define ASTROCS_v6_DRIZZLE_SCIENCE_H

// ============================================================================
// AstroCS v6 Phase1 Drizzle 科学核心（signal 单位 / SB 组合 / variance &
// correlation 传播 / 条件通量守恒）
//
// 任务: IMPL-P1-DRZ-001 (wave 5, write_scope = lib/healpix_db/)
// 冻结锚（逐条实现，不得偏离）:
//   FZ-UNIT-SIGNAL-SB   signal_sb            = ADU/px^2
//   FZ-UNIT-VAR-IN      pixel_variance_in    = ADU^2
//   FZ-UNIT-VAR-SB      sb_variance_out      = ADU^2/px^4
//   FZ-UNIT-IVAR-SB     sb_ivar_out          = px^4/ADU^2
//   FZ-UNIT-WINFO       W_info               = ADU^-2
//   FZ-UNIT-FLUX        flux F_hat           = ADU
//   FZ-UNIT-PSFSW       psfsw_robust_weight  = 1
//   FZ-FORMULA-DRIZZLE-SB
//       B_j = x_j / A_pixel_j ; S_p = Sum_j B_j a_jp / Sum_j a_jp
//           = Sum_j w_SB_jp x_j / D_p = Sum_j c_jp x_j
//       w_SB_jp = a_jp / A_pixel_j ; c_jp = w_SB_jp / D_p ; D_p = Sum_j a_jp
//   FZ-FORMULA-DRIZZLE-VAR
//       variance_p = Sum_j c_jp^2 v_j = Sum_j v_j w_SB_jp^2 / D_p^2
//       x -> alpha x  =>  var -> alpha^2 var , ivar -> ivar / alpha^2
//   FZ-FORMULA-COV-PROP
//       Cov(S_p,S_q) = Sum_j c_jp c_jq v_j ; C_out = R C_in R^T
//   FZ-COND-FLUX-CONSERV
//       Phi_out = Sum_p S_p D_p = Sum_j B_j A_drop_j = pixfrac^2 * Sum_j x_j
//       flux_conservation_factor = pixfrac^2 （必须落 provenance）
//   FZ-GATE-CONST-SB
//       x_j = B0 * A_pixel_j  =>  S_p = B0 对所有 pixfrac in (0,1]
//       容差 |S_p/B0 - 1| < 1e-3（沿用，不改数值）
//   FZ-GATE-PARENT-VAR  （PENDING_OWNER_SIGNOFF -> fail-closed）
//       父级对角归约须声明 lower_bound=true + 相关核/算子摘要 + deficit 记录；
//       未签字前不得声明精确。
//   FZ-PROV-KCORR / FZ-PROV-MINIMAL-SET
//       provenance 最小集齐备；k_corr 缺适用域或取 1.0 忽略相关 -> REJECT。
//   FZ-BUNIT-SEMANTICS
//       BUNIT 量纲可判：显式 px 幂次，或 BUNIT=ADU 且声明
//       pixel_semantics=surface_brightness + pixel_area_power(-2 signal / -4 var)。
//
// 本模块不接线任何 session（任务卡：不接线 session）。纯科学核心 + fail-closed
// 门；几何由 v6_spherical_overlap.h 提供，或由调用方直接给出 a_jp。
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace astrocs {
namespace v6 {
namespace drizzle {

// ---------------------------------------------------------------------------
// 1. 冻结单位表
// ---------------------------------------------------------------------------

// 量纲向量：ADU 幂次 × px 幂次（球面立体角等价）。
struct UnitDimension {
    int adu_power = 0;
    int px_power = 0;
};

inline bool operator==(const UnitDimension& a, const UnitDimension& b) {
    return a.adu_power == b.adu_power && a.px_power == b.px_power;
}
inline bool operator!=(const UnitDimension& a, const UnitDimension& b) {
    return !(a == b);
}

enum class UnitId {
    signal_sb,          // ADU/px^2       (FZ-UNIT-SIGNAL-SB)
    sb_variance_out,    // ADU^2/px^4     (FZ-UNIT-VAR-SB)
    sb_ivar_out,        // px^4/ADU^2     (FZ-UNIT-IVAR-SB)
    pixel_variance_in,  // ADU^2          (FZ-UNIT-VAR-IN)
    w_info,             // ADU^-2         (FZ-UNIT-WINFO)
    flux,               // ADU            (FZ-UNIT-FLUX)
    q,                  // ADU^-1         (FZ-UNIT-Q)
    psfsw_robust_weight,// 1              (FZ-UNIT-PSFSW)
    pixel_area,         // px^2
    dimensionless,      // 1  (w_SB / c coefficient)
};

struct FrozenUnit {
    UnitId id;
    const char* symbol;
    UnitDimension dim;
};

// 冻结单位表唯一事实源（与冻结表逐字一致）。
const FrozenUnit& frozen_unit(UnitId id);

// 量纲代数：variance 的幂次 = 2 × signal 的幂次；ivar = -variance。
UnitDimension square_dimension(UnitDimension d);
UnitDimension inverse_dimension(UnitDimension d);

// 门 G-STRUCT-UNIT-LAW: variance == signal^2 且 ivar == 1/variance。
bool unit_law_holds(UnitId signal, UnitId variance, UnitId ivar);

// ---------------------------------------------------------------------------
// 2. 算子原语：w_SB / c_jp / D_p
// ---------------------------------------------------------------------------

// w_SB_jp = a_jp / A_pixel_j   （无量纲；a_jp 与 A_pixel_j 同 px^2 单位）
double sb_weight(double a_jp, double A_pixel_j);

// c_jp = w_SB_jp / D_p          （作用于 x_j，无量纲）
double sb_combination_coefficient(double w_sb_jp, double D_p);

// 历史 legacy drop 权重 w_legacy_jp = a_jp / A_drop_j，
// 满足 w_SB_jp = pixfrac^2 * w_legacy_jp。
double legacy_drop_weight(double a_jp, double A_drop_j);
double legacy_to_sb_weight(double w_legacy_jp, double pixfrac);
double sb_to_legacy_weight(double w_sb_jp, double pixfrac);

// FZ-COND-FLUX-CONSERV 的 provenance 因子。
double flux_conservation_factor(double pixfrac);

// ---------------------------------------------------------------------------
// 3. fail-closed 错误面
// ---------------------------------------------------------------------------

enum class DrzError {
    ok = 0,
    invalid_pixfrac,               // pixfrac <= 0 或 > 1（显式拒绝，不夹逼）
    ring_ordering,                 // RING ordering 显式拒绝（只支持 NESTED）
    multi_channel,                 // 多通道显式拒绝
    missing_wcs,                   // 缺 WCS / 尺度非法
    nonfinite_input,               // NaN / Inf 输入
    invalid_source_area,           // A_pixel_j <= 0 或非有限
    overlap_exceeds_drop,          // Sum_p a_jp > A_drop_j（几何闭合失败上溢）
    coverage_mismatch,             // D_p 与 x 贡献不一致（零覆盖却贡献）
    legacy_normalization_at_pixfrac_lt_one, // legacy 归一用于 pixfrac<1 绝对面亮度
    missing_flux_conservation_factor,
    unit_undeterminable,
    invalid_argument,
};

const char* drz_error_name(DrzError e);

// 输入合法性（冻结 fail-closed 域）。
DrzError validate_pixfrac(double pixfrac);
DrzError validate_geometry_flags(bool nested, bool single_channel, bool wcs_present);

// 几何闭合：Sum_p a_jp == pixfrac^2 * A_pixel_j（全覆盖，线性恒等）。
// rel 为实测相对偏差；超出 rel_tol -> overlap_exceeds_drop。
DrzError validate_overlap_closure(double sum_a_jp, double A_pixel_j, double pixfrac,
                                  double rel_tol, double* rel_out);

// ---------------------------------------------------------------------------
// 4. Drizzle 线性算子
// ---------------------------------------------------------------------------
//
// 只承载 v6 目标态 SB 归一：c_jp = (a_jp / A_pixel_j) / D_p。
// legacy_a_drop 仅为等价映射/provenance 登记保留：pixfrac<1 时显式 fail-closed。
enum class NormalizationKind {
    sb_a_pixel,    // v6 目标态：w_SB = a/A_pixel
    legacy_a_drop, // 历史：w = a/A_drop；仅 pixfrac==1 与目标态一致
};

struct OperatorEntry {
    uint32_t src = 0;    // 源像素 j
    uint32_t dst = 0;    // 目标 leaf p（存储于行内时 == 行索引）
    double a_jp = 0.0;   // 球面交叠面积 [px^2]
    double w_sb = 0.0;   // a_jp / A_pixel_j
    double c = 0.0;      // w_sb / D_p
};

struct OperatorSource {
    double A_pixel = 0.0; // 源像素面积 [px^2]（pixfrac=1）
};

class DrizzleOperator {
public:
    DrizzleOperator() = default;

    // 由 (src, dst, a_jp) 三元组构建；内部转置为 per-target 行并累加 D_p。
    // overlaps: 每条 {src, dst, a_jp}。同一个 (src,dst) 只允许出现一次。
    static DrzError build(uint32_t n_src, uint32_t n_dst,
                          const std::vector<OperatorSource>& sources,
                          double pixfrac,
                          NormalizationKind kind,
                          const std::vector<OperatorEntry>& overlaps,
                          DrizzleOperator& out);

    uint32_t n_src() const { return n_src_; }
    uint32_t n_dst() const { return n_dst_; }
    double pixfrac() const { return pixfrac_; }
    NormalizationKind normalization() const { return kind_; }
    const std::vector<double>& D_p() const { return D_p_; }
    const std::vector<double>& A_pixel() const { return A_pixel_; }
    double A_pixel(uint32_t j) const { return A_pixel_[j]; }
    double D(uint32_t p) const { return D_p_[p]; }
    const std::vector<OperatorEntry>& row(uint32_t p) const { return rows_[p]; }

    // S_p = Sum_j c_jp x_j   [ADU/px^2]
    double signal_sb(uint32_t p, const double* x) const;

    // variance_p = Sum_j c_jp^2 v_j   [ADU^2/px^4]
    double variance_sb(uint32_t p, const double* v) const;

    // ivar_p = 1 / variance_p   [px^4/ADU^2]
    double ivar_sb(uint32_t p, const double* v) const;

    // Cov(S_p, S_q) = Sum_j c_jp c_jq v_j   [ADU^2/px^4]
    double covariance_sb(uint32_t p, uint32_t q, const double* v) const;

    // Phi_out = Sum_p S_p D_p   [ADU]
    double flux_out(const double* x) const;

    // Var(Sum_p a_p S_p) = Sum_{p,q} a_p a_q Cov(S_p,S_q)（精确二次型）
    // diag_only（可选输出）= Sum_p a_p^2 variance_p（严格下界）
    double aperture_variance(const double* weights, const double* v,
                             double* diag_only_out = nullptr) const;

    // 父级归约（per FZ-GATE-PARENT-VAR）
    double parent_variance_exact(const double* v) const;   // Sum D_p D_q / (Sum D)^2 Cov
    double parent_variance_diagonal(const double* v) const;
    double parent_deficit(const double* v) const;          // (exact - diag) / exact

private:
    uint32_t n_src_ = 0;
    uint32_t n_dst_ = 0;
    double pixfrac_ = 1.0;
    NormalizationKind kind_ = NormalizationKind::sb_a_pixel;
    std::vector<double> A_pixel_;
    std::vector<double> D_p_;
    std::vector<std::vector<OperatorEntry>> rows_;
};

// ---------------------------------------------------------------------------
// 5. 独立真值（供测试 Oracle 之外的实现内一致性检查使用；门自身只接受
//    "raw" 输入并重算，不信任任何已计算量，避免同实现自证）
// ---------------------------------------------------------------------------

// 由 raw (a_jp, A_pixel_j, D_p) 重算 c_jp（v6 目标态）。
double reference_sb_coefficient(double a_jp, double A_pixel_j, double D_p);

// ---------------------------------------------------------------------------
// 6. 门裁决
// ---------------------------------------------------------------------------

struct GateVerdict {
    const char* gate_id = "";
    bool pass = false;
    std::string reason;
};

// FZ-FORMULA-DRIZZLE-SB: claimed S_p 必须等于由 raw a_jp/A_pixel/D_p 重算的
// 目标态 SB 组合（逐 target，rel < tol）。
GateVerdict gate_sb_definition(const DrizzleOperator& op, const double* x,
                               const std::vector<double>& claimed_signal,
                               double rel_tol = 1e-11);

// FZ-GATE-CONST-SB: 断言 x_j = B0 * A_pixel_j，且重算 S_p/B0 - 1 < 1e-3。
// 若提供 claimed_signal，同样检查。
GateVerdict gate_constant_surface_brightness(const DrizzleOperator& op,
                                             const double* x, double B0,
                                             const std::vector<double>* claimed_signal = nullptr,
                                             double rel_tol = 1e-3);

// FZ-FORMULA-DRIZZLE-VAR: claimed variance 必须等于由 raw 输入重算的
// Sum_j (raw c_jp)^2 v_j（不是由 op 的 c 字段反推）。
GateVerdict gate_variance_identity(const DrizzleOperator& op, const double* v,
                                   const std::vector<double>& claimed_variance,
                                   double rel_tol = 1e-11);

// 缩放律：x -> alpha x 时 var -> alpha^2 var（逐像素精确；variance 只依赖 v）。
GateVerdict gate_variance_scale_law(const DrizzleOperator& op, const double* v,
                                    double alpha,
                                    const std::vector<double>& claimed_scaled_variance,
                                    const std::vector<double>& claimed_base_variance,
                                    double rel_tol = 1e-11);

// FZ-FORMULA-COV-PROP: 对角元 = variance；非对角一般非零；aperture 精确方差
// 严格 >= 对角-only，且当权重与重叠非负时严格 >（存在非对角贡献）。
GateVerdict gate_covariance_propagation(const DrizzleOperator& op, const double* v,
                                        const double* aperture_weights,
                                        bool claims_diagonal_exact,
                                        double diag_rel_tol = 1e-11);

// FZ-COND-FLUX-CONSERV: Phi_out == flux_conservation_factor * Sum_j x_j；
// pixfrac<1 且声明绝对通量却缺 factor -> REJECT。
GateVerdict gate_flux_conservation(const DrizzleOperator& op, const double* x,
                                   bool factor_recorded,
                                   bool claims_absolute_flux,
                                   double rel_tol = 1e-11);

// FZ-GATE-PARENT-VAR（fail-closed）: 父级归约必须声明 lower_bound=true、
// 提供相关核/可重建算子摘要、给出 deficit；在未签字阈值下不得声明精确。
struct ParentReductionRecord {
    bool is_lower_bound = false;
    bool claims_exact = false;
    bool has_correlation_kernel_summary = false;
    bool has_deficit_record = false;
    double deficit = 0.0;
    double owner_signed_threshold = 0.0;
    bool threshold_owner_signed = false;
};
GateVerdict gate_parent_reduction(const ParentReductionRecord& rec);

// FZ-BUNIT-SEMANTICS: BUNIT 量纲可判 + 二次律。
struct BunitDeclaration {
    std::string bunit;
    bool pixel_semantics_declared = false;
    std::string pixel_semantics;
    bool has_pixel_area_power = false;
    int pixel_area_power = 0;
    bool is_canonical_px_power = false; // BUNIT 显式含 px 幂次
};
GateVerdict gate_bunit_semantics(const BunitDeclaration& signal,
                                 const BunitDeclaration& variance);

// FZ-PROV-MINIMAL-SET / FZ-PROV-KCORR
struct ProvenanceRecord {
    std::string schema_version;
    std::string software_sha;
    std::string run_id;
    std::string input_hash;
    std::string config_hash;
    std::string units_bunit;
    bool has_pixel_area_power = false;
    int pixel_area_power = 0;
    std::string pixel_semantics;
    std::string frame;
    std::string pixel_sampling_semantics;
    std::string algorithm_id;
    std::string provider;
    std::string approximation_and_degradation;
    bool unavailable = false;
    std::string unavailable_reason;
    std::string normalization_version;
    bool has_correlation_kernel_summary = false;
    bool has_flux_conservation_factor = false;
    double flux_conservation_factor = 0.0;
    bool has_kcorr = false;
    double k_corr = 0.0;
    bool kcorr_has_domain = false;
    bool kcorr_zero_correlation_justified = false;
    std::string kcorr_calibration_ref;
    std::string generated_utc;
    std::string output_hash;
};
GateVerdict gate_provenance_minimal_set(const ProvenanceRecord& prov,
                                        double expected_flux_conservation_factor);

// G-STRUCT-UNIT-LAW 门。
GateVerdict gate_unit_law(UnitId signal, UnitId variance, UnitId ivar);

} // namespace drizzle
} // namespace v6
} // namespace astrocs

#endif // ASTROCS_v6_DRIZZLE_SCIENCE_H
