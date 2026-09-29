// ============================================================================
// p1star_reject_gate_test.cpp — REVIEW-stage3-SDet R1/R2/R3 共址回归锁
//
// 被验条目 (run/全仓-01/review/review-stage3-sdet.md 驳回项):
//   R1: O13 五码排异门在盲路径装配环缺失 + reject_star 第 5 判式
//       (SF_FWHM_TOO_LARGE) 被删。对齐 ALG
//       docs/science/algorithms/STAR_DETECTION_ALGORITHMS.md §3 :143-148、
//       设计稿 docs/detail/STAR_DETECTION_IMPL_DESIGN.md §5.13。
//   R2: 装配环 maxAxisRatio 死块 (只计算 smax/smin 未判定) 激活
//       (ALG :2282-2284; guided 活门对称)。
//   R3: O8 冻结语义恢复——11x11 局部极大扫描 (±5 全图窗、平局决胜左上、
//       域 [5,h-5)x[5,w-5)) 取代改构"deblend 叶内最大像素每叶一候选"。
//
// 回归锁语义 (未修复必红 / 已修复必绿):
//   RED  (修复前): 构造 1/2/3 各产出 1 星 (实测定案值, 见各构造注释);
//        构造 7/8 在改构语义下分别产出 3/2 星。
//   GREEN(修复后): 构造 1 被 R2 门拦 (smax/smin≈11.4 > 2.0);
//        构造 2 被 O13 码 3 拦 (roundness 0.4615 < 0.5, maxAxisRatio=0
//        关闭 R2 门隔离归因);
//        构造 3 被 O13 码 5 拦 (se_smax≈2.5 → fwhm_limit≈6.545 < 10.06);
//        构造 4 RMSE 门在场锁 (count=0 不变, 防门被删);
//        构造 5 绿例锚 (门不误杀正常星, 判别力反面);
//        构造 6 Q8 距 12 双星窗外并存 (R3 绿例);
//        构造 7 冻结 O8 语义锁: sigma_eff=sqrt(1.5^2+2^2)=2.5 平滑下距 7
//        不等幅三峰的弱峰被强坡吞没 (M 坡 x=45 处 3309 > S 峰 3079,
//        解析复算) → 局部极大唯一 → count=1 (改构旧行为 3 星必红);
//        构造 8 平局决胜锁: 等幅距 6 双峰像素位距 5 → 窗内等值 → 左上
//        先到者胜 → count=1 (改构旧行为 2 星必红);
//        构造 9 guided 路径同源生效: M5 双尺度预测位 rejected>=1。
//
// 判别力: 全部负例在修复前实测出星 (探针 run/stage3-fix/probe 定案),
// 非恒真门; 绿例锚定门的上界 (不误杀)。
//
// ctest 目标名: p1star_reject_gate
// 日期: 2026-09-15 (REVIEW-stage3-SDet R1/R2/R3 闭环)
// ============================================================================

#include "star_detector.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
int g_failures = 0;
#define CHECK(cond, msg)                                                  \
    do {                                                                  \
        if (cond) { std::printf("  [PASS] %s\n", msg); }                 \
        else { std::printf("  [FAIL] %s\n", msg); ++g_failures; }        \
    } while (0)

const int W = 96, H = 96;
const double BG = 100.0;

void add_gauss(std::vector<double>& img, double cx, double cy, double amp,
               double sx, double sy) {
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            const double dx = (x + 0.5) - cx, dy = (y + 0.5) - cy;
            img[(size_t)y * W + x] +=
                amp * std::exp(-0.5 * ((dx / sx) * (dx / sx) + (dy / sy) * (dy / sy)));
        }
}

