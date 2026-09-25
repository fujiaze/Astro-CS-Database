"""V2d: self-checks of the v2 ports against independently stated closed forms.

(1) gate's own d-scan identity (docs/science/PHASE2_UPM.md:431 and seam_footprint.py docstring):
    pure level step -> |rel_step_d4x| / |rel_step| = 1 ; pure gradient -> 4.
(2) gate's own deterministic detection floor (:451): rel_step = Delta/(L+Delta/2),
    so Delta/L = 1.0050% reads exactly 1e-2 -> boundary case.
(3) the draft's stated fixture ("3 ADU/px 平滑梯度") under several background levels:
    shipped reads 4g/bg, so 3 ADU/px -> 7.3e-3 requires bg = 4*3/7.3e-3 = 1643.8 ADU.
(4) experiment statistic on a STRICTLY linear ramp is identically zero (order-2 detrend).
"""
import numpy as np
import sys

sys.path.insert(0, ".")
from v2_seam_stats import shipped_rel_step, experiment_stats  # noqa: E402

NY, NX, XB = 512, 1024, 512
yy, xx = np.mgrid[0:NY, 0:NX].astype(float)
B = 1200.0

# (1) pure step, no gradient
d = 12.0
img = B + d * (xx >= XB)
s = shipped_rel_step(img, XB)
print("(1) pure step Delta=%.1f ADU, no gradient: d-scan ratio = %.6f (identity says 1)"
      % (d, s["d_scan_ratio"]))
img = B + 2.176830 * (xx - XB)
s = shipped_rel_step(img, XB)
print("    pure linear ramp, no step:            d-scan ratio = %.6f (identity says 4)"
      % s["d_scan_ratio"])

# (2) deterministic floor boundary case
for pct in (1.0049, 1.0050, 1.0051, 1.0100):
    d = pct / 100.0 * B
    img = B + d * (xx >= XB)
    s = shipped_rel_step(img, XB)
    closed = d / (B + d / 2.0)
    print("(2) Delta/L=%.4f%%: shipped |rel_step|=%.8f closed form=%.8f verdict=%s"
          % (pct, abs(s["rel_step"]), closed, "GREEN" if abs(s["rel_step"]) <= 1e-2 else "RED"))

# (3) the draft's stated slope under different bg
print("(3) shipped identity rel_step = 4g/bg:")
for g in (3.0, 2.17683):
    for bg in (1200.0, 1643.8):
        print("    g=%.5f bg=%.1f -> %.6e" % (g, bg, 4 * g / bg))

# (4) strictly linear ramp through the experiment statistic
img = B + 3.0 * (xx - XB)
e = experiment_stats(img, XB)
print("(4) experiment on STRICTLY linear ramp g=3: step=%.3e excess=%.3e (analytically 0)"
      % (e["step"], e["excess"]))
img = B + 3.0 * (xx - XB) + 1e-5 * (xx - XB) ** 2
e = experiment_stats(img, XB)
print("    same + tiny curvature 1e-5/px^2:      step=%.3e excess=%.3e" % (e["step"], e["excess"]))
