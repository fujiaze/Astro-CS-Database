"""Arbitrary-precision affine-strip fallback for a focused true HPX sliver.

Same mean-value strip algorithm as the independent C++ microprototype, with
mpmath.iv endpoints retained as exact Fractions through polygon clipping,
shoelace moments, error ledgers and stopping. No endpoint is rounded to double
for a scientific decision. Scope is deliberately explicit: ICRS TAN centered
at (45 deg,0), diagonal equal CD, optional SIP shear A_0_2=q. Nside=1 leaf0
near its lower vertex. Other leaf constraints are certified inactive below.
The algorithm does not use the analytic answer for geometric evaluation.
"""
from __future__ import annotations
from dataclasses import dataclass
import hashlib
import heapq
import json
from pathlib import Path
import platform
import time
from mpmath import iv, mp
from round2_science_budget import Q, Interval, rat
from round2_cpp_science_bridge import endpoint_q
from round2_affine_independent_review import fixture
from round2_sliver import analytic_new


def iconst(v):
    v = rat(v)
    return iv.mpf(v.numerator)/iv.mpf(v.denominator)


def enclosure(v):
    return Interval(endpoint_q(v._mpi_[0]), endpoint_q(v._mpi_[1]))


@dataclass
class Dual:
    v: object
    x: object = 0
    y: object = 0

    def __add__(self, b):
        b = dual(b)
        return Dual(self.v+b.v, self.x+b.x, self.y+b.y)

    __radd__ = __add__

    def __neg__(self):
        return Dual(-self.v, -self.x, -self.y)

    def __sub__(self, b):
        return self+-dual(b)

    def __rsub__(self, b):
        return dual(b)+-self

    def __mul__(self, b):
        b = dual(b)
        return Dual(self.v*b.v, self.x*b.v+self.v*b.x,
                    self.y*b.v+self.v*b.y)

    __rmul__ = __mul__

    def __truediv__(self, b):
        b = dual(b)
        return Dual(self.v/b.v, (self.x*b.v-self.v*b.x)/(b.v**2),
                    (self.y*b.v-self.v*b.y)/(b.v**2))

    def __rtruediv__(self, b):
        return dual(b)/self

    def square(self):
        return Dual(self.v**2, 2*self.v*self.x, 2*self.v*self.y)

    def sqrt(self):
        s = iv.sqrt(self.v)
        return Dual(s, self.x/(2*s), self.y/(2*s))

    def atan(self):
        # atan2 with x=1 provides the monotone interval atan operation.
        return Dual(iv.atan2(self.v, 1), self.x/(1+self.v**2),
                    self.y/(1+self.v**2))


def dual(v):
    return v if isinstance(v, Dual) else Dual(v)


def interval(lo, hi):
    ll, hh = iconst(lo), iconst(hi)
    return iv.mpf([ll.a, hh.b])


@dataclass
class Model:
    header: dict

    def __post_init__(self):
        h = self.header
        if (h['CRVAL1'] != 45 or h['CRVAL2'] != 0 or h['CRPIX1'] != 1
                or h['CD1_2'] != 0 or h['CD2_1'] != 0 or h['CD1_1'] != h['CD2_2']):
            raise ValueError("unsupported research geometry")
        self.k = iconst(h['CD1_1'])*iv.pi/180
        self.shift = Q(1)-rat(h['CRPIX2'])
        self.shear = iconst(h.get('A_0_2', 0))

    def evaluate(self, r):
        x0, x1, y0, y1 = r
        u = Dual(interval(x0, x1), iv.mpf(1), iv.mpf(0))
        v = Dual(interval(y0+self.shift, y1+self.shift), iv.mpf(0), iv.mpf(1))
        xi = dual(self.k)*(u+dual(self.shear)*v.square())
        eta = dual(self.k)*v
        length = (1+xi.square()+eta.square()).sqrt()
        z = eta/length
        theta = xi.atan()
        fminus = dual(2/iv.pi)*theta-dual(iconst('.75'))*z
        fplus = dual(2/iv.pi)*theta+dual(iconst('.75'))*z
        g = dual(self.k*self.k)/(length*length*length)
        return fminus, fplus, g, z, theta


@dataclass
class Affine:
    c: Q
    x: Q
    y: Q
    error: Q


def strip(point, whole, r):
    x0, x1, y0, y1 = r
    c, gx, gy = [enclosure(v).mid for v in (point.v, point.x, point.y)]
    wx, wy = enclosure(whole.x), enclosure(whole.y)
    err = enclosure(point.v).radius
    err += max(abs(wx.lo-gx), abs(wx.hi-gx))*(x1-x0)/2
    err += max(abs(wy.lo-gy), abs(wy.hi-gy))*(y1-y0)/2
    return Affine(c, gx, gy, err)


