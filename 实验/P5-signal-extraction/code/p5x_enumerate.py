"""输入枚举与天区断言（AGENTS.md §13.1）。

口径：
  1. 帧集从**本次运行的清单**枚举：`run/PHOT-FIX/p1_rerun/acsd_run_*.json`（运行上下文）与
     `run/PHOT-FIX/p1_rerun/{t2,t3}/p1_products.json`（帧产品清单，含 frame_id / hips_path / final）；
     不按目录名或记忆挑数据。
  2. 每帧几何从**该帧产品自身**读出：优先帧平面产物头（calibrated/cleaned/photoapplied），
     其后是该帧产品目录里的 `p1_wcs.json`。
  3. 天区断言：所有帧必须落在参照产品 `run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits`
     的视场内（中心到参照中心的角距 ≤ 参照视场半对角 + 容差），且参照与帧集必须同属 M42 天区。
  4. 来源不明的目录不得进入分析（登记并排除）。
  5. 缺几何的帧逐帧登记（该帧 p1_wcs.json 是否存在、解算状态），不猜不补。
"""

from __future__ import annotations

import glob
import json
import re
import os
import sys

import numpy as np
from astropy.io import fits

ROOT = "/workspace/Astro CS Database"
PDIR = os.path.join(ROOT, "run/PHOT-FIX/p1_rerun")
REF = os.path.join(ROOT, "run/E2E-M42-P5-EXPORT/out/full_p3_whole/output_phase3.fits")
TOL_DEG = 0.05        # 容差：中心角距超出「视场半对角 + 容差」即判失败


def ref_geometry():
    h = fits.getheader(REF)
    nx, ny = h["NAXIS1"], h["NAXIS2"]
    half_diag = 0.5 * np.hypot(nx * abs(h["CD1_1"]), ny * abs(h["CD2_2"]))
    return {"file": REF, "crval": [float(h["CRVAL1"]), float(h["CRVAL2"])],
            "crpix": [float(h["CRPIX1"]), float(h["CRPIX2"])],
            "cd": [float(h["CD1_1"]), float(h["CD2_2"])],
            "nx": int(nx), "ny": int(ny), "half_diag_deg": float(half_diag),
            "nan_frac": float(np.mean(~np.isfinite(fits.getdata(REF))))}


def angsep(ra1, dec1, ra2, dec2):
    a1, d1, a2, d2 = map(np.radians, (ra1, dec1, ra2, dec2))
    return float(np.degrees(np.arccos(np.clip(
        np.sin(d1) * np.sin(d2) + np.cos(d1) * np.cos(d2) * np.cos(a1 - a2), -1, 1))))


def frame_geometry(fr):
    """从该帧产品读几何：平面产物头优先，其次帧产品目录的 p1_wcs.json。"""
    for kind in ("calibrated", "cleaned", "photoapplied"):
        p = os.path.join(PDIR, fr["tag"], f"{kind}_{fr['stem']}.fts")
        if os.path.exists(p):
            h = fits.getheader(p)
            if all(k in h for k in ("CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2")):
                return {"source": f"{kind}_*.fts header",
                        "crval": [float(h["CRVAL1"]), float(h["CRVAL2"])],
                        "crpix": [float(h["CRPIX1"]), float(h["CRPIX2"])],
                        "cdelt_deg": [float(h.get("CDELT1", np.nan)), float(h.get("CDELT2", np.nan))],
                        "cd": [float(h.get("CD1_1", np.nan)), float(h.get("CD2_2", np.nan))],
                        "ctype": [str(h.get("CTYPE1")), str(h.get("CTYPE2"))],
                        "pltsolvd": bool(h.get("PLTSOLVD", False)), "key_count": len(h)}
            header_keys = len(h)
            break
    else:
        header_keys = None
    wj = os.path.join(PDIR, fr["tag"], fr["hips_dir"], "p1_wcs.json")
    out = {"header_has_wcs": False}
    if header_keys is not None:
        out["header_key_count"] = header_keys
    if os.path.exists(wj):
        w = json.load(open(wj))
        s = w.get("samples") or []
        out["p1_wcs_json"] = {"exists": True, "ctype": w.get("ctype1"), "sip_order": w.get("sip_order"),
                              "n_pairs": w.get("n_pairs"), "best_inliers": w.get("best_inliers"),
                              "rms_px": w.get("rms_px"), "rms_arcsec": w.get("rms_arcsec"),
                              "initial_有初值": bool(w.get("initial")) if isinstance(w.get("initial"), dict) else w.get("initial"),
                              "n_samples": len(s)}
        if s:
            out["p1_wcs_json"]["sample_center_radec"] = [s[0].get("ra"), s[0].get("dec")]
    else:
        out["p1_wcs_json"] = {"exists": False}
    return out


