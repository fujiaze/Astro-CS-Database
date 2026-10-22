"""Process-drivers for the implementation under test.

Two invocation routes are supported, exactly as required by
``docs/engineering/rebuild/VERIFICATION.md``:

``contract``
    the ``acsd-contract`` executable is started as an independent process and
    fed a frozen ``request.json``; the driver only moves fields and arrays,
    it performs no science.

``native``
    a small C++ program that only includes the public
    ``<acsd/contracts.hpp>`` header and calls the public C++ API in-process.

Both routes report honestly when the executable does not exist yet: the
result is ``pending_implementation`` and never a fabricated observation.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .vcommon import REPO_ROOT, VALIDATION_ROOT, sha256_file

# Candidate locations for the contract driver.  The build tree layout is not
# fixed by the specification, so several plausible prefixes are probed and the
# result is recorded in the manifest.
CONTRACT_BINARY_NAMES = ("acsd-contract", "acsd_contract")
BUILD_PREFIXES = (
    "build/dev",
    "build/linux-sci",
    "build/linux-dev",
    "build/linux-control",
    "build/asan",
)


@dataclass
class DriverRun:
    """Outcome of one attempt to call the implementation under test."""

    route: str
    status: str                      # "ok" | "pending_implementation" | "error"
    reason: str = ""
    command: list[str] = field(default_factory=list)
    exit_code: int | None = None
    stdout: str = ""
    stderr: str = ""
    wall_seconds: float = 0.0
    peak_rss_bytes: int | None = None
    response: Any = None
    missing_entry_points: list[str] = field(default_factory=list)
    artifacts: dict[str, str] = field(default_factory=dict)

    def as_dict(self) -> dict[str, Any]:
        return {
            "route": self.route,
            "status": self.status,
            "reason": self.reason,
            "command": self.command,
            "exit_code": self.exit_code,
            "stdout": self.stdout,
            "stderr": self.stderr,
            "wall_seconds": self.wall_seconds,
            "peak_rss_bytes": self.peak_rss_bytes,
            "response": self.response,
            "missing_entry_points": self.missing_entry_points,
            "artifacts": self.artifacts,
        }


def _find_binary(names: tuple[str, ...], extra_paths: tuple[str, ...] = ()) -> Path | None:
    for name in names:
        found = shutil.which(name)
        if found:
            return Path(found)
    for prefix in BUILD_PREFIXES:
        base = REPO_ROOT / prefix
        if not base.is_dir():
            continue
        for name in names:
            candidate = base / name
            if candidate.is_file() and os.access(candidate, os.X_OK):
                return candidate
    for extra in extra_paths:
        candidate = Path(extra)
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def contract_binary() -> Path | None:
    return _find_binary(CONTRACT_BINARY_NAMES)


def _peak_rss(cmd: list[str], cwd: Path | None, timeout: float) -> tuple[int | None, float,
                                                                       subprocess.CompletedProcess]:
    """Run a command and measure its peak resident set size (Linux, KiB)."""
    start = time.monotonic()
    proc = subprocess.Popen(cmd, cwd=str(cwd) if cwd else None, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, text=True)
    try:
        out, err = proc.communicate(timeout=timeout)
        code = proc.returncode
    except subprocess.TimeoutExpired:
        proc.kill()
        out, err = proc.communicate()
        code = -9
    wall = time.monotonic() - start
    peak = None
    try:
        import resource

        usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        peak = int(usage.ru_maxrss) * 1024
    except Exception:  # pragma: no cover
        peak = None
    return peak, wall, subprocess.CompletedProcess(cmd, code, out, err)


def run_contract(request_path: Path, response_path: Path, timeout: float = 600.0,
                 extra_args: list[str] | None = None) -> DriverRun:
    """Drive the public contract driver as an independent process."""
    binary = contract_binary()
    if binary is None:
        return DriverRun(
            route="contract",
            status="pending_implementation",
            reason=("the acsd-contract driver executable is not built in this checkout; "
                    "the contract protocol is declared in docs/engineering/rebuild/API.md "
                    "but the driver source apps/contract/ does not exist yet"),
            missing_entry_points=["acsd-contract"],
        )
    response_path.parent.mkdir(parents=True, exist_ok=True)
    cmd = [str(binary), "--request", str(request_path), "--response", str(response_path)]
    if extra_args:
        cmd.extend(extra_args)
    peak, wall, completed = _peak_rss(cmd, None, timeout)
    run = DriverRun(route="contract", status="error", command=cmd, exit_code=completed.returncode,
                    stdout=completed.stdout, stderr=completed.stderr, wall_seconds=wall,
                    peak_rss_bytes=peak)
    if response_path.is_file():
        try:
            run.response = json.loads(response_path.read_text(encoding="utf-8"))
            run.status = "ok"
        except Exception as exc:  # pragma: no cover
            run.reason = f"response file is not valid JSON: {exc}"
    else:
        run.reason = "the driver produced no response file"
    if run.status == "ok":
        code = _response_code(run.response)
        if code in {"unsupported", "unavailable"}:
            run.status = "pending_implementation"
            run.reason = f"the driver answered {code} for this operation"
    run.artifacts = {"response_file": str(response_path),
                     "response_sha256": sha256_file(response_path) if response_path.is_file() else ""}
    return run


def _response_code(response: Any) -> str | None:
    if isinstance(response, dict):
        result = response.get("result")
        if isinstance(result, dict) and isinstance(result.get("code"), str):
            return result["code"]
    return None


def run_native(binary: Path | None, argv: list[str], cwd: Path | None = None,
               timeout: float = 600.0) -> DriverRun:
    """Run an in-memory API driver program that links the science library."""
    if binary is None or not Path(binary).is_file():
        return DriverRun(
            route="native",
            status="pending_implementation",
            reason=("the native in-memory API driver is not built; build it with "
                    "validation/lib/build_native.sh"),
            missing_entry_points=["native-api-driver"],
        )
    cmd = [str(binary), *argv]
    peak, wall, completed = _peak_rss(cmd, cwd, timeout)
    run = DriverRun(route="native", status="error", command=cmd, exit_code=completed.returncode,
                    stdout=completed.stdout, stderr=completed.stderr, wall_seconds=wall,
                    peak_rss_bytes=peak)
    if completed.returncode == 0:
        run.status = "ok"
    else:
        run.reason = f"driver exited with code {completed.returncode}"
    return run


def native_binary(name: str) -> Path | None:
    """Locate a compiled native driver inside the validation build directory."""
    build = VALIDATION_ROOT / "build"
    for candidate in (build / name, build / f"{name}.exe"):
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    return None


def binary_identity(path: Path | None) -> dict[str, str]:
    if path is None or not Path(path).is_file():
        return {"path": "", "sha256": "", "size_bytes": "0"}
    p = Path(path)
    return {"path": str(p), "sha256": sha256_file(p), "size_bytes": str(p.stat().st_size)}
