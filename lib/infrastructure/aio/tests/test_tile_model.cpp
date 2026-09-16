// ============================================================================
// test_tile_model.cpp - WP-A Tile 模型与 signal/support 语义单元测试
//
// 测试范围 (依据 docs/stage1_fix/tasks.md WP-A 验收):
// 1. NSIDE=64, tile_nside=16 → n_leaf_per_tile = 16 (4^2, 不是 3072)
// 2. NSIDE=32768, tile_nside=64 → n_leaf_per_tile = 262144 (4^9, 满 Tile 上限)
// 3. NSIDE=4096, tile_nside=16 → n_leaf_per_tile = 65536 (4^8)
// 4. local_to_global / global_to_local 往返一致
// 5. signal = 累计通量 (不除面积)
// 6. support = 面积比 S = sum_area / A_p, 范围 [0,1]
//
// 编译 (PowerShell + mingw64):
// g++ -std=c++17 -O2 -I../include -I../src `
// test_tile_model.cpp ../src/hiss_tile_model.cpp ../src/hiss_common.cpp `
// -o test_tile_model.exe
// ./test_tile_model.exe
//
// 测试框架: 自维护通过/失败计数, 任何契约不满足必须真正失败
// (禁止 ASSERT_TRUE(true, "已知问题") 软通过, 依据 spec.md §3 步骤15)
// ============================================================================

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>

#include "hiss_format.h"     // DrizzleTileAccumulator
#include "hiss_tile_model.h" // HissTileGeometry, make_tile_geometry

// ============================================================================
// 测试框架: 计数 + 断言宏
// ============================================================================
static int g_pass_count = 0;
static int g_fail_count = 0;

// ASSERT_TRUE(cond, msg): cond 为假时记失败并打印
// 注意: 不支持 "true, 已知问题" 软通过 — 任何 false 都是真失败
#define ASSERT_TRUE(cond, msg) do { \
    if (cond) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s\n", msg); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (line %d)\n", msg, __LINE__); \
    } \
} while (0)

// ASSERT_EQ_INT(a, b, msg): 整数相等
#define ASSERT_EQ_INT(a, b, msg) do { \
    long _a = (long)(a); \
    long _b = (long)(b); \
    if (_a == _b) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s (=%ld)\n", msg, _a); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (got=%ld, want=%ld, line %d)\n", \
                msg, _a, _b, __LINE__); \
    } \
} while (0)

// ASSERT_EQ_FLT(a, b, tol, msg): 浮点数近似相等
#define ASSERT_EQ_FLT(a, b, tol, msg) do { \
    double _a = (double)(a); \
    double _b = (double)(b); \
    double _d = _a - _b; \
    if (_d < 0) _d = -_d; \
    if (_d <= (double)(tol)) { \
        g_pass_count++; \
        fprintf(stdout, "  OK: %s (a=%g, b=%g)\n", msg, _a, _b); \
    } else { \
        g_fail_count++; \
        fprintf(stderr, "  FAIL: %s (got=%g, want=%g, tol=%g, line %d)\n", \
                msg, _a, _b, (double)(tol), __LINE__); \
    } \
} while (0)

// ============================================================================
// 测试 1: NSIDE=64 → depth=2, tile_nside=16, n_leaf_per_tile=16
// 旧错误: tile_nside^2 * 12 = 16*16*12 = 3072 (全天像素数, 错误)
// 新正确: 4^2 = 16
// ============================================================================
static void test_nside64_tile16() {
    fprintf(stdout, "[TEST] NSIDE=64, tile_nside=16 → n_leaf=16 (不是 3072)\n");
    hiss::HissTileGeometry g = hiss::make_tile_geometry(64);

    ASSERT_EQ_INT(g.nside, 64, "nside=64");
    ASSERT_EQ_INT(g.depth, 2, "depth=2 (log2(64/16)=2)");
    ASSERT_EQ_INT(g.tile_nside, 16, "tile_nside=16 (64/2^2)");
    ASSERT_EQ_INT(g.n_leaf_per_tile, 16, "n_leaf_per_tile=16 (4^2, 不是 3072)");

    // 明确验证: 不是旧错误值 3072
    ASSERT_TRUE(g.n_leaf_per_tile != 3072,
                "n_leaf_per_tile != tile_nside^2 * 12 (3072)");
    // 明确验证: 等于 4^depth
    uint32_t expected_4d = 1u << (2 * g.depth);  // 4^d = 2^(2d)
    ASSERT_EQ_INT(g.n_leaf_per_tile, expected_4d,
                  "n_leaf_per_tile == 4^depth");
}

