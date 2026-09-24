#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""L4 接缝机器门：沿**真实帧足迹边界**的电平阶跃判据（能红能绿，内建正/负例）。

依据（逐条可核）：
  - ACCEPTANCE_SPEC.md §6.2「无接缝：**帧间**、块间无亮度/灰度阶跃」（帧间在前）；
  - ACCEPTANCE_SPEC.md §6.1（L4 产品生成链 = 整幅平面 FITS + 帧清单，两者都在手上）；
  - ASTROCS_DESIGN.md §2（SCI-C：接缝 = 帧集变化处的亮度阶跃）；
  - 实验/additive-sky-seamless/README.md:225（R5「相对接缝度量」< 1% = 1e-2）——
    本门阈值 1e-2 的唯一数值来源，不是另拍的；
  - AGENTS.md §9（判据必须能红能绿、配可执行正例/负例，而不是放松判据）。

为什么不是方差比（旧 V4 的盲区，必须写在工具里而不是只写在报告里）：
  eng/tools/e2e/render_vis.py::seam_metric（判据 V4）在**渲染脚本自切的 512 分块网格**上比
  「接缝处方差 / 块内方差」。电平阶跃 I → I + Δ **不改变方差** ⇒ 方差比对电平接缝**原理性失明**；
  且分块网格与帧足迹无关，真实帧间阶跃不落在 512 的整数倍行列上时完全不可见。
  实测（M42 整幅产品）：V4 = 0.9919 **判绿**，同一产品上本判据 |rel| max = 1.084e-1 **判红**。
  ⇒ V4 只作「渲染分块伪影」粗筛，不得再被引用为帧间接缝的证据
    （run/M42-E2E-01/pending-rulings.md R-2、run/VIS-P0FIX-01/REPORT.md 发现 4）。

判据（本工具）：
  seam(e)   = 沿第 e 条帧足迹边界、法向 ±d 的一阶差分（d 默认 2 px）
  ctrl(e)   = 同一条边界**法向平移 ctrl_shift 像素的平行线**上的同款差分（取有效样本多的一侧；
              扣掉局部结构梯度与噪声，与 SCI-C c7 的 off-locus 对照同义）
  excess(e) = median|seam(e)| − median|ctrl(e)|
  rel(e)    = excess(e) / bg(e)，bg = 该平行线上 |电平| 的中位数（相对口径，无量纲，可跨产品比）
  门：max_e |rel(e)| <= max_rel_excess（默认 1e-2）⇒ 绿（exit 0）；否则红（exit 1）。
  fail-closed（「无法判定」不得当「无接缝」）：
    · FITS / 帧清单 / 依赖不可用           ⇒ exit 2；
    · 有效帧边界数 == 0，或 rel 全部不可算  ⇒ **红**（exit 1）。

正/负例（--inject-frame k:amp，判别力自检；不依赖真值，靠已知注入）：
  把 amp 加进第 k 帧足迹多边形内部 = 沿该帧真实边界造一条**已知**电平阶跃，然后：
    · amp == 0 ⇒ 全部逐边度量必须与基线**一致**（真值无效应 ⇒ 必须回落到基线）；
    · amp != 0 ⇒ 该帧边界 max|excess| 必须 >= inject_detect_frac × |amp|（判据必须看得见），
                 同时打印旧 V4 方差比作对照（旧门对同一输入必须**不动** = 盲区复现）。
  任一不满足 ⇒ 该注入用例判红并计入 exit code。

--self-test（机器门常驻入口，不依赖 run/ 大产品）：合成夹具跑**同一代码路径**五组用例——
  S1 无台阶 ⇒ 绿；S2 注入已知台阶 ⇒ 红；S3 旧 V4 在 S2 输入上 ⇒ 绿（盲区复现）；
  S4 帧足迹落在画幅外 ⇒ 红（fail-closed）；S5 注入 0 ⇒ 与基线逐条一致且判绿（负例）。

用法：
  python3 eng/tools/e2e/seam_footprint.py --fits <整幅 FITS> --p1-dirs <dir...> \
      --json-out run/ci/seam-footprint/l4.json [--max-rel-excess 1e-2] \
      [--inject-frame 0:0.05] [--inject-frame 0:0]
  python3 eng/tools/e2e/seam_footprint.py --self-test \
      --json-out run/ci/seam-footprint/selftest.json
