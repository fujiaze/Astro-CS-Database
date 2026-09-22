// ============================================================================
// AstroCS v6 Phase1 Drizzle 科学核心实现 — 见 v6_drizzle_science.h
// 任务 IMPL-P1-DRZ-001；冻结锚 FZ-FORMULA-DRIZZLE-SB/VAR、FZ-FORMULA-COV-PROP、
// FZ-COND-FLUX-CONSERV、FZ-GATE-CONST-SB、FZ-GATE-PARENT-VAR、FZ-BUNIT-SEMANTICS、
// FZ-PROV-MINIMAL-SET、FZ-PROV-KCORR、FZ-UNIT-*。
//
// 所有门以 raw (a_jp, A_pixel_j, D_p) 重算真值，不读取任何已计算的 c/variance，
// 避免"同实现自证"。
// ============================================================================
#include "v6_drizzle_science.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>

namespace astrocs {
namespace v6 {
namespace drizzle {

namespace {

bool is_finite(double v) { return std::isfinite(v); }

double rel_diff(double a, double b) {
    const double scale = std::max(std::fabs(a), std::fabs(b));
    if (scale == 0.0) return 0.0;
    return std::fabs(a - b) / scale;
}

GateVerdict fail(const char* gate, std::string why) {
    GateVerdict v;
    v.gate_id = gate;
    v.pass = false;
    v.reason = std::move(why);
    return v;
}
GateVerdict pass(const char* gate) {
    GateVerdict v;
    v.gate_id = gate;
    v.pass = true;
    return v;
}

// 由 raw 输入重算 v6 目标态 c_jp（不读 op 的 c 字段）。
double raw_c(const DrizzleOperator& op, uint32_t p, const OperatorEntry& e) {
    return reference_sb_coefficient(e.a_jp, op.A_pixel(e.src), op.D(p));
}

} // namespace

// ---------------------------------------------------------------------------
// 单位表
// ---------------------------------------------------------------------------

const FrozenUnit& frozen_unit(UnitId id) {
    static const FrozenUnit table[] = {
        {UnitId::signal_sb,           "ADU/px^2",   {1, -2}},
        {UnitId::sb_variance_out,     "ADU^2/px^4", {2, -4}},
        {UnitId::sb_ivar_out,         "px^4/ADU^2", {-2, 4}},
        {UnitId::pixel_variance_in,   "ADU^2",      {2, 0}},
        {UnitId::w_info,              "ADU^-2",     {-2, 0}},
        {UnitId::flux,                "ADU",        {1, 0}},
        {UnitId::q,                   "ADU^-1",     {-1, 0}},
        /* PSFSW-RETIRE-03：psfsw_robust_weight 的行已**物理删除**（原 RETIRED 占位行）。
         * 该对象不是现行对象，冻结单位表不承载它的任何身份；识别/拒绝走字符串面
         * is_retired_unit_symbol + retired_unit_reject_reason。 */
        {UnitId::pixel_area,          "px^2",       {0, 2}},
        {UnitId::dimensionless,       "1",          {0, 0}},
    };
    for (const FrozenUnit& u : table) {
        if (u.id == id) return u;
    }
    /* 兜底 = 表末项 dimensionless。用表长表达式而非硬编码下标：删行会使下标平移，
     * 硬编码即越界（PSFSW-RETIRE-03 删 psfsw_robust_weight 行时即为此）。 */
    return table[sizeof(table) / sizeof(table[0]) - 1];
}

UnitDimension square_dimension(UnitDimension d) {
    return UnitDimension{d.adu_power * 2, d.px_power * 2};
}
UnitDimension inverse_dimension(UnitDimension d) {
    return UnitDimension{-d.adu_power, -d.px_power};
}

bool unit_law_holds(UnitId signal, UnitId variance, UnitId ivar) {
    const UnitDimension s = frozen_unit(signal).dim;
    const UnitDimension v = frozen_unit(variance).dim;
    const UnitDimension i = frozen_unit(ivar).dim;
    return square_dimension(s) == v && inverse_dimension(v) == i;
}

// ── 退役对象显式拒绝（PSFSW-RETIRE-01）──────────────────────────────────
namespace {
const char kRetiredUnitSymbol[] = "psfsw_robust_weight";

char ascii_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}
} // namespace

/* PSFSW-RETIRE-03：is_retired_unit_id 已随枚举项物理删除——退役对象不再有
 * 枚举身份，识别面只有字符串面（见头文件说明）。 */

bool is_retired_unit_symbol(const std::string& symbol) {
    // 大小写不敏感全等（与 coverage/resample 的 token 门同口径）。
    const std::size_t n = sizeof(kRetiredUnitSymbol) - 1; // 不含 '\0'
    if (symbol.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (ascii_lower(symbol[i]) != ascii_lower(kRetiredUnitSymbol[i])) return false;
    }
    return true;
}

std::string retired_unit_reject_reason(const std::string& symbol) {
    if (!is_retired_unit_symbol(symbol)) return std::string();
    return "retired unit/weight object '" + symbol +
           "' (FZ-UNIT-PSFSW retired; PSFSW-RETIRE-01): psfsw_robust_weight is not a "
           "current object (ASTROCS_DESIGN.md 3.1; UNIFIED_MODEL.md:58); allowed "
           "weight objects: point_information (W_info=1/Var(F_hat)) or surface_gls "
           "(A^T C^-1 A); PSF quality proxies (FWHM/residual) are diagnostics only";
}

// ---------------------------------------------------------------------------
// 算子原语
// ---------------------------------------------------------------------------

double sb_weight(double a_jp, double A_pixel_j) { return a_jp / A_pixel_j; }
double sb_combination_coefficient(double w_sb_jp, double D_p) { return w_sb_jp / D_p; }
double legacy_drop_weight(double a_jp, double A_drop_j) { return a_jp / A_drop_j; }
double legacy_to_sb_weight(double w_legacy_jp, double pixfrac) {
    return pixfrac * pixfrac * w_legacy_jp;
}
double sb_to_legacy_weight(double w_sb_jp, double pixfrac) {
    return w_sb_jp / (pixfrac * pixfrac);
}
double flux_conservation_factor(double pixfrac) { return pixfrac * pixfrac; }

double reference_sb_coefficient(double a_jp, double A_pixel_j, double D_p) {
    return (a_jp / A_pixel_j) / D_p;
}

// ---------------------------------------------------------------------------
// fail-closed
// ---------------------------------------------------------------------------

const char* drz_error_name(DrzError e) {
    switch (e) {
        case DrzError::ok: return "ok";
        case DrzError::invalid_pixfrac: return "invalid_pixfrac";
        case DrzError::ring_ordering: return "ring_ordering";
        case DrzError::multi_channel: return "multi_channel";
        case DrzError::missing_wcs: return "missing_wcs";
        case DrzError::nonfinite_input: return "nonfinite_input";
        case DrzError::invalid_source_area: return "invalid_source_area";
        case DrzError::overlap_exceeds_drop: return "overlap_exceeds_drop";
        case DrzError::coverage_mismatch: return "coverage_mismatch";
        case DrzError::legacy_normalization_at_pixfrac_lt_one:
            return "legacy_normalization_at_pixfrac_lt_one";
        case DrzError::missing_flux_conservation_factor:
            return "missing_flux_conservation_factor";
        case DrzError::unit_undeterminable: return "unit_undeterminable";
        case DrzError::invalid_argument: return "invalid_argument";
    }
    return "unknown";
}

DrzError validate_pixfrac(double pixfrac) {
    if (!is_finite(pixfrac)) return DrzError::invalid_pixfrac;
    if (pixfrac <= 0.0 || pixfrac > 1.0) return DrzError::invalid_pixfrac;
    return DrzError::ok;
}

DrzError validate_geometry_flags(bool nested, bool single_channel, bool wcs_present) {
    if (!nested) return DrzError::ring_ordering;
    if (!single_channel) return DrzError::multi_channel;
    if (!wcs_present) return DrzError::missing_wcs;
    return DrzError::ok;
}

DrzError validate_overlap_closure(double sum_a_jp, double A_pixel_j, double pixfrac,
                                  double rel_tol, double* rel_out) {
    if (validate_pixfrac(pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    if (!is_finite(sum_a_jp) || !is_finite(A_pixel_j) || A_pixel_j <= 0.0) {
        return DrzError::invalid_source_area;
    }
    const double expected = pixfrac * pixfrac * A_pixel_j;
    const double rel = (sum_a_jp - expected) / expected;
    if (rel_out) *rel_out = rel;
    if (rel > rel_tol) return DrzError::overlap_exceeds_drop;
    return DrzError::ok;
}

// ---------------------------------------------------------------------------
// DrizzleOperator
// ---------------------------------------------------------------------------

DrzError DrizzleOperator::build(uint32_t n_src, uint32_t n_dst,
                                const std::vector<OperatorSource>& sources,
                                double pixfrac, NormalizationKind kind,
                                const std::vector<OperatorEntry>& overlaps,
                                DrizzleOperator& out) {
    if (validate_pixfrac(pixfrac) != DrzError::ok) return DrzError::invalid_pixfrac;
    if (n_src == 0 || n_dst == 0) return DrzError::invalid_argument;
    if (sources.size() != n_src) return DrzError::invalid_argument;
    if (kind == NormalizationKind::legacy_a_drop && pixfrac < 1.0) {
        // FZ-FORMULA-DRIZZLE-SB fail-closed：legacy 归一不得用于 pixfrac<1 的
        // 绝对面亮度层。
        return DrzError::legacy_normalization_at_pixfrac_lt_one;
    }

    DrizzleOperator op;
    op.n_src_ = n_src;
    op.n_dst_ = n_dst;
    op.pixfrac_ = pixfrac;
    op.kind_ = kind;
    op.A_pixel_.resize(n_src);
    for (uint32_t j = 0; j < n_src; ++j) {
        const double a = sources[j].A_pixel;
        if (!is_finite(a) || a <= 0.0) return DrzError::invalid_source_area;
        op.A_pixel_[j] = a;
    }
    op.D_p_.assign(n_dst, 0.0);
    op.rows_.assign(n_dst, {});

    std::unordered_set<uint64_t> seen;
    seen.reserve(overlaps.size() * 2 + 1);
    for (const OperatorEntry& e : overlaps) {
        if (e.src >= n_src || e.dst >= n_dst) return DrzError::invalid_argument;
        if (!is_finite(e.a_jp)) return DrzError::nonfinite_input;
        if (e.a_jp < 0.0) return DrzError::invalid_argument;
        const uint64_t key = (static_cast<uint64_t>(e.src) << 32) | e.dst;
        if (!seen.insert(key).second) return DrzError::invalid_argument; // 重复 (src,dst)
        op.D_p_[e.dst] += e.a_jp;
    }
    for (const OperatorEntry& e : overlaps) {
        if (op.D_p_[e.dst] <= 0.0) return DrzError::coverage_mismatch;
        const double w = (kind == NormalizationKind::legacy_a_drop)
                             ? legacy_drop_weight(e.a_jp, pixfrac * pixfrac * op.A_pixel_[e.src])
                             : sb_weight(e.a_jp, op.A_pixel_[e.src]);
        OperatorEntry entry = e;
        entry.w_sb = w;
        entry.c = sb_combination_coefficient(w, op.D_p_[e.dst]);
        op.rows_[e.dst].push_back(entry);
    }
    // 行内按源像素 j 升序：covariance_sb 的归并扫描依赖该不变量。
    for (auto& r : op.rows_) {
        std::sort(r.begin(), r.end(), [](const OperatorEntry& a, const OperatorEntry& b) {
            return a.src < b.src;
        });
    }
    out = std::move(op);
    return DrzError::ok;
}

double DrizzleOperator::signal_sb(uint32_t p, const double* x) const {
    double s = 0.0;
    for (const OperatorEntry& e : rows_[p]) s += e.c * x[e.src];
    return s;
}

double DrizzleOperator::variance_sb(uint32_t p, const double* v) const {
    double acc = 0.0;
    for (const OperatorEntry& e : rows_[p]) acc += e.c * e.c * v[e.src];
    return acc;
}

double DrizzleOperator::ivar_sb(uint32_t p, const double* v) const {
    const double var = variance_sb(p, v);
    return (var > 0.0) ? (1.0 / var) : 0.0;
}

double DrizzleOperator::covariance_sb(uint32_t p, uint32_t q, const double* v) const {
    // 输入像素独立：Cov = Sum_j c_jp c_jq v_j。对两个稀疏行做归并。
    const std::vector<OperatorEntry>& a = rows_[p];
    const std::vector<OperatorEntry>& b = rows_[q];
    double acc = 0.0;
    size_t i = 0, k = 0;
    while (i < a.size() && k < b.size()) {
        if (a[i].src == b[k].src) {
            acc += a[i].c * b[k].c * v[a[i].src];
            ++i;
            ++k;
        } else if (a[i].src < b[k].src) {
            ++i;
        } else {
            ++k;
        }
    }
    return acc;
}

double DrizzleOperator::flux_out(const double* x) const {
    double phi = 0.0;
    for (uint32_t p = 0; p < n_dst_; ++p) phi += signal_sb(p, x) * D_p_[p];
    return phi;
}

double DrizzleOperator::aperture_variance(const double* weights, const double* v,
                                          double* diag_only_out) const {
    // exact = Sum_j v_j (Sum_p w_p c_jp)^2
    std::vector<double> per_src(n_src_, 0.0);
    for (uint32_t p = 0; p < n_dst_; ++p) {
        const double wp = weights[p];
        if (wp == 0.0) continue;
        for (const OperatorEntry& e : rows_[p]) per_src[e.src] += wp * e.c;
    }
    double exact = 0.0;
    for (uint32_t j = 0; j < n_src_; ++j) exact += v[j] * per_src[j] * per_src[j];

    if (diag_only_out) {
        double diag = 0.0;
        for (uint32_t p = 0; p < n_dst_; ++p) {
            diag += weights[p] * weights[p] * variance_sb(p, v);
        }
        *diag_only_out = diag;
    }
    return exact;
}

double DrizzleOperator::parent_variance_exact(const double* v) const {
    double sumD = 0.0;
    for (double d : D_p_) sumD += d;
    if (sumD <= 0.0) return 0.0;
    std::vector<double> per_src(n_src_, 0.0);
    for (uint32_t p = 0; p < n_dst_; ++p) {
        const double wp = D_p_[p] / sumD;
        if (wp == 0.0) continue;
        for (const OperatorEntry& e : rows_[p]) per_src[e.src] += wp * e.c;
    }
    double exact = 0.0;
    for (uint32_t j = 0; j < n_src_; ++j) exact += v[j] * per_src[j] * per_src[j];
    return exact;
}

double DrizzleOperator::parent_variance_diagonal(const double* v) const {
    double sumD = 0.0;
    for (double d : D_p_) sumD += d;
    if (sumD <= 0.0) return 0.0;
    double diag = 0.0;
    for (uint32_t p = 0; p < n_dst_; ++p) {
        const double wp = D_p_[p] / sumD;
        diag += wp * wp * variance_sb(p, v);
    }
    return diag;
}

double DrizzleOperator::parent_deficit(const double* v) const {
    const double exact = parent_variance_exact(v);
    if (exact <= 0.0) return 0.0;
    return (exact - parent_variance_diagonal(v)) / exact;
}

// ---------------------------------------------------------------------------
// 门
// ---------------------------------------------------------------------------

GateVerdict gate_sb_definition(const DrizzleOperator& op, const double* x,
                               const std::vector<double>& claimed_signal,
                               double rel_tol) {
    const char* g = "FZ-FORMULA-DRIZZLE-SB";
    if (op.normalization() != NormalizationKind::sb_a_pixel) {
        return fail(g, "normalization != sb_a_pixel");
    }
    if (claimed_signal.size() != op.n_dst()) return fail(g, "claim size mismatch");
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        double ref = 0.0;
        for (const OperatorEntry& e : op.row(p)) ref += raw_c(op, p, e) * x[e.src];
        if (!is_finite(ref) || !is_finite(claimed_signal[p])) {
            if (!(std::isnan(ref) && std::isnan(claimed_signal[p]))) {
                return fail(g, "non-finite signal");
            }
            continue; // NaN 传播一致（drizzle 层不掩膜，见 ALG-P1-001 §6.6）
        }
        if (rel_diff(ref, claimed_signal[p]) > rel_tol) {
            return fail(g, "S_p != Sum_j B_j a_jp / Sum_j a_jp");
        }
    }
    return pass(g);
}

GateVerdict gate_constant_surface_brightness(const DrizzleOperator& op, const double* x,
                                             double B0,
                                             const std::vector<double>* claimed_signal,
                                             double rel_tol) {
    const char* g = "FZ-GATE-CONST-SB";
    if (op.normalization() != NormalizationKind::sb_a_pixel) {
        return fail(g, "normalization != sb_a_pixel");
    }
    // (1) 构造必须是面亮度：x_j == B0 * A_pixel_j（禁止常量 ADU 构造）。
    for (uint32_t j = 0; j < op.n_src(); ++j) {
        const double expect = B0 * op.A_pixel(j);
        if (!is_finite(x[j]) || rel_diff(x[j], expect) > 1e-9) {
            return fail(g, "x_j != B0 * A_pixel_j (constant-ADU construction)");
        }
    }
    // (2) S_p = B0 对全部覆盖 target。
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        double s = 0.0;
        for (const OperatorEntry& e : op.row(p)) s += raw_c(op, p, e) * x[e.src];
        if (B0 == 0.0) {
            if (std::fabs(s) > rel_tol) return fail(g, "S_p != 0 for B0=0");
            continue;
        }
        if (std::fabs(s / B0 - 1.0) >= rel_tol) {
            return fail(g, "|S_p/B0 - 1| >= 1e-3 (frozen tolerance)");
        }
        if (claimed_signal && op.n_dst() == claimed_signal->size()) {
            if (rel_diff(s, (*claimed_signal)[p]) > rel_tol) {
                return fail(g, "claimed S_p != recomputed SB");
            }
        }
    }
    return pass(g);
}

GateVerdict gate_variance_identity(const DrizzleOperator& op, const double* v,
                                   const std::vector<double>& claimed_variance,
                                   double rel_tol) {
    const char* g = "FZ-FORMULA-DRIZZLE-VAR";
    if (op.normalization() != NormalizationKind::sb_a_pixel) {
        return fail(g, "normalization != sb_a_pixel");
    }
    if (claimed_variance.size() != op.n_dst()) return fail(g, "claim size mismatch");
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        double ref = 0.0;
        for (const OperatorEntry& e : op.row(p)) {
            const double c = raw_c(op, p, e);
            ref += c * c * v[e.src];
        }
        if (!is_finite(claimed_variance[p]) || rel_diff(ref, claimed_variance[p]) > rel_tol) {
            return fail(g, "variance_p != Sum_j c_jp^2 v_j");
        }
        if (rel_diff(ref, op.variance_sb(p, v)) > rel_tol) {
            return fail(g, "operator variance != Sum_j c_jp^2 v_j");
        }
    }
    return pass(g);
}

GateVerdict gate_variance_scale_law(const DrizzleOperator& op, const double* v,
                                    double alpha,
                                    const std::vector<double>& claimed_scaled_variance,
                                    const std::vector<double>& claimed_base_variance,
                                    double rel_tol) {
    const char* g = "FZ-FORMULA-DRIZZLE-VAR";
    if (claimed_scaled_variance.size() != op.n_dst() ||
        claimed_base_variance.size() != op.n_dst()) {
        return fail(g, "claim size mismatch");
    }
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        double base = 0.0;
        for (const OperatorEntry& e : op.row(p)) {
            const double c = raw_c(op, p, e);
            base += c * c * v[e.src];
        }
        if (rel_diff(base, claimed_base_variance[p]) > rel_tol) {
            return fail(g, "base variance claim mismatch");
        }
        const double expect = alpha * alpha * base;
        if (rel_diff(expect, claimed_scaled_variance[p]) > rel_tol) {
            return fail(g, "var(alpha x) != alpha^2 var(x)");
        }
    }
    return pass(g);
}

