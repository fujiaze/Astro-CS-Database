// ============================================================================
// IMPL-P1-DRZ-001 共址单元测试 — V6 Phase1 Drizzle signal 单位 / 球面 overlap /
// variance & correlation 传播 / 条件通量守恒
//
// 组: units | oracle | negative | geometry
// 运行: ./v6_p1_drz_test <group>
//
// 正向：冻结公式逐条数值一致（对照独立 Oracle，不调用被测实现生成期望）。
// 负向：注入违反冻结的实现/记录 -> 对应门必须红（pass=false），并有正向控制。
// 几何：真实球面 overlap（spherical_overlap.cpp + astrocs::healpix）参与。
// ============================================================================
#include "v6_drizzle_science.h"
#include "v6_spherical_overlap.h"
#include "v6_p1_drz_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace astrocs::v6::drizzle;

// 故障注入用的进程环境设置（Linux setenv / Windows _putenv_s）。
static void set_env(const char* key, const char* value) {
#ifdef _WIN32
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const std::string& msg) {
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  [FAIL] %s\n", msg.c_str());
    }
}

static void check_gate(const GateVerdict& v, bool want_pass, const std::string& what) {
    const bool ok = (v.pass == want_pass);
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        std::printf("  [FAIL] %s: gate=%s pass=%d reason=%s\n", what.c_str(),
                    v.gate_id, (int)v.pass, v.reason.c_str());
    }
}

static bool rel_close(double a, double b, double tol) {
    const double scale = std::max(std::fabs(a), std::fabs(b));
    if (scale == 0.0) return true;
    return std::fabs(a - b) / scale <= tol;
}

// ---------------------------------------------------------------------------
// 合成算子 fixture（闭合：Sum_p a_jp = pixfrac^2 * A_pixel_j）
// ---------------------------------------------------------------------------
struct Fixture {
    double pixfrac = 0.7;
    uint32_t n_src = 6;
    uint32_t n_dst = 4;
    std::vector<double> A_pixel;
    std::vector<OperatorSource> sources;
    std::vector<OperatorEntry> overlaps;
    std::vector<double> x, v, x_sb, aperture_w;
    double B0 = 5.0;
};

static Fixture make_fixture() {
    Fixture f;
    const double pf = f.pixfrac;
    const double pf2 = pf * pf;
    // (src, dst, a_jp)
    const double raw[][3] = {
        {0, 0, 1.0}, {0, 1, 0.5},
        {1, 0, 0.8}, {1, 1, 1.2},
        {2, 1, 2.0}, {2, 2, 0.7},
        {3, 2, 1.5}, {3, 3, 0.9},
        {4, 2, 0.6}, {4, 3, 2.2},
        {5, 0, 0.3}, {5, 3, 1.1},
    };
    std::vector<double> sum_a(f.n_src, 0.0);
    for (const auto& r : raw) {
        OperatorEntry e;
        e.src = (uint32_t)r[0];
        e.dst = (uint32_t)r[1];
        e.a_jp = r[2];
        f.overlaps.push_back(e);
        sum_a[e.src] += e.a_jp;
    }
    f.A_pixel.resize(f.n_src);
    f.sources.resize(f.n_src);
    for (uint32_t j = 0; j < f.n_src; ++j) {
        f.A_pixel[j] = sum_a[j] / pf2; // 闭合构造
        f.sources[j].A_pixel = f.A_pixel[j];
    }
    f.x = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0};
    f.v = {1.0, 4.0, 9.0, 16.0, 25.0, 36.0};
    f.aperture_w = {1.0, 2.0, 3.0, 4.0};
    f.x_sb.resize(f.n_src);
    for (uint32_t j = 0; j < f.n_src; ++j) f.x_sb[j] = f.B0 * f.A_pixel[j];
    return f;
}

static std::vector<double> op_signal(const DrizzleOperator& op, const std::vector<double>& x) {
    std::vector<double> out(op.n_dst());
    for (uint32_t p = 0; p < op.n_dst(); ++p) out[p] = op.signal_sb(p, x.data());
    return out;
}
static std::vector<double> op_variance(const DrizzleOperator& op, const std::vector<double>& v) {
    std::vector<double> out(op.n_dst());
    for (uint32_t p = 0; p < op.n_dst(); ++p) out[p] = op.variance_sb(p, v.data());
    return out;
}

static v6_p1_drz_oracle::Model oracle_from_fixture(const Fixture& f) {
    std::vector<v6_p1_drz_oracle::Item> items;
    for (const auto& e : f.overlaps) {
        items.push_back({e.src, e.dst, e.a_jp});
    }
    return v6_p1_drz_oracle::Model::build(f.n_src, f.n_dst, f.A_pixel, f.pixfrac, items);
}

