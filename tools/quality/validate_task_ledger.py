#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""validate_task_ledger.py — R0-003 V6.1 台账状态机与依赖验证器。

状态仅允许：NOT_STARTED / IN_PROGRESS / PASS / FAIL / BLOCKED / WAITING_WINDOWS / WAITING_DATA。

规则（GOV-002 先导）：
1. 任务 ID / gate / 依赖格式合法；ID 唯一；
2. 同一时刻最多一个 IN_PROGRESS；
3. PASS/IN_PROGRESS 任务的依赖必须全部 PASS（唯一例外：Owner 任务 REL-004 保持 NOT_STARTED）；
4. WAITING_WINDOWS 只允许 Windows 平台任务；
5. BLOCKED 必须记录阻塞对象/命令/时间（BLOCKED_REASON 列）；
6. 依赖图中无环、无未知依赖、无缺失任务；
7. REL-004（Owner）不允许 Agent 置 PASS；
8. 依赖属于后置 gate 报错。

输出 machine JSON 到 stdout；`--baseline-sha256` 校验台账 hash 未变（冻结台账）；`--structure-only` 跳过状态规则。

负面 fixture 在 tools/quality/fixtures/ledger/ 下，由 selftest 模式逐一验证必须失败。

用法:
  python3 tools/quality/validate_task_ledger.py LEDGER.csv [--baseline-sha256 HEX] [--structure-only] [--output out.json]
  python3 tools/quality/validate_task_ledger.py --selftest   # 运行全部负例
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from collections import Counter, defaultdict, deque
from pathlib import Path

REQUIRED_COLUMNS = {
    "task_id", "gate", "title", "depends_on", "platform", "change_class",
    "heavy_compute", "commit_required", "scope", "acceptance", "status",
}
OPTIONAL_COLUMNS = {"BLOCKED_REASON", "WAITING_REASON"}
STATUSES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED",
            "WAITING_WINDOWS", "WAITING_DATA"}
TASK_RE = re.compile(r"^[A-Z0-9]+-[0-9]{3}$")
GATE_RE = re.compile(r"^G([0-9]+)$")


