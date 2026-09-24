#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""frame_qc_grid -- 星点质量九宫格视觉确认工具 (ACSD 项目常备工具).

用途
----
把**任意一帧**天文图像自动拉伸后，在 **9 个固定位置**各裁一块 tile x tile 的
子图，拼成一张 (grid*tile) x (grid*tile) 的 PNG，供人眼直接确认星点质量；
同时输出逐块的**机器可读指标**，供批量统计与回归对比。

**可选叠加**：给定一个 WCS 解（解算结果 JSON 或帧头 WCS）与一份星表
（RA/Dec），把星表星点**逆投影**回像素并画**红色十字**，用于"解出来的 WCS
到底对不对、星点是否真的落在星上"的视觉确认。成功帧若 RMS 偏大或任何统计
异常，都应用这个模式复核。

9 块位置 (grid=3，负责人指定)
-----------------------------
取图像宽高的 (1/6, 3/6, 5/6) 三个分数位置做笛卡尔积，即：

    +-------------+-------------+-------------+
    | 角 左上     | 边 上中     | 角 右上     |   row 0
    +-------------+-------------+-------------+
    | 边 左中     | **中心**    | 边 右中     |   row 1
    +-------------+-------------+-------------+
    | 角 左下     | 边 下中     | 角 右下     |   row 2
    +-------------+-------------+-------------+

即 **中心 1 块 + 4 个角 + 4 个边中点 = 9 块**。理由：拼接缝隙、场边缘畸变、
渐晕、坏列/坏行恰好在**角与边中点**；4 宫（2x2 取每块中心）只覆盖 4 个象限
中心，恰好漏掉最需要检查的位置；中心块作为"场中心最好"的对照基准。
一般化：grid=G 时分数位置为 (2i+1)/(2G), i=0..G-1；只有 G=3 才是指定形态。

拉伸 (stretch)
--------------
    auto   (默认) asinh：black = median + 0.5*1.4826*MAD，white = p(hi)
    asinh        同 asinh 公式，black/white 由 --percentiles p_lo,p_hi 给出
    zscale       IRAF zscale（--zcontrast）；线性映射
这些帧是原始亮场（无暗平场/坏点校正），max 常被单像素热像素顶到 65535，
用 max 当白点会把整幅压黑；MAD 不被星点抬高，故 auto 用 MAD 定黑点。

用法
----
    # 单帧 / 批量目录
    python3 eng/tools/quality/frame_qc_grid.py --input frame.fts --outdir /tmp/qc
    python3 eng/tools/quality/frame_qc_grid.py --input testdata/xxx/lights --outdir /tmp/qc

    # 参数 + 标注版
    python3 eng/tools/quality/frame_qc_grid.py --input frame.fts --outdir /tmp/qc \\
        --tile 100 --grid 3 --stretch auto --percentiles 0.1,99.9 --label

    # WCS + 星表 逆投影红十叉叠加（视觉确认 WCS 是否正确）
    python3 eng/tools/quality/frame_qc_grid.py --input frame.fts --outdir /tmp/qc \\
        --wcs-json solve.json --overlay gaia.csv --overlay-tol 2.0

    # 自检（合成帧 + 校验 PNG 尺寸/9 块指标，不依赖任何仓库数据）
    python3 eng/tools/quality/frame_qc_grid.py --selftest

WCS 来源（--wcs-json 缺省时用帧头 WCS，两者都可无）
    --wcs-json  解算结果 JSON，至少含 crpix[2]/crval[2]/cd[4]，可选 ctype1/ctype2
                与 sip_a/sip_b/sip_ap/sip_bp（36 项，i*6+j 布局）+ sip_order/
                sip_ap_order。ipv 的 ipv_solve_* 输出与本工程探针的 .json 均满足。
星表来源（--overlay，可多份；三者按扩展名自动识别）
    *.csv   含 ra/dec（或 RA/DEC）两列，单位度
    *.bin   本工程探针 <prefix>.W.bin 格式（int32[2]=(n,4) 头 + float64 n x 4，
            列序 x,y,ra,dec）
    *.json  [{"ra":..,"dec":..}, ...]

产物
----
    <outdir>/<stem>.grid9.png             (grid*tile)^2 合成图，默认 300x300；
                                          有叠加时为 RGB（红十叉），否则灰度
    <outdir>/<stem>.grid9.labeled.png     --label 时额外输出：1px 分隔 + 9 行
                                          (row,col,x0,y0) 回查表的标注版
    <outdir>/<stem>.grid9.json            逐块指标 + 各块在母帧中的原点 + 拉伸参数
                                          + 叠加配对统计
    <outdir>/frame_qc_grid_summary.csv    批量汇总（一行一帧一块）

依赖
----
必需 numpy；astropy **可选**（FITS 读取与 WCS 逆投影优先用它，缺失时自动降级
为内置纯 numpy FITS 读取器与内置 TAN+CD+SIP(AP/BP) 逆投影；两者都不可用或文件
不是 FITS 时给出明确报错并非 0 退出，不静默产出坏图）。scipy 可选（连通域标记）；
Pillow 可选（仅用于 PNG 输出，缺失时明确报错）。

