#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""V81-ADOPT-002 工作区接管验收脚本（只读，无写盘副作用）。

校验内容：
  1. 证据目录下 PREEXISTING_CHANGES.md 与 REMOTE_RELATION.json 存在；
  2. REMOTE_RELATION.json schema 关键字段齐备且取值一致；
  3. REMOTE_RELATION.json 的 ahead/behind 与实际 `git rev-list --left-right --count
     HEAD...origin/main` 一致；
  4. HEAD == main == origin/main。

用法：
  python3 ci/verify_workspace_adoption.py --repo-root-from-git --preserve-dirty

- --repo-root-from-git：用 `git rev-parse --show-toplevel` 发现仓库根（cwd 须在仓库内）；
- --preserve-dirty：允许工作区存在未提交修改（本任务冻结预存状态，不要求 clean）。

退出码：0 = PASS；非 0 = FAIL。
本脚本不写入、不删除、不移动任何文件，不执行任何变更型 git 子命令。
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import List, Optional, Tuple

EVIDENCE_REL = Path("evidence") / "v8_1_ci_control" / "adoption"
PREEXISTING_MD = "PREEXISTING_CHANGES.md"
REMOTE_JSON = "REMOTE_RELATION.json"

# REMOTE_RELATION.json 必须存在的关键字段（字符串型）
REQUIRED_STR_FIELDS = (
    "task_id",
    "owner",
    "mode",
    "base_sha",
    "head_sha",
    "local_main_sha",
    "origin_main_sha",
    "current_branch",
    "remote_name",
    "remote_url_sanitized",
    "relation",
)
# 必须存在的数值字段
REQUIRED_INT_FIELDS = ("ahead", "behind", "fetch_exit_code")


class CheckFailure(Exception):
    """单个检查失败。"""


