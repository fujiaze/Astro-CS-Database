// eng/tests/unit/v6_p2_upm/v6_p2_upm_ma_test.cpp
//
// IMPL-P2-UPM-001 共址单元测试 — Phase2 UPM 乘法/加性分离
// （ALG-P2S-UPM.1..8；FZ-AP2S-RANK-RTOL / FZ-AP2S-KAPPA-MAX /
//   FZ-AP2S-UPM-MINFRAMES / FZ-PROV-KCORR / FZ-PROV-KCORR-VALUE /
//   FZ-UPM-CONVERGENCE / FZ-FORMULA-COV-PROP）。
//
// 真值来源（不调用被测实现自身作为真值）：
//   - 数值锚值 = 独立 NumPy Oracle（块坐标下降 ALS + scipy trf 交叉复核 +
//     NumPy SVD/eig 重算），随交付脚本：
//       eng/tests/unit/v6_p2_upm/oracle/upm_ma_oracle.py -> anchors.json
//     （跨环境/跨算法独立，非被测实现的输出；可复跑：cd oracle && python3
//      upm_ma_oracle.py）；kappa 锚 = A_exact 73.229900948，
//     F_illcond eps=5e-4 时 kappa=1.1701e8；
//   - 协方差在测试内用独立 Gauss-Jordan 求逆与三重积重算（不同代码路径）；
//   - 精确恢复数据集由已知 (g,s,b) 正向生成。
//
// 用法: v6_p2_upm_ma_test <units|structure|negative|additive|control_variance>
// 返回 0 = 全过；否则 = 失败断言数。

#include "astro/phase2/upm.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what.c_str());
    }
}

void check_near(double got, double want, double rtol, const std::string& what) {
    const double scale = std::max(1.0, std::fabs(want));
    const bool ok = std::isfinite(got) && std::fabs(got - want) <= rtol * scale;
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s  got=%.17g want=%.17g rtol=%g\n", what.c_str(), got, want, rtol);
    }
}

double pi_half() { return 1.57079632679489661923132169163975144209858469968755; }

// ---- 独立线性代数（测试侧，非被测实现）----
bool invert_n(const std::vector<double>& A, int n, std::vector<double>& out) {
    std::vector<double> a = A;
    std::vector<double> inv((std::size_t)n * n, 0.0);
    for (int i = 0; i < n; ++i) inv[(std::size_t)i * n + i] = 1.0;
    for (int col = 0; col < n; ++col) {
        int piv = col;
        for (int r = col + 1; r < n; ++r)
            if (std::fabs(a[(std::size_t)r * n + col]) > std::fabs(a[(std::size_t)piv * n + col]))
                piv = r;
        if (!(std::fabs(a[(std::size_t)piv * n + col]) > 1e-300)) return false;
        if (piv != col) {
            for (int j = 0; j < n; ++j) {
                std::swap(a[(std::size_t)col * n + j], a[(std::size_t)piv * n + j]);
                std::swap(inv[(std::size_t)col * n + j], inv[(std::size_t)piv * n + j]);
            }
        }
        const double pv = a[(std::size_t)col * n + col];
        for (int j = 0; j < n; ++j) {
            a[(std::size_t)col * n + j] /= pv;
            inv[(std::size_t)col * n + j] /= pv;
        }
        for (int r = 0; r < n; ++r) {
            if (r == col) continue;
            const double f = a[(std::size_t)r * n + col];
            if (f == 0.0) continue;
            for (int j = 0; j < n; ++j) {
                a[(std::size_t)r * n + j] -= f * a[(std::size_t)col * n + j];
                inv[(std::size_t)r * n + j] -= f * inv[(std::size_t)col * n + j];
            }
        }
    }
    out = inv;
    return true;
}

// 独立设计矩阵: full = [s(control 升序)] ++ [g(frame 升序)] ++ [b(frame 升序)],
// gauge = 每分量最小 frame_id 的 g/b 固定后按 full 下标升序取 free。
struct Layout {
    std::vector<std::uint64_t> frames;
    std::vector<std::uint64_t> controls;
    std::vector<std::size_t> free_full;    // free -> full
    std::vector<long long> full_free;      // full -> free (-1 固定)
};

std::size_t idx_of(const std::vector<std::uint64_t>& v, std::uint64_t x) {
    for (std::size_t i = 0; i < v.size(); ++i)
        if (v[i] == x) return i;
    return v.size();
}

