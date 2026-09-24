// eng/tests/unit/v6_p2_sky/v6_p2_sky_test.cpp
//
// FIX-A 天光面链共址测试（Oracle 正例 + 负例，fail-closed）。
// 被测面 = astrocs_phase2（lib/algorithms/coverage/src/sky_plane.cpp）。
//
// 用例：
//   patch       — 局部稳健背景估计（median/MAD/variance/门）
//   starmask    — 星点掩膜圆帽构造与包含判定
//   recovery    — 注入大尺度梯度 + 逐帧 δ 的恢复精度（独立解析真值）
//   gauge       — reference_frame / sum 规范
//   weighting   — 低 SNR/光污染帧加权前后对照
//   negative    — 无采样点/全掩膜/点不足/帧欠定/节点超限/越域/未知帧 必红
//   memory      — 内存不随帧数无界增长
//   determinism — 重复构建逐位一致（worker 数无关：本模块串行）
#include "astro/phase2/sky_plane.h"
#include "astro/phase2/upm.h"
#include <filesystem>
#include <string>
#include "healpix/healpix_core.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace {

int g_fail = 0;
void check(bool ok, const char* what) {
    if (!ok) { std::printf("  FAIL: %s\n", what); ++g_fail; }
    else std::printf("  ok  : %s\n", what);
}
bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

double truth_b(double u, double v) {
    return 100.0 + 6.0 * std::sin(u * 3.14159265358979323846 / 5.0) +
           4.0 * std::cos(v * 3.14159265358979323846 / 4.0) + 0.8 * u * v;
}

struct Synth {
    std::vector<P2SkySample> s;
    std::vector<double> delta_true;   // nf*3
    int nf = 0;
};

Synth make_synth(int nf, double sigma, unsigned seed, bool equal_variance,
                 int bad_frame = -1, double bad_offset = 0.0) {
    Synth o;
    o.nf = nf;
    o.delta_true.assign(static_cast<std::size_t>(nf) * 3, 0.0);
    for (int k = 1; k < nf; ++k) {
        o.delta_true[static_cast<std::size_t>(k) * 3 + 0] = 0.5 * ((k % 3) - 1);
        o.delta_true[static_cast<std::size_t>(k) * 3 + 1] = 0.2 * ((k % 2) ? 1.0 : -1.0);
        o.delta_true[static_cast<std::size_t>(k) * 3 + 2] = 0.1 * ((k % 2) ? -1.0 : 1.0);
    }
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> g(0.0, 1.0);
    const double uc = 2.0, vc = 2.0, us = 2.0, vs = 2.0;
    for (int k = 0; k < nf; ++k)
        for (int i = 0; i <= 20; ++i)
            for (int j = 0; j <= 20; ++j) {
                const double u = 4.0 * i / 20.0, v = 4.0 * j / 20.0;
                const double x = (u - uc) / us, y = (v - vc) / vs;
                const double dk = o.delta_true[static_cast<std::size_t>(k) * 3 + 0] +
                                  o.delta_true[static_cast<std::size_t>(k) * 3 + 1] * x +
                                  o.delta_true[static_cast<std::size_t>(k) * 3 + 2] * y;
                P2SkySample sm{};
                sm.frame_id = 1000 + static_cast<std::uint64_t>(k);
                sm.control_id = static_cast<std::uint64_t>(i * 21 + j);
                sm.ra_deg = u; sm.dec_deg = v;
                const double sig_k = (equal_variance || k != bad_frame) ? sigma : sigma * 10.0;
                // 光污染：局部非平面亮斑（不能被该帧平面 delta_k 吸收，
                // 只有加权才能阻止它污染 B_ref）。
                const double bu_ = u - 1.0, bv_ = v - 1.0;
                const double bump = (k == bad_frame)
                    ? bad_offset * std::exp(-(bu_ * bu_ + bv_ * bv_) / (2.0 * 0.25 * 0.25))
                    : 0.0;
                sm.value = truth_b(u, v) + dk + bump + g(rng) * sig_k;
                sm.variance = sig_k * sig_k;
                sm.snr = 1.0 / sig_k;
                sm.flags = 0;
                o.s.push_back(sm);
            }
    return o;
}

