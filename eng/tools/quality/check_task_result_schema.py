#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_task_result_schema.py — R0-003 TASK_RESULT 结构一致性检查器（**已退役，非门禁**）。

退役判定依据（先判定再处置；不是"只改退出码了事"）
  1) 注册面已登记退役：docs/ci/01_CHECKS.md §2.1「检查器退役与预留」:142 逐字列出
     「已退役检查器：TASK-RESULT-SCHEMA」，但本文件此前未按 :140–141 的退役契约改造。
  2) 判据对象已随控制包收口消失：唯一默认输入 evidence/v6_1_rework/tasks/*/TASK_RESULT.json
     属 ACTIVITY_STATE 登记的 ARCHIVED_SUPERSEDED 历史线（本文件自己的
     ARCHIVED_EVIDENCE_PREFIXES 就是为此设的），该目录现不存在。
  3) 默认锚算错：原 --schema 默认 = parents[3]/contracts/schemas/ = **<repo>/contracts/**，
     真源是 eng/contracts/schemas/task_result.schema.json ⇒ 恒红（fail-closed，但不具名）。
     本次把默认锚订正到真源（只订正路径，判据不动）。
  4) 复跑性仍在：判据（validate_one 的 15 类结构约束）未删，经 --legacy-check 可对显式
     --schema/--results-dir 复跑。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_main()，经 --legacy-check 复跑（保留可复跑性）；
  * 本检查器不写任何产物文件（原实现也不写）⇒ 不存在"产物落仓库根"的问题。

退出码：0 = --legacy-check 通过；1 = --legacy-check 判据违规（含零命中 fail-closed）；
        2 = 退役（无参调用）/ ANCHOR_STALE（锚失效，fail-closed）；3 = 用法错误。
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import re
import sys
from pathlib import Path

def _deduce_root() -> str:
    """仓库根推导：向上找到同时含 VERSION 与 eng/tools 的目录（可移植）。

    注：本文件位于 eng/tools/quality/（深度 3），固定层数的 dirname 链会把根算成
    <repo>/eng —— 实测会打印 ANCHOR_STALE: SCHEMA_REL <repo>/eng/eng/contracts/...
    这类错锚路径。改为按标记文件上溯，移动文件不再算错根。
    """
    cur = os.path.dirname(os.path.abspath(__file__))
    for _ in range(6):
        if (os.path.isfile(os.path.join(cur, "VERSION"))
                and os.path.isdir(os.path.join(cur, "eng", "tools"))):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            break
        cur = parent
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


REPO = _deduce_root()

# ── 判据锚（docs/ci/01_CHECKS.md §1:14 锚存活；失效即具名 ANCHOR_STALE） ──
SCHEMA_REL = os.path.join("eng", "contracts", "schemas", "task_result.schema.json")
RESULTS_DIR_REL = os.path.join("evidence", "v6_1_rework", "tasks")

SHA40 = re.compile(r"^[0-9a-f]{40}$")
SHA64 = re.compile(r"^[0-9a-f]{64}$")
REQUIRED_TOP = {"schema", "task_id", "status", "parent_commit", "control_sha256",
                "ledger_sha256", "change_class", "changed_paths", "impact",
                "commands", "tests", "evidence", "findings",
                "expected_commit_subject", "created_utc"}
IMPACT_FIELDS = {"science", "algorithm", "data_semantics", "public_api",
                 "pipeline", "binary", "reason"}
CMD_FIELDS = {"command_id", "argv_redacted", "timeout_seconds", "exit_code",
              "started_utc", "duration_seconds", "log_path", "log_sha256"}

# F-CI-002-03 (CI-002, owner 裁决锚 2026-09-11 "归档线证据不受现行 schema 复验"):
# ARCHIVED_SUPERSEDED 历史线(ACTIVITY_STATE 登记: V6.1 REWORK = ARCHIVED_SUPERSEDED,
# 仅作历史参照, 交付面由宪章对齐包接管)的 TASK_RESULT 属旧 schema 时代历史证据,
# 禁止改写证据文件本身; 现行 schema 复验只约束活跃线。命中下列前缀的文件显式
# 跳过(SCHEMA_CHECK_ARCHIVED_SKIP 逐条列出 + archived_skipped 计数, 非静默);
# 归档线集合变更以 ACTIVITY_STATE 登记为准, 修改本清单须 owner 授权。
ARCHIVED_EVIDENCE_PREFIXES = ("evidence/v6_1_rework/",)

RETIREMENT_MARKER = (
    "RETIRED: check_task_result_schema.py 的 CI 检查项 TASK-RESULT-SCHEMA 已退役"
    "（docs/ci/01_CHECKS.md §2.1「已退役检查器」清单）；判据对象 evidence/v6_1_rework/tasks 属"
    "ARCHIVED_SUPERSEDED 历史线且已随控制包收口消失。"
)


def validate_one(doc: dict, path: Path) -> list[str]:
    errs: list[str] = []
    if doc.get("schema") != "astrocs.task-result/v2":
        errs.append("schema != astrocs.task-result/v2")
    if not re.fullmatch(r"^[A-Z0-9]+-[0-9]{3}$", doc.get("task_id", "")):
        errs.append("bad task_id")
    if doc.get("status") not in {"PASS", "FAIL", "WAITING_WINDOWS"}:
        errs.append("bad status")
    for key in REQUIRED_TOP:
        if key not in doc:
            errs.append(f"missing top field: {key}")
    if not SHA40.fullmatch(doc.get("parent_commit", "")):
        errs.append("parent_commit not 40hex")
    if not SHA64.fullmatch(doc.get("control_sha256", "")):
        errs.append("control_sha256 not 64hex")
    if not SHA64.fullmatch(doc.get("ledger_sha256", "")):
        errs.append("ledger_sha256 not 64hex")
    for entry in doc.get("changed_paths", []):
        if entry.startswith("/") or ".." in Path(entry).parts:
            errs.append(f"unsafe changed path: {entry}")
    imp = doc.get("impact", {})
    for key in IMPACT_FIELDS:
        if key not in imp:
            errs.append(f"impact missing: {key}")
    cmds = doc.get("commands", [])
    if not cmds:
        errs.append("commands empty")
    for index, cmd in enumerate(cmds):
        for key in CMD_FIELDS:
            if key not in cmd:
                errs.append(f"commands[{index}] missing: {key}")
        if not SHA64.fullmatch(cmd.get("log_sha256", "")):
            errs.append(f"commands[{index}] log_sha256 not 64hex")
        try:
            datetime.datetime.fromisoformat(cmd.get("started_utc", "").replace("Z", "+00:00"))
        except (ValueError, TypeError):
            errs.append(f"commands[{index}] bad started_utc")
        try:
            float(cmd.get("duration_seconds", "x"))
        except (ValueError, TypeError):
            errs.append(f"commands[{index}] bad duration_seconds")
    ev = doc.get("evidence", [])
    if not ev:
        errs.append("evidence empty")
    for entry in ev:
        if not SHA64.fullmatch(entry.get("sha256", "")):
            errs.append("evidence sha256 not 64hex")
    try:
        datetime.datetime.fromisoformat(doc.get("created_utc", "").replace("Z", "+00:00"))
    except (ValueError, TypeError):
        errs.append("bad created_utc")
    if len(doc.get("expected_commit_subject", "")) < 8:
        errs.append("expected_commit_subject too short")
    return errs


def legacy_main(root: Path, schema: Path, results_dir: Path) -> int:
    """退役前的原判定实现（保留可复跑性；判据未改）。

    fail-closed（§1:13）：schema 或 results-dir 锚缺失 ⇒ 具名 ANCHOR_STALE + rc=2，
    不 traceback、不静默判绿；零命中 ⇒ rc=1。
    """
    if not schema.is_file():
        print(f"ANCHOR_STALE: SCHEMA_REL {schema}", file=sys.stderr)
        return 2
    if not results_dir.is_dir():
        print(f"ANCHOR_STALE: RESULTS_DIR_REL {results_dir}", file=sys.stderr)
        return 2
    try:
        json.loads(schema.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        print(f"SCHEMA_CHECK_FAIL: schema not JSON: {exc}", file=sys.stderr)
        return 2

    failures = 0
    checked = 0
    archived_skipped: list[str] = []
    for result_path in sorted(results_dir.glob("*/TASK_RESULT.json")):
        # F-CI-002-03: 归档线前缀排除(显式呈现, 非静默跳过)。
        try:
            rel_posix = result_path.resolve().relative_to(root).as_posix()
        except ValueError:
            rel_posix = result_path.as_posix()
        if rel_posix.startswith(ARCHIVED_EVIDENCE_PREFIXES):
            print(f"SCHEMA_CHECK_ARCHIVED_SKIP: {rel_posix}")
            archived_skipped.append(rel_posix)
            continue
        checked += 1
        try:
            doc = json.loads(result_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            print(f"SCHEMA_CHECK_FAIL: {result_path} not JSON: {exc}")
            failures += 1
            continue
        errs = validate_one(doc, result_path)
        if errs:
            print(f"SCHEMA_CHECK_FAIL: {result_path}")
            for err in errs[:12]:
                print(f"  - {err}", file=sys.stderr)
            failures += 1
        else:
            print(f"SCHEMA_CHECK_OK: {result_path}")
    if failures:
        print(f"SCHEMA_CHECK_FAIL total_failures={failures} checked={checked} "
              f"archived_skipped={len(archived_skipped)}")
        return 1
    if checked == 0:
        # GAP-027 fail-closed（CI-001）：零命中即「门空转」。
        print(f"SCHEMA_CHECK_FAIL: 零个 TASK_RESULT.json 被校验"
              f"（results-dir={results_dir}，archived_skipped={len(archived_skipped)}）"
              "—— fail-closed 判 FAIL")
        return 1
    print(f"SCHEMA_CHECK_PASS checked={checked} "
          f"archived_skipped={len(archived_skipped)}")
    return 0


# ────────────────────────────────────────────────────────────── self-test
def _valid_doc(task_id: str = "R0-003") -> dict:
    """合成一份结构合法的 TASK_RESULT（判据面字段全集）。"""
    return {
        "schema": "astrocs.task-result/v2",
        "task_id": task_id,
        "status": "PASS",
        "parent_commit": "a" * 40,
        "control_sha256": "b" * 64,
        "ledger_sha256": "c" * 64,
        "change_class": "code",
        "changed_paths": ["eng/tools/quality/check_task_result_schema.py"],
        "impact": {k: "none" for k in IMPACT_FIELDS},
        "commands": [{
            "command_id": "c1", "argv_redacted": "python3 -V", "timeout_seconds": 60,
            "exit_code": 0, "started_utc": "2026-01-01T00:00:00Z",
            "duration_seconds": 0.1, "log_path": "run/x.log", "log_sha256": "d" * 64,
        }],
        "tests": [],
        "evidence": [{"sha256": "e" * 64}],
        "findings": [],
        "expected_commit_subject": "feat: fixture",
        "created_utc": "2026-01-01T00:00:00Z",
    }


def _run_cli(args: list[str]) -> "subprocess.CompletedProcess":
    import subprocess
    return subprocess.run([sys.executable, os.path.abspath(__file__)] + args,
                          capture_output=True, text=True, timeout=300)


def _self_test() -> int:
    """可执行正/负例面：退役契约 + 原判据的能红能绿（§1:10–11）。"""
    import json as _json
    import shutil
    import tempfile

    problems: list[str] = []
    cases = 0

    def check(name: str, rc: int, want_rc: int, blob: str, token: str) -> None:
        nonlocal cases
        cases += 1
        ok = rc == want_rc and token in blob
        print("  SELFTEST_%s %-34s rc=%d want_rc=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing" % (name, rc, want_rc, token))

    # P0 退役契约：无参调用 ⇒ 退役标识 + exit 2
    r = _run_cli([])
    check("P0_retired_noarg", r.returncode, 2, r.stdout + r.stderr, "RETIRED")

    with tempfile.TemporaryDirectory(prefix="trs_st_") as td:
        root = Path(td)
        schema = root / SCHEMA_REL
        schema.parent.mkdir(parents=True, exist_ok=True)
        schema.write_text('{"title": "task_result"}', encoding="utf-8")
        # 活跃线夹具放在**非归档前缀**下（归档前缀面本身有独立用例 N2）。
        tasks = root / "run" / "active_tasks"
        tdir = tasks / "R0-003"
        tdir.mkdir(parents=True)
        good = tdir / "TASK_RESULT.json"

        # P1 正例：结构合法 ⇒ rc=0
        good.write_text(_json.dumps(_valid_doc()), encoding="utf-8")
        r = _run_cli(["--legacy-check", "--root", str(root), "--results-dir", str(tasks)])
        check("P1_legacy_green", r.returncode, 0, r.stdout + r.stderr, "SCHEMA_CHECK_PASS")

        # N1 负例：status 非法 + 坏 sha ⇒ rc=1（原判据仍能红）
        bad = _valid_doc()
        bad["status"] = "MAYBE"
        bad["control_sha256"] = "zz"
        good.write_text(_json.dumps(bad), encoding="utf-8")
        r = _run_cli(["--legacy-check", "--root", str(root), "--results-dir", str(tasks)])
        check("N1_legacy_red", r.returncode, 1, r.stdout + r.stderr, "bad status")

        # N2 归档线前缀 ⇒ 显式 SKIP + 零命中 fail-closed rc=1（不是静默绿）
        good.write_text(_json.dumps(_valid_doc()), encoding="utf-8")
        archived = root / RESULTS_DIR_REL / "R0-003"
        archived.mkdir(parents=True, exist_ok=True)
        (archived / "TASK_RESULT.json").write_text(_json.dumps(_valid_doc()), encoding="utf-8")
        r = _run_cli(["--legacy-check", "--root", str(root)])
        check("N2_archived_zero_hit_failclosed", r.returncode, 1, r.stdout + r.stderr,
              "零个 TASK_RESULT.json 被校验")

        # N3 schema 锚缺失 ⇒ ANCHOR_STALE rc=2
        schema.rename(schema.with_suffix(".bak"))
        r = _run_cli(["--legacy-check", "--root", str(root), "--results-dir", str(tasks)])
        check("N3_schema_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: SCHEMA_REL")
        schema.with_suffix(".bak").rename(schema)

        # N4 results-dir 锚缺失 ⇒ ANCHOR_STALE rc=2
        shutil.rmtree(tasks)
        r = _run_cli(["--legacy-check", "--root", str(root), "--results-dir", str(tasks)])
        check("N4_results_dir_missing_anchor", r.returncode, 2, r.stdout + r.stderr,
              "ANCHOR_STALE: RESULTS_DIR_REL")

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--legacy-check", action="store_true", dest="legacy_check",
                        help="复跑退役前的原判定实现（判据未改）")
    parser.add_argument("--root", type=Path, default=Path(REPO))
    parser.add_argument("--schema", type=Path, default=None,
                        help="schema 路径（默认 <root>/%s）" % SCHEMA_REL)
    parser.add_argument("--results-dir", type=Path, default=None,
                        help="TASK_RESULT 目录（默认 <root>/%s）" % RESULTS_DIR_REL)
    parser.add_argument("--self-test", action="store_true", dest="self_test")
    args = parser.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_check:
        # §2.1:140：退役检查器无参调用时打印退役标识并 exit 2
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-check [--root DIR] [--schema F] [--results-dir DIR]",
              file=sys.stderr)
        return 2
    root = args.root.resolve()
    schema = args.schema or (root / SCHEMA_REL)
    results_dir = args.results_dir or (root / RESULTS_DIR_REL)
    return legacy_main(root, schema, results_dir)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        raise SystemExit(3)
