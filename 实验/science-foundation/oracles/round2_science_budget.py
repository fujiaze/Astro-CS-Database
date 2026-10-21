"""Exact-rational area -> science budget oracle, independent of ACSD production.

Run: python round2_science_budget.py
Only Python's standard library is used.  Fractions are exact for supplied decimal
area bounds. They do not certify the WCS solver which supplied those bounds.
The synthetic ledger measures area in sr/(pi/3); multiplying every area and native
integral by pi/3 gives a physical-sr experiment with identical surface brightness.
There is no sampling or stochastic coverage claim in these experiments.
"""
from __future__ import annotations

from dataclasses import dataclass
from fractions import Fraction as Q
import hashlib
import itertools
import json
import math
import platform
from pathlib import Path
import time


def rat(v):
    # A Python float supplied by an interval solver denotes its exact binary64
    # endpoint; str(float) is a nearby decimal which can shrink a certificate.
    if isinstance(v, float):
        return Q.from_float(v)
    return v if isinstance(v, Q) else Q(str(v))


@dataclass(frozen=True)
class Interval:
    lo: Q
    hi: Q

    def __post_init__(self):
        object.__setattr__(self, "lo", rat(self.lo))
        object.__setattr__(self, "hi", rat(self.hi))
        if self.lo > self.hi:
            raise ValueError("inverted interval")

    @property
    def mid(self):
        return (self.lo+self.hi)/2

    @property
    def radius(self):
        return (self.hi-self.lo)/2

    def contains(self, v):
        return self.lo <= v <= self.hi

    def __add__(self, other):
        other = as_interval(other)
        return Interval(self.lo+other.lo, self.hi+other.hi)

    def __mul__(self, other):
        other = as_interval(other)
        vs = [a*b for a in (self.lo, self.hi) for b in (other.lo, other.hi)]
        return Interval(min(vs), max(vs))

    def __truediv__(self, other):
        other = as_interval(other)
        if other.lo <= 0 <= other.hi:
            raise ZeroDivisionError("denominator interval includes zero")
        return self * Interval(1/other.hi, 1/other.lo)

    def dump(self):
        lower, upper = float_lower(self.lo), float_upper(self.hi)
        # Width also bounds the serialized enclosing endpoints, not merely the
        # narrower exact-rational interval retained in the exact field.
        width = float_upper(Q.from_float(upper)-Q.from_float(lower))
        return {"lower": lower, "upper": upper,
                "width": width,
                "exact": [str(self.lo), str(self.hi)]}


def float_lower(value):
    out = float(value)
    return math.nextafter(out, -math.inf) if Q.from_float(out) > value else out


def float_upper(value):
    out = float(value)
    return math.nextafter(out, math.inf) if Q.from_float(out) < value else out


def as_interval(v):
    return v if isinstance(v, Interval) else Interval(v, v)


def isum(xs):
    out = Interval(0, 0)
    for x in xs:
        out = out+x
    return out


def weights(overlap, drop):
    """Containment enclosure for a/A; a <= A is an exact geometric constraint."""
    if drop.lo <= 0 or overlap.lo < 0 or overlap.lo > drop.hi:
        raise ValueError("invalid or unresolved drop area")
    return Interval(overlap.lo/drop.hi, min(Q(1), overlap.hi/drop.lo))


