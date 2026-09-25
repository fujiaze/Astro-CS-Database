"""V2 independent recompute: shipped seam gate statistic vs archived-experiment statistic.

Two ports written FROM SCRATCH from source (no repo import):

 (1) shipped machine gate  eng/tools/e2e/seam_footprint.py
       bilinear()      :245-256
       edge_metric()   :300-376  -> seam = img(p+n*d) - img(p-n*d), d = 2 px
                                     step = median(seam)            (criterion numerator)
                                     bg   = median(|img(p±n*d)|)    (local level)
                                     rel_step = step / bg           (gate: max|rel_step| <= 1e-2)
     ctrl line / step_net / v1 excess are diagnostics only (not in the criterion).

 (2) archived experiment criterion  实验/additive-sky-seamless/code/sci_c_common.py
       _step_at()    :356-374   quadratic (order=2) baseline fit over +/-base_win(64),
                                excluding [xb-halfwin-2, xb+halfwin-2], then
                                step = median(residual R halfwin) - median(residual L halfwin)
       seam_steps()  :377-410   profile m(x) = nanmedian_y mosaic
                                excess = step(xb) - median(step(xb+d)), d in (-96,-64,-32,32,64,96)
                                rel    = step / median(m in window)
     (this is the statistic behind C4/N5 'strong smooth common gradient stays green')

Structures below have NO step anywhere: they are purely coherent background.
"""
import numpy as np

D = 2.0                 # gate default sampling half-distance
CTRL_SHIFT = 200.0      # gate applicability requirement (both sides placeable)
MIN_SAMPLES = 20
GATE = 1e-2


def bilinear(img, x, y):
    """port of seam_footprint.bilinear (:245-256)"""
    h, w = img.shape
    ok = np.isfinite(x) & np.isfinite(y) & (x >= 1) & (y >= 1) & (x <= w - 2) & (y <= h - 2)
    xi = np.clip(x, 0, w - 2)
    yi = np.clip(y, 0, h - 2)
    x0 = np.floor(xi).astype(int)
    y0 = np.floor(yi).astype(int)
    fx = xi - x0
    fy = yi - y0
    v = (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy) +
         img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)
    return np.where(ok, v, np.nan)


def shipped_rel_step(img, xb, y0=0, y1=None, n_pts=256):
    """port of edge_metric's criterion (vertical frame boundary at x=xb).

    edge points walked in +y: gradient t=(0,1) -> n = (-ty, tx) = (-1,0), so
    seam = img(xb-2) - img(xb+2) (the gate takes max|rel_step|, sign irrelevant).
    """
    if y1 is None:
        y1 = img.shape[0]
    ex = np.full(n_pts, float(xb))
    ey = np.linspace(y0, y1 - 1, n_pts)
    tx = np.gradient(ex)
    ty = np.gradient(ey)
    tn = np.hypot(tx, ty)
    tn[tn == 0] = 1.0
    tx, ty = tx / tn, ty / tn
    nxv, nyv = -ty, tx
    seam = (bilinear(img, ex + nxv * D, ey + nyv * D) -
            bilinear(img, ex - nxv * D, ey - nyv * D))
    ok = np.isfinite(seam)
    # applicability: ctrl line placeable at +/-CTRL_SHIFT on BOTH sides
    nside = {}
    for s in (+1.0, -1.0):
        c = (bilinear(img, ex + nxv * (s * CTRL_SHIFT + D), ey + nyv * (s * CTRL_SHIFT + D)) -
             bilinear(img, ex + nxv * (s * CTRL_SHIFT - D), ey - nyv * (s * CTRL_SHIFT - D)))
        nside[s] = int((ok & np.isfinite(c)).sum())
    interior = nside[+1.0] >= MIN_SAMPLES and nside[-1.0] >= MIN_SAMPLES
    step = float(np.median(seam[ok]))
    lvl = np.concatenate([np.abs(bilinear(img, ex + nxv * D, ey + nyv * D))[ok],
                          np.abs(bilinear(img, ex - nxv * D, ey - nyv * D))[ok]])
    bg = float(np.median(lvl))
    # d-scan diagnostic shipped in the gate (pure step -> ratio 1, pure gradient -> 4)
    seam4 = (bilinear(img, ex + nxv * 4 * D, ey + nyv * 4 * D) -
             bilinear(img, ex - nxv * 4 * D, ey - nyv * 4 * D))
    step4 = float(np.median(seam4[np.isfinite(seam4)]))
    return dict(interior=interior, step=step, bg=bg, rel_step=step / bg,
                rel_step_d4x=step4 / bg,
                d_scan_ratio=(step4 / step) if step else float("nan"))


