// ============================================================================
// p1star_psf_shape_gate_test.cpp — R-58-1 点源形状门（O13b）回归锁
//
// 依据: run/FINAL-07/审核包/端到端/五帧越闸定性报告.md §6.1（R-58 裁决 1）
//   「修检测端: 半高宽落在点源扩宽比例区间内、峰占比上限、最小像素数;
//     负例 = 单像素尖峰 + 星云核延展区; 正例 = 真实暗源与 4 颗亮星的召回不得
//     下降（硬要求）」。本轮两条实测修正（都写进下方判据, 不藏）:
//     ① 被 O4a 热像元滤波拦掉的是**孤立单像素**; 真实帧里漏过全部 O13 门的是
//        「亮核 + 4 个正交弱卫星」这一族（拟合 fwhm 0.52-0.87 px）。负例用该族
//        （单独 1 px 在合成场里根本不出候选, 用它做断言是空断言）。
//     ② 「最小像素数」判据实测不能取半高: 本采样下 sigma 0.8 px 的**真实**窄星
//        半高等高线半径 1.177*sigma = 0.94 px < 1 px ⇒ n_half = 1, 与单像素尖峰
//        不可分; 改用 1/4 峰（半径 1.665*sigma）: 真实窄星 n = 5, 尖峰族 n = 1-2,
//        阈值 4 落在中间。默认 minQuarterMaxPixels = 4。
// 被测面: sdet_api.cpp 的 sdet_apply_psf_shape_gate<T>（盲路径 O13b, 在 O13
//   五码排异门之后、去重/截断之前）。
//
// 判据（能红能绿, 逐条可证伪）—— 每个用例成对跑「门 ON」与「门全关（-1）」:
//   RED 侧（门关 = 修复前行为）必须复现负例目标; 否则该 GREEN 断言是空断言;
//   GREEN 侧（门开）必须拒掉负例目标, 且保留集逐位 ⊆ 门关集合（纯过滤）。
// 合成场用固定 seed 的背景噪声（bg=216.2402, sigma=14.1382 —— 与生产帧 sdet
// 日志 "peaker: median=216.2402 bgnoise=14.1382" 同量级）。
//
// ctest 目标名: p1star_psf_shape_gate
// 被测源 = 与 p1star_tests 同一生产 TU 集（生产源即被测源, 无桩）。
// ============================================================================

#include "star_detector.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {
int g_failures = 0;
#define CHECK(cond, msg)                                                   \
    do {                                                                    \
        if (cond) { std::printf("  [PASS] %s\n", msg); }                    \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }           \
    } while (0)

const int W = 128, H = 128;
const double BG = 216.2402;
const double BGN = 14.1382;
const unsigned long long SEED = 20260929ULL;

std::vector<double> base_image(unsigned long long seed) {
    std::vector<double> img((size_t)W * H, BG);
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> nd(0.0, BGN);
    for (size_t i = 0; i < img.size(); ++i) img[i] += nd(rng);
    return img;
}

void add_gauss(std::vector<double>& img, double cx, double cy, double amp,
               double s) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const double dx = (x + 0.5) - cx, dy = (y + 0.5) - cy;
            img[(size_t)y * W + x] +=
                amp * std::exp(-0.5 * ((dx / s) * (dx / s) + (dy / s) * (dy / s)));
        }
}

// 生产帧实测的尖峰族形态: 亮核 + 4 个正交弱卫星（拟合 fwhm 0.52-0.87 px）
void add_spike_cluster(std::vector<double>& img, int ix, int iy) {
    img[(size_t)iy * W + ix] += 26000.0;
    img[(size_t)iy * W + ix + 1] += 2000.0;
    img[(size_t)iy * W + ix - 1] += 2000.0;
    img[(size_t)(iy + 1) * W + ix] += 2000.0;
    img[(size_t)(iy - 1) * W + ix] += 2000.0;
}

struct StarOut { double x, y; double fx, fy; };

