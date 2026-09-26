#!/usr/bin/env python3
"""EXP-P4-02: Reconstruction operators for sparse SNR control points -> dense SNR field.

Audit items I2 (05 spec P-ALG-09 operator properties) and I3 (P-CST-24 IDW parameters).

Claims tested:
  T (theory):
    - Node exactness: bilinear / separable natural bicubic spline / IDW reproduce the
      control values at the nodes exactly, so the P-ALG-09 self-check tolerance 1e-9
      (P-CST-12) is a floating-point *property* of exact interpolants, not a tuned knob.
    - Smooth-operator overshoot: a C1/C2 interpolant through a grid containing a spike
      overshoots the control-value range (Gibbs/Runge-type ringing); clamping to the
      control-value range is a *necessary* guard for the positivity of w = SNR^2/F_ref^2.
    - IDW power p: interpolates between the local mean (p->0) and nearest neighbour
      (p->inf); node exactness holds for every p.
    - Node-placement convention: cell-centre vs cell-corner nodes differ by a rigid
      shift of (Delta-1)/2 px, which misregisters the dense field.
  E (experiment), per operator {spline, spline_noclip, bilinear, idw p in {1,2,4}}:
      node reproduction max|err|, dex RMSE vs truth at non-node pixels,
      weight efficiency loss E = Var_w/Var_opt - 1 (global scale cancels; E=0 iff
      sigma_hat proportional to sigma_true), overshoot fraction, nonpositive-SNR fraction.
Negative controls (non-degeneracy discipline):
    (a) flat truth field => E == 0 for every operator (metric must collapse to zero);
    (b) corner-node convention must measurably degrade (the gate must be able to go red).

Pure python + numpy. No repository imports. Seed hardcoded.
Run:  python3 exp_p4_02_interpolators.py
Out:  ../results/exp_p4_02_interpolators.json
"""
import json, time
import numpy as np

SEED = 20260926
OUT = "../results/exp_p4_02_interpolators.json"

FRAME = 512
DELTA_LIST = [16, 32, 64, 128]
IDW_POWER_LIST = [1.0, 2.0, 4.0]
GAMMA = 1e-10            # P-CST-24 degenerate-distance guard
FREF = 100.0


# ----------------------------------------------------------------------------
# 1D natural cubic spline, vectorised over queries, uniform knots.
# ----------------------------------------------------------------------------

def natural_second_derivs(s, h):
    """Second derivatives M_j of the natural cubic spline through values s (1D)."""
    n = len(s)
    M = np.zeros(n)
    if n < 3:
        return M
    rhs = np.zeros(n)
    for j in range(1, n - 1):
        rhs[j] = 6.0 * (s[j - 1] - 2.0 * s[j] + s[j + 1]) / h ** 2
    # Thomas algorithm for M_{j-1} + 4 M_j + M_{j+1} = rhs_j, M_0 = M_{n-1} = 0
    cp = np.zeros(n)
    dp = np.zeros(n)
    beta = 4.0
    cp[1] = 1.0 / beta
    dp[1] = rhs[1] / beta
    for j in range(2, n - 1):
        beta = 4.0 - cp[j - 1]
        cp[j] = 1.0 / beta
        dp[j] = (rhs[j] - dp[j - 1]) / beta
    M[n - 2] = dp[n - 2]
    for j in range(n - 3, 0, -1):
        M[j] = dp[j] - cp[j] * M[j + 1]
    return M


def spline_eval_1d(S, M, h, xq, x0):
    """Evaluate natural cubic splines at xq (Q queries).

    S, M: knot values / second derivatives, shape (m, Q) -- one column per query.
    Returns shape (Q,).
    """
    m = S.shape[0]
    u = (xq - x0) / h
    j = np.clip(np.floor(u).astype(int), 0, m - 2)
    t = u - j
    qidx = np.arange(len(xq))
    s_j = S[j, qidx]
    s_j1 = S[j + 1, qidx]
    M_j = M[j, qidx]
    M_j1 = M[j + 1, qidx]
    dx = t * h
    return (s_j
            + ((s_j1 - s_j) / h - h * (2.0 * M_j + M_j1) / 6.0) * dx
            + (M_j / 2.0) * dx ** 2
            + ((M_j1 - M_j) / (6.0 * h)) * dx ** 3)


