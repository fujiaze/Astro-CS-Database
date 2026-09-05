#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""deep_ci_driver.py — V8-CI-005 linux-deep/linux-main hosted 检查驱动（owner=SA-CI-32）。

控制包依据（02_CI_TASKS.md V8-CI-005「GitHub Linux」）：
  - ubuntu-24.04；main push：Release GCC configure/build、轻量单测、
    全合成 Oracle、文档/合同/调用图检查（后三者已由 fast/linux-main 现有检查覆盖，
    本驱动只补真实构建链检查 BUILD-GCC-RELEASE）；
  - manual/schedule deep：Clang、ASan+UBSan、目标 TSan、coverage、complexity；
  - heavy 子项必须使用监控（checks.json 中这些项的 command 显式包
    ci/resource_monitor.py --，V8-CI-003 统一监控）。

子命令（每个 = 若干顺序步骤，argv 数组、shell=False、逐步超时）：
  build-gcc-release : Release GCC configure + build 全图（BUILD-GCC-RELEASE）
  build-clang       : Clang configure + build（DEEP-CLANG-BUILD，可 --werror）
  qa-sanitize       : clang ASan+UBSan 配置构建 + CTest 全量（DEEP-SAN-ASAN，
                      复用根 CMakeLists QA-002/QA-006 接线，独立 build dir）
  qa-sanitize-tsan  : clang TSan 同上（DEEP-SAN-TSAN；与 ASan 分 build dir，单选）
  coverage-cpp      : LLVM source-based 覆盖（DEEP-COV-CPP，ccov 目标，产物拷贝到
                      --output-dir 供 run.py outputs 校验）

复杂度基线测量（DEEP-COMPLEXITY）由 tools/quality/check_complexity.py 承担
（单一事实源，本驱动不重复实现）。

退出码：0=全步骤成功；其他=首个失败步骤退出码（timeout→124）。
仅 stdlib；所有外部命令带 timeout（AGENTS 纪律）；构建并行固定 -j2
（GitHub hosted runner 2 核；科学内核线程数选择不在本驱动职责内）。
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

__all__ = ["main", "run_step"]

# ---------------------------------------------------------------------- 基础 ----

def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def run_step(argv: list[str], *, timeout: int, cwd: Path | None = None,
             env: dict[str, str] | None = None) -> dict:
    """执行一步：argv 数组启动、捕获输出、超时杀进程组；返回结果 dict。"""
    try:
        proc = subprocess.Popen(
            argv, cwd=str(cwd or REPO), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=False,
            start_new_session=(sys.platform == "linux"),
        )
    except FileNotFoundError:
        return {"argv": argv, "exit_code": 127, "timed_out": False,
                "output_tail": f"可执行不存在：{argv[0]}"}
    try:
        out, _ = proc.communicate(timeout=timeout)
        exit_code, timed_out = proc.returncode, False
    except subprocess.TimeoutExpired:
        proc.kill()
        out, _ = proc.communicate()
        exit_code, timed_out = 124, True
    text = (out or b"").decode("utf-8", "replace")
    lines = text.splitlines()
    tail = lines[-25:] if len(lines) > 25 else lines
    return {"argv": argv, "exit_code": exit_code, "timed_out": timed_out,
            "output_tail": "\n".join(tail)}


def _resolve(rel: str) -> Path:
    p = Path(rel)
    return p if p.is_absolute() else REPO / p


def _ensure_inside_repo(rel: str, opt: str) -> Path:
    p = _resolve(rel).resolve(strict=False)
    try:
        return p.relative_to(REPO.resolve())
    except ValueError:
        raise SystemExit(f"deep_ci_driver: --{opt} 必须位于仓库内（{REPO}），实际 {rel}")


def _append_summary(out_file: Path | None, summary: dict) -> None:
    if out_file:
        out_file.parent.mkdir(parents=True, exist_ok=True)
        out_file.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                            encoding="utf-8")


# -------------------------------------------------------------------- 构建链 ----

def _cmake_configure_steps(build_dir: str, cache: list[str],
                           steps: list[dict]) -> None:
    steps.append({"name": "cmake-configure", "timeout": 300,
                  "argv": ["cmake", "-S", ".", "-B", build_dir, *cache]})
    steps.append({"name": "cmake-build", "timeout": 2400,
                  "argv": ["cmake", "--build", build_dir, "-j", "2"]})


def cmd_build(args: argparse.Namespace) -> int:
    """build-gcc-release / build-clang 共用：configure + build。"""
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    cache = ["-DCMAKE_BUILD_TYPE=" + args.build_type]
    if args.compiler == "clang":
        cache += ["-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
    if args.werror:
        # 警告升级仅第三方言区内放宽由 CMakeLists 现有 -w 覆盖；自有代码零容忍
        cache += ["-DCMAKE_CXX_FLAGS=-Werror", "-DCMAKE_C_FLAGS=-Werror"]
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), cache, steps)
    return _run_steps(steps, args.output)


