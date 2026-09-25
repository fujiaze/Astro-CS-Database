"""V2b: can ONE step-free structure give shipped ~7e-3 AND experiment-excess ~3e-5?

Scan a tanh nebular front (no step) whose steepest local slope is tuned so the shipped
gate reads exactly the documented M42 worst-edge value 7.2561e-3; vary the front width.
Ports are the same as v2_seam_stats.py.
"""
import numpy as np
import sys

sys.path.insert(0, ".")
from v2_seam_stats import (shipped_rel_step, experiment_stats, GATE)  # noqa: E402

NY, NX, XB = 512, 1024, 512
yy, xx = np.mgrid[0:NY, 0:NX].astype(float)
B = 1200.0
TARGET = 7.2561e-3        # docs/science/PHASE2_UPM.md:475 M42 reference product max|rel_step|
g = TARGET * B / 4.0      # slope that reproduces it exactly at the steepest point

print("shipped gradient identity: |rel_step| = 4*(dL/dn)/bg  => required slope g = %.6f ADU/px "
      "(%.4f %%/px)" % (g, 100 * g / B))
print("\n t (front width, px)   A (ADU)   shipped|rel_step|   exp|rel_step|   exp|rel_excess|  "
      "ship/exc")
for t in (8.0, 16.0, 32.0, 64.0, 100.0, 150.0, 200.0, 300.0, 500.0, 1000.0):
    A = 4.0 * t * g                    # max slope of tanh front = A/(4t)
    x0 = XB                            # steepest point of 1/(1+exp(-(x-x0)/t)) is x = x0
    img = B + A / (1.0 + np.exp(-(xx - x0) / t))
    s = abs(shipped_rel_step(img, XB)["rel_step"])
    e = experiment_stats(img, XB)
    se = abs(e["rel_excess"])
    print("  %8.0f          %8.1f     %.4e        %.4e       %.4e       %s"
          % (t, A, s, abs(e["rel_step"]), se, ("%.0fx" % (s / se)) if se else "inf"))

print("\n--- same family, but boundary 3t downstream of the front (flank, not steepest) ---")
for t in (32.0, 100.0, 300.0):
    A = 4.0 * t * g
    img = B + A / (1.0 + np.exp(-(xx - (XB - 3 * t)) / t))
    s = abs(shipped_rel_step(img, XB)["rel_step"])
    e = experiment_stats(img, XB)
    print("  t=%6.0f: shipped %.4e  exp|rel_step| %.4e  exp|rel_excess| %.4e"
          % (t, s, abs(e["rel_step"]), abs(e["rel_excess"])))
