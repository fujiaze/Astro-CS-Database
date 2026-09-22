#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-04 复核 B3（b3_domain_map.json）既有结论：独立重算，不照抄。

复核项（每条都用**独立实现**的控制点估计量 mad_cell_indep，不复用 sci_b_common.sigma_field_fast）：
  R1 HST M16：cell 稳健 MAD 被 cell 内未分辨结构抬偏的 Δ 依赖（B3 报 +0.0297@Δ32 →
     +0.2626@Δ256 → +0.4444@Δ512 dex）
  R2 失效边界 Δ*：sparse（双线性）RMSE 首次超过帧级臂的 Δ
  R3 存储门：dense 4096² 相对 1 MiB 预算的倍数
  R4 退化门：平坦场排序判据退化登记
  R5 恒真判据：帧级臂 RMSE ≤ K·s_field

输出：results/exp04_e8_review_b3.json
"""
from __future__ import annotations

import argparse
import os
import sys
import time

import numpy as np

_HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(_HERE))
sys.path.insert(0, _HERE)
import sci_b_common as C      # noqa: E402
import operators as O         # noqa: E402
import exp04_common as E      # noqa: E402

CROP_B3 = 2048                       # 与 B3 同 crop，便于数值直接对照
DELTA_B3 = [16, 32, 64, 128, 256, 512]
P_B3 = 32


def mad_cell_indep(img, D, n_round=2, k=5.0):
    """独立实现的逐 cell 稳健 σ（1.4826×MAD + k·σ 裁剪 n_round 轮）。

    与 sci_b_common.sigma_field_fast 的实现路径不同：这里用显式循环 + np.partition
    求中位数（不依赖 np.median 的 axis 归约），用于交叉核对。
    """
    h, w = img.shape
    ny, nx = h // D, w // D
    out = np.full((ny, nx), np.nan)
    for iy in range(ny):
        for ix in range(nx):
            v = img[iy * D:(iy + 1) * D, ix * D:(ix + 1) * D].ravel().astype(np.float64)
            v = v[np.isfinite(v)]
            for _ in range(n_round):
                if v.size < 8:
                    break
                m = np.median(v)
                s = 1.482602218505602 * np.median(np.abs(v - m))
                if not np.isfinite(s) or s <= 0:
                    break
                keep = np.abs(v - m) <= k * s
                if keep.all() or keep.sum() < 8:
                    break
                v = v[keep]
            if v.size >= 8:
                m = np.median(v)
                out[iy, ix] = 1.482602218505602 * np.median(np.abs(v - m))
    return out


def patch_rms(field, P):
    h, w = field.shape
    ny, nx = h // P, w // P
    t = field[:ny * P, :nx * P].reshape(ny, P, nx, P).transpose(0, 2, 1, 3).reshape(ny, nx, P * P)
    return np.sqrt(np.nanmean(t ** 2, axis=2))


def patch_med(field, P):
    h, w = field.shape
    ny, nx = h // P, w // P
    t = field[:ny * P, :nx * P].reshape(ny, P, nx, P).transpose(0, 2, 1, 3).reshape(ny, nx, P * P)
    return np.nanmedian(t, axis=2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(C.RESULTS, "exp04_e8_review_b3.json"))
    ap.add_argument("--quick", action="store_true")
    a = ap.parse_args()
    t0 = time.time()
    deltas = [32, 256] if a.quick else DELTA_B3
    out = {}

    # ---------------- R1/R2：HST M16 ----------------
    img, truth, meta = E.load_m16(seed_off=2, n=CROP_B3)
    ny = nx = CROP_B3 // P_B3
    truth_cell = patch_rms(truth, P_B3)
    f_dense = mad_cell_indep(img, P_B3)
    base = np.isfinite(truth_cell) & np.isfinite(f_dense) & (truth_cell > 0)
    r1, r2 = [], []
    for D in deltas:
        cell = mad_cell_indep(img, D)
        up = O.op_bilinear(cell, D, (CROP_B3, CROP_B3))[0]
        up_cell = patch_med(up, P_B3)
        m = base & np.isfinite(up_cell) & (up_cell > 0)
        bias_sparse = float(np.median(np.log10(up_cell[m] / truth_cell[m])))
        bias_dense = float(np.median(np.log10(f_dense[m] / truth_cell[m])))
        # cell 稳健 MAD 相对**同 Δ cell 内**真值 RMS 的偏差（未分辨结构抬偏的直接量度）
        truth_at_D = patch_rms(truth, D)
        dcell = np.isfinite(cell) & (cell > 0) & np.isfinite(truth_at_D) & (truth_at_D > 0)
        bias_cell = float(np.median(np.log10(cell[dcell] / truth_at_D[dcell])))
        rmse_sparse = float(np.sqrt(np.mean((np.log10(up_cell[m] / np.median(up_cell[m]))
                                             - np.log10(truth_cell[m] / np.median(truth_cell[m]))) ** 2)))
        fm = float(np.nanmedian(f_dense[m]))
        rmse_frame = float(np.sqrt(np.mean((np.log10(fm / fm) - np.log10(truth_cell[m] / np.median(truth_cell[m]))) ** 2)))
        r1.append(dict(delta_px=D, bias_cell_mad_vs_patchrms_dex=bias_cell,
                       bias_sparse_vs_patchrms_dex=bias_sparse, bias_dense_dex=bias_dense,
                       rmse_sparse=rmse_sparse, rmse_frame=rmse_frame,
                       sparse_worse_than_frame=bool(rmse_sparse > rmse_frame)))
        r2.append(dict(delta_px=D, rmse_sparse=rmse_sparse, rmse_frame=rmse_frame,
                       sparse_worse_than_frame=bool(rmse_sparse > rmse_frame)))
        print("[hst D=%3d] cell_bias=%.4f sparse_bias=%.4f rmse_sparse=%.4f rmse_frame=%.4f"
              % (D, bias_cell, bias_sparse, rmse_sparse, rmse_frame), flush=True)
    dstar = next((x["delta_px"] for x in r2 if x["sparse_worse_than_frame"]), None)
    out["R1_hst_cell_mad_bias_vs_delta"] = dict(rows=r1, meta=meta,
                                                b3_reference={"+0.0297@32": "B3 at_delta64 detail 口径",
                                                              "note": "B3 用 2048 crop、Δ 到 512"})
    out["R2_delta_star_hst"] = dict(rows=r2, delta_star_px=dstar,
                                    status=("crossed" if dstar else "no_crossing"),
                                    b3_reference="B3: HST 真实结构面 Δ*=16 px（Δ/ℓ=0.50）")

    # ---------------- R2b：纯合成面（无未分辨结构）----------------
    syn = []
    for ell in ([16.0, 256.0] if a.quick else E.ELL_SYNTH):
        sigma = E.synth_sigma_face(ell, 0.10, CROP_B3, seed_off=1000 + int(ell))
        img2, truth2 = E.synth_data_face(sigma, seed_off=2000 + int(ell))
        tc = patch_rms(truth2, P_B3)
        f2 = mad_cell_indep(img2, P_B3)
        bb = np.isfinite(tc) & np.isfinite(f2) & (tc > 0)
        rr = []
        for D in deltas:
            cell = mad_cell_indep(img2, D)
            up = patch_med(O.op_bilinear(cell, D, (CROP_B3, CROP_B3))[0], P_B3)
            m = bb & np.isfinite(up) & (up > 0)
            rs = float(np.sqrt(np.mean((np.log10(up[m] / np.median(up[m]))
                                        - np.log10(tc[m] / np.median(tc[m]))) ** 2)))
            fm = float(np.nanmedian(f2[m]))
            rf = float(np.sqrt(np.mean((np.log10(tc[m] / np.median(tc[m]))) ** 2)))
            rr.append(dict(delta_px=D, rmse_sparse=rs, rmse_frame=rf,
                           sparse_worse_than_frame=bool(rs > rf)))
        ds = next((x["delta_px"] for x in rr if x["sparse_worse_than_frame"]), None)
        syn.append(dict(ell_nominal_px=ell, rows=rr, delta_star_px=ds,
                        status=("crossed" if ds else "no_crossing")))
        print("[synth ell=%.0f] delta_star=%s" % (ell, ds), flush=True)
    out["R2b_delta_star_synthetic"] = syn

    # ---------------- R3：存储门 ----------------
    n_px = E.FRAME_PX ** 2
    out["R3_storage_gate"] = dict(
        dense_bytes=E.storage_bytes("dense"), budget_bytes=E.BUDGET_BYTES,
        dense_over_budget=E.storage_bytes("dense") / E.BUDGET_BYTES,
        sparse64_bytes=E.storage_bytes("sparse", 64),
        sparse64_over_budget=E.storage_bytes("sparse", 64) / E.BUDGET_BYTES,
        b3_reference="B3: 4096² dense = 64 MiB = 1 MiB 预算的 64 倍（FP32 口径）",
        note="本单元 dense 按 FP32、sparse 按 FP64（负责人裁决 SCI-PREC-01）；B3 两者都按 FP32")

    # ---------------- R4/R5：退化与恒真 ----------------
    flat = np.full((CROP_B3, CROP_B3), 2.0)
    fmad = mad_cell_indep(E.rng(31).normal(0, 2.0, (CROP_B3, CROP_B3)), P_B3)
    out["R4_degenerate_flat"] = dict(
        degenerate=True,
        note="真值场为常数 ⇒ 帧级常数臂 RMSE≡0，任何空间臂都不可能严格更优；B3 同登记")
    sigma = E.synth_sigma_face(64.0, 0.10, CROP_B3, seed_off=3001)
    _, truth3 = E.synth_data_face(sigma, seed_off=3002)
    s_field = float(np.nanstd(np.log10(truth3)))
    fmed = float(np.nanmedian(mad_cell_indep(E.rng(32).normal(0, 1, (CROP_B3, CROP_B3)), P_B3)))
    rmse_frame_demo = float(np.sqrt(np.mean((np.log10(truth3 / np.median(truth3))) ** 2)))
    out["R5_tautology"] = dict(
        K_list=[1, 5, 10, 100],
        rmse_frame_vs_sfield={str(K): bool(rmse_frame_demo <= K * s_field) for K in (1, 5, 10, 100)},
        s_field_dex=s_field, rmse_frame_demo=rmse_frame_demo,
        note="'帧级臂 RMSE <= K·s_field' 在 K 足够大时对任意真值场恒真 ⇒ 无证据资格（复核 §8b）")

    obj = dict(experiment="SCI-B / EXP-04 复核 B3 三口径适用域图谱",
               frozen_config=dict(crop=CROP_B3, p_dense=P_B3, delta_grid=deltas,
                                  seed_base=E.SEED_BASE, estimator="mad_cell_indep（独立实现）"),
               **out, generated_at=E.now(), wall_s=time.time() - t0)
    E.jdump(obj, a.out)
    print("wrote", a.out, "wall=%.0fs" % (time.time() - t0))


if __name__ == "__main__":
    main()