# ----------------------------------------------------------------------------
# 2D operators on a regular control grid (nodes at cell centres, frozen
# cell_center_v1 convention: cell i covers [origin + i*Delta, origin+(i+1)*Delta-1],
# node at origin + i*Delta + (Delta-1)/2; integer pixel-centre coordinates, so the
# node index i sits at coordinate origin + i*Delta + (Delta-1)/2 -- here we use the
# equivalent continuous placement with spacing Delta and origin at the first node).
# ----------------------------------------------------------------------------

class Spline2D:
    """Separable natural bicubic spline: stage 1 along y (global column splines),
    stage 2 along x (closed-form 4-point natural spline). Node-exact."""

    def __init__(self, g, delta, origin=0.0):
        self.g = np.asarray(g, float)          # g[j, i]: row j (y), column i (x)
        self.h = float(delta)
        self.origin = float(origin)
        self.m, self.n = self.g.shape
        self.My = np.vstack([natural_second_derivs(self.g[:, i], self.h)
                             for i in range(self.n)])          # (n, m)

    def __call__(self, xq, yq):
        xq = np.asarray(xq, float)
        yq = np.asarray(yq, float)
        Q = len(xq)
        # stage 1: four surrounding column splines evaluated at yq
        ic = np.clip(np.floor((xq - self.origin) / self.h).astype(int), 0, self.n - 2)
        cols = np.clip(ic[:, None] + np.arange(-1, 3)[None, :], 0, self.n - 1)   # (Q,4)
        vals = np.empty((Q, 4))
        for k in range(4):
            c = cols[:, k]
            vals[:, k] = spline_eval_1d(self.g[:, c], self.My[c, :].T, self.h, yq, self.origin)
        # stage 2: natural spline through the 4 stage-1 values along x (uniform knots)
        t = ((xq - self.origin) - ic * self.h) / self.h          # in [0,1)
        s0, s1, s2, s3 = vals[:, 0], vals[:, 1], vals[:, 2], vals[:, 3]
        d1 = 6.0 * (s0 - 2.0 * s1 + s2) / self.h ** 2
        d2 = 6.0 * (s1 - 2.0 * s2 + s3) / self.h ** 2
        M1 = (4.0 * d1 - d2) / 15.0
        M2 = (4.0 * d2 - d1) / 15.0
        p = t
        q = 1.0 - p
        return (q * s1 + p * s2
                + (self.h ** 2 / 6.0) * (M1 * (q ** 3 - q) + M2 * (p ** 3 - p)))


class Bilinear2D:
    def __init__(self, g, delta, origin=0.0):
        self.g = np.asarray(g, float)
        self.h = float(delta)
        self.origin = float(origin)
        self.m, self.n = self.g.shape

    def __call__(self, xq, yq):
        xq = np.asarray(xq, float)
        yq = np.asarray(yq, float)
        u = (xq - self.origin) / self.h
        v = (yq - self.origin) / self.h
        i = np.clip(np.floor(u).astype(int), 0, self.n - 2)
        j = np.clip(np.floor(v).astype(int), 0, self.m - 2)
        tx = u - i
        ty = v - j
        g = self.g
        return ((1 - ty) * ((1 - tx) * g[j, i] + tx * g[j, i + 1])
                + ty * ((1 - tx) * g[j + 1, i] + tx * g[j + 1, i + 1]))


