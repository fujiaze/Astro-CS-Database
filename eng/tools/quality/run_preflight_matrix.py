#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""预检语义矩阵的 CI 包装（E2E-501 步骤 3）：跑 eng/tests/cli/test_preflight_matrix.py
并把结果写成机器可读证据（eng/ci 的步骤必须有 outputs 落盘，否则 runner 记 fail）。

判据 fail-closed：
  * 套件非 0 退出 ⇒ verdict=FAIL，原样透传退出码；
  * 用例数必须 >= 11（矩阵 3 档 × 4 路里 11 条判据），少于 11 判红（防「用例被删空还判绿」）；
  * 输出文件必须写出，否则判红。
"""
import argparse
import io
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
MIN_CASES = 11


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", default="run/ci/cli/preflight_matrix.json")
    args = ap.parse_args()
    cmd = [sys.executable, "-B", "-m", "unittest", "discover",
           "-s", "eng/tests/cli", "-t", "eng/tests/cli", "-p", "test_preflight_matrix.py"]
    pr = subprocess.run(cmd, capture_output=True, text=True, cwd=REPO, timeout=900)
    out = (pr.stdout or "") + (pr.stderr or "")
    m = re.search(r"Ran (\d+) tests?", out)
    cases = int(m.group(1)) if m else 0
    findings = []
    if pr.returncode != 0:
        findings.append("suite rc=%d" % pr.returncode)
    if cases < MIN_CASES:
        findings.append("case count %d < %d（用例被删空/未收集）" % (cases, MIN_CASES))
    verdict = "PASS" if not findings else "FAIL"
    rec = {"tool": "run_preflight_matrix", "cases": cases, "min_cases": MIN_CASES,
           "rc": pr.returncode, "findings": findings, "verdict": verdict,
           "tail": out.strip().splitlines()[-6:]}
    p = os.path.join(REPO, args.output)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    io.open(p, "w", encoding="utf-8").write(json.dumps(rec, ensure_ascii=False, indent=1) + "\n")
    print("PREFLIGHT_MATRIX_%s: cases=%d rc=%d -> %s" % (verdict, cases, pr.returncode, args.output))
    for f in findings:
        print("  " + f)
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
