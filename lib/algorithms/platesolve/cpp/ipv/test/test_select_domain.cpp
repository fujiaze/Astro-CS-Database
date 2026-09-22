// ============================================================================
// test_select_domain.cpp — 图像侧选星有效域 + 空扫描 provenance 回归锁
// (ALG-WCS-001 §4a; 判据面 = 生产函数本体 select_image_stars /
//  compute_fov_density / estimate_mag_lim_iterative, 经生产静态库 astrocs_p1_ipv)
// ----------------------------------------------------------------------------
// 覆盖:
//   A. §4a.1 样本定义域: 饱和检测不得进入选星样本
//      (真实场型: 最亮的 N 颗全是饱和星 —— 旧规则下样本 100% 被饱和星占据)
//   B. §4a.1 候选不足: 取全部非饱和候选, 不用饱和星补足
//   C. §4a.2 密度同域: rho_img 的分子 = 实际样本基数 (样本基数变化按比例反映)
//   D. §4a.4 空扫描 provenance: empty_sweep / n_zero_queries / 扫描区间
//   E. 负向对照 (必须判红): 旧规则 (饱和与正常统一排序取前 N) 必须使 A/B/C 的
//      判据判红 —— 证明这些判据不是恒真门
// ============================================================================
#include "ipv_select.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace ipv;

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                    \
    do {                                                                    \
        ++g_check;                                                          \
        if (!(cond)) {                                                      \
            std::printf("FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__); \
            ++g_fail;                                                       \
        } else {                                                            \
            std::printf("ok  : %s\n", (msg));                              \
        }                                                                   \
    } while (0)

// ---------------------------------------------------------------------------
// 负向对照: 修复前的选星规则 (饱和与正常统一排序, 取前 N 颗)
// 只用于证明判据有鉴别力, 不参与生产。
// ---------------------------------------------------------------------------
static std::vector<int> legacy_select(const std::vector<double>& mag,
                                      int img_n_target) {
    std::vector<int> idx(mag.size());
    for (std::size_t i = 0; i < mag.size(); ++i) idx[i] = static_cast<int>(i);
    std::sort(idx.begin(), idx.end(), [&](int a, int b) {
        const bool an = std::isnan(mag[a]);
        const bool bn = std::isnan(mag[b]);
        if (an && bn) return false;
        if (an) return false;
        if (bn) return true;
        return mag[a] < mag[b];
    });
    const int k = std::min<int>(img_n_target, static_cast<int>(idx.size()));
    idx.resize(static_cast<std::size_t>(k));
    return idx;
}

static bool all_unsaturated(const std::vector<int>& sel,
                            const std::vector<bool>& sat) {
    for (int i : sel)
        if (i >= 0 && i < static_cast<int>(sat.size()) && sat[i]) return false;
    return true;
}

static int count_saturated(const std::vector<int>& sel,
                           const std::vector<bool>& sat) {
    int n = 0;
    for (int i : sel)
        if (i >= 0 && i < static_cast<int>(sat.size()) && sat[i]) ++n;
    return n;
}

// 合成检测表: n_sat 颗饱和 (最亮) + n_norm 颗正常
struct Fixture {
    std::vector<double> flux;
    std::vector<double> mag;
    std::vector<bool> sat;
};

static Fixture make_fixture(int n_sat, int n_norm) {
    Fixture f;
    for (int i = 0; i < n_sat; ++i) {          // 饱和星: box 积分星等最亮
        f.flux.push_back(1.0e6 + i);
        f.mag.push_back(-5.0 + 0.001 * i);
        f.sat.push_back(true);
    }
    for (int i = 0; i < n_norm; ++i) {         // 正常星: 更暗
        f.flux.push_back(1.0e4 - i);
        f.mag.push_back(0.0 + 0.01 * i);
        f.sat.push_back(false);
    }
    return f;
}

// ===========================================================================
// A/B. 样本定义域与候选不足
// ===========================================================================
static void test_sample_domain() {
    // A: 60 饱和 (最亮) + 240 正常, 目标 60
    {
        Fixture f = make_fixture(60, 240);
        std::vector<int> sel = select_image_stars(f.flux, f.mag, f.sat, 60, nullptr);
        CHECK(sel.size() == 60, "A: 非饱和候选充足时样本数 = img_n_target");
        CHECK(all_unsaturated(sel, f.sat), "A: 样本中不含任何饱和检测 (§4a.1)");
        CHECK(count_saturated(sel, f.sat) == 0, "A: 饱和计数为 0");
        // 逐位对照: 必须是"非饱和里最亮的 60 颗"
        std::vector<double> mag_ns;
        for (std::size_t i = 0; i < f.mag.size(); ++i)
            if (!f.sat[i]) mag_ns.push_back(f.mag[i]);
        std::sort(mag_ns.begin(), mag_ns.end());
        double worst = -1e300;
        for (int i : sel) worst = std::max(worst, f.mag[i]);
        CHECK(std::fabs(worst - mag_ns[59]) < 1e-12,
              "A: 样本恰为非饱和候选里最亮的 60 颗 (边界星等一致)");

        // E: 负向对照 —— 旧规则必须判红
        std::vector<int> legacy = legacy_select(f.mag, 60);
        CHECK(!all_unsaturated(legacy, f.sat),
              "E: 负向对照 —— 旧规则 (统一排序) 的样本含饱和星 => 判据可判红");
        CHECK(count_saturated(legacy, f.sat) == 60,
              "E: 负向对照 —— 旧规则样本 60/60 全为饱和星");
    }
    // B: 60 饱和 (最亮) + 40 正常, 目标 60 -> 不回填
    {
        Fixture f = make_fixture(60, 40);
        std::vector<int> sel = select_image_stars(f.flux, f.mag, f.sat, 60, nullptr);
        CHECK(sel.size() == 40, "B: 候选不足时样本 = 全部非饱和候选 (40)");
        CHECK(all_unsaturated(sel, f.sat), "B: 候选不足时仍不含饱和检测");
        std::vector<int> legacy = legacy_select(f.mag, 60);
        CHECK(legacy.size() == 60 && count_saturated(legacy, f.sat) == 60,
              "E: 负向对照 —— 旧规则在候选不足时用饱和星补足到 60");
    }
    // 全饱和 -> 空样本 (调用方 fail-closed)
    {
        Fixture f = make_fixture(100, 0);
        std::vector<int> sel = select_image_stars(f.flux, f.mag, f.sat, 60, nullptr);
        CHECK(sel.empty(), "A: 全饱和场 -> 空样本 (由调用方 fail-closed)");
    }
    // NaN 星等: 非饱和但 mag 失效者排在最后, 候选充足时不被选中
    {
        Fixture f = make_fixture(10, 3);
        for (int i = 0; i < 5; ++i) {          // 追加 5 颗 mag=NaN 的非饱和检测
            f.flux.push_back(1.0);
            f.mag.push_back(std::nan(""));
            f.sat.push_back(false);
        }
        std::vector<int> sel = select_image_stars(f.flux, f.mag, f.sat, 3, nullptr);
        CHECK(sel.size() == 3, "A: 候选充足时取满 img_n_target");
        bool any_nan = false;
        for (int i : sel) any_nan = any_nan || std::isnan(f.mag[i]);
        CHECK(!any_nan, "A: mag 为 NaN 的失效检测不被选中");
    }
    // 确定性
    {
        Fixture f = make_fixture(60, 240);
        std::vector<int> a = select_image_stars(f.flux, f.mag, f.sat, 60, nullptr);
        std::vector<int> b = select_image_stars(f.flux, f.mag, f.sat, 60, nullptr);
        CHECK(a == b, "A: 同输入两次选星结果逐位一致");
    }
}

// ===========================================================================
// C. 密度估计的分子 = 实际样本基数 (§4a.2)
// ===========================================================================
static void test_density_domain() {
    double s0 = 0, fov = 0, qr = 0, qa = 0, ia = 0, rho = 0, rt = 0;
    int nt = 0;
    compute_fov_density(1917.6, 9.0, 4096.0, 4096.0, 40,
                        1.5, 0.55, s0, fov, qr, qa, ia, rho, rt, nt, nullptr);
    const double rho40 = rho;
    const int nt40 = nt;
    compute_fov_density(1917.6, 9.0, 4096.0, 4096.0, 60,
                        1.5, 0.55, s0, fov, qr, qa, ia, rho, rt, nt, nullptr);
    const double rho60 = rho;
    const int nt60 = nt;

    char msg[192];
    std::snprintf(msg, sizeof(msg),
                  "C: rho_img 随样本基数同比例 (%.4f vs %.4f)", rho40, rho60);
    CHECK(std::fabs(rho40 / rho60 - 40.0 / 60.0) < 1e-12, msg);
    CHECK(rho40 < rho60, "C: 样本基数小 => rho_img 小 (不回填饱和星抬密度)");
    CHECK(nt40 >= 50 && nt40 <= 60 && nt60 >= 50 && nt60 <= 60,
          "C: n_target 恒落在 [50, 60] (§4a.2)");

    // E: 负向对照 —— 旧规则下样本基数恒为 60 (被饱和星占满), 密度被抬高
    Fixture f = make_fixture(60, 40);
    std::vector<int> legacy = legacy_select(f.mag, 60);
    CHECK(static_cast<int>(legacy.size()) == 60,
          "E: 负向对照 —— 旧规则样本基数 60 != 非饱和候选数 40 (密度被污染)");
    CHECK(static_cast<int>(legacy.size()) != 40,
          "E: 负向对照 —— 密度判据对旧规则判红");
}

// ===========================================================================
// D. 空扫描 provenance (§4a.4)
// ===========================================================================
struct StubQuery {
    int zero_calls = 0;      // 前 N 次查询返回 0
    int fail_at = -1;        // 1-based 失败点
    double n_after = 180.0;  // 之后返回的星数
    std::vector<double> m_seen;
    int operator()(double m, int& n_out) {
        m_seen.push_back(m);
        const int call = static_cast<int>(m_seen.size());
        if (fail_at >= 1 && call == fail_at) { n_out = 0; return -1; }
        if (call <= zero_calls) { n_out = 0; return 0; }
        n_out = static_cast<int>(n_after);
        return 0;
    }
};

static MagIterOutcome run_stub(StubQuery& s, const IPVSolverParams& p) {
    MagQueryFn fn = [&s](double m, int& n_out) { return s(m, n_out); };
    return estimate_mag_lim_iterative(fn, 60, 1917.6, 180.0, p, nullptr);
}

static void test_empty_sweep() {
    IPVSolverParams p;
    // D1: 全空扫描 (M42 T2 真实失效型: 每次查询都返回 0 颗)
    {
        StubQuery s; s.zero_calls = 1000;
        MagIterOutcome o = run_stub(s, p);
        char msg[192];
        std::snprintf(msg, sizeof(msg),
                      "D1: empty_sweep=%d n_zero=%d/%d sweep=[%.3f, %.3f]",
                      (int)o.empty_sweep, o.n_zero_queries, o.query_count,
                      o.m_sweep_first, o.m_sweep_last);
        CHECK(o.empty_sweep, msg);
        CHECK(o.n_zero_queries == o.query_count,
              "D1: 空结果次数 = 查询次数");
        CHECK(o.query_count == p.m_lim_max_iter,
              "D1: 空扫描走满查询上界 (不静默提前放弃)");
        CHECK(o.valid, "D1: 查询本身成功 (valid 描述查询通道, 非星数)");
        CHECK(!o.converged, "D1: 空扫描不是收敛");
        CHECK(std::fabs(o.m_sweep_first - 11.4348) < 1e-3,
              "D1: 扫描起点 = 曝光公式初值 m0 (§4a.3)");
        CHECK(o.m_sweep_last > o.m_sweep_first,
              "D1: 空结果向**更暗**步进 (禁止向更亮回退)");
        CHECK(std::fabs((o.m_sweep_last - o.m_sweep_first) -
                        (p.m_lim_max_iter - 1) * p.m_lim_zero_step) < 1e-9,
              "D1: 步长恒为 m_lim_zero_step (3.0)");
    }
    // D2: 仅首次为空 -> 不是空扫描
    {
        StubQuery s; s.zero_calls = 1;
        MagIterOutcome o = run_stub(s, p);
        CHECK(!o.empty_sweep, "D2: 仅首次为空 => empty_sweep=false");
        CHECK(o.n_zero_queries == 1, "D2: 空结果计数 = 1");
    }
    // D3: 查询通道失败 -> 与空扫描区分
    {
        StubQuery s; s.fail_at = 1;
        MagIterOutcome o = run_stub(s, p);
        CHECK(!o.empty_sweep, "D3: 查询失败 != 空扫描");
        CHECK(o.query_failed, "D3: 查询失败被标记");
        CHECK(o.n_zero_queries == 0, "D3: 失败查询不计入空结果次数");
    }
    // D4: 有星且不空 -> provenance 归零
    {
        StubQuery s; s.n_after = 180.0;
        MagIterOutcome o = run_stub(s, p);
        CHECK(!o.empty_sweep && o.n_zero_queries == 0,
              "D4: 正常路径 empty_sweep=false 且 n_zero_queries=0");
    }
}

int main() {
    std::printf("=== ALG-WCS-001 §4a 选星有效域 / 空扫描 provenance 回归锁 ===\n");
    test_sample_domain();
    test_density_domain();
    test_empty_sweep();
    std::printf("---- checks=%d failures=%d ----\n", g_check, g_fail);
    return g_fail == 0 ? 0 : 1;
}