P2SkyPlaneConfig base_cfg() {
    P2SkyPlaneConfig c = p2_sky_plane_default_config();
    c.node_spacing_deg = 1.0;
    c.frame_gradient_order = 1;
    c.gauge_mode = 0;
    c.weight_mode = 0;
    return c;
}

void test_patch() {
    std::printf("[patch]\n");
    std::vector<double> v;
    for (int i = 0; i < 100; ++i) v.push_back(10.0);
    P2SkyPatchEstimate e{};
    char err[256] = {0};
    int rc = p2_sky_patch_estimate(v.data(), nullptr, v.size(), nullptr, &e, err, sizeof(err));
    check(rc == P2_SKY_PATCH_OK, "constant patch ok");
    check(near(e.value, 10.0, 1e-12), "median == 10");
    check(e.variance > 0.0, "variance > 0");
    // variance = k_corr*(pi/2)*sigma^2/N ; sigma=1e-12 floor for zero MAD
    // outlier rejection: 99 zeros + 1 huge
    std::vector<double> w(100, 5.0);
    w[50] = 1e6;
    P2SkyPatchEstimate e2{};
    rc = p2_sky_patch_estimate(w.data(), nullptr, w.size(), nullptr, &e2, err, sizeof(err));
    check(rc == P2_SKY_PATCH_OK && near(e2.value, 5.0, 1e-9), "bright outlier rejected by median");
    // negative: too few samples
    std::vector<double> t(2, 1.0);
    P2SkyPatchEstimate e3{};
    rc = p2_sky_patch_estimate(t.data(), nullptr, t.size(), nullptr, &e3, err, sizeof(err));
    check(rc == P2_SKY_PATCH_INSUFFICIENT_SAMPLES, "too few samples -> fail-closed");
    // negative: null
    rc = p2_sky_patch_estimate(nullptr, nullptr, 0, nullptr, &e3, err, sizeof(err));
    check(rc == P2_SKY_PATCH_INVALID_ARGS, "null patch -> invalid");
}

void test_starmask() {
    std::printf("[starmask]\n");
    const double ra[4] = {10.0, 20.0, 30.0, 40.0};
    const double dec[4] = {0.0, 0.0, 0.0, 0.0};
    const double snr[4] = {100.0, 1.0, 50.0, 0.1};
    P2StarMaskCap caps[4];
    std::uint64_t n = 0;
    int rc = p2_star_mask_caps(ra, dec, snr, 4, 10.0, 0.05, caps, 4, &n);
    check(rc == 0 && n == 2, "two bright stars masked");
    check(p2_star_mask_contains(caps, n, 10.0, 0.0) == 1, "hit exact star");
    check(p2_star_mask_contains(caps, n, 10.0, 0.01) == 1, "hit inside radius");
    check(p2_star_mask_contains(caps, n, 10.0, 1.0) == 0, "miss outside radius");
    check(p2_star_mask_contains(caps, n, 20.0, 0.0) == 0, "faint star not masked");
    check(p2_star_mask_contains(nullptr, 0, 0.0, 0.0) == -1, "null caps -> -1");
}

