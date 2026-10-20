"""Independent detector-domain TAN+SIP / true-HEALPix overlap oracle.

No ACSD geometry, HPX chart, corner polygon or production code is imported.
The mathematical enclosure is conditional on the documented completeness of
astropy-healpix cone_search_lonlat and the accuracy of Astropy/WCSLIB forward
coordinates. Those external floating-point routines are NOT interval-certified.
mpmath.iv encloses the polynomial, Jacobian and mass integration for the exact
binary64 header coefficients. The explicit angular margin is an engineering
roundoff allowance, not a proof about WCSLIB or astropy-healpix rounding.

Run: python detector_independent.py --output ../evidence/detector_independent.json
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import heapq
import json
import math
from pathlib import Path
import platform
import sys
import time

import astropy
from astropy import units as au
from astropy.io.fits import Header
from astropy.wcs import WCS
import astropy_healpix
from astropy_healpix import HEALPix
from mpmath import iv, mp
import mpmath
import numpy as np


def _lo(x):
    return math.nextafter(float(x.a), -math.inf)


def _hi(x):
    return math.nextafter(float(x.b), math.inf)


def _absmax(x):
    return max(abs(_lo(x)), abs(_hi(x)))


def _poly(coeff, x, y, dx=0, dy=0):
    out = iv.mpf(0)
    for i in range(coeff.shape[0]):
        for j in range(coeff.shape[1]):
            if i < dx or j < dy or coeff[i, j] == 0:
                continue
            factor = math.prod(range(i - dx + 1, i + 1))
            factor *= math.prod(range(j - dy + 1, j + 1))
            out += iv.mpf(float(coeff[i, j])) * factor * x ** (i - dx) * y ** (j - dy)
    return out


class DetectorModel:
    """Forward Astropy WCS plus separately derived detector area measure."""

    def __init__(self, header_or_wcs):
        iv.dps = 30
        self.wcs = header_or_wcs.deepcopy() if isinstance(header_or_wcs, WCS) else WCS(header_or_wcs)
        w = self.wcs
        if w.pixel_n_dim != 2 or w.world_n_dim != 2:
            raise ValueError("Only two-dimensional celestial TAN[+SIP] is supported")
        if [s[:8] for s in w.wcs.ctype] != ["RA---TAN", "DEC--TAN"]:
            raise ValueError("Expected RA---TAN[-SIP], DEC--TAN[-SIP] axis order")
        if any(getattr(w, name, None) is not None for name in ("cpdis1", "cpdis2", "det2im1", "det2im2")):
            raise ValueError("Lookup-table distortions are outside this oracle's admitted model")
        if w.wcs.get_pv():
            raise ValueError("Projection PV parameters are outside this oracle's admitted model")
        if any(str(unit) != "deg" for unit in w.wcs.cunit):
            raise ValueError("Astropy WCS axes must normalize to degrees")
        if w.sip is None and any("SIP" in item for item in w.wcs.ctype):
            raise ValueError("SIP is declared but its forward coefficients are absent")
        self.crpix = np.array(w.wcs.crpix, dtype=float)
        self.cd_deg = np.array(w.pixel_scale_matrix, dtype=float)
        if w.wcs.has_cd():
            self.cd = [[iv.mpf(float(c)) * iv.pi / 180 for c in row] for row in w.wcs.cd]
        else:
            # Preserve the exact real product of normalized binary64 PC and
            # CDELT coefficients instead of interval-enclosing a rounded dot.
            self.cd = [[iv.mpf(float(w.wcs.cdelt[r]))*iv.mpf(float(w.wcs.get_pc()[r,c]))*iv.pi/180
                        for c in range(2)] for r in range(2)]
        self.detcd = self.cd[0][0] * self.cd[1][1] - self.cd[0][1] * self.cd[1][0]
        if _lo(self.detcd) <= 0 <= _hi(self.detcd):
            raise ValueError("Singular CD matrix")
        self.a = np.zeros((1, 1)) if w.sip is None else np.asarray(w.sip.a)
        self.b = np.zeros((1, 1)) if w.sip is None else np.asarray(w.sip.b)
        if w.sip is not None and not np.array_equal(w.sip.crpix, self.crpix):
            raise ValueError("SIP and WCS CRPIX differ")

    def uv(self, rect):
        x0, x1, y0, y1 = rect
        return (iv.mpf([x0, x1]) + 1 - iv.mpf(float(self.crpix[0])),
                iv.mpf([y0, y1]) + 1 - iv.mpf(float(self.crpix[1])))

    def derivative(self, x, y):
        return [[1 + _poly(self.a, x, y, 1, 0), _poly(self.a, x, y, 0, 1)],
                [_poly(self.b, x, y, 1, 0), 1 + _poly(self.b, x, y, 0, 1)]]

    def plane(self, x, y):
        p, q = x + _poly(self.a, x, y), y + _poly(self.b, x, y)
        return [self.cd[0][0] * p + self.cd[0][1] * q,
                self.cd[1][0] * p + self.cd[1][1] * q]

    def density_interval(self, rect):
        x, y = self.uv(rect)
        return self.density_uv(x,y)

    def density_uv(self,x,y):
        xi, eta = self.plane(x, y)
        j = self.derivative(x, y)
        detj = j[0][0] * j[1][1] - j[0][1] * j[1][0]
        return abs(self.detcd) * abs(detj) / ((1 + xi ** 2 + eta ** 2) ** iv.mpf("1.5"))

    def midpoint_mass_interval(self, rect):
        """Product midpoint quadrature, rigorously bounded by interval g_xx,g_yy.

        One-dimensional midpoint remainder applied successively gives
        |integral_Q g - area(Q)g(center)| <= area(Q)(Mxx*wx²+Myy*wy²)/24.
        No unproved successive-refinement stopping criterion is used.
        """
        x,y = self.uv(rect)
        xp,yp = self.plane(x,y)
        p,q = {},{}
        for dx,dy in ((1,0),(0,1),(2,0),(1,1),(0,2),(3,0),(2,1),(1,2),(0,3)):
            p[dx,dy] = _poly(self.a,x,y,dx,dy) + int((dx,dy) == (1,0))
            q[dx,dy] = _poly(self.b,x,y,dx,dy) + int((dx,dy) == (0,1))
        det = p[1,0]*q[0,1]-p[0,1]*q[1,0]
        r = 1+xp**2+yp**2
        h = r**iv.mpf("-1.5")
        second = []
        for axis in (0,1):
            z = (1,0) if axis == 0 else (0,1)
            zz = tuple(2*v for v in z)
            add = lambda a,b: tuple(u+v for u,v in zip(a,b))
            p_xz,p_yz = p[add((1,0),z)],p[add((0,1),z)]
            q_xz,q_yz = q[add((1,0),z)],q[add((0,1),z)]
            dz = p_xz*q[0,1]+p[1,0]*q_yz-p_yz*q[1,0]-p[0,1]*q_xz
            dzz = (p[add((1,0),zz)]*q[0,1]+2*p_xz*q_yz+p[1,0]*q[add((0,1),zz)]
                   -p[add((0,1),zz)]*q[1,0]-2*p_yz*q_xz-p[0,1]*q[add((1,0),zz)])
            xz = self.cd[0][0]*p[z]+self.cd[0][1]*q[z]
            yz = self.cd[1][0]*p[z]+self.cd[1][1]*q[z]
            xzz = self.cd[0][0]*p[zz]+self.cd[0][1]*q[zz]
            yzz = self.cd[1][0]*p[zz]+self.cd[1][1]*q[zz]
            rz = 2*(xp*xz+yp*yz)
            rzz = 2*(xz**2+xp*xzz+yz**2+yp*yzz)
            hz = -iv.mpf("1.5")*r**iv.mpf("-2.5")*rz
            hzz = iv.mpf("3.75")*r**iv.mpf("-3.5")*rz**2-iv.mpf("1.5")*r**iv.mpf("-2.5")*rzz
            # det has one fixed sign under the global injectivity certificate;
            # abs(g_zz) therefore equals this expression's absolute magnitude.
            second.append(abs(self.detcd)*(dzz*h+2*dz*hz+det*hzz))
        wx,wy = iv.mpf(rect[1])-iv.mpf(rect[0]),iv.mpf(rect[3])-iv.mpf(rect[2])
        volume = wx*wy
        eavg = (iv.mpf(_absmax(second[0]))*wx**2+iv.mpf(_absmax(second[1]))*wy**2)/24
        cx = (iv.mpf(rect[0])+iv.mpf(rect[1]))/2+1-iv.mpf(float(self.crpix[0]))
        cy = (iv.mpf(rect[2])+iv.mpf(rect[3]))/2+1-iv.mpf(float(self.crpix[1]))
        midpoint = self.density_uv(cx,cy)*volume
        error = _hi(eavg*volume)
        return midpoint+iv.mpf([-error,error])

    def density(self, xy):
        """Vectorized binary64 density for independent iid Monte Carlo only."""
        x = np.asarray(xy)[:, 0] + 1 - self.crpix[0]
        y = np.asarray(xy)[:, 1] + 1 - self.crpix[1]

        def p(c, dx=0, dy=0):
            out = np.zeros_like(x)
            for i in range(c.shape[0]):
                for j in range(c.shape[1]):
                    if i >= dx and j >= dy and c[i, j] != 0:
                        factor = math.prod(range(i - dx + 1, i + 1)) * math.prod(range(j - dy + 1, j + 1))
                        out += c[i, j] * factor * x ** (i - dx) * y ** (j - dy)
            return out

        z = np.deg2rad(self.cd_deg) @ np.stack((x + p(self.a), y + p(self.b)))
        detj = (1 + p(self.a, 1, 0)) * (1 + p(self.b, 0, 1)) - p(self.a, 0, 1) * p(self.b, 1, 0)
        return abs(np.linalg.det(np.deg2rad(self.cd_deg))) * abs(detj) / (1 + np.sum(z*z, axis=0)) ** 1.5

    def injectivity_certificate(self, rect):
        """Certify ||J(center)^-1 J(U)-I||_F < 1 on convex detector Q.

        Then the transformed SIP map is Id plus a strict contraction, hence
        globally injective. Nonzero determinant by itself would not prove this.
        """
        x0, x1, y0, y1 = rect
        xc, yc = self.uv(((x0+x1)/2, (x0+x1)/2, (y0+y1)/2, (y0+y1)/2))
        j0 = self.derivative(xc, yc)
        d0 = j0[0][0]*j0[1][1]-j0[0][1]*j0[1][0]
        if _lo(d0) <= 0 <= _hi(d0):
            raise ValueError("Center SIP Jacobian singular")
        inv = [[j0[1][1]/d0, -j0[0][1]/d0], [-j0[1][0]/d0, j0[0][0]/d0]]
        x, y = self.uv(rect)
        j = self.derivative(x, y)
        m = [[sum((inv[r][k]*j[k][c] for k in range(2)), iv.mpf(0)) - int(r == c)
              for c in range(2)] for r in range(2)]
        bound = _hi(iv.sqrt(sum((iv.mpf(_absmax(v))**2 for row in m for v in row),iv.mpf(0))))
        if not bound < 1:
            raise ValueError(f"Injectivity not certified on drop: contraction upper bound {bound}")
        return {"method": "interval_preconditioned_Jacobian_contraction", "frobenius_upper": bound}

    def cap(self, rect, angular_margin_rad):
        x0, x1, y0, y1 = rect
        cx, cy = (x0+x1)/2, (y0+y1)/2
        x, y = self.uv(rect)
        xc, yc = self.uv((cx, cx, cy, cy))
        plane, center_plane = self.plane(x, y), self.plane(xc, yc)
        # Unit TAN vector map has differential operator norm <= 1 everywhere.
        radius = _hi(iv.sqrt(sum((iv.mpf(_absmax(a-b))**2 for a,b in zip(plane,center_plane)),iv.mpf(0))))
        lon, lat = self.wcs.all_pix2world([[cx, cy]], 0)[0]
        if not np.isfinite([lon, lat, radius]).all():
            raise ValueError("Non-finite WCS forward or radius")
        return lon, lat, min(math.pi, math.nextafter(radius + angular_margin_rad, math.inf))


@dataclass
class Node:
    rect: tuple
    depth: int
    lower: float
    upper: float
    candidates: tuple

    @property
    def uncertainty(self):
        return math.nextafter(self.upper-self.lower,math.inf) if len(self.candidates) == 1 else self.upper


def solve_overlap(header_or_WCS, center_xy0=(0.0, 0.0), pixfrac=1.0, nside=8, *,
                  relative_tolerance=0.005, absolute_tolerance_sr=0.0, max_nodes=30000,
                  angular_margin_rad=3e-14, max_depth=40, integration_order=2):
    """Enclose true-leaf solid angles without approximating any curved boundary.

    Bounds concern exact header-model geometry, conditionally on external
    routines as stated above. Q is the exact rectangle of the returned binary64
    `detector_bounds_xy0`; the detector origin is zero-based pixel centers.
    Status 'tolerance_met' means every leaf interval width is at most the
    absolute requested budget. It does not mean relative accuracy for slivers.
    max_nodes bounds evaluated nodes, not final leaves. Time/memory O(N log N)
    / O(N+K) excluding the external inclusive-cone search candidate cost.
    """
    if not (0 < pixfrac <= 1) or relative_tolerance < 0 or absolute_tolerance_sr < 0:
        raise ValueError("Invalid pixfrac or tolerance")
    if nside < 1 or nside & (nside - 1):
        raise ValueError("nside must be a positive power of two")
    if max_nodes < 1 or angular_margin_rad < 0:
        raise ValueError("Invalid budget or angular margin")
    if integration_order not in (0,2):
        raise ValueError("integration_order must be 0 (natural interval) or 2 (bounded midpoint)")
    start = time.perf_counter()
    model, hp = DetectorModel(header_or_WCS), HEALPix(nside=nside, order="nested")
    cx, cy = map(float, center_xy0)
    rect = (cx-pixfrac/2, cx+pixfrac/2, cy-pixfrac/2, cy+pixfrac/2)
    injectivity = model.injectivity_certificate(rect)
    calls = 0

    def node(r, depth):
        nonlocal calls
        g = model.density_interval(r)
        volume = (iv.mpf(r[1])-iv.mpf(r[0])) * (iv.mpf(r[3])-iv.mpf(r[2]))
        mass = g * volume
        lon, lat, radius = model.cap(r, angular_margin_rad)
        candidates = tuple(map(int, hp.cone_search_lonlat(lon*au.deg, lat*au.deg, radius*au.rad)))
        if not candidates:
            raise ArithmeticError("External inclusive cone search returned no candidate")
        # Avoid expensive Hessians where the natural interval already consumes
        # a negligible part of the global budget. This changes only tightness,
        # never the validity of the selected mass interval.
        use_midpoint = (_hi(mass)-_lo(mass)) > relative_tolerance*.25*max(0.,_lo(mass))
        if len(candidates) == 1 and integration_order == 2 and use_midpoint:
            tighter = model.midpoint_mass_interval(r)
            mass = iv.mpf([max(_lo(mass),_lo(tighter)),min(_hi(mass),_hi(tighter))])
        calls += 1
        return Node(r, depth, max(0.0, _lo(mass)), _hi(mass), candidates)

    root = node(rect, 0)
    target = max(absolute_tolerance_sr, relative_tolerance*(root.lower+root.upper)/2)
    heap, serial = [(-root.uncertainty, 0, root)], 1
    uncertainty = root.uncertainty
    status = "tolerance_met"
    while uncertainty > target:
        if calls + 4 > max_nodes:
            status = "budget_exhausted"
            break
        _, _, old = heapq.heappop(heap)
        if old.depth >= max_depth:
            heapq.heappush(heap, (-old.uncertainty, serial, old))
            status = "resolution_exhausted"
            break
        x0, x1, y0, y1 = old.rect
        xm, ym = (x0+x1)/2, (y0+y1)/2
        if xm in (x0, x1) or ym in (y0, y1):
            heapq.heappush(heap, (-old.uncertainty, serial, old))
            status = "resolution_exhausted"
            break
        children = [node(r, old.depth+1) for r in ((x0,xm,y0,ym), (xm,x1,y0,ym),
                                                   (x0,xm,ym,y1), (xm,x1,ym,y1))]
        uncertainty += math.fsum(v.uncertainty for v in children) - old.uncertainty
        for child in children:
            heapq.heappush(heap, (-child.uncertainty, serial, child))
            serial += 1
    leaves = [entry[2] for entry in heap]
    lower_parts, upper_parts = {}, {}
    for item in leaves:
        for p in item.candidates:
            upper_parts.setdefault(p, []).append(item.upper)
            if len(item.candidates) == 1:
                lower_parts.setdefault(p, []).append(item.lower)
    areas = {}
    for p in sorted(upper_parts):
        lo = max(0.0, math.nextafter(math.fsum(lower_parts.get(p, [])), -math.inf))
        hi = math.nextafter(math.fsum(upper_parts[p]), math.inf)
        areas[str(p)] = {"lower_sr": lo, "upper_sr": hi, "mid_sr": (lo+hi)/2}
    uncertainty = math.nextafter(math.fsum(item.uncertainty for item in leaves),math.inf)
    if uncertainty > target and status == "tolerance_met":
        status = "roundoff_budget_not_met"
    return {
        "status": status,
        "areas": areas,
        "total_area": {"lower_sr": math.nextafter(math.fsum(n.lower for n in leaves), -math.inf),
                       "upper_sr": math.nextafter(math.fsum(n.upper for n in leaves), math.inf)},
        "unresolved_mass_upper_sr": math.fsum(n.upper for n in leaves if len(n.candidates) != 1),
        "classified_integration_width_sr": math.fsum(n.upper-n.lower for n in leaves if len(n.candidates) == 1),
        "per_leaf_width_budget_sr": target,
        "uncertainty_bound_sr": uncertainty,
        "evaluated_nodes": calls,
        "terminal_rectangles": len(leaves),
        "max_depth_reached": max(n.depth for n in leaves),
        "angular_roundoff_margin_rad": angular_margin_rad,
        "integration_order": integration_order,
        "injectivity": injectivity,
        "detector_bounds_xy0": rect,
        "convention": "Astropy origin=0 pixel centers; detector Q center +/- pixfrac/2",
        "bound_class": "conditional_numerical_enclosure_external_WCSLIB_HEALPix_not_interval_certified",
        "elapsed_seconds": time.perf_counter()-start,
    }


def tan_rectangle_area_mp(header_or_wcs, rect):
    """Independent high-precision exact spherical quadrilateral for linear TAN.

    Unlike SIP, linear TAN maps straight detector edges to great-circle arcs.
    At 70 decimal digits sum two solid-angle triangles in tangent frame.
    """
    model = DetectorModel(header_or_wcs)
    if np.any(model.a) or np.any(model.b):
        raise ValueError("Analytic quadrilateral reference is restricted to SIP-free TAN")
    with mp.workdps(70):
        cd = [[mp.mpf(float(c))*mp.pi/180 for c in row] for row in model.cd_deg]
        x0,x1,y0,y1 = rect
        corners = [(x0,y0), (x1,y0), (x1,y1), (x0,y1)]
        vectors = []
        for x, y in corners:
            u, v = mp.mpf(x)+1-mp.mpf(float(model.crpix[0])), mp.mpf(y)+1-mp.mpf(float(model.crpix[1]))
            q = [mp.mpf(1), cd[0][0]*u+cd[0][1]*v, cd[1][0]*u+cd[1][1]*v]
            norm = mp.sqrt(sum(t*t for t in q))
            vectors.append([t/norm for t in q])

        def triangle(a,b,c):
            det = mp.det(mp.matrix([a,b,c]))
            dot = lambda x,y: sum(u*v for u,v in zip(x,y))
            return abs(2*mp.atan2(det, 1+dot(a,b)+dot(b,c)+dot(c,a)))

        return float(triangle(vectors[0],vectors[1],vectors[2]) +
                     triangle(vectors[0],vectors[2],vectors[3]))


def iid_reference(header_or_wcs, center_xy0, pixfrac, nside, *, samples=200000,
                  seed=111, family_failure_probability=1e-5):
    """Independent iid membership integral with simultaneous Hoeffding bounds.

    Y_p = area(Q)*g(U)*1{W(U) in p} in [0,M]. Union bound over ALL 12*nside²
    leaves avoids data-selected-leaf bias. It is a probabilistic reference,
    never a no-sliver proof; these intervals are intentionally conservative.
    """
    model, hp = DetectorModel(header_or_wcs), HEALPix(nside=nside, order="nested")
    cx,cy = center_xy0
    rect = (cx-pixfrac/2,cx+pixfrac/2,cy-pixfrac/2,cy+pixfrac/2)
    rng = np.random.default_rng(seed)
    xy = rng.uniform([rect[0],rect[2]], [rect[1],rect[3]], size=(samples,2))
    lon,lat = model.wcs.all_pix2world(xy,0).T
    leaf = hp.lonlat_to_healpix(lon*au.deg,lat*au.deg)
    volume = (rect[1]-rect[0])*(rect[3]-rect[2])
    values = volume*model.density(xy)
    upper = volume*_hi(model.density_interval(rect))
    radius = upper*math.sqrt(math.log(2*(12*nside*nside+1)/family_failure_probability)/(2*samples))
    result = {}
    for key in np.unique(leaf):
        selected = leaf == key
        estimate = math.fsum(values[selected])/samples
        result[str(key)] = {"estimate_sr": estimate, "hoeffding_half_width_sr": radius,
                            "hits": int(np.sum(selected))}
    return {"samples": samples,"seed":seed,"family_failure_probability":family_failure_probability,
            "all_leaves_in_union_bound":12*nside*nside,"hoeffding_half_width_sr":radius,
            "total_area_estimate_sr":float(np.mean(values)), "areas":result}


def forward_density_check(header_or_wcs,center_xy0,step=.001):
    """Independent finite differences of Astropy sky unit vectors, not of g."""
    model = DetectorModel(header_or_wcs)
    x,y = center_xy0
    points = [[x+step,y],[x-step,y],[x,y+step],[x,y-step]]
    lon,lat = np.deg2rad(model.wcs.all_pix2world(points,0).T)
    vectors = np.stack((np.cos(lat)*np.cos(lon),np.cos(lat)*np.sin(lon),np.sin(lat)),axis=1)
    finite = np.linalg.norm(np.cross((vectors[0]-vectors[1])/(2*step),(vectors[2]-vectors[3])/(2*step)))
    analytic = model.density(np.array([[x,y]]))[0]
    return {"step_pixel":step,"astropy_vector_finite_difference_sr_per_pixel2":float(finite),
            "analytic_density_sr_per_pixel2":float(analytic),"relative_difference":float(finite/analytic-1),
            "qualification":"Finite difference convergence check, not a rigorous error bound"}


def make_header(lon=0.0, lat=0.0, scale_deg=4.0, *, sip=False, rotation_deg=19.0):
    h = Header()
    h["NAXIS"],h["NAXIS1"],h["NAXIS2"] = 2,32,32
    h["CTYPE1"] = "RA---TAN-SIP" if sip else "RA---TAN"
    h["CTYPE2"] = "DEC--TAN-SIP" if sip else "DEC--TAN"
    h["CUNIT1"],h["CUNIT2"] = "deg","deg"
    h["CRPIX1"],h["CRPIX2"] = 1.0,1.0
    h["CRVAL1"],h["CRVAL2"] = float(lon),float(lat)
    c,s = math.cos(math.radians(rotation_deg)),math.sin(math.radians(rotation_deg))
    h["CD1_1"],h["CD1_2"] = -scale_deg*c,scale_deg*s
    h["CD2_1"],h["CD2_2"] = scale_deg*s,scale_deg*c
    if sip:
        h["A_ORDER"],h["B_ORDER"] = 3,3
        h["A_2_0"],h["A_1_1"],h["A_0_3"] = 0.12,-0.07,0.035
        h["B_0_2"],h["B_1_1"],h["B_3_0"] = -0.10,0.04,-0.025
    return h


def fixtures():
    """Shared headers are data, not an imported geometry implementation."""
    hp = HEALPix(nside=1,order="nested")
    output = []
    for face in range(12):
        lon,lat = hp.healpix_to_lonlat(face)
        output.append({"name":f"base_face_{face:02d}","header":make_header(lon.deg,lat.deg,6.0),
                       "center_xy0":(0.,0.),"pixfrac":1.,"nside":8})
    output += [
        {"name":"ra_wrap_sip","header":make_header(359.99,2.,4.,sip=True),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        {"name":"north_pole_sip","header":make_header(17.,89.99,4.,sip=True),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        {"name":"south_pole_sip","header":make_header(281.,-89.99,4.,sip=True),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        {"name":"equatorial_transition_sip","header":make_header(45.,math.degrees(math.asin(2/3)),4.,sip=True),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        {"name":"pixfrac_0p37","header":make_header(90.,-30.,4.,sip=True),"center_xy0":(.2,-.1),"pixfrac":.37,"nside":8},
        # Deliberately terminate below sliver resolution; upper bounds retain
        # candidates with no Monte Carlo hits instead of dropping them.
        {"name":"four_leaf_vertex","header":make_header(0.000001,0.,.03,rotation_deg=0.),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        # For this scale/orientation the lower corners meet the neighboring
        # true leaf at CRVAL2 ~= .02773239523293825 deg (external membership
        # bisection, retained as fixture data). Moving down by 1e-5 deg creates
        # genuine tiny corner slivers. It is not a mathematically exact tangent.
        {"name":"tiny_boundary_sliver","header":make_header(0.,.02772239523293825,.03,rotation_deg=0.),"center_xy0":(0.,0.),"pixfrac":1.,"nside":8},
        {"name":"high_order_small_drop","header":make_header(359.99999,89.9,1/3600,sip=True),"center_xy0":(0.,0.),"pixfrac":.7,"nside":262144},
    ]
    return output


def negative_cases():
    """Known noninjective forward map and invalid scientific input evidence."""
    folded = make_header(45.,40.,1.,sip=True)
    folded["A_2_0"] = 2.
    folded["A_1_1"] = 0.
    folded["A_0_3"] = 0.
    folded["B_0_2"] = 0.
    folded["B_1_1"] = 0.
    folded["B_3_0"] = 0.
    # F_x(-.5)=F_x(0)=0, with identical y: explicit noninjectivity.
    cases = [("known_fold_x_plus_2x2",folded,{}),
             ("pixfrac_zero",make_header(),{"pixfrac":0.}),
             ("nside_not_power_two",make_header(),{"nside":3})]
    output = []
    for name,header,kw in cases:
        try:
            solve_overlap(header,max_nodes=1,**kw)
        except ValueError as exc:
            output.append({"name":name,"rejected":True,"reason":str(exc)})
        else:
            output.append({"name":name,"rejected":False})
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--relative-tolerance", type=float, default=.015)
    parser.add_argument("--max-nodes", type=int, default=5001)
    parser.add_argument("--samples", type=int, default=100000)
    parser.add_argument("--integration-order", type=int, choices=(0,2), default=2)
    parser.add_argument("--fixture", action="append")
    args = parser.parse_args()
    source_sha256 = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
    records = []
    for case in fixtures():
        name = case.pop("name")
        if args.fixture and name not in args.fixture:
            continue
        header = case["header"]
        result = solve_overlap(case.pop("header"), **case, relative_tolerance=args.relative_tolerance,max_nodes=args.max_nodes,integration_order=args.integration_order)
        mc = iid_reference(header,**case,samples=args.samples)
        checks = []
        for p in set(result["areas"]) | set(mc["areas"]):
            bound = result["areas"].get(p,{"lower_sr":0.,"upper_sr":0.})
            ref = mc["areas"].get(p,{"estimate_sr":0.,"hoeffding_half_width_sr":mc["hoeffding_half_width_sr"]})
            err = ref["hoeffding_half_width_sr"]
            checks.append(bound["lower_sr"] <= ref["estimate_sr"]+err and bound["upper_sr"] >= ref["estimate_sr"]-err)
        record = {"name":name,"header":str(header),"parameters":case,"enclosure":result,"iid_reference":mc,
                  "all_iid_confidence_intervals_intersect":all(checks)}
        record["forward_density_check"] = forward_density_check(header,case["center_xy0"])
        x0,x1,y0,y1 = result["detector_bounds_xy0"]
        world = WCS(header).all_pix2world([[x0,y0],[x1,y0],[x1,y1],[x0,y1]],0)
        record["corner_leaf_indices"] = list(map(int,HEALPix(nside=case["nside"],order="nested").lonlat_to_healpix(world[:,0]*au.deg,world[:,1]*au.deg)))
        record["candidate_leaves_without_iid_hits"] = sorted(set(result["areas"])-set(mc["areas"]))
        if name == "tiny_boundary_sliver":
            # Interior witnesses stay strictly inside detector Q. Numerical
            # external membership differs from the main leaf despite no iid
            # hit; these are deliberately tiny nonzero-support components.
            points = [[x0+1e-8,y0+1e-8],[x1-1e-8,y0+1e-8],[(x0+x1)/2,(y0+y1)/2]]
            sky = WCS(header).all_pix2world(points,0)
            ids = HEALPix(nside=case["nside"],order="nested").lonlat_to_healpix(sky[:,0]*au.deg,sky[:,1]*au.deg)
            record["sliver_witness"] = {"detector_points_xy0":points,"external_leaf_indices":list(map(int,ids))}
        if not (header["CTYPE1"].endswith("SIP")):
            exact = tan_rectangle_area_mp(header,result["detector_bounds_xy0"])
            record["tan_analytic_total_sr"] = exact
            record["analytic_total_inside_enclosure"] = result["total_area"]["lower_sr"] <= exact <= result["total_area"]["upper_sr"]
        records.append(record)
        print(name,result["status"],result["evaluated_nodes"],f"uncertainty={result['uncertainty_bound_sr']:.4g}",flush=True)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps({"command":sys.argv,"source_sha256":source_sha256,"environment":{"python":platform.python_version(),"numpy":np.__version__,"astropy":astropy.__version__,"astropy_healpix":astropy_healpix.__version__,"mpmath":mpmath.__version__},
            "definition":"integral_Q 1_H(W(u,v)) abs(det(CD_rad))*abs(det(I+SIP'))/(1+xi_rad^2+eta_rad^2)^1.5 du dv",
            "method":"Independent detector adaptive interval quadrature with external inclusive cap search; no HPX chart code",
            "qualification":"Conditional numerical enclosures. External WCSLIB and HEALPix routines are binary64, not formally interval certified. Angular roundoff margin 3e-14 rad is stated, not proved. IID reference confidence is simultaneous per fixture only.",
            "source_urls":["https://astropy-healpix.readthedocs.io/en/latest/api/astropy_healpix.HEALPix.html","https://docs.astropy.org/en/stable/wcs/","https://mpmath.org/doc/current/contexts.html#arbitrary-precision-interval-arithmetic-iv"],"negative_cases":negative_cases(),"cases":records},indent=2,ensure_ascii=False),encoding="utf-8")


if __name__ == "__main__":
    main()