// ---------------------------------------------------------------------------
// 组 1: units
// ---------------------------------------------------------------------------
static void run_units() {
    std::printf("[units]\n");
    const FrozenUnit& us = frozen_unit(UnitId::signal_sb);
    const FrozenUnit& uv = frozen_unit(UnitId::sb_variance_out);
    const FrozenUnit& ui = frozen_unit(UnitId::sb_ivar_out);
    check(std::strcmp(us.symbol, "ADU/sr") == 0, "signal_sb symbol");
    check(std::strcmp(uv.symbol, "ADU^2/sr^2") == 0, "sb_variance_out symbol");
    check(std::strcmp(ui.symbol, "sr^2/ADU^2") == 0, "sb_ivar_out symbol");
    check((frozen_unit(UnitId::pixel_variance_in).dim == UnitDimension{2, 0}),
          "pixel_variance_in=ADU^2");
    check((frozen_unit(UnitId::w_info).dim == UnitDimension{-2, 0}), "W_info=ADU^-2");
    /* PSFSW-RETIRE-03：psfsw_robust_weight 已**物理删除**（UnitId 无其项、冻结单位表
       无其行）——本行原为 frozen_unit(UnitId::psfsw_robust_weight).dim 断言。退役对象
       的识别面只剩字符串面，故改为锁定"仍被显式识别 + 现行符号不被误判"（不是删断言）。 */
    check(is_retired_unit_symbol("psfsw_robust_weight"),
          "retired unit symbol still recognized (string face)");
    check(!is_retired_unit_symbol("W_info"), "live unit symbol not misjudged as retired");
    check(unit_law_holds(UnitId::signal_sb, UnitId::sb_variance_out, UnitId::sb_ivar_out),
          "unit law variance=signal^2");

    check_gate(gate_unit_law(UnitId::signal_sb, UnitId::sb_variance_out, UnitId::sb_ivar_out),
               true, "unit law positive control");
    // 负向 M-S4: 把 sb_variance_out 写成 ADU^2
    check_gate(gate_unit_law(UnitId::signal_sb, UnitId::pixel_variance_in, UnitId::sb_ivar_out),
               false, "unit law negative (variance!=signal^2)");
    check_gate(gate_unit_law(UnitId::signal_sb, UnitId::sb_variance_out, UnitId::w_info),
               false, "unit law negative (ivar!=1/var)");

    // BUNIT 二次律（FZ-BUNIT-SEMANTICS）
    BunitDeclaration sig;
    sig.bunit = "ADU/sr";
    sig.is_canonical_px_power = true;
    sig.pixel_area_power = -2;
    BunitDeclaration var;
    var.bunit = "ADU^2/sr^2";
    var.is_canonical_px_power = true;
    var.pixel_area_power = -4;
    check_gate(gate_bunit_semantics(sig, var), true, "BUNIT canonical positive");
    BunitDeclaration bare;
    bare.bunit = "ADU"; // 无 pixel_semantics 声明
    check_gate(gate_bunit_semantics(bare, var), false, "BUNIT negative (bare ADU)");
    BunitDeclaration sig_decl;
    sig_decl.bunit = "ADU";
    sig_decl.pixel_semantics_declared = true;
    sig_decl.pixel_semantics = "surface_brightness";
    sig_decl.has_pixel_area_power = true;
    sig_decl.pixel_area_power = -2;
    BunitDeclaration var_decl = sig_decl;
    var_decl.pixel_area_power = -4;
    check_gate(gate_bunit_semantics(sig_decl, var_decl), true, "BUNIT declared-SB positive");
    BunitDeclaration var_wrong = var_decl;
    var_wrong.pixel_area_power = -2; // 一次幂，违反二次律
    check_gate(gate_bunit_semantics(sig_decl, var_wrong), false, "BUNIT negative (not quadratic)");

    // provenance 最小集 + k_corr
    ProvenanceRecord pr;
    pr.schema_version = "astrocs.v6.signal/v1";
    pr.software_sha = "deadbeef";
    pr.run_id = "run-1";
    pr.input_hash = "in";
    pr.config_hash = "cfg";
    pr.units_bunit = "ADU/sr";
    pr.has_pixel_area_power = true;
    pr.pixel_area_power = -2;
    pr.pixel_semantics = "surface_brightness";
    pr.frame = "icrs";
    pr.pixel_sampling_semantics = "healpix_nested_leaf";
    pr.algorithm_id = "ALG-P1-DRZ-SB-001";
    pr.provider = "cpu";
    pr.approximation_and_degradation = "none";
    pr.normalization_version = "sb_a_pixel/v1";
    pr.has_correlation_kernel_summary = true;
    pr.has_flux_conservation_factor = true;
    pr.flux_conservation_factor = 1.0;  // DRZ-FLUX-FIX-01: drop 面积归一 ⇒ 恒 1
    pr.has_kcorr = true;
    pr.k_corr = 1.02;
    pr.kcorr_has_domain = true;
    pr.kcorr_calibration_ref = "calib/kcorr/v1";
    pr.generated_utc = "2026-09-15T00:00:00Z";
    pr.output_hash = "out";
    check_gate(gate_provenance_minimal_set(pr, 1.0), true, "provenance positive");
    ProvenanceRecord p2 = pr;
    p2.has_flux_conservation_factor = false;
    check_gate(gate_provenance_minimal_set(p2, 1.0), false, "provenance negative (no flux factor)");
    ProvenanceRecord p3 = pr;
    p3.k_corr = 1.0; // 忽略相关
    check_gate(gate_provenance_minimal_set(p3, 1.0), false, "provenance negative (k_corr=1)");
    ProvenanceRecord p4 = pr;
    p4.kcorr_has_domain = false;
    check_gate(gate_provenance_minimal_set(p4, 1.0), false, "provenance negative (k_corr no domain)");
    ProvenanceRecord p5 = pr;
    p5.run_id.clear();
    check_gate(gate_provenance_minimal_set(p5, 0.49), false, "provenance negative (missing key)");
    ProvenanceRecord p6 = pr;
    p6.unavailable = true;
    check_gate(gate_provenance_minimal_set(p6, 0.49), false, "provenance negative (unavailable w/o reason)");
}