// ============================================================================
// 测试 2: NSIDE=32768 → depth=9, tile_nside=64, n_leaf_per_tile=262144
// 满 Tile 上限: 4^9 = 262144
// ============================================================================
static void test_nside32768_full_tile() {
    fprintf(stdout, "[TEST] NSIDE=32768, tile_nside=64 → n_leaf=262144 (满 Tile 上限)\n");
    hiss::HissTileGeometry g = hiss::make_tile_geometry(32768);

    ASSERT_EQ_INT(g.nside, 32768, "nside=32768");
    ASSERT_EQ_INT(g.depth, 9, "depth=9 (min(9, log2(32768/16))=min(9,11)=9)");
    ASSERT_EQ_INT(g.tile_nside, 64, "tile_nside=64 (32768/2^9)");
    ASSERT_EQ_INT(g.n_leaf_per_tile, 262144, "n_leaf_per_tile=262144 (4^9, 满 Tile 上限)");
    ASSERT_TRUE(g.n_leaf_per_tile != 64*64*12,
                "n_leaf_per_tile != tile_nside^2*12 (49152)");
}

// ============================================================================
// 测试 3: NSIDE=4096 → depth=8, tile_nside=16, n_leaf_per_tile=65536
// ============================================================================
static void test_nside4096_tile16() {
    fprintf(stdout, "[TEST] NSIDE=4096, tile_nside=16 → n_leaf=65536\n");
    hiss::HissTileGeometry g = hiss::make_tile_geometry(4096);

    ASSERT_EQ_INT(g.nside, 4096, "nside=4096");
    ASSERT_EQ_INT(g.depth, 8, "depth=8 (log2(4096/16)=8)");
    ASSERT_EQ_INT(g.tile_nside, 16, "tile_nside=16 (4096/2^8)");
    ASSERT_EQ_INT(g.n_leaf_per_tile, 65536, "n_leaf_per_tile=65536 (4^8)");
    ASSERT_TRUE(g.n_leaf_per_tile != 16*16*12,
                "n_leaf_per_tile != tile_nside^2*12 (3072)");
}

