# -*- coding: utf-8 -*-
"""S04 STEP-1 · 合规选帧（AGENTS.md §13.1）

口径
----
1. **帧集从清单枚举**：`实验/shared/data_catalog.json`（S00 落地，base_sha 记于文件内），
   不按目录名/文件名子串猜。
2. **几何取自产品自身**：逐候选帧打开 FITS 头，读 `CRVAL/CRPIX/CD/CTYPE`。
3. **天区一致性断言**：同 `(dataset, FILTER, EXPTIME)` 内按 CRVAL 聚类；与参照帧球面距
   超过容差的帧判**输入不合格**并逐帧报出，不进入分析。
4. **缺几何的帧单独登记**（不猜补）。

输出：results/frame_inventory.json、results/frame_inventory.csv
"""
from __future__ import annotations

import csv
import hashlib
import json
import os
import sys
import time

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import s04_common as C  # noqa: E402

TOL_DEG = 0.01          # 天区一致性容差（36″；M42 同一 mosaic panel 的实际指向散布 ≤0.004°）


def main():
    t0 = time.perf_counter()
    cat = C.catalog_frames()
    frames = cat["frames"]
    out = dict(
        step="S04-1 frame inventory",
        base_sha=cat.get("base_sha"),
        catalog=dict(path=os.path.relpath(C.CATALOG, C.REPO),
                     sha256=C.sha256_file(C.CATALOG),
                     generated_at=cat.get("generated_at"),
                     schema=cat.get("$schema")),
        tolerance_deg=TOL_DEG,
        n_catalog_frames=len(frames),
        n_with_geometry=int(sum(1 for f in frames if f.get("wcs_center_deg"))),
        n_missing_geometry=int(sum(1 for f in frames if not f.get("wcs_center_deg"))),
    )

    # ---- 缺几何帧登记（不猜补）----
    missing = [dict(frame_id=f["frame_id"], location=f["location"],
                    dataset=f["dataset"], filter=f.get("filter"),
                    exposure_s=f.get("exposure_s"), bitpix=f.get("bitpix"),
                    bzero=f.get("bzero"), bscale=f.get("bscale"))
               for f in frames if not f.get("wcs_center_deg")]
    out["frames_missing_geometry"] = dict(
        n=len(missing),
        note="缺 CRVAL/CRPIX 的帧按 §13.1「缺几何的帧单独登记」处理，不猜补、不进入分析",
        frames=missing)

    # ---- 参照帧：M42 数据集中首个 basename 含 M42/M1/T2/Red 的帧（与 step8 原口径同源，
    #      但此处是从清单枚举得到，而非 os.walk 猜测）----
    m42 = [f for f in frames if f["dataset"].startswith("M42")]
    ref = None
    for f in sorted(m42, key=lambda x: x["frame_id"]):
        b = os.path.basename(f["location"])
        if "M42" in b and "M1" in b and "T2" in b and "Red" in b:
            ref = f
            break
    assert ref is not None, "M42 M1 T2 Red 参照帧未在清单中枚举到"
    ref_path = os.path.join(C.REPO, ref["location"])
    ref_geom = C.read_geom(ref_path)
    ref_crval = [ref_geom["CRVAL1"], ref_geom["CRVAL2"]]
    out["reference_frame"] = dict(
        frame_id=ref["frame_id"], location=ref["location"],
        catalog_sha256=ref["file_sha256"], recomputed_sha256=C.sha256_file(ref_path),
        catalog_crval=ref.get("wcs_center_deg"), header_crval=ref_crval,
        sep_catalog_vs_header_deg=C.angular_sep_deg(*(ref.get("wcs_center_deg") or [0, 0]),
                                                    *ref_crval),
        filter=ref_geom["FILTER"], exptime=ref_geom["EXPTIME"],
        observer=ref_geom["OBSERVER"], date_obs=ref_geom["DATE-OBS"],
        pixel_scale_arcsec=ref_geom["pixel_scale_arcsec"],
        fov_deg=ref_geom["pixel_scale_arcsec"] * ref_geom["NAXIS1"] / 3600.0)

    # ---- 候选：同 dataset / FILTER / EXPTIME 且几何齐全 ----
    cands = []
    for f in m42:
        if not f.get("wcs_center_deg"):
            continue
        if f.get("filter") != ref_geom["FILTER"]:
            continue
        if abs(float(f.get("exposure_s", -1)) - float(ref_geom["EXPTIME"])) > 1e-6:
            continue
        p = os.path.join(C.REPO, f["location"])
        geom = C.read_geom(p)
        if geom["n_present"] != 8:
            cands.append(dict(frame_id=f["frame_id"], location=f["location"],
                              status="REJECT_MISSING_GEOMETRY", missing=geom["missing"]))
            continue
        sep = C.angular_sep_deg(geom["CRVAL1"], geom["CRVAL2"], *ref_crval)
        cands.append(dict(
            frame_id=f["frame_id"], location=f["location"],
            catalog_sha256=f["file_sha256"],
            catalog_crval=f.get("wcs_center_deg"), header_crval=[geom["CRVAL1"], geom["CRVAL2"]],
            sep_catalog_vs_header_deg=C.angular_sep_deg(*(f.get("wcs_center_deg") or [0, 0]),
                                                        geom["CRVAL1"], geom["CRVAL2"]),
            sep_from_reference_deg=sep,
            observer=geom["OBSERVER"], telescop=geom["TELESCOP"], instrument=geom["INSTRUME"],
            filter=geom["FILTER"], exptime=geom["EXPTIME"], date_obs=geom["DATE-OBS"],
            crpix=[geom["CRPIX1"], geom["CRPIX2"]],
            cd=[[geom["CD1_1"], geom["CD1_2"]], [geom["CD2_1"], geom["CD2_2"]]],
            pixel_scale_arcsec=geom["pixel_scale_arcsec"],
            rotation_deg=float(np.degrees(np.arctan2(geom["CD1_2"], geom["CD1_1"]))),
            status="COMPLIANT" if sep <= TOL_DEG else "REJECT_CRVAL_MISMATCH"))

    # ---- 断言失败即报出 ----
    compliant = [c for c in cands if c["status"] == "COMPLIANT"]
    rejected = [c for c in cands if c["status"] != "COMPLIANT"]
    # 同一 mosaic 内其它 panel（M2..M6）也会被枚举到，用以演示断言确实能翻红
    for c in compliant:
        c["sha256_recomputed"] = C.sha256_file(os.path.join(C.REPO, c["location"]))
        c["sha256_catalog_match"] = c["sha256_recomputed"] == c["catalog_sha256"]
        c["geom_catalog_match"] = bool(c["sep_catalog_vs_header_deg"] < 1e-9)

    out["candidates_same_config"] = cands
    out["compliant_set"] = compliant
    out["rejected_set"] = rejected
    out["compliant_frame_ids"] = [c["frame_id"] for c in compliant]
    out["assertions"] = dict(
        n_candidates=len(cands), n_compliant=len(compliant), n_rejected=len(rejected),
        all_compliant_within_tol=bool(all(c["sep_from_reference_deg"] <= TOL_DEG
                                          for c in compliant)),
        all_catalog_sha256_match=bool(all(c["sha256_catalog_match"] for c in compliant)),
        all_catalog_geom_match=bool(all(c["geom_catalog_match"] for c in compliant)),
        max_sep_within_compliant_deg=float(max(c["sep_from_reference_deg"]
                                               for c in compliant)),
        min_sep_among_rejected_deg=float(min((c["sep_from_reference_deg"] for c in rejected
                                              if "sep_from_reference_deg" in c),
                                             default=float("nan"))))
    # 分组：同配置（同 OBSERVER）与跨仪器同天区
    obs = sorted({c["observer"] for c in compliant})
    out["subsets"] = {
        "same_config_T2": [c["frame_id"] for c in compliant if c["observer"] == "Telescope2"],
        "cross_instrument_T3": [c["frame_id"] for c in compliant
                                if c["observer"] == "Telescope3"],
        "observers": obs,
    }
    out["wall_time_s"] = float(time.perf_counter() - t0)
    p = C.jdump(out, "frame_inventory.json")

    # ---- CSV（人可读）----
    csvp = os.path.join(C.RESULTS, "frame_inventory.csv")
    with open(csvp, "w", newline="", encoding="utf-8") as f:
        wtr = csv.writer(f)
        wtr.writerow(["status", "frame_id", "observer", "filter", "exptime",
                      "crval1", "crval2", "sep_from_ref_deg", "sha256",
                      "sha256_catalog_match", "date_obs"])
        for c in sorted(cands, key=lambda x: (x["status"], x["frame_id"])):
            hc = c.get("header_crval") or [None, None]
            wtr.writerow([c["status"], c["frame_id"], c.get("observer"), c.get("filter"),
                          c.get("exptime"), hc[0], hc[1],
                          c.get("sep_from_reference_deg"), c.get("catalog_sha256"),
                          c.get("sha256_catalog_match"), c.get("date_obs")])
        for m in missing:
            wtr.writerow(["REJECT_MISSING_WCS", m["frame_id"], None, m["filter"],
                          m["exposure_s"], None, None, None, None, None, None])

    print(f"[inventory] ref = {ref['frame_id']}")
    print(f"[inventory] compliant n={len(compliant)} rejected n={len(rejected)}")
    for c in compliant:
        print(f"   {c['observer']:11s} {c['date_obs']}  sep={c['sep_from_reference_deg']:.6f}°"
              f"  crval={c['header_crval']}  sha_match={c['sha256_catalog_match']}")
    print(f"[inventory] wrote {os.path.relpath(p, C.REPO)} and "
          f"{os.path.relpath(csvp, C.REPO)}")


if __name__ == "__main__":
    main()
