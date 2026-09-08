// ============================================================================
// sdet_saturation_cursor_test.cpp - 饱和星 edge-walking 游标污染验证
// (NON_PRODUCTION_TOOL_ONLY, P1-2 共址单测)
//
// 缺陷机理 (lib/star_detector/src/sdet_api.cpp peaker 阶段):
//   外层扫描 for (int y...) for (int x = r; x < width - r; x++) 中,
//   检出候选后先 `x += r` (跳 r 像素, star_finder.c:310 上游原设计), 随后
//   `int xx = x - r; int yy = y;` 单独另算局部中心。饱和分支 edge-walking
//   结束处原实现 `x += xr;` 把"候选到饱和区右缘的距离"回写进外层游标 x,
//   而该偏移已由局部中心变量消费 (xx += (xr+xl)/2, yy += (yu+yd)/2),
//   回写导致锚点所在行 (同一 y) 的后续扫描区间 [x+1, x+xr] 被跳过 →
//   候选检出集合漂移 (锚点同行、平台右缘方向的亮星漏检)。
//   修复: 删除 `x += xr;` 回写, 偏移只作用于局部中心 xx/yy。
//
// 实证几何 (由插桩版逐锚点观测确认, 参数见 run/local/bughunt_p1_batchN/p1_sdet/):
//   场景2: 平台 x,y ∈ [80,160] (81x81), Vp=47000 (仍走饱和分支:
//     meanhigh-bg = 46200 >= minsatlevel 45314; 平台恒值 → pixel0-minhigh=0
//     <= satrange 6473)。平台首个候选锚点 = (100,101) (平滑层 11x11 局部
//     极大 + 平局决胜的唯一点), edge-walking 得 xr=58 → 修复前 y=101 行
//     扫描游标从 x=100 直接跳到 158, 区间 [101,158] 被跳过。
//   普通星 B: A=8000, sigma=2, 中心 (135.5,101.5) — 叠加在平台上的亮星
//     (峰值 55000 < 65535 不截断), 整数峰 (135,101) 恰在修复前被跳过的
//     区间 [101,158] 内; 其平滑峰 (~平台47000 + ~4000) 使 (135,101) 成为
//     11x11 窗口内唯一局部极大; B 峰值 55000 - 平台 47000 = 8000 >
//     satrange 6473 → B 自身走非饱和分支 (has_saturated=0)。
//     修复前: (135,101) 未被扫描 → B 漏检 (n=1, 仅对照星);
//     修复后: 游标不回写, (135,101) 正常扫描 → B 检出 (n=2)。
//   对照星 C: 高斯星 (A=3000, sigma=3), 中心 (60.5,200.5), 远离平台,
//     修复前后均应检出 (证明修复不改变正常行为)。
//
// 场景1: 全饱和平台 (65535) x,y ∈ [80,140] + 远处对照星 C'(200.5,60.5),
//   不触发"锚点同行后续候选"路径, 作为基线回归 (平台候选被下游 Moffat4
//   拟合丢弃为既有算法行为, 不断言平台自身)。
//
// 断言:
//   场景1: rc=0 且对照星 C' 检出;
//   场景2: rc=0; B 在 (135.5,101.5) 附近检出 (修复前漂移: FAIL);
//          对照星 C 检出; 最终星表同时含 B 与 C 各 1 条。
// 风格对齐 sdet_fp64_test.cpp (CHECK 宏 + printf)。
// ============================================================================
#include "star_detector.h"
#include <cstdio>
#include <cmath>
#include <vector>
#include <cstring>
#include <cstdlib>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

static void add_gaussian_star(std::vector<float> &img, int W, int H,
                              double cx, double cy, double A, double sigma) {
    for (int y = -12; y <= 12; y++) {
        for (int x = -12; x <= 12; x++) {
            int px = (int)cx + x, py = (int)cy + y;
            if (px < 0 || py < 0 || px >= W || py >= H) continue;
            double dx = px + 0.5 - cx, dy = py + 0.5 - cy;
            double v = A * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
            double cur = img[(size_t)py * W + px];
            img[(size_t)py * W + px] = (float)(cur + v);
        }
    }
}

static bool has_star_near(const double *xs, const double *ys, int n,
                          double tx, double ty, double tol, double *dist_out) {
    double best = 1e30;
    for (int i = 0; i < n; i++) {
        double d = std::sqrt((xs[i] - tx) * (xs[i] - tx) + (ys[i] - ty) * (ys[i] - ty));
        if (d < best) best = d;
    }
    if (dist_out) *dist_out = best;
    return best <= tol;
}

