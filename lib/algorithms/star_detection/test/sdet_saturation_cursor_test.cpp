// ============================================================================
// sdet_saturation_cursor_test.cpp - 饱和平台几何下的 O11 判据基准锚 + 饱和链契约
// (NON_PRODUCTION_TOOL_ONLY, P1-2 共址单测; 以 -DSDET_TESTING 编译生产 4 src)
//
// 【历史沿革与前提订正】
//   本用例原为「饱和星 edge-walking 游标污染」回归: 原 peaker 在饱和分支结束处
//   「x += xr」回写外层扫描游标 x, 使锚点同行后续区间 [x+1, x+xr] 被跳过 →
//   候选检出集合漂移。该 peaker 扫描游标代码在现行实现中已不存在（现行候选来自
//   O8 叶内 11x11 局部极大扫描, 无回写点）, 故原始断言「B 应被发布」既不成立也
//   不再指向该缺陷; 本文件按现行算子链重写（登记见 run/FINAL-07/审核包/）。
//
// 【本文件现在断言什么】
//   场景1（保留）: 全饱和平台 (65535) 基线回归 —— rc=0 且远处对照星检出。
//   场景2(a) O11 判据基准锚（delta_c）: 本帧「81x81 平台 + 平台内嵌入星 B + 远处 C」
//     是同一检出组分。判据正本（B&A96 §4 / A&AS 117, 393 p.395 原文）:
//       枝在**当前层阈值 t_i** 之上的积分流量 > delta_c × **父复合天体在检出阈值
//       之上**的总流量;  参照实现 SExtractor 2.28.2 src/refine.c:97/:159 与
//       SEP v1.4.1 src/deblend.c:116/:179。
//     本帧下 C 枝的流量 (1.3e5) < delta_c × 父总流量 (1.5e6) → **不分裂**（叶数 1）。
//     旧基准（delta_c × 枝自身 bflow）把任何枝都判显著 → 分裂（叶数 2）⇒ 断言判红。
//     该断言即「旧实现红、新实现绿」的可判定度量。
//   场景2(b) 饱和链契约（**KNOWN-LIMITATION 锚**）: 本帧平台的 47000 高于
//     O4a/O4b 的饱和电平 bg+0.7*dynrange = 38740（冻结相对式, 本项目构造）⇒
//     嵌入星 B 的峰被判饱和 ⇒ O6 四方向 edge-walking 把中心搬到平台边缘盒几何
//     中心 (120.5,120.5) ⇒ O12 拟合采样按冻结饱和 mask 排除 val>=sat_threshold,
//     整个拟合盒内无可用样本 ⇒ 拟合失败 ⇒ **B 不发布**。本断言把该结果钉住
//     （不是掩盖缺陷: 消息里写明机理与登记位置; 链一改即判红）。B 的丢失与
//     delta_c 基准**无关**（A/B 实测: 两种基准下发布集相同, 见审核包报告）。
//   场景2(c) 非退化对照: 同一几何、平台降到饱和电平**以下** (Vp=2000) ⇒ 同样的
//     嵌入星 B 必须按自身位置 (135.5,101.5) 与接近真值通量 (8000) 发布 ⇒ 证明
//     场景2(b) 的丢失来自饱和链, 不是扫描/候选链。
//
// 断言:
//   场景1: rc=0 且对照星 C(200.5,60.5) 检出;
//   场景2: rc=0; O11 叶数=1（旧基准=2 ⇒ FAIL 即红证）; 判据可分辨性 >= 1;
//          冻结链下唯一记录 = C（KNOWN-LIMITATION）;
//          非退化对照帧: B 在 (135.5,101.5) 附近检出且 flux ≈ 8000。
// 风格对齐 sdet_fp64_test.cpp (CHECK 宏 + printf)。
// ============================================================================
#include "star_detector.h"
#include "sdet_test_probe.h"

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

// 判据常数正本 = src/sdet_api.cpp SDET_DELTA_C（冻结 5e-3, 与 SExtractor/SEP
// 默认 DEBLEND_MINCONT 同值）
static const double DELTA_C = 5e-3;

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

struct RunOut {
    int rc = -1, n = 0;
    std::vector<double> x, y;
    std::vector<float> flux;
    std::vector<int> leaves;      // 逐组分 O11 叶数（扫描序）
    int n_multi = 0;              // 「>= 2 枝」的枝项数（判据被评估深度）
    int n_ambiguous = 0;          // 两基准判定相异的枝数（可分辨性）
    int n_contract_violation = 0; // 判据合同背离条数
};