void test_recovery() {
    std::printf("[recovery]\n");
    Synth sy = make_synth(4, 0.05, 12345, false);
    P2SkyPlaneConfig cfg = base_cfg();
    void* model = nullptr;
    char err[512] = {0};
    int rc = p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &model, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "build ok");
    if (rc != 0) return;
    P2SkyPlaneInfo info{};
    p2_sky_plane_info(model, &info);
    std::printf("  info: n_nodes=%llu n_free_params=%llu rank=%llu kappa=%.3e rms_w=%.5f\n",
                (unsigned long long)info.n_nodes, (unsigned long long)info.n_params,
                (unsigned long long)info.rank, info.kappa, info.rms_weighted);
    // B_ref recovery (ref frame delta=0 -> B_ref == truth)
    double rms = 0.0, mx = 0.0; int cnt = 0;
    for (int i = 0; i <= 20; ++i)
        for (int j = 0; j <= 20; ++j) {
            const double u = 4.0 * i / 20.0, v = 4.0 * j / 20.0;
            double val = 0.0; int st = -1;
            p2_sky_plane_eval(model, 1000, u, v, &val, &st);
            if (st != P2_SKY_EVAL_OK) continue;
            const double e = val - truth_b(u, v);
            rms += e * e; mx = std::max(mx, std::fabs(e)); ++cnt;
        }
    rms = std::sqrt(rms / cnt);
    std::printf("  B_ref recovery rms=%.5f max=%.5f\n", rms, mx);
    // theoretical ~ sigma/sqrt(N/n_params) = 0.05/sqrt(1764/58)=0.0091
    check(rms < 0.030, "B_ref recovery RMS < 0.030 (<=3x theoretical)");
    check(mx < 0.10, "B_ref recovery max < 0.10");
    // delta constants
    for (int k = 1; k < sy.nf; ++k) {
        double d[3] = {0, 0, 0}; std::uint64_t dn = 0;
        p2_sky_plane_frame_delta(model, 1000 + static_cast<std::uint64_t>(k), d, 3, &dn);
        check(near(d[0], sy.delta_true[static_cast<std::size_t>(k) * 3], 0.01),
              "delta constant recovered");
    }
    // reference frame delta identically zero
    double d0[3] = {9, 9, 9}; std::uint64_t dn0 = 0;
    p2_sky_plane_frame_delta(model, 1000, d0, 3, &dn0);
    check(d0[0] == 0.0 && d0[1] == 0.0 && d0[2] == 0.0, "reference frame delta == 0");
    // residuals recomputed independently
    double rw = 0.0, ru = 0.0; std::uint64_t nu = 0;
    p2_sky_plane_residuals(model, sy.s.data(), sy.s.size(), &rw, &ru, &nu);
    std::printf("  residual: recompute rw=%.8f ru=%.8f nu=%llu ; build rw=%.8f ru=%.8f\n",
                rw, ru, (unsigned long long)nu, info.rms_weighted, info.rms_unweighted);
    check(near(rw, info.rms_weighted, 1e-9), "residual recompute matches build");
}

void test_gauge() {
    std::printf("[gauge]\n");
    Synth sy = make_synth(4, 0.05, 777, false);
    P2SkyPlaneConfig cfg = base_cfg();
    cfg.gauge_mode = 1;   // sum-zero offsets
    void* model = nullptr;
    char err[512] = {0};
    int rc = p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &model, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "sum-gauge build ok");
    if (rc != 0) return;
    double sum = 0.0;
    for (int k = 0; k < sy.nf; ++k) {
        double d[3] = {0, 0, 0}; std::uint64_t dn = 0;
        p2_sky_plane_frame_delta(model, 1000 + static_cast<std::uint64_t>(k), d, 3, &dn);
        sum += d[0];
    }
    check(std::fabs(sum) < 1e-9, "sum-gauge: sum of delta offsets == 0");
    P2SkyPlaneInfo info{};
    p2_sky_plane_info(model, &info);
    check(std::fabs(info.gauge_shift) > 0.0, "gauge_shift recorded");
    p2_sky_plane_close(model);
}

void test_weighting() {
    std::printf("[weighting]\n");
    // Frame 3 is a low-SNR, light-polluted frame (10x sigma, +2 ADU offset).
    Synth sy = make_synth(5, 0.05, 4242, false, 3, 2.0);
    // (a) inverse-variance weighted (true variances)
    void* mw = nullptr; char err[512] = {0};
    P2SkyPlaneConfig cfg = base_cfg();
    int rc = p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &mw, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "weighted build ok");
    // (b) equal-variance (emulates unweighted / equal weight)
    Synth sy2 = sy;
    for (auto& sm : sy2.s) { sm.variance = 0.05 * 0.05; sm.snr = 1.0 / 0.05; }
    void* mu = nullptr;
    rc = p2_sky_plane_build(sy2.s.data(), sy2.s.size(), &cfg, &mu, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "unweighted build ok");
    if (mw == nullptr || mu == nullptr) return;
    // 只在污染亮斑中心量 B_ref 偏差（该处平面 delta_k 无法吸收）。
    auto bias = [&](void* m) {
        double val = 0.0; int st = -1;
        p2_sky_plane_eval(m, 1000, 1.0, 1.0, &val, &st);
        return (st == P2_SKY_EVAL_OK) ? std::fabs(val - truth_b(1.0, 1.0)) : 0.0;
    };
    const double bw = bias(mw), bu = bias(mu);
    std::printf("  B_ref bias: weighted=%.5f unweighted=%.5f\n", bw, bu);
    check(bw < 0.5 * bu, "inverse-variance weighting suppresses polluted-frame bias");
    p2_sky_plane_close(mw);
    p2_sky_plane_close(mu);
}

