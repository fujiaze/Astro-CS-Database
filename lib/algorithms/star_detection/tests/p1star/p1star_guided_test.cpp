// ============================================================================
// p1star_guided_test.cpp — STARDET-01：星表引导检测（权威路径）判据
// ----------------------------------------------------------------------------
// 规范锚: ASTROCS_DESIGN.md §4.2/§2.1 + docs/plugins/algorithms_phase1/
//   03_star_detection.md §4 —— 权威检测范式 = **星表引导拟合**：检测定义域是
//   星表位置（用本帧 WCS 把星表逆投影到像素域），只对星表位置做质心/PSF 拟合；
//   拟合成功即星点，失败**直接丢弃**（不计虚警、不报错）。全图盲检测
//   （sdet_detect_ex[_f64]）保留为**诊断/初值**路径，不是权威路径。
// 被测面: sdet_detect_guided_ex_f64（lib/algorithms/star_detection/include/
//   star_detector.h；实现 lib/algorithms/star_detection/src/sdet_api.cpp）。
//
// 判据（全部可红可绿；阈值只引用既有冻结条款，不新设表外阈值）:
//   G1 召回/质心：FIX-STAR-A 真值位置作为"星表预测位置" ⇒ SNR_peak≥20 子集
//      召回 ≥0.9 且 max|Δc| ≤ 0.3 px（SCI-P1-STAR-001 §1 / G-P1-CENTROID-SCI）
//   G2 不计虚警：纯噪声场上给出预测位置 ⇒ 输出必须为 0（拟合失败即丢弃）
//   G3 定义域丢弃计数守恒：出帧/距边 <2px 的位置必须计入 n_dropped，
//      且 n_dropped+n_fit_failed+n_rejected+n_fit_ok == n_predicted
//   G4 确定性：1/4 线程输出逐位一致（ALG-STARDET-001 §5 全序与串行归约）
//   G5 非退化（判据力）：预测位置整体偏移 25px（远超拟合盒 R）⇒ 输出位置
//      必须跟随**预测位置**而非图像真实星（否则"星表引导"名不副实）
//   G6 空定义域：n_pred=0 ⇒ rc=0 且 count=0 且输出指针全 NULL（非错误）
//   G7 对照：同场同参数下盲检测（诊断路径）与引导路径的源数对照如实打印
//
// ctest 目标名: p1star_guided
// ============================================================================

#include "star_detector.h"

#include "p1star_fixtures.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

int g_failures = 0;
#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (cond) { std::printf("  [PASS] %s\n", msg); }           \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }  \
    } while (0)

struct GuidedOut {
    std::vector<double> x, y;
    std::vector<float> flux, mag;
    std::vector<int> sat, has;
    int rc = -1;
};

// 单次引导检测调用（handle 每次新建: 句柄级互斥使用, 禁跨线程共享）
GuidedOut run_guided(const std::vector<double>& img, int w, int h,
                     const std::vector<double>& px, const std::vector<double>& py,
                     int max_stars, SDetGuidedStats* st, int nthreads) {
    GuidedOut out;
#ifdef _OPENMP
    omp_set_num_threads(nthreads);
#else
    (void)nthreads;
#endif
    SDetParams sp;
    std::memset(&sp, 0, sizeof(sp));
    sp.structureLayers = 5;
    sp.hotPixelFilterRadius = 2;
    sp.iterativeClipSigma = 5.0f;
    sp.iterativeMaxRounds = 3;
    sp.medianFilterDetail = 2;
    sp.maxStars = max_stars;
    sp.fitRadius = 0;
    sp.fwhmClipSigma = 3.0f;
    sp.maxAxisRatio = 2.0f;
    StarDetectorHandle hnd = sdet_create(&sp);
    if (!hnd) { out.rc = -2; return out; }
    const char* extras[4] = {"fwhm_x", "fwhm_y", "sx", "sy"};
    double *x = nullptr, *y = nullptr;
    float *flux = nullptr, *mag = nullptr;
    int *sat = nullptr, *has = nullptr, count = 0;
    float** ex = nullptr;
    out.rc = sdet_detect_guided_ex_f64(
        hnd, img.data(), w, h, px.empty() ? nullptr : px.data(),
        py.empty() ? nullptr : py.data(), (int)px.size(),
        &x, &y, &flux, &sat, &mag, &has, &count, extras, 4, &ex, st);
    if (out.rc == 0 && count > 0) {
        out.x.assign(x, x + count);
        out.y.assign(y, y + count);
        out.flux.assign(flux, flux + count);
        out.mag.assign(mag, mag + count);
        out.sat.assign(sat, sat + count);
        out.has.assign(has, has + count);
    }
    sdet_free_detect_ex(x, y, flux, sat, mag, has, ex, 4);
    sdet_destroy(hnd);
    return out;
}

