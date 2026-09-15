/* p1psfw_tests_psfsw.cpp - 四分量 / 共同星集 / 复合与组内归一 (正+负) */
#include "p1psfw_fixtures.hpp"
#include "p1psfw_oracle.hpp"
#include "p1psfw_test_main.hpp"

#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace astrocs::v6::p1psfw;

P1PSFW_REGISTER(components) {
    (void)mode;
    FrameComponentInput in;
    in.a_nea = 4.0;
    in.fhat = {100.0, 102.0, 98.0, 101.0, 99.0, 100.5, 99.5, 101.5};
    in.background_robust_mean = 0.5;
    in.a_ref = 4.0;
    in.signal_samples = {800.0, 801.0, 799.0, 800.5};
    in.concentration_samples = {200.0, 200.2, 199.8, 200.1};
    in.background_samples = {2.0, 2.0, 2.0, 2.0};
    in.noise_samples = {1.5, 1.4, 1.6, 1.5};
    const PsfswFrameComponents fc = extract_psfsw_components(in);
    P1_CHECKF("components_ok", fc.ok);
    double s = 0.0;
    for (double v : in.fhat) s += v;
    P1_CHECK_NEAR(fc.s, s, 1e-12);
    P1_CHECK_NEAR(fc.conc, (s / in.fhat.size()) / in.a_nea, 1e-12);
    P1_CHECK_NEAR(fc.n, oracle::o_robust_scale_mad(in.fhat), 1e-12);
    P1_CHECK_NEAR(fc.b, 2.0, 1e-12);
    /* 四分量 measurement_id 互异 (FZ-FIELD-PSFSW-4COMP) */
    std::set<std::string> ids = {fc.signal.measurement_id, fc.concentration.measurement_id,
                                 fc.noise.measurement_id, fc.background.measurement_id};
    P1_CHECK(ids.size() == 4);
    P1_CHECK(fc.concentration.measurement_id != fc.signal.measurement_id);
    P1_CHECK(fc.signal.unit == "ADU");
    P1_CHECK(fc.concentration.unit == "ADU/px^2");
    /* p05<=p50<=p95 + valid_area_fraction */
    for (const ComponentMeasure* c : {&fc.signal, &fc.concentration, &fc.noise, &fc.background}) {
        P1_CHECK(c->p05 <= c->p50 && c->p50 <= c->p95);
        P1_CHECK(c->valid_area_fraction >= 0.0 && c->valid_area_fraction <= 1.0);
    }
    /* 分量语义: Conc != FWHM; Noise != pixel sigma (由不同输入构造验证) */
    P1_CHECK(fc.conc != fc.n);

    /* 负向: 星数不足 / A_NEA 非正 / 有效帧全假 */
    FrameComponentInput few = in;
    few.fhat = {100.0, 101.0};
    const PsfswFrameComponents f_few = extract_psfsw_components(few);
    P1_CHECK(!f_few.ok && f_few.reason == PsfswReason::insufficient_valid_stars);
    FrameComponentInput bad_anea = in;
    bad_anea.a_nea = 0.0;
    P1_CHECK(!extract_psfsw_components(bad_anea).ok);
    FrameComponentInput none_valid = in;
    none_valid.fhat_valid.assign(in.fhat.size(), 0);
    P1_CHECK(!extract_psfsw_components(none_valid).ok);
    FrameComponentInput mis = in;
    mis.fhat_valid = {1, 0};
    P1_CHECK(!extract_psfsw_components(mis).ok);

    /* ---- 合成帧组: S 单调方向 ---- */
    std::vector<ComponentValues> frames = {
        {100.0, 4.0, 2.0, 200.0}, {200.0, 4.0, 2.0, 200.0}, {150.0, 4.0, 2.0, 200.0}};
    const CompositeResult cr = compute_psfsw_weights(frames);
    P1_CHECKF("components_composite", cr.ok);
    P1_CHECK(cr.w_psfsw.size() == 3);
    for (double w : cr.w_psfsw) P1_CHECK(w > 0.0);
    P1_CHECK_NEAR(oracle::o_median(cr.w_psfsw), 1.0, kCompositeMedianRtol);
    P1_CHECK(cr.w_psfsw[1] > cr.w_psfsw[0]);   /* S 更大 -> W 更大 */
    /* Conc↑ -> W↑; N↑ -> W↓; B↑ -> W↓ */
    std::vector<ComponentValues> f2 = {{100.0, 8.0, 2.0, 200.0}, {100.0, 4.0, 2.0, 200.0}, {100.0, 4.0, 2.0, 200.0}};
    P1_CHECK(compute_psfsw_weights(f2).w_psfsw[0] > compute_psfsw_weights(f2).w_psfsw[1]);
    std::vector<ComponentValues> f3 = {{100.0, 4.0, 4.0, 200.0}, {100.0, 4.0, 2.0, 200.0}, {100.0, 4.0, 2.0, 200.0}};
    P1_CHECK(compute_psfsw_weights(f3).w_psfsw[0] < compute_psfsw_weights(f3).w_psfsw[1]);
    std::vector<ComponentValues> f4 = {{100.0, 4.0, 2.0, 400.0}, {100.0, 4.0, 2.0, 200.0}, {100.0, 4.0, 2.0, 200.0}};
    P1_CHECK(compute_psfsw_weights(f4).w_psfsw[0] < compute_psfsw_weights(f4).w_psfsw[1]);

    /* 负向 mutation: 去掉组内 median 归一 (W=Wt) -> 组内 median != 1 (被 G15 检出) */
    auto wt_of = [](const ComponentValues& f) {
        return std::pow(f.s, kCompositeAlpha) * std::pow(f.conc, kCompositeBeta) /
               (std::pow(f.n, kCompositeGamma) * std::pow(f.b, kCompositeDelta));
    };
    std::vector<double> raw = {wt_of(frames[0]), wt_of(frames[1]), wt_of(frames[2])};
    P1_CHECK(std::fabs(oracle::o_median(raw) - 1.0) > kCompositeMedianRtol);

    /* 负向: C_norm 影响 W_psfsw 的伪实现可检出 (尺度简并) */
    P1_CHECK(cnorm_invariance_deviation(frames, 1e9) < 1e-9);

    /* 负向: fail-closed 顺序 (先 fail-closed 后 floor) —— B<=0 必报背景原因 */
    std::vector<ComponentValues> fb = {{100.0, 4.0, 2.0, 0.0}};
    CompositeResult rb = compute_psfsw_weights(fb);
    P1_CHECK(!rb.ok && rb.reason == PsfswReason::background_nonpositive_undefined_transform);
    P1_CHECK(std::string(rb.reject) == "background_nonpositive");
    std::vector<ComponentValues> fn = {{100.0, 4.0, 0.0, 200.0}};
    CompositeResult rn = compute_psfsw_weights(fn);
    P1_CHECK(!rn.ok && rn.reason == PsfswReason::insufficient_valid_stars);
    /* floor 取消 -> 拒绝 */
    CompositeParams nofloor;
    nofloor.floor = 0.0;
    P1_CHECK(!compute_psfsw_weights(frames, nofloor).ok);
    /* 指数非法 */
    CompositeParams bad_a;
    bad_a.alpha = 0.0;
    bad_a.beta = 0.0;
    P1_CHECK(!composite_exponents_valid(bad_a));
    CompositeParams bad_g;
    bad_g.gamma = 0.0;
    bad_g.delta = 0.0;
    P1_CHECK(!composite_exponents_valid(bad_g));
    P1_CHECK(composite_exponents_valid(CompositeParams()));
    /* C_norm 不同 -> W_psfsw 逐位/容差一致 (G16) */
    CompositeParams cn;
    cn.c_norm = 1e9;
    const CompositeResult rc = compute_psfsw_weights(frames, cn);
    for (std::size_t i = 0; i < rc.w_psfsw.size(); ++i)
        P1_CHECK_NEAR(rc.w_psfsw[i], cr.w_psfsw[i], 1e-12);
}

