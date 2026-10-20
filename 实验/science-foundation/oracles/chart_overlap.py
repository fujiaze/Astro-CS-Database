"""真实 HEALPix 等面积参数域的有界交叠参考算法；不调用生产几何。

目标域以 12 个 face 的四叉树覆盖。源 TAN 平面的每条 SIP 边是
Bernstein 多项式曲线；凸包分割用于证明目标块不与真实边相交。
只有证明内含的块才计入下界，所有未决块保留在上界。
mpmath.iv 执行包含型运算；最终二进制报告端点向外扩一 ulp。
"""
from __future__ import annotations
import argparse
import heapq
import json
import math
import time
from fractions import Fraction
from pathlib import Path

import numpy as np
from mpmath import iv

iv.dps = 25
JR = [2]*4 + [3]*4 + [4]*4
JP = [1, 3, 5, 7, 0, 2, 4, 6, 1, 3, 5, 7]


def low(x):
    return math.nextafter(float(x.a), -math.inf)


def high(x):
    return math.nextafter(float(x.b), math.inf)


def bounds(x):
    return low(x), high(x)


def hull(a, b):
    return iv.mpf([min(low(a), low(b)), max(high(a), high(b))])


def intersect(x, lo, hi):
    a, b = max(low(x), lo), min(high(x), hi)
    return None if a > b else iv.mpf([a, b])


def face_vector(face, s, t):
    """全 12 face 的连续 NESTED 坐标；返回单位向量的区间包围。

    s,t∈[0,1]，并跨 r=1,3 分支取并包。极点 nr=0 时 phi
    使用整片经度楔，避免 0/0。边界零测集可重复表达、不重复积分。
    """
    r, d = JR[face] - s - t, s - t
    pieces = []
    for region, (a, b) in enumerate(((0, 1), (1, 3), (3, 4))):
        rr = intersect(r, a, b)
        if rr is None:
            continue
        if region == 1:
            z = (2 - rr) * 2 / 3
            phi = iv.pi / 4 * (JP[face] + d)
        else:
            nr = rr if region == 0 else 4 - rr
            ratio = iv.mpf([-1, 1]) if low(nr) <= 0 else intersect(d / nr, -1, 1)
            if ratio is None:
                continue
            phi = iv.pi / 4 * (JP[face] + ratio)
            z = (1 - nr**2 / 3) * (1 if region == 0 else -1)
        zz = intersect(1 - z**2, 0, 1)
        rho = iv.sqrt(zz)
        pieces.append((rho * iv.cos(phi), rho * iv.sin(phi), z))
    return tuple(iv.mpf([min(low(p[k]) for p in pieces),
                         max(high(p[k]) for p in pieces)]) for k in range(3))


def dot(a, b):
    return sum((x*y for x, y in zip(a, b)), iv.mpf(0))


def poly_add(dst, src, scale=1):
    for k, v in src.items():
        dst[k] = dst.get(k, iv.mpf(0)) + scale*v


def line_power(u0, du, v0, dv, i, j):
    out = {}
    for a in range(i+1):
        for b in range(j+1):
            c = math.comb(i, a)*math.comb(j, b)*u0**(i-a)*du**a*v0**(j-b)*dv**b
            out[a+b] = out.get(a+b, iv.mpf(0)) + c
    return out


def bernstein(power, n):
    return [sum((power.get(i, iv.mpf(0))*math.comb(k, i)/math.comb(n, i)
                 for i in range(k+1)), iv.mpf(0)) for k in range(n+1)]


def split_curve(control):
    rows = [control]
    while len(rows[-1]) > 1:
        last = rows[-1]
        rows.append([tuple((a+b)/2 for a, b in zip(last[i], last[i+1]))
                     for i in range(len(last)-1)])
    return [row[0] for row in rows], [row[-1] for row in rows[::-1]]


def curve_box(control):
    return (min(low(p[0]) for p in control), max(high(p[0]) for p in control),
            min(low(p[1]) for p in control), max(high(p[1]) for p in control))


def disjoint(a, b):
    return a[1] < b[0] or b[1] < a[0] or a[3] < b[2] or b[3] < a[2]