Layout make_layout(std::vector<std::uint64_t> frames, std::vector<std::uint64_t> controls,
                   std::vector<std::uint64_t> refs) {
    Layout L;
    std::sort(frames.begin(), frames.end());
    std::sort(controls.begin(), controls.end());
    L.frames = frames;
    L.controls = controls;
    const std::size_t nf = frames.size(), np = controls.size();
    L.full_free.assign(np + 2 * nf, -1);
    for (std::uint64_t rf : refs) {
        const std::size_t fi = idx_of(frames, rf);
        L.full_free[np + fi] = -2;
        L.full_free[np + nf + fi] = -2;
    }
    long long k = 0;
    for (std::size_t j = 0; j < L.full_free.size(); ++j)
        if (L.full_free[j] == -1) {
            L.full_free[j] = k++;
            L.free_full.push_back(j);
        }
    return L;
}

struct Obs {
    std::uint64_t frame;
    std::uint64_t control;
    double y;
    double ivar;
};

// 从解重建 J（观测模型 Jacobian）；W=diag(ivar)。
std::vector<double> build_J(const Layout& L, const std::vector<Obs>& obs,
                            const std::vector<double>& theta_full) {
    const std::size_t nf = L.frames.size(), np = L.controls.size();
    std::vector<double> J(obs.size() * L.free_full.size(), 0.0);
    for (std::size_t i = 0; i < obs.size(); ++i) {
        const std::size_t fi = idx_of(L.frames, obs[i].frame);
        const std::size_t ci = idx_of(L.controls, obs[i].control);
        const double g = theta_full[np + fi];
        const double s = theta_full[ci];
        const long long cols[3] = {L.full_free[ci], L.full_free[np + fi],
                                   L.full_free[np + nf + fi]};
        const double vals[3] = {g, s, 1.0};
        for (int c = 0; c < 3; ++c)
            if (cols[c] >= 0) J[i * L.free_full.size() + (std::size_t)cols[c]] = vals[c];
    }
    return J;
}

std::vector<double> normal_matrix(const Layout& L, const std::vector<Obs>& obs,
                                  const std::vector<double>& theta_full) {
    const std::size_t n = L.free_full.size();
    const std::vector<double> J = build_J(L, obs, theta_full);
    std::vector<double> H(n * n, 0.0);
    for (std::size_t i = 0; i < obs.size(); ++i)
        for (std::size_t a = 0; a < n; ++a)
            for (std::size_t b = 0; b < n; ++b)
                H[a * n + b] += obs[i].ivar * J[i * n + a] * J[i * n + b];
    return H;
}

// ---- 数据集（与 oracle 镜像）----
std::vector<Obs> ds_exact() {
    const std::uint64_t F[3] = {10, 20, 30};
    const std::uint64_t C[3] = {100, 200, 300};
    const double st[3] = {10.0, 20.0, 40.0};
    const double gt[3] = {1.0, 0.9, 1.1};
    const double bt[3] = {0.0, 2.5, -1.5};
    std::vector<Obs> v;
    for (int k = 0; k < 3; ++k)
        for (int p = 0; p < 3; ++p) v.push_back({F[k], C[p], gt[k] * st[p] + bt[k], 1.0});
    return v;
}

std::vector<Obs> ds_disconnected() {
    const std::uint64_t F[4] = {1, 2, 101, 102};
    const std::uint64_t C[4] = {11, 12, 111, 112};
    const double st[4] = {3.0, 5.0, 7.0, 9.0};
    const double gt[4] = {1.0, 1.5, 1.0, 0.5};
    const double bt[4] = {0.0, 4.0, 0.0, -2.0};
    std::vector<Obs> v;
    for (int g = 0; g < 2; ++g)
        for (int k = 0; k < 2; ++k)
            for (int p = 0; p < 2; ++p)
                v.push_back({F[2 * g + k], C[2 * g + p], gt[2 * g + k] * st[2 * g + p] + bt[2 * g + k], 1.0});
    return v;
}

std::vector<Obs> ds_constant_s() {
    std::vector<Obs> v;
    for (std::uint64_t c : {11u, 12u}) {
        v.push_back({1, c, 5.0, 1.0});
        v.push_back({2, c, 0.7 * 5.0 + 3.0, 1.0});
    }
    return v;
}

