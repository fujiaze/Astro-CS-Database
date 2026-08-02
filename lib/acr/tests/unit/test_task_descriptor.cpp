// lib/acr/tests/unit/test_task_descriptor.cpp — TaskDescriptor 单元测试
// Phase B3：覆盖 TaskDescriptor 构造、work_size 派生、Precision 工具。
#include <gtest/gtest.h>

#include "task_descriptor.hpp"

#include <cstddef>
#include <string>

using namespace astro::compute;

// ============================================================================
// make_range_descriptor
// ============================================================================

TEST(TaskDescriptor, MakeRangeDescriptorBasic) {
    TaskTraits traits;
    traits.task_class = TaskClass::elementwise;
    Range1D range{0, 1000};
    TaskDescriptor d = make_range_descriptor("op.axpy", range, traits, Precision::FP32);
    EXPECT_EQ(d.operation_id, "op.axpy");
    EXPECT_EQ(d.range.begin, 0u);
    EXPECT_EQ(d.range.end, 1000u);
    EXPECT_EQ(d.work_size(), 1000u);
    EXPECT_EQ(d.precision, Precision::FP32);
    EXPECT_FALSE(d.is_2d());
    EXPECT_FALSE(d.is_batch());
}

TEST(TaskDescriptor, MakeRangeDescriptorEmptyRange) {
    TaskTraits traits;
    Range1D range{100, 100};
    TaskDescriptor d = make_range_descriptor("op.empty", range, traits, Precision::Default);
    EXPECT_EQ(d.work_size(), 0u);
}

TEST(TaskDescriptor, MakeRangeDescriptorReversedRange) {
    TaskTraits traits;
    Range1D range{200, 100};
    TaskDescriptor d = make_range_descriptor("op.rev", range, traits, Precision::Default);
    EXPECT_EQ(d.work_size(), 0u);  // end <= begin → 0
}

// ============================================================================
// make_tiles_descriptor
// ============================================================================

TEST(TaskDescriptor, MakeTilesDescriptorBasic) {
    TaskTraits traits;
    traits.task_class = TaskClass::stencil_2d;
    Extent2D ext{1024, 768};
    TileShape tile{64, 64};
    TaskDescriptor d = make_tiles_descriptor("op.stencil", ext, tile, traits, Precision::FP32);
    EXPECT_EQ(d.operation_id, "op.stencil");
    EXPECT_EQ(d.extent.width, 1024u);
    EXPECT_EQ(d.extent.height, 768u);
    EXPECT_EQ(d.extent.count(), 1024u * 768u);
    EXPECT_EQ(d.work_size(), 1024u * 768u);
    EXPECT_TRUE(d.is_2d());
    EXPECT_FALSE(d.is_batch());
}

TEST(TaskDescriptor, MakeTilesDescriptorZeroExtent) {
    TaskTraits traits;
    Extent2D ext{0, 0};
    TileShape tile{64, 64};
    TaskDescriptor d = make_tiles_descriptor("op.zero", ext, tile, traits, Precision::Default);
    EXPECT_EQ(d.work_size(), 0u);
    EXPECT_FALSE(d.is_2d());  // count()==0
}

// ============================================================================
// make_batch_descriptor
// ============================================================================

TEST(TaskDescriptor, MakeBatchDescriptorBasic) {
    TaskTraits traits;
    traits.task_class = TaskClass::batch_independent;
    TaskDescriptor d = make_batch_descriptor("op.batch", 500, traits, Precision::Default);
    EXPECT_EQ(d.operation_id, "op.batch");
    EXPECT_EQ(d.item_count, 500u);
    EXPECT_EQ(d.work_size(), 500u);
    EXPECT_TRUE(d.is_batch());
    EXPECT_FALSE(d.is_2d());
}

TEST(TaskDescriptor, MakeBatchDescriptorZeroItems) {
    TaskTraits traits;
    TaskDescriptor d = make_batch_descriptor("op.empty", 0, traits, Precision::Default);
    EXPECT_EQ(d.work_size(), 0u);
    EXPECT_FALSE(d.is_batch());  // item_count==0 → false
}

// ============================================================================
// make_reduce_descriptor
// ============================================================================

TEST(TaskDescriptor, MakeReduceDescriptorBasic) {
    TaskTraits traits;
    traits.task_class = TaskClass::reduction;
    Range1D range{0, 4096};
    TaskDescriptor d = make_reduce_descriptor("op.reduce", range, traits, Precision::FP64);
    EXPECT_EQ(d.operation_id, "op.reduce");
    EXPECT_EQ(d.range.begin, 0u);
    EXPECT_EQ(d.range.end, 4096u);
    EXPECT_EQ(d.work_size(), 4096u);
    EXPECT_EQ(d.precision, Precision::FP64);
}

// ============================================================================
// work_size 优先级
// ============================================================================

TEST(TaskDescriptor, WorkSizePriorityRangeFirst) {
    TaskDescriptor d;
    d.range = {0, 100};
    d.extent = {10, 10};
    d.item_count = 50;
    // range 优先
    EXPECT_EQ(d.work_size(), 100u);
}

TEST(TaskDescriptor, WorkSizeExtentSecond) {
    TaskDescriptor d;
    d.range = {0, 0};
    d.extent = {10, 10};
    d.item_count = 50;
    // range.size()==0 → 用 extent
    EXPECT_EQ(d.work_size(), 100u);
}

TEST(TaskDescriptor, WorkSizeItemCountLast) {
    TaskDescriptor d;
    d.range = {0, 0};
    d.extent = {0, 0};
    d.item_count = 50;
    // range 和 extent 都为 0 → 用 item_count
    EXPECT_EQ(d.work_size(), 50u);
}

// ============================================================================
// Precision 工具
// ============================================================================

TEST(PrecisionUtils, PrecisionStr) {
    EXPECT_STREQ(precision_str(Precision::Default), "default");
    EXPECT_STREQ(precision_str(Precision::FP32), "fp32");
    EXPECT_STREQ(precision_str(Precision::FP64), "fp64");
    EXPECT_STREQ(precision_str(Precision::Integer), "integer");
}

TEST(PrecisionUtils, NumericPolicyToPrecisionFp32) {
    NumericPolicy np;
    np.compute = NumericPolicy::Compute::fp32;
    EXPECT_EQ(numeric_policy_to_precision(np), Precision::FP32);
}

TEST(PrecisionUtils, NumericPolicyToPrecisionFp64) {
    NumericPolicy np;
    np.compute = NumericPolicy::Compute::fp64;
    EXPECT_EQ(numeric_policy_to_precision(np), Precision::FP64);
}

// ============================================================================
// TaskTraits 默认值校验
// ============================================================================

TEST(TaskTraitsValidation, DefaultTraitsValid) {
    TaskTraits t;
    EXPECT_TRUE(task_traits_valid(t));
}

TEST(TaskTraitsValidation, ActiveFractionOutOfRange) {
    TaskTraits t;
    t.active_fraction_hint = 1.5;
    EXPECT_FALSE(task_traits_valid(t));
    t.active_fraction_hint = -0.1;
    EXPECT_FALSE(task_traits_valid(t));
}

TEST(TaskTraitsValidation, ActiveFractionBoundary) {
    TaskTraits t;
    t.active_fraction_hint = 0.0;
    EXPECT_TRUE(task_traits_valid(t));
    t.active_fraction_hint = 1.0;
    EXPECT_TRUE(task_traits_valid(t));
}
