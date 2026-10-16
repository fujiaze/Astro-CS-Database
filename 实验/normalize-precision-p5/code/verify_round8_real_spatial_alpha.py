#!/usr/bin/env python3
"""ROUND8 / 子任务 C: 真实 applied + spatial_gain 帧的 α 合同独立复核。

输入身份（按 AGENTS.md §13.1 从运行产物枚举, 不猜路径）:
  * <product_root>/frame_science.json + models/（本次运行的正式产品）;
  * <p1_phot.json>（同一运行的上游测光 provenance: photscales + spatial_gain）。

独立复算（不复用生产 helper, 多项式与中心化在 numpy 内重写）:
  1. α_j = k_photo · m(x_j,y_j)，m = 10^(−Σ_j c_j(B_j(x̃,ỹ) − center_j))，
     basis(order2) = [x̃, ỹ, x̃², x̃ỹ, ỹ²], x̃=(x−x_ref)/x_scale;
     与 sidecar psf payload 的 `source_pixel_alpha` 逐项对拍;
  2. α_*(u) 与 sidecar `reference_alpha_star` 对拍（u = 控制点 source_x0/y0）;
  3. 逐点 F_ref(u) = α_*(u)·F_native（F_native = 配置 flux；native_calibrated ⇒
     该位置 applied 总通量）与 control.reference_flux 对拍;
  4. h_sphere == R·planar_template（α 合同与算子自洽）;
  5. α_j 确实非恒等（证明真的走了空间路径, 不是退化为常量）。
用法: python3 verify_round8_real_spatial_alpha.py <product_root> <p1_phot.json>
退出码: 0=全部一致; 1=不一致。
"""
from __future__ import annotations

import json
import math
import os
import re
import sys

import numpy as np