std::vector<Obs> ds_minframes() {
    return {{7, 71, 4.0, 1.0}, {7, 72, 8.0, 1.0}};
}

std::vector<Obs> ds_illcond(double eps) {
    const double s0 = 1.0, s1 = 1.0 + eps;
    return {{1, 11, s0, 1.0}, {1, 12, s1, 1.0},
            {2, 11, 0.7 * s0 + 3.0, 1.0}, {2, 12, 0.7 * s1 + 3.0, 1.0}};
}

std::vector<P2UpmMaObservation> to_api(const std::vector<Obs>& v) {
    std::vector<P2UpmMaObservation> o(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) {
        o[i].frame_id = v[i].frame;
        o[i].control_id = v[i].control;
        o[i].value = v[i].y;
        o[i].control_ivar = v[i].ivar;
    }
    return o;
}

P2UpmMaConfig zero_cfg() {
    P2UpmMaConfig c;
    std::memset(&c, 0, sizeof(c));
    return c;
}

// ===========================================================================
// units: 数值锚（Oracle 独立）+ 精确恢复 + 参数协方差 + 确定性
// ===========================================================================
int run_units() {
    const std::vector<Obs> obs = ds_exact();
    const std::vector<P2UpmMaObservation> api = to_api(obs);
    void* model = nullptr;
    const int rc = p2_upm_ma_build(api.data(), api.size(), nullptr, &model);
    check(rc == 0, "units: build rc==0");
    if (rc != 0) return g_failures;

    P2UpmMaInfo info;
    check(p2_upm_ma_info(model, &info) == 0, "units: info rc==0");
    check(info.n_frames == 3 && info.n_controls == 3, "units: F=3 P=3");
    check(info.n_components == 1, "units: n_components==1");
    check(info.n_params == 7, "units: n_params==7 (3+6-2)");
    check(info.rank == 7, "units: rank==n_params (Oracle A)");
    check(info.min_frames == 2, "units: min_frames==2 (FZ-AP2S-UPM-MINFRAMES)");
    check(info.gauge_mode == 0, "units: gauge_mode==min_frame_id");
    // UPM-KAPPA-UNIFY-01：判决位只有一个 —— identifiable ⟺ r_eff == n ⟺ κ < 1/τ。
    // 已退休：kappa_max 绝对常数（FZ-AP2S-KAPPA-MAX=1e6）与「rank 用 σ(J)、κ 用 λ(H)」
    // 的同函数内两把尺（相差 10 个数量级）。
    check(info.identifiable == 1, "units: identifiable==1 (the single verdict bit)");
    check(info.rank == info.n_params && info.n_unidentified == 0,
          "units: r_eff==n_params, 0 unidentified directions");
    check(info.kappa < 1.0 / info.rank_rtol_effective,
          "units: verdict == (kappa < 1/tau), tau=" + std::to_string(info.rank_rtol_effective));
    check(info.rank_rtol_effective >= info.rank_rtol,
          "units: tau_eff >= requested tau (precision floor may tighten it)");

    // Oracle A: g/b/s 精确恢复（ALS+scipy 交叉复核，maxdiff 3.4e-14）
    double g = 0, b = 0, s = 0;
    check(p2_upm_ma_solution(model, 10, 100, &g, &b, &s) == 0, "units: solution(10,100)");
    check_near(g, 1.0, 1e-9, "units: g(10)==1 (gauge)");
    check_near(b, 0.0, 1e-9, "units: b(10)==0 (gauge)");
    check_near(s, 10.0, 1e-9, "units: s(100)==10 (Oracle)");
    check(p2_upm_ma_solution(model, 20, 200, &g, &b, &s) == 0, "units: solution(20,200)");
    check_near(g, 0.9, 1e-9, "units: g(20)==0.9 (Oracle)");
    check_near(b, 2.5, 1e-9, "units: b(20)==2.5 (Oracle)");
    check_near(s, 20.0, 1e-9, "units: s(200)==20 (Oracle)");
    check(p2_upm_ma_solution(model, 30, 300, &g, &b, &s) == 0, "units: solution(30,300)");
    check_near(g, 1.1, 1e-9, "units: g(30)==1.1 (Oracle)");
    check_near(b, -1.5, 1e-9, "units: b(30)==-1.5 (Oracle)");
    check_near(s, 40.0, 1e-9, "units: s(300)==40 (Oracle)");

    // kappa 锚（Oracle 独立 NumPy eig）
    check_near(info.kappa, 73.229900948, 1e-6, "units: kappa==73.229900948 (Oracle)");

    // 参数协方差: 测试侧独立 Gauss-Jordan 求逆 H = JᵀWJ
    const Layout L = make_layout({10, 20, 30}, {100, 200, 300}, {10});
    std::vector<double> theta_full(9, 0.0);
    theta_full[0] = 10.0; theta_full[1] = 20.0; theta_full[2] = 40.0;
    theta_full[3] = 1.0;  theta_full[4] = 0.9;  theta_full[5] = 1.1;
    theta_full[6] = 0.0;  theta_full[7] = 2.5;  theta_full[8] = -1.5;
    const std::vector<double> H = normal_matrix(L, obs, theta_full);
    std::vector<double> Hinv;
    check(invert_n(H, 7, Hinv), "units: independent invert of J^T W J");
    std::vector<double> Capi(7 * 7, 0.0);
    check(p2_upm_ma_param_cov(model, Capi.data(), 7) == 0, "units: param_cov rc==0");
    double maxrel = 0.0;
    for (int i = 0; i < 7; ++i)
        for (int j = 0; j < 7; ++j) {
            const double want = Hinv[(std::size_t)i * 7 + j];
            const double got = Capi[(std::size_t)i * 7 + j];
            const double den = std::max(1.0, std::fabs(want));
            maxrel = std::max(maxrel, std::fabs(got - want) / den);
        }
    check(maxrel < 1e-9, "units: C_theta == inv(J^T W J) (independent)");
    // Oracle 锚
    check_near(Capi[0], 0.808893093661304, 1e-9, "units: C_theta[0][0] (Oracle)");
    check_near(Capi[6 * 7 + 6], 3.31499999999998, 1e-9, "units: C_theta[6][6] (Oracle)");

    // C_out = C_stat + J_out C_theta J_out^T（测试侧三重积独立重算）
    {
        const int n = 7;
        std::vector<double> Jout((std::size_t)n * n, 0.0);
        for (int i = 0; i < n; ++i) Jout[(std::size_t)i * n + i] = 1.0;
        std::vector<double> Cstat((std::size_t)n * n, 0.0);
        for (int i = 0; i < n; ++i) Cstat[(std::size_t)i * n + i] = 2.0;
        std::vector<double> Cout((std::size_t)n * n, 0.0);
        check(p2_upm_ma_c_out(model, Jout.data(), n, n, Cstat.data(), n, Cout.data(), n) == 0,
              "units: c_out rc==0");
        double maxrel2 = 0.0;
        for (int a = 0; a < n; ++a)
            for (int b = 0; b < n; ++b) {
                double want = Cstat[(std::size_t)a * n + b];
                for (int i = 0; i < n; ++i)
                    for (int j = 0; j < n; ++j)
                        want += Jout[(std::size_t)a * n + i] * Capi[(std::size_t)i * n + j] *
                                Jout[(std::size_t)b * n + j];
                const double got = Cout[(std::size_t)a * n + b];
                maxrel2 = std::max(maxrel2, std::fabs(got - want) / std::max(1.0, std::fabs(want)));
            }
        check(maxrel2 < 1e-9, "units: C_out == C_stat + J_out C_theta J_out^T");
        // 非方 J_out（2×7）功能性重算
        std::vector<double> J2(2 * 7, 0.0);
        J2[0 * 7 + 0] = 1.0;   // s(100)
        J2[1 * 7 + 6] = 1.0;   // b(30)
        std::vector<double> Cs2 = {0.5, 0.0, 0.0, 0.25};
        std::vector<double> Co2(4, 0.0);
        check(p2_upm_ma_c_out(model, J2.data(), 7, 2, Cs2.data(), 2, Co2.data(), 2) == 0,
              "units: c_out 2x7 rc==0");
        const double want00 = 0.5 + Capi[0];
        const double want11 = 0.25 + Capi[6 * 7 + 6];
        check_near(Co2[0], want00, 1e-9, "units: C_out(2x7)[0][0]");
        check_near(Co2[3], want11, 1e-9, "units: C_out(2x7)[1][1]");
    }

    // 禁止权重反推: C_out 必须含 J C_theta Jᵀ 项（与纯 C_stat 明显不同）
    {
        std::vector<double> Jout(49, 0.0);
        for (int i = 0; i < 7; ++i) Jout[(std::size_t)i * 7 + i] = 1.0;
        std::vector<double> Cstat(49, 0.0);
        for (int i = 0; i < 7; ++i) Cstat[(std::size_t)i * 7 + i] = 2.0;
        std::vector<double> Cout(49, 0.0);
        check(p2_upm_ma_c_out(model, Jout.data(), 7, 7, Cstat.data(), 7, Cout.data(), 7) == 0,
              "units: c_out diagonal rc==0");
        check(std::fabs(Cout[0] - Cstat[0]) > 1e-3,
              "units: C_out != C_stat (UPM term present)");
    }

    // 确定性: 观测顺序无关（hash + 逐值一致）
    {
        std::vector<P2UpmMaObservation> rev(api.rbegin(), api.rend());
        void* model2 = nullptr;
        check(p2_upm_ma_build(rev.data(), rev.size(), nullptr, &model2) == 0,
              "units: reversed-order build rc==0");
        P2UpmMaInfo info2;
        p2_upm_ma_info(model2, &info2);
        check(std::strcmp(info.model_hash, info2.model_hash) == 0,
              "units: model_hash order-independent");
        double g2 = 0, b2 = 0, s2 = 0;
        p2_upm_ma_solution(model2, 30, 300, &g2, &b2, &s2);
        check(g2 == g && b2 == b && s2 == s, "units: solution order-independent (bitwise)");
        p2_upm_ma_close(model2);
    }

    p2_upm_ma_close(model);
    return g_failures;
}

