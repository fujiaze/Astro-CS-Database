#!/usr/bin/env python3
"""SNR-DESIGN EXP-3: multi-frame weight penalty from sparse-SNR reconstruction,
plus the frame-scalar consistency check (MAD sigma vs the sigma actually used by
the SNR), on REAL production frames.

Part A - scalar consistency (production data):
  p1_snr.json  frames[].variance  = (1.482602218505602*MAD(cleaned frame))^2
  p1_sources.json frames[].noise_sigma = star detector clipped RMS about the
                                   clipped median (the sigma EVERY SNR uses)
  If these disagree, the published frame-level variance/ivar is inconsistent with
  the SNR in the same product, and any weight built as 1/variance mis-weights.

Part B - real weight penalty:
  For a set of fully overlapping frames of one tile, build the per-frame local
  sigma field on a patch grid, treat w_f = 1/sigma_f^2 as the truth, reconstruct
  it from N sparse control points, and evaluate at every pixel
      penalty = 1 + sum_f p_f (delta_f - deltabar)^2 ,  p_f = w_f / sum w,
                delta_f = w_hat_f / w_f - 1
  which EXP-1 proved equals Var_approx/Var_opt to second order.  We compare it
  with the worst-case 1 + 4*RMSE^2 (independent reconstruction errors).

Run: TMPDIR=/dev/shm/astrocs_snrd python3 exp3_multiframe_weight_penalty.py \
        --norm-dir <.../L4-rebuild/norm> --tile t2_m2_red \
        --out exp3_multiframe_weight_penalty.json
"""
import argparse, glob, json, os, time
import numpy as np

MAD2SIG = 1.482602218505602


def patch_sigma(data, pb, clip_n=2, clip_k=5.0):
    h, w = data.shape
    gy, gx = h // pb, w // pb
    v = data[:gy * pb, :gx * pb].reshape(gy, pb, gx, pb).transpose(0, 2, 1, 3)
    v = v.reshape(gy * gx, pb * pb)
    med = np.median(v, axis=1)
    x = v - med[:, None]
    for _ in range(clip_n):
        s = MAD2SIG * np.median(np.abs(x), axis=1)
        m = np.abs(x) <= clip_k * np.maximum(s, 1e-12)[:, None]
        med = np.nanmedian(np.where(m, v, np.nan), axis=1)
        x = v - med[:, None]
    s = MAD2SIG * np.median(np.abs(x), axis=1)
    return s.reshape(gy, gx)


def clipped_rms(data):
    """Exactly the star detector estimator (star_detector.cpp:41-67)."""
    x = data[np.isfinite(data)].ravel()
    keep = x
    for _ in range(2):
        med = np.median(keep)
        s = MAD2SIG * np.median(np.abs(keep - med))
        s = s if s > 0 else 1e-9
        keep = keep[np.abs(keep - med) <= 3.0 * s]
    med = np.median(keep)
    sig = np.sqrt(np.sum((keep - med) ** 2) / max(keep.size, 1))
    return float(sig if sig > 1e-9 else 1e-9), float(med)