// ---------------------------------------------------------------------------
// 组 2: oracle — 冻结公式 vs 独立 Oracle + 正向门
// ---------------------------------------------------------------------------
static void run_oracle() {
    std::printf("[oracle]\n");
    Fixture f = make_fixture();
    DrizzleOperator op;
    DrzError e = DrizzleOperator::build(f.n_src, f.n_dst, f.sources, f.pixfrac,
                                        NormalizationKind::drop_area, f.overlaps, op);
    check(e == DrzError::ok, "fixture build ok");
    if (e != DrzError::ok) return;

    v6_p1_drz_oracle::Model om = oracle_from_fixture(f);
    const double* x = f.x.data();
    const double* v = f.v.data();

    // S_p
    std::vector<double> s_impl = op_signal(op, f.x);
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        check(v6_p1_drz_oracle::rel_close(op.signal_sb(p, x), om.signal(p, x), 1e-12L),
              "S_p vs oracle target " + std::to_string(p));
    }
    // variance
    std::vector<double> var_impl = op_variance(op, f.v);
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        check(v6_p1_drz_oracle::rel_close(op.variance_sb(p, v), om.variance(p, v), 1e-12L),
              "variance_p vs oracle target " + std::to_string(p));
    }
    // covariance
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        for (uint32_t q = 0; q < op.n_dst(); ++q) {
            check(v6_p1_drz_oracle::rel_close(op.covariance_sb(p, q, v), om.covariance(p, q, v), 1e-12L),
                  "Cov vs oracle (" + std::to_string(p) + "," + std::to_string(q) + ")");
        }
    }
    // aperture
    double diag = 0.0;
    const double ap = op.aperture_variance(f.aperture_w.data(), v, &diag);
    check(v6_p1_drz_oracle::rel_close(ap, om.aperture(f.aperture_w.data(), v), 1e-12L),
          "aperture exact vs oracle");
    check(v6_p1_drz_oracle::rel_close(diag, om.aperture_diag(f.aperture_w.data(), v), 1e-12L),
          "aperture diag vs oracle");
    check(ap > diag, "aperture exact strictly > diagonal-only");
    // parent
    check(v6_p1_drz_oracle::rel_close(op.parent_variance_exact(v), om.parent_exact(v), 1e-12L),
          "parent exact vs oracle");
    check(v6_p1_drz_oracle::rel_close(op.parent_variance_diagonal(v), om.parent_diag(v), 1e-12L),
          "parent diag vs oracle");
    // flux
    check(v6_p1_drz_oracle::rel_close(op.flux_out(x), om.flux(x), 1e-12L),
          "flux_out vs oracle");

    // 常量面亮度：S_p = B0 全 target
    std::vector<double> s_sb = op_signal(op, f.x_sb);
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        check(std::fabs(s_sb[p] / f.B0 - 1.0) < 1e-3, "constant SB S_p/B0 - 1 < 1e-3");
    }

    // 正向门
    check_gate(gate_sb_definition(op, x, s_impl), true, "gate SB definition");
    check_gate(gate_constant_surface_brightness(op, f.x_sb.data(), f.B0, &s_sb), true,
               "gate constant SB");
    check_gate(gate_variance_identity(op, v, var_impl), true, "gate variance identity");
    {
        const double alpha = 3.0;
        std::vector<double> v2(f.v.size());
        for (size_t i = 0; i < f.v.size(); ++i) v2[i] = alpha * alpha * f.v[i];
        std::vector<double> scaled = op_variance(op, v2);
        check_gate(gate_variance_scale_law(op, v, alpha, scaled, var_impl), true,
                   "gate variance scale law");
    }
    check_gate(gate_covariance_propagation(op, v, f.aperture_w.data(), false), true,
               "gate covariance propagation");
    check_gate(gate_flux_conservation(op, x, true, true), true, "gate flux conservation");

    ParentReductionRecord rec;
    rec.is_lower_bound = true;
    rec.claims_exact = false;
    rec.has_correlation_kernel_summary = true;
    rec.has_deficit_record = true;
    rec.deficit = op.parent_deficit(v);
    check(rec.deficit > 0.0 && rec.deficit < 1.0, "parent deficit in (0,1)");
    check_gate(gate_parent_reduction(rec), true, "gate parent reduction (lower bound)");

    // 单位律 + BUNIT 正向（生产语义一致性）
    check_gate(gate_unit_law(UnitId::signal_sb, UnitId::sb_variance_out, UnitId::sb_ivar_out),
               true, "unit law on frozen table");
}