// ===========================================================================
// structure: overlap graph / gauge / 秩 / κ / provenance
// ===========================================================================
int run_structure() {
    // B: 两断开分量，各自独立 gauge（Oracle: refs 1 与 101）
    {
        const std::vector<Obs> obs = ds_disconnected();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), nullptr, &model) == 0,
              "structure: disconnected build rc==0");
        if (model == nullptr) return g_failures;
        P2UpmMaInfo info;
        p2_upm_ma_info(model, &info);
        check(info.n_components == 2, "structure: n_components==2 (overlap graph)");
        check(info.n_frames == 4 && info.n_controls == 4, "structure: F=4 P=4");
        check(info.n_params == 8, "structure: n_params==8 (4+8-4)");
        check(info.rank == 8, "structure: rank==n_params (Oracle B)");
        std::uint64_t c0 = 99, c1 = 99, c2 = 99, c3 = 99;
        check(p2_upm_ma_component_of_frame(model, 1, &c0) == 0, "structure: component(frame1)");
        check(p2_upm_ma_component_of_frame(model, 2, &c1) == 0, "structure: component(frame2)");
        check(p2_upm_ma_component_of_frame(model, 101, &c2) == 0, "structure: component(frame101)");
        check(p2_upm_ma_component_of_frame(model, 102, &c3) == 0, "structure: component(frame102)");
        check(c0 == c1, "structure: frames 1,2 same component");
        check(c2 == c3, "structure: frames 101,102 same component");
        check(c0 != c2, "structure: two components are distinct");
        std::uint64_t r0 = 0, r1 = 0;
        check(p2_upm_ma_component_ref_frame(model, 0, &r0) == 0, "structure: ref comp0");
        check(p2_upm_ma_component_ref_frame(model, 1, &r1) == 0, "structure: ref comp1");
        check(r0 == 1 && r1 == 101, "structure: refs = min frame_id per component (1,101)");
        // gauge: g_ref=1, b_ref=0 in each component
        double g = 0, b = 0, s = 0;
        for (std::uint64_t rf : {1u, 101u}) {
            check(p2_upm_ma_solution(model, rf, rf == 1 ? 11 : 111, &g, &b, &s) == 0,
                  "structure: solution of ref frame");
            check_near(g, 1.0, 1e-12, "structure: scale gauge g_ref==1");
            check_near(b, 0.0, 1e-12, "structure: level gauge b_ref==0");
        }
        // Oracle B anchors: g(2)=1.5, s(12)=5, g(102)=0.5, s(112)=9
        p2_upm_ma_solution(model, 2, 12, &g, &b, &s);
        check_near(g, 1.5, 1e-9, "structure: g(2)==1.5 (Oracle B)");
        check_near(s, 5.0, 1e-9, "structure: s(12)==5 (Oracle B)");
        p2_upm_ma_solution(model, 102, 112, &g, &b, &s);
        check_near(g, 0.5, 1e-9, "structure: g(102)==0.5 (Oracle B)");
        check_near(s, 9.0, 1e-9, "structure: s(112)==9 (Oracle B)");
        p2_upm_ma_close(model);
    }

    // A: provenance 必需键
    {
        const std::vector<Obs> obs = ds_exact();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), nullptr, &model) == 0,
              "structure: provenance build rc==0");
        if (model == nullptr) return g_failures;
        std::vector<char> buf(8192, 0);
        check(p2_upm_ma_provenance(model, buf.data(), buf.size()) == 0,
              "structure: provenance rc==0");
        const std::string j(buf.data());
        for (const char* key : {"gauge_mode", "component_ref_frame_ids", "rank", "rank_rtol",
                                "rank_rtol_effective", "kappa", "n_unidentified", "identifiable",
                                "C_theta", "model_hash", "min_frames",
                                "any_fail_closed_reason", "covariance_method",
                                "variance_from", "variance_from_weight",
                                "uses_relative_weight_as_ivar", "J_C_theta_JT_present"}) {
            check(j.find(key) != std::string::npos, std::string("structure: provenance has ") + key);
        }
        check(j.find("\"variance_from_weight\":false") != std::string::npos,
              "structure: variance_from_weight=false");
        check(j.find("\"uses_relative_weight_as_ivar\":false") != std::string::npos,
              "structure: uses_relative_weight_as_ivar=false");
        char tiny[8];
        check(p2_upm_ma_provenance(model, tiny, sizeof(tiny)) == 2,
              "structure: provenance small-buffer -> rc=2");
        p2_upm_ma_close(model);
    }
    return g_failures;
}

