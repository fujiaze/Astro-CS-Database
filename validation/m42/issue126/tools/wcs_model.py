"""Exact replica of the V2 `acsd.WcsModel` forward map (src/coordinates/tan_sip.cpp).

    u = x + 1 - CRPIX,  q = CD (u + A(u,v), v + B(u,v)),
    n = normalize(c + qx e + qy north),  ra = atan2(ny,nx), dec = asin(nz)

No astropy WCS reconstruction is involved: the model JSON is evaluated with the
same formula the production pipeline uses, so a geometry check in python and a
pixel check in C++ condition on the same camera model.
"""
from __future__ import annotations

import json
import math

import numpy as np


class WcsModel:
    def __init__(self, doc: dict):
        self.doc = doc
        self.crpix = np.array(doc["crpix_fits1"], float)
        self.cd = np.array(doc["cd_rad_per_pixel"], float).reshape(2, 2)
        self.c = np.array(doc["c"], float)
        self.c /= np.linalg.norm(self.c)
        self.e = np.array(doc["e"], float)
        self.e /= np.linalg.norm(self.e)
        self.north = np.array(doc.get("north", np.cross(self.c, self.e)), float)
        self.north /= np.linalg.norm(self.north)
        self.sip_a = {(t["x_power"], t["y_power"]): t["coefficient"]
                      for t in doc.get("sip_a", {}).get("terms", [])}
        self.sip_b = {(t["x_power"], t["y_power"]): t["coefficient"]
                      for t in doc.get("sip_b", {}).get("terms", [])}

    @classmethod
    def load(cls, path: str) -> "WcsModel":
        return cls(json.load(open(path)))

    def _poly(self, coeffs, u, v):
        total = np.zeros_like(u, dtype=float)
        for (i, j), c in coeffs.items():
            total = total + c * u ** i * v ** j
        return total

    def forward(self, x, y):
        x = np.asarray(x, float)
        y = np.asarray(y, float)
        u = x + 1.0 - self.crpix[0]
        v = y + 1.0 - self.crpix[1]
        a = self._poly(self.sip_a, u, v)
        b = self._poly(self.sip_b, u, v)
        qx = self.cd[0, 0] * (u + a) + self.cd[0, 1] * (v + b)
        qy = self.cd[1, 0] * (u + a) + self.cd[1, 1] * (v + b)
        n = (self.c[None, :] + qx[:, None] * self.e[None, :] + qy[:, None] * self.north[None, :])
        n = n / np.linalg.norm(n, axis=1)[:, None]
        ra = np.degrees(np.arctan2(n[:, 1], n[:, 0])) % 360.0
        dec = np.degrees(np.arcsin(np.clip(n[:, 2], -1.0, 1.0)))
        return ra, dec

    def corner_sky(self, x0, y0, x1, y1, n=21):
        xs = np.linspace(x0, x1, n)
        ys = np.linspace(y0, y1, n)
        gx, gy = np.meshgrid(xs, ys)
        return self.forward(gx.ravel(), gy.ravel())