GateVerdict gate_covariance_propagation(const DrizzleOperator& op, const double* v,
                                        const double* aperture_weights,
                                        bool claims_diagonal_exact,
                                        double diag_rel_tol) {
    const char* g = "FZ-FORMULA-COV-PROP";
    // 对角元 == Sum_j c_jp^2 v_j。
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        double ref = 0.0;
        for (const OperatorEntry& e : op.row(p)) {
            const double c = raw_c(op, p, e);
            ref += c * c * v[e.src];
        }
        if (rel_diff(ref, op.covariance_sb(p, p, v)) > diag_rel_tol) {
            return fail(g, "diag(Cov) != variance_p");
        }
    }
    double diag = 0.0;
    const double exact = op.aperture_variance(aperture_weights, v, &diag);
    const double scale = std::max(std::fabs(exact), std::fabs(diag));
    const double tol = diag_rel_tol * std::max(scale, 1.0);
    if (exact + tol < diag) {
        return fail(g, "aperture exact variance < diagonal-only (PSD violation)");
    }
    const bool has_offdiag = (exact - diag) > tol;
    if (claims_diagonal_exact && has_offdiag) {
        return fail(g, "diagonal-only reduction claimed exact while off-diagonal != 0");
    }
    return pass(g);
}

GateVerdict gate_flux_conservation(const DrizzleOperator& op, const double* x,
                                   bool factor_recorded, bool claims_absolute_flux,
                                   double rel_tol) {
    const char* g = "FZ-COND-FLUX-CONSERV";
    const double pf = op.pixfrac();
    const double factor = flux_conservation_factor(pf);
    if (claims_absolute_flux && pf < 1.0 && !factor_recorded) {
        return fail(g, "pixfrac<1 absolute flux without flux_conservation_factor");
    }
    if (!factor_recorded) {
        return fail(g, "missing flux_conservation_factor in provenance");
    }
    double sum_x = 0.0;
    for (uint32_t j = 0; j < op.n_src(); ++j) sum_x += x[j];
    const double expect = factor * sum_x;
    const double phi = op.flux_out(x);
    if (rel_diff(phi, expect) > rel_tol) {
        return fail(g, "Phi_out != pixfrac^2 * Sum_j x_j");
    }
    return pass(g);
}