// ---------------------------------------------------------------------------
// 组 3: negative — 注入违反冻结的实现/记录 -> 门必红
// ---------------------------------------------------------------------------
struct MutantClaims {
    std::vector<double> signal_missing_D;   // S_p=F_p（漏 D_p 归一）
    std::vector<double> signal_legacy;      // 分母取覆盖面积 D_p（缺 N_p ⇒ 偏差 1/pf^2）
    std::vector<double> variance_missing_D2; // sum v w_sb^2
    std::vector<double> variance_no_square;  // sum v w_sb/D
};

static MutantClaims make_mutants(const DrizzleOperator& op, const Fixture& f) {
    MutantClaims m;
    m.signal_missing_D.assign(op.n_dst(), 0.0);
    m.signal_legacy.assign(op.n_dst(), 0.0);
    m.variance_missing_D2.assign(op.n_dst(), 0.0);
    m.variance_no_square.assign(op.n_dst(), 0.0);
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        for (const OperatorEntry& e : op.row(p)) {
            const double A_sb = op.A_pixel(e.src);
            const double A_drop = f.pixfrac * f.pixfrac * A_sb;
            m.signal_missing_D[p] += e.w_sb * f.x[e.src];
            m.signal_legacy[p] += (e.a_jp / A_drop) / op.D(p) * f.x[e.src];
            m.variance_missing_D2[p] += f.v[e.src] * e.w_sb * e.w_sb;
            m.variance_no_square[p] += f.v[e.src] * e.w_sb / op.D(p);
        }
    }
    return m;
}

