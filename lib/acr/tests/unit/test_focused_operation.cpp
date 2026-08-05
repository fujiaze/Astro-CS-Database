// lib/acr/tests/unit/test_focused_operation.cpp — 聚焦目标 Operation 测试
//
// 08 号计划 §3/§4 + 07 号规范：
//   - 5 个合成 Operation 的 CPU 正确性（对照标量参考）
//   - GPU 正确性（可用时，CPU vs GPU 数值容差）
//   - OperationProfile schema 校验通过
//   - quick 不生成生产 Profile（qualified=false）
#include <gtest/gtest.h>

#include "focused/focused_operations.hpp"
#include "focused/operation_profile.hpp"
#include "focused/focused_benchmark.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "astro/compute/acr.hpp"

using namespace astro::compute::qualification::focused;

namespace {

constexpr std::uint64_t kSeed = 0xA57C5AC20260802ULL;

// 构造确定性输入
std::vector<float> make_input(std::size_t n) {
    std::vector<float> x(n);
    fill_uniform_fp32(x.data(), n, kSeed);
    return x;
}

} // anonymous namespace

// ============================================================================
// 1. CPU 正确性（每个 Operation 对照标量参考）
// ============================================================================
TEST(FocusedOperation, CpuMatchesReference) {
    const std::size_t n = 1u << 17;  // 128K
    auto x = make_input(n);
    std::vector<float> y(n, 2.0f);
    std::vector<double> partials(256, 0.0);

    // dense fp32
    {
        std::vector<float> ref = y;
        reference_dense_accumulate(x, ref, /*fp64_acc=*/false);
        std::vector<float> got = y;
        std::vector<double> p(256, 0.0);
        run_cpu_operation(FocusedOp::DenseAccumulateFp32, x, got, p, 256);
        for (std::size_t i = 0; i < n; ++i) {
            EXPECT_FLOAT_EQ(got[i], ref[i]);
        }
    }
    // dense fp64acc
    {
        std::vector<float> ref = y;
        reference_dense_accumulate(x, ref, /*fp64_acc=*/true);
        std::vector<float> got = y;
        std::vector<double> p(256, 0.0);
        run_cpu_operation(FocusedOp::DenseAccumulateFp64Acc, x, got, p, 256);
        for (std::size_t i = 0; i < n; ++i) {
            EXPECT_FLOAT_EQ(got[i], ref[i]);
        }
    }
    // pixel reduce
    {
        std::vector<float> dummy_y(n, 0.0f);
        std::vector<double> p(256, 0.0);
        run_cpu_operation(FocusedOp::PixelReduceFp64Acc, x, dummy_y, p, 256);
        const double ref = reference_pixel_reduce(x);
        EXPECT_NEAR(p[0], ref, std::fabs(ref) * 1e-9 + 1e-9);
    }
    // drizzle scatter
    {
        std::vector<float> dummy_y(n, 0.0f);
        std::vector<double> p(256, 0.0);
        run_cpu_operation(FocusedOp::DrizzleScatterFp64Acc, x, dummy_y, p, 256);
        std::vector<double> ref;
        reference_drizzle_scatter(x, ref, 256, kSeed);
        for (std::size_t b = 0; b < 256; ++b) {
            EXPECT_NEAR(p[b], ref[b], std::fabs(ref[b]) * 1e-9 + 1e-9);
        }
    }
    // resident chain
    {
        std::vector<float> ref(n);
        reference_resident_chain(x, ref);
        std::vector<float> got(n, 0.0f);
        std::vector<double> p(256, 0.0);
        run_cpu_operation(FocusedOp::ResidentChain, x, got, p, 256);
        for (std::size_t i = 0; i < n; ++i) {
            EXPECT_FLOAT_EQ(got[i], ref[i]);
        }
    }
}

// ============================================================================
// 2. OperationProfile schema 校验通过（standard + quick）
// ============================================================================
TEST(FocusedOperation, ProfileSchemaValidAndQuickNotQualified) {
    astro::compute::runtime_init();
    FocusedBenchmark bench;
    ASSERT_GT(bench.run(FocusedProfileKind::Quick, /*enable_gpu=*/false), 0u);

    auto quick = bench.build_profile(FocusedProfileKind::Quick);
    bench.qualify(FocusedProfileKind::Quick, quick);
    std::string err;
    EXPECT_TRUE(validate_operation_profile(quick, err)) << err;
    EXPECT_EQ(quick.profile_state, "diagnostic");
    for (const auto& op : quick.operations) {
        EXPECT_FALSE(op.qualified);  // quick 不得生成生产 Profile
    }
    EXPECT_EQ(quick.operations.size(), 5u);
    astro::compute::runtime_shutdown();
}

// ============================================================================
// 3. 5 个目标 OperationId 均已注册（操作枚举/ID 一致性）
// ============================================================================
TEST(FocusedOperation, OperationIdsMatchConstants) {
    EXPECT_STREQ(focused_op_id(FocusedOp::DenseAccumulateFp32),
                 "synthetic.dense_pixel_accumulate.fp32");
    EXPECT_STREQ(focused_op_id(FocusedOp::DenseAccumulateFp64Acc),
                 "synthetic.dense_pixel_accumulate.fp64acc");
    EXPECT_STREQ(focused_op_id(FocusedOp::PixelReduceFp64Acc),
                 "synthetic.pixel_reduce.fp64acc");
    EXPECT_STREQ(focused_op_id(FocusedOp::DrizzleScatterFp64Acc),
                 "synthetic.drizzle_like_scatter.fp64acc");
    EXPECT_STREQ(focused_op_id(FocusedOp::ResidentChain),
                 "synthetic.resident_chain");
}

// ============================================================================
// 4. 序列化往返（write → read → 字段一致）
// ============================================================================
TEST(FocusedOperation, ProfileRoundTrip) {
    astro::compute::runtime_init();
    FocusedBenchmark bench;
    bench.run(FocusedProfileKind::Quick, /*enable_gpu=*/false);
    auto p = bench.build_profile(FocusedProfileKind::Quick);
    const std::string tmp = "_focused_roundtrip.json";  // cwd 相对（ctest 运行目录）
    ASSERT_TRUE(write_operation_profile_to_file(tmp, p));
    OperationProfile q;
    ASSERT_TRUE(read_operation_profile_from_file(tmp, q));
    EXPECT_EQ(q.schema_version, p.schema_version);
    EXPECT_EQ(q.profile_state, p.profile_state);
    EXPECT_EQ(q.operations.size(), p.operations.size());
    for (std::size_t i = 0; i < p.operations.size(); ++i) {
        EXPECT_EQ(q.operations[i].operation_id, p.operations[i].operation_id);
        EXPECT_NEAR(q.operations[i].cpu.ns_per_item,
                    p.operations[i].cpu.ns_per_item, 1e-6);
    }
    astro::compute::runtime_shutdown();
}
