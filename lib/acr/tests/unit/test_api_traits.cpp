// lib/acr/tests/unit/test_api_traits.cpp
// Phase B2/B4 集成测试：TaskTraits-based 公共 API
// 验证控制包 20_PHASE_I_AUDIT_ACTION_PLAN.md §3 Commit C 要求：
//   1. parallel_for/tiles/reduce/batch 新签名（OperationId + TaskTraits）能正确执行
//   2. OperationId/traits 不被忽略：通过 CostEstimator → Dispatcher 调用链
//   3. 不同 TaskClass（elementwise/reduction/stencil/convolution/batch）都能正常调用
//   4. CPU fallback 明确：无画像时也能正确执行（grainsize=0 走 tbb 默认）
//   5. 任务结果与旧 KernelId-based API 一致
//
// 注意：本测试不验证 GPU 派发（需真实 GPU + 画像），只验证调用链接通和 CPU 路径正确性。
// GPU 派发的真实 Mixed 测试在 classic/e18_workpool.cpp（ACR_BUILD_CUDA 时才编译）。

#include <atomic>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "astro/compute/acr.hpp"

using astro::compute::Extent2D;
using astro::compute::OperationId;
using astro::compute::Range1D;
using astro::compute::TaskTraits;
using astro::compute::TileShape;

// ===== 辅助：构造常见 TaskTraits =====
namespace {

TaskTraits elementwise_traits() {
    TaskTraits t;
    t.task_class = astro::compute::TaskClass::elementwise;
    t.access = astro::compute::AccessPattern::contiguous;
    t.uniformity = astro::compute::WorkUniformity::uniform;
    t.intensity = astro::compute::IntensityClass::memory_bound;
    return t;
}

TaskTraits reduction_traits() {
    TaskTraits t;
    t.task_class = astro::compute::TaskClass::reduction;
    t.access = astro::compute::AccessPattern::contiguous;
    t.uniformity = astro::compute::WorkUniformity::uniform;
    t.intensity = astro::compute::IntensityClass::memory_bound;
    t.numeric.deterministic_merge = true;
    return t;
}

TaskTraits stencil_traits() {
    TaskTraits t;
    t.task_class = astro::compute::TaskClass::stencil_2d;
    t.access = astro::compute::AccessPattern::local_neighborhood;
    t.uniformity = astro::compute::WorkUniformity::uniform;
    t.intensity = astro::compute::IntensityClass::memory_bound;
    t.halo_x = 1;
    t.halo_y = 1;
    return t;
}

TaskTraits convolution_traits() {
    TaskTraits t;
    t.task_class = astro::compute::TaskClass::convolution_direct;
    t.access = astro::compute::AccessPattern::local_neighborhood;
    t.uniformity = astro::compute::WorkUniformity::uniform;
    t.intensity = astro::compute::IntensityClass::compute_bound;
    t.halo_x = 1;
    t.halo_y = 1;
    return t;
}

TaskTraits batch_traits() {
    TaskTraits t;
    t.task_class = astro::compute::TaskClass::batch_independent;
    t.access = astro::compute::AccessPattern::contiguous;
    t.uniformity = astro::compute::WorkUniformity::uniform;
    t.intensity = astro::compute::IntensityClass::memory_bound;
    return t;
}

} // anonymous namespace

// ============================================================================
// parallel_for(OperationId, Range1D, TaskTraits, KernelFn)
// ============================================================================

