// lib/algorithms/coverage/tests/retired_object_reject_negative_test.cpp
//
// 任务 PSFSW-RETIRE-01 共址负例回归门（跨模块的退役对象拒绝面）。
//
// 负责人裁决（原话）：「只要纯净信号/噪声的信噪比。要求跨帧可用，不基于参考帧。
// 而是绝对标定。」⇒ psfsw_robust_weight 不是现行对象（ASTROCS_DESIGN.md §3.1 订正后；
// docs/design/UNIFIED_MODEL.md:58：旧产品若声明该对象 ⇒ 显式拒绝 + 迁移提示）。
//
// 为什么本文件落在 lib/algorithms/coverage/tests/：
//   本任务写域不含 eng/tests/**，而 resample 模块没有共址测试子目录（根 CMakeLists
//   只在 enable_testing() 之后注册 lib/algorithms/coverage 的共址测试）；故按 coverage
//   模块既有共址测试位登记本负例，被测对象是 resample / calibration / drizzle 三个
//   模块的退役对象拒绝面（源文件由本 target 直接编译/链接，见 CMakeLists）。
//
// 能红能绿：把任一模块的退役对象重新变成"静默接受"（不产生 Reject/不返回 false），
// 本文件即失败；mutation 实测见 run/PSFSW-RETIRE-01/REPORT.md。
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "astrocs/calibration/v6_calibration_covariance.h"
#include "p3_rsmp.h"
#include "v6_drizzle_science.h"

namespace {

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

// 退役对象在 resample 侧的门号（沿用既有 G-P3-GLB-01：全局权重/方差来源门）。
const astrocs::p3rsmp::GateResult* find_gate(
    const std::vector<astrocs::p3rsmp::GateResult>& all, const std::string& code) {
    for (const auto& g : all) {
        if (g.code == code) return &g;
    }
    return nullptr;
}

astrocs::p3rsmp::ProductRecord sb_record() {
    astrocs::p3rsmp::ProductRecord rec;
    rec.mode_declared = true;
    rec.mode = astrocs::p3rsmp::P3Mode::SurfaceBrightness;
    return rec;
}

}  // namespace

// ── resample（Phase3）：variance_from 声明退役对象 ⇒ 显式拒绝 + 迁移提示 ─────
TEST(RetiredWeightObject, ResampleVarianceFromRetiredObjectRejected) {
    astrocs::p3rsmp::ProductRecord rec = sb_record();
    rec.variance_from = "psfsw_robust_weight";

    const std::vector<astrocs::p3rsmp::GateResult> all =
        astrocs::p3rsmp::check_failclosed_all(rec, astrocs::p3rsmp::GateConfig{});
    const astrocs::p3rsmp::GateResult* g = find_gate(all, "G-P3-GLB-01");
    ASSERT_NE(g, nullptr) << "declaring the retired object must be rejected (G-P3-GLB-01)";
    EXPECT_EQ(g->status, astrocs::p3rsmp::Status::Reject);
    EXPECT_TRUE(contains(g->reason, "retired_canonical_object=psfsw_robust_weight"))
        << "reason must name the retired object; got: " << g->reason;
    EXPECT_TRUE(contains(g->reason, "not a current object"))
        << "reason must state retirement (not merely a generic forbidden token): " << g->reason;
    EXPECT_TRUE(contains(g->reason, "migration"))
        << "reason must carry a migration hint; got: " << g->reason;
}

// ── resample：weight_sources 声明退役对象 ⇒ 同样显式拒绝（不得静默接受） ─────
TEST(RetiredWeightObject, ResampleWeightSourcesRetiredObjectRejected) {
    astrocs::p3rsmp::ProductRecord rec = sb_record();
    rec.weight_sources = {"psfsw_robust_weight"};

    const std::vector<astrocs::p3rsmp::GateResult> all =
        astrocs::p3rsmp::check_failclosed_all(rec, astrocs::p3rsmp::GateConfig{});
    const astrocs::p3rsmp::GateResult* g = find_gate(all, "G-P3-GLB-01");
    ASSERT_NE(g, nullptr);
    EXPECT_EQ(g->status, astrocs::p3rsmp::Status::Reject);
    EXPECT_TRUE(contains(g->reason, "retired_canonical_object=psfsw_robust_weight"))
        << g->reason;
}