def load(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def spatial_m(sg, x, y):
    order = int(sg.get("order", 0))
    if order <= 0:
        return 1.0
    n = 2 if order == 1 else 5
    coef = np.asarray(sg["coef"][:n], dtype=np.float64)
    ctr = np.asarray(sg["center"][:n], dtype=np.float64)
    nz = sg["norm"]
    xt = (x - float(nz["x_ref"])) / float(nz["x_scale"])
    yt = (y - float(nz["y_ref"])) / float(nz["y_scale"])
    b = [xt, yt]
    if n >= 5:
        b += [xt * xt, xt * yt, yt * yt]
    B = np.asarray(b, dtype=np.float64)
    return float(10.0 ** (-float(np.sum(coef * (B - ctr)))))


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    root, phot_path = sys.argv[1], sys.argv[2]
    doc = load(os.path.join(root, "frame_science.json"))
    models = {m["model_id"]: m["file"]["path"] for m in doc.get("models", [])}
    phot = load(phot_path)
    frame_key = doc["frame_id"]
    # 逐帧标量与空间场（provenance 的权威位置）
    k_photo = None
    for k, v in phot.get("photoscales", {}).items():
        if k == frame_key or frame_key in k or k in frame_key:
            k_photo = float(v)
            break
    if k_photo is None:
        k_photo = float(phot.get("photscal", 0.0))
    sg = None
    for k, v in phot.get("spatial_gain", {}).items():
        if k == frame_key or frame_key in k or k in frame_key:
            sg = v
            break
    print("frame:", frame_key)
    print("  k_photo = %.12e" % k_photo)
    print("  spatial_gain order=%s status=%s identifiable=%s n_stars=%s"
          % (sg.get("order"), sg.get("status"), sg.get("identifiable"),
             sg.get("n_stars")) if sg else "  spatial_gain: ABSENT")
    print("  photometry_applied=%s n_spatial_applied=%s"
          % (phot.get("photometry_applied"), phot.get("n_spatial_applied")))
    if not sg or int(sg.get("order", 0)) <= 0:
        print("FAIL: frame has no spatial gain model — not a spatial-α E2E")
        return 1

    f_native = float(doc["precision"]["controls"][0].get("reference_flux") or 0.0)
    # 配置声明 flux=1000 native_calibrated; F_native 由 sidecar 反推:
    # control.reference_flux = α_*·F_native ⇒ 用 provenance 的归一约定核对。
    n_ok = n_bad = 0
    worst_alpha = worst_astar = worst_fref = worst_h = 0.0
    alphas_all = []
    for c in doc["precision"]["controls"]:
        cid = c["control_id"]
        if c["status"]["availability"] != "available":
            continue
        psf = load(os.path.join(root, models[cid + ".psf"]))
        op = load(os.path.join(root, models[cid + ".drizzle_operator"]))
        sup = load(os.path.join(root, models[cid + ".support"]))
        idx = psf["source_pixel_index"]
        w = 4096
        xs = np.array([i % w for i in idx], dtype=np.float64)
        ys = np.array([i // w for i in idx], dtype=np.float64)
        alpha_pred = np.array([k_photo * spatial_m(sg, x, y)
                              for x, y in zip(xs, ys)])
        alpha_got = np.asarray(psf["source_pixel_alpha"], dtype=np.float64)
        rel_a = float(np.max(np.abs(alpha_pred - alpha_got)
                             / np.maximum(np.abs(alpha_got), 1e-300)))
        worst_alpha = max(worst_alpha, rel_a)
        # α_* at the control point
        sp0 = c.get("source_pixel0") or [c.get("source_x0"), c.get("source_y0")]
        cx, cy = float(sp0[0]), float(sp0[1])
        astar_pred = k_photo * spatial_m(sg, cx, cy)
        astar_got = float(psf["reference_alpha_star"])
        rel_s = abs(astar_pred - astar_got) / max(abs(astar_got), 1e-300)
        worst_astar = max(worst_astar, rel_s)
        # F_ref(u) = α_*·F_native; F_native = 配置 1000（native_calibrated）
        fref_pred = astar_pred * 1000.0
        fref_got = float(c.get("reference_flux") or 0.0)
        rel_f = abs(fref_pred - fref_got) / max(abs(fref_got), 1e-300)
        worst_fref = max(worst_fref, rel_f)
        # h_sphere == R·planar_template
        off, col, val = op["csr_row_offsets"], op["csr_column_indices"], op["csr_coefficients"]
        nl = len(off) - 1
        R = np.zeros((nl, len(idx)), dtype=np.float64)
        for l in range(nl):
            for k in range(off[l], off[l + 1]):
                R[l, col[k]] = val[k]
        h = np.asarray(psf["values"], dtype=np.float64)
        hp = np.asarray(psf["planar_template"], dtype=np.float64)
        rel_h = float(np.max(np.abs(R.dot(hp) - h)) / max(1e-300, float(np.max(np.abs(h)))))
        worst_h = max(worst_h, rel_h)
        alphas_all.append((alpha_got.min(), alpha_got.max()))
        bad = []
        if rel_a > 1e-6:
            bad.append("alpha rel %.3e" % rel_a)
        if rel_s > 1e-9:
            bad.append("alpha_star rel %.3e" % rel_s)
        if rel_f > 1e-9:
            bad.append("F_ref(u) rel %.3e" % rel_f)
        if rel_h > 1e-12:
            bad.append("h_sphere != R·planar rel %.3e" % rel_h)
        if bad:
            print("  [FAIL] %s: %s" % (cid, "; ".join(bad)))
            n_bad += 1
        else:
            n_ok += 1
    lo = min(a[0] for a in alphas_all)
    hi = max(a[1] for a in alphas_all)
    print("  controls ok=%d bad=%d" % (n_ok, n_bad))
    print("  worst rel: alpha=%.3e alpha_star=%.3e F_ref(u)=%.3e h=%.3e"
          % (worst_alpha, worst_astar, worst_fref, worst_h))
    print("  alpha range over controls: %.6e .. %.6e (non-identity=%s)"
          % (lo, hi, "yes" if hi / max(lo, 1e-300) > 1.0 + 1e-9 else "NO"))
    # α 与 k_photo 的比值 = m(x,y) 的空间变化幅度（真实空间通道是否被消费）
    print("  m range across all control pixels: %.6f .. %.6f"
          % (min(a[0] for a in alphas_all) / k_photo,
             max(a[1] for a in alphas_all) / k_photo))
    ok = (n_bad == 0 and n_ok > 0
          and hi / max(lo, 1e-300) > 1.0 + 1e-9
          and worst_alpha < 1e-6 and worst_astar < 1e-9
          and worst_fref < 1e-9 and worst_h < 1e-12)
    print("\n%s" % ("ALL PASS: real applied+spatial_gain α contract verified"
                    if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
