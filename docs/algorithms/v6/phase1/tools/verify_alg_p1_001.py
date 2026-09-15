#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ALG-P1-001 独立 Oracle + 结构一致性检查 + 负向 mutation 门。

任务：工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/ALG-P1-001.md
写域：docs/algorithms/v6/phase1/（本脚本是该任务范围内的可复跑验证器）

验证三层：
  1. 结构检查：从 alg_p1_001_spec.json 核对冻结继承集、单位律、禁止 token、
     四个算法域、冻结门、生产模式枚举、psfsw 边界、父级方差与 provenance。
  2. 独立 Oracle：纯 numpy 按冻结公式独立重推导（不 import/不调用任何生产实现
     或生产测试二进制），覆盖 Drizzle(SB/方差/通量/相关)、校准 covariance
     (J C J^T / 共享 master)、W_info(A_NEA / P^T C^-1 P / GLS)、PSFSW 复合。
  3. 负向 mutation：每个 mutation 注入一处错误，其目标门必须变红；
     注入仍绿 = MISSED = 失败（先证伪门再声明门有效）。

退出码（与 Wave-1 兄弟任务一致）：
  0  = 基线全绿（--selftest）或全部 mutation 均 CAUGHT（--all-mutations）
  1  = 基线有红，或存在 MISSED mutation
  2  = 单 mutation 注入被目标门捕获（--mutation ID，负向门为红）
