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
//   docs/science/PSF_SIGNAL_WEIGHT.md §1/§4（§4 为单一权重口径：阶段1 稀疏 SNR 控制点
//     → 阶段2 重建稠密 SNR 面 → 逆方差定权 → 叠加；无模式选择）。
//   docs/design/UNIFIED_MODEL.md:58（旧产品若声明该对象 ⇒ 显式拒绝 + 迁移提示）。
//   ENGINEERING_SPEC.md §8（每项检查有正例与负例，能红能绿）。
//
// 能红能绿：
//   绿 = 退役对象的声明 token 在**权重来源 token 门**与**运行面路由**上都被显式拒绝，
//        且拒绝理由可诊断（被拒 token + FZ-MODE-RETIRED + 迁移提示）。
//   红 = 把 psfsw_robust_weight 从 kForbiddenWeightSourceTokens 移除、或让运行面路由
//        对任一 token 走 kProduction 分支，本文件即失败。
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

// ── 正例（能绿）：单一口径的合法权重来源仍被接受（收窄不得误伤） ─────────────
TEST(PsfswRetire, SinglePathWeightSourcesAccepted) {
    const char* ok[] = {"psf", "photometric_response", "noise_covariance"};
    for (const char* t : ok) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_source_token_reject(&t, 1, err, sizeof(err)), 0)
            << "legitimate weight source must pass: " << t;
        EXPECT_STREQ(err, "") << "accepted token must not write an error: " << t;
    }
}

// ── 负例（能红）：诊断量不得进权重来源面（FZ-GATE-MEDIAN-SNR / SUPPORT-COVERAGE） ──
TEST(PsfswRetire, DiagnosticTokensRejectedAsWeightSources) {
    const char* bad[] = {"median_source_snr", "support", "coverage", "fwhm", "residual"};
    for (const char* t : bad) {
        char err[512] = {0};
        EXPECT_EQ(p2_weight_source_token_reject(&t, 1, err, sizeof(err)), 1)
            << "diagnostic token must be rejected: " << t;
        EXPECT_NE(as_string(err).size(), 0u) << "reject must be diagnosable: " << t;
    }
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
        // 迁移提示必须指向**单一权重口径**（重建稠密 SNR 面 → 逆方差），不得再指向
        // 任何"可选择的口径"。
        EXPECT_TRUE(contains(msg, "SNR^2") || contains(msg, "inverse-variance")) << msg;
        EXPECT_FALSE(contains(msg, "allowed production modes")) << msg;
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
    const astrocs::v6runtime::ModeRoute retired = astrocs::v6runtime::route_phase2_weight_token("psfsw_robust");
    EXPECT_EQ(retired.kind, RouteKind::kReject) << "runtime route must reject psfsw_robust";
    EXPECT_NE(retired.rc, 0) << "reject must carry a non-zero rc (fail-closed)";
    EXPECT_TRUE(contains(retired.reason, "FZ-MODE-RETIRED")) << retired.reason;
    EXPECT_TRUE(contains(retired.reason, "migration")) << retired.reason;

    // FZ-WEIGHT-SINGLE-PATH：phase2 的权重口径 token 面**没有合法取值**。
    // 能红能绿：给任一 token 加 kProduction 分支，本块立即转红。
    for (const char* m : {"point_information", "surface_gls", "psfsw_robust",
                          "psf_snr_power", "auto", "support_x_snr2", "0", "1", "2",
                          "bogus"}) {
        const astrocs::v6runtime::ModeRoute r = astrocs::v6runtime::route_phase2_weight_token(m);
        EXPECT_EQ(r.kind, RouteKind::kReject) << "token=" << m;
        EXPECT_EQ(r.rc, 2) << "token=" << m;
    }
    /* §9.73 裁决 A44：原 documented baseline 面（equal / pixel_ivar → kBaseline，rc=0）
       已删除。其唯一理由是「legacy 整数路由的映射目标登记」；整数路由删除后理由消失，
       且它们是**输入路径**（CLI --mode）上的口径 token ⇒ 与其余 token 同归 fail-closed
       （ASTROCS_DESIGN.md §3.1:175「没有可选择项」；PSF_SIGNAL_WEIGHT.md §4:72）。 */
    for (const char* m : {"equal", "pixel_ivar"}) {
        const astrocs::v6runtime::ModeRoute r = astrocs::v6runtime::route_phase2_weight_token(m);
        EXPECT_EQ(r.kind, RouteKind::kReject) << "token=" << m;
        EXPECT_EQ(r.rc, 2) << "token=" << m;
    }
    /* legacy 整数 weight_mode：纯拒绝面，全值域拒绝。 */
    for (const int v : {0, 1, 2, 7}) {
        const astrocs::v6runtime::ModeRoute r =
            astrocs::v6runtime::route_legacy_weight_mode_int(v);
        EXPECT_EQ(r.kind, RouteKind::kReject) << "legacy int=" << v;
        EXPECT_EQ(r.rc, 2) << "legacy int=" << v;
        EXPECT_TRUE(contains(r.reason, "FZ-FIELD-WEIGHTMODE")) << r.reason;
    }
    const astrocs::v6runtime::ModeRoute deferred =
        astrocs::v6runtime::route_phase2_weight_token("psf_snr_power");
    EXPECT_TRUE(contains(deferred.reason, "FZ-MODE-DEFERRED")) << deferred.reason;
    EXPECT_TRUE(contains(astrocs::v6runtime::route_phase2_weight_token("bogus").reason,
                         "FZ-WEIGHT-SINGLE-PATH"))
        << "unknown token reject reason must state the single weight path";
}