// ===========================================================================
// additive: 单帧 min_frames 门 / additive-only 显式降级
// ===========================================================================
int run_additive() {
    const std::vector<Obs> obs = ds_minframes();
    const std::vector<P2UpmMaObservation> api = to_api(obs);

    void* model = nullptr;
    check(p2_upm_ma_build(api.data(), api.size(), nullptr, &model) == 5,
          "additive: single-frame without declaration -> rc=5 (FZ-AP2S-UPM-MINFRAMES)");
    check(model == nullptr, "additive: rc=5 yields no model");

    P2UpmMaConfig cfg = zero_cfg();
    cfg.allow_additive_only_single_frame = 1;
    model = nullptr;
    check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 0,
          "additive: declared additive-only -> rc=0");
    if (model == nullptr) return g_failures;
    P2UpmMaInfo info;
    p2_upm_ma_info(model, &info);
    check(info.n_params == 2, "additive: n_params==2 (only s)");
    check(info.rank == 2, "additive: rank==2 (Oracle E)");
    check(info.additive_only_components == 1, "additive: one additive-only component");
    double g = 0, b = 0, s = 0;
    check(p2_upm_ma_solution(model, 7, 71, &g, &b, &s) == 0, "additive: solution");
    check_near(g, 1.0, 1e-12, "additive: g==1 (g fixed, no g fit)");
    check_near(b, 0.0, 1e-12, "additive: b==0 (level gauge)");
    check_near(s, 4.0, 1e-12, "additive: s(71)==4");
    check(p2_upm_ma_solution(model, 7, 72, nullptr, nullptr, &s) == 0, "additive: solution s only");
    check_near(s, 8.0, 1e-12, "additive: s(72)==8");
    p2_upm_ma_close(model);
    return g_failures;
}

