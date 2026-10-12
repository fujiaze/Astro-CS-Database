#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""跨指向组（M1…M6）天区覆盖的**独立复核**（不依赖 P5 的 flux+WCS 匹配路径）。

数据源：**输入亮场原始 `.fts` 的头**（PinPoint 6.1.3 已解算的 TAN + 完整 CD 矩阵）。
理由：头里的 CRVAL/CRPIX/CD 是拍摄与解算时写定的**直接**指向与尺度，
与任何下游通量匹配、星表配对无关；4096×4096 的角点经 TAN 反投影即得 footprint。

方法：
  1. 逐帧读头（CRVAL/CRPIX/CD/NAXIS/FILTER/DATE-OBS）；
  2. 四角像素 → 天球（TAN 反投影）→ 投影到以全体现测中心为原点的切平面；
  3. 多边形取交（Sutherland–Hodgman，凸多边形）；面积比 = 交/较小者；
  4. 组并集、组间两两交叠比例、组内指向离散。

输出 results/p1fix/footprint_overlap.json
"""
from __future__ import annotations
import json, math, re, itertools
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[4]
BASE = ROOT / "testdata/M42_T2T3_mosaic_Flying_dutchman"
OUT = ROOT / "实验/photometric-magnitude/results/p1fix"
D2R = math.pi / 180.0


def read_header(path):
    with open(path, "rb") as f:
        hdr = f.read(2880 * 4)
    cards = []
    for i in range(0, len(hdr), 80):
        c = hdr[i:i + 80].decode("ascii", errors="replace")
        if c.startswith("END"):
            break
        if "=" in c:
            k = c[:8].strip()
            v = c[10:].split("/")[0].strip()
            cards.append((k, v))
    d = dict(cards)
    out = {}
    for k in ("CRVAL1", "CRVAL2", "CRPIX1", "CRPIX2", "CD1_1", "CD1_2", "CD2_1", "CD2_2",
              "CDELT1", "CDELT2"):
        if k in d:
            try:
                out[k] = float(d[k])
            except ValueError:
                pass
    for k in ("NAXIS1", "NAXIS2"):
        if k in d:
            try:
                out[k] = int(float(d[k]))
            except ValueError:
                pass
    out["FILTER"] = d.get("FILTER", "").strip().strip("'").strip()
    out["DATE-OBS"] = d.get("DATE-OBS", "").strip().strip("'")
    out["OBJECT"] = d.get("OBJECT", "").strip().strip("'")
    out["CTYPE1"] = d.get("CTYPE1", "")
    out["CTYPE2"] = d.get("CTYPE2", "")
    return out


def pix_to_sky(x, y, h):
    """TAN 反投影（标准坐标 → 天球）。x,y 为 0 基像素。"""
    dx = x + 1.0 - h["CRPIX1"]
    dy = y + 1.0 - h["CRPIX2"]
    xi = (h["CD1_1"] * dx + h["CD1_2"] * dy) * D2R
    eta = (h["CD2_1"] * dx + h["CD2_2"] * dy) * D2R
    ra0 = h["CRVAL1"] * D2R
    dec0 = h["CRVAL2"] * D2R
    rho = math.hypot(xi, eta)
    if rho == 0:
        return h["CRVAL1"], h["CRVAL2"]
    c = math.atan(rho)
    dec = math.asin(math.cos(c) * math.sin(dec0) + eta * math.sin(c) * math.cos(dec0) / rho)
    ra = ra0 + math.atan2(xi * math.sin(c),
                          rho * math.cos(dec0) * math.cos(c) - eta * math.sin(dec0) * math.sin(c))
    return (math.degrees(ra) % 360.0), math.degrees(dec)


def corners(h):
    w = h["NAXIS1"] - 1
    ht = h["NAXIS2"] - 1
    px = [(0, 0), (w, 0), (w, ht), (0, ht)]
    return [pix_to_sky(x, y, h) for x, y in px]


def project(pts, ra0, dec0):
    """切平面（原点 ra0,dec0）投影，单位 deg。"""
    out = []
    for ra, dec in pts:
        dra = (ra - ra0 + 180.0) % 360.0 - 180.0
        out.append((dra * math.cos(dec0 * D2R), dec - dec0))
    return out


def clip_convex(subject, clipper):
    """Sutherland–Hodgman：用凸 clipper 裁 subject（逆时针假定）。"""
    def inside(p, a, b):
        # subject/clipper 均为 ccw(...) 归一后的逆时针多边形 ⇒ 内侧 = 叉积 > 0
        return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0]) >= 0
    def inter(p, q, a, b):
        # 线 p-q 与 a-b 的交
        x1, y1 = p; x2, y2 = q; x3, y3 = a; x4, y4 = b
        den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
        if abs(den) < 1e-18:
            return q
        px = ((x1 * y2 - y1 * x2) * (x3 - x4) - (x1 - x2) * (x3 * y4 - y3 * x4)) / den
        py = ((x1 * y2 - y1 * x2) * (y3 - y4) - (y1 - y2) * (x3 * y4 - y3 * x4)) / den
        return (px, py)
    out = list(subject)
    n = len(clipper)
    for i in range(n):
        a = clipper[i]; b = clipper[(i + 1) % n]
        inp = out
        out = []
        if not inp:
            break
        for j in range(len(inp)):
            cur = inp[j]; prv = inp[j - 1]
            if inside(cur, a, b):
                if not inside(prv, a, b):
                    out.append(inter(prv, cur, a, b))
                out.append(cur)
            elif inside(prv, a, b):
                out.append(inter(prv, cur, a, b))
    return out


def area(poly):
    if len(poly) < 3:
        return 0.0
    a = 0.0
    for i in range(len(poly)):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % len(poly)]
        a += x1 * y2 - x2 * y1
    return abs(a) / 2.0


def ccw(poly):
    a = 0.0
    for i in range(len(poly)):
        x1, y1 = poly[i]; x2, y2 = poly[(i + 1) % len(poly)]
        a += x1 * y2 - x2 * y1
    return poly if a > 0 else poly[::-1]


def main():
    frames = []
    for t in ("T2", "T3"):
        for m in ("M1", "M2", "M3", "M4", "M5", "M6"):
            d = BASE / t / m
            if not d.is_dir():
                continue
            for fp in sorted(d.glob("*.fts")):
                h = read_header(fp)
                if "CRVAL1" not in h or "CD1_1" not in h or "NAXIS1" not in h:
                    continue
                frames.append({"telescope": t, "group": m, "path": str(fp.relative_to(ROOT)),
                               "file": fp.name, "h": h, "corners": corners(h)})
    if not frames:
        print("no frames"); return 1
    ra0 = float(np.mean([f["h"]["CRVAL1"] for f in frames]))
    dec0 = float(np.mean([f["h"]["CRVAL2"] for f in frames]))
    for f in frames:
        poly = project(f["corners"], ra0, dec0)
        f["poly"] = ccw(poly)
        f["area"] = area(f["poly"])
        f["fov_x"] = f["h"]["NAXIS1"] * abs(f["h"]["CDELT1"])
        f["fov_y"] = f["h"]["NAXIS2"] * abs(f["h"]["CDELT2"])

    print(f"frames with solved WCS: {len(frames)}   tangent origin RA={ra0:.5f} Dec={dec0:.5f}")
    print()
    print("=== 各组中心指向与视场 ===")
    print(f"{'组':6s} {'n':>3s} {'CRVAL1中位':>12s} {'CRVAL2中位':>12s} {'跨度"':>8s} "
          f"{'FoV_x':>7s} {'FoV_y':>7s} {'并集面积deg²':>12s} {'并集/FoV':>9s}")
    groups = {}
    for f in frames:
        groups.setdefault(f"{f['telescope']}/{f['group']}", []).append(f)
    table = {}
    for g in sorted(groups):
        v = groups[g]
        c1 = np.median([f["h"]["CRVAL1"] for f in v])
        c2 = np.median([f["h"]["CRVAL2"] for f in v])
        span = math.hypot((max(f["h"]["CRVAL1"] for f in v) - min(f["h"]["CRVAL1"] for f in v)) * math.cos(dec0 * D2R) * 3600,
                          (max(f["h"]["CRVAL2"] for f in v) - min(f["h"]["CRVAL2"] for f in v)) * 3600)
        # 并集面积：用格点采样（组内帧少，采样足够）
        xs = [p[0] for f in v for p in f["poly"]]; ys = [p[1] for f in v for p in f["poly"]]
        x0, x1 = min(xs), max(xs); y0, y1 = min(ys), max(ys)
        NX = NY = 400
        cnt = 0
        for i in range(NX):
            px = x0 + (x1 - x0) * (i + 0.5) / NX
            for j in range(NY):
                py = y0 + (y1 - y0) * (j + 0.5) / NY
                for f in v:
                    poly = f["poly"]
                    ins = True
                    for k in range(len(poly)):
                        a = poly[k]; b = poly[(k + 1) % len(poly)]
                        if (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]) < 0:
                            ins = False; break
                    if ins:
                        cnt += 1; break
        ua = cnt / (NX * NY) * (x1 - x0) * (y1 - y0)
        fov = v[0]["fov_x"] * v[0]["fov_y"]
        table[g] = {"n": len(v), "crval1_med": c1, "crval2_med": c2, "span_arcsec": span,
                    "fov_x": v[0]["fov_x"], "fov_y": v[0]["fov_y"],
                    "union_area": ua, "union_over_fov": ua / fov}
        print(f"{g:6s} {len(v):3d} {c1:12.5f} {c2:12.5f} {span:8.1f} "
              f"{v[0]['fov_x']:7.4f} {v[0]['fov_y']:7.4f} {ua:12.4f} {ua/fov:9.2f}")

    print()
    print("=== 组间两两交叠（交面积 / 较小者）===")
    keys = sorted(groups)
    print("        " + "".join(f"{k.split('/')[1]:>10s}" for k in keys))
    pair = {}
    for a in keys:
        rowtxt = f"{a:7s} "
        for b in keys:
            best = 0.0
            for fa in groups[a]:
                for fb in groups[b]:
                    inter = clip_convex(fa["poly"], fb["poly"])
                    ar = area(inter)
                    if ar > 0:
                        frac = ar / min(fa["area"], fb["area"])
                        best = max(best, frac)
            pair[(a, b)] = best
            rowtxt += f"{best*100:9.1f}%" if a != b or True else " " * 10
        print(rowtxt)

    print()
    print("=== 判定用：跨组/同组统计 ===")
    cross = [v for (a, b), v in pair.items() if a.split("/")[0] == b.split("/")[0] and a.split("/")[1] != b.split("/")[1]]
    same = [v for (a, b), v in pair.items() if a != b and a.split("/")[0] == b.split("/")[0] and a.split("/")[1] == b.split("/")[1]]
    print(f"  同望远镜跨组对 n={len(cross)}  有交叠(>0) 比例={sum(1 for v in cross if v>0)/len(cross)*100:.0f}%  "
          f"中位交叠={np.median(cross)*100:.1f}%  max={max(cross)*100:.1f}%")
    print(f"  跨望远镜同组对 n={len(same)}  有交叠比例={sum(1 for v in same if v>0)/len(same)*100:.0f}%  "
          f"中位交叠={np.median(same)*100:.1f}%" if same else "  跨望远镜同组对 n=0")

    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "footprint_overlap.json").write_text(json.dumps({
        "source": "input light .fts headers (PinPoint TAN+CD)",
        "tangent_origin": {"ra_deg": ra0, "dec_deg": dec0},
        "n_frames": len(frames), "groups": table,
        "pair_overlap": {f"{a}|{b}": v for (a, b), v in pair.items()},
        "frames": [{"telescope": f["telescope"], "group": f["group"], "file": f["file"],
                    "crval1": f["h"]["CRVAL1"], "crval2": f["h"]["CRVAL2"],
                    "crpix1": f["h"]["CRPIX1"], "crpix2": f["h"]["CRPIX2"],
                    "cd": [f["h"].get("CD1_1"), f["h"].get("CD1_2"), f["h"].get("CD2_1"), f["h"].get("CD2_2")],
                    "naxis": [f["h"]["NAXIS1"], f["h"]["NAXIS2"]],
                    "filter": f["h"]["FILTER"], "date_obs": f["h"]["DATE-OBS"],
                    "corner_ra_dec": f["corners"]} for f in frames],
    }, indent=1, ensure_ascii=False))
    print(f"\nwrote {OUT/'footprint_overlap.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
