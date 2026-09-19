#!/usr/bin/env python3
"""SNR-DESIGN EXP-4: analytic control-point-density vs dense-SNR reconstruction error.

Model: the intra-frame SNR field is treated as a realisation of a stationary 2-D
Gaussian process with correlation length ell.  We compute, as a function of the
control-point spacing Delta (in units of ell):

  * kriging (GP) prediction std at the worst point of a square cell, using a
    3x3 and a 5x5 neighbourhood of control points;
  * bilinear interpolation std (the operator already implemented in
    lib/algorithms/integration/v6/src/weight_chain.cpp: bilinear_regular_grid_v1);
  * the fraction of the field variance carried by scales below Delta (the
    irreducible part that NO interpolator on a Delta-grid can recover).

Both a squared-exponential (SE, very smooth) and an exponential (OU, rougher)
covariance are reported; the SE case is the optimistic bound, the OU case the
pessimistic one.

Conversion to the final product (EXP-1):  delta = 2*eps  =>  weight penalty
Var_approx/Var_opt = 1 + 4*Var(eps).  We report the worst-case (independent
errors) penalty 1 + 4*eps^2 and the sigma penalty sqrt(1+4 eps^2)-1.

Run: TMPDIR=/dev/shm/astrocs_snrd python3 exp4_kriging_scaling.py --out exp4_kriging_scaling.json
"""
import argparse, json
import numpy as np


def cov(r, ell, kind):
    r = np.asarray(r, float)
    if kind == "se":
        return np.exp(-0.5 * (r / ell) ** 2)
    if kind == "ou":
        return np.exp(-r / ell)
    raise ValueError(kind)


def kriging_var(delta, ell, kind, nhalf):
    """Ordinary-kriging variance at the centre of a square cell of side delta,
    using a (2*nhalf+1)^2 node neighbourhood.  Returns variance/sigma^2."""
    # control points sit at CELL CORNERS; the query is the cell CENTRE
    off = (np.arange(-nhalf, nhalf + 1) + 0.5) * delta
    Y, X = np.meshgrid(off, off, indexing="ij")
    nodes = np.column_stack([X.ravel(), Y.ravel()])
    D = np.sqrt(((nodes[:, None, :] - nodes[None, :, :]) ** 2).sum(-1))
    C = cov(D, ell, kind)
    C = C + 1e-10 * np.eye(len(nodes))          # tiny nugget for conditioning
    k = cov(np.sqrt((nodes ** 2).sum(-1)), ell, kind)   # cov(centre, node)
    w = np.linalg.solve(C, k)
    return float(1.0 - k @ w)


def bilinear_var(delta, ell, kind, nhalf=1):
    """Variance of bilinear interpolation error at the cell centre, assuming the
    four corner nodes are noise-free (same assumption as kriging_var)."""
    off = np.array([-delta / 2, delta / 2])
    Y, X = np.meshgrid(off, off, indexing="ij")
    nodes = np.column_stack([X.ravel(), Y.ravel()])
    D = np.sqrt(((nodes[:, None, :] - nodes[None, :, :]) ** 2).sum(-1))
    C = cov(D, ell, kind) + 1e-10 * np.eye(4)
    k = cov(np.sqrt((nodes ** 2).sum(-1)), ell, kind)
    w = np.linalg.solve(C, k)                   # kriging weights == 1/4 for SE?
    wb = np.full(4, 0.25)                       # bilinear weights at the centre
    # Var(f0 - sum wb f_i) = sigma^2 - 2 wb.k + wb.C.wb
    return float(1.0 - 2.0 * wb @ k + wb @ C @ wb)


def lowpass_fraction(delta, ell, kind, n=4000):
    """Fraction of the field variance carried by scales BELOW delta, i.e. the
    part a Delta-grid cannot represent (1-D proxy, isotropic 2-D spectral
    integral evaluated numerically)."""
    k = np.linspace(1e-4, 60.0 / ell, n)
    if kind == "se":
        # 2-D SE power spectrum (radial) ~ exp(-k^2 ell^2)
        P = np.exp(-(k * ell) ** 2) * k
    else:
        P = (1.0 + (k * ell) ** 2) ** -1.5 * k
    kc = 2.0 * np.pi / delta
    tot = np.trapezoid(P, k)
    lo = np.trapezoid(P[k <= kc], k[k <= kc])
    return float(lo / tot)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="exp4_kriging_scaling.json")
    a = ap.parse_args()

    rows = []
    for kind in ("se", "ou"):
        for ratio in (0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0):
            v3 = kriging_var(ratio, 1.0, kind, 1)
            v5 = kriging_var(ratio, 1.0, kind, 2)
            vb = bilinear_var(ratio, 1.0, kind)
            lp = lowpass_fraction(ratio, 1.0, kind)
            e = float(np.sqrt(max(v5, 0.0)))
            rows.append(dict(
                kernel=kind, delta_over_ell=ratio,
                kriging_std_3x3=float(np.sqrt(max(v3, 0.0))),
                kriging_std_5x5=e,
                bilinear_std=float(np.sqrt(max(vb, 0.0))),
                bilinear_over_kriging=(float(np.sqrt(max(vb, 0.0))) / e
                                       if e > 0 else None),
                fraction_resolved_by_grid=lp,
                irreducible_fraction_below_delta=float(1.0 - lp),
                weight_penalty_pct_worstcase=float(100.0 * 4.0 * e * e),
                sigma_penalty_pct_worstcase=float(100.0 * (np.sqrt(1 + 4 * e * e) - 1)),
            ))
    out = dict(
        exp="EXP-4 analytic control-point density vs dense-SNR reconstruction error",
        note="variance normalised to the field variance sigma^2; ell = 1",
        scan=rows,
    )
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)

    for kind in ("se", "ou"):
        print(f"== kernel = {kind} (ell = 1) ==")
        print("%8s %14s %14s %14s %10s %12s %12s"
              % ("D/ell", "krig_std_3x3", "krig_std_5x5", "bilinear_std",
                 "bil/krig", "irred<D", "sig_pen%"))
        for r in rows:
            if r["kernel"] != kind:
                continue
            print("%8.2f %14.4f %14.4f %14.4f %10.3f %12.4f %12.4f"
                  % (r["delta_over_ell"], r["kriging_std_3x3"],
                     r["kriging_std_5x5"], r["bilinear_std"],
                     r["bilinear_over_kriging"] or float("nan"),
                     r["irreducible_fraction_below_delta"],
                     r["sigma_penalty_pct_worstcase"]))
        print()
    print("wrote", a.out)


if __name__ == "__main__":
    main()
