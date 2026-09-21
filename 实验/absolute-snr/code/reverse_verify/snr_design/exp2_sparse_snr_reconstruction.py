#!/usr/bin/env python3
"""SNR-DESIGN EXP-2: sparse control-point SNR -> dense SNR reconstruction error.

REAL DATA experiment on a production calibrated frame from the RELEASE-02 L4 run:

  run/RELEASE-02/L4-rebuild/norm/t2_m1_red/calibrated_...-20251212@012404-300S-Red.fts
  run/RELEASE-02/L4-rebuild/norm/t2_m1_red/p1_sources.json   (x, y, flux, fwhm_px)
  run/RELEASE-02/L4-rebuild/norm/t2_m1_red/p1_snr.json       (flux_adu, sigma_f_adu, snr_f)

We define the *dense reference* intra-frame SNR field exactly as the design
document does (SCI-NOISE + SCI-PSFW):

    SNR_ref(x,y) = a(x,y) * F_ref / sigma_F(x,y)
    sigma_F(x,y) = sigma_pix(x,y) * sqrt(A_NEA(x,y)) / a(x,y)
    => SNR_ref(x,y) prop to a(x,y) / ( sigma_pix(x,y) * FWHM(x,y) )   [A_NEA prop FWHM^2]

i.e. the spatial shape of the point-source SNR field is set by the local blank-sky
sigma field and the local PSF size, both of which are smooth fields.  We measure
sigma_pix on a 32x32-px patch grid (MAD, 5-sigma x 2 rounds) and FWHM from the
per-source PSF catalogue, then ask: how well can a SPARSE set of control points
reconstruct this field, as a function of control-point count N?

Error metric: eps = RMSE(log SNR_ref) = RMSE(relative SNR error).
Final impact: the inverse-variance weight error is delta = 2*eps, and EXP-1 proved
    Var_approx/Var_opt = 1 + Var_p(delta) = 1 + 4*Var(eps)   (to second order).

Run:
  TMPDIR=/dev/shm/astrocs_snrd python3 exp2_sparse_snr_reconstruction.py \
      --frame <path.fts> --p1-sources <p1_sources.json> --p1-snr <p1_snr.json> \
      --out exp2_sparse_snr_reconstruction.json
"""
import argparse, json, os, time
import numpy as np

MAD2SIG = 1.482602218505602   # docs/science/NOISE_MODEL.md section 9 frozen constant


# --------------------------------------------------------------------------- #
# dense reference field
# --------------------------------------------------------------------------- #
def patch_sigma_field(data, pb, clip_n=2, clip_k=5.0):
    """MAD-based blank-sky sigma on a pb x pb pixel patch grid (SCI-NOISE sec.5)."""
    h, w = data.shape
    gy, gx = h // pb, w // pb
    v = data[:gy * pb, :gx * pb].reshape(gy, pb, gx, pb).transpose(0, 2, 1, 3)
    v = v.reshape(gy * gx, pb * pb)
    med = np.median(v, axis=1)
    x = v - med[:, None]
    m = np.ones_like(x, dtype=bool)
    for _ in range(clip_n):
        s = MAD2SIG * np.median(np.abs(x), axis=1)
        m = np.abs(x) <= clip_k * np.maximum(s, 1e-12)[:, None]
        xm = np.where(m, v, np.nan)
        med = np.nanmedian(xm, axis=1)
        x = v - med[:, None]
    sig = MAD2SIG * np.median(np.abs(x), axis=1)
    n_sky = m.sum(axis=1)
    return sig.reshape(gy, gx), n_sky.reshape(gy, gx), med.reshape(gy, gx)