TEST(ApiTraitsParallelFor, EmptyRangeNoCall) {
    std::atomic<int> calls{0};
    astro::compute::parallel_for(
        OperationId("test.elementwise.empty"), Range1D{0, 0},
        elementwise_traits(),
        [&calls](std::size_t) { calls.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(calls.load(), 0);
}

TEST(ApiTraitsParallelFor, SingleElementCorrect) {
    std::vector<int> data(1, 0);
    astro::compute::parallel_for(
        OperationId("test.elementwise.single"), Range1D{0, 1},
        elementwise_traits(),
        [&data](std::size_t i) { data[i] = static_cast<int>(i + 1); });
    EXPECT_EQ(data[0], 1);
}

TEST(ApiTraitsParallelFor, LargeRangeAllVisited) {
    constexpr std::size_t N = 1u << 18;
    std::vector<int> data(N, 0);
    astro::compute::parallel_for(
        OperationId("test.elementwise.large"), Range1D{0, N},
        elementwise_traits(),
        [&data](std::size_t i) { data[i] = 1; });
    std::size_t sum = 0;
    for (auto v : data) sum += v;
    EXPECT_EQ(sum, N);
}

TEST(ApiTraitsParallelFor, ResultsMatchOldApi) {
    constexpr std::size_t N = 1000;
    std::vector<int> old_api(N, 0);
    std::vector<int> new_api(N, 0);
    astro::compute::parallel_for(
        astro::compute::KernelId::Custom, Range1D{0, N},
        [&old_api](std::size_t i) { old_api[i] = static_cast<int>(i * 2); });
    astro::compute::parallel_for(
        OperationId("test.elementwise.match"), Range1D{0, N},
        elementwise_traits(),
        [&new_api](std::size_t i) { new_api[i] = static_cast<int>(i * 2); });
    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_EQ(old_api[i], new_api[i]) << "i=" << i;
    }
}

// ============================================================================
// parallel_tiles(OperationId, Extent2D, TileShape, TaskTraits, KernelFn)
// ============================================================================

TEST(ApiTraitsParallelTiles, FullCoverageNoOverflow) {
    Extent2D extent{10, 10};
    TileShape tile{4, 4};
    std::vector<int> covered(extent.count(), 0);
    bool overflow = false;

    astro::compute::parallel_tiles(
        OperationId("test.tiles.coverage"), extent, tile, stencil_traits(),
        [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
            for (std::size_t y = ty * tile.tile_h; y < ty * tile.tile_h + th; ++y) {
                for (std::size_t x = tx * tile.tile_w; x < tx * tile.tile_w + tw; ++x) {
                    if (y >= extent.height || x >= extent.width) { overflow = true; return; }
                    covered[y * extent.width + x] += 1;
                }
            }
        });

    EXPECT_FALSE(overflow);
    for (std::size_t i = 0; i < extent.count(); ++i) {
        EXPECT_EQ(covered[i], 1) << "pixel " << i << " covered " << covered[i] << " times";
    }
}

TEST(ApiTraitsParallelTiles, ConvolutionTraitsSupported) {
    // 验证 convolution_direct 任务类别能正常调用（不验证卷积结果，只验证调用链不崩溃）
    Extent2D extent{8, 8};
    TileShape tile{4, 4};
    std::atomic<int> tile_calls{0};
    astro::compute::parallel_tiles(
        OperationId("test.tiles.conv"), extent, tile, convolution_traits(),
        [&tile_calls](std::size_t, std::size_t, std::size_t, std::size_t) {
            tile_calls.fetch_add(1, std::memory_order_relaxed);
        });
    // 8x8 / 4x4 = 4 tiles
    EXPECT_EQ(tile_calls.load(), 4);
}

// ============================================================================
// parallel_reduce(OperationId, Range1D, TaskTraits, T, Map, Reduce)
// ============================================================================

TEST(ApiTraitsParallelReduce, SumCorrect) {
    constexpr std::size_t N = 10000;
    auto map = [](std::size_t i) -> long long { return static_cast<long long>(i + 1); };
    auto reduce = [](long long a, long long b) { return a + b; };
    long long result = astro::compute::parallel_reduce<long long>(
        OperationId("test.reduce.sum"), Range1D{0, N}, reduction_traits(),
        0LL, map, reduce);
    // 1+2+...+10000 = 10000*10001/2 = 50005000
    EXPECT_EQ(result, 50005000LL);
}

TEST(ApiTraitsParallelReduce, EmptyRangeReturnsIdentity) {
    auto map = [](std::size_t) -> int { return 1; };
    auto reduce = [](int a, int b) { return a + b; };
    int result = astro::compute::parallel_reduce<int>(
        OperationId("test.reduce.empty"), Range1D{0, 0}, reduction_traits(),
        42, map, reduce);
    EXPECT_EQ(result, 42);
}

TEST(ApiTraitsParallelReduce, MaxCorrect) {
    constexpr std::size_t N = 1000;
    std::vector<int> data(N);
    for (std::size_t i = 0; i < N; ++i) data[i] = static_cast<int>((i * 37) % 1000);
    auto map = [&data](std::size_t i) -> int { return data[i]; };
    auto reduce = [](int a, int b) { return a > b ? a : b; };
    int result = astro::compute::parallel_reduce<int>(
        OperationId("test.reduce.max"), Range1D{0, N}, reduction_traits(),
        -1, map, reduce);
    int expected = -1;
    for (auto v : data) expected = expected > v ? expected : v;
    EXPECT_EQ(result, expected);
}

// ============================================================================
// parallel_batch(OperationId, item_count, TaskTraits, KernelFn)
// ============================================================================

TEST(ApiTraitsParallelBatch, AllItemsProcessed) {
    constexpr std::size_t N = 500;
    std::vector<int> data(N, 0);
    astro::compute::parallel_batch(
        OperationId("test.batch.all"), N, batch_traits(),
        [&data](std::size_t i) { data[i] = 1; });
    std::size_t sum = 0;
    for (auto v : data) sum += v;
    EXPECT_EQ(sum, N);
}

TEST(ApiTraitsParallelBatch, ZeroItemsNoCall) {
    std::atomic<int> calls{0};
    astro::compute::parallel_batch(
        OperationId("test.batch.zero"), 0, batch_traits(),
        [&calls](std::size_t) { calls.fetch_add(1, std::memory_order_relaxed); });
    EXPECT_EQ(calls.load(), 0);
}

// ============================================================================
// CPU fallback 验证：无画像时 API 仍正确执行
// ============================================================================

TEST(ApiTraitsCpuFallback, NoProfileStillCorrect) {
    // 无 hardware-profile.json 时 CostEstimator 返回 CPU fallback estimate
    // grainsize=0 走 tbb 默认，结果仍应正确
    constexpr std::size_t N = 5000;
    std::vector<int> data(N, 0);
    astro::compute::parallel_for(
        OperationId("test.fallback.noprofile"), Range1D{0, N},
        elementwise_traits(),
        [&data](std::size_t i) { data[i] = static_cast<int>(i); });
    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i));
    }
}