def run_git(args: List[str], cwd: Path, timeout: float = 60.0) -> Tuple[int, str]:
    """运行只读 git 命令，返回 (exit_code, stdout)。失败抛 CheckFailure。"""
    try:
        proc = subprocess.run(
            ["git", *args],
            cwd=str(cwd),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise CheckFailure(f"git {' '.join(args)} 执行异常: {exc}") from exc
    return proc.returncode, proc.stdout.strip()


def require_git_ok(args: List[str], cwd: Path, timeout: float = 60.0) -> str:
    code, out = run_git(args, cwd, timeout)
    if code != 0:
        raise CheckFailure(f"git {' '.join(args)} 退出码 {code}（预期 0）")
    return out


def parse_args(argv: Optional[List[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="V81-ADOPT-002 工作区接管验收（只读）"
    )
    parser.add_argument(
        "--repo-root-from-git",
        action="store_true",
        required=True,
        help="用 git rev-parse --show-toplevel 从 cwd 发现仓库根（必须给出）",
    )
    parser.add_argument(
        "--preserve-dirty",
        action="store_true",
        required=True,
        help="允许工作区存在未提交修改（预存状态冻结模式，必须给出）",
    )
    return parser.parse_args(argv)


def main(argv: Optional[List[str]] = None) -> int:
    args = parse_args(argv)
    failures: List[str] = []

    # -- 仓库根发现 --------------------------------------------------------
    try:
        root_out = require_git_ok(["rev-parse", "--show-toplevel"], Path.cwd())
        repo_root = Path(root_out).resolve()
        if not repo_root.is_dir():
            raise CheckFailure(f"仓库根不是目录: {repo_root}")
    except CheckFailure as exc:
        print(f"[FAIL] 仓库根发现: {exc}")
        return 1
    print(f"[PASS] 仓库根: {repo_root}")

    # -- 证据文件存在性 ----------------------------------------------------
    evidence_dir = repo_root / EVIDENCE_REL
    preexisting_md = evidence_dir / PREEXISTING_MD
    remote_json = evidence_dir / REMOTE_JSON
    for path, name in ((preexisting_md, PREEXISTING_MD), (remote_json, REMOTE_JSON)):
        if not path.is_file() or path.stat().st_size == 0:
            failures.append(f"[FAIL] 证据文件缺失或为空: {path.relative_to(repo_root)}")
        else:
            print(f"[PASS] 证据文件存在: {path.relative_to(repo_root)}")

    # -- PREEXISTING_CHANGES.md 内容关键字段 -------------------------------
    if preexisting_md.is_file():
        md_text = preexisting_md.read_text(encoding="utf-8", errors="replace")
        for needle in ("V81-ADOPT-002", "SA-ADOPT-31", "dist/audit/AstroCS_V6_1_AUDIT_20260902T042239Z_faad602da555.zip"):
            if needle in md_text:
                print(f"[PASS] PREEXISTING_CHANGES.md 含关键字段: {needle}")
            else:
                failures.append(
                    f"[FAIL] PREEXISTING_CHANGES.md 缺少关键字段: {needle}"
                )

    # -- REMOTE_RELATION.json schema ---------------------------------------
    remote_data: dict = {}
    if remote_json.is_file():
        try:
            loaded = json.loads(remote_json.read_text(encoding="utf-8"))
            if not isinstance(loaded, dict):
                raise ValueError("顶层不是 JSON object")
            remote_data = loaded
            print(f"[PASS] REMOTE_RELATION.json 可解析为 JSON object")
        except (ValueError, OSError) as exc:
            failures.append(f"[FAIL] REMOTE_RELATION.json 解析失败: {exc}")

        missing_str = [k for k in REQUIRED_STR_FIELDS if not isinstance(remote_data.get(k), str) or not remote_data.get(k)]
        missing_int = [k for k in REQUIRED_INT_FIELDS if not isinstance(remote_data.get(k), int)]
        if missing_str:
            failures.append(f"[FAIL] REMOTE_RELATION.json 缺少/非法字符串字段: {missing_str}")
        else:
            print(f"[PASS] REMOTE_RELATION.json 字符串字段齐备 ({len(REQUIRED_STR_FIELDS)} 项)")
        if missing_int:
            failures.append(f"[FAIL] REMOTE_RELATION.json 缺少/非法整数字段: {missing_int}")
        else:
            print(f"[PASS] REMOTE_RELATION.json 整数字段齐备 ({len(REQUIRED_INT_FIELDS)} 项)")

    # -- 实测 git 状态并与 JSON 对照 ---------------------------------------
    git_ok = True
    head = main_ref = origin_main = ""
    try:
        head = require_git_ok(["rev-parse", "HEAD"], repo_root)
        main_ref = require_git_ok(["rev-parse", "main"], repo_root)
        origin_main = require_git_ok(["rev-parse", "origin/main"], repo_root)
        count_out = require_git_ok(
            ["rev-list", "--left-right", "--count", "HEAD...origin/main"], repo_root
        )
    except CheckFailure as exc:
        failures.append(f"[FAIL] git 只读校验: {exc}")
        git_ok = False

    if git_ok:
        # HEAD == main == origin/main
        if head == main_ref == origin_main:
            print(f"[PASS] HEAD == main == origin/main == {head}")
        else:
            failures.append(
                f"[FAIL] SHA 不一致: HEAD={head} main={main_ref} origin/main={origin_main}"
            )

        # 实测 ahead/behind
        try:
            parts = count_out.split()
            if len(parts) != 2 or not all(p.lstrip("-").isdigit() for p in parts):
                raise ValueError(f"无法解析 rev-list 输出: {count_out!r}")
            actual_ahead, actual_behind = int(parts[0]), int(parts[1])
        except ValueError as exc:
            failures.append(f"[FAIL] ahead/behind 解析: {exc}")
        else:
            print(f"[PASS] 实测 ahead/behind = {actual_ahead}/{actual_behind}")
            if remote_data:
                for key, actual in (("ahead", actual_ahead), ("behind", actual_behind)):
                    recorded = remote_data.get(key)
                    if recorded == actual:
                        print(f"[PASS] REMOTE_RELATION.{key} 与实测一致 ({actual})")
                    else:
                        failures.append(
                            f"[FAIL] REMOTE_RELATION.{key}={recorded!r} 与实测 {actual} 不一致"
                        )

            # JSON 内部一致性
            if remote_data:
                expect = {
                    "head_sha": head,
                    "local_main_sha": main_ref,
                    "origin_main_sha": origin_main,
                }
                mismatch = [
                    f"{k}: json={remote_data.get(k)!r} actual={v!r}"
                    for k, v in expect.items()
                    if remote_data.get(k) != v
                ]
                if mismatch:
                    failures.append("[FAIL] REMOTE_RELATION SHA 字段与实测不一致: " + "; ".join(mismatch))
                else:
                    print("[PASS] REMOTE_RELATION SHA 字段与实测一致")

                expected_relation = "SYNCED" if (actual_ahead == 0 and actual_behind == 0) else None
                if expected_relation is not None:
                    if remote_data.get("relation") == expected_relation:
                        print(f"[PASS] relation = {expected_relation}")
                    else:
                        failures.append(
                            f"[FAIL] relation={remote_data.get('relation')!r} 预期 {expected_relation!r}"
                        )

    # -- 汇总 ---------------------------------------------------------------
    if failures:
        print()
        print(f"RESULT: FAIL（{len(failures)} 项）")
        for line in failures:
            print(line)
        return 1
    print()
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
