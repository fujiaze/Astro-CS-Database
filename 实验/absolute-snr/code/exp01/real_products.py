#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实产物（testdata / run 下 p1_*.json）的**低内存**流式读取器。

为什么不用 json.load：生产 p1_sources.json 单文件最大 337 MB，json.load 会把每个源
展开成 dict（百万级对象），峰值内存可达数 GB（本仓有过内存事故）。本模块按
JSONDecoder.raw_decode **逐元素**解析 sources / psf_params 数组，峰值内存 = 文本 + 数值数组。

只读：本模块不写任何产物文件。
"""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Dict, Iterator, List, Optional

import numpy as np

ROOT = Path(__file__).resolve().parents[4]        # 仓根

# 帧级标量（nlohmann::json 默认 std::map ⇒ 键按字母序）
_FRAME_SCALARS = ("background", "noise_sigma", "n_sources", "n_detected",
                  "n_psf_valid", "n_fit_input", "fit_limit")
_SCALAR_RE = re.compile(
    r'"(' + "|".join(_FRAME_SCALARS) + r')":\s*(-?[0-9]+(?:\.[0-9]+)?(?:[eE][-+]?[0-9]+)?)')


def _skip_ws(text: str, i: int) -> int:
    n = len(text)
    while i < n and text[i] in " \t\r\n,":
        i += 1
    return i


def iter_frames(path: Path, max_frames: Optional[int] = None) -> Iterator[Dict[str, object]]:
    """逐帧流式解析 DATA-P1-SOURCES。产出 dict：
    {meta: {scalar}, psf: {star_id, flux, A, sx, sy, fwhm_x, fwhm_y}, src: {id, flux, fwhm_px}}
    其中 psf/src 为 numpy 数组（object/float）。
    """
    text = Path(path).read_text(encoding="utf-8")
    dec = json.JSONDecoder()
    n = len(text)
    i = text.index('"frames"')
    i = text.index("[", i) + 1
    n_frame = 0
    while True:
        i = _skip_ws(text, i)
        if i >= n or text[i] == "]":
            break
        if text[i] != "{":
            raise ValueError("unexpected token at %d: %r" % (i, text[i:i + 40]))
        fstart = i
        kp = text.index('"psf_params"', fstart)
        meta = {k: float(v) for k, v in _SCALAR_RE.findall(text[fstart:kp])}
        # --- psf_params ---
        ip = text.index("[", kp) + 1
        pid, pflux, pA, psx, psy, pfx, pfy = [], [], [], [], [], [], []
        while True:
            ip = _skip_ws(text, ip)
            if text[ip] == "]":
                ip += 1
                break
            o, ip = dec.raw_decode(text, ip)
            pid.append(o.get("star_id"))
            pflux.append(o.get("flux"))
            pA.append(o.get("A"))
            psx.append(o.get("sx"))
            psy.append(o.get("sy"))
            pfx.append(o.get("fwhm_x"))
            pfy.append(o.get("fwhm_y"))
        # --- sources ---
        ks = text.index('"sources"', ip)
        isrc = text.index("[", ks) + 1
        sid, sflux, sfwhm = [], [], []
        while True:
            isrc = _skip_ws(text, isrc)
            if text[isrc] == "]":
                isrc += 1
                break
            o, isrc = dec.raw_decode(text, isrc)
            sid.append(o.get("id"))
            sflux.append(o.get("flux"))
            sfwhm.append(o.get("fwhm_px"))
        yield {
            "meta": meta,
            "psf": {
                "star_id": np.array(pid, dtype=object),
                "flux": np.array(pflux, dtype=float),
                "A": np.array(pA, dtype=float),
                "sx": np.array(psx, dtype=float),
                "sy": np.array(psy, dtype=float),
                "fwhm_x": np.array(pfx, dtype=float),
                "fwhm_y": np.array(pfy, dtype=float),
            },
            "src": {
                "id": np.array(sid, dtype=object),
                "flux": np.array(sflux, dtype=float),
                "fwhm_px": np.array(sfwhm, dtype=float),
            },
        }
        # 跳过本帧的收尾 '}'（其后为 ',' 或 ']'）
        j = isrc
        while j < n and text[j] not in "{]":
            j += 1
        i = j
        n_frame += 1
        if max_frames is not None and n_frame >= max_frames:
            break
    del text


def find_products(min_bytes: int = 50_000_000, limit: int = 8) -> List[Path]:
    """列出可用于真实数据臂的 p1_sources.json（体积降序，取前 limit 个）。"""
    hits = [p for p in (ROOT / "run").rglob("p1_sources.json")
            if p.stat().st_size >= min_bytes]
    hits.sort(key=lambda p: -p.stat().st_size)
    return hits[:limit]


def match_psf_to_sources(src: Dict[str, np.ndarray],
                         psf: Dict[str, np.ndarray]) -> Dict[str, np.ndarray]:
    """按 star_id <-> id 关联盒和通量与 PSF 解析通量（返回同长度配对数组）。"""
    if psf["star_id"].size == 0 or src["id"].size == 0:
        return {k: np.zeros(0) for k in
                ("box_flux", "psf_flux", "fwhm_det", "sx", "sy", "A", "fwhm_x")}
    idx = {sid: k for k, sid in enumerate(src["id"])}
    rows = []
    for k, sid in enumerate(psf["star_id"]):
        j = idx.get(sid)
        if j is None:
            continue
        rows.append((src["flux"][j], psf["flux"][k], src["fwhm_px"][j],
                     psf["sx"][k], psf["sy"][k], psf["A"][k], psf["fwhm_x"][k]))
    arr = np.array(rows, dtype=float) if rows else np.zeros((0, 7))
    return {"box_flux": arr[:, 0], "psf_flux": arr[:, 1], "fwhm_det": arr[:, 2],
            "sx": arr[:, 3], "sy": arr[:, 4], "A": arr[:, 5], "fwhm_x": arr[:, 6]}