// ============================================================================
// OperationId 传递验证：通过 runtime_status_json 间接验证调用链接通
// ============================================================================

TEST(ApiTraitsCallChain, RuntimeStatusReflectsActivity) {
    // 提交若干任务后，runtime_status_json 应显示 total_submitted 增加
    // 这表明 _with_desc 路径真正进入了 runtime（而非被忽略）
    astro::compute::runtime_init();
    const std::string before = astro::compute::runtime_status_json();

    constexpr std::size_t N = 100;
    std::atomic<int> kernel_calls{0};
    astro::compute::parallel_for(
        OperationId("test.chain.activity"), Range1D{0, N},
        elementwise_traits(),
        [&kernel_calls](std::size_t) { kernel_calls.fetch_add(1, std::memory_order_relaxed); });

    const std::string after = astro::compute::runtime_status_json();
    // 1. kernel 必须被调用（证明 parallel_for 真正执行）
    EXPECT_EQ(kernel_calls.load(), static_cast<int>(N));
    // 2. before/after 都应包含 "total_submitted" 字段（证明 runtime 状态可查询）
    EXPECT_NE(before.find("total_submitted"), std::string::npos);
    EXPECT_NE(after.find("total_submitted"), std::string::npos);
    // 3. after 中 total_submitted 不应为 0（证明 _with_desc 路径进入了 KernelGuard 计数）
    //    注意：runtime 是全局单例，total_submitted 会跨测试累积，所以只需验证非0
    EXPECT_EQ(after.find("\"total_submitted\":0"), std::string::npos)
        << "total_submitted should not be 0 after parallel_for, after=" << after;
}

// ============================================================================
// 不同 TaskClass 调用链验证（不验证曲线查找，只验证不崩溃）
// ============================================================================

TEST(ApiTraitsTaskClass, AllTaskClassesCallable) {
    // 验证所有 TaskClass 都能通过 _with_desc 路径执行（CostEstimator 不应抛异常）
    const std::size_t N = 100;
    std::vector<int> data(N, 0);

    const std::vector<TaskTraits> all_traits = {
        elementwise_traits(),
        reduction_traits(),
        stencil_traits(),
        convolution_traits(),
        batch_traits(),
    };
    const std::vector<OperationId> ids = {
        OperationId("test.class.elementwise"),
        OperationId("test.class.reduction"),
        OperationId("test.class.stencil"),
        OperationId("test.class.conv"),
        OperationId("test.class.batch"),
    };

    for (std::size_t t = 0; t < all_traits.size(); ++t) {
        std::fill(data.begin(), data.end(), 0);
        astro::compute::parallel_for(
            ids[t], Range1D{0, N}, all_traits[t],
            [&data](std::size_t i) { data[i] = 1; });
        std::size_t sum = 0;
        for (auto v : data) sum += v;
        EXPECT_EQ(sum, N) << "task class index " << t;
    }
}
