#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/validate_registry.py — CI 检查注册表结构校验器（V8-CI-001）。

用法:
    python3 ci/validate_registry.py --registry ci/checks.json [--strict]

校验（--strict 下任一 FAIL → exit 1；非 strict 时仅结构错误致命）:
  R1  registry 可解析且为对象，含 schema_version==1 与非空 checks 数组；
  R2  每项必需字段齐备且类型正确（id/profiles/platform/command/timeout_seconds/
      heavy/mutates_workspace/outputs/waivable/changed_paths/requires_monitor）；
      可选 prerequisite_tools 必须是非空字符串数组（外部可执行名如 cmake/clang，
       或 CI-REG-002/STD-F10 的 <exe>:<module> Python 模块依赖如 python3:astropy）；
      可选 ctest_targets 必须是非空字符串数组（CI-REG-002 显式登记的 CTest 目标名）；
  R3  id 唯一无重复；
  R4  command[0] ∈ {python3, python} 且 command[1] 指向仓库内存在的文件；
  R5  profiles 值 ⊆ {fast, linux-main, windows-main, linux-deep, fatduck} 且非空；
  R6  command[1] 引用的脚本文件确实存在于仓库（与 R4 互补：非 python 命令时
      检查 command 中出现的仓库相对路径文件存在）；
  R7  heavy=true 的项 requires_monitor=true；
  R8  mutates_workspace=true 的项不得出现在 fast profile；
  R9  命令元素不得硬编码线程/核心数（-j<N>/--jobs/--threads/NPROC/
      OMP_NUM_THREADS/MKL_NUM_THREADS/NUMEXPR_NUM_THREADS）→
      hardcoded_core_in_command；ci/resource_monitor.py 前缀的 monitor
      自身参数（--timeout/--output/--）白名单放行，-- 之后的子命令仍扫描。
  R10 linux-main 选中序末位必须是聚合型 KNOWN-FAILURES-BASELINE-CHECK
      （B3-A5/R-15：它读同 run 全部上游 per-check 结果与 JUnit，排在中间
      就读不全）→ last_linux_main_entry_must_be_check_gate；
  R11 unittest discover 目录采集用例数必须 > 0，且目录内每个"直跑验收脚本"
      （模块级 def main + sys.exit(main())）必须贡献 >=1 个 TestCase 用例
      （M8-F-001: 0 用例入口在 CI 内永不执行却记 PASS）→ discover_case_gap。

输出: stdout 一份 JSON 摘要 {"registry", "checks", "errors", "verdict"}；
      全部通过 exit 0，任一 FAIL exit 1。本脚本只读，不写任何文件。