class Curve:
    def __init__(self, control):
        self.control = control
        self.box = curve_box(control)
        self.children = None

    def exclude(self, box, depth=0):
        """证明整块与曲线及弦的同伦条带无交，返回有向折线。

        每段曲线和直弦同在其 Bernstein 凸包矩形，矩形与目标
        分离意味着以弦替边不改变目标块内的 winding number。
        """
        if disjoint(self.box, box):
            return [self.control[0], self.control[-1]]
        if depth >= 16:
            return None
        # 与目标块相比极小的未决曲线必然只影响边界，交给目标域细分。
        if max(self.box[1]-self.box[0], self.box[3]-self.box[2]) < max(box[1]-box[0], box[3]-box[2])/8:
            return None
        if self.children is None:
            self.children = [Curve(c) for c in split_curve(self.control)]
        a = self.children[0].exclude(box, depth+1)
        if a is None:
            return None
        b = self.children[1].exclude(box, depth+1)
        return None if b is None else a[:-1]+b


def winding(poly, box):
    """区间谓词；端点或射线相交比较不确定时拒绝作内外判定。"""
    for fraction in (0.371, 0.619, 0.213):
        px = iv.mpf(box[0] + fraction*(box[1]-box[0]))
        py = iv.mpf(box[2] + fraction*(box[3]-box[2]))
        count, okay = 0, True
        for a, b in zip(poly, poly[1:]+poly[:1]):
            if high(a[1]) < low(py):
                aa = False
            elif low(a[1]) > high(py):
                aa = True
            else:
                okay = False
                break
            if high(b[1]) < low(py):
                bb = False
            elif low(b[1]) > high(py):
                bb = True
            else:
                okay = False
                break
            if aa == bb:
                continue
            xx = a[0] + (py-a[1])*(b[0]-a[0])/(b[1]-a[1])
            if low(xx) > high(px):
                count += 1
            elif high(xx) >= low(px):
                okay = False
                break
        if okay:
            return count % 2 == 1
    return None


