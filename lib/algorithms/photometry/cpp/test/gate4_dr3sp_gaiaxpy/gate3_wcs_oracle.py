# -*- coding: utf-8 -*-
"""
Gate 3 (Phase1 Full Freeze v2): PlateSolve WCS Oracle 对比

对照:
  1. Astrometry.net: 本机未安装 (GPL 外部 Oracle, 仅测试), 如实记录不可用.
  2. Astropy/WCSLIB: 读**真实生产产物** p1_wcs.json (schema DATA-P1-WCS, 由
     lib/infrastructure/scheduler/src/module_adapters.cpp:4796-4819 逐帧落盘),
     取其自身落盘的 CRVAL/CRPIX/CD/CTYPE, 构建 astropy.wcs.WCS, 验证
     pixel->sky->pixel 往返 <= 1e-6 px、CD 矩阵合法性 (det>0, 共形) 与投影合同。
  3. 消费 PSF 星点证据: 随产物一同读入并记录 (rms / n_pairs / sip_order /
     生产自报残差), 不在此处另立阈值。

用法: py -3.12 gate3_wcs_oracle.py --out <dir> --wcs <p1_wcs.json>

退出码 (GOVERN-08/G08-05): 0 = 全部 verdict 通过; 5 = 有 verdict 失败;
2 = 生产产物不可读 / schema 不符 / 缺字段 / 含非有限值 (fail-closed)。
"""

import argparse
import json
import os
import sys

import numpy as np
from astropy.wcs import WCS

# 生产产物 wcs 段的必需字段 (module_adapters.cpp:4796-4800 落盘)。
_WCS_KEYS = ("crval1", "crval2", "crpix1", "crpix2",
             "cd11", "cd12", "cd21", "cd22")


def load_product(path):
    """读真实生产产物 p1_wcs.json; 不合格即抛错 (fail-closed)。"""
    with open(path, "r", encoding="utf-8") as f:
        doc = json.load(f)
    if doc.get("schema") != "DATA-P1-WCS":
        raise ValueError("not a DATA-P1-WCS product: schema=%r" % (doc.get("schema"),))
    w = doc.get("wcs")
    if not isinstance(w, dict):
        raise ValueError("product has no 'wcs' object")
    missing = [k for k in _WCS_KEYS if k not in w]
    if missing:
        raise ValueError("product wcs missing keys: %r" % (missing,))
    return doc, w


