#!/usr/bin/env python3
"""Issue #92 —— 「解算产物 WCS」→「star_matcher 真正消费的投影对象」逐字段对照。

数据来源:
  · 生产者: <run>/*/p1_wcs.json 的 "wcs" 对象 (p1_op_wcs 写入)
  · 消费者: module_adapters.cpp:6252-6308 读取 → FramePhotFitRequest →
            pc_api.cpp:run_with_gaia_impl → pc::WcsTransform(构造)
            (frame_photometry_fit.cpp:266 调 pc_calibrate_simple_with_gaia_f64_v2_qf)
  · 运行期实证: 真实 CLI stderr 里的 `[wcs_transform] 初始化:` 行 (消费者自己打印的构造函数入参)
"""
from __future__ import annotations

import argparse
import json
import re


def parse_init_line(stderr_path: str):
    "取 star_matcher 实际构造 WcsTransform 时打印的入参行。"
    pat = re.compile(
        r"\[wcs_transform\] 初始化: CRVAL=\(([-0-9.]+),([-0-9.]+)\), "
        r"CRPIX=\(([-0-9.]+),([-0-9.]+)\), "
        r"CD=\[([-0-9.eE+]+),([-0-9.eE+]+);([-0-9.eE+]+),([-0-9.eE+]+)\], "
        r"det=([-0-9.eE+]+), sip_order=(\d+)")
    rows = []
    with open(stderr_path, errors="ignore") as f:
        for line in f:
            m = pat.search(line)
            if m:
                rows.append({
                    "crval1": float(m.group(1)), "crval2": float(m.group(2)),
                    "crpix1": float(m.group(3)), "crpix2": float(m.group(4)),
                    "cd11": float(m.group(5)), "cd12": float(m.group(6)),
                    "cd21": float(m.group(7)), "cd22": float(m.group(8)),
                    "det": float(m.group(9)), "sip_order": int(m.group(10)),
                })
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--p1wcs", required=True)
    ap.add_argument("--stderr", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    d = json.load(open(a.p1wcs))
    w = d["wcs"]
    sip = w.get("sip", {})
    inits = parse_init_line(a.stderr)
    # 找与本源同一 CRVAL 的消费者构造行 (tol 1e-4 deg)
    match = None
    for r in inits:
        if abs(r["crval1"] - w["crval1"]) < 1e-4 and abs(r["crval2"] - w["crval2"]) < 1e-4:
            match = r
            break

    def cmp_field(name, prod, cons, tol):
        if cons is None:
            return {"field": name, "producer": prod, "consumer_log": None,
                    "match": None, "note": "consumer construction row not found"}
        ok = abs(prod - cons) <= tol
        return {"field": name, "producer": prod, "consumer_log": cons,
                "abs_diff": abs(prod - cons), "tolerance": tol, "match": ok}

    fields = []
    if match:
        # 容差 = 构造函数打印格式的量化步长 (CRVAL %.6f / CRPIX %.3f / CD %.3e),
        # 不是数值不一致 —— 消费者日志本身只印 4-7 位有效数字。
        fields = [
            cmp_field("crpix1", w["crpix1"], match["crpix1"], 5e-4),
            cmp_field("crpix2", w["crpix2"], match["crpix2"], 5e-4),
            cmp_field("crval1", w["crval1"], match["crval1"], 5e-7),
            cmp_field("crval2", w["crval2"], match["crval2"], 5e-7),
            cmp_field("cd11", w["cd11"], match["cd11"], 1e-7),
            cmp_field("cd12", w["cd12"], match["cd12"], 1e-7),
            cmp_field("cd21", w["cd21"], match["cd21"], 1e-7),
            cmp_field("cd22", w["cd22"], match["cd22"], 1e-7),
            cmp_field("sip_order", sip.get("order", 0), match["sip_order"], 0),
        ]

    def fnum(x, n=17):
        return format(float(x), f".{n}g")

    out = {
        "producer_file": a.p1wcs,
        "pixel_origin": {
            "producer_pixel_origin_field": d.get("pixel_origin"),
            "fits_pixel_origin": d.get("fits_pixel_origin"),
            "consumer_convention": "wcs_transform.h:10-12: CRPIX 1-based (FITS); 像素坐标 0-based; 天球度",
            "consumer_code": "wcs_transform.cpp:192-193  dx = x - (crpix1 - 1.0)",
            "vision": "生产者 samples[].x/y 为 0-based (0..4095); 消费者 sky→pixel 输出 0-based; "
                      "Astropy(all_pix2world(...,origin=0)) 复现 samples 至 1e-10 arcsec ⇒ 基准一致",
        },
        "image_size": {"width": 4096, "height": 4096,
                       "consumer": "pc_api.cpp:836 matchWithKdTree(..., width, height); "
                                   "module_adapters.cpp:6298 freq.width=fim.w()"},
        "icrs_units": {"producer": "degrees (J2000)", "consumer": "degrees (D2R in wcs_transform.cpp:19)"},
        "sip_arrays": {
            "order": sip.get("order"), "ap_order": sip.get("ap_order"),
            "a0": (sip.get("a") or [None])[0], "b0": (sip.get("b") or [None])[0],
            "ap0": (sip.get("ap") or [None])[0], "bp0": (sip.get("bp") or [None])[0],
            "consumer_note": "module_adapters.cpp:6307-6308 只把 sip.order 传为 freq.sip_order; "
                             "ap_order 不进入消费者。WcsTransform 只有一个 m_sip_order, "
                             "AP/BP 也用 sip_order 求值 (wcs_transform.cpp:235-236) ⇒ AP 的 4/5 阶被截断。",
        },
        "field_by_field": fields,
        "all_fields_match": all(f.get("match") for f in fields) if fields else None,
        "consumer_init_rows_found": len(inits),
        "consumer_init_row_matched": match,
        "consumer_construction_log_fields": ["CRVAL", "CRPIX", "CD", "det", "sip_order"],
        "consumer_construction_log_missing": ["sip a/b/ap/bp (构造函数不打印系数)"],
    }
    json.dump(out, open(a.out, "w"), indent=2, ensure_ascii=False)
    print(json.dumps(out["field_by_field"], indent=2, ensure_ascii=False))
    print("all_fields_match:", out["all_fields_match"],
          "| consumer rows:", len(inits), "| matched:", bool(match))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