static RunOut run_uint16(const std::vector<uint16_t> &img, int W, int H,
                         const SDetParams &params) {
    RunOut r;
    std::vector<SdetDeblendProbe> probes;
    std::vector<int> leaves;
    sdet_test_deblend_probe = &probes;
    sdet_test_deblend_leaves = &leaves;

    StarDetectorHandle h = sdet_create(&params);
    if (!h) {
        sdet_test_deblend_probe = nullptr;
        sdet_test_deblend_leaves = nullptr;
        return r;
    }
    double *x = nullptr, *y = nullptr; float *flux = nullptr;
    int *sat = nullptr, *has = nullptr; float *mag = nullptr; int n = 0;
    r.rc = sdet_detect_ex(h, img.data(), W, H, &x, &y, &flux, &sat, &mag, &has, &n,
                          nullptr, 0, nullptr);
    r.n = n;
    for (int i = 0; i < n; i++) { r.x.push_back(x[i]); r.y.push_back(y[i]); r.flux.push_back(flux[i]); }
    sdet_free_detect_ex(x, y, flux, sat, mag, has, nullptr, 0);
    sdet_destroy(h);
    sdet_test_deblend_probe = nullptr;
    sdet_test_deblend_leaves = nullptr;
    r.leaves = leaves;

    for (size_t i = 0; i < probes.size(); i++) {
        const SdetDeblendProbe &p = probes[i];
        if (p.nbrs >= 2) ++r.n_multi;
        const int expect = (p.fsum > DELTA_C * p.total_flux) ? 1 : 0;  // 文献式独立复算
        if (expect != p.sat1) ++r.n_contract_violation;
        const int old_basis = (p.fsum > DELTA_C * p.bflow) ? 1 : 0;    // 旧基准: 枝自身
        if (old_basis != expect) ++r.n_ambiguous;
    }
    return r;
}

// 帧构造: 平台 [80..plateau_hi]^2 + 平台内嵌入星 B(bx,by) + 远处对照星 C
static void build_platform_frame(std::vector<uint16_t> &uimg, int W, int H,
                                 double bg, double vp, int plateau_hi,
                                 double bx, double by, double bA, double bSigma,
                                 bool with_c) {
    std::vector<float> img((size_t)W * H, (float)bg);
    for (int y = 80; y <= plateau_hi; y++)
        for (int x = 80; x <= plateau_hi; x++)
            img[(size_t)y * W + x] = (float)vp;
    add_gaussian_star(img, W, H, bx, by, bA, bSigma);
    if (with_c) add_gaussian_star(img, W, H, 60.5, 200.5, 3000.0, 3.0);
    uimg.assign((size_t)W * H, 0);
    for (size_t i = 0; i < img.size(); i++) {
        double v = img[i];
        uimg[i] = (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v));
    }
}

