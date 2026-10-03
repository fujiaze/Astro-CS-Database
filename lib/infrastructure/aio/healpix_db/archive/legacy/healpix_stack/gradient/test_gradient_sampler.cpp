// test_gradient_sampler.cpp - gradient_sampler 编译验证 + 基本逻辑测试
//
// 验证:
//   1. 结构体大小 (SampleRow = 36 字节；与 gradient_sampler.h:41 的
//      static_assert(sizeof(SampleRow) == 36) 同源)
//   2. 空输入处理（sample 必回非 0、零行、零样本数）
//   3. 无效 gaia 目录 + 不存在的 hiss 路径（回 2 且 lastError 非空）
//
// 判据纪律（本文件的整改要点，勿回退）:
//   判据一律走 CHECK_NOW / CHECK_EQ 这类**显式计数**宏，**不得**改回裸 assert()。
//   原因: assert() 是 NDEBUG 条件编译宏，发布构建（仓库默认 CMAKE_BUILD_TYPE=Release，
//   见根 CMakeLists.txt:30-31 与 CMakePresets.json:75 ⇒ -DNDEBUG）下整条判据被预处理器
//   消掉，而通过文案照常打印 ⇒ 判据全部消失、文案仍称全过。显式计数 + 非 0 退出码
//   不依赖任何编译期开关，发布构建下判据照样执行、失败照样让门变红。
//   同时: [PASS] 文案只在**该项自己**通过时打印，总文案只在 failures == 0 时打印。

#include "gradient_sampler.h"
#include "snr_evaluator.h"

#include <cstdio>

using namespace gradient;

// ── 显式判据计数（不依赖 NDEBUG / 任何编译期开关） ─────────────────────────
static int g_failures = 0;
static int g_checks = 0;

static void report(int ok, const char* what, const char* detail) {
    ++g_checks;
    if (ok) return;
    ++g_failures;
    std::fprintf(stderr, "FAIL %s: %s%s%s\n", __FILE__, what,
                 detail ? " — " : "", detail ? detail : "");
}

// cond 为假 ⇒ 记一次失败；detail 用于把实际值印出来（可空）
#define CHECK_NOW(cond, detail) report((cond) ? 1 : 0, #cond, (detail))
#define CHECK_EQ(actual, expected, label)                                     \
    do {                                                                      \
        const long long a_ = (long long)(actual);                            \
        const long long e_ = (long long)(expected);                          \
        char d_[128];                                                         \
        std::snprintf(d_, sizeof(d_), "%s: 期望 %lld 实得 %lld", (label), e_, a_); \
        report(a_ == e_ ? 1 : 0, #actual " == " #expected, d_);               \
    } while (0)

// 测试 1: 结构体大小（阈值 36 不变；头文件 static_assert 同一数值）
static void test_struct_size() {
    char d_[64];
    std::snprintf(d_, sizeof(d_), "SampleRow=%zu 字节（期望 36）", sizeof(SampleRow));
    const int ok = (sizeof(SampleRow) == 36);
    report(ok, "sizeof(SampleRow) == 36", d_);
    if (ok) std::printf("[PASS] test_struct_size (SampleRow=%zu bytes)\n",
                        sizeof(SampleRow));
}

// 测试 2: 空输入（阈值不变：rc != 0 / rows 空 / total_samples == 0）
static void test_empty_input() {
    const int before = g_failures;   // 本项自己的 [PASS] 只看自己这几条判据
    GradientSampler sampler;
    SampleResult result;
    SamplerParams params;
    const int rc = sampler.sample(nullptr, 0, "", params, result);
    char d_[64];
    std::snprintf(d_, sizeof(d_), "rc=%d（期望 != 0）", rc);
    CHECK_NOW(rc != 0, d_);
    CHECK_NOW(result.rows.empty(), "空输入不得产出任何行");
    CHECK_EQ(result.total_samples, 0, "total_samples");
    if (g_failures == before) std::printf("[PASS] test_empty_input (rc=%d)\n", rc);
}

// 测试 3: 不存在的文件 + 无效 gaia 目录（阈值不变：rc == 2 / lastError 非空）
static void test_nonexistent_file() {
    const int before = g_failures;
    GradientSampler sampler;
    SampleResult result;
    SamplerParams params;
    FrameInfo frame;
    frame.hiss_path = "nonexistent.hiss";
    frame.frame_id = 0;
    // gaia_data_dir 为空 → gaia_client_create_ex 失败 → sample 返回 2
    const int rc = sampler.sample(&frame, 1, "", params, result);
    CHECK_EQ(rc, 2, "rc（gaia 创建失败）");
    CHECK_NOW(!sampler.lastError().empty(), "失败必须留 lastError（禁静默）");
    if (g_failures == before)
        std::printf("[PASS] test_nonexistent_file (rc=%d, gaia创建失败预期)\n", rc);
}

int main() {
    std::printf("=== test_gradient_sampler ===\n\n");
    const int before_struct = g_failures;
    test_struct_size();
    const int after_struct = g_failures;
    test_empty_input();
    test_nonexistent_file();

    std::printf("\n=== test_gradient_sampler: %d/%d 判据通过, %d 失败 ===\n",
                g_checks - g_failures, g_checks, g_failures);
    if (g_failures != 0) {
        std::fprintf(stderr,
                     "test_gradient_sampler FAILED (%d 项判据未通过; "
                     "struct_size 段 %s)\n", g_failures,
                     after_struct > before_struct ? "已红" : "绿");
        return 1;   // 非 0 退出码 ⇒ 门红；不得在判据未过时报全过
    }
    std::printf("=== test_gradient_sampler: ALL PASS ===\n");
    return 0;
}