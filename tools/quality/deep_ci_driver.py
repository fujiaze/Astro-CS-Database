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
                      --output-dir 供 run.py outputs 校验；V8-CI-012 修复轮 2：
                      ctest 失败仍执行 merge/report 与产物归集——failing tests
                      不阻止覆盖率测量，驱动退出码仍取首个失败步骤）
  ctest-full        : 全量 CTest 门（CTEST-LINUX-FULL，CI-REG-002）：configure +
                      全图 build + ctest 全量；承接 STD-F7 处置 2「linux-main 必须
                      包含全量 ctest」。ctest 串行执行（-j 不传）：p1wcs_performance /
                      p1noise_performance 等带时序哨兵，并行会引入与代码无关的抖动。
  ctest-target      : 单 CTest 目标门（CTEST-<TARGET>，CI-REG-002）：在对内已构建的
                      build dir 上跑 ctest -R ^<target>$；承接 STD-F7 处置 1「本轮新增
                      测试逐个注册为显式检查项」。前置构建由登记顺序保证
                      （BUILD-GCC-RELEASE / CTEST-LINUX-FULL 先行）。

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
    # QA-006 接线（V8-CI-012 F-D1 修复）：外层 build 只需让自定义目标存在，
    # 即传 -DASTROCS_SANITIZE{RUNTIME,THREAD}=ON（与 CMakeLists.txt L511-561
    # 的 option 名对齐）；ASan+UBSan 共存由目标内部重配置时传
    # -DASTROCS_ENABLE_SANITIZERS=ON（复用 QA-002 接线，无独立 UNDEFINED 变量）。
    # 旧行为误传 ENABLE_SANITIZERS → 目标从未定义 → "No rule to make target
    # 'qa-sanitize'"（exit 2）；TSan 分支传 SANITIZE_THREAD 为正确同构参照。
    cache = ["-DCMAKE_BUILD_TYPE=Debug",
             "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++",
             "-DASTROCS_SANITIZE_THREAD=ON" if args.tsan
             else "-DASTROCS_SANITIZE_RUNTIME=ON"]
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), cache, steps)
    steps.append({"name": "qa-target", "timeout": 2400,
                  "argv": ["cmake", "--build", str(build_dir), "--target", target]})
    return _run_steps(steps, args.output)


def cmd_coverage_cpp(args: argparse.Namespace) -> int:
    """coverage-cpp：ccov 目标（插桩 + ctest + llvm 报告），产物归集 output-dir。

    V8-CI-012 修复轮 2（COV-CPP）：ccov 目标内部 ctest 失败时（本仓库
    core_pipeline / cpu001_selftest_avx512 既有失败，Error 8）不再跳过
    coverage 报告与产物归集——failing tests 不应阻止覆盖率测量（业界
    标准语义）。实现：ccov-target 步失败后继续执行独立的 merge/report
    步（直接调 cmake/qa_coverage_report.sh，与 ccov 目标第 4 步同参），
    归集 build/coverage/*.profraw + coverage.json 到 --output-dir；
    最终退出码仍取首个失败步骤退出码（verdict 仍 FAIL，覆盖数值照常
    产出，12_FAILURE_POLICY 归属不变）。merge/report 步自身失败时以
    其退出码替代（报错误导出覆盖数据不可得，需如实上报）。
    """
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    out_dir = _ensure_inside_repo(args.output_dir, "output-dir")
    # V8-CI-012 修复轮 3（F8）：coverage 构建强制 clang 工具链（与上方
    # sanitizer 分支同构）。旧行为只传 -fprofile-instr-generate/-fcoverage-
    # mapping（CMakeLists ASTROCS_BUILD_COVERAGE 分支），不指定编译器 →
    # hosted 默认 GNU cc → clang 专属插桩旗标无插桩语义 → profraw 零产出
    # → qa_coverage_report.sh 合并链断 → C++ 覆盖率数值缺位（轮 3 hosted
    # 实证 C compiler identification is GNU 13.3.0）。
    cache = ["-DCMAKE_BUILD_TYPE=Debug", "-DASTROCS_BUILD_COVERAGE=ON",
             "-DCMAKE_C_COMPILER=clang", "-DCMAKE_CXX_COMPILER=clang++"]
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), cache, steps)
    steps.append({"name": "ccov-target", "timeout": 3000,
                  "argv": ["cmake", "--build", str(build_dir), "--target", "ccov"]})
    steps.append({"name": "coverage-merge-report", "timeout": 300,
                  "argv": ["/bin/sh", str(REPO / "cmake" / "qa_coverage_report.sh"),
                           str(build_dir), str(REPO)]})
    rc = _run_steps(steps, args.output, stop_on_failure=False)
    cov_src = build_dir / "coverage"
    copied = []
    if cov_src.is_dir():
        out_dir.mkdir(parents=True, exist_ok=True)
        for f in sorted(cov_src.iterdir()):
            if f.is_file() and f.suffix in (".json", ".profdata", ".profraw"):
                (out_dir / f.name).write_bytes(f.read_bytes())
                copied.append(f"{out_dir}/{f.name}")
    print(json.dumps({"driver": "deep_ci_driver.py", "subcommand": "coverage-cpp",
                      "copied_outputs": copied,
                      "verdict": "FAIL" if rc != 0 else "PASS"},
                     ensure_ascii=False))
    return rc


