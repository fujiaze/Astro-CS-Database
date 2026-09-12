#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""STD-F1 导出边界 +1 桥接第三方(astropy)交叉验证 + 九宫格显式判定。

合同锚:
  - docs/science/ASTROMETRY.md §5/§7 (xp = x+1; CRPIX 1-based; 往返 <1e-6 px;
    冻结门 1e-4 px)
  - docs/standards/STANDARDS_REGISTRY.md STD-F1 (FITS WCS Paper I §2.1.1)
  - 前台裁决 R-02(方案 b) / R-25: ipv 内部 0-based 自洽; FITS 导出 1-based;
    唯一桥接点 = Phase3 导出边界 (lib/phase3_session/p3_wcs.cpp fits_pixel_1based)

本脚本做两件事 (均为第三方 astropy 独立实现对拍, 只入测试面, 零生产依赖):

  §1 Phase3 导出边界九宫格 (中心 1 格 + 四角 4 格 + 四边中点 4 格, 每格
     100x100 px, 逐像素; 生产侧统计由 tests/unit/p3_wcs_test.cpp 导出 JSON):
       a) 声明桥接 (astropy origin=0 ⇔ xp = x0+1): 与生产逐点一致 < 1e-9 deg
          ⇒ **无 1px 偏移** (正向锁);
       b) 负向注入「桥接移除」(origin=1) 与「双重桥接」(origin=0 且 x0+1):
          像素偏差必须 ≥ 1 px (2D = √2 px) ⇒ 对拍必须 FAIL
          (证明桥接不是恒真装饰);
       c) 逆向: astropy all_world2pix vs 生产像素 < 1e-4 px;
       d) 产出结构化结论 JSON + 可读预览 PNG (九宫格逐格判定)。

  §2 ipv 导出侧四向桥接扫描 (冻结 run 期探针为可重跑用例; 输入 = p1wcs apbp
     组导出的 wcs003_cross_input.json, 由本脚本自行重跑生成):
       offset = +1 (声明, 必须 PASS 机器精度) / 0 / -1 / +2 (必须 FAIL ≥ 1px)。

工作目录解析 (环境无关; 宪章 §14.1 不写死服务器绝对路径):
  --work-dir > STD_F1_WORK_DIR > 仓库内相对 run/std_f1_adj > tempfile 兜底;
  不可创建/不可写 → 立即 rc=2 fail-fast (不静默跳过)。

退出码: 0 = 全部判定通过; 1 = 判定失败; 2 = 环境/二进制缺失 (fail-fast);
        3 = 自检 (守卫) 失败。
