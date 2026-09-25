"""V2c: does the coherent-background term in the SHIPPED criterion cancel a real seam?

The shipped criterion is a SIGNED median of the +-d difference, so on a boundary that also
carries a normal gradient g it reads  rel_step = (Delta + 4*g) / bg : the gradient term adds
algebraically. A gradient of opposite sign therefore cancels a genuine level step.

Test: inject a TRUE step of exactly the gate's own deterministic detection floor
Delta/L = 1.0050% (docs/science/PHASE2_UPM.md:451) and superpose the step-free ramp that the
same gate reads 7.2561e-3 on (the documented M42 worst-edge magnitude, :475), with opposite sign.
Compare against the archived experiment criterion (detrended + off-locus excess).
"""
import numpy as np
import sys

sys.path.insert(0, ".")
from v2_seam_stats import shipped_rel_step, experiment_stats, GATE  # noqa: E402

NY, NX, XB = 512, 1024, 512
yy, xx = np.mgrid[0:NY, 0:NX].astype(float)
B = 1200.0
FLOOR = 0.010050                 # gate's own deterministic detection floor (Delta/L)
G_RAMP = 7.2561e-3 * B / 4.0     # slope the shipped gate reads 7.2561e-3 on (documented M42 max)

delta = FLOOR * B
print("true step injected: Delta = %.4f ADU (Delta/L = %.4f%% >= floor 1.0050%%)"
      % (delta, 100 * FLOOR))
print("ramp slope g = %.6f ADU/px  -> 4g/bg = %.6e (opposing sign below)\n" % (G_RAMP, 4 * G_RAMP / B))

for tag, slope in (("no ramp", 0.0),
                   ("ramp aiding the step  (+)", +G_RAMP),
                   ("ramp opposing the step (-)", -G_RAMP),
                   ("ramp opposing, 1.02x tuned for exact cancel", -1.02 * G_RAMP)):
    base = B + slope * (xx - XB)
    lvl_right = B + slope * 0.0
    img = base + delta * (xx >= XB)
    s = shipped_rel_step(img, XB)
    e = experiment_stats(img, XB)
    print("%-44s shipped rel_step=%+.6e (%+6.1f%% of gate) verdict=%-4s | "
          "exp rel_excess=%+.6e verdict_vs_its_own_scale=%s"
          % (tag, s["rel_step"], 100 * s["rel_step"] / GATE,
             "GREEN" if abs(s["rel_step"]) <= GATE else "RED",
             e["rel_excess"], "RED-ish" if abs(e["rel_excess"]) > GATE else "n/a"))

print("\n--- how big a real step survives an opposing 7.2561e-3-class gradient? ---")
for frac in (0.5, 0.7, 0.9, 1.005, 1.2, 1.5, 2.0):
    d = frac / 100.0 * B
    img = (B - G_RAMP * (xx - XB)) + d * (xx >= XB)
    s = shipped_rel_step(img, XB)
    e = experiment_stats(img, XB)
    print("  Delta/L = %6.3f%% : shipped rel_step=%+.4e -> %-5s | experiment rel_excess=%+.4e"
          % (frac, s["rel_step"],
             "GREEN" if abs(s["rel_step"]) <= GATE else "RED", e["rel_excess"]))