int run_detect(const std::vector<double>& img, float lo, float hi, float pf,
               int nq, std::vector<StarOut>* out) {
    SDetParams p;
    std::memset(&p, 0, sizeof(p));
    p.structureLayers = 5;
    p.hotPixelFilterRadius = 1;
    p.iterativeClipSigma = 9.0f;
    p.iterativeMaxRounds = 5;
    p.medianFilterDetail = 1;
    p.maxStars = 2000;
    p.fitRadius = 6;
    p.fwhmClipSigma = 3.0f;
    p.maxAxisRatio = 2.0f;
    p.psfFwhmLoRatio = lo;
    p.psfFwhmHiRatio = hi;
    p.maxPeakFraction = pf;
    p.minQuarterMaxPixels = nq;
    StarDetectorHandle h = sdet_create(&p);
    if (!h) return -1;
    double *x = nullptr, *y = nullptr;
    float *fl = nullptr, *mg = nullptr;
    int *sa = nullptr, *hs = nullptr;
    int cnt = -1;
    const char* en[2] = {"fwhm_x", "fwhm_y"};
    float** ex = nullptr;
    const int rc = sdet_detect_ex_f64(h, img.data(), W, H, &x, &y, &fl, &sa,
                                      &mg, &hs, &cnt, en, 2, &ex);
    sdet_destroy(h);
    if (rc != 0) return -1;
    out->clear();
    for (int i = 0; i < cnt; ++i) {
        StarOut s;
        s.x = x[i]; s.y = y[i];
        s.fx = ex ? (double)ex[0][i] : 0.0;
        s.fy = ex ? (double)ex[1][i] : 0.0;
        out->push_back(s);
    }
    sdet_free_detect_ex(x, y, fl, sa, mg, hs, ex, 2);
    return cnt;
}

bool has_near(const std::vector<StarOut>& v, double cx, double cy, double tol) {
    for (size_t i = 0; i < v.size(); ++i)
        if (std::fabs(v[i].x - cx) <= tol && std::fabs(v[i].y - cy) <= tol) return true;
    return false;
}

int subset_count(const std::vector<StarOut>& a, const std::vector<StarOut>& b) {
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i)
        for (size_t j = 0; j < b.size(); ++j)
            if (a[i].x == b[j].x && a[i].y == b[j].y) { ++n; break; }
    return n;
}

void dump(const char* tag, const std::vector<StarOut>& v) {
    for (size_t i = 0; i < v.size(); ++i)
        std::printf("  [info] %-12s (%.2f,%.2f) fwhm=(%.2f,%.2f)\n",
                    tag, v[i].x, v[i].y, v[i].fx, v[i].fy);
}

const float OFF = -1.0f;
const int   OFF_I = -1;
}  // namespace