依赖: astropy, numpy, Pillow (仅预览图; 缺失时跳过 PNG 并显式记录, 不静默)。
"""
from __future__ import annotations

import argparse
import json
import math
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
from astropy.wcs import WCS, Sip

WORK_DIR_NAME = "std_f1_adj"
WORK_DIR_ENV = "STD_F1_WORK_DIR"
P1WCS_BIN_ENV = "P1WCS_TESTS_BIN"
P3_BIN_ENV = "P3WCS_TESTS_BIN"

FREEZE_PX = 1e-4        # 冻结门 (Paper I 交叉) — 不放宽
ROUNDTRIP_PX = 1e-6     # SCI-WCS-001 §7 往返不变量
FWD_DEG = 1e-9          # 前向第三方交叉门
BRIDGE_DECLARED = 1.0   # 声明桥接偏移 (STD-F1 / R-02 方案 b)
WRONG_OFFSETS = (0.0, -1.0, 2.0)   # 负向注入: 移除/错置桥接

EXIT_JUDGE_FAIL = 1
EXIT_ENV_FAIL = 2
EXIT_GUARD_FAIL = 3


class EnvError(RuntimeError):
    """环境不满足 (工作目录不可写 / 二进制缺失) — fail-fast, 不静默跳过。"""


# --------------------------------------------------------------------- 路径 ----
def repo_root():
    here = Path(__file__).resolve()
    for parent in here.parents:
        if (parent / "CMakeLists.txt").is_file() and (parent / "tests").is_dir():
            return parent
    return None


def default_work_dir():
    root = repo_root()
    if root is not None:
        return root / "run" / WORK_DIR_NAME
    return Path(tempfile.gettempdir()) / WORK_DIR_NAME


def resolve_work_dir(cli):
    if cli:
        return Path(cli).expanduser(), "--work-dir"
    env = os.environ.get(WORK_DIR_ENV, "").strip()
    if env:
        return Path(env).expanduser(), WORK_DIR_ENV
    return default_work_dir(), "default(repo-relative run/%s)" % WORK_DIR_NAME


def ensure_writable_dir(path, source):
    path = Path(path)
    try:
        os.makedirs(str(path), exist_ok=True)
    except OSError as exc:
        raise EnvError("work dir not creatable: %s (source: %s): %s: %s"
                       % (path, source, type(exc).__name__, exc))
    probe = path / ".std_f1_write_probe"
    try:
        with open(str(probe), "w") as fh:
            fh.write("std_f1\n")
        os.unlink(str(probe))
    except OSError as exc:
        raise EnvError("work dir not writable: %s (source: %s): %s: %s"
                       % (path, source, type(exc).__name__, exc))
    return path


def find_binary(env_name, names):
    """按 env → 常见构建树位置查找测试二进制; 找不到 → EnvError (不静默)。"""
    env = os.environ.get(env_name, "").strip()
    if env:
        p = Path(env)
        if p.is_file():
            return p
        raise EnvError("%s=%s 不存在" % (env_name, env))
    root = repo_root()
    cands = []
    if root is not None:
        for build in ("run/ci/build-gcc-release", "build", "out/build"):
            for sub in ("tests/unit", "tests/unit/p1wcs", "tests/unit/p3wcs"):
                for n in names:
                    cands.append(root / build / sub / n)
    for c in cands:
        if c.is_file():
            return c
    raise EnvError("找不到测试二进制 %s (设置 %s 或先构建)" % (names, env_name))


def run_cmd(cmd, env, log_path, timeout=1800):
    """带 timeout 执行外部命令并落日志; 返回 (rc, stdout, stderr)。"""
    merged = dict(os.environ)
    merged.update(env)
    try:
        proc = subprocess.run(cmd, env=merged, capture_output=True, text=True,
                              timeout=timeout)
        rc, out, err = proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired as exc:
        rc, out, err = 124, exc.stdout or "", "TIMEOUT after %ss" % timeout
    with open(str(log_path), "w") as fh:
        fh.write("argv: %s\ncwd: %s\nrc: %s\n--- stdout ---\n%s\n--- stderr ---\n%s\n"
                 % (" ".join(str(c) for c in cmd), os.getcwd(), rc, out, err))
    return rc, out, err


# ------------------------------------------------------------- 工具: 天球量 ----
def sep_deg(ra1, dec1, ra2, dec2):
    """两天天球坐标的角距 (deg), 小角近似足够 (1px 量级) 但用 haversine 更稳。"""
    r1, d1, r2, d2 = map(np.radians, (ra1, dec1, ra2, dec2))
    dra = r2 - r1
    dra = (dra + np.pi) % (2 * np.pi) - np.pi
    s = np.sin((d2 - d1) / 2.0) ** 2 + np.cos(d1) * np.cos(d2) * np.sin(dra / 2.0) ** 2
    return np.degrees(2.0 * np.arcsin(np.minimum(1.0, np.sqrt(s))))


def parse_keywords(text):
    """'CRPIX1 = 513.0\nCRVAL1 = ...' → {'CRPIX1': 513.0, ...}"""
    kw = {}
    for line in text.splitlines():
        if "=" not in line:
            continue
        k, v = line.split("=", 1)
        k = k.strip()
        v = v.strip().strip("'").strip()
        try:
            kw[k] = float(v)
        except ValueError:
            kw[k] = v
    return kw


def wcs_from_keywords(kw):
    w = WCS(naxis=2)
    w.wcs.crpix = [kw["CRPIX1"], kw["CRPIX2"]]
    w.wcs.crval = [kw["CRVAL1"], kw["CRVAL2"]]
    w.wcs.cd = np.array([[kw["CD1_1"], kw["CD1_2"]], [kw["CD2_1"], kw["CD2_2"]]])
    w.wcs.ctype = ["RA---TAN", "DEC--TAN"]
    scale = math.sqrt(abs(kw["CD1_1"] * kw["CD2_2"] - kw["CD1_2"] * kw["CD2_1"]))
    return w, scale


def build_sip_matrix(flat, order, stride):
    n = order + 1
    m = np.zeros((n, n))
    for i in range(order + 1):
        for j in range(order + 1 - i):
            m[i][j] = flat[i * stride + j]
    return m


# ------------------------------------------------------------------- §1 九宫格 ----
def section1_p3_nine_grid(p3_bin, work_dir, report):
    export = work_dir / "p3_nine_grid_export.json"
    log = work_dir / "logs" / "p3_wcs_test_nine_grid.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    rc, out, err = run_cmd([str(p3_bin)], {"STD_F1_P3_EXPORT": str(export)}, log)
    if rc != 0:
        raise EnvError("p3_wcs_test 运行失败 rc=%d (见 %s)" % (rc, log))
    data = json.loads(export.read_text())
    if data.get("schema") != "std_f1/p3-nine-grid-export-v1":
        raise EnvError("导出 schema 不符: %s" % data.get("schema"))

    report["nine_grid"] = {
        "schema": "std_f1/p3-nine-grid-astropy-cross-v1",
        "astropy_version": __import__("astropy").__version__,
        "export": str(export),
        "frozen_gate_px": FREEZE_PX,
        "roundtrip_gate_px": ROUNDTRIP_PX,
        "forward_gate_deg": FWD_DEG,
        "bridge_declared_offset_px": BRIDGE_DECLARED,
        "negative_injection_offsets_px": list(WRONG_OFFSETS),
        "cells": [],
        "all_pass": True,
    }
    ok_all = True
    for run in data["runs"]:
        parity = run["descriptor"]["parity"]
        w, scale = wcs_from_keywords(parse_keywords(run["descriptor"]["keywords"]))
        for cell in run["cells"]:
            s = cell["samples"]
            x = np.array([p["x"] for p in s])
            y = np.array([p["y"] for p in s])
            ra = np.array([p["ra"] for p in s])
            dec = np.array([p["dec"] for p in s])

            # a) 声明桥接 (origin=0 ⇔ xp = x0+1): 与生产一致
            ra_a, dec_a = w.all_pix2world(x, y, 0)
            d_decl = float(np.max(sep_deg(ra_a, dec_a, ra, dec)))
            px_decl = d_decl / scale

            # b) 负向注入: 桥接移除 (origin=1) / 双重桥接 (origin=0 且 x0+1)
            ra_nb, dec_nb = w.all_pix2world(x, y, 1)
            px_nb = float(np.max(sep_deg(ra_nb, dec_nb, ra, dec))) / scale
            ra_db, dec_db = w.all_pix2world(x + 1.0, y + 1.0, 0)
            px_db = float(np.max(sep_deg(ra_db, dec_db, ra, dec))) / scale

            # c) 逆向: astropy 数值反演 vs 生产像素
            px_a, py_a = w.all_world2pix(ra, dec, 0, tolerance=1e-9, maxiter=200)
            rev_px = float(np.max(np.hypot(px_a - x, py_a - y)))

            ok = (px_decl < ROUNDTRIP_PX and rev_px < FREEZE_PX
                  and px_nb > FREEZE_PX and px_db > FREEZE_PX)
            ok_all = ok_all and ok
            report["nine_grid"]["cells"].append({
                "parity": parity,
                "name": cell["name"],
                "kind": cell["kind"],
                "x0": cell["x0"], "y0": cell["y0"],
                "width": cell["width"], "height": cell["height"],
                "n_pixels": cell["n_pixels"],
                "n_samples": len(s),
                "prod_roundtrip_max_px": cell["max_roundtrip_px"],
                "astropy_declared_max_px": px_decl,
                "astropy_declared_max_deg": d_decl,
                "astropy_reverse_max_px": rev_px,
                "neg_nobridge_max_px": px_nb,
                "neg_double_max_px": px_db,
                "verdict": "PASS" if ok else "FAIL",
            })
            print("[STD-F1 nine-grid/%s] %-9s decl=%.3e px rev=%.3e px "
                  "nobridge=%.6f px double=%.6f px %s"
                  % (parity, cell["name"], px_decl, rev_px, px_nb, px_db,
                     "PASS" if ok else "FAIL"))
    report["nine_grid"]["all_pass"] = ok_all
    return ok_all


# ----------------------------------------------------------- §2 ipv 四向扫描 ----
def section2_ipv_bridge_sweep(p1wcs_bin, work_dir, report):
    cross_input = work_dir / "wcs003_cross_input.json"
    log = work_dir / "logs" / "p1wcs_apbp_export.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    rc, out, err = run_cmd([str(p1wcs_bin), "apbp"],
                           {"P1WCS_CROSS_OUT": str(cross_input)}, log)
    if rc != 0:
        raise EnvError("p1wcs_tests apbp 运行失败 rc=%d (见 %s)" % (rc, log))
    data = json.loads(cross_input.read_text())

    report["ipv_bridge_sweep"] = {
        "schema": "std_f1/ipv-bridge-sweep-v1",
        "cross_input": str(cross_input),
        "offsets_px": [BRIDGE_DECLARED] + list(WRONG_OFFSETS),
        "fixtures": [],
        "all_pass": True,
    }
    ok_all = True
    for fx in data["fixtures"]:
        pts = fx["points"]
        x_f = np.array([p["x_f"] for p in pts])
        y_f = np.array([p["y_f"] for p in pts])
        ra_f = np.array([p["ra_fwd"] for p in pts])
        dec_f = np.array([p["dec_fwd"] for p in pts])
        x_it = np.array([p["x_iter"] for p in pts])
        y_it = np.array([p["y_iter"] for p in pts])
        cd = np.array(fx["cd"])
        scale = math.sqrt(abs(cd[0][0] * cd[1][1] - cd[0][1] * cd[1][0]))
        a = build_sip_matrix(fx["A"], fx["sip_order"], 6)
        b = build_sip_matrix(fx["B"], fx["sip_order"], 6)
        ap = build_sip_matrix(fx["APx"], fx["apx_order"], 10)
        bp = build_sip_matrix(fx["BPx"], fx["apx_order"], 10)

        entry = {"name": fx["name"], "dist_scale": fx["dist_scale"],
                 "pixel_scale_deg_per_px": scale, "arms": []}
        for off in [BRIDGE_DECLARED] + list(WRONG_OFFSETS):
            crpix = np.array([fx["crpix"][0] + off, fx["crpix"][1] + off])
            w = WCS(naxis=2)
            w.wcs.crpix = crpix
            w.wcs.crval = fx["crval"]
            w.wcs.cd = cd
            w.wcs.ctype = ["RA---TAN-SIP", "DEC--TAN-SIP"]
            w.sip = Sip(a, b, ap, bp, crpix)
            ra_a, dec_a = w.all_pix2world(x_f, y_f, 0)
            fwd_deg = float(np.max(sep_deg(ra_a, dec_a, ra_f, dec_f)))
            fwd_px = fwd_deg / scale
            px_a, py_a = w.all_world2pix(ra_f, dec_f, 0, tolerance=1e-9, maxiter=200)
            rev_px = float(np.max(np.hypot(px_a - x_it, py_a - y_it)))
            declared = (abs(off - BRIDGE_DECLARED) < 1e-12)
            if declared:
                arm_ok = (fwd_deg < FWD_DEG) and (rev_px < FREEZE_PX)
            else:
                # 负向注入: 必须被检出 (任一方向 ≥ 1px 量级)
                arm_ok = (fwd_px > 1.0) or (rev_px > 1.0)
            entry["arms"].append({"offset_px": off, "declared": declared,
                                  "forward_max_deg": fwd_deg,
                                  "forward_max_px": fwd_px,
                                  "reverse_max_px": rev_px,
                                  "verdict": "PASS" if arm_ok else "FAIL"})
            ok_all = ok_all and arm_ok
            print("[STD-F1 ipv/%s] offset=%+g fwd=%.3e deg (%.3f px) rev=%.3e px %s"
                  % (fx["name"], off, fwd_deg, fwd_px, rev_px,
                     "PASS" if arm_ok else "FAIL"))
        report["ipv_bridge_sweep"]["fixtures"].append(entry)
    report["ipv_bridge_sweep"]["all_pass"] = ok_all
    return ok_all


# --------------------------------------------------------------- 预览 PNG ----
def write_preview(report, work_dir):
    """九宫格可读预览: 左 = 帧布局 + 逐格判定; 右 = 逐格偏差条形 (px)。"""
    try:
        from PIL import Image, ImageDraw, ImageFont
    except Exception as exc:  # pragma: no cover
        report["preview_png"] = None
        report["preview_note"] = "Pillow 不可用 (%s): 跳过 PNG, 结论以 JSON 为准" % exc
        return None
    cells = report["nine_grid"]["cells"]
    left = [c for c in cells if c["parity"] == "east_left"]
    W, H = 1180, 560
    img = Image.new("RGB", (W, H), "white")
    dr = ImageDraw.Draw(img)
    try:
        font = ImageFont.load_default()
    except Exception:  # pragma: no cover
        font = None
    dr.text((16, 12), "STD-F1 export-boundary +1 bridge | nine-grid 9x100x100 px "
                      "| frame 1024x1024 | astropy cross", fill="black", font=font)
    # 左: 帧示意 (缩放 0.45)
    sc = 0.45
    ox, oy = 20, 60
    dr.rectangle([ox, oy, ox + 1024 * sc, oy + 1024 * sc], outline="black")
    for c in left:
        x0 = ox + c["x0"] * sc
        y0 = oy + c["y0"] * sc
        x1 = x0 + c["width"] * sc
        y1 = y0 + c["height"] * sc
        good = c["verdict"] == "PASS"
        dr.rectangle([x0, y0, x1, y1], outline=("green" if good else "red"),
                     fill=("#e8f5e9" if good else "#ffebee"))
        dr.text((x0 + 2, y0 + 2), c["name"], fill="black", font=font)
        dr.text((x0 + 2, y0 + 14), "decl %.1e px" % c["astropy_declared_max_px"],
                fill="black", font=font)
        dr.text((x0 + 2, y0 + 26), "no-br %.3f px" % c["neg_nobridge_max_px"],
                fill="darkred", font=font)
        dr.text((x0 + 2, y0 + 38), "dbl %.3f px" % c["neg_double_max_px"],
                fill="darkred", font=font)
    dr.text((20, oy + 1024 * sc + 8),
            "cells: green = declared bridge matches astropy (no 1px offset); "
            "no-br/dbl = negative injection (must be ~1.41 px)", fill="black", font=font)
    # 右: 逐格偏差条形
    bx, by = 560, 90
    dr.text((bx, by - 26), "per-cell offset vs astropy (px, log10)", fill="black",
            font=font)
    dr.line([bx, by + 360, bx + 580, by + 360], fill="black")
    for frac, label in ((0.0, "1e-10"), (0.5, "1e-5"), (1.0, "1e0")):
        yy = by + 360 - frac * 340
        dr.line([bx, yy, bx + 580, yy], fill="#dddddd")
        dr.text((bx + 584, yy - 6), label, fill="black", font=font)
    def logmap(v):
        v = max(v, 1e-12)
        return (math.log10(v) + 10.0) / 10.0
    for k, c in enumerate(left):
        x = bx + 10 + k * 62
        for j, (val, col) in enumerate((
                (c["astropy_declared_max_px"], "green"),
                (c["neg_nobridge_max_px"], "red"),
                (c["neg_double_max_px"], "orange"))):
            h = logmap(val) * 340
            dr.rectangle([x + j * 18, by + 360 - h, x + j * 18 + 14, by + 360],
                         fill=col)
        dr.text((x, by + 366), c["name"][:6], fill="black", font=font)
    dr.text((bx, by + 392), "green=declared (pass)  red=bridge removed  "
                            "orange=double bridge", fill="black", font=font)
    out = work_dir / "nine_grid_preview.png"
    img.save(str(out))
    report["preview_png"] = str(out)
    return out


# ------------------------------------------------------------------- main ----
def main(argv=None):
    ap = argparse.ArgumentParser(description="STD-F1 导出边界桥接 astropy 交叉 + 九宫格")
    ap.add_argument("--work-dir", default=None)
    ap.add_argument("--p1wcs-bin", default=None)
    ap.add_argument("--p3-bin", default=None)
    args = ap.parse_args(argv)

    try:
        work_dir, source = resolve_work_dir(args.work_dir)
        ensure_writable_dir(work_dir, source)
        (work_dir / "logs").mkdir(parents=True, exist_ok=True)
        p1wcs_bin = (Path(args.p1wcs_bin) if args.p1wcs_bin
                     else find_binary(P1WCS_BIN_ENV, ["p1wcs_tests"]))
        p3_bin = (Path(args.p3_bin) if args.p3_bin
                  else find_binary(P3_BIN_ENV, ["p3_wcs_test"]))
    except EnvError as exc:
        print("FAIL(env): %s" % exc)
        return EXIT_ENV_FAIL

    print("[std-f1] work_dir=%s (source: %s)" % (work_dir, source))
    print("[std-f1] p1wcs_bin=%s p3_bin=%s" % (p1wcs_bin, p3_bin))

    report = {"schema": "std_f1/bridge-cross-report-v1",
              "work_dir": str(work_dir),
              "p1wcs_bin": str(p1wcs_bin), "p3_bin": str(p3_bin)}
    try:
        ok1 = section1_p3_nine_grid(p3_bin, work_dir, report)
        ok2 = section2_ipv_bridge_sweep(p1wcs_bin, work_dir, report)
    except EnvError as exc:
        print("FAIL(env): %s" % exc)
        return EXIT_ENV_FAIL

    write_preview(report, work_dir)
    out = work_dir / "std_f1_bridge_cross.json"
    out.write_text(json.dumps(report, indent=1))
    print("[std-f1] report=%s" % out)

    # 自检: 声明的两条判定必须真的被求值过 (防恒真/空跑)
    n_cells = len(report["nine_grid"]["cells"])
    n_arms = sum(len(f["arms"]) for f in report["ipv_bridge_sweep"]["fixtures"])
    if n_cells != 18 or n_arms == 0:
        print("FAIL(guard): 判定未被真实求值 (cells=%d arms=%d)" % (n_cells, n_arms))
        return EXIT_GUARD_FAIL

    if ok1 and ok2:
        print("STD-F1 BRIDGE CROSS PASS (九宫格 18 格无 1px 偏移; 负向注入全部检出)")
        return 0
    print("STD-F1 BRIDGE CROSS FAIL")
    return EXIT_JUDGE_FAIL


if __name__ == "__main__":
    sys.exit(main())
