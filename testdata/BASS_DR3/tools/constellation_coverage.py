# -*- coding: utf-8 -*-
"""BASS DR3 覆盖天区 -> 星座归属分析

方法:
  1. 用 coords.csv 四角坐标栅格化巡天足印 (0.05° 网格, J2000)
  2. 足印格点经岁差转到 B1875 (Roman 1987 边界历元)
  3. 用 VizieR VI/42 (Roman 1987) 星座边界表逐格归属
  4. 输出每星座覆盖面积 (deg2) 与帧数, 保存 constellation_coverage.csv

边界数据: tools/roman1987_constellation_boundaries.tsv (VizieR VI/42, 随工具入库)
用法:
  py -3.12 constellation_coverage.py [--dir ..]
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import sys
from pathlib import Path

import numpy as np
from astropy.coordinates import SkyCoord, FK4
from astropy.time import Time

CELL_DEG = 0.05

# 星座 3 字母码 -> 中文名
CN = {
    "And": "仙女座", "Ant": "唧筒座", "Aps": "天燕座", "Aql": "天鹰座",
    "Aqr": "宝瓶座", "Ara": "天坛座", "Ari": "白羊座", "Aur": "御夫座",
    "Boo": "牧夫座", "Cae": "雕具座", "Cam": "鹿豹座", "Cap": "摩羯座",
    "Car": "船底座", "Cas": "仙后座", "Cen": "半人马座", "Cep": "仙王座",
    "Cet": "鲸鱼座", "Cha": "蝘蜓座", "Cir": "圆规座", "CMa": "大犬座",
    "CMi": "小犬座", "Cnc": "巨蟹座", "Col": "天鸽座", "Com": "后发座",
    "CrA": "南冕座", "CrB": "北冕座", "Crt": "巨爵座", "Cru": "南十字座",
    "Crv": "乌鸦座", "CVn": "猎犬座", "Cyg": "天鹅座", "Del": "海豚座",
    "Dor": "剑鱼座", "Dra": "天龙座", "Equ": "小马座", "Eri": "波江座",
    "For": "天炉座", "Gem": "双子座", "Gru": "天鹤座", "Her": "武仙座",
    "Hor": "时钟座", "Hya": "长蛇座", "Hyi": "水蛇座", "Ind": "印第安座",
    "Lac": "蝎虎座", "Leo": "狮子座", "Lep": "天兔座", "Lib": "天秤座",
    "LMi": "小狮座", "Lup": "豺狼座", "Lyn": "天猫座", "Lyr": "天琴座",
    "Men": "山案座", "Mic": "显微镜座", "Mon": "麒麟座", "Mus": "苍蝇座",
    "Nor": "矩尺座", "Oct": "南极座", "Oph": "蛇夫座", "Ori": "猎户座",
    "Pav": "孔雀座", "Peg": "飞马座", "Per": "英仙座", "Phe": "凤凰座",
    "Pic": "绘架座", "PsA": "南鱼座", "Psc": "双鱼座", "Pup": "船尾座",
    "Pyx": "罗盘座", "Ret": "网罟座", "Scl": "玉夫座", "Sco": "天蝎座",
    "Sct": "盾牌座", "Ser": "巨蛇座", "Sex": "六分仪座", "Sge": "天箭座",
    "Sgr": "人马座", "Tau": "金牛座", "Tel": "望远镜座", "Tri": "三角座",
    "TrA": "南三角座", "Tuc": "杜鹃座", "UMa": "大熊座", "UMi": "小熊座",
    "Vel": "船帆座", "Vir": "室女座", "Vol": "飞鱼座", "Vul": "狐狸座",
}


def load_boundaries(path: Path):
    """解析 VI/42 TSV -> (RA_low_h, RA_up_h, DE_low_deg, const) 列表, 按 DE_low 升序。

    格式: 首行表头 recno RA_low RA_up DE_low const, 数据行前有单位行与 ----- 分隔行。
    """
    rows = []
    in_data = False
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("#") or not line.strip():
            continue
        if line.strip().startswith("recno"):
            in_data = True
            continue
        if not in_data or set(line.strip()) <= set("- \t"):
            continue
        parts = line.split()
        if len(parts) != 5:
            continue
        try:
            ra_low = float(parts[1])
            ra_up = float(parts[2])
            de_low = float(parts[3])
        except ValueError:
            continue
        const = parts[4].strip()
        rows.append((ra_low, ra_up, de_low, const))
    rows.sort(key=lambda r: r[2])
    return rows


def assign(boundaries, ra_h: np.ndarray, dec_d: np.ndarray) -> np.ndarray:
    """RA 单位小时(0-24, B1875), Dec 单位度(B1875). 返回星座码数组。

    Roman 1987 表: 记录按 DE_low 降序扫描, 第一条
    (DE_low <= Dec 且 RA 落在 [RA_low, RA_up)) 的记录即归属星座。
    """
    out = np.empty(len(ra_h), dtype=object)
    out[:] = ""
    pending = np.ones(len(ra_h), dtype=bool)  # 尚未归属
    for ra_low, ra_up, de_low, const in sorted(boundaries, key=lambda r: -r[2]):
        mask = pending & (dec_d >= de_low)
        if mask.any():
            if ra_low <= ra_up:
                in_ra = (ra_h[mask] >= ra_low) & (ra_h[mask] < ra_up)
            else:  # 跨 0h
                in_ra = (ra_h[mask] >= ra_low) | (ra_h[mask] < ra_up)
            idx = np.nonzero(mask)[0][in_ra]
            out[idx] = const
            pending[idx] = False
        if not pending.any():
            break
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", type=Path, default=Path(__file__).resolve().parent.parent)
    args = ap.parse_args()
    base: Path = args.dir.resolve()

    boundaries = load_boundaries(base / "tools" / "roman1987_constellation_boundaries.tsv")
    print(f"[boundaries] rows={len(boundaries)}")

    # 1. 栅格化足印 (与 analyze_footprint 相同逻辑, 0.05°)
    rows = list(csv.DictReader(open(base / "coords.csv", encoding="utf-8-sig")))
    n_dec = int(round(140.0 / CELL_DEG))  # -55..85
    n_ra = int(round(360.0 / CELL_DEG))
    covered = np.zeros((n_dec, n_ra), dtype=np.int16)
    dec_vals = -55.0 + (np.arange(n_dec) + 0.5) * CELL_DEG
    dec_idx0 = np.arange(n_dec)

    def mark_frame(corners):
        xs, ys = corners[:, 0], corners[:, 1]
        area = 0.0
        for i in range(4):
            j = (i + 1) % 4
            area += xs[i] * ys[j] - xs[j] * ys[i]
        if area < 0:
            xs, ys = xs[::-1], ys[::-1]
        ymin, ymax = ys.min(), ys.max()
        xmin, xmax = xs.min(), xs.max()
        if xmax - xmin > 350 or ymax - ymin > 5:
            return
        i0 = max(int(np.searchsorted(dec_vals, ymin)) - 1, 0)
        i1 = min(int(np.searchsorted(dec_vals, ymax)) + 1, n_dec)
        c0 = int(np.floor((xmin - CELL_DEG) / CELL_DEG))
        c1 = int(np.ceil((xmax + CELL_DEG) / CELL_DEG))
        if c1 - c0 > n_ra:
            return
        for i in range(i0, i1):
            yy = dec_vals[i]
            if yy < ymin or yy > ymax:
                continue
            cols = np.arange(c0, c1 + 1)
            tx = cols * CELL_DEG
            inside = np.ones(len(cols), dtype=bool)
            for k in range(4):
                x1, y1 = xs[k], ys[k]
                x2, y2 = xs[(k + 1) % 4], ys[(k + 1) % 4]
                inside &= (x2 - x1) * (yy - y1) - (y2 - y1) * (tx - x1) >= 0
            if inside.any():
                covered[i, cols[inside] % n_ra] += 1

    for r in rows:
        base_ra = float(r["ra"])
        xs = np.array([float(r[k]) for k in ("ra_lb", "ra_lt", "ra_rt", "ra_rb")])
        ys = np.array([float(r[k]) for k in ("dec_lb", "dec_lt", "dec_rt", "dec_rb")])
        if (
            xs.max() - xs.min() < 0.1
            or ys.max() - ys.min() < 0.1
            or xs.max() - xs.min() > 1.0
            or ys.max() - ys.min() > 1.0
        ):
            continue
        d = xs - base_ra
        xs = np.where(d > 180, xs - 360, np.where(d < -180, xs + 360, xs))
        mark_frame(np.stack([xs, ys], axis=1))

    ii, jj = np.nonzero(covered > 0)
    ra_c = (jj + 0.5) * CELL_DEG
    dec_c = dec_vals[ii]
    print(f"[footprint] covered cells={len(ii)}  area~{len(ii)*CELL_DEG*CELL_DEG:,.0f} deg2")

    # 2. J2000 -> B1875
    sc = SkyCoord(ra_c, dec_c, unit="deg", frame="icrs")
    sc1875 = sc.transform_to(FK4(equinox=Time("B1875", scale="tt")))
    ra_h = sc1875.ra.hourangle
    dec_d = sc1875.dec.deg

    # 3. 归属
    const_cells = assign(boundaries, ra_h, dec_d)
    unassigned = int((const_cells == "").sum())
    print(f"[const] unassigned cells={unassigned}")

    area = {}
    for code in np.unique(const_cells[const_cells != ""]):
        area[code] = int((const_cells == code).sum()) * CELL_DEG * CELL_DEG

    # 4. 帧中心归属计数
    ra_f = np.array([float(r["ra"]) for r in rows])
    dec_f = np.array([float(r["dec"]) for r in rows])
    sf = SkyCoord(ra_f, dec_f, unit="deg", frame="icrs").transform_to(
        FK4(equinox=Time("B1875", scale="tt"))
    )
    const_frames = assign(boundaries, sf.ra.hourangle, sf.dec.deg)
    frame_cnt = {code: int((const_frames == code).sum()) for code in set(area) | set(np.unique(const_frames))}

    out_path = base / "constellation_coverage.csv"
    with open(out_path, "w", newline="", encoding="utf-8-sig") as fh:
        w = csv.writer(fh)
        w.writerow(["const", "cn", "area_deg2", "science_frames"])
        for code in sorted(area, key=lambda c: -area[c]):
            w.writerow([code, CN.get(code, ""), area[code], frame_cnt.get(code, 0)])
    print(f"[out] {out_path}")
    print(f"{'码':>4} {'星座':<6} {'面积deg2':>10} {'帧数':>8}")
    for code in sorted(area, key=lambda c: -area[c]):
        print(f"{code:>4} {CN.get(code,''):<8} {area[code]:>10,.0f} {frame_cnt.get(code,0):>8,}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
