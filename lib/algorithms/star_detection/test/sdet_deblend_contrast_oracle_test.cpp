// ============================================================================
// sdet_deblend_contrast_oracle_test.cpp - O11 delta_c 权重基准 Oracle
// (NON_PRODUCTION_TOOL_ONLY, P1-2 共址单测; 仅以 -DSDET_TESTING 编译)
//
// 判据正本（B&A96 §4 去混合对比度判据 / A&A 117, 393 §4）:
//   枝在**当前层阈值 t** 之上的积分流量  >  delta_c * **父组分在检出阈值之上
//   的总流量**
// 参照实现（逐字，两侧刻意不对称——分子按当前层阈值扣除，基准是父检出的流量,
// 不是枝自身流量）:
//   SExtractor 2.28.2 (commit 5e82a8d1) src/refine.c parcelout()
//     :97  value0 = objlist[0].obj[0].fdflux*prefs.deblend_mincont;
//     :159 obj[j].fdflux - obj[j].dthresh*obj[j].fdnpix > value0
//     :163 if (m>1)
//   SEP v1.4.1 (commit 4c3acca6) src/deblend.c deblend()
//     :116 value0 = objlist[0].obj[0].fdflux * deblend_mincont;
//     :179 obj[j].fdflux - obj[j].thresh * obj[j].fdnpix > value0
//   两项目默认 DEBLEND_MINCONT = 0.005 / DEBLEND_NTHRESH = 32
//   （sextractor src/preflist.h:95-96,207-208; sep/sep.pyx:608-615）。
//
// 本 Oracle 的两条断言腿（都能红能绿）:
//   ① 判据合同: 对观察面记录的每一枝，测试内以 delta_c * total_flux 独立复算,
//      与实现判定逐条比对。基准若退回「枝自身 bflow」→ 比对判红。
//   ② 可分辨性（非退化）: 必须存在至少一枝使
//      (fsum > delta_c*bflow) != (fsum > delta_c*total_flux)
//      —— 否则本 Oracle 对该缺陷无判别力, 直接判红（防「恒真门」）。
//   ③ 行为合同: 暗伴星（自身阈上总流量 < delta_c × 父组分总流量）不得分裂
//      （该组分叶数 = 1）; 明伴星（> delta_c）必须分裂（叶数 >= 2, 判据非恒假）。
//
// 复跑: ctest --test-dir build -R w34_sdet_deblend_contrast_oracle --output-on-failure
// 红证（负例注入）: 把 sdet_api.cpp 判据改回 fsum > SDET_DELTA_C * bflow 重编本
//   目标 → ① ③ 判红; 见 run/FINAL-07/审核包/ 报告「红绿证据」节。
// ============================================================================
#include "star_detector.h"
#include "sdet_test_probe.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); ++g_pass; } \
    else { printf("  [FAIL] %s\n", msg); ++g_fail; } \
} while (0)

// 判据常数正本 = lib/algorithms/star_detection/src/sdet_api.cpp SDET_DELTA_C
// （冻结 5e-3; 与 SExtractor/SEP 默认 DEBLEND_MINCONT 同值）
static const double DELTA_C = 5e-3;

// ---- 合成帧参数（全部解析可复算, 无随机真值依赖）----
static const int    W = 320, H = 320;
static const double BG = 1000.0;          // 背景
static const double NOISE_SIGMA = 5.0;    // 噪声 sigma（阈值 = median + 5*sigma_bg ⇒ thr ≈ 1025）
static const double P_X = 160.5, P_Y = 120.5;   // 主星（父组分）
static const double P_A = 492000.0, P_SIGMA = 3.0;
static const double S_SIGMA = 3.0;
static const double SEP_PX = 28.0;        // 主星-伴星间距（平滑后鞍点 ≈ 5.3e-4 × 主峰）

// xorshift32: 确定性伪噪声（无平台/库依赖, 逐像素独立）
static uint32_t g_rng = 0x12345678u;
static double noise_draw(void) {
    g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5;
    const double u = (double)(g_rng >> 8) / (double)(1u << 24);   // [0,1)
    return (u - 0.5) * (NOISE_SIGMA * 3.4641016151377544);        // σ = NOISE_SIGMA
}

static void add_gaussian(std::vector<double> &img, double cx, double cy,
                         double A, double sigma) {
    const int r = (int)(10.0 * sigma) + 2;
    for (int y = -r; y <= r; ++y) {
        for (int x = -r; x <= r; ++x) {
            const int px = (int)cx + x, py = (int)cy + y;
            if (px < 0 || py < 0 || px >= W || py >= H) continue;
            const double dx = px + 0.5 - cx, dy = py + 0.5 - cy;
            img[(size_t)py * W + px] += A * std::exp(-(dx * dx + dy * dy) / (2.0 * sigma * sigma));
        }
    }
}

// 构造「主星 + 伴星」复合体帧; A_S 由调用方给定（对比度 = A_S / A_P）
static void build_composite(std::vector<double> &img, double A_S) {
    g_rng = 0x12345678u;
    img.assign((size_t)W * H, 0.0);
    for (size_t i = 0; i < img.size(); ++i) img[i] = BG + noise_draw();
    add_gaussian(img, P_X, P_Y, P_A, P_SIGMA);
    add_gaussian(img, P_X, P_Y + SEP_PX, A_S, S_SIGMA);
}