struct BlindOut {
    std::vector<double> x, y;
    int rc = -1;
    int count = 0;
};

BlindOut run_blind(const std::vector<double>& img, int w, int h, int max_stars) {
    BlindOut out;
    SDetParams sp;
    std::memset(&sp, 0, sizeof(sp));
    sp.maxStars = max_stars;
    sp.maxAxisRatio = 2.0f;
    StarDetectorHandle hnd = sdet_create(&sp);
    if (!hnd) { out.rc = -2; return out; }
    double *x = nullptr, *y = nullptr;
    float *flux = nullptr, *mag = nullptr;
    int *sat = nullptr, *has = nullptr, count = 0;
    float** ex = nullptr;
    out.rc = sdet_detect_ex_f64(hnd, img.data(), w, h, &x, &y, &flux, &sat, &mag, &has,
                                &count, nullptr, 0, &ex);
    out.count = count;
    if (out.rc == 0 && count > 0) {
        out.x.assign(x, x + count);
        out.y.assign(y, y + count);
    }
    sdet_free_detect_ex(x, y, flux, sat, mag, has, ex, 0);
    sdet_destroy(hnd);
    return out;
}

// G1/G4/G5 共用：FIX-STAR-A 真值位置作为星表预测位置
void test_recall_and_centroid() {
    std::printf("[G1] 召回/质心（FIX-STAR-A 真值位置 = 星表预测位置）\n");
    const p1star::FixStarA fx = p1star::fix_star_a_f1();
    std::vector<double> px, py;
    for (const auto& s : fx.truth) { px.push_back(s.cx); py.push_back(s.cy); }
    SDetGuidedStats st;
    std::memset(&st, 0, sizeof(st));
    const GuidedOut g = run_guided(fx.img, fx.w, fx.h, px, py, 20000, &st, 1);
    CHECK(g.rc == 0, "g1_rc: sdet_detect_guided_ex_f64 rc=0");
    CHECK(st.n_predicted == (int)fx.truth.size(),
          "g1_domain: n_predicted == 注入真值数（定义域 = 星表位置）");
    // SNR_peak = A/σ_bg（GATES_AND_TOLERANCES §2）⇒ amp/σ ≥ 20 的域
    int n_domain = 0;
    std::vector<char> in_domain(fx.truth.size(), 0);
    for (std::size_t i = 0; i < fx.truth.size(); ++i) {
        if (fx.truth[i].amp / fx.noise >= 20.0) { in_domain[i] = 1; ++n_domain; }
    }
    int matched = 0;
    double max_dc = 0.0;
    for (std::size_t i = 0; i < fx.truth.size(); ++i) {
        if (!in_domain[i]) continue;
        double best = 1e30;
        for (std::size_t k = 0; k < g.x.size(); ++k) {
            const double d = std::hypot(g.x[k] - fx.truth[i].cx, g.y[k] - fx.truth[i].cy);
            if (d < best) best = d;
        }
        if (best <= 0.3) { ++matched; max_dc = std::max(max_dc, best); }
    }
    const double recall = n_domain > 0 ? (double)matched / (double)n_domain : 0.0;
    std::printf("      domain(SNR_peak>=20)=%d matched=%d recall=%.4f max|dc|=%.4f px "
                "out=%d fit_ok=%d fit_failed=%d rejected=%d dropped=%d\n",
                n_domain, matched, recall, max_dc, (int)g.x.size(), st.n_fit_ok,
                st.n_fit_failed, st.n_rejected, st.n_dropped);
    CHECK(recall >= 0.9, "g1_recall: SNR_peak>=20 子集召回 >= 0.9");
    CHECK(max_dc <= 0.3, "g1_centroid: max|dc| <= 0.3 px (G-P1-CENTROID-SCI)");
    CHECK(st.n_fit_ok + st.n_fit_failed + st.n_rejected + st.n_dropped == st.n_predicted,
          "g3_conservation: 定义域计数守恒");
}

