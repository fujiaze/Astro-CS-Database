// lib/acr/tests/unit/test_api.cpp
// ACR 公共 API 单元测试（GoogleTest）
// 覆盖 12_TEST_VALIDATION_MATRIX.md §2 的公共 API 单测项：
// parallel_for / for_2d / tiles / reduce / batch / chunks / run_for
// + Buffer/BufferView + Event + StatusCode/AcrError
//
// 运行时说明：Phase B 为同步执行，parallel_for 等返回的 Event 立即 ready；
// 异常 kernel 被 runtime 捕获后 mark_failed(KernelFailed)，不崩溃。

#include <atomic>
#include <cstddef>
#include <functional>
#include <numeric>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "astro/compute/acr.hpp"

using astro::compute::AcrError;
using astro::compute::Buffer;
using astro::compute::BufferView;
using astro::compute::Event;
using astro::compute::Extent2D;
using astro::compute::KernelId;
using astro::compute::Range1D;
using astro::compute::StatusCode;
using astro::compute::TileShape;

// ============================================================================
// parallel_for
// ============================================================================

// 空范围 parallel_for 不调用 kernel（用计数器验证）
TEST(ApiParallelFor, EmptyRangeDoesNotCallKernel) {
    std::atomic<int> call_count{0};
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 0},
        [&call_count](std::size_t) {
            call_count.fetch_add(1, std::memory_order_relaxed);
        });
    EXPECT_EQ(call_count.load(), 0);
    // 空 Event（impl_ 为 null）也视为 ready
    EXPECT_TRUE(ev.ready());
}

// 单元素范围 parallel_for 调用 1 次
TEST(ApiParallelFor, SingleElementCallsKernelOnce) {
    std::atomic<int> call_count{0};
    astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1},
        [&call_count](std::size_t) {
            call_count.fetch_add(1, std::memory_order_relaxed);
        });
    EXPECT_EQ(call_count.load(), 1);
}

// 极小范围（1, 2, 5 元素）正确
TEST(ApiParallelFor, TinyRangesCorrect) {
    for (std::size_t N : {1u, 2u, 5u}) {
        std::vector<int> data(N, 0);
        astro::compute::parallel_for(
            KernelId::Custom, Range1D{0, N},
            [&data](std::size_t i) { data[i] = static_cast<int>(i + 1); });
        for (std::size_t i = 0; i < N; ++i) {
            EXPECT_EQ(data[i], static_cast<int>(i + 1)) << "N=" << N << " i=" << i;
        }
    }
}

// 极大范围（1<<20）结果正确
TEST(ApiParallelFor, LargeRangeCorrect) {
    constexpr std::size_t N = 1u << 20;
    std::vector<float> data(N, 0.0f);
    astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, N},
        [&data](std::size_t i) { data[i] = 1.0f; });
    double sum = 0.0;
    for (std::size_t i = 0; i < N; ++i) sum += data[i];
    EXPECT_DOUBLE_EQ(sum, static_cast<double>(N));
}

// ============================================================================
// parallel_tiles
// ============================================================================

// 非整 Tile（10x10 extent, 4x4 tile）覆盖所有像素且不越界
// 10x10 / 4x4 → tiles_x=3, tiles_y=3, 边界 tile 宽高被 clamp 为 2
TEST(ApiTiles, NonIntegralTileCoverageNoOverflow) {
    Extent2D extent{10, 10};
    TileShape tile{4, 4};
    std::vector<int> covered(extent.count(), 0);
    bool out_of_bounds = false;

    astro::compute::parallel_tiles(
        KernelId::Custom, extent, tile,
        [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
            // tile 左上角 (tx*tile_w, ty*tile_h)，实际宽高 tw/th
            for (std::size_t y = ty * tile.tile_h; y < ty * tile.tile_h + th; ++y) {
                for (std::size_t x = tx * tile.tile_w; x < tx * tile.tile_w + tw; ++x) {
                    if (x >= extent.width || y >= extent.height) {
                        out_of_bounds = true;
                        continue;
                    }
                    covered[y * extent.width + x] = 1;
                }
            }
        });

    EXPECT_FALSE(out_of_bounds) << "tile 越界";
    int total = std::accumulate(covered.begin(), covered.end(), 0);
    EXPECT_EQ(total, 100) << "应覆盖全部 100 像素";
}

