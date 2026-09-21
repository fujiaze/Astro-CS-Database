#!/usr/bin/env python3
"""SNR-EXP-AUDIT 4: SP-0 applicability -- does the "frame scalar is optimal"
conclusion survive when the frames have DIFFERENT spatial noise shapes?

Requested by the responsible lead (GAP_AUDIT 9.46): the SP-0 verdict must not be
generalised from the common-mode-degenerate case.  A mosaic combines frames of
DIFFERENT pointings / nights / airmasses, so the per-frame sky (and therefore
the per-frame sigma field) has a different SPATIAL SHAPE at the same sky
position -- exactly the situation in which the frame scalar is NOT optimal.

This is a REAL-DATA control experiment (no simulation):
  * every calibrated frame of run/RELEASE-02/L4-rebuild/norm/* is reduced to its
    32 px patch sigma grid (128x128) plus its WCS;
  * for a set of frames that share sky, a grid of common SKY positions is mapped
    into each frame with its own WCS and the local sigma is sampled;
  * the frame-scalar and the 64 px sparse weight efficiencies are then compared
    on exactly the same sky points.

Two regimes are contrasted:
  (A) same tile, 4 frames, same pointing   -> the design's EXP-3 dataset
  (B) different tiles / different airmass  -> the mosaic regime

Run:
  TMPDIR=/dev/shm/astrocs_snraudit python3 audit_mosaic_shape.py \
     --out ../../../../run/reverse_verify/snr_design/audit/audit_mosaic_shape.json
"""
import argparse, glob, json, os, sys, time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import physnoise as pn

NORM = "../../../../../../run/RELEASE-02/L4-rebuild/norm"
PB = 32


def frame_sigma_and_wcs(path):
    from astropy.io import fits
    from astropy.wcs import WCS
    with fits.open(path, memmap=True) as h:
        hdr = h[0].header
        d = np.array(h[0].data, dtype=np.float64)
    sig, n_sky = pn.patch_sigma_clipped(d, PB)
    del d
    return sig, n_sky, WCS(hdr), hdr


def footprint(wcs, shape):
    ny, nx = shape
    corners = np.array([[0, 0], [0, nx - 1], [ny - 1, 0], [ny - 1, nx - 1]], float)
    return wcs.all_pix2world(corners, 0)


def sample_sigma(sig, wcs, ra, dec):
    """Bilinear sample of the patch sigma grid at sky positions."""
    px, py = wcs.all_world2pix(np.column_stack([ra, dec]), 0).T
    fx, fy = px / PB - 0.5, py / PB - 0.5      # patch-centre coordinates
    gy, gx = sig.shape
    ok = (fx >= 0) & (fx <= gx - 1) & (fy >= 0) & (fy <= gy - 1)
    x0 = np.clip(np.floor(fx).astype(int), 0, gx - 2)
    y0 = np.clip(np.floor(fy).astype(int), 0, gy - 2)
    wx = np.clip(fx - x0, 0, 1)
    wy = np.clip(fy - y0, 0, 1)
    v = (sig[y0, x0] * (1 - wx) * (1 - wy) + sig[y0, x0 + 1] * wx * (1 - wy)
         + sig[y0 + 1, x0] * (1 - wx) * wy + sig[y0 + 1, x0 + 1] * wx * wy)
    return v, ok


def sparse_sigma_field(sig, stride=2):
    rec, _ = pn.bilinear_nodes(sig[::stride, ::stride], sig.shape, stride)
    out = np.full(sig.shape, np.nan)
    out[:rec.shape[0], :rec.shape[1]] = rec
    return out


