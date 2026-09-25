"""V1 independent recompute: control_ivar under a constant sky patch.

Ported FROM SCRATCH (no repo import) from the arithmetic in
  lib/algorithms/coverage/src/sampler.cpp :795-877  (patch statistic + control_variance/ivar)
  lib/algorithms/coverage/src/sky_plane.cpp :285-307 (same floor, single-patch variant)

Source facts pinned at HEAD c8f64e9a:
  sampler.cpp:864  const double sigma = (s0 > 0.0) ? s0 : 1e-12;
  sampler.cpp:872  const double n_ret = std::max((double)n_retained, 1.0);
  sampler.cpp:873  kcorr_f = frames[f].kcorr > 0 ? frames[f].kcorr : cfg.control_k_corr
  sampler.cpp:875  cvar = kcorr_f * kPiHalf * sigma * sigma / n_ret
  sampler.cpp:877  civar = cvar > 0.0 ? 1.0 / cvar : 0.0
  sampler.cpp:83-84  kControlCorrDefault = 1.4 ; kPiHalf = pi/2
  sampler.cpp:306-308 default background_patch_radius = 8 (=> 17x17 = 289), clip_sigma = 3.0, iters = 3
"""
import math

MAD_TO_SIGMA = 1.482602218505602   # sampler.cpp:846
K_PI_HALF = 1.57079632679489661923  # sampler.cpp:84
K_CORR = 1.4                        # sampler.cpp:83
RADIUS = 8                          # default background_patch_radius
CLIP_SIGMA = 3.0
CLIP_ITERS = 3
MIN_SAMPLES = 5
FLOOR = 1e-12                       # sampler.cpp:864 literal


def median_of(v):
    """sampler.cpp:199 median_of (odd n -> true median; even n -> upper-hinge style)."""
    if not v:
        return 0.0
    s = sorted(v)
    n = len(s)
    mid = n // 2
    if n % 2 == 1:
        return s[mid]
    # sampler.cpp:199-210 returns the mean of the two central elements
    return 0.5 * (s[mid - 1] + s[mid])


def patch_stat(vals, radius=RADIUS, clip_sigma=CLIP_SIGMA, iters=CLIP_ITERS,
               k_corr=K_CORR, floor=FLOOR):
    """Replicate sampler.cpp :795-877 exactly: median, MAD-sigma, bright-end clip, floor, cvar, civar."""
    n_total = len(vals)
    if n_total < MIN_SAMPLES:
        return dict(accepted=False, reason=1)
    m0 = median_of(vals)
    s0 = MAD_TO_SIGMA * median_of([abs(v - m0) for v in vals])
    ret = list(vals)
    for _ in range(iters):
        nr = [v for v in ret if v <= m0 + clip_sigma * s0]
        if len(nr) < MIN_SAMPLES:
            break
        nm = median_of(nr)
        if abs(nm - m0) < 1e-12 * max(abs(m0), 1e-12):
            ret = nr
            break
        m0 = nm
        ret = nr
        s1 = MAD_TO_SIGMA * median_of([abs(v - m0) for v in ret])
        if s1 <= 0.0:
            break
        s0 = s1
    sigma_raw = s0                                  # what §5.4 calls sigma_bg_raw
    sigma = s0 if s0 > 0.0 else floor               # sampler.cpp:864
    n_retained = len(ret)
    n_ret = max(float(n_retained), 1.0)
    cvar = k_corr * K_PI_HALF * sigma * sigma / n_ret
    civar = 1.0 / cvar if cvar > 0.0 else 0.0
    return dict(accepted=True, value=m0, sigma_raw=sigma_raw, sigma_used=sigma,
                n_total=n_total, n_retained=n_retained, cvar=cvar, civar=civar)


def civar_if_no_floor(sigma_raw, n_retained, k_corr=K_CORR):
    """Analytic limit with the floor removed, i.e. the spec's 'sigma_raw = 0' branch."""
    cvar = k_corr * K_PI_HALF * sigma_raw * sigma_raw / float(max(n_retained, 1))
    return (1.0 / cvar) if cvar > 0.0 else 0.0


def main():
    side = 2 * RADIUS + 1
    print("patch side = %dx%d = %d pixels (default radius %d)" % (side, side, side * side, RADIUS))

    # (1) constant background block -- the case under audit
    const = [7.0] * (side * side)
    r = patch_stat(const)
    print("\n[A] CONSTANT PATCH (pure coherent background, zero variance)")
    for k in ("value", "sigma_raw", "sigma_used", "n_total", "n_retained", "cvar", "civar"):
        print("   %-11s = %.6e" % (k, r[k]))
    print("   civar (repr) = %r" % r["civar"])
    print("   analytic limit without floor: cvar=0 -> civar = %r (spec §5.4 wants 0)"
          % civar_if_no_floor(0.0, r["n_retained"]))

    # closed form of the floored value
    closed = r["n_retained"] / (K_CORR * K_PI_HALF * FLOOR * FLOOR)
    print("   closed form n_ret/(k_corr*pi/2*1e-24) = %.6e" % closed)

    # (2) constant patch with a different level -> scale invariance of the pathology
    for level in (0.0, 1e-6, 1.0, 5.0e4):
        rr = patch_stat([level] * (side * side))
        print("   level=%-8g civar=%.6e" % (level, rr["civar"]))

    # (3) realistic noisy control for contrast (sigma_bg = 1 ADU)
    import random
    rnd = random.Random(20260925)
    noisy = [7.0 + rnd.gauss(0.0, 1.0) for _ in range(side * side)]
    rn = patch_stat(noisy)
    print("\n[B] NOISY PATCH (sigma_true = 1.0 ADU) for contrast")
    print("   sigma_raw=%.6e cvar=%.6e civar=%.6e" % (rn["sigma_raw"], rn["cvar"], rn["civar"]))

    ratio = r["civar"] / rn["civar"]
    print("\n[C] HIJACK RATIO  civar(constant)/civar(noisy) = %.6e  (log10 = %.2f)"
          % (ratio, math.log10(ratio)))

    # (4) per-control normalisation consequence: upm.cpp:668-672 raw_w/sums*reliability
    print("\n[D] PER-CONTROL WEIGHT NORMALISATION (upm.cpp:668-672, reliability=1)")
    peers = [rn["civar"]] * 7 + [r["civar"]]   # 7 normal frames + 1 constant patch
    raws = peers
    s = sum(raws)
    wnorm = [x / s for x in raws]
    for i, w in enumerate(wnorm):
        print("   obs %d: raw=%.6e -> w_norm=%.10f%s" % (i, raws[i], w,
              "   <-- constant patch" if i == 7 else ""))
    print("   sum(w_norm) = %.12f" % sum(wnorm))

    # (5) production reference scale quoted in docs/algorithms/PHASE2_SAMPLER.md:82 (5.6e-22)
    print("\n[E] against production median control_ivar 5.6e-22 (PHASE2_SAMPLER.md:82)")
    print("   civar(constant)/5.6e-22 = %.6e (log10 = %.2f)"
          % (r["civar"] / 5.6e-22, math.log10(r["civar"] / 5.6e-22)))

    # (6) sky_plane.cpp single-patch variant: same floor, n_retained dependent
    print("\n[F] sky_plane.cpp:296-307 variant (same 1e-12 floor)")
    for n_ret in (289, 441, 1):
        cvar = K_CORR * K_PI_HALF * FLOOR * FLOOR / float(n_ret)
        print("   n_retained=%-4d cvar=%.6e ivar=%.6e" % (n_ret, cvar, 1.0 / cvar))


if __name__ == "__main__":
    main()