class LedgerError(ValueError):
    pass


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_ledger(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as stream:
        reader = csv.DictReader(stream)
        if reader.fieldnames is None:
            raise LedgerError(f"ledger has no header: {path}")
        fields = set(reader.fieldnames)
        if not REQUIRED_COLUMNS.issubset(fields):
            raise LedgerError(f"ledger columns mismatch: {reader.fieldnames}")
        unknown = fields - REQUIRED_COLUMNS - OPTIONAL_COLUMNS
        if unknown:
            raise LedgerError(f"ledger has unknown columns: {sorted(unknown)}")
        rows = list(reader)
    if not rows:
        raise LedgerError("ledger is empty")
    return rows


def validate_rows(rows: list[dict[str, str]], *, enforce_states: bool = True) -> dict:
    by_id: dict[str, dict[str, str]] = {}
    order: list[str] = []
    for line_no, row in enumerate(rows, start=2):
        task_id = row["task_id"]
        if not TASK_RE.fullmatch(task_id):
            raise LedgerError(f"line {line_no}: invalid task_id {task_id!r}")
        if task_id in by_id:
            raise LedgerError(f"duplicate task_id: {task_id}")
        if row["status"] not in STATUSES:
            raise LedgerError(f"{task_id}: invalid status {row['status']!r}")
        if row["heavy_compute"] not in {"yes", "no"} or row["commit_required"] not in {"yes", "no"}:
            raise LedgerError(f"{task_id}: heavy_compute/commit_required must be yes/no")
        if not GATE_RE.fullmatch(row["gate"]):
            raise LedgerError(f"{task_id}: invalid gate {row['gate']!r}")
        if not all(row[name].strip() for name in ("title", "platform", "change_class", "scope", "acceptance")):
            raise LedgerError(f"{task_id}: required text field is empty")
        if row["status"] == "WAITING_WINDOWS" and "Windows" not in row["platform"]:
            raise LedgerError(f"{task_id}: WAITING_WINDOWS is legal only for a Windows task")
        if row["status"] == "BLOCKED":
            if not row.get("BLOCKED_REASON", "").strip():
                raise LedgerError(f"{task_id}: BLOCKED requires BLOCKED_REASON (object/command/time)")
            if row["commit_required"] == "yes":
                raise LedgerError(f"{task_id}: BLOCKED is not a final state for a commit_required task")
        by_id[task_id] = row
        order.append(task_id)

    deps: dict[str, list[str]] = {}
    reverse: dict[str, list[str]] = defaultdict(list)
    indegree: dict[str, int] = {}
    for task_id, row in by_id.items():
        task_deps = [item.strip() for item in row["depends_on"].split(";") if item.strip()]
        if len(task_deps) != len(set(task_deps)):
            raise LedgerError(f"{task_id}: duplicate dependency")
        task_gate = int(GATE_RE.fullmatch(row["gate"]).group(1))
        for dep in task_deps:
            if dep not in by_id:
                raise LedgerError(f"{task_id}: unknown dependency {dep}")
            dep_gate = int(GATE_RE.fullmatch(by_id[dep]["gate"]).group(1))
            if dep_gate > task_gate:
                raise LedgerError(f"{task_id}: dependency {dep} belongs to a later gate")
            reverse[dep].append(task_id)
        deps[task_id] = task_deps
        indegree[task_id] = len(task_deps)

    queue = deque(task_id for task_id in order if indegree[task_id] == 0)
    topo: list[str] = []
    while queue:
        task_id = queue.popleft()
        topo.append(task_id)
        for next_id in reverse[task_id]:
            indegree[next_id] -= 1
            if indegree[next_id] == 0:
                queue.append(next_id)
    if len(topo) != len(rows):
        raise LedgerError(f"dependency cycle: {sorted(k for k, v in indegree.items() if v > 0)}")

    if enforce_states:
        if sum(row["status"] == "IN_PROGRESS" for row in rows) > 1:
            raise LedgerError("more than one task is IN_PROGRESS")
        for task_id, task_deps in deps.items():
            state = by_id[task_id]["status"]
            if state in {"PASS", "IN_PROGRESS"}:
                bad = [dep for dep in task_deps if by_id[dep]["status"] != "PASS"]
                if bad:
                    raise LedgerError(f"{task_id}: {state} with non-PASS dependencies {bad}")
        owner = by_id.get("REL-004")
        if owner is not None and owner["status"] == "PASS":
            raise LedgerError("REL-004 is Owner-only and cannot be marked PASS by the Agent")

    counts = Counter(row["status"] for row in rows)
    gates: dict[str, str] = {}
    for gate in sorted({row["gate"] for row in rows}, key=lambda value: int(value[1:])):
        states = [row["status"] for row in rows if row["gate"] == gate]
        if all(state == "PASS" for state in states):
            gates[gate] = "PASS"
        elif "FAIL" in states:
            gates[gate] = "FAIL"
        elif "BLOCKED" in states:
            gates[gate] = "BLOCKED"
        elif "IN_PROGRESS" in states:
            gates[gate] = "IN_PROGRESS"
        elif "WAITING_WINDOWS" in states:
            gates[gate] = "WAITING_WINDOWS"
        elif "WAITING_DATA" in states:
            gates[gate] = "WAITING_DATA"
        else:
            gates[gate] = "NOT_STARTED"
    return {"task_count": len(rows), "task_counts": dict(sorted(counts.items())),
            "gate_status": gates, "topological_order": topo}


# ── selftest 负例 fixtures（内嵌，保证可复现；也支持外部目录） ──

def _ledger(statuses: dict[str, str], deps: dict[str, str] | None = None,
            platform: dict[str, str] | None = None,
            blocked_reason: dict[str, str] | None = None) -> str:
    lines = ["task_id,gate,title,depends_on,platform,change_class,heavy_compute,commit_required,scope,acceptance,status,BLOCKED_REASON"]
    ids = list(statuses)
    for task_id in ids:
        gate = "G0" if task_id.startswith("R0") or task_id == "GOV-001" else "G1"
        plats = (platform or {}).get(task_id, "Linux")
        dep = (deps or {}).get(task_id, "")
        reason = (blocked_reason or {}).get(task_id, "")
        lines.append(f"{task_id},{gate},{task_id} title,{dep},{plats},governance,no,yes,scope,acceptance,{statuses[task_id]},{reason}")
    return "\n".join(lines) + "\n"


def _write_tmp(name: str, content: str) -> Path:
    path = Path(sys.argv[0]).resolve().parent.parent.parent / "run" / "temp" / f"ledger_fixture_{name}.csv"
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path


SELFTEST_CASES: list[tuple[str, str, str]] = [
    ("illegal_state", _ledger({"R0-001": "REVIEW_PENDING", "R0-002": "NOT_STARTED"}),
     "invalid status"),
    ("two_in_progress", _ledger({"R0-001": "IN_PROGRESS", "R0-002": "IN_PROGRESS"}),
     "more than one task is IN_PROGRESS"),
    ("pass_with_waiting_dep", _ledger({"R0-001": "PASS", "R0-002": "WAITING_WINDOWS"},
                                       {"R0-001": "R0-002"}, platform={"R0-002": "Windows"}),
     "non-PASS dependencies"),
    ("unknown_dep", _ledger({"R0-001": "PASS", "R0-002": "PASS"}, {"R0-002": "NO-SUCH"}),
     "unknown dependency"),
    ("cycle", _ledger({"R0-001": "NOT_STARTED", "R0-002": "NOT_STARTED"},
                      {"R0-001": "R0-002", "R0-002": "R0-001"}), "dependency cycle"),
    ("rel_early_pass", _ledger({"R0-001": "PASS", "REL-004": "PASS"}, {"REL-004": "R0-001"}),
     "Owner-only"),
    ("waiting_windows_non_win", _ledger({"R0-001": "WAITING_WINDOWS"}, platform={"R0-001": "Linux"}),
     "legal only for a Windows task"),
    ("blocked_no_reason", _ledger({"R0-001": "BLOCKED"}), "BLOCKED requires BLOCKED_REASON"),
    ("bad_task_id", _ledger({"bad": "NOT_STARTED"}), "invalid task_id"),
    ("missing_task", _ledger({"R0-001": "PASS", "R0-003": "PASS"}, {"R0-003": "R0-002"}),
     "unknown dependency"),
]


def run_selftest() -> int:
    failures = 0
    for name, content, expected in SELFTEST_CASES:
        path = _write_tmp(name, content)
        try:
            validate_rows(load_ledger(path), enforce_states=True)
        except LedgerError as exc:
            if expected in str(exc):
                print(f"SELFTEST_PASS {name}: caught {exc}")
                continue
            print(f"SELFTEST_FAIL {name}: wrong error: {exc}")
            failures += 1
        else:
            print(f"SELFTEST_FAIL {name}: expected failure ({expected}) but PASSED")
            failures += 1
    if failures:
        print(f"SELFTEST_FAIL total={failures}")
        return 1
    print(f"SELFTEST_PASS total={len(SELFTEST_CASES)}")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("ledger", nargs="?", type=Path)
    parser.add_argument("--baseline-sha256")
    parser.add_argument("--structure-only", action="store_true")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args(argv)

    if args.selftest:
        return run_selftest()

    if args.ledger is None:
        print("TASK_LEDGER_FAIL: ledger path required (or --selftest)", file=sys.stderr)
        return 2
    try:
        digest = sha256_file(args.ledger)
        if args.baseline_sha256 and digest != args.baseline_sha256.lower():
            raise LedgerError(f"ledger hash changed: {digest}")
        result = validate_rows(load_ledger(args.ledger), enforce_states=not args.structure_only)
        result["ledger_sha256"] = digest
        rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            args.output.write_text(rendered, encoding="utf-8")
        else:
            print(rendered, end="")
        print(f"TASK_LEDGER_PASS tasks={result['task_count']}", file=sys.stderr)
        return 0
    except (OSError, LedgerError) as exc:
        print(f"TASK_LEDGER_FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