"""
import argparse
import copy
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SCOPE = os.path.dirname(HERE)  # docs/algorithms/v6/phase1

REQUIRED_FREEZE_IDS = [
    "FZ-UNIT-WINFO", "FZ-UNIT-PSFSW", "FZ-BUNIT-SEMANTICS", "FZ-FORMULA-DRIZZLE-SB",
    "FZ-GATE-CONST-SB", "FZ-FORMULA-WINFO", "FZ-FORMULA-GLS", "FZ-GATE-PIXIVAR-APPROX",
    "FZ-FORMULA-COV-PROP", "FZ-FIELD-PSFSW-4COMP", "FZ-GATE-PSFSW-COV", "FZ-GATE-PSFSW-EPSF",
    "FZ-MODE-PRODUCTION", "FZ-MODE-DEFERRED", "FZ-GATE-MEDIAN-SNR", "FZ-P3-QW-RECOMPUTE",
    "FZ-P3-FAILCLOSED", "FZ-DEGRADE-SCALAR", "FZ-PROV-KCORR",
]
PRODUCTION_MODES_EXPECTED = {"point_information", "surface_gls", "psfsw_robust"}
FORBIDDEN_PRODUCTION_VALUES = {"psf_snr_power", "0", "auto", "support_x_snr2"}
PSFSW_KEYS_BANNED = {"ivar", "variance", "var", "sigma", "sigma2", "inverse_variance",
                     "fisher", "information", "w_info", "w_psf"}


def load_json(name):
    with open(os.path.join(SCOPE, name), encoding="utf-8") as f:
        return json.load(f)


# ============================================================ structure checks

def chk_freeze_inherit(subj):
    inherited = set(subj["spec"].get("inherited_frozen_ids", []))
    return all(fz in inherited for fz in REQUIRED_FREEZE_IDS), "required 19 freeze ids all inherited"


def chk_unit_law(subj):
    u = subj["spec"]["units_table"]
    ok = (u.get("signal_sb") == "ADU/px^2" and u.get("sb_variance_out") == "ADU^2/px^4"
          and u.get("sb_ivar_out") == "px^4/ADU^2" and u.get("pixel_variance_in") == "ADU^2"
          and u.get("W_info") == "ADU^-2" and u.get("psfsw_robust_weight") == "1")
    return ok, "variance=signal^2, ivar=1/variance, W_info=ADU^-2, psfsw=1"


def chk_forbidden_tokens(subj):
    toks = set(subj["spec"].get("forbidden_weight_source_tokens", []))
    need = {"median_source_snr", "support", "coverage", "fwhm", "residual",
            "psfsw_robust_weight", "psfsw"}
    return need.issubset(toks) and not (FORBIDDEN_PRODUCTION_VALUES & toks), "forbidden token set complete"


def chk_alg_areas(subj):
    need = {"calibration_covariance", "psf_information", "psfsw_components", "drizzle"}
    return need.issubset(set(subj["spec"].get("algorithms", {}))), "four algorithm areas present"


def chk_gates(subj):
    gids = {g["id"] for g in subj["spec"].get("gates", [])}
    need = {"FZ-GATE-CONST-SB", "FZ-FORMULA-COV-PROP", "FZ-GATE-PSFSW-FAILCLOSED"}
    return need.issubset(gids), "frozen gates present in gates set"


def chk_mode_production(subj):
    spec = subj["spec"]
    prod = set(spec.get("production_weight_modes", []))
    defer = set(spec.get("deferred_modes", []))
    fb = set(spec.get("forbidden_production_mode_values", []))
    return (prod == PRODUCTION_MODES_EXPECTED and "psf_snr_power" in defer
            and "psf_snr_power" not in prod and fb == FORBIDDEN_PRODUCTION_VALUES), \
        "production 3 modes; psf_snr_power deferred; legacy/auto forbidden"


def chk_cal_representation(subj):
    reps = subj["spec"]["algorithms"]["calibration_covariance"]["shared_terms"]["allowed_representations"]
    return {"lowrank", "correlation_kernel", "common_master_id"}.issubset({r["kind"] for r in reps}), \
        "shared master: 3 allowed representations"


def chk_cal_no_clip(subj):
    neg = subj["spec"]["algorithms"]["calibration_covariance"]["linearization"].get("negative_values", "")
    return ("不得裁切负值" in neg and "pedestal" in neg), "no clipping / no pedestal"


def chk_cal_shared_gate(subj):
    txt = subj["spec"]["algorithms"]["calibration_covariance"]["shared_terms"].get("gate", "")
    return ("> 1" in txt and "REJECT" in txt), "shared-vs-naive ratio>1 detection declared"


def chk_winfo_diag_approx(subj):
    disc = subj["spec"]["algorithms"]["psf_information"]["discrete"].get("approximate_diagonal_C", "")
    return ("报告" in disc and "偏差" in disc), "approx diagonal must report c~^T C c~ and deviation"


def chk_winfo_forbidden(subj):
    fb = set(subj["spec"]["algorithms"]["psf_information"].get("forbidden_sources", []))
    need = {"median_source_snr", "support", "coverage", "fwhm", "residual", "drizzle_pixel_ivar_only"}
    return need.issubset(fb), "W_info forbidden sources complete"


def chk_psfsw_4comp(subj):
    comp = subj["spec"]["algorithms"]["psfsw_components"]
    names = {c["name"] for c in comp["components"]}
    need = {"psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"}
    s = comp.get("spatial_summary", "")
    return (need.issubset(names) and "measurement_id" in s
            and "p05<=p50<=p95" in s and "valid_area_fraction" in s), "four distinct components + spatial summary"


def chk_psfsw_unit_scope(subj):
    comp = subj["spec"]["algorithms"]["psfsw_components"]
    covb = comp.get("covariance_boundary", {})
    return ("median = 1" in comp["composite"].get("group_normalization", "")
            and "scope=group" in comp["composite"].get("phase1_interface", "")
            and covb.get("method") == "propagated_from_composite_coefficients"
            and covb.get("variance_from_weight") is False
            and covb.get("uses_relative_weight_as_ivar") is False), \
        "psfsw unit/scope/covariance boundary frozen"


def chk_psfsw_epsf(subj):
    return subj["spec"]["algorithms"]["psfsw_components"]["covariance_boundary"].get("effective_psf_required") is True, \
        "effective PSF required"


def chk_psfsw_failclosed(subj):
    fc = subj["spec"]["algorithms"]["psfsw_components"]["fail_closed"]
    need = {"no_common_star_set", "background_nonpositive_undefined_transform",
            "insufficient_valid_stars", "selection_bias_gate_failed",
            "spatial_nonuniformity_gate_failed"}
    return need.issubset(set(fc["reasons"])) and "weight_value 必须为 null" in fc.get("rule", ""), \
        "fail-closed whitelist; valid=false => null weight"


def chk_psfsw_nokeys(subj):
    return PSFSW_KEYS_BANNED.issubset(set(subj["spec"].get("psfsw_forbidden_keys", []))), \
        "psfsw forbidden keys present"


def chk_psfsw_common_star_set(subj):
    indep = subj["spec"]["algorithms"]["psfsw_components"]["common_star_set"].get("independence", "")
    return ("外部参考星表" in indep and "单一门限" in indep and "禁止" in indep), \
        "common star set independent of frame measurement"


def chk_parent_var(subj):
    pv = subj["spec"]["algorithms"]["drizzle"]["correlation"]["parent_tile"]
    txt = " ".join(pv.get("requirements", []))
    return ("lower_bound" in txt and "相关核" in txt and "deficit" in txt), \
        "parent variance: lower_bound + kernel + deficit gate"


def chk_drz_flux_prov(subj):
    fc = subj["spec"]["algorithms"]["drizzle"]["flux_conservation"]
    return ("flux_conservation_factor = pixfrac^2" in fc.get("provenance", "")), "flux factor in provenance"


def chk_drz_sb_definition(subj):
    sig = subj["spec"]["algorithms"]["drizzle"]["signal"]
    return ("B_j a_jp" in sig["formula"] and "c_jp" in sig["coefficients"]["c_jp"]), \
        "SB-preserving normalization declared"


def chk_so_registered(subj):
    so = {x["id"] for x in subj["spec"].get("so_items_registered_only", [])}
    return {f"SO-0{i}" for i in range(1, 8)}.issubset(so), "SO-01..07 registered only, not signed"


def chk_ctrl_registered(subj):
    ci = {x["id"] for x in subj["spec"].get("controller_items_registered_only", [])}
    return {"CTRL-F1", "CTRL-AR033"}.issubset(ci), "controller-level items registered only"



# ============================================================ numeric fixtures / oracle

def fixture_drizzle(pixfrac, j=12):
    """平面 1D 代理几何（非生产几何）：J 源像素 A_pix=1；drop 覆盖相邻两目标各半。"""
    A_pix = 1.0
    A_drop = pixfrac * pixfrac * A_pix
    P = j + 1
    a = np.zeros((j, P))
    for jj in range(j):
        if jj < P - 1:
            a[jj, jj] = 0.5 * A_drop
            a[jj, jj + 1] = 0.5 * A_drop
        else:
            a[jj, P - 1] = A_drop
    D = a.sum(axis=0)
    wSB = a / A_pix         # w_SB_jp = a_jp / A_pixel_j
    wL = a / A_drop         # legacy drop weight
    c = wSB / D             # c_jp = w_SB_jp / D_p
    return {"pixfrac": pixfrac, "A_pix": A_pix, "A_drop": A_drop, "P": P,
            "a": a, "D": D, "wSB": wSB, "wL": wL, "c": c}


def chk_drz_const_sb(subj):
    B0 = 37.0
    for pf in (0.25, 0.5, 0.8, 1.0):
        fw = fixture_drizzle(pf)
        x = B0 * fw["A_pix"] * np.ones(fw["a"].shape[0])
        if subj["fw"].get("use_legacy_drop_weight"):
            S = (fw["wL"] * x[:, None]).sum(axis=0) / fw["D"]
        elif subj["fw"].get("signal_is_flux_no_D"):
            S = (fw["wSB"] * x[:, None]).sum(axis=0)
        else:
            S = (fw["wSB"] * x[:, None]).sum(axis=0) / fw["D"]
        if not np.all(np.abs(S / B0 - 1.0) < 1e-3):
            return False, "const-SB invariant failed at pixfrac=%.2f" % pf
    return True, "S_p=B0 for pixfrac in {0.25,0.5,0.8,1.0}, |S/B0-1|<1e-3"


def chk_drz_variance(subj):
    for pf in (0.5, 0.8, 1.0):
        fw = fixture_drizzle(pf)
        v = 0.03 + 0.001 * np.arange(fw["a"].shape[0])
        c = fw["c"]
        exact = (c ** 2 * v[:, None]).sum(axis=0)
        if subj["fw"].get("variance_omit_square"):
            got = (c * v[:, None]).sum(axis=0)
        elif subj["fw"].get("variance_omit_D2"):
            got = (fw["wSB"] ** 2 * v[:, None]).sum(axis=0)
        else:
            got = (c ** 2 * v[:, None]).sum(axis=0)
        if not np.allclose(got, exact, rtol=1e-12, atol=0.0):
            return False, "variance formula != sum c^2 v at pixfrac=%.2f" % pf
        alpha = 2.5
        v2 = alpha * alpha * v
        if subj["fw"].get("variance_omit_square"):
            got2 = (c * v2[:, None]).sum(axis=0)
        elif subj["fw"].get("variance_omit_D2"):
            got2 = (fw["wSB"] ** 2 * v2[:, None]).sum(axis=0)
        else:
            got2 = (c ** 2 * v2[:, None]).sum(axis=0)
        if not np.allclose(got2, alpha * alpha * got, rtol=1e-12, atol=0.0):
            return False, "scale law var->alpha^2 var failed at pixfrac=%.2f" % pf
    return True, "variance_p=sum c_jp^2 v_j and alpha^2 scale law exact"


def chk_drz_flux(subj):
    B0 = 11.0
    for pf in (0.25, 0.5, 0.8, 1.0):
        fw = fixture_drizzle(pf)
        x = B0 * fw["A_pix"] * np.ones(fw["a"].shape[0])
        S = (fw["wSB"] * x[:, None]).sum(axis=0) / fw["D"]
        phi_out = float((S * fw["D"]).sum())
        expected = (pf * pf) * float(x.sum())
        if subj["fw"].get("flux_factor_one"):
            expected = float(x.sum())
        if not np.isclose(phi_out, expected, rtol=1e-12, atol=0.0):
            return False, "flux conservation factor wrong at pixfrac=%.2f (got %.9g exp %.9g)" % (pf, phi_out, expected)
    return True, "Phi_out=sum_p S_p D_p = pixfrac^2 * sum_j x_j exact"


def chk_drz_correlation(subj):
    fw = fixture_drizzle(0.8)
    v = 0.03 + 0.001 * np.arange(fw["a"].shape[0])
    c = fw["c"]
    C = (c[:, :, None] * c[:, None, :] * v[:, None, None]).sum(axis=0)  # P x P
    diag = np.diag(C).copy()
    if subj["fw"].get("diagonal_is_exact"):
        C = np.diag(diag)
    if not np.allclose(diag, (c ** 2 * v[:, None]).sum(axis=0), rtol=1e-12, atol=0.0):
        return False, "diag(Cov) != sum c^2 v"
    off = C - np.diag(diag)
    if not np.any(np.abs(off) > 0):
        return False, "off-diagonal covariance identically zero (expected non-zero)"
    return True, "Cov(S_p,S_q)=sum_j c_jp c_jq v_j with non-zero off-diagonal"


def chk_drz_aperture(subj):
    fw = fixture_drizzle(0.8)
    v = 0.03 + 0.001 * np.arange(fw["a"].shape[0])
    c = fw["c"]
    C = (c[:, :, None] * c[:, None, :] * v[:, None, None]).sum(axis=0)
    ap = np.array([0.3, 1.0, 1.0, 0.4] + [0.0] * (fw["P"] - 4))
    ap = ap[:fw["P"]]
    exact = float(ap @ C @ ap)
    diag_only = float((ap ** 2 * np.diag(C)).sum())
    if subj["fw"].get("diagonal_is_exact"):
        exact = diag_only
    if not (exact > diag_only * (1.0 + 1e-9)):
        return False, "aperture exact variance not strictly above diagonal-only (%.9g vs %.9g)" % (exact, diag_only)
    return True, "aperture exact variance strictly greater than diagonal-only lower bound"


def chk_drz_parent(subj):
    fw = fixture_drizzle(0.8)
    v = 0.03 + 0.001 * np.arange(fw["a"].shape[0])
    c = fw["c"]
    C = (c[:, :, None] * c[:, None, :] * v[:, None, None]).sum(axis=0)
    D = fw["D"]
    w = D / D.sum()
    exact = float(w @ C @ w)
    diag = float((w ** 2 * np.diag(C)).sum())
    if subj["fw"].get("parent_diag_is_exact"):
        exact = diag
    deficit = (exact - diag) / exact if exact != 0 else 0.0
    if not (deficit >= 0.0 and exact >= diag):
        return False, "parent diagonal reduction not a lower bound"
    # 对角声明精确（deficit>0 却声称 exact）=> 红
    if subj["fw"].get("parent_claims_exact") and deficit > 0:
        return False, "parent diagonal claimed exact while deficit=%.6f>0" % deficit
    return True, "parent Var is a lower bound; deficit=%.6f (threshold pending SO-07)" % deficit


# ---- calibration covariance

def _cal_reference(alpha=0.6, f=2.0, y=5.0):
    """独立解析参考：J C J^T（同一 bias master 只计一次）。"""
    J = np.array([1.0 / f, -(1.0 - alpha) / f, -alpha / f, -y / f])
    V = np.array([0.9, 0.04, 0.09, 0.0004])
    return float((J ** 2 * V).sum())


def chk_cal_formula(subj):
    alpha, f, y = 0.6, 2.0, 5.0
    ref = _cal_reference(alpha, f, y)
    Vr, Vb, Vd, Vf = 0.9, 0.04, 0.09, 0.0004
    if subj["fw"].get("cal_uses_double_bias"):
        got = (Vr + Vb + alpha * alpha * (Vd + Vb) + y * y * Vf) / (f * f)
    else:
        got = (Vr + (1 - alpha) ** 2 * Vb + alpha * alpha * Vd + y * y * Vf) / (f * f)
    if not np.isclose(got, ref, rtol=1e-12, atol=0.0):
        return False, "cal per-pixel variance != J C J^T (got %.12g ref %.12g)" % (got, ref)
    return True, "cal per-pixel variance == J C J^T (same master folded as (1-alpha)^2)"


def chk_cal_shared(subj):
    nframes, npix = 4, 8
    v_ind = 0.05
    v_shared = 0.02
    c = np.full(nframes, 1.0 / nframes)
    if subj["fw"].get("shared_is_independent"):
        var_joint = float((c ** 2).sum()) * v_ind * npix
        var_naive = var_joint
    else:
        # 每像素跨帧共同模式：Cov(k,l) = v_ind delta_kl + v_shared (all k,l)
        var_joint = float(((c ** 2).sum()) * v_ind + (c.sum() ** 2) * v_shared) * npix
        var_naive = float(((c ** 2).sum()) * v_ind) * npix
    ratio = var_joint / var_naive
    if not (ratio > 1.0 + 1e-9):
        return False, "shared master not detected: ratio=%.9g (must be >1)" % ratio
    return True, "shared master detected: var_joint/var_naive=%.6f > 1" % ratio


def chk_cal_unit(subj):
    def sq(unit):
        if unit == "ADU":
            return "ADU^2"
        if unit == "ADU/px^2":
            return "ADU^2/px^4"
        return None
    u = subj["spec"]["units_table"]
    return sq(u.get("signal_sb")) == u.get("sb_variance_out"), "variance unit == (signal unit)^2"


# ---- PSF information

def fixture_psf(n=9):
    x = np.arange(n, dtype=float) - (n - 1) / 2.0
    r2 = x * x
    P = 1.0 / (1.0 + r2) ** 4
    P = P / P.sum()
    return P


def fixture_spd_c(n=9):
    idx = np.arange(n)
    C = 0.7 ** np.abs(idx[:, None] - idx[None, :])
    return C


def chk_winfo_white(subj):
    P = fixture_psf()
    a, sigma = 1.3, 0.7
    A_NEA = 1.0 / (P ** 2).sum()
    a_nea = (P ** 2).sum() if subj["fw"].get("anea_reciprocal") else A_NEA
    W_direct = a * a * (P ** 2).sum() / (sigma ** 2)
    if subj["fw"].get("winfo_reciprocal"):
        W_white = a * a * (sigma ** 2) * a_nea
    else:
        W_white = a * a / (sigma ** 2 * a_nea)
    if not np.isclose(W_direct, W_white, rtol=1e-12, atol=0.0):
        return False, "white-noise identity failed (%.12g vs %.12g)" % (W_direct, W_white)
    return True, "W=a^2 sum P^2/sigma^2 == a^2/(sigma^2 A_NEA); A_NEA=1/sum P^2"


def chk_winfo_gls(subj):
    P = fixture_psf()
    C = fixture_spd_c()
    a = 1.3
    A = a * P
    Cinv = np.linalg.inv(C)
    W = float(A @ Cinv @ A)
    if subj["fw"].get("winfo_a_not_squared"):
        W = float(a * (P @ Cinv @ P))
    F_true = 4.2
    d = A * F_true
    Q = float(A @ Cinv @ d)
    F_hat = Q / W
    c = Cinv @ A / (A @ Cinv @ A)
    var_from_c = float(c @ C @ c)
    if not (np.isclose(F_hat, F_true, rtol=1e-9) and np.isclose(var_from_c, 1.0 / W, rtol=1e-9)):
        return False, "GLS/Q-W mismatch (F_hat=%.9g 1/W=%.9g cC c=%.9g)" % (F_hat, 1.0 / W, var_from_c)
    W2 = float((2 * a) * (2 * a) * (P @ Cinv @ P))
    if subj["fw"].get("winfo_a_not_squared"):
        W2 = float((2 * a) * (P @ Cinv @ P))
    if not np.isclose(W2 / W, 4.0, rtol=1e-9):
        return False, "a^2 law failed: W(2a)/W(a)=%.6f" % (W2 / W)
    return True, "F_hat=Q/W==GLS; Var=1/W=c^T C c; W(2a)/W(a)=4"


# ---- PSFSW record validation

def default_psfsw_record():
    return {
        "weight_kind": "relative_dimensionless",
        "weight_units": "1",
        "weight_value": 1.0,
        "components": {
            "signal": {"measurement_id": "m_S", "p05": 0.8, "p50": 1.0, "p95": 1.2, "valid_area_fraction": 0.9,
                       "unit": "ADU", "value": 120.0},
            "concentration": {"measurement_id": "m_C", "p05": 0.7, "p50": 1.0, "p95": 1.3,
                              "valid_area_fraction": 0.9, "unit": "ADU/px", "value": 4.1},
            "noise": {"measurement_id": "m_N", "p05": 0.9, "p50": 1.0, "p95": 1.1,
                      "valid_area_fraction": 0.9, "unit": "ADU", "value": 2.5},
            "background": {"measurement_id": "m_B", "p05": 0.9, "p50": 1.0, "p95": 1.2,
                           "valid_area_fraction": 0.9, "unit": "ADU", "value": 8.0},
        },
        "normalization": {"scope": "group", "median_target": 1.0, "constants_version": "psfsw-v0-placeholder",
                          "group_key": "bandxcomponentxphotometric"},
        "validity": {"valid": True, "reason": None, "n_common": 42,
                     "common_star_set_id": "css-1", "selection_function_id": "sf-1"},
        "covariance": {"method": "propagated_from_composite_coefficients",
                       "variance_from_weight": False,
                       "uses_relative_weight_as_ivar": False,
                       "effective_psf_id": "epsf-1"},
    }


def _psfsw_has_banned_keys(obj):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in PSFSW_KEYS_BANNED:
                return True
            if _psfsw_has_banned_keys(v):
                return True
    elif isinstance(obj, list):
        for v in obj:
            if _psfsw_has_banned_keys(v):
                return True
    return False


def chk_psfsw_record(subj):
    rec = subj["fw"].get("psfsw_record") or default_psfsw_record()
    comps = rec.get("components", {})
    need = {"signal", "concentration", "noise", "background"}
    if not need.issubset(comps):
        return False, "psfsw four components incomplete"
    mids = [comps[k].get("measurement_id") for k in need]
    if len(set(mids)) != len(mids):
        return False, "psfsw measurement_id not distinct (component collapse)"
    for k in need:
        c = comps[k]
        if not (c["p05"] <= c["p50"] <= c["p95"]):
            return False, "psfsw %s p05<=p50<=p95 violated" % k
        if not (0.0 <= c["valid_area_fraction"] <= 1.0):
            return False, "psfsw %s valid_area_fraction out of [0,1]" % k
    if rec.get("weight_kind") != "relative_dimensionless" or rec.get("weight_units") != "1":
        return False, "psfsw weight kind/units must be relative_dimensionless / 1"
    norm = rec.get("normalization", {})
    if norm.get("scope") != "group" or float(norm.get("median_target", -1)) != 1.0 or not norm.get("constants_version"):
        return False, "psfsw normalization must be group / median_target=1 / versioned"
    cov = rec.get("covariance", {})
    if cov.get("method") != "propagated_from_composite_coefficients":
        return False, "psfsw covariance method wrong"
    if cov.get("variance_from_weight") is not False or cov.get("uses_relative_weight_as_ivar") is not False:
        return False, "psfsw variance_from_weight / uses_relative_weight_as_ivar must be false"
    if not cov.get("effective_psf_id"):
        return False, "psfsw effective PSF missing"
    val = rec.get("validity", {})
    if val.get("valid"):
        if rec.get("weight_value") is None or not np.isfinite(rec.get("weight_value", np.nan)):
            return False, "psfsw valid=true requires finite weight_value"
    else:
        if rec.get("weight_value") is not None:
            return False, "psfsw invalid must have weight_value=None (no median-SNR fallback)"
        whitelist = {"no_common_star_set", "background_nonpositive_undefined_transform",
                     "insufficient_valid_stars", "selection_bias_gate_failed",
                     "spatial_nonuniformity_gate_failed"}
        if val.get("reason") not in whitelist:
            return False, "psfsw invalid reason not in whitelist"
    if _psfsw_has_banned_keys(rec):
        return False, "psfsw product contains forbidden key"
    return True, "psfsw record conforms (4 components, unit, scope, covariance, validity, no banned keys)"


def chk_psfsw_group_norm(subj):
    S = np.array([10.0, 14.0, 5.0, 20.0])
    Conc = np.array([3.0, 3.5, 2.0, 4.0])
    N = np.array([2.0, 2.2, 1.8, 2.5])
    B = np.array([8.0, 7.0, 9.0, 6.0])
    alpha, beta, gamma, delta = 2.0, 1.0, 2.0, 1.0
    Wt = (S ** alpha) * (Conc ** beta) / ((N ** gamma) * (B ** delta))
    if subj["fw"].get("psfsw_normalize_by_mean"):
        W = Wt / Wt.mean()
    else:
        W = Wt / np.median(Wt)
    if not np.isclose(np.median(W), 1.0, rtol=1e-12):
        return False, "psfsw group median != 1"
    if not np.all(W > 0):
        return False, "psfsw weight not all positive"
    # 方向性：S、Conc 单调增；N、B 单调减
    base = Wt[0]
    if not ((S[0] * 1.1) ** alpha * (Conc[0] ** beta) / ((N[0] ** gamma) * (B[0] ** delta)) > base):
        return False, "psfsw not increasing in signal"
    if not ((S[0] ** alpha) * ((Conc[0] * 1.1) ** beta) / ((N[0] ** gamma) * (B[0] ** delta)) > base):
        return False, "psfsw not increasing in concentration"
    if not ((S[0] ** alpha) * (Conc[0] ** beta) / (((N[0] * 1.1) ** gamma) * (B[0] ** delta)) < base):
        return False, "psfsw not decreasing in noise"
    if not ((S[0] ** alpha) * (Conc[0] ** beta) / ((N[0] ** gamma) * ((B[0] * 1.1) ** delta)) < base):
        return False, "psfsw not decreasing in background"
    return True, "group median=1, all positive, monotone S/Conc up and N/B down"


# ============================================================ degrade-scalar structural check

def chk_degrade_scalar(subj):
    txt = subj["spec"]["algorithms"]["psf_information"].get("spatial_default", "")
    return ("p05/p50/p95" in txt and "空间残差/趋势门" in txt and "功率损失门" in txt), \
        "scalar downgrade needs dual gate + p05/p50/p95 + domain"


def chk_cov_prop_combined(subj):
    ok1, d1 = chk_drz_correlation(subj)
    ok2, d2 = chk_drz_aperture(subj)
    return (ok1 and ok2), "C_out = R C_in R^T: %s | %s" % (d1, d2)


def chk_psfsw_cov_boundary(subj):
    rec = subj["fw"].get("psfsw_record") or default_psfsw_record()
    cov = rec.get("covariance", {})
    ok = (cov.get("method") == "propagated_from_composite_coefficients"
          and cov.get("variance_from_weight") is False
          and cov.get("uses_relative_weight_as_ivar") is False
          and bool(cov.get("effective_psf_id")))
    return ok, "psfsw covariance boundary: method / no weight-derived variance / epsf present"


def chk_registry_covers_matrix(subj):
    tm = subj.get("tm") or load_json("alg_p1_001_test_matrix.json")
    self_id = "G-REGISTRY-COVERS-MATRIX"
    known = set(CHECKS.keys()) | {self_id}
    missing_tm = sorted({r["gate"] for r in tm["rows"]} - known)
    spec_gates = {g["id"] for g in subj["spec"].get("gates", [])}
    missing_spec = sorted(spec_gates - known)
    return (not missing_tm and not missing_spec), \
        "test-matrix/spec gate ids subset of executed CHECKS (missing tm=%s spec=%s)" % (missing_tm, missing_spec)


# ============================================================ registries

CHECKS = {
    # structure
    "G-STRUCT-FREEZE-INHERIT": chk_freeze_inherit,
    "G-STRUCT-UNIT-LAW": chk_unit_law,
    "G-STRUCT-FORBIDDEN-TOKENS": chk_forbidden_tokens,
    "G-STRUCT-ALG-AREAS": chk_alg_areas,
    "G-STRUCT-GATES": chk_gates,
    "G-MODE-PRODUCTION": chk_mode_production,
    "G-SO-REGISTERED": chk_so_registered,
    "G-CTRL-REGISTERED": chk_ctrl_registered,
    "G-REGISTRY-COVERS-MATRIX": chk_registry_covers_matrix,
    # calibration covariance
    "CAL-COV-REPRESENTATION": chk_cal_representation,
    "CAL-NO-CLIP": chk_cal_no_clip,
    "ADJ-OBS-01-SHARED": chk_cal_shared,
    "CAL-COV-FORMULA": chk_cal_formula,
    "CAL-UNIT": chk_cal_unit,
    # psf information
    "FZ-FORMULA-WINFO": chk_winfo_gls,
    "FZ-COND-WHITENOISE": chk_winfo_white,
    "FZ-WINFO-DIAG-APPROX": chk_winfo_diag_approx,
    "FZ-GATE-MEDIAN-SNR": chk_winfo_forbidden,
    "FZ-DEGRADE-SCALAR": chk_degrade_scalar,
    # drizzle
    "FZ-GATE-CONST-SB": chk_drz_const_sb,
    "FZ-FORMULA-DRIZZLE-VAR": chk_drz_variance,
    "FZ-COND-FLUX-CONSERV": chk_drz_flux,
    "FZ-DRZ-CORRELATION": chk_drz_correlation,
    "FZ-DRZ-APERTURE": chk_drz_aperture,
    "FZ-FORMULA-COV-PROP": chk_cov_prop_combined,
    "FZ-GATE-PARENT-VAR": chk_parent_var,
    "FZ-DRZ-PARENT-NUMERIC": chk_drz_parent,
    "FZ-DRZ-FLUX-PROV": chk_drz_flux_prov,
    "FZ-DRZ-SB-DEF": chk_drz_sb_definition,
    # psfsw
    "FZ-FIELD-PSFSW-4COMP": chk_psfsw_4comp,
    "FZ-FIELD-PSFSW-UNIT": chk_psfsw_unit_scope,
    "FZ-GATE-PSFSW-EPSF": chk_psfsw_epsf,
    "FZ-GATE-PSFSW-COV": chk_psfsw_cov_boundary,
    "FZ-GATE-PSFSW-FAILCLOSED": chk_psfsw_failclosed,
    "FZ-GATE-PSFSW-NOKEYS": chk_psfsw_nokeys,
    "FZ-PSFSW-COMMON-STAR-SET": chk_psfsw_common_star_set,
    "FZ-FORMULA-PSFSW-COMPOSITE": chk_psfsw_group_norm,
    "FZ-PSFSW-RECORD": chk_psfsw_record,
}


def _del_inherited(subj, fz):
    subj["spec"]["inherited_frozen_ids"] = [x for x in subj["spec"]["inherited_frozen_ids"] if x != fz]


def _mut_record(subj, fn):
    rec = default_psfsw_record()
    fn(rec)
    subj["fw"]["psfsw_record"] = rec


MUTATIONS = {
    # ---- structure
    "M-S1": {"gate": "G-STRUCT-FREEZE-INHERIT", "desc": "drop FZ-FORMULA-COV-PROP from inherited_frozen_ids",
             "apply": lambda s: _del_inherited(s, "FZ-FORMULA-COV-PROP")},
    "M-S2": {"gate": "G-STRUCT-FORBIDDEN-TOKENS", "desc": "drop median_source_snr from forbidden tokens",
             "apply": lambda s: s["spec"]["forbidden_weight_source_tokens"].remove("median_source_snr")},
    "M-S3": {"gate": "G-STRUCT-ALG-AREAS", "desc": "delete psfsw_components algorithm area",
             "apply": lambda s: s["spec"]["algorithms"].pop("psfsw_components")},
    "M-S4": {"gate": "G-STRUCT-UNIT-LAW", "desc": "set sb_variance_out=ADU^2 (unit law broken)",
             "apply": lambda s: s["spec"]["units_table"].__setitem__("sb_variance_out", "ADU^2")},
    "M-S5": {"gate": "G-STRUCT-GATES", "desc": "drop FZ-GATE-CONST-SB from gates",
             "apply": lambda s: s["spec"].__setitem__("gates", [g for g in s["spec"]["gates"] if g["id"] != "FZ-GATE-CONST-SB"])},
    "M-S6": {"gate": "G-MODE-PRODUCTION", "desc": "put psf_snr_power into production modes",
             "apply": lambda s: (s["spec"]["production_weight_modes"].append("psf_snr_power"),
                                 s["spec"]["deferred_modes"].remove("psf_snr_power"))},
    "M-S7": {"gate": "FZ-GATE-PSFSW-NOKEYS", "desc": "drop ivar from psfsw forbidden keys",
             "apply": lambda s: s["spec"]["psfsw_forbidden_keys"].remove("ivar")},
    "M-S8": {"gate": "FZ-GATE-PSFSW-EPSF", "desc": "effective_psf_required=false",
             "apply": lambda s: s["spec"]["algorithms"]["psfsw_components"]["covariance_boundary"].__setitem__("effective_psf_required", False)},
    "M-S9": {"gate": "FZ-GATE-PSFSW-FAILCLOSED", "desc": "drop no_common_star_set from fail-closed reasons",
             "apply": lambda s: s["spec"]["algorithms"]["psfsw_components"]["fail_closed"]["reasons"].remove("no_common_star_set")},
    "M-S10": {"gate": "FZ-PSFSW-COMMON-STAR-SET", "desc": "common star set not independent of frame measurement",
              "apply": lambda s: s["spec"]["algorithms"]["psfsw_components"]["common_star_set"].__setitem__("independence", "per-frame detection threshold")},
    "M-S11": {"gate": "FZ-GATE-PARENT-VAR", "desc": "drop parent variance requirements",
              "apply": lambda s: s["spec"]["algorithms"]["drizzle"]["correlation"]["parent_tile"].__setitem__("requirements", [])},
    "M-S12": {"gate": "FZ-GATE-MEDIAN-SNR", "desc": "drop residual from W_info forbidden sources",
              "apply": lambda s: s["spec"]["algorithms"]["psf_information"]["forbidden_sources"].remove("residual")},
    "M-S13": {"gate": "CAL-COV-REPRESENTATION", "desc": "drop shared master allowed representations",
              "apply": lambda s: s["spec"]["algorithms"]["calibration_covariance"]["shared_terms"].__setitem__("allowed_representations", [])},
    "M-S14": {"gate": "CAL-NO-CLIP", "desc": "clamp negative calibrated values",
              "apply": lambda s: s["spec"]["algorithms"]["calibration_covariance"]["linearization"].__setitem__("negative_values", "clamp negative to 0")},
    "M-S15": {"gate": "FZ-WINFO-DIAG-APPROX", "desc": "drop diagonal-approx reporting requirement",
              "apply": lambda s: s["spec"]["algorithms"]["psf_information"]["discrete"].pop("approximate_diagonal_C")},
    "M-S16": {"gate": "FZ-DEGRADE-SCALAR", "desc": "drop power-loss gate / percentiles for scalar downgrade",
              "apply": lambda s: s["spec"]["algorithms"]["psf_information"].__setitem__("spatial_default", "scalar allowed")},
    # ---- drizzle numeric
    "M-S17": {"gate": "G-REGISTRY-COVERS-MATRIX", "desc": "test-matrix references a gate with no executed checker",
              "apply": lambda s: s["tm"]["rows"][0].__setitem__("gate", "NO-SUCH-GATE")},
    "M-D1": {"gate": "FZ-GATE-CONST-SB", "desc": "use legacy drop weight w=a/A_drop in signal layer",
             "apply": lambda s: s["fw"].__setitem__("use_legacy_drop_weight", True)},
    "M-D3": {"gate": "FZ-GATE-CONST-SB", "desc": "define S_p=F_p (omit D_p normalization)",
             "apply": lambda s: s["fw"].__setitem__("signal_is_flux_no_D", True)},
    "M-D2": {"gate": "FZ-FORMULA-DRIZZLE-VAR", "desc": "variance omits the square: var=sum c v",
             "apply": lambda s: s["fw"].__setitem__("variance_omit_square", True)},
    "M-D7": {"gate": "FZ-FORMULA-DRIZZLE-VAR", "desc": "variance omits /D_p: var=sum w^2 v",
             "apply": lambda s: s["fw"].__setitem__("variance_omit_D2", True)},
    "M-D4": {"gate": "FZ-COND-FLUX-CONSERV", "desc": "claim flux conservation without pixfrac^2 factor",
             "apply": lambda s: s["fw"].__setitem__("flux_factor_one", True)},
    "M-D5": {"gate": "FZ-DRZ-APERTURE", "desc": "claim diagonal-only aperture variance is exact",
             "apply": lambda s: s["fw"].__setitem__("diagonal_is_exact", True)},
    "M-D6": {"gate": "FZ-DRZ-PARENT-NUMERIC", "desc": "claim parent diagonal reduction exact while deficit>0",
             "apply": lambda s: s["fw"].__setitem__("parent_claims_exact", True)},
    # ---- calibration numeric
    "M-C1": {"gate": "ADJ-OBS-01-SHARED", "desc": "treat shared master as independent random",
             "apply": lambda s: s["fw"].__setitem__("shared_is_independent", True)},
    "M-C2": {"gate": "CAL-COV-FORMULA", "desc": "double-count same bias master (V_b+alpha^2 V_b)",
             "apply": lambda s: s["fw"].__setitem__("cal_uses_double_bias", True)},
    "M-C4": {"gate": "CAL-UNIT", "desc": "variance unit not square of signal unit",
             "apply": lambda s: s["spec"]["units_table"].__setitem__("sb_variance_out", "ADU^2")},
    # ---- W_info numeric
    "M-W1": {"gate": "FZ-COND-WHITENOISE", "desc": "reciprocal white-noise law W=a^2 sigma^2 A_NEA",
             "apply": lambda s: s["fw"].__setitem__("winfo_reciprocal", True)},
    "M-W3": {"gate": "FZ-COND-WHITENOISE", "desc": "A_NEA=sum P^2 (reciprocal error)",
             "apply": lambda s: s["fw"].__setitem__("anea_reciprocal", True)},
    "M-W2": {"gate": "FZ-FORMULA-WINFO", "desc": "W=a P^T C^-1 P (a not squared)",
             "apply": lambda s: s["fw"].__setitem__("winfo_a_not_squared", True)},
    # ---- psfsw numeric / record
    "M-P1": {"gate": "FZ-PSFSW-RECORD", "desc": "collapse concentration into signal (same measurement_id)",
             "apply": lambda s: _mut_record(s, lambda r: r["components"]["concentration"].__setitem__("measurement_id", "m_S"))},
    "M-P2": {"gate": "FZ-FORMULA-PSFSW-COMPOSITE", "desc": "normalize group by mean instead of median",
             "apply": lambda s: s["fw"].__setitem__("psfsw_normalize_by_mean", True)},
    "M-P3": {"gate": "FZ-PSFSW-RECORD", "desc": "psfsw weight units flux^-2",
             "apply": lambda s: _mut_record(s, lambda r: r.__setitem__("weight_units", "flux^-2"))},
    "M-P4": {"gate": "FZ-GATE-PSFSW-COV", "desc": "variance = 1/relative weight",
             "apply": lambda s: _mut_record(s, lambda r: r["covariance"].__setitem__("variance_from_weight", True))},
    "M-P5": {"gate": "FZ-PSFSW-RECORD", "desc": "invalid falls back to median source SNR",
             "apply": lambda s: _mut_record(s, lambda r: (r["validity"].__setitem__("valid", False), r.__setitem__("weight_value", 0.42)))},
    "M-P6": {"gate": "FZ-PSFSW-RECORD", "desc": "drop background component",
             "apply": lambda s: _mut_record(s, lambda r: r["components"].pop("background"))},
    "M-P7": {"gate": "FZ-PSFSW-RECORD", "desc": "drop effective PSF",
             "apply": lambda s: _mut_record(s, lambda r: r["covariance"].__setitem__("effective_psf_id", ""))},
    "M-P8": {"gate": "FZ-PSFSW-RECORD", "desc": "inject forbidden ivar key into psfsw product",
             "apply": lambda s: _mut_record(s, lambda r: r.__setitem__("ivar", 1.0))},
    "M-P9": {"gate": "FZ-PSFSW-RECORD", "desc": "normalization scope=global",
             "apply": lambda s: _mut_record(s, lambda r: r["normalization"].__setitem__("scope", "global"))},
}


# ============================================================ runner

def new_subject():
    return {"spec": load_json("alg_p1_001_spec.json"),
            "tm": load_json("alg_p1_001_test_matrix.json"), "fw": {}}


def run_gate(subj, gate):
    fn = CHECKS.get(gate)
    if fn is None:
        return False, "unknown gate %s" % gate
    try:
        ok, detail = fn(subj)
    except Exception as exc:  # noqa: BLE001
        return False, "EXCEPTION: %r" % (exc,)
    return bool(ok), detail


def baseline_run():
    subj = new_subject()
    rows = []
    for gid in CHECKS:
        ok, detail = run_gate(subj, gid)
        rows.append((gid, ok, detail))
    return rows


def mutation_run(mid):
    mut = MUTATIONS[mid]
    subj = new_subject()
    mut["apply"](subj)
    ok, detail = run_gate(subj, mut["gate"])
    return (ok is False), mut["gate"], detail


def print_baseline(rows, as_json):
    n = len(rows)
    npass = sum(1 for _, ok, _ in rows if ok)
    if as_json:
        print(json.dumps({"gate": "selftest", "checks": n, "passed": npass,
                          "rows": [{"id": g, "pass": ok, "detail": d} for g, ok, d in rows]},
                         ensure_ascii=False, indent=2))
        return n, npass
    print("== ALG-P1-001 baseline (independent Oracle + structure) ==")
    for gid, ok, detail in rows:
        print("  [%s] %-32s %s" % ("PASS" if ok else "FAIL", gid, detail))
    print("  -> %d/%d PASS" % (npass, n))
    return n, npass


def main(argv=None):
    ap = argparse.ArgumentParser(description="ALG-P1-001 verifier")
    ap.add_argument("--selftest", action="store_true", help="run baseline checks only (default)")
    ap.add_argument("--all-mutations", action="store_true", help="require every mutation CAUGHT")
    ap.add_argument("--mutation", metavar="ID", help="inject one mutation; rc=2 if CAUGHT, 1 if MISSED")
    ap.add_argument("--list", action="store_true", help="list gate/mutation ids")
    ap.add_argument("--json", action="store_true", help="json output")
    args = ap.parse_args(argv)

    if args.list:
        print("gates:")
        for g in CHECKS:
            print("  " + g)
        print("mutations:")
        for m in MUTATIONS:
            print("  %-6s -> %s : %s" % (m, MUTATIONS[m]["gate"], MUTATIONS[m]["desc"]))
        return 0

    if len(CHECKS) == 0 or len(MUTATIONS) == 0:
        print("FAIL: zero checks or zero mutations (skip-only not allowed)")
        return 1

    rows = baseline_run()
    n, npass = print_baseline(rows, args.json)
    if npass != n:
        print("BASELINE FAIL: %d/%d" % (npass, n))
        return 1
    if n < 20:
        print("BASELINE FAIL: only %d checks (need >=20)" % n)
        return 1

    if args.mutation:
        if args.mutation not in MUTATIONS:
            print("FAIL: unknown mutation %s" % args.mutation)
            return 1
        caught, gate, detail = mutation_run(args.mutation)
        print("  mutation %s target=%s -> %s (%s)" % (args.mutation, gate, "CAUGHT" if caught else "MISSED", detail))
        return 2 if caught else 1

    if args.all_mutations:
        missed = []
        print("== negative mutations ==")
        for mid in MUTATIONS:
            caught, gate, detail = mutation_run(mid)
            print("  [%s] %-6s target=%-28s %s" % ("CAUGHT" if caught else "MISSED", mid, gate, detail))
            if not caught:
                missed.append(mid)
        print("  -> %d/%d CAUGHT" % (len(MUTATIONS) - len(missed), len(MUTATIONS)))
        if missed:
            print("MISSED: " + ",".join(missed))
            return 1
        return 0

    # default: baseline only
    return 0


if __name__ == "__main__":
    sys.exit(main())
