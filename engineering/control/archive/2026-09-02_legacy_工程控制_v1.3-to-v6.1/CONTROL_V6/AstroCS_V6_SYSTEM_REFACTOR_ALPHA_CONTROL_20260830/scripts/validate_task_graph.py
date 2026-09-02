#!/usr/bin/env python3
"""Validate AstroCS task ledger dependencies and state transitions."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import pathlib
import re
import sys
from collections import Counter, defaultdict, deque

REQUIRED_COLUMNS = {
    "task_id", "gate", "title", "depends_on", "platform", "change_class",
    "heavy_compute", "commit_required", "scope", "acceptance", "status",
}
STATUSES = {"NOT_STARTED", "IN_PROGRESS", "PASS", "FAIL", "BLOCKED", "WAITING_WINDOWS"}
TASK_RE = re.compile(r"^[A-Z0-9]+-[0-9]{3}$")
GATE_RE = re.compile(r"^G([0-9]+)$")


class LedgerError(ValueError):
    pass


def sha256_file(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def load_ledger(path: pathlib.Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None or set(reader.fieldnames) != REQUIRED_COLUMNS:
            raise LedgerError(f"ledger columns mismatch: {reader.fieldnames}")
        rows = list(reader)
    if not rows:
        raise LedgerError("ledger is empty")
    return rows


def validate_rows(rows: list[dict[str, str]], *, require_pass_dependencies: bool = True) -> dict:
    by_id: dict[str, dict[str, str]] = {}
    order: list[str] = []
    for line, row in enumerate(rows, start=2):
        tid = row["task_id"]
        if not TASK_RE.fullmatch(tid):
            raise LedgerError(f"line {line}: invalid task_id {tid!r}")
        if tid in by_id:
            raise LedgerError(f"duplicate task_id: {tid}")
        if row["status"] not in STATUSES:
            raise LedgerError(f"{tid}: invalid status {row['status']!r}")
        if row["heavy_compute"] not in {"yes", "no"}:
            raise LedgerError(f"{tid}: heavy_compute must be yes/no")
        if row["commit_required"] not in {"yes", "no"}:
            raise LedgerError(f"{tid}: commit_required must be yes/no")
        if not GATE_RE.fullmatch(row["gate"]):
            raise LedgerError(f"{tid}: invalid gate {row['gate']!r}")
        if not row["title"].strip() or not row["scope"].strip() or not row["acceptance"].strip():
            raise LedgerError(f"{tid}: title/scope/acceptance must be non-empty")
        by_id[tid] = row
        order.append(tid)

    deps: dict[str, list[str]] = {}
    reverse: dict[str, list[str]] = defaultdict(list)
    indegree: dict[str, int] = {}
    for tid, row in by_id.items():
        task_deps = [d.strip() for d in row["depends_on"].split(";") if d.strip()]
        if len(task_deps) != len(set(task_deps)):
            raise LedgerError(f"{tid}: duplicate dependency")
        task_gate = int(GATE_RE.fullmatch(row["gate"]).group(1))
        for dep in task_deps:
            if dep not in by_id:
                raise LedgerError(f"{tid}: unknown dependency {dep}")
            dep_gate = int(GATE_RE.fullmatch(by_id[dep]["gate"]).group(1))
            if dep_gate > task_gate:
                raise LedgerError(f"{tid}: dependency {dep} is in later gate")
            reverse[dep].append(tid)
        deps[tid] = task_deps
        indegree[tid] = len(task_deps)

    queue = deque([tid for tid in order if indegree[tid] == 0])
    topo: list[str] = []
    while queue:
        tid = queue.popleft()
        topo.append(tid)
        for nxt in reverse[tid]:
            indegree[nxt] -= 1
            if indegree[nxt] == 0:
                queue.append(nxt)
    if len(topo) != len(rows):
        cyclic = sorted(tid for tid, degree in indegree.items() if degree > 0)
        raise LedgerError(f"dependency cycle: {cyclic}")

    if require_pass_dependencies:
        for tid, task_deps in deps.items():
            state = by_id[tid]["status"]
            if state == "PASS":
                bad = [d for d in task_deps if by_id[d]["status"] != "PASS"]
                if bad:
                    raise LedgerError(f"{tid}: PASS with non-PASS dependencies {bad}")
            if state == "IN_PROGRESS":
                bad = [d for d in task_deps if by_id[d]["status"] != "PASS"]
                if bad:
                    raise LedgerError(f"{tid}: IN_PROGRESS before dependencies PASS {bad}")

    counts = Counter(row["status"] for row in rows)
    gates: dict[str, str] = {}
    for gate in sorted({row["gate"] for row in rows}, key=lambda x: int(x[1:])):
        states = [row["status"] for row in rows if row["gate"] == gate]
        if all(s == "PASS" for s in states):
            gates[gate] = "PASS"
        elif "FAIL" in states:
            gates[gate] = "FAIL"
        elif "BLOCKED" in states:
            gates[gate] = "BLOCKED"
        elif "IN_PROGRESS" in states:
            gates[gate] = "IN_PROGRESS"
        elif "WAITING_WINDOWS" in states:
            gates[gate] = "WAITING_WINDOWS"
        else:
            gates[gate] = "NOT_STARTED"

    return {"task_count": len(rows), "task_counts": dict(sorted(counts.items())), "gate_status": gates, "topological_order": topo}


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser()
    p.add_argument("ledger", type=pathlib.Path)
    p.add_argument("--baseline-sha256")
    p.add_argument("--output", type=pathlib.Path)
    p.add_argument("--structure-only", action="store_true")
    args = p.parse_args(argv)
    try:
        digest = sha256_file(args.ledger)
        if args.baseline_sha256 and digest != args.baseline_sha256.lower():
            raise LedgerError(f"ledger hash changed: {digest}")
        result = validate_rows(load_ledger(args.ledger), require_pass_dependencies=not args.structure_only)
        result["ledger_sha256"] = digest
        text = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            args.output.write_text(text, encoding="utf-8")
        else:
            print(text, end="")
        print(f"TASK_GRAPH_PASS tasks={result['task_count']}", file=sys.stderr)
        return 0
    except (OSError, LedgerError) as exc:
        print(f"TASK_GRAPH_FAIL: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
