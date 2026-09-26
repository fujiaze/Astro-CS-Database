#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P4-03: sparse control-point -> dense SNR-field reconstruction.

Reproduces, independently and in miniature, the domain map of the P4 module:
given a true (slowly varying) sigma field sampled at control points on a
Delta x Delta grid, reconstruct the dense field with the frozen operator
vocabulary and measure E (weight-efficiency loss) and RMSE(dex).

Arms:
  operators : bilinear, natural bicubic spline + value clamp,
              mesh(3x3 median) + bicubic + clamp, IDW(p=2,K=16), nearest
  spacing   : Delta in {16, 32, 64, 128, 256}
  domains   : smooth slow field (correlation length ell=128 px, "ground seeing"),
              high-contrast field (cell-level spikes, "HST-like"),
              flat field (negative control: no-effect => metric zero)

Theory legs checked numerically:
  T1 bilinear interpolation error on a smooth field scales ~ O(Delta^2)
  T2 cubic-spline error scales ~ O(Delta^4)
Standalone, numpy only, seed fixed.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp03_sparse_dense_reconstruction.json"
SIZE = 1024


def grf_field(size, ell, seed):
    """Gaussian random field with Gaussian power spectrum, correlation length ell."""
    rng = np.random.default_rng(seed)
    ky = np.fft.fftfreq(size)[:, None]
    kx = np.fft.fftfreq(size)[None, :]
    k2 = kx**2 + ky**2
    k2[0, 0] = 1.0
    amp = np.exp(-2.0 * (np.pi * ell) ** 2 * k2)
    amp[0, 0] = 0.0
    white = rng.normal(size=(size, size))
    f = np.fft.ifft2(np.fft.fft2(white) * np.sqrt(amp)).real
    f /= f.std()
    return f


def make_truth(kind, size, seed):
    if kind == "smooth":
        f = grf_field(size, 128, seed + 11)
        return 2.0 * np.exp(0.5 * f)      # ~1.2 dex dynamic range, smooth
    if kind == "highcontrast":
        f = grf_field(size, 16, seed + 12)
        rng = np.random.default_rng(seed + 13)
        spikes = np.zeros((size, size))
        idx = rng.choice(size * size, size=40, replace=False)
        spikes.ravel()[idx] = 1.5
        return 2.0 * np.exp(0.5 * f + spikes)
    if kind == "flat":
        return np.full((size, size), 2.0)
    raise ValueError(kind)


