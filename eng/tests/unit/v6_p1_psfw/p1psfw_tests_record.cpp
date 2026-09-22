/* p1psfw_tests_record.cpp - 记录门 PSFSW-G01..G25 正向控制 + 负向 mutation
 * 负向 mutation 编号对齐 docs/validation/v6/NEGATIVE_MUTATION_CATALOG.md。 */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

namespace {
bool has_gate(const RecordValidation& v, const char* gate) {
    for (const auto& f : v.findings)
        if (f.gate == gate) return true;
    return false;
}
}  /* namespace */

P1PSFW_REGISTER(record) {
    (void)mode;
    /* 正向控制: 合法记录 ACCEPT (否则门为空门) */
    const PsfswRecord good = fixture::good_record();
    const RecordValidation vg = validate_psfsw_record(good);
    P1_CHECKF("record_accept", vg.accept);
    P1_CHECK(vg.findings.empty());

    /* m01 units -> flux^-2 */
    { PsfswRecord r = good; r.weight_units = "flux^-2";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G02")); }
    /* m02 weight_kind -> ivar */
    { PsfswRecord r = good; r.weight_kind = "ivar";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G02")); }
    /* m03 concentration 复制 signal (measurement_id 塌陷) */
    { PsfswRecord r = good; r.components[1].measurement_id = r.components[0].measurement_id;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G04")); }
    /* m04 产物注入 ivar 键 */
    { PsfswRecord r = good; r.produced_keys.push_back("psfsw.ivar");
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G05")); }
    { PsfswRecord r = good; r.produced_keys.push_back("psfsw.noise.variance");
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G05")); }
    { PsfswRecord r = good; r.produced_keys.push_back("psfsw.support");
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G05")); }
    /* m05 covariance 由权重反推 */
    { PsfswRecord r = good; r.covariance_method = "variance_from_weight";
      r.variance_from_weight = true;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G06")); }
    /* m06 scope -> global */
    { PsfswRecord r = good; r.normalization_scope = "global";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G03")); }
    /* m07 版本字段置空 / 指数非法 */
    { PsfswRecord r = good; r.composite.version = "";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G14")); }
    { PsfswRecord r = good; r.composite.alpha = -2.0;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G14")); }
    { PsfswRecord r = good; r.composite.floor = 0.0;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G14")); }
    /* m08 删除共同星集/selection function */
    { PsfswRecord r = good; r.common_star_set_id.clear();
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G08")); }
    /* m09 valid=false 仍写值 (median SNR 回退) */
    { PsfswRecord r = good; r.valid = false; r.has_weight_value = true;
      r.reason = PsfswReason::insufficient_valid_stars;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G12")); }
    { PsfswRecord r = good; r.valid = false; r.has_weight_value = false;
      r.reason = PsfswReason::insufficient_valid_stars; r.weight_value_source = "median_source_snr";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G12")); }
    /* m09b 合法 fail-closed 记录 ACCEPT */
    { PsfswRecord r = good; r.valid = false; r.has_weight_value = false;
      r.reason = PsfswReason::insufficient_valid_stars;
      r.weight_value_source.clear();
      P1_CHECK(validate_psfsw_record(r).accept); }
    /* m10 valid=true 带 reason */
    { PsfswRecord r = good; r.reason = PsfswReason::selection_bias_gate_failed;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G13")); }
    /* m11 effective PSF 缺失 / 仅 FWHM */
    { PsfswRecord r = good; r.effective_psf_id.clear();
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G07")); }
    { PsfswRecord r = good; r.effective_psf_only_fwhm = true;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G07")); }
    { PsfswRecord r = good; r.effective_psf_normalization_declared = false;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G07")); }
    /* m12 n_common=2 */
    { PsfswRecord r = good; r.n_common = 2;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G10")); }
    /* m13 深度门未过 / 未评估 */
    { PsfswRecord r = good; r.depth_gate_ok = false;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G11")); }
    { PsfswRecord r = good; r.depth_gate_evaluated = false;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G11")); }
    { PsfswRecord r = good; r.depth_max_rel_dev = 0.20;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G11")); }
    { PsfswRecord r = good; r.depth_k = 2;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G11")); }
    /* m14 independence proof 非法 */
    { PsfswRecord r = good; r.independence_proof = "per_frame_threshold_intersection";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G09")); }
    /* m15 非均匀 0.45 */
    { PsfswRecord r = good;
      r.components[0].samples = {60.0, 100.0, 140.0, 80.0, 120.0};
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G18")); }
    /* m17 weight_mode=0 (legacy support x snr^2) */
    { PsfswRecord r = good; r.weight_mode = "0";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G21")); }
    { PsfswRecord r = good; r.weight_mode = "2";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G21")); }
    /* m18 variance_from=psfsw_robust_weight */
    { PsfswRecord r = good; r.variance_from = "psfsw_robust_weight";
      const RecordValidation v = validate_psfsw_record(r);
      P1_CHECK(has_gate(v, "PSFSW-G06") || has_gate(v, "PSFSW-G22")); }
    /* m19 group_normalized=false */
    { PsfswRecord r = good; r.group_normalized = false;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G03")); }
    /* m20 concentration 用 fwhm 代替 */
    { PsfswRecord r = good; r.components[1].estimator = "fwhm";
      r.weight_sources.push_back("fwhm");
      const RecordValidation v = validate_psfsw_record(r);
      P1_CHECK(has_gate(v, "PSFSW-G22")); }
    /* m21 组内中值归一被去掉 */
    { PsfswRecord r = good; r.w_psfsw = {0.4, 0.5, 0.6};
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G15")); }
    /* m23 calibration == acceptance */
    { PsfswRecord r = good; r.acceptance_sample_id = r.calibration_sample_id;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G24")); }
    /* m24 baseline claim fisher_optimal */
    { PsfswRecord r = good; r.baseline_claim = "fisher_optimal";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G25")); }
    { PsfswRecord r = good; r.baseline_claim = "better_than";
      r.baseline_bootstrap_resamples = 50; r.baseline_confidence = 0.68;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G25")); }
    { PsfswRecord r = good; r.baseline_claim = "better_than";
      r.baseline_bootstrap_resamples = 200; r.baseline_confidence = 0.95;
      P1_CHECK(validate_psfsw_record(r).accept); }
    /* m25 样本派生共同星集 */
    { PsfswRecord r = good; r.independence_proof = "per_frame_threshold_intersection";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G09")); }
    /* m26 无理由 fail-closed: reason=none 但 valid=false */
    { PsfswRecord r = good; r.valid = false; r.has_weight_value = false;
      r.reason = PsfswReason::none;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G12")); }
    /* G17 分位倒挂 / 覆盖越界 */
    { PsfswRecord r = good; r.components[0].p05 = 10.0; r.components[0].p50 = 5.0;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G17")); }
    { PsfswRecord r = good; r.components[2].valid_area_fraction = 1.5;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G17")); }
    /* G19 标量降级门 */
    { PsfswRecord r = good; r.power_loss = 0.20;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G19")); }
    { PsfswRecord r = good; r.flux_bias = 0.10;
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G19")); }
    /* G23 单位不一致 */
    { PsfswRecord r = good; r.components[2].unit = "ADU/px^2";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G23")); }
    { PsfswRecord r = good; r.components[1].unit = "ADU";
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G23")); }
    /* G01 非退役身份：auto / support_x_snr2 / psf_snr_power / 曾经被当作生产口径的
     * token（point_information / surface_gls）一律记录族身份不符。 */
    for (const char* t : {"auto", "support_x_snr2", "psf_snr_power", "unknown",
                          "point_information", "surface_gls"}) {
        PsfswRecord r = good; r.weight_mode = t;
        P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G01"));
    }
    /* 诊断别名进 weight_sources */
    { PsfswRecord r = good; r.weight_sources.push_back("coverage");
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G22")); }
    { PsfswRecord r = good; r.weight_sources = {"median_source_snr"};
      P1_CHECK(has_gate(validate_psfsw_record(r), "PSFSW-G22")); }

    /* ---- 身份/枚举结构检查（PSFSW-RETIRE-03 + FZ-WEIGHT-SINGLE-PATH）----
     * 权重只有一个口径（阶段1 稀疏 SNR 控制点 → 阶段2 重建稠密 SNR 面 → 逆方差定权
     * → 叠加）⇒ **不存在**"生产权重口径集合"（该集合与成员函数已删除）。
     * psfsw_robust_weight 是**退役对象**：本记录族唯一合法身份 = 退役 token，
     * 且必须能被识别并给出迁移提示（拒绝面保留，不是删断言）。
     * 能红能绿：让任何一个非退役 token 通过 G01，本块立即转红。 */
    P1_CHECK(is_retired_weight_mode_token("psfsw_robust"));
    {
        const std::string r = retired_weight_mode_reject_reason("psfsw_robust");
        P1_CHECK(r.find("FZ-MODE-RETIRED") != std::string::npos);
        P1_CHECK(r.find("psfsw_robust") != std::string::npos);
        P1_CHECK(r.find("not a current object") != std::string::npos);
        /* 迁移提示必须指向单一权重口径，不得再指向"可选择的口径"。 */
        P1_CHECK(r.find("FZ-WEIGHT-SINGLE-PATH") != std::string::npos);
        P1_CHECK(r.find("SNR^2") != std::string::npos);
        P1_CHECK(r.find("allowed production modes") == std::string::npos);
    }
    /* 非退役 token 一律不是本记录族的合法身份（含曾被当作生产口径的 token）。 */
    for (const char* t : {"point_information", "surface_gls", "equal", "pixel_ivar",
                          "psf_snr_power", "auto", "support_x_snr2", "0"}) {
        P1_CHECK(!is_retired_weight_mode_token(t));
        P1_CHECK(retired_weight_mode_reject_reason(t).empty());
    }
}