// parallel_tiles 覆盖（整除情况 8x8 / 4x4 = 4 tiles）
TEST(ApiTiles, IntegralTileCoverage) {
    Extent2D extent{8, 8};
    TileShape tile{4, 4};
    std::vector<int> covered(extent.count(), 0);

    astro::compute::parallel_tiles(
        KernelId::Custom, extent, tile,
        [&](std::size_t tx, std::size_t ty, std::size_t tw, std::size_t th) {
            for (std::size_t y = ty * tile.tile_h; y < ty * tile.tile_h + th; ++y) {
                for (std::size_t x = tx * tile.tile_w; x < tx * tile.tile_w + tw; ++x) {
                    covered[y * extent.width + x] = 1;
                }
            }
        });

    int total = std::accumulate(covered.begin(), covered.end(), 0);
    EXPECT_EQ(total, 64);
}

// ============================================================================
// parallel_for_2d
// ============================================================================

// parallel_for_2d 覆盖所有像素（10x10）
TEST(ApiFor2d, CoverAllPixels) {
    Extent2D extent{10, 10};
    std::vector<int> covered(extent.count(), 0);

    astro::compute::parallel_for_2d(
        KernelId::Custom, extent,
        [&](std::size_t x, std::size_t y) {
            covered[y * extent.width + x] = 1;
        });

    int total = std::accumulate(covered.begin(), covered.end(), 0);
    EXPECT_EQ(total, 100);
}

// ============================================================================
// parallel_reduce
// ============================================================================

// parallel_reduce 正确性：sum [0,100) = 4950
TEST(ApiReduce, SumZeroToHundred) {
    int sum = astro::compute::parallel_reduce<int>(
        KernelId::Custom, Range1D{0, 100}, 0,
        [](std::size_t i) { return static_cast<int>(i); },
        std::plus<int>{});
    EXPECT_EQ(sum, 4950);
}

// parallel_reduce identity 正确（空范围返回 identity）
TEST(ApiReduce, IdentityOnEmptyRange) {
    int sum = astro::compute::parallel_reduce<int>(
        KernelId::Custom, Range1D{0, 0}, 42,
        [](std::size_t i) { return static_cast<int>(i); },
        std::plus<int>{});
    EXPECT_EQ(sum, 42);
}

// 别名检测：parallel_reduce 输入输出不别名（声明性，Phase B 不强制）
TEST(ApiReduce, NoAliasDeclaration) {
    // 声明性契约：parallel_reduce 的 map_fn 读取源数据，结果写入独立 result，
    // 二者不应别名。Phase B 实现未强制运行时检查，本用例标记 skipped，
    // 待 Phase H 强化后再转为强制断言。
    GTEST_SKIP() << "Phase B: 别名检测为声明性契约，暂不强制（待 Phase H 强化）";
}

// ============================================================================
// parallel_batch
// ============================================================================

// parallel_batch 计数
TEST(ApiBatch, CountAllItems) {
    constexpr std::size_t N = 100;
    std::atomic<int> count{0};
    astro::compute::parallel_batch(
        KernelId::Custom, N,
        [&count](std::size_t) {
            count.fetch_add(1, std::memory_order_relaxed);
        });
    EXPECT_EQ(count.load(), static_cast<int>(N));
}

// ============================================================================
// parallel_chunks
// ============================================================================

// parallel_chunks 覆盖
TEST(ApiChunks, CoverAllRanges) {
    constexpr std::size_t N = 1000;
    constexpr std::size_t chunk_size = 100;
    std::vector<int> data(N, 0);

    astro::compute::parallel_chunks(
        KernelId::Custom, Range1D{0, N}, chunk_size,
        [&](std::size_t b, std::size_t e) {
            for (std::size_t i = b; i < e; ++i) data[i] = 1;
        });

    int total = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(total, static_cast<int>(N));
}

// ============================================================================
// run_for
// ============================================================================

// run_for 串行正确性
TEST(ApiRunFor, SerialCorrectness) {
    constexpr std::size_t N = 1000;
    std::vector<int> data(N, 0);
    astro::compute::run_for(
        KernelId::Custom, Range1D{0, N},
        [&data](std::size_t i) { data[i] = static_cast<int>(i); });
    for (std::size_t i = 0; i < N; ++i) {
        EXPECT_EQ(data[i], static_cast<int>(i)) << "i=" << i;
    }
}

// ============================================================================
// Event
// ============================================================================

// 同步 Event：parallel_for 返回的 Event 立即 ready
TEST(ApiEvent, ImmediatelyReadyAfterParallelFor) {
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1000},
        [](std::size_t) {});
    EXPECT_TRUE(ev.ready());
}

