"""The end-to-end run of X01: the three commands on the frozen frames.

The script runs ``acsd normalize`` / ``mosaic`` / ``export`` on the frozen
synthetic observation and records every event of the protocol.  The verdict is
the state of the run, not an expectation: a failed command is a failure of the
end-to-end path and is reported with the exact stage and reason.
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List

CASE_ROOT = Path(__file__).resolve().parent
REPO_ROOT = CASE_ROOT.parents[1]
CLI_DIR = CASE_ROOT / "input" / "cli"

sys.path.insert(0, str(CASE_ROOT))
import cli_inputs  # noqa: E402

COMMANDS = [
    ("normalize", "normalize.config.json"),
    ("mosaic", "mosaic.config.json"),
    ("export", "export.config.json"),
]


def cli_binary() -> Path | None:
    for candidate in ("build/science/acsd", "build/linux-sci-s07/acsd", "build/dev/acsd"):
        path = REPO_ROOT / candidate
        if path.is_file():
            return path
    return None


def run_command(binary: Path, command: str, config: str, timeout: float = 900.0) -> Dict[str, Any]:
    completed = subprocess.run([str(binary), command, "--json", config], capture_output=True,
                               text=True, timeout=timeout, cwd=str(CLI_DIR))
    events: List[Dict[str, Any]] = []
    for line in completed.stdout.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            events.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    frames = [event for event in events if event.get("event") == "frame_finished"]
    finished = [event for event in events if event.get("event") == "run_finished"]
    summary = finished[-1].get("summary") if finished else None
    return {
        "command": command,
        "config": config,
        "exit_code": completed.returncode,
        "events": events,
        "frame_results": [{"frame_id": event.get("frame_id"), "code": event.get("code"),
                           "message": event.get("message")} for event in frames],
        "summary": summary,
        "published": (summary or {}).get("published", 0),
        "unsolved": (summary or {}).get("unsolved", 0),
        "failed_frames": (summary or {}).get("failed", 0),
        "stderr": completed.stderr[-600:],
    }


def main() -> int:
    binary = cli_binary()
    observed_dir = CASE_ROOT / "observed"
    observed_dir.mkdir(parents=True, exist_ok=True)

    manifest = cli_inputs.write_inputs()
    if binary is None:
        result = {"status": "pending_implementation",
                  "reason": "the acsd command line binary has not been built in this checkout",
                  "commands": []}
        (observed_dir / "e2e_summary.json").write_text(json.dumps(result, indent=1) + "\n",
                                                       encoding="utf-8")
        print(json.dumps(result, indent=1))
        return 0

    results = []
    for command, config in COMMANDS:
        results.append(run_command(binary, command, config))
    (observed_dir / "e2e_events.jsonl").write_text(
        "\n".join(json.dumps(event) for result in results for event in result["events"]) + "\n",
        encoding="utf-8")

    # The stage where the path stopped and the reason of the last failure.
    stages = []
    for result in results:
        failing = [frame for frame in result["frame_results"] if frame.get("code") != "ok"]
        stages.append({
            "command": result["command"],
            "exit_code": result["exit_code"],
            "published": result["published"],
            "unsolved": result["unsolved"],
            "failed_frames": result["failed_frames"],
            "first_failure": failing[0] if failing else None,
        })

    status = "pass" if all(result["exit_code"] == 0 for result in results) else "fail"
    summary = {
        "status": status,
        "route": "the three commands of the acsd command line on the frozen frames",
        "binary": str(binary.relative_to(REPO_ROOT)),
        "inputs": manifest,
        "stages": stages,
        "commands": [{"command": result["command"], "exit_code": result["exit_code"],
                      "published": result["published"], "unsolved": result["unsolved"],
                      "stderr": result["stderr"]} for result in results],
    }
    (observed_dir / "e2e_summary.json").write_text(json.dumps(summary, indent=1) + "\n",
                                                    encoding="utf-8")
    print(json.dumps({"status": status, "stages": stages}, indent=1))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