class Source:
    def __init__(self, header, center_xy0, pixfrac):
        h = header
        if not h['CTYPE1'].startswith('RA---TAN') or not h['CTYPE2'].startswith('DEC--TAN'):
            raise ValueError('reference solver supports ICRS TAN[-SIP] only')
        if h.get('RADESYS') != 'ICRS' or h.get('LONPOLE', 180) != 180:
            raise ValueError('nonstandard celestial frame/orientation requires explicit conversion')
        if any(h.get('CUNIT'+str(i),'deg') != 'deg' for i in (1,2)):
            raise ValueError('reference input must normalize CD/CUNIT to degrees')
        if any(str(key).startswith(('PV1_','PV2_','CPDIS','D2IM')) for key in h):
            raise ValueError('additional projection/distortion must be explicitly decoded')
        if not 0 < pixfrac <= 1:
            raise ValueError('pixfrac must be in (0,1]')
        self.cd = [[iv.mpf(h[f'CD{i+1}_{j+1}']) for j in range(2)] for i in range(2)]
        if 0 in (self.cd[0][0]*self.cd[1][1]-self.cd[0][1]*self.cd[1][0]):
            raise ValueError('singular CD')
        a, b = iv.mpf(h['CRVAL1'])*iv.pi/180, iv.mpf(h['CRVAL2'])*iv.pi/180
        self.c = (iv.cos(b)*iv.cos(a), iv.cos(b)*iv.sin(a), iv.sin(b))
        self.e = (-iv.sin(a), iv.cos(a), iv.mpf(0))
        self.n = (-iv.sin(b)*iv.cos(a), -iv.sin(b)*iv.sin(a), iv.cos(b))
        self.coeff = []
        for name in ('A', 'B'):
            self.coeff.append({(i, j): iv.mpf(h.get(f'{name}_{i}_{j}', 0))
                               for i in range(int(h.get(name+'_ORDER', 0))+1)
                               for j in range(int(h.get(name+'_ORDER', 0))+1-i)
                               if h.get(f'{name}_{i}_{j}', 0) != 0})
        u, v = [iv.mpf(center_xy0[k])+1-iv.mpf(h[f'CRPIX{k+1}']) for k in range(2)]
        f = iv.mpf(pixfrac)/2
        # 在整个源矩形上 ||∇(A,B)||_inf<1 是全局单射的充分证据。
        uu, vv = hull(u-f, u+f), hull(v-f, v+f)
        row_bounds = []
        for coeff in self.coeff:
            ds = []
            for axis in (0, 1):
                val = iv.mpf(0)
                for (i, j), c in coeff.items():
                    power = i if axis == 0 else j
                    if power:
                        val += c*power*uu**(i-(axis == 0))*vv**(j-(axis == 1))
                ds.append(max(abs(low(val)), abs(high(val))))
            row_bounds.append(sum(ds))
        self.contraction_upper = max(row_bounds)
        if self.contraction_upper >= 1:
            raise ValueError('injectivity not certified by reference contraction criterion')
        corners = [(u-f,v-f), (u+f,v-f), (u+f,v+f), (u-f,v+f)]
        self.edges = []
        for p, q in zip(corners, corners[1:]+corners[:1]):
            powers = [{0:p[0],1:q[0]-p[0]}, {0:p[1],1:q[1]-p[1]}]
            for axis in (0,1):
                for (i,j), c in self.coeff[axis].items():
                    poly_add(powers[axis], line_power(p[0], q[0]-p[0], p[1], q[1]-p[1], i,j), c)
            mapped = [{},{}]
            for axis in (0,1):
                for j in (0,1):
                    poly_add(mapped[axis], powers[j], self.cd[axis][j])
            degree = max(max(x) for x in mapped)
            bx, by = [bernstein(x, degree) for x in mapped]
            self.edges.append(Curve(list(zip(bx,by))))
        self.box = (min(x.box[0] for x in self.edges), max(x.box[1] for x in self.edges),
                    min(x.box[2] for x in self.edges), max(x.box[3] for x in self.edges))

    def classify(self, face, depth, ix, iy):
        den = 2**depth
        s, t = iv.mpf([ix/den,(ix+1)/den]), iv.mpf([iy/den,(iy+1)/den])
        vector = face_vector(face,s,t)
        dc, de, dn = dot(vector,self.c), dot(vector,self.e), dot(vector,self.n)
        if high(dc) <= 0:
            return False
        bx = [iv.mpf(x)*iv.pi/180 for x in self.box]
        if high(de-bx[0]*dc) < 0 or high(bx[1]*dc-de) < 0 or high(dn-bx[2]*dc) < 0 or high(bx[3]*dc-dn) < 0:
            return False
        if low(dc) <= 0:
            return None
        xx, yy = de/dc*180/iv.pi, dn/dc*180/iv.pi
        box = (*bounds(xx), *bounds(yy))
        if disjoint(box,self.box):
            return False
        poly = []
        for edge in self.edges:
            segment = edge.exclude(box)
            if segment is None:
                return None
            poly.extend(segment[:-1])
        return winding(poly,box)


def morton(ix,iy,order):
    return sum(((ix>>k)&1)<<(2*k) | ((iy>>k)&1)<<(2*k+1) for k in range(order))


