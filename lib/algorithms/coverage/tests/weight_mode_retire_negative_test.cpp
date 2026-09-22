// lib/algorithms/coverage/tests/weight_mode_retire_negative_test.cpp
//
// 任务 PSFSW-RETIRE-01（负责人裁决）共址负例回归门。
//
// 负责人裁决（原话）：「只要纯净信号/噪声的信噪比。要求跨帧可用，不基于参考帧。
// 而是绝对标定。」⇒ 受 PixInsight PSFSW 启发的稳健复合帧权重 psfsw_robust_weight
// **不是现行对象**，退役。
//
// 依据（权威，只读）：
//   ASTROCS_DESIGN.md §3.1（订正后）：权重只能来自纯净信号与噪声之比（SNR 逆方差）；
//     跨帧可用、不基于参考帧、绝对标定；PSF 拟合质量代理只作诊断。
//   docs/science/PSF_SIGNAL_WEIGHT.md §1/§4（§4 的 Phase2 选择表已移除 psfsw_robust 行，
//     仅剩 point_information（默认）/ psf_snr_power（DEFERRED）/ surface_gls）。
//   docs/design/UNIFIED_MODEL.md:58（旧产品若声明该对象 ⇒ 显式拒绝 + 迁移提示）。
//   ENGINEERING_SPEC.md §8（每项检查有正例与负例，能红能绿）。
//
// 能红能绿（mutation 实测见 run/PSFSW-RETIRE-01/REPORT.md）：
//   绿 = 生产模式门只接受 {point_information, surface_gls}；psfsw_robust 显式拒绝。
//   红 = 把 psfsw_robust 放回 allowed 集合、或让退役分支静默返回 0，本文件即失败。
#include <gtest/gtest.h>

#include <string>

#include "astro/phase2/coverage.h"
#include "v6_runtime_contract.h"

namespace {

std::string as_string(const char* p) { return std::string(p == nullptr ? "" : p); }

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

std::string lower(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

}  // namespace

// ── 正例（能绿）：现行生产模式仍被接受 ──────────────────────────────────────
TEST(PsfswRetire, CurrentProductionModesAccepted) {
    const char* allowed[] = {"point_information", "surface_gls"};
    for (const char* m : allowed) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_mode_check(m, err, sizeof(err)), 0) << "mode=" << m;
        EXPECT_STREQ(err, "") << "accepted mode must not write an error: " << m;
    }
}

// ── 负例（能红）：退役对象 psfsw_robust 必须被显式拒绝，且消息可诊断 ─────────
TEST(PsfswRetire, RetiredModeRejectedWithDiagnostic) {
    char err[512] = {0};
    const int rc = p2_weight_mode_check("psfsw_robust", err, sizeof(err));
    const std::string msg = as_string(err);

    EXPECT_EQ(rc, 1) << "psfsw_robust must be REJECT (rc=1), got " << rc;
    EXPECT_NE(msg.find("psfsw_robust"), std::string::npos)
        << "reject message must name the rejected mode; got: " << msg;
    EXPECT_TRUE(contains(msg, "point_information") && contains(msg, "surface_gls"))
        << "reject message must state the allowed set; got: " << msg;
    EXPECT_TRUE(contains(msg, "FZ-MODE-RETIRED"))
        << "reject message must carry the frozen node id; got: " << msg;
    // 退役对象必须有迁移提示（UNIFIED_MODEL.md:58），不得只说"未知模式"。
    EXPECT_TRUE(contains(msg, "not a current object"))
        << "reject message must state the object is retired; got: " << msg;
    // 大小写不敏感同口径（与 token 门一致）：不得靠大小写绕过。
    char err2[512] = {0};
    EXPECT_EQ(p2_weight_mode_check("PSFSW_ROBUST", err2, sizeof(err2)), 1)
        << "case variant must not bypass the retired-object rejection";
}

