#!/usr/bin/env python3
"""v6_p2_oracle.py — P2-INTEGRATE-001 独立 Oracle（不调用被测实现）

从磁盘重开 Phase1/Phase2 产物，用 NumPy 从第一性原理重算：
  * point_information 独立帧合并 Q=ΣQ_k, W=ΣW_info_k, F=Q/W, Var=1/W
  * point_information 相关帧联合 GLS W=A^T C^-1 A, Q=A^T C^-1 d
  * surface_gls  x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1
  * psfsw_robust 已退役（FZ-MODE-RETIRED）：本 Oracle 只断言"声明它必须被显式拒绝 +
    迁移提示"，不再复算其 conventional coadd / C_out
并做结构门：output_hash=磁盘实际 sha256、BUNIT 二次律、provenance 最小集、
权重面禁诊断来源、P33 撤销、effective PSF 必输与归一约定、non-vacuity 注入必红。
"""
import argparse
import hashlib
import json
import math
import os
import shutil
import sys
import tempfile

import numpy as np

ERRORS = []
CHECKS = [0]


def check(cond, msg):
    CHECKS[0] += 1
    if not cond:
        ERRORS.append(msg)
    return bool(cond)


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def unit_exponents(u):
    """unit = ADU^a * px^p -> (a, p)；失败 raise。"""
    import re
    u = u.strip()
    if u == "1":
        return (0, 0)
    # 支持 ADU, ADU^2, px^2, ADU/px^2, ADU^2/px^4, px^4/ADU^2
    m = re.fullmatch(r"(ADU)(?:\^(-?\d+))?", u)
    if m:
        return (int(m.group(2) or 1), 0)
    m = re.fullmatch(r"(ADU)(?:\^(-?\d+))?/(px)(?:\^(\d+))?", u)
    if m:
        return (int(m.group(2) or 1), -int(m.group(4) or 1))
    m = re.fullmatch(r"(px)(?:\^(\d+))?/(ADU)(?:\^(\d+))?", u)
    if m:
        return (-int(m.group(4) or 1), int(m.group(2) or 1))
    raise ValueError("unparseable unit: " + u)


PROV_MIN = [
    "product", "software_sha", "run_id", "input_product_hashes", "config_hash", "units",
    "coordinate", "pixel_semantics", "sampling", "algorithm_ids", "module", "provider",
    "approximations", "degradations", "normalization_version", "weight_mode_version",
    "correlation_summary", "flux_conservation_factor", "k_corr", "generated_utc", "output_hash",
]

FORBIDDEN_SOURCES = [
    "median_source_snr", "median_snr", "source_snr_median", "support", "support_area",
    "coverage", "coverage_area", "fwhm", "psf_fwhm", "median_fwhm", "source_fwhm",
    "residual", "psf_residual", "psf_fit_residual", "fit_residual", "psfsw_robust_weight",
    "psfsw", "rejection", "probability", "rejection_probability", "effective_psf", "source_snr",
]
FORBIDDEN_PSFSW_KEYS = ["ivar", "inverse_variance", "variance", "var", "sigma", "sigma2",
                        "fisher", "fisher_information", "information", "w_info", "w_psf"]
P33_KEYS = ["snr_frame_coefficient", "snr_frame_science", "snr_coefficient", "snr_coef",
            "median_snr_weight", "support_x_snr2", "support_x_snr", "snr_frame"]


def scan_p33(obj):
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k in P33_KEYS:
                return k
            r = scan_p33(v)
            if r:
                return r
    elif isinstance(obj, list):
        for e in obj:
            r = scan_p33(e)
            if r:
                return r
    return None


def fwhm(profile):
    """独立实现（冻结惯例）：围绕全局极大值左右各找半高交点，线性插值，返回半高全宽。"""
    y = np.asarray(profile, dtype=float)
    n = y.size
    if n < 2:
        return 0.0
    imax = int(np.argmax(y))
    peak = y[imax]
    if not peak > 0:
        return 0.0
    half = 0.5 * peak
    left = right = 0.0
    have_l = have_r = False
    for i in range(imax - 1, -1, -1):
        if y[i] <= half:
            denom = y[i + 1] - y[i]
            t = (half - y[i]) / denom if denom != 0.0 else 0.0
            left = imax - (i + t)
            have_l = True
            break
    for i in range(imax + 1, n):
        if y[i] <= half:
            denom = y[i] - y[i - 1]
            t = (half - y[i - 1]) / denom if denom != 0.0 else 0.0
            right = (i - 1 + t) - imax
            have_r = True
            break
    if not have_l and not have_r:
        return 0.0
    if not have_l:
        left = right
    if not have_r:
        right = left
    return float(left + right)