def fractional_box_extreme(w, omega, x, maximize):
    """Exact extremum of sum(w*x)/sum(w*omega) on a positive-denominator box.

    Dinkelbach's finite-vertex method. At trial q, optimize separately each
    w_i*(x_i-q*omega_i).  Its maximum uses omega.lo for q>=0, omega.hi
    for q<0; minimum uses the opposite. The w endpoint is then selected
    by the residual sign.  Stop iff the exact residual is zero.
    This optimizes a containing box; shared primitive constraints can tighten it.
    """
    nlo = sum((wi.lo*oi.lo for wi, oi in zip(w, omega)), Q(0))
    if nlo <= 0:
        raise ValueError("strictly positive denominator lower bound required")
    q = Q(0)
    for iteration in range(1, 10001):
        num = den = Q(0)
        for wi, oi, xi in zip(w, omega, x):
            # For q=0 either omega endpoint maximizes the objective; choose lo.
            if q == 0:
                om = oi.lo
            else:
                om = (oi.lo if (q > 0) == maximize else oi.hi)
            r = xi-q*om
            use_upper = (r >= 0) == maximize
            ww = wi.hi if use_upper else wi.lo
            num += ww*xi
            den += ww*om
        residual = num-q*den
        if residual == 0:
            return q, iteration
        q = num/den
    raise ArithmeticError("fractional optimizer work exhausted")


def ratio_box(w, omega, x):
    lo, il = fractional_box_extreme(w, omega, x, False)
    hi, ih = fractional_box_extreme(w, omega, x, True)
    return Interval(lo, hi), il+ih


def evaluate_integrated(overlap, drop, omega, x, *, weak_normalization=0,
                        absolute_tolerance=None):
    """x is an exact signed native integral. Area bounds use one common unit.

    Return F allocation, N normalization, B interval, sparse R coefficient bounds,
    and a deterministic residual error bound around the central feasible box.
    A scientific tolerance is absolute on B, not relative to a zero signal.
    """
    x = [rat(v) for v in x]
    if not (len(overlap) == len(drop) == len(omega) == len(x)):
        raise ValueError("shape mismatch")
    if not x:
        return {"weights": [], "allocated_flux": Interval(0, 0),
                "normalization": Interval(0, 0), "support": "no_support",
                "optimizer_iterations": 0, "surface": None, "r": None,
                "estimate": None, "science_status": "unavailable_normalization"}
    if any(oi.lo <= 0 for oi in omega):
        raise ValueError("native source area must have positive lower bound")
    w = [weights(ai, di) for ai, di in zip(overlap, drop)]
    f = isum(wi*xi for wi, xi in zip(w, x))
    n = isum(wi*oi for wi, oi in zip(w, omega))
    result = {"weights": w, "allocated_flux": f, "normalization": n,
              "support": "no_support" if n.hi == 0 else
                         "support_unresolved" if n.lo == 0 else
                         "weak_support" if n.lo < rat(weak_normalization) else
                         "positive_support", "optimizer_iterations": 0}
    if n.lo <= 0:
        # Valid conditional hull, not a published normalized signal: the true
        # denominator can be zero, so no value is assigned to B or R here.
        hull = [as_interval(xi)/oi for xi, oi in zip(x, omega)]
        result.update(surface=None, r=None, estimate=None,
                      conditional_positive_support_hull=Interval(
                          min(h.lo for h in hull), max(h.hi for h in hull)),
                      science_status="unavailable_normalization")
        return result
    b, work = ratio_box(w, omega, x)
    w0 = [wi.mid for wi in w]
    o0 = [oi.mid for oi in omega]
    n0 = sum((ww*oo for ww, oo in zip(w0, o0)), Q(0))
    b0 = sum((ww*xi for ww, xi in zip(w0, x)), Q(0))/n0
    # Exact identity B-B0 = [sum dw*(x-B0*Omega0)
    #                       -B0*sum w*dOmega]/N.
    residual = sum((wi.radius*abs(xi-b0*oo)+abs(b0)*wi.hi*oi.radius
                    for wi, oi, xi, oo in zip(w, omega, x, o0)), Q(0))/n.lo
    rr = []
    for j in range(len(w)):
        unit = [Q(int(i == j)) for i in range(len(w))]
        ri, rw = ratio_box(w, omega, unit)
        rr.append(ri)
        work += rw
    actual_radius = max(b0-b.lo, b.hi-b0)
    if actual_radius > residual:
        raise AssertionError("residual theorem violated")
    target = None if absolute_tolerance is None else rat(absolute_tolerance)
    result.update(surface=b, estimate=b0, r=rr,
                  r_estimate=[ww/n0 for ww in w0],
                  residual_error_bound=residual,
                  exact_box_error_bound=actual_radius,
                  optimizer_iterations=work,
                  science_status="not_requested" if target is None else
                     "science_bounded" if actual_radius <= target else
                     "science_unconverged")
    return result


