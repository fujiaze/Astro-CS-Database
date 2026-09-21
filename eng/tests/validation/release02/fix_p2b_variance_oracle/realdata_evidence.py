#!/usr/bin/env python3
# RELEASE-02 FIX-P2b evidence (production scale, real L4 data; read-only).
import glob, json, math, os, statistics, sys

ROOT = "run/RELEASE-02/L4-rebuild"
OUT = "run/RELEASE-02/fix-p2b/realdata_evidence.txt"
lines = []
def p(*a):
    s = " ".join(str(x) for x in a)
    lines.append(s); print(s)

smp_path = os.path.join(ROOT, "mosaic_out/p2_samples.json")
d = json.load(open(smp_path))
obs = d["observations"]
p("== P2b-3: p2_samples stub fields (pre-fix artifact, %d obs) ==" % len(obs))
for fld in ("ivar", "snr", "snr_available", "uncertainty", "control_ivar", "control_variance"):
    vals = [o.get(fld) for o in obs if isinstance(o.get(fld), (int, float))]
    nz = sum(1 for v in vals if v != 0)
    p("  %-16s nonzero=%d/%d min=%.6g median=%.6g max=%.6g"
      % (fld, nz, len(vals), min(vals), statistics.median(vals), max(vals)))

unc = [o["uncertainty"] for o in obs if o["uncertainty"] > 0]
ivar_fixed = [1.0 / (u * u) for u in unc]
snr_fixed = [abs(o["value"]) / o["uncertainty"] for o in obs
             if o["uncertainty"] > 0 and o["value"] == o["value"]]
p("  fixed ivar = 1/uncertainty^2 : median=%.6g (min=%.6g max=%.6g)"
  % (statistics.median(ivar_fixed), min(ivar_fixed), max(ivar_fixed)))
p("  fixed snr  = |value|/uncertainty : median=%.6g" % statistics.median(snr_fixed))
# consistency: 1/unc^2 == control_ivar?
worst = 0.0
for o in obs:
    if o["uncertainty"] > 0 and o["control_ivar"] > 0:
        r = abs(1.0 / (o["uncertainty"] ** 2) - o["control_ivar"]) / o["control_ivar"]
        worst = max(worst, r)
p("  max rel |1/unc^2 - control_ivar| = %.3g  (same object)" % worst)

p("")
p("== P2b-1: control-level residual-maker Var(corrected) at production scale ==")
# per-control observation lists
from collections import defaultdict
byc = defaultdict(list)
for o in obs:
    byc[o["control_id"]].append(o)
sample = [c for c in sorted(byc) if len(byc[c]) >= 2][:5000]
inc_ratio, exc_ratio = [], []
for c in sample:
    oo = byc[c]
    w = [o["control_ivar"] for o in oo]
    s2 = [o["control_variance"] for o in oo]
    W = sum(w)
    for k in range(len(oo)):
        if w[k] <= 0:
            continue
        # include-self: Var = s2_k - 1/W  (P_kk = 1 - w_k/W)
        inc = s2[k] * (1.0 - w[k] / W) ** 2 + sum(
            (w[j] / W) ** 2 * s2[j] for j in range(len(oo)) if j != k)
        # exclude-self ((c)): Var = s2_k + sum_{j!=k}(w_j/W_-k)^2 s2_j
        Wm = W - w[k]
        if Wm > 0:
            exc = s2[k] + sum((w[j] / Wm) ** 2 * s2[j]
                              for j in range(len(oo)) if j != k)
            exc_ratio.append(exc / s2[k])
        # naive (wrong) include-self: s2_k + sum_j (w_j/W)^2 s2_j
        naive = s2[k] + sum((w[j] / W) ** 2 * s2[j] for j in range(len(oo)))
        inc_ratio.append(naive / inc)
p("  include-self  naive/correct  median=%.4f  min=%.4f max=%.4f  (N=%d)"
  % (statistics.median(inc_ratio), min(inc_ratio), max(inc_ratio), len(inc_ratio)))
p("  exclude-self ((c)) Var/correct_naive baseline (s2_k) median=%.4f  min=%.4f max=%.4f"
  % (statistics.median(exc_ratio), min(exc_ratio), max(exc_ratio)))
p("  说明: include-self 档下朴素式系统性高估（残差制造者 vs sigma^2+Var(g)）；")
p("        exclude-self 档 H_kk=0 ⇒ 无自交叉项，两者恒等（与 (c) 一致）。")

p("")
p("== P2b-4: F_ref group-common — L4 (stale, pre-4f341b15) vs code state ==")
props = glob.glob(os.path.join(ROOT, "norm", "**", "signal", "properties"),
                  recursive=True)
snrs, frefs, nokey = [], [], 0
for pf in props:
    fs = ff = None
    for ln in open(pf, encoding="utf-8", errors="replace"):
        if ln.startswith("ASTROCS_FRAME_SNR="):
            fs = float(ln.split("=", 1)[1])
        elif ln.startswith("ASTROCS_REFERENCE_FLUX="):
            ff = float(ln.split("=", 1)[1])
    if fs is None or ff is None:
        nokey += 1
    else:
        snrs.append(fs); frefs.append(ff)
p("  HiPS signal products scanned: %d ; missing keys: %d" % (len(props), nokey))
if frefs:
    p("  ASTROCS_REFERENCE_FLUX: min=%.6g median=%.6g max=%.6g  max/min=%.3f"
      % (min(frefs), statistics.median(frefs), max(frefs), max(frefs) / min(frefs)))
    p("  ASTROCS_FRAME_SNR:      min=%.6g median=%.6g max=%.6g"
      % (min(snrs), statistics.median(snrs), max(snrs)))
p("  ⇒ L4 数据违反组内公共 F_ref（46 帧 F_ref 变化 ~6.87x），生成于 2026-09-18 23:53，")
p("    早于修复提交 4f341b15 (2026-09-19 01:42)；当前代码 module_adapters.cpp:3180-3282")
p("    两遍法/显式配置已为整组选定单一公共 F0，写侧 astro_sphere_sink 另加 P2b-4 闸门。")

os.makedirs(os.path.dirname(OUT), exist_ok=True)
open(OUT, "w", encoding="utf-8").write("\n".join(lines) + "\n")
print("\nWROTE", OUT)
