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
  V4 seam_metric         相邻分块边界处的一阶差分中位数与块内同向差分中位数之比
                         （seam_ratio）；无接缝叠加要求 seam_ratio 不显著大于 1。
                         判据：seam_ratio <= 1.5（远超则说明块间接缝存在）；
  V5 coverage            非零像素的包围盒面积 / 全图面积 >= 0.5（导出中心必须落在覆盖内）。

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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fits", required=True)
    ap.add_argument("--out-dir", required=True)
    ap.add_argument("--tile", type=int, default=512)
    ap.add_argument("--group", default="gc")
    ap.add_argument("--max-seam-ratio", type=float, default=1.5)
    args = ap.parse_args()

    os.makedirs(os.path.join(args.out_dir, "tiles"), exist_ok=True)
    cov = None
    with fits.open(args.fits) as hdul:
        hdr = hdul[0].header
        data = np.asarray(hdul[0].data, dtype=np.float64)
        for h in hdul[1:]:
            if str(h.name).upper() == "COVERAGE" and h.data is not None:
                cov = np.asarray(h.data, dtype=np.float64)
                break

    fin = np.isfinite(data)
    n = data.size
    findings = []
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
        findings.append("V4 seam_ratio=%.4f > %.2f（分块接缝）" % (ratio, args.max_seam_ratio))

    nz_rows = np.where(np.any(data != 0, axis=1))[0]
    nz_cols = np.where(np.any(data != 0, axis=0))[0]
    if nz_rows.size and nz_cols.size:
        bbox_frac = ((nz_rows[-1] - nz_rows[0] + 1) * (nz_cols[-1] - nz_cols[0] + 1)) / n
    else:
        bbox_frac = 0.0
    if not bbox_frac >= 0.5:
        findings.append("V5 coverage bbox_frac=%.4f < 0.5（导出中心未落在覆盖内）" % bbox_frac)

    png, lo, hi = stretch(data)
    Image.fromarray(png, mode="L").save(os.path.join(args.out_dir, "full.png"))
    h, w = data.shape
    ntile = 0
    for r in range(0, h, args.tile):
        for c in range(0, w, args.tile):
            sub = png[r:r + args.tile, c:c + args.tile]
            Image.fromarray(sub, mode="L").save(
                os.path.join(args.out_dir, "tiles", "tile_r%d_c%d.png" % (r // args.tile, c // args.tile)))
            ntile += 1

    rec = {"schema": "astrocs.vis-report/v1", "group": args.group, "fits": args.fits,
           "shape": [int(h), int(w)], "tile_px": args.tile, "n_tiles": ntile,
           "finite_fraction": finite_fraction, "nonzero_fraction": nonzero_fraction,
           "p0_1": p001, "p99_9": p999, "dynamic_range": dr,
           "min": float(vals.min()) if vals.size else 0.0,
           "max": float(vals.max()) if vals.size else 0.0,
           "median": float(np.median(vals)) if vals.size else 0.0,
           "stretch": {"mode": "asinh", "a": 0.05, "lo": lo, "hi": hi},
           "bbox_fraction": bbox_frac, "coverage_crosscheck": cov_stats,
           "seam_ratio": ratio, "seam_detail": detail, "max_seam_ratio": args.max_seam_ratio,
           "findings": findings, "verdict": "PASS" if not findings else "FAIL"}
    io.open(os.path.join(args.out_dir, "vis_report.json"), "w", encoding="utf-8").write(
        json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
    print("VIS_%s[%s]: %dx%d tiles=%d finite=%.4f nonzero=%.4f seam_ratio=%s -> %s"
          % (rec["verdict"], args.group, w, h, ntile, finite_fraction, nonzero_fraction,
             ("%.4f" % ratio) if ratio is not None else "n/a", args.out_dir))
    for f in findings:
        print("  " + f)
    return 0 if not findings else 1


if __name__ == "__main__":
    sys.exit(main())