// 盲路径检测 (FP64 全程), 返回 count; extras 取 fwhm_x/fwhm_y 供判别输出
int detect_blind(const std::vector<double>& img, float max_axis_ratio,
                 int* count_out) {
    // 值初始化（非 memset）: 四个形状门子参数取 0 = 用内置默认，与生产 memset(0)
    // 后逐位一致。原先 SDetParams p; 未初始化 → 读栈垃圾 → 形状门用随机阈值判星（UB）。
    SDetParams p{};
    p.structureLayers = 5; p.hotPixelFilterRadius = 1;
    p.iterativeClipSigma = 9.0f; p.iterativeMaxRounds = 5;
    p.medianFilterDetail = 1; p.maxStars = 2000;
    p.fitRadius = 6; p.fwhmClipSigma = 3.0f; p.maxAxisRatio = max_axis_ratio;
    StarDetectorHandle h = sdet_create(&p);
    double *x = nullptr, *y = nullptr;
    float *fl = nullptr, *mg = nullptr;
    int *sa = nullptr, *hs = nullptr;
    int cnt = -1;
    const char* en[2] = {"fwhm_x", "fwhm_y"};
    float** ex = nullptr;
    const int rc = sdet_detect_ex_f64(h, img.data(), W, H, &x, &y, &fl, &sa,
                                      &mg, &hs, &cnt, en, 2, &ex);
    sdet_destroy(h);
    if (rc != 0) { cnt = -1; }
    else if (rc == 0 && cnt > 0 && ex) {
        std::printf("    [info] star0 fwhm=(%.3f,%.3f)\n", ex[0][0], ex[1][0]);
    }
    if (rc == 0) sdet_free_detect_ex(x, y, fl, sa, mg, hs, ex, 2);
    *count_out = cnt;
    return rc;
}
}  // namespace

