#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_master_unit_guard.py —— UNIT-001 母版单位/归一化消费门（可执行正负例）。

权威：
  docs/science/CALIBRATION.md（SCI-CAL-001）§3/§6/§8/§11  ← 标度声明 + 判定带 + 四条负例/两条正例
  docs/algorithms/CALIBRATION_ALGORITHMS.md（ALG-CAL-001）§2 标度声明表 + §10 DISP-CAL-013
  docs/contracts/DATA_SEMANTICS.md §9.1/§9.1a（DATA-P1-CAL）
外部依据：XISF 1.0 §Image（bounds=可表示域；浮点实型必须声明）、PCL XISFReader NormalizeSamples
          （Float32[0,1] ↔ UInt16[0,65535]，MaxSampleValue=65535）、FITS 3.0 BSCALE/BZERO。

判据（对真实二进制 build/astrocs 的端到端注入，不依赖文件名/目录名推断单位）：
  N1 母版 [0,1] 归一化 + 亮场 ADU，未声明标度  → rc=2 且诊断含 MASTER_UNIT_MISMATCH
  N2 master flat 未归一（median 出判定带），未声明归一 → rc=2 且含 MASTER_FLAT_NOT_NORMALIZED
  N3 提供 master_dark 而未显式声明 bias 约定（dark_optimization 缺失） → rc=2 且含
     MASTER_DARK_CONVENTION_UNDECLARED
  N4 声明自洽（U4）：master_units.*="normalized" 却未给 master_scale.*（=无换算因子），
     或 bias/dark 换算因子不一致 → rc=2 且含 MASTER_UNIT_DECLARATION_INVALID
  P1 同一组归一化母版 + 显式声明（master_units/master_scale/master_flat_normalize/
     dark_optimization） → 通过消费门，且 calibrated_* 中位数落在声明换算后的 ADU 预测值容差内
  P2 **本就合规**（母版本就 ADU + flat 本就归一 + 约定已声明，无需任何标度声明）→ 必须通过
     （防「一律拒绝」回归锚）
负例还必须不留 calibrated_* 半成品（ENGINEERING_SPEC §9；.fts/.fits 两种后缀都判）。
--real 模式用真实 T2 母版 + 真实亮场；正例判据为**逐像素 NumPy oracle**（独立于生产实现）。

用法：
  tools/quality/check_master_unit_guard.py --binary build/astrocs [--real] [--self-test]
出口：0 全过；1 判据不满足；2 环境错；3 自检（--self-test）异常。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

import numpy as np

TOK_UNIT = "MASTER_UNIT_MISMATCH"
TOK_FLAT = "MASTER_FLAT_NOT_NORMALIZED"
TOK_DARK = "MASTER_DARK_CONVENTION_UNDECLARED"
TOK_DECL = "MASTER_UNIT_DECLARATION_INVALID"

# 判定带默认值必须与 config/defaults.json 的 calibration.master_flat_median_range 一致
FLAT_BAND_DEFAULT = [0.5, 2.0]
XISF_SCALE_16BIT = 65535.0


# ────────────────────────────── 最简 FITS / XISF 写读（独立性：不导入 AstroCS） ──────────────

def _fits_cards(pairs):
    out = []
    for k, v, c in pairs:
        if isinstance(v, str):
            body = "%-8s= '%s'" % (k, v)
            body = body[:10] + body[10:].ljust(70)[:70]
        elif isinstance(v, bool):
            body = "%-8s= %20s" % (k, "T" if v else "F")
        elif isinstance(v, int):
            body = "%-8s= %20d" % (k, v)
        else:
            body = "%-8s= %20.10E" % (k, float(v))
        card = (body.ljust(80))[:80]
        out.append(card)
    return out


def write_fits_primary(path: Path, arr: np.ndarray, *, bitpix: int, cards_extra=()):
    h, w = arr.shape
    cards = [("SIMPLE", True, ""), ("BITPIX", bitpix, ""), ("NAXIS", 2, ""),
             ("NAXIS1", w, ""), ("NAXIS2", h, "")]
    cards += list(cards_extra)
    cards.append(("END", "", ""))
    hdr = "".join(_fits_cards([c])[0] for c in cards).encode("ascii")
    pad = (-len(hdr)) % 2880
    hdr += b" " * pad
    if bitpix == 16:
        data = arr.astype(">i2").tobytes()
    elif bitpix == -32:
        data = arr.astype(">f4").tobytes()
    else:
        raise ValueError(bitpix)
    path.write_bytes(hdr + data)