def patch_fwhm_field(xs, ys, fw, pb, shape, min_n=3):
    """Median per-source FWHM per patch, holes filled with the global median."""
    gy, gx = shape
    ix = np.clip((xs // pb).astype(int), 0, gx - 1)
    iy = np.clip((ys // pb).astype(int), 0, gy - 1)
    flat = iy * gx + ix
    order = np.argsort(flat)
    fs, vs = flat[order], fw[order]
    b = np.searchsorted(fs, np.arange(gy * gx + 1))
    out = np.full(gy * gx, np.nan)
    for k in range(gy * gx):
        if b[k + 1] - b[k] >= min_n:
            out[k] = np.median(vs[b[k]:b[k + 1]])
    out = out.reshape(gy, gx)
    frac_missing = float(np.mean(~np.isfinite(out)))
    out = np.where(np.isfinite(out), out, np.nanmedian(out))
    return out, frac_missing


# --------------------------------------------------------------------------- #
# reconstruction operators
# --------------------------------------------------------------------------- #
def sample_grid(field, stride):
    """Control points = every stride-th node of the dense patch grid."""
    return field[::stride, ::stride]


def bilinear_from_nodes(nodes, shape, stride):
    """Bilinear interpolation from a coarse node grid back to the dense grid
    (no extrapolation: the dense grid is cropped to the node span)."""
    gy, gx = shape
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    fy = yq / stride
    fx = xq / stride
    y0 = np.clip(np.floor(fy).astype(int), 0, ny - 2)
    x0 = np.clip(np.floor(fx).astype(int), 0, nx - 2)
    wy = (fy - y0)[:, None]
    wx = (fx - x0)[None, :]
    a = nodes[np.ix_(y0, x0)]
    b = nodes[np.ix_(y0, x0 + 1)]
    c = nodes[np.ix_(y0 + 1, x0)]
    d = nodes[np.ix_(y0 + 1, x0 + 1)]
    rec = (a * (1 - wx) * (1 - wy) + b * wx * (1 - wy)
           + c * (1 - wx) * wy + d * wx * wy)
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def cubic_from_nodes(nodes, shape, stride):
    from scipy.interpolate import RectBivariateSpline
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    sp = RectBivariateSpline(ye, xe, nodes, kx=3, ky=3, s=0)
    rec = sp(yq, xq)
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def kriging_from_nodes(nodes, shape, stride, ell_px, nugget=0.0):
    """Ordinary-kriging (GP) prediction on the dense grid with a squared-
    exponential covariance of correlation length ell_px (separable grid)."""
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    Cy = np.exp(-0.5 * (ye[:, None] - ye[None, :]) ** 2 / ell_px ** 2)
    Cx = np.exp(-0.5 * (xe[:, None] - xe[None, :]) ** 2 / ell_px ** 2)
    Cy += nugget * np.eye(ny)
    Cx += nugget * np.eye(nx)
    Ky = np.exp(-0.5 * (yq[:, None] - ye[None, :]) ** 2 / ell_px ** 2)
    Kx = np.exp(-0.5 * (xq[:, None] - xe[None, :]) ** 2 / ell_px ** 2)
    Wy = np.linalg.solve(Cy, Ky.T).T
    Wx = np.linalg.solve(Cx, Kx.T).T
    rec = Wy @ nodes @ Wx.T
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def fit_ell(logfield, pb):
    """Estimate the correlation length (px) from the field variogram."""
    lags = np.array([1, 2, 3, 4, 6, 8, 12, 16, 24, 32])
    g = []
    for L in lags:
        d = logfield[:, L:] - logfield[:, :-L]
        g.append(np.nanmean(d ** 2) / 2.0)
    g = np.array(g)
    g_inf = float(np.nanmax(g))
    best, best_r = None, np.inf
    for ell in np.linspace(0.5, 200.0, 400):
        model = g_inf * (1.0 - np.exp(-(lags * pb) ** 2 / (2 * (ell * pb) ** 2)))
        r = np.nansum((model - g) ** 2)
        if r < best_r:
            best_r, best = r, ell
    return float(best * pb), lags * pb, g.tolist(), g_inf


# --------------------------------------------------------------------------- #
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--frame", required=True)
    ap.add_argument("--p1-sources", required=True)
    ap.add_argument("--p1-snr", required=True)
    ap.add_argument("--pb", type=int, default=32)
    ap.add_argument("--out", default="exp2_sparse_snr_reconstruction.json")
    a = ap.parse_args()

    from astropy.io import fits
    t0 = time.time()
    with fits.open(a.frame, memmap=True) as h:
        data = np.array(h[0].data, dtype=np.float64)
    sig, n_sky, bkg = patch_sigma_field(data, a.pb)
    gy, gx = sig.shape

    src = json.load(open(a.p1_sources))["frames"][0]["sources"]
    snr = json.load(open(a.p1_snr))["frames"][0]["sources"]
    xs = np.array([s["x"] for s in src], float)
    ys = np.array([s["y"] for s in src], float)
    fw = np.array([s["fwhm_px"] for s in src], float)
    sf = np.array([s["sigma_f_adu"] for s in snr], float)
    ok = (fw > 0) & (sf > 0) & np.isfinite(xs) & np.isfinite(ys)
    fwhm_field, frac_missing = patch_fwhm_field(xs[ok], ys[ok], fw[ok], a.pb,
                                                (gy, gx))

    snr_field = 1.0 / (sig * fwhm_field)
    logf = np.log(snr_field)
    truth_se = 1.44 / np.sqrt(np.maximum(n_sky, 1))
    ell_px, lags_px, vario, g_inf = fit_ell(logf, a.pb)

    ix = np.clip((xs[ok] // a.pb).astype(int), 0, gx - 1)
    iy = np.clip((ys[ok] // a.pb).astype(int), 0, gy - 1)
    pred = np.log(snr_field[iy, ix])
    meas = np.log(1.0 / sf[ok])
    good = np.isfinite(pred) & np.isfinite(meas)
    cross = dict(
        n_sources=int(good.sum()),
        pearson_log=float(np.corrcoef(pred[good], meas[good])[0, 1]),
        scatter_dex=float(np.std(pred[good] - meas[good])),
        note="log(sigma_F) from per-source Horne extraction vs patch-based "
             "sigma_pix*FWHM prediction (shape agreement only; the absolute "
             "scale differs by the arbitrary F_ref)",
    )

    # --- FAIR common evaluation window ---------------------------------------
    # All control grids are compared on the SAME central window, and the window
    # is strictly interior to every grid (interpolation, never extrapolation).
    wy0, wy1, wx0, wx1 = gy // 2 - 16, gy // 2 + 16, gx // 2 - 16, gx // 2 + 16
    W = (slice(wy0, wy1), slice(wx0, wx1))
    ref_log = logf[W]

    # control-point measurement variance (nugget): SE(log sigma) ~ 1.44/sqrt(N_sky)
    node_nugget = float(np.nanmean((1.44 / np.sqrt(np.maximum(n_sky, 1))) ** 2))

    def plane_fit(nodes, shape, stride):
        """Least-squares PLANE over the control points - exactly the production
        NoiseWeightModelV1 spatial field var(x,y)=a+b*x+c*y (SCI-NOISE sec.5)."""
        ny, nx = nodes.shape
        yy, xx = np.mgrid[0:ny, 0:nx]
        A = np.column_stack([np.ones(ny * nx), (xx * stride).ravel(),
                             (yy * stride).ravel()])
        b = nodes.ravel()
        m = np.isfinite(b)
        coef, *_ = np.linalg.lstsq(A[m], b[m], rcond=None)
        Y, X = np.mgrid[0:shape[0], 0:shape[1]]
        rec = coef[0] + coef[1] * X + coef[2] * Y
        return rec, (slice(0, shape[0]), slice(0, shape[1]))

    rows = []
    for stride in (2, 4, 8, 16, 32, 64):
        nodes = sample_grid(logf, stride)
        n = int(nodes.size)
        # guard: the common window must be strictly interior to this grid
        ny_n, nx_n = nodes.shape
        if (ny_n - 1) * stride < wy1 or (nx_n - 1) * stride < wx1:
            print(f"  [skip stride={stride}] window not interior "
                  f"(grid span {(ny_n-1)*stride} < {wy1})")
            continue
        recs = {}
        rec, sl = bilinear_from_nodes(nodes, (gy, gx), stride)
        recs["bilinear"] = rec[W]
        try:
            rec3, _ = cubic_from_nodes(nodes, (gy, gx), stride)
            recs["bicubic"] = rec3[W]
        except Exception:
            pass
        for nug, tag in ((0.0, "kriging_gp"), (node_nugget, "kriging_gp_nugget")):
            try:
                reck, _ = kriging_from_nodes(nodes, (gy, gx), stride, ell_px,
                                             nugget=nug)
                recs[tag] = reck[W]
            except Exception:
                pass
        try:
            recp, _ = plane_fit(nodes, (gy, gx), stride)
            recs["plane_fit_production"] = recp[W]
        except Exception:
            pass
        for name, r in recs.items():
            e = (r - ref_log)[np.isfinite(ref_log) & np.isfinite(r)]
            rmse = float(np.sqrt(np.mean(e ** 2)))
            rows.append(dict(
                stride=stride, n_control_points=n, method=name,
                spacing_px=stride * a.pb,
                rmse_log_snr=rmse, bias_log_snr=float(np.mean(e)),
                weight_penalty_pct=float(100.0 * 4.0 * rmse ** 2),
                sigma_penalty_pct=float(100.0 * (np.sqrt(1.0 + 4.0 * rmse ** 2) - 1.0)),
            ))

    # --- negative controls ---------------------------------------------------
    scalar = float(np.nanmean(logf[W]))
    e1 = (logf[W] - scalar).ravel()
    e1 = e1[np.isfinite(e1)]
    neg_scalar = dict(rmse_log_snr=float(np.sqrt(np.mean(e1 ** 2))),
                      weight_penalty_pct=float(100.0 * 4.0 * np.mean(e1 ** 2)))
    nodes = sample_grid(logf, 8)
    neg_ell = []
    for mult in (0.1, 0.25, 0.5, 1.0, 2.0, 4.0, 10.0):
        r, _ = kriging_from_nodes(nodes, (gy, gx), 8,
                                  max(ell_px * mult, a.pb),
                                  nugget=node_nugget)
        e = (r[W] - ref_log).ravel()
        e = e[np.isfinite(e)]
        neg_ell.append(dict(ell_multiplier=mult,
                            ell_px=float(max(ell_px * mult, a.pb)),
                            rmse_log_snr=float(np.sqrt(np.mean(e ** 2)))))
    rng = np.random.default_rng(7)
    sh = nodes.copy().ravel()
    rng.shuffle(sh)
    r, _ = bilinear_from_nodes(sh.reshape(nodes.shape), (gy, gx), 8)
    e = (r[W] - ref_log).ravel()
    e = e[np.isfinite(e)]
    neg_shuffle = dict(rmse_log_snr=float(np.sqrt(np.mean(e ** 2))))
    # negative control: evaluation window NOT interior to a coarse grid would
    # silently mix extrapolation into the metric -> must be caught by the guard
    guard = dict(window_patches=[wy0, wy1, wx0, wx1],
                 window_interior_to_coarsest=bool(
                     wy0 >= 0 and wx0 >= 0
                     and (gy // 64 - 1) * 64 >= wy1 - 1
                     and (gx // 64 - 1) * 64 >= wx1 - 1))

    out = dict(
        exp="EXP-2 sparse control-point SNR -> dense reconstruction (real frame)",
        frame=os.path.basename(a.frame),
        patch_px=a.pb, grid=[int(gy), int(gx)],
        sigma_field_adu=dict(median=float(np.median(sig)),
                             p05=float(np.percentile(sig, 5)),
                             p95=float(np.percentile(sig, 95)),
                             p95_over_p05=float(np.percentile(sig, 95)
                                                / np.percentile(sig, 5))),
        fwhm_field_px=dict(median=float(np.median(fwhm_field)),
                           p05=float(np.percentile(fwhm_field, 5)),
                           p95=float(np.percentile(fwhm_field, 95)),
                           missing_patch_fraction=frac_missing),
        snr_field=dict(log_std=float(np.nanstd(logf)),
                       p95_over_p05=float(np.exp(np.nanpercentile(logf, 95)
                                                 - np.nanpercentile(logf, 5)))),
        correlation_length_px=ell_px,
        variogram=dict(lags_px=lags_px.tolist(), gamma=vario, sill=g_inf),
        truth_estimator_se=dict(median=float(np.median(truth_se)),
                                p95=float(np.percentile(truth_se, 95)),
                                note="SE(sigma_hat)/sigma ~ 1.44/sqrt(N_sky) "
                                     "(NOISE_MODEL sec.5a sky-budget constant)"),
        cross_check_source_sigma_f=cross,
        scan=rows,
        negative_controls=dict(frame_scalar=neg_scalar,
                               wrong_correlation_length=neg_ell,
                               shuffled_control_points=neg_shuffle),
        elapsed_s=time.time() - t0,
    )
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)

    print(f"frame          : {out['frame']}")
    print(f"grid           : {gy}x{gx} patches of {a.pb}px")
    print(f"sigma_pix [ADU]: med={np.median(sig):.3f} p05={np.percentile(sig,5):.3f} "
          f"p95={np.percentile(sig,95):.3f} ratio={np.percentile(sig,95)/np.percentile(sig,5):.3f}")
    print(f"FWHM [px]      : med={np.median(fwhm_field):.3f} "
          f"p05={np.percentile(fwhm_field,5):.3f} p95={np.percentile(fwhm_field,95):.3f} "
          f"missing={frac_missing:.3f}")
    print(f"SNR field      : log-std={np.nanstd(logf):.4f} "
          f"p95/p05={out['snr_field']['p95_over_p05']:.3f}")
    print(f"corr length    : ell={ell_px:.1f} px   truth SE(median)={np.median(truth_se):.4f}")
    print(f"cross-check    : pearson(log pred, log meas)={cross['pearson_log']:.3f} "
          f"scatter={cross['scatter_dex']:.4f} dex  (n={cross['n_sources']})")
    print()
    print(f"{'spacing':>8} {'N_ctrl':>8} {'method':>10} {'RMSE(logSNR)':>13} "
          f"{'VarPen%':>8} {'SigPen%':>8}")
    for r in rows:
        print(f"{r['spacing_px']:8d} {r['n_control_points']:8d} {r['method']:>10} "
              f"{r['rmse_log_snr']:13.5f} {r['weight_penalty_pct']:8.4f} "
              f"{r['sigma_penalty_pct']:8.4f}")
    print()
    print("negative controls:")
    print("  frame-level scalar  RMSE=%.5f  penalty=%.3f%%"
          % (neg_scalar["rmse_log_snr"], neg_scalar["weight_penalty_pct"]))
    print("  shuffled nodes      RMSE=%.5f" % neg_shuffle["rmse_log_snr"])
    for d in neg_ell:
        print("  ell x%-5.2f (%7.1f px) RMSE=%.5f"
              % (d["ell_multiplier"], d["ell_px"], d["rmse_log_snr"]))
    print("\nwrote", a.out)


if __name__ == "__main__":
    main()
