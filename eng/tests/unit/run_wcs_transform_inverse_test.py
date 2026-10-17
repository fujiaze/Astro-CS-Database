#!/usr/bin/env python3
"""编译并运行 Issue #92 A1-FIX 的 WcsTransform 逆解单元测试。

以**当前树内** `lib/algorithms/photometry/cpp/src/wcs_transform.cpp` 编译（不链接旧归档），
真实六帧 WCS/samples 从 `实验/issue92-wcs-gaia-match/fixtures/*.json` 生成 `wcs_fixtures.inc`。
`--impl` 可指向修复前副本做 A/B（此时真实帧判据应 FAIL，即负例复现）。

用法:
    python3 eng/tests/unit/run_wcs_transform_inverse_test.py
    python3 eng/tests/unit/run_wcs_transform_inverse_test.py --impl /tmp/wcs_prefix.cpp
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TEST = os.path.join(REPO, "eng", "tests", "unit", "wcs_transform_inverse_test.cpp")
DEFAULT_IMPL = os.path.join(REPO, "lib", "algorithms", "photometry", "cpp", "src",
                            "wcs_transform.cpp")
DEFAULT_FIXTURES = os.path.join(REPO, "实验", "issue92-wcs-gaia-match", "fixtures")
INCLUDES = ["lib/algorithms/photometry/cpp/src",
            "lib/algorithms/photometry/cpp/include"]


def arr36(vals):
    vals = list(vals) + [0.0] * (36 - len(vals))
    vals = vals[:36]
    return "{" + ", ".join(repr(float(v)) for v in vals) + "}"


def gen_fixtures(fixtures_dir: str, out_inc: str) -> int:
    files = sorted(glob.glob(os.path.join(fixtures_dir, "*.json")))
    if not files:
        print(f"no fixtures in {fixtures_dir}", file=sys.stderr)
        return 1
    frames = []
    max_n = 0
    for path in files:
        d = json.load(open(path))
        w = d["wcs"]
        sip = w.get("sip") or {}
        samples = d["samples"]
        max_n = max(max_n, len(samples))
        frames.append((os.path.basename(path), w, sip, samples))
    lines = [
        "// Issue #92 A1-FIX 自动生成: 真实六帧 WCS + samples, 勿手改。",
        "struct FixFrame {",
        "  const char* name;",
        "  double crpix1, crpix2, crval1, crval2, cd11, cd12, cd21, cd22;",
        "  int order, ap_order;",
        "  double a[36], b[36], ap[36], bp[36];",
        f"  int n; double sx[{max_n}], sy[{max_n}], sra[{max_n}], sdec[{max_n}];",
        "};",
        "static const FixFrame kFrames[] = {",
    ]
    for name, w, sip, samples in frames:
        sx = [s["x"] for s in samples]
        sy = [s["y"] for s in samples]
        sra = [s["ra"] for s in samples]
        sdec = [s["dec"] for s in samples]
        pad = max_n - len(samples)
        sx += [0.0] * pad; sy += [0.0] * pad; sra += [0.0] * pad; sdec += [0.0] * pad
        lines.append("  {")
        lines.append(f'    "{name}",')
        lines.append("    %r, %r, %r, %r," % (w["crpix1"], w["crpix2"], w["crval1"], w["crval2"]))
        lines.append("    %r, %r, %r, %r," % (w["cd11"], w["cd12"], w["cd21"], w["cd22"]))
        lines.append(f'    {int(sip.get("order", 0))}, {int(sip.get("ap_order", 0))},')
        lines.append("    " + arr36(sip.get("a") or []) + ",")
        lines.append("    " + arr36(sip.get("b") or []) + ",")
        lines.append("    " + arr36(sip.get("ap") or []) + ",")
        lines.append("    " + arr36(sip.get("bp") or []) + ",")
        lines.append(f"    {len(samples)},")
        lines.append("    {" + ", ".join(repr(float(v)) for v in sx) + "},")
        lines.append("    {" + ", ".join(repr(float(v)) for v in sy) + "},")
        lines.append("    {" + ", ".join(repr(float(v)) for v in sra) + "},")
        lines.append("    {" + ", ".join(repr(float(v)) for v in sdec) + "},")
        lines.append("  },")
    lines.append("};")
    lines.append(f"static const int kNFrames = {len(frames)};")
    with open(out_inc, "w") as f:
        f.write("\n".join(lines) + "\n")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--impl", default=DEFAULT_IMPL, help="被测 wcs_transform.cpp")
    ap.add_argument("--fixtures", default=DEFAULT_FIXTURES)
    ap.add_argument("--work", default="/tmp/wcs_inverse_test")
    ap.add_argument("--out", default="", help="stdout 落盘路径 (可选)")
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)
    inc = os.path.join(a.work, "wcs_fixtures.inc")
    rc = gen_fixtures(a.fixtures, inc)
    if rc != 0:
        return rc

    exe = os.path.join(a.work, "wcs_inverse_test")
    cmd = ["g++", "-std=c++17", "-O1"]
    for i in INCLUDES:
        cmd += ["-I", os.path.join(REPO, i)]
    cmd += ["-I", a.work, TEST, a.impl, "-lm", "-o", exe]
    cp = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True)
    if cp.returncode != 0:
        print(cp.stderr[-4000:])
        print("COMPILE FAILED")
        return 1
    run = subprocess.run([exe], cwd=REPO, capture_output=True, text=True)
    text = run.stdout
    if run.stderr:
        text += "\n[stderr]\n" + run.stderr[-2000:]
    print(text, end="")
    if a.out:
        open(a.out, "w").write(text)
    return run.returncode


if __name__ == "__main__":
    sys.exit(main())
