#!/usr/bin/env python3
"""复核 p1_wcs.json#samples 的几何契约：用 cd+纯 TAN 与 cd+TAN+SIP 两种前向复现 ra/dec。

契约（docs/detail/registry/acsd.phase1.wcs-platesolve.md《p1_wcs.json 的 samples[] 几何契约》）：
samples 的 ra/dec 必须是**含 SIP 的完整前向**。像素桥接恰好一次（x 0 基、xp = x + 1）。
用法：python3 samples_geometry_check.py <path/to/p1_wcs.json>
"""
import json, math, sys
D2R = math.pi / 180.0


def forward(w, sip, xp, yp, use_sip):
    dx, dy = xp - w["crpix1"], yp - w["crpix2"]
    if use_sip and sip and sip.get("order", 0) > 0:
        o = sip["order"]
        A = sum(sip["a"][i * 6 + j] * dx ** i * dy ** j
                for i in range(o + 1) for j in range(o + 1 - i))
        B = sum(sip["b"][i * 6 + j] * dx ** i * dy ** j
                for i in range(o + 1) for j in range(o + 1 - i))
        U, V = dx + A, dy + B
    else:
        U, V = dx, dy
    xi = (w["cd11"] * U + w["cd12"] * V) * D2R
    eta = (w["cd21"] * U + w["cd22"] * V) * D2R
    ra0, dec0 = w["crval1"] * D2R, w["crval2"] * D2R
    rho = math.hypot(xi, eta)
    if rho == 0:
        return w["crval1"], w["crval2"]
    c = math.atan(rho)
    dec = math.asin(math.cos(c) * math.sin(dec0) + eta * math.sin(c) * math.cos(dec0) / rho)
    ra = ra0 + math.atan2(xi * math.sin(c),
                          rho * math.cos(dec0) * math.cos(c) - eta * math.sin(dec0) * math.sin(c))
    return math.degrees(ra) % 360.0, math.degrees(dec)


def sep_arcsec(a1, d1, a2, d2):
    p1, p2 = math.radians(d1), math.radians(d2)
    dl = math.radians(a2 - a1)
    c = max(-1.0, min(1.0, math.sin(p1) * math.sin(p2) + math.cos(p1) * math.cos(p2) * math.cos(dl)))
    return math.degrees(math.acos(c)) * 3600.0


def main(path):
    d = json.load(open(path))
    w, sip = d["wcs"], d["wcs"].get("sip")
    mt = ms = 0.0
    for s in d["samples"]:
        xp, yp = s["x"] + 1.0, s["y"] + 1.0
        ra_t, dec_t = forward(w, sip, xp, yp, False)
        ra_s, dec_s = forward(w, sip, xp, yp, True)
        mt = max(mt, sep_arcsec(s["ra"], s["dec"], ra_t, dec_t))
        ms = max(ms, sep_arcsec(s["ra"], s["dec"], ra_s, dec_s))
    print("samples n =", len(d["samples"]), " wcs_source =", d.get("wcs_source"),
          " sip.order =", (sip or {}).get("order"))
    print("max|samples - cd+TAN     | = %.4f arcsec   (修后应≈SIP 作用量, 修前为 0)" % mt)
    print("max|samples - cd+TAN+SIP | = %.6f arcsec   (修后应≈0, 修前为 SIP 作用量)" % ms)
    if d.get("max_sip_effect_deg") is not None:
        print("product max_sip_effect_deg = %.6f deg = %.2f arcsec"
              % (d["max_sip_effect_deg"], d["max_sip_effect_deg"] * 3600))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1] if len(sys.argv) > 1 else "p1_wcs.json"))