def evaluate_brightness(overlap, drop, omega, brightness):
    """Declared identity x_j=b_j*Omega_j, unlike uncertain-Omega exact x input."""
    c = [weights(ai, di)*oi for ai, di, oi in zip(overlap, drop, omega)]
    return ratio_box(c, [Interval(1, 1)]*len(c), [rat(b) for b in brightness])[0]


def overlap_radius_budget(x, omega, drop, overlap_center, tolerance, allocation=None):
    """Sufficient absolute a_j budget, with A/Omega exact and box centered at a0.

    sum lambda <1; each eps_a_j = lambda_j*tol*N0*A_j /
    (abs(x_j-B0*Omega_j)+tol*Omega_j).  This simultaneously controls
    denominator reduction and numerator residual, including signed x.
    For uncertain A and Omega use the full residual budget in the document.
    """
    x, o, d, a = [list(map(rat, vv)) for vv in (x, omega, drop, overlap_center)]
    n = len(x)
    if allocation is None:
        allocation = [Q(1, 2*n)]*n
    allocation = list(map(rat, allocation))
    if min(allocation) < 0 or sum(allocation) >= 1:
        raise ValueError("invalid error allocation")
    eps = rat(tolerance)
    ww = [aa/dd for aa, dd in zip(a, d)]
    n0 = dot(ww, o)
    b0 = dot(ww, x)/n0
    return [ll*eps*n0*dd/(abs(xx-b0*oo)+eps*oo)
            for xx, oo, dd, ll in zip(x, o, d, allocation)]


def covariance_entry_error(ep, eq, rp, rq, covariance):
    """Absolute error bound for r_p C r_q^T, with one fixed known covariance."""
    cp = matvec(covariance, rp)
    cq = matvec(covariance, rq)
    return (dot(ep, [abs(v) for v in cq]) + dot(eq, [abs(v) for v in cp])
            + dot(ep, matvec([[abs(v) for v in row] for row in covariance], eq)))


def dot(a, b):
    return sum((x*y for x, y in zip(a, b)), Q(0))


def matvec(a, b):
    return [dot(row, b) for row in a]


def transpose(a):
    return list(map(list, zip(*a)))


def inverse(a):
    """Small exact-rational Gauss-Jordan oracle, not the production solver."""
    n = len(a)
    b = [list(row)+[Q(int(i == j)) for j in range(n)] for i, row in enumerate(a)]
    for j in range(n):
        pivot = next((i for i in range(j, n) if b[i][j]), None)
        if pivot is None:
            raise ValueError("singular covariance")
        b[j], b[pivot] = b[pivot], b[j]
        scale = b[j][j]
        b[j] = [v/scale for v in b[j]]
        for i in range(n):
            if i != j:
                fac = b[i][j]
                b[i] = [v-fac*w for v, w in zip(b[i], b[j])]
    return [row[n:] for row in b]


def blue_geometry_enclosure(gamma0, gamma_error):
    """Conservative infinity-norm perturbation enclosure for BLUE weights.

    rho=||abs(Gamma0^-1) E_Gamma||_inf < 1 guarantees every enclosed
    covariance is nonsingular. Variance 1/s and weights q/s need s lower>0.
    This can explicitly fail even for a valid central covariance.
    """
    g = inverse(gamma0)
    n = len(g)
    q0 = matvec(g, [Q(1)]*n)
    rho = max(sum((abs(g[i][k])*gamma_error[k][j]
                   for k in range(n) for j in range(n)), Q(0)) for i in range(n))
    if rho >= 1:
        return {"status": "geometry_solve_unresolved", "rho": rho}
    dq = rho*max(map(abs, q0))/(1-rho)
    qi = [Interval(v-dq, v+dq) for v in q0]
    si = isum(qi)
    if si.lo <= 0:
        return {"status": "normalization_unresolved", "rho": rho}
    return {"status": "bounded", "rho": rho,
            "weights": [v/si for v in qi], "variance": as_interval(1)/si}


