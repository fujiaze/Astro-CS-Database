// ============================================================================
// test_triangle_budget.cpp — P11-IPV-BUDGET 搜索预算回归锁
//
// 锁定: triangle_match 在进入 O(n_A_tri·n_B_tri) 描述符枚举前用剪枝前上界
//       C(n_A,3)·C(n_B,3) 做预算判定; 超界 -> budget_exhausted=true,
//       success=false, fail_reason 非空, 且**在有界时间内返回** (不分配/不枚举);
//       生产 n=60 (上界 1.171e9 < 2e9) 永不触发, 结果与预算前逐位一致。
//
// 日期: 2026-09-14
// ============================================================================
#include "ipv_triangle.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {
int g_fail = 0;
void check(bool cond, const char* what) {
    if (!cond) { std::printf("FAIL: %s\n", what); ++g_fail; }
}
std::vector<ipv::StarPoint> make_stars(int n, double s0, unsigned seed) {
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> ux(-2048.0, 2048.0);
    std::vector<ipv::StarPoint> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        ipv::StarPoint sp{};
        sp.x = ux(rng);
        sp.y = ux(rng);
        sp.flux = (double)(1000 - i);
        sp.saturated = false;
        (void)s0;
        v.push_back(sp);
    }
    return v;
}
}  // namespace

int main() {
    const double s0 = 0.9890161182738412;
    // --- A. 生产 n=60: 预算不触发, 两次调用逐位一致 (确定性) ---
    auto U60 = make_stars(60, s0, 12345);
    auto W60 = make_stars(60, s0, 99999);
    auto t0 = std::chrono::steady_clock::now();
    ipv::TriangleMatchResult r1 = ipv::triangle_match(U60, W60, 60, 60, 0.002, s0);
    ipv::TriangleMatchResult r2 = ipv::triangle_match(U60, W60, 60, 60, 0.002, s0);
    auto t1 = std::chrono::steady_clock::now();
    check(!r1.budget_exhausted, "n=60 budget not exhausted");
    check(r1.success == r2.success, "n=60 deterministic success");
    check(r1.max_vote == r2.max_vote, "n=60 deterministic max_vote");
    check(r1.top_pairs.size() == r2.top_pairs.size(), "n=60 deterministic pair count");
    for (size_t i = 0; i < r1.top_pairs.size() && i < r2.top_pairs.size(); ++i) {
        check(r1.top_pairs[i].u == r2.top_pairs[i].u &&
              r1.top_pairs[i].w == r2.top_pairs[i].w, "n=60 deterministic pair values");
    }
    std::printf("n=60: max_vote=%d pairs=%zu elapsed=%.1fms\n", r1.max_vote,
                r1.top_pairs.size(),
                std::chrono::duration<double, std::milli>(t1 - t0).count());

    // --- B. 越界输入: 立即 fail-closed, 有明确原因, 且快 (不再枚举) ---
    // C(66,3)=45760, 平方 2.094e9 > 2e9 -> 触发。
    auto U66 = make_stars(66, s0, 7);
    auto W66 = make_stars(66, s0, 8);
    auto b0 = std::chrono::steady_clock::now();
    ipv::TriangleMatchResult rb = ipv::triangle_match(U66, W66, 66, 66, 0.002, s0);
    auto b1 = std::chrono::steady_clock::now();
    double bms = std::chrono::duration<double, std::milli>(b1 - b0).count();
    check(rb.budget_exhausted, "n=66 budget exhausted");
    check(!rb.success, "n=66 fail-closed");
    check(rb.fail_reason[0] != '\0', "n=66 has explicit fail reason");
    check(rb.top_pairs.empty(), "n=66 no pairs emitted");
    check(bms < 500.0, "n=66 bounded (<500ms, no enumeration)");
    std::printf("n=66: budget=%d elapsed=%.3fms reason=%s\n",
                (int)rb.budget_exhausted, bms, rb.fail_reason);

    // --- C. 大幅越界 (n=200): 也必须快速 fail-closed, 而不是先分配/枚举 ---
    auto U200 = make_stars(200, s0, 11);
    auto W200 = make_stars(200, s0, 12);
    auto c0 = std::chrono::steady_clock::now();
    ipv::TriangleMatchResult rc = ipv::triangle_match(U200, W200, 200, 200, 0.002, s0);
    auto c1 = std::chrono::steady_clock::now();
    double cms = std::chrono::duration<double, std::milli>(c1 - c0).count();
    check(rc.budget_exhausted, "n=200 budget exhausted");
    check(!rc.success, "n=200 fail-closed");
    check(rc.fail_reason[0] != '\0', "n=200 has explicit fail reason");
    check(cms < 500.0, "n=200 bounded (<500ms)");
    std::printf("n=200: budget=%d elapsed=%.3fms\n", (int)rc.budget_exhausted, cms);

    if (g_fail != 0) { std::printf("IPV_TRIANGLE_BUDGET: FAIL (%d)\n", g_fail); return 1; }
    std::printf("IPV_TRIANGLE_BUDGET: PASS\n");
    return 0;
}
