#!/usr/bin/env python3
"""p1_snr.json 生产盘点（FRAME-SNR-CANON, RELEASE-02）。

目的：**用真实产物反推生产实际执行了哪一条分支**，而不是靠读代码猜。

判据（先行写死）：
  C1  `snr_reference.snr_f == flux_adu / sigma_f_adu`（相对容差 1e-9）逐帧成立
      -> 落盘的就是**通量型 F_ref/sigma_F**（定案式 2.4）
  C2  **逐产品**（同一 output_dir）`reference_flux_adu` 组内相对偏差 == 0
      -> 组内公共 F_ref（定案要求）。注意：跨产品比较无意义（不同目标/夜/滤光片）
  C3  反推 `sigma_f_adu * sqrt(sum_p2(fwhm))` 是否等于 `p1_sources.json` 的 `noise_sigma`
      -> 若相等，则生产走的是 `snr_science.cpp:176-177` 的 **gain<=0 天空受限分支**（定案式 2.7）
  C4  `frame_depth_m5_mag` 非空计数（记录 `m_5` 是否真的产出）
  C5  `frame_snr` 字段是否**自称不是 SNR**（自文档化检查）

**易变性登记**：`run/` 是并行工作项共用的活目录，随时被重写/删除；
本脚本对**解析失败（并发写中）**的文件只登记不失败，并输出快照时间。

输出: run/reverse_verify/frame_snr/p1_snr_inventory.json
"""
import json
import math
import os
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(REPO, "run", "reverse_verify", "frame_snr", "p1_snr_inventory.json")


def moffat4_sum_p2(fwhm_px: float, half: int) -> float:
    """与生产 snr_science.cpp:41-84 同规则的离散归一化 Moffat4 的 sum P_i^2。"""
    sigma = fwhm_px / 1.230310
    alpha2 = 2.0 * sigma * sigma
    vals = []
    for j in range(-half, half + 1):
        for i in range(-half, half + 1):
            r2 = float(i * i + j * j)
            t = 1.0 + r2 / alpha2
            vals.append(1.0 / (t * t * t * t))
    s = sum(vals)
    return sum((v / s) ** 2 for v in vals)


def _agg(frames, fn):
    """对 frames 里所有有限的 snr_reference.snr_f 取 fn；空则 None（不抛）。"""
    vals = []
    for fr in frames or []:
        if not isinstance(fr, dict):
            continue
        v = (fr.get("snr_reference") or {}).get("snr_f")
        if isinstance(v, (int, float)) and math.isfinite(float(v)):
            vals.append(float(v))
    return fn(vals) if vals else None


