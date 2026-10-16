#!/usr/bin/env python3
"""ROUND4 独立读回: 从发布 sidecar 复核 NOISE_SNR §2 总通量口径的 α 合同。

判据（全部由独立 numpy 路径给出, 不复用生产 helper）:
  1. psf 模型必须携带 `planar_template`（h_j=(α_j/α_*)·P_j）、`source_pixel_alpha`
     （α_j, 与 source_pixel_index 同长）、`reference_alpha_star`（α_*）;
  2. **h_sphere == R · planar_template**: 用同控制点 drizzle_operator 的 CSR
     独立组装 R（nl×m）, 与 psf payload 的 values（h_sphere）逐项对拍;
  3. 窗口标度自洽: `alpha_window_scale`（support note, = Σ_j α_jP_j）
     == α_* · Σ_j planar_template_j（α_j 在 Σα_jP_j 中与 (α_j/α_*)P_j 相消,
     该恒等式对任意 α 场成立, 与是否空间 α 无关）;
  4. 长度/有限性: len(alpha)==len(source_pixel_index)==len(planar_template)==m,
     α_j>0 有限, α_*>0 有限;
  5. α≡1 帧（photometry 缺席）: planar_template ≡ P 且 α_j≡1、α_*=1; 同时
     h_sphere == R·P 由 (2) 覆盖。

用法: python3 verify_round6_alpha_contract.py <frame_product_root>
退出码: 0=全部通过; 1=存在不一致或数据错误。
"""
from __future__ import annotations

import json
import os
import re
import sys

import numpy as np

REL_TOL = 1e-12


def load(root, rel):
    with open(os.path.join(root, rel), "r", encoding="utf-8") as f:
        return json.load(f)


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: verify_round6_alpha_contract.py <frame_product_root>")
        return 2
    root = sys.argv[1]
    doc = load(root, "frame_science.json")
    prec = doc.get("precision", {})
    controls = prec.get("controls", [])
    models = {m["model_id"]: m["file"]["path"] for m in doc.get("models", [])}
    # 帧级参考归一身份: 只有**未施加**测光的帧才对 α≡1 作断言; 施加了（空间或
    # 常量）标度的帧的 α 由真实 m(x,y) 决定（另见 verify_round8_real_spatial_alpha.py）。
    scale_state = doc.get("signal", {}).get("scale_state", "")
    applied = bool(doc.get("photometry", {}).get("scale_applied", False))
    print("frame:", doc.get("frame_id"), "controls:", len(controls),
          "scale_state:", scale_state)

    n_ok = n_bad = n_skip = 0
    worst_h = 0.0
    worst_scale = 0.0
    alpha_nonone = 0
    for c in controls:
        cid = c.get("control_id", "?")
        if c.get("status", {}).get("availability") != "available":
            n_skip += 1
            continue
        try:
            psf = load(root, models[cid + ".psf"])
            op = load(root, models[cid + ".drizzle_operator"])
            sup = load(root, models[cid + ".support"])
        except (KeyError, OSError, json.JSONDecodeError) as e:
            print("  [FAIL] %s model load: %s" % (cid, e))
            n_bad += 1
            continue
        bad = []
        # 1/4. 字段存在与形状/有限性
        if "planar_template" not in psf or "source_pixel_alpha" not in psf \
                or "reference_alpha_star" not in psf:
            bad.append("psf payload missing alpha-contract fields")
        else:
            h = np.asarray(psf["values"], dtype=np.float64)
            hp = np.asarray(psf["planar_template"], dtype=np.float64)
            al = np.asarray(psf["source_pixel_alpha"], dtype=np.float64)
            astar = float(psf["reference_alpha_star"])
            idx = psf.get("source_pixel_index", [])
            if not (hp.shape == al.shape == (len(idx),)):
                bad.append("shape m mismatch hp=%s alpha=%s idx=%d"
                           % (hp.shape, al.shape, len(idx)))
            if not (np.all(np.isfinite(hp)) and np.all(np.isfinite(al))
                    and np.all(al > 0.0)):
                bad.append("non-finite/non-positive planar_template|alpha")
            if not (np.isfinite(astar) and astar > 0.0):
                bad.append("reference_alpha_star not positive finite")
            # 2. h_sphere == R · planar_template
            off = op["csr_row_offsets"]
            col = op["csr_column_indices"]
            val = op["csr_coefficients"]
            nl = len(off) - 1
            m = len(idx)
            if h.shape != (nl,):
                bad.append("h_sphere shape %s != nl=%d" % (h.shape, nl))
            else:
                R = np.zeros((nl, m), dtype=np.float64)
                for l in range(nl):
                    for k in range(off[l], off[l + 1]):
                        R[l, col[k]] = val[k]
                hrec = R.dot(hp)
                den = max(1e-300, float(np.max(np.abs(h))))
                rel = float(np.max(np.abs(hrec - h))) / den
                worst_h = max(worst_h, rel)
                if rel > REL_TOL:
                    bad.append("h_sphere != R·planar_template (rel %.3e)" % rel)
            # 3. alpha_window_scale == α_* · Σ planar_template（对任意 α 恒等）
            note = sup.get("support_note", "")
            mtok = re.search(r"alpha_window_scale=([0-9.eE+-]+)", note)
            if not mtok:
                bad.append("support note missing alpha_window_scale")
            else:
                # note 以 %.9g 写 alpha_window_scale（9 位有效数字）, 文本舍入
                # 相对误差 ~1e-9 量级 ⇒ 该对拍容差取 1e-7（文本精度, 不是物理
                # 容差; 数值本身另有 16 位精度的产品路径核对）。
                aws = float(mtok.group(1))
                pred = astar * float(np.sum(hp))
                d = abs(pred - aws) / max(1e-300, abs(aws))
                worst_scale = max(worst_scale, d)
                if d > 1e-7:
                    bad.append("alpha_window_scale %.12g != alpha_*·Σplanar %.12g"
                               " (rel %.3e)" % (aws, pred, d))
            # 5. 未施加标度的帧: 逐像元 α 与 α_* 必须都为 1（诚实中性）
            if not applied:
                if not (np.all(al == 1.0) and astar == 1.0):
                    bad.append("unscaled frame but alpha not identically 1")
            if not np.all(al == 1.0):
                alpha_nonone += 1
        if bad:
            print("  [FAIL] %s: %s" % (cid, "; ".join(bad)))
            n_bad += 1
        else:
            n_ok += 1

    print("alpha-contract read-back: %d ok, %d failed, %d skipped"
          % (n_ok, n_bad, n_skip))
    print("worst |h_sphere - R·planar_template| / max|h| = %.3e" % worst_h)
    print("worst |alpha_window_scale - alpha_*·Σplanar| rel = %.3e" % worst_scale)
    print("controls with α_j not identically 1: %d" % alpha_nonone)
    return 0 if n_bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
