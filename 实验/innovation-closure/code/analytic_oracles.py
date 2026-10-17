#!/usr/bin/env python3
"""P1-P5 dependency-free mathematical oracles; not production C++ or FITS E2E.

Run: python3 实验/innovation-closure/code/analytic_oracles.py
No real data, no random draws, no libraries except Python standard library.
These finite examples test *counterexamples and algebraic prerequisites*,
not actual algorithm convergence, P4 coverage, or real HEALPix geometry.
"""
import math
import unittest


class TheoryOracles(unittest.TestCase):
    def test_p1_spatial_response_spectral_bias(self):
        reference_sed = (1.0, 1.0)
        source_sed = (2.0, 1.0)
        ref_response = (1.0, 1.0)
        instr = ((2.0, 0.0), (0.0, 2.0))
        weights = (0.8, 0.2)
        scale = [sum(a*b for a,b in zip(reference_sed, ref_response)) /
                 sum(a*b for a,b in zip(reference_sed, r)) for r in instr]
        effective = [sum(weights[k]*scale[k]*instr[k][i]
                         for k in range(len(instr))) for i in range(2)]
        self.assertAlmostEqual(sum(a*b for a,b in zip(reference_sed, effective)), 2.0)
        delta = sum((source_sed[i]-reference_sed[i]) *
                    (effective[i]-ref_response[i]) for i in range(2))
        self.assertAlmostEqual(delta, 0.6)
        self.assertAlmostEqual(sum(a*b for a,b in zip(source_sed, effective)) -
                               sum(a*b for a,b in zip(source_sed, ref_response)), delta)

    def test_p2_point_source_information_not_surface_ivar(self):
        variances = (1.0, 1.0)
        point_info = (1.0, 0.5)
        a = [u / sum(point_info) for u in point_info]
        actual = sum(a[i]*a[i]*variances[i] for i in range(2))
        optimum = 1 / sum(1 / v for v in variances)
        self.assertAlmostEqual(actual, 5.0 / 9.0)
        self.assertAlmostEqual(optimum, 0.5)
        self.assertGreater(actual, optimum)

    def test_p2_drizzle_information_projection(self):
        h = (1.0, 2.0)
        # R=(1,1), C=Identity. Scalar projected noise Var=2.
        before = sum(x*x for x in h)
        after = (sum(h)**2) / 2
        self.assertAlmostEqual(before, 5.0)
        self.assertAlmostEqual(after, 4.5)
        self.assertLessEqual(after, before)

    def test_p3_set_overlap_bound_illustration_only(self):
        # Equal-area microcells are an *abstract set* illustration, NOT an
        # independent real HEALPix boundary oracle.
        D = {0, 1, 2, 3}
        Dtilde = {0, 1, 2, 4}
        H = {0, 1, 5}
        Htilde = {0, 5, 6}
        left = abs(len(D & H) - len(Dtilde & Htilde))
        right = len(D ^ Dtilde) + len(H ^ Htilde)
        self.assertLessEqual(left, right)

    def test_p4_taylor_error_and_minimax_condition(self):
        # log variance u(x)=0.1*x^2: second derivative M=0.2.
        u0, u2 = 0.0, 0.4
        interpolated, truth = (u0 + u2) / 2, 0.1
        M = 0.2
        bound = (M / 2) * (0.5*1.0**2 + 0.5*1.0**2)
        self.assertAlmostEqual(abs(interpolated - truth), bound)
        # Independent upper variances yield exact minimax coefficients.
        U = (1.0, 4.0)
        inv_sum = sum(1.0/v for v in U)
        a = [1.0/v / inv_sum for v in U]
        worst_independent = sum(a[i]**2*U[i] for i in range(2))
        worst_perfectly_correlated = sum(a[i]*math.sqrt(U[i]) for i in range(2))**2
        self.assertAlmostEqual(worst_independent, 0.8)
        self.assertAlmostEqual(worst_perfectly_correlated, 1.44)
        self.assertGreater(worst_perfectly_correlated, min(U))

    def test_p5_weighted_common_mode_annihilation(self):
        g = (1.0, 2.0)
        W = (1.0, 0.25)
        denom = sum(W[i]*g[i]*g[i] for i in range(2))
        Q = [[float(i == j)-g[i]*W[j]*g[j]/denom
              for j in range(2)] for i in range(2)]
        for i in range(2):
            self.assertAlmostEqual(sum(Q[i][j]*g[j] for j in range(2)), 0.0)
        self.assertAlmostEqual(W[0]*Q[0][1], W[1]*Q[1][0])

    def test_p5_floor_bin_not_bilinear_least_squares(self):
        x = (0.25, 0.75)
        target = ((0.5, 1.5), (-0.5, -1.5))
        binned = ((1.0, 1.0), (-1.0, -1.0))
        exact = ((2*x[0], 2*x[1]), (-2*x[0], -2*x[1]))
        loss = lambda pred: sum((pred[k][t]-target[k][t])**2
                                for k in range(2) for t in range(2))
        self.assertAlmostEqual(loss(binned), 1.0)
        self.assertAlmostEqual(loss(exact), 0.0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
