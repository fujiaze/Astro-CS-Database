"""§13.1 天区断言：逐帧从**该帧产品自身**读几何，与参照产品对照。

帧集来自 run/PHOT-FIX/p1_rerun/{t2,t3}/p1_products.json（帧产品清单），
平面产物文件名取自清单条目的 input_light 基名（清单驱动，不按目录名挑）。
"""
import glob, json, os, sys
import numpy as np
from astropy.io import fits

ROOT = "/workspace/Astro CS Database"
PDIR = os.path.join(ROOT, "run/PHOT-FIX/p1_rerun")
REF = os.path.join(ROOT, "run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits")
TOL = 0.05

def angsep(a1, d1, a2, d2):
    a1, d1, a2, d2 = map(np.radians, (a1, d1, a2, d2))
    return float(np.degrees(np.arccos(np.clip(
        np.sin(d1)*np.sin(d2) + np.cos(d1)*np.cos(d2)*np.cos(a1-a2), -1, 1))))

h = fits.getheader(REF)
nx, ny = int(h["NAXIS1"]), int(h["NAXIS2"])
half_diag = 0.5*np.hypot(nx*abs(h["CD1_1"]), ny*abs(h["CD2_2"]))
limit = half_diag + TOL
ref = {"crval": [float(h["CRVAL1"]), float(h["CRVAL2"])], "half_diag_deg": float(half_diag),
       "limit_deg": float(limit), "nx": nx, "ny": ny,
       "nan_frac": float(np.mean(~np.isfinite(fits.getdata(REF))))}

rows, n_in, n_out, n_nogeo = [], 0, 0, 0
for tag in ("t2", "t3"):
    man = json.load(open(os.path.join(PDIR, tag, "p1_products.json")))
    for e in man["frames"]:
        stem = os.path.basename(e["input_light"]).replace(".fts", "")
        fdir = os.path.join(PDIR, tag, os.path.basename(e["hips_path"]))
        row = {"tag": tag, "frame_id": e["frame_id"], "input_light": os.path.basename(e["input_light"]),
               "hips_path": e["hips_path"], "final": e.get("final"),
               "n_tiles": e.get("n_tiles"), "nside": e.get("nside")}
        geo = None
        for kind in ("calibrated", "cleaned", "photoapplied"):
            p = os.path.join(PDIR, tag, f"{kind}_{stem}.fts")
            if os.path.exists(p):
                hh = fits.getheader(p)
                row["plane_product"] = os.path.basename(p)
                row["plane_header_keys"] = len(hh)
                if all(k in hh for k in ("CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2")):
                    geo = {"source": "plane_product_header", "crval": [float(hh["CRVAL1"]), float(hh["CRVAL2"])],
                           "crpix": [float(hh["CRPIX1"]), float(hh["CRPIX2"])],
                           "cdelt_arcsec": [float(hh.get("CDELT1", np.nan))*3600, float(hh.get("CDELT2", np.nan))*3600],
                           "ctype": [str(hh.get("CTYPE1")), str(hh.get("CTYPE2"))],
                           "pltsolvd": bool(hh.get("PLTSOLVD", False))}
                break
        if geo is None:
            wj = os.path.join(fdir, "p1_wcs.json")
            if os.path.exists(wj):
                w = json.load(open(wj)); s = w.get("samples") or []
                geo = {"source": "p1_wcs.json", "crval": None, "p1_wcs": {
                    "exists": True, "ctype": w.get("ctype1"), "sip_order": w.get("sip_order"),
                    "n_pairs": w.get("n_pairs"), "best_inliers": w.get("best_inliers"),
                    "rms_px": w.get("rms_px"), "rms_arcsec": w.get("rms_arcsec"), "n_samples": len(s),
                    "sample0_radec": [s[0].get("ra"), s[0].get("dec")] if s else None}}
            else:
                geo = {"source": None, "p1_wcs": {"exists": False}}
        row["geometry"] = geo
        if geo.get("crval"):
            row["sep_from_ref_deg"] = angsep(geo["crval"][0], geo["crval"][1], ref["crval"][0], ref["crval"][1])
            row["in_reference_field"] = bool(row["sep_from_ref_deg"] <= limit)
            n_in += row["in_reference_field"]; n_out += (not row["in_reference_field"])
        else:
            row["sep_from_ref_deg"] = None; row["in_reference_field"] = None; n_nogeo += 1
        rows.append(row)
out = {"manifests": {"t2": os.path.join(PDIR, "t2/p1_products.json"), "t3": os.path.join(PDIR, "t3/p1_products.json"),
                     "acsd_run": sorted(glob.glob(os.path.join(PDIR, "*/acsd_run_*.json"))),
                     "entries": {"t2": 16, "t3": 33, "total": len(rows)}},
       "reference": ref,
       "assertion": {"tolerance_deg": TOL, "n_frames": len(rows), "n_in_field": n_in,
                     "n_out_of_field": n_out, "n_no_header_geometry": n_nogeo,
                     "pass": bool(n_out == 0)},
       "frames": rows}
dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "实验/P5-signal-extraction/results/p5w/geometry_assertion.json")
json.dump(out, open(dst, "w"), indent=1, ensure_ascii=False)
seps = [r["sep_from_ref_deg"] for r in rows if r["sep_from_ref_deg"] is not None]
print("帧数 %d（清单 t2=16 + t3=33）| 有头几何 %d | 只有 p1_wcs.json %d | 视场外 %d" % (
    len(rows), len(seps), n_nogeo, n_out))
if seps:
    print("有头几何帧：到参照中心角距 min %.3f° max %.3f°（限 %.3f°）" % (min(seps), max(seps), limit))
print("断言:", "通过" if out["assertion"]["pass"] else "**失败**")
print("写入", dst)
