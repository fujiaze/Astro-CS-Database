// ============================================================================
// test_mag_iter.cpp - P4-magiter 极限星等割线迭代单元/回归锁
//
// 覆盖 (全部使用固定 stub 查询序列, 不依赖网络/Gaia 数据/真实图像):
//   1. 收敛性锁   : 幂律 N(m) 在 alpha=0.243/0.2885/0.456 下 <=4 次查询收敛到 <=10%
//   2. 确定性锁   : 同一 stub 两次调用逐位一致 + 金标准 (m_lim_final/query_count/alpha)
//   3. 触顶防御锁 : 返回数触 Gaia 每文件上限 (200000 整数倍) -> 停用 alpha 更新、视为达标、终止
//   4. N=0 步长锁 : 初值过亮 -> +m_lim_zero_step (不产生 log10(0))
//   5. 上界锁     : 永不达标的 stub -> 查询次数恰为 m_lim_max_iter, 不无限循环
//   6. 参数可覆盖锁: safety / max_iter / alpha_prior / clamp 全部来自 IPVSolverParams
//   7. 异常路径   : n_target<=0 / 查询失败 / 空回调
//
// 编译面: 直接链接生产静态库 astrocs_p1_ipv (被测函数在 ipv_select.cpp, 零改动)。
// ============================================================================
#include "ipv_select.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>

using namespace ipv;

static int g_fail = 0;
static int g_check = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        ++g_check;                                                              \
        if (!(cond)) {                                                          \
            std::printf("FAIL: %s  (%s:%d)\n", (msg), __FILE__, __LINE__);     \
            ++g_fail;                                                           \
        } else {                                                                \
            std::printf("ok  : %s\n", (msg));                                   \
        }                                                                       \
    } while (0)

static uint64_t bits(double v) {
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    return u;
}

// ---------------------------------------------------------------------------
// 固定 stub 查询序列: N(m) = round(N_star * 10^(alpha*(m - m_star))) + n0
// 记录每次被查询的 m, 便于断言步进行为。
// ---------------------------------------------------------------------------
struct Stub {
    double alpha = 0.2885;
    double m_star = 12.0;
    double N_star = 180.0;
    double n0 = 0.0;
    double cap_value = -1.0;   // >=0: 当 N 超过该值时改为返回该值 (模拟触顶)
    int    fail_at_call = -1;  // 1-based; >=1 时该次查询返回失败
    int    zero_calls = 0;     // 前 N 次查询强制返回 0
    mutable std::vector<double> m_seen;

    int operator()(double m, int& n_out) const {
        m_seen.push_back(m);
        const int call = static_cast<int>(m_seen.size());
        if (fail_at_call >= 1 && call == fail_at_call) { n_out = 0; return -1; }
        if (call <= zero_calls) { n_out = 0; return 0; }
        double n = N_star * std::pow(10.0, alpha * (m - m_star)) + n0;
        if (cap_value >= 0.0 && n > cap_value) n = cap_value;
        n_out = static_cast<int>(std::llround(n));
        return 0;
    }
};

static MagIterOutcome run_iter(Stub& s, int n_target, const IPVSolverParams& p,
                               double focal_mm = 1877.0, double t_s = 300.0) {
    MagQueryFn fn = [&s](double m, int& n_out) { return s(m, n_out); };
    return estimate_mag_lim_iterative(fn, n_target, focal_mm, t_s, p, nullptr);
}

// N(m) 真值 (与 Stub 同一模型, 用于收敛判定)
static double n_true(const Stub& s, double m) {
    return s.N_star * std::pow(10.0, s.alpha * (m - s.m_star)) + s.n0;
}

// ===========================================================================
// 1. 收敛性锁
// ===========================================================================
static void test_convergence() {
    const double alphas[] = {0.243, 0.2885, 0.456};
    for (double a : alphas) {
        Stub s; s.alpha = a; s.m_star = 14.0;   // 远离 m0, 强制多步
        IPVSolverParams p;                       // 生产默认 (safety=3, max_iter=4, tol=0.1)
        const double N_target = 60.0 * p.m_lim_safety;
        s.N_star = N_target;
        MagIterOutcome o = run_iter(s, 60, p);
        char msg[160];
        std::snprintf(msg, sizeof(msg),
            "收敛 alpha=%.4f: converged=%d query_count=%d m=%.4f", a,
            (int)o.converged, o.query_count, o.m_lim_final);
        CHECK(o.converged, msg);
        CHECK(o.query_count <= p.m_lim_max_iter, "查询次数不超过 m_lim_max_iter");
        const double rel = std::fabs(n_true(s, o.m_lim_final) - N_target) / N_target;
        std::snprintf(msg, sizeof(msg),
            "收敛残差 alpha=%.4f: rel=%.4f (求解值 %.4f, 理论 %.4f)",
            a, rel, o.m_lim_final, s.m_star);
        CHECK(rel <= p.density_tolerance + 1e-9, msg);
    }
}