# -------------------------------------------------------------- CTest 门（CI-REG-002） ----

def cmd_ctest_full(args: argparse.Namespace) -> int:
    """ctest-full：configure + 全图 build + CTest 全量（CTEST-LINUX-FULL）。

    CI-REG-002 / STD-F7 处置 2：linux-main 必须含不可豁免的全量 ctest 门。
    复用 BUILD-GCC-RELEASE 的 build dir（首个 linux-main 构建检查）；
    已构建时 configure/build 为增量幂等，独立运行时自举完整构建。
    ctest 不传并行旗标：p1wcs_performance / p1noise_performance 等用例带
    时序哨兵，并行执行会引入与被测代码无关的抖动（非确定性门禁）。
    """
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    steps: list[dict] = []
    _cmake_configure_steps(str(build_dir), ["-DCMAKE_BUILD_TYPE=Release"], steps)
    # 内部超时预算 300(configure)+2400(build)+900(ctest) = 3600 = 检查项
    # timeout_seconds，驱动步超时先于 runner 总超时触发（归因清晰）。
    steps.append({"name": "ctest-full", "timeout": 900,
                  "argv": ["ctest", "--output-on-failure"],
                  "cwd": str(REPO / build_dir)})
    rc = _run_steps(steps, args.output)
    print(json.dumps({"driver": "deep_ci_driver.py", "subcommand": "ctest-full",
                      "build_dir": str(build_dir),
                      "verdict": "FAIL" if rc != 0 else "PASS"}, ensure_ascii=False))
    return rc


def cmd_ctest_target(args: argparse.Namespace) -> int:
    """ctest-target：单 CTest 目标门（CTEST-<TARGET>，CI-REG-002）。

    CI-REG-002 / STD-F7 处置 1：本轮新增测试目标逐个成为显式、不可豁免的
    CI 检查项。build dir 由登记顺序保证已构建（BUILD-GCC-RELEASE 或
    CTEST-LINUX-FULL 先行）；未构建时 ctest 报 "No tests were found" 非零，
    按 FAIL 如实传导（不静默绿）。
    """
    build_dir = _ensure_inside_repo(args.build_dir, "build-dir")
    target = args.target
    steps: list[dict] = [{
        "name": "ctest-target",
        "timeout": args.step_timeout,
        "argv": ["ctest", "-R", "^%s$" % target, "--output-on-failure"],
        "cwd": str(REPO / build_dir),
    }]
    rc = _run_steps(steps, args.output)
    print(json.dumps({"driver": "deep_ci_driver.py", "subcommand": "ctest-target",
                      "target": target, "build_dir": str(build_dir),
                      "verdict": "FAIL" if rc != 0 else "PASS"}, ensure_ascii=False))
    return rc


def _run_steps(steps: list[dict], output: str | None,
               stop_on_failure: bool = True) -> int:
    """顺序执行步骤；stop_on_failure=False 时失败后继续（coverage 语义）。

    rc 始终记首个失败步骤退出码；summary["failed_step"] 同名记录。
    """
    summary = {"driver": "deep_ci_driver.py", "generated_utc": utc_iso(),
               "steps": [], "exit_code": 0}
    rc = 0
    for step in steps:
        print(f"[deep_ci_driver] {step['name']}: {' '.join(step['argv'])}", flush=True)
        res = run_step(step["argv"], timeout=step["timeout"],
                       cwd=Path(step["cwd"]) if step.get("cwd") else None)
        res["name"] = step["name"]
        summary["steps"].append(res)
        print(res["output_tail"], flush=True)
        if res["exit_code"] != 0:
            if rc == 0:
                rc = res["exit_code"]
                summary["exit_code"] = rc
                summary["failed_step"] = step["name"]
            if stop_on_failure:
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

    # CI-REG-002（STD-F7 处置 1/2）：linux-main 全量 ctest 门 + 逐目标显式门
    f = sub.add_parser("ctest-full", help="全量 CTest 门（CTEST-LINUX-FULL）")
    f.add_argument("--build-dir", default="run/ci/build-gcc-release")
    f.add_argument("--output", default=None)
    f.set_defaults(func=cmd_ctest_full)

    t = sub.add_parser("ctest-target", help="单 CTest 目标门（CTEST-<TARGET>）")
    t.add_argument("--build-dir", default="run/ci/build-gcc-release")
    t.add_argument("--target", required=True, help="ctest 测试目标名（精确匹配 ^name$）")
    t.add_argument("--step-timeout", type=int, default=600,
                   help="ctest 单步超时（秒）；检查项 timeout_seconds 应大于该值")
    t.add_argument("--output", default=None)
    t.set_defaults(func=cmd_ctest_target)

    return ap


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