def penalty_matrix(sig_matrix, weight_matrix):
    """sig_matrix, weight_matrix: (F, N).  Returns mean penalty and Var_p."""
    w = weight_matrix
    v = sig_matrix ** 2
    t = 1.0 / v
    p = t / t.sum(axis=0, keepdims=True)
    var_approx = (w ** 2 * v).sum(axis=0) / w.sum(axis=0) ** 2
    var_opt = 1.0 / t.sum(axis=0)
    pen = var_approx / var_opt
    d = w / t - 1.0
    dbar = (p * d).sum(axis=0)
    var_p = (p * (d - dbar) ** 2).sum(axis=0)
    return float(np.mean(pen)), float(np.mean(var_p)), float(np.mean(dbar))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="audit_mosaic_shape.json")
    ap.add_argument("--stride", type=int, default=2)
    a = ap.parse_args()
    t0 = time.time()
    out = {"audit": "SNR-EXP-AUDIT 4 -- SP-0 applicability in the mosaic regime "
                    "(real L4 data, different pointings)",
           "gap_audit_9_46": "SP-0 is a diagnostic, not a switch; its verdict "
                             "must not be generalised from the common-mode case"}

    frames = {}
    for tile in sorted(os.listdir(NORM)):
        fs = sorted(glob.glob(os.path.join(NORM, tile, "calibrated_*.fts")))
        for f in fs:
            if hdr0 := None:
                pass
            with __import__("astropy.io.fits", fromlist=["fits"]).open(f) as _h:
                _hdr = _h[0].header
            if _hdr.get("CRVAL1") is None or _hdr.get("CRVAL2") is None:
                continue
            sig, n_sky, wcs, hdr = frame_sigma_and_wcs(f)
            key = "%s/%s" % (tile, os.path.basename(f)[-38:-4])
            frames[key] = dict(tile=tile, path=f, sig=sig, n_sky=n_sky, wcs=wcs,
                               airmass=float(hdr.get("AIRMASS") or 0.0),
                               crval=(float(hdr.get("CRVAL1")), float(hdr.get("CRVAL2"))))
    out["n_frames_reduced"] = len(frames)

    def analyse(keys, tag, n_grid=48):
        ra_min, ra_max = -1e9, 1e9
        dec_min, dec_max = -1e9, 1e9
        fps = []
        for k in keys:
            fp = footprint(frames[k]["wcs"], frames[k]["sig"].shape)
            fps.append(fp)
            ra_min = max(ra_min, fp[:, 0].min()); ra_max = min(ra_max, fp[:, 0].max())
            dec_min = max(dec_min, fp[:, 1].min()); dec_max = min(dec_max, fp[:, 1].max())
        if ra_min >= ra_max or dec_min >= dec_max:
            return None
        ra = np.linspace(ra_min, ra_max, n_grid)
        dec = np.linspace(dec_min, dec_max, n_grid)
        RA, DEC = np.meshgrid(ra, dec)
        RA, DEC = RA.ravel(), DEC.ravel()
        S, OK = [], np.ones(RA.size, bool)
        for k in keys:
            v, ok = sample_sigma(frames[k]["sig"], frames[k]["wcs"], RA, DEC)
            S.append(v); OK &= ok
        S = np.stack(S)[:, OK]
        if S.shape[1] < 200:
            return None
        F = len(keys)
        # frame scalars
        w_mean = np.stack([np.full(S.shape[1], 1.0 / np.mean(frames[k]["sig"]) ** 2)
                           for k in keys])
        w_mad = np.stack([np.full(S.shape[1],
                                  1.0 / pn.whole_frame_mad(frames[k]["sig"]) ** 2)
                          for k in keys])
        # sparse: reconstruct each frame's sigma field from its own 64 px nodes
        w_sp = []
        for k in keys:
            fld = sparse_sigma_field(frames[k]["sig"], a.stride)
            v, ok2 = sample_sigma(fld, frames[k]["wcs"], RA, DEC)
            w_sp.append(1.0 / v[OK] ** 2)
        w_sp = np.stack(w_sp)
        res = dict(frames=keys, tile_mix=sorted({frames[k]["tile"] for k in keys}),
                   airmass=[frames[k]["airmass"] for k in keys],
                   n_sky_points=int(S.shape[1]),
                   sigma_median_adu=[float(np.median(s)) for s in S],
                   sigma_spread=float(max(np.median(s) for s in S)
                                      / min(np.median(s) for s in S)))
        for name, w in (("scalar_mean_of_patch_sigma", w_mean),
                        ("scalar_whole_frame_mad", w_mad),
                        ("sparse_64px_bilinear", w_sp)):
            pen, vp, db = penalty_matrix(S, w)
            res[name] = dict(penalty=pen, var_p_delta=vp, delta_bar=db,
                             sigma_penalty_pct=100 * (np.sqrt(pen) - 1))
        return res

    # (A) same tile, same pointing: the design's EXP-3 regime
    same = [k for k in frames if frames[k]["tile"] == "t2_m2_red"]
    out["A_same_tile_4_frames"] = analyse(same, "same_tile")

    # (B) mosaic regime: frames of different tiles / airmass sharing sky
    combos = []
    keys = sorted(frames)
    for i in range(len(keys)):
        for j in range(i + 1, len(keys)):
            if frames[keys[i]]["tile"] == frames[keys[j]]["tile"]:
                continue
            r = analyse([keys[i], keys[j]], "pair")
            if r:
                r["pair"] = [keys[i], keys[j]]
                combos.append(r)
    combos.sort(key=lambda r: -r["scalar_mean_of_patch_sigma"]["penalty"])
    out["B_cross_tile_pairs"] = combos[:8]

    # (C) worst case: mix the most different airmasses available in one set
    by_air = sorted(keys, key=lambda k: frames[k]["airmass"])
    mix = []
    for k in by_air:
        if not mix or all(abs(frames[k]["airmass"] - frames[m]["airmass"]) > 0.15
                          for m in mix):
            mix.append(k)
        if len(mix) == 4:
            break
    if len(mix) >= 2:
        out["C_extreme_airmass_mix"] = analyse(mix, "airmass_mix")
        if out["C_extreme_airmass_mix"]:
            out["C_extreme_airmass_mix"]["frames"] = mix

    out["elapsed_s"] = time.time() - t0
    with open(a.out, "w") as f:
        json.dump(out, f, indent=2, default=str)

    for tag in ("A_same_tile_4_frames", "C_extreme_airmass_mix"):
        r = out.get(tag)
        if not r:
            continue
        print("==", tag, "==", r["tile_mix"], "airmass", r["airmass"],
              "n_sky", r["n_sky_points"], "sigma spread %.3f" % r["sigma_spread"])
        for k in ("scalar_mean_of_patch_sigma", "scalar_whole_frame_mad",
                  "sparse_64px_bilinear"):
            print("   %-30s penalty %.6f  (%.4f%%)" % (k, r[k]["penalty"],
                                                       r[k]["sigma_penalty_pct"]))
    print("== B cross-tile pairs (top 8 by scalar penalty) ==")
    for r in out["B_cross_tile_pairs"]:
        print("   %-46s spread %.3f  scalar %.6f  sparse %.6f"
              % (" + ".join(x.split("/")[0] for x in r["pair"]), r["sigma_spread"],
                 r["scalar_mean_of_patch_sigma"]["penalty"],
                 r["sparse_64px_bilinear"]["penalty"]))
    print("wrote", a.out, "elapsed %.1fs" % out["elapsed_s"])


if __name__ == "__main__":
    main()
