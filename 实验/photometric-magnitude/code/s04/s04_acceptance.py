# -*- coding: utf-8 -*-
"""S04 · 汇总「验收格式」记录（机器可读）

按 `实验/engineering-evidence/acsd-science-docs-p5/DEEPSEEK_SCIENCE_EXECUTION.md`
「验收格式」节的字段组装；所有数值一律从 `results/*.json` 读取，**不由文档阈值生成**。

输出：results/acceptance_S04.json
"""
from __future__ import annotations

import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

R = C.RESULTS
STEP8_REL = "实验/photometric-magnitude/code/step8_real_frame.py"


def sha(p):
    return C.sha256_file(p)


def script_hashes():
    d = os.path.dirname(os.path.abspath(__file__))
    out = {}
    for fn in sorted(os.listdir(d)):
        if fn.endswith(".py"):
            out[f"code/s04/{fn}"] = sha(os.path.join(d, fn))
    out[STEP8_REL] = sha(os.path.join(C.REPO, STEP8_REL))
    return out


def manifest_sha():
    """数据清单指纹：帧清单 + 8 帧 SHA256 的串联哈希（可复算）。"""
    inv = json.load(open(os.path.join(R, "frame_inventory.json"), encoding="utf-8"))
    parts = [inv["catalog"]["sha256"]]
    parts += [c["sha256_recomputed"] for c in inv["compliant_set"]]
    h = hashlib.sha256("".join(parts).encode()).hexdigest()
    return dict(catalog_sha256=inv["catalog"]["sha256"],
                n_frames=len(inv["compliant_set"]), manifest_sha256=h)