def load_fits(path):
    from astropy.io import fits
    with fits.open(path) as hdul:
        out = {}
        for h in hdul:
            name = (h.header.get("EXTNAME") or "").strip()
            out[name] = {"data": np.asarray(h.data, dtype=float) if h.data is not None else None,
                         "bunit": str(h.header.get("BUNIT", "")).strip()}
        return out


def check_product(prod_dir, results):
    """返回 (errors, info)。独立重开校验。"""
    errs = []
    rec_path = os.path.join(prod_dir, "phase2_product.json")
    fits_path = os.path.join(prod_dir, "mosaic.fits")
    with open(rec_path) as f:
        rec = json.load(f)
    # 1) output_hash == 磁盘实际 sha256
    real = sha256_file(fits_path)
    if rec["manifest"]["output_hash"] != real:
        errs.append("manifest.output_hash != real sha256")
    if rec["provenance"]["output_hash"] not in ("sha256:" + real, real):
        errs.append("provenance.output_hash != real sha256")
    # 2) P33 撤销
    p33 = scan_p33(rec)
    if p33:
        errs.append("P33 key reintroduced: " + p33)
    # 3) provenance 最小集
    for k in PROV_MIN:
        if k not in rec["provenance"]:
            errs.append("provenance missing key: " + k)
    # 4) 模式门（FZ-MODE-PRODUCTION）：allowed = point_information | surface_gls
    if rec["weight_mode"] not in ("point_information", "surface_gls"):
        errs.append("weight_mode not in production set (FZ-MODE-PRODUCTION): "
                    + str(rec["weight_mode"]))
    # 5) BUNIT 二次律 + 各 HDU
    hdus = load_fits(fits_path)
    b_sig = hdus.get("", {}).get("bunit", "")
    b_var = hdus.get("VARIANCE", {}).get("bunit", "")
    b_ivar = hdus.get("IVAR", {}).get("bunit", "")
    if b_sig != "ADU/px^2":
        errs.append("SIGNAL BUNIT != ADU/px^2")
    if unit_exponents(b_var) != tuple(2 * x for x in unit_exponents(b_sig)):
        errs.append("variance BUNIT not signal^2")
    if unit_exponents(b_ivar) != tuple(-x for x in unit_exponents(b_var)):
        errs.append("ivar BUNIT not 1/variance")
    if "FLUX" in hdus and hdus["FLUX"]["bunit"] != "ADU":
        errs.append("FLUX BUNIT != ADU")
    if "EFFECTIVE_PSF" in hdus and hdus["EFFECTIVE_PSF"]["bunit"] != "1":
        errs.append("EFFECTIVE_PSF BUNIT != 1")
    # 6) effective PSF 必输 + 归一约定
    ep = rec["effective_psf"]
    if not ep.get("effective_psf_id"):
        errs.append("effective_psf_id empty")
    if ep.get("normalization") not in ("peak", "integral"):
        errs.append("effective PSF normalization undeclared")
    vom = ep.get("values_or_model", {})
    if vom.get("kind") != "values" or not vom.get("values"):
        errs.append("effective PSF only fwhm scalar")
    else:
        prof = np.asarray(vom["values"], dtype=float)
        if ep["normalization"] == "peak":
            if abs(prof.max() - 1.0) > 1e-9:
                errs.append("peak-normalized P_eff max != 1")
        else:
            if abs(prof.sum() - 1.0) > 1e-9:
                errs.append("integral-normalized P_eff sum != 1")
        rec_fwhm = ep["fwhm_from_effective"]["value"]
        if abs(fwhm(prof) - rec_fwhm) > 1e-9:
            errs.append("FWHM not measured from P_eff (median substitution?)")
    # 7) covariance 来源 + 权重面
    cov = rec["covariance"]
    if cov["variance_from"] not in ("combination_coefficients", "linear_combination_coefficients",
                                    "actual_combination_coefficients"):
        errs.append("variance_from not frozen enum")
    if rec["weight_mode_record"]["weight"]["sources"]:
        for s in rec["weight_mode_record"]["weight"]["sources"]:
            if s in FORBIDDEN_SOURCES:
                errs.append("diagnostic token in weight.sources: " + s)
    # FZ-MODE-RETIRED：退役对象 psfsw_robust_weight 不是现行对象（ASTROCS_DESIGN.md
    # §3.1；UNIFIED_MODEL.md:58）⇒ 产品声明它即判红（显式拒绝 + 迁移提示，不得静默接受）。
    # 该分支同时保留历史词表禁区判定：退役产品也不得夹带 ivar/variance 冒充。
    if rec["weight_mode"] == "psfsw_robust":
        errs.append("FZ-MODE-RETIRED: retired weight_mode 'psfsw_robust' present "
                    "(psfsw_robust_weight is not a current object; allowed weight "
                    "objects: point_information|surface_gls)")
        ps = rec.get("psfsw", {})
        for k in FORBIDDEN_PSFSW_KEYS:
            if k in ps:
                errs.append("retired psfsw product carries forbidden key: " + k)
    return errs, rec


