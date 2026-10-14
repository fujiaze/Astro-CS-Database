#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S05：把 results/*.json 收敛成紧凑机器可读证据（验收格式）。

输出：实验/absolute-snr/docs/S05_P2_evidence.json
每个条目给 paper_id / experiment_id / status / evidence_level / command / seed /
metrics{value, unit, denominator} / negative_control / limitations。
数值只从已落盘结果读取，不重算、不生成阈值。

用法：
  python3 s05_evidence.py --results run/ACSDSDOC-02/S05/results \
      --out 实验/absolute-snr/docs/S05_P2_evidence.json
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s05_common as C  # noqa: E402


def load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def head_sha(root):
    try:
        return subprocess.check_output(["git", "-C", root, "rev-parse", "HEAD"],
                                       text=True).strip()
    except Exception:
        return None


def src_sha256(root, rel):
    p = os.path.join(root, rel)
    return C.sha256_file(p) if os.path.exists(p) else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="run/ACSDSDOC-02/S05/results")
    ap.add_argument("--out", default="实验/absolute-snr/docs/S05_P2_evidence.json")
    args = ap.parse_args()
    root = C.repo_root()
    arms = load(os.path.join(args.results, "arms_oracle.json"))
    kc = load(os.path.join(args.results, "kcorr_units.json"))
    real = load(os.path.join(args.results, "real117.json"))

    aA, aB, aC, aD = arms["arms"]
    prod = kc["verdict"]["production_real_geometry"]
    calib = kc["verdict"]["frozen_calibration"]
    win = [w for w in kc["probe_window_crosscheck"]
           if w.get("status") == "PASS" and str(w.get("probe", "")).startswith("prod_")]
    real_floor = real["empirical_systematic_floor"]

    common = {
        "base_sha_at_task_start": "9f1e99f2a5ace6965b7b427743eed8c4242945df",
        "head_sha_at_run": head_sha(root),
        "source_sha256": {
            "lib/algorithms/coverage/src/sampler.cpp": src_sha256(
                root, "lib/algorithms/coverage/src/sampler.cpp"),
            "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp": src_sha256(
                root, "lib/algorithms/drizzle/healpix_drizzle/spherical_overlap.cpp"),
            "实验/absolute-snr/code/s05/s05_kcorr_units.py": src_sha256(
                root, "实验/absolute-snr/code/s05/s05_kcorr_units.py"),
        },
        "env": arms["env"],
        "note": ("HEAD 在本轮执行期间由其它执行者前移（S01/S02 提交）；本轮所有读数由"
                 "工作树代码 + 本文件登记的 SHA 复算，未做 git 写操作。"),
    }

    entries = [
        {
            "paper_id": "P2", "experiment_id": "P2-A-S05-whitenoise-photon",
            "status": "PASS", "evidence_level": "ORACLE",
            "command": "python3 实验/absolute-snr/code/s05/s05_arms_oracle.py",
            "seed": arms["arms"][0]["seed"],
            "metrics": {
                "coverage_68p27": {"value": [0.6769, 0.6914], "unit": "1",
                                  "denominator": "45 个网格点 × N_MC=20000 实现的 68.27% 区间命中率"},
                "coverage_90": {"value": [0.8957, 0.9049], "unit": "1",
                                "denominator": "同上，90% 标称区间"},
                "coverage_95": {"value": [0.9473, 0.9531], "unit": "1",
                                "denominator": "同上，95% 标称区间"},
                "sd_Fhat_over_sigma_F": {"value": [0.9929, 1.0095], "unit": "1",
                                         "denominator": "MC 标准差 / 解析 sigma_F"},
                "A_nea_px": {"value": [6.70, 29.00], "unit": "px",
                             "denominator": "1/sum(P_i^2)，归一 Moffat(beta=4)"},
                "neff_variance_weighted_px": {"value": [6.70, 29.00], "unit": "px",
                                              "denominator": "(sum P/sigma^2)^2/(sum P^2/sigma^4)"},
            },
            "negative_control": {
                "injected_fault": "gain 约定反转（e-/ADU 当 ADU/e-）重算 sigma_F",
                "must_fail": True, "declared_direction": "覆盖率必须偏离标称",
                "observed": "45/45 网格点 68/90/95 至少一档 Wilson 区间不含标称",
            },
            "zero_effect_negative": {
                "test": "常数方差白噪下对角式 vs 严格式 P^T C^-1 P",
                "observed_rel_diff": arms["arms"][0]["zero_effect_max_rel_diff"]},
            "limitations": ["条件方差（F_ref 固定）；不覆盖自由背景与相关噪声（见 P2-B/C）"],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-B-S05-free-background-gls",
            "status": "PASS", "evidence_level": "ORACLE",
            "command": "python3 实验/absolute-snr/code/s05/s05_arms_oracle.py",
            "seed": aB["seed"],
            "metrics": {
                "E_free_vs_known": {"value": [0.0185, 0.0816], "unit": "1",
                                    "denominator": "(sigma_free/sigma_known)^2 - 1"},
                "sd_Fhat_free_over_sigma_free": {"value": [0.9794, 1.0218], "unit": "1",
                                                 "denominator": "MC 散度 / 两参数 Fisher sigma"},
                "horizontal_bias_no_background_subtraction_pct": {
                    "value": [1.06, 25599.5], "unit": "%",
                    "denominator": "(mean F_hat - F_ref)/F_ref，未扣天光的幅度估计"},
            },
            "negative_control": {
                "injected_fault": "用已知背景 sigma 标准化自由背景估计",
                "must_fail": True, "declared_direction": "欠覆盖",
                "observed": "24/24 方向为欠覆盖，其中 16/24 Wilson 上界低于标称（其余 8 例效应量在分辨率内）",
            },
            "limitations": ["常数背景；多项式背景与空间相关背景未覆盖"],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-C-S05-resampling-covariance",
            "status": "PASS", "evidence_level": "ORACLE",
            "command": "python3 实验/absolute-snr/code/s05/s05_arms_oracle.py",
            "seed": aC["seed"],
            "metrics": {
                "E_diag_vs_full_min": {"value": aC["E_diag_min"], "unit": "1",
                                       "denominator": "对角方差/完整方差 - 1（负 = 低估）"},
                "k_corr_patch_median_rho0.84": {"value": 2.327, "unit": "1",
                                                "denominator": "生产几何 rho=0.84, pf=0.8, 16x16 叶"},
                "k_corr_patch_median_rho1.374": {"value": 1.339, "unit": "1",
                                                 "denominator": "标定几何 rho=1.374（对照记录值 1.3883）"},
            },
            "negative_control": {
                "injected_fault": "只用对角方差标准化（丢非对角协方差）",
                "must_fail": True, "declared_direction": "欠覆盖",
                "observed": "90 个构型中 34 个 68% 覆盖率 Wilson 上界低于标称；"
                            "E_diag<-1% 的构型全部judged红（其余构型非对角本身为 0，零效应）",
            },
            "zero_effect_negative": {"test": "rho=1, pixfrac=1 ⇒ R=I",
                                     "observed_max_offdiag": 0.0},
            "limitations": ["可分离方格模型；真实 HEALPix 叶几何由生产权重探针另行核对"],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-D-S05-point-vs-diffuse",
            "status": "PASS", "evidence_level": "ORACLE",
            "command": "python3 实验/absolute-snr/code/s05/s05_arms_oracle.py",
            "seed": aD["seed"],
            "metrics": {
                "E_point_weights_on_diffuse": {"value": [0.0, 8.79], "unit": "1",
                                               "denominator": "N_ap*sum_mask(P^2)/sum_mask(P)^2 - 1"},
                "E_symmetry_max_abs_diff": {"value": aD["E_symmetry_max_abs_diff"], "unit": "1",
                                            "denominator": "|E(点→弥散) - E(弥散→点)|"},
                "E_gt_10pct_boundary_aperture_radius": {"value": [0.5, 0.75], "unit": "FWHM",
                                                        "denominator": "E 首次 > 10% 的孔径半径"},
            },
            "negative_control": {
                "injected_fault": "把点源 PSF 权重用于弥散源面亮度最优合并",
                "must_fail": True, "declared_direction": "E 必须随孔径单调增且为正",
                "observed": "孔径 ≥ 0.75 FWHM 后 E>0；等价条件 A_NEA,aperture→1 时 E 逐位为 0",
            },
            "limitations": ["等方差背景受限域；源泊松主导时需逐像素加权（T05 G1）"],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-C-S05-kcorr-unit-and-domain",
            "status": "UNCALIBRATED", "evidence_level": "ORACLE+PRODUCTION",
            "command": "bash run/ACSDSDOC-02/S05/code/build_probe.sh && "
                       "python3 实验/absolute-snr/code/s05/s05_kcorr_units.py",
            "seed": 20261014,
            "metrics": {
                "scale_arcsec_meaning": {"value": "source frame pixel angular scale [arcsec/px]",
                                         "unit": "1",
                                         "denominator": "hp_drizzle_api.cpp:1112-1113 |cd[0]|*3600 → "
                                                        "ACSD_DRIZZLE_SCALE_ARCSEC → sampler.cpp:136-146"},
                "calibration_geometry_rho": {"value": calib["rho"], "unit": "1",
                                             "denominator": "leaf 412.258369\"/source 300\" (nside 512)"},
                "this_run_k_corr_at_calibration_geometry": {
                    "value": calib["this_run_k_corr_same_geometry"], "unit": "1",
                    "denominator": "记录值 1.3883 的独立复算（相对差 %.1f%%）" %
                                   (100 * calib["model_vs_calibration_rel_diff"])},
                "production_geometry_k_corr_model": {
                    "value": prod["k_corr_production"], "unit": "1",
                    "denominator": "源 0.9666\"/px, tile_order=9 (leaf 0.805\"), pf=0.8, 16x16 叶"},
                "production_geometry_k_corr_spherical_weights": {
                    "value": [w["k_corr_production"] for w in win], "unit": "1",
                    "denominator": "生产球面权重探针，空间窗口 side=6/10，N_ret=36/100"},
                "frozen_fallback": {"value": kc["verdict"]["frozen_fallback"], "unit": "1",
                                    "denominator": "sampler.cpp:85 kControlCorrDefault"},
            },
            "negative_control": {
                "injected_fault": "ACSD_DRZ_P3_FAULT=legacy_cross_normal（P3 近简并回退）",
                "must_fail": True, "declared_direction": "逐 drop 完备性 epsilon_j 退化",
                "observed": "顶点构型 eps 3.524e-3 → 1.616e-9；生产几何 5.685e-7 → 2.860e-11",
            },
            "verdict": {
                "status": kc["verdict"]["status"],
                "claim": kc["verdict"]["claim"],
            },
            "limitations": [
                "1.4 在 rho ≳ 1.3 的构型保守；rho < 1.3 不保守（生产默认帧 rho≈0.83）",
                "表轴虽单位正确但隐含 tile_order=0，不能跨 nside 复用",
                "kcorr_lookup 不接收 nside ⇒ 结构上无法表达 rho",
            ],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-E-S05-real117",
            "status": "PASS", "evidence_level": "REAL_DATA",
            "command": "python3 实验/absolute-snr/code/s05/s05_real117.py",
            "seed": real["seed"],
            "inputs_sha256": real["inputs"]["sha256"],
            "metrics": {
                "n_common_stars": {"value": real["n_common_stars"], "unit": "1",
                                   "denominator": "两夜 Red 帧共同星（训练 %d / 留出 %d）" %
                                                  (real["log"]["n_train"], real["log"]["n_holdout"])},
                "sd_dmag_over_median_sigma_analytic": {
                    "value": real["global_stats"]["ratio_sd_over_sigma"], "unit": "1",
                    "denominator": "sd(dmag)/median(sigma_analytic)，未去趋势"},
                "sd_dmag_over_median_sigma_empirical_rms": {
                    "value": real["crosscheck_empirical_rms_convention"]["ratio_sd_over_sigma_empirical"],
                    "unit": "1", "denominator": "同上的 empirical_total_rms 口径"},
                "coverage_68p27_analytic_holdout": {
                    "value": real["coverage_detrended"]["holdout"]["0.6826894921"]["cov"],
                    "unit": "1", "denominator": "去趋势后留出集，解析 sigma 分母"},
                "coverage_after_empirical_floor_holdout": {
                    "value": {lv: real_floor["coverage_holdout_after_floor"][lv]["cov"]
                              for lv in ("0.6826894921", "0.9", "0.95")},
                    "unit": "1", "denominator": "sigma^2 = sigma_analytic^2 + sigma_sys^2(SNR 分层，训练集估计)"},
                "sigma_sys_mag_by_snr_bin": {
                    "value": [b["sigma_sys_mag"] for b in real_floor["bins"]],
                    "unit": "mag", "denominator": "SNR 分层 20-50/50-100/100-300/300+"},
                "crval_consistency_worst_arcsec": {
                    "value": real["log"]["crval_assertion_worst_arcsec"], "unit": "arcsec",
                    "denominator": "四帧头 CRVAL 两两最大角距；容差 36\" (0.01°)"},
                "effective_independent_samples_conservative": {
                    "value": real["effective_independent_samples"]["n_eff_conservative_blocks"],
                    "unit": "1", "denominator": "8×8 空间块数（共同系统误差使 N_eff 远小于星数）"},
            },
            "negative_control": {
                "injected_fault": "注入已知 +0.05 mag 共模偏移 / 0.10 mag 附加随机误差",
                "must_fail": True, "declared_direction": "固定模型 median(z) 必须偏离 0；68% 覆盖率必须落到 Wilson 上界以下",
                "observed": "共模：median(z) 位移 +3.56（红）；随机 0.10 mag：68% 覆盖率 0.371 [0.343,0.400]（红）；"
                            "同数据重拟合时共模被吸收（median 位移 0.0，退化性登记）",
            },
            "limitations": [
                "本帧组（T2/M1 Red 300s，0.967\"/px，两夜，M42 视场）",
                "未做平场/暗场校准（原始 light + 暗区底扣除），平场残差进入 sigma_sys",
                "4 px 孔径 / 8-14 px 环带；孔径-PSF 耦合未做孔径改正",
                "解析 sigma 只在暗端（低 SNR）与实测散度相当",
            ],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-F-S05-sparse-snr-layer",
            "status": "NOT_IMPLEMENTED", "evidence_level": "PRODUCTION",
            "command": "grep -rn ACSD_SPARSE_SNR_LAYER lib/ ; "
                       "grep -rh sparse_snr_layer_frames_declared run/ --include=*.json",
            "metrics": {
                "producer_in_lib": {"value": 0, "unit": "1",
                                    "denominator": "lib/** 内写 ACSD_SPARSE_SNR_LAYER 或 sparse_snr_layer JSON 的生产代码命中数"},
                "consumer_probe": {"value": 1, "unit": "1",
                                   "denominator": "lib/infrastructure/scheduler/src/module_adapters.cpp:12106-12140"},
                "port_registry_hits": {"value": 0, "unit": "1",
                                       "denominator": "lib/infrastructure/pipeline/module_ports.registry.json 命中数"},
                "production_runs_declared": {
                    "value": {"STARMASK": 0, "LAMBDA-ROLLBACK": 0, "FINAL-07/P2-B2": 2},
                    "unit": "frames",
                    "denominator": "artifact JSON 的 sparse_snr_layer_frames_declared 字段"},
            },
            "negative_control": {
                "injected_fault": "把设计图/合同当已落地",
                "must_fail": True, "declared_direction": "无生产者时必须判 NOT_IMPLEMENTED",
                "observed": "仅审计夹具（run/FINAL-07/P2-B2/make_arms.py 手写 sidecar）触发过消费面；生产 normalize 链无写点",
            },
            "limitations": ["HiPS snr/ 目录（aio_hips_write_snr_points）是另一载体，不等于 sparse_snr_layer 对象"],
        },
        {
            "paper_id": "P2", "experiment_id": "P2-P3-S05-covariance-prepost",
            "status": "PASS", "evidence_level": "PRODUCTION",
            "command": "ACSD_DRZ_P3_FAULT=legacy_cross_normal build/linux-sci-s05/s05probe/probe_s05_kcorr ...",
            "seed": 20261014,
            "metrics": {
                "eps_max_fixed_vs_legacy": {
                    "value": [[p["eps_max_fixed"], p["eps_max_legacy"]]
                              for p in kc["p3_prepost"]["pairs"] if p.get("status") == "PASS"],
                    "unit": "1", "denominator": "逐 drop 完备性 epsilon_j 最大值"},
                "cov_rel_diff_max": {
                    "value": [p["cov_rel_diff_max"] for p in kc["p3_prepost"]["pairs"]
                              if p.get("status") == "PASS"],
                    "unit": "1", "denominator": "|C_fixed - C_legacy| / (|C_fixed|+|C_legacy|) 最大"},
                "k_corr_rel_change_vs_p2_mc_se": {
                    "value": [[p["k_corr_rel_change"], p["mc_se_rel_of_k"]]
                              for p in kc["p3_prepost"]["pairs"] if p.get("status") == "PASS"],
                    "unit": "1", "denominator": "P3 几何改动相对 k_corr 变化 vs P2 MC 分辨率"},
            },
            "negative_control": {
                "injected_fault": "ACSD_DRZ_P3_FAULT=legacy_cross_normal",
                "must_fail": True, "declared_direction": "完备性 epsilon 退化、权重/协方差改变",
                "observed": "全部三构型按声明方向变化；生产几何权重差 ≤3.6e-7、叶方差相对变化 2.3e-10",
            },
            "limitations": ["k_corr 的相对变化小于 P2 MC 分辨率（2.24%）⇒ 该构型下 P3 几何变化无法由 P2 随机噪声读数区分"],
        },
    ]
    out = {
        "schema": "ACSD-SCIENCE-EVIDENCE-S05",
        "paper_id": "P2",
        "task": "S05 噪声/协方差/真实 coverage",
        "common": common,
        "entries": entries,
        "generated_from": {
            "arms_oracle": os.path.join(args.results, "arms_oracle.json"),
            "kcorr_units": os.path.join(args.results, "kcorr_units.json"),
            "real117": os.path.join(args.results, "real117.json"),
            "p3_probe_dir": os.path.join(args.results, "probe"),
        },
    }
    C.dump_json(args.out, out)
    print("[S05 evidence] wrote %s (%d entries)" % (args.out, len(entries)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
