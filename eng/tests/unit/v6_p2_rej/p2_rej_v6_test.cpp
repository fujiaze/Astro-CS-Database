// eng/tests/unit/v6_p2_rej/p2_rej_v6_test.cpp
//
// IMPL-P2-REJ-001 共址单元测试：V6 Phase2 分类排异
//   - 阈值 = 预测残差方差 sigma_eff^2 = sigma_phase1^2 + J C_theta J^T
//   - reason（4 继承：accepted/rejected_low/rejected_high/underdetermined）
//   - reason_class（6 污染类，与拒绝方向正交，ADJ-GEN-02）
//   - probability（仅门/推断，禁进权重面）
//   - 小样本 n<=2 -> underdetermined 全接受 recall=0 显式
//   - 校准门（BINMIN=50 / ABS=0.10 / BSS_MIN=0.10）
//
// 独立 Oracle：oracle_expected.inc 由 eng/tests/unit/v6_p2_rej/oracle_rej.py
// （纯标准库、不调用 astrocs）机械生成；本文件另有独立转写参考实现
// （ref_decide，逐条按 ALG-P2S-REJ 公式重写，不调用被测函数）。
//
// 子命令（ctest 逐项注册）：units / oracle / reference / negative /
//                            calibration / determinism / all
#include "astro/phase2/rejection.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include "oracle_expected.inc"

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__,     \
                         __LINE__, #cond);                                 \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                              \
    do {                                                                   \
        const double va = (a), vb = (b);                                   \
        if (!(std::fabs(va - vb) <= (tol))) {                              \
            std::fprintf(stderr,                                           \
                         "CHECK_NEAR failed %s:%d: %s=%.17g vs "           \
                         "%s=%.17g tol=%g\n",                              \
                         __FILE__, __LINE__, #a, va, #b, vb, (double)(tol)); \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static const char* status_name(int s) {
    switch (s) {
        case P2_STATUS_OK: return "OK";
        case P2_STATUS_MIN_SAMPLES: return "MIN_SAMPLES";
        case P2_STATUS_ALL_REJECTED: return "ALL_REJECTED";
        case P2_STATUS_INVALID_INPUT: return "INVALID_INPUT";
        case P2_STATUS_UNDERDETERMINED: return "UNDERDETERMINED";
        case P2_STATUS_INVALID_CONFIGURATION: return "INVALID_CONFIGURATION";
        case P2_STATUS_INVALID_METHOD: return "INVALID_METHOD";
        case P2_STATUS_INTERNAL_ERROR: return "INTERNAL_ERROR";
        default: return "?";
    }
}

static const char* calib_status_name(int s) {
    switch (s) {
        case P2_CALIB_OK: return "OK";
        case P2_CALIB_INSUFFICIENT_SAMPLES: return "INSUFFICIENT_SAMPLES";
        case P2_CALIB_RELIABILITY_FAIL: return "RELIABILITY_FAIL";
        case P2_CALIB_BSS_FAIL: return "BSS_FAIL";
        case P2_CALIB_INVALID_INPUT: return "INVALID_INPUT";
        default: return "?";
    }
}

static P2RejectionPlan default_plan() {
    P2RejectionPlanRequest req{};
    req.request = P2_REJECT_SIGMA;
    req.underdetermined_n = 2;
    P2RejectionPlan plan{};
    p2_reject_plan_resolve(&req, &plan, nullptr, 0);
    return plan;
}

static P2RejectClassifyConfig default_cfg() {
    P2RejectClassifyConfig cfg{};
    cfg.plan = default_plan();
    cfg.profile_version = P2_REJECT_CLASSIFY_PROFILE;
    cfg.motion_min_px = P2_REJ_PROFILE_MOTION_MIN_PX;
    cfg.psf_anomaly_min = P2_REJ_PROFILE_PSF_ANOMALY_MIN;
    cfg.contamination_prior = P2_REJ_PROFILE_CONTAM_PRIOR;
    cfg.outlier_inflation = P2_REJ_PROFILE_OUTLIER_KAPPA;
    cfg.keep_moving_source = 1;
    return cfg;
}

// ---- 被测输出缓冲 ----
struct OutBuf {
    std::vector<std::uint8_t> reasons, classes, deleted, preserved;
    std::vector<double> sigma_eff, z, p, clsprob;
    P2RejectClassifyOutput out{};
    void resize(std::uint32_t n) {
        reasons.assign(n, 0);
        classes.assign(n, 0);
        deleted.assign(n, 0);
        preserved.assign(n, 0);
        sigma_eff.assign(n, 0.0);
        z.assign(n, 0.0);
        p.assign(n, 0.0);
        clsprob.assign((std::size_t)n * P2_REJECT_CLASS_COUNT, 0.0);
        out = P2RejectClassifyOutput{};
        out.reasons = reasons.data();
        out.reason_classes = classes.data();
        out.deleted = deleted.data();
        out.preserved = preserved.data();
        out.sigma_eff = sigma_eff.data();
        out.z = z.data();
        out.probability = p.data();
        out.class_probability = clsprob.data();
    }
};

// =====================================================================
// 独立参考实现（in-test transcription；不调用被测实现）
// =====================================================================
struct RefDecision {
    double sigma_eff = 0.0, z = 0.0, p = 0.0;
    int reason = P2_REASON_ACCEPTED;
    int cls = P2_CLASS_NONE;
    double cp[P2_REJECT_CLASS_COUNT] = {0};
    int deleted = 0, preserved = 0;
};

static double ref_posterior(double z) {
    const double kappa = 4.0, prior = 0.05;
    const double zz = z * z;
    const double lo = std::log(prior) + (-zz / (2.0 * kappa * kappa) - std::log(kappa));
    const double lc = std::log(1.0 - prior) + (-zz / 2.0);
    const double d = lc - lo;
    if (d > 700.0) return 0.0;
    if (d < -700.0) return 1.0;
    return 1.0 / (1.0 + std::exp(d));
}

static RefDecision ref_decide(double r, double s1, double uv, bool growth,
                              bool compact, bool column, double motion,
                              bool lowfreq, double psf, bool keep_moving) {
    RefDecision d;
    d.sigma_eff = std::sqrt(s1 * s1 + uv);
    d.z = r / d.sigma_eff;
    if (d.z <= -4.0) d.reason = P2_REASON_REJECTED_LOW;
    else if (d.z >= 3.0) d.reason = P2_REASON_REJECTED_HIGH;
    else d.reason = P2_REASON_ACCEPTED;
    d.p = ref_posterior(d.z);
    const bool moving = motion >= 0.5;
    double w[P2_REJECT_CLASS_COUNT] = {0};
    if (d.reason != P2_REASON_ACCEPTED || moving) {
        w[P2_CLASS_COSMIC_RAY] = (compact && !growth) ? 1.0 : 0.0;
        w[P2_CLASS_SATELLITE_TRAIL] = growth ? 1.0 : 0.0;
        w[P2_CLASS_BAD_COLUMN] = column ? 1.0 : 0.0;
        w[P2_CLASS_MOVING_SOURCE] = moving ? 1.0 : 0.0;
        w[P2_CLASS_CLOUD_GRADIENT] = lowfreq ? 1.0 : 0.0;
        w[P2_CLASS_DEFOCUS_TRAIL] = (psf >= 0.2) ? 1.0 : 0.0;
    }
    double ws = 0.0;
    for (int c = 1; c < P2_REJECT_CLASS_COUNT; ++c) ws += w[c];
    if (ws > 0.0) {
        double best = -1.0;
        for (int c = 1; c < P2_REJECT_CLASS_COUNT; ++c) {
            d.cp[c] = d.p * (w[c] / ws);
            if (d.cp[c] > best) { best = d.cp[c]; d.cls = c; }
        }
    }
    d.preserved = (d.cls == P2_CLASS_MOVING_SOURCE) ? 1 : 0;
    d.deleted = (d.reason == P2_REASON_REJECTED_LOW ||
                 d.reason == P2_REASON_REJECTED_HIGH)
                    ? 1
                    : 0;
    if (d.preserved && keep_moving) {
        d.deleted = 0;
        d.reason = P2_REASON_ACCEPTED;
    }
    return d;
}

// =====================================================================
// 固定 fixture 表（来自 oracle_expected.inc）
// =====================================================================
static const double* const kRes[ORACLE_CASE_COUNT] = {
    oracle_0_residual, oracle_1_residual, oracle_2_residual};
static const double* const kSig[ORACLE_CASE_COUNT] = {
    oracle_0_sigma_phase1, oracle_1_sigma_phase1, oracle_2_sigma_phase1};
static const double* const kUpm[ORACLE_CASE_COUNT] = {
    oracle_0_upm, oracle_1_upm, oracle_2_upm};
static const unsigned char* const kFlags[ORACLE_CASE_COUNT] = {
    oracle_0_flags, oracle_1_flags, oracle_2_flags};
static const unsigned char* const kGrowth[ORACLE_CASE_COUNT] = {
    oracle_0_growth, oracle_1_growth, oracle_2_growth};
static const unsigned char* const kCompact[ORACLE_CASE_COUNT] = {
    oracle_0_compact, oracle_1_compact, oracle_2_compact};
static const unsigned char* const kColumn[ORACLE_CASE_COUNT] = {
    oracle_0_column, oracle_1_column, oracle_2_column};
static const double* const kMotion[ORACLE_CASE_COUNT] = {
    oracle_0_motion, oracle_1_motion, oracle_2_motion};
static const unsigned char* const kLowfreq[ORACLE_CASE_COUNT] = {
    oracle_0_lowfreq, oracle_1_lowfreq, oracle_2_lowfreq};
static const double* const kPsf[ORACLE_CASE_COUNT] = {
    oracle_0_psf, oracle_1_psf, oracle_2_psf};
static const double* const kESigma[ORACLE_CASE_COUNT] = {
    oracle_0_sigma_eff, oracle_1_sigma_eff, oracle_2_sigma_eff};
static const double* const kEZ[ORACLE_CASE_COUNT] = {
    oracle_0_z, oracle_1_z, oracle_2_z};
static const double* const kEP[ORACLE_CASE_COUNT] = {
    oracle_0_p, oracle_1_p, oracle_2_p};
static const int* const kEReason[ORACLE_CASE_COUNT] = {
    oracle_0_reason, oracle_1_reason, oracle_2_reason};
static const int* const kEClass[ORACLE_CASE_COUNT] = {
    oracle_0_class, oracle_1_class, oracle_2_class};
static const int* const kEDeleted[ORACLE_CASE_COUNT] = {
    oracle_0_deleted, oracle_1_deleted, oracle_2_deleted};
static const int* const kEPreserved[ORACLE_CASE_COUNT] = {
    oracle_0_preserved, oracle_1_preserved, oracle_2_preserved};
static const double* const kEClsProb[ORACLE_CASE_COUNT] = {
    oracle_0_clsprob, oracle_1_clsprob, oracle_2_clsprob};
static const int kEAcc[ORACLE_CASE_COUNT] = {oracle_0_accepted,
                                             oracle_1_accepted,
                                             oracle_2_accepted};
static const int kELow[ORACLE_CASE_COUNT] = {oracle_0_low, oracle_1_low,
                                             oracle_2_low};
static const int kEHigh[ORACLE_CASE_COUNT] = {oracle_0_high, oracle_1_high,
                                              oracle_2_high};
static const double kERecall[ORACLE_CASE_COUNT] = {oracle_0_recall,
                                                   oracle_1_recall,
                                                   oracle_2_recall};

static const double* const kCalibP[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_p, oracle_calib_1_p, oracle_calib_2_p, oracle_calib_3_p,
    oracle_calib_4_p, oracle_calib_5_p};
static const unsigned char* const kCalibY[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_y, oracle_calib_1_y, oracle_calib_2_y, oracle_calib_3_y,
    oracle_calib_4_y, oracle_calib_5_y};
static const char* const kCalibStatus[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_status, oracle_calib_1_status, oracle_calib_2_status,
    oracle_calib_3_status, oracle_calib_4_status, oracle_calib_5_status};
static const double kCalibBrier[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_brier, oracle_calib_1_brier, oracle_calib_2_brier,
    oracle_calib_3_brier, oracle_calib_4_brier, oracle_calib_5_brier};
static const double kCalibBrierRef[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_brier_ref, oracle_calib_1_brier_ref,
    oracle_calib_2_brier_ref, oracle_calib_3_brier_ref,
    oracle_calib_4_brier_ref, oracle_calib_5_brier_ref};
static const double kCalibBss[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_bss, oracle_calib_1_bss, oracle_calib_2_bss,
    oracle_calib_3_bss, oracle_calib_4_bss, oracle_calib_5_bss};
static const double kCalibDev[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_max_abs_reliability_dev,
    oracle_calib_1_max_abs_reliability_dev,
    oracle_calib_2_max_abs_reliability_dev,
    oracle_calib_3_max_abs_reliability_dev,
    oracle_calib_4_max_abs_reliability_dev,
    oracle_calib_5_max_abs_reliability_dev};
static const int kCalibUsed[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_bins_used, oracle_calib_1_bins_used,
    oracle_calib_2_bins_used, oracle_calib_3_bins_used,
    oracle_calib_4_bins_used, oracle_calib_5_bins_used};
static const int kCalibSkipped[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_bins_skipped_small, oracle_calib_1_bins_skipped_small,
    oracle_calib_2_bins_skipped_small, oracle_calib_3_bins_skipped_small,
    oracle_calib_4_bins_skipped_small, oracle_calib_5_bins_skipped_small};
static const int kCalibUsedSamples[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_used_samples, oracle_calib_1_used_samples,
    oracle_calib_2_used_samples, oracle_calib_3_used_samples,
    oracle_calib_4_used_samples, oracle_calib_5_used_samples};
static const int kCalibGate[ORACLE_CALIB_COUNT] = {
    oracle_calib_0_gate, oracle_calib_1_gate, oracle_calib_2_gate,
    oracle_calib_3_gate, oracle_calib_4_gate, oracle_calib_5_gate};

static int run_oracle_case(int ci) {
    P2RejectClassifyInput in{};
    in.residual = kRes[ci];
    in.sigma_phase1 = kSig[ci];
    in.upm_variance = kUpm[ci];
    in.noise_flags = kFlags[ci];
    in.large_scale_growth = kGrowth[ci];
    in.compact_single_frame = kCompact[ci];
    in.column_consistent = kColumn[ci];
    in.cross_frame_motion = kMotion[ci];
    in.low_frequency = kLowfreq[ci];
    in.psf_shape_anomaly = kPsf[ci];
    in.count = (std::uint32_t)oracle_n[ci];
    OutBuf buf;
    buf.resize(in.count);
    P2RejectClassifyConfig cfg = default_cfg();
    const int rc = p2_reject_classify(&in, &cfg, &buf.out);
    CHECK(rc == 0);
    const std::uint32_t n = in.count;
    for (std::uint32_t k = 0; k < n; ++k) {
        CHECK_NEAR(buf.sigma_eff[k], kESigma[ci][k], 1e-12);
        CHECK_NEAR(buf.z[k], kEZ[ci][k], 1e-12);
        CHECK_NEAR(buf.p[k], kEP[ci][k], 1e-12);
        CHECK(buf.reasons[k] == (std::uint8_t)kEReason[ci][k]);
        CHECK(buf.classes[k] == (std::uint8_t)kEClass[ci][k]);
        CHECK(buf.deleted[k] == (std::uint8_t)kEDeleted[ci][k]);
        CHECK(buf.preserved[k] == (std::uint8_t)kEPreserved[ci][k]);
        for (int c = 0; c < P2_REJECT_CLASS_COUNT; ++c)
            CHECK_NEAR(buf.clsprob[(std::size_t)k * P2_REJECT_CLASS_COUNT + c],
                       kEClsProb[ci][(std::size_t)k * P2_REJECT_CLASS_COUNT + c],
                       1e-12);
    }
    CHECK((int)buf.out.accepted_count == kEAcc[ci]);
    CHECK((int)buf.out.rejected_low == kELow[ci]);
    CHECK((int)buf.out.rejected_high == kEHigh[ci]);
    CHECK_NEAR(buf.out.recall, kERecall[ci], 1e-12);
    CHECK(std::strcmp(status_name(buf.out.status), oracle_status[ci]) == 0);
    return 0;
}

// =====================================================================
// mode: units
// =====================================================================
static int mode_units() {
    CHECK(P2_REASON_ACCEPTED == 0);
    CHECK(P2_REASON_REJECTED_LOW == 1);
    CHECK(P2_REASON_REJECTED_HIGH == 2);
    CHECK(P2_REASON_UNDERDETERMINED == 3);
    CHECK(P2_CLASS_NONE == 0);
    CHECK(P2_CLASS_COSMIC_RAY == 1);
    CHECK(P2_CLASS_SATELLITE_TRAIL == 2);
    CHECK(P2_CLASS_BAD_COLUMN == 3);
    CHECK(P2_CLASS_MOVING_SOURCE == 4);
    CHECK(P2_CLASS_CLOUD_GRADIENT == 5);
    CHECK(P2_CLASS_DEFOCUS_TRAIL == 6);
    CHECK(P2_REJECT_CLASS_COUNT == 7);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_NONE), "none") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_COSMIC_RAY),
                      "cosmic_ray") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_SATELLITE_TRAIL),
                      "satellite_trail") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_BAD_COLUMN),
                      "bad_column") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_MOVING_SOURCE),
                      "moving_source") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_CLOUD_GRADIENT),
                      "cloud_gradient") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(P2_CLASS_DEFOCUS_TRAIL),
                      "defocus_trail") == 0);
    CHECK(std::strcmp(p2_rejection_class_id(99), "unknown") == 0);
    // 校准门数值（PENDING_OWNER_SIGNOFF SO-07，fail-closed 按文档值）
    CHECK(P2_REJ_CALIB_BINMIN == 50u);
    CHECK(P2_REJ_CALIB_ABS == 0.10);
    CHECK(P2_REJ_CALIB_BSS_MIN == 0.10);

    // 继承阈值未被改动
    P2RejectionPlan plan = default_plan();
    CHECK(p2_reject_plan_thresholds_inherited(&plan) == 1);
    CHECK(p2_reject_plan_thresholds_inherited(nullptr) == 0);

    // 逐项 mutation：任一继承阈值被改 -> 非继承
    {
        P2RejectionPlan p = plan; p.sigma.lower_sigma = 3.9;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.sigma.upper_sigma = 2.9;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.sigma.max_iterations = 9;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.winsorized.lower_sigma = 3.5;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.averaged.upper_sigma = 3.5;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.median_sigma.lower_sigma = 3.0;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.linear_fit.lower = 4.9;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.linear_fit.upper = 3.4;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.linear_fit.max_iterations = 7;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.esd.alpha = 0.04;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.esd.max_outliers = 9;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.percentile.low_fraction = 0.19;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.percentile.high_fraction = 0.09;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.minmax.min_kept = 3;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.rcr.technique = 1;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.large_scale.enabled = 1;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.large_scale.min_structure_pixels = 7;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.large_scale.low_grow_radius_pixels = 3;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }
    {
        P2RejectionPlan p = plan; p.large_scale.high_grow_radius_pixels = 1;
        CHECK(p2_reject_plan_thresholds_inherited(&p) == 0);
    }

    // 权重面守卫：合法来源干净
    {
        const char* ok[] = {"psf", "noise_covariance", "upm_scale",
                            "design_matrix"};
        char err[256] = {0};
        CHECK(p2_rejection_weight_surface_guard(ok, 4, err, sizeof(err)) == 0);
    }
    // probability / support / coverage / median SNR / FWHM / residual /
    // psfsw / 延迟模式 全部必须 REJECT
    {
        const char* bad[] = {
            "rejection_probability", "probability",   "rejection",
            "support",               "support_area",  "coverage",
            "median_source_snr",     "median_snr",    "fwhm",
            "residual",              "psfsw",         "psf_snr_power",
            "support_x_snr2",        "auto",          "0"};
        for (std::size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
            const char* one[1] = {bad[i]};
            char err[256] = {0};
            const int rc =
                p2_rejection_weight_surface_guard(one, 1, err, sizeof(err));
            if (rc == 0)
                std::fprintf(stderr, "guard missed forbidden token '%s'\n",
                             bad[i]);
            CHECK(rc != 0);
        }
    }
    {
        char err[256] = {0};
        CHECK(p2_rejection_weight_surface_guard(nullptr, 0, err, sizeof(err)) ==
              2);
    }
    return 0;
}

