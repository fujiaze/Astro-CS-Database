#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""S06 图件：只用已落盘的真实读数绘制，不合成曲线。

输入（run/ACSDSDOC-02/S06/results/）：
  S06_B_overshoot.json               过冲曲线读数 + PCHIP 对照
  S06_C_holdout_cv.json              六臂留出 CV + dense/cell/frame
  probe_s06_results.json             生产域外状态机 + 钳制咬合
输出（run/ACSDSDOC-02/S06/figures/）：
  fig_S06_b_overshoot.png            自然样条 vs PCHIP（y=[0,2,0,0]）
  fig_S06_c_domain_out_of_domain.png 2×2/3×3/全幅 域外占比（生产读数）
  fig_S06_c_arm_var_ratio.png        六臂留出方差比（Δ=16）
  fig_S06_c_dense_vs_cell.png        逐场型 E_frame/E_cell/E_dense

命令：python3 实验/dense-snr-reconstruct/code/s06/s06_figures.py
"""
from __future__ import annotations

import json
import os
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np

from s06_common import natural_spline_eval, out_dir, pchip_eval, run_meta, sha256_file

SCRIPT = "s06_figures.py"
COMMAND = "python3 实验/dense-snr-reconstruct/code/s06/s06_figures.py"
OUT = out_dir()
FIG = Path(os.environ.get("S06_FIG", "run/ACSDSDOC-02/S06/figures"))
FIG.mkdir(parents=True, exist_ok=True)

B = json.loads((OUT / "S06_B_overshoot.json").read_text(encoding="utf-8"))
C = json.loads((OUT / "S06_C_holdout_cv.json").read_text(encoding="utf-8"))
P = json.loads((OUT / "probe_s06_results.json").read_text(encoding="utf-8"))

made = []

# ---------------------------------------------------------------- fig 1 过冲
y = np.array([0.0, 2.0, 0.0, 0.0])
x = np.array([0.0, 1.0, 2.0, 3.0])
q = np.linspace(0, 3, 1201)
fig, ax = plt.subplots(figsize=(7.2, 4.4))
ax.plot(q, natural_spline_eval(y, x, q), lw=2,
        label="natural cubic spline (unclipped)")
ax.plot(q, pchip_eval(x, y, q), lw=2, ls="--",
        label="Fritsch–Carlson monotone Hermite (PCHIP)")
ax.axhline(2.0, color="k", lw=1, ls=":", label="max(node) = 2")
ax.plot(x, y, "ko", ms=5, label="nodes [0,2,0,0]")
mx = B["cases"]["y_0_2_0_0"]
ax.annotate(f"max = {mx['max_value']:.9f}\novershoot = +{mx['overshoot_abs']:.6e}\n"
            f"min = {mx['min_value']:.6f}",
            xy=(mx["argmax_x"], mx["max_value"]), xytext=(1.55, 1.15),
            arrowprops=dict(arrowstyle="->", lw=1))
ax.set_xlabel("x [node units]")
ax.set_ylabel("S(x) [same unit as nodes]")
ax.set_title("S06-B: natural spline overshoot/undershoot vs shape-preserving PCHIP\n"
             f"(y=[0,2,0,0], seed={B['seed']}, script sha={B['script_sha256'][:12]})")
ax.legend(fontsize=8, loc="upper right")
ax.grid(alpha=0.3)
fig.tight_layout()
p = FIG / "fig_S06_b_overshoot.png"
fig.savefig(p, dpi=140)
plt.close(fig)
made.append(str(p))

# ------------------------------------------------- fig 2 生产域外占比
rows = [e for e in P["experiments"] if e.get("id") == "S06_D1_domain_states"]
labels = [f"{e['case']}\n{e['nx']}x{e['ny']}, dx={e['dx']:g}" for e in rows]
vals = [e["frac_out_of_domain"] * 100.0 for e in rows]
colors = ["#c0392b" if v > 0 else "#27ae60" for v in vals]
fig, ax = plt.subplots(figsize=(8.6, 4.2))
bars = ax.bar(range(len(rows)), vals, color=colors)
for i, (b, e) in enumerate(zip(bars, rows)):
    ax.text(b.get_x() + b.get_width() / 2, b.get_height() + 0.8,
            f"{vals[i]:.2f}%\n{e['G_domain_complete_verdict']}",
            ha="center", fontsize=8)
ax.set_xticks(range(len(rows)))
ax.set_xticklabels(labels, fontsize=8)
ax.set_ylabel("out-of-domain query fraction [%]")
ax.set_ylim(0, 65)
ax.set_title("S06-D1 (production SparseSnrReconstructor::eval): out-of-domain fraction\n"
             "guard G_domain_complete: prepare_ok ⇒ fraction == 0 (RED for 2x2/3x3)")
ax.grid(alpha=0.3, axis="y")
fig.tight_layout()
p = FIG / "fig_S06_c_domain_out_of_domain.png"
fig.savefig(p, dpi=140)
plt.close(fig)
made.append(str(p))

# ------------------------------------------------- fig 3 六臂方差比
recs = [r for r in C["records"] if r.get("protocol") == "P" and r.get("status") == "PASS"
        and r["delta"] == 16]
fields = [r["field"] for r in recs]
arms = ["constant", "bilinear", "idw_p2", "idw_p1", "natural_spline_clip", "dense"]
fig, ax = plt.subplots(figsize=(10.5, 4.6))
w = 0.13
xs = np.arange(len(fields))
for k, a in enumerate(arms):
    ys = []
    for r in recs:
        m = r["arms"].get(a, {})
        ys.append(m.get("var_ratio") if m.get("var_ratio") is not None else np.nan)
    ax.bar(xs + (k - 2.5) * w, ys, width=w, label=a)
ax.axhline(1.0, color="k", lw=1, ls=":")
ax.set_yscale("log")
ax.set_xticks(xs)
ax.set_xticklabels(fields, rotation=12)
ax.set_ylabel("holdout variance ratio  Var_w/Var_opt  (1 = optimal, log scale)")
ax.set_title("S06-C: six-arm node-level holdout CV at Δ=16 (W=H=192)\n"
             "train = even-index nodes, score = interior pixels of held-out cells")
ax.legend(fontsize=8, ncol=3)
ax.grid(alpha=0.3, axis="y")
fig.tight_layout()
p = FIG / "fig_S06_c_arm_var_ratio.png"
fig.savefig(p, dpi=140)
plt.close(fig)
made.append(str(p))

# ------------------------------------------------- fig 4 dense vs cell vs frame
S = C["subexperiment_S_dense_vs_cell_vs_frame"]
fig, ax = plt.subplots(figsize=(9.5, 4.4))
xs = np.arange(len(S))
for k, key in enumerate(("E_eff_frame", "E_eff_cell", "E_eff_dense")):
    ax.bar(xs + (k - 1) * 0.26, [r[key] for r in S], width=0.26,
           label=key.replace("E_eff_", ""))
ax.set_xticks(xs)
ax.set_xticklabels([r["field"] for r in S], rotation=12)
ax.set_ylabel("E_eff = Var_w/Var_opt − 1  (0 = optimal)")
ax.set_yscale("symlog", linthresh=1e-3)
ax.set_title("S06-C sub-experiment S: dense vs cell vs frame, full data at Δ=64\n"
             "(production 8×8 MAD² + cell plane fit nodes; re-check of E_dense=0.577 vs E_cell=0.00903)")
ax.legend(fontsize=8)
ax.grid(alpha=0.3, axis="y")
fig.tight_layout()
p = FIG / "fig_S06_c_dense_vs_cell.png"
fig.savefig(p, dpi=140)
plt.close(fig)
made.append(str(p))

manifest = {
    **run_meta(SCRIPT, COMMAND),
    "figures": [{"path": f, "sha256": sha256_file(f)} for f in made],
    "inputs": {n: {"path": str(OUT / n), "sha256": sha256_file(OUT / n)}
               for n in ("S06_B_overshoot.json", "S06_C_holdout_cv.json",
                         "probe_s06_results.json")},
    "note": "所有曲线/柱来自上述真实读数文件，无合成曲线",
}
(FIG / "S06_figures_manifest.json").write_text(
    json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
print(f"[S06-fig] wrote {len(made)} figures to {FIG}")