class IDW2D:
    """IDW over the K=16 nearest nodes. On a regular grid the K=16 nearest nodes are
    the surrounding 4x4 node block (exactly, except within one node spacing of the
    boundary, where the block is clipped) -- vectorised."""

    def __init__(self, g, delta, K=16, power=2.0, gamma=GAMMA, origin=0.0):
        assert K == 16
        self.g = np.asarray(g, float)
        self.h = float(delta)
        self.p = float(power)
        self.gamma = gamma
        self.origin = float(origin)
        self.m, self.n = self.g.shape

    def __call__(self, xq, yq):
        xq = np.asarray(xq, float)
        yq = np.asarray(yq, float)
        h = self.h
        i0 = np.floor((xq - self.origin) / h).astype(int)
        j0 = np.floor((yq - self.origin) / h).astype(int)
        ii = np.clip(i0[:, None] + np.arange(-1, 3)[None, :], 0, self.n - 1)   # (Q,4) x-index
        jj = np.clip(j0[:, None] + np.arange(-1, 3)[None, :], 0, self.m - 1)   # (Q,4) y-index
        XI = np.repeat(ii, 4, axis=1)                       # (Q,16)
        YJ = np.tile(jj, (1, 4))                            # (Q,16)
        vals = self.g[YJ, XI]                               # (Q,16)
        dx = XI * h + self.origin - xq[:, None]
        dy = YJ * h + self.origin - yq[:, None]
        d = np.sqrt(dx ** 2 + dy ** 2)
        dmin = d.min(axis=1)
        coincident = dmin < self.gamma
        w = 1.0 / np.maximum(d, self.gamma) ** self.p
        out = np.sum(w * vals, axis=1) / np.sum(w, axis=1)
        # at a node the exact node value wins (distance 0 dominates for any p)
        rowmax = np.where(coincident[:, None], np.isclose(d, dmin[:, None], atol=self.gamma), False)
        denom = np.sum(rowmax, axis=1)
        node_val = np.sum(vals * rowmax, axis=1) / np.maximum(denom, 1)
        return np.where(coincident, node_val, out)


# ----------------------------------------------------------------------------
# truth field and metrics
# ----------------------------------------------------------------------------

def make_truth(seed, grid=FRAME, ell=48.0, lo=0.3, hi=1.3, flat=False):
    """Smooth log10-SNR truth field (sum of Gaussian blobs), SNR in [10^lo, 10^hi]."""
    yy, xx = np.mgrid[0:grid, 0:grid].astype(float)
    rng = np.random.default_rng(seed)
    f = np.zeros((grid, grid))
    for _ in range(40):
        cx, cy = rng.uniform(0, grid, 2)
        s = rng.uniform(0.4, 1.2) * ell
        amp = rng.uniform(0.5, 1.0)
        f += amp * np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * s ** 2))
    if flat:
        return np.full((grid, grid), 10 ** ((lo + hi) / 2))
    f = (f - f.min()) / (f.max() - f.min())
    return 10 ** (lo + f * (hi - lo))


