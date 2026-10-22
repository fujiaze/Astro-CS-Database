#!/usr/bin/env python3
"""Resource and throughput measurement for the ACSD v2 commands.

The harness runs the real CLI on a locked configuration and records, for each
run: wall and CPU time, peak resident set size of the child process, the bytes
read and written, the work units consumed, the spill volume and the number of
non-zeros of the produced operator, together with the exact command line, the
input hashes and the machine identity.

The numbers are only meaningful together with the input, the tolerance, the
arithmetic mode and the source revision; all of them are written into the
result file.  A run that fails is recorded with its exit code and is never
replaced by a successful retry.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import resource
import shutil
import subprocess
import sys
import time
from pathlib import Path


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1 << 20), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def tree_sha256(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(p for p in root.rglob("*") if p.is_file()):
        digest.update(str(path.relative_to(root)).encode("utf-8"))
        digest.update(file_sha256(path).encode("ascii"))
    return "sha256:" + digest.hexdigest()


def machine_identity() -> dict:
    info = {
        "platform": platform.platform(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "python": platform.python_version(),
        "cpu_count": os.cpu_count(),
    }
    try:
        model = Path("/proc/cpuinfo").read_text(encoding="utf-8")
        for line in model.splitlines():
            if line.lower().startswith("model name"):
                info["cpu_model"] = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    try:
        mem = Path("/proc/meminfo").read_text(encoding="utf-8")
        for line in mem.splitlines():
            if line.startswith("MemTotal"):
                info["mem_total"] = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    return info


def source_identity(root: Path) -> str:
    try:
        revision = subprocess.run(
            ["git", "-C", str(root), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=False).stdout.strip()
        dirty = subprocess.run(
            ["git", "-C", str(root), "status", "--porcelain"],
            capture_output=True, text=True, check=False).stdout.strip()
    except OSError:
        revision, dirty = "", ""
    return {"revision": revision, "dirty": bool(dirty)}


def measure(command: list[str], cwd: Path, env: dict | None = None) -> dict:
    before = resource.getrusage(resource.RUSAGE_CHILDREN)
    start = time.perf_counter()
    completed = subprocess.run(command, cwd=str(cwd), env=env, capture_output=True, text=True)
    wall = time.perf_counter() - start
    after = resource.getrusage(resource.RUSAGE_CHILDREN)
    return {
        "command": command,
        "exit_code": completed.returncode,
        "wall_seconds": wall,
        "user_cpu_seconds": after.ru_utime - before.ru_utime,
        "system_cpu_seconds": after.ru_stime - before.ru_stime,
        "peak_rss_bytes": after.ru_maxrss * 1024,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
    }


def parse_events(text: str) -> list[dict]:
    events = []
    for line in text.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return events


def summarise_events(events: list[dict]) -> dict:
    summary = {
        "event_count": len(events),
        "events_by_kind": {},
        "terminal": None,
        "issues": [],
        "published": [],
    }
    for event in events:
        kind = event.get("event", "unknown")
        summary["events_by_kind"][kind] = summary["events_by_kind"].get(kind, 0) + 1
        if kind == "run_finished":
            summary["terminal"] = event
        if kind == "issue":
            summary["issues"].append(event)
        if kind == "product_published":
            summary["published"].append(event)
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True, type=Path, help="CLI configuration file")
    parser.add_argument("--binary", default="acsd", help="path to the scientific CLI")
    parser.add_argument("--output", required=True, type=Path, help="result file to write")
    parser.add_argument("--root", default=".", type=Path, help="repository root (source identity)")
    parser.add_argument("--label", default="", help="label for this measurement")
    parser.add_argument("--events", default="", help="optional file with captured JSONL events")
    args = parser.parse_args()

    config = args.config.resolve()
    binary = shutil.which(args.binary) or str(Path(args.binary).resolve())
    if not Path(binary).exists():
        print(f"the CLI binary {args.binary} was not found", file=sys.stderr)
        return 2

    result = {
        "format": "acsd.performance",
        "major": 1,
        "minor": 0,
        "label": args.label,
        "machine": machine_identity(),
        "source": source_identity(args.root.resolve()),
        "config": str(config),
        "config_hash": file_sha256(config),
    }
    inputs = []
    for key, value in (json.loads(config.read_text(encoding="utf-8")).get("inputs") or {}).items():
        if isinstance(value, str) and Path(value).exists():
            inputs.append({"key": key, "path": value, "hash": file_sha256(Path(value))})
        elif isinstance(value, list):
            for item in value:
                if isinstance(item, str) and Path(item).exists():
                    inputs.append({"key": key, "path": item, "hash": file_sha256(Path(item))})
    result["inputs"] = inputs
    result["input_tree_hash"] = tree_sha256(config.parent)

    run = measure([binary, "normalize", "--json", str(config)], cwd=config.parent)
    result["run"] = {k: v for k, v in run.items() if k not in ("stdout", "stderr")}
    events = parse_events(run["stdout"])
    if args.events and Path(args.events).exists():
        events = events or parse_events(Path(args.events).read_text(encoding="utf-8"))
    result["events"] = summarise_events(events)
    result["stdout_head"] = run["stdout"][:4000]
    result["stderr_head"] = run["stderr"][:4000]

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({k: result[k] for k in ("label", "run", "events")}, indent=2, ensure_ascii=False))
    return 0 if result["run"]["exit_code"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
