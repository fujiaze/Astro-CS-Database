#!/usr/bin/env python3
"""Export the frozen registry manifest from the built runtime.

The manifest is a *build product*: its only source is the compiled module
descriptors plus the frozen registry.  This tool never derives process or
operation names from the documentation table, and there is no second
hand-written registry.

Usage:
  tools/generate_process_manifest.py [--binary PATH] [--build-dir DIR]
                                     [--output PATH] [--source-root DIR]

The C++ exporter acsd-runtime-manifest builds the same RegistryBuilder as the
production assembly, calls the generated register_builtin_types /
register_processes entry points, freezes the registry and writes
manifest_json() atomically.  When no module registration source has landed yet
the manifest is valid but contains an empty process list; the coverage check
reports exactly what is missing.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

PROTOCOL = "acsd.process/2"


def find_binary(explicit: str | None, build_dir: Path | None) -> Path:
    if explicit:
        path = Path(explicit).resolve()
        if not path.is_file():
            raise SystemExit(f"manifest exporter not found: {path}")
        return path
    candidates: list[Path] = []
    if build_dir is not None:
        candidates.extend(build_dir.glob("acsd-runtime-manifest"))
        candidates.extend(build_dir.glob("**/acsd-runtime-manifest"))
    else:
        root = Path(__file__).resolve().parents[1]
        for base in (root / "build",):
            candidates.extend(base.glob("acsd-runtime-manifest"))
            candidates.extend(base.glob("**/acsd-runtime-manifest"))
    seen: set[Path] = set()
    unique = []
    for candidate in candidates:
        resolved = candidate.resolve()
        if resolved in seen or not resolved.is_file():
            continue
        seen.add(resolved)
        unique.append(resolved)
    if not unique:
        raise SystemExit(
            "acsd-runtime-manifest is not built. Configure with "
            "-DACSD_BUILD_SCIENCE=ON and build the acsd-runtime-manifest "
            "target, or pass --binary.")
    # Newest binary wins: concurrent build directories are common.
    unique.sort(key=lambda p: p.stat().st_mtime, reverse=True)
    return unique[0]


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", default=None,
                        help="path to acsd-runtime-manifest")
    parser.add_argument("--build-dir", default=None,
                        help="directory searched for acsd-runtime-manifest")
    parser.add_argument("--output", default=str(root / "build" /
                                                "process-manifest.json"))
    parser.add_argument("--source-root", default=str(root))
    args = parser.parse_args()

    binary = find_binary(args.binary,
                         Path(args.build_dir).resolve() if args.build_dir
                         else None)
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)

    # The exporter itself commits atomically (temporary file + rename); the
    # tool writes to a sibling path so a crashed exporter cannot truncate an
    # existing manifest.
    fd, temporary = tempfile.mkstemp(prefix=".process-manifest.",
                                     suffix=".json", dir=output.parent)
    os.close(fd)
    temporary_path = Path(temporary)
    environment = dict(os.environ)
    environment["ACSD_SOURCE_ROOT"] = str(Path(args.source_root).resolve())
    try:
        completed = subprocess.run(
            [str(binary), "--output", str(temporary_path)],
            cwd=str(Path(args.source_root).resolve()),
            capture_output=True, text=True, env=environment)
        if completed.returncode != 0:
            sys.stderr.write(completed.stdout)
            sys.stderr.write(completed.stderr)
            raise SystemExit(
                f"manifest exporter failed with exit code "
                f"{completed.returncode}")
        if not temporary_path.is_file() or temporary_path.stat().st_size == 0:
            raise SystemExit("manifest exporter produced no manifest")
        try:
            manifest = json.loads(temporary_path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            raise SystemExit(f"manifest exporter wrote invalid JSON: {error}")
        if manifest.get("protocol") != PROTOCOL:
            raise SystemExit(
                f"manifest protocol mismatch: {manifest.get('protocol')!r} != "
                f"{PROTOCOL!r}")
        processes = manifest.get("processes")
        if not isinstance(processes, list):
            raise SystemExit("manifest has no processes array")
        # Validate shape enough that the coverage check cannot silently accept
        # a truncated export.
        for process in processes:
            if not process.get("process") or not process.get("module"):
                raise SystemExit("manifest process without process/module")
            if not isinstance(process.get("operations"), list):
                raise SystemExit(
                    f"manifest process {process.get('process')!r} has no "
                    f"operations array")
            for operation in process["operations"]:
                if not operation.get("operation"):
                    raise SystemExit("manifest operation without id")
                if not operation.get("input_type", {}).get("schema"):
                    raise SystemExit(
                        "manifest operation without input TypeId schema")
                if not operation.get("output_type", {}).get("schema"):
                    raise SystemExit(
                        "manifest operation without output TypeId schema")
        os.replace(temporary_path, output)
    finally:
        if temporary_path.exists():
            temporary_path.unlink()

    operations = sum(len(p.get("operations", [])) for p in processes)
    print(f"wrote {output} ({len(processes)} processes, {operations} operations)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
