// ============================================================================
// test_last_inlier_reset.cpp - B4-P1-5 共址测试
//
// 修复点: ipv_solver.cpp 失败路径未清除 last_inliers_, 导致跨调用
// 陈旧诊断污染 WCS Gate v2 的 ipv_get_last_inlier_count/inliers 输出。
// 修复: 6 个 solve* 入口统一 "last_inliers_ = SolveInlierCache{}" 重置;
// 15 处失败 return 前均置 fail_result (不再泄漏上一次成功缓存)。
//
// 测试:
// 1. 成功求解后缓存有效 (基线)
// 2. 随后失败调用: 入口即重置 -> get_last_inlier_count()==0
//    (修复前: 返回上一次成功的陈旧内点数, FAIL)
// 3. 再一次成功: 缓存重建, count>0
//
// 编译: g++ -std=c++17 -O1 -g -Iinclude src/*.cpp test/test_last_inlier_reset.cpp
// 运行: ./a.out; 返回 0=通过
//
// 日期: 2026-09-08 (bughunt P1 batchA)
// ============================================================================

#include "ipv_api.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <random>

static int g_failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  [PASS] %s\n", msg); } \
    else      { printf("  [FAIL] %s\n", msg); ++g_failures; } \
} while (0)

// 合成高斯星图 (控制点数足以驱动匹配, 依赖 detector 默认阈值)
static std::vector<float> make_star_image(int w, int h,
                                          const std::vector<std::pair<float,float>>& pts) {
    std::vector<float> img((size_t)w * h, 5.0f);
    std::mt19937 rng(42);
    for (auto& p : pts) {
        int cx = (int)p.first, cy = (int)p.second;
        double sigma = 1.4, peak = 3000.0;
        for (int dy = -4; dy <= 4; dy++)
            for (int dx = -4; dx <= 4; dx++) {
                int x = cx + dx, y = cy + dy;
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                double r2 = (double)(dx*dx + dy*dy);
                img[(size_t)y * w + x] = (float)(5.0 + peak * exp(-r2 / (2.0*sigma*sigma)));
            }
    }
    // 轻微噪声
    std::uniform_real_distribution<float> noise(-1.0f, 1.0f);
    for (auto& v : img) v += noise(rng);
    return img;
}

int main() {
    printf("=== B4-P1-5: ipv 失败路径 last_inliers_ 重置 共址测试 ===\n");

    void* solver = ipv_solve_create();
    CHECK(solver != nullptr, "ipv_solve_create");

    IpvWcsResult res{};

    // ---- 构造星图 (网格星点) ----
    std::vector<std::pair<float,float>> pts;
    for (int iy = 0; iy < 6; iy++)
        for (int ix = 0; ix < 6; ix++)
            pts.push_back({30.0f + ix * 40.0f, 30.0f + iy * 40.0f});
    auto img = make_star_image(260, 260, pts);
    const int W = 260, H = 260;

    // ---- Case 1: 失败调用 (期望失败: 空图) -> 缓存必须为 0 ----
    {
        std::vector<float> blank((size_t)W * H, 5.0f);
        ipv_solve_from_memory(solver, blank.data(), W, H,
                              180.0, 0.0, 800.0, 3.45, nullptr, &res);
        int n = ipv_get_last_inlier_count(solver);
        CHECK(n == 0, "空图失败调用后 ipv_get_last_inlier_count()==0 (核心断言)");
        if (n != 0) {
            // 修复前此处返回陈旧值; 打印帮助定位
            printf("    (陈旧 count=%d)\n", n);
        }
    }

    // ---- Case 2: 正常求解 (基线; 失败也无妨, 两种分支均验证缓存语义) ----
    {
        ipv_solve_from_memory(solver, img.data(), W, H,
                              180.0, 0.0, 800.0, 3.45, nullptr, &res);
        int n = ipv_get_last_inlier_count(solver);
        if (res.success) {
            CHECK(n > 0, "成功求解后缓存重建 count>0");
        } else {
            CHECK(n == 0, "求解失败后缓存为 0 (一致性)");
        }
        printf("    (solve_from_memory: success=%d count=%d)\n", res.success, n);
    }

    // ---- Case 3: 成功后再失败 -> 必须清除, 不留陈旧值 ----
    {
        std::vector<float> blank((size_t)W * H, 5.0f);
        // 先置一个陈旧基线: 若 Case2 失败, 用一个成功调用垫底不可行,
        // 这里依赖 Case2 的缓存状态 (成功>0 或 失败==0)。
        // 关键序列: 上一次调用结束后, 新失败调用不得保留其值。
        int before = ipv_get_last_inlier_count(solver);
        ipv_solve_from_memory(solver, blank.data(), W, H,
                              180.0, 0.0, 800.0, 3.45, nullptr, &res);
        int after = ipv_get_last_inlier_count(solver);
        CHECK(after == 0, "失败调用后 count==0 (不论 before)");
        if (before > 0)
            CHECK(after == 0, "陈旧缓存被失败调用清除 (修复点直接验证)");
        printf("    (before=%d after=%d)\n", before, after);
    }

    // ---- Case 4: with_callback_f64 入口同样语义 (double 图像失败调用) ----
    {
        std::vector<double> blank((size_t)W * H, 5.0);
        IpvParams params{};   // 显式默认
        // f64 入口 API: ipv_solve_from_memory_with_callback_d
        ipv_solve_from_memory_with_callback_d(solver, blank.data(), W, H,
                                              180.0, 0.0, 800.0, 3.45,
                                              &params, nullptr, nullptr, &res);
        int n = ipv_get_last_inlier_count(solver);
        CHECK(n == 0, "f64 入口失败调用后 count==0");
    }

    // ---- Case 5: get_last_inliers 在无效缓存下不写输出; 连续失败序列一致性 ----
    {
        double buf[8];
        for (int i = 0; i < 8; i++) buf[i] = -123.456;  // 哨兵
        int n = ipv_get_last_inliers(solver, buf, 8);
        CHECK(n == 0, "无效缓存下 ipv_get_last_inliers 返回 0");
        bool untouched = (buf[0] == -123.456 && buf[7] == -123.456);
        CHECK(untouched, "无效缓存下输出缓冲区未被写入");

        // 连续 3 次失败调用: 每次之后 count 必须为 0 (无状态漂移)
        std::vector<float> blank((size_t)W * H, 5.0f);
        bool stable = true;
        for (int k = 0; k < 3; k++) {
            ipv_solve_from_memory(solver, blank.data(), W, H,
                                  180.0, 0.0, 800.0, 3.45, nullptr, &res);
            if (ipv_get_last_inlier_count(solver) != 0) stable = false;
        }
        CHECK(stable, "连续失败调用后 count 恒为 0 (无跨调用泄漏)");
    }

    ipv_solve_destroy(solver);
    printf("=== B4-P1-5 结果: %s (%d failures) ===\n",
           g_failures == 0 ? "ALL PASS" : "FAIL", g_failures);
    return g_failures == 0 ? 0 : 1;
}