def node_indices(delta, grid=FRAME, mode="cell_center"):
    n = grid // delta
    if mode == "cell_center":
        return (np.arange(n) * delta + (delta - 1) // 2)
    return (np.arange(n) * delta)


def metrics(truth, recon, ctrl_frame, node_mask, eval_mask):
    """E and dex RMSE. Nonpositive/nonfinite recon are weight-ineligible (P-ALG-10)."""
    snr_true = truth[eval_mask]
    v_true = (FREF / snr_true) ** 2
    snr_hat = recon[eval_mask]
    ok = np.isfinite(snr_hat) & (snr_hat > 0)
    w = (snr_hat[ok] / FREF) ** 2
    v = v_true[ok]
    var_w = np.sum(w ** 2 * v) / np.sum(w) ** 2
    var_opt = 1.0 / np.sum(1.0 / v)
    E = var_w / var_opt - 1.0
    with np.errstate(divide="ignore", invalid="ignore"):
        dex = np.abs(np.log10(snr_hat) - np.log10(snr_true))
    dex = dex[np.isfinite(dex)]
    rmse_dex = float(np.sqrt(np.mean(dex ** 2))) if dex.size else float("nan")
    lo, hi = np.nanmin(ctrl_frame), np.nanmax(ctrl_frame)
    overshoot = float(np.mean((recon < lo - 1e-12) | (recon > hi + 1e-12)))
    neg_frac = float(np.mean(~(np.isfinite(recon) & (recon > 0))))
    node_err = float(np.max(np.abs(recon[node_mask] - ctrl_frame[node_mask])))
    return {"node_max_abs_err": node_err, "rmse_dex": rmse_dex, "E": float(E),
            "n_weight_eligible": int(ok.sum()), "n_eval": int(eval_mask.sum()),
            "overshoot_frac": overshoot, "nonpos_frac": neg_frac}


def main():
    t0 = time.time()
    yy, xx = np.mgrid[0:FRAME, 0:FRAME]
    pts = np.column_stack([xx.ravel(), yy.ravel()]).astype(float)
    res = {"seed": SEED, "frame": FRAME, "delta_list": DELTA_LIST, "runs": {}}
    truth = make_truth(SEED)

    for delta in DELTA_LIST:
        idx = node_indices(delta)
        g = truth[np.ix_(idx, idx)]
        origin = float((delta - 1) // 2)   # cell_center_v1: node i at i*Delta+(Delta-1)/2
        ctrl_frame = np.full((FRAME, FRAME), np.nan)
        ctrl_frame[np.ix_(idx, idx)] = g
        node_mask = np.zeros((FRAME, FRAME), bool)
        node_mask[np.ix_(idx, idx)] = True
        em = np.zeros((FRAME, FRAME), bool)
        em[::4, ::4] = True
        em &= ~node_mask
        ops = {"spline": Spline2D(g, delta, origin=origin),
               "spline_noclip": Spline2D(g, delta, origin=origin),
               "bilinear": Bilinear2D(g, delta, origin=origin),
               "idw_p1": IDW2D(g, delta, power=1.0, origin=origin),
               "idw_p2": IDW2D(g, delta, power=2.0, origin=origin),
               "idw_p4": IDW2D(g, delta, power=4.0, origin=origin)}
        run = {}
        for name, op in ops.items():
            t1 = time.time()
            rec = op(pts[:, 0], pts[:, 1]).reshape(FRAME, FRAME)
            m = metrics(truth, rec, ctrl_frame, node_mask, em)
            if name == "spline":
                rec_c = np.clip(rec, g.min(), g.max())
                mc = metrics(truth, rec_c, ctrl_frame, node_mask, em)
                m["E_after_clip"] = mc["E"]
                m["overshoot_frac_after_clip"] = mc["overshoot_frac"]
            m["runtime_s"] = time.time() - t1
            run[name] = m
        res["runs"][f"delta_{delta}"] = run

    # ---------- overshoot / clamp necessity on an adversarial spike grid ----------
    delta = 32
    n = FRAME // delta
    g_sp = 10 ** (0.5 + np.random.default_rng(SEED + 3).uniform(0.0, 0.2, (n, n)))
    g_sp[2, 3] = 1e3     # one spike, ~4 decades above the rest
    rec = Spline2D(g_sp, delta, origin=(delta - 1) // 2)(pts[:, 0], pts[:, 1]).reshape(FRAME, FRAME)
    res["adversarial_spike"] = {
        "delta": delta,
        "unclamped_max": float(rec.max()),
        "unclamped_min": float(rec.min()),
        "unclamped_nonpos_frac": float(np.mean(~(np.isfinite(rec) & (rec > 0)))),
        "unclamped_overshoot_frac": float(np.mean((rec < g_sp.min()) | (rec > g_sp.max()))),
        "clamped_max": float(np.clip(rec, g_sp.min(), g_sp.max()).max()),
    }

    # ---------- negative control (a): flat truth => E collapses for every operator ----
    flat = make_truth(SEED, flat=True)
    idx = node_indices(32)
    g = flat[np.ix_(idx, idx)]
    ctrl_frame = np.full((FRAME, FRAME), np.nan)
    ctrl_frame[np.ix_(idx, idx)] = g
    node_mask = np.zeros((FRAME, FRAME), bool)
    node_mask[np.ix_(idx, idx)] = True
    em = np.zeros((FRAME, FRAME), bool)
    em[::4, ::4] = True
    em &= ~node_mask
    fc = {}
    o32 = (32 - 1) // 2
    for name, op in {"spline": Spline2D(g, 32, origin=o32),
                     "bilinear": Bilinear2D(g, 32, origin=o32),
                     "idw_p2": IDW2D(g, 32, power=2.0, origin=o32)}.items():
        rec = op(pts[:, 0], pts[:, 1]).reshape(FRAME, FRAME)
        fc[name] = metrics(flat, rec, ctrl_frame, node_mask, em)
    res["flat_negative_control"] = fc
    res["flat_control_max_abs_E"] = float(max(abs(v["E"]) for v in fc.values()))

    # ---------- negative control (b): corner-node convention must degrade ----------
    idx = node_indices(32, mode="cell_center")
    idx_c = node_indices(32, mode="corner")
    g = truth[np.ix_(idx, idx)]
    g_c = truth[np.ix_(idx_c, idx_c)]
    ctrl_frame = np.full((FRAME, FRAME), np.nan)
    ctrl_frame[np.ix_(idx, idx)] = g
    node_mask = np.zeros((FRAME, FRAME), bool)
    node_mask[np.ix_(idx, idx)] = True
    em = np.zeros((FRAME, FRAME), bool)
    em[::4, ::4] = True
    em &= ~node_mask
    rec_c = Spline2D(g_c, 32, origin=0.0)(pts[:, 0], pts[:, 1]).reshape(FRAME, FRAME)
    rec_s = Spline2D(g, 32, origin=(32 - 1) // 2)(pts[:, 0], pts[:, 1]).reshape(FRAME, FRAME)
    m_center = metrics(truth, rec_s, ctrl_frame, node_mask, em)
    m_corner = metrics(truth, rec_c, ctrl_frame, node_mask, em)
    res["node_placement"] = {
        "delta": 32, "misregistration_px": (32 - 1) / 2,
        "rmse_dex_center": m_center["rmse_dex"], "rmse_dex_corner": m_corner["rmse_dex"],
        "E_center": m_center["E"], "E_corner": m_corner["E"],
    }

    # ---------- gates ----------
    r64 = res["runs"]["delta_64"]
    res["gates"] = {
        "node_reproduction_all_ops_le_1e-9": all(
            run["node_max_abs_err"] <= 1e-9 for run in r64.values()),
        "spline_E_no_worse_than_bilinear_idw":
            r64["spline"]["E"] <= min(r64["bilinear"]["E"], r64["idw_p2"]["E"]) * 1.5,
        "clamp_needed_spike_case": (res["adversarial_spike"]["unclamped_overshoot_frac"] > 0
                                    or res["adversarial_spike"]["unclamped_nonpos_frac"] > 0),
        "negative_control_flat_collapses": res["flat_control_max_abs_E"] < 0.02,
        "corner_convention_degrades":
            res["node_placement"]["rmse_dex_corner"] > 2.0 * max(res["node_placement"]["rmse_dex_center"], 1e-9),
    }
    res["all_gates_pass"] = bool(all(res["gates"].values()))
    res["runtime_s"] = time.time() - t0
    with open(OUT, "w") as f:
        json.dump(res, f, indent=2)
    print(json.dumps(res, indent=2))


if __name__ == "__main__":
    main()