void test_negative() {
    std::printf("[negative]\n");
    char err[512] = {0};
    P2SkyPlaneConfig cfg = base_cfg();
    void* m = nullptr;
    // no samples
    check(p2_sky_plane_build(nullptr, 0, &cfg, &m, err, sizeof(err)) == P2_SKY_PLANE_INVALID_ARGS,
          "null samples -> INVALID_ARGS");
    // all masked
    Synth sy = make_synth(3, 0.05, 1, false);
    for (auto& sm : sy.s) sm.flags = P2_SKY_FLAG_MASKED;
    check(p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &m, err, sizeof(err)) == P2_SKY_PLANE_NO_USABLE_SAMPLES,
          "all masked -> NO_USABLE_SAMPLES");
    // too few samples
    std::vector<P2SkySample> few(sy.s.begin(), sy.s.begin() + 3);
    for (auto& sm : few) sm.flags = 0;
    check(p2_sky_plane_build(few.data(), few.size(), &cfg, &m, err, sizeof(err)) == P2_SKY_PLANE_TOO_FEW_SAMPLES,
          "too few samples -> TOO_FEW_SAMPLES");
    // frame underdetermined: one frame with 2 points, delta plane needs 4
    std::vector<P2SkySample> und;
    for (int k = 0; k < 2; ++k)
        for (int i = 0; i < (k == 0 ? 60 : 2); ++i) {
            P2SkySample sm{};
            sm.frame_id = 1000 + static_cast<std::uint64_t>(k);
            sm.ra_deg = 1.0 + 0.01 * i; sm.dec_deg = 1.0;
            sm.value = 10.0; sm.variance = 0.01; sm.snr = 10.0; sm.flags = 0;
            und.push_back(sm);
        }
    check(p2_sky_plane_build(und.data(), und.size(), &cfg, &m, err, sizeof(err)) == P2_SKY_PLANE_FRAME_UNDERDETERMINED,
          "frame with too few points -> FRAME_UNDERDETERMINED");
    // too many nodes
    Synth big = make_synth(2, 0.05, 9, false);
    P2SkyPlaneConfig dense = base_cfg();
    dense.node_spacing_deg = 0.02;   // ~200x200 nodes > max_nodes
    check(p2_sky_plane_build(big.s.data(), big.s.size(), &dense, &m, err, sizeof(err)) == P2_SKY_PLANE_TOO_MANY_NODES,
          "dense grid -> TOO_MANY_NODES");
    // extrapolation + unknown frame
    Synth ok = make_synth(2, 0.02, 5, false);
    int rc = p2_sky_plane_build(ok.s.data(), ok.s.size(), &cfg, &m, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "valid build for eval tests");
    if (rc == 0) {
        double val = 0.0; int st = -1;
        p2_sky_plane_eval(m, 1000, 90.0, 80.0, &val, &st);
        check(st == P2_SKY_EVAL_OUT_OF_DOMAIN, "far eval -> OUT_OF_DOMAIN (no extrapolation)");
        p2_sky_plane_eval(m, 999999, 2.0, 2.0, &val, &st);
        check(st == P2_SKY_EVAL_UNKNOWN_FRAME, "unknown frame -> UNKNOWN_FRAME");
        std::uint8_t sts[1] = {0};
        double ra[1] = {90.0}, dec[1] = {80.0};
        check(p2_sky_plane_eval_block(m, 1000, ra, dec, 1, &val, sts) == 2,
              "eval_block reports out-of-domain as rc=2");
        p2_sky_plane_close(m);
    }
}

