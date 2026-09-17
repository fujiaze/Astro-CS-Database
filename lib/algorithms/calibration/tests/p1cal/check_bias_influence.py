#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BIAS-001 · bias 参与门 + ignore-bias 变异注入（可执行负例面）。

判据（exit 0 = PASS）：
  B1 基线：用**生产源**编译 bias_influence_harness.cpp → 全部断言必过；
  M1 ignore-bias：把标准式的 `v -= bias[i]` 变异为 `v -= 0.0*bias[i]` → 必红；
  M2 ignore-K   ：把标准式的 `v -= k * dark[i]` 变异为 `v -= dark[i]` → 必红；
  M3 compat-bias：把兼容式的 `k * (dark[i] - bias[i])` 变异为 `k * dark[i]` → 必红。
变异模式必须**命中**（命中失败 = 实现漂移，判红而非跳过）——禁止"模式找不到就放行"。

用法：python3 lib/algorithms/calibration/tests/p1cal/check_bias_influence.py [--root .] [--json-out F]
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

SRCS = ["src/calibrator.cpp", "src/ac_api.cpp", "src/master_generator.cpp",
        "src/cosmetic_corrector.cpp"]
REL = "lib/algorithms/calibration"
HARNESS = REL + "/tests/p1cal/bias_influence_harness.cpp"
INCLUDE = REL + "/include"
# astro_calibration.h:172 为未接线的 ac::optimize_dark_k 声明引入 hiss_format.h（aio）
EXTRA_INCLUDE = ["lib/infrastructure/aio/include"]
BIN = "bias_influence_gate"

# (id, 文件, 原文, 变异文本, 说明)  —— 原文必须在源文件中逐字命中
MUTATIONS = [
    ("M1_ignore_bias", "src/calibrator.cpp",
     "if (bias) v -= bias[i];", "if (bias) v -= 0.0f * bias[i];",
     "标准式丢弃 bias 项（等价旧实现）"),
    ("M2_ignore_k", "src/calibrator.cpp",
     "if (dark) v -= k * dark[i];", "if (dark) v -= dark[i];",
     "标准式丢弃 K 缩放（等价旧实现强制 k=1）"),
    ("M3_compat_no_separation", "src/calibrator.cpp",
     "k * (dark[i] - bias[i])", "k * dark[i]",
     "兼容式不再做 bias/dark 分离"),
    ("M4_compat_ignore_bias", "src/calibrator.cpp",
     "v = light[i] - bias[i] - k * (dark[i] - bias[i]);",
     "v = light[i] - k * (dark[i] - bias[i]);",
     "兼容式丢弃显式 -bias 项（两条分支的 bias 参与都不可被忽略）"),
]


def compile_gate(root: str, src_dir: str, out_dir: str) -> tuple[int, str]:
    include = os.path.join(root, INCLUDE)
    harness = os.path.join(root, HARNESS)
    if not os.path.isfile(harness):
        return 1, "harness missing: " + harness
    inc = ["-I", include]
    for e in EXTRA_INCLUDE:
        inc += ["-I", os.path.join(root, e)]
    cmd = ["g++", "-std=c++17", "-O2", "-Wno-unknown-pragmas"] + inc + [harness]
    cmd += [os.path.join(src_dir, s) for s in SRCS]
    cmd += ["-o", os.path.join(out_dir, BIN)]
    r = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    return r.returncode, (r.stdout + r.stderr)[-4000:]


def run_gate(root: str, out_dir: str) -> tuple[int, str]:
    exe = os.path.join(out_dir, BIN)
    r = subprocess.run([exe], cwd=root, capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    result = {"check": "BIAS-001-bias-influence-gate", "verdict": "PASS", "steps": []}

    tmp = tempfile.mkdtemp(prefix="bias001_gate_")
    try:
        # ---- B1 基线：生产源必过 ----
        rc_c, log_c = compile_gate(root, os.path.join(root, REL), tmp)
        if rc_c != 0:
            result["verdict"] = "FAIL"
            result["steps"].append({"id": "B1_compile", "status": "FAIL", "detail": log_c})
            print(json.dumps(result, ensure_ascii=False, indent=1))
            return 1
        rc_b, log_b = run_gate(root, tmp)
        base_ok = rc_b == 0 and "BIAS-GATE RESULT: PASS" in log_b
        result["steps"].append({"id": "B1_baseline", "status": "PASS" if base_ok else "FAIL",
                                "rc": rc_b, "detail": log_b.strip().splitlines()[-1:]})
        if not base_ok:
            result["verdict"] = "FAIL"

        # ---- M1..M3 变异注入：每一个都必须让门判红 ----
        for mid, rel, old, new, why in MUTATIONS:
            mdir = os.path.join(tmp, mid)
            shutil.copytree(os.path.join(root, REL), mdir)
            path = os.path.join(mdir, rel)
            with open(path, encoding="utf-8") as fh:
                text = fh.read()
            hits = text.count(old)
            if hits == 0:
                result["steps"].append({"id": mid, "status": "FAIL",
                                        "detail": f"mutation pattern not found (hits=0): {old!r}"})
                result["verdict"] = "FAIL"
                continue
            with open(path, "w", encoding="utf-8") as fh:
                fh.write(text.replace(old, new))
            rc_mc, log_mc = compile_gate(root, mdir, mdir)
            if rc_mc != 0:
                result["steps"].append({"id": mid, "status": "FAIL",
                                        "detail": "mutant compile failed: " + log_mc})
                result["verdict"] = "FAIL"
                continue
            rc_m, log_m = run_gate(root, mdir)
            red = rc_m != 0 and "BIAS-GATE RESULT: FAIL" in log_m
            failed_checks = sorted(set(re.findall(r"BIAS-GATE (T\S+) FAIL", log_m)))
            result["steps"].append({"id": mid, "status": "PASS" if red else "FAIL",
                                    "why": why, "hits": hits, "mutant_rc": rc_m,
                                    "failed_checks": failed_checks})
            if not red:
                result["verdict"] = "FAIL"
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    payload = json.dumps(result, ensure_ascii=False, indent=1, sort_keys=True)
    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(payload + "\n")
    print(payload)
    print("BIAS001_BIAS_INFLUENCE_GATE: " + result["verdict"])
    return 0 if result["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