// =====================================================================
// mode: oracle（独立 Python Oracle 期望向量）
// =====================================================================
static int mode_oracle() {
    for (int ci = 0; ci < ORACLE_CASE_COUNT; ++ci) run_oracle_case(ci);
    return 0;
}

// =====================================================================
// mode: reference（in-test 独立转写 + 随机 fixture 交叉校验）
// =====================================================================
static int mode_reference() {
    std::mt19937 rng(20260915u);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    const int n = 64;
    std::vector<double> r(n), s1(n), uv(n), motion(n), psf(n);
    std::vector<std::uint8_t> flags(n, (std::uint8_t)P2_NOISE_REQUIRED);
    std::vector<std::uint8_t> growth(n), compact(n), column(n), lowfreq(n);
    for (int k = 0; k < n; ++k) {
        r[k] = (u01(rng) - 0.5) * 30.0;
        s1[k] = 0.05 + u01(rng) * 4.0;
        uv[k] = u01(rng) * 3.0;
        motion[k] = u01(rng) * 1.5;
        psf[k] = u01(rng) * 0.6;
        growth[k] = (u01(rng) < 0.3) ? 1 : 0;
        compact[k] = (u01(rng) < 0.5) ? 1 : 0;
        column[k] = (u01(rng) < 0.2) ? 1 : 0;
        lowfreq[k] = (u01(rng) < 0.25) ? 1 : 0;
    }
    P2RejectClassifyInput in{};
    in.residual = r.data();
    in.sigma_phase1 = s1.data();
    in.upm_variance = uv.data();
    in.noise_flags = flags.data();
    in.large_scale_growth = growth.data();
    in.compact_single_frame = compact.data();
    in.column_consistent = column.data();
    in.cross_frame_motion = motion.data();
    in.low_frequency = lowfreq.data();
    in.psf_shape_anomaly = psf.data();
    in.count = (std::uint32_t)n;
    OutBuf buf;
    buf.resize((std::uint32_t)n);
    P2RejectClassifyConfig cfg = default_cfg();
    CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
    std::uint32_t acc = 0, lo = 0, hi = 0;
    for (int k = 0; k < n; ++k) {
        const RefDecision d =
            ref_decide(r[k], s1[k], uv[k], growth[k] != 0, compact[k] != 0,
                       column[k] != 0, motion[k], lowfreq[k] != 0, psf[k], true);
        CHECK_NEAR(buf.sigma_eff[k], d.sigma_eff, 1e-12);
        CHECK_NEAR(buf.z[k], d.z, 1e-12);
        CHECK_NEAR(buf.p[k], d.p, 1e-12);
        CHECK(buf.reasons[k] == (std::uint8_t)d.reason);
        CHECK(buf.classes[k] == (std::uint8_t)d.cls);
        CHECK(buf.deleted[k] == (std::uint8_t)d.deleted);
        CHECK(buf.preserved[k] == (std::uint8_t)d.preserved);
        for (int c = 0; c < P2_REJECT_CLASS_COUNT; ++c)
            CHECK_NEAR(buf.clsprob[(std::size_t)k * P2_REJECT_CLASS_COUNT + c],
                       d.cp[c], 1e-12);
        if (d.reason == P2_REASON_REJECTED_LOW) ++lo;
        else if (d.reason == P2_REASON_REJECTED_HIGH) ++hi;
        else ++acc;
    }
    CHECK(buf.out.accepted_count == acc);
    CHECK(buf.out.rejected_low == lo);
    CHECK(buf.out.rejected_high == hi);
    CHECK_NEAR(buf.out.recall, (double)(lo + hi) / n, 1e-12);
    return 0;
}

