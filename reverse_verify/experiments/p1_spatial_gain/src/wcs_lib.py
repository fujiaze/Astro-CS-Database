#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""PHOT-VERIFY: exact port of AstroCS pc::WcsTransform (wcs_transform.cpp) to numpy.

Used only for read-only measurement; production code is untouched.
"""
import json, math
import numpy as np

D2R = math.pi / 180.0
R2D = 180.0 / math.pi

class Wcs:
    def __init__(self, j):
        w = j['wcs'] if 'wcs' in j else j
        self.crval1 = w['crval1']; self.crval2 = w['crval2']
        self.crpix1 = w['crpix1']; self.crpix2 = w['crpix2']
        self.cd = np.array([[w['cd11'], w['cd12']], [w['cd21'], w['cd22']]], float)
        det = self.cd[0,0]*self.cd[1,1] - self.cd[0,1]*self.cd[1,0]
        self.cdInv = np.array([[self.cd[1,1], -self.cd[0,1]], [-self.cd[1,0], self.cd[0,0]]], float)/det
        sip = w.get('sip') or {}
        self.order = int(sip.get('order', 0) or 0)
        self.has_sip = self.order > 0
        def arr(k):
            v = sip.get(k)
            if v is None: return np.zeros((6,6))
            a = np.zeros(36); a[:len(v)] = v
            return a.reshape(6,6)
        self.A = arr('a'); self.B = arr('b'); self.AP = arr('ap'); self.BP = arr('bp')
        self.has_ap = sip.get('ap') is not None and any(abs(x) > 0 for x in (sip.get('ap') or []))
        self.has_bp = sip.get('bp') is not None and any(abs(x) > 0 for x in (sip.get('bp') or []))

    def _sip(self, C, dx, dy):
        out = np.zeros_like(dx)
        for i in range(self.order+1):
            for j in range(self.order-i+1):
                c = C[i, j]
                if c != 0.0:
                    out = out + c * (dx**i) * (dy**j)
        return out

    def pixel_to_sky(self, x, y):
        dx = np.asarray(x, float) - (self.crpix1 - 1.0)
        dy = np.asarray(y, float) - (self.crpix2 - 1.0)
        if self.has_sip:
            dx = dx + self._sip(self.A, dx, dy)
            dy = dy + self._sip(self.B, dx, dy)
        xi  = self.cd[0,0]*dx + self.cd[0,1]*dy
        eta = self.cd[1,0]*dx + self.cd[1,1]*dy
        xi_r = xi*D2R; eta_r = eta*D2R
        rho = np.sqrt(xi_r**2 + eta_r**2)
        dec0 = self.crval2*D2R
        sdec0 = math.sin(dec0); cdec0 = math.cos(dec0)
        c = np.arctan(rho); sinc = np.sin(c); cosc = np.cos(c)
        with np.errstate(invalid='ignore', divide='ignore'):
            sin_dec = cosc*sdec0 + eta_r*sinc*cdec0/rho
            sin_dec = np.clip(sin_dec, -1.0, 1.0)
            dec = np.arcsin(sin_dec)
            dra = np.arctan2(xi_r*sinc, rho*cdec0*cosc - eta_r*sdec0*sinc)
        # rho -> 0 case
        tiny = rho < 1e-12
        dec = np.where(tiny, self.crval2*D2R, dec)
        dra = np.where(tiny, 0.0, dra)
        ra = self.crval1*D2R + dra
        ra = np.mod(ra, 2*math.pi)
        return ra*R2D, dec*R2D

    def sky_to_pixel(self, ra, dec):
        ra_r = np.asarray(ra, float)*D2R; dec_r = np.asarray(dec, float)*D2R
        ra0 = self.crval1*D2R; dec0 = self.crval2*D2R
        sdec0 = math.sin(dec0); cdec0 = math.cos(dec0)
        sdec = np.sin(dec_r); cdec = np.cos(dec_r)
        dra = ra_r - ra0; cdra = np.cos(dra); sdra = np.sin(dra)
        cosc = sdec0*sdec + cdec0*cdec*cdra
        xi_r  = cdec*sdra/cosc
        eta_r = (cdec0*sdec - sdec0*cdec*cdra)/cosc
        xi = xi_r*R2D; eta = eta_r*R2D
        dx = self.cdInv[0,0]*xi + self.cdInv[0,1]*eta
        dy = self.cdInv[1,0]*xi + self.cdInv[1,1]*eta
        if self.has_sip:
            if self.has_ap and self.has_bp:
                dx = dx + self._sip(self.AP, dx, dy)
                dy = dy + self._sip(self.BP, dx, dy)
            else:
                u = dx.copy(); v = dy.copy()
                for _ in range(3):
                    f = self._sip(self.A, u, v); g = self._sip(self.B, u, v)
                    u_new = dx - f; v_new = dy - g
                    if np.all(np.abs(u_new-u) < 1e-10) and np.all(np.abs(v_new-v) < 1e-10):
                        u = u_new; v = v_new; break
                    u = u_new; v = v_new
                dx = u; dy = v
        return dx + (self.crpix1-1.0), dy + (self.crpix2-1.0)