def polygon(r):
    x0, x1, y0, y1 = r
    return [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]


def clip(poly, a, b, c):
    """Exact rational a*x+b*y+c>=0 half-plane clipping."""
    result = []
    for p, q in zip(poly, poly[1:]+poly[:1]):
        fp, fq = a*p[0]+b*p[1]+c, a*q[0]+b*q[1]+c
        inside_p, inside_q = fp >= 0, fq >= 0
        if inside_p:
            result.append(p)
        if inside_p != inside_q:
            t = fp/(fp-fq)
            result.append((p[0]+t*(q[0]-p[0]), p[1]+t*(q[1]-p[1])))
    return result


def constrain(poly, affine, r, sign, inner):
    x0, x1, y0, y1 = r
    cx, cy = (x0+x1)/2, (y0+y1)/2
    a, b = sign*affine.x, sign*affine.y
    c = sign*affine.c-a*cx-b*cy
    c += -affine.error if inner else affine.error
    return clip(poly, a, b, c)


def moments(poly):
    area = mx = my = Q(0)
    for p, q in zip(poly, poly[1:]+poly[:1]):
        det = p[0]*q[1]-q[0]*p[1]
        area += det
        mx += (p[0]+q[0])*det
        my += (p[1]+q[1])*det
    return area/2, mx/6, my/6


def integrate(poly, density, r):
    area, mx, my = moments(poly)
    if not area:
        return Interval(0, 0)
    assert area > 0
    x0, x1, y0, y1 = r
    main = density.c*area+density.x*(mx-(x0+x1)/2*area)
    main += density.y*(my-(y0+y1)/2*area)
    radius = density.error*area
    return Interval(max(Q(0), main-radius), main+radius)


def evaluate(model, r):
    x0, x1, y0, y1 = r
    cx, cy = (x0+x1)/2, (y0+y1)/2
    whole = model.evaluate(r)
    point = model.evaluate((cx, cx, cy, cy))
    fminus, fplus, density = [strip(p, w, r) for p, w in zip(point[:3], whole[:3])]
    inside = outside = polygon(r)
    inside = constrain(inside, fminus, r, -1, True)
    outside = constrain(outside, fminus, r, -1, False)
    inside = constrain(inside, fplus, r, 1, True)
    outside = constrain(outside, fplus, r, 1, False)
    return Interval(integrate(inside, density, r).lo, integrate(outside, density, r).hi)


def total_area(model, subdivisions=16):
    """Independent full-drop density integral; no HEALPix membership involved."""
    lo = hi = Q(0)
    for i in range(subdivisions):
        for j in range(subdivisions):
            x0, x1 = Q(i, subdivisions)-Q(1, 2), Q(i+1, subdivisions)-Q(1, 2)
            y0, y1 = Q(j, subdivisions)-Q(1, 2), Q(j+1, subdivisions)-Q(1, 2)
            r = (x0, x1, y0, y1)
            cx, cy = (x0+x1)/2, (y0+y1)/2
            whole = model.evaluate(r)[2]
            point = model.evaluate((cx, cx, cy, cy))[2]
            answer = integrate(polygon(r), strip(point, whole, r), r)
            lo += answer.lo
            hi += answer.hi
    return Interval(lo, hi)


def admission(model):
    """Other Nside1 leaf0 inequalities are inactive on the entire source.

    Equatorial jp=1+fminus, jm=1+fplus. Leaf0 is jp in [0,1],
    jm in [1,2]. We retain only fminus<=0, fplus>=0 after proving
    fminus>=-1, fplus<=1 and |z|<2/3 on the full source.
    """
    r = (Q(-1, 2), Q(1, 2), Q(-1, 2), Q(1, 2))
    fm, fp, _, z, _ = model.evaluate(r)
    bm, bp, bz = map(enclosure, (fm.v, fp.v, z.v))
    if bm.lo <= -1 or bp.hi >= 1 or bz.lo <= Q(-2, 3) or bz.hi >= Q(2, 3):
        raise ValueError("research chart admission unresolved")
    return {'fminus': bm.dump(), 'fplus': bp.dump(), 'z': bz.dump(),
            'remaining_leaf_inequalities_proved_inactive': True,
            'injectivity': 'u+q*v^2,v shear has global polynomial inverse and determinant exactly1'}