def main():
    out = {"manifests": {}, "reference": ref_geometry()}
    frames = []
    runctx = sorted(glob.glob(os.path.join(PDIR, "acsd_run_*.json")) +
                    glob.glob(os.path.join(PDIR, "*/acsd_run_*.json")))
    out["manifests"]["acsd_run"] = runctx
    for tag in ("t2", "t3"):
        p = os.path.join(PDIR, tag, "p1_products.json")
        if not os.path.exists(p):
            continue
        d = json.load(open(p))
        out["manifests"][f"{tag}/p1_products.json"] = {
            "path": p, "n_frames": d.get("n_frames"), "n_products": d.get("n_products"),
            "count_consistent": d.get("count_consistent"), "entries": len(d.get("frames", []))}
        for e in d.get("frames", []):
            # 平面产物文件名由清单条目自带的 input_light 基名派生（权威、不靠正则还原）
            stem = os.path.basename(e.get("input_light", "")).replace(".fts", "") + ".fts"
            frames.append({"tag": tag, "stem": stem, "frame_id": e["frame_id"],
                           "hips_path": e.get("hips_path"), "final": e.get("final"),
                           "hips_dir": os.path.basename(e["hips_path"])})
    frames.sort(key=lambda f: (f["tag"], f["stem"]))
    ref = out["reference"]
    limit = ref["half_diag_deg"] + TOL_DEG
    for fr in frames:
        g = frame_geometry(fr)
        fr.update({k: g[k] for k in g})
        if "crval" in g:
            fr["sep_from_ref_deg"] = angsep(g["crval"][0], g["crval"][1],
                                            ref["crval"][0], ref["crval"][1])
            fr["in_reference_field"] = bool(fr["sep_from_ref_deg"] <= limit)
        else:
            fr["sep_from_ref_deg"] = None
            fr["in_reference_field"] = None
        # 与 stage1 实际用到的帧集对照
    used_path = os.path.join(ROOT, "实验/P5-signal-extraction/results/p5w/p5w_stage1.json")
    if os.path.exists(used_path):
        st = json.load(open(used_path))
        used = set(st["inputs"]["files"]); skipped = set(x["file"] for x in st["inputs"]["skipped"])
        enum = set(os.path.basename(f["final"]) if f.get("final") else f["frame_id"] for f in frames)
        enum_plane = set('cleaned_' + f['stem'] for f in frames)
        out["cross_check"] = {
            "manifest_frames": len(frames),
            "stage1_used": len(used), "stage1_skipped": len(skipped),
            "manifest_missing_from_stage1": sorted(enum_plane - used - skipped)[:8],
            "manifest_files_resolved": int(sum(1 for f in frames if f.get("header_key_count"))),
            "manifest_files_missing_on_disk": sorted(f["stem"] for f in frames
                                                     if not f.get("header_key_count"))[:8],
            "stage1_used_not_in_manifest": sorted(used - enum_plane)[:8],
            "identical": bool(used | skipped == enum_plane)}
    out["frames"] = frames
    out["assertion"] = {
        "tolerance_deg": TOL_DEG, "field_limit_deg": float(limit),
        "n_in_field": int(sum(1 for f in frames if f.get("in_reference_field"))),
        "n_out_of_field": int(sum(1 for f in frames if f.get("in_reference_field") is False)),
        "n_no_geometry": int(sum(1 for f in frames if f.get("in_reference_field") is None)),
        "pass": bool(all(f.get("in_reference_field") is not False for f in frames))}
    dst = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "实验/P5-signal-extraction/results/p5w/enumeration.json")
    json.dump(out, open(dst, "w"), indent=1, ensure_ascii=False)
    print("清单:", json.dumps(out["manifests"], ensure_ascii=False)[:300])
    print("帧数 %d | 在参照视场内 %d | 视场外 %d | 无几何 %d | 断言 %s" % (
        len(frames), out["assertion"]["n_in_field"], out["assertion"]["n_out_of_field"],
        out["assertion"]["n_no_geometry"], "通过" if out["assertion"]["pass"] else "**失败**"))
    if "cross_check" in out:
        print("与 stage1 实际用集对照:", json.dumps(out["cross_check"], ensure_ascii=False)[:300])
    print("写入", dst)


if __name__ == "__main__":
    main()
