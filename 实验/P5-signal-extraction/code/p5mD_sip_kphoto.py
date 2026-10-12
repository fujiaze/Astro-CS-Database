"""修正版：用含 SIP 的正确几何重做「共同星通量比 vs k_photo 比」交叉验证（修正前/后并列）。

几何：p1_wcs.json 的 wcs.cd+crpix+crval+sip（astropy RA---TAN-SIP）前向；
通量：p1_flux.json（p5m7 已证同口径孔径测光与逐星通量结论一致，故此处不重复读影像）。
组别：组内对（同指向）+ 跨组对（现在可用，V1 零偏移命中 186）。
"""
import json, os, sys
import numpy as np
import astropy.wcs as aw
from scipy.spatial import cKDTree
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import importlib.util
spec = importlib.util.spec_from_file_location("m5", os.path.join(os.path.dirname(os.path.abspath(__file__)), "p5m5_star_ratio_sip.py"))
m5 = importlib.util.module_from_spec(spec); spec.loader.exec_module(m5)
PDIR = "/workspace/Astro CS Database/run/PHOT-FIX/p1_rerun"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "results", "p5w")


def sip_wcs(wj):
    d = json.load(open(wj)); w = d["wcs"]
    W = aw.WCS(naxis=2)
    W.wcs.crpix = [w["crpix1"], w["crpix2"]]; W.wcs.crval = [w["crval1"], w["crval2"]]
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
        a, b, ap, bp = arr("a"), arr("b"), arr("ap"), arr("bp")
        if a is not None and b is not None:
            W.sip = aw.Sip(a, b, ap if ap is not None else np.zeros_like(a),
                           bp if bp is not None else np.zeros_like(b), W.wcs.crpix)
    return W


def unit(ra, dec):
    ra, dec = np.radians(ra), np.radians(dec)
    return np.stack([np.cos(dec) * np.cos(ra), np.cos(dec) * np.sin(ra), np.sin(dec)], 1)


res = {}
for tag in ("t2", "t3"):
    man = json.load(open(f"{PDIR}/{tag}/p1_products.json"))
    phot = json.load(open(f"{PDIR}/{tag}/p1_phot.json"))["photscale_detail"]
    fl = json.load(open(f"{PDIR}/{tag}/p1_flux.json"))
    src = {fr["file"].replace("cleaned_", "").replace(".fts", ""):
           [r for r in fr["results"] if r.get("valid") and r["flux"] > 0] for fr in fl["frames"]}
    F = []
    for e in man["frames"]:
        fid = e["frame_id"]; stem = os.path.basename(e["input_light"]).replace(".fts", "")
        wj = os.path.join(PDIR, tag, fid, "p1_wcs.json")
        if not os.path.exists(wj) or not src.get(stem): continue
        r = src[stem]
        xy = np.array([[s["x"], s["y"]] for s in r]); flux = np.array([s["flux"] for s in r])
        W = sip_wcs(wj); ra, dec = W.all_pix2world(xy[:, 0], xy[:, 1], 0)
        f5, _, _ = m5.fit_geom(wj); ra5, dec5 = f5(xy[:, 0], xy[:, 1])
        F.append({"fid": fid, "pt": stem.split("_M")[1][0], "flux": flux,
                  "xyz_sip": unit(ra, dec), "xyz_sam": unit(ra5, dec5),
                  "kp": phot[fid]["k_photo"] if fid in phot else np.nan})
    def devs(keyw, same_only=True):
        out = []
        for a in range(len(F)):
            for b in range(a + 1, len(F)):
                if same_only and F[a]["pt"] != F[b]["pt"]: continue
                tr = cKDTree(F[b][keyw]); d, idx = tr.query(F[a][keyw], k=1)
                c = np.clip(np.sum(F[a][keyw] * F[b][keyw][idx], axis=1), -1, 1)
                sep = np.degrees(np.arccos(c)) * 3600
                ok = np.flatnonzero(sep < 1.0)
                if ok.size < 20: continue
                ratio = F[a]["flux"][ok] / F[b]["flux"][idx[ok]]
                med = np.median(ratio); keep = (ratio > med / 1.5) & (ratio < med * 1.5)
                out.append(np.log10(float(np.median(ratio[keep])) / (F[a]["kp"] / F[b]["kp"])))
        return np.array(out)
    d_sip_in = devs("xyz_sip", True); d_sam_in = devs("xyz_sam", True)
    d_sip_cr = devs("xyz_sip", False); d_sam_cr = devs("xyz_sam", False)
    def st(v):
        return None if v.size == 0 else {"n": int(v.size), "median": float(np.median(v)),
                                         "rms": float(np.sqrt(np.mean(v ** 2))),
                                         "p16": float(np.percentile(v, 16)), "p84": float(np.percentile(v, 84)),
                                         "se_median": float(1.4826 * np.median(np.abs(v - np.median(v))) / max(np.sqrt(v.size), 1))}
    res[tag] = {"within_sip": st(d_sip_in), "within_samples": st(d_sam_in),
                "cross_sip": st(d_sip_cr), "cross_samples": st(d_sam_cr), "n_frames": len(F)}
    print("[%s] 帧 %d" % (tag, len(F)))
    for lbl, v in (("组内·SIP(修正后)", d_sip_in), ("组内·samples(修正前)", d_sam_in),
                   ("跨组·SIP(修正后)", d_sip_cr), ("跨组·samples(修正前)", d_sam_cr)):
        if v.size == 0: print("   %-18s 无可比帧对" % lbl); continue
        print("   %-18s 对 %3d | log10(星比比/k_photo比) 中位 %+.4f dex | RMS %.4f dex | p16/p84 %+.4f/%+.4f | 中位SE %.4f" % (
            lbl, v.size, np.median(v), np.sqrt(np.mean(v ** 2)), np.percentile(v, 16), np.percentile(v, 84),
            1.4826 * np.median(np.abs(v - np.median(v))) / max(np.sqrt(v.size), 1)))
json.dump(res, open(f"{OUT}/p5mD_sip_kphoto.json", "w"), indent=1)
print("写入 results/p5w/p5mD_sip_kphoto.json")