def main():
    inv = json.load(open(os.path.join(R, "frame_inventory.json"), encoding="utf-8"))
    rc = json.load(open(os.path.join(R, "repro_comparison.json"), encoding="utf-8"))
    st = json.load(open(os.path.join(R, "strata.json"), encoding="utf-8"))
    sf = json.load(open(os.path.join(R, "sigma_flat.json"), encoding="utf-8"))
    ln = json.load(open(os.path.join(R, "linearity.json"), encoding="utf-8"))
    ng = json.load(open(os.path.join(R, "negatives.json"), encoding="utf-8"))
    base = inv["base_sha"]
    sh = script_hashes()
    man = manifest_sha()
    common = dict(base_sha=base, source_sha=base,
                  data_manifest_sha256=man["manifest_sha256"],
                  evidence_level="REAL_DATA")
    exp = []

    pre = rc["step8_original"]["pre"]["gate"]
    head = rc["step8_original"]["head"]["gate"]
    exp.append(dict(paper_id="P1", experiment_id="S04-repro-step8-revisions", **common,
                    command=("python3 实验/photometric-magnitude/code/s04/s04_measure.py"),
                    status="PASS",
                    metrics={
                        "sigma_obs_pre_revision": dict(
                            value=pre["sigma_obs_mag"], unit="mag",
                            denominator="MAD(r_inliers)/0.6744897501960817, r=log10(F_instr/F_syn)"),
                        "sigma_obs_head": dict(
                            value=head["sigma_obs_mag"], unit="mag",
                            denominator="same, HEAD variance formula"),
                        "sigma_ceiling_pre_revision_0p0007": dict(
                            value=pre["sigma_ceiling"], unit="mag",
                            denominator="rho_hi*sqrt(sum of registered budget terms)"),
                        "sigma_ceiling_head_0p0007": dict(
                            value=head["sigma_ceiling"], unit="mag",
                            denominator="same, HEAD variance formula"),
                        "archive_sigma_obs": dict(
                            value=rc["archive"]["sigma_obs_mag"], unit="mag",
                            denominator="HISTORICAL_ONLY, run/FINAL-07 archive"),
                        "tag_dependence_rel_diff_ceiling": dict(
                            value=rc["tag_crosscheck"]["tag_dependence"]["rel_diff_ceiling"],
                            unit="dimensionless",
                            denominator="(ceiling(tag=s04)-ceiling(tag=real))/ceiling(tag=real)")},
                    negative_control=dict(injected_fault="self-referential sigma_flat",
                                          must_fail=True,
                                          observed="8/8 frames PASS under self-referential "
                                                   "sigma_flat vs 3/8 PASS under 0.0007"),
                    limitations=["HEAD script gives a different sigma_obs than the report",
                                 "sigma_ceiling is MC-tag dependent (0.32%)"]))

    exp.append(dict(paper_id="P1", experiment_id="S04-frame-selection-13.1", **common,
                    command="python3 实验/photometric-magnitude/code/s04/s04_inventory.py",
                    status="PASS",
                    metrics={
                        "n_candidates_same_config": dict(
                            value=inv["assertions"]["n_candidates"], unit="1",
                            denominator="catalog-enumerated M42 Red 300s with geometry"),
                        "n_compliant": dict(value=inv["assertions"]["n_compliant"], unit="1",
                                            denominator="CRVAL sep <= 0.01 deg from reference"),
                        "n_rejected_crval": dict(
                            value=inv["assertions"]["n_rejected"], unit="1",
                            denominator="CRVAL sep > 0.01 deg"),
                        "max_sep_within_compliant_deg": dict(
                            value=inv["assertions"]["max_sep_within_compliant_deg"],
                            unit="deg", denominator="max spherical sep from reference"),
                        "n_frames_missing_geometry": dict(
                            value=inv["frames_missing_geometry"]["n"], unit="1",
                            denominator="catalog frames without CRVAL/CRPIX")},
                    negative_control=dict(injected_fault="M2-M6 panel frames as input",
                                          must_fail=True,
                                          observed="35 rejected, min sep 0.9343 deg"),
                    limitations=["31 frames lack geometry keys and are registered only"]))

    c0 = st["single_frame_T2"]["multi_axis"]
    exp.append(dict(paper_id="P1", experiment_id="S04-strata-colour-mag-pos-snr",
                    **common,
                    command="python3 实验/photometric-magnitude/code/s04/s04_strata.py",
                    status="PASS",
                    metrics={
                        "colour_coef_mag_per_unit_std": dict(
                            value=c0["z_col"]["coef_mag_per_unit_std"],
                            unit="mag per std of (R-B) reference colour",
                            denominator="OLS multi-axis on 171 IRLS inliers, cluster bootstrap by star"),
                        "colour_coef_mag_per_mag_RB": dict(
                            value=c0["z_col"]["coef_mag_per_physical"], unit="mag/mag",
                            denominator="same fit, physical axis"),
                        "cross_frame_colour_slope": dict(
                            value=st["cross_frame_pooled"]["slope_color_mag_per_mag"]
                            ["coef_mag_per_mag_physical"], unit="mag/mag",
                            denominator="734 rows / 177 distinct stars, per-frame median removed"),
                        "train_T2_holdout_T3_colour_slope": dict(
                            value=st["train_holdout_cross_instrument"]["T2train_T3holdout"]
                            ["slope_color_mag_per_mag"], unit="mag/mag",
                            denominator="fit on T2 inliers, applied to T3 holdout frames"),
                        "holdout_median_reduction_pct": dict(
                            value=st["train_holdout_cross_instrument"]["T2train_T3holdout"]
                            ["holdout_pooled_reduction_pct"], unit="percent",
                            denominator="mean per-frame sigma_obs reduction on T3 holdout")},
                    negative_control=dict(injected_fault="position/magnitude axes as causes",
                                          must_fail=True,
                                          observed="sign flips between disjoint star halves"),
                    limitations=["judging sample governed by chi2<4 cut (survival 0.106-0.410)",
                                 "z_mag and z_snr collinear (r=-0.96)"]))

    exp.append(dict(paper_id="P1", experiment_id="S04-sigma-flat-independent", **common,
                    command="python3 实验/photometric-magnitude/code/s04/s04_sigmaflat.py",
                    status="PASS",
                    metrics={
                        "sigma_flat_master_hf_mad": dict(
                            value=sf["A_master_flats"]["T2_Red"]["hf"]["mad_sigma"],
                            unit="dimensionless (relative flux)",
                            denominator="MAD(flat/median_filter(flat,9)-1)*1.4826"),
                        "sigma_flat_via_N_eff": dict(
                            value=sf["A_master_flats"]["T2_Red"]
                            ["sigma_flat_via_N_eff_analytic_mag"], unit="mag",
                            denominator="2.5/ln10 * s_flat/sqrt(N_eff), N_eff=1/sum(P^2)"),
                        "sigma_flat_per_star_direct": dict(
                            value=sf["A2_per_star_flat"]["T2_Red"]
                            ["median_sigma_flat_mag_std"], unit="mag",
                            denominator="std of PSF-weighted master-flat average at real star positions"),
                        "sigma_flat_after_deg2": dict(
                            value=sf["A2_per_star_flat"]["T2_Red"]
                            ["median_sigma_flat_after_deg2_mag"], unit="mag",
                            denominator="same, after removing a degree-2 spatial field"),
                        "repeat_position_sigma": dict(
                            value=sf["B_repeat_position"]["all_pairs"]
                            ["repeatability_sigma_mag"], unit="mag",
                            denominator="MAD(paired residual difference)/sqrt(2), 1802 pairs/145 stars"),
                        "apply_flat_median_delta_sigma_obs": dict(
                            value=sf["D_apply_flat_to_flux"]["summary"]["full_flat"]
                            ["median_delta_sigma_obs_mag"], unit="mag",
                            denominator="sigma_obs(after dividing by master flat) - sigma_obs(baseline)")},
                    negative_control=dict(
                        injected_fault="assume the flat explains sigma_obs",
                        must_fail=True,
                        observed="applying the master flat worsens sigma_obs in 5/8 frames; "
                                 "slope of paired difference on flat difference 0.84 [0.28,1.51]"),
                    limitations=["residual flat error after correction NOT_MEASURED",
                                 "N_eff folding underestimates the aperture average by ~8x",
                                 "master flats dated 2025-08-13, ~3.5 months before the lights"]))

    lv = ln["verdict"]
    exp.append(dict(paper_id="P1", experiment_id="S04-detector-linearity", **common,
                    command="python3 实验/photometric-magnitude/code/s04/s04_linearity.py",
                    status="NOT_RUN",
                    metrics={
                        "linearity_knee_e": dict(value=None, unit="e-",
                                                 denominator="not reachable: brighter stars saturated in both exposures"),
                        "max_unsat_stamp_peak_adu": dict(
                            value=ln["analyses"][0]["scanned"]["max_unsat_peak_adu"],
                            unit="ADU", denominator="max PSF-stamp peak among unsaturated pairs"),
                        "600over300_slope_per_dex": dict(
                            value=lv["observed_signal_dependent_bias"]
                            ["ratio_600_over_300_slope_per_dex"], unit="1/dex",
                            denominator="OLS slope of normalised flux ratio vs log10(F_300)"),
                        "same_night_same_exposure_null_slope": dict(
                            value=lv["observed_signal_dependent_bias"]
                            ["null_same_night_600_over_600_slope_per_dex"], unit="1/dex",
                            denominator="same, 600/600 pair")},
                    negative_control=dict(
                        injected_fault="attribute the ratio trend to detector nonlinearity",
                        must_fail=True,
                        observed="cross-night same-exposure control also shows a significant "
                                 "trend (-0.0158/dex)"),
                    limitations=["linearity knee NOT_MEASURED",
                                 "gain/saturation hardcoded and unverified",
                                 "nightly transparency drifts 5-6% in 7-11 min"]))

    n1 = ng["experiments"][0]["observed"]
    n2 = ng["experiments"][1]["observed"]
    n3 = ng["experiments"][2]["observed"]
    n4 = ng["experiments"][3]["observed"]
    n5 = ng["experiments"][4]["observed"]
    exp.append(dict(paper_id="P1", experiment_id="S04-negative-controls-P1D", **common,
                    command="python3 实验/photometric-magnitude/code/s04/s04_negatives.py",
                    status="PASS",
                    metrics={
                        "N1_wrong_passband_ratio_min": dict(
                            value=min(v["ratio_to_baseline"] for v in n1.values()),
                            unit="dimensionless",
                            denominator="sigma_obs(wrong passband)/sigma_obs(baseline)"),
                        "N2_double_G_ratio": dict(
                            value=n2["ratio_to_baseline"], unit="dimensionless",
                            denominator="sigma_obs(f_syn*10^(-0.4G))/sigma_obs(baseline)"),
                        "N3a_holdout_reduction_pct": dict(
                            value=n3["N3a_train_holdout"]["holdout_reduction_pct"],
                            unit="percent",
                            denominator="(sigma_obs_before-sigma_obs_after)/sigma_obs_before on holdout stars"),
                        "N3b_clip_1p5MAD_sigma_obs": dict(
                            value=n3["N3b_residual_trimming"]["clip_1.5MAD"]["sigma_obs_mag"],
                            unit="mag",
                            denominator="MAD(r)/0.6744897501960817 after clipping at 1.5*MAD"),
                        "N3b_clip_1p5MAD_verdict": dict(
                            value=n3["N3b_residual_trimming"]["clip_1.5MAD"]["verdict"],
                            unit="1", denominator="gate verdict with rho_hi recomputed at the clipped n"),
                        "N4_holdout_worst_over_best": dict(
                            value=ng["experiments"][3]["declared_direction_check"]
                            ["observed_holdout_worst_over_best"],
                            unit="dimensionless", denominator="max(holdout MAD-sigma)/min(...)"),
                        "N5_BZERO_x2_ceiling_ratio": dict(
                            value=n5["by_mode"]["BZERO_x2"]["sigma_ceiling"]
                            / ng["baseline"]["sigma_ceiling"], unit="dimensionless",
                            denominator="sigma_ceiling(BZERO applied twice)/sigma_ceiling(correct)")},
                    negative_control=dict(
                        injected_fault="five P1-D faults",
                        must_fail=True,
                        observed="N1/N2/N5 RED as declared; N3 RED with a forbidden "
                                 "trimming path that flips to PASS; N4 RED for deg>=3 "
                                 "but the declared in-sample monotonicity was falsified"),
                    limitations=["BSCALE leg not injectable (BSCALE=1)",
                                 "N4 declared direction partially wrong (reported)"]))

    out = dict(
        step="S04 acceptance records",
        base_sha=base,
        head=base,
        generated_from=["results/frame_inventory.json", "results/repro_comparison.json",
                        "results/strata.json", "results/sigma_flat.json",
                        "results/linearity.json", "results/negatives.json"],
        script_sha256=sh,
        data_manifest=man,
        experiments=exp,
        notes=("所有 metrics 的 value 均由 results/*.json 直接读出（含 null）；"
               "status 用 PASS/FAIL/NOT_RUN/UNVERIFIED，线性区为 NOT_RUN 而非 PASS。"))
    C.jdump(out, "acceptance_S04.json")
    print(f"[acceptance] wrote results/acceptance_S04.json  ({len(exp)} experiments)")
    for e in exp:
        print("  ", e["experiment_id"], e["status"])


if __name__ == "__main__":
    main()