void test_memory() {
    std::printf("[memory]\n");
    // Same node grid, increasing frame count: n_nodes must not grow, and the
    // peak RSS delta must stay bounded (Schur elimination is frame-count-free
    // in the reduced system; only per-frame delta coefficients grow linearly).
    auto rss_kb = []() -> long {
#if defined(_WIN32)
        return 0;
#else
        std::FILE* f = std::fopen("/proc/self/statm", "r");
        if (!f) return 0;
        long size = 0, res = 0;
        if (std::fscanf(f, "%ld %ld", &size, &res) != 2) res = 0;
        std::fclose(f);
        return res * (sysconf(_SC_PAGESIZE) / 1024);
#endif
    };
    Synth s8 = make_synth(8, 0.05, 11, false);
    Synth s64 = make_synth(64, 0.05, 11, false);
    P2SkyPlaneConfig cfg = base_cfg();
    void* m8 = nullptr; char err[256] = {0};
    int rc = p2_sky_plane_build(s8.s.data(), s8.s.size(), &cfg, &m8, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "8-frame build ok");
    P2SkyPlaneInfo i8{}; if (m8) p2_sky_plane_info(m8, &i8);
    const long r0 = rss_kb();
    void* m64 = nullptr;
    rc = p2_sky_plane_build(s64.s.data(), s64.s.size(), &cfg, &m64, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "64-frame build ok");
    P2SkyPlaneInfo i64{}; if (m64) p2_sky_plane_info(m64, &i64);
    const long r1 = rss_kb();
    std::printf("  n_nodes: 8f=%llu 64f=%llu ; rss delta=%ld KB\n",
                (unsigned long long)i8.n_nodes, (unsigned long long)i64.n_nodes, r1 - r0);
    check(i8.n_nodes == i64.n_nodes, "n_nodes independent of frame count");
    // UPM-KAPPA-UNIFY-01：n_params 的语义改为**判据矩阵的阶**（Schur 消元后的
    // B_ref 自由节点数）；δ_k 已被精确消去、不是判据面对的未知量 ⇒ 它**不**随帧数增长。
    // 求解器全部未知量 = n_params_full，那个才随帧数增长（每帧多 m 个 δ 系数）。
    check(i64.n_params == i8.n_params,
          "n_params (criterion matrix order) independent of frame count");
    check(i64.n_params_full > i8.n_params_full, "n_params_full grows with frames");
    check(i64.rank_full == i64.rank + (i64.n_frames - 1) * 3,
          "rank_full == r_eff + (n_frames-1)*m (delta block full rank)");
    check((r1 - r0) < 200 * 1024, "peak RSS delta for 8x frames < 200 MB");
    (void)r0; (void)r1;
    if (m8) p2_sky_plane_close(m8);
    if (m64) p2_sky_plane_close(m64);
}

void test_determinism() {
    std::printf("[determinism]\n");
    Synth sy = make_synth(6, 0.05, 31415, false);
    P2SkyPlaneConfig cfg = base_cfg();
    char err[256] = {0};
    char h1[65] = {0}, h2[65] = {0};
    for (int rep = 0; rep < 2; ++rep) {
        void* m = nullptr;
        int rc = p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &m, err, sizeof(err));
        check(rc == P2_SKY_PLANE_OK, "determinism build ok");
        if (rc != 0) return;
        P2SkyPlaneInfo info{};
        p2_sky_plane_info(m, &info);
        if (rep == 0) std::memcpy(h1, info.model_hash, sizeof(h1));
        else std::memcpy(h2, info.model_hash, sizeof(h2));
        p2_sky_plane_close(m);
    }
    check(std::strcmp(h1, h2) == 0, "repeated build model_hash bitwise identical");
    check(std::strlen(h1) == 64, "model_hash is 64 hex chars");
    // save/open round-trip preserves evaluation
    void* m = nullptr;
    int rc = p2_sky_plane_build(sy.s.data(), sy.s.size(), &cfg, &m, err, sizeof(err));
    if (rc != 0) return;
    // 输出落到显式 run 目录（编译期注入；不依赖 cwd 是仓库根），
    // 未注入时退到系统临时目录；绝不写仓库根。
    std::string path;