def covariance(r, c):
    return [[dot(p, matvec(c, q)) for q in r] for p in r]


def symmetric_bound(v, eps, nonnegative=True):
    v, eps = rat(v), rat(eps)
    return Interval(max(Q(0), v-eps) if nonnegative else v-eps, v+eps)


def exhaustive_vertex_check():
    """Independent enumeration, not the residual-root implementation."""
    tests = [(["-7", "2", "11"], ["0", ".2", ".1"]),
             (["-11", "-3", "-1"], [".02", ".2", ".4"]),
             (["0", "0", "0"], [".02", ".2", ".4"])]
    reports = []
    for x, wl in tests:
        x = list(map(rat, x))
        ws = [Interval(l, rat(l)+Q(1, 10)) for l in wl]
        os = [Interval(".7", "1.2"), Interval("1.1", "1.3"),
              Interval(".9", "1.9")]
        truth = []
        for bits in itertools.product((0, 1), repeat=6):
            ww = [v.hi if bit else v.lo for v, bit in zip(ws, bits[:3])]
            oo = [v.hi if bit else v.lo for v, bit in zip(os, bits[3:])]
            truth.append(dot(ww, x)/dot(ww, oo))
        got, work = ratio_box(ws, os, x)
        assert got == Interval(min(truth), max(truth))
        reports.append({"x": list(map(str, x)), "vertices": len(truth),
                        "exact_extrema": got.dump(), "iterations": work})
    return reports


def scenario(name, brightness, overlap, omega, drop, tolerance,
             initial_relative=".02", weak=0, omega_exact=False, max_levels=24):
    """Known rational area ledger, progressive supplied-area refinement.

    This measures propagation/refinement demand, not WCS integration runtime.
    Each refinement decreases area radii by four; true values are off-center
    by 1/4 radius to produce a nonzero deterministic numerical error.
    """
    start = time.perf_counter()
    b, a, o, d = [list(map(rat, vs)) for vs in (brightness, overlap, omega, drop)]
    x = [bi*oi for bi, oi in zip(b, o)]
    wt = [aa/dd for aa, dd in zip(a, d)]
    nt = dot(wt, o)
    ft = dot(wt, x)
    truth = None if nt == 0 else dot(wt, x)/nt
    history = []
    for level in range(max_levels+1):
        rel = rat(initial_relative)/4**level
        # Conservative error ledgers include explicit positive uncertainty on
        # a true zero, unless the input overlap is proved zero separately.
        def box(vals, is_overlap=False, exact=False):
            out = []
            for v, scale in zip(vals, d):
                radius = Q(0) if exact else rel*scale
                mid = v+radius/4
                out.append(Interval(max(Q(0), mid-radius), mid+radius))
            return out
        aa = box(a, True)
        dd = box(d)
        oo = box(o, exact=omega_exact)
        result = evaluate_integrated(aa, dd, oo, x, weak_normalization=weak,
                                     absolute_tolerance=tolerance)
        rec = {"level": level, "area_radius_relative_to_drop": float(rel),
               "support": result["support"], "science_status": result["science_status"],
               "normalization": result["normalization"].dump(),
               "allocated_flux": result["allocated_flux"].dump(),
               "allocated_flux_true": float(ft),
               "allocated_flux_mid_error": float(abs(
                   dot([v.mid for v in result["weights"]], x)-ft)),
               "optimizer_iterations": result["optimizer_iterations"]}
        assert result["allocated_flux"].contains(ft)
        assert result["normalization"].contains(nt)
        if result["surface"] is not None:
            assert result["surface"].contains(truth)
            rec.update(surface=result["surface"].dump(),
                       estimate=float(result["estimate"]),
                       actual_error=float(abs(result["estimate"]-truth)),
                       predicted_error_bound=float(result["exact_box_error_bound"]),
                       residual_error_bound=float(result["residual_error_bound"]))
        history.append(rec)
        if result["science_status"] == "science_bounded":
            break
    return {"name": name, "brightness": list(map(str, b)),
            "true_overlap_chart_area": list(map(str, a)),
            "true_native_chart_area": list(map(str, o)),
            "true_drop_chart_area": list(map(str, d)),
            "true_surface": None if truth is None else float(truth),
            "science_absolute_tolerance": float(rat(tolerance)),
            "omega_exact": omega_exact, "history": history,
            "elapsed_seconds": time.perf_counter()-start}