// Event::wait 不阻塞（已完成）
TEST(ApiEvent, WaitDoesNotBlockWhenDone) {
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1000},
        [](std::size_t) {});
    ev.wait();  // 已完成，应立即返回
    EXPECT_TRUE(ev.ready());
}

// Event::cancel 后 cancelled==true
TEST(ApiEvent, CancelSetsCancelledFlag) {
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1000},
        [](std::size_t) {});
    ev.wait();      // 确保 kernel 已完成
    ev.cancel();    // 对已完成 Event 置 cancelled 标志
    EXPECT_TRUE(ev.cancelled());
}

// Event::status 返回 Ok（正常完成）
TEST(ApiEvent, StatusOkAfterSuccess) {
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 1000},
        [](std::size_t) {});
    EXPECT_EQ(ev.status(), StatusCode::Ok);
}

// 异常 kernel：parallel_for 内 kernel 抛异常 → Event::status 返回 KernelFailed，不崩溃
TEST(ApiEvent, ExceptionKernelReturnsFailedStatus) {
    Event ev = astro::compute::parallel_for(
        KernelId::Custom, Range1D{0, 100},
        [](std::size_t) { throw std::runtime_error("boom"); });
    EXPECT_EQ(ev.status(), StatusCode::KernelFailed);
    EXPECT_TRUE(ev.ready());
}

// ============================================================================
// Buffer / BufferView（API 集成层面）
// ============================================================================

// BufferView shape/stride/subview 正确
TEST(ApiBufferView, ShapeStrideSubview) {
    std::vector<float> data(100);
    BufferView<float> v(data.data(), 100, 2, 10);
    EXPECT_EQ(v.count(), 100u);
    EXPECT_EQ(v.stride(), 2u);
    EXPECT_EQ(v.pitch(), 10u);

    BufferView<float> sub = v.subview(10, 20);
    EXPECT_EQ(sub.data(), data.data() + 10);
    EXPECT_EQ(sub.count(), 20u);
    EXPECT_EQ(sub.stride(), 2u);   // subview 继承 stride
    EXPECT_EQ(sub.pitch(), 10u);   // subview 继承 pitch
}

// BufferView subview 越界抛 AcrError(OutOfBounds)
TEST(ApiBufferView, SubviewOutOfBoundsThrows) {
    std::vector<float> data(10);
    BufferView<float> v(data.data(), 10);
    EXPECT_THROW(v.subview(8, 5), AcrError);
    try {
        v.subview(8, 5);
    } catch (const AcrError& e) {
        EXPECT_EQ(e.code(), StatusCode::OutOfBounds);
    }
}

// Buffer 只读/读写访问
TEST(ApiBuffer, ReadOnlyWriteAccess) {
    Buffer<float> buf(10, 1.0f);
    // 读写访问
    buf[0] = 5.0f;
    EXPECT_FLOAT_EQ(buf[0], 5.0f);
    EXPECT_FLOAT_EQ(buf[1], 1.0f);
    // data 返回可写指针
    float* p = buf.data();
    p[2] = 3.0f;
    EXPECT_FLOAT_EQ(buf[2], 3.0f);
    // const Buffer 只读访问
    const Buffer<float>& cb = buf;
    const float* cp = cb.data();
    EXPECT_FLOAT_EQ(cp[0], 5.0f);
}

// Buffer resize
TEST(ApiBuffer, ResizeGrowAndShrink) {
    Buffer<int> buf(5, 1);
    EXPECT_EQ(buf.count(), 5u);
    buf.resize(10);  // 扩大，值初始化为 0
    EXPECT_EQ(buf.count(), 10u);
    for (std::size_t i = 0; i < 10; ++i) EXPECT_EQ(buf[i], 0) << "i=" << i;
    buf.resize(3);   // 缩小
    EXPECT_EQ(buf.count(), 3u);
}

// host Buffer 生命周期（unique_ptr 释放）
TEST(ApiBuffer, HostLifetimeUniquePtr) {
    Buffer<int>* buf = new Buffer<int>(100, 7);
    EXPECT_NE(buf->data(), nullptr);
    delete buf;  // 析构释放内存，不应崩溃
    SUCCEED();
}

// ============================================================================
// main：初始化 runtime → 运行所有测试 → 关闭 runtime
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    astro::compute::runtime_init();
    int result = RUN_ALL_TESTS();
    astro::compute::runtime_shutdown();
    return result;
}