exit 0 = 绿；1 = 红（超门接缝 / 正负例未命中）；2 = 输入/环境错误（fail-closed）。
"""
from __future__ import annotations

import argparse
import glob
import io
import json
import os
import shutil
import sys

import numpy as np
from astropy.io import fits
from astropy.wcs import Sip, WCS

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_WORK_DIR = os.path.join(REPO, "run/ci/seam-footprint")
EDGE_NAMES = ("bottom", "top", "left", "right")
FIXTURE_NPIX = 1200      # 自检夹具画幅边长（px）
FIXTURE_FRAME_N = 360    # 自检夹具帧像素边长


# --------------------------------------------------------------------------- #
# 帧 WCS / 足迹
# --------------------------------------------------------------------------- #
def load_wcs_from_p1(path):
    """读 p1 阶段的帧 WCS 旁车（p1_wcs.json 的 wcs 块）。"""
    d = json.load(io.open(path, encoding="utf-8"))["wcs"]
    w = WCS(naxis=2)
    w.wcs.ctype = [d.get("ctype1", "RA---TAN"), d.get("ctype2", "DEC--TAN")]
    w.wcs.crval = [d["crval1"], d["crval2"]]
    w.wcs.crpix = [d["crpix1"], d["crpix2"]]
    w.wcs.cd = np.array([[d["cd11"], d["cd12"]], [d["cd21"], d["cd22"]]], dtype=float)
    sip = d.get("sip")
    if sip:
        def mat(key, order):
            return np.asarray(sip[key], dtype=float).reshape(6, 6)[:order + 1, :order + 1]
        o = int(sip.get("order", 3))
        ao = int(sip.get("ap_order", 5))
        w.sip = Sip(mat("a", o), mat("b", o), mat("ap", ao), mat("bp", ao),
                    [d["crpix1"], d["crpix2"]])
    return w, d


def frame_edges_p3(wframe, wp3, naxis, n_pts=256):
    """把帧的四条边界（帧像素坐标）投到产品网格，返回 [(name, Nx2 数组), ...]。

    顺序固定为 bottom(frame y=0) / top(y=ny-1) / left(x=0) / right(x=nx-1)，
    与 polygon_mask 的闭合顺序一致。
    """
    nx, ny = naxis
    t = np.linspace(0.0, 1.0, n_pts)
    specs = [(t * (nx - 1), np.zeros_like(t)), (t * (nx - 1), np.full_like(t, ny - 1)),
             (np.zeros_like(t), t * (ny - 1)), (np.full_like(t, nx - 1), t * (ny - 1))]
    out = []
    for nm, (xs, ys) in zip(EDGE_NAMES, specs):
        ra, dec = wframe.all_pix2world(xs, ys, 0)
        x3, y3 = wp3.all_world2pix(ra, dec, 0)
        out.append((nm, np.stack([x3, y3], axis=1)))
    return out


def wcs_from_header(hdr):
    """产品网格 WCS（从已打开的 FITS 头构造；供调用方复用已读入内存的产品数组）。"""
    w = WCS(naxis=2)
    w.wcs.ctype = [hdr["CTYPE1"], hdr["CTYPE2"]]
    w.wcs.crval = [hdr["CRVAL1"], hdr["CRVAL2"]]
    w.wcs.crpix = [hdr["CRPIX1"], hdr["CRPIX2"]]
    w.wcs.cd = np.array([[hdr["CD1_1"], hdr["CD1_2"]],
                         [hdr["CD2_1"], hdr["CD2_2"]]], float)
    return w


def product_wcs(path):
    with fits.open(path, memmap=True) as h:
        img = np.asarray(h[0].data, dtype=np.float32)
        hdr = h[0].header
    return img, wcs_from_header(hdr)


def collect_frames(p1_dirs):
    """按 --p1-dirs 顺序收集 <dir>/<frame>/p1_wcs.json，返回排序后的路径表。"""
    frames = []
    for d in p1_dirs:
        frames += sorted(glob.glob(os.path.join(d, "*", "p1_wcs.json")))
    return frames


# --------------------------------------------------------------------------- #
# 采样与度量
# --------------------------------------------------------------------------- #
def bilinear(img, x, y):
    h, w = img.shape
    ok = np.isfinite(x) & np.isfinite(y) & (x >= 1) & (y >= 1) & (x <= w - 2) & (y <= h - 2)
    xi = np.clip(x, 0, w - 2)
    yi = np.clip(y, 0, h - 2)
    x0 = np.floor(xi).astype(int)
    y0 = np.floor(yi).astype(int)
    fx = xi - x0
    fy = yi - y0
    v = (img[y0, x0] * (1 - fx) * (1 - fy) + img[y0, x0 + 1] * fx * (1 - fy) +
         img[y0 + 1, x0] * (1 - fx) * fy + img[y0 + 1, x0 + 1] * fx * fy)
    return np.where(ok, v, np.nan)


def med_abs(x):
    x = np.asarray(x, float)
    x = x[np.isfinite(x)]
    return float(np.median(np.abs(x))) if x.size else None


def mad(x):
    x = np.asarray(x, float)
    x = x[np.isfinite(x)]
    if x.size < 3:
        return None
    m = np.median(x)
    return float(1.4826 * np.median(np.abs(x - m)))


def edge_metric(img, e, d=2.0, ctrl_shift=200.0):
    """单条帧边界的 seam / ctrl / excess / rel_excess（法向平移平行线对照）。"""
    ex, ey = e[:, 0], e[:, 1]
    tx = np.gradient(ex)
    ty = np.gradient(ey)
    tn = np.hypot(tx, ty)
    tn[tn == 0] = 1.0
    tx, ty = tx / tn, ty / tn
    nxv, nyv = -ty, tx
    seam = bilinear(img, ex + nxv * d, ey + nyv * d) - bilinear(img, ex - nxv * d, ey - nyv * d)
    # 对照：法向 ±ctrl_shift 的平行线；选「有效样本多」的一侧（两侧同效时取 + 侧，确定性）
    cand, n_ok = [], []
    for s in (+1.0, -1.0):
        cxp = ex + nxv * (s * ctrl_shift + d)
        cyp = ey + nyv * (s * ctrl_shift + d)
        cxm = ex + nxv * (s * ctrl_shift - d)
        cym = ey + nyv * (s * ctrl_shift - d)
        c = bilinear(img, cxp, cyp) - bilinear(img, cxm, cym)
        cand.append(c)
        n_ok.append(int(np.isfinite(c).sum()))
    k = 0 if n_ok[0] >= n_ok[1] else 1
    ctrl = cand[k]
    sgn = 1.0 if k == 0 else -1.0
    m = np.isfinite(seam) & np.isfinite(ctrl)
    # 相对口径（SCI-C R5 同款）：阶跃 / 局部背景电平
    bg_vals = np.abs(bilinear(img, ex + nxv * (sgn * ctrl_shift),
                              ey + nyv * (sgn * ctrl_shift)))
    bg_vals = bg_vals[np.isfinite(bg_vals)]
    bg = float(np.median(bg_vals)) if bg_vals.size else None
    ex_ = (med_abs(seam[m]) - med_abs(ctrl[m])) if m.sum() else None
    return dict(n=int(m.sum()), ctrl_side=("+" if k == 0 else "-"),
                seam_med=med_abs(seam[m]), ctrl_med=med_abs(ctrl[m]),
                seam_mad=mad(seam[m]), ctrl_mad=mad(ctrl[m]),
                seam_p90=float(np.percentile(np.abs(seam[m]), 90)) if m.sum() else None,
                ctrl_p90=float(np.percentile(np.abs(ctrl[m]), 90)) if m.sum() else None,
                excess=ex_, bg=bg,
                rel_excess=(ex_ / bg) if (ex_ is not None and bg) else None)


def v4_tile_ratio(img, tile=512):
    """复刻 eng/tools/e2e/render_vis.py::seam_metric（同口径，逐项对照用）。

    这是**方差比**：分子分母都是 |一阶差分| 的中位数（局部尺度量），与均值/电平无关。
    电平阶跃不改变它 ⇒ 本函数存在的意义就是「旧门在同一输入上不动」的对照证据。
    """
    a = np.nan_to_num(np.asarray(img, dtype=np.float64))
    vr = np.where(np.abs(a).sum(axis=1) > 0)[0]
    vc = np.where(np.abs(a).sum(axis=0) > 0)[0]
    if vr.size < 4 or vc.size < 4:
        return None, None
    r0, r1 = int(vr[0]), int(vr[-1])
    c0, c1 = int(vc[0]), int(vc[-1])

    def m(v):
        v = v[np.isfinite(v)]
        return float(np.median(np.abs(v))) if v.size else 0.0

    cols = list(range(c0 + tile, c1, tile))
    rows = list(range(r0 + tile, r1, tile))
    sv = m(np.concatenate([a[r0:r1 + 1, c] - a[r0:r1 + 1, c - 1] for c in cols])) if cols else 0.0
    iv = m(np.concatenate([a[r0:r1 + 1, c - 3] - a[r0:r1 + 1, c - 4] for c in cols])) if cols else 0.0
    sh = m(np.concatenate([a[r, c0:c1 + 1] - a[r - 1, c0:c1 + 1] for r in rows])) if rows else 0.0
    ih = m(np.concatenate([a[r - 3, c0:c1 + 1] - a[r - 4, c0:c1 + 1] for r in rows])) if rows else 0.0
    num, den = max(sv, sh), max(iv, ih)
    return ((num / den) if den > 0 else (float("inf") if num > 0 else 1.0),
            dict(seam_v=sv, inner_v=iv, seam_h=sh, inner_h=ih,
                 n_col=len(cols), n_row=len(rows)))


def polygon_mask(edges, shape):
    """帧足迹多边形掩模（与 edge_metric 采样的边界曲线同源）。"""
    from PIL import Image, ImageDraw
    h, w = shape
    pts = ([tuple(p) for p in edges[0][1]] + [tuple(p) for p in edges[3][1]] +
           [tuple(p) for p in edges[1][1]][::-1] + [tuple(p) for p in edges[2][1]][::-1])
    im = Image.new("L", (w, h), 0)
    ImageDraw.Draw(im).polygon([(float(x), float(y)) for x, y in pts], fill=1)
    return np.asarray(im, dtype=bool)


# --------------------------------------------------------------------------- #
# 判据（可单独调用：门判定与正负例断言都走这里，自检与生产同一代码路径）
# --------------------------------------------------------------------------- #
def summarize(per_edge, min_samples=20):
    """逐边度量 → 门统计量。只取「有效样本足够」的边界，避免退化边污染判据。"""
    sel = [p for p in per_edge
           if p.get("excess") is not None and int(p.get("n") or 0) >= min_samples]
    ex = np.array([p["excess"] for p in sel], dtype=float)
    rel = np.array([abs(p["rel_excess"]) for p in sel if p.get("rel_excess") is not None],
                   dtype=float)
    out = {"n_edges_total": len(per_edge), "n_edges_valid": len(sel),
           "n_edges_rel": int(rel.size)}
    if ex.size:
        out.update(ex_med=float(np.median(np.abs(ex))),
                   ex_p90=float(np.percentile(np.abs(ex), 90)),
                   ex_max=float(np.abs(ex).max()))
    if rel.size:
        out.update(rel_med=float(np.median(rel)),
                   rel_p90=float(np.percentile(rel, 90)),
                   rel_max=float(rel.max()))
    return out


def gate_decision(per_edge, max_rel_excess, min_samples=20):
    """门判定（fail-closed）：返回 dict(verdict, reason, max_abs_rel, n_exceed, stats)。"""
    stats = summarize(per_edge, min_samples)
    if not stats.get("n_edges_valid"):
        return dict(verdict="FAIL", max_abs_rel=None, n_exceed=None, stats=stats,
                    reason="有效帧边界数为 0（无法判定 ≠ 无接缝；fail-closed 判红）")
    if not stats.get("n_edges_rel"):
        return dict(verdict="FAIL", max_abs_rel=None, n_exceed=None, stats=stats,
                    reason="全部有效边界的 rel_excess 不可计算（背景电平为 0？fail-closed 判红）")
    sel = [p for p in per_edge
           if p.get("rel_excess") is not None and int(p.get("n") or 0) >= min_samples]
    rel = np.array([abs(p["rel_excess"]) for p in sel], dtype=float)
    n_exceed = int((rel > max_rel_excess).sum())
    mx = float(rel.max())
    if n_exceed:
        worst = max(sel, key=lambda p: abs(p["rel_excess"]))
        return dict(verdict="FAIL", max_abs_rel=mx, n_exceed=n_exceed, stats=stats,
                    reason=("max|rel_excess| = %.6g > %.6g（超门边界 %d/%d 条；最差 %s/%s；"
                            "SCI-C R5 相对接缝门）"
                            % (mx, max_rel_excess, n_exceed, rel.size,
                               worst.get("frame"), worst.get("edge"))))
    return dict(verdict="PASS", max_abs_rel=mx, n_exceed=0, stats=stats,
                reason="max|rel_excess| = %.6g <= %.6g（门 %d 条有效边界全过）"
                       % (mx, max_rel_excess, rel.size))


def _close(a, b, tol=1e-12):
    if a is None or b is None:
        return a is b
    return abs(float(a) - float(b)) <= tol * max(1.0, abs(float(a)), abs(float(b)))


def injection_assert(base_per, inj_per, frame, amp, detect_frac=0.5, min_samples=20):
    """正/负例断言：amp==0 ⇒ 与基线逐条一致；amp!=0 ⇒ 该帧边界必须看得见 |amp|。

    返回 (ok, note, detail)。
    """
    if len(base_per) != len(inj_per):
        return False, "注入前后逐边条目数不一致（%d vs %d）" % (len(base_per), len(inj_per)), {}
    if amp == 0.0:
        bad = []
        for a, b in zip(base_per, inj_per):
            for k in ("n", "seam_med", "ctrl_med", "excess", "rel_excess"):
                if not _close(a.get(k), b.get(k)):
                    bad.append("%s/%s.%s: %r -> %r" % (a.get("frame"), a.get("edge"), k,
                                                       a.get(k), b.get(k)))
        if bad:
            return (False, "注入 0 后度量未回落基线（真值无效应却有差异）：" + "; ".join(bad[:4]),
                    {"n_diff": len(bad)})
        return True, "注入 0 ⇒ %d 条边界度量与基线逐条一致（真值无效应 ⇒ 回落基线）" % len(base_per), \
            {"n_identical": len(base_per)}
    sel = [p for p in inj_per
           if p.get("frame") == frame and p.get("excess") is not None
           and int(p.get("n") or 0) >= min_samples]
    if not sel:
        return False, "注入 %s += %g 后该帧没有任何有效边界（判据看不见）" % (frame, amp), {}
    mx = max(abs(p["excess"]) for p in sel)
    ok = mx >= detect_frac * abs(amp)
    note = ("注入 %s += %g ⇒ 该帧 %d 条边界 max|excess| = %.6g，"
            "需 >= %.6g（= %.2f×|amp|）⇒ %s"
            % (frame, amp, len(sel), mx, detect_frac * abs(amp), detect_frac,
               "可见" if ok else "**不可见**（判据对该已知阶跃失明）"))
    return ok, note, {"frame": frame, "amp": amp, "max_abs_excess": mx,
                      "required": detect_frac * abs(amp), "n_edges": len(sel)}


# --------------------------------------------------------------------------- #
# 单次评估（读产品 + 帧清单 → 逐边度量 + V4 对照 + 注入）
# --------------------------------------------------------------------------- #
def evaluate_product(fits_path, p1_dirs, frame_naxis=(4096, 4096), tile=512, d=2.0,
                     ctrl_shift=200.0, min_samples=20, max_rel_excess=1e-2,
                     injections=(), detect_frac=0.5, img=None, wp3=None):
    """跑一次完整判定。img/wp3 可预置（调用方已把产品读进内存时避免二次 I/O）。"""
    frames = collect_frames(p1_dirs)
    if not frames:
        raise RuntimeError("--p1-dirs 下没有找到任何 <frame>/p1_wcs.json：%s" % list(p1_dirs))
    if img is None or wp3 is None:
        img2, wp32 = product_wcs(fits_path)
        img = img2 if img is None else img
        wp3 = wp32 if wp3 is None else wp3  # 调用方预置 img 时必须同时预置 wp3
    edges_by_frame = []
    for wj in frames:
        wf, _ = load_wcs_from_p1(wj)
        edges_by_frame.append((os.path.basename(os.path.dirname(wj)),
                               frame_edges_p3(wf, wp3, frame_naxis)))

    def run(im):
        per = []
        for name, edges in edges_by_frame:
            for nm, e in edges:
                r = edge_metric(im, e, d=d, ctrl_shift=ctrl_shift)
                r["frame"] = name
                r["edge"] = nm
                per.append(r)
        return per

    base_v4, base_v4d = v4_tile_ratio(img, tile)
    base_per = run(img)
    base_gate = gate_decision(base_per, max_rel_excess, min_samples)
    rec = {"fits": (os.path.abspath(fits_path) if fits_path else None),
           "shape": [int(img.shape[0]), int(img.shape[1])],
           "n_frames": len(frames), "frame_naxis": list(frame_naxis), "tile": tile,
           "norm_d": d, "ctrl_shift": ctrl_shift, "min_samples": min_samples,
           "max_rel_excess": max_rel_excess,
           "v4": {"ratio": base_v4, "detail": base_v4d, "note": "方差比粗筛（对电平阶跃原理性失明）"},
           "baseline": {"gate": base_gate, "per_edge": base_per},
           "inject": []}
    for spec in injections:
        if ":" not in spec:
            raise RuntimeError("--inject-frame 需为 <k>:<amp>，实得 %r" % spec)
        ks, amps = spec.split(":", 1)
        k, amp = int(ks), float(amps)
        if not (0 <= k < len(edges_by_frame)):
            raise RuntimeError("--inject-frame 帧号 %d 越界（共 %d 帧）" % (k, len(edges_by_frame)))
        fname = edges_by_frame[k][0]
        mask = polygon_mask(edges_by_frame[k][1], img.shape)
        a2 = img.copy()
        if amp:
            a2[mask] += amp
        v4r, _ = v4_tile_ratio(a2, tile)
        per2 = run(a2)
        g2 = gate_decision(per2, max_rel_excess, min_samples)
        ok, note, detail = injection_assert(base_per, per2, fname, amp, detect_frac, min_samples)
        rec["inject"].append({
            "spec": spec, "frame_index": k, "frame": fname, "amp": amp,
            "mask_px": int(mask.sum()), "v4_ratio": v4r,
            "gate": g2, "assert_ok": bool(ok), "assert_note": note, "assert_detail": detail,
            "per_edge": per2})
        del a2, mask
    return rec


# --------------------------------------------------------------------------- #
# 合成夹具 + 自检（机器门常驻入口；不依赖 run/ 下的大产品）
# --------------------------------------------------------------------------- #
def _rot_cd(theta_deg, scale=5e-4):
    """帧 CD 矩阵 = 旋转 theta 的 diag(-s, s)（与产品 CD 同量级，足迹落在产品网格里）。"""
    t = np.deg2rad(theta_deg)
    r = np.array([[np.cos(t), -np.sin(t)], [np.sin(t), np.cos(t)]])
    return r @ np.array([[-scale, 0.0], [0.0, scale]])


def make_fixture(work_dir, seed=20260924, npix=FIXTURE_NPIX, frame_n=FIXTURE_FRAME_N,
                 theta_deg=0.7, bg=1.0, noise=1e-4, centers=None):
    """合成夹具：一张平滑产品 + N 帧帧清单（帧足迹为略倾斜的方块，四边两侧都有数据）。

    返回 (fits_path, [p1_dir], frame_naxis)。
    """
    from PIL import Image  # noqa: F401  （与 polygon_mask 同依赖，缺失时立刻报错）
    os.makedirs(work_dir, exist_ok=True)
    fits_path = os.path.join(work_dir, "synthetic_product.fits")
    rng = np.random.default_rng(seed)
    yy, xx = np.mgrid[0:npix, 0:npix]
    img = (bg + 0.02 * xx / float(npix) + noise * rng.standard_normal((npix, npix)))
    hdr = fits.Header()
    hdr["CTYPE1"] = "RA---TAN"
    hdr["CTYPE2"] = "DEC--TAN"
    hdr["CRVAL1"] = 84.0
    hdr["CRVAL2"] = -5.4
    hdr["CRPIX1"] = npix / 2.0 + 0.5
    hdr["CRPIX2"] = npix / 2.0 + 0.5
    hdr["CD1_1"] = -5e-4
    hdr["CD1_2"] = 0.0
    hdr["CD2_1"] = 0.0
    hdr["CD2_2"] = 5e-4
    fits.PrimaryHDU(data=img.astype(np.float32), header=hdr).writeto(fits_path, overwrite=True)

    if centers is None:
        q = npix // 3
        centers = [(q, q), (2 * q, q), (q, 2 * q), (2 * q, 2 * q)]
    p1_dir = os.path.join(work_dir, "p1")
    if os.path.isdir(p1_dir):
        shutil.rmtree(p1_dir)
    cd = _rot_cd(theta_deg)
    for i, (cx, cy) in enumerate(centers):
        d = os.path.join(p1_dir, "synth_frame_%02d" % i)
        os.makedirs(d, exist_ok=True)
        wcs = {"cd11": float(cd[0, 0]), "cd12": float(cd[0, 1]),
               "cd21": float(cd[1, 0]), "cd22": float(cd[1, 1]),
               "crpix1": frame_n / 2.0 + 0.5, "crpix2": frame_n / 2.0 + 0.5,
               "crval1": 84.0, "crval2": -5.4,
               "ctype1": "RA---TAN", "ctype2": "DEC--TAN"}
        # 帧中心落在产品像素 (cx, cy)：由 crval 反解（产品 CD 为 diag(-5e-4, 5e-4)）
        dra = -5e-4 * (cx - (npix / 2.0 + 0.5))
        ddec = 5e-4 * (cy - (npix / 2.0 + 0.5))
        wcs["crval1"] = 84.0 + dra
        wcs["crval2"] = -5.4 + ddec
        io.open(os.path.join(d, "p1_wcs.json"), "w", encoding="utf-8").write(
            json.dumps({"schema": "astrocs.p1-wcs/synthetic", "wcs": wcs},
                       ensure_ascii=False, indent=1) + "\n")
    return fits_path, [p1_dir], (frame_n, frame_n)


def self_test(work_dir, max_rel_excess=1e-2):
    """五组用例，全部走 evaluate_product / gate_decision 同一代码路径。"""
    cases = []

    def add(name, ok, note):
        cases.append({"name": name, "ok": bool(ok), "note": note})

    fits_path, p1_dirs, fnax = make_fixture(os.path.join(work_dir, "fixture"))
    kw = dict(frame_naxis=fnax, tile=512, min_samples=20, max_rel_excess=max_rel_excess)

    # S1：无台阶 ⇒ 必须绿
    r1 = evaluate_product(fits_path, p1_dirs, **kw)
    g1 = r1["baseline"]["gate"]
    add("S1-synthetic-no-step-green", g1["verdict"] == "PASS",
        "无注入基线：%s（%s）" % (g1["verdict"], g1["reason"]))

    # S2：注入已知台阶 ⇒ **注入后**的门必须红，且该帧边界必须看得见
    amp = 0.05
    r2 = evaluate_product(fits_path, p1_dirs, injections=["0:%g" % amp], **kw)
    g2 = r2["inject"][0]["gate"]
    inj2 = r2["inject"][0]
    add("S2-injected-step-red",
        g2["verdict"] == "FAIL" and inj2["assert_ok"],
        "注入 %g：注入后门 %s（max|rel|=%.4e，基线 %.4e）；%s"
        % (amp, g2["verdict"], g2["max_abs_rel"] if g2["max_abs_rel"] is not None else float("nan"),
           r2["baseline"]["gate"]["max_abs_rel"], inj2["assert_note"]))

    # S3：旧 V4 方差比在同一输入上必须**不动**（盲区复现 = 新判据非冗余）
    v4b, v4i = r1["v4"]["ratio"], r2["inject"][0]["v4_ratio"]
    add("S3-legacy-v4-blind-on-injected",
        v4i is not None and v4i <= 1.5,
        "旧 V4 方差比：基线 %.6f → 注入已知台阶 %.6f（门 1.5）⇒ %s"
        % (v4b if v4b is not None else float("nan"), v4i if v4i is not None else float("nan"),
           "判绿 = 对电平阶跃失明" if (v4i is not None and v4i <= 1.5) else "意外判红"))

    # S4：帧足迹落在画幅外 ⇒ 有效边界 0 ⇒ 必须红（fail-closed）
    off = 20 * FIXTURE_NPIX
    f2, p2, fn2 = make_fixture(os.path.join(work_dir, "fixture_off"),
                               centers=[(off, off), (off + 1, off), (off, off + 1), (off + 1, off + 1)])
    r4 = evaluate_product(f2, p2, **kw)
    g4 = r4["baseline"]["gate"]
    add("S4-degenerate-footprint-red", g4["verdict"] == "FAIL",
        "帧足迹全在画幅外：%s（%s）" % (g4["verdict"], g4["reason"]))

    # S5：注入 0 ⇒ 与基线逐条一致且判绿（负例：真值无效应必须回落）
    r5 = evaluate_product(fits_path, p1_dirs, injections=["0:0"], **kw)
    inj5 = r5["inject"][0]
    add("S5-zero-injection-identity",
        inj5["assert_ok"] and r5["baseline"]["gate"]["verdict"] == "PASS",
        "注入 0：%s；门 %s" % (inj5["assert_note"], r5["baseline"]["gate"]["verdict"]))

    bad = [c["name"] for c in cases if not c["ok"]]
    return {"cases": cases, "n_pass": len(cases) - len(bad), "n_total": len(cases),
            "failed": bad, "verdict": "PASS" if not bad else "FAIL"}


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #
def _write_json(path, rec):
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    io.open(path, "w", encoding="utf-8").write(
        json.dumps(rec, ensure_ascii=False, indent=1) + "\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description="L4 接缝机器门（沿真实帧足迹边界的电平阶跃判据）")
    ap.add_argument("--fits", default=None, help="整幅平面产品 FITS（L4 导出产品）")
    ap.add_argument("--p1-dirs", nargs="+", default=None,
                    help="P1 阶段帧目录（内含 <frame>/p1_wcs.json），可给多个夜次目录")
    ap.add_argument("--json-out", "--out", dest="json_out", default=None,
                    help="机器可读证据落盘路径")
    ap.add_argument("--max-rel-excess", type=float, default=1e-2,
                    help="门：max|rel_excess| 上限（默认 1e-2 = SCI-C R5「相对接缝度量 < 1%%」）")
    ap.add_argument("--min-samples", type=int, default=20,
                    help="一条边界计入判据所需的最少有效采样点数（默认 20）")
    ap.add_argument("--tile", type=int, default=512, help="V4 方差比对照用的分块边长")
    ap.add_argument("--frame-naxis", type=int, default=4096, help="帧像素边长（默认 4096）")
    ap.add_argument("--norm-d", type=float, default=2.0, help="法向差分半距（px，默认 2）")
    ap.add_argument("--ctrl-shift", type=float, default=200.0,
                    help="对照平行线的法向平移量（px，默认 200）")
    ap.add_argument("--inject-frame", action="append", default=[],
                    help="正/负例：<k>:<amp>，把 amp 加进第 k 帧足迹内部（amp=0 为负例）")
    ap.add_argument("--inject-detect-frac", type=float, default=0.5,
                    help="注入 amp 后该帧边界 |excess| 至少需达 |amp| 的该比例（默认 0.5）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="跑合成夹具五组用例（机器门常驻入口，不需要 --fits/--p1-dirs）")
    ap.add_argument("--work-dir", default=DEFAULT_WORK_DIR,
                    help="自检夹具与默认证据落盘根（默认 run/ci/seam-footprint）")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    out = args.json_out or os.path.join(args.work_dir, "seam_footprint.json")

    if args.self_test:
        try:
            st = self_test(args.work_dir, args.max_rel_excess)
        except Exception as exc:  # noqa: BLE001 - 夹具不可用 = 环境错误（fail-closed）
            print("SEAM_FOOTPRINT_SELFTEST_ERROR: %s: %s" % (type(exc).__name__, exc),
                  file=sys.stderr)
            return 2
        rec = {"schema": "astrocs.seam-footprint/v1", "tool": "seam_footprint",
               "mode": "self-test", "max_rel_excess": args.max_rel_excess,
               "work_dir": os.path.abspath(args.work_dir), **st}
        _write_json(out, rec)
        for c in st["cases"]:
            print("SELFTEST %s %s  %s" % ("PASS" if c["ok"] else "FAIL", c["name"], c["note"]))
        print("SEAM_FOOTPRINT_SELFTEST_%s: %d/%d -> %s"
              % (st["verdict"], st["n_pass"], st["n_total"], out))
        return 0 if st["verdict"] == "PASS" else 1

    if not args.fits or not args.p1_dirs:
        print("SEAM_FOOTPRINT_ERROR: 需 --fits 与 --p1-dirs（或 --self-test）", file=sys.stderr)
        return 2
    if not os.path.isfile(args.fits):
        print("SEAM_FOOTPRINT_ERROR: FITS 不存在: %s（fail-closed）" % args.fits, file=sys.stderr)
        return 2
    missing = [d for d in args.p1_dirs if not os.path.isdir(d)]
    if missing:
        print("SEAM_FOOTPRINT_ERROR: --p1-dirs 不存在: %s（fail-closed）" % missing, file=sys.stderr)
        return 2

    try:
        rec = evaluate_product(args.fits, args.p1_dirs,
                               frame_naxis=(args.frame_naxis, args.frame_naxis),
                               tile=args.tile, d=args.norm_d, ctrl_shift=args.ctrl_shift,
                               min_samples=args.min_samples,
                               max_rel_excess=args.max_rel_excess,
                               injections=args.inject_frame,
                               detect_frac=args.inject_detect_frac)
    except Exception as exc:  # noqa: BLE001 - 读不动/参数非法 = 输入错误（fail-closed）
        print("SEAM_FOOTPRINT_ERROR: %s: %s" % (type(exc).__name__, exc), file=sys.stderr)
        return 2

    gate = rec["baseline"]["gate"]
    rec["schema"] = "astrocs.seam-footprint/v1"
    rec["tool"] = "seam_footprint"
    rec["mode"] = "product"
    findings = []
    if gate["verdict"] == "FAIL":
        findings.append("SEAM %s" % gate["reason"])
    for inj in rec["inject"]:
        if not inj["assert_ok"]:
            findings.append("SEAM-INJECT[%s] %s" % (inj["spec"], inj["assert_note"]))
        if inj["amp"] != 0.0 and inj["v4_ratio"] is not None and inj["v4_ratio"] <= 1.5:
            inj["v4_blind_confirmed"] = True
    rec["findings"] = findings
    rec["verdict"] = "PASS" if not findings else "FAIL"
    _write_json(out, rec)

    if not args.quiet:
        print("n_frames=%d  edges=%d  V4(base)=%s（方差比，粗筛）"
              % (rec["n_frames"], rec["baseline"]["gate"]["stats"]["n_edges_total"],
                 ("%.6f" % rec["v4"]["ratio"]) if rec["v4"]["ratio"] is not None else "n/a"))
        st = rec["baseline"]["gate"]["stats"]
        print("baseline : %s | |rel| med=%.4e p90=%.4e max=%.4e | 门 %.1e"
              % (gate["verdict"], st.get("rel_med", float("nan")),
                 st.get("rel_p90", float("nan")), st.get("rel_max", float("nan")),
                 args.max_rel_excess))
        for inj in rec["inject"]:
            print("%-28s V4=%-10s 门=%-4s %s"
                  % ("inject " + inj["spec"],
                     ("%.6f" % inj["v4_ratio"]) if inj["v4_ratio"] is not None else "n/a",
                     inj["gate"]["verdict"], inj["assert_note"]))
        for f in findings:
            print("  " + f)
    print("SEAM_FOOTPRINT_%s: findings=%d -> %s" % (rec["verdict"], len(findings), out))
    return 0 if not findings else 1


if __name__ == "__main__":
    sys.exit(main())