def _step_at(m, xb, halfwin=8, base_win=64, order=2, min_valid=8):
    """port of sci_c_common._step_at (:356-374)"""
    m = np.asarray(m)
    nx = m.size
    lo, hi = max(0, xb - base_win), min(nx, xb + base_win)
    xs = np.arange(lo, hi)
    good = np.isfinite(m[lo:hi])
    excl = (xs >= xb - halfwin - 2) & (xs <= xb + halfwin - 2)
    fitm = good & (~excl)
    if fitm.sum() < order + 2:
        return np.nan, np.nan
    coef = np.polyfit(xs[fitm], m[lo:hi][fitm], order)
    res = m[lo:hi] - np.polyval(coef, xs)
    L = res[(xs >= xb - halfwin) & (xs <= xb - 1)]
    R = res[(xs >= xb) & (xs <= xb + halfwin - 1)]
    L = L[np.isfinite(L)]
    R = R[np.isfinite(R)]
    if L.size < min_valid or R.size < min_valid:
        return np.nan, float(np.nanmedian(m[lo:hi]))
    return float(np.median(R) - np.median(L)), float(np.nanmedian(m[lo:hi]))


def experiment_stats(img, xb, offsets=(-96, -64, -32, 32, 64, 96)):
    """port of sci_c_common.seam_steps (:377-410): profile + detrend + off-locus excess"""
    m = np.nanmedian(img, axis=0)
    step, lvl = _step_at(m, xb)
    ctrl = []
    for d in offsets:
        xc = xb + d
        if 0 <= xc < m.size:
            s2, _ = _step_at(m, xc)
            if np.isfinite(s2):
                ctrl.append(s2)
    excess = float(step - np.median(ctrl)) if ctrl else float("nan")
    return dict(step=step, level=lvl, excess=excess,
                rel_step=step / lvl, rel_excess=excess / lvl)


def make(ny=512, nx=1024, xb=512):
    yy, xx = np.mgrid[0:ny, 0:nx].astype(float)
    return yy, xx, xb


def report(name, img, xb):
    s = shipped_rel_step(img, xb)
    e = experiment_stats(img, xb)
    print("\n%s" % name)
    print("  shipped   : interior=%s step=%+.6e bg=%.4f  |rel_step|=%.6e  (%.1f%% of 1e-2 gate)"
          % (s["interior"], s["step"], s["bg"], abs(s["rel_step"]), 100 * abs(s["rel_step"]) / GATE))
    print("  shipped d-scan: rel_step_d4x=%+.6e  ratio=%+.3f (1=pure step, 4=pure gradient)"
          % (s["rel_step_d4x"], s["d_scan_ratio"]))
    print("  experiment: step=%+.6e excess=%+.6e level=%.4f | rel_step=%.3e rel_excess=%.3e"
          % (e["step"], e["excess"], e["level"], abs(e["rel_step"]), abs(e["rel_excess"])))
    print("  verdict with 1e-2 gate: %s" % ("PASS (green)" if abs(s["rel_step"]) <= GATE else "FAIL (red)"))
    return abs(s["rel_step"]), abs(e["rel_excess"])


