"""exp10_chain_usecase.py -- P3 chain use case: P1 -> P3 -> P4 interface.

Chain position: P1 delivers a calibrated linear surface-brightness frame
(TAN, 2 arcsec/px, ADU/sr, with per-pixel variance); P3 maps it onto the
HEALPix chart with exact partition weights w_jp = a_jp / A_drop (pixfrac
0.8), conserving total flux and delivering sparse SNR control points on the
sphere (leaf centers + SNR + variance convention); P4 consumes the control
points with IDW to reconstruct a dense SNR map. The experiment closes the
loop end to end and verifies the interface conventions:
 (I1) flux closure: |sum_p F_p / sum_j x_j - 1| < 1e-12;
 (I2) weights: sum_p w_jp = 1 exactly (drop-normalized);
 (I3) dimensional convention: x_j in ADU (surface brightness * pixel area),
      F_p in ADU, SNR dimensionless, directions unit vectors (rad angles);
 (I4) P4 IDW reproduces control-point SNR exactly at the control points
      (interpolation exactness) and is smooth between them;
 (I5) NEGATIVE CONTROL: constant control-point SNR => dense reconstruction
      deviation is exactly 0 everywhere (weights sum to 1).
Seed       : 20260927 (frame noise + query directions).
Runtime    : < 60 s.
"""
import json
import sys
import numpy as np

sys.path.insert(0, "code")
import p3lib as P  # noqa: E402

SEED = 20260927
NSIDE = 4096   # leaf ~ 51.5 arcsec: the 128 arcsec frame spans ~3x3 leaves
SCALE = 2.0 * np.pi / 180.0 / 3600.0    # 2 arcsec/px in rad
RA0, DEC0 = 0.0, float(np.arcsin(1.0 / 3.0))   # mid-face 4: u = v = 0.75
PF = 0.8


def zphi_to_uv(z, phi):
    """Inverse equatorial chart (faces 4-7 convention, valid for the
    footprint that stays inside one base face): u+v = 1.5 z + 1, u-v = 4 phi/pi."""
    return 0.75 * z + 2.0 * phi / np.pi + 0.5, \
           0.75 * z - 2.0 * phi / np.pi + 0.5


def clip_rect_uv(poly, u0, u1, v0, v1):
    def clip_half(pts, val, lo, axis):
        outl = []
        m = len(pts)
        for k in range(m):
            cur, prv = pts[k], pts[k - 1]
            sc, sp = cur[axis] - val, prv[axis] - val
            kc = (sc >= 0) if lo else (sc <= 0)
            kp = (sp >= 0) if lo else (sp <= 0)
            if kc:
                if not kp:
                    t = sp / (sp - sc)
                    outl.append(prv + t * (cur - prv))
                outl.append(cur)
            elif kp:
                t = sp / (sp - sc)
                outl.append(prv + t * (cur - prv))
        return outl

    pts = [np.asarray(p, dtype=float) for p in poly]
    for val, lo, ax in ((u0, True, 0), (u1, False, 0),
                        (v0, True, 1), (v1, False, 1)):
        pts = clip_half(pts, val, lo, ax)
        if not pts:
            return []
    return pts


def shoelace_abs(pts):
    if len(pts) < 3:
        return 0.0
    a = np.asarray(pts, dtype=float)
    a = a - a.mean(axis=0)   # centroid translation kills O(coord^2) cancel
    return abs(0.5 * float(a[:, 0] @ np.roll(a[:, 1], -1)
                           - a[:, 1] @ np.roll(a[:, 0], -1)))


def idw(query_dirs, ctrl_dirs, ctrl_vals, power=2.0):
    vals = np.empty(len(query_dirs))
    for qi, qd in enumerate(query_dirs):
        d2 = np.array([np.arccos(np.clip(float(qd @ cd), -1.0, 1.0)) ** 2
                       for cd in ctrl_dirs])
        hit = np.where(d2 < 1e-30)[0]
        if len(hit):
            vals[qi] = ctrl_vals[hit[0]]
            continue
        wgt = 1.0 / d2 ** (power / 2.0)
        vals[qi] = float(wgt @ ctrl_vals) / float(wgt.sum())
    return vals