// ===========================================================================
// 2. 确定性锁 (+ 金标准)
// ===========================================================================
static void test_determinism() {
    IPVSolverParams p;
    Stub s1; s1.N_star = 180.0; s1.m_star = 13.0;
    Stub s2; s2.N_star = 180.0; s2.m_star = 13.0;
    MagIterOutcome a = run_iter(s1, 60, p);
    MagIterOutcome b = run_iter(s2, 60, p);
    CHECK(bits(a.m_lim_final) == bits(b.m_lim_final), "确定性: m_lim_final 逐位一致");
    CHECK(a.query_count == b.query_count, "确定性: query_count 一致");
    CHECK(bits(a.alpha_final) == bits(b.alpha_final), "确定性: alpha_final 逐位一致");
    CHECK(a.n_returned == b.n_returned, "确定性: n_returned 一致");
    CHECK(s1.m_seen == s2.m_seen, "确定性: 查询序列逐位一致");

    // 金标准: 固定 stub + 默认参数下的确切输出 (算法任何改动都会打破)
    std::printf("GOLDEN default: m_lim_final=%.10f query_count=%d n_returned=%d "
                "alpha_final=%.10f converged=%d capped=%d valid=%d\n",
                a.m_lim_final, a.query_count, a.n_returned, a.alpha_final,
                (int)a.converged, (int)a.capped, (int)a.valid);
    CHECK(a.query_count == 2, "金标准: 默认参数下 2 次查询收敛");
    CHECK(a.converged && a.valid && !a.capped, "金标准: 收敛/有效/未触顶");
    // 金标准数值 (默认参数 + 固定 stub 的确切输出; 算法改动会打破此锁)
    CHECK(std::fabs(a.m_lim_final - 12.9850849855) < 1e-9,
          "金标准: m_lim_final == 12.9850849855");
    CHECK(a.n_returned == 178, "金标准: n_returned == 178");
    CHECK(bits(a.alpha_final) == bits(p.m_lim_alpha_prior),
          "金标准: 2 次查询恰好不触发 alpha 更新 (仅 1 个差分对可用前已收敛)");
}

// ---------------------------------------------------------------------------
// 2b. alpha 实测更新路径 (必须真正发生更新, 而不是永远只用先验)
// ---------------------------------------------------------------------------
static void test_alpha_update() {
    IPVSolverParams p;
    Stub s; s.N_star = 180.0; s.m_star = 20.0;   // 初值远离真值 -> 多步
    MagIterOutcome o = run_iter(s, 60, p);
    std::printf("GOLDEN alpha-update: m_lim_final=%.10f query_count=%d alpha_final=%.10f\n",
                o.m_lim_final, o.query_count, o.alpha_final);
    CHECK(o.query_count == 4, "alpha 更新: 4 次查询 (上界内)");
    CHECK(o.converged, "alpha 更新: 末次进入 10% 容差");
    CHECK(bits(o.alpha_final) != bits(p.m_lim_alpha_prior),
          "alpha 更新: alpha 确实由实测差分更新 (非先验)");
    CHECK(o.alpha_final > 0.20 && o.alpha_final < 0.35,
          "alpha 更新: 更新后回到真实斜率附近 (0.2..0.35)");
}

