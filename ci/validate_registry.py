#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/validate_registry.py — CI 检查注册表结构校验器（V8-CI-001）。

用法:
    python3 ci/validate_registry.py --registry ci/checks.json [--strict]

校验（--strict 下任一 FAIL → exit 1；非 strict 时仅结构错误致命）:
  R1  registry 可解析且为对象，含 schema_version==1 与非空 checks 数组；
  R2  每项必需字段齐备且类型正确（id/profiles/platform/command/timeout_seconds/
      heavy/mutates_workspace/outputs/waivable/changed_paths/requires_monitor）；
  R3  id 唯一无重复；
  R4  command[0] ∈ {python3, python} 且 command[1] 指向仓库内存在的文件；
  R5  profiles 值 ⊆ {fast, linux-main, windows-main, linux-deep, fatduck} 且非空；
  R6  command[1] 引用的脚本文件确实存在于仓库（与 R4 互补：非 python 命令时
      检查 command 中出现的仓库相对路径文件存在）；
  R7  heavy=true 的项 requires_monitor=true；
  R8  mutates_workspace=true 的项不得出现在 fast profile。

输出: stdout 一份 JSON 摘要 {"registry", "checks", "errors", "verdict"}；
      全部通过 exit 0，任一 FAIL exit 1。本脚本只读，不写任何文件。
"""
from __future__ import annotations

import argparse
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
REQUIRED = ("id", "profiles", "platform", "command", "timeout_seconds",
            "heavy", "mutates_workspace", "outputs", "waivable") + BOOL_FIELDS[:0]
REQUIRED = ("id", "profiles", "platform", "command", "timeout_seconds",
            "heavy", "mutates_workspace", "outputs", "waivable",
            "changed_paths", "requires_monitor")


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
        extra = [f for f in c if f not in REQUIRED]
        if extra:
            errors.append(f"R2 {where}: unexpected fields {extra}")
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
