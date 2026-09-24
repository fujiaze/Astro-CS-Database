#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""VIS-501 成品帧渲染与自检（R 通道）。

依据：控制包 VIS-501 任务书（整幅 FITS + 分块 PNG + 逐块自检 + 接缝度量）；
      AGENTS §9（视觉层提交前自行逐块检查，分辨率不足时裁剪放大再读；分段计时找热点）。

判据（fail-closed，都能红）：
  V1 覆盖域自洽          V1a 覆盖==0 ⇒ 必须非有限（未覆盖不得有值；抓幻影数据）；
                         V1b 非有限像素中**不与画幅边界连通**的连通域数 == 0
                             （内部空洞 = 被有限值包围的非有限像素，真缺陷）；
                         无覆盖平面时 V1a 不适用，V1b 同判（同一规则，不另设口径）。
     为什么 V1b 判「内部空洞」而不是「覆盖却非有限」：导出画幅由用户给定，**可以大于数据
     足迹** —— 此时边框上必然出现非有限黑边，这是设计允许的（ASTROCS_DESIGN.md §6：
     export 按用户指定 WCS 导出平面；负责人 2026-09-23 裁决：默认导出不得裁剪任何有效像素，
     允许导出黑边，由用户到平面后自行裁剪）。因此「覆盖 >0 ⇒ 必须有限」这个前提是错的：
     它把「画幅大于足迹」这一合法几何误判成缺陷（M42 全画幅导出实测 464,263 px / 2.77%，
     全部落在足迹边缘的窄带内）。真正该判的是 ACCEPTANCE_SPEC.md §6.2「无"黑洞"：
     无异常零值/死区/**未填充孔洞**」——即内部空洞，阈值 0。
  V2 nonzero_fraction    非零像素占比 > 0（全零产品 = 空图，判红）；
  V3 dynamic_range       有限像素的 p99.9/p0.1 > 1（常数图判红）；
  V4 seam_metric         相邻**渲染分块**边界处的一阶差分中位数与块内同向差分中位数之比
                         （seam_ratio）；判据 seam_ratio <= 1.5。
                         **口径限定（务必读）**：这是**方差比**（分子分母都是 |一阶差分| 的中位数，
                         即局部尺度量），只筛「渲染分块伪影 / 方差结构异常」。电平阶跃
                         I → I + Δ **不改变方差** ⇒ V4 对**电平接缝原理性失明**；且分块网格由
                         本脚本自切，与帧足迹无关，真实帧间阶跃不落在 512 的整数倍行列上时
                         完全不可见。实测 M42 整幅产品：V4 = 0.9919 判绿，而同一产品上 V6
                         的 |rel| max = 1.08e-1 判红。
                         ⇒ **V4 不得再被引用为「帧间无接缝」的证据**（run/M42-E2E-01/
                         pending-rulings.md R-2、run/VIS-P0FIX-01/REPORT.md 发现 4）；
                         帧间接缝由 V6 判定。
  V5 coverage            非零像素的包围盒面积 / 全图面积 >= 0.5（导出中心必须落在覆盖内）；
  V6 seam_level_step     **电平阶跃**判据（ACCEPTANCE_SPEC §6.2「帧间…无亮度/灰度阶跃」）：
                         沿**真实帧足迹边界**取法向 ±d 差分（seam），同一边界法向平移
                         ctrl_shift 的平行线取同款差分（ctrl，off-locus 对照），
                         excess = median|seam| − median|ctrl|，rel = excess / 局部背景电平；
                         判据 max|rel| <= --max-rel-step（默认 1e-2 = SCI-C R5「相对接缝度量 < 1%」）。
                         判据实现唯一事实源：eng/tools/e2e/seam_footprint.py（同一模块被 L4 机器门
                         CHK-L4-SEAM-FOOTPRINT 使用；本脚本只做入口，不复制判据）。
                         fail-closed：缺 --p1-dirs（帧足迹）⇒ **判红**（V4 不得代替帧间接缝判据；
                         「无法判定」≠「无接缝」）。纯渲染用途可显式 --no-seam-eval 跳过，
                         此时报告记 seam_level_step.status = SKIPPED_BY_FLAG 并在结论行打印。

用法要点：
  · 判接缝必须给 --p1-dirs <P1 帧目录...>（内含 <frame>/p1_wcs.json）；
  · --inject-frame k:amp 为判别力自检（正/负例）：amp≠0 ⇒ 必须判红；amp=0 ⇒ 度量回落基线。
    注入模式下**不写 PNG/分块**（避免把注入图当交付渲染图），只出判据证据；
  · --self-test 跑合成夹具的能红能绿自检（不需要 --fits）。

输出：out/<group>/{full.png, tiles/tile_r{r}_c{c}.png, vis_report.json}
"""
from __future__ import annotations

import argparse
import io
import json
import os
import sys

import numpy as np
from astropy.io import fits
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# V6 判据的**唯一事实源**：eng/tools/e2e/seam_footprint.py（与 L4 机器门
# CHK-L4-SEAM-FOOTPRINT 共用同一实现，本脚本不复制判据）。同目录导入；依赖缺失时
# V6 判红（fail-closed），不得静默跳过。
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


def _seam_mod():
    import seam_footprint
    return seam_footprint


def stretch(a, lo_pct=0.1, hi_pct=99.9, mode="asinh", asinh_a=0.05):
    """返回 uint8 图 + 拉伸参数（确定性：分位数来自有限像素）。"""
    fin = np.isfinite(a)
    vals = a[fin]
    lo = float(np.percentile(vals, lo_pct))
    hi = float(np.percentile(vals, hi_pct))
    if not np.isfinite(lo) or not np.isfinite(hi) or hi <= lo:
        hi = lo + 1.0
    x = np.clip((np.nan_to_num(a, nan=lo, posinf=hi, neginf=lo) - lo) / (hi - lo), 0.0, 1.0)
    if mode == "asinh":
        x = np.arcsinh(x / asinh_a) / np.arcsinh(1.0 / asinh_a)
    return (np.clip(x, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8), lo, hi


def _bfs_reach(nf, w, seeds):
    """4-连通泛洪：只能走 nf 为真的像素。返回 (visited 位图, 成员下标列表)。

    nf 为 bytearray（长度 = h*w，行主序，1 = 可通行）。种子下标为 0-based 平坦下标。
    """
    n = len(nf)
    visited = bytearray(n)
    members = []
    stack = list(seeds)
    while stack:
        i = stack.pop()
        if visited[i]:
            continue
        visited[i] = 1
        members.append(i)
        c = i % w
        if i >= w and nf[i - w] and not visited[i - w]:
            stack.append(i - w)
        if i + w < n and nf[i + w] and not visited[i + w]:
            stack.append(i + w)
        if c > 0 and nf[i - 1] and not visited[i - 1]:
            stack.append(i - 1)
        if c + 1 < w and nf[i + 1] and not visited[i + 1]:
            stack.append(i + 1)
    return visited, members


def hole_analysis(nonfinite):
    """V1b 判据实现：把非有限像素分成「边界黑边」与「内部空洞」两类（4-连通）。

    边界黑边  = 与画幅边界连通的非有限连通域 —— 画幅大于数据足迹时的合法黑边，设计允许；
    内部空洞  = 其余非有限像素（被有限值包围）—— ACCEPTANCE_SPEC §6.2「无黑洞」判红，阈值 0。

    返回 (hole_mask, boundary_px, n_components, largest_component_px)。
    无第三方依赖（不用 scipy）：bytearray 上的确定性泛洪，4096² 实测量级 = 亚秒。
    """
    h, w = nonfinite.shape
    flat = np.ascontiguousarray(nonfinite, dtype=np.uint8).ravel()
    nf = bytearray(flat.tobytes())
    border = np.zeros((h, w), dtype=bool)
    if h and w:
        border[0, :] = True
        border[-1, :] = True
        border[:, 0] = True
        border[:, -1] = True
    seeds = [int(i) for i in np.flatnonzero(nonfinite & border)]
    reached, border_members = _bfs_reach(nf, w, seeds)
    reached_arr = np.frombuffer(bytes(reached), dtype=np.uint8).reshape(h, w).astype(bool)
    hole_mask = nonfinite & ~reached_arr
    # 空洞连通域计数（同样 4-连通；只对残集泛洪，量级 = 缺陷像素数，不是全图）
    remaining = bytearray(nf)
    for i in border_members:
        remaining[i] = 0
    n_comp = 0
    largest = 0
    for i in np.flatnonzero(hole_mask):
        i = int(i)
        if not remaining[i]:
            continue
        _v, members = _bfs_reach(remaining, w, [i])
        n_comp += 1
        largest = max(largest, len(members))
        for j in members:
            remaining[j] = 0
    return hole_mask, len(border_members), n_comp, largest


def seam_metric(img, tile):
    """接缝度量：分块边界处 |Δ| 中位数 / 块内同向 |Δ| 中位数。

    只在**有效（非零）行/列**上取，避免把空白天区当成接缝。
    """
    a = img.astype(np.float64)
    h, w = a.shape
    valid_rows = np.where(np.abs(a).sum(axis=1) > 0)[0]
    valid_cols = np.where(np.abs(a).sum(axis=0) > 0)[0]
    if valid_rows.size < 4 or valid_cols.size < 4:
        return None, None
    r0, r1 = int(valid_rows[0]), int(valid_rows[-1])
    c0, c1 = int(valid_cols[0]), int(valid_cols[-1])

    def med(v):
        v = v[np.isfinite(v)]
        return float(np.median(np.abs(v))) if v.size else 0.0

    # 竖直接缝（列方向分块边界）
    cols = [c for c in range(c0 + tile, c1, tile)]
    seam_v, inner_v = [], []
    for c in cols:
        seam_v.append(a[r0:r1 + 1, c] - a[r0:r1 + 1, c - 1])
        inner_v.append(a[r0:r1 + 1, c - 3] - a[r0:r1 + 1, c - 4])
    # 水平接缝（行方向分块边界）
    rows = [r for r in range(r0 + tile, r1, tile)]
    seam_h, inner_h = [], []
    for r in rows:
        seam_h.append(a[r, c0:c1 + 1] - a[r - 1, c0:c1 + 1])
        inner_h.append(a[r - 3, c0:c1 + 1] - a[r - 4, c0:c1 + 1])
    sv = med(np.concatenate(seam_v)) if seam_v else 0.0
    iv = med(np.concatenate(inner_v)) if inner_v else 0.0
    sh = med(np.concatenate(seam_h)) if seam_h else 0.0
    ih = med(np.concatenate(inner_h)) if inner_h else 0.0
    num = max(sv, sh)
    den = max(iv, ih)
    ratio = (num / den) if den > 0 else float("inf") if num > 0 else 1.0
    return ratio, {"seam_v": sv, "inner_v": iv, "seam_h": sh, "inner_h": ih,
                   "n_col_seams": len(cols), "n_row_seams": len(rows)}


def _worst_edges(per_edge, max_rel_step, n=5, min_samples=20):
    """V6 最差边界（供报告定位；只记定位与量值，不参与判定）。"""
    sel = [p for p in per_edge
           if p.get("rel_excess") is not None and int(p.get("n") or 0) >= min_samples]
    sel.sort(key=lambda p: -abs(p["rel_excess"]))
    return [{"frame": p.get("frame"), "edge": p.get("edge"), "n": p.get("n"),
             "excess": p.get("excess"), "rel_excess": p.get("rel_excess"),
             "bg": p.get("bg"), "over": bool(abs(p["rel_excess"]) > max_rel_step)}
            for p in sel[:n]]


def apply_frame_injection(data, hdr, p1_dirs, spec, frame_naxis=4096):
    """把 <k>:<amp> 加进第 k 帧足迹多边形内部（判别力自检用），返回注入记录。

    就地改 data；调用方负责不要在注入后写交付渲染图。帧足迹不可用 ⇒ 抛错（调用方判红）。
    """
    sf = _seam_mod()
    ks, amps = spec.split(":", 1)
    frames = sf.collect_frames(p1_dirs or [])
    if not frames:
        raise RuntimeError("--inject-frame 需要 --p1-dirs（帧足迹）才能定位注入面")
    k, amp = int(ks), float(amps)
    if not (0 <= k < len(frames)):
        raise RuntimeError("--inject-frame 帧号 %d 越界（共 %d 帧）" % (k, len(frames)))
    wf, _ = sf.load_wcs_from_p1(frames[k])
    mask = sf.polygon_mask(
        sf.frame_edges_p3(wf, sf.wcs_from_header(hdr), (frame_naxis, frame_naxis)), data.shape)
    if amp:
        data[mask] += amp
    rec = {"spec": spec, "frame": os.path.basename(os.path.dirname(frames[k])),
           "amp": amp, "mask_px": int(mask.sum())}
    del mask
    return rec


def seam_level_step(data, hdr, p1_dirs, max_rel_step=1e-2, min_samples=20, frame_naxis=4096,
                    norm_d=2.0, ctrl_shift=200.0):
    """V6：按**真实帧足迹**的电平阶跃判据（判据实现见 eng/tools/e2e/seam_footprint.py）。

    与 V4 的本质差别在判据量本身：V4 比的是方差（局部尺度量），**电平阶跃不改变方差**
    ⇒ 对帧间接缝原理性失明；V6 直接量沿真实帧边界法向的电平阶跃，并用同一边界法向平移
    的平行线（off-locus）扣掉局部结构梯度。阈值 1e-2 = SCI-C R5「相对接缝度量 < 1%」。
    fail-closed：判据模块不可用 / 帧足迹不可用 / 有效边界为 0 / 背景电平为 0 ⇒ 判红。
    """
    try:
        sf = _seam_mod()
    except Exception as exc:  # noqa: BLE001 - 依赖缺失不得静默跳过
        return {"status": "ERROR", "verdict": "FAIL",
                "reason": "V6 判据模块不可用（%s: %s）" % (type(exc).__name__, exc)}
    try:
        img = np.ascontiguousarray(data, dtype=np.float32)
        rec = sf.evaluate_product(
            None, p1_dirs, frame_naxis=(frame_naxis, frame_naxis), d=norm_d,
            ctrl_shift=ctrl_shift, min_samples=min_samples, max_rel_excess=max_rel_step,
            img=img, wp3=sf.wcs_from_header(hdr))
    except Exception as exc:  # noqa: BLE001 - 读不动/无帧 = 判红，不静默
        return {"status": "ERROR", "verdict": "FAIL",
                "reason": "V6 评估失败（%s: %s）" % (type(exc).__name__, exc)}
    gate = rec["baseline"]["gate"]
    st = gate["stats"]
    out = {"status": "EVALUATED", "verdict": gate["verdict"], "reason": gate["reason"],
           "max_rel_step": max_rel_step, "n_edges_total": st["n_edges_total"],
           "n_edges_valid": st["n_edges_valid"], "n_exceed": gate["n_exceed"],
           "rel_med": st.get("rel_med"), "rel_p90": st.get("rel_p90"),
           "rel_max": st.get("rel_max"), "ex_max": st.get("ex_max"),
           "worst": _worst_edges(rec["baseline"]["per_edge"], max_rel_step),
           "v4_crosscheck": {"ratio": rec["v4"]["ratio"], "note": rec["v4"]["note"]}}
    return out


def self_test(out_dir):
    """V6 判别力自检（合成夹具，不需要 --fits）：能红能绿 + V4 盲区复现 + fail-closed。"""
    sf = _seam_mod()
    cases = []

    def add(name, ok, note):
        cases.append({"name": name, "ok": bool(ok), "note": note})

    work = os.path.join(out_dir, "selftest_fixture")
    fits_path, p1_dirs, fnax = sf.make_fixture(os.path.join(work, "fixture"))
    with fits.open(fits_path) as h:
        hdr = h[0].header
        data = np.asarray(h[0].data, dtype=np.float64)
    kw = dict(max_rel_step=1e-2, frame_naxis=fnax[0])

    v6_base = seam_level_step(data, hdr, p1_dirs, **kw)
    add("V6-no-step-green", v6_base["verdict"] == "PASS",
        "无台阶：V6 %s（%s）" % (v6_base["verdict"], v6_base["reason"]))

    # 正例：注入已知台阶 ⇒ 必须判红（走 main 的同一路径：先注入 data，再判）
    amp = 0.05
    d_inj = data.copy()
    inj = apply_frame_injection(d_inj, hdr, p1_dirs, "0:%g" % amp, frame_naxis=fnax[0])
    v6_inj = seam_level_step(d_inj, hdr, p1_dirs, **kw)
    add("V6-injected-step-red", v6_inj["verdict"] == "FAIL",
        "注入 %s += %g（mask %d px）：V6 %s（%s）"
        % (inj["frame"], amp, inj["mask_px"], v6_inj["verdict"], v6_inj["reason"]))
    del d_inj

    # 旧 V4 在同一注入输入上必须**不动**（盲区复现 = V6 非冗余）
    v4b = v6_base["v4_crosscheck"]["ratio"]
    v4i = v6_inj["v4_crosscheck"]["ratio"]
    add("V4-blind-on-injected-step",
        v4i is not None and v4i <= 1.5 and v4b is not None and v4b <= 1.5,
        "旧 V4 方差比：基线 %.6f → 注入已知台阶 %.6f（门 1.5）⇒ 判绿 = 对电平阶跃失明"
        % (v4b if v4b is not None else float("nan"),
           v4i if v4i is not None else float("nan")))

    # 负例：注入 0 ⇒ 度量必须逐项回落基线且判绿（真值无效应）
    d_zero = data.copy()
    apply_frame_injection(d_zero, hdr, p1_dirs, "0:0", frame_naxis=fnax[0])
    v6_zero = seam_level_step(d_zero, hdr, p1_dirs, **kw)
    keys = ("n_edges_valid", "n_exceed", "rel_med", "rel_p90", "rel_max", "ex_max")
    diff = [k for k in keys if v6_zero.get(k) != v6_base.get(k)]
    add("V6-zero-injection-identity",
        not diff and v6_zero["verdict"] == "PASS",
        "注入 0 ⇒ 度量与基线逐项一致（%s）且判绿；差异项 %s"
        % ("全部一致" if not diff else "有差异", diff or "无"))
    del d_zero

    bad = [c["name"] for c in cases if not c["ok"]]
    return {"cases": cases, "n_pass": len(cases) - len(bad), "n_total": len(cases),
            "failed": bad, "verdict": "PASS" if not bad else "FAIL"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fits", required=False)
    ap.add_argument("--out-dir", required=False)
    ap.add_argument("--tile", type=int, default=512)
    ap.add_argument("--group", default="gc")
    ap.add_argument("--max-seam-ratio", type=float, default=1.5,
                    help="V4 方差比门（只筛渲染分块/方差异常，不是帧间接缝判据）")
    ap.add_argument("--p1-dirs", nargs="+", default=None,
                    help="P1 帧目录（内含 <frame>/p1_wcs.json）——V6 电平阶跃判据的必需输入")
    ap.add_argument("--max-rel-step", type=float, default=1e-2,
                    help="V6 门：max|rel| 上限（默认 1e-2 = SCI-C R5「相对接缝度量 < 1%%」）")
    ap.add_argument("--frame-naxis", type=int, default=4096, help="帧像素边长（默认 4096）")
    ap.add_argument("--seam-min-samples", type=int, default=20,
                    help="V6 一条边界计入判据所需最少有效采样点数")
    ap.add_argument("--inject-frame", default=None,
                    help="V6 判别力自检：<k>:<amp> 注入第 k 帧足迹（amp=0 为负例）；注入模式不写 PNG")
    ap.add_argument("--no-seam-eval", action="store_true", dest="no_seam_eval",
                    help="显式跳过 V6（纯渲染用途）；报告记 SKIPPED_BY_FLAG，结论行同时打印")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="V6 能红能绿自检（合成夹具，不需要 --fits）")
    args = ap.parse_args()

    if args.self_test:
        out_dir = args.out_dir or os.path.join(REPO, "run/ci/seam-footprint")
        os.makedirs(out_dir, exist_ok=True)
        st = self_test(out_dir)
        rec = {"schema": "astrocs.vis-v6-selftest/v1", "tool": "render_vis",
               "mode": "v6-self-test", "max_rel_step": args.max_rel_step, **st}
        io.open(os.path.join(out_dir, "v6_selftest.json"), "w", encoding="utf-8").write(
            json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
        for c in st["cases"]:
            print("SELFTEST %s %s  %s" % ("PASS" if c["ok"] else "FAIL", c["name"], c["note"]))
        print("RENDER_VIS_V6_SELFTEST_%s: %d/%d -> %s"
              % (st["verdict"], st["n_pass"], st["n_total"],
                 os.path.join(out_dir, "v6_selftest.json")))
        return 0 if st["verdict"] == "PASS" else 1
    if not args.fits or not args.out_dir:
        print("RENDER_VIS_ERROR: 需 --fits 与 --out-dir（或 --self-test）", file=sys.stderr)
        return 2

    os.makedirs(args.out_dir, exist_ok=True)
    if not args.inject_frame:      # 注入模式不写 PNG/分块（只出判据证据）
        os.makedirs(os.path.join(args.out_dir, "tiles"), exist_ok=True)
    cov = None
    with fits.open(args.fits) as hdul:
        hdr = hdul[0].header
        data = np.asarray(hdul[0].data, dtype=np.float64)
        for h in hdul[1:]:
            if str(h.name).upper() == "COVERAGE" and h.data is not None:
                cov = np.asarray(h.data, dtype=np.float64)
                break

    # ---- 注入正/负例（判别力自检）----
    # 在**所有判据之前**改内存数组：V1–V6 看到的是同一份（注入后）数组；注入模式不写 PNG
    # （见下），绝不把注入了已知台阶的数组当成交付渲染图。
    injected, inject_error = None, None
    if args.inject_frame:
        try:
            sf = _seam_mod()
            spec = args.inject_frame
            ks, amps = spec.split(":", 1)
            frames = sf.collect_frames(args.p1_dirs or [])
            if not frames:
                raise RuntimeError("--inject-frame 需要 --p1-dirs（帧足迹）才能定位注入面")
            k, amp = int(ks), float(amps)
            if not (0 <= k < len(frames)):
                raise RuntimeError("--inject-frame 帧号 %d 越界（共 %d 帧）" % (k, len(frames)))
            wf, _ = sf.load_wcs_from_p1(frames[k])
            mask = sf.polygon_mask(
                sf.frame_edges_p3(wf, sf.wcs_from_header(hdr),
                                  (args.frame_naxis, args.frame_naxis)), data.shape)
            if amp:
                data[mask] += amp
            injected = {"spec": spec, "frame": os.path.basename(os.path.dirname(frames[k])),
                        "amp": amp, "mask_px": int(mask.sum())}
            del mask
        except Exception as exc:  # noqa: BLE001 - 注入不可用 = 判红，不静默
            inject_error = "V6 注入用例不可用（%s: %s）" % (type(exc).__name__, exc)

    fin = np.isfinite(data)
    n = data.size
    findings = []
    if inject_error:
        findings.append(inject_error)
    finite_fraction = float(fin.sum()) / n
    # V1 判据：产品必须"值域与覆盖域自洽"。请求画幅可以大于数据足迹 ⇒ 合法未覆盖区必然是
    # 非有限值，因此"全图必须全有限"既会误杀正确产品、又会把"覆盖平面说有数据而信号是 NaN"
    # 这个真缺陷混在一起看不出来。有覆盖平面时按两条互斥判据分别判。
    # V1b 对**所有**产品同判（与有无 COVERAGE 平面无关）：非有限像素按「是否与画幅边界
    # 4-连通」二分 —— 与边界连通 = 合法黑边；其余 = 内部空洞，判红（阈值 0）。
    hole_mask, boundary_nonfinite, n_hole_comp, largest_hole = hole_analysis(~fin)
    internal_hole_px = int(hole_mask.sum())
    cov_stats = {"boundary_nonfinite_px": boundary_nonfinite,
                 "internal_hole_px": internal_hole_px,
                 "internal_hole_components": n_hole_comp,
                 "internal_hole_largest_px": largest_hole}
    if cov is not None and cov.shape == data.shape:
        covered = cov > 0
        phantom = int((fin & ~covered).sum())       # 未覆盖却带着有限值
        cov_stats["covered_fraction"] = float(covered.mean())
        cov_stats["phantom_data_px"] = phantom
        # 诊断量（非判据）：覆盖域内的非有限像素数。含合法黑边的内侧像素，
        # 因此**不作为判据**——判据是上面的内部空洞（V1b）。
        cov_stats["covered_but_nonfinite_px"] = int((~fin & covered).sum())
        if phantom:
            findings.append("V1a 未覆盖却有值 phantom_data_px=%d" % phantom)
    if internal_hole_px:
        findings.append("V1b 内部空洞 internal_hole_px=%d（连通域 %d 个，最大 %d px；"
                        "阈值 0；被有限值包围的非有限像素 = 真缺陷）"
                        % (internal_hole_px, n_hole_comp, largest_hole))
    nz = int(np.count_nonzero(data[fin]))
    nonzero_fraction = nz / n
    if not nonzero_fraction > 0:
        findings.append("V2 nonzero_fraction == 0（空图）")
    vals = data[fin]
    p001 = float(np.percentile(vals, 0.1)) if vals.size else 0.0
    p999 = float(np.percentile(vals, 99.9)) if vals.size else 0.0
    dr = (p999 / p001) if p001 > 0 else float("inf")
    if not dr > 1.0:
        findings.append("V3 dynamic_range p99.9/p0.1 = %s <= 1（常数图）" % dr)
    ratio, detail = seam_metric(np.nan_to_num(data), args.tile)
    if ratio is None:
        findings.append("V4 无法计算接缝度量（有效区太小）")
    elif not ratio <= args.max_seam_ratio:
        findings.append("V4 seam_ratio=%.4f > %.2f（渲染分块/方差异常）" % (ratio, args.max_seam_ratio))

    # ---- V6：按真实帧足迹的电平阶跃判据（ACCEPTANCE_SPEC §6.2「帧间…无亮度/灰度阶跃」）----
    # V4 是方差比，对电平阶跃原理性失明；「无接缝」的判定权在 V6。
    if args.no_seam_eval:
        v6 = {"status": "SKIPPED_BY_FLAG", "verdict": None,
              "reason": "--no-seam-eval：V6 未评估；本次结论**不构成**「帧间无接缝」的证据"}
        print("  [WARN] V6 电平接缝判据被 --no-seam-eval 显式跳过：本报告不得引用为无接缝证据")
    elif not args.p1_dirs:
        v6 = {"status": "MISSING_INPUT", "verdict": "FAIL",
              "reason": "缺 --p1-dirs（帧足迹）⇒ 无法判定帧间电平接缝；V4 方差比对电平阶跃"
                        "原理性失明，不得代替（fail-closed 判红）"}
        findings.append("V6 " + v6["reason"])
    else:
        v6 = seam_level_step(data, hdr, args.p1_dirs, max_rel_step=args.max_rel_step,
                             min_samples=args.seam_min_samples,
                             frame_naxis=args.frame_naxis)
        if v6.get("verdict") == "FAIL":
            findings.append("V6 %s" % v6.get("reason"))
        elif v6.get("status") != "EVALUATED":
            findings.append("V6 未判定：%s" % v6.get("reason"))
    if injected:
        v6["injected"] = injected
        if v6.get("inject") and not v6["inject"].get("assert_ok"):
            findings.append("V6 注入正/负例未命中：%s" % v6["inject"].get("note"))

    nz_rows = np.where(np.any(data != 0, axis=1))[0]
    nz_cols = np.where(np.any(data != 0, axis=0))[0]
    if nz_rows.size and nz_cols.size:
        bbox_frac = ((nz_rows[-1] - nz_rows[0] + 1) * (nz_cols[-1] - nz_cols[0] + 1)) / n
    else:
        bbox_frac = 0.0
    if not bbox_frac >= 0.5:
        findings.append("V5 coverage bbox_frac=%.4f < 0.5（导出中心未落在覆盖内）" % bbox_frac)

    h, w = data.shape
    ntile = 0
    if injected:
        # 注入模式只出判据证据：绝不把注入了已知台阶的数组写成交付渲染图
        lo = hi = 0.0
        png_written = False
    else:
        png, lo, hi = stretch(data)
        Image.fromarray(png, mode="L").save(os.path.join(args.out_dir, "full.png"))
        for r in range(0, h, args.tile):
            for c in range(0, w, args.tile):
                sub = png[r:r + args.tile, c:c + args.tile]
                Image.fromarray(sub, mode="L").save(
                    os.path.join(args.out_dir, "tiles",
                                 "tile_r%d_c%d.png" % (r // args.tile, c // args.tile)))
                ntile += 1
        png_written = True

    rec = {"schema": "astrocs.vis-report/v1", "group": args.group, "fits": args.fits,
           "shape": [int(h), int(w)], "tile_px": args.tile, "n_tiles": ntile,
           "png_written": png_written, "injected": injected,
           "finite_fraction": finite_fraction, "nonzero_fraction": nonzero_fraction,
           "p0_1": p001, "p99_9": p999, "dynamic_range": dr,
           "min": float(vals.min()) if vals.size else 0.0,
           "max": float(vals.max()) if vals.size else 0.0,
           "median": float(np.median(vals)) if vals.size else 0.0,
           "stretch": {"mode": "asinh", "a": 0.05, "lo": lo, "hi": hi},
           "bbox_fraction": bbox_frac, "coverage_crosscheck": cov_stats,
           "seam_ratio": ratio, "seam_detail": detail, "max_seam_ratio": args.max_seam_ratio,
           "seam_level_step": v6, "max_rel_step": args.max_rel_step,
           "findings": findings, "verdict": "PASS" if not findings else "FAIL"}
    io.open(os.path.join(args.out_dir, "vis_report.json"), "w", encoding="utf-8").write(
        json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
    print("VIS_%s[%s]: %dx%d tiles=%d finite=%.4f nonzero=%.4f "
          "V4_seam_ratio=%s(方差比,粗筛) V6_level_step=%s -> %s"
          % (rec["verdict"], args.group, w, h, ntile, finite_fraction, nonzero_fraction,
             ("%.4f" % ratio) if ratio is not None else "n/a",
             ("%s(max|rel|=%.4g,门%.1g)" % (v6.get("verdict"), v6.get("rel_max", float("nan")),
                                             args.max_rel_step))
             if v6.get("status") == "EVALUATED" else v6.get("status"), args.out_dir))
    for f in findings:
        print("  " + f)
    return 0 if not findings else 1


if __name__ == "__main__":
    sys.exit(main())