// ===========================================================================
// 3. 触顶防御锁
// ===========================================================================
static void test_cap_defense() {
    IPVSolverParams p;
    // 3a: 首次查询即触顶 -> 立即达标终止, 停用 alpha 更新
    {
        Stub s; s.N_star = 1e9; s.cap_value = 200000.0;
        MagIterOutcome o = run_iter(s, 60, p);
        CHECK(o.capped, "触顶: capped=true");
        // P14-N-10 缺陷 1: 触顶不是收敛 (旧实现错记 converged=true)。
        CHECK(!o.converged, "触顶: capped -> converged=false (旧实现错记 true)");
        CHECK(o.query_count == 1, "触顶: 不再继续查询 (query_count==1)");
        CHECK(o.n_returned == 200000, "触顶: N_returned==200000");
        CHECK(bits(o.alpha_final) == bits(p.m_lim_alpha_prior),
              "触顶: alpha 未被更新 (防 alpha->0 使割线发散)");
    }
    // 3b: 先欠后触顶 (200000 整数倍 => 仍判触顶)
    {
        Stub s; s.N_star = 1e9; s.cap_value = 600000.0;
        MagIterOutcome o = run_iter(s, 60, p);
        CHECK(o.capped && o.n_returned == 600000, "触顶: 600000 (3x 每文件上限) 同样判触顶");
        CHECK(!o.converged && o.query_count == 1, "触顶: 600000 终止且非收敛");
    }
    // 3c: 低于每文件上限不误判
    {
        Stub s; s.N_star = 180.0; s.m_star = 12.0;
        MagIterOutcome o = run_iter(s, 60, p);
        CHECK(!o.capped, "触顶: 低于每文件上限返回不被误判为触顶");
    }
    // 3d (P14-N-10 缺陷 2): 部分文件截断 => 总数 = k*cap + partial (非整数倍)。
    // 旧 fmod 启发式漏判 (fmod(250000,200000)=50000!=0); 新可靠判据
    // (n_ret >= 每文件上限) 必须判触顶。
    {
        Stub s; s.N_star = 1e9; s.cap_value = 250000.0;   // 200000 + 50000
        MagIterOutcome o = run_iter(s, 60, p);
        CHECK(o.capped, "触顶: 部分文件截断 250000 (非整数倍) 必须判触顶");
        CHECK(!o.converged, "触顶: 部分截断同样不是收敛");
        CHECK(o.n_returned == 250000, "触顶: N_returned==250000");
        CHECK(o.query_count == 1, "触顶: 部分截断立即终止 (不入 alpha 差分)");
    }
}

// ===========================================================================
// 4. N=0 步长锁
// ===========================================================================
static void test_zero_step() {
    IPVSolverParams p;
    Stub s; s.N_star = 180.0; s.m_star = 11.0; s.zero_calls = 1;
    MagIterOutcome o = run_iter(s, 60, p);
    CHECK(o.query_count >= 2, "N=0: 未立即终止, 继续查询");
    CHECK(s.m_seen.size() >= 2, "N=0: 至少两次查询");
    const double step = s.m_seen[1] - s.m_seen[0];
    CHECK(std::fabs(step - p.m_lim_zero_step) < 1e-12, "N=0: 步长恰为 m_lim_zero_step=3.0");
    CHECK(o.valid, "N=0: 后续查询产生有效结果");
}

// ===========================================================================
// 5. 查询次数上界锁 (永不达标 -> 恰好 max_iter 次, 不无限循环)
// ===========================================================================
static void test_query_bound() {
    IPVSolverParams p;
    Stub s; s.N_star = 1.0; s.m_star = 30.0;   // 任何 m 都 >> 目标? 反向: N 极小
    s.N_star = 1e-9; s.n0 = 1.0;               // 恒返回 1 颗 -> 永不达标
    MagIterOutcome o = run_iter(s, 60, p);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "上界: query_count=%d == m_lim_max_iter=%d",
                  o.query_count, p.m_lim_max_iter);
    CHECK(o.query_count == p.m_lim_max_iter, msg);
    CHECK(!o.converged, "上界: 未收敛标记为 false (防御路径)");
    CHECK(o.valid, "上界: 仍返回有效结果 (取末次查询)");
}

// ===========================================================================
// 6. 参数可覆盖锁 (宪章 10.4: 不硬编码, 全部走 IPVSolverParams)
// ===========================================================================
static void test_param_override() {
    Stub s; s.N_star = 300.0; s.m_star = 16.0;
    IPVSolverParams p;
    p.m_lim_safety = 5.0;              // N_target = 300
    p.m_lim_max_iter = 2;              // 最多 2 次查询
    MagIterOutcome o = run_iter(s, 60, p);
    char msg[128];
    std::snprintf(msg, sizeof(msg), "参数: safety=5 -> N_target_eff=%.1f", o.n_target_eff);
    CHECK(std::fabs(o.n_target_eff - 300.0) < 1e-9, msg);
    CHECK(o.query_count == 2, "参数: m_lim_max_iter=2 生效 (恰 2 次后停止)");

    // clamp 上界生效 (恒欠查 -> m 顶到 clamp_hi)
    IPVSolverParams p2; p2.m_lim_clamp_hi = 15.0;
    Stub s2; s2.N_star = 1e-9; s2.n0 = 1.0;
    MagIterOutcome o2 = run_iter(s2, 60, p2);
    CHECK(o2.m_lim_final <= 15.0 + 1e-9, "参数: m_lim_clamp_hi=15 生效 (m 不越界)");

    // alpha 先验生效: 极端先验改变首次步进
    IPVSolverParams pa; pa.m_lim_alpha_prior = 0.05;
    Stub sa; sa.N_star = 180.0; sa.m_star = 16.0;
    MagIterOutcome oa = run_iter(sa, 60, pa);
    CHECK(oa.query_count >= 2 && sa.m_seen.size() >= 2, "参数: alpha 先验改变步进(仍迭代)");
    CHECK(oa.m_lim_final >= sa.m_seen[0], "参数: 小 alpha -> 步进更大(向暗端)");
}