static void run_negative() {
    std::printf("[negative]\n");
    Fixture f = make_fixture();
    DrizzleOperator op;
    DrzError e = DrizzleOperator::build(f.n_src, f.n_dst, f.sources, f.pixfrac,
                                        NormalizationKind::drop_area, f.overlaps, op);
    check(e == DrzError::ok, "negative fixture build ok");
    if (e != DrzError::ok) return;
    const double* x = f.x.data();
    const double* v = f.v.data();
    std::vector<double> good_s = op_signal(op, f.x);
    std::vector<double> good_v = op_variance(op, f.v);
    MutantClaims m = make_mutants(op, f);

    // 正控制 + M-D3（S_p=F_p，漏 D_p）
    check_gate(gate_sb_definition(op, x, good_s), true, "M-D3 control (correct S_p)");
    check_gate(gate_sb_definition(op, x, m.signal_missing_D), false, "M-D3 S_p=F_p red");
    check_gate(gate_sb_definition(op, x, m.signal_legacy), false,
               "M-D1/D3 coverage-area denominator red");

    // M-D2（方差漏 D^2 / 漏平方）
    check_gate(gate_variance_identity(op, v, good_v), true, "M-D2 control (correct variance)");
    check_gate(gate_variance_identity(op, v, m.variance_missing_D2), false,
               "M-D2 variance missing D^2 red");
    check_gate(gate_variance_identity(op, v, m.variance_no_square), false,
               "M-D2 variance missing square red");

    // M-D7（缩放律被破坏）
    {
        const double alpha = 3.0;
        std::vector<double> v2(f.v.size());
        for (size_t i = 0; i < f.v.size(); ++i) v2[i] = alpha * alpha * f.v[i];
        std::vector<double> good_scaled = op_variance(op, v2);
        check_gate(gate_variance_scale_law(op, v, alpha, good_scaled, good_v), true,
                   "M-D7 control (alpha^2)");
        std::vector<double> v1(f.v.size());
        for (size_t i = 0; i < f.v.size(); ++i) v1[i] = alpha * f.v[i];
        std::vector<double> bad_scaled = op_variance(op, v1);
        check_gate(gate_variance_scale_law(op, v, alpha, bad_scaled, good_v), false,
                   "M-D7 scale law broken red");
    }

    // M-D1（常量 ADU 构造）
    {
        std::vector<double> x_const(f.n_src, 7.0);
        check_gate(gate_constant_surface_brightness(op, f.x_sb.data(), f.B0, nullptr), true,
                   "M-D1 control (B0 construction)");
        check_gate(gate_constant_surface_brightness(op, x_const.data(), 7.0, nullptr), false,
                   "M-D1 constant-ADU construction red");
    }

    // M-D5（对角-only 声明精确）
    check_gate(gate_covariance_propagation(op, v, f.aperture_w.data(), false), true,
               "M-D5 control (not claiming exact)");
    check_gate(gate_covariance_propagation(op, v, f.aperture_w.data(), true), false,
               "M-D5 diagonal-exact claim red");

    // M-D4（缺 flux_conservation_factor / pixfrac<1 绝对通量）
    check_gate(gate_flux_conservation(op, x, true, true), true, "M-D4 control (factor present)");
    check_gate(gate_flux_conservation(op, x, false, true), false,
               "M-D4 missing flux_conservation_factor red");
    check_gate(gate_flux_conservation(op, x, false, false), false,
               "M-D4 missing factor (even without absolute claim) red");

    // M-D6（父级对角归约声明精确 / 缺相关核）
    {
        ParentReductionRecord r;
        r.is_lower_bound = true;
        r.claims_exact = true;
        r.has_correlation_kernel_summary = true;
        r.has_deficit_record = true;
        check_gate(gate_parent_reduction(r), false, "M-D6 parent claims exact red");
        ParentReductionRecord r2 = r;
        r2.claims_exact = false;
        check_gate(gate_parent_reduction(r2), true, "M-D6 control (lower bound)");
        ParentReductionRecord r3 = r2;
        r3.is_lower_bound = false;
        check_gate(gate_parent_reduction(r3), false, "M-D6 is_lower_bound=false red");
        ParentReductionRecord r4 = r2;
        r4.has_correlation_kernel_summary = false;
        check_gate(gate_parent_reduction(r4), false, "M-D6 missing correlation kernel red");
    }

    // fail-closed API 负向
    check(validate_pixfrac(0.0) == DrzError::invalid_pixfrac, "pixfrac=0 rejected");
    check(validate_pixfrac(-0.5) == DrzError::invalid_pixfrac, "pixfrac<0 rejected");
    check(validate_pixfrac(1.5) == DrzError::invalid_pixfrac, "pixfrac>1 rejected");
    check(validate_pixfrac(std::nan("")) == DrzError::invalid_pixfrac, "pixfrac NaN rejected");
    check(validate_pixfrac(0.5) == DrzError::ok, "pixfrac=0.5 accepted");
    check(validate_pixfrac(1.0) == DrzError::ok, "pixfrac=1 accepted");
    check(validate_geometry_flags(false, true, true) == DrzError::ring_ordering, "RING rejected");
    check(validate_geometry_flags(true, false, true) == DrzError::multi_channel,
          "multi-channel rejected");
    check(validate_geometry_flags(true, true, false) == DrzError::missing_wcs, "missing WCS rejected");

    // DRZ-FLUX-FIX-01: drop 面积归一即 canonical 口径 —— legacy_a_drop 在 pixfrac<1
    // 不再 fail-closed；两种参数化必须给出**同一个**算子（等价的强断言，非删断言）。
    {
        // 用 fixture 自身 pixfrac (0.7) 构造: 只有该 pixfrac 下几何闭合
        // (Sum_p a_jp = pixfrac^2 * A_pixel_j) 成立, Phi_out 才等于 Sum_j x_j。
        DrizzleOperator o_drop, o_sb;
        const DrzError e2 = DrizzleOperator::build(
            f.n_src, f.n_dst, f.sources, f.pixfrac, NormalizationKind::drop_area,
            f.overlaps, o_drop);
        const DrzError e3 = DrizzleOperator::build(
            f.n_src, f.n_dst, f.sources, f.pixfrac, NormalizationKind::sb_a_pixel,
            f.overlaps, o_sb);
        check(e2 == DrzError::ok, "drop-area normalization at pixfrac<1 accepted");
        check(e3 == DrzError::ok, "sb parametrization at pixfrac<1 accepted");
        // c_jp 在两种参数化下的形状相同 (w/N vs w'/N'), 但浮点运算次序不同
        // (a/A_drop 与 a/A_pixel 不是同一表达式, N 的累加项也不同) ⇒ 只能要求
        // **相对一致** 而非逐位相同; 判据取 1e-12 (双精度 1e-16 量级上留 4 个数量级裕量)。
        double worst_c = 0.0;
        for (uint32_t p = 0; p < o_drop.n_dst(); ++p) {
            for (const OperatorEntry& a : o_drop.row(p)) {
                for (const OperatorEntry& b : o_sb.row(p)) {
                    if (a.src != b.src) continue;
                    const double sc = std::max(std::fabs(a.c), std::fabs(b.c));
                    if (sc == 0.0) continue;
                    worst_c = std::max(worst_c, std::fabs(a.c - b.c) / sc);
                }
            }
        }
        check(worst_c < 1e-12,
              "two parametrizations give the same c_jp (rel < 1e-12)");
        // 通量守恒：Phi_out = Sum_j x_j（与 pixfrac 无关），两种参数化同值。
        double sx = 0.0;
        for (double xv : f.x) sx += xv;
        check(std::fabs(o_drop.flux_out(f.x.data()) - sx) < 1e-9 * std::fabs(sx),
              "Phi_out == Sum_j x_j under drop-area normalization");
        // sb 参数化的核是 a/A_pixel ⇒ Phi_out = pixfrac^2 * Sum_j x_j（同一算子
        // 的不同通量泛函）; 这里断言其精确值, 避免把两种参数化混为一谈。
        const double pf2 = f.pixfrac * f.pixfrac;
        check(std::fabs(o_sb.flux_out(f.x.data()) - pf2 * sx) < 1e-9 * std::fabs(sx),
              "Phi_out == pixfrac^2 * Sum_j x_j under sb parametrization");
        DrizzleOperator o3;
        check(DrizzleOperator::build(f.n_src, f.n_dst, f.sources, 1.0,
                                     NormalizationKind::drop_area, f.overlaps,
                                     o3) == DrzError::ok,
              "drop-area normalization at pixfrac=1 accepted");
    }

    // 几何闭合上溢 -> reject；**亏损侧同样必须 reject**（DRZ-PF-CORRECT-01:
    // 判据取绝对值 —— 面积失效只会使 rel<0，旧判据 rel>tol 对亏损恒为假）。
    {
        double rel = 0.0;
        const double A = f.A_pixel[0];
        const double expect = f.pixfrac * f.pixfrac * A;
        check(validate_overlap_closure(expect, A, f.pixfrac, 1e-6, &rel) == DrzError::ok,
              "closure exact accepted");
        check(validate_overlap_closure(expect * 1.01, A, f.pixfrac, 1e-6, &rel) ==
                  DrzError::overlap_exceeds_drop,
              "closure overflow rejected");
        const DrzError deficit =
            validate_overlap_closure(expect * 0.99, A, f.pixfrac, 1e-6, &rel);
        check(deficit == DrzError::overlap_area_deficit && rel < 0.0,
              "closure deficit rejected with named error (abs(rel) judge)");
        check(validate_overlap_closure(expect * (1.0 - 1e-9), A, f.pixfrac, 1e-6, &rel) ==
                  DrzError::ok,
              "closure deficit inside tolerance accepted (negative control)");
    }

    // A_pixel 非法
    {
        std::vector<OperatorSource> bad = f.sources;
        bad[0].A_pixel = 0.0;
        DrizzleOperator o;
        check(DrizzleOperator::build(f.n_src, f.n_dst, bad, f.pixfrac,
                                     NormalizationKind::drop_area, f.overlaps, o) ==
                  DrzError::invalid_source_area,
              "nonpositive A_pixel rejected");
    }

    // 重复 (src,dst)
    {
        std::vector<OperatorEntry> dup = f.overlaps;
        dup.push_back(dup.front());
        DrizzleOperator o;
        check(DrizzleOperator::build(f.n_src, f.n_dst, f.sources, f.pixfrac,
                                     NormalizationKind::drop_area, dup, o) ==
                  DrzError::invalid_argument,
              "duplicate (src,dst) rejected");
    }

    // 非有限 a_jp
    {
        std::vector<OperatorEntry> nanv = f.overlaps;
        nanv[0].a_jp = std::nan("");
        DrizzleOperator o;
        check(DrizzleOperator::build(f.n_src, f.n_dst, f.sources, f.pixfrac,
                                     NormalizationKind::drop_area, nanv, o) ==
                  DrzError::nonfinite_input,
              "non-finite a_jp rejected");
    }
}

