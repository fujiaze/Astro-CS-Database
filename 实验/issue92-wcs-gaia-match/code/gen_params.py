#!/usr/bin/env python3
"""Issue #92 —— 从真实 p1_wcs.json 生成 C++ 探针参数头 `params_issue92.inc`。

只读取产品字段, 不改任何数值; 消费口径与 `module_adapters.cpp:6303-6308`
(p1_op_photometry 送入 FramePhotFitRequest 的字段) 逐项一致。
"""
from __future__ import annotations

import argparse
import json
import os


def arr36(name: str, vals) -> str:
    vals = list(vals) + [0.0] * (36 - len(vals))
    vals = vals[:36]
    body = ", ".join(repr(float(v)) for v in vals)
    return f"static const double P_{name}[36] = {{{body}}};"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("p1_wcs", help="真实 p1_wcs.json (含 wcs.sip)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--json-out", default="", help="同时把消费字段落成 JSON 便于对照")
    a = ap.parse_args()

    d = json.load(open(a.p1_wcs))
    w = d["wcs"]
    sip = w.get("sip") or {}
    order = int(sip.get("order", 0))
    ap_order = int(sip.get("ap_order", 0))
    a_arr = sip.get("a") or []
    b_arr = sip.get("b") or []
    ap_arr = sip.get("ap") or []
    bp_arr = sip.get("bp") or []

    lines = [
        "// Issue #92 自动生成: 真实 p1_wcs.json 的天测字段, 勿手改。",
        f'// source: {os.path.basename(a.p1_wcs)}  schema={d.get("schema")} solver={d.get("solver")}',
        f"static const char* P_SRC = {json.dumps(a.p1_wcs)};",
        f"static const double P_CRPIX1 = {repr(float(w['crpix1']))};",
        f"static const double P_CRPIX2 = {repr(float(w['crpix2']))};",
        f"static const double P_CRVAL1 = {repr(float(w['crval1']))};",
        f"static const double P_CRVAL2 = {repr(float(w['crval2']))};",
        f"static const double P_CD11 = {repr(float(w['cd11']))};",
        f"static const double P_CD12 = {repr(float(w['cd12']))};",
        f"static const double P_CD21 = {repr(float(w['cd21']))};",
        f"static const double P_CD22 = {repr(float(w['cd22']))};",
        f'static const char* P_CTYPE1 = {json.dumps(w.get("ctype1", ""))};',
        f'static const char* P_CTYPE2 = {json.dumps(w.get("ctype2", ""))};',
        f"static const int P_ORDER = {order};",
        f"static const int P_AP_ORDER = {ap_order};",
        arr36("A", a_arr),
        arr36("B", b_arr),
        arr36("AP", ap_arr),
        arr36("BP", bp_arr),
        "static const int P_W = 4096;",
        "static const int P_H = 4096;",
        f"static const int P_NPAIRS = {int(d.get('n_pairs', -1))};",
        f"static const double P_RMS_ARCSEC = {float(d.get('rms_arcsec', 0.0))};",
        f"static const double P_MAX_ROUNDTRIP_PX = {float(d.get('max_roundtrip_px', 0.0))};",
        f"static const double P_MAX_SIP_EFFECT_DEG = {float(d.get('max_sip_effect_deg', 0.0))};",
        f"static const double P_MAX_FORWARD_CROSS_DEG = {float(d.get('max_forward_cross_deg', 0.0))};",
        f"static const int P_NSAMPLES = {int(d.get('n_samples', 0))};",
    ]
    with open(a.out, "w") as f:
        f.write("\n".join(lines) + "\n")

    if a.json_out:
        consumed = {
            "source_file": a.p1_wcs,
            "crpix1": w["crpix1"], "crpix2": w["crpix2"],
            "crval1": w["crval1"], "crval2": w["crval2"],
            "cd11": w["cd11"], "cd12": w["cd12"],
            "cd21": w["cd21"], "cd22": w["cd22"],
            "ctype1": w.get("ctype1"), "ctype2": w.get("ctype2"),
            "sip_order": order, "sip_ap_order": ap_order,
            "sip_a": a_arr, "sip_b": b_arr, "sip_ap": ap_arr, "sip_bp": bp_arr,
            "sip_a0": a_arr[0] if a_arr else None,
            "sip_b0": b_arr[0] if b_arr else None,
            "sip_ap0": ap_arr[0] if ap_arr else None,
            "sip_bp0": bp_arr[0] if bp_arr else None,
            "wcs_pixel_origin_field": d.get("pixel_origin"),
            "fits_pixel_origin": d.get("fits_pixel_origin"),
            "n_pairs": d.get("n_pairs"), "rms_arcsec": d.get("rms_arcsec"),
            "max_roundtrip_px": d.get("max_roundtrip_px"),
            "max_sip_effect_deg": d.get("max_sip_effect_deg"),
            "max_forward_cross_deg": d.get("max_forward_cross_deg"),
        }
        json.dump(consumed, open(a.json_out, "w"), indent=2, ensure_ascii=False)
    print("wrote", a.out, "order", order, "ap_order", ap_order)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
