// lib/algorithms/coverage/tests/rejection_nonfinite_weights_test.cpp
//
// 依据（权威，只读）：
//   docs/science/REJECTION.md §4（:34）「support/weights 有限性在资格层校验，
//     非有限 ⇒ INVALID_INPUT hard fail」
//   docs/science/REJECTION.md §8（:87）「非有限 weights/support → INVALID_INPUT hard fail」
//   ENGINEERING_SPEC.md §5.1（边界、NaN/Inf、错误输入用例必备）
//   ENGINEERING_SPEC.md §8（每项检查有正例与负例，能红能绿）
//
// 本文件是 M4-C-06 的共址回归门：非有限 weights 必须在资格层被判不合格、
// 并在 kernel 入口触发 INVALID_INPUT（fail-closed），而不是被静默带入
// 加权方法核（RCR）继续计算。
#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "astro/phase2/rejection.h"

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

P2RejectionPlan make_rcr_plan() {
    P2RejectionPlan plan{};
    plan.method = P2_REJECT_RCR;
    plan.minimum_n = 3;
    plan.underdetermined_n = 2;
    // §5/chk: RCR 要求 normalization=NONE（p2_reject_stack_ex 配置门）
    plan.normalization = P2_NORMALIZE_NONE;
    plan.normalization_floor = 1e-12;
    plan.rcr.technique = 0;
    return plan;
}

}  // namespace

// 正例（能绿）：全有限 weights ⇒ 不得被判 INVALID_INPUT
TEST(Phase2RejectionNonfiniteWeights, FiniteWeightsStayValid) {
    std::vector<double> values{1.0, 1.01, 0.99, 1.02, 0.98};
    std::vector<double> weights{1.0, 1.0, 1.0, 1.0, 1.0};
    P2CandidateStack stack{};
    stack.values = values.data();
    stack.weights = weights.data();
    stack.count = static_cast<std::uint32_t>(values.size());
    stack.data_type = 1;

    P2RejectionPlan plan = make_rcr_plan();
    std::vector<std::uint8_t> reasons(values.size(), 0);
    P2RejectionDecision dec{};
    dec.reasons = reasons.data();

    ASSERT_EQ(p2_reject_stack_ex(&stack, &plan, &dec), 0);
    EXPECT_NE(dec.status, P2_STATUS_INVALID_INPUT)
        << "全有限 weights 被误判为 INVALID_INPUT";
}

// 负例（改前红）：NaN weight ⇒ INVALID_INPUT hard fail
TEST(Phase2RejectionNonfiniteWeights, NaNWeightIsInvalidInput) {
    std::vector<double> values{1.0, 1.01, 0.99, 1.02, 0.98};
    std::vector<double> weights{1.0, kNaN, 1.0, 1.0, 1.0};
    P2CandidateStack stack{};
    stack.values = values.data();
    stack.weights = weights.data();
    stack.count = static_cast<std::uint32_t>(values.size());
    stack.data_type = 1;

    P2RejectionPlan plan = make_rcr_plan();
    std::vector<std::uint8_t> reasons(values.size(), 0);
    P2RejectionDecision dec{};
    dec.reasons = reasons.data();

    ASSERT_EQ(p2_reject_stack_ex(&stack, &plan, &dec), 0);
    EXPECT_EQ(dec.status, P2_STATUS_INVALID_INPUT)
        << "NaN weight 未按 SCI REJECTION §4/§8 hard fail";
}

// 负例（改前红）：+Inf weight ⇒ INVALID_INPUT hard fail
TEST(Phase2RejectionNonfiniteWeights, InfWeightIsInvalidInput) {
    std::vector<double> values{1.0, 1.01, 0.99, 1.02, 0.98};
    std::vector<double> weights{1.0, 1.0, kInf, 1.0, 1.0};
    P2CandidateStack stack{};
    stack.values = values.data();
    stack.weights = weights.data();
    stack.count = static_cast<std::uint32_t>(values.size());
    stack.data_type = 1;

    P2RejectionPlan plan = make_rcr_plan();
    std::vector<std::uint8_t> reasons(values.size(), 0);
    P2RejectionDecision dec{};
    dec.reasons = reasons.data();

    ASSERT_EQ(p2_reject_stack_ex(&stack, &plan, &dec), 0);
    EXPECT_EQ(dec.status, P2_STATUS_INVALID_INPUT)
        << "+Inf weight 未按 SCI REJECTION §4/§8 hard fail";
}

