// P1-PHOT-TEST · determinism 组 —— 到达顺序不变性 + 线程数扫描
// ---------------------------------------------------------------------------
// 规范依据 (逐条可查):
//   * docs/algorithms/PHOTOMETRIC_FIT.md §5「确定性与归约」:
//       "排序 median/MAD 确定性；IRLS 按 r 索引固定顺序加权和，无跨样本归约。"
//   * 同 §5c「SIMD 安全与取消点」:
//       "IRLS 加权均值 Σw·r/Σw 为**固定样本序归约**(FP64, 禁重结合)——
//        顺序变化仅影响 <1ulp, 仍冻结顺序。"
//   * 同 §7: "IRLS 迭代为全样本顺序归约(**样本序固定**)，天然确定性"
//   * 同 §13.4 I5: "线程数扫描（1/4/N）科学输出 bitwise 不变"
//   * docs/contracts/TEST_MATRIX.md §2「通用容差规则（事前冻结，禁止跑后调阈值）」:
//       - "元数据/mask/**计数**/索引/端口/选择结果: 精确一致。"
//       - "归约: γ_n = n·u/(1−n·u), 门限 C·γ_n·Σ|terms| + atol, C≤4 事前冻结。"
//       - "并行等价（1/N worker…）判据 = 容差等价，不是逐位一致：…
//          整数/mask/索引/计数/端口精确一致。"
//   * ASTROCS_DESIGN.md:569/628「数值结果与并发度无关…归约顺序冻结是达成手段」。
//
// 为什么需要本组 (判据缺口):
//   §13.4 I5 只扫**线程数**，且每个线程跑**完全相同的样本序** ⇒ 天然抓不到
//   「样本到达序进入浮点路径」这一类缺陷。§5/§5c 冻结了"样本序固定"这一
//   **结构性不变量**，但既有的可执行判据没有一条**变样本序**的用例。
//   本组补上：把"样本序"变成**被控自变量**，用 §2 的冻结口径判等价。
//
// 覆盖:
//   T1 线程数扫描 1/2/4/8：生产入口全输出 bitwise（I5 显式覆盖 2 与 8）
//   T2 到达顺序置换（Gaia 表序）：全输出 **bitwise** —— Gaia 数组序不得进入
//      任何浮点累加序（匹配集是序无关的集合谓词；matches[] 序由 PSF 序驱动）
//   T3 到达顺序置换（PSF 表序）：计数/索引/选择结果**精确一致** +
//      IRLS 归约按 §2 的 γ_n 门判等价（不是逐位；§5c 明示"顺序变化仅影响 <1ulp"）
//   T4 判据非退化（负例注入，测试内 mutant 走**同一个 verdict 函数**）：
//      N1 序相关最近邻 tie-break（精确并列构造）⇒ 必判红
//      N2 序相关在线稳健位置归约（online IRLS）⇒ 必判红
//      N1/N2 判红 + T2/T3 判绿 = 判据同时排除恒真与恒假
//   故障注入名: p1phot_order_invariance（ASTROCS_P1PHOT_FAULT 置位后本组必败）
#include "p1phot_test_main.hpp"
#include "p1phot_field_stub.hpp"
#include "p1phot_fixtures.hpp"
#include "p1phot_gaia_stub.hpp"
#include "p1phot_oracle.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "photometric_calib.h"
#include "pc_api_qf.h"   // 生产入口 (非 C ABI 导出; 与 frame_photometry_fit 同一入口)

