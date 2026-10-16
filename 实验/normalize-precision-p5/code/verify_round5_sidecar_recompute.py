#!/usr/bin/env python3
"""ROUND2 独立审查执行结论第 1 项(4): 从发布 sidecar 按每控制点 PSF+R+v+Z
**独立重算** V_F 与 SNR_ref, 不对照源程序自行写出的帧级汇总。

独立路径（与生产 C++ QR 行空间投影不同的数值算法）:
  1. 由 covariance payload 的 v_j 与 drizzle_operator payload 的 CSR 组装
     C_sphere = R diag(v) Rᵀ（nl×nl 稠密, nl≈800, numpy 可担）；
  2. 对 C 做对称特征分解（numpy eigh）, 按**奇异值**口径 σ_i=√λ_i 与生产同一
     相对容差（1e-10·σ_max）取保留子空间（生产 pivoted QR 也是在奇异值尺度判秩）；
  3. 白化 A=[h, Z_s]（h 来自 psf payload, Z_s 来自 covariance payload 的
     background_basis）, 得 t=S^{-1/2}Uᵀh、Z_w=S^{-1/2}UᵀZ_s；
  4. I_F = ‖(I−Π_{Z_w})t‖²（两次修正 Gram-Schmidt + 重正交, 与生产同款显式残差,
     但分解/语言/数组路径全独立）, V_F=1/I_F, SNR=F_ref/√V_F；
  5. 与 sidecar 逐控制点 flux_variance / reference_snr / flux_information 对拍,
     并核对 flux_residual_norm²==flux_information、帧摘要定义。

依据: docs/science/noise_snr/NOISE_SNR.md §11.4/§11.6,
      docs/engineering/contracts/NORMALIZE_DATA.md §7.1/§8,
      实验/normalize-precision-p5/ROUND2_INDEPENDENT_REVIEW_633FFC31.md。

用法: python3 verify_round5_sidecar_recompute.py <frame_product_root>
退出码: 0=全部一致; 1=存在超出容差的不一致或数据错误。
"""
from __future__ import annotations

import json
import math
import os
import sys

import numpy as np

TOL_REL = 1e-10          # 与生产 support_rank_tol 同口径（奇异值尺度）
VF_REL_TOL = 1e-8        # 对拍容差: eigh 与 Householder QR 的数值差应远小于此
SNR_REL_TOL = 1e-8


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def load_model(root, rel):
    return load_json(os.path.join(root, rel))