def bilinear(nodes, shape, stride):
    gy, gx = shape
    ny, nx = nodes.shape
    ye = np.arange(ny) * stride
    xe = np.arange(nx) * stride
    yq = np.arange(0, ye[-1] + 1)
    xq = np.arange(0, xe[-1] + 1)
    fy, fx = yq / stride, xq / stride
    y0 = np.clip(np.floor(fy).astype(int), 0, ny - 2)
    x0 = np.clip(np.floor(fx).astype(int), 0, nx - 2)
    wy, wx = (fy - y0)[:, None], (fx - x0)[None, :]
    a, b = nodes[np.ix_(y0, x0)], nodes[np.ix_(y0, x0 + 1)]
    c, d = nodes[np.ix_(y0 + 1, x0)], nodes[np.ix_(y0 + 1, x0 + 1)]
    rec = (a * (1 - wx) * (1 - wy) + b * wx * (1 - wy)
           + c * (1 - wx) * wy + d * wx * wy)
    return rec, (slice(0, ye[-1] + 1), slice(0, xe[-1] + 1))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--norm-dir", required=True)
    ap.add_argument("--tile", default="t2_m2_red")
    ap.add_argument("--pb", type=int, default=32)
    ap.add_argument("--out", default="exp3_multiframe_weight_penalty.json")
    ap.add_argument("--max-tiles", type=int, default=4,
                    help="limit Part A to this many tiles (large JSON parsing)")
    a = ap.parse_args()

    from astropy.io import fits
    t0 = time.time()
    out = {"exp": "EXP-3 multi-frame sparse-SNR weight penalty + scalar consistency"}

    # ---------------- Part A: scalar consistency over all tiles --------------
    rows = []
    tiles = sorted(os.listdir(a.norm_dir))[: a.max_tiles]
    for tile in tiles:
        d = os.path.join(a.norm_dir, tile)
        ps, pn = os.path.join(d, "p1_sources.json"), os.path.join(d, "p1_snr.json")
        if not (os.path.isfile(ps) and os.path.isfile(pn)):
            continue
        try:
            src = json.load(open(ps))["frames"]
            snr = json.load(open(pn))["frames"]
        except Exception as e:
            rows.append(dict(tile=tile, error=str(e)))
            continue
        for s, n in zip(src, snr):
            v = n.get("variance")
            ns = s.get("noise_sigma")
            if not v or not ns:
                continue
            rows.append(dict(
                tile=tile, file=n.get("file"),
                sigma_mad=float(np.sqrt(v)), sigma_clipped_rms=float(ns),
                sigma_ratio=float(np.sqrt(v) / ns),
                variance_ratio=float(v / (ns * ns)),
                background=float(n.get("background") or 0.0),
                n_sources=s.get("n_sources"),
                frame_snr_ref=(n.get("snr_reference") or {}).get("snr_f"),
            ))
    good = [r for r in rows if "sigma_ratio" in r]
    if good:
        sr = np.array([r["sigma_ratio"] for r in good])
        vr = np.array([r["variance_ratio"] for r in good])
        out["partA_scalar_consistency"] = dict(
            n_frames=len(good),
            sigma_ratio_median=float(np.median(sr)),
            sigma_ratio_min=float(sr.min()), sigma_ratio_max=float(sr.max()),
            variance_ratio_median=float(np.median(vr)),
            note="sigma_mad = sqrt(p1_snr.frames[].variance); "
                 "sigma_clipped_rms = p1_sources.frames[].noise_sigma "
                 "(the sigma that scales every SNR in p1_snr.json)",
        )
        out["partA_frames"] = good

    # ---------------- Part B: real multi-frame weight penalty ----------------
    td = os.path.join(a.norm_dir, a.tile)
    with open(os.path.join(td, "p1_sources.json")) as f:
        sdoc = json.load(f)
    files = [fr["file"] for fr in sdoc["frames"]]
    sig_fields, names = [], []
    for fr in sdoc["frames"]:
        base = fr["file"]
        # p1_sources.json names the *cleaned* frame; the calibrated sibling drops
        # the "cleaned_" prefix (observed on all RELEASE-02 L4 runs).
        stem = base[len("cleaned_"):] if base.startswith("cleaned_") else base
        cand = (glob.glob(os.path.join(td, "calibrated_" + stem))
                or glob.glob(os.path.join(td, "cleaned_" + stem))
                or glob.glob(os.path.join(td, base)))
        if not cand:
            continue
        with fits.open(cand[0], memmap=True) as h:
            dat = np.array(h[0].data, dtype=np.float64)
        sig_fields.append(patch_sigma(dat, a.pb))
        names.append(base)
    S = np.stack(sig_fields)            # (F, gy, gx)
    F, gy, gx = S.shape
    W = 1.0 / S ** 2                    # relative inverse-variance (truth)
    p = W / W.sum(axis=0, keepdims=True)

    wy0, wy1, wx0, wx1 = gy // 2 - 16, gy // 2 + 16, gx // 2 - 16, gx // 2 + 16
    win = (slice(wy0, wy1), slice(wx0, wx1))

    scan = []
    for stride in (2, 4, 8, 16, 32):
        ny_n = len(range(0, gy, stride))
        if (ny_n - 1) * stride < wy1:
            continue
        phat = []
        rmse_list = []
        for k in range(F):
            nodes = np.log(S[k])[::stride, ::stride]
            rec, _ = bilinear(nodes, (gy, gx), stride)
            s_hat = np.exp(rec[win])
            phat.append(1.0 / s_hat ** 2)
            e = (np.log(s_hat) - np.log(S[k][win])).ravel()
            rmse_list.append(float(np.sqrt(np.mean(e ** 2))))
        Phat = np.stack(phat)
        pt = p[:, win[0], win[1]]
        wt = W[:, win[0], win[1]]
        d = Phat / wt - 1.0
        dbar = (pt * d).sum(axis=0)
        var_p = (pt * (d - dbar) ** 2).sum(axis=0)
        pen = 1.0 + var_p
        rmse = float(np.mean(rmse_list))
        scan.append(dict(
            stride=stride, spacing_px=stride * a.pb,
            n_control_points=int(np.ceil(gy / stride) * np.ceil(gx / stride)),
            rmse_log_sigma=rmse,
            penalty_measured_mean=float(np.mean(pen)),
            penalty_measured_p95=float(np.percentile(pen, 95)),
            penalty_worstcase_independent=float(1.0 + 4.0 * rmse ** 2),
            sigma_penalty_pct=float(100.0 * (np.sqrt(np.mean(pen)) - 1.0)),
        ))
    # scalar-only control: one number per frame
    phat = np.stack([np.full((wy1 - wy0, wx1 - wx0), 1.0 / np.mean(S[k]) ** 2)
                     for k in range(F)])
    pt = p[:, win[0], win[1]]
    wt = W[:, win[0], win[1]]
    d = phat / wt - 1.0
    dbar = (pt * d).sum(axis=0)
    pen0 = 1.0 + (pt * (d - dbar) ** 2).sum(axis=0)
    out["partB"] = dict(
        tile=a.tile, n_frames=F, patch_px=a.pb, grid=[int(gy), int(gx)],
        frames=names,
        sigma_frame_median_adu=[float(np.median(s)) for s in S],
        sigma_frame_spread=float(np.max([np.median(s) for s in S])
                                 / np.min([np.median(s) for s in S])),
        window=[wy0, wy1, wx0, wx1],
        scan=scan,
        scalar_only=dict(penalty_mean=float(np.mean(pen0)),
                         sigma_penalty_pct=float(100.0 * (np.sqrt(np.mean(pen0)) - 1.0))),
    )
    out["elapsed_s"] = time.time() - t0
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2)

    if good:
        A = out["partA_scalar_consistency"]
        print("== Part A: frame-scalar consistency (%d frames) ==" % A["n_frames"])
        print("   sigma_MAD / sigma_clippedRMS : median %.4f  range [%.4f, %.4f]"
              % (A["sigma_ratio_median"], A["sigma_ratio_min"], A["sigma_ratio_max"]))
        print("   variance ratio (MAD^2/RMS^2) : median %.4f" % A["variance_ratio_median"])
    print()
    B = out["partB"]
    print("== Part B: tile %s, %d frames ==" % (B["tile"], B["n_frames"]))
    print("   per-frame median sigma [ADU]:",
          ["%.3f" % v for v in B["sigma_frame_median_adu"]])
    print("   frame-to-frame sigma spread  : %.4f" % B["sigma_frame_spread"])
    print()
    print("%9s %8s %13s %16s %16s %10s" % ("spacing", "N_ctrl", "RMSE(logsig)",
                                            "penalty_meas", "pen_worstcase", "sig_pen%"))
    for r in scan:
        print("%9d %8d %13.5f %16.6f %16.6f %10.4f"
              % (r["spacing_px"], r["n_control_points"], r["rmse_log_sigma"],
                 r["penalty_measured_mean"], r["penalty_worstcase_independent"],
                 r["sigma_penalty_pct"]))
    print("   scalar-only frame weight: penalty %.6f (sigma penalty %.4f%%)"
          % (B["scalar_only"]["penalty_mean"], B["scalar_only"]["sigma_penalty_pct"]))
    print("\nwrote", a.out)


if __name__ == "__main__":
    main()