#ifdef SKY_TEST_OUTDIR
    {
        std::error_code ec;
        std::filesystem::create_directories(SKY_TEST_OUTDIR, ec);
        path = std::string(SKY_TEST_OUTDIR) + "/sky_plane_roundtrip_test.json";
    }
#else
    path = (std::filesystem::temp_directory_path() / "sky_plane_roundtrip_test.json").string();
#endif
    check(p2_sky_plane_save(m, path.c_str()) == P2_SKY_PLANE_OK, "save ok");
    void* m2 = nullptr;
    check(p2_sky_plane_open(path.c_str(), &m2) == P2_SKY_PLANE_OK, "open ok");
    if (m2) {
        double a = 0.0, b = 0.0; int sa = -1, sb = -1;
        p2_sky_plane_eval(m, 1000, 2.0, 2.0, &a, &sa);
        p2_sky_plane_eval(m2, 1000, 2.0, 2.0, &b, &sb);
        check(sa == P2_SKY_EVAL_OK && sb == P2_SKY_EVAL_OK && near(a, b, 1e-12),
              "save/open eval identical");
        p2_sky_plane_close(m2);
    }
    p2_sky_plane_close(m);
}

void test_seam() {
    std::printf("[seam]\n");
    // 3 帧完全重叠：注入大尺度梯度 + 每帧面板台阶（加性场）+ 噪声。
    // 天光面 b_k(x)=B_ref+δ_k 校准后 corrected=raw−b_k，跨帧台阶应被抹平。
    const int nf = 3;
    std::mt19937_64 rng(20260812);
    std::normal_distribution<double> nd(0.0, 0.02);
    std::vector<P2SkySample> s;
    std::vector<double> field_true;   // 注入的每帧加性场（逐样本）
    for (int k = 0; k < nf; ++k)
        for (int i = 0; i <= 20; ++i)
            for (int j = 0; j <= 20; ++j) {
                const double u = 4.0 * i / 20.0, v = 4.0 * j / 20.0;
                // 面板台阶：每帧常数 + 各自平缓梯度
                const double panel = 1.0 * k + 0.25 * k * (u - 2.0) + 0.15 * k * (v - 2.0);
                P2SkySample sm{};
                sm.frame_id = 1000 + static_cast<std::uint64_t>(k);
                sm.control_id = static_cast<std::uint64_t>(i * 21 + j);
                sm.ra_deg = u; sm.dec_deg = v;
                sm.value = truth_b(u, v) + panel + nd(rng);
                sm.variance = 0.02 * 0.02;
                sm.snr = 50.0;
                sm.flags = 0;
                s.push_back(sm);
                field_true.push_back(panel);
            }
    P2SkyPlaneConfig cfg = base_cfg();
    void* m = nullptr; char err[512] = {0};
    int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    check(rc == P2_SKY_PLANE_OK, "seam build ok");
    if (rc != 0) return;
    // 每帧每 control 的校准值（用参考帧 1000 的 b 场作为公共参考面）
    auto corrected = [&](std::size_t idx, std::uint64_t fid) {
        double b = 0.0; int st = -1;
        p2_sky_plane_eval(m, fid, s[idx].ra_deg, s[idx].dec_deg, &b, &st);
        return (st == P2_SKY_EVAL_OK) ? s[idx].value - b : s[idx].value;
    };
    double seam_raw = 0.0, seam_cal = 0.0; int cnt = 0;
    for (std::size_t t = 0; t < s.size(); ++t) {
        if (s[t].frame_id != 1000) continue;
        for (int k = 1; k < nf; ++k) {
            const std::size_t tk = t + static_cast<std::size_t>(k) * 441;
            seam_raw += std::fabs(s[tk].value - s[t].value);
            seam_cal += std::fabs(corrected(tk, s[tk].frame_id) - corrected(t, 1000));
            ++cnt;
        }
    }
    seam_raw /= cnt; seam_cal /= cnt;
    std::printf("  cross-frame seam: raw=%.5f calibrated=%.5f\n", seam_raw, seam_cal);
    check(seam_raw > 0.5, "injected panel step is significant");
    check(seam_cal < 0.05 * seam_raw, "sky plane removes cross-frame panel step (>20x)");
    p2_sky_plane_close(m);
}