// =====================================================================
// mode: negative（注入违反冻结 -> 必红）
// =====================================================================
static int mode_negative() {
    const std::uint32_t n = 8;
    std::vector<double> r(n, 1.0), s1(n, 1.0), uv(n, 0.0);
    std::vector<std::uint8_t> flags(n, (std::uint8_t)P2_NOISE_REQUIRED);
    std::vector<std::uint8_t> compact(n, 1), growth(n, 0), column(n, 0),
        lowfreq(n, 0);
    std::vector<double> motion(n, 0.0), psf(n, 0.0);
    P2RejectClassifyInput in{};
    in.residual = r.data();
    in.sigma_phase1 = s1.data();
    in.upm_variance = uv.data();
    in.noise_flags = flags.data();
    in.large_scale_growth = growth.data();
    in.compact_single_frame = compact.data();
    in.column_consistent = column.data();
    in.cross_frame_motion = motion.data();
    in.low_frequency = lowfreq.data();
    in.psf_shape_anomaly = psf.data();
    in.count = n;
    OutBuf buf;
    buf.resize(n);

    // N1: 任一继承阈值改动 -> INVALID_CONFIGURATION
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.plan.sigma.lower_sigma = 3.9;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
        CHECK(buf.out.accepted_count == n);
        CHECK(buf.reasons[0] == P2_REASON_UNDERDETERMINED);
        CHECK(buf.out.recall == 0.0);
    }
    // N2: method=AUTO（永不进 kernel）-> INVALID_METHOD
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.plan.method = P2_REJECT_AUTO;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_METHOD);
    }
    // N3: profile 版本不符 -> INVALID_CONFIGURATION
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.profile_version = "astrocs.rejection.classify.v2";
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
    }
    // N4: profile 常量未版本化改动 -> INVALID_CONFIGURATION
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.motion_min_px = 0.6;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
    }
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.outlier_inflation = 3.0;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
    }
    // N5: sigma_eff 缺 UPM 项（flags 未声明 UPM）-> INVALID_INPUT
    {
        std::fill(flags.begin(), flags.end(), (std::uint8_t)P2_NOISE_PHASE1_DECLARED);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_INPUT);
        CHECK(buf.out.accepted_count == n);
    }
    // N6: sigma_eff 缺 Phase1 项 -> INVALID_INPUT
    {
        std::fill(flags.begin(), flags.end(), (std::uint8_t)P2_NOISE_UPM_DECLARED);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_INPUT);
    }
    std::fill(flags.begin(), flags.end(), (std::uint8_t)P2_NOISE_REQUIRED);
    // N7: 非 finite 输入 -> INVALID_INPUT
    {
        r[3] = std::numeric_limits<double>::quiet_NaN();
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_INPUT);
        r[3] = 1.0;
    }
    // N8: 负 sigma_phase1 -> INVALID_INPUT
    {
        s1[1] = -1.0;
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_INPUT);
        s1[1] = 1.0;
    }
    // N9: sigma_eff==0 -> INVALID_INPUT
    {
        std::vector<double> s0(n, 0.0), u0(n, 0.0);
        P2RejectClassifyInput in0 = in;
        in0.sigma_phase1 = s0.data();
        in0.upm_variance = u0.data();
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in0, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_INPUT);
    }
    // N10: 小样本 n<=2 -> UNDERDETERMINED 全接受 recall=0 显式
    {
        std::vector<double> r2 = {50.0, -30.0};
        std::vector<double> s2 = {1.0, 1.0}, u2 = {0.0, 0.0};
        std::vector<std::uint8_t> f2 = {(std::uint8_t)P2_NOISE_REQUIRED,
                                        (std::uint8_t)P2_NOISE_REQUIRED};
        std::vector<std::uint8_t> c2 = {1, 1};
        P2RejectClassifyInput in2{};
        in2.residual = r2.data();
        in2.sigma_phase1 = s2.data();
        in2.upm_variance = u2.data();
        in2.noise_flags = f2.data();
        in2.compact_single_frame = c2.data();
        in2.count = 2;
        OutBuf b2;
        b2.resize(2);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in2, &cfg, &b2.out) == 0);
        CHECK(b2.out.status == P2_STATUS_UNDERDETERMINED);
        CHECK(b2.out.accepted_count == 2);
        CHECK(b2.out.rejected_low == 0 && b2.out.rejected_high == 0);
        CHECK(b2.out.recall == 0.0);
        CHECK(b2.reasons[0] == P2_REASON_UNDERDETERMINED);
        CHECK(b2.reasons[1] == P2_REASON_UNDERDETERMINED);
        CHECK(b2.classes[0] == P2_CLASS_NONE);
        CHECK(b2.p[0] == 0.0 && b2.p[1] == 0.0);
        CHECK(b2.deleted[0] == 0 && b2.deleted[1] == 0);
    }
    // N11: 空栈 -> MIN_SAMPLES
    {
        P2RejectClassifyInput in0{};
        in0.count = 0;
        OutBuf b0;
        b0.resize(0);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in0, &cfg, &b0.out) == 0);
        CHECK(b0.out.status == P2_STATUS_MIN_SAMPLES);
    }
    // N12: 裸残差陷阱 —— 大原始残差但 sigma_eff 大 -> ACCEPTED；
    //      小原始残差但 sigma_eff 小 -> REJECTED_HIGH
    {
        std::vector<double> r3 = {10.0, 0.2};
        std::vector<double> s3 = {4.0, 0.05};
        std::vector<double> u3 = {0.0, 0.0};
        std::vector<std::uint8_t> f3(2, (std::uint8_t)P2_NOISE_REQUIRED);
        std::vector<std::uint8_t> c3 = {0, 0};
        P2RejectClassifyInput in3{};
        in3.residual = r3.data();
        in3.sigma_phase1 = s3.data();
        in3.upm_variance = u3.data();
        in3.noise_flags = f3.data();
        in3.compact_single_frame = c3.data();
        in3.count = 2;
        OutBuf b3;
        b3.resize(2);
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.plan.underdetermined_n = 1;  // n=2 走正常路径（测试用，非生产默认）
        CHECK(p2_reject_classify(&in3, &cfg, &b3.out) == 0);
        CHECK(b3.reasons[0] == P2_REASON_ACCEPTED);       // z=2.5
        CHECK(b3.reasons[1] == P2_REASON_REJECTED_HIGH);  // z=4.0
    }
    // N13: 移动源保留独立层（科学信号默认不删）；n=4 走正常路径
    {
        std::vector<double> r4 = {5.0, 0.1, 0.1, 0.1};
        std::vector<double> s4(4, 1.0), u4(4, 0.0);
        std::vector<double> m4 = {0.9, 0.0, 0.0, 0.0};
        std::vector<std::uint8_t> f4(4, (std::uint8_t)P2_NOISE_REQUIRED);
        P2RejectClassifyInput in4{};
        in4.residual = r4.data();
        in4.sigma_phase1 = s4.data();
        in4.upm_variance = u4.data();
        in4.noise_flags = f4.data();
        in4.cross_frame_motion = m4.data();
        in4.count = 4;
        OutBuf b4;
        b4.resize(4);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in4, &cfg, &b4.out) == 0);
        CHECK(b4.classes[0] == P2_CLASS_MOVING_SOURCE);
        CHECK(b4.preserved[0] == 1);
        CHECK(b4.deleted[0] == 0);
        CHECK(b4.reasons[0] == P2_REASON_ACCEPTED);
        // 显式 keep=0 -> 按阈值照删
        OutBuf b5;
        b5.resize(4);
        cfg.keep_moving_source = 0;
        CHECK(p2_reject_classify(&in4, &cfg, &b5.out) == 0);
        CHECK(b5.classes[0] == P2_CLASS_MOVING_SOURCE);
        CHECK(b5.deleted[0] == 1);
        CHECK(b5.reasons[0] == P2_REASON_REJECTED_HIGH);
    }
    // N14: reason 与 reason_class 正交（拒绝方向字段 + 污染机制字段分离）
    {
        std::vector<double> r6 = {-6.0, 0.1, 0.1, 0.1};
        std::vector<double> s6(4, 1.0), u6 = {1.0, 0.0, 0.0, 0.0};
        std::vector<std::uint8_t> f6(4, (std::uint8_t)P2_NOISE_REQUIRED);
        std::vector<std::uint8_t> g6 = {1, 0, 0, 0};
        P2RejectClassifyInput in6{};
        in6.residual = r6.data();
        in6.sigma_phase1 = s6.data();
        in6.upm_variance = u6.data();
        in6.noise_flags = f6.data();
        in6.large_scale_growth = g6.data();
        in6.count = 4;
        OutBuf b6;
        b6.resize(4);
        P2RejectClassifyConfig cfg = default_cfg();
        CHECK(p2_reject_classify(&in6, &cfg, &b6.out) == 0);
        CHECK(b6.reasons[0] == P2_REASON_REJECTED_LOW);
        CHECK(b6.classes[0] == P2_CLASS_SATELLITE_TRAIL);
    }
    // N15: probability 不得进权重面
    {
        const char* w[1] = {"rejection_probability"};
        char err[256] = {0};
        CHECK(p2_rejection_weight_surface_guard(w, 1, err, sizeof(err)) != 0);
    }
    // N16: 继承的 方法×normalization 组合非法 -> INVALID_CONFIGURATION
    {
        P2RejectClassifyConfig cfg = default_cfg();
        cfg.plan.method = P2_REJECT_RCR;
        cfg.plan.normalization = P2_NORMALIZE_MEDIAN_CENTER;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
        cfg.plan.method = P2_REJECT_PERCENTILE;
        cfg.plan.normalization = P2_NORMALIZE_NONE;
        CHECK(p2_reject_classify(&in, &cfg, &buf.out) == 0);
        CHECK(buf.out.status == P2_STATUS_INVALID_CONFIGURATION);
    }
    return 0;
}