def main():
    print("structural identities (analytic, before numerics):")
    print("  shipped   rel_step(linear ramp g)  = 2*d*g/bg = 4g/bg  -> gradient NOT removed")
    print("  experiment _step_at(linear/quadratic) = 0 exactly (order-2 fit absorbs it)")
    print("  experiment excess(tanh/sine)          = O((halfwin/L)^k) residual, then off-locus median")

    yy, xx, xb = make()
    B = 1200.0                      # M42-like local background level (ADU)

    # (a) pure coherent linear ramp tuned so the shipped gate reads the documented
    #     M42 worst edge max|rel_step| = 7.2561e-3 (docs/science/PHASE2_UPM.md:475)
    target = 7.2561e-3
    g = target * B / 4.0
    img = B + g * (xx - xb)
    report("(a) PURE LINEAR RAMP, no step; slope tuned to the documented M42 worst reading "
           "(g=%.6f ADU/px = %.4f%%/px of bg)" % (g, 100 * g / B), img, xb)

    # (b) physically shaped nebular front (tanh), no step, max slope tuned to same reading
    w = 100.0
    A = 4 * w * g                   # so max dL/dx = A/(4w) = g
    img = B + A / (1.0 + np.exp(-(xx - (xb + 3 * w)) / w))
    report("(b) TANH NEBULAR FRONT amplitude %.1f ADU over ~%.0f px, step-free, "
           "steepest point 300 px left of the boundary" % (A, 4 * w), img, xb)

    # (c) smooth multi-scale sky field like sci_c_common.smooth_sky_field (waves 420/260)
    img = (B + 0.15 * B * np.sin(2 * np.pi * xx / 420.0)
           + 0.08 * B * np.sin(2 * np.pi * (xx + 0.3 * yy) / 260.0))
    report("(c) TWO-SINE SMOOTH SKY FIELD (lambda 420/260 px), step-free", img, xb)

    # (d) pure quadratic curvature (centred ON the boundary -> zero local slope; and off-centre)
    img = B + 3e-4 * (xx - xb) ** 2
    report("(d0) PURE QUADRATIC CURVATURE centred on the boundary, step-free", img, xb)
    img = B + 3e-4 * (xx - (xb - 200)) ** 2
    report("(d1) PURE QUADRATIC CURVATURE centred 200 px away, step-free", img, xb)

    # (e) control: a TRUE step of exactly 1.0050% (the gate's deterministic detection floor),
    #     on top of the linear ramp -> shows what the experiment's excess reads for a real seam
    delta = 0.010050 * B
    img_no = B + g * (xx - xb)
    img_st = img_no + delta * (xx >= xb)
    sn, en = report("(e0) control: ramp only (no step)", img_no, xb)
    sx, ex_ = report("(e1) same ramp + TRUE STEP Delta/L = 1.0050%% (%.4f ADU)" % delta, img_st, xb)
    print("\n  detection power on the SAME structure: shipped |rel_step| %.3e -> %.3e (x%.1f)"
          % (sn, sx, sx / sn))
    print("  experiment |rel_excess| %.3e -> %.3e (x%.1f)" % (en, ex_, ex_ / max(en, 1e-300)))

    print("\n=== ratio of the two criteria on step-free structure (a)-(d) ===")
    for nm, im in (("a linear ramp", B + g * (xx - xb)),
                   ("b tanh front", B + A / (1 + np.exp(-(xx - (xb + 3 * w)) / w))),
                   ("c two-sine field", B + 0.15 * B * np.sin(2 * np.pi * xx / 420)
                    + 0.08 * B * np.sin(2 * np.pi * (xx + 0.3 * yy) / 260)),
                   ("d quadratic", B + 3e-4 * (xx - (xb - 200)) ** 2)):
        s = abs(shipped_rel_step(im, xb)["rel_step"])
        e = abs(experiment_stats(im, xb)["rel_excess"])
        print("  %s: shipped %.4e | experiment-excess %.4e | shipped/experiment = %s"
              % (nm, s, e, ("%.1f" % (s / e)) if e > 0 else "inf (experiment exactly 0)"))

    # where does the divergence live? scan the coherent structure's characteristic scale
    print("\n=== scale scan: step-free sine of period L, amplitude fixed at 15%% of bg ===")
    print("  period(px)  shipped|rel_step|  exp|rel_step|  exp|rel_excess|  ship/excess")
    for per in (40.0, 80.0, 160.0, 260.0, 420.0, 800.0, 2000.0, 8000.0):
        im = B + 0.15 * B * np.sin(2 * np.pi * xx / per)
        s = abs(shipped_rel_step(im, xb)["rel_step"])
        ee = experiment_stats(im, xb)
        se = abs(ee["rel_excess"])
        print("  %9.0f   %.6e      %.6e     %.6e       %s"
              % (per, s, abs(ee["rel_step"]), se,
                 ("%.1f" % (s / se)) if se > 0 else "inf"))


if __name__ == "__main__":
    main()