// ===========================================================================
// 7. 异常路径
// ===========================================================================
static void test_error_paths() {
    IPVSolverParams p;
    Stub s; s.N_star = 180.0;
    MagIterOutcome o = run_iter(s, 0, p);
    CHECK(!o.valid && o.query_count == 0, "异常: n_target<=0 -> 无查询且 invalid");

    MagQueryFn empty;
    MagIterOutcome oe = estimate_mag_lim_iterative(empty, 60, 1877.0, 300.0, p, nullptr);
    CHECK(!oe.valid && oe.query_count == 0, "异常: 空回调 -> invalid");

    Stub sf; sf.N_star = 180.0; sf.m_star = 14.0; sf.fail_at_call = 2;
    MagIterOutcome of = run_iter(sf, 60, p);
    CHECK(of.query_failed, "异常: 第 2 次查询失败被标记");
    CHECK(of.valid, "异常: 末次成功结果保留 (valid)");
    CHECK(of.query_count == 2, "异常: 失败后立即停止 (query_count==2)");

    Stub s1; s1.fail_at_call = 1;
    MagIterOutcome o1 = run_iter(s1, 60, p);
    CHECK(!o1.valid && o1.query_count == 1, "异常: 首次查询失败 -> invalid");
}

// ===========================================================================
// 8. 失效面 fail-closed 锁 (P14-N-10 缺陷 3)
//   NaN/非法 focal 或 exposure / 非法 alpha 限幅 => 明确无效 (零查询),
//   不得静默 clamp 到 13.0 后照常迭代。
// ===========================================================================
static void test_invalid_inputs() {
    IPVSolverParams p;
    // focal = NaN
    {
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, p, std::nan(""), 300.0);
        CHECK(!o.valid, "失效面: focal=NaN -> invalid");
        CHECK(o.query_count == 0, "失效面: focal=NaN -> 零查询 (不静默迭代)");
        CHECK(!(o.m_lim_final == 13.0), "失效面: focal=NaN 不得静默落 13.0");
    }
    // exposure = NaN
    {
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, p, 1877.0, std::nan(""));
        CHECK(!o.valid, "失效面: exposure=NaN -> invalid");
        CHECK(o.query_count == 0, "失效面: exposure=NaN -> 零查询");
    }
    // focal <= 0
    {
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, p, 0.0, 300.0);
        CHECK(!o.valid && o.query_count == 0, "失效面: focal=0 -> invalid 且零查询");
    }
    // exposure <= 0
    {
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, p, 1877.0, 0.0);
        CHECK(!o.valid && o.query_count == 0, "失效面: exposure=0 -> invalid 且零查询");
    }
    // alpha_min <= 0 (旧实现只把它当限幅下界, <=0 时静默禁用 alpha 更新)
    {
        IPVSolverParams pa; pa.m_lim_alpha_min = 0.0;
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, pa);
        CHECK(!o.valid && o.query_count == 0, "失效面: alpha_min=0 -> invalid 且零查询");
    }
    // alpha_max <= alpha_min
    {
        IPVSolverParams pa; pa.m_lim_alpha_max = pa.m_lim_alpha_min;
        Stub s; s.N_star = 180.0;
        MagIterOutcome o = run_iter(s, 60, pa);
        CHECK(!o.valid && o.query_count == 0,
              "失效面: alpha_max<=alpha_min -> invalid 且零查询");
    }
    // 合法性保持: 生产默认参数仍然正常迭代 (不误伤)
    {
        Stub s; s.N_star = 180.0; s.m_star = 13.0;
        MagIterOutcome o = run_iter(s, 60, p);
        CHECK(o.valid && o.query_count >= 1, "失效面: 合法默认参数仍正常迭代 (不误伤)");
    }
}

int main() {
    std::printf("=== P4-magiter ipv_mag_iter 单元/回归锁 ===\n");
    test_convergence();
    test_determinism();
    test_alpha_update();
    test_cap_defense();
    test_zero_step();
    test_query_bound();
    test_param_override();
    test_error_paths();
    test_invalid_inputs();
    std::printf("=== checks=%d fail=%d ===\n", g_check, g_fail);
    if (g_fail != 0) { std::printf("RESULT: FAIL\n"); return 1; }
    std::printf("RESULT: PASS\n");
    return 0;
}
