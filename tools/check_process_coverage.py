#!/usr/bin/env python3
"""Check the frozen registry manifest against the process whitelist.

The single source of truth for coverage is:
  * docs/engineering/rebuild/processes/README.md  (22 processes, 57 typed
    operations; every row is the whitelist of one process), and
  * build/process-manifest.json                    (production build product
    exported from the frozen registry).

There is deliberately no second handwritten registry in this tool.  The check
fails with a named missing/extra/attribution/document/probe item; a partial
manifest during M0 is reported accurately and is not accepted as complete.

Usage:
  tools/check_process_coverage.py [--manifest PATH] [--source-root DIR]
                                  [--readme PATH]
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

PROTOCOL = "acsd.process/2"
EXPECTED_PROCESSES = 22
EXPECTED_OPERATIONS = 57
LEGAL_MODULES = {
    "model", "numerics", "coordinates", "observation", "catalogue", "sources",
    "response", "photometry", "geometry", "operators", "uncertainty",
    "information", "sky", "coadd", "projection", "storage", "resources",
    "runtime", "workflows",
}
DOC_LINK = re.compile(r"^\[([^\]]+)\]\(([^)]+)\)$")


class ExpectedProcess:
    def __init__(self, process_id: str, module: str, document: Path,
                 operations: list[str]):
        self.process_id = process_id
        self.module = module
        self.document = document
        self.operations = operations


def parse_whitelist(readme: Path) -> list[ExpectedProcess]:
    processes: list[ExpectedProcess] = []
    for line_number, line in enumerate(
            readme.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.startswith("|") or "](" not in line:
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 3:
            raise SystemExit(
                f"{readme}:{line_number}: malformed process row, expected "
                f"3 cells")
        match = DOC_LINK.match(cells[0])
        if not match:
            raise SystemExit(
                f"{readme}:{line_number}: first cell is not a markdown link "
                f"to the process document")
        process_id, link = match.group(1), match.group(2)
        document = (readme.parent / link).resolve()
        operations = [item.strip() for item in cells[2].split(",")
                      if item.strip()]
        processes.append(ExpectedProcess(process_id, cells[1], document,
                                         operations))
    return processes


def check_whitelist(processes: list[ExpectedProcess],
                    errors: list[str]) -> None:
    if len(processes) != EXPECTED_PROCESSES:
        errors.append(
            f"process whitelist has {len(processes)} rows, expected "
            f"{EXPECTED_PROCESSES} (docs/engineering/rebuild/processes/"
            f"README.md is the coverage source)")
    operation_count = sum(len(process.operations) for process in processes)
    if operation_count != EXPECTED_OPERATIONS:
        errors.append(
            f"process whitelist has {operation_count} typed operations, "
            f"expected {EXPECTED_OPERATIONS}")
    seen_processes: dict[str, ExpectedProcess] = {}
    seen_operations: dict[str, str] = {}
    for process in processes:
        if process.process_id in seen_processes:
            errors.append(
                f"process document table lists process "
                f"'{process.process_id}' twice")
            continue
        seen_processes[process.process_id] = process
        if not process.document.is_file():
            errors.append(
                f"process '{process.process_id}': document "
                f"'{process.document}' does not exist")
        for operation in process.operations:
            if operation in seen_operations:
                errors.append(
                    f"operation '{operation}' is listed under both "
                    f"'{seen_operations[operation]}' and "
                    f"'{process.process_id}'")
            else:
                seen_operations[operation] = process.process_id


def load_manifest(path: Path) -> list[dict]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise SystemExit(
            f"manifest not found: {path}; build acsd-runtime-manifest and run "
            f"tools/generate_process_manifest.py first")
    except json.JSONDecodeError as error:
        raise SystemExit(f"manifest is not valid JSON: {error}")
    if not isinstance(manifest, dict):
        raise SystemExit("manifest root is not an object")
    if manifest.get("protocol") != PROTOCOL:
        raise SystemExit(
            f"manifest protocol {manifest.get('protocol')!r} != {PROTOCOL!r}; "
            f"the wire/registry product format is current-only")
    processes = manifest.get("processes")
    if not isinstance(processes, list):
        raise SystemExit("manifest has no processes array")
    return processes


def canonical_document(value: str, source_root: Path) -> Path:
    path = Path(value.replace("\\", "/"))
    if not path.is_absolute():
        path = source_root / path
    return path.resolve()


def check_manifest(processes: list[ExpectedProcess], manifest: list[dict],
                   source_root: Path, errors: list[str]) -> None:
    expected = {process.process_id: process for process in processes}
    expected_operation_owner: dict[str, str] = {}
    for process in processes:
        for operation in process.operations:
            expected_operation_owner[operation] = process.process_id

    seen_processes: dict[str, dict] = {}
    seen_operations: dict[tuple[str, str], None] = {}
    probe_owner: dict[str, str] = {}

    for entry in manifest:
        if not isinstance(entry, dict):
            errors.append("manifest contains a non-object process entry")
            continue
        process_id = entry.get("process")
        if not isinstance(process_id, str) or not process_id:
            errors.append("manifest process entry has no process id")
            continue
        if process_id in seen_processes:
            errors.append(
                f"manifest registers process '{process_id}' twice")
            continue
        seen_processes[process_id] = entry

        module = entry.get("module")
        semantic_revision = entry.get("semantic_revision")
        document = entry.get("document")
        dependencies = entry.get("dependencies", [])
        probes = entry.get("probes", [])
        operations = entry.get("operations", [])

        if not isinstance(module, str) or not module:
            errors.append(f"process '{process_id}': empty module")
        if not isinstance(semantic_revision, int) or semantic_revision < 1:
            errors.append(
                f"process '{process_id}': semantic_revision must be >= 1")
        if not isinstance(document, str) or not document.strip():
            errors.append(
                f"process '{process_id}': empty document path; a registered "
                f"process must name its unique document")
        if not isinstance(dependencies, list) or any(
                not isinstance(dependency, str) or not dependency
                for dependency in dependencies):
            errors.append(
                f"process '{process_id}': malformed dependencies array")
        if not isinstance(probes, list) or not probes:
            errors.append(
                f"process '{process_id}': missing probe owner; at least one "
                f"probe declaration is required")
        if not isinstance(operations, list):
            errors.append(f"process '{process_id}': missing operations array")
            operations = []

        # Probe ownership is global: a probe id has exactly one process.
        for probe in probes if isinstance(probes, list) else []:
            if not isinstance(probe, dict):
                errors.append(f"process '{process_id}': malformed probe")
                continue
            probe_id = probe.get("id")
            if not isinstance(probe_id, str) or not probe_id:
                errors.append(f"process '{process_id}': probe without id")
                continue
            if not isinstance(probe.get("kind"), str) or not probe["kind"]:
                errors.append(
                    f"process '{process_id}': probe '{probe_id}' has no kind")
            owner = probe_owner.get(probe_id)
            if owner is not None:
                errors.append(
                    f"probe '{probe_id}' has two owners: '{owner}' and "
                    f"'{process_id}'")
            else:
                probe_owner[probe_id] = process_id

        operation_names: dict[str, dict] = {}
        for operation in operations:
            if not isinstance(operation, dict):
                errors.append(
                    f"process '{process_id}': malformed operation entry")
                continue
            operation_id = operation.get("operation")
            if not isinstance(operation_id, str) or not operation_id:
                errors.append(
                    f"process '{process_id}': operation without id")
                continue
            key = (process_id, operation_id)
            if key in seen_operations:
                errors.append(
                    f"process '{process_id}': operation '{operation_id}' "
                    f"registered twice")
            seen_operations[key] = None
            operation_names[operation_id] = operation
            for direction in ("input_type", "output_type"):
                type_id = operation.get(direction)
                if (not isinstance(type_id, dict) or
                        not isinstance(type_id.get("schema"), str) or
                        not type_id["schema"]):
                    errors.append(
                        f"process '{process_id}' operation '{operation_id}': "
                        f"{direction} has no schema id")
                elif (not isinstance(type_id.get("revision"), int) or
                      type_id["revision"] < 1):
                    errors.append(
                        f"process '{process_id}' operation '{operation_id}': "
                        f"{direction} revision must be >= 1")
            backends = operation.get("backends")
            if backends is not None and (
                    not isinstance(backends, list) or
                    any(not isinstance(backend, str) or backend != "cpu"
                        for backend in backends)):
                errors.append(
                    f"process '{process_id}' operation '{operation_id}': "
                    f"only the CPU backend may be reported implemented")

        # Process level comparison.
        wanted = expected.get(process_id)
        if wanted is None:
            errors.append(
                f"extra process '{process_id}' is registered but is not in the "
                f"process whitelist")
            continue
        if module != wanted.module:
            errors.append(
                f"process '{process_id}': module '{module}' does not match "
                f"the whitelist module '{wanted.module}'")
        expected_document = canonical_document(str(wanted.document),
                                               source_root)
        if isinstance(document, str) and document:
            if canonical_document(document, source_root) != expected_document:
                errors.append(
                    f"process '{process_id}': document '{document}' does not "
                    f"match the whitelist document "
                    f"'{wanted.document.relative_to(source_root) if wanted.document.is_relative_to(source_root) else wanted.document}'")
            elif not Path(document).is_file() and not (
                    source_root / document).is_file():
                errors.append(
                    f"process '{process_id}': document '{document}' does not "
                    f"exist under '{source_root}'")

        # Operation level comparison: missing, extra and wrong attribution.
        wanted_operations = set(wanted.operations)
        registered = set(operation_names)
        for operation in sorted(wanted_operations - registered):
            errors.append(
                f"missing operation '{operation}' in process "
                f"'{process_id}' (declared by the process whitelist)")
        for operation in sorted(registered - wanted_operations):
            other = expected_operation_owner.get(operation)
            if other is not None:
                errors.append(
                    f"operation '{operation}' belongs to process '{other}' "
                    f"but is registered under '{process_id}'")
            else:
                errors.append(
                    f"extra operation '{operation}' is registered under "
                    f"'{process_id}' but is not in the process whitelist")

    for process_id, wanted in sorted(expected.items()):
        if process_id in seen_processes:
            continue
        errors.append(
            f"missing process '{process_id}' ({len(wanted.operations)} "
            f"operations) declared by the process whitelist")

    # Dependency names in the manifest must still be legal modules, services,
    # or registered process ids.  The registry freeze enforces this; the
    # coverage check re-reads the product so a tampered manifest is caught.
    registered_processes = set(seen_processes)
    for process_id, entry in seen_processes.items():
        for dependency in entry.get("dependencies", []) or []:
            if not isinstance(dependency, str):
                continue
            if (dependency in LEGAL_MODULES or dependency in registered_processes
                    or dependency.startswith("service.")):
                continue
            errors.append(
                f"process '{process_id}': unknown dependency '{dependency}'")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest",
                        default=str(root / "build" / "process-manifest.json"))
    parser.add_argument("--source-root", default=str(root))
    parser.add_argument("--readme",
                        default=str(root / "docs/engineering/rebuild/"
                                    "processes/README.md"))
    args = parser.parse_args()

    source_root = Path(args.source_root).resolve()
    readme = Path(args.readme).resolve()
    manifest_path = Path(args.manifest).resolve()

    if not readme.is_file():
        raise SystemExit(f"process whitelist not found: {readme}")
    whitelist = parse_whitelist(readme)
    manifest = load_manifest(manifest_path)

    errors: list[str] = []
    check_whitelist(whitelist, errors)
    check_manifest(whitelist, manifest, source_root, errors)

    if errors:
        print("process coverage FAIL")
        for error in errors:
            print(f"  - {error}")
        total = sum(len(process.operations) for process in whitelist)
        print(f"{len(errors)} error(s); manifest is {manifest_path}")
        return 1
    total = sum(len(process.operations) for process in whitelist)
    print(f"process coverage PASS: {len(whitelist)} processes, {total} "
          f"operations match {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