// =====================================================================
// mode: calibration
// =====================================================================
static int mode_calibration() {
    for (int ci = 0; ci < ORACLE_CALIB_COUNT; ++ci) {
        P2RejectCalibrationOutput co{};
        const int rc = p2_reject_calibration(kCalibP[ci], kCalibY[ci],
                                             (std::uint32_t)oracle_calib_n[ci],
                                             P2_REJ_PROFILE_BIN_COUNT, &co);
        CHECK(rc == 0);
        CHECK(std::strcmp(calib_status_name(co.status), kCalibStatus[ci]) == 0);
        CHECK_NEAR(co.brier, kCalibBrier[ci], 1e-12);
        CHECK_NEAR(co.brier_ref, kCalibBrierRef[ci], 1e-12);
        CHECK_NEAR(co.bss, kCalibBss[ci], 1e-12);
        CHECK_NEAR(co.max_abs_reliability_dev, kCalibDev[ci], 1e-12);
        CHECK((int)co.bins_used == kCalibUsed[ci]);
        CHECK((int)co.bins_skipped_small == kCalibSkipped[ci]);
        CHECK((int)co.used_samples == kCalibUsedSamples[ci]);
        CHECK(co.probability_is_scientific_gate == kCalibGate[ci]);
    }
    // 非法输入 -> INVALID_INPUT，且不得声明概率为科学门
    {
        const double pbad[2] = {1.5, 0.5};
        const std::uint8_t y[2] = {0, 1};
        P2RejectCalibrationOutput co{};
        CHECK(p2_reject_calibration(pbad, y, 2, 0, &co) == 0);
        CHECK(co.status == P2_CALIB_INVALID_INPUT);
        CHECK(co.probability_is_scientific_gate == 0);
        const double pnan[2] = {std::numeric_limits<double>::quiet_NaN(), 0.5};
        CHECK(p2_reject_calibration(pnan, y, 2, 0, &co) == 0);
        CHECK(co.status == P2_CALIB_INVALID_INPUT);
        const std::uint8_t ybad[2] = {0, 2};
        const double pok[2] = {0.2, 0.7};
        CHECK(p2_reject_calibration(pok, ybad, 2, 0, &co) == 0);
        CHECK(co.status == P2_CALIB_INVALID_INPUT);
        CHECK(p2_reject_calibration(nullptr, nullptr, 0, 0, &co) == 0);
        CHECK(co.status == P2_CALIB_INVALID_INPUT);
    }
    return 0;
}