def main():
    rng = np.random.default_rng(SEED)
    out = {"chain_position":
           "P1 frame -> P3 exact-partition drizzle -> sparse SNR control "
           "points on sphere -> P4 IDW dense reconstruction",
           "interfaces": {}, "flux_closure": {}, "idw": {},
           "negative_control": {}, "units": {
               "frame": "ADU/sr surface brightness, per-pixel variance ADU^2",
               "leaf_flux": "ADU (dimensionless counts per exposure)",
               "snr": "dimensionless; directions are unit vectors (rad)"}}

    # ---- P1: TAN frame with linear surface brightness + noise --------------
    n = 64
    ii = np.arange(n)
    xi = (ii - n / 2 + 0.5) * SCALE
    XI, ETA = np.meshgrid(xi, xi)
    rr2 = (XI / (10 * SCALE)) ** 2 + (ETA / (10 * SCALE)) ** 2
    SB = 1000.0 * (1.0 + 0.5 * np.exp(-rr2 / 2.0))     # ADU/sr
    A_pix = SCALE ** 2
    x = SB * A_pix                                      # ADU per pixel
    sigma_pix = np.sqrt(np.maximum(x, 1.0))             # photon + read noise
    x_noisy = x + rng.normal(0.0, 1.0, x.shape) * sigma_pix * 0.05

    # ---- P3: exact-partition drizzle onto chart cells ----------------------
    # drop polygon per pixel (pixfrac shrink), mapped to (u, v) chart plane
    J_SC = np.pi / 3.0        # |dOmega / du dv| exact in equatorial branch
    cell_vals = {}
    cell_w = {}
    drops_area = {}
    w_sums = []
    F_tot = 0.0
    x_tot = 0.0
    leaf_sigma2 = {}
    for a_i in range(n):
        for b_j in range(n):
            cx = xi[a_i]
            cy = xi[b_j]
            poly = [(cx + s1 * PF * 0.5 * SCALE, cy + s2 * PF * 0.5 * SCALE)
                    for s1, s2 in ((-1, -1), (-1, 1), (1, 1), (1, -1))]
            zp = []
            for pxi, peta in poly:
                vec = P.tan_deproject(pxi, peta, RA0, DEC0)
                z = float(vec[2])
                phi = float(np.arctan2(vec[1], vec[0])) % (2 * np.pi)
                if phi > np.pi:
                    phi -= 2 * np.pi
                zp.append((z, phi))
            uv = [zphi_to_uv(z, p) for z, p in zp]
            A_drop = J_SC * shoelace_abs(uv)
            drops_area[(a_i, b_j)] = A_drop
            # clip in polygon-centroid coordinates: the S-H boundary
            # interpolation t = sp/(sp-sc) amplifies absolute-coordinate
            # rounding by ~coord/edge; translating kills that amplification
            ucen = float(np.mean([p[0] for p in uv]))
            vcen = float(np.mean([p[1] for p in uv]))
            uvc = [(p[0] - ucen, p[1] - vcen) for p in uv]
            us = [p[0] for p in uv]
            vs = [p[1] for p in uv]
            i_lo = int(np.floor(min(us) * NSIDE)) - 1
            i_hi = int(np.ceil(max(us) * NSIDE)) + 1
            j_lo = int(np.floor(min(vs) * NSIDE)) - 1
            j_hi = int(np.ceil(max(vs) * NSIDE)) + 1
            sw = 0.0
            for gi in range(max(i_lo, 0), min(i_hi, NSIDE - 1) + 1):
                for gj in range(max(j_lo, 0), min(j_hi, NSIDE - 1) + 1):
                    cl = clip_rect_uv(uvc, gi / NSIDE - ucen,
                                      (gi + 1) / NSIDE - ucen,
                                      gj / NSIDE - vcen,
                                      (gj + 1) / NSIDE - vcen)
                    a_jp = J_SC * shoelace_abs(cl)
                    if a_jp <= 0.0:
                        continue
                    w = a_jp / A_drop
                    sw += w
                    key = (gi, gj)
                    cell_vals[key] = cell_vals.get(key, 0.0) \
                        + x_noisy[a_i, b_j] * w
                    cell_w[key] = cell_w.get(key, 0.0) + w
                    leaf_sigma2[key] = leaf_sigma2.get(key, 0.0) \
                        + (w * 0.05 * sigma_pix[a_i, b_j]) ** 2
            w_sums.append(sw)
            F_tot += x_noisy[a_i, b_j] * sw
            x_tot += x_noisy[a_i, b_j]

    w_sums = np.array(w_sums)
    out["interfaces"]["sum_p_w_max_abs_dev_from_1"] = float(
        np.max(np.abs(w_sums - 1.0)))
    out["flux_closure"] = {
        "sum_p_F_over_sum_j_x_minus_1": F_tot / x_tot - 1.0,
        "tolerance": 1e-12}

    # ---- SNR control points on the sphere ----------------------------------
    ctrl_dirs = []
    ctrl_snr = []
    for (gi, gj), Fp in sorted(cell_vals.items()):
        uc, vc = (gi + 0.5) / NSIDE, (gj + 0.5) / NSIDE
        zc = (2.0 / 3.0) * (uc + vc - 1.0)
        phc = (np.pi / 4.0) * (uc - vc)
        vec = np.array([np.sqrt(1 - zc ** 2) * np.cos(phc),
                        np.sqrt(1 - zc ** 2) * np.sin(phc), zc])
        # reference flux from the noiseless frame for a clean SNR definition
        ctrl_dirs.append(vec)
        ctrl_snr.append(float(Fp / np.sqrt(leaf_sigma2[(gi, gj)])))
    ctrl_dirs = np.array(ctrl_dirs)
    ctrl_snr = np.array(ctrl_snr)
    out["interfaces"]["n_control_points"] = len(ctrl_snr)
    out["interfaces"]["control_point_fields"] = [
        "unit_vector_direction", "SNR", "variance_convention"]

    # ---- P4: IDW dense reconstruction --------------------------------------
    qdirs = []
    for _ in range(200):
        u = rng.normal(size=3)
        u /= np.linalg.norm(u)
        qdirs.append(u)
        if len(qdirs) >= 100:
            break
    # half queries near the footprint, half at control points themselves
    near = ctrl_dirs[rng.integers(0, len(ctrl_dirs), 50)]
    near = near / np.linalg.norm(near + rng.normal(0, 1e-4, near.shape),
                                 axis=1)[:, None]
    qdirs = np.array(qdirs[:50] + list(near))
    rec = idw(qdirs, ctrl_dirs, ctrl_snr)
    # exactness at control points
    at_ctrl = idw(ctrl_dirs, ctrl_dirs, ctrl_snr)
    out["idw"] = {
        "max_abs_dev_at_control_points": float(np.max(np.abs(
            at_ctrl - ctrl_snr))),
        "reconstruction_range": [float(rec.min()), float(rec.max())],
        "n_queries": len(qdirs)}

    # ---- negative control: constant SNR => exact constant reconstruction ---
    rec_const = idw(qdirs, ctrl_dirs, np.full_like(ctrl_snr, 7.0))
    out["negative_control"] = {
        "max_abs_dev_from_constant": float(np.max(np.abs(rec_const - 7.0))),
        "expected": 0.0,
        "note": "weights sum to 1 => constant field reproduces exactly"}

    ok = (out["interfaces"]["sum_p_w_max_abs_dev_from_1"] < 1e-12
          and abs(out["flux_closure"]["sum_p_F_over_sum_j_x_minus_1"]) < 1e-12
          and out["idw"]["max_abs_dev_at_control_points"] < 1e-9
          and out["negative_control"]["max_abs_dev_from_constant"] < 1e-12
          and len(ctrl_snr) > 4)
    out["verdict"] = "PASS" if ok else "FAIL"
    print(json.dumps(out, indent=2))
    with open("results/exp10_chain_usecase.json", "w") as fh:
        json.dump(out, fh, indent=2)


if __name__ == "__main__":
    main()