int main() {
    std::printf("[R-58-1] 点源形状门（O13b）回归锁\n");

    // ── C1 尖峰族负例（亮核 + 4 正交弱卫星）────────────────────────────
    {
        std::vector<double> img = base_image(SEED);
        add_gauss(img, 30.5, 30.5, 5000.0, 1.2);
        add_gauss(img, 95.5, 95.5, 5000.0, 1.2);
        add_gauss(img, 20.5, 100.5, 5000.0, 1.2);
        add_gauss(img, 100.5, 20.5, 5000.0, 1.2);
        add_spike_cluster(img, 64, 64);
        std::vector<StarOut> on, off;
        run_detect(img, 0.0f, 0.0f, 0.0f, 0, &on);
        run_detect(img, OFF, OFF, OFF, OFF_I, &off);
        std::printf("  [info] C1 门ON n=%zu, 门OFF n=%zu\n", on.size(), off.size());
        dump("C1_OFF", off);
        dump("C1_ON", on);
        CHECK(has_near(off, 64.0, 64.0, 1.5),
              "c1_red_control: 门关时尖峰族确实出星（负例非空断言）");
        CHECK(!has_near(on, 64.0, 64.0, 1.5), "c1_green: 门开时尖峰族被拒");
        CHECK(has_near(on, 30.5, 30.5, 1.5) && has_near(on, 95.5, 95.5, 1.5) &&
              has_near(on, 20.5, 100.5, 1.5) && has_near(on, 100.5, 20.5, 1.5),
              "c1_recall: 同场 4 颗真实 PSF 星全部保留");
        CHECK(subset_count(on, off) == (int)on.size(),
              "c1_subset: 门开保留集逐位包含于门关集合（纯过滤, 不扰动幸存者）");
    }

    // ── C2 星云核延展区负例（弥散源 + 10 颗真实星, 参考取真实星）────────
    {
        std::vector<double> img = base_image(SEED + 1);
        add_gauss(img, 64.0, 64.0, 12000.0, 20.0);      // fwhm ≈ 47 px
        const double px[10] = {14.5, 34.5, 54.5, 74.5, 94.5, 114.5, 24.5, 44.5, 84.5, 104.5};
        const double py[10] = {14.5, 14.5, 14.5, 14.5, 14.5, 14.5, 112.5, 112.5, 112.5, 112.5};
        for (int i = 0; i < 10; ++i) add_gauss(img, px[i], py[i], 6000.0, 1.2);
        std::vector<StarOut> on, off;
        run_detect(img, 0.0f, 0.0f, 0.0f, 0, &on);
        run_detect(img, OFF, OFF, OFF, OFF_I, &off);
        std::printf("  [info] C2 门ON n=%zu, 门OFF n=%zu\n", on.size(), off.size());
        dump("C2_OFF", off);
        dump("C2_ON", on);
        int off_real = 0, on_real = 0;
        for (int i = 0; i < 10; ++i) {
            if (has_near(off, px[i], py[i], 1.5)) {
                ++off_real;
                if (has_near(on, px[i], py[i], 1.5)) ++on_real;
            }
        }
        std::printf("  [info] C2 真实星: 门关 %d/10, 门开 %d\n", off_real, on_real);
        CHECK(has_near(off, 64.0, 64.0, 10.0),
              "c2_red_control: 门关时星云核延展区被当成星点输出（负例非空断言）");
        CHECK(!has_near(on, 64.0, 64.0, 10.0),
              "c2_green: 门开时星云核延展区被拒（fwhm 47 px 远超 2.5*f0）");
        CHECK(off_real >= 8, "c2_red_baseline: 门关时弥散场中至少 8 颗真实星被检出");
        CHECK(on_real == off_real, "c2_recall: 弥散场中真实星召回不下降（硬要求）");
        CHECK(subset_count(on, off) == (int)on.size(),
              "c2_subset: 门开保留集逐位包含于门关集合（纯过滤）");
    }

    // ── C3 召回正例（12 颗, 硬要求）────────────────────────────────────
    {
        std::vector<double> img = base_image(SEED + 2);
        for (int i = 0; i < 12; ++i) {
            const double cx = 12.5 + 9.0 * (i % 4);
            const double cy = 20.5 + 22.0 * (i / 4);
            add_gauss(img, cx, cy, 1500.0 + 600.0 * i, 0.80 + 0.075 * i);
        }
        std::vector<StarOut> on, off;
        run_detect(img, 0.0f, 0.0f, 0.0f, 0, &on);
        run_detect(img, OFF, OFF, OFF, OFF_I, &off);
        int kept = 0, base = 0;
        for (int i = 0; i < 12; ++i) {
            const double cx = 12.5 + 9.0 * (i % 4);
            const double cy = 20.5 + 22.0 * (i / 4);
            if (has_near(off, cx, cy, 1.5)) { ++base; if (has_near(on, cx, cy, 1.5)) ++kept; }
        }
        std::printf("  [info] C3 门ON n=%zu, 门OFF n=%zu | 12 颗中 门关 %d 门开 %d\n",
                    on.size(), off.size(), base, kept);
        dump("C3_OFF", off);
        CHECK(base == 12, "c3_red_baseline: 门关时 12 颗全部检出（召回判据非退化）");
        CHECK(kept == 12, "c3_recall: 门开时 12 颗全部召回（sigma 0.80..1.625）");
        CHECK(on.size() == off.size(), "c3_no_loss: 纯 PSF 场门开/门关输出数相同");
    }

    // ── C4 上限门不误杀边缘宽星（sigma 2.0 → fwhm 4.71 < 2.5*f0）────────
    {
        std::vector<double> img = base_image(SEED + 3);
        add_gauss(img, 64.5, 64.5, 8000.0, 2.0);
        add_gauss(img, 24.5, 30.5, 5000.0, 1.2);
        add_gauss(img, 100.5, 100.5, 5000.0, 1.2);
        std::vector<StarOut> on;
        run_detect(img, 0.0f, 0.0f, 0.0f, 0, &on);
        dump("C4_ON", on);
        CHECK(has_near(on, 64.5, 64.5, 1.5),
              "c4_no_false_reject: sigma 2.0 宽星保留（比例窗上限有裕度）");
    }

    // ── C5 帧相对性: 同一尖峰族在窄 PSF 场与宽 PSF 场都被拒 ─────────────
    {
        std::vector<double> narrow = base_image(SEED + 4);
        add_gauss(narrow, 30.5, 30.5, 5000.0, 0.9);
        add_gauss(narrow, 95.5, 30.5, 5000.0, 0.9);
        add_spike_cluster(narrow, 64, 90);
        std::vector<double> wide = base_image(SEED + 5);
        add_gauss(wide, 30.5, 30.5, 5000.0, 1.8);
        add_gauss(wide, 95.5, 30.5, 5000.0, 1.8);
        add_spike_cluster(wide, 64, 90);
        std::vector<StarOut> on_n, on_w, off_n, off_w;
        run_detect(narrow, 0.0f, 0.0f, 0.0f, 0, &on_n);
        run_detect(wide, 0.0f, 0.0f, 0.0f, 0, &on_w);
        run_detect(narrow, OFF, OFF, OFF, OFF_I, &off_n);
        run_detect(wide, OFF, OFF, OFF, OFF_I, &off_w);
        std::printf("  [info] C5 窄场 ON=%zu OFF=%zu | 宽场 ON=%zu OFF=%zu\n",
                    on_n.size(), off_n.size(), on_w.size(), off_w.size());
        dump("C5_OFF_narrow", off_n);
        dump("C5_OFF_wide", off_w);
        CHECK(has_near(off_n, 64.0, 90.0, 1.5) && has_near(off_w, 64.0, 90.0, 1.5),
              "c5_red_control: 两种 PSF 宽度下场关都出尖峰族");
        CHECK(!has_near(on_n, 64.0, 90.0, 1.5) && !has_near(on_w, 64.0, 90.0, 1.5),
              "c5_frame_relative: 两种 PSF 宽度下场开都拒尖峰族（阈值随帧, 不写死）");
        CHECK(has_near(on_n, 30.5, 30.5, 1.5) && has_near(on_w, 30.5, 30.5, 1.5),
              "c5_recall: 两种场下真实星（含 sigma 0.9 窄星）均保留");
    }

    // ── C6 子门分解: 三个子门各自单独生效（可归因, 非合并判据）──────────
    {
        std::vector<double> img = base_image(SEED + 6);
        add_gauss(img, 30.5, 30.5, 5000.0, 1.2);
        add_gauss(img, 95.5, 95.5, 5000.0, 1.2);
        add_gauss(img, 20.5, 100.5, 5000.0, 1.2);
        add_gauss(img, 100.5, 20.5, 5000.0, 1.2);
        add_spike_cluster(img, 64, 64);
        std::vector<StarOut> only_lo, only_hi, only_pf, only_nq;
        run_detect(img, 0.0f, OFF, OFF, OFF_I, &only_lo);
        run_detect(img, OFF, 0.0f, OFF, OFF_I, &only_hi);
        run_detect(img, OFF, OFF, 0.0f, OFF_I, &only_pf);
        run_detect(img, OFF, OFF, OFF, 0, &only_nq);
        std::printf("  [info] C6 单开 下限窗=%zu 上限窗=%zu 峰占比=%zu 1/4峰像素=%zu\n",
                    only_lo.size(), only_hi.size(), only_pf.size(), only_nq.size());
        CHECK(!has_near(only_lo, 64.0, 64.0, 1.5), "c6_lo: 仅下限比例窗即可拒尖峰族");
        CHECK(!has_near(only_pf, 64.0, 64.0, 1.5), "c6_pf: 仅峰占比上限即可拒尖峰族");
        CHECK(!has_near(only_nq, 64.0, 64.0, 1.5), "c6_nq: 仅最小像素数即可拒尖峰族");
        CHECK(has_near(only_hi, 30.5, 30.5, 1.5) && has_near(only_hi, 95.5, 95.5, 1.5) &&
              has_near(only_hi, 20.5, 100.5, 1.5) && has_near(only_hi, 100.5, 20.5, 1.5),
              "c6_hi_recall: 仅上限比例窗时 4 颗真实星全部保留");
    }

    if (g_failures == 0) std::printf("R58-1 PSF SHAPE GATE: PASS\n");
    else std::printf("R58-1 PSF SHAPE GATE: FAIL (%d)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