def solve_overlap(header, center_xy0=(0.,0.), pixfrac=1., nside=16, *, absolute_tolerance_sr=1e-6, max_nodes=100000, max_depth=48):
    """返回每叶真实曲边交叠的上下界；达到预算才 status=bounded。

    不对不收敛叶强制归零，不归一化面积以掩盖误差。
    """
    order = int(math.log2(nside))
    if 2**order != nside or nside < 1 or not math.isfinite(absolute_tolerance_sr) or absolute_tolerance_sr <= 0:
        raise ValueError('power-of-two nside and positive area tolerance required')
    if not order <= max_depth <= 48 or max_nodes < 1:
        raise ValueError('order<=max_depth<=48 and positive work budget required')
    source = Source(header,center_xy0,pixfrac)
    queue, accepted = [], {}
    counter, work = 0, 0
    start = time.perf_counter()
    for face in range(12):
        # mass 使用精确有理数：即使深层小块与大块相加也不丢未决质量。
        heapq.heappush(queue,(-Fraction(4**order),counter,face,0,0,0,None))
        counter += 1
    unit = iv.pi/(3*nside*nside)
    budget = Fraction(absolute_tolerance_sr)/Fraction(high(unit))
    unresolved = sum(-q[0] for q in queue)
    coarse_count = 12 if order > 0 else 0
    stalled = []
    while queue and (unresolved > budget or coarse_count) and work < max_nodes:
        neg,c,face,depth,ix,iy,known = heapq.heappop(queue)
        work += 1
        if depth < order:
            coarse_count -= 1
        unresolved += neg
        kind = True if known is True else source.classify(face,depth,ix,iy)
        if kind is False:
            continue
        if kind is True and depth >= order:
            leaf = face*nside*nside+morton(ix>>(depth-order),iy>>(depth-order),order)
            accepted[leaf] = accepted.get(leaf,Fraction(0))-neg
            continue
        if depth >= max_depth:
            stalled.append((neg,c,face,depth,ix,iy,known))
            unresolved -= neg
            continue
        for dx,dy in ((0,0),(1,0),(0,1),(1,1)):
            heapq.heappush(queue,(neg/4,counter,face,depth+1,2*ix+dx,2*iy+dy,kind))
            counter += 1
            if depth+1 < order:
                coarse_count += 1
        unresolved -= neg
    pending,coarse = {},[]

    def measure(mass):
        return unit*iv.mpf(mass.numerator)/mass.denominator

    for neg,c,face,depth,ix,iy,known in queue+stalled:
        if depth < order:
            coarse.append({'face':face,'depth':depth,'ix':ix,'iy':iy,
                           'upper_sr':high(measure(-neg))})
            continue
        leaf = face*nside*nside+morton(ix>>(depth-order),iy>>(depth-order),order)
        pending[leaf] = pending.get(leaf,0)-neg
    areas = {}
    for leaf in sorted(set(accepted)|set(pending)):
        lower,upper = accepted.get(leaf,0), accepted.get(leaf,0)+pending.get(leaf,0)
        a,b = max(0,low(measure(Fraction(lower)))),high(measure(Fraction(upper)))
        areas[str(leaf)] = {'lower_sr':a,'upper_sr':b,'mid_sr':(a+b)/2}
    accepted_mass = sum(accepted.values(),Fraction(0))
    return {'areas':areas, 'coarse_unresolved_candidates':coarse,
            'total_area':{'lower_sr':max(0,low(measure(accepted_mass))),
             'upper_sr':high(measure(accepted_mass+unresolved))},
            'unresolved_mass_upper_sr':high(measure(unresolved)),
            'status':'bounded' if unresolved <= budget and not coarse else ('precision_exhausted' if stalled and not queue else 'budget_exhausted'),
            'processed_nodes':work,'seconds':time.perf_counter()-start,
            'contraction_upper':source.contraction_upper,'interval_decimal_precision':iv.dps,
            'exact_dyadic_mass_accumulation':True,'max_depth':max_depth}


def fixture(ra=0.,dec=0.,scale_deg=2.,sip=True):
    h = {'CTYPE1':'RA---TAN-SIP' if sip else 'RA---TAN',
         'CTYPE2':'DEC--TAN-SIP' if sip else 'DEC--TAN',
         'RADESYS':'ICRS','CRVAL1':ra,'CRVAL2':dec,'CRPIX1':1.,'CRPIX2':1.,
         'CD1_1':-scale_deg,'CD1_2':.12*scale_deg,
         'CD2_1':.07*scale_deg,'CD2_2':scale_deg}
    if sip:
        h.update(A_ORDER=4,B_ORDER=4,A_2_0=.08,A_1_1=-.03,A_0_3=.02,
                 B_2_0=.12,B_1_2=-.025,B_0_4=.015)
    return h


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('--output',type=Path)
    p.add_argument('--tolerance',type=float,default=1e-5)
    p.add_argument('--max-nodes',type=int,default=60000)
    args = p.parse_args()
    result = solve_overlap(fixture(),nside=16,absolute_tolerance_sr=args.tolerance,max_nodes=args.max_nodes)
    txt = json.dumps(result,indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(txt,encoding='utf-8')
    print(txt)