void test_gain() {
    std::printf("[gain]\n");
    // 乘法响应：y_k = g_k·B(x) + b_k（帧级加性）。v6 UPM MA 求解器恢复 g_k。
    const int nf = 3;
    const double gtrue[3] = {1.0, 1.08, 0.94};
    std::mt19937_64 rng(7);
    std::normal_distribution<double> nd(0.0, 0.02);
    std::vector<P2UpmMaObservation> obs;
    for (int k = 0; k < nf; ++k)
        for (int i = 0; i <= 10; ++i)
            for (int j = 0; j <= 10; ++j) {
                const double u = 4.0 * i / 10.0, v = 4.0 * j / 10.0;
                P2UpmMaObservation o{};
                o.frame_id = 1000 + static_cast<std::uint64_t>(k);
                o.control_id = static_cast<std::uint64_t>(i * 21 + j);
                o.value = gtrue[k] * truth_b(u, v) + 0.5 * k + nd(rng);
                o.control_ivar = 1.0 / (0.02 * 0.02);
                obs.push_back(o);
            }
    P2UpmMaConfig cfg{};
    cfg.min_frames = 2;
    cfg.gauge_mode = 0;
    void* m = nullptr;
    int rc = p2_upm_ma_build(obs.data(), obs.size(), &cfg, &m);
    check(rc == 0, "MA build ok");
    if (rc != 0) return;
    for (int k = 0; k < nf; ++k) {
        double g = 0.0, b = 0.0;
        if (p2_upm_ma_solution(m, 1000 + static_cast<std::uint64_t>(k), 0, &g, &b, nullptr) == 0) {
            const double ratio = g / gtrue[k];   // gauge g_ref=1 -> ratio == gtrue[0]
            std::printf("  frame %d: g=%.5f ratio=%.5f (expect %.5f)\n", k, g, ratio, gtrue[0]);
            check(near(ratio, gtrue[0], 0.02), "multiplicative response recovered (ratio to ref)");
        }
    }
    p2_upm_ma_close(m);
}
void test_dcgain() {
    std::printf("[dcgain]\n");
    // 增益均匀（帧间仅加性/相位差异，无乘法增益）：DC 比必须 ≈1（<1%）。
    {
        std::vector<P2SkySample> s;
        for (int k = 0; k < 3; ++k)
            for (int i = 0; i <= 20; ++i)
                for (int j = 0; j <= 20; ++j) {
                    const double u = 4.0 * i / 20.0, v = 4.0 * j / 20.0;
                    P2SkySample sm{};
                    sm.frame_id = 1000 + (std::uint64_t)k;
                    sm.control_id = (std::uint64_t)(i * 21 + j);
                    sm.ra_deg = u; sm.dec_deg = v;
                    sm.value = truth_b(u, v) + 0.5 * std::sin((double)k + 0.01 * u + 0.02 * v);
                    sm.variance = 1e-4; sm.snr = 50.0; sm.flags = 0;
                    s.push_back(sm);
                }
        for (int k = 1; k < 3; ++k) {
            double g = 0.0;
            const int rc = p2_sky_estimate_gain_dc(s.data(), s.size(), 1000, 1000 + (std::uint64_t)k, &g);
            std::printf("  uniform frame %d: g_dc=%.5f\n", k, g);
            check(rc == 0, "DC gain rc ok");
            check(near(g, 1.0, 0.01), "gain-uniform DC ratio within 1% (no spurious gain)");
        }
    }
    {
        const double gt[3] = {1.0, 1.08, 0.94};
        std::vector<P2SkySample> s;
        for (int k = 0; k < 3; ++k)
            for (int i = 0; i <= 20; ++i)
                for (int j = 0; j <= 20; ++j) {
                    const double u = 4.0 * i / 20.0, v = 4.0 * j / 20.0;
                    P2SkySample sm{};
                    sm.frame_id = 1000 + (std::uint64_t)k;
                    sm.control_id = (std::uint64_t)(i * 21 + j);
                    sm.ra_deg = u; sm.dec_deg = v;
                    sm.value = gt[k] * truth_b(u, v);
                    sm.variance = 1e-4; sm.snr = 50.0; sm.flags = 0;
                    s.push_back(sm);
                }
        for (int k = 1; k < 3; ++k) {
            double g = 0.0;
            const int rc = p2_sky_estimate_gain_dc(s.data(), s.size(), 1000, 1000 + (std::uint64_t)k, &g);
            std::printf("  gain frame %d: g_dc=%.5f (true %.5f)\n", k, g, gt[k]);
            check(rc == 0, "real-gain DC rc ok");
            check(near(g, gt[k], 0.01), "real multiplicative gain recovered within 1%");
        }
    }
    {
        std::vector<P2SkySample> s(2);
        s[0].frame_id = 1; s[0].control_id = 1; s[0].value = 1.0; s[0].variance = 1e-4;
        s[1].frame_id = 2; s[1].control_id = 2; s[1].value = 1.0; s[1].variance = 1e-4;
        double g = 0.0;
        check(p2_sky_estimate_gain_dc(s.data(), s.size(), 1, 2, &g) != 0,
              "no common control -> fail-closed");
    }
}