// ============================================================================
// 测试 4: local_to_global / global_to_local 往返一致
// 覆盖多个 NSIDE 和多个 parent_ipix
// ============================================================================
static void test_local_global_roundtrip() {
    fprintf(stdout, "[TEST] local_to_global / global_to_local 往返一致\n");

    // 测试矩阵: (nside, parent_ipix) 组合
    struct Case { uint32_t nside; uint64_t parent; };
    Case cases[] = {
        {64,     0},
        {64,     1},
        {64,     100},
        {32768,  0},
        {32768,  12345},
        {4096,   7},
        {4096,   255},
        {16,     0},   // 边界: nside=16, depth=0, n_leaf=1
        {8,      0},   // nside<16, depth=0, n_leaf=1
    };

    for (const auto& c : cases) {
        hiss::HissTileGeometry g = hiss::make_tile_geometry_for_parent(c.nside, c.parent);

        fprintf(stdout, "  -- nside=%u, parent=%llu, depth=%d, n_leaf=%u\n",
                c.nside, (unsigned long long)c.parent, g.depth, g.n_leaf_per_tile);

        // 遍历所有 local_ipix, 验证往返
        bool all_ok = true;
        uint32_t n_fail = 0;
        for (uint32_t local = 0; local < g.n_leaf_per_tile; local++) {
            uint64_t global = g.local_to_global(local);
            if (global == UINT64_MAX) {
                all_ok = false; n_fail++; continue;
            }
            uint32_t local_back = g.global_to_local(global);
            if (local_back != local) {
                all_ok = false; n_fail++;
                if (n_fail <= 3) {
                    fprintf(stderr,
                            "     FAIL: local=%u → global=%llu → local=%u\n",
                            local, (unsigned long long)global, local_back);
                }
            }
            // 同时验证 parent 关系
            uint64_t parent_back = g.global_to_parent(global);
            if (parent_back != c.parent) {
                all_ok = false; n_fail++;
                if (n_fail <= 3) {
                    fprintf(stderr,
                            "     FAIL: local=%u → global=%llu → parent=%llu (want %llu)\n",
                            local, (unsigned long long)global,
                            (unsigned long long)parent_back,
                            (unsigned long long)c.parent);
                }
            }
        }
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "nside=%u parent=%llu 往返一致 (n_leaf=%u, fail=%u)",
                      c.nside, (unsigned long long)c.parent,
                      g.n_leaf_per_tile, n_fail);
        ASSERT_TRUE(all_ok, msg);
    }

    // 额外: owns_global 校验
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry_for_parent(64, 5);
        // parent=5, depth=2, n_leaf=16, local 0..15 都属于 parent 5
        bool owns_ok = true;
        for (uint32_t local = 0; local < g.n_leaf_per_tile; local++) {
            uint64_t global = g.local_to_global(local);
            if (!g.owns_global(global)) { owns_ok = false; break; }
        }
        // parent=6 的像素不应该属于 parent=5
        // global_ipix for parent=6, local=0 = (6 << 4) | 0 = 96
        uint64_t other_global = ((uint64_t)6 << (2 * g.depth)) | 0;
        bool not_owns = !g.owns_global(other_global);
        ASSERT_TRUE(owns_ok && not_owns, "owns_global 校验正确 (自己拥有, 他人不拥有)");
    }

    // 额外: 非法 local_ipix 应返回 UINT64_MAX
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry(64);
        uint64_t bad = g.local_to_global(g.n_leaf_per_tile);  // 越界
        ASSERT_EQ_INT(bad, UINT64_MAX, "越界 local_ipix 返回 UINT64_MAX");
    }

    // 额外: is_valid_local
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry(64);
        ASSERT_TRUE(g.is_valid_local(0), "is_valid_local(0) = true");
        ASSERT_TRUE(g.is_valid_local(15), "is_valid_local(15) = true");
        ASSERT_TRUE(!g.is_valid_local(16), "is_valid_local(16) = false (越界)");
    }
}

// ============================================================================
// 测试 5: signal = 累计通量 (不除面积)
// 构造已知 sum_flux 和 sum_area 的累加器, 验证 signal 输出 == sum_flux
// ============================================================================
static void test_signal_is_cumulative_flux() {
    fprintf(stdout, "[TEST] signal = 累计通量 (不除面积)\n");

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = 0;
    acc.pixel_area = 1.0;  // 任意值, signal 不依赖 A_p

    // 构造 4 个像素:
    // px0: sum_flux=100.0, sum_area=0.5 → 旧错误 signal=200.0, 新正确 signal=100.0
    // px1: sum_flux=50.0, sum_area=1.0 → 旧错误 signal=50.0, 新正确 signal=50.0
    // px2: sum_flux=0.0, sum_area=0.0 → 无贡献, signal=0.0
    // px3: sum_flux=250.0, sum_area=0.25 → 旧错误 signal=1000.0, 新正确 signal=250.0
    acc.pixels.resize(4);
    acc.pixels[0].sum_flux = 100.0; acc.pixels[0].sum_area = 0.5;
    acc.pixels[1].sum_flux = 50.0;  acc.pixels[1].sum_area = 1.0;
    acc.pixels[2].sum_flux = 0.0;   acc.pixels[2].sum_area = 0.0;
    acc.pixels[3].sum_flux = 250.0; acc.pixels[3].sum_area = 0.25;

    std::vector<float> signal;
    acc.finalize_signal(signal);

    ASSERT_EQ_INT(signal.size(), 4u, "signal 长度 == pixels.size()");

    // 验证 signal 直接等于 sum_flux (不除面积)
    ASSERT_EQ_FLT(signal[0], 100.0f, 1e-5, "signal[0]=100.0 (累计通量, 不是 200.0)");
    ASSERT_EQ_FLT(signal[1], 50.0f,  1e-5, "signal[1]=50.0 (累计通量)");
    ASSERT_EQ_FLT(signal[2], 0.0f,   1e-5, "signal[2]=0.0 (无贡献)");
    ASSERT_EQ_FLT(signal[3], 250.0f, 1e-5, "signal[3]=250.0 (累计通量, 不是 1000.0)");

    // 通量守恒: 总 signal = 400.0 (= 总 sum_flux)
    double total_signal = (double)signal[0] + signal[1] + signal[2] + signal[3];
    ASSERT_EQ_FLT(total_signal, 400.0, 1e-4, "总 signal == 总 sum_flux (通量守恒)");
}

