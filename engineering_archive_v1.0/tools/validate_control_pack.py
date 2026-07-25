#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    "00_START_HERE.md",
    "01_PROJECT_CHARTER.md",
    "02_BASELINE_AUDIT.md",
    "control/PROJECT_STATE.yaml",
    "control/CURRENT_WORK.md",
    "control/MASTER_TASK_REGISTER.csv",
    "agent/MASTER_AGENT_PROMPT.md",
    "agent/AUTONOMOUS_MASTER_AGENT_PROMPT.md",
    "agent/AUTONOMOUS_REVIEW_PROTOCOL.md",
    "agent/SESSION_RESUME_PROMPT.md",
    "tasks/P00_BASELINE_AND_REPOSITORY_INTEGRITY.md",
    "checklists/TASK_ENTRY_CHECKLIST.md",
    "templates/TASK_REPORT.md",
    "tools/task_controller.py",
]
ACTIVE = {"IN_PROGRESS", "IN_REVIEW", "BLOCKED"}


def deps_done(row: dict[str, str], lookup: dict[str, dict[str, str]]) -> bool:
    deps = [item.strip() for item in row.get("depends_on", "").split(";") if item.strip()]
    return all(dep in lookup and lookup[dep]["status"] == "DONE" for dep in deps)


def main() -> int:
    missing = [path for path in REQUIRED if not (ROOT / path).is_file()]
    if missing:
        print("Missing required files:")
        for path in missing:
            print(" -", path)
        return 1

    with (ROOT / "control" / "MASTER_TASK_REGISTER.csv").open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    ids = [row["task_id"] for row in rows]
    if len(ids) != len(set(ids)):
        print("Duplicate task IDs found")
        return 2

    lookup = {row["task_id"]: row for row in rows}
    unknown_deps: list[str] = []
    for row in rows:
        for dep in [item.strip() for item in row.get("depends_on", "").split(";") if item.strip()]:
            if dep not in lookup:
                unknown_deps.append(f"{row['task_id']}->{dep}")
    if unknown_deps:
        print("Unknown dependencies:", ", ".join(unknown_deps))
        return 3

    active = [row for row in rows if row["status"] in ACTIVE]
    all_done = all(row["status"] == "DONE" for row in rows)
    if not all_done and len(active) != 1:
        print("Expected exactly one active task; found:", [(row["task_id"], row["status"]) for row in active])
        return 4
    if active and active[0]["status"] != "BLOCKED" and not deps_done(active[0], lookup):
        print("Active task has unfinished dependencies:", active[0]["task_id"])
        return 5

    state_text = (ROOT / "control" / "PROJECT_STATE.yaml").read_text(encoding="utf-8")
    match = re.search(r"(?m)^current_task:\s*(\S+)\s*$", state_text)
    state_task = match.group(1) if match else None
    expected = active[0]["task_id"] if active else "null"
    if state_task != expected:
        print(f"PROJECT_STATE current_task mismatch: state={state_task}, register={expected}")
        return 6

    print(f"OK: {len(REQUIRED)} required files, {len(rows)} tasks, active={expected}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