def cmd_qa_sanitize(args: argparse.Namespace) -> int:
    """qa-sanitize / qa-sanitize-tsan：独立 build dir + QA-006 自定义目标。

    目标内部 = clang 重新配置(ASan+UBSan 或 TSan) + 全图构建 + CTest 全量；
    sanitizer 违例 → ctest 非零 → 本驱动非零（合同：sanitizer 失败必须修复）。
    """
    target = "qa-sanitize-tsan" if args.tsan else "qa-sanitize"
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    cache = ["-DCMAKE_BUILD_TYPE=Debug",
             "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
             "-DASTROCS_SANITIZE_THREAD=ON" if args.tsan
             else "-DASTROCS_ENABLE_SANITIZERS=ON"]
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), cache, steps)
    steps.append({"name": "qa-target", "timeout": 2400,
                  "argv": ["cmake", "--build", str(build_dir), "--target", target]})
    return _run_steps(steps, args.output)


def cmd_coverage_cpp(args: argparse.Namespace) -> int:
    """coverage-cpp：ccov 目标（插桩 + ctest + llvm 报告），产物归集 output-dir。"""
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    out_dir = _ensure_inside_repo(args.output_dir, "output-dir")
    cache = ["-DCMAKE_BUILD_TYPE=Debug", "-DASTROCS_BUILD_COVERAGE=ON"]
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), cache, steps)
    steps.append({"name": "ccov-target", "timeout": 3000,
                  "argv": ["cmake", "--build", str(build_dir), "--target", "ccov"]})
    rc = _run_steps(steps, args.output)
    if rc != 0:
        return rc
    cov_src = build_dir / "coverage"
    copied = []
    if cov_src.is_dir():
        out_dir.mkdir(parents=True, exist_ok=True)
        for f in sorted(cov_src.iterdir()):
            if f.is_file() and f.suffix in (".json", ".profdata"):
                (out_dir / f.name).write_bytes(f.read_bytes())
                copied.append(f"{out_dir}/{f.name}")
    print(json.dumps({"driver": "deep_ci_driver.py", "subcommand": "coverage-cpp",
                      "copied_outputs": copied}, ensure_ascii=False))
    return 0


def _run_steps(steps: list[dict], output: str | None) -> int:
    summary = {"driver": "deep_ci_driver.py", "generated_utc": utc_iso(),
               "steps": [], "exit_code": 0}
    rc = 0
    for step in steps:
        print(f"[deep_ci_driver] {step['name']}: {' '.join(step['argv'])}", flush=True)
        res = run_step(step["argv"], timeout=step["timeout"])
        res["name"] = step["name"]
        summary["steps"].append(res)
        print(res["output_tail"], flush=True)
        if res["exit_code"] != 0:
            rc = res["exit_code"]
            summary["exit_code"] = rc
            summary["failed_step"] = step["name"]
            break
    _append_summary(_resolve(output) if output else None, summary)
    return rc


# ----------------------------------------------------------------------- CLI ----

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(prog="deep_ci_driver.py",
                                 description="linux-deep hosted 检查驱动（V8-CI-005）")
    sub = ap.add_subparsers(dest="subcommand", required=True)

    b = sub.add_parser("build-gcc-release", help="Release GCC configure+build")
    b.add_argument("--build-dir", default="run/ci/build-gcc-release")
    b.add_argument("--build-type", default="Release")
    b.add_argument("--compiler", default="gcc", choices=["gcc", "clang"])
    b.add_argument("--werror", action="store_true")
    b.add_argument("--output", default=None, help="步骤摘要 JSON 输出路径")
    b.set_defaults(func=cmd_build)

    c = sub.add_parser("build-clang", help="Clang configure+build")
    c.add_argument("--build-dir", default="run/ci/build-clang")
    c.add_argument("--build-type", default="Debug")
    c.add_argument("--compiler", default="clang", choices=["gcc", "clang"])
    c.add_argument("--werror", action="store_true")
    c.add_argument("--output", default=None)
    c.set_defaults(func=cmd_build)

    for name, tsan in (("qa-sanitize", False), ("qa-sanitize-tsan", True)):
        q = sub.add_parser(name, help=("clang TSan 全量" if tsan
                                       else "clang ASan+UBSan 全量"))
        q.add_argument("--build-dir",
                       default="run/ci/build-qa-tsan" if tsan
                       else "run/ci/build-qa-asan")
        q.add_argument("--tsan", action="store_true", default=tsan)
        q.add_argument("--output", default=None)
        q.set_defaults(func=cmd_qa_sanitize)

    v = sub.add_parser("coverage-cpp", help="LLVM source-based 覆盖（ccov 目标）")
    v.add_argument("--build-dir", default="run/ci/build-cov")
    v.add_argument("--output-dir", default="run/ci/coverage-cpp")
    v.add_argument("--output", default=None)
    v.set_defaults(func=cmd_coverage_cpp)

    return ap


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