def control_grid(truth, delta):
    """Control points at cell centers (frozen geometry: node = cell center)."""
    n = truth.shape[0]
    centers = np.arange(delta // 2, n, delta)
    if centers[-1] != n - 1 - (delta - 1) // 2:
        pass  # grid may not tile exactly; acceptable for the experiment
    yy, xx = np.meshgrid(centers, centers, indexing="ij")
    return centers, truth[np.ix_(centers, centers)]


def bilinear(centers, vals, xs, ys):
    """Row-wise then column-wise linear interpolation (vectorized)."""
    rows = np.empty((len(ys), len(centers)))
    for j in range(len(centers)):
        rows[:, j] = np.interp(ys, centers, vals[:, j])
    out = np.empty((len(ys), len(xs)))
    for i in range(len(ys)):
        out[i, :] = np.interp(xs, centers, rows[i, :])
    return out


def natural_cubic_spline_1d(xp, yp, xq):
    """Solve for natural cubic spline coefficients and evaluate (vectorized per
    query point; O(n) tridiagonal solve)."""
    n = len(xp)
    h = np.diff(xp)
    if n < 3:
        return np.interp(xq, xp, yp)
    A = np.zeros((n, n))
    b = np.zeros(n)
    A[0, 0] = A[-1, -1] = 1.0
    for i in range(1, n - 1):
        A[i, i - 1] = h[i - 1]
        A[i, i] = 2.0 * (h[i - 1] + h[i])
        A[i, i + 1] = h[i]
        b[i] = 6.0 * ((yp[i + 1] - yp[i]) / h[i] - (yp[i] - yp[i - 1]) / h[i - 1])
    c = np.linalg.solve(A, b)
    i_arr = np.clip(np.searchsorted(xp, xq) - 1, 0, n - 2)
    dt = xq - xp[i_arr]
    b = (yp[i_arr + 1] - yp[i_arr]) / h[i_arr] - h[i_arr] * (2.0 * c[i_arr] + c[i_arr + 1]) / 6.0
    d = (c[i_arr + 1] - c[i_arr]) / (6.0 * h[i_arr])
    out = yp[i_arr] + b * dt + c[i_arr] / 2.0 * dt**2 + d * dt**3
    return out


def spline_reconstruct(centers, vals, xs, ys):
    """Separable natural bicubic spline: y-direction then x-direction, then clamp."""
    # evaluate at each query row: first interpolate along y for all center x
    tmp = np.empty((len(ys), len(centers)))
    for j, cx in enumerate(centers):
        tmp[:, j] = natural_cubic_spline_1d(centers, vals[:, j], ys)
    out = np.empty((len(ys), len(xs)))
    for i, qy in enumerate(ys):
        out[i, :] = natural_cubic_spline_1d(centers, tmp[i, :], xs)
    lo, hi = vals.min(), vals.max()
    return np.clip(out, lo, hi)


def mesh_median(centers, vals):
    v = vals.copy()
    p = np.pad(v, 1, mode="edge")
    stack = np.stack([p[0:-2, 0:-2], p[0:-2, 1:-1], p[0:-2, 2:],
                      p[1:-1, 0:-2], p[1:-1, 1:-1], p[1:-1, 2:],
                      p[2:, 0:-2], p[2:, 1:-1], p[2:, 2:]])
    return np.median(stack, axis=0)


def idw_reconstruct(centers, vals, xs, ys, power=2.0, k=16):
    pts = np.array([(x, y) for y in centers for x in centers])
    v = vals.ravel()
    out = np.empty((len(ys), len(xs)))
    for iy, y in enumerate(ys):
        for ix, x in enumerate(xs):
            d2 = (pts[:, 0] - x) ** 2 + (pts[:, 1] - y) ** 2
            j = np.argpartition(d2, min(k, len(d2) - 1))[:k]
            d = np.sqrt(d2[j])
            if d.min() < 1e-9:
                out[iy, ix] = v[j][np.argmin(d)]
            else:
                wgt = 1.0 / d ** power
                out[iy, ix] = (wgt * v[j]).sum() / wgt.sum()
    return out


def nearest_reconstruct(centers, vals, xs, ys):
    out = np.empty((len(ys), len(xs)))
    for iy, y in enumerate(ys):
        jy = np.abs(centers - y).argmin()
        for ix, x in enumerate(xs):
            jx = np.abs(centers - x).argmin()
            out[iy, ix] = vals[jy, jx]
    return out


def metrics(sigma_true, sigma_hat):
    w = 1.0 / sigma_hat**2
    var_w = (w**2 * sigma_true**2).sum() / w.sum() ** 2
    var_opt = 1.0 / (1.0 / sigma_true**2).sum()
    E = var_w / var_opt - 1.0
    rmse_dex = float(np.sqrt(np.mean((np.log10(sigma_hat) - np.log10(sigma_true)) ** 2)))
    return float(E), rmse_dex


def main():
    rng = np.random.default_rng(SEED)
    res = {"seed": SEED, "size": SIZE,
           "query": "dense eval on a 256x256 subgrid with random phase",
           "domains": {}}
    # random-phase query grid inside the frame
    q0 = int(rng.integers(0, SIZE - 256))
    qy = np.arange(q0, q0 + 256, 4).astype(float)
    qx = np.arange(q0, q0 + 256, 4).astype(float)

    deltas = [16, 32, 64, 128, 256]
    for kind in ["smooth", "highcontrast", "flat"]:
        truth = make_truth(kind, SIZE, SEED)
        # subgrid truth for metrics
        truth_q = truth[np.ix_(qy.astype(int), qx.astype(int))]
        table = {}
        for delta in deltas:
            centers, vals = control_grid(truth, delta)
            # control values are the *ideal* structure-aware local sigma at the
            # node (this experiment isolates the interpolation operator, not
            # the sigma estimator; estimator bias is a separate arm)
            arms = {}
            arms["bilinear_regular_grid_v1"] = bilinear(centers, vals, qx, qy)
            arms["natural_bicubic_spline_clip_v1"] = spline_reconstruct(centers, vals, qx, qy)
            arms["..._mesh_median_v1"] = spline_reconstruct(centers, mesh_median(centers, vals), qx, qy)
            arms["idw_p2_k16"] = idw_reconstruct(centers, vals, qx, qy, 2.0, 16)
            arms["nearest_control_point_v1"] = nearest_reconstruct(centers, vals, qx, qy)
            row = {}
            for op, sh in arms.items():
                sh = np.where(np.isfinite(sh) & (sh > 0), sh, vals.min())
                E, rd = metrics(truth_q, sh)
                row[op] = {"E": E, "rmse_dex": rd}
            table[str(delta)] = row
        res["domains"][kind] = table

    # ---- theory legs: convergence orders on an analytic smooth field -------
    # 1-D test: bilinear ~ O(Delta^2), natural cubic spline ~ O(Delta^4)
    f = lambda x: 1.0 + 0.5 * np.sin(2 * np.pi * x / 512.0)
    orders = {}
    orders_end = {}
    for delta_pair in [(16, 32), (32, 64)]:
        d1, d2 = delta_pair
        errs, errs_end = {}, {}
        for op_name in ["bilinear", "spline"]:
            e, e_end = [], []
            for d in (d1, d2):
                centers_d = np.arange(d // 2, SIZE, d).astype(float)
                vals_d = f(centers_d)
                # interior queries exclude 4 cells at each end: the natural
                # spline end condition S''=0 does not match the true curvature
                # => O(h^2) endpoint layer; the interior is O(h^4)
                xq_in = np.linspace(4.0 * d, SIZE - 4.0 * d, 4000)
                xq_all = np.linspace(8.0, SIZE - 8.0, 4000)
                if op_name == "bilinear":
                    qi = np.interp(xq_in, centers_d, vals_d)
                    qa = np.interp(xq_all, centers_d, vals_d)
                else:
                    qi = natural_cubic_spline_1d(centers_d, vals_d, xq_in)
                    qa = natural_cubic_spline_1d(centers_d, vals_d, xq_all)
                e.append(float(np.sqrt(np.mean((qi - f(xq_in)) ** 2))))
                e_end.append(float(np.sqrt(np.mean((qa - f(xq_all)) ** 2))))
            errs[op_name] = float(np.log(e[0] / e[1]) / np.log(d2 / d1))
            errs_end[op_name] = float(np.log(e_end[0] / e_end[1]) / np.log(d2 / d1))
        orders[f"order_{d1}_{d2}"] = errs
        orders_end[f"order_{d1}_{d2}"] = errs_end
    res["theory_convergence_order"] = orders
    res["theory_convergence_order_with_natural_end_layer"] = orders_end
    res["theory_convergence_note"] = (
        "bilinear expected -2 interior; natural cubic spline expected -4 "
        "interior; including the natural-end-condition boundary layer "
        "degrades the measured order -- a real, spec-relevant property of "
        "natural boundary splines on frames with nonzero edge curvature.");

    # ---- negative control: flat field => E == 0 and RMSE == 0 --------------
    flat = res["domains"]["flat"]
    worst = max(flat[str(delta)][op]["E"] for delta in deltas
                for op in ["bilinear_regular_grid_v1", "natural_bicubic_spline_clip_v1"])
    res["negative_control_flat"] = {
        "claim": "flat truth (no spatial effect) => E == 0, RMSE == 0 for exact-reproduction operators",
        "worst_E_over_operators_deltas": worst,
        "pass": bool(worst < 1e-9),
    }
    # control points must be reproduced exactly by interpolation operators
    centers, vals = control_grid(make_truth("smooth", SIZE, SEED), 64)
    rec = spline_reconstruct(centers, vals, centers.astype(float), centers.astype(float))
    repro = float(np.max(np.abs(rec - vals)))
    res["node_exact_reproduction"] = {"max_abs_deviation": repro, "pass": bool(repro < 1e-8)}
    res["all_pass"] = bool(res["negative_control_flat"]["pass"] and res["node_exact_reproduction"]["pass"])
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=2, ensure_ascii=False)
    print("domains:", {k: {str(d): {op: round(v["E"], 5) for op, v in row.items()}
                           for d, row in t.items()} for k, t in res["domains"].items() if k == "smooth"})
    print("theory orders:", orders)
    print("negative_control:", res["negative_control_flat"])
    print("node_reproduction:", res["node_exact_reproduction"])


if __name__ == "__main__":
    main()
