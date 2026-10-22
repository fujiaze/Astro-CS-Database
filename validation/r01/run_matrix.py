"""The R01 run matrix: the same run under different resource limits.

The matrix re-runs the published export with different thread counts and
different memory budgets and compares the results.  The comparison keys are
the content of the published payload (the bytes of the signal) and the
configuration hash of the manifest, so a difference in the numbers cannot be
hidden by a different file name.

The matrix also exercises the two failure paths that a run has to name:
a write that cannot be completed and a run that is cancelled.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import signal
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]
CASE_ROOT = Path(__file__).resolve().parent
BASE_CONFIG = REPO_ROOT / "validation" / "apps" / "evidence" / "export" / "config.json"
BASE_SCIENTIFIC = BASE_CONFIG.with_name("export.scientific.json")
WORK_DIR = CASE_ROOT / "work"


def sha256_file(path: Path) -> Optional[str]:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def product_keys(output: Path) -> Dict[str, Any]:
    """The comparison keys of a published product."""
    keys: Dict[str, Any] = {"signal_sha256": None, "configuration_hash": None,
                            "manifest_sha256": None, "objects": 0, "blocks": 0}
    manifest_path = output / "manifest.json"
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        keys["configuration_hash"] = manifest.get("configuration_hash")
        keys["objects"] = len(manifest.get("objects", []) or [])
        keys["blocks"] = len(manifest.get("blocks", []) or [])
    keys["manifest_sha256"] = sha256_file(manifest_path)
    keys["signal_sha256"] = sha256_file(output / "signal.fits")
    return keys


def run_cli(binary: Path, config: Path, output: Path, timeout: float = 600.0,
            cancel_after: Optional[float] = None) -> Dict[str, Any]:
    """One run of the command line; the events are parsed strictly."""
    environment = dict(os.environ)
    completed: Optional[subprocess.CompletedProcess] = None
    process = subprocess.Popen([str(binary), "export", "--json", str(config)],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                               cwd=str(config.parent), env=environment)
    if cancel_after is not None:
        time.sleep(cancel_after)
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
    stdout, stderr = process.communicate(timeout=timeout)
    completed = subprocess.CompletedProcess(process.args, process.returncode, stdout, stderr)
    events: List[Dict[str, Any]] = []
    foreign: List[str] = []
    for line in completed.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        if not line.startswith("{"):
            foreign.append(line)
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError:
            foreign.append(line)
    finished = [event for event in events if event.get("event") == "run_finished"]
    return {
        "exit_code": completed.returncode,
        "events": events,
        "foreign_stdout_lines": foreign,
        "stdout_is_pure_jsonl": not foreign,
        "summary": finished[-1].get("summary") if finished else None,
        "run_finished": finished[-1] if finished else None,
        "stderr_tail": completed.stderr[-400:],
    }


def write_config(path: Path, resources: Dict[str, Any], output: str) -> None:
    config = json.loads(BASE_CONFIG.read_text(encoding="utf-8"))
    config["resources"] = {**config.get("resources", {}), **resources}
    config["output"] = output
    path.write_text(json.dumps(config, indent=1) + "\n", encoding="utf-8")


def main() -> int:
    binary = REPO_ROOT / "build" / "science" / "acsd"
    if not binary.is_file():
        print(json.dumps({"status": "pending_implementation",
                          "reason": "the acsd command line binary has not been built"}, indent=1))
        return 0
    WORK_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(BASE_SCIENTIFIC, WORK_DIR / "export.scientific.json")

    variants = [
        {"id": "threads_1", "resources": {"threads": 1, "bytes": 1073741824}},
        {"id": "threads_4", "resources": {"threads": 4, "bytes": 1073741824}},
        {"id": "bytes_32MiB", "resources": {"threads": 1, "bytes": 33554432}},
        {"id": "bytes_256MiB", "resources": {"threads": 1, "bytes": 268435456}},
        {"id": "work_1", "resources": {"threads": 1, "bytes": 1073741824, "work": 1}},
    ]

    runs: List[Dict[str, Any]] = []
    for variant in variants:
        output = f"products/{variant['id']}"
        config = WORK_DIR / f"config_{variant['id']}.json"
        write_config(config, variant["resources"], output)
        target = WORK_DIR / output
        if target.exists():
            shutil.rmtree(target)
        result = run_cli(binary, config, WORK_DIR / output)
        keys = product_keys(WORK_DIR / output)
        runs.append({"id": variant["id"], "resources": variant["resources"],
                     "exit_code": result["exit_code"], "keys": keys,
                     "stdout_is_pure_jsonl": result["stdout_is_pure_jsonl"],
                     "summary": result["summary"],
                     "run_finished": result["run_finished"],
                     "foreign_stdout_lines": result["foreign_stdout_lines"]})

    # the comparison: the payload content must not depend on the resources
    reference = None
    comparisons = []
    for run in runs:
        if run["id"] == "work_1":
            continue
        if reference is None:
            reference = run["keys"]
            continue
        # the payload content must not depend on the resources; the
        # configuration hash differs on purpose (it carries the resources)
        same = run["keys"]["signal_sha256"] == reference["signal_sha256"]
        comparisons.append({"id": run["id"], "reference": "threads_1",
                            "identical_payload": same,
                            "signal_sha256": run["keys"]["signal_sha256"],
                            "reference_sha256": reference["signal_sha256"]})

    # the write failure: the output directory cannot be created
    write_failure: Dict[str, Any] = {}
    config = WORK_DIR / "config_write_failure.json"
    write_config(config, {"threads": 1, "bytes": 1073741824}, "products/read_only")
    blocked = WORK_DIR / "products" / "read_only"
    blocked.mkdir(parents=True, exist_ok=True)
    os.chmod(blocked, 0o500)
    try:
        result = run_cli(binary, config, blocked)
        write_failure = {"exit_code": result["exit_code"],
                         "run_finished": result["run_finished"],
                         "summary": result["summary"],
                         "named_failure": bool(result["run_finished"])
                         and result["run_finished"].get("code") != "ok",
                         "published": (result["summary"] or {}).get("published", 0)}
    finally:
        os.chmod(blocked, 0o700)

    # the cancellation: the run is interrupted
    config = WORK_DIR / "config_cancel.json"
    write_config(config, {"threads": 1, "bytes": 1073741824}, "products/cancelled")
    cancelled = run_cli(binary, config, WORK_DIR / "products" / "cancelled", cancel_after=0.05)
    cancellation = {"exit_code": cancelled["exit_code"],
                    "run_finished": cancelled["run_finished"],
                    "summary": cancelled["summary"]}

    report = {
        "case": "R01",
        "route": "the command line under a matrix of resource limits",
        "binary": str(binary.relative_to(REPO_ROOT)),
        "runs": runs,
        "comparisons": comparisons,
        "write_failure": write_failure,
        "cancellation": cancellation,
        "cache_option": "the configuration schema has no cache switch; the cache state "
                        "cannot be varied from outside",
    }
    (CASE_ROOT / "observed").mkdir(parents=True, exist_ok=True)
    (CASE_ROOT / "observed" / "run_matrix.json").write_text(json.dumps(report, indent=1) + "\n",
                                                             encoding="utf-8")
    print(json.dumps({"runs": [{"id": run["id"], "exit_code": run["exit_code"],
                                "signal_sha256": run["keys"]["signal_sha256"]} for run in runs],
                      "comparisons": comparisons,
                      "write_failure": write_failure.get("exit_code"),
                      "cancellation": cancellation.get("exit_code")}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