// ---------------------------------------------------------------------------
// 组 4: geometry — 真实球面 overlap（TAN WCS + HEALPix NESTED）
// ---------------------------------------------------------------------------
namespace {

struct TanWcs {
    double crpix_x = 8.5;
    double crpix_y = 8.5;
    double ra0 = 10.0 * 3.14159265358979323846 / 180.0;
    double dec0 = 20.0 * 3.14159265358979323846 / 180.0;
    double scale = 1.0 / 3600.0; // rad/px

    bool eval(double px, double py, double& ra_deg, double& dec_deg) const {
        const double x = (px - crpix_x) * scale;
        const double y = (py - crpix_y) * scale;
        const double rho = std::sqrt(x * x + y * y);
        const double kRad2Deg = 180.0 / 3.14159265358979323846;
        if (rho <= 0.0) {
            ra_deg = ra0 * kRad2Deg;
            dec_deg = dec0 * kRad2Deg;
            return true;
        }
        const double c = std::atan(rho);
        const double sinc = std::sin(c), cosc = std::cos(c);
        const double xx = x * sinc / rho, yy = y * sinc / rho;
        const double sd = std::sin(dec0), cd = std::cos(dec0);
        const double decr = std::asin(cosc * sd + yy * cd);
        const double rar = ra0 + std::atan2(xx, cosc * cd - yy * sd);
        ra_deg = rar * kRad2Deg;
        dec_deg = decr * kRad2Deg;
        return true;
    }
};

bool tan_callback(double px, double py, double& ra, double& dec, void* ud) {
    return static_cast<TanWcs*>(ud)->eval(px, py, ra, dec);
}

} // namespace

