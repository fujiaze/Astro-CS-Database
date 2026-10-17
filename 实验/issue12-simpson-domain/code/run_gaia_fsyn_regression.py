#!/usr/bin/env python3
"""Issue #12 —— 真实 Gaia XPSD + 真实响应曲线的 F_syn/零点/光子率回归驱动。

1. 读真实响应曲线 `config/response_curves/*.json`（`wavelength_nm`/`value`）展平成两列文本；
2. 用指定 `--impl`（pre/post 源码）编译 `gaia_fsyn_regression.cpp`；
3. 对真实 `--gaia-dir` 锥形搜索取星+光谱, 打印真实网格元数据, 输出逐星 CSV。

用法:
    python3 run_gaia_fsyn_regression.py --tag post \
        --impl lib/algorithms/photometry/cpp/src/spectrum_integrator.cpp \
        --out /workspace/issue12-scratch
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
HARNESS = os.path.join(REPO, "实验/issue12-simpson-domain/code/gaia_fsyn_regression.cpp")
GAIA_LIB = "build/linux-sci-p5/lib/infrastructure/gaia_xpsd_client"
INCLUDES = ["lib/algorithms/photometry/cpp/src", "lib/algorithms/photometry/cpp/include",
            "lib/infrastructure/gaia_xpsd_client/src"]
# M42 天区, 与既有 XPSD 绝对刻度实证同一量级域
RA, DEC, RADIUS = 83.2835420429, -6.3744273786, 0.5
MAG_LOW, MAG_HIGH = 6.0, 18.0


def flatten_curve(json_path: str, out_txt: str) -> int:
    d = json.load(open(json_path, encoding="utf-8"))
    wl = d["wavelength_nm"]
    val = d.get("value")
    if val is None:
        val = d.get("transmission", d.get("efficiency"))
    with open(out_txt, "w") as f:
        f.write("# %s (%s)\n" % (d.get("name"), os.path.basename(json_path)))
        for a, b in zip(wl, val):
            f.write("%.10g %.10g\n" % (a, b))
    return len(wl)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--tag", required=True)
    ap.add_argument("--impl", required=True, help="被测 spectrum_integrator.cpp")
    ap.add_argument("--out", required=True)
    ap.add_argument("--gaia-dir", default=os.path.join(REPO, "gaia", "GaiaDR3SP"))
    ap.add_argument("--filter", default="Baader_7nm_H_alpha.json")
    ap.add_argument("--qe", default="Ideal_QE_curve.json")
    ap.add_argument("--no-qe", action="store_true")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    curves = os.path.join(a.out, "curves_" + a.tag)
    os.makedirs(curves, exist_ok=True)
    fw = os.path.join(curves, "filter.txt")
    qw = os.path.join(curves, "qe.txt")
    nf = flatten_curve(os.path.join(REPO, "config/response_curves", a.filter), fw)
    nq = 0 if a.no_qe else flatten_curve(os.path.join(REPO, "config/response_curves", a.qe), qw)
    print(f"[{a.tag}] filter={a.filter} ({nf} pts) qe={'-' if a.no_qe else a.qe} ({nq} pts)")

    exe = os.path.join(a.out, "gaia_fsyn_" + a.tag)
    # 生产 .so 把 legacy gaia_client_* 符号本地化 (导出面仅 acsd_module_query_v1),
    # 故直接编译 gaia_client.c 源(默认 GAIA_EXPORT 可见)供本回归程序调用。
    gaia_c = os.path.join(REPO, "lib/infrastructure/gaia_xpsd_client/src/gaia_client.c")
    gaia_o = os.path.join(a.out, "gaia_client_" + a.tag + ".o")
    cc = ["gcc", "-std=c11", "-O2", "-fopenmp",
          "-I", os.path.join(REPO, "lib/infrastructure/gaia_xpsd_client/src"),
          "-I", os.path.join(REPO, "lib/infrastructure/gaia_xpsd_client/include"),
          "-I", os.path.join(REPO, "lib/include"), "-c", gaia_c, "-o", gaia_o]
    cp = subprocess.run(cc, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-3000:])
        return 1
    cmd = ["g++", "-std=c++17", "-O2", "-fopenmp"]
    for i in INCLUDES + ["lib/infrastructure/gaia_xpsd_client/include", "lib/include"]:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += [HARNESS, os.path.abspath(a.impl), gaia_o, "-lz", "-lpthread", "-ldl", "-lm",
            "-o", exe]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-3000:])
        return 1
    csv = os.path.join(a.out, "fsyn_" + a.tag + ".csv")
    run = subprocess.run([exe, a.gaia_dir, str(RA), str(DEC), str(RADIUS),
                          str(MAG_LOW), str(MAG_HIGH), fw, "-" if a.no_qe else qw, csv],
                         cwd=REPO, capture_output=True, text=True)
    log = os.path.join(a.out, "fsyn_" + a.tag + ".log")
    open(log, "w").write(run.stdout + run.stderr)
    print(run.stdout, end="")
    print(run.stderr[-2000:], end="")
    print(f"[{a.tag}] exit={run.returncode} csv={csv}")
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
