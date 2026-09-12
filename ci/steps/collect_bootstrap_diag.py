#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/steps/collect_bootstrap_diag.py — 失败后 bootstrap 诊断收集（CI-001B 迁出 workflow）。

来源：.github/workflows/ci-linux.yml 与 ci-windows.yml 原 step
「Collect bootstrap diagnostics」的内联 heredoc 体（两平台各一份，逐行等价）。
CI-001B 目标 1：workflow 的 run: 体不再内联业务命令，统一由 ci/ 声明体承担
（声明见 ci/workflow_binding.json，role=infra-step）。

语义与迁移前逐行等价：

- 仅在失败路径被调用（workflow 侧 if: failure()），不掩盖首要失败原因；
- 采集 ci/bootstrap.py --json 的 rc / 报告 / 结构化 stderr FAIL 载荷；
- 落 artifacts/ci/BOOTSTRAP_DIAG.json（与 run.py 证据同目录，随 artifact 上传）。

退出码恒为 0：诊断步不得把首要失败原因替换成自身失败；采集异常记入
collection_error 字段而非抛出。
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def collect(platform: str, repo: Path, timeout: float = 120.0) -> dict:
    """跑 bootstrap 探测并汇总为诊断载荷（异常不抛出，记 collection_error）。"""
    diag = {
        "schema_version": 1,
        "purpose": "bootstrap diagnostics collected after a failed step (does not mask the primary cause)",
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "platform": platform,
        "bootstrap_exit_code": None,
        "report": None,
        "stderr_fail_payload": None,
    }
    try:
        res = subprocess.run(
            [sys.executable, "ci/bootstrap.py",
             "--policy", "ci/toolchain.policy.json", "--platform", platform, "--json"],
            cwd=str(repo), capture_output=True, text=True, timeout=timeout,
            encoding="utf-8", errors="replace")
        diag["bootstrap_exit_code"] = res.returncode
        if res.stdout.strip():
            try:
                diag["report"] = json.loads(res.stdout)
            except json.JSONDecodeError:
                pass
        for line in reversed((res.stderr or "").strip().splitlines()):
            try:
                cand = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(cand, dict) and cand.get("verdict") == "FAIL":
                diag["stderr_fail_payload"] = cand
                break
    except Exception as exc:  # noqa: BLE001 - 诊断步不得因采集异常替换首要失败原因
        diag["collection_error"] = repr(exc)
    return diag


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Collect bootstrap diagnostics on failure")
    ap.add_argument("--platform", required=True, choices=("linux", "windows", "fatduck"))
    ap.add_argument("--repo-root", default=None, help="仓库根覆盖（单测注入 fixture）")
    ap.add_argument("--out", default="artifacts/ci/BOOTSTRAP_DIAG.json")
    ap.add_argument("--timeout", type=float, default=120.0)
    args = ap.parse_args(argv)

    repo = Path(args.repo_root).resolve() if args.repo_root else REPO
    diag = collect(args.platform, repo, args.timeout)
    out = Path(args.out)
    if not out.is_absolute():
        out = repo / out
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(diag, ensure_ascii=False, indent=2), encoding="utf-8")
    print("BOOTSTRAP_DIAG.json written (bootstrap_exit_code=%s)"
          % diag["bootstrap_exit_code"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