// ── 负例（能红）：DEFERRED 模式仍被显式拒绝（收窄不得误放行） ───────────────
TEST(PsfswRetire, DeferredAndLegacyStillRejected) {
    const char* bad[] = {"psf_snr_power", "auto", "support_x_snr2", "0", "1", "2",
                         "unknown", ""};
    for (const char* m : bad) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_mode_check(m, err, sizeof(err)), 1) << "mode=" << m;
        EXPECT_NE(as_string(err).size(), 0u) << "reject must be diagnosable: " << m;
    }
    char err[512] = {0};
    EXPECT_EQ(p2_weight_mode_check(nullptr, err, sizeof(err)), 1);
    EXPECT_EQ(p2_weight_mode_check("equal", err, sizeof(err)), 2);
    EXPECT_EQ(p2_weight_mode_check("pixel_ivar", err, sizeof(err)), 2);
}

// ── 负例（能红）：声明退役对象的产品在 token 门被显式拒绝（非静默接受） ─────
TEST(PsfswRetire, RetiredObjectTokenRejectedNotSilentlyAccepted) {
    const char* tokens[] = {"psfsw_robust_weight", "psfsw", "PSFSW_ROBUST_WEIGHT"};
    for (const char* t : tokens) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_source_token_reject(&t, 1, err, sizeof(err)), 1)
            << "retired-object token must be rejected: " << t;
        // 消息用冻结表里的 canonical token 名（大小写不敏感同口径）。
        EXPECT_TRUE(contains(lower(as_string(err)), lower(t)))
            << "reject message must name the token; got: " << as_string(err);
    }
    // 退役对象走"退役 + 迁移"诊断路径，不得只说"诊断量不是权重"。
    {
        const char* t = "psfsw_robust_weight";
        char err[512] = {0};
        EXPECT_EQ(p2_weight_source_token_reject(&t, 1, err, sizeof(err)), 1);
        const std::string msg = as_string(err);
        EXPECT_TRUE(contains(msg, "FZ-MODE-RETIRED")) << msg;
        EXPECT_TRUE(contains(msg, "not a current object")) << msg;
        EXPECT_TRUE(contains(msg, "migration")) << msg;
        EXPECT_TRUE(contains(msg, "point_information") && contains(msg, "surface_gls")) << msg;
    }
    // 阳性对照：现行权重对象 token 不被误杀。
    const char* ok[] = {"psf", "psfsw.signal", "photometric_response", "pixel_ivar"};
    for (const char* t : ok) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_source_token_reject(&t, 1, err, sizeof(err)), 0)
            << "benign token must pass: " << t;
    }
}

// ── 负例（能红）：运行面路由（CLI 唯一契约点）同步收窄 ──────────────────────
TEST(PsfswRetire, RuntimeRouteRejectsRetiredMode) {
    using astrocs::v6runtime::RouteKind;
    const astrocs::v6runtime::ModeRoute retired = astrocs::v6runtime::route_phase2_mode("psfsw_robust");
    EXPECT_EQ(retired.kind, RouteKind::kReject) << "runtime route must reject psfsw_robust";
    EXPECT_NE(retired.rc, 0) << "reject must carry a non-zero rc (fail-closed)";
    EXPECT_TRUE(contains(retired.reason, "FZ-MODE-RETIRED")) << retired.reason;
    EXPECT_TRUE(contains(retired.reason, "point_information") &&
                contains(retired.reason, "surface_gls"))
        << "runtime reject reason must state the allowed set: " << retired.reason;

    for (const char* m : {"point_information", "surface_gls"}) {
        const astrocs::v6runtime::ModeRoute r = astrocs::v6runtime::route_phase2_mode(m);
        EXPECT_EQ(r.kind, RouteKind::kProduction) << "mode=" << m;
        EXPECT_EQ(r.rc, 0) << "mode=" << m;
    }
    const astrocs::v6runtime::ModeRoute deferred =
        astrocs::v6runtime::route_phase2_mode("psf_snr_power");
    EXPECT_EQ(deferred.kind, RouteKind::kReject);
    EXPECT_TRUE(contains(deferred.reason, "FZ-MODE-DEFERRED")) << deferred.reason;
}