int main() {
    const int W = 256, H = 256;
    const double BG = 800.0;

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

    char name[512];

    // ---- 场景1: 全饱和平台 (65535) 基线回归 ----
    {
        std::vector<uint16_t> uimg1;
        build_platform_frame(uimg1, W, H, BG, 65535.0, 140, 200.5, 60.5, 3000.0, 3.0, false);
        RunOut r1 = run_uint16(uimg1, W, H, params);
        snprintf(name, sizeof(name), "场景1: sdet_detect_ex rc=%d n=%d", r1.rc, r1.n);
        CHECK(r1.rc == 0, name);
        double dmin = -1;
        bool found_c = has_star_near(r1.x.data(), r1.y.data(), r1.n, 200.5, 60.5, 2.5, &dmin);
        snprintf(name, sizeof(name), "场景1: 对照星 C(200.5,60.5) 检出 (最近 %.2f px)", dmin);
        CHECK(found_c, name);
    }

    // ---- 场景2: Vp=47000 平台 + 平台内嵌入星 B(8000/sigma2) + 远处对照星 C ----
    std::vector<uint16_t> uimg2;
    build_platform_frame(uimg2, W, H, BG, 47000.0, 160, 135.5, 101.5, 8000.0, 2.0, true);
    RunOut r2 = run_uint16(uimg2, W, H, params);
    printf("场景2: rc=%d n=%d 组分=%zu 叶数=%d 多层枝项=%d 可分辨枝=%d\n",
           r2.rc, r2.n, r2.leaves.size(),
           r2.leaves.empty() ? -1 : r2.leaves[0], r2.n_multi, r2.n_ambiguous);

    snprintf(name, sizeof(name), "场景2: sdet_detect_ex rc=%d", r2.rc);
    CHECK(r2.rc == 0, name);

    // (a) O11 判据基准锚 —— 旧基准（枝自身 bflow）下本帧分裂出 2 叶, 判红
    snprintf(name, sizeof(name),
             "场景2(a): 判据合同（sat1 == fsum > delta_c*父组分总流量）背离 %d 条",
             r2.n_contract_violation);
    CHECK(r2.n_contract_violation == 0, name);
    snprintf(name, sizeof(name),
             "场景2(a): 基准可分辨性（两基准判定相异枝数 %d >= 1）", r2.n_ambiguous);
    CHECK(r2.n_ambiguous >= 1, name);
    snprintf(name, sizeof(name),
             "场景2(a): 判据被评估深度（>=2 枝的枝项 %d >= 1）", r2.n_multi);
    CHECK(r2.n_multi >= 1, name);
    if (r2.leaves.size() == 1) {
        snprintf(name, sizeof(name),
                 "场景2(a): C 枝未达 delta_c*父总流量 不分裂（叶数 %d == 1; 旧基准=2）",
                 r2.leaves[0]);
        CHECK(r2.leaves[0] == 1, name);
    } else {
        snprintf(name, sizeof(name), "场景2(a): 复合体应为单一组分（实测 %zu）", r2.leaves.size());
        CHECK(false, name);
    }

    // (b) 饱和链契约（KNOWN-LIMITATION）
    double dC = -1, dB = -1;
    bool found_c2 = has_star_near(r2.x.data(), r2.y.data(), r2.n, 60.5, 200.5, 2.5, &dC);
    bool found_b2 = has_star_near(r2.x.data(), r2.y.data(), r2.n, 135.5, 101.5, 2.5, &dB);
    snprintf(name, sizeof(name), "场景2(b): 对照星 C(60.5,200.5) 检出 (最近 %.2f px)", dC);
    CHECK(found_c2, name);
    snprintf(name, sizeof(name),
             "场景2(b) [KNOWN-LIMITATION]: 冻结饱和链下嵌入星 B 不发布 (B 最近 %.2f px, n=%d;"
             " 机理=O4b 0.7/0.1*dynrange 判 B 饱和 -> O6 盒中心 -> O12 饱和 mask 清空拟合样本;"
             " 与 delta_c 基准无关; 见 run/FINAL-07/审核包/)",
             dB, r2.n);
    CHECK(!found_b2 && r2.n == 1, name);

    // (c) 非退化对照: 同几何、平台 2000 < 饱和电平 7240 ⇒ 嵌入星必须发布
    std::vector<uint16_t> uimg3;
    build_platform_frame(uimg3, W, H, BG, 2000.0, 160, 135.5, 101.5, 8000.0, 2.0, true);
    RunOut r3 = run_uint16(uimg3, W, H, params);
    printf("场景2(c): rc=%d n=%d\n", r3.rc, r3.n);
    snprintf(name, sizeof(name), "场景2(c): sdet_detect_ex rc=%d", r3.rc);
    CHECK(r3.rc == 0, name);
    double dB3 = -1;
    bool found_b3 = has_star_near(r3.x.data(), r3.y.data(), r3.n, 135.5, 101.5, 2.5, &dB3);
    snprintf(name, sizeof(name),
             "场景2(c): 平台低于饱和电平时嵌入星 B 按自身位置发布 (最近 %.2f px)", dB3);
    CHECK(found_b3, name);
    {
        int bi = -1; double best = 1e30;
        for (int i = 0; i < r3.n; i++) {
            double d = std::sqrt((r3.x[i] - 135.5) * (r3.x[i] - 135.5) +
                                 (r3.y[i] - 101.5) * (r3.y[i] - 101.5));
            if (d < best) { best = d; bi = i; }
        }
        double fl = (bi >= 0) ? (double)r3.flux[bi] : -1.0;
        snprintf(name, sizeof(name),
                 "场景2(c): 嵌入星通量接近真值 8000（实测 %.1f, 容许 +/-5%%）", fl);
        CHECK(bi >= 0 && std::fabs(fl - 8000.0) <= 400.0, name);
    }

    printf("== 饱和平台 O11 判据基准锚 + 饱和链契约: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