GateVerdict gate_parent_reduction(const ParentReductionRecord& rec) {
    const char* g = "FZ-GATE-PARENT-VAR";
    if (!rec.is_lower_bound) return fail(g, "parent diagonal reduction must be lower_bound=true");
    if (rec.claims_exact) return fail(g, "diagonal reduction must not claim exact");
    if (!rec.has_correlation_kernel_summary) {
        return fail(g, "missing correlation kernel / reconstructable operator summary");
    }
    if (!rec.has_deficit_record) return fail(g, "missing deficit error record");
    if (rec.threshold_owner_signed) {
        if (!(rec.deficit <= rec.owner_signed_threshold)) {
            return fail(g, "deficit exceeds owner-signed threshold");
        }
    }
    // 未签字：fail-closed —— 只允许下界声明，不允许精确声明（已在上方强制）。
    return pass(g);
}

namespace {
// 从 BUNIT 声明解析有效像素幂次；不可判 -> false。
bool effective_px_power(const BunitDeclaration& d, int* power) {
    if (d.is_canonical_px_power) {
        if (d.bunit.find("px^") == std::string::npos) return false;
        *power = d.pixel_area_power;
        return true;
    }
    if (d.bunit == "ADU" && d.pixel_semantics_declared &&
        d.pixel_semantics == "surface_brightness" && d.has_pixel_area_power) {
        *power = d.pixel_area_power;
        return true;
    }
    return false;
}
} // namespace