def solve(model, absolute_width, max_nodes=20001, max_depth=40):
    start = time.perf_counter()
    root = (Q(-1, 2), Q(1, 2), Q(-1, 2), Q(1, 2))
    result = evaluate(model, root)
    lo, hi = result.lo, result.hi
    queue = []
    sequence = 0
    heapq.heappush(queue, (-(hi-lo), sequence, root, result, 0))
    nodes, maximum_queue, deepest = 1, 1, 0
    status = 'budget_exhausted'
    while hi-lo > absolute_width:
        if nodes+4 > max_nodes:
            break
        _, _, r, old, depth = heapq.heappop(queue)
        if depth >= max_depth:
            status = 'resolution_exhausted'
            heapq.heappush(queue, (-(old.hi-old.lo), sequence, r, old, depth))
            break
        x0, x1, y0, y1 = r
        cx, cy = (x0+x1)/2, (y0+y1)/2
        lo -= old.lo
        hi -= old.hi
        for child in ((x0, cx, y0, cy), (cx, x1, y0, cy),
                      (x0, cx, cy, y1), (cx, x1, cy, y1)):
            value = evaluate(model, child)
            lo += value.lo
            hi += value.hi
            nodes += 1
            if value.hi > value.lo:
                sequence += 1
                heapq.heappush(queue, (-(value.hi-value.lo), sequence, child, value, depth+1))
            deepest = max(deepest, depth+1)
        maximum_queue = max(maximum_queue, len(queue))
    if hi-lo <= absolute_width:
        status = 'area_width_met'
    return {'area': Interval(lo, hi), 'status': status, 'evaluated_nodes': nodes,
            'maximum_active_queue': maximum_queue, 'deepest_level': deepest,
            'seconds': time.perf_counter()-start}


def main():
    out = Path(__file__).resolve().parents[1]/'evidence/round2_mp_affine_fallback.json'
    here = Path(__file__).resolve().parent
    iv.dps = 60
    result = {'command': 'python 实验/science-foundation/oracles/round2_mp_affine_fallback.py',
              'source_sha256': {n: hashlib.sha256((here/n).read_bytes()).hexdigest() for n in
                  ('round2_mp_affine_fallback.py', 'round2_science_budget.py',
                   'round2_cpp_science_bridge.py', 'round2_affine_independent_review.py', 'round2_sliver.py')},
              'environment': {'python': platform.python_version(), 'platform': platform.platform()},
              'precision_decimal_digits': 60, 'seed': None,
              'caveat': 'conditional on mpmath.iv elementary-function enclosures; exact rational clipping/ledger and stopping; not formal machine proof',
              'scope': 'focused Nside1 leaf0, small diagonal TAN at ICRS45,0 with optional A_0_2 shear; explicit admission, no generic-WCS support claim',
              'cases': []}
    for shear in (0, .07):
        h = fixture(1e-7)
        if shear:
            h.update(CTYPE1='RA---TAN-SIP', CTYPE2='DEC--TAN-SIP', A_ORDER=2, B_ORDER=2, A_0_2=shear)
        # Separate analytic phi,z integral is never used by evaluate or solve.
        analytic = analytic_new(h)
        truth = Interval(Q.from_float(analytic['area']['lower_sr']), Q.from_float(analytic['area']['upper_sr']))
        iv.dps = 60
        model = Model(h)
        admissible = admission(model)
        begin = time.perf_counter()
        native = total_area(model, 16)
        native_seconds = time.perf_counter()-begin
        target = Q(1, 100)
        x = Q(10**12)
        # Leave room for the independently enclosed normalization A.
        requested = target*native.lo/(2*x)
        case = {'header': h, 'analytic_truth': analytic, 'admission': admissible,
                'native_drop_area_sr': native.dump(), 'native_area_subdivisions_per_axis': 16,
                'native_area_seconds': native_seconds, 'requested_flux_error_L': float(target),
                'requested_leaf_area_width_sr': float(requested), 'attempts': []}
        result['cases'].append(case)
        for label, nodes in (('limited_budget', 101), ('science_refined', 20001)):
            print('start', shear, label, flush=True)
            sol = solve(model, requested, max_nodes=nodes)
            aa = sol.pop('area')
            assert aa.lo <= truth.lo <= truth.hi <= aa.hi
            science = []
            for sign in (1, -1):
                flux = aa/native*(sign*x)
                truth_flux = truth/native*(sign*x)
                error = abs(flux.mid-truth_flux.mid)+truth_flux.radius
                bound = flux.radius
                assert error <= bound
                science.append({'native_integral_L': float(sign*x),
                                'allocated_flux_L': flux.dump(), 'true_flux_L': truth_flux.dump(),
                                'actual_error_upper_L': float(error), 'predicted_error_bound_L': float(bound),
                                'status': 'science_bounded' if bound <= target else 'science_unconverged'})
            case['attempts'].append({'name': label, 'max_nodes': nodes, 'geometry': sol,
                                     'leaf0_area_sr': aa.dump(), 'science': science})
            out.write_text(json.dumps(result, indent=2, allow_nan=False)+'\n', encoding='utf-8')
            print(shear, label, sol, [(s['predicted_error_bound_L'],s['status']) for s in science], flush=True)
        assert all(s['status'] == 'science_bounded' for s in case['attempts'][-1]['science'])


if __name__ == '__main__':
    main()