// ============================================================================
// 测试 6: support = 面积比 S = sum_area / A_p, 范围 [0,1]
// 构造已知 sum_area 和 A_p 的累加器, 验证 support 输出 == round(255 * S)
// ============================================================================
static void test_support_is_area_ratio() {
    fprintf(stdout, "[TEST] support = 面积比 S = sum_area / A_p\n");

    hiss::DrizzleTileAccumulator acc;
    acc.tile_nside = 16;
    acc.parent_ipix = 0;

    // 设置 A_p = 0.5 (球面度, 任意值用于测试)
    // S = sum_area / A_p
    // support = round(255 * clamp(S, 0, 1))
    const double A_p = 0.5;
    acc.pixel_area = A_p;

    // 构造 5 个像素:
    // px0: sum_area=0.0 → S=0.0 → support=0
    // px1: sum_area=0.25 → S=0.5 → support=round(127.5)=128 (银行家舍入) 或 127
    // px2: sum_area=0.5 → S=1.0 → support=255
    // px3: sum_area=0.1 → S=0.2 → support=round(51.0)=51
    // px4: sum_area=1.0 → S=2.0 → 钳制到 1.0 → support=255 (异常情况, 浮点级超限钳制)
    acc.pixels.resize(5);
    acc.pixels[0].sum_area = 0.0;
    acc.pixels[1].sum_area = 0.25;
    acc.pixels[2].sum_area = 0.5;
    acc.pixels[3].sum_area = 0.1;
    acc.pixels[4].sum_area = 1.0;  // 明显超 1 (S=2.0), 钳制到 1.0

    std::vector<uint8_t> support;
    acc.finalize_support(support);

    ASSERT_EQ_INT(support.size(), 5u, "support 长度 == pixels.size()");

    // 验证 support = round(255 * clamp(sum_area/A_p, 0, 1))
    ASSERT_EQ_INT(support[0], 0,   "support[0]=0 (S=0.0)");
    // S=0.5, 255*0.5=127.5, round 半偶数 → 128 (std::lround 真正四舍五入 → 128)
    ASSERT_EQ_INT(support[1], 128, "support[1]=128 (S=0.5, round(127.5)=128)");
    ASSERT_EQ_INT(support[2], 255, "support[2]=255 (S=1.0, 完全覆盖)");
    // S=0.2, 255*0.2=51.0, round=51
    ASSERT_EQ_INT(support[3], 51,  "support[3]=51 (S=0.2, round(51.0)=51)");
    // S=2.0 钳制到 1.0 → support=255
    ASSERT_EQ_INT(support[4], 255, "support[4]=255 (S=2.0 钳制到 1.0)");

    // 验证旧错误: 如果不归一化 (A_p=1.0), sum_area=0.5 → support=128
    // 现在归一化后 (A_p=0.5), sum_area=0.5 → S=1.0 → support=255
    // 这证明了 support 是面积比, 不是 sum_area 直接值
    ASSERT_TRUE(support[2] == 255,
                "sum_area=0.5, A_p=0.5 → S=1.0 → support=255 (证明归一化生效)");

    // validate_support: px4 (S=2.0) 明显超 1, 应返回 <0
    int v = acc.validate_support();
    ASSERT_TRUE(v < 0, "validate_support: S=2.0 明显超限 → 返回 <0");

    // 修正后: px4 sum_area 改为 0.5 (S=1.0, 合法), validate_support 应返回 0
    acc.pixels[4].sum_area = 0.5;
    int v2 = acc.validate_support();
    ASSERT_TRUE(v2 == 0, "validate_support: 所有 S 在 [0,1] → 返回 0");
}