// =====================================================================
// mode: determinism
// =====================================================================
static int mode_determinism() {
    const std::uint32_t n = 32;
    std::mt19937 rng(7u);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::vector<double> r(n), s1(n), uv(n), motion(n), psf(n);
    std::vector<std::uint8_t> flags(n, (std::uint8_t)P2_NOISE_REQUIRED);
    std::vector<std::uint8_t> growth(n), compact(n), column(n), lowfreq(n);
    for (std::uint32_t k = 0; k < n; ++k) {
        r[k] = (u01(rng) - 0.5) * 40.0;
        s1[k] = 0.05 + u01(rng) * 3.0;
        uv[k] = u01(rng) * 2.0;
        motion[k] = u01(rng);
        psf[k] = u01(rng) * 0.5;
        growth[k] = (u01(rng) < 0.3) ? 1 : 0;
        compact[k] = (u01(rng) < 0.5) ? 1 : 0;
        column[k] = (u01(rng) < 0.2) ? 1 : 0;
        lowfreq[k] = (u01(rng) < 0.25) ? 1 : 0;
    }
    P2RejectClassifyInput in{};
    in.residual = r.data();
    in.sigma_phase1 = s1.data();
    in.upm_variance = uv.data();
    in.noise_flags = flags.data();
    in.large_scale_growth = growth.data();
    in.compact_single_frame = compact.data();
    in.column_consistent = column.data();
    in.cross_frame_motion = motion.data();
    in.low_frequency = lowfreq.data();
    in.psf_shape_anomaly = psf.data();
    in.count = n;
    P2RejectClassifyConfig cfg = default_cfg();
    OutBuf a, b;
    a.resize(n);
    b.resize(n);
    CHECK(p2_reject_classify(&in, &cfg, &a.out) == 0);
    CHECK(p2_reject_classify(&in, &cfg, &b.out) == 0);
    CHECK(a.reasons == b.reasons);
    CHECK(a.classes == b.classes);
    CHECK(a.deleted == b.deleted);
    CHECK(a.preserved == b.preserved);
    CHECK(a.sigma_eff == b.sigma_eff);
    CHECK(a.z == b.z);
    CHECK(a.p == b.p);
    CHECK(a.clsprob == b.clsprob);
    CHECK(a.out.accepted_count == b.out.accepted_count);
    CHECK(a.out.recall == b.out.recall);
    CHECK(a.out.status == b.out.status);

    // 置换输入 -> 每样本决策跟随样本（固定序，无全局排序依赖）
    std::vector<std::uint32_t> perm(n);
    for (std::uint32_t k = 0; k < n; ++k) perm[k] = k;
    std::mt19937 rng2(99u);
    std::shuffle(perm.begin(), perm.end(), rng2);
    std::vector<double> rr(n), ss(n), uu(n), mm(n), pp(n);
    std::vector<std::uint8_t> gg(n), cc(n), co(n), ll(n);
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t src = perm[k];
        rr[k] = r[src];
        ss[k] = s1[src];
        uu[k] = uv[src];
        mm[k] = motion[src];
        pp[k] = psf[src];
        gg[k] = growth[src];
        cc[k] = compact[src];
        co[k] = column[src];
        ll[k] = lowfreq[src];
    }
    P2RejectClassifyInput in2 = in;
    in2.residual = rr.data();
    in2.sigma_phase1 = ss.data();
    in2.upm_variance = uu.data();
    in2.cross_frame_motion = mm.data();
    in2.psf_shape_anomaly = pp.data();
    in2.large_scale_growth = gg.data();
    in2.compact_single_frame = cc.data();
    in2.column_consistent = co.data();
    in2.low_frequency = ll.data();
    OutBuf c;
    c.resize(n);
    CHECK(p2_reject_classify(&in2, &cfg, &c.out) == 0);
    for (std::uint32_t k = 0; k < n; ++k) {
        const std::uint32_t src = perm[k];
        CHECK(c.reasons[k] == a.reasons[src]);
        CHECK(c.classes[k] == a.classes[src]);
        CHECK(c.deleted[k] == a.deleted[src]);
        CHECK(c.preserved[k] == a.preserved[src]);
        CHECK(c.p[k] == a.p[src]);
        CHECK(c.z[k] == a.z[src]);
    }
    return 0;
}

int main(int argc, char** argv) {
    const std::string mode = (argc > 1) ? argv[1] : "all";
    if (mode == "units" || mode == "all") mode_units();
    if (mode == "oracle" || mode == "all") mode_oracle();
    if (mode == "reference" || mode == "all") mode_reference();
    if (mode == "negative" || mode == "all") mode_negative();
    if (mode == "calibration" || mode == "all") mode_calibration();
    if (mode == "determinism" || mode == "all") mode_determinism();
    if (failures != 0) {
        std::fprintf(stderr, "[p2_rej_v6] mode=%s FAILURES=%d\n",
                     mode.c_str(), failures);
        return 1;
    }
    std::printf("[p2_rej_v6] mode=%s OK\n", mode.c_str());
    return 0;
}
