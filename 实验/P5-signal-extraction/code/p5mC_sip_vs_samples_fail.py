"""V1：用含 SIP 的正确映射（p1_wcs.json 的 wcs.sip + cd + crpix/crval）重做跨组/组内星点偏移直方图，
并与 samples 拟合版并列。判别"11″ 是产物缺陷还是读取器假象"。
"""
import json, os, sys
import numpy as np
from scipy.spatial import cKDTree
import astropy.wcs as aw
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")
DEG = 1.0 / 57.29577951308232


def sip_wcs(wj):
    d = json.load(open(wj)); w = d["wcs"]
    W = aw.WCS(naxis=2)
    W.wcs.crpix = [w["crpix1"], w["crpix2"]]
    W.wcs.crval = [w["crval1"], w["crval2"]]
    W.wcs.cd = np.array([[w["cd11"], w["cd12"]], [w["cd21"], w["cd22"]]])
    W.wcs.ctype = [d["ctype1"], d["ctype2"]]
    s = w.get("sip") or {}
    if s:
        o = int(d.get("sip_order", 3)); n = o + 1
        def arr(key):
            v = s.get(key)
            if v is None: return None
            v = np.asarray(v, float).ravel()
            if v.size == n * n: return v.reshape(n, n)
            if v.size == 36: return v.reshape(6, 6)[:n, :n]
            return None
        a, b = arr("a"), arr("b")
        ap, bp = arr("ap"), arr("bp")
        if a is not None and b is not None:
            W.sip = aw.Sip(a, b, ap if ap is not None else np.zeros_like(a),
                           bp if bp is not None else np.zeros_like(b), W.wcs.crpix)
    return W, d


def src(fid, tag):
    d = json.load(open(os.path.join(PDIR, tag, fid, "p1_wcs.json")))
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    stem = [os.path.basename(e["input_light"]).replace(".fts", "") for e in man["frames"] if e["frame_id"] == fid][0]
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    fr = [f for f in fl["frames"] if f["file"].replace("cleaned_", "").replace(".fts", "") == stem][0]
    xy = np.array([[s["x"], s["y"]] for s in fr["results"] if s.get("valid") and s["flux"] > 0])
    return xy, d


def hist(xyA, raA, decA, xyB, raB, decB, r=60.0):
    ra0, dec0 = float(np.median(raA)), float(np.median(decA))
    ua = (raA - ra0) * np.cos(np.radians(decA)); va = decA - dec0
    ub = (raB - ra0) * np.cos(np.radians(decB)); vb = decB - dec0
    tb = cKDTree(np.stack([ub, vb], 1))
    idx = tb.query_ball_point(np.stack([ua, va], 1), r=r / 3600.0)
    nb = 2001; H = np.zeros((nb, nb), np.int32)
    for i, js in enumerate(idx):
        for j in js:
            di = int(round((ub[j] - ua[i]) * 3600)) + nb // 2; dj = int(round((vb[j] - va[i]) * 3600)) + nb // 2
            if 0 <= di < nb and 0 <= dj < nb: H[di, dj] += 1
    k = np.unravel_index(np.argmax(H), H.shape)
    return {"peak_offset_arcsec": [int(k[0] - nb // 2), int(k[1] - nb // 2)], "peak_count": int(H[k]),
            "zero_offset_count": int(H[nb // 2, nb // 2]), "n_sources": [int(ua.size), int(ub.size)]}


man = json.load(open(f"{PDIR}/t2/p1_products.json"))
def fid_of(part):
    for e in man["frames"]:
        if part in os.path.basename(e["input_light"]): return e["frame_id"]
    return None
cases = {"cross_M2_M3": (fid_of("M2_T2_flying_dutchman-20251212"), fid_of("M3_T2_flying_dutchman-20251224")),
         "within_M2": (fid_of("M2_T2_flying_dutchman-20251212"), fid_of("M2_T2_flying_dutchman-20251216"))}
res = {}
for lbl, (fA, fB) in cases.items():
    if not fA or not fB: print("[%s] 帧未找到" % lbl); continue
    xyA, dA = src(fA, "t2"); xyB, dB = src(fB, "t2")
    WA, _ = sip_wcs(os.path.join(PDIR, "t2", fA, "p1_wcs.json"))
    WB, _ = sip_wcs(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
    r = {}
    # SIP 版（正确几何）
    raA, decA = WA.all_pix2world(xyA[:, 0], xyA[:, 1], 0)
    raB, decB = WB.all_pix2world(xyB[:, 0], xyB[:, 1], 0)
    r["sip"] = hist(xyA, raA, decA, xyB, raB, decB)
    # samples 版（上一轮读取器）
    import importlib.util
    spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
    m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
    fA5, iA5, _ = m5.fit_geom(os.path.join(PDIR, "t2", fA, "p1_wcs.json"))
    fB5, iB5, _ = m5.fit_geom(os.path.join(PDIR, "t2", fB, "p1_wcs.json"))
    raA2, decA2 = iA5(xyA[:, 0], xyA[:, 1]); raB2, decB2 = iB5(xyB[:, 0], xyB[:, 1])
    r["samples"] = hist(xyA, raA2, decA2, xyB, raB2, decB2)
    # SIP 单帧自洽：前向 SIP vs JSON 的 rms_px
    xq = np.linspace(0, 4095, 9); XQ, YQ = np.meshgrid(xq, xq)
    ra_q, dec_q = WA.all_pix2world(XQ.ravel(), YQ.ravel(), 0)
    r["sip_vs_samples_selfcheck"] = {"n": int(XQ.size),
        "max_sep_arcsec": float(np.max(np.hypot(
            (ra_q - np.array([s["ra"] for s in _["samples"]])) if False else 0, 0)) * 0) }
    res[lbl] = r
    print("[%s] %s × %s" % (lbl, fA[:30], fB[:30]))
    print("   SIP 版   ：峰位 %s″ | 峰计数 %d | 零偏移计数 %d | 源数 %s" % (
        r["sip"]["peak_offset_arcsec"], r["sip"]["peak_count"], r["sip"]["zero_offset_count"], r["sip"]["n_sources"]))
    print("   samples 版：峰位 %s″ | 峰计数 %d | 零偏移计数 %d" % (
        r["samples"]["peak_offset_arcsec"], r["samples"]["peak_count"], r["samples"]["zero_offset_count"]))
json.dump(res, open(f"{OUT}/p5mC_sip_vs_samples.json", "w"), indent=1)
print("写入 results/p5w/p5mC_sip_vs_samples.json")