def write_xisf(path: Path, arr: np.ndarray, *, bounds="0:1", kws=()):
    """最简 XISF：Float32 + bounds 声明 + 未压缩 attachment（与真实母版同声明形态）。"""
    h, w = arr.shape
    off = 4096
    size = w * h * 4
    kwxml = "".join(
        '<FITSKeyword name="%s" value="%s" comment="%s"/>' % (k, v, c) for k, v, c in kws)
    xml = ('<?xml version="1.0" encoding="UTF-8"?><xisf version="1.0" '
           'xmlns="http://www.pixinsight.com/xisf"><Image geometry="%d:%d:1" '
           'sampleFormat="Float32" bounds="%s" colorSpace="Gray" '
           'location="attachment:%d:%d">%s</Image></xisf>' % (w, h, bounds, off, size, kwxml))
    head = xml.encode("utf-8")
    if 16 + len(head) > off:
        raise ValueError("header too large for offset")
    blob = b"XISF0100" + struct.pack("<I", len(head)) + struct.pack("<I", 0) + head
    blob = blob.ljust(off, b" ")
    path.write_bytes(blob + arr.astype("<f4").tobytes())


def read_fits_primary(path: Path):
    raw = path.read_bytes()
    cards = []
    pos = 0
    while True:
        blk = raw[pos:pos + 2880]
        pos += 2880
        for i in range(0, 2880, 80):
            card = blk[i:i + 80].decode("ascii", "replace")
            cards.append(card)
            if card.startswith("END"):
                break
        else:
            continue
        break
    hdr = {}
    for card in cards:
        key = card[:8].strip()
        if card[8:10] == "= " and key and key not in ("COMMENT", "HISTORY"):
            val = card[10:].split("/")[0].strip()
            if val.startswith("'"):
                val = val.strip("'").strip()
            hdr[key] = val
    bitpix = int(hdr["BITPIX"])
    w, h = int(hdr["NAXIS1"]), int(hdr["NAXIS2"])
    n = w * h
    dt = {16: ">i2", -32: ">f4", 32: ">i4"}[bitpix]
    arr = np.frombuffer(raw[pos:pos + n * abs(bitpix) // 8], dtype=dt).astype(np.float64)
    bscale = float(hdr.get("BSCALE", 1.0)); bzero = float(hdr.get("BZERO", 0.0))
    if bscale != 1.0 or bzero != 0.0:
        arr = bscale * arr + bzero
    return arr.reshape(h, w), hdr


def array_of(path: Path) -> "np.ndarray":
    """独立读取器（不导入 AstroCS）：XISF Float32 附件 / FITS 主 HDU（施加 BSCALE/BZERO）。"""
    if path.suffix.lower() == ".xisf":
        raw = path.read_bytes()
        hlen = struct.unpack("<I", raw[8:12])[0]
        xml = raw[16:16 + hlen].decode("utf-8")
        m = re.search(r'<Image[^>]*geometry="(\d+):(\d+):(\d+)"', xml)
        loc = re.search(r'location="attachment:(\d+):(\d+)"', xml)
        w, h = int(m.group(1)), int(m.group(2))
        off, size = int(loc.group(1)), int(loc.group(2))
        return np.frombuffer(raw[off:off + size], dtype="<f4").astype(np.float64).reshape(h, w)
    arr, _ = read_fits_primary(path)
    return arr


def median_of(path: Path) -> float:
    return float(np.median(array_of(path)))


def real_oracle(light_path: Path, bias_path: Path, dark_path: Path, flat_path: Path, k: float):
    """真实链路逐像素 oracle（**独立于生产实现**；声明换算 master_scale=65535 + flat 按 median 归一，
    兼容式 `(raw − bias − K·(dark−bias)) / max(flat_norm, 0.1)`）。返回 (预测中位数, 亮场中位数)。"""
    light = array_of(light_path)
    bias = array_of(bias_path) * XISF_SCALE_16BIT
    dark = array_of(dark_path) * XISF_SCALE_16BIT
    flat = array_of(flat_path) * XISF_SCALE_16BIT
    flat_n = flat / float(np.median(flat))
    cal = (light - bias - k * (dark - bias)) / np.maximum(flat_n, 0.1)
    return float(np.median(cal)), float(np.median(light))


# ────────────────────────────────────── 夹具 ──────────────────────────────────────

def build_synthetic(fx: Path, repo: Path, log):
    """合成夹具：亮场 ADU（BITPIX=16, BZERO=32768）；母版 ADU（float32）与 [0,1] XISF 两套。"""
    fx.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(20260917)
    h = w = 64
    yy, xx = np.mgrid[0:h, 0:w]
    light = (2000.0 + 40.0 * xx / w + 25.0 * yy / h + rng.normal(0, 3, (h, w))).astype(np.float64)
    bias = (1000.0 + 6.0 * xx / w + rng.normal(0, 1, (h, w))).astype(np.float64)
    dark_total = (1010.0 + 6.0 * xx / w + 3.0 * yy / h + rng.normal(0, 1, (h, w))).astype(np.float64)  # 含 bias
    flat_norm = (1.0 + 0.02 * np.sin(xx / 7.0) + 0.01 * np.cos(yy / 5.0)).astype(np.float64)  # 已归一（median≈1）
    flat_raw = (0.2064 * flat_norm).astype(np.float64)                                        # 未归一（XISF 归一化域）

    write_fits_primary(fx / "light.fits", light - 32768.0, bitpix=16, cards_extra=[
        ("BZERO", 32768.0, "offset"), ("BSCALE", 1.0, "scale"),
        ("EXPTIME", 600.0, "s"), ("IMAGETYP", "Light Frame", ""), ("FILTER", "Red", "")])
    write_fits_primary(fx / "bias_adu.fits", bias, bitpix=-32, cards_extra=[
        ("EXPTIME", 0.0, "s"), ("IMAGETYP", "Master Bias", "")])
    write_fits_primary(fx / "dark_adu.fits", dark_total, bitpix=-32, cards_extra=[
        ("EXPTIME", 600.0, "s"), ("IMAGETYP", "Master Dark", "")])
    write_fits_primary(fx / "flat_norm_adu.fits", flat_norm, bitpix=-32, cards_extra=[
        ("EXPTIME", 3.0, "s"), ("IMAGETYP", "Master Flat", "")])
    write_fits_primary(fx / "flat_raw_adu.fits", flat_raw * XISF_SCALE_16BIT, bitpix=-32, cards_extra=[
        ("EXPTIME", 3.0, "s"), ("IMAGETYP", "Master Flat", "")])
    write_xisf(fx / "bias_norm.xisf", bias / XISF_SCALE_16BIT, kws=[
        ("IMAGETYP", "'Master Bias'", ""), ("EXPTIME", "0.00", "")])
    write_xisf(fx / "dark_norm.xisf", dark_total / XISF_SCALE_16BIT, kws=[
        ("IMAGETYP", "'Master Dark'", ""), ("EXPTIME", "600.00", "")])
    write_xisf(fx / "flat_norm.xisf", flat_raw, kws=[
        ("IMAGETYP", "'Master Flat'", ""), ("EXPTIME", "3.00", "")])
    # 独立 NumPy oracle（不调用生产实现）：兼容式（dark 含 bias, dark_optimization=true）
    #   cal = (light − bias − K·(dark−bias)) / max(flat_norm, 0.1), K = t_light/t_dark = 1
    flat_used = flat_raw / float(np.median(flat_raw))
    oracle = (light - bias - 1.0 * (dark_total - bias)) / np.maximum(flat_used, 0.1)
    oracle_p2 = (light - bias - 1.0 * (dark_total - bias)) / np.maximum(flat_norm, 0.1)
    log(f"[fixtures] synthetic in {fx} (oracle median={float(np.median(oracle)):.4f} ADU, "
        f"already-normalized-flat oracle={float(np.median(oracle_p2)):.4f} ADU)")
    return {"light": fx / "light.fits",
            "light_median": float(np.median(light)), "bias_median": float(np.median(bias)),
            "dark_median": float(np.median(dark_total)),
            "predict_median": float(np.median(oracle)),
            "predict_median_already_norm": float(np.median(oracle_p2))}


def build_real(fx: Path, repo: Path, log):
    """真实夹具：E2E/BIAS-001 同款 T2 母版（XISF）+ 真实 ADU 亮场。"""
    t2 = repo / "testdata/T2 calibration files"
    lights = sorted((repo / "testdata/NGC1727_T2_flying_dutchman/lights").glob("*600S-Red.fts"))
    if not t2.is_dir() or not lights:
        raise SystemExit("environment: real testdata missing")
    fx.mkdir(parents=True, exist_ok=True)
    F = {"light": lights[0],
         "bias": t2 / "masterBias_BIN-1_4096x4096.xisf",
         "dark": t2 / "masterDark_BIN-1_4096x4096_EXPOSURE-600.00s.xisf",
         "flat": t2 / "masterFlat_BIN-1_4096x4096_FILTER-Red_mono.xisf"}
    # 逐像素 oracle（K=1: 亮场与暗场同 600 s）——正例判据用它，不用中位数近似
    pred, light_med = real_oracle(F["light"], F["bias"], F["dark"], F["flat"], 1.0)
    F["predict_median"], F["light_median"] = pred, light_med
    log(f"[fixtures] real T2 in {fx}: light median={light_med:.2f} ADU, "
        f"逐像素 oracle median={pred:.3f} ADU")
    return F


# ────────────────────────────────────── 运行与判定 ──────────────────────────────────────

def run_case(binary: Path, repo: Path, work: Path, name: str, cfg: dict, timeout: int):
    out_dir = work / "out" / name
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    cfg = dict(cfg)
    cfg["output_dir"] = str(out_dir.relative_to(repo)) if out_dir.is_relative_to(repo) else str(out_dir)
    cfg_path = work / "configs" / f"{name}.json"
    cfg_path.parent.mkdir(parents=True, exist_ok=True)
    cfg_path.write_text(json.dumps(cfg, indent=2, ensure_ascii=False), encoding="utf-8")
    t0 = time.time()
    pr = subprocess.run([str(binary), "normalize", "--json", str(cfg_path), "-y"],
                        cwd=str(repo), capture_output=True, text=True, timeout=timeout)
    dt = time.time() - t0
    blob = pr.stdout + "\n" + pr.stderr
    for mf in out_dir.glob("astrocs_run_*.json"):
        blob += "\n" + mf.read_text(encoding="utf-8", errors="ignore")
    return {"name": name, "rc": pr.returncode, "seconds": round(dt, 1), "out_dir": str(out_dir),
            "config": str(cfg_path), "blob": blob}


def check_expectation(case, expect, *, invert=False):
    """返回 (ok, notes)。invert=True 用于 --self-test：把判据反过来，检查器必须判红。"""
    rc, blob = case["rc"], case["blob"]
    out_dir = Path(case["out_dir"])
    # 真实链路产物为 .fts（FITS 命名约定），合成夹具写出 .fits ⇒ 两种后缀都要判（否则"无半成品"
    # 断言在合成模式下会变成空断言）。
    prods = sorted(list(out_dir.glob("calibrated_*.fts")) + list(out_dir.glob("calibrated_*.fits")))
    exp = dict(expect)
    if invert and exp["kind"] == "reject":
        # 变异注入（--self-test）：把期望 token 改成哨兵值。检查器若仍判绿 ⇒ 无判别力。
        exp["token"] = "MUTATED_TOKEN_SENTINEL"
    notes = [f"rc={rc}", f"products={len(prods)}"]
    if exp["kind"] == "reject":
        ok = rc == 2 and exp["token"] in blob and exp["names"] in blob and not prods
        if rc != 2:
            notes.append(f"期望 rc=2（DATA 拒绝），实测 rc={rc}")
        if exp["token"] not in blob:
            notes.append(f"诊断缺 {exp['token']}")
        if exp["names"] not in blob:
            notes.append(f"诊断未点名 {exp['names']}")
        if prods:
            notes.append("拒绝路径留下 calibrated_* 半成品")
        return ok, notes
    # accept：消费门通过 + 产物存在 + ADU 数值自洽
    tol = exp.get("tol", 0.02)
    if rc != 0:
        notes.append(f"正例 rc={rc}（后续阶段失败，若已过消费门则单列）")
    if not prods:
        notes.append("无 calibrated_* 产物（消费门未通过）")
        return False, notes
    got = median_of(prods[0])
    pred = exp["predict_median"]
    rel = abs(got - pred) / max(1.0, abs(pred))
    notes.append(f"median={got:.3f} 预测={pred:.3f} 相对差={rel:.2e}")
    ok = rel <= tol
    return ok, notes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--binary", default=None)
    ap.add_argument("--work", default=None)
    ap.add_argument("--real", action="store_true", help="用真实 T2 母版/亮场（慢）")
    ap.add_argument("--self-test", action="store_true", help="可执行负例面：判据反转必须判红")
    ap.add_argument("--timeout", type=int, default=1800)
    ap.add_argument("--out-json", default=None)
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    binary = Path(args.binary) if args.binary else repo / "build/astrocs"
    if not binary.is_file():
        print(f"environment: binary missing: {binary}", file=sys.stderr)
        return 2
    work = Path(args.work) if args.work else repo / "run/master_unit_guard"
    work.mkdir(parents=True, exist_ok=True)
    lines = []

    def log(m):
        lines.append(m)
        print(m, flush=True)

    fx = work / "fixtures"
    if args.real:
        F = build_real(fx, repo, log)
        base = {"schema_version": "1", "input_lights": [str(F["light"])],
                "filter_passband": "red", "cosmetic": {"enabled": True},
                "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0},
                "wcs": {"init_source": "header_pointing", "gaia_data_dir": "GaiaDR3/"}, "snr": {}}
        bias_med, dark_med = median_of(F["bias"]) * XISF_SCALE_16BIT, median_of(F["dark"]) * XISF_SCALE_16BIT
        light_med, flat_med = median_of(F["light"]), median_of(F["flat"])
        predict = F["predict_median"]   # K=1（600s/600s）；逐像素 oracle（见 build_real）
        cases = [
            ("N1_real_unit_mix", dict(base, master_bias=str(F["bias"]), master_dark=str(F["dark"]),
                                      master_flat=str(F["flat"]), dark_optimization=True),
             {"kind": "reject", "token": TOK_UNIT, "names": F["bias"].name}),
            # 注意：必须带 master_flat —— 否则 CLI 预检（缺标定帧）先判 error，走不到 cal 节点，
            # 门就会误报"诊断缺 token"（真实事故：首轮 N3_real 即此形状）。U3 在守卫内先于 U1/U2。
            ("N3_real_dark_convention", dict(base, master_bias=str(F["bias"]), master_dark=str(F["dark"]),
                                             master_flat=str(F["flat"])),
             {"kind": "reject", "token": TOK_DARK, "names": F["dark"].name}),
            ("P1_real_declared", dict(base, master_bias=str(F["bias"]), master_dark=str(F["dark"]),
                                      master_flat=str(F["flat"]), dark_optimization=True,
                                      master_units={"bias": "normalized", "dark": "normalized",
                                                    "flat": "normalized"},
                                      master_scale={"bias": XISF_SCALE_16BIT, "dark": XISF_SCALE_16BIT,
                                                    "flat": XISF_SCALE_16BIT},
                                      master_flat_normalize="median"),
             {"kind": "accept", "predict_median": predict, "tol": 0.03}),
            ("N4_real_declaration_invalid", dict(base, master_bias=str(F["bias"]),
                                                master_dark=str(F["dark"]), master_flat=str(F["flat"]),
                                                dark_optimization=True,
                                                master_units={"dark": "normalized"}),
             {"kind": "reject", "token": TOK_DECL, "names": "master_scale.dark"}),
        ]
    else:
        F = build_synthetic(fx, repo, log)
        bias_med, dark_med = F["bias_median"], F["dark_median"]
        light_med = F["light_median"]
        predict = F["predict_median"]   # 逐像素 NumPy oracle（不再用中位数近似）
        base = {"schema_version": "1", "input_lights": [str(F["light"])],
                "filter_passband": "red", "cosmetic": {"enabled": True},
                "drizzle": {"nside": 512, "nested": 1, "pixfrac": 1.0, "precision_mode": 0},
                "wcs": {"init_source": "header_pointing", "gaia_data_dir": "GaiaDR3/"}, "snr": {}}
        cases = [
            ("N1_synth_unit_mix", dict(base, master_bias=str(fx / "bias_norm.xisf"),
                                       master_dark=str(fx / "dark_norm.xisf"),
                                       master_flat=str(fx / "flat_norm_adu.fits"),
                                       dark_optimization=True),
             {"kind": "reject", "token": TOK_UNIT, "names": "bias_norm.xisf"}),
            ("N2_synth_flat_unnormalized", dict(base, master_bias=str(fx / "bias_adu.fits"),
                                                master_dark=str(fx / "dark_adu.fits"),
                                                master_flat=str(fx / "flat_raw_adu.fits"),
                                                dark_optimization=True),
             {"kind": "reject", "token": TOK_FLAT, "names": "flat_raw_adu.fits"}),
            ("N3_synth_dark_convention", dict(base, master_bias=str(fx / "bias_adu.fits"),
                                              master_dark=str(fx / "dark_adu.fits"),
                                              master_flat=str(fx / "flat_norm_adu.fits")),
             {"kind": "reject", "token": TOK_DARK, "names": "dark_adu.fits"}),
            ("P1_synth_declared", dict(base, master_bias=str(fx / "bias_norm.xisf"),
                                       master_dark=str(fx / "dark_norm.xisf"),
                                       master_flat=str(fx / "flat_norm.xisf"),
                                       dark_optimization=True,
                                       master_units={"bias": "normalized", "dark": "normalized",
                                                     "flat": "normalized"},
                                       master_scale={"bias": XISF_SCALE_16BIT, "dark": XISF_SCALE_16BIT,
                                                     "flat": XISF_SCALE_16BIT},
                                       master_flat_normalize="median"),
             {"kind": "accept", "predict_median": predict, "tol": 0.01}),
            # N4: 声明自洽（U4）——声明 normalized 却未给到 ADU 的换算因子（文件不携带该因子）
            ("N4_synth_declaration_invalid", dict(base, master_bias=str(fx / "bias_adu.fits"),
                                                 master_dark=str(fx / "dark_adu.fits"),
                                                 master_flat=str(fx / "flat_norm_adu.fits"),
                                                 dark_optimization=True,
                                                 master_units={"bias": "normalized"}),
             {"kind": "reject", "token": TOK_DECL, "names": "master_scale.bias"}),
            # P2: **不过度拒绝**——母版本就 ADU（median>1）+ 平场本就归一（median∈带内）+ 约定已声明
            #      ⇒ 无需任何标度声明即应通过（回归锚：门不是"一律拒绝"）
            ("P2_synth_already_conforming", dict(base, master_bias=str(fx / "bias_adu.fits"),
                                                master_dark=str(fx / "dark_adu.fits"),
                                                master_flat=str(fx / "flat_norm_adu.fits"),
                                                dark_optimization=True),
             {"kind": "accept", "predict_median": F["predict_median_already_norm"], "tol": 0.03}),
        ]

    results = []
    for name, cfg, expect in cases:
        c = run_case(binary, repo, work, name, cfg, args.timeout)
        ok, notes = check_expectation(c, expect, invert=args.self_test)
        results.append({"case": name, "expect": expect["kind"], "ok": ok, "notes": notes,
                        "rc": c["rc"], "seconds": c["seconds"], "config": c["config"]})
        log(f"[{'PASS' if ok else 'FAIL'}] {name} ({expect['kind']}) " + " | ".join(notes))

    all_ok = all(r["ok"] for r in results)
    report = {"tool": "check_master_unit_guard", "mode": "real" if args.real else "synthetic",
              "self_test": bool(args.self_test), "passed": all_ok, "cases": results,
              "flat_band_default": FLAT_BAND_DEFAULT, "tokens": [TOK_UNIT, TOK_FLAT, TOK_DARK, TOK_DECL]}
    if args.out_json:
        Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        Path(args.out_json).write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
    (work / ("self_test.log" if args.self_test else "check.log")).write_text("\n".join(lines) + "\n",
                                                                            encoding="utf-8")
    if args.self_test:
        # 自检语义：期望 token 被替换为哨兵后，三条负例必须全部判红；
        # 若有任一条仍判绿 ⇒ 检查器的 token 判据是空转（无判别力）。
        if all_ok:
            print("SELF-TEST FAIL: token 变异后仍全绿 ⇒ 检查器无判别力", file=sys.stderr)
            return 3
        print("SELF-TEST OK: token 变异后按预期判红（检查器有判别力）")
        return 0
    print(f"RESULT: {'PASS' if all_ok else 'FAIL'} ({sum(1 for r in results if r['ok'])}/{len(results)})")
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