void test_overflow() {
    std::printf("[overflow]\n");
    // 回归：宽视场 + 粗样条下 bbox 角节点无支撑（bi[b]==-1）。
    // 修复前 H_data[ia*n_free-1] 越界写（heap-buffer-overflow → 段错误）。
    std::vector<P2SkySample> s;
    for (int k = 0; k < 3; ++k)
        for (int gy = 0; gy < 8; ++gy)
            for (int gx = 0; gx < 8; ++gx) {
                const std::uint64_t z = astrocs::healpix::xy_to_nested_local(
                    (std::uint32_t)(gx * 64 + 32), (std::uint32_t)(gy * 64 + 32), 9u);
                double ra = 0.0, dec = 0.0;
                astrocs::healpix::pix2ang_nest(512u, z, ra, dec);
                P2SkySample sm{};
                sm.frame_id = (std::uint64_t)k;
                sm.control_id = (std::uint64_t)(gy * 8 + gx);
                sm.ra_deg = ra; sm.dec_deg = dec;
                sm.value = 10.0 + 0.5 * std::sin((double)k + 0.01 * gx + 0.02 * gy);
                sm.variance = 1e-5; sm.snr = 0.0;
                sm.flags = P2_SKY_FLAG_NO_LOCAL_SNR;
                s.push_back(sm);
            }
    P2SkyPlaneConfig cfg = base_cfg();
    cfg.max_nodes = 200000;   // 越过节点数守卫，强制进入 H_data 组装（越界写路径）
    void* m = nullptr; char err[512] = {0};
    const int rc = p2_sky_plane_build(s.data(), s.size(), &cfg, &m, err, sizeof(err));
    std::printf("  wide-field build rc=%d (%s)\n", rc, err);
    check(rc == P2_SKY_PLANE_OK || rc == P2_SKY_PLANE_RANK_DEFICIENT ||
          rc == P2_SKY_PLANE_TOO_FEW_SAMPLES, "wide-field build fails closed without crash");
    if (rc == P2_SKY_PLANE_OK) p2_sky_plane_close(m);
}
}  // namespace

int main(int argc, char** argv) {
    const std::string t = argc > 1 ? argv[1] : "all";
    if (t == "patch" || t == "all") test_patch();
    if (t == "starmask" || t == "all") test_starmask();
    if (t == "recovery" || t == "all") test_recovery();
    if (t == "gauge" || t == "all") test_gauge();
    if (t == "weighting" || t == "all") test_weighting();
    if (t == "negative" || t == "all") test_negative();
    if (t == "memory" || t == "all") test_memory();
    if (t == "dcgain" || t == "all") test_dcgain();
    if (t == "overflow" || t == "all") test_overflow();
    if (t == "seam" || t == "all") test_seam();
    if (t == "gain" || t == "all") test_gain();
    if (t == "determinism" || t == "all") test_determinism();
    if (g_fail) { std::printf("RESULT: FAIL (%d checks)\n", g_fail); return 1; }
    std::printf("RESULT: PASS\n");
    return 0;
}