// ===========================================================================
// negative: 冻结违反 → fail-closed（rc!=0）
// ===========================================================================
int run_negative() {
    // rc=2: control_ivar<=0 / 非有限
    {
        for (double bad : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
            std::vector<Obs> obs = ds_exact();
            obs[3].ivar = bad;
            const std::vector<P2UpmMaObservation> api = to_api(obs);
            void* model = nullptr;
            check(p2_upm_ma_build(api.data(), api.size(), nullptr, &model) == 2,
                  "negative: invalid control_ivar -> rc=2");
            check(model == nullptr, "negative: rc=2 yields no model");
        }
    }
    // rc=3: 恒常 s → g/b 退化 → 秩亏
    {
        const std::vector<Obs> obs = ds_constant_s();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), nullptr, &model) == 3,
              "negative: constant s (g/b degenerate) -> rc=3 rank-deficient");
        check(model == nullptr, "negative: rc=3 yields no model");
    }
    // 病态（同一判据的红侧）：s 场近简并到 κ(H_eq) > 1/τ = 1e10 ⇒ rc=3（欠定/病态同一 rc）。
    // **已退休**：原 rc=4「kappa > 1e6」绝对常数门（Oracle F 的 eps=5e-4 ⇒ κ=1.1701e8）。
    {
        const std::vector<Obs> obs = ds_illcond(1e-7);
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        const int rc = p2_upm_ma_build(api.data(), api.size(), nullptr, &model);
        check(rc == 3, std::string("negative: kappa>1/tau -> rc=3 (ill-conditioned, got rc=") +
                           std::to_string(rc) + ")");
        check(model == nullptr, "negative: red verdict yields no model (fail-closed)");
    }
    // **口径订正的实证**：eps=5e-4 ⇒ κ=1.1701e8。旧绝对常数门 1e6 判红，
    // 新判据（κ < 1/τ = 1e10）判绿——同一份数据、两个相反结论，正是本次统一的对象。
    {
        const std::vector<Obs> obs = ds_illcond(5e-4);
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        const int rc = p2_upm_ma_build(api.data(), api.size(), nullptr, &model);
        check(rc == 0, std::string("retired-gate witness: eps=5e-4 now builds, rc=") +
                           std::to_string(rc));
        if (rc == 0 && model) {
            P2UpmMaInfo i{};
            p2_upm_ma_info(model, &i);
            check(i.identifiable == 1 && i.n_unidentified == 0,
                  "retired-gate witness: identifiable==1 at kappa=" + std::to_string(i.kappa));
            check(i.kappa > 1e6 && i.kappa < 1.0 / i.rank_rtol_effective,
                  "retired-gate witness: 1e6 < kappa < 1/tau (the old constant was the defect)");
            p2_upm_ma_close(model);
        }
    }
    // rc=6: 共享系统项按独立处理
    {
        const std::vector<Obs> obs = ds_exact();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        P2UpmMaConfig cfg = zero_cfg();
        cfg.c_in_has_unrepresented_shared_terms = 1;
        void* model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 6,
              "negative: unrepresented shared systematic -> rc=6");
    }
    // rc=7: k_corr provenance（忽略相关/缺适用域/域外推）
    {
        const std::vector<Obs> obs = ds_exact();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        for (double kc : {1.0, 1.5}) {
            P2UpmMaConfig cfg = zero_cfg();
            cfg.k_corr = kc;
            cfg.k_corr_applicability_domain = "drizzle/pixfrac=0.8/patch=8/robust-median/spherical";
            void* model = nullptr;
            check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 7,
                  "negative: k_corr=1.0/out-of-domain -> rc=7 (FZ-PROV-KCORR)");
        }
        P2UpmMaConfig cfg = zero_cfg();
        cfg.k_corr = 1.4;   // 域内冻结值但缺适用域
        void* model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 7,
              "negative: k_corr missing applicability domain -> rc=7");
        cfg.k_corr_applicability_domain = "declared-domain";
        model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 0,
              "negative: k_corr=1.4 in declared domain -> rc=0");
        if (model != nullptr) {
            std::vector<char> buf(8192, 0);
            p2_upm_ma_provenance(model, buf.data(), buf.size());
            check(std::string(buf.data()).find("\"k_corr\":1.4") != std::string::npos,
                  "negative: provenance records k_corr=1.4");
            p2_upm_ma_close(model);
        }
        // 1.5 带标定 run id + 域 → 允许（非域内值但提供标定证据）
        cfg = zero_cfg();
        cfg.k_corr = 1.5;
        cfg.k_corr_applicability_domain = "declared-domain";
        cfg.k_corr_calibration_run_id = "mc-seed-20260915";
        model = nullptr;
        check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 0,
              "negative: k_corr=1.5 with MC run id -> rc=0");
        if (model != nullptr) p2_upm_ma_close(model);
    }
    // 参数错误
    {
        const std::vector<Obs> obs = ds_exact();
        const std::vector<P2UpmMaObservation> api = to_api(obs);
        void* model = nullptr;
        check(p2_upm_ma_build(nullptr, 0, nullptr, &model) == 1, "negative: null obs -> rc=1");
        check(p2_upm_ma_build(api.data(), 0, nullptr, &model) == 1, "negative: n_obs=0 -> rc=1");
        P2UpmMaConfig cfg = zero_cfg();
        cfg.gauge_mode = 7;
        check(p2_upm_ma_build(api.data(), api.size(), &cfg, &model) == 1,
              "negative: unknown gauge_mode -> rc=1");
    }
    return g_failures;
}

