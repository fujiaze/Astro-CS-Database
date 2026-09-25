"""AUD-201 R3: independent numeric verification of the two robust constants that
docs/science/PHOTOMETRY.md freezes -- MAD->sigma and the Tukey biweight c=4.685.

Why derived here rather than trusted: standards/02 sec.1 requires an A-grade source
whose domain matches the claim. The repo cites Beaton & Tukey 1974 / Mosteller &
Tukey 1977 for "c=4.685 => 95% asymptotic Gaussian efficiency". I could not retrieve
those papers on this node, so the claim is re-derived numerically from the definition
of the M-estimator's asymptotic variance. If the derivation reproduces 0.95 the
constant is self-certified; if not, the doc's justification is wrong.

Score implied by star_matcher.cpp:559-570 (w=(1-u^2)^2, u=(r-loc)/(c*S)):
    psi(x) = x*(1-(x/c)^2)^2   for |x| < c,   else 0        (sigma = 1)
    psi'(x) = (1-t^2)*(1-5t^2),  t = x/c                    (derived by hand,
                                                             verified numerically)
Asymptotic variance relative to the MLE:
    A(psi, Phi) = int psi^2 phi dx / ( int psi' phi dx )^2 ,  efficiency = 1/A
    (A = 1 exactly for the MLE score psi = x -> used as the internal consistency check)

Usage: python -B aud201_r3_robust_constants.py
"""
import io
import math
import os

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "aud201_r3_robust_constants.out")

N = 2_000_001
LO, HI = -12.0, 12.0
X = np.linspace(LO, HI, N)
PHI = np.exp(-0.5 * X * X) / math.sqrt(2.0 * math.pi)
H = (HI - LO) / (N - 1)


def trap(y):
    return float((y.sum() - 0.5 * (y[0] + y[-1])) * H)


def psi_fn(c, x):
    t = np.abs(x) / c
    w = np.where(t < 1.0, x * (1.0 - t * t) ** 2, 0.0)
    return w


def dpsi_fn(c, x):
    t = x / c
    return np.where(np.abs(t) < 1.0, (1.0 - t * t) * (1.0 - 5.0 * t * t), 0.0)


def efficiency(c):
    p = psi_fn(c, X)
    d = dpsi_fn(c, X)
    num = trap(p * p * PHI)
    den = trap(d * PHI)
    return 1.0 / (num / (den * den)), num, den


def main():
    L = []
    A = L.append

    A("=== 1. MAD -> sigma identity  (PHOTOMETRY.md:29-30, star_matcher.cpp:21) ===")
    lo, hi = 0.0, 2.0
    for _ in range(200):                       # solve 2*Phi(m)-1 = 0.5
        m = 0.5 * (lo + hi)
        v = math.erf(m / math.sqrt(2.0))
        if v < 0.5:
            lo = m
        else:
            hi = m
    m75 = 0.5 * (lo + hi)
    doc = 0.6744897501960817
    A("  Phi^-1(0.75) solved on erf            : %.17f" % m75)
    A("  _MAD_SCALE as written in code         : %.17f" % doc)
    A("  relative difference                   : %+.3e  (1-ulp level -> identity CONFIRMED)"
      % ((doc - m75) / m75))
    A("  1/_MAD_SCALE                          : %.16f" % (1.0 / doc))
    A("  doc alternative form 1.482602218505602: diff = %+.1e" % (1.0 / doc - 1.482602218505602))
    A("  4-digit 0.6745 vs full, relative      : %+.7e   (doc:296 claims +1.5196e-05)"
      % ((0.6745 - doc) / doc))
    A("")

    A("=== 2. internal consistency of the efficiency machinery ===")
    e_mle, num_mle, den_mle = None, None, None
    p = X.copy()
    num_mle = trap(p * p * PHI)
    den_mle = trap(np.ones_like(X) * PHI)
    A("  MLE score psi=x : int psi^2 phi = %.12f (expect 1), int psi' phi = %.12f (expect 1)"
      % (num_mle, den_mle))
    A("  => A(MLE) = %.12f, efficiency = %.12f  (must be 1.000000)"
      % (num_mle / den_mle ** 2, den_mle ** 2 / num_mle))
    A("")

    A("=== 3. psi' analytic vs numerical derivative ===")
    c0 = 4.685
    eps = 1e-6
    sub = X[::2000]
    num_d = (psi_fn(c0, sub + eps) - psi_fn(c0, sub - eps)) / (2.0 * eps)
    ana_d = dpsi_fn(c0, sub)
    err = float(np.max(np.abs(num_d - ana_d)))
    A("  max |analytic psi' - central difference| = %.3e  (must be < 1e-6)" % err)
    A("")

    A("=== 4. Tukey biweight c = 4.685 -> claimed 95% Gaussian efficiency ===")
    A("  %-8s %-12s %-12s" % ("c", "efficiency", "A"))
    for c in (2.0, 3.0, 4.0, 4.5, 4.685, 5.0, 6.0, 8.0, 12.0):
        e, num, den = efficiency(c)
        A("  %-8.3f %-12.5f %-12.5f" % (c, e, 1.0 / e))
    e4685, _, _ = efficiency(4.685)
    A("  c=4.685 -> efficiency = %.5f   (doc claim: 0.95)" % e4685)
    lo, hi = 1.0, 12.0
    for _ in range(45):
        mid = 0.5 * (lo + hi)
        # efficiency is monotonically increasing in c (verified just below), so
        # eff(mid) > 0.95 means the solution lies at a smaller c.
        if efficiency(mid)[0] > 0.95:
            hi = mid
        else:
            lo = mid
    c95 = 0.5 * (lo + hi)
    A("  c solving efficiency = 0.95000        = %.4f" % c95)
    A("  monotone check: efficiency increases with c -> %s"
      % ("YES" if all(efficiency(c)[0] <= efficiency(c + 0.5)[0] + 1e-9
         for c in (2.0, 3.0, 4.0, 5.0, 6.0, 8.0)) else "NO"))
    A("  => claim 'c=4.685 yields 95%% asymptotic efficiency at the Gaussian' is %s"
      % ("REPRODUCED" if abs(e4685 - 0.95) < 0.005 else "NOT REPRODUCED"))
    A("")

    A("=== 5. consequence for sigma_residual taken from Tukey INLIERS only ===")
    tail = math.erfc(4.685 / math.sqrt(2.0))
    A("  P(|z| > 4.685) = %.3e  => trimming removes ~%.1f ppm of a Gaussian sample."
      % (tail, tail * 1e6))
    A("  So MAD(r_inliers) is unbiased to leading order vs MAD(all r): the")
    A("  'truncated-sample bias' worry about PHOTOMETRY.md:213 is NOT a real bias")
    A("  source at c=4.685, and sigma_residual = MAD(r_inliers)/Phi^-1(3/4) is a")
    A("  consistent sigma estimator under the Gaussian model.")
    text = "\n".join(L)
    with io.open(OUT, "w", encoding="utf-8") as f:
        f.write(text + "\n")
    print(text.encode("ascii", "backslashreplace").decode("ascii"))


if __name__ == "__main__":
    main()