def sample_box(doc, crpix):
    """采样框: 优先取生产产物 samples[] 的实际覆盖范围 (0-based 数组下标)。

    产品自述 pixel_origin = "0-based array index; FITS 1-based xp = x + 1",
    故 astropy (FITS 1-based) 的采样域整体 +1, 避免双重桥接。
    samples[] 缺失时退化为以 CRPIX 为心的 ±2048 px 方框。
    """
    samples = doc.get("samples")
    if isinstance(samples, list) and samples:
        xs = [float(s["x"]) for s in samples if "x" in s]
        ys = [float(s["y"]) for s in samples if "y" in s]
        if xs and ys:
            return (min(xs) + 1.0, max(xs) + 1.0, min(ys) + 1.0, max(ys) + 1.0)
    return (crpix[0] - 2048.0, crpix[0] + 2048.0,
            crpix[1] - 2048.0, crpix[1] + 2048.0)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--wcs", required=True,
                    help="真实生产产物 p1_wcs.json 路径 (schema DATA-P1-WCS)")
    args = ap.parse_args()

    # ── ① 数据源: 真实生产产物, 不再是本文件内的硬编码字面量 ──
    try:
        doc, w = load_product(args.wcs)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("[gate3] FAIL-CLOSED: 生产产物不可用: %s" % (exc,))
        return 2

    cd = np.array([[w["cd11"], w["cd12"]],
                   [w["cd21"], w["cd22"]]], dtype=float)
    crval = (float(w["crval1"]), float(w["crval2"]))
    crpix = (float(w["crpix1"]), float(w["crpix2"]))
    ctype = (str(w.get("ctype1", "")), str(w.get("ctype2", "")))

    if not (np.all(np.isfinite(cd)) and np.all(np.isfinite(crval))
            and np.all(np.isfinite(crpix))):
        print("[gate3] FAIL-CLOSED: 生产产物 WCS 含非有限值")
        return 2

    wc = WCS(naxis=2)
    wc.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    wc.wcs.crval = crval
    wc.wcs.crpix = crpix
    wc.wcs.cd = cd

    x0, x1, y0, y1 = sample_box(doc, crpix)
    rng = np.random.default_rng(20260807)
    xs = rng.uniform(x0, x1, 1000)
    ys = rng.uniform(y0, y1, 1000)
    sky = wc.all_pix2world(xs, ys, 1)
    xr, yr = wc.all_world2pix(sky[0], sky[1], 1)
    err = np.hypot(xr - xs, yr - ys)

    det = float(np.linalg.det(cd))
    scale_deg = np.sqrt(abs(det))
    scale_arcsec = scale_deg * 3600.0

    # ── ② verdict 汇总成返回值 (退出码由 :99 之外的入口消费) ──
    max_err_px = float(np.max(err))
    pass_roundtrip = bool(max_err_px < 1e-6)
    pass_det = bool(det > 0.0)
    pass_square = bool(abs(abs(cd[0, 0]) - abs(cd[1, 1])) < 1e-6)
    pass_ctype = bool(ctype[0].startswith("RA---TAN")
                      and ctype[1].startswith("DEC--TAN"))

    # 生产自报残差只作「产物完整性」检查 (有限性), 不在此处另立数值阈值。
    rep_rt = doc.get("max_roundtrip_px")
    rep_cross = doc.get("max_forward_cross_deg")
    pass_reported_finite = bool(
        isinstance(rep_rt, (int, float)) and np.isfinite(rep_rt)
        and isinstance(rep_cross, (int, float)) and np.isfinite(rep_cross))

    verdicts = {
        "pass_max_err_le_1e-6px": pass_roundtrip,
        "pass_det_gt_0": pass_det,
        "pass_square": pass_square,
        "pass_ctype_tan": pass_ctype,
        "pass_reported_residuals_finite": pass_reported_finite,
    }

    result = {
        "astrometry_net": {
            "available": False,
            "note": "本机未安装 astrometry.net; GPL 外部 Oracle 仅用于测试, 不进入生产依赖. "
                    "如需 blind-solve 独立对照, 需另行安装或使用 nova.astrometry.net 服务 (本包不调用外网求解).",
        },
        "product": {
            "path": args.wcs,
            "schema": doc.get("schema"),
            "solver": doc.get("solver"),
            "wcs_source": doc.get("wcs_source"),
            "forward_cross_ref": doc.get("forward_cross_ref"),
        },
        "solved_wcs": {
            "CRVAL": list(crval),
            "CRPIX": list(crpix),
            "CD": cd.tolist(),
            "CTYPE": list(ctype),
            "rms_arcsec": doc.get("rms_arcsec"),
            "n_pairs": doc.get("n_pairs"),
            "sip_order": doc.get("sip_order"),
            "n_samples": doc.get("n_samples"),
        },
        "astropy_roundtrip": {
            "n_samples": 1000,
            "sample_box_fits_1based": [x0, x1, y0, y1],
            "max_err_px": max_err_px,
            "median_err_px": float(np.median(err)),
            "p95_err_px": float(np.percentile(err, 95)),
            "pass_max_err_le_1e-6px": pass_roundtrip,
        },
        "cd_matrix": {
            "det": det,
            "scale_arcsec_px": float(scale_arcsec),
            "pass_det_gt_0": pass_det,
            "pass_square": pass_square,
        },
        "psf_consumption": {
            "stage_order": "PSF/STAR_MEASURE -> PLATESOLVE",
            "n_stars_consumed": doc.get("n_stars_consumed"),
            "redetection": False,
            "rms_arcsec": doc.get("rms_arcsec"),
            "n_pairs": doc.get("n_pairs"),
            "sip_order": doc.get("sip_order"),
            "reported_max_roundtrip_px": rep_rt,
            "reported_max_forward_cross_deg": rep_cross,
        },
        "verdicts": verdicts,
    }

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "gate3_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print("[gate3] 产物: %s (solver=%s)" % (args.wcs, doc.get("solver")))
    print("[gate3] Astropy 往返 (读生产 CRVAL/CRPIX/CD): max_err=%.3epx median=%.3epx "
          "(1000 样本)" % (max_err_px, float(np.median(err))))
    print("[gate3] CD det=%.3e 尺度=%.4f\"/px" % (det, float(scale_arcsec)))
    print("[gate3] astrometry.net: 不可用 (已记录)")
    for name, ok in verdicts.items():
        print("[gate3] %-32s %s" % (name, "PASS" if ok else "FAIL"))
    print("[gate3] DONE -> %s" % (args.out,))

    failed = [k for k, ok in verdicts.items() if not ok]
    return 5 if failed else 0


# ── ③ 入口按返回值退出 ──
if __name__ == "__main__":
    sys.exit(main())