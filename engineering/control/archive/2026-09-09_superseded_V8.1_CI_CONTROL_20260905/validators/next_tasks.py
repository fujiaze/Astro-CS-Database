#!/usr/bin/env python3
"""Print dependency-ready tasks as compact JSON."""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ledger", type=Path, required=True)
    parser.add_argument("--state", type=Path)
    args = parser.parse_args()
    with args.ledger.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    state = {}
    if args.state and args.state.exists():
        payload = json.loads(args.state.read_text(encoding="utf-8"))
        state = payload.get("tasks", payload)
    def status(row):
        item = state.get(row["task_id"])
        if item is None:
            return row["status"]
        return item.get("status", row["status"]) if isinstance(item, dict) else item
    closed = {row["task_id"] for row in rows if status(row) in {"PASS", "CLOSED"}}
    ready = []
    for row in rows:
        if status(row) not in {"NOT_STARTED", "READY", "FATDUCK_PENDING"}:
            continue
        deps = {x for x in row["depends_on"].split("|") if x}
        if deps <= closed:
            ready.append({
                "task_id": row["task_id"], "owner": row["owner"], "mode": row["mode"],
                "goal": row["goal"], "acceptance_command": row["acceptance_command"],
                "heavy": row["heavy"] == "true", "gate": row["gate"]
            })
    print(json.dumps({"ready": ready}, ensure_ascii=False, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
