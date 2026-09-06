#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci_coverage_runner.py — V8-CI-005 linux-deep 覆盖率检查驱动（owner=SA-CI-32）。

合同（07_CI_MACHINE_CONTRACT.md）：第一次 deep CI 只测量并记录覆盖基线，
不虚构覆盖率阈值。本驱动是 pytest-cov 的薄封装：

  1. 校验 --output-dir 位于仓库内（仓库外 → 受控报错 exit 2，
     stderr 单行诊断，无 traceback；输出目录在仓库外会破坏 workspace
     纯净性与 outputs 登记，属检查配置错误而非测试失败）。
     F6 修复：校验后 out_dir 保持绝对形式，报告落点串统一经
     _as_repo_rel 计算（仓库内 → 仓库相对 posix，落点与 checks.json
     outputs 登记一致；非子路径退化 os.path.relpath；永不抛
     ValueError）——修复前校验成功后 out_dir 被改写为相对路径，输出
     报告处再对其调 relative_to(REPO)（绝对形式）必然抛 ValueError，
     hosted 以相对路径 run/ci/coverage-py 调用时在 --cov-report
     组装处秒败；
  2. 预创建输出目录（pytest-cov 不保证 mkdir 父目录）；
  3. 以固定参数调用 pytest（--cov=lib --cov=cli --cov=tools --cov-branch，
     XML/JSON 报告写入 --output-dir），argv 数组传递、永不 shell=True；
  4. 透传 pytest 退出码（0=全过；其他=测试失败/收集失败）。

依赖：pytest、pytest-cov 由 CI 环境（ubuntu-24.04）提供；缺失时子进程以
非零退出（ci/checks.json 中 DEEP-COV 登记 waivable=true 且
prerequisite_tools=["pytest"]，交由部署任务补齐依赖后解除 waiver）。
本地容器无 pytest → ENV 限制，仅静态验证。
仅 stdlib；外部命令仅 pytest，带 timeout（AGENTS 纪律）。
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def _as_repo_rel(p: Path) -> str:
    """仓库相对 posix 字符串（报告落点展示形态，与 checks.json outputs 一致）。

    F6：Path.relative_to 只在"同一形式且确为子路径"时成功。修复前 out_dir
    在校验后被改写为仓库相对路径，而输出报告处再次对其调
    relative_to(REPO)（绝对）→ 必然 ValueError（hosted 以相对路径
    run/ci/coverage-py 调用时秒败）。本 helper 对仓库内子路径给相对形式，
    非子路径（仓库外，仅受控 exit-2 路径可到达；或 --tests 传绝对路径）
    退化 os.path.relpath，仍失败则给绝对 posix——永不抛 ValueError。
    """
    try:
        return p.resolve(strict=False).relative_to(REPO).as_posix()
    except ValueError:
        try:
            return os.path.relpath(p, REPO)
        except ValueError:
            return p.resolve(strict=False).as_posix()


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description="pytest-cov 覆盖率基线驱动（CI 检查用，透传 pytest 退出码）")
    ap.add_argument("--output-dir", default="run/ci/coverage",
                    help="覆盖率报告输出目录（仓库相对，默认 run/ci/coverage）")
    ap.add_argument("--timeout", type=int, default=1700,
                    help="pytest 子进程超时秒数（默认 1700）")
    ap.add_argument("--tests", default="tests",
                    help="pytest 收集根（仓库相对，默认 tests）")
    args = ap.parse_args(argv)

    out_dir = Path(args.output_dir)
    if not out_dir.is_absolute():
        out_dir = REPO / out_dir
    out_dir = out_dir.resolve(strict=False)
    try:
        out_dir.relative_to(REPO)
    except ValueError:
        # 仓库外输出目录仍为受控报错 exit 2（契约不变：破坏 workspace 纯净性
        # 与 checks.json outputs 登记，属检查配置错误而非测试失败）。
        print("ci_coverage_runner: --output-dir 必须位于仓库内（"
              f"仓库根 {REPO}），实际 {args.output_dir}", file=sys.stderr)
        return 2
    out_dir.mkdir(parents=True, exist_ok=True)

    tests_dir = _as_repo_rel(REPO / args.tests)
    pytest_argv = [
        sys.executable, "-m", "pytest",
        tests_dir,
        "--cov=lib", "--cov=cli", "--cov=tools",
        "--cov-branch",
        f"--cov-report=xml:{_as_repo_rel(out_dir)}/coverage.xml",
        f"--cov-report=json:{_as_repo_rel(out_dir)}/coverage.json",
        "-p", "no:cacheprovider",  # 禁止 .pytest_cache 落在工作区根
        "-q",
    ]
    # coverage.py 默认在 cwd 写 .coverage 数据文件；重定向到输出目录，
    # 保证 mutates_workspace=false 语义下工作区不被污染。
    env = dict(os.environ)
    env["COVERAGE_FILE"] = str(out_dir / ".coverage")
    entry = {"command": pytest_argv, "timeout_seconds": args.timeout}
    try:
        proc = subprocess.run(pytest_argv, cwd=str(REPO), capture_output=True,
                              text=True, timeout=args.timeout, env=env)
        exit_code = proc.returncode
        tail = (proc.stderr or "").strip().splitlines()[-8:]
        entry.update({"exit_code": exit_code, "timed_out": False,
                      "stderr_tail": "\n".join(tail)})
    except FileNotFoundError:
        entry.update({"exit_code": 3, "timed_out": False,
                      "stderr_tail": "pytest 不可用（依赖缺失，CI 环境需提供）"})
        exit_code = 3
    except subprocess.TimeoutExpired:
        entry.update({"exit_code": 124, "timed_out": True,
                      "stderr_tail": f"pytest 超时（{args.timeout}s）"})
        exit_code = 124

    summary = {
        "driver": "tools/quality/ci_coverage_runner.py",
        "generated_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "threshold": None,
        "note": "首次基线测量（合同：只记录不判阈值），阈值冻结由后续质量任务完成",
        "outputs": {
            "xml": f"{_as_repo_rel(out_dir)}/coverage.xml",
            "json": f"{_as_repo_rel(out_dir)}/coverage.json",
        },
        "pytest": entry,
        "exit_code": exit_code,
    }
    (out_dir / "coverage-summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