"""
from __future__ import annotations

import argparse
import ast
import json
import pathlib
import re
import sys

REPO = pathlib.Path(__file__).resolve().parent.parent

ALLOWED_PROFILES = {"fast", "linux-main", "windows-main", "linux-deep", "fatduck"}
ALLOWED_PLATFORM = {"any", "linux", "windows", "fatduck"}
ALLOWED_RUNNERS = {"python3", "python"}
ID_RE = re.compile(r"^[A-Z0-9][A-Z0-9_.-]+$")

BOOL_FIELDS = ("heavy", "mutates_workspace", "waivable", "requires_monitor")
STR_LIST_FIELDS = ("profiles", "command", "outputs", "changed_paths")
# 可选字段：依赖的外部工具名（探测逻辑见 ci/run.py probe_prerequisite）；
# per-check dirty 豁免（消费逻辑见 ci/run.py execute_check，schema 见
# ci/checks.schema.json 同名字段——CI-001 对齐两者预存断链，否则 strict
# 永远对 checks[52]/[53] 报 unexpected fields）。
OPT_STR_LIST_FIELDS = ("prerequisite_tools", "dirty_ignore_exact",
                       "dirty_ignore_prefixes", "ctest_targets")
REQUIRED = ("id", "profiles", "platform", "command", "timeout_seconds",
            "heavy", "mutates_workspace", "outputs", "waivable") + BOOL_FIELDS[:0]
REQUIRED = ("id", "profiles", "platform", "command", "timeout_seconds",
            "heavy", "mutates_workspace", "outputs", "waivable",
            "changed_paths", "requires_monitor")

# R9（V8-CI-009 GAP-G2 补齐）：硬编码线程/核心数模式。命令元素显式携带
# 线程参数或 *_NUM_THREADS/NPROC 赋值即违规——线程选择一律经
# ci/resource_monitor.py（逐内核 benchmark）注入，禁止写死在 check 命令里。
HARDCODED_THREAD_RE = re.compile(
    r"(?:^|[\s=])-j(?:\d|\s|$)"
    r"|(?:^|[\s=])--jobs(?:\d|[=\s]|$)"
    r"|(?:^|[\s=])--threads(?:\d|[=\s]|$)"
    r"|\b(?:NPROC|OMP_NUM_THREADS|MKL_NUM_THREADS|NUMEXPR_NUM_THREADS)\b",
    re.IGNORECASE,
)
MONITOR_SCRIPT = "ci/resource_monitor.py"


def _collect_test_files(root: pathlib.Path, pattern: str) -> list[pathlib.Path]:
    """按 unittest discover 语义收集候选文件: 目录内 + 含 __init__.py 的子包。"""
    files = sorted(p for p in root.glob(pattern) if p.is_file())
    for sub in sorted(root.iterdir()):
        if sub.is_dir() and (sub / "__init__.py").is_file():
            files.extend(_collect_test_files(sub, pattern))
    return files


def _discover_case_gap(where: str, target: pathlib.Path, cmd: list[str]) -> list[str]:
    """R11：unittest discover 目录不得 0 用例, 且直跑验收脚本必须可被采集。

    M8-F-001 根因: tests/abi 的四个验收脚本 (main() + sys.exit(main())) 无
    TestCase 类, discover 只收到 0 用例却记 PASS; 纯"目录用例数>0"无法发现
    (同目录另有 abi002 的 18 例)。故两条判据并用:
      1) 目录采集用例数 == 0 → 门空转;
      2) 每个"直跑验收脚本"(模块级 def main + sys.exit(main())) 必须贡献
         >=1 个 TestCase test_* 方法 —— 否则该脚本在 CI 内永不执行。
    静态 AST 解析, 无副作用(不 import 被测模块)。
    """
    pattern = "test*.py"
    if "-p" in cmd:
        pattern = cmd[cmd.index("-p") + 1]
    total = 0
    problems: list[str] = []
    for f in _collect_test_files(target, pattern):
        rel = f.relative_to(REPO)
        try:
            tree = ast.parse(f.read_text(encoding="utf-8", errors="ignore"))
        except SyntaxError as exc:
            problems.append(f"R11 {where}: {rel} 解析失败: {exc}")
            continue
        has_main = any(
            isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name == "main"
            for n in tree.body)
        direct_run = any(
            isinstance(n, ast.Call) and isinstance(n.func, ast.Attribute)
            and n.func.attr == "exit" and n.args
            and isinstance(n.args[0], ast.Call)
            and isinstance(n.args[0].func, ast.Name)
            and n.args[0].func.id == "main"
            for n in ast.walk(tree))
        cases = 0
        for node in ast.walk(tree):
            if not isinstance(node, ast.ClassDef):
                continue
            bases = [ast.unparse(b) for b in node.bases]
            if any("TestCase" in b for b in bases):
                cases += sum(
                    1 for x in node.body
                    if isinstance(x, (ast.FunctionDef, ast.AsyncFunctionDef))
                    and x.name.startswith("test"))
        total += cases
        if direct_run and has_main and cases == 0:
            problems.append(
                f"R11 {where}: {rel} 是直跑验收脚本(main+sys.exit)但 discover "
                f"采集 0 用例 —— 该门在 CI 内永不执行")
    if total == 0:
        problems.append(
            f"R11 {where}: discover 目录 {target.relative_to(REPO)} 采集 0 用例(门空转)")
    return problems


def validate(registry_path: pathlib.Path, strict: bool) -> tuple[list[str], int]:
    errors: list[str] = []

    # R1 可解析 + 顶层结构
    try:
        data = json.loads(registry_path.read_text(encoding="utf-8"))
    except Exception as exc:
        return [f"R1 registry-unreadable: {exc}"], 0
    if not isinstance(data, dict):
        return [f"R1 top-level must be object, got {type(data).__name__}"], 0
    if data.get("schema_version") != 1:
        errors.append(f"R1 schema_version must be 1, got {data.get('schema_version')!r}")
    checks = data.get("checks")
    if not isinstance(checks, list) or not checks:
        errors.append("R1 checks must be a non-empty array")
        return errors, 0

    seen: dict[str, int] = {}
    for i, c in enumerate(checks):
        where = f"checks[{i}]"
        if not isinstance(c, dict):
            errors.append(f"R2 {where}: entry must be object")
            continue

        # R2 必需字段 + 类型
        missing = [f for f in REQUIRED if f not in c]
        if missing:
            errors.append(f"R2 {where}: missing fields {missing}")
            continue
        extra = [f for f in c if f not in REQUIRED + OPT_STR_LIST_FIELDS]
        if extra:
            errors.append(f"R2 {where}: unexpected fields {extra}")
        for f in OPT_STR_LIST_FIELDS:
            if f in c and (not isinstance(c[f], list) or not c[f]
                           or not all(isinstance(x, str) and x for x in c[f])):
                errors.append(f"R2 {where}.{f}: must be non-empty array of non-empty strings")
        cid = c["id"]
        if not isinstance(cid, str) or not ID_RE.match(cid):
            errors.append(f"R2 {where}: bad id {cid!r} (must match {ID_RE.pattern})")
        for f in STR_LIST_FIELDS:
            if not isinstance(c[f], list) or not all(isinstance(x, str) for x in c[f]):
                errors.append(f"R2 {where}.{f}: must be array of strings")
        if not isinstance(c["platform"], str) or c["platform"] not in ALLOWED_PLATFORM:
            errors.append(f"R2 {where}.platform: must be one of {sorted(ALLOWED_PLATFORM)}")
        if not isinstance(c["timeout_seconds"], int) or isinstance(c["timeout_seconds"], bool) \
                or c["timeout_seconds"] < 1:
            errors.append(f"R2 {where}.timeout_seconds: must be integer >= 1")
        for f in BOOL_FIELDS:
            if not isinstance(c[f], bool):
                errors.append(f"R2 {where}.{f}: must be boolean")

        # R3 id 唯一
        if isinstance(cid, str):
            if cid in seen:
                errors.append(f"R3 duplicate id {cid!r} (first at checks[{seen[cid]}])")
            else:
                seen[cid] = i

        # R5 profiles
        profs = c.get("profiles")
        if isinstance(profs, list):
            if not profs:
                errors.append(f"R5 {where}.profiles: empty")
            bad = [p for p in profs if p not in ALLOWED_PROFILES]
            if bad:
                errors.append(f"R5 {where}.profiles: unknown values {bad}")

        # R4/R6 command
        cmd = c.get("command")
        if isinstance(cmd, list) and cmd and all(isinstance(x, str) for x in cmd):
            if cmd[0] not in ALLOWED_RUNNERS:
                errors.append(f"R4 {where}.command[0]: {cmd[0]!r} not in {sorted(ALLOWED_RUNNERS)}")
            else:
                if len(cmd) < 2:
                    errors.append(f"R4 {where}.command: needs script path as command[1]")
                elif cmd[1].startswith("-"):
                    # 形如 "python3 -B -m unittest discover -s <dir> -t <dir>"：
                    # command[1] 为解释器旗标，转而校验 -s 指向的仓库内目录存在
                    if "-s" in cmd:
                        target = REPO / cmd[cmd.index("-s") + 1]
                        if not target.is_dir():
                            errors.append(f"R4 {where}.command -s: dir not found: {cmd[cmd.index('-s') + 1]}")
                        else:
                            # R11（M8-F-001）: discover 采集用例数 > 0 且直跑
                            # 验收脚本必须可被采集。
                            errors.extend(_discover_case_gap(where, target, cmd))
                    else:
                        errors.append(f"R4 {where}.command[1]: flag {cmd[1]!r} without resolvable target")
                else:
                    script = REPO / cmd[1]
                    if not script.is_file():
                        errors.append(f"R4 {where}.command[1]: file not found: {cmd[1]}")
            # R6：已知“输入类”旗标后的仓库相对路径必须存在；
            # 输出类旗标（--output/--out-json/--json-out/--out-junit）指向待生成产物，不校验存在。
            INPUT_FLAGS = {"--csv", "--index", "--schema", "--policy", "--actual",
                           "--repo", "--registry", "--trace", "--ir", "--module-index",
                           "--commits", "--results-dir"}
            if strict:
                for i, tok in enumerate(cmd):
                    if tok in INPUT_FLAGS and i + 1 < len(cmd):
                        arg = cmd[i + 1]
                        if not arg.startswith("-") and "/" in arg and not arg.startswith("/"):
                            if not (REPO / arg).exists():
                                errors.append(f"R6 {where}.command: input path not found: {arg}")

        # R7 heavy → monitor
        if c.get("heavy") is True and c.get("requires_monitor") is not True:
            errors.append(f"R7 {where} ({cid}): heavy=true requires requires_monitor=true")

        # R8 fast 不得写工作区
        if c.get("mutates_workspace") is True and isinstance(profs, list) and "fast" in profs:
            errors.append(f"R8 {where} ({cid}): mutates_workspace=true must not be in fast profile")

        # R9 硬编码线程/核心数（GAP-G2）：ci/resource_monitor.py 与其 `--`
        # 之间的元素是 monitor 自身参数（--timeout/--output/--），白名单放行；
        # `--` 之后为被监视子命令，恢复逐一扫描。非 monitor 命令全元素扫描。
        if isinstance(cmd, list) and all(isinstance(x, str) for x in cmd):
            monitor_ctx = False
            for tok in cmd:
                if tok == MONITOR_SCRIPT:
                    monitor_ctx = True
                elif monitor_ctx and tok == "--":
                    monitor_ctx = False
                elif not monitor_ctx and HARDCODED_THREAD_RE.search(tok):
                    errors.append(
                        f"R9 {where} ({cid}): hardcoded_core_in_command: {tok!r}")

    # R10（B3-A5）：聚合型 known-failures 基线门必须排在 linux-main 选中序末位。
    # 它在运行期读取同 run 全部上游 per-check 结果与 CTEST-LINUX-FULL 的 JUnit；
    # 排在中间会读不全 → 静态强制，杜绝注册表重排造成的静默退化。
    linux_main_ids = [c["id"] for c in checks
                      if isinstance(c, dict) and "linux-main" in c.get("profiles", [])]
    if linux_main_ids and linux_main_ids[-1] != "KNOWN-FAILURES-BASELINE-CHECK":
        errors.append(
            "R10 linux-main: last selected entry must be "
            f"KNOWN-FAILURES-BASELINE-CHECK, got {linux_main_ids[-1]!r}")

    return errors, len(checks)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Validate ci/checks.json registry")
    ap.add_argument("--registry", required=True, help="registry JSON path (repo-relative or absolute)")
    ap.add_argument("--strict", action="store_true", help="fail on any rule violation")
    args = ap.parse_args(argv)

    reg = pathlib.Path(args.registry)
    if not reg.is_absolute():
        reg = (pathlib.Path.cwd() / reg).resolve()
    errors, n = validate(reg, args.strict)
    summary = {
        "registry": str(reg),
        "strict": bool(args.strict),
        "checks": n,
        "errors": errors,
        "error_count": len(errors),
        "verdict": "PASS" if not errors else "FAIL",
    }
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