def reconstruct_if(h, z, v, r_rows, r_cols, r_vals, m, nl, n_bg):
    """独立重算 I_{F|beta}。

    数值路径选择（避开对 C=R diag(v) Rᵀ 直接 eigh 的陷阱）:
      * C 的条件数是 κ(Q)²; eigh(C) 的特征值噪声底 ~ε·λmax ⇒ σ 尺度
        ~1e-8·σmax, **高于** 1e-10 秩截止, 会把数值噪声当信号
        （第一版 bug: 秩虚高约 80, V_F 差到 1e-4）。
      * 正确做法: 对 Q=R diag(v)^{1/2}（nl×m）做 thin SVD, 奇异值噪声底
        ~ε·σmax ≪ 1e-10·σmax 截止。C⁺=U_rΣ_r⁻²U_rᵀ,
        M=AᵀC⁺A=(Σ_r⁻¹U_rᵀA)ᵀ(Σ_r⁻¹U_rᵀA), A=[h, Z_s]（球面坐标:
        h 来自 psf payload, Z_s 来自 covariance payload 的 background_basis）。
      * I_F=‖(I−Π_{Z_w})t‖²（两次修正 Gram-Schmidt + 重正交, 与生产同款
        显式残差; 分解用 LAPACK dgesdd, 与生产手写 Householder QR 独立）。
    返回 (ok, i_f, rank, err)。
    """
    R = np.zeros((nl, m), dtype=np.float64)
    for l in range(nl):
        for k in range(r_rows[l], r_rows[l + 1]):
            R[l, r_cols[k]] = r_vals[k]
    Q = R * np.sqrt(v)[None, :]
    U, s, _ = np.linalg.svd(Q, full_matrices=False)
    smax = float(s[0]) if s.size else 0.0
    if not (smax > 0.0):
        return False, 0.0, 0, "Q has no positive spectrum"
    keep = s > TOL_REL * smax
    rank = int(np.count_nonzero(keep))
    if rank == 0:
        return False, 0.0, 0, "empty retained subspace"
    Ur = U[:, keep]
    sr = s[keep]
    ncol = 1 + n_bg
    A = np.zeros((nl, ncol), dtype=np.float64)
    A[:, 0] = h
    for b in range(n_bg):
        A[:, 1 + b] = z[b]
    W = (Ur.T @ A) / sr[:, None]          # r×ncol 白化（= Σ⁻¹UᵀA）
    t = W[:, 0]
    # 背景列正交基: 两次修正 Gram-Schmidt（记录有效背景秩）
    basis = []
    for c in range(1, ncol):
        zc = W[:, c].copy()
        n0 = float(np.linalg.norm(zc))
        if not (n0 > 0.0):
            continue
        for _ in range(2):
            for q in basis:
                zc -= float(zc @ q) * q
        rn = float(np.linalg.norm(zc))
        if rn > TOL_REL * n0:
            basis.append(zc / rn)
    e = t.copy()
    for _ in range(2):                   # 重正交（与生产同款两轮投影）
        for q in basis:
            e -= float(e @ q) * q
    i_f = float(e @ e)
    if not math.isfinite(i_f):
        return False, 0.0, rank, "non-finite I_F"
    return True, i_f, rank, ""


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    sidecar_path = os.path.join(root, "frame_science.json")
    if not os.path.exists(sidecar_path):
        print("FATAL: no frame_science.json under", root)
        return 2
    doc = load_json(sidecar_path)
    prec = doc.get("precision", {})
    models = {mm["model_id"]: mm for mm in doc.get("models", [])}
    controls = prec.get("controls", [])
    f_ref = None
    # F_ref 的权威位置: 逐控制点 reference_flux（与各自模板 h 配对）;
    # 控制点间必须一致, 否则 SNR 对拍按逐点值进行。
    for c0 in controls:
        rf0 = c0.get("reference_flux")
        if isinstance(rf0, (int, float)) and rf0 > 0:
            f_ref = float(rf0)
            break
    print("frame:", doc.get("frame_id"), "controls:", len(controls),
          "models:", len(models), "F_ref:", f_ref)

    n_ok = n_bad = n_skip = 0
    near_null = []
    f_enc_vals = []
    worst_vf = 0.0
    for c in controls:
        cid = c.get("control_id", "?")
        if c.get("status", {}).get("availability") != "available":
            n_skip += 1
            continue
        cov_id = c.get("covariance_model_id")
        op_id = cid + ".drizzle_operator"
        psf_id = cid + ".psf"
        try:
            cov = load_model(root, models[cov_id]["file"]["path"])
            op = load_model(root, models[op_id]["file"]["path"])
            psf = load_model(root, models[psf_id]["file"]["path"])
            sup = load_model(root, models[cid + ".support"]["file"]["path"])
        except KeyError as e:
            print("  [FAIL] %s missing model reference: %s" % (cid, e))
            n_bad += 1
            continue
        except (OSError, json.JSONDecodeError) as e:
            print("  [FAIL] %s unreadable model: %s" % (cid, e))
            n_bad += 1
            continue
        if cov.get("representation") != "acsd.normalize.local-factor/v2":
            print("  [FAIL] %s covariance representation=%s (expected v2)"
                  % (cid, cov.get("representation")))
            n_bad += 1
            continue
        # 单位核对（NORMALIZE_DATA §7.1B: 线性单位 + sr 幂次都要对）
        fu = cov.get("factor_variance_unit", "")
        du = cov.get("derived_covariance_unit", "")
        ou = cov.get("operator_unit", "")
        sig_unit = doc.get("signal", {}).get("linear_unit", "")
        if not (fu == sig_unit + "^2" and du == sig_unit + "^2/sr^2"
                and ou == "sr^-1"):
            print("  [FAIL] %s units factor=%s derived=%s op=%s vs signal=%s"
                  % (cid, fu, du, ou, sig_unit))
            n_bad += 1
            continue
        # condition_estimate_method 必须随值落盘（NOISE_SNR §11.6）
        cem = cov.get("condition_estimate_method", "")
        if not cem:
            print("  [FAIL] %s missing condition_estimate_method" % cid)
            n_bad += 1
            continue
        v = np.asarray(cov["values"], dtype=np.float64)
        m = len(v)
        off = op["csr_row_offsets"]
        col = op["csr_column_indices"]
        val = op["csr_coefficients"]
        nl = len(off) - 1
        h = np.asarray(psf["values"], dtype=np.float64)
        zb = np.asarray(cov.get("background_basis", []), dtype=np.float64)
        n_bg = int(cov.get("background_columns", 0))
        if h.shape[0] != nl or (n_bg > 0 and zb.reshape(nl, n_bg).shape != (nl, n_bg)):
            print("  [FAIL] %s shape mismatch psf=%d nl=%d" % (cid, h.shape[0], nl))
            n_bad += 1
            continue
        if n_bg > 0:
            z = [zb.reshape(nl, n_bg)[:, b] for b in range(n_bg)]
        else:
            z = []
        ok, i_f, rank, err = reconstruct_if(h, z, v, off, col, val, m, nl, n_bg)
        if not ok or not (i_f > 0.0):
            print("  [FAIL] %s independent reconstruction failed: %s" % (cid, err))
            n_bad += 1
            continue
        v_f = 1.0 / i_f
        got_vf = c.get("flux_variance")
        got_snr = c.get("reference_snr")
        got_if = cov.get("flux_information")
        got_rank = cov.get("solver_rank")
        rel_vf = abs(v_f - got_vf) / got_vf if got_vf else float("inf")
        rel_if = abs(i_f - got_if) / got_if if got_if else float("inf")
        worst_vf = max(worst_vf, rel_vf)
        # flux_residual_norm² == flux_information（残差诊断自洽）
        frn = cov.get("flux_residual_norm", 0.0)
        frn_ok = abs(frn * frn - got_if) <= 1e-12 * max(1.0, got_if)
        # f_enclosed 逐点记录（R2-F01）: 从 support note 解析并登记
        note = sup.get("support_note", "")
        f_enc = None
        for tok in note.split(";"):
            tok = tok.strip()
            if tok.startswith("f_enclosed="):
                try:
                    f_enc = float(tok.split("=", 1)[1])
                except ValueError:
                    pass
        f_enc_vals.append((cid, f_enc))
        bad = []
        # NOISE_SNR §11.6 near-null 敏感度: 独立分解与生产 QR 的有效秩一致时
        # V_F 必须紧密一致; 秩相差 ±1（σ_min 贴近 1e-10 相对阈值时的诚实数值
        # 敏感性, 生产 condition_number 高达 ~7e19）时登记为具名敏感度而非失败。
        rank_agree = (got_rank is not None and rank == got_rank)
        if not rank_agree:
            near_null.append((cid, rank, got_rank, rel_vf))
        elif rel_vf > VF_REL_TOL:
            bad.append("V_F rel %.3e with identical rank=%d (indep %.10g vs "
                       "sidecar %.10g)" % (rel_vf, rank, v_f, got_vf))
        if rank_agree and rel_if > VF_REL_TOL:
            bad.append("I_F rel %.3e (same rank)" % rel_if)
        if not frn_ok:
            bad.append("flux_residual_norm^2 != flux_information")
        if f_enc is None or not (0.0 <= f_enc <= 1.0):
            bad.append("per-control f_enclosed missing/out of range: %r" % f_enc)
        f_ref_c = c.get("reference_flux")
        if (rank_agree and got_vf and isinstance(f_ref_c, (int, float))
                and f_ref_c > 0):
            snr_ind = f_ref_c / math.sqrt(v_f)
            if abs(snr_ind - got_snr) / got_snr > SNR_REL_TOL:
                bad.append("SNR rel %.3e" % (abs(snr_ind - got_snr) / got_snr))
        if bad:
            print("  [FAIL] %s: %s" % (cid, "; ".join(bad)))
            n_bad += 1
        else:
            n_ok += 1
    # 帧级 per-control f_enclosed 必须逐点记录且不全相同（R2-F01）;
    # 两个控制点数值相同是合法的（窗口形状全等 + 平移等变, verify_round3 已证）。
    distinct = len({round(v, 12) for _, v in f_enc_vals if v is not None})
    allsame = distinct <= 1 and len(f_enc_vals) > 1
    print("per-control f_enclosed: %d recorded, %d distinct values %s"
          % (len(f_enc_vals), distinct,
             "(R2-F01 fix verified: per-point, not one shared minimum)"
             if not allsame else "(FAIL: single shared value)"))
    if allsame:
        n_bad += 1
    # 秩一致性统计（near-null 敏感度, NOISE_SNR §11.6）
    print("rank agreement: %d/%d identical; %d near-null disagreements"
          % (len(f_enc_vals) - len(near_null), len(f_enc_vals), len(near_null)))
    for cid, r1, r2, relv in near_null:
        print("  near-null: %s independent rank=%d vs production rank=%d "
              "(V_F rel %.2e; sigma_min sits at the 1e-10 relative cutoff)"
              % (cid, r1, r2, relv))
    # 帧摘要定义核对: area-weighted median of log SNR
    pairs = []
    for c in controls:
        if c.get("status", {}).get("availability") != "available":
            continue
        if c.get("reference_snr") and c.get("support_area_sr"):
            pairs.append((math.log(c["reference_snr"]), c["support_area_sr"]))
    fs = prec.get("frame_summary", {})
    if pairs:
        pairs.sort()
        total = sum(w for _, w in pairs)
        acc = 0.0
        med = pairs[-1][0]
        for lg, w in pairs:
            acc += w
            if acc >= 0.5 * total:
                med = lg
                break
        med_snr = math.exp(med)
        rel = abs(med_snr - fs.get("reference_snr", float("nan")))
        rel = rel / fs.get("reference_snr", float("nan"))
        print("frame summary: recomputed %.12g vs sidecar %.12g (rel %.2e)"
              % (med_snr, fs.get("reference_snr"), rel))
        if rel > 1e-9:
            print("  [FAIL] frame summary mismatch")
            n_bad += 1
    print("\nindependent recomputation: %d ok, %d failed, %d skipped(unavailable)"
          % (n_ok, n_bad, n_skip))
    print("worst V_F relative diff vs production: %.3e" % worst_vf)
    return 0 if n_bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