退出码：0 成功；2 参数/依赖错误；3 读取/写图失败。
"""
from __future__ import annotations

import argparse
import csv as _csv
import json
import math
import os
import sys
import tempfile

# --------------------------------------------------------------------------
# 依赖探测（最小化依赖 + 清晰报错）
# --------------------------------------------------------------------------
try:
    import numpy as np
except Exception as exc:  # pragma: no cover
    sys.stderr.write("[frame_qc_grid] FATAL: numpy is required (%s)\n" % exc)
    raise SystemExit(2)

try:
    from PIL import Image
except Exception:
    Image = None

try:
    from astropy.io import fits as _astropy_fits
except Exception:
    _astropy_fits = None

try:
    from astropy.wcs import WCS as _AstropyWCS
    from astropy.wcs import Sip as _AstropySip
except Exception:
    _AstropyWCS = None
    _AstropySip = None

try:
    from scipy import ndimage as _ndimage
except Exception:
    _ndimage = None

ARCSEC_PER_DEG = 3600.0


# --------------------------------------------------------------------------
# FITS 读取：astropy 优先，缺失时用内置纯 numpy 读取器
# --------------------------------------------------------------------------
def read_fits(path):
    """-> (data float64 2D, header_like dict).  Raises RuntimeError on failure."""
    if _astropy_fits is not None:
        with _astropy_fits.open(path, memmap=False) as hdul:
            hdu = hdul[0]
            data = np.asarray(hdu.data)
            hdr = hdu.header
            meta = {
                "bitpix": int(hdr.get("BITPIX", 0)),
                "bscale": float(hdr.get("BSCALE", 1.0) or 1.0),
                "bzero": float(hdr.get("BZERO", 0.0) or 0.0),
                "date_obs": hdr.get("DATE-OBS"), "filter": hdr.get("FILTER"),
                "exptime": hdr.get("EXPTIME"), "object": hdr.get("OBJECT"),
                "instrume": hdr.get("INSTRUME"), "telescop": hdr.get("TELESCOP"),
                "crval1": hdr.get("CRVAL1"), "crval2": hdr.get("CRVAL2"),
                "has_header_wcs": hdr.get("CRVAL1") is not None and hdr.get("CRVAL2") is not None,
                "reader": "astropy",
            }
            header_wcs = None
            if meta["has_header_wcs"]:
                try:
                    header_wcs = _AstropyWCS(hdr)
                except Exception:
                    header_wcs = None
        return np.asarray(data, dtype=np.float64), meta, header_wcs
    data, meta = _read_fits_builtin(path)
    return data, meta, None


def _read_fits_builtin(path):
    """Minimal primary-HDU FITS reader (BITPIX 8/16/32/-32/-64, BZERO/BSCALE)."""
    with open(path, "rb") as fh:
        cards = b""
        while True:
            blk = fh.read(2880)
            if len(blk) != 2880:
                raise RuntimeError("truncated FITS header: %s" % path)
            cards += blk
            if any(blk[i * 80:i * 80 + 3] == b"END" for i in range(36)):
                break
        hdr = {}
        for i in range(0, len(cards), 80):
            card = cards[i:i + 80]
            key = card[:8].decode("ascii", "replace").strip()
            if not key or key in ("COMMENT", "HISTORY", "END"):
                continue
            if card[8:10] == b"= ":
                raw = card[10:].decode("ascii", "replace")
                if raw.lstrip().startswith("'"):
                    end = raw.find("'", raw.find("'") + 1)
                    hdr[key] = raw[raw.find("'") + 1:end].strip()
                else:
                    tok = raw.split("/")[0].strip()
                    try:
                        hdr[key] = int(tok)
                    except ValueError:
                        try:
                            hdr[key] = float(tok.replace("D", "E").replace("d", "e"))
                        except ValueError:
                            hdr[key] = tok
        nx, ny = int(hdr.get("NAXIS1", 0)), int(hdr.get("NAXIS2", 0))
        bitpix = int(hdr.get("BITPIX", 0))
        if nx <= 0 or ny <= 0:
            raise RuntimeError("NAXIS1/NAXIS2 missing in %s" % path)
        dtype = {8: ">i1", 16: ">i2", 32: ">i4", -32: ">f4", -64: ">f8"}.get(bitpix)
        if dtype is None:
            raise RuntimeError("unsupported BITPIX=%s in %s" % (bitpix, path))
        n = nx * ny
        raw = fh.read(int(np.dtype(dtype).itemsize) * n)
        if len(raw) != np.dtype(dtype).itemsize * n:
            raise RuntimeError("truncated FITS data: %s" % path)
        data = np.frombuffer(raw, dtype=dtype).astype(np.float64).reshape(ny, nx)
        bscale = float(hdr.get("BSCALE", 1.0) or 1.0)
        bzero = float(hdr.get("BZERO", 0.0) or 0.0)
        if bscale != 1.0 or bzero != 0.0:
            data = data * bscale + bzero
        meta = {"bitpix": bitpix, "bscale": bscale, "bzero": bzero,
                "date_obs": hdr.get("DATE-OBS"), "filter": hdr.get("FILTER"),
                "exptime": hdr.get("EXPTIME"), "object": hdr.get("OBJECT"),
                "instrume": hdr.get("INSTRUME"), "telescop": hdr.get("TELESCOP"),
                "crval1": hdr.get("CRVAL1"), "crval2": hdr.get("CRVAL2"),
                "has_header_wcs": False, "reader": "builtin"}
        return data, meta


# --------------------------------------------------------------------------
# 拉伸
# --------------------------------------------------------------------------
def _mad(a):
    med = float(np.median(a))
    return med, 1.4826 * float(np.median(np.abs(a - med)))


def _zscale(a, contrast=0.25, maxiter=5, sample=10000):
    flat = a.ravel()
    if flat.size > sample:
        flat = flat[::max(1, flat.size // sample)]
    flat = np.sort(flat.astype(np.float64))
    n = flat.size
    if n < 10:
        return float(flat[0]), float(flat[-1])
    lo, hi = 0, n - 1
    for _ in range(maxiter):
        span = flat[hi] - flat[lo]
        if span <= 0:
            break
        mid = 0.5 * (flat[lo] + flat[hi])
        idx = np.arange(lo, hi + 1)
        keep = np.abs(flat[lo:hi + 1] - mid) < 0.5 * span
        if keep.all():
            break
        sel = idx[keep]
        if sel.size < 2:
            break
        lo, hi = int(sel[0]), int(sel[-1])
    z1, z2 = float(flat[lo]), float(flat[hi])
    if z2 <= z1:
        z2 = z1 + 1.0
    return z1, z2


def stretch_to_uint8(tile, mode="auto", percentiles=(0.1, 99.9), contrast=0.25):
    a = np.asarray(tile, dtype=np.float64)
    p_lo, p_hi = float(percentiles[0]), float(percentiles[1])
    if mode in ("auto", "asinh"):
        if mode == "auto":
            med, mad = _mad(a)
            black = med + 0.5 * mad
            white = float(np.percentile(a, p_hi))
        else:
            black = float(np.percentile(a, p_lo))
            white = float(np.percentile(a, p_hi))
        if not np.isfinite(black):
            black = float(np.min(a))
        if not np.isfinite(white) or white <= black:
            white = black + max(1.0, abs(black) * 1e-3)
        x = np.clip((a - black) / (white - black), 0.0, 1.0)
        y = np.arcsinh(10.0 * x) / math.asinh(10.0)
        info = {"mode": "auto" if mode == "auto" else "asinh",
                "black": black, "white": white, "a": 10.0,
                "percentiles": [p_lo, p_hi]}
    elif mode == "zscale":
        z1, z2 = _zscale(a, contrast=contrast)
        y = np.clip((a - z1) / (z2 - z1), 0.0, 1.0)
        info = {"mode": "zscale", "z1": z1, "z2": z2, "contrast": contrast}
    else:
        raise ValueError("unknown stretch mode: %s" % mode)
    return (y * 255.0 + 0.5).astype(np.uint8), info


# --------------------------------------------------------------------------
# WCS：astropy 优先，缺失时用内置 TAN + CD + SIP(AP/BP) 逆投影
# --------------------------------------------------------------------------
class _BuiltinWCS(object):
    """TAN + CD + inverse SIP (AP/BP) sky -> pixel (1-based FITS)."""

    def __init__(self, crpix, crval, cd, ap=None, bp=None, ap_order=0):
        self.crpix = [float(crpix[0]), float(crpix[1])]
        self.crval = [float(crval[0]), float(crval[1])]
        self.cd = np.array([[float(cd[0]), float(cd[1])],
                            [float(cd[2]), float(cd[3])]], dtype=np.float64)
        self.cdinv = np.linalg.inv(self.cd)
        self.ap = np.array(ap, dtype=np.float64).reshape(6, 6) if ap else None
        self.bp = np.array(bp, dtype=np.float64).reshape(6, 6) if bp else None
        self.ap_order = int(ap_order)

    @staticmethod
    def _poly(p, order, x, y):
        s = 0.0
        for i in range(order + 1):
            for j in range(order + 1 - i):
                s += p[i, j] * (x ** i) * (y ** j)
        return s

    def sky_to_pixel(self, ra, dec):
        d2r = math.pi / 180.0
        ra_r, dec_r = ra * d2r, dec * d2r
        r0, d0 = self.crval[0] * d2r, self.crval[1] * d2r
        cosc = math.sin(d0) * math.sin(dec_r) + math.cos(d0) * math.cos(dec_r) * math.cos(ra_r - r0)
        if cosc <= 1e-12:
            return None
        xi = math.cos(dec_r) * math.sin(ra_r - r0) / cosc
        eta = (math.cos(d0) * math.sin(dec_r) -
               math.sin(d0) * math.cos(dec_r) * math.cos(ra_r - r0)) / cosc
        u, v = self.cdinv.dot(np.array([xi, eta]))
        if self.ap is not None:
            du = self._poly(self.ap, self.ap_order, u, v)
            dv = self._poly(self.bp, self.ap_order, u, v)
        else:
            du = dv = 0.0
        return (self.crpix[0] + u + du, self.crpix[1] + v + dv)


class _AstropyWCSWrap(object):
    def __init__(self, w):
        self._w = w

    def sky_to_pixel(self, ra, dec):
        try:
            xy = self._w.all_world2pix(np.array([ra]), np.array([dec]), 1)
            x, y = float(xy[0][0]), float(xy[1][0])
        except Exception:
            return None
        if not (np.isfinite(x) and np.isfinite(y)):
            return None
        return (x, y)


def load_wcs_json(path):
    with open(path, encoding="utf-8") as fh:
        d = json.load(fh)
    crpix, crval = d.get("crpix"), d.get("crval")
    cd = d.get("cd")
    if crpix is None or crval is None or cd is None:
        raise RuntimeError("--wcs-json needs crpix[2]/crval[2]/cd[4]: %s" % path)
    sip_order = int(d.get("sip_order") or 0)
    ap_order = int(d.get("sip_ap_order") or 0)
    a, b = d.get("sip_a"), d.get("sip_b")
    ap, bp = d.get("sip_ap"), d.get("sip_bp")
    ctype = [d.get("ctype1") or "RA---TAN-SIP", d.get("ctype2") or "DEC--TAN-SIP"]
    if _AstropyWCS is not None and _AstropySip is not None and sip_order > 0 and a and b:
        try:
            w = _AstropyWCS(naxis=2)
            w.wcs.crpix = [float(crpix[0]), float(crpix[1])]
            w.wcs.crval = [float(crval[0]), float(crval[1])]
            w.wcs.cd = np.array([[float(cd[0]), float(cd[1])],
                                 [float(cd[2]), float(cd[3])]], dtype=np.float64)
            w.wcs.ctype = ctype
            am = np.array(a, dtype=np.float64).reshape(6, 6)
            bm = np.array(b, dtype=np.float64).reshape(6, 6)
            apm = np.array(ap or np.zeros(36), dtype=np.float64).reshape(6, 6)
            bpm = np.array(bp or np.zeros(36), dtype=np.float64).reshape(6, 6)
            w.sip = _AstropySip(am, bm, apm, bpm,
                                [float(crpix[0]), float(crpix[1])])
            return _AstropyWCSWrap(w), "astropy"
        except Exception:
            pass
    if ap is None:
        ap = np.zeros(36)
        bp = np.zeros(36)
        ap_order = 0
    return _BuiltinWCS(crpix, crval, cd, ap, bp, ap_order), "builtin"


# --------------------------------------------------------------------------
# 星表读取（--overlay）
# --------------------------------------------------------------------------
def load_catalog(path):
    """-> [(ra_deg, dec_deg, mag_or_nan), ...].  Supported:
    *.csv  ra/dec[/mag] 列名大小写不敏感
    *.bin  探针 .W.bin (int32[2]=(n,4) 头 + float64 n x 4，列序 x,y,ra,dec)
           或探针 .gcat.bin (int32[2]=(n,4) 头 + float64 n x 4，列序 ra,dec,mag,source_id)
    *.json [{"ra":..,"dec":..,"mag":..}, ...]
    由列内容自动判别 ra/dec 列（前两列若像角度即认为 (ra,dec[,mag])）。"""
    ext = os.path.splitext(path)[1].lower()
    if ext == ".bin":
        with open(path, "rb") as fh:
            n, c = np.frombuffer(fh.read(8), dtype="<i4")
            arr = np.frombuffer(fh.read(), dtype="<f8").reshape(int(n), int(c))
        out = []
        for r in arr:
            a, b = float(r[0]), float(r[1])
            if -0.1 <= a <= 360.1 and -90.1 <= b <= 90.1:
                # 前两列已是 (ra, dec)  => 探针 .gcat.bin 布局
                out.append((a, b, float(r[2]) if c >= 3 else float("nan")))
            elif c >= 4:
                out.append((float(r[2]), float(r[3]), float("nan")))
        return out
    if ext == ".json":
        with open(path, encoding="utf-8") as fh:
            d = json.load(fh)
        out = []
        for e in d:
            ra = e.get("ra", e.get("RA"))
            dec = e.get("dec", e.get("DEC"))
            if ra is not None and dec is not None:
                out.append((float(ra), float(dec),
                            float(e.get("mag", e.get("magG", float("nan"))))))
        return out
    out = []
    with open(path, newline="", encoding="utf-8") as fh:
        rd = _csv.DictReader(fh)
        names = {k.lower(): k for k in (rd.fieldnames or [])}
        kr = names.get("ra") or names.get("_ra") or names.get("raj2000")
        kd = names.get("dec") or names.get("_dec") or names.get("dej2000")
        km = names.get("mag") or names.get("magg") or names.get("mag_g") or names.get("gmag")
        if kr is None or kd is None:
            raise RuntimeError("catalog %s needs ra/dec columns" % path)
        for row in rd:
            try:
                out.append((float(row[kr]), float(row[kd]),
                            float(row[km]) if km and row.get(km) not in (None, "") else float("nan")))
            except Exception:
                continue
    return out


def build_overlay(catalog, wcs, width, height, tile, grid, origins, tol=2.0):
    """逆投影 + 画十叉用像素坐标。-> (list of (x,y), per-tile stats)"""
    pix = []
    for (ra, dec) in catalog:
        xy = wcs.sky_to_pixel(ra, dec)
        if xy is None:
            continue
        x, y = xy
        if not (np.isfinite(x) and np.isfinite(y)):
            continue
        if -tol <= x <= width + tol and -tol <= y <= height + tol:
            pix.append((float(x), float(y)))
    return pix


# --------------------------------------------------------------------------
# 星点指标
# --------------------------------------------------------------------------
def _label_components(mask):
    if _ndimage is not None:
        lab, n = _ndimage.label(mask, structure=np.ones((3, 3), dtype=int))
        return lab, int(n)
    h, w = mask.shape
    lab = np.zeros((h, w), dtype=np.int32)
    parent = [0]

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    nxt = 1
    for y in range(h):
        for x in range(w):
            if not mask[y, x]:
                continue
            nb = [lab[y + dy, x + dx] for dy, dx in ((-1, -1), (-1, 0), (-1, 1), (0, -1))
                  if 0 <= y + dy < h and 0 <= x + dx < w and lab[y + dy, x + dx]]
            if not nb:
                lab[y, x] = nxt
                parent.append(nxt)
                nxt += 1
            else:
                m = min(nb)
                lab[y, x] = m
                for v in nb:
                    ra_, rb_ = find(m), find(v)
                    if ra_ != rb_:
                        parent[max(ra_, rb_)] = min(ra_, rb_)
    remap, out, k = {}, np.zeros((h, w), dtype=np.int32), 0
    for y in range(h):
        for x in range(w):
            v = lab[y, x]
            if not v:
                continue
            r = find(v)
            if r not in remap:
                k += 1
                remap[r] = k
            out[y, x] = remap[r]
    return out, k


def tile_metrics(tile, sat_level, edge_margin=5.0, threshold_sigma=5.0,
                 min_area=3, max_stars=400, overlay_pix=None, overlay_tol=2.0):
    a = np.asarray(tile, dtype=np.float64)
    h, w = a.shape
    med = float(np.median(a))
    mad = 1.4826 * float(np.median(np.abs(a - med)))
    if not np.isfinite(mad) or mad <= 0:
        mad = float(np.std(a)) or 1.0
    thr = med + threshold_sigma * mad
    mask = a > thr
    lab, nlab = _label_components(mask)
    stars, n_small = [], 0
    for i in range(1, nlab + 1):
        ys, xs = np.nonzero(lab == i)
        area = ys.size
        if area < min_area:
            n_small += 1
            continue
        vals = a[ys, xs] - med
        flux = float(vals.sum())
        if flux <= 0:
            n_small += 1
            continue
        cx = float((xs * vals).sum() / flux)
        cy = float((ys * vals).sum() / flux)
        vx = float((vals * (xs - cx) ** 2).sum() / flux)
        vy = float((vals * (ys - cy) ** 2).sum() / flux)
        vxy = float((vals * (xs - cx) * (ys - cy)).sum() / flux)
        tr, det = vx + vy, vx * vy - vxy * vxy
        disc = max(0.0, tr * tr / 4.0 - det)
        l1, l2 = max(tr / 2.0 + math.sqrt(disc), 0.0), max(tr / 2.0 - math.sqrt(disc), 0.0)
        fwhm = 2.354820045 * math.sqrt(math.sqrt(l1 * l2)) if l1 > 0 and l2 > 0 else float("nan")
        ax, bx = math.sqrt(l1) if l1 > 0 else 0.0, math.sqrt(l2) if l2 > 0 else 0.0
        ellip = (1.0 - bx / ax) if ax > 0 else float("nan")
        peak = float(a[ys, xs].max()) - med
        stars.append({"x": cx, "y": cy, "area": int(area), "flux": flux, "peak": peak,
                      "snr_peak": peak / mad, "snr_int": flux / (mad * math.sqrt(area)),
                      "fwhm": fwhm, "ellip": ellip,
                      "edge": bool(cx < edge_margin or cy < edge_margin or
                                   cx > w - 1 - edge_margin or cy > h - 1 - edge_margin),
                      "saturated": bool(int((a[ys, xs] >= sat_level).sum()) > 0)})
    stars.sort(key=lambda s: -s["flux"])
    stars = stars[:max_stars]

    def pct(key, q):
        vals = [s[key] for s in stars if np.isfinite(s[key])]
        return float(np.percentile(vals, q)) if vals else float("nan")

    n = len(stars)
    ov = {"overlay_n": 0, "overlay_matched": 0, "overlay_match_fraction": float("nan"),
          "overlay_dr_p50": float("nan"), "overlay_dr_p90": float("nan"),
          "overlay_unmatched": 0}
    if overlay_pix:
        tol2 = float(overlay_tol) ** 2
        drs, matched = [], 0
        for (x, y) in overlay_pix:
            if not (0 <= x < w and 0 <= y < h):
                continue
            ov["overlay_n"] += 1
            best = None
            for s in stars:
                d2 = (s["x"] - x) ** 2 + (s["y"] - y) ** 2
                if best is None or d2 < best:
                    best = d2
            if best is not None and best <= tol2:
                matched += 1
                drs.append(math.sqrt(best))
        ov["overlay_matched"] = matched
        ov["overlay_unmatched"] = ov["overlay_n"] - matched
        ov["overlay_match_fraction"] = (matched / ov["overlay_n"]) if ov["overlay_n"] else float("nan")
        if drs:
            ov["overlay_dr_p50"] = float(np.percentile(drs, 50))
            ov["overlay_dr_p90"] = float(np.percentile(drs, 90))
    out = {"n_stars": n, "n_outliers_small": n_small,
           "background_median": med, "noise_mad": mad, "threshold": thr,
           "snr_peak_p10": pct("snr_peak", 10), "snr_peak_p50": pct("snr_peak", 50),
           "snr_peak_p90": pct("snr_peak", 90), "snr_int_p50": pct("snr_int", 50),
           "fwhm_p10": pct("fwhm", 10), "fwhm_p50": pct("fwhm", 50),
           "fwhm_p90": pct("fwhm", 90), "ellip_p50": pct("ellip", 50),
           "ellip_p90": pct("ellip", 90), "n_edge": int(sum(1 for s in stars if s["edge"])),
           "edge_fraction": (sum(1 for s in stars if s["edge"]) / n) if n else 0.0,
           "n_saturated_stars": int(sum(1 for s in stars if s["saturated"])),
           "saturated_star_fraction": (sum(1 for s in stars if s["saturated"]) / n) if n else 0.0,
           "saturated_pixel_fraction": float((a >= sat_level).mean()),
           "total_flux": float(sum(s["flux"] for s in stars)),
           "stars": stars}
    out.update(ov)
    return out


# --------------------------------------------------------------------------
# 九宫格
# --------------------------------------------------------------------------
def detect_stars_global(data, sat_level, threshold_sigma=5.0, min_area=3, max_stars=4000):
    """整帧星点检测（仅质心），用于叠加的**帧级**配对统计。-> [(x, y), ...]"""
    a = np.asarray(data, dtype=np.float64)
    med = float(np.median(a))
    mad = 1.4826 * float(np.median(np.abs(a - med)))
    if not np.isfinite(mad) or mad <= 0:
        mad = float(np.std(a)) or 1.0
    mask = a > med + threshold_sigma * mad
    lab, nlab = _label_components(mask)
    if nlab == 0:
        return []
    objs = _ndimage.find_objects(lab) if _ndimage is not None else None
    out = []
    if objs is not None:
        for i, sl in enumerate(objs, start=1):
            if sl is None:
                continue
            ys, xs = np.nonzero(lab[sl] == i)
            if ys.size < min_area:
                continue
            ys = ys + sl[0].start
            xs = xs + sl[1].start
            vals = a[ys, xs] - med
            flux = float(vals.sum())
            if flux <= 0:
                continue
            out.append((float((xs * vals).sum() / flux), float((ys * vals).sum() / flux), flux))
    else:
        for i in range(1, nlab + 1):
            ys, xs = np.nonzero(lab == i)
            if ys.size < min_area:
                continue
            vals = a[ys, xs] - med
            flux = float(vals.sum())
            if flux <= 0:
                continue
            out.append((float((xs * vals).sum() / flux), float((ys * vals).sum() / flux), flux))
    out.sort(key=lambda t: -t[2])
    return [(t[0], t[1]) for t in out[:max_stars]]


def frame_overlay_match(overlay_pix, gstars, tol=2.0):
    """帧级：每个逆投影点找最近检测星 -> 命中数与残差分位。"""
    if not overlay_pix:
        return None
    tol2 = float(tol) ** 2
    gx = np.array([g[0] for g in gstars], dtype=np.float64) if gstars else np.zeros(0)
    gy = np.array([g[1] for g in gstars], dtype=np.float64) if gstars else np.zeros(0)
    drs, unmatched = [], 0
    for (x, y) in overlay_pix:
        if gx.size == 0:
            unmatched += 1
            continue
        d2 = (gx - x) ** 2 + (gy - y) ** 2
        k = int(np.argmin(d2))
        if d2[k] <= tol2:
            drs.append(math.sqrt(float(d2[k])))
        else:
            unmatched += 1
    n = len(overlay_pix)
    return {"n_cross": n, "n_detected_frame": len(gstars), "n_matched": len(drs),
            "n_unmatched": unmatched,
            "match_fraction": (len(drs) / n) if n else float("nan"),
            "dr_p50": float(np.percentile(drs, 50)) if drs else float("nan"),
            "dr_p90": float(np.percentile(drs, 90)) if drs else float("nan"),
            "dr_max": float(np.max(drs)) if drs else float("nan"),
            "tol_px": float(tol)}


def tile_origins(width, height, grid, tile):
    out = []
    for r in range(grid):
        for c in range(grid):
            fy, fx = (2.0 * r + 1.0) / (2.0 * grid), (2.0 * c + 1.0) / (2.0 * grid)
            x0 = int(round(fx * width - tile / 2.0))
            y0 = int(round(fy * height - tile / 2.0))
            x0 = max(0, min(x0, max(0, width - tile)))
            y0 = max(0, min(y0, max(0, height - tile)))
            out.append((r, c, x0, y0, fx, fy))
    return out


def tile_position_name(row, col, grid):
    if grid == 3:
        return {(0, 0): "corner_top_left", (0, 1): "edge_top", (0, 2): "corner_top_right",
                (1, 0): "edge_left", (1, 1): "center", (1, 2): "edge_right",
                (2, 0): "corner_bottom_left", (2, 1): "edge_bottom",
                (2, 2): "corner_bottom_right"}.get((row, col), "r%dc%d" % (row, col))
    return "r%dc%d" % (row, col)


def default_sat_level(data, meta):
    if int(meta.get("bitpix") or 0) == 16:
        return 0.99 * (float(meta.get("bzero") or 0.0) + 32767.0 * float(meta.get("bscale") or 1.0))
    return 0.99 * float(np.max(data))


def _draw_cross(img_rgb, cx, cy, size=7, thickness=1):
    h, w = img_rgb.shape[0], img_rgb.shape[1]
    x0, x1 = max(0, int(cx) - size), min(w - 1, int(cx) + size)
    y0, y1 = max(0, int(cy) - size), min(h - 1, int(cy) + size)
    for t in range(thickness):
        yy = min(max(int(cy) + t, 0), h - 1)
        img_rgb[yy, x0:x1 + 1] = (255, 0, 0)
        xx = min(max(int(cx) + t, 0), w - 1)
        img_rgb[y0:y1 + 1, xx] = (255, 0, 0)


def process_frame(path, outdir, tile=100, grid=3, stretch="auto",
                  percentiles=(0.1, 99.9), label=False, zcontrast=0.25,
                  threshold_sigma=5.0, min_area=3, edge_margin=5.0,
                  wcs_json=None, overlay_files=(), overlay_tol=2.0,
                  cross_size=7, overview_size=1024,
                  overlay_mag_max=None, overlay_max_stars=2000, quiet=False):
    data, meta, header_wcs = read_fits(path)
    if data.ndim != 2:
        raise RuntimeError("expected a 2D image, got shape %s" % (data.shape,))
    height, width = data.shape
    if height < tile or width < tile:
        raise RuntimeError("frame %dx%d smaller than tile %d" % (width, height, tile))
    if not np.all(np.isfinite(data)):
        med = float(np.median(data[np.isfinite(data)])) if np.any(np.isfinite(data)) else 0.0
        data = np.nan_to_num(data, nan=med)
    sat_level = default_sat_level(data, meta)

    # 解出像素坐标（用于红十叉）
    wcs, wcs_kind = None, None
    if wcs_json:
        wcs, wcs_kind = load_wcs_json(wcs_json)
    elif header_wcs is not None:
        wcs, wcs_kind = _AstropyWCSWrap(header_wcs), "header"

    origins = tile_origins(width, height, grid, tile)
    overlay_pix = []
    catalog_n = 0
    per_tile_cat = {i: [] for i in range(len(origins))}
    if wcs is not None and overlay_files:
        for cf in overlay_files:
            cat = load_catalog(cf)
            if overlay_mag_max is not None and any(c[2] == c[2] for c in cat):
                cat = [c for c in cat if not (c[2] == c[2]) or c[2] <= overlay_mag_max]
            # 只保留投影落在帧内的，再按亮度取前 N（默认 2000 亮星）
            in_frame = []
            for (ra, dec, mag) in cat:
                xy = wcs.sky_to_pixel(ra, dec)
                if xy is None:
                    continue
                x, y = xy
                if -overlay_tol <= x <= width + overlay_tol and -overlay_tol <= y <= height + overlay_tol:
                    in_frame.append((ra, dec, mag, x, y))
            if any(e[2] == e[2] for e in in_frame):
                in_frame.sort(key=lambda e: e[2])
            if overlay_max_stars and len(in_frame) > overlay_max_stars:
                in_frame = in_frame[:overlay_max_stars]
            catalog_n += len(cat)
            overlay_pix.extend([(e[3], e[4]) for e in in_frame])
        for (x, y) in overlay_pix:
            for i, (r, c, x0, y0, fx, fy) in enumerate(origins):
                if x0 <= x < x0 + tile and y0 <= y < y0 + tile:
                    per_tile_cat[i].append((x - x0, y - y0))
                    break

    global_match = None
    if overlay_pix:
        gstars = detect_stars_global(data, sat_level, threshold_sigma=threshold_sigma,
                                     min_area=min_area)
        global_match = frame_overlay_match(overlay_pix, gstars, tol=overlay_tol)

    canvas = np.zeros((grid * tile, grid * tile), dtype=np.uint8)
    tiles = []
    for i, (r, c, x0, y0, fx, fy) in enumerate(origins):
        sub = data[y0:y0 + tile, x0:x0 + tile]
        img8, sinfo = stretch_to_uint8(sub, mode=stretch, percentiles=percentiles,
                                       contrast=zcontrast)
        canvas[r * tile:(r + 1) * tile, c * tile:(c + 1) * tile] = img8
        m = tile_metrics(sub, sat_level, edge_margin=edge_margin,
                         threshold_sigma=threshold_sigma, min_area=min_area,
                         overlay_pix=per_tile_cat[i], overlay_tol=overlay_tol)
        m.update({"row": r, "col": c, "position": tile_position_name(r, c, grid),
                  "x0": x0, "y0": y0, "x1": x0 + tile, "y1": y0 + tile,
                  "frac_x": round(fx, 6), "frac_y": round(fy, 6), "stretch": sinfo})
        tiles.append(m)

    rgb = np.repeat(canvas[:, :, None], 3, axis=2)
    for i, (r, c, x0, y0, fx, fy) in enumerate(origins):
        for (lx, ly) in per_tile_cat[i]:
            _draw_cross(rgb, c * tile + lx, r * tile + ly, size=cross_size)

    base = os.path.splitext(os.path.basename(path))[0]
    os.makedirs(outdir, exist_ok=True)
    if Image is None:
        raise RuntimeError("Pillow (PIL) is required to write PNG output")
    png = os.path.join(outdir, base + ".grid9.png")
    if overlay_pix:
        Image.fromarray(rgb, mode="RGB").save(png)
    else:
        Image.fromarray(canvas, mode="L").save(png)

    overview = None
    if overlay_pix:
        try:
            full8, _fi = stretch_to_uint8(data, mode=stretch, percentiles=percentiles,
                                          contrast=zcontrast)
            ov = Image.fromarray(full8, mode="L").convert("RGB")
            ow, oh = ov.size
            k = max(1, int(math.ceil(max(ow, oh) / float(overview_size))))
            ov_small = np.array(ov.resize((max(1, ow // k), max(1, oh // k))))
            sx = ov_small.shape[1] / float(ow)
            sy = ov_small.shape[0] / float(oh)
            for (x, y) in overlay_pix:
                _draw_cross(ov_small, x * sx, y * sy, size=cross_size, thickness=1)
            overview = os.path.join(outdir, base + ".overlay_overview.png")
            Image.fromarray(ov_small, mode="RGB").save(overview)
        except Exception:
            overview = None

    labeled = None
    if label:
        # 带 1px 分隔 + 块号徽标 + 9 行 (row,col,x0,y0) 回查条的标注版
        from PIL import ImageDraw
        pad, line_h = 2, 13
        strip = line_h * (grid * grid + 1)
        W2 = grid * tile + pad * (grid + 1)
        H2 = grid * tile + pad * (grid + 1) + strip
        big = Image.new("RGB", (W2, H2), (16, 16, 16))
        dr = ImageDraw.Draw(big)
        for (r, c, x0, y0, fx, fy) in origins:
            yy, xx = r * (tile + pad), c * (tile + pad)
            blk = rgb[r * tile:(r + 1) * tile, c * tile:(c + 1) * tile]
            big.paste(Image.fromarray(blk, mode="RGB"), (xx, yy))
            dr.rectangle([xx - 1, yy - 1, xx + tile, yy + tile], outline=(0, 160, 255))
        y = grid * tile + pad * (grid + 1) + 2
        dr.text((4, y), "grid=%d tile=%d  stretch=%s  row,col -> x0,y0 (parent frame %dx%d)" % (
            grid, tile, stretch, width, height), fill=(255, 255, 0))
        for k, (r, c, x0, y0, fx, fy) in enumerate(origins):
            dr.text((4, y + line_h * (k + 1)),
                    "[%d] %-20s row=%d col=%d  x0,y0=%d,%d  frac=(%.4f,%.4f)" % (
                        k, tile_position_name(r, c, grid), r, c, x0, y0, fx, fy),
                    fill=(200, 200, 200))
        labeled = os.path.join(outdir, base + ".grid9.labeled.png")
        big.save(labeled)

    summary = {
        "frame": os.path.abspath(path), "width": width, "height": height,
        "tile": tile, "grid": grid, "grid_px": [grid * tile, grid * tile],
        "stretch": stretch, "percentiles": [percentiles[0], percentiles[1]],
        "sat_level": sat_level,
        "tile_positions": ("center + 4 corners + 4 edge midpoints" if grid == 3 else None),
        "meta": meta,
        "frame_metrics": {
            "background_median": float(np.median(data)), "noise_mad": _mad(data)[1],
            "min": float(np.min(data)), "max": float(np.max(data)),
            "saturated_pixel_fraction": float((data >= sat_level).mean())},
        "wcs": {"source": wcs_kind, "overlay_mag_max": overlay_mag_max,
                "overlay_max_stars": overlay_max_stars,
                "wcs_json": os.path.abspath(wcs_json) if wcs_json else None,
                "catalog_files": [os.path.abspath(f) for f in overlay_files],
                "catalog_n": catalog_n, "projected_in_frame": len(overlay_pix),
                "overlay_tol_px": overlay_tol},
        "overlay_matched_total": int(sum(t["overlay_matched"] for t in tiles)),
        "overlay_n_total": int(sum(t["overlay_n"] for t in tiles)),
        "overlay_global": global_match,
        "overlay_overview_png": overview,
        "tiles": tiles,
        "png": png, "png_labeled": labeled,
        "tile_origin_table": [{"row": t["row"], "col": t["col"], "position": t["position"],
                               "x0": t["x0"], "y0": t["y0"], "x1": t["x1"], "y1": t["y1"]}
                              for t in tiles],
    }
    jp = os.path.join(outdir, base + ".grid9.json")
    with open(jp, "w", encoding="utf-8") as fh:
        json.dump(summary, fh, ensure_ascii=False, indent=1)
    if not quiet:
        extra = ""
        if wcs is not None and overlay_files:
            g = global_match or {}
            extra = "  overlay frame-level %s/%s matched (WCS=%s)%s" % (
                g.get("n_matched"), g.get("n_cross"), wcs_kind,
                "" if not g.get("n_cross") else
                "  match=%.1f%% dr_p50=%.3fpx" % (100.0 * g["match_fraction"], g["dr_p50"]))
        print("[frame_qc_grid] %s -> %s (%dx%d), %d tiles, %d stars%s" % (
            os.path.basename(path), png, grid * tile, grid * tile, len(tiles),
            sum(t["n_stars"] for t in tiles), extra))
    return summary


CSV_COLS = ["frame", "row", "col", "position", "x0", "y0", "n_stars",
            "snr_peak_p50", "fwhm_p50", "ellip_p50", "background_median",
            "noise_mad", "n_edge", "edge_fraction", "n_saturated_stars",
            "saturated_star_fraction", "saturated_pixel_fraction",
            "overlay_n", "overlay_matched", "overlay_match_fraction", "overlay_dr_p50"]


def append_summary_csv(outdir, summaries):
    path = os.path.join(outdir, "frame_qc_grid_summary.csv")
    new = not os.path.exists(path)
    with open(path, "a", newline="", encoding="utf-8") as fh:
        wr = _csv.DictWriter(fh, fieldnames=CSV_COLS, extrasaction="ignore")
        if new:
            wr.writeheader()
        for s in summaries:
            for t in s["tiles"]:
                row = {"frame": s["frame"]}
                row.update({k: t.get(k) for k in CSV_COLS if k in t})
                row["row"], row["col"] = t["row"], t["col"]
                row["position"], row["x0"], row["y0"] = t["position"], t["x0"], t["y0"]
                wr.writerow(row)
    return path


# --------------------------------------------------------------------------
FITS_EXT = (".fts", ".fits", ".fit", ".fts.gz", ".fits.gz")


def expand_inputs(inputs):
    out = []
    for it in inputs:
        if os.path.isdir(it):
            for root, _dirs, files in os.walk(it):
                for f in sorted(files):
                    if f.lower().endswith(FITS_EXT):
                        out.append(os.path.join(root, f))
        else:
            out.append(it)
    return out


def _write_simple_fits(path, img, crpix=None, crval=None, cd=None):
    h, w = img.shape
    data = np.clip(np.round(img), 0, 65535).astype(">i2")
    cards = []

    def card(k, v, c=""):
        s = "%-8s= %20s" % (k, v)
        if c:
            s += " / " + c
        cards.append(s.ljust(80)[:80])

    card("SIMPLE", "T")
    card("BITPIX", 16)
    card("NAXIS", 2)
    card("NAXIS1", w)
    card("NAXIS2", h)
    card("BZERO", 32768)
    card("BSCALE", 1)
    card("OBJECT", "'SELFTEST'")
    if crpix and crval and cd:
        card("CTYPE1", "'RA---TAN'")
        card("CTYPE2", "'DEC--TAN'")
        card("CRPIX1", crpix[0])
        card("CRPIX2", crpix[1])
        card("CRVAL1", crval[0])
        card("CRVAL2", crval[1])
        card("CD1_1", cd[0])
        card("CD1_2", cd[1])
        card("CD2_1", cd[2])
        card("CD2_2", cd[3])
    header = "".join(cards) + "END".ljust(80)
    pad = (-len(header)) % 2880
    payload = data.tobytes()
    payload += b"\x00" * ((-len(payload)) % 2880)
    with open(path, "wb") as fh:
        fh.write(header.encode("ascii") + b" " * pad)
        fh.write(payload)


def _selftest():
    """合成帧自检：9 个已知星点 + 一个已知 WCS 的逆投影红十叉。"""
    try:
        import astropy as _ap
        _apv = _ap.__version__
    except Exception:
        _apv = None
    try:
        import scipy as _sp
        _spv = _sp.__version__
    except Exception:
        _spv = None
    print("[selftest] numpy=%s astropy=%s scipy=%s PIL=%s" % (
        np.__version__, _apv, _spv, getattr(Image, "__version__", None)))
    grid, tile = 3, 100
    H = W = 400
    rng = np.random.RandomState(7)
    img = rng.normal(100.0, 2.0, (H, W))
    star_pix = []
    for (r, c, x0, y0, _fx, _fy) in tile_origins(W, H, grid, tile):
        cx, cy = x0 + tile // 2, y0 + tile // 2
        yy, xx = np.mgrid[0:tile, 0:tile]
        img[y0:y0 + tile, x0:x0 + tile] += 900.0 * np.exp(
            -((xx - (cx - x0)) ** 2 + (yy - (cy - y0)) ** 2) / (2 * 2.0 ** 2))
        star_pix.append((cx, cy))
    scale = 1.0 / 3600.0  # 1"/px
    crpix = [W / 2.0, H / 2.0]
    crval = [83.0, -5.0]
    cd = [scale, 0.0, 0.0, scale]
    with tempfile.TemporaryDirectory() as td:
        src = os.path.join(td, "synthetic.fts")
        _write_simple_fits(src, img, crpix, crval, cd)
        s = process_frame(src, td, tile=tile, grid=grid, stretch="auto", label=True)
        assert len(s["tiles"]) == 9, "expected 9 tiles"
        assert s["grid_px"] == [300, 300], s["grid_px"]
        if Image is not None:
            with Image.open(s["png"]) as im:
                assert im.size == (grid * tile, grid * tile), "PNG size %s" % (im.size,)
        bad = [(t["position"], t["n_stars"]) for t in s["tiles"] if t["n_stars"] < 1]
        assert not bad, "tiles without stars: %s" % bad
        assert os.path.exists(s["png_labeled"])
        assert len(s["tile_origin_table"]) == 9
        # --- overlay 自检：用同一 WCS 逆投影 9 个星点 -> 应全部命中 ---
        cat = []
        for (r, c, x0, y0, fx, fy) in tile_origins(W, H, grid, tile):
            for (cx, cy) in star_pix:
                if x0 <= cx < x0 + tile and y0 <= cy < y0 + tile:
                    dra = (cx - (crpix[0] - 0.5)) * scale
                    ddec = (cy - (crpix[1] - 0.5)) * scale
                    cat.append({"ra": crval[0] + dra / math.cos(math.radians(crval[1])),
                                "dec": crval[1] + ddec})
        catp = os.path.join(td, "cat.json")
        with open(catp, "w", encoding="utf-8") as fh:
            json.dump(cat, fh)
        s2 = process_frame(src, td, tile=tile, grid=grid, stretch="auto",
                           overlay_files=(catp,), overlay_tol=2.0)
        assert s2["overlay_n_total"] == 9, "overlay_n=%d" % s2["overlay_n_total"]
        assert s2["overlay_matched_total"] == 9, "overlay_matched=%d (WCS convention?)" % (
            s2["overlay_matched_total"],)
        with Image.open(s2["png"]) as im:
            assert im.mode == "RGB" and im.size == (300, 300)
        print("[selftest] OK: 300x300 grayscale + labeled + RGB overlay; "
              "9/9 synthetic stars detected and 9/9 catalog entries re-projected onto them")
    return 0


# --------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="frame_qc_grid",
        description="星点质量九宫格视觉确认工具（中心+四角+四边中点，自动拉伸，"
                    "300x300 合成图；可选 Gaia 逆投影红十叉叠加）",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--input", "-i", nargs="*", default=[],
                    help="FITS 文件或目录（目录递归查找 .fts/.fits/.fit）")
    ap.add_argument("--outdir", "-o", default=".", help="输出目录")
    ap.add_argument("--tile", type=int, default=100, help="每块边长 px（默认 100）")
    ap.add_argument("--grid", type=int, default=3, help="每边块数（默认 3）")
    ap.add_argument("--stretch", choices=("auto", "asinh", "zscale"), default="auto")
    ap.add_argument("--percentiles", default="0.1,99.9", help="p_lo,p_hi")
    ap.add_argument("--zcontrast", type=float, default=0.25)
    ap.add_argument("--threshold-sigma", type=float, default=5.0)
    ap.add_argument("--min-area", type=int, default=3)
    ap.add_argument("--edge-margin", type=float, default=5.0)
    ap.add_argument("--label", action="store_true", help="额外输出标注版 PNG")
    ap.add_argument("--wcs-json", default=None, help="解算结果 JSON（含 crpix/crval/cd[/sip_*]）")
    ap.add_argument("--overlay", nargs="*", default=[],
                    help="星表文件（.csv 含 ra/dec；.bin 为探针 W.bin；.json 为 [{ra,dec}]）")
    ap.add_argument("--overlay-tol", type=float, default=2.0, help="判定命中的半径 px")
    ap.add_argument("--overlay-mag-max", type=float, default=None,
                    help="只叠加亮于该星等的星表星")
    ap.add_argument("--overlay-max-stars", type=int, default=2000,
                    help="帧内按亮度取前 N 颗星叠加（默认 2000，0=不限）")
    ap.add_argument("--cross-size", type=int, default=7, help="红十叉半臂长 px")
    ap.add_argument("--overview-size", type=int, default=1024,
                    help="整帧叠加总览图长边像素（默认 1024）")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)

    if args.selftest:
        return _selftest()
    try:
        p_lo, p_hi = [float(x) for x in args.percentiles.split(",")]
    except Exception:
        sys.stderr.write("[frame_qc_grid] --percentiles must be 'p_lo,p_hi'\n")
        return 2
    if not args.input:
        sys.stderr.write("[frame_qc_grid] --input is required (or use --selftest)\n")
        return 2
    files = expand_inputs(args.input)
    if not files:
        sys.stderr.write("[frame_qc_grid] no input files found\n")
        return 2

    summaries, failures = [], 0
    for p in files:
        try:
            summaries.append(process_frame(
                p, args.outdir, tile=args.tile, grid=args.grid, stretch=args.stretch,
                percentiles=(p_lo, p_hi), label=args.label, zcontrast=args.zcontrast,
                threshold_sigma=args.threshold_sigma, min_area=args.min_area,
                edge_margin=args.edge_margin, wcs_json=args.wcs_json,
                overlay_files=tuple(args.overlay), overlay_tol=args.overlay_tol,
                cross_size=args.cross_size, overview_size=args.overview_size,
                overlay_mag_max=args.overlay_mag_max,
                overlay_max_stars=args.overlay_max_stars, quiet=args.quiet))
        except Exception as exc:
            failures += 1
            sys.stderr.write("[frame_qc_grid] FAILED %s: %s\n" % (p, exc))
    if summaries:
        csvp = append_summary_csv(args.outdir, summaries)
        if not args.quiet:
            print("[frame_qc_grid] %d frame(s) ok, %d failed; summary: %s" % (
                len(summaries), failures, csvp))
    return 0 if failures == 0 else 3


if __name__ == "__main__":
    raise SystemExit(main())