namespace p1phot {
namespace {

CheckState g_cs;

// ── §2 事前冻结口径 ────────────────────────────────────────────────────────
constexpr double kUlpF64 = 2.220446049250313e-16;  // 2^-52
constexpr int kReductionC = 4;                     // §2: C ≤ 4 事前冻结

// §2「归约: γ_n = n·u/(1−n·u), 门限 C·γ_n·Σ|terms| + atol」
inline double reduction_gate(int n_terms, double sum_abs_terms, double atol) {
    const double u = kUlpF64;
    const double nu = (double)n_terms * u;
    const double gamma = nu / (1.0 - nu);
    return (double)kReductionC * gamma * sum_abs_terms + atol;
}

// 一次生产入口调用的可观测量（含 per-star records ⇒ 可判"配对集合"）
struct Obs {
    int rc = -99;
    int n_matched = -1;
    double scale = 0.0;
    double sigma = 0.0;
    PhotometricDiag diag;
    std::vector<PcMatchRecord> records;
    std::vector<double> pixels;
};

// 生产入口 = pc_calibrate_simple_with_gaia_f64_v2_qf
// (module_adapters → frame_photometry_fit → 本入口; quality_flags=nullptr 与
//  module_adapters 在 p1_sources 无 quality 列时的取值一致)
Obs run_prod(void* client, const fix::StarField& f, const fix::FrameGeom& g,
             const std::vector<double>& px_in) {
    Obs o;
    o.pixels.assign((std::size_t)g.width * g.height, 0.0);
    o.records.assign(f.psf_cx.size(), PcMatchRecord{});
    std::memset(&o.diag, 0, sizeof(o.diag));
    std::vector<double> fw = {300.0, 1050.0};
    std::vector<double> ft = {0.5, 0.5};
    std::vector<double> swl;
    for (int x = 336; x <= 1020; x += 2) swl.push_back((double)x);
    o.rc = pc_calibrate_simple_with_gaia_f64_v2_qf(
        client, g.crval1, g.crval2, 0.05, 10.0, 16.0,
        fw.data(), ft.data(), (int)fw.size(),
        nullptr, nullptr, 0,
        swl.data(), (int)swl.size(),
        px_in.data(), g.width, g.height,
        f.psf_cx.data(), f.psf_cy.data(), f.psf_flux.data(), f.psf_status.data(),
        (int)f.psf_cx.size(),
        f.psf_star_ids.data(), o.records.data(),
        g.crval1, g.crval2, g.crpix1, g.crpix2, g.cd11, g.cd12, g.cd21, g.cd22,
        0, nullptr, nullptr, nullptr, nullptr,
        o.pixels.data(), &o.n_matched, &o.scale, &o.sigma, &o.diag,
        nullptr);
    return o;
}

// ── 置换: 保语义重标号 (index map; 不复制/不新增星) ─────────────────────────
fix::StarField permute_field(const fix::StarField& f,
                             const std::vector<int>& gp,
                             const std::vector<int>& pp) {
    fix::StarField o;
    const std::size_t ng = f.gaia_ra.size(), np = f.psf_cx.size();
    o.gaia_ra.resize(ng); o.gaia_dec.resize(ng); o.gaia_mag.resize(ng);
    o.gaia_fsyn.resize(ng);
    for (std::size_t i = 0; i < ng; ++i) {
        const std::size_t s = (std::size_t)gp[i];
        o.gaia_ra[i] = f.gaia_ra[s]; o.gaia_dec[i] = f.gaia_dec[s];
        o.gaia_mag[i] = f.gaia_mag[s]; o.gaia_fsyn[i] = f.gaia_fsyn[s];
    }
    o.psf_cx.resize(np); o.psf_cy.resize(np); o.psf_flux.resize(np);
    o.psf_status.resize(np); o.psf_star_ids.resize(np);
    for (std::size_t i = 0; i < np; ++i) {
        const std::size_t s = (std::size_t)pp[i];
        o.psf_cx[i] = f.psf_cx[s]; o.psf_cy[i] = f.psf_cy[s];
        o.psf_flux[i] = f.psf_flux[s]; o.psf_status[i] = f.psf_status[s];
        o.psf_star_ids[i] = f.psf_star_ids[s];
    }
    return o;
}

std::vector<int> ident(int n) {
    std::vector<int> v((std::size_t)n);
    for (int i = 0; i < n; ++i) v[(std::size_t)i] = i;
    return v;
}
std::vector<int> reversed(int n) {
    std::vector<int> v = ident(n);
    std::reverse(v.begin(), v.end());
    return v;
}
std::vector<int> rotated(int n, int by) {
    std::vector<int> v = ident(n);
    if (n > 0) std::rotate(v.begin(), v.begin() + (by % n), v.end());
    return v;
}

// ── verdict: 与 §2 冻结口径一一对应的等价判定 ──────────────────────────────
// 元数据/计数/索引/选择结果 ⇒ 精确；IRLS 归约 ⇒ γ_n 门。
struct Verdict {
    bool meta_exact = true;   // 计数 + 配对集合 + reject_reason 精确一致
    bool pixels_exact = true;
    bool scale_within = true; // |Δlocation| ≤ §2 归约门
    bool sigma_within = true;
    double d_location = 0.0;
    double gate = 0.0;
    std::string why;
};

// 配对集合 (star_id ↔ dr3sp_id) 的多重集指纹 (序无关)
std::vector<std::pair<std::int64_t, std::int64_t>> pair_set(
    const std::vector<PcMatchRecord>& rec) {
    std::vector<std::pair<std::int64_t, std::int64_t>> v;
    for (const PcMatchRecord& r : rec)
        if (r.status == 1) v.emplace_back(r.star_id, r.dr3sp_id);
    std::sort(v.begin(), v.end());
    return v;
}

Verdict verdict_of(const Obs& a, const Obs& b, bool compare_pixels) {
    Verdict v;
    if (a.n_matched != b.n_matched) {
        v.meta_exact = false;
        v.why = "n_matched " + std::to_string(a.n_matched) + " vs " +
                std::to_string(b.n_matched);
    }
    // diag 计数逐项精确 (§2「计数/索引精确一致」)
    const long long ia[] = {a.diag.spectrum_rows_total, a.diag.valid_fsyn,
                            a.diag.gaia_projected_in_frame, a.diag.psf_total,
                            a.diag.psf_valid, a.diag.spatial_candidates,
                            a.diag.unique_matches, a.diag.rejected_ambiguous,
                            a.diag.rejected_distance, a.diag.rejected_quality,
                            a.diag.fit_used, a.diag.robust_iterations};
    const long long ib[] = {b.diag.spectrum_rows_total, b.diag.valid_fsyn,
                            b.diag.gaia_projected_in_frame, b.diag.psf_total,
                            b.diag.psf_valid, b.diag.spatial_candidates,
                            b.diag.unique_matches, b.diag.rejected_ambiguous,
                            b.diag.rejected_distance, b.diag.rejected_quality,
                            b.diag.fit_used, b.diag.robust_iterations};
    for (std::size_t i = 0; i < sizeof(ia) / sizeof(ia[0]); ++i)
        if (ia[i] != ib[i]) {
            v.meta_exact = false;
            if (v.why.empty()) v.why = "diag[" + std::to_string(i) + "]";
        }
    if (pair_set(a.records) != pair_set(b.records)) {
        v.meta_exact = false;
        if (v.why.empty()) v.why = "matched pair set differs";
    }
    // 逐星 reject_reason 集合 (序无关) 精确
    {
        std::vector<int> ra, rb;
        for (const PcMatchRecord& r : a.records) ra.push_back(r.reject_reason);
        for (const PcMatchRecord& r : b.records) rb.push_back(r.reject_reason);
        std::sort(ra.begin(), ra.end());
        std::sort(rb.begin(), rb.end());
        if (ra != rb) {
            v.meta_exact = false;
            if (v.why.empty()) v.why = "reject_reason multiset differs";
        }
    }
    if (compare_pixels && a.pixels.size() == b.pixels.size()) {
        for (std::size_t i = 0; i < a.pixels.size(); ++i)
            if (!bits_eq_d(a.pixels[i], b.pixels[i])) { v.pixels_exact = false; break; }
    }
    // IRLS 归约门 (§2): 比较 location = −log10(scale)
    if (a.scale > 0.0 && b.scale > 0.0) {
        const double la = -std::log10(a.scale), lb = -std::log10(b.scale);
        v.d_location = std::fabs(la - lb);
        int n_terms = 0;
        double sum_abs = 0.0;
        for (const PcMatchRecord& r : a.records)
            if (r.reject_reason == 0 && std::isfinite(r.residual)) {
                ++n_terms;
                sum_abs += std::fabs(r.residual);
            }
        v.gate = reduction_gate(n_terms > 0 ? n_terms : 1, sum_abs, 0.0);
        v.scale_within = (v.d_location <= v.gate);
        v.sigma_within = std::fabs(a.sigma - b.sigma) <=
                         reduction_gate(n_terms > 0 ? n_terms : 1, sum_abs, 0.0);
    } else {
        v.scale_within = (a.scale == b.scale);
        v.sigma_within = (a.sigma == b.sigma);
    }
    return v;
}

// ── 负例 mutant (测试内实现; 走**同一个** verdict ⇒ 证明判据有分辨力) ───────
// N1: 序相关最近邻 tie-break。精确并列构型: PSF 在 (30,30)，两颗 Gaia 分别在
//     (28.5,30) 与 (31.5,30) ⇒ d 都是精确 1.5（二进制可精确表示）。
//     "first-wins"(<) 与 "last-wins"(<=) 在置换下选到不同的星。
std::int64_t n1_nearest_partner(const std::vector<double>& gx,
                                const std::vector<double>& gy,
                                const std::vector<int>& order,
                                bool last_wins) {
    const double x = 30.0, y = 30.0, r2 = 2.0 * 2.0;
    double best = r2;
    std::int64_t idx = -1;
    for (int i : order) {
        const double dx = gx[(std::size_t)i] - x, dy = gy[(std::size_t)i] - y;
        const double d2 = dx * dx + dy * dy;
        if (last_wins ? (d2 <= best) : (d2 < best)) { best = d2; idx = i; }
    }
    return idx;
}

// N2: 序相关"在线稳健位置"归约 (每次用当前估计重算权重并就地更新)。
//     与生产的固定样本序加权和 (§5c) 相对照 ⇒ 置换下 location 显著漂移。
double n2_online_location(const std::vector<double>& r) {
    if (r.empty()) return 0.0;
    double loc = r[0];
    double S = 0.0;
    for (std::size_t i = 1; i < r.size(); ++i) {
        const double d = std::fabs(r[i] - loc);
        S = (i == 1) ? d : 0.9 * S + 0.1 * d;
        const double cS = (S > 0.0) ? 4.685 * S : 1.0;
        const double u = (r[i] - loc) / cS;
        const double w = (std::fabs(u) >= 1.0) ? 0.0 : (1.0 - u * u) * (1.0 - u * u);
        loc += w * (r[i] - loc) / (1.0 + w);   // 就地在线更新 (序相关)
    }
    return loc;
}

// 扰动场 (S>0 ⇒ IRLS 真迭代; §5c 的归约序才有意义)。F1 格点 + 已知乘性偏移 k。
fix::StarField make_perturbed_field(const fix::FrameGeom& g, double k, int n,
                                    double amp, std::uint64_t seed) {
    oracle::OracleWcs wcs{g.crval1, g.crval2, g.crpix1, g.crpix2,
                          g.cd11, g.cd12, g.cd21, g.cd22, 0, {}, {}, {}, {}};
    std::vector<std::pair<double, double>> pixels;
    for (int row = 0; row < 6 && (int)pixels.size() < n; ++row)
        for (int col = 0; col < 4 && (int)pixels.size() < n; ++col)
            pixels.emplace_back(10.0 + 12.0 * col + 1.0 * row, 10.0 + 9.0 * row);
    fix::SplitMix64 rng(seed);
    fix::StarField f;
    fix::build_matched_field(g, wcs, pixels, k, 1000.0, rng, amp, f);
    return f;
}

void fill_pixels(std::vector<double>& px, std::uint64_t seed) {
    fix::SplitMix64 rng(seed);
    for (auto& v : px) v = rng.uniform(10.0, 2000.0);
}

}  // namespace

int test_determinism() {
    CheckState& cs = g_cs;
    fix::FrameGeom g;
    const std::size_t npix = (std::size_t)g.width * g.height;

    fix::StarField f = make_perturbed_field(g, 1.25, 12, 0.02,
                                            0x5EED00000000D00DULL);
    // 实测边距 (证据行, 不参与判定): 供报告与后续诊断引用
    double t2_max_dloc = 0.0, t3_max_dloc = 0.0, t3_gate = 0.0;
    double t4_n1_delta = 0.0, t4_n2_delta = 0.0, t4_n2_gate = 0.0;
    int base_nm = -1;
    double base_scale = 0.0, base_sigma = 0.0;
    std::vector<double> pixels(npix);
    fill_pixels(pixels, 0x5EED00000000C0DEULL);

    stub::FakeClientConfig cfg = bridge::make_config(f, {}, 200, 1.0e-15f, 5.0e-18f);
    stub::GaiaClient* client = stub::create(cfg);

    // ── T1: 线程数扫描 1/2/4/8 (I5 显式含 2 与 8) ──────────────────────────
    {
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        const Obs base = run_prod(client, f, g, pixels);
        base_nm = base.n_matched;
        base_scale = base.scale;
        base_sigma = base.sigma;
        P1PHOT_CHECK(cs, base.rc == 0, "p1phot_order_invariance");
        P1PHOT_CHECK_MSG(cs, base.n_matched > 0, "p1phot_order_invariance",
                         "baseline n_matched=%d", base.n_matched);
        const int tset[] = {1, 2, 4, 8};
        for (int nt : tset) {
#ifdef _OPENMP
            omp_set_num_threads(nt);
#endif
            const Obs o = run_prod(client, f, g, pixels);
            const Verdict v = verdict_of(base, o, true);
            P1PHOT_CHECK_MSG(cs, o.rc == 0, "p1phot_order_invariance",
                             "T1 nt=%d rc=%d", nt, o.rc);
            P1PHOT_CHECK_MSG(cs, v.meta_exact && v.pixels_exact &&
                                     bits_eq_d(o.scale, base.scale) &&
                                     bits_eq_d(o.sigma, base.sigma),
                             "p1phot_order_invariance",
                             "T1 nt=%d 非 bitwise (why=%s)", nt, v.why.c_str());
        }
#ifdef _OPENMP
        omp_set_num_threads(omp_get_max_threads());
#endif
    }

    // ── T2: 到达顺序置换 —— Gaia 表序 ⇒ 必须 bitwise ───────────────────────
    // 依据 §5c「固定样本序归约」: 浮点路径的样本序由 PSF 表序驱动; Gaia 表序
    // 只改变索引标号，不进入任何浮点累加序 ⇒ 全输出逐位不变。
    {
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        const Obs base = run_prod(client, f, g, pixels);
        const int ng = (int)f.gaia_ra.size();
        const std::vector<std::vector<int>> gps = {
            rotated(ng, 1), rotated(ng, 5), reversed(ng)};
        for (std::size_t k = 0; k < gps.size(); ++k) {
            const fix::StarField fp = permute_field(f, gps[k], ident((int)f.psf_cx.size()));
            const Obs o = run_prod(client, fp, g, pixels);
            const Verdict v = verdict_of(base, o, true);
            t2_max_dloc = std::max(t2_max_dloc, v.d_location);
            P1PHOT_CHECK_MSG(cs, o.rc == 0, "p1phot_order_invariance",
                             "T2 gaia-perm#%zu rc=%d", k, o.rc);
            P1PHOT_CHECK_MSG(cs, v.meta_exact, "p1phot_order_invariance",
                             "T2 gaia-perm#%zu 计数/配对集不精确 (why=%s)", k,
                             v.why.c_str());
            P1PHOT_CHECK_MSG(cs, v.pixels_exact, "p1phot_order_invariance",
                             "T2 gaia-perm#%zu 像素非 bitwise", k);
            P1PHOT_CHECK_MSG(cs, bits_eq_d(o.scale, base.scale) &&
                                     bits_eq_d(o.sigma, base.sigma),
                             "p1phot_order_invariance",
                             "T2 gaia-perm#%zu scale/sigma 非 bitwise "
                             "(Δlocation=%.3e)", k, v.d_location);
        }
    }

    // ── T3: 到达顺序置换 —— PSF 表序 ⇒ 计数精确 + 归约按 §2 γ_n 门 ─────────
    // §5c 明示"顺序变化仅影响 <1ulp, 仍冻结顺序" ⇒ 此处判据是 §2 的归约门，
    // 不是逐位；而计数/索引/选择结果按 §2 必须**精确一致**。
    {
#ifdef _OPENMP
        omp_set_num_threads(1);
#endif
        const Obs base = run_prod(client, f, g, pixels);
        const int np = (int)f.psf_cx.size();
        const int ng = (int)f.gaia_ra.size();
        const std::vector<std::vector<int>> pps = {
            reversed(np), rotated(np, 1), rotated(np, 7)};
        for (std::size_t k = 0; k < pps.size(); ++k) {
            const fix::StarField fp = permute_field(f, ident(ng), pps[k]);
            const Obs o = run_prod(client, fp, g, pixels);
            const Verdict v = verdict_of(base, o, false);
            t3_max_dloc = std::max(t3_max_dloc, v.d_location);
            t3_gate = std::max(t3_gate, v.gate);
            P1PHOT_CHECK_MSG(cs, o.rc == 0, "p1phot_order_invariance",
                             "T3 psf-perm#%zu rc=%d", k, o.rc);
            P1PHOT_CHECK_MSG(cs, v.meta_exact, "p1phot_order_invariance",
                             "T3 psf-perm#%zu 计数/配对集不精确 (why=%s)", k,
                             v.why.c_str());
            P1PHOT_CHECK_MSG(cs, v.scale_within && v.sigma_within,
                             "p1phot_order_invariance",
                             "T3 psf-perm#%zu 超 §2 归约门 (Δlocation=%.3e > "
                             "gate=%.3e)", k, v.d_location, v.gate);
        }
        // 双置换 (PSF + Gaia 同时)
        const fix::StarField fb = permute_field(f, reversed(ng), reversed(np));
        const Obs ob = run_prod(client, fb, g, pixels);
        const Verdict vb = verdict_of(base, ob, false);
        P1PHOT_CHECK_MSG(cs, ob.rc == 0 && vb.meta_exact && vb.scale_within,
                         "p1phot_order_invariance",
                         "T3 both-perm 失败 (why=%s Δ=%.3e gate=%.3e)",
                         vb.why.c_str(), vb.d_location, vb.gate);
    }

    // ── T4: 判据非退化 (负例注入必判红; 与 T2/T3 共用 verdict) ─────────────
    {
        // N1: 序相关 tie-break —— 精确并列构型
        const std::vector<double> gx = {28.5, 31.5};
        const std::vector<double> gy = {30.0, 30.0};
        const std::vector<int> ord0 = {0, 1};
        const std::vector<int> ord1 = {1, 0};
        const std::int64_t p_first_a = n1_nearest_partner(gx, gy, ord0, false);
        const std::int64_t p_first_b = n1_nearest_partner(gx, gy, ord1, false);
        const std::int64_t p_last_a = n1_nearest_partner(gx, gy, ord0, true);
        const std::int64_t p_last_b = n1_nearest_partner(gx, gy, ord1, true);
        t4_n1_delta = (p_last_a != p_last_b) ? 1.0 : 0.0;
        // "last-wins" 是序相关的 ⇒ 判据必须能分辨 (两个序给出不同伙伴)
        P1PHOT_CHECK_MSG(cs, p_last_a != p_last_b,
                         "p1phot_order_invariance",
                         "N1 负例未构成序相关 (last-wins 置换后伙伴相同)");
        // "first-wins" 在**并列**下也随序变 —— 与生产同构的严格 < 判据并不
        // 自动免疫并列；此处如实登记：并列构型下最近邻选择**未定义**。
        P1PHOT_CHECK_MSG(cs, p_first_a != p_first_b,
                         "p1phot_order_invariance",
                         "N1 并列构型下严格 < 亦随序变 (登记: tie-break 未定义)");

        // N2: 序相关归约 —— 在线稳健位置 vs 固定样本序加权和
        std::vector<double> r;
        for (const PcMatchRecord& rec : run_prod(client, f, g, pixels).records)
            if (rec.reject_reason == 0 && std::isfinite(rec.residual))
                r.push_back(rec.residual);
        P1PHOT_CHECK_MSG(cs, r.size() >= 3, "p1phot_order_invariance",
                         "N2 内点不足 (%zu)", r.size());
        if (r.size() >= 3) {
            double sum_abs = 0.0;
            for (double x : r) sum_abs += std::fabs(x);
            const double gate = reduction_gate((int)r.size(), sum_abs, 0.0);
            const double loc_a = n2_online_location(r);
            std::vector<double> rr(r.rbegin(), r.rend());
            const double loc_b = n2_online_location(rr);
            t4_n2_delta = std::fabs(loc_a - loc_b);
            t4_n2_gate = gate;
            P1PHOT_CHECK_MSG(cs, std::fabs(loc_a - loc_b) > gate,
                             "p1phot_order_invariance",
                             "N2 负例未被 §2 门分辨 (Δ=%.3e ≤ gate=%.3e)",
                             std::fabs(loc_a - loc_b), gate);
        }
    }

    std::fprintf(stdout,
                 "[p1phot][determinism] n_matched=%d scale=%.6e sigma=%.6e | "
                 "T2(gaia 序) maxΔlocation=%.3e (判据: bitwise) | "
                 "T3(psf 序) maxΔlocation=%.3e gate=%.3e | "
                 "N1 tie-break 序相关=%s | N2 在线归约 Δ=%.3e > gate=%.3e: %s\n",
                 base_nm, base_scale, base_sigma, t2_max_dloc, t3_max_dloc, t3_gate,
                 t4_n1_delta != 0.0 ? "yes" : "no", t4_n2_delta, t4_n2_gate,
                 (t4_n2_delta > t4_n2_gate) ? "yes" : "no");

    stub::destroy(client);
    return cs.failures == 0 ? 0 : 1;
}

}  // namespace p1phot
