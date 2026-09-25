"""V4/V5 numeric backbone: what the two tolerance_relative settings actually mean at
production scale, and which terminal convergence state each path lands in.

Facts pinned from source (HEAD c8f64e9a):
  upm.h:78-80      tolerance semantics: 0 = absolute (max_dM/max_dC < tolerance)
                                     1 = relative   (< tolerance * max(scale,1))
  upm.h:113        struct default: int tolerance_relative = 0
  upm.cpp:712-730  scale_obs = median(|obs.value|)  (upper-median, nth_element mid)
  upm.cpp:1035-1040 tol_M = tol_C = tolerance * max(scale_obs, 1.0) when relative
  upm.cpp:1041-1063 order of checks: converged(1) test FIRST, then stall counter
  upm.cpp:732-733  kStallPatience = 5, kObjImproveFloor = 1e-12 (function-local const)
  module_adapters.cpp:9571,9573   scheduler path: tolerance=1e-6, tolerance_relative=1
  p2_session.cpp:204              session path:   tolerance=1e-6, tolerance_relative unset -> 0
  docs/science/PHASE2_UPM.md:98-116   SCI-UPM-CONV-001「容差随观测尺度归一，禁绝对容差」
                   「生产必须 tolerance_relative=1」; scale from 1210 ADU/px / Omega_px
"""
import math

DBL_EPS = 2.220446049250313e-16
TOL = 1e-6                      # both paths set the same number
REL_PATIENCE = 5
FLOOR = 1e-12


def ulp(x):
    return abs(x) * DBL_EPS if x else DBL_EPS


def report(label, scale_obs):
    rel_thr = TOL * max(scale_obs, 1.0)
    print("\n%s" % label)
    print("  scale_obs = %.6e" % scale_obs)
    print("  ulp(scale_obs) [smallest representable step]      = %.6e" % ulp(scale_obs))
    print("  ABSOLUTE criterion (p2_session.cpp:204, rel=0): threshold = %.6e" % TOL)
    print("     reachable?  min step %.6e  >  threshold %.6e  => %s"
          % (ulp(scale_obs), TOL,
             "NO  (state 1 unreachable -> stall counter wins => converged=2)"
             if ulp(scale_obs) > TOL else "yes"))
    print("     threshold / ulp = %.3e  (abs thr is %.3g x of one ulp)"
          % (TOL / ulp(scale_obs), TOL / ulp(scale_obs)))
    print("  RELATIVE criterion (module_adapters.cpp:9573, rel=1): threshold = %.6e" % rel_thr)
    print("     reachable?  min step %.6e  <  threshold %.6e  => %s (threshold = %.3g x ulp)"
          % (ulp(scale_obs), rel_thr, "yes (state 1 reachable)" if ulp(scale_obs) < rel_thr
             else "NO", rel_thr / ulp(scale_obs)))
    print("  in pixel units (ADU/px, Omega_px=2.2991e-11 sr): abs thr=%.3e  rel thr=%.3e"
          % (TOL * 2.2991e-11, rel_thr * 2.2991e-11))
    return rel_thr


# production surface-brightness scale (ADU sr^-1) as recorded in the science doc
sky_adu_px = 1210.0     # docs/science/PHASE2_UPM.md:111-112 (M42 真帧天空 1210 ADU/px)
omega_px = 2.2991e-11
report("PRODUCTION C-field scale (docs/science/PHASE2_UPM.md:111-112: 1210 ADU/px / "
       "Omega_px=2.2991e-11 sr => ~5.26e13)", sky_adu_px / omega_px)

# the experiment probe scale (e-) used by SCI-C c1/c5 arms
report("SCI-C experiment probe scale (~300 e- per control value)", 300.0)

# small synthetic (near-zero scale) => max(scale,1.0) degenerates to absolute
report("small synthetic case, scale_obs = 0.5 (max(...,1) floor active)", 0.5)

print("\n=== ordering consequence (upm.cpp:1041-1063) ===")
print("  step test (=>1) is evaluated BEFORE the stall counter (=>2):")
print("    state 2 needs  steps >= tol  AND  rel_improve(objective) < 1e-12 for 5 consecutive rounds")
print("  with the absolute criterion at production scale, tol=1e-6 is %.3g x BELOW ulp, so"
      % (TOL / ulp(sky_adu_px / omega_px)))
print("  state 1 is unachievable by construction; the only terminal states reachable on that")
print("  path are 2 (stalled, once the objective plateaus at FP floor) or 0 (iteration budget).")
print("  max_iterations=100 (both paths) >= patience 5, so 2 is reachable within the budget.")

print("\n=== which archived arms could ever show 2? ===")
print("  c5_weights.json production_convergence.iters = [4, 1, 300]")
print("    iters=4  -> state 1 (relative criterion met fast)")
print("    iters=1  -> state 1 (single-shot: max_iterations=1 fixture)")
print("    iters=300-> state 0 (max_iter exhaustion at 300 rounds; NOT a 5-round stall)")
print("  observed values in the tracked corpus: {0, 1} only (see v5_output.txt census)")
print("  and the four-state code landed at d77fd11f (2026-09-22), one day AFTER those")
print("  result files were last written by 9a2b5d11 (2026-09-21).")