struct RunResult {
    int rc = -1;
    int n_out = 0;
    std::vector<int> leaves;                 // 逐组分叶数（扫描序）
    std::vector<SdetDeblendProbe> probes;    // 逐组分 × 逐层 × 逐枝判据读数
    int n_levels_multi = 0;                  // 记录过的「>=2 枝」层-枝项数
    int n_ambiguous = 0;                     // 两种基准判定相异的枝数（可分辨性）
    int n_contract_violation = 0;            // 判据合同违背枝数
};

static RunResult run_case(const std::vector<double> &img) {
    RunResult r;
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

    std::vector<SdetDeblendProbe> probes;
    std::vector<int> leaves;
    sdet_test_deblend_probe = &probes;
    sdet_test_deblend_leaves = &leaves;

    StarDetectorHandle h = sdet_create(&params);
    if (!h) { sdet_test_deblend_probe = nullptr; sdet_test_deblend_leaves = nullptr; return r; }
    double *x = nullptr, *y = nullptr; float *flux = nullptr;
    int *sat = nullptr, *has = nullptr; float *mag = nullptr; int n = 0;
    r.rc = sdet_detect_ex_f64(h, img.data(), W, H, &x, &y, &flux, &sat, &mag, &has, &n,
                              nullptr, 0, nullptr);
    r.n_out = n;
    sdet_free_detect_ex(x, y, flux, sat, mag, has, nullptr, 0);
    sdet_destroy(h);

    sdet_test_deblend_probe = nullptr;
    sdet_test_deblend_leaves = nullptr;
    r.leaves = leaves;
    r.probes = probes;

    for (size_t i = 0; i < r.probes.size(); ++i) {
        const SdetDeblendProbe &p = r.probes[i];
        if (p.nbrs >= 2) ++r.n_levels_multi;
        const int expect = (p.fsum > DELTA_C * p.total_flux) ? 1 : 0;   // 文献式独立复算
        if (expect != p.sat1) ++r.n_contract_violation;
        const int old_basis = (p.fsum > DELTA_C * p.bflow) ? 1 : 0;     // 旧基准（枝自身 bflow）
        if (old_basis != expect) ++r.n_ambiguous;
    }
    return r;
}

int main() {
    char name[400];

    // ---- 用例 A: 暗伴星（对比度 2e-3 < delta_c = 5e-3）——判据应**不**分裂 ----
    std::vector<double> img_a;
    build_composite(img_a, P_A * 2e-3);
    RunResult ra = run_case(img_a);
    printf("[A 暗伴星 对比度=2e-3] rc=%d n_out=%d 组分=%zu 叶数=%d 多层枝项=%d 可分辨枝=%d\n",
           ra.rc, ra.n_out, ra.leaves.size(),
           ra.leaves.empty() ? -1 : ra.leaves[0], ra.n_levels_multi, ra.n_ambiguous);
    snprintf(name, sizeof(name), "A: sdet_detect_ex_f64 rc=%d", ra.rc);
    CHECK(ra.rc == 0, name);
    snprintf(name, sizeof(name), "A: 复合体为单一组分 (实测 %zu)", ra.leaves.size());
    CHECK(ra.leaves.size() == 1, name);
    snprintf(name, sizeof(name),
             "A: 判据合同（每枝 sat1 == fsum > delta_c*total_flux）违背 %d 条",
             ra.n_contract_violation);
    CHECK(ra.n_contract_violation == 0, name);
    snprintf(name, sizeof(name),
             "A: 可分辨性（旧基准判定 != 文献式判定的枝数 %d >= 1）", ra.n_ambiguous);
    CHECK(ra.n_ambiguous >= 1, name);
    snprintf(name, sizeof(name),
             "A: 判据在多层被评估（>=2 枝的枝项 %d >= 1）", ra.n_levels_multi);
    CHECK(ra.n_levels_multi >= 1, name);
    if (ra.leaves.size() == 1) {
        snprintf(name, sizeof(name),
                 "A: 暗伴星未分裂（叶数 %d == 1; 旧基准下为 2）", ra.leaves[0]);
        CHECK(ra.leaves[0] == 1, name);
    }

    // ---- 用例 B: 明伴星（对比度 5e-2 > delta_c）——判据应分裂（判别力负例）----
    std::vector<double> img_b;
    build_composite(img_b, P_A * 5e-2);
    RunResult rb = run_case(img_b);
    printf("[B 明伴星 对比度=5e-2] rc=%d n_out=%d 组分=%zu 叶数=%d 多层枝项=%d 可分辨枝=%d\n",
           rb.rc, rb.n_out, rb.leaves.size(),
           rb.leaves.empty() ? -1 : rb.leaves[0], rb.n_levels_multi, rb.n_ambiguous);
    snprintf(name, sizeof(name), "B: sdet_detect_ex_f64 rc=%d", rb.rc);
    CHECK(rb.rc == 0, name);
    snprintf(name, sizeof(name), "B: 判据合同违背 %d 条", rb.n_contract_violation);
    CHECK(rb.n_contract_violation == 0, name);
    snprintf(name, sizeof(name), "B: 明伴星分裂（证据: 多层枝项 %d >= 1 且存在显著枝）",
             rb.n_levels_multi);
    CHECK(rb.leaves.size() == 1 && rb.leaves[0] >= 2, name);

    printf("== O11 delta_c 权重基准 Oracle: %d 通过, %d 失败 ==\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