def recompute_point_independent(repo_dir, workdir, rec):
    """独立帧 Q/W 合并（读 Phase1 记录，非本实现）。"""
    prod = {}
    for name in ("frame_a", "frame_b", "frame_c"):
        p = os.path.join(workdir, "ph1", name + ".p1", "phase1_product.json")
        with open(p) as f:
            prod[name] = json.load(f)
    W = sum(d["point_information"]["W_info"]["value"] for d in prod.values())
    Q = sum(d["point_information"]["Q"]["value"] for d in prod.values())
    F = Q / W
    var = 1.0 / W
    pi = rec["point_information"]
    return (abs(pi["W_info"]["value"] - W) / W, abs(pi["Q"]["value"] - Q) / abs(Q),
            abs(pi["flux"]["value"] - F) / abs(F), abs(pi["flux_variance"]["value"] - var) / var)


def recompute_point_joint(rj):
    A = np.asarray(rj["a_psf"], dtype=float).reshape(-1, 1)
    d = np.asarray(rj["data"], dtype=float).reshape(-1, 1)
    C = np.asarray(rj["c_in"], dtype=float).reshape(A.size, A.size)
    m = int(rj["m"])
    K = A.size // m
    Ci = np.linalg.inv(C)
    M = float((A.T @ Ci @ A).item())
    W = M
    Q = float((A.T @ Ci @ d).item())
    F = Q / W
    var = 1.0 / W
    # alpha_k = a_k Σ_i P_ki (C^-1 A)_i / W ；a_psf 即 a_k P_ki
    CA = Ci @ A
    alpha = []
    for k in range(K):
        s = float((np.asarray(rj["a_psf"][k * m:(k + 1) * m]) @ CA[k * m:(k + 1) * m]).item())
        # a_k P_ki 与 P_ki 的比例 = a_k，这里直接用 a_psf 近似 a_k=1（fixture）
        alpha.append(s / W)
    # naive（独立帧）：每帧自己的块对角 C_kk（块逆，非仅对角）
    Wn = 0.0
    for k in range(K):
        Ckk = C[k * m:(k + 1) * m, k * m:(k + 1) * m]
        blk = A[k * m:(k + 1) * m]
        Wn += float((blk.T @ np.linalg.inv(Ckk) @ blk).item())
    ratio = Wn / W
    errs = []
    if abs(rj["w_info"] - W) / W > 1e-9:
        errs.append("joint W mismatch")
    if abs(rj["flux"] - F) / abs(F) > 1e-9:
        errs.append("joint F mismatch")
    if abs(rj["var_f"] - var) / var > 1e-9:
        errs.append("joint Var mismatch")
    if abs(rj["corr_ratio"] - ratio) / ratio > 1e-9:
        errs.append("corr_ratio mismatch: %.12g vs %.12g" % (rj["corr_ratio"], ratio))
    if ratio < 1.05:
        errs.append("correlated detection ratio < 1.05")
    return errs, ratio


