#!/usr/bin/env python3
"""Independent ACSD P5 overlap-graph and P3 planar affine-reproduction oracles.

Only NumPy required. These are small-matrix mathematical controls, not an
HEALPix geometric implementation, Gaia flux test, or production E2 result.
Run: python3 实验/innovation-closure/code/graph_and_affine_oracles.py
"""
import unittest
import numpy as np


def component_labels(n_nodes, edges):
    """Connected-component labels for nodes 0..n-1 (isolates included)."""
    if n_nodes < 1:
        raise ValueError("empty graph")
    parent = list(range(n_nodes))

    def root(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for i, j, w, value in edges:
        if not (0 <= i < n_nodes and 0 <= j < n_nodes and i != j
                and np.isfinite(w) and w > 0 and np.isfinite(value)):
            raise ValueError("invalid edge")
        a, b = root(i), root(j)
        parent[b] = a
    roots = [root(i) for i in range(n_nodes)]
    idx = {r: k for k, r in enumerate(sorted(set(roots)))}
    return np.array([idx[r] for r in roots], dtype=int)


def relative_log_gain_fit(n_nodes, edges, tol=1e-10):
    """Solve edge model d_ij = eta_i-eta_j + e; per-component zero-sum gauge.

    Weighted observation noise model: independent edges, Var(e_ij)=1/weight.
    Parameter covariance is the pseudoinverse of the graph Laplacian on the
    identifiable per-component subspaces. No absolute log gain identifiable.
    """
    labels = component_labels(n_nodes, edges)
    n_components = len(set(labels))
    B = np.zeros((len(edges), n_nodes), dtype=float)
    y = np.zeros(len(edges), dtype=float)
    w = np.zeros(len(edges), dtype=float)
    for k, (i, j, invvar, diff) in enumerate(edges):
        B[k, i], B[k, j] = 1.0, -1.0
        y[k], w[k] = diff, invvar
    if not len(edges):
        return np.zeros(n_nodes), np.zeros((n_nodes, n_nodes)), labels, 0.0
    L = B.T @ (w[:, None] * B)
    z = B.T @ (w * y)
    eigval, Q = np.linalg.eigh(L)
    # Singular spectrum has exactly one gauge per component. Do not declare
    # a barely-connected edge "healthy" merely because its inverse exists.
    thresh = tol * max(float(np.max(eigval)), 1.0)
    rank = int(np.sum(eigval > thresh))
    if rank != n_nodes - n_components:
        raise ValueError(f"numerically rank deficient: {rank} < {n_nodes-n_components}")
    pinv = (Q[:, eigval > thresh] / eigval[eigval > thresh]) @ Q[:, eigval > thresh].T
    eta = pinv @ z
    loss = float(np.sum(w * (B @ eta - y)**2))
    return eta, pinv, labels, loss


def affine_barycentric(q, target=(0.0, 0.0), tol=1e-12):
    """3 point *planar* triangle coefficients, with scientific support status.

    Affine precision requires sum a=1, sum a*x=x_target, sum a*y=y_target.
    No extrapolation beyond a nondegenerate triangle (a_i>=-tol). Call with
    actual tangent-plane *leaf centers*. Not a drop-in Stage3 C++ fix.
    """
    q = np.asarray(q, float)
    t = np.asarray(target, float)
    if q.shape != (3, 2) or t.shape != (2,) or not np.isfinite(q).all():
        raise ValueError("bad geometry")
    V = np.stack([np.ones(3), q[:, 0], q[:, 1]])
    if np.linalg.cond(V) > 1/tol:
        raise ValueError("degenerate triangle")
    a = np.linalg.solve(V, [1.0, t[0], t[1]])
    if np.min(a) < -tol:
        raise ValueError("target outside supported triangle")
    return a


class AnalyticChecks(unittest.TestCase):
    def test_three_frame_log_gain_and_covariance(self):
        truth = np.array([0.2, -0.1, -0.1])
        edges = [(0, 1, 4.0, truth[0]-truth[1]),
                 (1, 2, 1.0, truth[1]-truth[2]),
                 (2, 0, 9.0, truth[2]-truth[0])]
        eta, cov, cc, obj = relative_log_gain_fit(3, edges)
        np.testing.assert_allclose(eta, truth, atol=1e-13)
        self.assertAlmostEqual(obj, 0.0, places=13)
        self.assertEqual(len(set(cc)), 1)
        # Var(eta_i-eta_j)=e_ij^T L^+ e_ij: effective resistance.
        contrast = np.array([1., -1., 0.])
        self.assertGreater(float(contrast @ cov @ contrast), 0)
        np.testing.assert_allclose(cov @ np.ones(3), np.zeros(3), atol=1e-13)

    def test_disconnected_gauges(self):
        eta, cov, cc, obj = relative_log_gain_fit(
            5, [(0,1,2.,0.3), (1,2,2.,0.1), (3,4,1.,-0.2)])
        self.assertEqual(len(set(cc)), 2)
        for members in ([0,1,2], [3,4]):
            self.assertAlmostEqual(sum(eta[i] for i in members), 0, places=12)
        self.assertAlmostEqual(obj, 0.0, places=12)
        self.assertEqual(cov[0,4], 0.)

    def test_triangle_affine_and_rectangle_shortcut_failure(self):
        corners = np.array([[-1.,-1.], [1.,-1.], [-2.,1.], [1.,1.]])
        naive = np.array([0.25]*4)
        self.assertAlmostEqual(float(naive @ corners[:,0]), -0.25)
        triangle = corners[[0,1,3]]
        a = affine_barycentric(triangle)
        self.assertAlmostEqual(float(a @ triangle[:,0]), 0.0)
        self.assertAlmostEqual(float(a @ triangle[:,1]), 0.0)
        self.assertAlmostEqual(sum(a), 1.0)

    def test_extrapolation_and_degeneracy_declared(self):
        with self.assertRaisesRegex(ValueError, "outside"):
            affine_barycentric([[1,1],[2,1],[1,2]])
        with self.assertRaisesRegex(ValueError, "degenerate"):
            affine_barycentric([[-1,-1],[0,0],[1,1]])


if __name__ == "__main__":
    unittest.main(verbosity=2)