def covariance_experiment(radius_scale=1):
    radius_scale = rat(radius_scale)
    o = list(map(rat, [".8", "1.3", ".9"]))
    d = list(map(rat, [".4", ".5", ".3"]))
    # Every source column sums exactly to its drop area. Hence allocated flux
    # over all leaves is exact for every legal refinement, not independent errors.
    a = [[rat(v) for v in row] for row in [
        [".1", ".2", ".1"], [".2", ".1", ".15"], [".1", ".2", ".05"]]]
    rtruth, r0, errs = [], [], []
    for row in a:
        wt = [aa/dd for aa, dd in zip(row, d)]
        n = dot(wt, o)
        rtruth.append([ww/n for ww in wt])
        got = evaluate_integrated([symmetric_bound(v, rat("0.0001")*radius_scale) for v in row],
                                  [symmetric_bound(v, rat("0.00002")*radius_scale) for v in d],
                                  [symmetric_bound(v, rat("0.00003")*radius_scale) for v in o],
                                  ["-3", "100", ".0002"])
        r0.append(got["r_estimate"])
        errs.append([max(abs(rr.lo-v), abs(rr.hi-v))
                     for rr, v in zip(got["r"], got["r_estimate"])])
    u = list(map(rat, [".5", "-1", "2"]))
    c = [[u[i]*u[j]+(rat([1, 4, 9][i]) if i == j else Q(0))
          for j in range(3)] for i in range(3)]
    ct, c0 = covariance(rtruth, c), covariance(r0, c)
    checks = []
    error_matrix = [[Q(0)]*3 for _ in range(3)]
    for p in range(3):
        for q in range(3):
            bound = covariance_entry_error(errs[p], errs[q], r0[p], r0[q], c)
            error_matrix[p][q] = bound
            error = abs(ct[p][q]-c0[p][q])
            assert error <= bound
            checks.append({"p": p, "q": q, "true_covariance": float(ct[p][q]),
                           "actual_error": float(error), "bound": float(bound)})
    t = list(map(rat, [1, -1, ".3"]))
    vt, v0 = dot(t, matvec(ct, t)), dot(t, matvec(c0, t))
    back = matvec(transpose(r0), t)
    delta = matvec(transpose(errs), list(map(abs, t)))
    vb = covariance_entry_error(delta, delta, back, back, c)
    assert abs(vt-v0) <= vb
    # Raw allocated flux F=W*x, complete partition, exact column identity.
    assert all(sum((row[j] for row in a), Q(0)) == d[j] for j in range(3))
    xpart = list(map(rat, ["-3", "100", ".0002"]))
    delta = [rat(".0001"), rat("-.0002"), rat(".0001")]
    apert = [[aa+delta[i] for aa in row] for i, row in enumerate(a)]
    fpert = sum((sum((row[j]/d[j]*xpart[j] for j in range(3)), Q(0))
                 for row in apert), Q(0))
    assert fpert == sum(xpart, Q(0))
    blue = blue_geometry_enclosure(c0, error_matrix)
    bt = matvec(inverse(ct), [Q(1)]*3)
    st = sum(bt, Q(0))
    bt = [v/st for v in bt]
    if blue["status"] == "bounded":
        assert all(v.contains(q) for v, q in zip(blue["weights"], bt))
        assert blue["variance"].contains(1/st)
    return {"input_covariance": [[float(v) for v in row] for row in c],
            "entries": checks, "signed_aperture": list(map(float, t)),
            "aperture_true_variance": float(vt), "aperture_mid_variance": float(v0),
            "aperture_actual_error": float(abs(vt-v0)), "aperture_bound": float(vb),
            "allocated_flux_complete_partition_error": 0,
            "partition_perturbed_allocations_sum": float(fpert),
            "naive_independent_output_variance": float(sum(
                (tt*tt*ct[i][i] for i, tt in enumerate(t)), Q(0))),
            "area_radius_scale": float(radius_scale),
            "blue_covariance_geometry_propagation": {
                "status": blue["status"], "rho": float(blue["rho"]),
                "weights_intervals": [v.dump() for v in blue.get("weights", [])],
                "true_weights": list(map(float, bt)),
                "variance_interval": blue["variance"].dump() if "variance" in blue else None,
                "true_variance": float(1/st)}}


