# -*- coding: utf-8 -*-
"""
Gate 3 (Phase1 Full Freeze v2): PlateSolve WCS Oracle 对比

对照:
  1. Astrometry.net: 本机未安装 (GPL 外部 Oracle, 仅测试), 如实记录不可用.
  2. Astropy/WCSLIB: 读**真实生产产物** p1_wcs.json (schema DATA-P1-WCS, 由
     lib/infrastructure/scheduler/src/module_adapters.cpp:5262-5266 逐帧落盘),
     取其自身落盘的 CRVAL/CRPIX/CD/CTYPE 与 wcs.sip, 构建 astropy.wcs.WCS
     (**CTYPE 取产物值, 不被字面量覆写**), 验证 pixel->sky->pixel 往返
     <= 1e-6 px、CD 矩阵合法性 (det>0, 共形) 与投影合同。
  3. SIP 路径: 产物自带的 wcs.sip (像素域多项式, 生产实现
     module_adapters.cpp:4487 p1_sip_poly 与 wcs_sip.cpp:255-261 同一约定)
     接入被检验模型, 参与往返计算与证据记录; CTYPE 与 wcs.sip 的一致性
     (带畸变必带 -SIP 后缀) 单独成项。**SIP 往返的数值判决暂缺容差口径,
     见下方「容差口径」段与 result.sip_roundtrip.verdict。**
  4. 消费解算/PSF 自报证据: 产物的 samples[] / max_roundtrip_px /
     max_forward_cross_deg 直接按**生产自己的契约**判决
     (module_adapters.cpp:5232-5241: roundtrip < 1e-6 px、前向交叉 <= 1e-9 deg),
     期望量与被测量同出产物自身字段, 不在此处另立阈值。

容差口径 (GATES_AND_TOLERANCES.md §3 / R1「本表唯一来源」):
  - roundtrip 1e-6 px: 取自生产落盘前的 fail-closed 契约
    (module_adapters.cpp:5232-5235), 亦为本脚本原判据的数值; **未改动**。
    注: 冻结门表另有 G-P1-WCS-RT (1e-4 px) / G-P1-WCS-BRIDGE-GLOBAL (1e-6 px),
    本门落在后者数值上, 归属登记见审核包。
  - SIP 往返: **无口径**。冻结门表里与 SIP 反演相关的两行给出互相冲突的读数 ——
    G-P1-WCS-F2 (astropy 前向/逆向 1e-4 px, 合成 order=2 SIP 场) 与
    G-P1-WCS-RT-APBP (一步 APx/BPx 表示残差 <= 10% x 边缘畸变预算, px)。
    对仓内 49 份真实产物, 前者判红、后者判绿 ⇒ 需要先裁决本门走哪一行,
    在裁决前只记录不判决 (result.sip_roundtrip.verdict = null + reason)。

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

# 生产产物 wcs 段的必需字段 (module_adapters.cpp:5256-5261 落盘)。
# ctype1/ctype2 是必需项: 产物必须自带 CTYPE, 本门不再用字面量补齐。
_WCS_KEYS = ("crval1", "crval2", "crpix1", "crpix2",
             "cd11", "cd12", "cd21", "cd22", "ctype1", "ctype2")

# 生产落盘前的 fail-closed 契约 (module_adapters.cpp:5232-5241), 逐字取值:
#   if (max_rt >= 1e-6)            -> "WcsTan roundtrip ... exceeds 1e-6 contract"
#   if (max_forward_cross_deg > 1e-9) -> "... exceeds 1e-9 absolute contract"
K_ROUNDTRIP_CONTRACT_PX = 1e-6
K_FORWARD_CROSS_CONTRACT_DEG = 1e-9

# 生产对 SIP 阶数的正式支持域 (module_adapters.cpp:4426-4430: order 越 [0,5] 即
# DATA 拒绝, 禁静默截断); 落盘系数固定 36 项 (i*6+j 布局)。
K_SIP_ORDER_MAX = 5
K_SIP_COEFFS = 36


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


def load_sip(w):
    """读产物自带 wcs.sip (module_adapters.cpp:4558-4566 p1_sip_to_json)。

    缺该键 = 无畸变 (生产明确「无 SIP = 不写 sip 对象」), 合法, 返回 None。
    存在则按生产解析契约 (module_adapters.cpp:4411-4456) 逐项校验:
    阶数落在 [0,5]、a/b/ap/bp 各 36 项有限数、系数存 i*6+j 布局。
    """
    sip = w.get("sip")
    if sip is None:
        return None
    if not isinstance(sip, dict):
        raise ValueError("product wcs.sip is not an object: %r" % (type(sip).__name__,))
    out = {}
    for key in ("order", "ap_order"):
        v = sip.get(key, 0)
        if not isinstance(v, int) or isinstance(v, bool):
            raise ValueError("product wcs.sip.%s must be integer: %r" % (key, v))
        if v < 0 or v > K_SIP_ORDER_MAX:
            raise ValueError("product wcs.sip.%s out of production order domain [0,5]: %r"
                             % (key, v))
        out[key] = v
    for key in ("a", "b", "ap", "bp"):
        v = sip.get(key, [0.0] * K_SIP_COEFFS)
        if not isinstance(v, list) or len(v) != K_SIP_COEFFS:
            raise ValueError("product wcs.sip.%s must be %d numbers: %r"
                             % (key, K_SIP_COEFFS, v if not isinstance(v, list) else len(v)))
        arr = np.asarray(v, dtype=float)
        if not np.all(np.isfinite(arr)):
            raise ValueError("product wcs.sip.%s contains non-finite coefficient" % (key,))
        out[key] = arr
    return out


def sip_poly(coeffs, dx, dy, order):
    """像素域 SIP 多项式, 与生产同式同布局 (p1_sip_poly, module_adapters.cpp:4487;
    同一约定的第二实现见 wcs_sip.cpp:79-95 evalSipT): sum c[i*6+j]*dx^i*dy^j, i+j<=order。"""
    c = np.asarray(coeffs, dtype=float).reshape(6, 6)
    acc = np.zeros(np.shape(dx), dtype=float)
    for i in range(order + 1):
        for j in range(order - i + 1):
            acc = acc + c[i, j] * dx ** i * dy ** j
    return acc


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


def sample_grid(doc):
    """产物 samples[] 自带的 (x, y) -> FITS 1-based 网格; 缺失即返回 None。"""
    samples = doc.get("samples")
    if not (isinstance(samples, list) and samples):
        return None
    if not all(("x" in s and "y" in s) for s in samples):
        return None
    return (np.asarray([float(s["x"]) for s in samples], dtype=float) + 1.0,
            np.asarray([float(s["y"]) for s in samples], dtype=float) + 1.0)


def sip_roundtrip(wc, grid, crpix, sip):
    """产物自带 SIP 接入被检验模型后的往返 (前向 A/B + 逆向 AP/BP 一步)。

    生产把畸变加在**像素域**: 前向 U = dx + A(dx,dy)、V = dy + B(dx,dy)
    (module_adapters.cpp:4754-4757, wcs_sip.cpp:255-261); 逆向用 AP/BP 一步
    (wcs_sip.cpp:307-311)。注意本仓 SIP 系数是像素域多项式, **不是** astropy
    wcs.sip 期望的中间世界坐标(度)域, 故不得直接写进 astropy.wcs.sip。
    """
    xs, ys = grid
    order, ap_order = sip["order"], sip["ap_order"]
    dx = xs - crpix[0]
    dy = ys - crpix[1]
    a = sip_poly(sip["a"], dx, dy, order)
    b = sip_poly(sip["b"], dx, dy, order)
    # 无畸变 (order==0) 时前端等价于线性面; 生产对此不写 sip 对象。
    sky = wc.all_pix2world(xs + a, ys + b, 1)
    bx, by = wc.all_world2pix(sky[0], sky[1], 1)
    inv_a = sip_poly(sip["ap"], bx - crpix[0], by - crpix[1], ap_order)
    inv_b = sip_poly(sip["bp"], bx - crpix[0], by - crpix[1], ap_order)
    err = np.hypot((bx + inv_a) - xs, (by + inv_b) - ys)
    return {
        "forward_distortion_max_px": float(np.max(np.hypot(a, b))) if order > 0 else 0.0,
        "max_err_px": float(np.max(err)),
        "median_err_px": float(np.median(err)),
        "p95_err_px": float(np.percentile(err, 95)),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--wcs", required=True,
                    help="真实生产产物 p1_wcs.json 路径 (schema DATA-P1-WCS)")
    args = ap.parse_args()

    # ── ① 数据源: 真实生产产物, 不再是本文件内的硬编码字面量 ──
    try:
        doc, w = load_product(args.wcs)
        sip = load_sip(w)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print("[gate3] FAIL-CLOSED: 生产产物不可用: %s" % (exc,))
        return 2

    cd = np.array([[w["cd11"], w["cd12"]],
                   [w["cd21"], w["cd22"]]], dtype=float)
    crval = (float(w["crval1"]), float(w["crval2"]))
    crpix = (float(w["crpix1"]), float(w["crpix2"]))
    # CTYPE 取产物自身落盘值; 本门**不再**用字面量覆写 (旧版 :91 曾硬编码
    # ["RA---TAN","DEC--TAN"], 使产物的 -SIP 后缀与投影族从未进入被检验的 WCS)。
    ctype = (str(w["ctype1"]), str(w["ctype2"]))

    if not (np.all(np.isfinite(cd)) and np.all(np.isfinite(crval))
            and np.all(np.isfinite(crpix))):
        print("[gate3] FAIL-CLOSED: 生产产物 WCS 含非有限值")
        return 2

    wc = WCS(naxis=2)
    try:
        wc.wcs.ctype = list(ctype)
        wc.wcs.crval = crval
        wc.wcs.crpix = crpix
        wc.wcs.cd = cd
    except Exception as exc:  # astropy 拒绝产物 CTYPE ⇒ fail-closed, 不静默退回字面量
        print("[gate3] FAIL-CLOSED: 产物 CTYPE 不可用于 astropy: %s (%r/%r)"
              % (exc, ctype[0], ctype[1]))
        return 2

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
    pass_roundtrip = bool(max_err_px < K_ROUNDTRIP_CONTRACT_PX)
    pass_det = bool(det > 0.0)
    pass_square = bool(abs(abs(cd[0, 0]) - abs(cd[1, 1])) < 1e-6)
    pass_ctype = bool(ctype[0].startswith("RA---TAN")
                      and ctype[1].startswith("DEC--TAN"))

    # CTYPE 与 wcs.sip 必须互相印证: 生产按 `sip.present ? "-SIP" : ""` 选 CTYPE
    # (module_adapters.cpp:4726-4728; ipv 面 :5250 `sip.present = r.sip_order > 0`)。
    # 离散合同, 不需要数值容差。wcs.sip 已由 load_sip 按生产解析契约校验完毕。
    sip_present = bool(sip is not None and sip["order"] > 0)
    ctype_sip_suffix = bool(ctype[0].endswith("-SIP") and ctype[1].endswith("-SIP"))
    pass_ctype_matches_sip = bool(sip_present == ctype_sip_suffix)

    # SIP 接入被检验模型后的往返: **只记录不判决** —— 仓内无对应容差口径
    # (冻结门表 G-P1-WCS-F2 1e-4 px 与 G-P1-WCS-RT-APBP 10% x 边缘畸变预算
    # 对同一批真实产物给出相反判决, 见文件头「容差口径」段)。
    grid = sample_grid(doc)
    sip_rt = None
    if sip is not None and grid is not None:
        sip_rt = sip_roundtrip(wc, grid, crpix, sip)

    # 生产自报残差: 期望量 = 生产落盘前的 fail-closed 契约
    # (module_adapters.cpp:5232-5241); 被测量 = 产物自身落盘的字段。
    rep_rt = doc.get("max_roundtrip_px")
    rep_cross = doc.get("max_forward_cross_deg")
    rep_rt_ok = isinstance(rep_rt, (int, float)) and not isinstance(rep_rt, bool) \
        and np.isfinite(rep_rt)
    rep_cross_ok = isinstance(rep_cross, (int, float)) and not isinstance(rep_cross, bool) \
        and np.isfinite(rep_cross)
    pass_reported_roundtrip = bool(rep_rt_ok and rep_rt < K_ROUNDTRIP_CONTRACT_PX)
    pass_reported_forward_cross = bool(rep_cross_ok
                                       and rep_cross <= K_FORWARD_CROSS_CONTRACT_DEG)

    # 汇总字段必须能由产物自带的 samples[] 残差向量重新导出 (逐项精确相等;
    # 生产由 `if (rt > max_rt) max_rt = rt` 保证, 无需数值容差)。
    sample_rt = [s.get("roundtrip_px") for s in (doc.get("samples") or [])]
    sample_rt = [v for v in sample_rt
                 if isinstance(v, (int, float)) and not isinstance(v, bool)
                 and np.isfinite(v)]
    pass_reported_max_matches_samples = bool(
        sample_rt and rep_rt_ok and max(sample_rt) == rep_rt)

    verdicts = {
        "pass_max_err_le_1e-6px": pass_roundtrip,
        "pass_det_gt_0": pass_det,
        "pass_square": pass_square,
        "pass_ctype_tan": pass_ctype,
        "pass_ctype_matches_sip": pass_ctype_matches_sip,
        "pass_reported_roundtrip_lt_1e-6px": pass_reported_roundtrip,
        "pass_reported_forward_cross_le_1e-9deg": pass_reported_forward_cross,
        "pass_reported_max_matches_samples": pass_reported_max_matches_samples,
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
            "CTYPE_source": "product wcs.ctype1/ctype2 (verbatim, not overridden)",
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
            "note": "线性核心往返 (CTYPE 取产物值); 畸变项见 sip_roundtrip。",
        },
        "sip": {
            "present_in_product": sip is not None,
            "order": None if sip is None else sip["order"],
            "ap_order": None if sip is None else sip["ap_order"],
            "top_level_sip_order": doc.get("sip_order"),
            "coefficient_layout": "i*6+j, 36 项 (生产落盘布局, module_adapters.cpp:4558)",
            "domain": "pixel offset from CRPIX (生产实现 module_adapters.cpp:4487 / "
                      "wcs_sip.cpp:255-261); 非 astropy wcs.sip 的中间世界坐标(度)域",
            "passed_to_wcs_model": bool(sip_rt is not None),
            "pass_ctype_matches_sip": pass_ctype_matches_sip,
        },
        "sip_roundtrip": dict(sip_rt, **({
            "verdict": None,
            "reason": "仓内无「astropy + 生产 SIP 产物」往返的容差口径: "
                      "G-P1-WCS-F2 (1e-4 px) 与 G-P1-WCS-RT-APBP "
                      "(<=10% x 边缘畸变预算) 对同一批产物结论相反; 待裁决后再判决。",
        } if sip_rt is not None else {
            "verdict": None,
            "reason": "产物无 wcs.sip 或无 samples[] 网格, 无可算的 SIP 往返。",
        })),
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
        "reported_residual_verdicts": {
            "expectation_source": "production fail-closed contract, "
                                  "module_adapters.cpp:5232-5241 "
                                  "(roundtrip < 1e-6 px; forward cross <= 1e-9 deg)",
            "measured_source": "product max_roundtrip_px / max_forward_cross_deg "
                               "+ samples[].roundtrip_px",
            "forward_cross_ref": doc.get("forward_cross_ref"),
            "pass_reported_roundtrip_lt_1e-6px": pass_reported_roundtrip,
            "pass_reported_forward_cross_le_1e-9deg": pass_reported_forward_cross,
            "pass_reported_max_matches_samples": pass_reported_max_matches_samples,
        },
        "verdicts": verdicts,
    }

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "gate3_result.json"), "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2)

    print("[gate3] 产物: %s (solver=%s)" % (args.wcs, doc.get("solver")))
    print("[gate3] CTYPE 取自产物: %s / %s" % (ctype[0], ctype[1]))
    print("[gate3] Astropy 往返 (读生产 CRVAL/CRPIX/CD/CTYPE): max_err=%.3epx median=%.3epx "
          "(1000 样本)" % (max_err_px, float(np.median(err))))
    if sip_rt is not None:
        print("[gate3] SIP(order=%s, ap_order=%s) 前向畸变 max=%.4f px; "
              "含畸变往返 max=%.4f px (**未判决**: 无容差口径)"
              % (sip["order"], sip["ap_order"],
                 sip_rt["forward_distortion_max_px"], sip_rt["max_err_px"]))
    print("[gate3] 产物自报: roundtrip=%.3e px (契约 <%.0e)  cross=%.3e deg (契约 <=%.0e)  "
          "forward_cross_ref=%s"
          % (rep_rt if rep_rt_ok else float("nan"), K_ROUNDTRIP_CONTRACT_PX,
             rep_cross if rep_cross_ok else float("nan"), K_FORWARD_CROSS_CONTRACT_DEG,
             doc.get("forward_cross_ref")))
    print("[gate3] CD det=%.3e 尺度=%.4f\"/px" % (det, float(scale_arcsec)))
    print("[gate3] astrometry.net: 不可用 (已记录)")
    for name, ok in verdicts.items():
        print("[gate3] %-42s %s" % (name, "PASS" if ok else "FAIL"))
    print("[gate3] DONE -> %s" % (args.out,))

    failed = [k for k, ok in verdicts.items() if not ok]
    return 5 if failed else 0


# ── ③ 入口按返回值退出 ──
if __name__ == "__main__":
    sys.exit(main())