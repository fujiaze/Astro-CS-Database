// lib/acr/tests/unit/test_buffer.cpp
// ACR Buffer / BufferView 专项单元测试（GoogleTest）
// 覆盖构造、访问、视图、subview、resize、生命周期等基础行为
//
// 说明：本文件只测 Buffer/BufferView 的纯数据结构与内存行为，
// 不依赖 runtime（不调用 runtime_init）。

#include <cstddef>
#include <vector>

#include <gtest/gtest.h>

#include "astro/compute/acr.hpp"

using astro::compute::AcrError;
using astro::compute::Buffer;
using astro::compute::BufferView;
using astro::compute::StatusCode;

// ============================================================================
// Buffer 构造
// ============================================================================

// Buffer 默认构造空
TEST(BufferTest, DefaultConstructEmpty) {
    Buffer<int> buf;
    EXPECT_EQ(buf.count(), 0u);
    EXPECT_TRUE(buf.empty());
    EXPECT_EQ(buf.data(), nullptr);
}

// Buffer(count) 分配（值初始化为 0）
TEST(BufferTest, AllocateWithCount) {
    Buffer<int> buf(50);
    EXPECT_EQ(buf.count(), 50u);
    EXPECT_FALSE(buf.empty());
    EXPECT_NE(buf.data(), nullptr);
    for (std::size_t i = 0; i < 50; ++i) EXPECT_EQ(buf[i], 0) << "i=" << i;
}

// Buffer(count, init) 初始化
TEST(BufferTest, AllocateWithInit) {
    Buffer<float> buf(20, 3.14f);
    EXPECT_EQ(buf.count(), 20u);
    for (std::size_t i = 0; i < 20; ++i) EXPECT_FLOAT_EQ(buf[i], 3.14f) << "i=" << i;
}

// ============================================================================
// BufferView 基础属性
// ============================================================================

// BufferView data/count/stride/pitch
TEST(BufferViewTest, DataCountStridePitch) {
    float arr[10];
    BufferView<float> v(arr, 10, 4, 8);
    EXPECT_EQ(v.data(), arr);
    EXPECT_EQ(v.count(), 10u);
    EXPECT_EQ(v.stride(), 4u);
    EXPECT_EQ(v.pitch(), 8u);
    EXPECT_FALSE(v.empty());
}

// BufferView 空构造（默认 stride=1, pitch=0）
TEST(BufferViewTest, DefaultConstructEmpty) {
    BufferView<float> v;
    EXPECT_EQ(v.data(), nullptr);
    EXPECT_EQ(v.count(), 0u);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.stride(), 1u);
    EXPECT_EQ(v.pitch(), 0u);
}

// ============================================================================
// BufferView subview
// ============================================================================

// BufferView subview 正常
TEST(BufferViewTest, SubviewNormal) {
    float arr[20];
    BufferView<float> v(arr, 20);
    BufferView<float> sub = v.subview(5, 10);
    EXPECT_EQ(sub.data(), arr + 5);
    EXPECT_EQ(sub.count(), 10u);
    // subview 继承原 view 的 stride/pitch
    EXPECT_EQ(sub.stride(), 1u);
    EXPECT_EQ(sub.pitch(), 0u);
}

// BufferView subview 越界抛 AcrError(OutOfBounds)
TEST(BufferViewTest, SubviewOutOfBoundsThrows) {
    float arr[10];
    BufferView<float> v(arr, 10);
    // 越界：offset + sub_count > count
    EXPECT_THROW(v.subview(8, 5), AcrError);
    EXPECT_THROW(v.subview(11, 0), AcrError);
    // 验证错误码
    try {
        v.subview(8, 5);
    } catch (const AcrError& e) {
        EXPECT_EQ(e.code(), StatusCode::OutOfBounds);
    }
    // 边界刚好不越界（offset+sub_count == count 合法）
    EXPECT_NO_THROW(v.subview(0, 10));
    EXPECT_NO_THROW(v.subview(10, 0));
}

// ============================================================================
// Buffer 访问
// ============================================================================

// Buffer operator[] 读写
TEST(BufferTest, IndexReadWrite) {
    Buffer<int> buf(5);
    buf[0] = 10;
    buf[4] = 40;
    EXPECT_EQ(buf[0], 10);
    EXPECT_EQ(buf[4], 40);
    EXPECT_EQ(buf[1], 0);  // 未赋值仍为 0（值初始化）
}

// ============================================================================
// Buffer resize
// ============================================================================

// Buffer resize 扩大/缩小
TEST(BufferTest, ResizeGrowAndShrink) {
    Buffer<int> buf(5, 1);
    EXPECT_EQ(buf.count(), 5u);

    buf.resize(10);  // 扩大，新内存值初始化为 0
    EXPECT_EQ(buf.count(), 10u);
    for (std::size_t i = 0; i < 10; ++i) EXPECT_EQ(buf[i], 0) << "i=" << i;

    buf.resize(3);   // 缩小
    EXPECT_EQ(buf.count(), 3u);
    buf[0] = 99;
    EXPECT_EQ(buf[0], 99);
}

// ============================================================================
// Buffer view()
// ============================================================================

// Buffer view() 返回正确视图（共享底层内存）
TEST(BufferTest, ViewReturnsCorrectView) {
    Buffer<int> buf(10, 5);
    BufferView<int> v = buf.view();
    EXPECT_EQ(v.count(), 10u);
    EXPECT_EQ(v.data(), buf.data());
    // view 共享底层内存，写 view 影响 buf
    v[0] = 99;
    EXPECT_EQ(buf[0], 99);
}

// const Buffer view() const 返回只读视图
TEST(BufferTest, ConstView) {
    Buffer<int> buf(10, 5);
    const Buffer<int>& cb = buf;
    BufferView<const int> v = cb.view();
    EXPECT_EQ(v.count(), 10u);
    EXPECT_EQ(v.data(), buf.data());
    EXPECT_EQ(v[0], 5);
    // const view 不可写（编译期保证，下面若取消注释应编译失败）
    // v[0] = 99;
}

// ============================================================================
// main（不依赖 runtime，直接运行测试）
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