// ===========================================================================
// control_variance: ALG-P2S-UPM.7 / FZ-PROV-KCORR / FZ-PROV-KCORR-VALUE
// ===========================================================================
int run_control_variance() {
    double cv = 0.0, ivar = 0.0;
    const char* dom = "drizzle/pixfrac=0.8/patch8/median";
    check(p2_upm_control_variance(1.4, 5.0, 100, dom, nullptr, &cv, &ivar) == 0,
          "cv: frozen 1.4 in-domain -> rc=0");
    const double want = 1.4 * pi_half() * 25.0 / 100.0;
    check_near(cv, want, 1e-12, "cv: control_variance = k_corr*(pi/2)*sigma^2/N");
    check_near(ivar, 1.0 / want, 1e-12, "cv: control_ivar = 1/control_variance");
    check(p2_upm_control_variance(1.0, 5.0, 100, dom, nullptr, nullptr, nullptr) == 2,
          "cv: k_corr=1.0 (ignores correlation) -> rc=2");
    check(p2_upm_control_variance(1.5, 5.0, 100, dom, nullptr, nullptr, nullptr) == 3,
          "cv: out-of-domain without MC run id -> rc=3");
    check(p2_upm_control_variance(1.5, 5.0, 100, dom, "mc-1", nullptr, nullptr) == 0,
          "cv: out-of-domain with MC run id -> rc=0");
    check(p2_upm_control_variance(1.4, 5.0, 100, nullptr, nullptr, nullptr, nullptr) == 4,
          "cv: missing applicability domain -> rc=4");
    check(p2_upm_control_variance(1.4, 5.0, 0, dom, nullptr, nullptr, nullptr) == 1,
          "cv: N_retained=0 -> rc=1");
    check(p2_upm_control_variance(1.4, 0.0, 100, dom, nullptr, nullptr, nullptr) == 1,
          "cv: sigma_bg=0 -> rc=1");
    check(p2_upm_control_variance(0.0, 5.0, 100, dom, nullptr, nullptr, nullptr) == 1,
          "cv: k_corr=0 -> rc=1");
    // SCI-FIX-WEIGHT / R-2 D-10：k_corr ≥ 1 是定义性约束（N_eff ≤ N_retained）。
    // k<1 ⇒ N_eff > N_retained，物理不可达，必须显式拒（旧实现 rc=0 接受）。
    check(p2_upm_control_variance(0.9, 5.0, 100, dom, "mc-1", nullptr, nullptr) == 1,
          "cv: k_corr<1 (0.9) -> rc=1（越域，不接受）");
    check(p2_upm_control_variance(0.5, 5.0, 100, dom, "mc-1", nullptr, nullptr) == 1,
          "cv: k_corr<1 (0.5) -> rc=1（越域，不接受）");
    check(p2_upm_control_variance(0.999999, 5.0, 100, dom, "mc-1", nullptr, nullptr) == 1,
          "cv: k_corr<1 (边界) -> rc=1");
    check(p2_upm_control_variance(1.0000001, 5.0, 100, dom, "mc-1", nullptr, nullptr) == 0,
          "cv: k_corr≥1 且带 MC run id -> rc=0");
    return g_failures;
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "units";
    if (mode == "units") run_units();
    else if (mode == "structure") run_structure();
    else if (mode == "additive") run_additive();
    else if (mode == "negative") run_negative();
    else if (mode == "control_variance") run_control_variance();
    else {
        std::printf("unknown mode: %s\n", mode.c_str());
        return 2;
    }
    std::printf("[%s] checks=%d failures=%d\n", mode.c_str(), g_checks, g_failures);
    return g_failures;
}
