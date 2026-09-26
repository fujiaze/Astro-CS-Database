#!/usr/bin/env python3
# edge_geom.py -- shared true-curve edge machinery for the ring-corner HEALPix vertex grid.
#
# Curve families (Gorski et al. 2005 sec 5.3, verified verbatim from arXiv:astro-ph/0409513):
#   equatorial belt: cos(theta) = a + b*phi   -> z = a + b*phi  (straight in (phi,z))
#   polar caps:      cos(theta) = a + b/phi^2 -> z = a + b/phi_t^2 (phi_t from quadrant edge; a=1 north, a=-1 south)
#   quadrant meridians: phi = const (great circles)
# Every leaf edge connects vertex-grid points of adjacent levels and lies wholly inside one zone.
# Meridian detection: endpoints share phi (mod 2pi). Pole edges (z=+-1) are meridians.
import numpy as np

def wrap_pi(x):
    return (x + np.pi) % (2*np.pi) - np.pi

def to_vec(phi, z):
    r = np.sqrt(np.maximum(0.0, 1.0 - z*z))
    return np.stack([r*np.cos(phi), r*np.sin(phi), z], axis=-1)

def sample_edge(p1, p2, n=65):
    """Sample n points along the true HEALPix boundary curve between corner vertices
    p=(phi,z). Returns (n,3) unit vectors. Raises ValueError if no family fits."""
    phi1, z1 = float(p1[0]), float(p1[1]); phi2, z2 = float(p2[0]), float(p2[1])
    dphi = wrap_pi(phi2 - phi1)
    za, zb = max(abs(z1), abs(z2)), min(abs(z1), abs(z2))
    cap = zb >= 2.0/3.0 - 1e-12
    t = np.linspace(0.0, 1.0, n)
    if abs(abs(z1)-1.0) < 1e-15 or abs(abs(z2)-1.0) < 1e-15:
        # pole edge: meridian through the non-pole endpoint
        phi1 = phi1 if abs(abs(z1)-1.0) > 1e-15 else phi2
        z2 = z1 if abs(abs(z1)-1.0) < 1e-15 else z2
        dphi = 0.0
    if abs(dphi) < 1e-12:                       # meridian (great circle)
        v1, v2 = to_vec(phi1, z1), to_vec(phi1, z2)
        om = np.arccos(np.clip(np.dot(v1, v2), -1, 1))
        if om < 1e-15:
            return np.repeat(v1[None, :], n, axis=0)
        axis = np.cross(v1, v2); axis = axis/np.linalg.norm(axis)
        ang = t*om
        return (v1[None,:]*np.cos(ang)[:,None] + np.cross(axis, v1)[None,:]*np.sin(ang)[:,None]
                + axis[None,:]*(np.dot(axis, v1))*((1-np.cos(ang))[:,None]))
    phi = phi1 + t*dphi
    if not cap:                                  # belt: straight in (phi,z)
        z = z1 + t*(z2 - z1)
        return to_vec(phi, z)
    # polar cap: z = sgn - c/phi_t^2 with phi_t measured from a quadrant edge
    sgn = 1.0 if z1 > 0 else -1.0
    best = None
    for mode in ("left", "right"):
        # quadrant index from unwrapped midpoint
        phm = phi1 + 0.5*dphi
        f = np.floor(phm/(np.pi/2))
        if mode == "left":
            tt1, tt2 = phi1 - f*np.pi/2, phi1 + dphi - f*np.pi/2
        else:
            tt1, tt2 = (f+1)*np.pi/2 - phi1, (f+1)*np.pi/2 - (phi1 + dphi)
        c1 = (sgn - z1)*tt1*tt1; c2 = (sgn - z2)*tt2*tt2
        if min(abs(tt1), abs(tt2)) < 1e-9:
            continue
        err = abs(c1 - c2)/max(abs(c1), abs(c2))
        if best is None or err < best[0]:
            best = (err, mode, f, c1, tt1)
    if best is None or best[0] > 1e-6:
        raise ValueError(f"no cap curve family fits edge {p1} {p2}: {best}")
    err, mode, f, c, tt1 = best
    if mode == "left":
        tt = phi - f*np.pi/2
    else:
        tt = (f+1)*np.pi/2 - phi
    tt = np.where(np.abs(tt) < 1e-12, 1e-12, tt)
    z = sgn - c/(tt*tt)
    return to_vec(phi, z)

def edge_sagitta_hp_res(p1, p2, hp_res, n=129):
    """Max angular deviation of the true curve from its great-circle chord, in hp_res units."""
    V = sample_edge(p1, p2, n=n)
    v1, v2 = V[0], V[-1]
    nrm = np.cross(v1, v2); ln = np.linalg.norm(nrm)
    if ln < 1e-300:
        return 0.0
    nrm = nrm/ln
    d = np.abs(V @ nrm)
    return float(np.arcsin(np.clip(d.max(), 0.0, 1.0))/hp_res)