static void run_geometry_case(int nside, double pixfrac) {
    TanWcs wcs;
    ::healpix::HealpixCore hp(nside, true);
    std::vector<SourceDropSpec> sources;
    for (double px = 1.5; px <= 7.5; px += 2.0) {
        for (double py = 1.5; py <= 7.5; py += 2.0) {
            SourceDropSpec s;
            s.px = px;
            s.py = py;
            s.pixfrac = pixfrac;
            sources.push_back(s);
        }
    }
    DrizzleOperator op;
    std::vector<uint64_t> target_ipix;
    std::vector<OverlapRow> rows;
    // 几何表示容差：既有球面几何用 4 角大圆弧近似 HEALPix 边界，交叠分割的
    // 相对残差可达 ~1e-3（与冻结常量场门同量级）；公式恒等本身在合成闭合
    // fixture（oracle 组）以 1e-11 证明。
    DrzError e = build_operator_from_sources(hp, sources, tan_callback, &wcs, 1e-3, op,
                                             &target_ipix, &rows);
    const std::string tag = "nside=" + std::to_string(nside) + " pf=" + std::to_string(pixfrac);
    check(e == DrzError::ok, "geometry build ok " + tag);
    if (e != DrzError::ok) {
        std::printf("    DrzError=%s\n", drz_error_name(e));
        return;
    }
    check(op.n_dst() > 0, "geometry targets nonempty " + tag);

    // 几何闭合（结构证据）：|Sum_p a_jp - pixfrac^2 A_pixel| / (pixfrac^2 A_pixel) <= 1e-3
    for (size_t j = 0; j < rows.size(); ++j) {
        check(std::fabs(rows[j].closure_rel) <= 1e-3,
              "overlap closure " + tag + " src " + std::to_string(j));
    }

    // 独立 Oracle（由 raw a_jp 重算，不调用被测实现）
    std::vector<v6_p1_drz_oracle::Item> items;
    std::vector<double> A_pixel(sources.size());
    for (size_t j = 0; j < rows.size(); ++j) {
        A_pixel[j] = rows[j].A_pixel;
        for (const OverlapHit& h : rows[j].hits) {
            items.push_back({(uint32_t)j, 0, h.a_jp});
        }
    }
    // 需要 dst 局部索引：从 op 行反查 target_ipix
    std::vector<uint64_t> local_to_global = target_ipix;
    {
        // 重建 items 的 dst 局部索引（target_ipix 顺序即局部索引）
        items.clear();
        for (size_t j = 0; j < rows.size(); ++j) {
            for (const OverlapHit& h : rows[j].hits) {
                uint32_t dst = 0;
                for (uint32_t t = 0; t < local_to_global.size(); ++t) {
                    if (local_to_global[t] == h.target_ipix) { dst = t; break; }
                }
                items.push_back({(uint32_t)j, dst, h.a_jp});
            }
        }
    }
    v6_p1_drz_oracle::Model om = v6_p1_drz_oracle::Model::build(
        (uint32_t)sources.size(), op.n_dst(), A_pixel, pixfrac, items);

    std::vector<double> x(sources.size()), v(sources.size());
    const double B0 = 3.5;
    for (size_t j = 0; j < sources.size(); ++j) {
        x[j] = B0 * op.A_pixel((uint32_t)j);
        v[j] = 2.0 + 0.5 * (double)j;
    }
    for (uint32_t p = 0; p < op.n_dst(); ++p) {
        check(v6_p1_drz_oracle::rel_close(op.signal_sb(p, x.data()), om.signal(p, x.data()), 1e-11L),
              "geometry S_p vs oracle " + tag + " " + std::to_string(p));
        check(v6_p1_drz_oracle::rel_close(op.variance_sb(p, v.data()), om.variance(p, v.data()), 1e-11L),
              "geometry variance vs oracle " + tag + " " + std::to_string(p));
    }
    std::vector<double> s_impl = op_signal(op, x);
    std::vector<double> var_impl = op_variance(op, v);
    check_gate(gate_sb_definition(op, x.data(), s_impl), true, "geometry gate SB " + tag);
    check_gate(gate_constant_surface_brightness(op, x.data(), B0, &s_impl), true,
               "geometry gate constant SB " + tag);
    check_gate(gate_variance_identity(op, v.data(), var_impl), true,
               "geometry gate variance " + tag);

    // aperture：非负权重 + 共享源 -> 精确 > 对角；对角-only 声明精确必红
    std::vector<double> aw(op.n_dst(), 1.0);
    check_gate(gate_covariance_propagation(op, v.data(), aw.data(), false), true,
               "geometry gate covariance " + tag);
    double diag = 0.0;
    const double exact = op.aperture_variance(aw.data(), v.data(), &diag);
    check(exact > diag, "geometry aperture exact > diag " + tag);
    check_gate(gate_covariance_propagation(op, v.data(), aw.data(), true), false,
               "geometry diagonal-exact claim red " + tag);

    // 条件通量守恒：Phi_out = pixfrac^2 * Sum x（几何闭合残差 <=1e-3 传导，
    // 用冻结常量场门同量级 1e-3；精确恒等在 oracle 组合成闭合 fixture）
    check_gate(gate_flux_conservation(op, x.data(), true, true, 1e-3), true,
               "geometry gate flux conservation " + tag);

    // 父级 deficit 与下界声明
    ParentReductionRecord rec;
    rec.is_lower_bound = true;
    rec.has_correlation_kernel_summary = true;
    rec.has_deficit_record = true;
    rec.deficit = op.parent_deficit(v.data());
    check(std::isfinite(rec.deficit) && rec.deficit >= 0.0 && rec.deficit < 1.0,
          "geometry parent deficit in [0,1) " + tag);
    check_gate(gate_parent_reduction(rec), true, "geometry parent lower-bound " + tag);
    rec.claims_exact = true;
    check_gate(gate_parent_reduction(rec), false, "geometry parent exact red " + tag);
}