def main() -> int:
    hits = []
    for root, _dirs, files in os.walk(os.path.join(REPO, "run")):
        for f in files:
            if f == "p1_snr.json":
                hits.append(os.path.join(root, f))
    hits.sort()

    recs, bad = [], []
    for p in hits:
        try:
            with open(p, encoding="utf-8") as fh:
                d = json.load(fh)
        except Exception as e:  # noqa: BLE001  （并发写中/半截文件）
            bad.append({"path": os.path.relpath(p, REPO), "error": str(e)[:120]})
            continue
        frames = [fr for fr in (d.get("frames") or []) if isinstance(fr, dict)]
        frefs, ident_ok, ident_tot, m5_nonnull, ref_frames = [], 0, 0, 0, 0
        fs_self_doc = None
        for fr in frames:
            sr = fr.get("snr_reference")
            if isinstance(sr, dict):
                ref_frames += 1
                fa, sf, sn = sr.get("flux_adu"), sr.get("sigma_f_adu"), sr.get("snr_f")
                if all(isinstance(x, (int, float)) for x in (fa, sf, sn)) and sf:
                    ident_tot += 1
                    frefs.append(float(fa))
                    if abs(sn - fa / sf) <= 1e-9 * max(1.0, abs(sn)):
                        ident_ok += 1
            if fr.get("frame_depth_m5_mag") is not None:
                m5_nonnull += 1
            fs = fr.get("frame_snr")
            if isinstance(fs, dict) and fs_self_doc is None:
                fs_self_doc = str(fs.get("definition", ""))[:200]
        rel = ((max(frefs) - min(frefs)) / max(frefs)) if frefs else None
        # snr_schema 是**帧级**键（不在顶层）
        schemas = sorted({str(fr.get("snr_schema")) for fr in frames if fr.get("snr_schema")})
        recs.append({
            "path": os.path.relpath(p, REPO),
            "snr_schema": (schemas[0] if len(schemas) == 1 else (schemas or None)),
            "snr_definition": next((str(fr.get("snr_definition")) for fr in frames
                                    if fr.get("snr_definition")), None),
            "schema_version": d.get("schema_version"),
            "n_frames": len(frames),
            "n_frames_with_snr_reference": ref_frames,
            "n_snr_identity_ok": ident_ok,
            "n_snr_identity_total": ident_tot,
            "n_m5_nonnull": m5_nonnull,
            "reference_flux_adu": (frefs[0] if frefs else None),
            "reference_flux_rel_dev_within_product": rel,
            "snr_f_min": _agg(frames, min),
            "snr_f_max": _agg(frames, max),
            "frame_snr_field_is_self_documented_not_snr": bool(
                fs_self_doc and "NOT a whole-frame scalar SNR" in fs_self_doc),
            "frame_snr_definition_string": fs_self_doc,
        })

    n_fr = sum(r["n_frames"] for r in recs)
    n_ref = sum(r["n_frames_with_snr_reference"] for r in recs)
    n_ok = sum(r["n_snr_identity_ok"] for r in recs)
    n_id = sum(r["n_snr_identity_total"] for r in recs)
    n_m5 = sum(r["n_m5_nonnull"] for r in recs)
    devs = [r["reference_flux_rel_dev_within_product"] for r in recs
            if isinstance(r["reference_flux_rel_dev_within_product"], float)]
    n_prod_group_common = sum(1 for dv in devs if dv == 0.0)

    # C3: 反推分支（取任一有 snr_reference 且能找到 p1_sources.json 的产品）
    c3 = {"status": "NOT_RUN", "reason": "未找到同时具备 snr_reference 与 p1_sources.json 的产品"}
    for r in sorted(recs, key=lambda x: -x["n_snr_identity_total"]):
        if not r["n_snr_identity_total"] or not r["reference_flux_adu"]:
            continue
        d = os.path.join(REPO, os.path.dirname(r["path"]))
        for cand in ("p1_sources.json", "p1_sources_phot.json"):
            sp = os.path.join(d, cand)
            if not os.path.exists(sp):
                continue
            try:
                with open(sp, encoding="utf-8") as fh:
                    sd = json.load(fh)
            except Exception:  # noqa: BLE001
                continue
            srcs = [s for s in (sd.get("sources") or sd.get("stars") or []) if isinstance(s, dict)]
            ns = sorted(float(s["noise_sigma"]) for s in srcs
                        if isinstance(s.get("noise_sigma"), (int, float)))
            fw = sorted(float(s["fwhm_px"]) for s in srcs
                        if isinstance(s.get("fwhm_px"), (int, float)))
            if not ns or not fw:
                continue
            fwhm = fw[len(fw) // 2]
            half = max(30, min(256, int(math.ceil(12.0 * fwhm))))
            sp2 = moffat4_sum_p2(fwhm, half)
            sig_f = r["reference_flux_adu"] / r["snr_f_min"]
            implied_sky = sig_f * math.sqrt(sp2)
            ns_med = ns[len(ns) // 2]
            c3 = {"status": "OK", "product": r["path"], "sources_file": cand,
                  "noise_sigma_median": ns_med, "fwhm_px_median": fwhm,
                  "grid_half": half, "sum_p2": sp2,
                  "implied_sigma_sky_adu": implied_sky,
                  "rel_diff_vs_noise_sigma": abs(implied_sky - ns_med) / ns_med}
            break
        if c3["status"] == "OK":
            break

    res = {
        "snapshot_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "volatility_note": "run/ 是并行工作项共用的活目录；本快照随时可能过期。",
        "n_products": len(recs),
        "n_products_unparseable_concurrent_write": len(bad),
        "unparseable": bad,
        "n_frames_total": n_fr,
        "n_frames_with_snr_reference": n_ref,
        "C1_snr_identity_flux_over_sigma_frames_ok": n_ok,
        "C1_snr_identity_frames_total": n_id,
        "C2_products_with_group_common_reference_flux": n_prod_group_common,
        "C2_products_with_reference_flux": len(devs),
        "C2_max_reference_flux_rel_dev_within_product": (max(devs) if devs else None),
        "C3_branch_inference": c3,
        "C4_m5_nonnull_frames": n_m5,
        "C5_n_products_frame_snr_self_documented_not_snr":
            sum(1 for r in recs if r["frame_snr_field_is_self_documented_not_snr"]),
        "products": recs,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=2)

    print("快照时间(UTC): %s" % res["snapshot_utc"])
    print("p1_snr.json 产品数: %d（解析失败/并发写中 %d）" % (len(recs), len(bad)))
    print("帧数 %d，其中带 snr_reference 的 %d" % (n_fr, n_ref))
    print("C1  snr_f == flux_adu/sigma_f_adu : %d/%d 帧" % (n_ok, n_id))
    print("C2  逐产品 F_ref 组内相对偏差 == 0 的产品数: %d/%d（最大 %s）"
          % (n_prod_group_common, len(devs),
             res["C2_max_reference_flux_rel_dev_within_product"]))
    print("C3  分支反推: %s" % json.dumps(c3, ensure_ascii=False))
    print("C4  frame_depth_m5_mag 非空帧数: %d/%d" % (n_m5, n_fr))
    print("C5  自称『不是帧级 SNR』的 frame_snr 字段产品数: %d/%d"
          % (res["C5_n_products_frame_snr_self_documented_not_snr"], len(recs)))
    print("written:", os.path.relpath(OUT, REPO))

    ok = (n_id > 0 and n_ok == n_id and len(devs) > 0 and n_prod_group_common == len(devs))
    print("VERDICT:", "PASS" if ok else "FAIL")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
