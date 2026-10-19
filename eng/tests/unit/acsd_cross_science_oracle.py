#!/usr/bin/env python3
"""ACSD R1-R5 cross-model mathematical oracle, NOT C++ or real FITS E2E.

Run: python3 eng/tests/unit/acsd_cross_science_oracle.py
Requires NumPy. Each comparison uses its own algebraic construction,
not the ACSD production functions. Reference: Issues #97-#103.
"""
import math
import numpy as np


def close(a, b, tol=1e-11):
    assert np.allclose(a, b, rtol=tol, atol=tol), (a, b)


def hpx_geometry():
    # Analytic true north-polar apex leaf and half-strip directly
    # integrated with the physical spherical area element (2 sigma / 3).
    for nside in (4, 16, 256, 8192):
        h = 1.0 / nside
        omega_hpx = math.pi * h * h / 3
        half = 2 * math.pi * h * h / 9
        close(half / omega_hpx, 2 / 3)
    # Locally unit-Jacobian SIP warp eta=v+k*u^2; halfplane eta<0.
    k = 0.12
    true_intersection = 0.5 - k / 12
    corners_only = 0.5 - k / 4
    close(true_intersection, 0.49)
    close(corners_only, 0.47)
    assert abs(true_intersection - corners_only) > 0.01
    return true_intersection, corners_only


def fisher_scene_distinct():
    pa, pb = np.array([0.5, 0.5]), np.array([0.9, 0.1])
    va, vb = 0.5, 0.82
    close((pa @ pa) / va, 1)
    close((pb @ pb) / vb, 1)
    assert va != vb
    return va, vb


def sky_same_data_cross():
    x = np.column_stack([np.ones(4), np.arange(4, dtype=float)])
    c = np.diag(np.arange(1, 5, dtype=float))
    l = np.linalg.solve(x.T @ x, x.T)
    p = x @ l
    correct = (np.eye(4) - p) @ c @ (np.eye(4) - p).T
    incorrect = c + p @ c @ p.T  # pretends beta independent of y
    close(np.diag(correct), [0.6, 1.3, 2.2, 0.9])
    close(np.diag(incorrect), [2., 2.5, 4., 6.5])
    return np.diag(correct).tolist()


def shared_parameter_gls():
    independent = np.array([1., 4., 9.])
    common = 0.25
    gamma = np.diag(independent) + common * np.ones((3, 3))
    a = np.linalg.solve(gamma, np.ones(3))
    a = a / a.sum()
    variance = a @ gamma @ a
    close(a, [36 / 49, 9 / 49, 4 / 49])
    close(variance, 0.9846938775510203)
    wrong = 1 / np.sum(1 / (independent + common))
    assert wrong < variance
    return float(variance), float(wrong)


def percentile_not_translation_equivariant():
    def statistic(x):
        m = float(np.median(x))
        chosen = x[(x >= m - 0.2 * abs(m)) & (x <= m + 0.1 * abs(m))]
        return float(chosen.mean())
    x = np.array([-2., -1., 0., 1., 2.])
    close(statistic(x), 0)
    close(statistic(x + 10), 9.5)
    assert statistic(x + 10) != statistic(x) + 10


def narrowband_edge_integral():
    """Boundary-aware analytic integral vs naive 2 nm stepped sampling (#104).

    Units are the existing XPMODEL-style integrand convention: f_lambda * QE * nm.
    Deliberately holds QE=0.8 known rather than silently setting unknown QE to 1.
    """
    grid = np.arange(652., 664., 2.)
    results = []
    for width in (3., 5., 7.):
        center = 656.5  # not aligned with the even-nm grid
        left, right = center - width / 2., center + width / 2.
        exact = 0.8 * (right * right - left * left) / 2.
        values = np.where((grid >= left) & (grid <= right), 0.8 * grid, 0.)
        sampled = (np.trapezoid(values, grid) if hasattr(np, "trapezoid")
                   else np.trapz(values, grid))
        assert abs(sampled - exact) / exact > 0.05, (width, sampled, exact)
        results.append((width, float(exact), float(sampled)))
    # Same FWHM 3 nm and center; triangle at offset 1.3 nm differs from top-hat.
    line = 1.3
    top_hat = 1.0
    triangular = 1.0 - line / 3.0
    assert 0 < triangular < top_hat
    return results, (top_hat, triangular)


def mixed_response():
    # Two line probes: H-alpha and [N II]; spatially fixed weights.
    r = np.array([[1.0, 0.1], [0.6, 0.9]])
    weights = np.array([0.8, 0.2])
    close(weights @ r, [0.92, 0.26])
    assert not np.allclose(weights @ r, r[0])


if __name__ == "__main__":
    a, b = hpx_geometry()
    fisher = fisher_scene_distinct()
    sky = sky_same_data_cross()
    gls, incorrect = shared_parameter_gls()
    percentile_not_translation_equivariant()
    mixed_response()
    nb, lines = narrowband_edge_integral()
    print("R2 SIP true/corners:", a, b)
    print("R3 equal Fisher, unequal leaf variance:", fisher)
    print("R4 same-data corrected covariance diagonal:", sky)
    print("R3/R5 shared GLS versus wrong independent:", gls, incorrect)
    print("R1 top-hat analytic vs coarse 2nm sampling:", nb, "line shapes:", lines)
    print("R5 percentile shift counterexample and mixed response: confirmed")
    print("Scope: mathematical oracle only; C++/HST/M42 NOT RUN")
