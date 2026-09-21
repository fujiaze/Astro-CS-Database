#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/validate_registry.py — CI 检查注册表结构校验器（V8-CI-001）。

用法:
    python3 eng/ci/validate_registry.py --registry eng/ci/checks.json [--strict]

校验（--strict 下任一 FAIL → exit 1；非 strict 时仅结构错误致命）:
  R1  registry 可解析且为对象，含 schema_version==1 与非空 checks 数组；
  R2  每项必需字段齐备且类型正确（id/profiles/platform/command/timeout_seconds/
      heavy/mutates_workspace/outputs/waivable/changed_paths/requires_monitor）；
      可选 prerequisite_tools 必须是非空字符串数组（外部可执行名如 eng/cmake/clang，
       或 CI-REG-002/STD-F10 的 <exe>:<module> Python 模块依赖如 python3:astropy）；
      可选 ctest_targets 必须是非空字符串数组（CI-REG-002 显式登记的 CTest 目标名）；
  R3  id 唯一无重复；
  R4  command[0] ∈ {python3, python} 且 command[1] 指向仓库内存在的文件；
  R5  profiles 值 ⊆ {fast, integration, linux-main, windows-main, linux-deep,
      prerelease, fatduck} 且非空；
  R6  command[1] 引用的脚本文件确实存在于仓库（与 R4 互补：非 python 命令时
      检查 command 中出现的仓库相对路径文件存在）；
  R7  heavy=true 的项 requires_monitor=true；
  R8  mutates_workspace=true 的项不得出现在 fast profile；
  R9  命令元素不得硬编码线程/核心数（-j<N>/--jobs/--threads/NPROC/
      OMP_NUM_THREADS/MKL_NUM_THREADS/NUMEXPR_NUM_THREADS）→
      hardcoded_core_in_command；eng/ci/resource_monitor.py 前缀的 monitor
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

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

ALLOWED_PROFILES = {"fast", "integration", "linux-main", "windows-main",
                    "linux-deep", "prerelease", "fatduck"}
ALLOWED_PLATFORM = {"any", "linux", "windows", "fatduck"}
ALLOWED_RUNNERS = {"python3", "python"}
ID_RE = re.compile(r"^[A-Z0-9][A-Z0-9_.-]+$")

BOOL_FIELDS = ("heavy", "mutates_workspace", "waivable", "requires_monitor")
STR_LIST_FIELDS = ("profiles", "command", "outputs", "changed_paths")
# 可选字段：依赖的外部工具名（探测逻辑见 eng/ci/run.py probe_prerequisite）；
# per-check dirty 豁免（消费逻辑见 eng/ci/run.py execute_check，schema 见
# eng/ci/checks.schema.json 同名字段——CI-001 对齐两者预存断链，否则 strict
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
# eng/ci/resource_monitor.py（逐内核 benchmark）注入，禁止写死在 check 命令里。
HARDCODED_THREAD_RE = re.compile(
    r"(?:^|[\s=])-j(?:\d|\s|$)"
    r"|(?:^|[\s=])--jobs(?:\d|[=\s]|$)"
    r"|(?:^|[\s=])--threads(?:\d|[=\s]|$)"
    r"|\b(?:NPROC|OMP_NUM_THREADS|MKL_NUM_THREADS|NUMEXPR_NUM_THREADS)\b",
    re.IGNORECASE,
)
MONITOR_SCRIPT = "eng/ci/resource_monitor.py"


def _collect_test_files(root: pathlib.Path, pattern: str) -> list[pathlib.Path]:
    """按 unittest discover 语义收集候选文件: 目录内 + 含 __init__.py 的子包。"""
    files = sorted(p for p in root.glob(pattern) if p.is_file())
    for sub in sorted(root.iterdir()):
        if sub.is_dir() and (sub / "__init__.py").is_file():
            files.extend(_collect_test_files(sub, pattern))
    return files