// ---------------------------------------------------------------------------
// 组 4b: 面积失效不得静默吞掉 (DRZ-PF-CORRECT-01 / S1 第 19 条)
//
// 负例注入: ASTROCS_V6_DRZ_FAULT=invalid_area 把**一个**候选 target 的交叠面积
// 置为 NaN。修复前该分支是裸 continue —— sum_a_jp 偏小、闭合亏损 (rel<0)，而
// 闭合判据只判 rel>tol ⇒ 面积亏损静默进产品，本测试无从察觉。
// 修复后要求: ① 计数 n_area_rejected 上升; ② 算子构建以具名错误
// overlap_area_invalid 失败（回传非零）。因此本门在修复前必红。
// ---------------------------------------------------------------------------
static void run_geometry_area_invalid_injection() {
    std::printf("[geometry/area-invalid]\n");
    TanWcs wcs;
    ::healpix::HealpixCore hp(4096, true);
    std::vector<SourceDropSpec> sources;
    SourceDropSpec s;
    s.px = 8.5;
    s.py = 8.5;
    s.pixfrac = 0.5;
    sources.push_back(s);

    DrizzleOperator op;
    std::vector<uint64_t> target_ipix;
    OverlapDiagnostics diag;

    // 正例控制: 无注入 ⇒ 构建成功且零失效
    set_env("ASTROCS_V6_DRZ_FAULT", "");
    DrzError e0 = build_operator_from_sources(hp, sources, tan_callback, &wcs, 1e-3, op,
                                              &target_ipix, nullptr, &diag);
    check(e0 == DrzError::ok, "area-invalid positive control: build ok without injection");
    check(diag.n_area_rejected == 0, "area-invalid positive control: 0 area-rejected");
    const std::size_t n_hits_clean = (std::size_t)op.n_dst();
    check(n_hits_clean > 0, "area-invalid positive control: targets nonempty");

    // 负例: 注入一个面积无效像素 ⇒ 必须计数 + 具名失败
    set_env("ASTROCS_V6_DRZ_FAULT", "invalid_area");
    DrizzleOperator op_bad;
    OverlapDiagnostics diag_bad;
    const DrzError e1 = build_operator_from_sources(hp, sources, tan_callback, &wcs, 1e-3,
                                                    op_bad, nullptr, nullptr, &diag_bad);
    set_env("ASTROCS_V6_DRZ_FAULT", "");
    check(e1 == DrzError::overlap_area_invalid,
          std::string("area-invalid injected -> named failure (got ") +
              drz_error_name(e1) + ")");
    check(diag_bad.n_area_rejected > 0,
          "area-invalid injected -> rejected count visible (product provenance)");

    // 单行视图: 同一注入下 OverlapRow 的计数与具名失败
    set_env("ASTROCS_V6_DRZ_FAULT", "invalid_area");
    OverlapRow row;
    const DrzError e2 = compute_overlap_row(hp, s, tan_callback, &wcs, 1e-3, &row);
    set_env("ASTROCS_V6_DRZ_FAULT", "");
    check(e2 == DrzError::overlap_area_invalid,
          std::string("area-invalid row-level named failure (got ") +
              drz_error_name(e2) + ")");
    check(row.n_area_rejected == 1, "area-invalid row-level count == 1");
}

static void run_geometry() {
    std::printf("[geometry]\n");
    run_geometry_case(4096, 1.0);
    run_geometry_case(4096, 0.5);
    run_geometry_case(2048, 1.0);
    run_geometry_area_invalid_injection();
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    const std::string group = (argc > 1) ? argv[1] : "all";
    if (group == "all" || group == "units") run_units();
    if (group == "all" || group == "oracle") run_oracle();
    if (group == "all" || group == "negative") run_negative();
    if (group == "all" || group == "geometry") run_geometry();

    std::printf("[%s] pass=%d fail=%d\n", group.c_str(), g_pass, g_fail);
    if (g_pass == 0) {
        std::printf("[FAIL] zero executed cases\n");
        return 2;
    }
    return g_fail == 0 ? 0 : 1;
}