// 资格层负例（改前红）：非有限 weight 必须判该样本不合格
TEST(Phase2RejectionNonfiniteWeights, EligibilityRejectsNonfiniteWeight) {
    std::vector<double> values{1.0, 1.01, 0.99, 1.02};
    std::vector<double> weights{1.0, 1.0, kNaN, 1.0};
    std::vector<double> out_values(values.size(), 0.0);
    std::vector<double> out_weights(values.size(), 0.0);
    std::vector<std::uint8_t> eligible(values.size(), 0);
    std::uint32_t eligible_count = 0;

    P2EligibilityInput in{};
    in.values = values.data();
    in.weights = weights.data();
    in.count = static_cast<std::uint32_t>(values.size());
    in.support_threshold = 0.0;

    P2EligibilityOutput out{};
    out.values = out_values.data();
    out.weights = out_weights.data();
    out.eligible = eligible.data();
    out.eligible_count = &eligible_count;

    ASSERT_EQ(p2_eligibility_filter(&in, &out), 0);
    EXPECT_EQ(eligible_count, 3u)
        << "非有限 weight 样本未被资格层剔除";
    EXPECT_EQ(eligible[2], 0u);
    EXPECT_GE(out.invalid_finite, 1u);
    for (std::uint32_t i = 0; i < eligible_count; ++i)
        EXPECT_TRUE(std::isfinite(out_weights[i]));
}

// 资格层正例（能绿）：全有限 ⇒ 全部合格且不被计数
TEST(Phase2RejectionNonfiniteWeights, EligibilityKeepsFiniteWeights) {
    std::vector<double> values{1.0, 1.01, 0.99, 1.02};
    std::vector<double> weights{1.0, 2.0, 3.0, 4.0};
    std::vector<double> out_values(values.size(), 0.0);
    std::vector<double> out_weights(values.size(), 0.0);
    std::vector<std::uint8_t> eligible(values.size(), 0);
    std::uint32_t eligible_count = 0;

    P2EligibilityInput in{};
    in.values = values.data();
    in.weights = weights.data();
    in.count = static_cast<std::uint32_t>(values.size());
    in.support_threshold = 0.0;

    P2EligibilityOutput out{};
    out.values = out_values.data();
    out.weights = out_weights.data();
    out.eligible = eligible.data();
    out.eligible_count = &eligible_count;

    ASSERT_EQ(p2_eligibility_filter(&in, &out), 0);
    EXPECT_EQ(eligible_count, 4u);
    EXPECT_EQ(out.invalid_finite, 0u);
    for (std::uint32_t i = 0; i < eligible_count; ++i)
        EXPECT_DOUBLE_EQ(out_weights[i], weights[i]);
}

// 资格层负例（改前红，strided 生产收集器）：非有限 weight 判不合格
TEST(Phase2RejectionNonfiniteWeights, GatherRejectsNonfiniteWeight) {
    const float inf_f = std::numeric_limits<float>::infinity();
    std::vector<float> values{1.0f, 1.01f, 0.99f};
    std::vector<float> weights{1.0f, inf_f, 1.0f};
    std::vector<double> out_values(values.size(), 0.0);
    std::vector<double> out_weights(values.size(), 0.0);
    std::vector<std::uint32_t> source_indices(values.size(), 0);
    std::uint32_t eligible_count = 0;

    P2EligibilityGatherInput in{};
    in.values = values.data();
    in.value_stride = 1;
    in.weights = weights.data();
    in.weight_stride = 1;
    in.count = static_cast<std::uint32_t>(values.size());
    in.pixel = 0;
    in.support_threshold = 0.0;
    in.value_dtype = 0;  // fp32

    P2EligibilityGatherOutput out{};
    out.values = out_values.data();
    out.weights = out_weights.data();
    out.source_indices = source_indices.data();
    out.eligible_count = &eligible_count;

    ASSERT_EQ(p2_collect_candidate_stack(&in, &out), 0);
    EXPECT_EQ(eligible_count, 2u)
        << "strided 收集器未剔除非有限 weight";
    EXPECT_GE(out.invalid_finite, 1u);
}