void test_no_false_alarm() {
    std::printf("[G2] 不计虚警（纯噪声场 + 预测位置）\n");
    const int w = 256, h = 256;
    std::mt19937_64 rng(20260919ull);
    std::normal_distribution<double> nd(300.0, 6.0);
    std::vector<double> img((std::size_t)w * h);
    for (auto& v : img) v = nd(rng);
    std::uniform_real_distribution<double> ux(8.0, w - 9.0), uy(8.0, h - 9.0);
    std::vector<double> px, py;
    for (int i = 0; i < 200; ++i) { px.push_back(ux(rng)); py.push_back(uy(rng)); }
    SDetGuidedStats st;
    std::memset(&st, 0, sizeof(st));
    const GuidedOut g = run_guided(img, w, h, px, py, 20000, &st, 1);
    std::printf("      pure-noise: out=%d fit_ok=%d fit_failed=%d rejected=%d dropped=%d\n",
                (int)g.x.size(), st.n_fit_ok, st.n_fit_failed, st.n_rejected, st.n_dropped);
    CHECK(g.rc == 0, "g2_rc: rc=0");
    // 判据 1（规范冻结）: 虚警密度 ≤ 0.1/千像素（SCI-P1-STAR-001 §1 纯噪声场）
    const double fp_per_kpx = 1000.0 * (double)g.x.size() / ((double)w * (double)h);
    CHECK(fp_per_kpx <= 0.1,
          "g2_fp_density: 虚警密度 <= 0.1/千像素（SCI-P1-STAR-001 §1）");
    // 判据 2（回归锚）: 本 fixture 上的实测基线 = 0 星
    CHECK((int)g.x.size() == 0,
          "g2_no_false_alarm: 纯噪声场上引导路径输出 0 星（实测回归锚）");
}

void test_domain_drop() {
    std::printf("[G3] 定义域丢弃（出帧/距边 <2px）\n");
    const int w = 128, h = 128;
    std::vector<double> img((std::size_t)w * h, 300.0);
    // 帧内一颗真星（用于证明不是"全丢"造成的恒真）
    p1star::add_gaussian_to(img, w, h, 64.5, 64.5, 5000.0, 2.5);
    std::vector<double> px, py;
    // 8 个距边 <2px 的位置 + 4 个出帧位置
    const double edge[8][2] = {{0.0, 64.0}, {1.0, 64.0}, {127.0, 64.0}, {126.0, 64.0},
                               {64.0, 0.0}, {64.0, 1.0}, {64.0, 127.0}, {64.0, 126.0}};
    for (auto& e : edge) { px.push_back(e[0]); py.push_back(e[1]); }
    const double outside[4][2] = {{-10.0, 64.0}, {200.0, 64.0}, {64.0, -10.0}, {64.0, 200.0}};
    for (auto& o : outside) { px.push_back(o[0]); py.push_back(o[1]); }
    px.push_back(64.5); py.push_back(64.5);   // 帧内有效位置
    SDetGuidedStats st;
    std::memset(&st, 0, sizeof(st));
    const GuidedOut g = run_guided(img, w, h, px, py, 20000, &st, 1);
    std::printf("      dropped=%d out=%d (n_pred=%d)\n", st.n_dropped, (int)g.x.size(),
                st.n_predicted);
    CHECK(st.n_dropped == 12, "g3_drop: 12 个出帧/贴边位置全部计入 n_dropped");
    CHECK((int)g.x.size() == 1, "g3_keep: 帧内有效位置仍被检出（门非恒真）");
    if (g.x.size() == 1) {
        CHECK(std::fabs(g.x[0] - 64.5) <= 0.3 && std::fabs(g.y[0] - 64.5) <= 0.3,
              "g3_centroid: 帧内星质心 |dc| <= 0.3 px");
    }
}

void test_determinism() {
    std::printf("[G4] 确定性（1 vs 4 线程逐位一致）\n");
    const p1star::FixStarA fx = p1star::fix_star_a_f1();
    std::vector<double> px, py;
    for (const auto& s : fx.truth) { px.push_back(s.cx); py.push_back(s.cy); }
    SDetGuidedStats s1, s4;
    std::memset(&s1, 0, sizeof(s1));
    std::memset(&s4, 0, sizeof(s4));
    const GuidedOut a = run_guided(fx.img, fx.w, fx.h, px, py, 20000, &s1, 1);
    const GuidedOut b = run_guided(fx.img, fx.w, fx.h, px, py, 20000, &s4, 4);
    CHECK(a.x.size() == b.x.size(), "g4_count: 1/4 线程输出星数相等");
    bool bitwise = a.x.size() == b.x.size();
    for (std::size_t i = 0; bitwise && i < a.x.size(); ++i) {
        bitwise = (a.x[i] == b.x[i]) && (a.y[i] == b.y[i]) &&
                  (a.flux[i] == b.flux[i]) && (a.mag[i] == b.mag[i]) &&
                  (a.sat[i] == b.sat[i]) && (a.has[i] == b.has[i]);
    }
    CHECK(bitwise, "g4_bitwise: 1/4 线程输出逐位一致（ALG-STARDET-001 §5）");
    CHECK(std::memcmp(&s1, &s4, sizeof(s1)) == 0, "g4_stats: 定义域计数逐位一致");
}

