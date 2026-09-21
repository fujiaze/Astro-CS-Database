#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_resource_gate_real.py — RESOURCE-GATE-REAL 的判定校验器 + 可执行负例面。

背景（`docs/owner/RELEASE_STATUS.md §5`）：「重计算面必须显式请求 `--gate-required`
并附判定证据，缺失即 fail-closed」。原 CI 注册表的 11 个 CHK-RESOURCE 步全是静态面，
**零真实运行利用率观测** ⇒ 该口径在 CI 上无机器实现。本校验器把真实观测接进 CI：

- **正例**：`eng/ci/resource_monitor.py --gate-required --gate-compute-interval <S>` 监控
  `resource_gate_probe.py --mode busy` 的多进程重计算区间 ⇒ 期望
  `frozen_gate.verdict == "pass"` 且监控 rc == 0；
- **负例（可执行负例面）**：同一门监控 `--mode serial`（单进程空转，与「重计算面」
  定义相反）⇒ 期望 `frozen_gate.verdict == "fail"` 且监控 rc == 10（门真的能红）。

用法:
  python3 eng/tools/quality/check_resource_gate_real.py                 # 正例 + 负例都跑
  python3 eng/tools/quality/check_resource_gate_real.py --fault-inject serial
  python3 eng/tools/quality/check_resource_gate_real.py --self-test     # 等价于默认（自证两态）
exit 0 = 两态均符合预期；1 = 判定不符（真红）；2 = 输入/依赖不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[3]
MONITOR_REL = "eng/ci/resource_monitor.py"
PROBE_REL = "eng/tools/quality/resource_gate_probe.py"
OUT_DIR_REL = "run/ci/resource-gate-real"


def run_case(mode: str, seconds: int, out_dir: pathlib.Path) -> tuple[int, dict, str]:
    out = out_dir / f"resource_gate_{mode}.json"
    if out.is_file():
        out.unlink()
    cmd = [sys.executable, MONITOR_REL, "--timeout", str(seconds * 4 + 120),
           "--output", str(out.relative_to(REPO)),
           "--gate-required", "--gate-compute-interval", str(seconds),
           "--", sys.executable, PROBE_REL, "--mode", mode, "--seconds", str(seconds)]
    proc = subprocess.run(cmd, cwd=REPO, capture_output=True, text=True,
                          timeout=seconds * 6 + 300)
    gate = {}
    if out.is_file():
        try:
            gate = json.loads(out.read_text(encoding="utf-8")).get("frozen_gate") or {}
        except Exception as exc:  # noqa: BLE001
            return proc.returncode, {}, f"监控证据不可解析: {exc}"
    else:
        return proc.returncode, {}, f"监控证据缺失: {out}"
    tail = (proc.stdout or proc.stderr or "").strip().splitlines()
    return proc.returncode, gate, tail[-1] if tail else ""


def main() -> int:
    ap = argparse.ArgumentParser(description="真实资源利用率门的判定校验器（含负例面）")
    ap.add_argument("--seconds", type=int, default=20, help="计算区间秒（须 > 门的最小适用区间）")
    ap.add_argument("--fault-inject", choices=("serial",), default=None,
                    help="只跑负例注入形态（期望门判红）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="正例 + 负例都跑并断言两态（默认行为）")
    args = ap.parse_args()

    for rel in (MONITOR_REL, PROBE_REL):
        if not (REPO / rel).is_file():
            print(f"RESOURCE_GATE_REAL_FAIL: 依赖缺失 {rel}（fail-closed）", file=sys.stderr)
            return 2
    out_dir = REPO / OUT_DIR_REL
    out_dir.mkdir(parents=True, exist_ok=True)

    cases = [("serial", "fail", 10)] if args.fault_inject else             [("busy", "pass", 0), ("serial", "fail", 10)]
    failures = []
    for mode, want_verdict, want_rc in cases:
        rc, gate, note = run_case(mode, args.seconds, out_dir)
        got = gate.get("verdict")
        viol = gate.get("violations")
        print(f"[{mode}] rc={rc} frozen_gate.verdict={got!r} violations={viol} {note}")
        if rc != want_rc:
            failures.append(f"{mode}: 监控 rc={rc} 期望 {want_rc}")
        if got != want_verdict:
            failures.append(f"{mode}: frozen_gate.verdict={got!r} 期望 {want_verdict!r}")

    if failures:
        print("RESOURCE_GATE_REAL_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print(f"RESOURCE_GATE_REAL_PASS: 真实重计算区间门判 pass（rc=0）且串行注入判 fail（rc=10）"
          f"（interval={args.seconds}s，证据 {OUT_DIR_REL}/resource_gate_*.json）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