def zero_and_sliver_experiment():
    one = Interval(1, 1)
    # A guaranteed ordinary contributor permits scientific bounding of a second
    # unresolved sliver; by itself a [0,epsilon] sliver has no normalized signal.
    reports = []
    for label, sliver, bright, tol in [
        ("proved_disjoint", Interval(0, 0), "1e12", "1e-6"),
        ("proved_touch_zero_area", Interval(0, 0), "1e12", "1e-6"),
        ("tiny_positive_with_lower_bound", Interval("9e-16", "11e-16"), "1", "1e-6"),
        ("tiny_unresolved_science_acceptable", Interval(0, "1e-15"), "1", "1e-6"),
        ("tiny_unresolved_enormous_brightness", Interval(0, "1e-15"), "1e12", "1e-6"),
        ("tiny_bright_refined_but_unresolved", Interval(0, "1e-20"), "1e12", "1e-6")]:
        got = evaluate_integrated([one, sliver], [one, one], [one, one],
                                  [0, bright], absolute_tolerance=tol)
        alone = evaluate_integrated([sliver], [one], [one], [bright],
                                    absolute_tolerance=tol)
        reports.append({"name": label, "sliver_area": sliver.dump(),
                        "brightness": float(rat(bright)), "target": float(rat(tol)),
                        "with_guaranteed_support": got["surface"].dump(),
                        "science_status": got["science_status"],
                        "sliver_support": alone["support"],
                        "sliver_only_status": alone["science_status"],
                        "contact_identity": "analytic boundary only" if "touch" in label else None})
    # Constant brightness is exact only when native area is known, or when the
    # input contract explicitly preserves x=b*Omega as an identity.
    ws = [Interval(".1", ".4"), Interval(".3", ".9")]
    os = [Interval(".5", "1.1"), Interval(".8", "1.7")]
    constant = evaluate_brightness(ws, [one, one], os, ["-8", "-8"])
    assert constant == Interval(-8, -8)
    true_zero = evaluate_integrated([one], [one], [one], [0], absolute_tolerance="1e-6")
    assert true_zero["surface"] == Interval(0, 0)
    return {"cases": reports, "identity_preserved_constant": constant.dump(),
            "true_zero_signal": {"support": true_zero["support"],
                                 "science_status": true_zero["science_status"],
                                 "surface": true_zero["surface"].dump()},
            "note": "zero overlap and boundary-only touch share measure zero; classification needs geometry proof, not absence of samples"}


