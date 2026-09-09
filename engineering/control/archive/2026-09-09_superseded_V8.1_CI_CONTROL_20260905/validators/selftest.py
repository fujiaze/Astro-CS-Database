#!/usr/bin/env python3
"""Negative tests for control-package validators."""
from __future__ import annotations

import argparse
import csv
import json
import shutil
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
from validate_control import ValidationError, validate


def expect_fail(root: Path, mutate, name: str) -> None:
    with tempfile.TemporaryDirectory(prefix="astrocs-control-selftest-") as tmp:
        copy = Path(tmp) / "control"
        shutil.copytree(root, copy)
        for name_ in ["CONTROL_MANIFEST.json", "SHA256SUMS"]:
            (copy / name_).unlink(missing_ok=True)
        mutate(copy)
        try:
            validate(copy)
        except ValidationError:
            print(f"NEGATIVE_PASS {name}")
            return
        raise AssertionError(f"negative case was accepted: {name}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    validate(root)

    expect_fail(root, lambda p: (p / "START_PROMPT.txt").write_text("甲" * 101, encoding="utf-8"), "long_prompt")

    def duplicate_task(p: Path) -> None:
        path = p / "CONTROL_TASK_LEDGER.csv"
        lines = path.read_text(encoding="utf-8").splitlines()
        path.write_text("\n".join(lines + [lines[1]]) + "\n", encoding="utf-8")
    expect_fail(root, duplicate_task, "duplicate_task")

    def cycle(p: Path) -> None:
        path = p / "CONTROL_TASK_LEDGER.csv"
        with path.open(encoding="utf-8", newline="") as handle:
            rows = list(csv.DictReader(handle)); fields = handle.seek(0)
        rows[0]["depends_on"] = rows[-1]["task_id"]
        with path.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=rows[0].keys()); writer.writeheader(); writer.writerows(rows)
    expect_fail(root, cycle, "dependency_cycle")

    def pr_trigger(p: Path) -> None:
        path = p / "templates/fatduck-realdata.yml"
        path.write_text(path.read_text(encoding="utf-8") + "\npull_request:\n", encoding="utf-8")
    expect_fail(root, pr_trigger, "untrusted_trigger")

    def fatduck_checkout(p: Path) -> None:
        path = p / "templates/fatduck-realdata.yml"
        text = path.read_text(encoding="utf-8").replace("      - name: Download candidate only", "      - uses: actions/checkout@deadbeef\n      - name: Download candidate only")
        path.write_text(text, encoding="utf-8")
    expect_fail(root, fatduck_checkout, "fatduck_checkout")

    def raw_upload(p: Path) -> None:
        path = p / "ci/publish_policy.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["raw_data_upload_allowed"] = True
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, raw_upload, "raw_upload")

    def no_jpg_authorization(p: Path) -> None:
        path = p / "ci/publish_policy.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["owner_authorization"] = ""
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, no_jpg_authorization, "jpg_without_authorization")

    def no_monitor(p: Path) -> None:
        path = p / "ci/checks.seed.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["checks"][5]["requires_monitor"] = False
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, no_monitor, "heavy_without_monitor")

    def allow_clone(p: Path) -> None:
        path = p / "ci/host_layout.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["create_or_clone_repo"] = True
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, allow_clone, "clone_allowed")

    def fixed_repo_path(p: Path) -> None:
        path = p / "ci/host_layout.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["repo_root"] = "/srv/astrocs/main"
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, fixed_repo_path, "fixed_repo_path")

    def old_migration_task(p: Path) -> None:
        path = p / "CONTROL_TASK_LEDGER.csv"
        path.write_text(path.read_text(encoding="utf-8").replace("V81-ADOPT-001", "V8-MIG-001"), encoding="utf-8")
    expect_fail(root, old_migration_task, "old_migration_task")

    def destructive_acceptance(p: Path) -> None:
        path = p / "CONTROL_TASK_LEDGER.csv"
        path.write_text(path.read_text(encoding="utf-8").replace("python3 validators/validate_control.py --root .", "git reset --hard"), encoding="utf-8")
    expect_fail(root, destructive_acceptance, "destructive_acceptance")

    def reprovision_agent_host(p: Path) -> None:
        path = p / "ci/toolchain.policy.json"
        data = json.loads(path.read_text(encoding="utf-8")); data["agent_host"]["host_reprovisioning"] = True
        path.write_text(json.dumps(data), encoding="utf-8")
    expect_fail(root, reprovision_agent_host, "agent_host_reprovisioning")

    print("SELFTEST_PASS cases=13")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