int main() {
    const int W = 256, H = 256;
    const double BG = 800.0;

    // 场景1: 全饱和平台 (65535) + 远处对照星 C' (基线回归)
    std::vector<float> img1((size_t)W * H, (float)BG);
    for (int y = 80; y <= 140; y++)
        for (int x = 80; x <= 140; x++)
            img1[(size_t)y * W + x] = 65535.0f;
    add_gaussian_star(img1, W, H, 200.5, 60.5, 3000.0, 3.0); // 对照 C'
    std::vector<uint16_t> uimg1((size_t)W * H);
    for (size_t i = 0; i < img1.size(); i++) {
        double v = img1[i];
        uimg1[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
    }

    // 场景2 (核心): Vp=47000 平台 x,y ∈ [80,160] (81x81, 仍走饱和分支),
    //   锚点 (100,101), xr=58 → 修复前 y=101 行跳过 [101,158]。
    //   B 叠加在平台上: 峰值 55000 不截断, 整数峰 (135,101) 在被跳过区间内。
    std::vector<float> img2((size_t)W * H, (float)BG);
    for (int y = 80; y <= 160; y++)
        for (int x = 80; x <= 160; x++)
            img2[(size_t)y * W + x] = 47000.0f;
    add_gaussian_star(img2, W, H, 135.5, 101.5, 8000.0, 2.0); // 污染路径上的 B
    add_gaussian_star(img2, W, H, 60.5, 200.5, 3000.0, 3.0);  // 对照 C
    std::vector<uint16_t> uimg2((size_t)W * H);
    for (size_t i = 0; i < img2.size(); i++) {
        double v = img2[i];
        uimg2[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
    }

    SDetParams params;
    std::memset(&params, 0, sizeof(params));
    params.structureLayers = 5;
    params.hotPixelFilterRadius = 1;
    params.iterativeClipSigma = 9.0f;
    params.iterativeMaxRounds = 5;
    params.medianFilterDetail = 1;
    params.maxStars = 100;
    params.fitRadius = 0;
    params.fwhmClipSigma = 3.0f;
    params.maxAxisRatio = 2.0f;

    char name[200];

    // ---- 场景1: 宽平台回归 (平台候选被下游拟合丢弃为现有算法行为,
    //      此处仅断言 rc=0 且远处对照星检出不受平台影响) ----
    {
        StarDetectorHandle h = sdet_create(&params);
        if (!h) { printf("[FAIL] sdet_create\n"); return 1; }
        double *x = nullptr, *y = nullptr; float *flux = nullptr;
        int *sat = nullptr, *has = nullptr; float *mag = nullptr; int n = 0;
        int rc = sdet_detect_ex(h, uimg1.data(), W, H,
                                &x, &y, &flux, &sat, &mag, &has, &n,
                                nullptr, 0, nullptr);
        snprintf(name, sizeof(name), "场景1: sdet_detect_ex rc=%d n=%d", rc, n);
        CHECK(rc == 0, name);
        double dmin = -1;
        bool found_c = has_star_near(x, y, n, 200.5, 60.5, 2.5, &dmin);
        snprintf(name, sizeof(name), "场景1: 对照星 C'(200.5,60.5) 检出 (最近 %.2f px)", dmin);
        CHECK(found_c, name);
        sdet_free_detect_ex(x, y, flux, sat, mag, has, nullptr, 0);
        sdet_destroy(h);
    }

    // ---- 场景2: 污染路径上的 B 必须正常检出 (修复前此处漂移) ----
    {
        StarDetectorHandle h = sdet_create(&params);
        if (!h) { printf("[FAIL] sdet_create\n"); return 1; }
        double *x = nullptr, *y = nullptr; float *flux = nullptr;
        int *sat = nullptr, *has = nullptr; float *mag = nullptr; int n = 0;
        int rc = sdet_detect_ex(h, uimg2.data(), W, H,
                                &x, &y, &flux, &sat, &mag, &has, &n,
                                nullptr, 0, nullptr);
        snprintf(name, sizeof(name), "场景2: sdet_detect_ex rc=%d n=%d", rc, n);
        CHECK(rc == 0, name);

        // 候选/输出层断言 (修复前: x += xr 跳过 [101,158] → B 漏检 → FAIL;
        // 修复后: 游标不回写, B 正常检出)
        double dB = -1;
        bool found_b = has_star_near(x, y, n, 135.5, 101.5, 2.5, &dB);
        snprintf(name, sizeof(name), "场景2: 污染路径星 B(135.5,101.5) 检出 (最近 %.2f px)", dB);
        CHECK(found_b, name);

        double dC = -1;
        bool found_c = has_star_near(x, y, n, 60.5, 200.5, 2.5, &dC);
        snprintf(name, sizeof(name), "场景2: 对照星 C(60.5,200.5) 检出 (最近 %.2f px)", dC);
        CHECK(found_c, name);

        // 输出层回归: B/C 拟合通过后进入最终星表 (不因候选层漂移丢失)
        int nb = 0, nc = 0;
        for (int i = 0; i < n; i++) {
            if (std::fabs(x[i] - 135.5) <= 2.5 && std::fabs(y[i] - 101.5) <= 2.5) nb++;
            if (std::fabs(x[i] - 60.5) <= 2.5 && std::fabs(y[i] - 200.5) <= 2.5) nc++;
        }
        snprintf(name, sizeof(name), "场景2: 最终星表含 B(%d) 与 C(%d)", nb, nc);
        CHECK(nb == 1 && nc == 1, name);

        sdet_free_detect_ex(x, y, flux, sat, mag, has, nullptr, 0);
        sdet_destroy(h);
    }

    printf("== 饱和游标污染验证: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