def planner_experiment():
    reports = []
    o = list(map(rat, [".8", "1.3", ".9"]))
    d = list(map(rat, [".4", ".5", ".3"]))
    a = list(map(rat, [".12", ".23", ".11"]))
    for name, b, target in [("constant", [7, 7, 7], "1e-6"),
                            ("signed", [-8, 0, 5], "1e-5"),
                            ("high_dynamic_range", [0, "1e9", ".0001"], ".01")]:
        x = [rat(bi)*oi for bi, oi in zip(b, o)]
        eps = overlap_radius_budget(x, o, d, a, target)
        got = evaluate_integrated([symmetric_bound(v, e) for v, e in zip(a, eps)],
                                  [as_interval(v) for v in d],
                                  [as_interval(v) for v in o], x,
                                  absolute_tolerance=target)
        assert got["science_status"] == "science_bounded"
        reports.append({"name": name, "tolerance": float(rat(target)),
                        "overlap_absolute_radius": list(map(float, eps)),
                        "overlap_relative_radius": [float(e/v) for e, v in zip(eps, a)],
                        "predicted_surface_error": float(got["exact_box_error_bound"]),
                        "positive_denominator_lower": float(got["normalization"].lo)})
    return reports


def rect_area(rect):
    _, x0, x1, y0, y1 = rect
    return max(Q(0), x1-x0)*max(Q(0), y1-y0)


def rect_overlap(a, b):
    if a[0] != b[0]:
        return Q(0)
    return max(Q(0), min(a[2], b[2])-max(a[1], b[1]))*max(
        Q(0), min(a[4], b[4])-max(a[3], b[3]))


def equal_area_region_experiment():
    """Actual exact chart-rectangle intersections at pole/RA seam/face seam.

    Input regions are synthetic square-chart unions, not TAN detector pixels.
    The integral truth is Fubini's rectangle area times pi/3, independently of
    HEALPix's sky-angle parameterization. No four-corner spherical area is used.
    """
    def rr(face, *coords):
        return (face, *map(rat, coords))
    groups = {
        # Face 4 has phi=pi/4*(s-t), so these rectangles cross RA=0.
        "ra0_seam": [
            [rr(4, ".4375", ".5625", ".4375", ".5625")],
            [rr(4, ".46875", ".59375", ".4375", ".5625")],
            [rr(4, ".4375", ".5625", ".46875", ".59375")]],
        # s=t=1 is north apex; four adjacent faces meet at exactly one point.
        "north_apex": [
            [rr(0, ".9375", 1, ".9375", 1), rr(1, ".9375", 1, ".9375", 1)],
            [rr(0, ".96875", 1, ".9375", 1), rr(3, ".9375", 1, ".96875", 1)],
            [rr(1, ".9375", 1, ".96875", 1), rr(2, ".9375", 1, ".9375", 1)]],
        # Face0 s=1 and face1 t=1 coincide along phi=pi/2.
        "face0_face1_edge": [
            [rr(0, ".96875", 1, ".375", ".5"), rr(1, ".375", ".5", ".96875", 1)],
            [rr(0, ".9375", 1, ".40625", ".53125"), rr(1, ".40625", ".53125", ".9375", 1)],
            [rr(0, ".96875", 1, ".4375", ".5625"), rr(1, ".4375", ".5625", ".96875", 1)]]}
    out = []
    for name, sources in groups.items():
        started = time.perf_counter()
        o = [sum((rect_area(r) for r in region), Q(0)) for region in sources]
        b = list(map(rat, ["-4", "1e5", ".00002"]))
        x = [bi*oi for bi, oi in zip(b, o)]
        leaves = []
        for face in sorted({r[0] for s in sources for r in s}):
            for ix in range(32):
                for iy in range(32):
                    target = (face, Q(ix, 32), Q(ix+1, 32), Q(iy, 32), Q(iy+1, 32))
                    a = [sum((rect_overlap(r, target) for r in region), Q(0))
                         for region in sources]
                    if not any(a):
                        continue
                    # Propagation tolerance sets the area budget, rather than
                    # selecting a common percent of the native pixel area.
                    target_error = rat(".0001")
                    eps = overlap_radius_budget(x, o, o, a, target_error)
                    ai = [symmetric_bound(v, e) for v, e in zip(a, eps)]
                    got = evaluate_integrated(ai, [as_interval(v) for v in o],
                                              [as_interval(v) for v in o], x,
                                              absolute_tolerance=target_error)
                    wt = [aa/oo for aa, oo in zip(a, o)]
                    truth = dot(wt, x)/dot(wt, o)
                    assert got["surface"].contains(truth)
                    assert got["science_status"] == "science_bounded"
                    leaves.append({"face": face, "ix": ix, "iy": iy,
                                   "exact_chart_overlap": list(map(str, a)),
                                   "true_surface": float(truth),
                                   "actual_error": float(abs(got["estimate"]-truth)),
                                   "predicted_error": float(got["exact_box_error_bound"]),
                                   "interval_width": float(got["surface"].hi-got["surface"].lo),
                                   "target": float(target_error)})
        out.append({"name": name, "nside": 32,
                    "source_regions": [[list(map(str, r)) for r in region] for region in sources],
                    "leaves": leaves, "elapsed_seconds": time.perf_counter()-started})
    return out