def _discover_case_gap(where: str, target: pathlib.Path, cmd: list[str]) -> list[str]:
    """R11：unittest discover 目录不得 0 用例, 且直跑验收脚本必须可被采集。

    M8-F-001 根因: eng/tests/abi 的四个验收脚本 (main() + sys.exit(main())) 无
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


# CI-001 ID 收敛：聚合项 steps 的必需/可选字段（step 是自描述最小检查单元）
STEP_REQUIRED = ("id", "command", "timeout_seconds", "profiles", "platform")
STEP_OPT_FIELDS = ("heavy", "mutates_workspace", "outputs", "waivable", "changed_paths",
                   "requires_monitor", "prerequisite_tools", "ctest_targets",
                   "reads_run_results",
                   "dirty_ignore_exact", "dirty_ignore_prefixes")


def _cmd_errors(where: str, cid: str, cmd, strict: bool) -> list[str]:
    """R4/R6：命令可执行体与输入路径规则（注册项与 step 的命令用同一判据）。"""
    errors: list[str] = []
    if not (isinstance(cmd, list) and cmd and all(isinstance(x, str) for x in cmd)):
        return errors
    if cmd[0] not in ALLOWED_RUNNERS:
        errors.append(f"R4 {where}: command[0]: {cmd[0]!r} not in {sorted(ALLOWED_RUNNERS)}")
    else:
        if len(cmd) < 2:
            errors.append(f"R4 {where}: command needs script path as command[1]")
        elif cmd[1].startswith("-"):
            if "-s" in cmd:
                target = REPO / cmd[cmd.index("-s") + 1]
                if not target.is_dir():
                    errors.append(f"R4 {where}.command -s: dir not found: {cmd[cmd.index('-s') + 1]}")
                else:
                    errors.extend(_discover_case_gap(where, target, cmd))
            else:
                errors.append(f"R4 {where}: command[1]: flag {cmd[1]!r} without resolvable target")
        else:
            script = REPO / cmd[1]
            if not script.is_file():
                errors.append(f"R4 {where}: command[1]: file not found: {cmd[1]}")
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
    return errors


def _step_errors(where: str, step, strict: bool) -> list[str]:
    """R2（CI-001 收敛）：step 结构 + 与注册项同判据的命令规则。"""
    errors: list[str] = []
    if not isinstance(step, dict):
        return [f"R2 {where}: step must be object"]
    missing = [f for f in STEP_REQUIRED if f not in step]
    if missing:
        return [f"R2 {where}: step missing fields {missing}"]
    extra = [f for f in step if f not in STEP_REQUIRED + STEP_OPT_FIELDS]
    if extra:
        errors.append(f"R2 {where}: step unexpected fields {extra}")
    sid = step.get("id")
    if not isinstance(sid, str) or not ID_RE.match(sid):
        errors.append(f"R2 {where}: step bad id {sid!r}")
    for f in ("profiles", "command", "outputs", "changed_paths"):
        if f in step and (not isinstance(step[f], list)
                          or not all(isinstance(x, str) for x in step[f])):
            errors.append(f"R2 {where}.{f}: must be array of strings")
    if not step.get("profiles"):
        errors.append(f"R2 {where}.profiles: empty")
    bad = [p for p in step.get("profiles", []) if p not in ALLOWED_PROFILES]
    if bad:
        errors.append(f"R2 {where}.profiles: unknown values {bad}")
    if step.get("platform") not in ALLOWED_PLATFORM:
        errors.append(f"R2 {where}.platform: must be one of {sorted(ALLOWED_PLATFORM)}")
    to = step.get("timeout_seconds")
    if not isinstance(to, int) or isinstance(to, bool) or to < 1:
        errors.append(f"R2 {where}.timeout_seconds: must be integer >= 1（禁止无超时执行）")
    for f in BOOL_FIELDS:
        if f in step and not isinstance(step[f], bool):
            errors.append(f"R2 {where}.{f}: must be boolean")
    errors.extend(_cmd_errors(f"{where}.steps[{sid}]", sid, step.get("command"), strict))
    return errors


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
        extra = [f for f in c if f not in REQUIRED + OPT_STR_LIST_FIELDS + ("steps",)]
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

        # R4/R6 command（CI-001 ID 收敛后：注册项与其每个 step 的命令同一判据）
        cmd = c.get("command")
        errors.extend(_cmd_errors(where, cid, cmd, strict))

        # R12（CI-001）：聚合项 steps 结构 + 派发入口
        if "steps" in c:
            steps = c["steps"]
            if not isinstance(steps, list) or not steps:
                errors.append(f"R2 {where}.steps: must be a non-empty array")
            else:
                seen_steps: set[str] = set()
                for j, step in enumerate(steps):
                    errors.extend(_step_errors(f"{where}.steps[{j}]", step, strict))
                    sid = step.get("id") if isinstance(step, dict) else None
                    if isinstance(sid, str):
                        if sid in seen_steps:
                            errors.append(f"R3 {where}.steps: duplicate step id {sid!r}")
                        seen_steps.add(sid)
                if not any(tok == "eng/ci/run_checks.py" for tok in (cmd or [])):
                    errors.append(
                        f"R12 {where} ({cid}): 聚合项 command 必须经 eng/ci/run_checks.py 派发 steps"
                        "（否则旧入口只跑第一步 → 静默丢覆盖）")

        # R7/R8（CI-001）：按**执行单元（unit）**判定——无 steps 的注册项即 unit，
        # 有 steps 的逐 step 展开。聚合项父级的 heavy/mutates_workspace 是各 step 的
        # 并集投影，用它判 R7/R8 会把「个别 step 的 heavy/写入面」错误外推到整组。
        unit_specs = []
        if isinstance(c.get("steps"), list) and c["steps"]:
            for s in c["steps"]:
                if isinstance(s, dict):
                    unit_specs.append((f"{where}.steps[{s.get('id')}]", s))
        else:
            unit_specs.append((where, c))
        for u_where, u in unit_specs:
            u_id = u.get("id", cid)
            # R7 heavy → monitor
            if u.get("heavy") is True and u.get("requires_monitor") is not True:
                errors.append(f"R7 {u_where} ({u_id}): heavy=true requires requires_monitor=true")
            # R8 fast 不得写工作区
            if u.get("mutates_workspace") is True and "fast" in (u.get("profiles") or []):
                errors.append(f"R8 {u_where} ({u_id}): mutates_workspace=true must not be in fast profile")

        # R9 硬编码线程/核心数（GAP-G2）：eng/ci/resource_monitor.py 与其 `--`
        # 之间的元素是 monitor 自身参数（--timeout/--output/--），白名单放行；
        # `--` 之后为被监视子命令，恢复逐一扫描。非 monitor 命令全元素扫描。
        # CI-001：注册项命令与其每个 step 命令同判据。
        all_cmds = [cmd]
        if isinstance(c.get("steps"), list):
            all_cmds += [s.get("command") for s in c["steps"] if isinstance(s, dict)]
        for one_cmd in all_cmds:
            if not (isinstance(one_cmd, list) and all(isinstance(x, str) for x in one_cmd)):
                continue
            monitor_ctx = False
            for tok in one_cmd:
                if tok == MONITOR_SCRIPT:
                    monitor_ctx = True
                elif monitor_ctx and tok == "--":
                    monitor_ctx = False
                elif not monitor_ctx and HARDCODED_THREAD_RE.search(tok):
                    errors.append(
                        f"R9 {where} ({cid}): hardcoded_core_in_command: {tok!r}")

    # R10（B3-A5 + TEST-GREEN-001）：聚合型 known-failures 基线门必须排在
    # linux-main 选中序末位。它在运行期读取同 run 全部上游 per-check 结果与
    # CTEST-LINUX-FULL 的 JUnit；排在中间会读不全 → 静态强制，杜绝注册表重排
    # 造成的静默退化。CI-001 收敛后判据落在**执行单元（unit）**层：无 steps 的
    # 注册项即 unit；有 steps 的逐 step 展开为 unit（旧 ID 原样），因此末位
    # 判据与收敛前语义等价且更强（step 次序也被纳入）。
    units: list[tuple[str, list[str]]] = []
    for c in checks:
        if not isinstance(c, dict):
            continue
        steps = c.get("steps")
        if isinstance(steps, list) and steps:
            for s in steps:
                if isinstance(s, dict):
                    units.append((s.get("id", c["id"]), s.get("profiles", []) or []))
        else:
            units.append((c["id"], c.get("profiles", []) or []))
    linux_main_units = [uid for uid, profs in units if "linux-main" in profs]
    if linux_main_units and linux_main_units[-1] != "KNOWN-FAILURES-BASELINE-CHECK":
        errors.append(
            "R10 linux-main: last selected unit must be "
            f"KNOWN-FAILURES-BASELINE-CHECK, got {linux_main_units[-1]!r}")

    # R13/R14/R15（CI-001）：ID 治理、迁移映射覆盖、豁免登记。
    # 仅对仓库唯一注册表（eng/ci/checks.json）生效；fixture 注册表（单测注入）不适用。
    if registry_path.resolve() == (REPO / "eng" / "ci" / "checks.json").resolve():
        map_path = REPO / "eng" / "ci" / "id_migration_map.json"
        if not map_path.is_file():
            errors.append("R13 缺 eng/ci/id_migration_map.json（ID 收敛迁移映射）")
        else:
            try:
                mmap = json.loads(map_path.read_text(encoding="utf-8"))
            except Exception as exc:  # noqa: BLE001
                errors.append(f"R13 eng/ci/id_migration_map.json 不可解析：{exc}")
                mmap = None
            if isinstance(mmap, dict):
                declared = {m.get("old_id"): m for m in mmap.get("mappings", [])
                            if isinstance(m, dict)}
                entry_ids = {c["id"] for c in checks if isinstance(c, dict)}
                unit_ids: list[str] = []
                for c in checks:
                    steps = c.get("steps")
                    if isinstance(steps, list) and steps:
                        unit_ids += [s["id"] for s in steps if isinstance(s, dict)]
                    else:
                        unit_ids.append(c["id"])
                # 覆盖率：源注册表逐条登记
                cov = mmap.get("coverage", {})
                absorbed = set(cov.get("absorbed_entries", []) or [])
                if cov.get("source_entry_count") != len(declared) - len(absorbed):
                    errors.append(
                        f"R13 迁移映射覆盖不足：source_entry_count={cov.get('source_entry_count')} "
                        f"mappings={len(declared)} absorbed={len(absorbed)}")
                if cov.get("silent_drops") not in (0, None):
                    errors.append(f"R13 迁移映射声明了 silent_drops={cov.get('silent_drops')}")
                # 无孤儿：注册表 unit 必须都在映射里
                orphan_units = sorted(set(unit_ids) - set(declared))
                if orphan_units:
                    errors.append(f"R13 孤儿 unit（不在迁移映射）：{orphan_units}")
                # 目标一致：MERGED-INTO/KEPT 的 target 必须等于该 unit 的父项 ID
                parent_of: dict[str, str] = {}
                for c in checks:
                    steps = c.get("steps")
                    if isinstance(steps, list) and steps:
                        for s in steps:
                            if isinstance(s, dict):
                                parent_of[s["id"]] = c["id"]
                    else:
                        parent_of[c["id"]] = c["id"]
                for uid in unit_ids:
                    m = declared.get(uid)
                    if not m:
                        continue
                    if m.get("decision") in ("MERGED-INTO", "KEPT") and m.get("target") != parent_of.get(uid):
                        errors.append(
                            f"R13 {uid}: 映射 target={m.get('target')!r} 与注册表父项 "
                            f"{parent_of.get(uid)!r} 不一致")
                # 目标 ID 空间：注册项 ID ∈ 目标 ∪ 扩展 ∪ RETIRE-PENDING
                allowed_targets = set(mmap.get("targets", {})) | set(mmap.get("reserved_targets", {}))
                pending = {m["old_id"] for m in declared.values()
                           if str(m.get("decision", "")).startswith(("RETIRE-PENDING",
                                                                     "KEPT-PENDING"))}
                unknown_entries = sorted(entry_ids - allowed_targets - pending)
                if unknown_entries:
                    errors.append(f"R13 未登记的注册项 ID（既非目标也非 RETIRE-PENDING）：{unknown_entries}")
        # R14：豁免登记（docs/ci/01_CHECKS.md §1；只减不增）
        exemptions = REPO / "eng" / "ci" / "exemptions.json"
        if not exemptions.is_file():
            errors.append("R14 缺 eng/ci/exemptions.json（docs/ci/01_CHECKS.md §1 要求豁免显式登记）")
        else:
            try:
                ex = json.loads(exemptions.read_text(encoding="utf-8"))
            except Exception as exc:  # noqa: BLE001
                errors.append(f"R14 eng/ci/exemptions.json 不可解析：{exc}")
                ex = None
            if isinstance(ex, dict):
                entries = ex.get("exemptions")
                if not isinstance(entries, list):
                    errors.append("R14 exemptions 必须是数组")
                else:
                    high = ex.get("high_water", {})
                    max_entries = high.get("max_entries") if isinstance(high, dict) else None
                    if not isinstance(max_entries, int) or isinstance(max_entries, bool):
                        errors.append("R14 缺 high_water.max_entries（只减不增的高水位）")
                    elif len(entries) > max_entries:
                        errors.append(
                            f"R14 豁免条目 {len(entries)} 超过高水位 {max_entries}（只减不增）")
                    for i, item in enumerate(entries):
                        if not isinstance(item, dict):
                            errors.append(f"R14 exemptions[{i}] 必须是对象")
                            continue
                        for field in ("id", "reason", "approved_by", "approved_utc",
                                      "expiry", "evidence"):
                            if not item.get(field):
                                errors.append(f"R14 exemptions[{i}] 缺 {field}")
                        if item.get("approved_by") != "负责人":
                            errors.append(f"R14 exemptions[{i}].approved_by 必须是「负责人」")
                        if item.get("id") not in entry_ids and item.get("id") not in unit_ids:
                            errors.append(f"R14 exemptions[{i}].id 不在注册表：{item.get('id')!r}")

    return errors, len(checks)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Validate eng/ci/checks.json registry")
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