def recompute_surface(rs):
    A = np.asarray(rs["design"], dtype=float).reshape(-1, 1)
    d = np.asarray(rs["data"], dtype=float).reshape(-1, 1)
    C = np.asarray(rs["c_in"], dtype=float).reshape(A.size, A.size)
    Ci = np.linalg.inv(C)
    M = float((A.T @ Ci @ A).item())
    xhat = float((A.T @ Ci @ d)[0, 0] / M)
    var = 1.0 / M
    errs = []
    if abs(rs["x_hat"] - xhat) / max(abs(xhat), 1e-300) > 1e-9:
        errs.append("surface x_hat mismatch")
    if abs(rs["var_gls"] - var) / var > 1e-9:
        errs.append("surface var_gls mismatch")
    return errs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--workdir", required=True)
    ap.add_argument("--repo", default=".")
    args = ap.parse_args()
    work = args.workdir
    results_path = os.path.join(work, "v6_p2_results.json")
    if not os.path.exists(results_path):
        print("ORACLE_FAIL missing results.json", file=sys.stderr)
        return 1
    with open(results_path) as f:
        results = json.load(f)

    # 结构/重开门（point / surface）。psfsw_robust 已按负责人裁决退役（FZ-MODE-RETIRED），
    # 不再有产品块：其证据改为"必须被显式拒绝"的负例（见下方 psfsw_retired）。
    for key in ("point", "point_joint", "surface"):
        if key not in results:
            check(False, "missing results block: " + key)
            continue
        d = results[key]["out"]
        errs, rec = check_product(d, results)
        for e in errs:
            check(False, key + ": " + e)
        if not errs:
            check(True, key + " structural")

    # 独立数值重算
    if "point" in results:
        d = results["point"]["out"]
        with open(os.path.join(d, "phase2_product.json")) as f:
            rec = json.load(f)
        rel = recompute_point_independent(args.repo, work, rec)
        for name, v in zip(("W", "Q", "F", "Var"), rel):
            check(v < 1e-9, "point independent %s rel err %.3g" % (name, v))
    if "point_joint" in results:
        errs, ratio = recompute_point_joint(results["point_joint"])
        for e in errs:
            check(False, e)
        check(not errs, "point joint independent recompute (ratio=%.6f)" % ratio)
    # 非"逐帧 FWHM 摘要替代"：FWHM(P_eff) 与 median(FWHM_k) 必须可区分（Oracle C6）。
    if "point" in results:
        with open(os.path.join(results["point"]["out"], "phase2_product.json")) as f:
            rec_point = json.load(f)
        peff = np.asarray(rec_point["effective_psf"]["values_or_model"]["values"], float)
        frame_fwhms = []
        for name in ("frame_a", "frame_b", "frame_c"):
            with open(os.path.join(work, "ph1", name + ".p1", "phase1_product.json")) as f:
                prof = json.load(f)["phase1_extensions"]["psf_profile"]
            frame_fwhms.append(fwhm(prof))
        med_k = float(np.median(frame_fwhms))
        check(abs(fwhm(peff) - med_k) > 1e-6,
              "FWHM(P_eff) must differ from median(FWHM_k) (median substitution)")
        check(abs(rec_point["effective_psf"]["fwhm_from_effective"]["value"] - fwhm(peff)) <= 1e-9,
              "recorded FWHM must equal FWHM(P_eff)")
    if "surface" in results:
        errs = recompute_surface(results["surface"])
        for e in errs:
            check(False, e)
        check(not errs, "surface GLS independent recompute")
    # FZ-MODE-RETIRED：退役对象 psfsw_robust 必须被显式拒绝（不静默接受、不产出产品），
    # 且拒绝理由可诊断：被拒对象 + 允许的权重对象 + 迁移提示。
    check("psfsw_retired" in results, "psfsw_retired negative evidence present")
    if "psfsw_retired" in results:
        rej = results["psfsw_retired"]
        check(rej.get("rejected") is True,
              "retired psfsw_robust rejected (no product written)")
        msg = rej.get("error", "")
        for tok in ("FZ-MODE-RETIRED", "psfsw_robust_weight", "point_information",
                    "surface_gls", "migration"):
            check(tok in msg, "retired reject reason carries %r" % tok)

    # non-vacuity：注入 mutation 后本 Oracle 检查器必须变红
    if "point" in results:
        d = results["point"]["out"]
        tmp = tempfile.mkdtemp(prefix="v6p2_oracle_mut_")
        try:
            shutil.copytree(d, os.path.join(tmp, "p"))
            mp = os.path.join(tmp, "p", "phase2_product.json")
            with open(mp) as f:
                rec = json.load(f)
            rec["covariance"]["variance_from"] = "psfsw_robust_weight"
            with open(mp, "w") as f:
                json.dump(rec, f)
            errs, _ = check_product(os.path.join(tmp, "p"), results)
            check(len(errs) > 0, "oracle non-vacuity: variance_from mutation must be red")
            with open(mp) as f:
                rec = json.load(f)
            rec["covariance"]["variance_from"] = "actual_combination_coefficients"
            rec["phase2_extensions"]["snr_frame_coefficient"] = 1.0
            with open(mp, "w") as f:
                json.dump(rec, f)
            errs, _ = check_product(os.path.join(tmp, "p"), results)
            check(len(errs) > 0, "oracle non-vacuity: P33 key mutation must be red")
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    if ERRORS:
        for e in ERRORS[:40]:
            print("  [FAIL] " + e, file=sys.stderr)
        print("ORACLE_FAIL checks=%d fails=%d" % (CHECKS[0], len(ERRORS)), file=sys.stderr)
        return 1
    print("ORACLE_PASS checks=%d" % CHECKS[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