def main():
    started = time.perf_counter()
    common = dict(overlap=[".12", ".23", ".11"], omega=[".8", "1.3", ".9"],
                  drop=[".4", ".5", ".3"])
    scenes = [scenario("constant_known_native_area", ["7", "7", "7"],
                       **common, tolerance="1e-12", omega_exact=True),
              scenario("constant_uncertain_native_area", ["7", "7", "7"],
                       **common, tolerance="1e-5"),
              scenario("signed_field", ["-8", "0", "5"], **common, tolerance="1e-5"),
              scenario("bright_star_dark_background", ["0", "1e9", ".0001"],
                       **common, tolerance=".01"),
              scenario("weak_extended_structure", ["1e-6", "3e-6", "-2e-6"],
                       **common, tolerance="1e-10"),
              scenario("steep_fine_structure", ["-1e4", "2e4", "-3e4"],
                       **common, tolerance=".001"),
              scenario("partial_support", ["2", "-3", "1"],
                       overlap=[".0001", ".0003", ".0002"],
                       omega=common["omega"], drop=common["drop"], tolerance="1e-4"),
              scenario("extremely_low_positive_support", ["2", "-3", "1"],
                       overlap=["1e-15", "3e-15", "2e-15"],
                       omega=common["omega"], drop=common["drop"], tolerance="1e-4",
                       initial_relative="1e-14", weak="1e-10")]
    output = {"source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              "environment": {"python": platform.python_version(), "platform": platform.platform()},
              "run_command": "python 实验/science-foundation/oracles/round2_science_budget.py",
              "random_seed": None,
              "area_unit": "chart area = sr/(pi/3), common conversion cancels in B",
              "integrated_signal_unit": "native integral divided by pi/3; B unchanged",
              "scope": "exact rational propagation of supplied bounds; not a WCS containment certificate",
              "fractional_optimizer_vertex_checks": exhaustive_vertex_check(),
              "scenarios": scenes, "zero_sliver": zero_and_sliver_experiment(),
              "science_budget_planner": planner_experiment(),
              "equal_area_sky_regions": equal_area_region_experiment(),
              "correlated_geometry_and_noise_attempts": [covariance_experiment(scale)
                  for scale in ("1", ".01", ".0001")],
              "elapsed_seconds": time.perf_counter()-started}
    assert all(s["history"][-1]["science_status"] == "science_bounded" for s in scenes)
    destination = Path(__file__).resolve().parents[1]/"evidence"/"round2_science_budget.json"
    destination.write_text(json.dumps(output, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"evidence": str(destination), "seconds": output["elapsed_seconds"],
                      "scenarios": len(scenes)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
