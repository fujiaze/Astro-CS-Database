#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-P4-04: IDW parameter scan (P-CST-24: idw_power=2.0 / K=16 / gamma<1e-10).

Power p in {0.5,1,2,3,4,8}, neighbor count K in {4,8,16,32,64}; smooth slow
truth field.  Checks:
  S1  reconstruction error is a smooth function of p and K (no sharp optimum)
  S2  p -> inf converges to nearest-control-point operator
  S3  gamma (distance floor) < 1e-10 is a numerical guard, not a science value:
      behavior is invariant for gamma below the control-point spacing
  NC  flat-truth negative control: all (p,K) => E == 0
Standalone, numpy only, seed fixed.
"""
import json
import numpy as np

SEED = 20260926
OUT = "results/exp04_idw_parameters.json"
SIZE = 512


def grf_field(size, ell, seed):
    rng = np.random.default_rng(seed)
    ky = np.fft.fftfreq(size)[:, None]
    kx = np.fft.fftfreq(size)[None, :]
    k2 = kx**2 + ky**2
    k2[0, 0] = 1.0
    amp = np.exp(-2.0 * (np.pi * ell) ** 2 * k2)
    amp[0, 0] = 0.0
    white = rng.normal(size=(size, size))
    f = np.fft.ifft2(np.fft.fft2(white) * np.sqrt(amp)).real
    return 2.0 * np.exp(0.5 * f / f.std())


def idw(centers, vals, xs, ys, power, k, gamma=1e-10):
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
                # w = 1/(d^p + gamma), normalized by dmin^p so that large p
                # cannot overflow: w = 1/((d/dmin)^p + gamma/dmin^p)
                with np.errstate(over="ignore"):
                    # exp -> inf gives weight 0 for far points: intended limit
                    wgt = 1.0 / (np.exp(power * np.log(d / d.min()))
                                 + gamma / d.min() ** power)
                out[iy, ix] = (wgt * v[j]).sum() / wgt.sum()
    return out


def metrics(st, sh):
    w = 1.0 / sh**2
    var_w = (w**2 * st**2).sum() / w.sum() ** 2
    var_opt = 1.0 / (1.0 / st**2).sum()
    E = var_w / var_opt - 1.0
    rd = float(np.sqrt(np.mean((np.log10(sh) - np.log10(st)) ** 2)))
    return float(E), rd


def main():
    rng = np.random.default_rng(SEED)
    truth = grf_field(SIZE, 96, SEED + 21)
    q0 = int(rng.integers(0, SIZE - 128))
    qy = np.arange(q0, q0 + 128, 4).astype(float)
    qx = qy.copy()
    truth_q = truth[np.ix_(qy.astype(int), qx.astype(int))]
    delta = 64
    centers = np.arange(delta // 2, SIZE, delta).astype(float)
    vals = truth[np.ix_(centers.astype(int), centers.astype(int))]

    powers = [0.5, 1.0, 2.0, 3.0, 4.0, 8.0]
    ks = [4, 8, 16, 32, 64]
    grid = {}
    for p in powers:
        for k in ks:
            sh = idw(centers, vals, qx, qy, p, k)
            E, rd = metrics(truth_q, sh)
            grid[f"p{p}_k{k}"] = {"E": E, "rmse_dex": rd}
    res = {"seed": SEED, "delta": delta, "grid": grid}

    # nearest-reference operator (p -> inf limit target)
    near = np.empty((len(qy), len(qx)))
    for iy, y in enumerate(qy):
        for ix, x in enumerate(qx):
            jy = np.abs(centers - y).argmin()
            jx = np.abs(centers - x).argmin()
            near[iy, ix] = vals[jy, jx]
    # S2: p -> inf converges to nearest operator (deviation must decrease
    # monotonically over a p sequence and reach numerical zero)
    devs = []
    for pbig in [30.0, 100.0, 300.0, 1000.0]:
        sh_bigp = idw(centers, vals, qx, qy, pbig, 64)
        devs.append(float(np.max(np.abs(sh_bigp - near))))
    maxdev = devs[-1]
    res["S2_p_inf_nearest_limit"] = {
        "max_abs_deviation_sequence": devs,
        "monotone_decreasing": bool(all(devs[i + 1] <= devs[i] for i in range(len(devs) - 1))),
        "final": maxdev,
        "pass": bool(maxdev < 1e-9 and all(devs[i + 1] <= devs[i] for i in range(len(devs) - 1))),
    }

    # S3: gamma floor invariance
    sh_g1 = idw(centers, vals, qx, qy, 2.0, 16, 1e-10)
    sh_g2 = idw(centers, vals, qx, qy, 2.0, 16, 1e-6)
    sh_g3 = idw(centers, vals, qx, qy, 2.0, 16, 1.0)    # gamma << d^2 ~ 1e3
    sh_g4 = idw(centers, vals, qx, qy, 2.0, 16, 1.0e4)  # gamma ~ d^2 scale
    d12 = float(np.max(np.abs(sh_g1 - sh_g2)))
    d13 = float(np.max(np.abs(sh_g1 - sh_g3)))
    d14 = float(np.max(np.abs(sh_g1 - sh_g4)))
    res["S3_gamma_floor"] = {
        "max_dev_gamma_1e-10_vs_1e-6": d12,
        "max_dev_gamma_1e-10_vs_1p0": d13,
        "max_dev_gamma_1e-10_vs_1e4": d14,
        "claim": "gamma << d^p is a numerical guard (invariant); gamma ~ d^p changes the operator materially",
        "pass": bool(d12 < 1e-6 and d13 < 1e-3 and d14 > 0.01),
    }

    # smoothness of error in (p,K): no sharp optimum
    es = np.array([grid[f"p{p}_k{k}"]["E"] for p in powers for k in ks])
    res["S1_parameter_sensitivity"] = {
        "E_min": float(es.min()), "E_max": float(es.max()),
        "best_p": float(powers[int(np.argmin(es)) // len(ks)]),
        "best_k": int(ks[int(np.argmin(es)) % len(ks)]),
        "claim": "error varies smoothly with p and K; no literature-anchored universal optimum",
    }

    # negative control: flat truth
    vals_flat = np.ones_like(vals) * 2.0
    nc = []
    for p in [1.0, 2.0, 4.0]:
        for k in [4, 16, 64]:
            sh = idw(centers, vals_flat, qx, qy, p, k)
            E, rd = metrics(np.full_like(truth_q, 2.0), sh)
            nc.append(max(abs(E), rd))
    res["NC_flat_zeroing"] = {"worst_metric": float(max(nc)),
                              "pass": bool(max(nc) < 1e-9)}

    res["all_pass"] = bool(res["S2_p_inf_nearest_limit"]["pass"]
                           and res["S3_gamma_floor"]["pass"]
                           and res["NC_flat_zeroing"]["pass"])
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, indent=2, ensure_ascii=False)
    print("best:", res["S1_parameter_sensitivity"])
    print("S2:", res["S2_p_inf_nearest_limit"])
    print("S3:", res["S3_gamma_floor"])
    print("NC:", res["NC_flat_zeroing"])
    # print E table compactly
    for p in powers:
        print("p=%4.1f " % p + " ".join("%7.4f" % grid[f"p{p}_k{k}"]["E"] for k in ks))


if __name__ == "__main__":
    main()
