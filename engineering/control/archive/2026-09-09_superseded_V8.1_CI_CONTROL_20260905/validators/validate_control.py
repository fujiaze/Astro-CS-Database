#!/usr/bin/env python3
"""Validate the control package with Python standard library only."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
import sys
from pathlib import Path


REQUIRED = [
    "START_PROMPT.txt", "VERSION", "00_READ_FIRST.md", "01_FROZEN_CONSTRAINTS.md",
    "02_CURRENT_STATE_AUDIT.md", "03_VM_AND_CI_ARCHITECTURE.md",
    "04_FOREGROUND_AGENT_RUNBOOK.md", "05_FIXED_SUBAGENT_BINDINGS.yaml",
    "06_TASK_GRAPH_AND_GATES.md", "07_CI_MACHINE_CONTRACT.md",
    "08_MAIN_ONLY_GIT_PROTOCOL.md", "09_WINDOWS_AND_FATDUCK_VALIDATION.md",
    "10_SECURITY_AND_RUNNER.md", "11_EVIDENCE_AND_AUDIT_PACKAGE.md",
    "12_FAILURE_POLICY.md", "13_PRIMARY_REFERENCES.md", "14_TOKEN_EFFICIENT_EXECUTION.md",
    "15_SUPERSESSION_NOTICE.md",
    "CONTROL_TASK_LEDGER.csv", "GATE_REQUIREMENTS.csv", "CURRENT_CHECKPOINT.json",
    "ci/checks.schema.json", "ci/checks.seed.json", "ci/host_layout.json",
    "ci/toolchain.policy.json", "ci/publish_policy.json", "ci/actions.lock.template.json",
    "templates/linux-ci.yml", "templates/windows-ci.yml", "templates/fatduck-realdata.yml",
    "templates/FATDUCK_HARNESS_CONTRACT.md", "tasks/01_WORKSPACE_ADOPTION_TASKS.md",
    "tasks/02_CI_TASKS.md", "tasks/03_P0_REMEDIATION_TASKS.md",
    "tasks/04_FATDUCK_AND_RELEASE_TASKS.md",
    "baseline/V7_1_STATIC_TASK_LEDGER.csv", "baseline/REVIEW_CLAIMED_TASK_STATE.csv",
    "baseline/README.md", "baseline/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip",
]


class ValidationError(Exception):
    pass


def fail(message: str) -> None:
    raise ValidationError(message)


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        fail(f"invalid json {path}: {exc}")


def validate_ledger(root: Path) -> set[str]:
    path = root / "CONTROL_TASK_LEDGER.csv"
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    fields = {"task_id", "owner", "mode", "depends_on", "acceptance_command", "timeout_s", "heavy", "gate", "status"}
    if not rows or not fields.issubset(rows[0]):
        fail("task ledger missing rows or required columns")
    ids = [row["task_id"].strip() for row in rows]
    if len(ids) != len(set(ids)):
        fail("duplicate task_id")
    idset = set(ids)
    with (root / "baseline/V7_1_STATIC_TASK_LEDGER.csv").open(encoding="utf-8", newline="") as handle:
        inherited_ids = {row["task_id"].strip() for row in csv.DictReader(handle)}
    if idset & inherited_ids:
        fail(f"V8 task IDs collide with V7.1: {sorted(idset & inherited_ids)}")
    graph: dict[str, list[str]] = {}
    for row in rows:
        task = row["task_id"].strip()
        if row["mode"] not in {"read", "write"}:
            fail(f"invalid mode: {task}")
        if row["heavy"] not in {"true", "false"}:
            fail(f"invalid heavy flag: {task}")
        try:
            timeout = int(row["timeout_s"])
        except ValueError:
            fail(f"invalid timeout: {task}")
        if timeout <= 0 or not row["acceptance_command"].strip():
            fail(f"missing acceptance/timeout: {task}")
        if f"--require-gate {row['gate']}" in row["acceptance_command"]:
            fail(f"task acceptance depends on its own gate: {task}")
        deps = [x for x in row["depends_on"].split("|") if x]
        unknown = set(deps) - idset
        if unknown:
            fail(f"unknown dependencies for {task}: {sorted(unknown)}")
        graph[task] = deps
    visiting: set[str] = set()
    done: set[str] = set()
    def visit(node: str) -> None:
        if node in visiting:
            fail(f"dependency cycle at {node}")
        if node in done:
            return
        visiting.add(node)
        for dep in graph[node]:
            visit(dep)
        visiting.remove(node)
        done.add(node)
    for node in ids:
        visit(node)
    bindings = (root / "05_FIXED_SUBAGENT_BINDINGS.yaml").read_text(encoding="utf-8")
    owners = set(re.findall(r"^\s*- id:\s*([A-Z0-9-]+)\s*$", bindings, re.MULTILINE))
    missing = {row["owner"] for row in rows} - owners
    if missing:
        fail(f"task owners missing from fixed bindings: {sorted(missing)}")
    return idset


def validate_gates(root: Path, task_ids: set[str]) -> None:
    with (root / "GATE_REQUIREMENTS.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    gates = {row["gate"] for row in rows}
    if gates != {"G-ADOPT", "G-CI", "G-FIX", "G-FAT", "G-REL"}:
        fail(f"unexpected gates: {sorted(gates)}")
    for row in rows:
        required = {x for x in row["required_tasks"].split("|") if x}
        if not required or required - task_ids:
            fail(f"invalid gate tasks for {row['gate']}: {sorted(required - task_ids)}")


def validate_ci(root: Path) -> None:
    checks = load_json(root / "ci/checks.seed.json")
    if checks.get("schema_version") != 1 or not checks.get("checks"):
        fail("empty or invalid checks seed")
    ids = [item.get("id") for item in checks["checks"]]
    if len(ids) != len(set(ids)):
        fail("duplicate CI check id")
    required = {"VERSION-CONSISTENCY", "TRACEABILITY", "PRODUCTION-GRAPH", "NO-SERIAL-HEAVY", "ACR-DORMANT", "SYNTHETIC-SCIENCE", "WINDOWS-BUILD-PACKAGE", "FATDUCK-PUBLISH-POLICY"}
    if required - set(ids):
        fail(f"missing seed checks: {sorted(required - set(ids))}")
    for item in checks["checks"]:
        if item.get("timeout_seconds", 0) <= 0:
            fail(f"invalid check timeout: {item.get('id')}")
        if item.get("heavy") and not item.get("requires_monitor"):
            fail(f"heavy check lacks monitor: {item.get('id')}")
        if not item.get("waivable", True) and item.get("id") == "LINUX-DEEP":
            fail("deep observation baseline may be waivable during bootstrap")
    policy = load_json(root / "ci/publish_policy.json")
    forbidden = {x.lower() for x in policy.get("forbidden_extensions", [])}
    if not {".fit", ".fits", ".hiss", ".npy"}.issubset(forbidden):
        fail("publish policy does not forbid raw scientific formats")
    if policy.get("raw_data_upload_allowed") is not False:
        fail("raw upload must be false")
    if policy.get("derived_science_image_upload_allowed") is not True or not policy.get("owner_authorization"):
        fail("derived JPG upload requires frozen owner authorization")
    lock = load_json(root / "ci/actions.lock.template.json")
    if not lock.get("actions") or any(item.get("commit") != "__RESOLVE_FULL_SHA__" for item in lock["actions"]):
        fail("action lock template must retain explicit unresolved guards")


def validate_existing_workspace_contract(root: Path, task_ids: set[str]) -> None:
    if (root / "VERSION").read_text(encoding="utf-8").strip() != "8.1":
        fail("control package VERSION must be 8.1")
    layout = load_json(root / "ci/host_layout.json")
    required_false = {
        "create_or_clone_repo", "relocate_repo", "create_branch_allowed",
        "git_worktree_allowed", "extra_development_clones_allowed",
        "delete_existing_worktrees_allowed",
    }
    if layout.get("use_existing_workspace") is not True:
        fail("existing workspace must be authoritative")
    for key in required_false:
        if layout.get(key) is not False:
            fail(f"workspace mutation must be false: {key}")
    if layout.get("repo_root") != "DISCOVER_AT_RUNTIME_WITH_GIT_REV_PARSE_SHOW_TOPLEVEL":
        fail("repo root must be discovered at runtime")
    if layout.get("allowed_branch") != "main":
        fail("only main may be used")
    if "V81-ADOPT-001" not in task_ids or "V8-MIG-001" in task_ids:
        fail("V8.1 adoption task set is missing or old migration tasks remain")
    checkpoint = load_json(root / "CURRENT_CHECKPOINT.json")
    if checkpoint.get("first_task") != "V81-ADOPT-001" or checkpoint.get("active_gate") != "G-ADOPT":
        fail("checkpoint does not start from existing-workspace adoption")
    ledger = (root / "CONTROL_TASK_LEDGER.csv").read_text(encoding="utf-8")
    forbidden = ["/srv/astrocs", "Fresh clone", "Provision canonical", "SA-MIG", "V8-MIG"]
    for token in forbidden:
        if token in ledger:
            fail(f"old migration directive remains in active ledger: {token}")
    with (root / "CONTROL_TASK_LEDGER.csv").open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if rows[0]["task_id"] != "V81-ADOPT-001":
        fail("first ledger task must adopt the existing workspace")
    dangerous_commands = ["git clone", "git worktree", "git reset", "git stash", "git clean", "git init"]
    for row in rows:
        command = row["acceptance_command"].lower()
        if any(token in command for token in dangerous_commands):
            fail(f"destructive or duplicate-workspace command in ledger: {row['task_id']}")
        if row.get("allowed_paths", "").startswith("/"):
            fail(f"absolute Unix allowed path in ledger: {row['task_id']}")
    bindings = (root / "05_FIXED_SUBAGENT_BINDINGS.yaml").read_text(encoding="utf-8")
    for token in ["single_writable_repo: /", "SA-MIG-", "role: vm_migration"]:
        if token in bindings:
            fail(f"old fixed-layout binding remains: {token}")
    toolchain = load_json(root / "ci/toolchain.policy.json")
    host = toolchain.get("agent_host", {})
    if host.get("require_exact_hosted_versions") is not False or host.get("host_reprovisioning") is not False:
        fail("agent host must inventory existing tools without hosted-version reprovisioning")


def validate_workflows(root: Path) -> None:
    linux = (root / "templates/linux-ci.yml").read_text(encoding="utf-8")
    windows = (root / "templates/windows-ci.yml").read_text(encoding="utf-8")
    fatduck = (root / "templates/fatduck-realdata.yml").read_text(encoding="utf-8")
    for name, text in [("linux", linux), ("windows", windows), ("fatduck", fatduck)]:
        if "pull_request" in text or "pull_request_target" in text:
            fail(f"untrusted trigger in {name} workflow")
        if "__" not in text or "FULL_SHA__" not in text:
            fail(f"workflow template lost action SHA guards: {name}")
        if re.search(r"(?:--parallel|-j)\s*[1-9][0-9]*", text):
            fail(f"hardcoded parallelism in {name} workflow")
    marker = "  fatduck-validate:"
    notify = "  notify-owner:"
    if marker not in fatduck or notify not in fatduck:
        fail("fatduck workflow missing required jobs")
    if "workflow_dispatch" in fatduck:
        fail("Fatduck workflow must not expose public manual dispatch")
    body = fatduck.split(marker, 1)[1].split(notify, 1)[0]
    banned = ["actions/checkout", "git clone", "git fetch", "issues: write", "pip install", "choco install"]
    for token in banned:
        if token in body:
            fail(f"forbidden Fatduck job operation: {token}")
    required = ["actions/download-artifact", "run-validation.cmd", "actions/upload-artifact", "fatduck-realdata"]
    for token in required:
        if token not in body:
            fail(f"Fatduck job missing: {token}")


def validate_manifest(root: Path) -> None:
    sums = root / "SHA256SUMS"
    manifest = root / "CONTROL_MANIFEST.json"
    if not sums.exists() and not manifest.exists():
        return
    if not sums.exists() or not manifest.exists():
        fail("manifest and SHA256SUMS must appear together")
    data = load_json(manifest)
    entries = data.get("files", [])
    listed = set()
    for item in entries:
        rel = item["path"]
        target = root / rel
        if not target.is_file():
            fail(f"manifest file missing: {rel}")
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        if digest != item["sha256"] or target.stat().st_size != item["size"]:
            fail(f"manifest mismatch: {rel}")
        listed.add(rel)
    actual = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file() and p.name not in {"CONTROL_MANIFEST.json", "SHA256SUMS"}}
    if listed != actual:
        fail(f"manifest coverage mismatch missing={sorted(actual-listed)} extra={sorted(listed-actual)}")


def validate(root: Path) -> None:
    for rel in REQUIRED:
        if not (root / rel).is_file():
            fail(f"required file missing: {rel}")
    prompt = (root / "START_PROMPT.txt").read_text(encoding="utf-8").strip()
    if not prompt or len(prompt) > 100:
        fail(f"START_PROMPT must be 1..100 characters; got {len(prompt)}")
    bad = [p for p in root.rglob("*") if p.is_file() and ("__pycache__" in p.parts or p.suffix == ".pyc")]
    if bad:
        fail("bytecode/cache files present")
    oversized = [p for p in root.rglob("*") if p.is_file() and p.stat().st_size > 10 * 1024 * 1024]
    if oversized:
        fail(f"oversized control files: {[str(p) for p in oversized]}")
    inherited = root / "baseline/AstroCS_ALPHA3_MODULAR_REFOUNDATION_CONTROL_V7_1_20260902_FINAL3.zip"
    expected = "005448fb3b6892d3063d23149692fbd9226a892a2c1891e4cb8d14a80e8491c3"
    if hashlib.sha256(inherited.read_bytes()).hexdigest() != expected:
        fail("inherited V7.1 control archive hash mismatch")
    for path in root.rglob("*.json"):
        if path.name != "CONTROL_MANIFEST.json":
            load_json(path)
    task_ids = validate_ledger(root)
    validate_gates(root, task_ids)
    validate_ci(root)
    validate_existing_workspace_contract(root, task_ids)
    validate_workflows(root)
    validate_manifest(root)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    try:
        validate(args.root.resolve())
    except ValidationError as exc:
        print(f"CONTROL_FAIL: {exc}", file=sys.stderr)
        return 1
    count = sum(1 for p in args.root.rglob("*") if p.is_file())
    print(f"CONTROL_PASS files={count}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