int main() {
    std::printf("[REVIEW-stage3-SDet] R1/R2/R3 排异门与 O8 冻结语义回归锁\n");
    std::vector<double> base((size_t)W * H, BG);
    const double CY = 47.5;

    // ── 1. FIX-R2: maxAxisRatio 门 (ALG :2282-2284) ─────────────────────
    // 行条 sigma=(0.35,4.0) 拟合塌缩 fwhm=(0.824,9.419) ratio≈11.4 > 2.0;
    // 修复前死块放行出 1 星, 修复后拦 → count=0。
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 500.0, 0.35, 4.0);
        int cnt = -1;
        const int rc = detect_blind(img, 2.0f, &cnt);
        CHECK(rc == 0 && cnt == 0,
              "r2_axis_ratio_gate: 行条轴比 11.4>2.0 被拦 (修复前 1 星)");
    }

    // ── 2. FIX-R1 码 3: SF_ROUNDNESS_BELOW_CRIT ─────────────────────────
    // 椭圆 sigma=(2.6,1.2) 拟合 fwhm=(6.123,2.826) roundness=2.826/6.123
    // =0.4615 < 0.5; maxAxisRatio=0 关闭 R2 门隔离码 3 归因
    // (修复前 1 星, RMSE 未拦 = 消元)。
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 3000.0, 2.6, 1.2);
        int cnt = -1;
        const int rc = detect_blind(img, 0.0f, &cnt);
        CHECK(rc == 0 && cnt == 0,
              "r1_code3_roundness: 圆度 0.4615<0.5 被拦 (修复前 1 星)");
    }

    // ── 3. FIX-R1 码 5: SF_FWHM_TOO_LARGE (第 5 判式恢复) ────────────────
    // 双尺度星: 核 sigma=1.5 amp=2800 + 裙 sigma=9.0 amp=900 → 拟合
    // fwhm=10.059; 候选零交叉读数 se_smax≈2.5 > 2.0 → fwhm_limit =
    // 2.5*2.3548200450309493*(1+0.5*ln(1.25))≈6.545 < 10.059 → 拦
    // (修复前四判式全过出 1 星)。
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 2800.0, 1.5, 1.5);
        add_gauss(img, 47.5, CY, 900.0, 9.0, 9.0);
        int cnt = -1;
        const int rc = detect_blind(img, 2.0f, &cnt);
        CHECK(rc == 0 && cnt == 0,
              "r1_code5_fwhm_limit: 拟合 fwhm 10.06 > 零交叉上限 6.55 被拦 "
              "(修复前 1 星)");
    }

    // ── 4. FIX-R1 码 4: SF_RMSE_TOO_LARGE 在场锁 ─────────────────────────
    // 中心星 sigma=2.0 amp=400 + 环带 r∈[3.5,5] amp=2800 → 结构残差巨大,
    // RMSE 门活拦 count=0 (修复前后同值; 锁"门在场", 防将来误删)。
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 400.0, 2.0, 2.0);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const double dx = (x + 0.5) - 47.5, dy = (y + 0.5) - CY;
                const double r = std::sqrt(dx * dx + dy * dy);
                if (r >= 3.5 && r <= 5.0) img[(size_t)y * W + x] += 2800.0;
            }
        int cnt = -1;
        const int rc = detect_blind(img, 2.0f, &cnt);
        CHECK(rc == 0 && cnt == 0, "r1_code4_rmse_lock: RMSE 门在场 (环星拦)");
    }

    // ── 5. 绿例锚: 门不误杀正常星 (判别力反面) ───────────────────────────
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 3000.0, 1.5, 1.5);
        int cnt = -1;
        const int rc = detect_blind(img, 2.0f, &cnt);
        CHECK(rc == 0 && cnt == 1,
              "green_normal_star: 单星正常出表 count=1 (门不误杀)");
    }

    // ── 6. FIX-R3 绿例: 窗外双星并存 ────────────────────────────────────
    // sigma=0.9 双星连续距 12 → 像素位距 11 >> 5 窗 → 两局部极大并存。
    {
        auto img = base;
        add_gauss(img, 41.5, CY, 3000.0, 0.9, 0.9);
        add_gauss(img, 53.5, CY, 3000.0, 0.9, 0.9);
        int cnt = -1;
        const int rc = detect_blind(img, 2.0f, &cnt);
        CHECK(rc == 0 && cnt == 2, "r3_far_pair: 距 12 双星并存 count=2");
    }

    // ── 7. FIX-R3 行为锁: 冻结 O8 吞没弱峰 (改构 3 星必红) ───────────────
    // 三峰链 sigma=1.5 距 7: sigma_eff=sqrt(1.5^2+2^2)=2.5 平滑下弱峰
    // (3000/2500) 落在强峰 (4000) 平滑坡上且坡值更高 → 非局部极大。
    {
        auto img = base;
        add_gauss(img, 43.0, CY, 4000.0, 1.5, 1.5);
        add_gauss(img, 50.0, CY, 3000.0, 1.5, 1.5);
        add_gauss(img, 57.0, CY, 2500.0, 1.5, 1.5);
        int cnt7 = -1;
        const int rc7 = detect_blind(img, 2.0f, &cnt7);
        CHECK(rc7 == 0 && cnt7 == 1,
              "r3_frozen_o8_swallow: 冻结 O8 弱峰吞没 count=1 (改构 3 星)");
    }

    // ── 8. FIX-R3 平局决胜锁: 等值双峰左上胜 (改构 2 星必红) ─────────────
    // 等幅 sigma=1.5 距 6 → 像素位 44/49 距 5 → 窗内等值 → 左上先到者胜。
    {
        auto img = base;
        add_gauss(img, 44.0, CY, 3000.0, 1.5, 1.5);
        add_gauss(img, 50.0, CY, 3000.0, 1.5, 1.5);
        int cnt8 = -1;
        const int rc8 = detect_blind(img, 2.0f, &cnt8);
        CHECK(rc8 == 0 && cnt8 == 1,
              "r3_tie_break_upper_left: 等值双峰平局决胜 count=1 (改构 2 星)");
    }

    // ── 9. FIX-R1 guided 同源: M5 双尺度预测位被拒 ───────────────────────
    {
        auto img = base;
        add_gauss(img, 47.5, CY, 2800.0, 1.5, 1.5);
        add_gauss(img, 47.5, CY, 900.0, 9.0, 9.0);
        // 同上: 值初始化，四个形状门子参数取 0 = 用内置默认（原为未初始化 ⇒ UB）。
        SDetParams p{};
        p.structureLayers = 5; p.hotPixelFilterRadius = 1;
        p.iterativeClipSigma = 9.0f; p.iterativeMaxRounds = 5;
        p.medianFilterDetail = 1; p.maxStars = 2000;
        p.fitRadius = 6; p.fwhmClipSigma = 3.0f; p.maxAxisRatio = 2.0f;
        StarDetectorHandle h = sdet_create(&p);
        double px[1] = {47.5}, py[1] = {CY};
        double *x = nullptr, *y = nullptr;
        float *fl = nullptr, *mg = nullptr;
        int *sa = nullptr, *hs = nullptr;
        int cnt = -1;
        SDetGuidedStats st;
        std::memset(&st, 0, sizeof(st));
        const int rc = sdet_detect_guided_ex_f64(h, img.data(), W, H, px, py, 1,
                                                 &x, &y, &fl, &sa, &mg, &hs,
                                                 &cnt, nullptr, 0, nullptr, &st);
        sdet_destroy(h);
        const int rejected = st.n_rejected;
        if (rc == 0) sdet_free_detect_ex(x, y, fl, sa, mg, hs, nullptr, 0);
        CHECK(rc == 0 && rejected >= 1,
              "r1_guided_same_gate: guided 路径 M5 预测位被排异门拒");
    }

    std::printf("\n[%s] failures=%d\n", (g_failures == 0 ? "PASS" : "FAIL"),
                g_failures);
    return g_failures == 0 ? 0 : 1;
}
