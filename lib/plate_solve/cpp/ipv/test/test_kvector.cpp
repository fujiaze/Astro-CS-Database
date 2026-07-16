// ============================================================================
// test_kvector.cpp - IPV k-vector 模块单元测试
//
// 测试内容:
//   1. 构建: N_W=80 合成星点, 验证 n_pairs == 80*79/2 = 3160
//   2. 查询空区间 (d_lo > d_max): 返回 0 个
//   3. 查询单点 (d_lo == d_hi == 某个距离): 返回对应星对
//   4. 查询多点 (正常区间): 验证返回的星对距离都在 [d_lo, d_hi] 内
//   5. 查询边界 (d_lo = d_min, d_hi = d_max): 返回全部星对
//   6. 与暴力 O(M) 查询对比, 验证结果一致
//
// 编译: g++ -std=c++17 -O2 -I include src/ipv_kvector.cpp test/test_kvector.cpp -o test_kvector.exe
// 运行: ./test_kvector.exe
// 返回: 0=通过, 非0=失败
//
// 日期: 2026-07-02
// ============================================================================

#include "ipv_kvector.h"

#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>

using namespace ipv;

// ----------------------------------------------------------------------------
// 辅助: 计算两点欧氏距离
// ----------------------------------------------------------------------------
static inline double euclidean_dist(const StarPoint& a, const StarPoint& b) {
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// ----------------------------------------------------------------------------
// 辅助: 暴力 O(M) 查询, 返回所有 a<b 且距离在 [d_lo, d_hi] 内的星对
// ----------------------------------------------------------------------------
static std::vector<std::pair<int,int>> brute_force_query(
    const std::vector<StarPoint>& W,
    double d_lo, double d_hi)
{
    std::vector<std::pair<int,int>> result;
    int n = (int)W.size();
    for (int a = 0; a < n; ++a) {
        for (int b = a + 1; b < n; ++b) {
            double d = euclidean_dist(W[a], W[b]);
            if (d >= d_lo && d <= d_hi) {
                result.push_back({a, b});
            }
        }
    }
    return result;
}

// ----------------------------------------------------------------------------
// 辅助: 比较两组星对是否一致 (顺序无关)
// ----------------------------------------------------------------------------
static bool pairs_match(std::vector<std::pair<int,int>> a,
                        std::vector<std::pair<int,int>> b)
{
    if (a.size() != b.size()) return false;
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// 测试用例 1: 构建验证 (n_pairs == 3160)
// ----------------------------------------------------------------------------
static int test_build(const std::vector<StarPoint>& W, const KVectorIndex& kv) {
    const int N_W = 80;
    const size_t expected_pairs = (size_t)N_W * (N_W - 1) / 2;  // 3160

    if (kv.n_stars != N_W) {
        std::fprintf(stderr, "[FAIL] test_build: n_stars=%d, expected=%d\n",
                     kv.n_stars, N_W);
        return 1;
    }
    if (kv.n_pairs != expected_pairs) {
        std::fprintf(stderr, "[FAIL] test_build: n_pairs=%zu, expected=%zu\n",
                     kv.n_pairs, expected_pairs);
        return 1;
    }
    if (kv.distances.size() != expected_pairs) {
        std::fprintf(stderr, "[FAIL] test_build: distances.size=%zu, expected=%zu\n",
                     kv.distances.size(), expected_pairs);
        return 1;
    }
    if (kv.pairs.size() != expected_pairs) {
        std::fprintf(stderr, "[FAIL] test_build: pairs.size=%zu, expected=%zu\n",
                     kv.pairs.size(), expected_pairs);
        return 1;
    }
    if (!kv.built) {
        std::fprintf(stderr, "[FAIL] test_build: built=false\n");
        return 1;
    }

    // 验证 distances 升序
    for (size_t i = 1; i < kv.distances.size(); ++i) {
        if (kv.distances[i] < kv.distances[i-1]) {
            std::fprintf(stderr, "[FAIL] test_build: distances 未升序 (i=%zu)\n", i);
            return 1;
        }
    }

    // 验证 d_min / d_max
    if (std::abs(kv.d_min - kv.distances.front()) > 1e-9) {
        std::fprintf(stderr, "[FAIL] test_build: d_min 不匹配\n");
        return 1;
    }
    if (std::abs(kv.d_max - kv.distances.back()) > 1e-9) {
        std::fprintf(stderr, "[FAIL] test_build: d_max 不匹配\n");
        return 1;
    }

    // 验证 pairs 中 a < b
    for (const auto& p : kv.pairs) {
        if (p.first >= p.second) {
            std::fprintf(stderr, "[FAIL] test_build: pair(%d,%d) 不满足 a<b\n",
                         p.first, p.second);
            return 1;
        }
    }

    std::printf("[PASS] test_build: n_stars=%d n_pairs=%zu d_min=%.4f d_max=%.4f\n",
                kv.n_stars, kv.n_pairs, kv.d_min, kv.d_max);
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 2: 空区间 (d_lo > d_max)
// ----------------------------------------------------------------------------
static int test_empty_range(const KVectorIndex& kv) {
    auto result = kvector_query(kv, kv.d_max + 1.0, kv.d_max + 10.0);
    if (!result.empty()) {
        std::fprintf(stderr, "[FAIL] test_empty_range: 返回 %zu 个 (期望 0)\n",
                     result.size());
        return 1;
    }
    std::printf("[PASS] test_empty_range: d_lo>d_max 返回 0 个\n");
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 3: 单点查询 (d_lo == d_hi == 某个距离)
// ----------------------------------------------------------------------------
static int test_single_point(const std::vector<StarPoint>& W,
                              const KVectorIndex& kv) {
    // 取第 100 个星对的距离
    if (kv.n_pairs < 100) {
        std::fprintf(stderr, "[SKIP] test_single_point: 星对数不足\n");
        return 0;
    }
    double d_target = kv.distances[100];
    auto result = kvector_query(kv, d_target, d_target);

    // 验证返回的所有星对距离都等于 d_target (考虑浮点误差)
    for (const auto& p : result) {
        double d = euclidean_dist(W[p.first], W[p.second]);
        if (std::abs(d - d_target) > 1e-9) {
            std::fprintf(stderr, "[FAIL] test_single_point: 星对(%d,%d) d=%.6f 期望=%.6f\n",
                         p.first, p.second, d, d_target);
            return 1;
        }
    }

    // 验证: 至少返回 1 个 (distances[100] 对应的星对必在结果中)
    if (result.empty()) {
        std::fprintf(stderr, "[FAIL] test_single_point: 单点查询返回空\n");
        return 1;
    }

    std::printf("[PASS] test_single_point: d=%.4f 返回 %zu 个星对\n",
                d_target, result.size());
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 4: 多点查询 (正常区间, 验证距离都在范围内)
// ----------------------------------------------------------------------------
static int test_range_query(const std::vector<StarPoint>& W,
                             const KVectorIndex& kv) {
    // 取 [d_min + 10, d_min + 100] 区间
    double d_lo = kv.d_min + 10.0;
    double d_hi = kv.d_min + 100.0;
    auto result = kvector_query(kv, d_lo, d_hi);

    // 验证所有返回星对距离都在 [d_lo, d_hi] 内
    for (const auto& p : result) {
        double d = euclidean_dist(W[p.first], W[p.second]);
        if (d < d_lo - 1e-9 || d > d_hi + 1e-9) {
            std::fprintf(stderr, "[FAIL] test_range_query: 星对(%d,%d) d=%.4f 越界 [%.4f,%.4f]\n",
                         p.first, p.second, d, d_lo, d_hi);
            return 1;
        }
    }

    std::printf("[PASS] test_range_query: [%.2f,%.2f] 返回 %zu 个星对, 全部在范围内\n",
                d_lo, d_hi, result.size());
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 5: 边界查询 (d_lo=d_min, d_hi=d_max → 全部星对)
// ----------------------------------------------------------------------------
static int test_full_range(const KVectorIndex& kv) {
    auto result = kvector_query(kv, kv.d_min, kv.d_max);
    if (result.size() != kv.n_pairs) {
        std::fprintf(stderr, "[FAIL] test_full_range: 返回 %zu, 期望 %zu\n",
                     result.size(), kv.n_pairs);
        return 1;
    }
    std::printf("[PASS] test_full_range: [d_min,d_max] 返回全部 %zu 个星对\n",
                result.size());
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 6: 与暴力 O(M) 查询对比
// ----------------------------------------------------------------------------
static int test_against_brute_force(const std::vector<StarPoint>& W,
                                     const KVectorIndex& kv) {
    // 多个测试区间
    struct TestRange { double lo, hi; const char* name; };
    std::vector<TestRange> ranges = {
        {kv.d_min, kv.d_max,                           "全范围"},
        {kv.d_min + 50, kv.d_min + 200,                "中段"},
        {kv.d_max - 100, kv.d_max + 100,               "上边界外扩"},
        {kv.d_min - 100, kv.d_min + 50,                "下边界外扩"},
        {kv.d_min + 500, kv.d_min + 500.5,             "窄区间"},
        {kv.d_max + 1, kv.d_max + 10,                  "完全在外"},
        {kv.d_min, kv.d_min,                           "单点d_min"},
        {kv.d_max, kv.d_max,                           "单点d_max"},
    };

    int failures = 0;
    for (const auto& r : ranges) {
        auto kv_result  = kvector_query(kv, r.lo, r.hi);
        auto bf_result  = brute_force_query(W, r.lo, r.hi);

        if (!pairs_match(kv_result, bf_result)) {
            std::fprintf(stderr,
                "[FAIL] test_brute_force[%s]: kvector=%zu brute=%zu 不一致\n",
                r.name, kv_result.size(), bf_result.size());
            failures++;
        } else {
            std::printf("[PASS] test_brute_force[%s]: [%g,%g] kvector=%zu brute=%zu 一致\n",
                        r.name, r.lo, r.hi, kv_result.size(), bf_result.size());
        }
    }
    return failures > 0 ? 1 : 0;
}

// ----------------------------------------------------------------------------
// 测试用例 7: 边界情况 (d_lo > d_hi 应返回空)
// ----------------------------------------------------------------------------
static int test_invalid_range(const KVectorIndex& kv) {
    auto result = kvector_query(kv, 100.0, 50.0);
    if (!result.empty()) {
        std::fprintf(stderr, "[FAIL] test_invalid_range: d_lo>d_hi 返回 %zu (期望 0)\n",
                     result.size());
        return 1;
    }
    std::printf("[PASS] test_invalid_range: d_lo>d_hi 返回 0 个\n");
    return 0;
}

// ----------------------------------------------------------------------------
// 测试用例 8: 极少星点 (N_W < 2)
// ----------------------------------------------------------------------------
static int test_edge_cases() {
    // N_W = 0
    {
        std::vector<StarPoint> empty;
        KVectorIndex kv = kvector_build(empty);
        if (!kv.built || kv.n_pairs != 0 || kv.n_stars != 0) {
            std::fprintf(stderr, "[FAIL] test_edge_cases: 空集构建错误\n");
            return 1;
        }
        auto r = kvector_query(kv, 0.0, 100.0);
        if (!r.empty()) {
            std::fprintf(stderr, "[FAIL] test_edge_cases: 空集查询返回非空\n");
            return 1;
        }
    }
    // N_W = 1
    {
        std::vector<StarPoint> single = {{1.0, 2.0, 1.0, false}};
        KVectorIndex kv = kvector_build(single);
        if (!kv.built || kv.n_pairs != 0 || kv.n_stars != 1) {
            std::fprintf(stderr, "[FAIL] test_edge_cases: 单星构建错误\n");
            return 1;
        }
    }
    std::printf("[PASS] test_edge_cases: 空集/单星 边界正确\n");
    return 0;
}

// ============================================================================
// main
// ============================================================================
int main()
{
    std::printf("=== IPV k-vector 单元测试开始 ===\n");

    // ---- 生成 N_W=80 合成星点 (随机分布在 4000"×4000" 区域) ----
    const int N_W = 80;
    const double REGION = 4000.0;  // 角秒
    std::mt19937 rng(42);          // 固定种子, 可复现
    std::uniform_real_distribution<double> ud(-REGION / 2.0, REGION / 2.0);

    std::vector<StarPoint> W;
    W.reserve(N_W);
    for (int i = 0; i < N_W; ++i) {
        W.push_back({ud(rng), ud(rng), 1.0, false});
    }

    // ---- 构建 k-vector ----
    KVectorIndex kv = kvector_build(W);

    // ---- 运行所有测试 ----
    int n_fail = 0;
    n_fail += test_build(W, kv);
    n_fail += test_empty_range(kv);
    n_fail += test_single_point(W, kv);
    n_fail += test_range_query(W, kv);
    n_fail += test_full_range(kv);
    n_fail += test_against_brute_force(W, kv);
    n_fail += test_invalid_range(kv);
    n_fail += test_edge_cases();

    // ---- 汇总 ----
    std::printf("=== 测试汇总 ===\n");
    if (n_fail == 0) {
        std::printf("[ALL PASS] 所有测试用例通过\n");
        return 0;
    } else {
        std::printf("[FAILED] %d 个测试用例失败\n", n_fail);
        return n_fail;
    }
}