P1PSFW_REGISTER(common) {
    (void)mode;
    CommonStarSet set;
    set.common_star_set_id = "css-1";
    set.selection.selection_function_id = "sel-1";
    set.selection.reference_catalog_id = "gaia-dr3";
    set.selection.reference_catalog_version_hash = "hash-abc";
    set.selection.mag_min = 12.0;
    set.selection.mag_max = 18.0;
    set.selection.detection_threshold_sigma = 5.0;
    set.selection.matching_radius_arcsec = 1.0;
    set.selection.epoch_pm_handling = "propagated_to_epoch";
    set.selection.applied_at = "2026-09-15T00:00:00Z";
    set.selection.independence_proof = "external_reference_catalog";
    for (int i = 0; i < 12; ++i) {
        CommonStarMember m;
        m.star_id = 1000 + i;
        m.valid_in_frame.assign(3, 1);
        set.members.push_back(m);
    }
    const CommonStarValidation ok = validate_common_star_set(set, 3);
    P1_CHECKF("common_ok", ok.ok);
    P1_CHECK(ok.n_common == 12);
    P1_CHECK(ok.per_frame_valid.size() == 3 && ok.per_frame_valid[0] == 12);
    P1_CHECK(n_common_tier(ok.n_common) == NCommonTier::standard);

    /* 排除旗标: saturated 计入 -> n_common 降为 11 */
    CommonStarSet flagged = set;
    flagged.members[0].flags.saturated = true;
    P1_CHECK(validate_common_star_set(flagged, 3).n_common == 11);

    /* 负向: 缺 selection function -> no_common_star_set */
    CommonStarSet miss = set;
    miss.selection.selection_function_id.clear();
    CommonStarValidation vm = validate_common_star_set(miss, 3);
    P1_CHECK(!vm.ok && vm.reason == PsfswReason::no_common_star_set);
    /* 负向: 逐帧检测交集构造 -> selection_bias_gate_failed (PSFSW-G09 / m14/m25) */
    CommonStarSet per_frame = set;
    per_frame.selection.independence_proof = "per_frame_threshold_intersection";
    CommonStarValidation vp = validate_common_star_set(per_frame, 3);
    P1_CHECK(!vp.ok && vp.reason == PsfswReason::selection_bias_gate_failed);
    /* 负向: n_common<3 */
    CommonStarSet small = set;
    small.members.resize(2);
    CommonStarValidation vs = validate_common_star_set(small, 3);
    P1_CHECK(!vs.ok && vs.reason == PsfswReason::insufficient_valid_stars);
    /* 负向: 某帧有效星 <3 */
    CommonStarSet frame_short = set;
    for (auto& m : frame_short.members) m.valid_in_frame[2] = 0;
    CommonStarValidation vf = validate_common_star_set(frame_short, 3);
    P1_CHECK(!vf.ok && vf.reason == PsfswReason::insufficient_valid_stars);
}