// ============================================================================
// 测试 7: 旧错误值 tile_nside^2 * 12 必须不等于 n_leaf_per_tile
// 这是回归保护: 防止代码回退到旧错误公式
// ============================================================================
static void test_not_old_wrong_formula() {
    fprintf(stdout, "[TEST] 回归保护: n_leaf != tile_nside^2 * 12 (旧错误公式)\n");

    uint32_t nsides[] = {16, 32, 64, 128, 256, 512, 1024, 2048, 4096,
                         8192, 16384, 32768, 65536, 131072, 262144,
                         524288, 1048576, 2097152, 4194304};
    for (uint32_t nside : nsides) {
        hiss::HissTileGeometry g = hiss::make_tile_geometry(nside);
        uint64_t wrong = (uint64_t)g.tile_nside * g.tile_nside * 12;
        char msg[128];
        std::snprintf(msg, sizeof(msg),
                      "nside=%u: n_leaf=%u != tile_nside^2*12=%llu",
                      nside, g.n_leaf_per_tile, (unsigned long long)wrong);
        ASSERT_TRUE(g.n_leaf_per_tile != wrong, msg);
    }
}

// ============================================================================
// 测试 8: NESTED 父子关系位运算正确性
// 验证 global = (parent << 2d) | local 的具体位模式
// ============================================================================
static void test_nested_bit_pattern() {
    fprintf(stdout, "[TEST] NESTED 父子位运算: global = (parent << 2d) | local\n");

    // NSIDE=64, depth=2, tile_nside=16
    // parent=3, local=5 → global = (3 << 4) | 5 = 48 + 5 = 53
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry_for_parent(64, 3);
        uint64_t global = g.local_to_global(5);
        ASSERT_EQ_INT(global, 53, "NSIDE=64 parent=3 local=5 → global=53");
    }
    // NSIDE=4096, depth=8, tile_nside=16
    // parent=7, local=100 → global = (7 << 16) | 100 = 458752 + 100 = 458852
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry_for_parent(4096, 7);
        uint64_t global = g.local_to_global(100);
        ASSERT_EQ_INT(global, 458852, "NSIDE=4096 parent=7 local=100 → global=458852");
    }
    // NSIDE=32768, depth=9, tile_nside=64
    // parent=12345, local=1 → global = (12345 << 18) | 1 = 3230216192 + 1 = 3230216193
    {
        hiss::HissTileGeometry g = hiss::make_tile_geometry_for_parent(32768, 12345);
        uint64_t global = g.local_to_global(1);
        uint64_t expected = ((uint64_t)12345 << 18) | 1ULL;
        ASSERT_EQ_INT(global, (long)expected,
                      "NSIDE=32768 parent=12345 local=1 → global=(12345<<18)|1");
    }
}

// ============================================================================
// 主入口: 运行所有测试, 返回 0=全部通过, 非 0=有失败
// ============================================================================
int main() {
    fprintf(stdout, "============================================================\n");
    fprintf(stdout, "WP-A Tile 模型与 signal/support 语义单元测试\n");
    fprintf(stdout, "============================================================\n");

    test_nside64_tile16();
    test_nside32768_full_tile();
    test_nside4096_tile16();
    test_local_global_roundtrip();
    test_signal_is_cumulative_flux();
    test_support_is_area_ratio();
    test_not_old_wrong_formula();
    test_nested_bit_pattern();

    fprintf(stdout, "------------------------------------------------------------\n");
    fprintf(stdout, "总计: PASS=%d, FAIL=%d\n", g_pass_count, g_fail_count);
    fprintf(stdout, "------------------------------------------------------------\n");

    if (g_fail_count == 0) {
        fprintf(stdout, "[RESULT] 全部通过 ✓\n");
        return 0;
    } else {
        fprintf(stderr, "[RESULT] 有失败用例 ✗\n");
        return 1;
    }
}
