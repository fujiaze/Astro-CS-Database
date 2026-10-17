#!/usr/bin/env python3
"""Small independent P5 additive-field GLS oracle with exact common-mode elimination.

Requires NumPy. No FITS files or production C++ are read. It is a research
reference implementation for the fixed-gain linear problem only, NOT the
complete nonlinear residual-gain solver, robust IRLS, or a proof of real E2.
The caller supplies a correct common-scene H (including fixed gain/PSF),
a per-frame additive evaluation design A (same as actual apply basis),
a nullspace parametrization G for explicit gauge constraints, and full
SPD observation covariance C. The minimized objective is
  (y - H*S - A*G*beta)^T C^-1 (y - H*S - A*G*beta).
"""
from __future__ import annotations

import numpy as np


def projected_gls(y, H, A, G, C, rtol=1e-10):
    """Return scene, physical additive coefficients, objective, additive rank.

    The projection is performed in the C^-1/2 whitened space using SVD.
    A zero/near-zero singular direction in the gauge-constrained additive
    design is an *identifiability failure*, not permission for arbitrary ridge.
    """
    y = np.asarray(y, dtype=float)
    H = np.asarray(H, dtype=float)
    A = np.asarray(A, dtype=float)
    G = np.asarray(G, dtype=float)
    C = np.asarray(C, dtype=float)
    if (y.ndim != 1 or H.ndim != 2 or A.ndim != 2 or G.ndim != 2
            or C.shape != (y.size, y.size) or H.shape[0] != y.size
            or A.shape[0] != y.size or G.shape[0] != A.shape[1]
            or H.shape[1] == 0 or G.shape[1] == 0
            or not all(np.all(np.isfinite(v)) for v in (y, H, A, G, C))
            or not np.isfinite(rtol) or not 0 < rtol < 1):
        raise ValueError("invalid arrays, finite entries or rtol")
    chol = np.linalg.cholesky(C)
    yw = np.linalg.solve(chol, y)
    Hw = np.linalg.solve(chol, H)
    Bw = np.linalg.solve(chol, A @ G)
    U, s, _ = np.linalg.svd(Hw, full_matrices=False)
    rank_h = int(np.count_nonzero(s > rtol*s[0])) if s.size else 0
    if rank_h != H.shape[1]:
        raise ValueError(f"common scene unsupported: rank={rank_h}/{H.shape[1]}")
    Uh = U[:, :rank_h]
    # Weighted projector Q: removes only the modelled *common* astronomical sky.
    rhs = yw - Uh @ (Uh.T @ yw)
    Bp = Bw - Uh @ (Uh.T @ Bw)
    s2 = np.linalg.svd(Bp, compute_uv=False)
    rank_a = int(np.count_nonzero(s2 > rtol*s2[0])) if s2.size else 0
    if rank_a != G.shape[1]:
        raise ValueError(
            f"relative additive field unidentifiable: rank={rank_a}/{G.shape[1]}"
        )
    beta = np.linalg.lstsq(Bp, rhs, rcond=rtol)[0]
    sky = np.linalg.lstsq(Hw, yw - Bw @ beta, rcond=rtol)[0]
    obj = float(np.linalg.norm(yw - Hw @ sky - Bw @ beta)**2)
    return sky, G @ beta, obj, rank_a


def synthetic_demo():
    # Three frames, seven spatial locations, fixed but *different* gains.
    nframes = 3
    x = np.linspace(-1, 1, 7)
    truth_sky = np.array([7.0, 8.5, 9.7, 11.2, 9.0, 8.2, 10.1])
    gains = (1.0, 1.1, 0.8)
    truth_add = np.array([[0.2, 0.1], [-0.1, 0.25], [-0.1, -0.35]])
    # Gauge: sum_k additive constant = sum_k additive slope = 0.
    m = nframes * x.size
    H = np.zeros((m, x.size))
    A = np.zeros((m, nframes * 2))
    y = np.zeros(m)
    for k, gain in enumerate(gains):
        for p, xx in enumerate(x):
            t = k * x.size + p
            H[t, p] = gain
            A[t, 2*k:2*k+2] = (1, xx)  # same basis in fit and evaluation
            y[t] = gain * truth_sky[p] + A[t] @ truth_add.flatten()
    G = np.zeros((nframes*2, (nframes-1)*2))
    for k in range(nframes - 1):
        for j in range(2):
            G[2*k+j, 2*k+j] = 1
            G[2*(nframes-1)+j, 2*k+j] = -1
    var = 1 + 0.03*np.arange(m)
    # SPD with simple nonzero inter-sample covariance.
    C = np.diag(var)
    for i in range(m-1):
        C[i, i+1] = C[i+1, i] = 0.02
    sky, add, obj, rank = projected_gls(y, H, A, G, C)
    assert np.max(np.abs(sky - truth_sky)) < 1e-9
    assert np.max(np.abs(add - truth_add.flatten())) < 1e-9
    assert obj < 1e-16
    assert rank == 4
    # Negative control: no x diversity => the slope parameters are unidentifiable.
    A_bad = A.copy()
    for k in range(nframes):
        for p in range(x.size):
            A_bad[k*x.size+p, 2*k+1] = 0
    try:
        projected_gls(y, H, A_bad, G, C)
    except ValueError as ex:
        assert "unidentifiable" in str(ex)
    else:
        raise AssertionError("a rank-deficient additive model was accepted")
    print("P5 fixed-gain profiled GLS: exact noiseless recovery, correlated C,")
    print("gauge-constrained additive rank=4, unsupported slope rejected.")


if __name__ == "__main__":
    synthetic_demo()
