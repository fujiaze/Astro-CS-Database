"""Helpers for the acsd-contract process route.

The contract driver is the only route that can exercise the public protocol
end to end (the process boundary, the object store and the wire format).  The
helpers here only move fields: the requests are built from the frozen inputs
of a case and the responses are compared against the independent truth.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO_ROOT = Path(__file__).resolve().parents[2]

# The candidate locations of the built contract driver, newest build first.
CANDIDATE_BINARIES = [
    "build/science/acsd-contract",
    "build/linux-sci-s07/acsd-contract",
    "build/linux-dev/acsd-contract",
    "build/linux-control/acsd-contract",
    "build/stats/acsd-contract",
]


def contract_binary() -> Optional[Path]:
    """The contract driver of the checkout, if it has been built."""
    override = os.environ.get("ACSD_CONTRACT")
    if override:
        path = Path(override)
        return path if path.is_file() else None
    for candidate in CANDIDATE_BINARIES:
        path = REPO_ROOT / candidate
        if path.is_file() and os.access(path, os.X_OK):
            return path
    return None


def run_contract(request: Dict[str, Any], work_dir: Path, timeout: float = 600.0) -> Dict[str, Any]:
    """Run one contract request and return the parsed response.

    The returned dictionary always carries the keys ``status``, ``code``,
    ``reason`` and ``response``.  A missing driver is reported as
    ``pending_implementation`` instead of an error.
    """
    binary = contract_binary()
    if binary is None:
        return {"status": "pending_implementation", "code": "unavailable", "reason":
                "the acsd-contract driver has not been built in this checkout",
                "response": None}
    work_dir.mkdir(parents=True, exist_ok=True)
    request_path = work_dir / "request.json"
    response_path = work_dir / "response.json"
    request_path.write_text(json.dumps(request, indent=1) + "\n", encoding="utf-8")
    if response_path.exists():
        response_path.unlink()
    completed = subprocess.run([str(binary), "--request", str(request_path),
                                 "--response", str(response_path)],
                                capture_output=True, text=True, timeout=timeout, cwd=str(REPO_ROOT))
    if not response_path.is_file():
        return {"status": "error", "code": "io_error",
                "reason": (completed.stderr or completed.stdout)[-800:],
                "exit_code": completed.returncode, "response": None}
    try:
        response = json.loads(response_path.read_text(encoding="utf-8"))
    except Exception as exc:  # pragma: no cover - a malformed response is a defect
        return {"status": "error", "code": "io_error", "reason": str(exc), "response": None}
    result = response.get("result", {}) or {}
    return {"status": "ok", "code": result.get("code", "unknown"),
            "reason": "", "response": response, "exit_code": completed.returncode,
            "stderr": completed.stderr[-800:]}


def object_ref(response: Dict[str, Any], type_name: str) -> Optional[str]:
    """The reference of the first object of the given type in a response."""
    objects = (response or {}).get("objects", {}) or {}
    for key in objects:
        if key.startswith(type_name + ":"):
            return {"$ref": key}
    return None


def object_refs(response: Dict[str, Any], type_name: str) -> List[str]:
    return [key for key in ((response or {}).get("objects", {}) or {})
            if key.startswith(type_name + ":")]


def result_value(response: Dict[str, Any]) -> Any:
    return ((response or {}).get("result", {}) or {}).get("value")


def result_code(response: Dict[str, Any]) -> str:
    return ((response or {}).get("result", {}) or {}).get("code", "unknown")


def result_issues(response: Dict[str, Any]) -> List[Dict[str, Any]]:
    assessment = ((response or {}).get("result", {}) or {}).get("assessment", {}) or {}
    return assessment.get("issues", []) or []


def result_usage(response: Dict[str, Any]) -> Dict[str, Any]:
    return ((response or {}).get("result", {}) or {}).get("usage", {}) or {}


def base_request(operation: str, request_id: str, output_root: str) -> Dict[str, Any]:
    """The common envelope of a contract request."""
    return {
        "protocol": "acsd.contract",
        "major": 1,
        "request_id": request_id,
        "operation": operation,
        "input_objects": {},
        "arguments": {},
        "request": {},
        "resources": {"bytes": 67108864, "max_depth": 32, "max_precision_bits": 256,
                      "scratch_directory": ".work", "spill_bytes": 0, "threads": 1,
                      "work": 1000000},
        "output_root": output_root,
    }


def clean_work_dir(path: Path) -> None:
    if path.exists():
        shutil.rmtree(path, ignore_errors=True)