GateVerdict gate_bunit_semantics(const BunitDeclaration& signal,
                                 const BunitDeclaration& variance) {
    const char* g = "FZ-BUNIT-SEMANTICS";
    int sp = 0, vp = 0;
    if (!effective_px_power(signal, &sp)) {
        return fail(g, "signal BUNIT undeterminable (needs px power or SB declaration)");
    }
    if (!effective_px_power(variance, &vp)) {
        return fail(g, "variance BUNIT undeterminable (needs px power or SB declaration)");
    }
    if (sp != -2) return fail(g, "signal_sb pixel_area_power != -2");
    if (vp != -4) return fail(g, "sb_variance_out pixel_area_power != -4");
    if (vp != 2 * sp) return fail(g, "variance BUNIT != signal BUNIT^2");
    return pass(g);
}

GateVerdict gate_provenance_minimal_set(const ProvenanceRecord& prov,
                                        double expected_flux_conservation_factor) {
    const char* g = "FZ-PROV-MINIMAL-SET";
    const std::string* required[] = {
        &prov.schema_version, &prov.software_sha, &prov.run_id,
        &prov.input_hash, &prov.config_hash, &prov.units_bunit,
        &prov.frame, &prov.pixel_sampling_semantics, &prov.algorithm_id,
        &prov.provider, &prov.approximation_and_degradation,
        &prov.normalization_version, &prov.generated_utc, &prov.output_hash,
    };
    for (const std::string* s : required) {
        if (s->empty()) return fail(g, "missing provenance minimal-set key");
    }
    if (!prov.has_pixel_area_power) return fail(g, "missing pixel_area_power");
    if (prov.unavailable && prov.unavailable_reason.empty()) {
        return fail(g, "unavailable without reason");
    }
    if (!prov.has_correlation_kernel_summary) {
        return fail(g, "missing correlation kernel / operator summary");
    }
    if (!prov.has_flux_conservation_factor) {
        return fail(g, "missing flux_conservation_factor");
    }
    if (rel_diff(prov.flux_conservation_factor, expected_flux_conservation_factor) > 1e-12) {
        return fail(g, "flux_conservation_factor != pixfrac^2");
    }
    if (!prov.has_kcorr) return fail(g, "missing k_corr");
    if (!prov.kcorr_has_domain) return fail(g, "k_corr missing applicability domain");
    if (prov.kcorr_calibration_ref.empty()) return fail(g, "k_corr missing calibration ref");
    if (prov.k_corr == 1.0 && !prov.kcorr_zero_correlation_justified) {
        return fail(g, "k_corr == 1.0 ignoring correlation");
    }
    return pass(g);
}

GateVerdict gate_unit_law(UnitId signal, UnitId variance, UnitId ivar) {
    const char* g = "G-STRUCT-UNIT-LAW";
    const UnitDimension s = frozen_unit(signal).dim;
    const UnitDimension v = frozen_unit(variance).dim;
    const UnitDimension i = frozen_unit(ivar).dim;
    if (square_dimension(s) != v) return fail(g, "variance != signal^2");
    if (inverse_dimension(v) != i) return fail(g, "ivar != 1/variance");
    return pass(g);
}

} // namespace drizzle
} // namespace v6
} // namespace astrocs