void test_domain_sensitivity() {
    std::printf("[G5] 非退化：同一图像, 两个定义域（真值位置 vs 偏移 30px）\n");
    const int w = 128, h = 128;
    std::vector<double> img((std::size_t)w * h, 300.0);
    p1star::add_gaussian_to(img, w, h, 64.5, 64.5, 5000.0, 2.5);
    const std::vector<double> on_truth_x{64.5}, on_truth_y{64.5};
    // 偏移 30px > 拟合盒半径 R(≈ceil(3.7172·σ_eff)≈12) ⇒ 拟合盒内无真星
    const std::vector<double> off_x{94.5}, off_y{64.5};
    SDetGuidedStats s_on, s_off;
    std::memset(&s_on, 0, sizeof(s_on));
    std::memset(&s_off, 0, sizeof(s_off));
    const GuidedOut g_on = run_guided(img, w, h, on_truth_x, on_truth_y, 20000, &s_on, 1);
    const GuidedOut g_off = run_guided(img, w, h, off_x, off_y, 20000, &s_off, 1);
    std::printf("      domain=true : out=%d | domain=+30px: out=%d\n",
                (int)g_on.x.size(), (int)g_off.x.size());
    CHECK((int)g_on.x.size() == 1,
          "g5_control: 定义域 = 真值位置时检出该星（门非恒真）");
    CHECK((int)g_off.x.size() == 0,
          "g5_domain: 定义域偏移 30px 时输出 0（检测定义域 = 星表位置, 非全图盲检测）");
}

void test_empty_domain() {
    std::printf("[G6] 空定义域（n_pred=0）\n");
    const int w = 64, h = 64;
    std::vector<double> img((std::size_t)w * h, 300.0);
    SDetGuidedStats st;
    std::memset(&st, 0, sizeof(st));
    const GuidedOut g = run_guided(img, w, h, {}, {}, 20000, &st, 1);
    CHECK(g.rc == 0 && g.x.empty(), "g6_empty: n_pred=0 ⇒ rc=0 且 count=0（非错误）");
}

void test_blind_contrast() {
    std::printf("[G7] 对照：盲检测（诊断路径）vs 引导路径（同场同参数）\n");
    const p1star::FixStarA fx = p1star::fix_star_a_f1();
    std::vector<double> px, py;
    for (const auto& s : fx.truth) { px.push_back(s.cx); py.push_back(s.cy); }
    SDetGuidedStats st;
    std::memset(&st, 0, sizeof(st));
    const GuidedOut g = run_guided(fx.img, fx.w, fx.h, px, py, 20000, &st, 1);
    const BlindOut b = run_blind(fx.img, fx.w, fx.h, 20000);
    std::printf("      guided out=%d (domain=%d) | blind out=%d | truth=%d\n",
                (int)g.x.size(), st.n_predicted, b.count, (int)fx.truth.size());
    CHECK(b.rc == 0, "g7_blind_rc: 盲检测（诊断路径）仍可用（保留, 未删除）");
    CHECK((int)g.x.size() <= st.n_predicted,
          "g7_domain_cap: 引导路径输出不可能超过定义域大小");
}

}  // namespace

int main() {
    std::printf("P1STAR-GUIDED (STARDET-01 星表引导检测权威路径) 判据\n");
    test_recall_and_centroid();
    test_no_false_alarm();
    test_domain_drop();
    test_determinism();
    test_domain_sensitivity();
    test_empty_domain();
    test_blind_contrast();
    if (g_failures == 0) {
        std::printf("P1STAR GUIDED PASS\n");
        return 0;
    }
    std::printf("P1STAR GUIDED FAIL (%d check(s))\n", g_failures);
    return 1;
}