// ── 阳性对照（能绿）：现行方差来源不被该门误杀 ──────────────────────────────
TEST(RetiredWeightObject, ResampleCurrentVarianceFromNotRejectedByGlbGate) {
    astrocs::p3rsmp::ProductRecord rec = sb_record();
    rec.variance_from = "actual_combination_coefficients";
    const std::vector<astrocs::p3rsmp::GateResult> all =
        astrocs::p3rsmp::check_failclosed_all(rec, astrocs::p3rsmp::GateConfig{});
    EXPECT_EQ(find_gate(all, "G-P3-GLB-01"), nullptr)
        << "current variance source must not trip the retired/relative-weight gate";
}

// ── calibration（Phase1 covariance）：variance_from 声明退役对象 ⇒ 拒绝 ──────
TEST(RetiredWeightObject, CalibrationVarianceFromRetiredObjectRejected) {
    astrocs::calibration::v6::CovarianceRecord rec;
    rec.representation = "diagonal_variance";
    rec.combination_coefficients = {1.0};
    rec.variance_from = "psfsw_robust_weight";
    std::string err;
    EXPECT_FALSE(astrocs::calibration::v6::validate_covariance_record(rec, &err))
        << "retired object must be rejected, not silently accepted";
    EXPECT_TRUE(contains(err, "retired canonical object")) << err;
    EXPECT_TRUE(contains(err, "psfsw_robust_weight")) << err;
    EXPECT_TRUE(contains(err, "migration")) << err;
    // 该 token 同时仍在禁止来源表内（拒绝面不得被"残留清理"删除）。
    EXPECT_TRUE(astrocs::calibration::v6::is_forbidden_variance_source("psfsw_robust_weight"));
}

// ── calibration 阳性对照（能绿）：现行方差来源仍被接受 ──────────────────────
TEST(RetiredWeightObject, CalibrationCurrentVarianceFromAccepted) {
    astrocs::calibration::v6::CovarianceRecord rec;
    rec.representation = "diagonal_variance";
    rec.combination_coefficients = {1.0};
    std::string err;
    EXPECT_TRUE(astrocs::calibration::v6::validate_covariance_record(rec, &err)) << err;
}

// ── drizzle（Phase1 单位表）：退役单位符号 ⇒ 显式拒绝 + 迁移提示 ────────────
// PSFSW-RETIRE-03：psfsw_robust_weight 已**物理删除**（UnitId 无其项、冻结单位表无
// 其行；原 is_retired_unit_id 随之删除），识别面只剩字符串面。
TEST(RetiredWeightObject, DrizzleRetiredUnitSymbolRejected) {
    using astrocs::v6::drizzle::frozen_unit;
    using astrocs::v6::drizzle::is_retired_unit_symbol;
    using astrocs::v6::drizzle::retired_unit_reject_reason;
    using astrocs::v6::drizzle::UnitId;

    EXPECT_TRUE(is_retired_unit_symbol("psfsw_robust_weight"));
    EXPECT_TRUE(is_retired_unit_symbol("PSFSW_ROBUST_WEIGHT")) << "case-insensitive parity";

    // 锁定物理删除：任何**现行**单位项都不得再映射到退役符号（枚举/单位表无其身份）。
    const UnitId live[] = {UnitId::signal_sb,       UnitId::sb_variance_out,
                           UnitId::sb_ivar_out,     UnitId::pixel_variance_in,
                           UnitId::w_info,          UnitId::flux,
                           UnitId::q,               UnitId::pixel_area,
                           UnitId::dimensionless};
    for (UnitId id : live) {
        EXPECT_STRNE(frozen_unit(id).symbol, "psfsw_robust_weight")
            << "retired unit must not be a live frozen-unit row (PSFSW-RETIRE-03)";
        EXPECT_FALSE(is_retired_unit_symbol(frozen_unit(id).symbol));
    }

    const std::string reason = retired_unit_reject_reason("psfsw_robust_weight");
    EXPECT_TRUE(contains(reason, "psfsw_robust_weight")) << reason;
    EXPECT_TRUE(contains(reason, "not a current object")) << reason;
    EXPECT_TRUE(contains(reason, "point_information") && contains(reason, "surface_gls"))
        << "reject reason must state the allowed weight objects: " << reason;

    // 阳性对照（能绿）：现行单位符号不被误判为退役对象。
    EXPECT_FALSE(is_retired_unit_symbol("W_info"));
    EXPECT_FALSE(is_retired_unit_symbol("ADU^2"));
    EXPECT_TRUE(retired_unit_reject_reason("W_info").empty());
}
