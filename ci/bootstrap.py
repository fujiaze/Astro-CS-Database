#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci/bootstrap.py —— V8-CI-007 hosted runner 工具链 bootstrap 门禁。

对 ``ci/toolchain.policy.json`` 中 ``linux_hosted`` / ``windows_hosted``
节的每一项要求逐项探测当前 host 并记录
``{tool, required, observed, ok}``；任一项缺失或版本不足：

- stderr 输出结构化 JSON（``verdict=FAIL`` + 逐项失败清单，无 traceback）；
- exit 2（受控失败）。

``--json`` 在 stdout 输出完整报告（三态均可机读）。
fatduck 节在本脚本不使用（V8-CI-008 域）：--platform 仅接受
linux|windows。

Exit code：0=全部满足；2=受控失败（缺失/版本不足/policy 非法）。
所有外部探测命令带 timeout（30s）；不做任何写操作。
"""
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DEFAULT_POLICY = Path(__file__).resolve().parent / "toolchain.policy.json"

SCHEMA_VERSION = 1
TASK_ID = "V8-CI-007"
PROBE_TIMEOUT_S = 30

_EXIT_OK = 0
_EXIT_CONTROLLED_FAIL = 2

_CMAKE_MIN = (3, 31, 12)
_LINUX_ARCH_OK = {"x86_64", "amd64"}
_WIN_ARCH_OK = {"amd64", "x86_64"}

_VSWHERE_REL = os.path.join("Microsoft Visual Studio", "Installer", "vswhere.exe")


def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# -------------------------------------------------------------------- 探测 ----

def probe_command_version(argv0: str, args: tuple[str, ...] = ("--version",)) -> tuple[bool, str | None]:
    """探测 <tool> 在 PATH 且 ``--version`` 可执行，返回 (present, 首行文本)。"""
    exe = shutil.which(argv0)
    if exe is None:
        return False, None
    try:
        res = subprocess.run([exe, *args], capture_output=True, text=True,
                             timeout=PROBE_TIMEOUT_S)
    except (subprocess.TimeoutExpired, OSError):
        return True, None  # 可执行但探测失败：存在性成立，版本未知
    text = (res.stdout or res.stderr or "").strip().splitlines()
    return res.returncode == 0, (text[0].strip() if text else None)


def _major_of(text: str | None) -> int | None:
    if not text:
        return None
    m = re.search(r"(\d+)\.\d+\.\d+", text)
    return int(m.group(1)) if m else None


def _version_tuple(text: str | None) -> tuple[int, ...] | None:
    if not text:
        return None
    m = re.search(r"(\d+(?:\.\d+)+)", text)
    return tuple(int(x) for x in m.group(1).split(".")) if m else None


def probe_runner() -> dict:
    """GitHub hosted runner 证据（GITHUB_ACTIONS/ImageOS/RUNNER_OS）。"""
    hosted = os.environ.get("GITHUB_ACTIONS", "") == "true"
    return {
        "hosted": hosted,
        "image_os": os.environ.get("ImageOS"),
        "runner_os": os.environ.get("RUNNER_OS"),
        "runner_name": os.environ.get("RUNNER_NAME"),
        "os_release_id": None,
        "os_release_version": None,
    }


def _read_os_release() -> dict:
    info: dict[str, str] = {}
    path = Path("/etc/os-release")
    if path.is_file():
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            key, _, value = line.partition("=")
            info[key.strip()] = value.strip().strip('"')
    return info


def probe_hosted_linux_runner() -> dict:
    obs = probe_runner()
    info = _read_os_release()
    obs["os_release_id"] = info.get("ID")
    obs["os_release_version"] = info.get("VERSION_ID")
    return obs


def probe_vs_instance() -> tuple[bool, str | None, str | None]:
    """(found, installationVersion, toolset_hint)；vswhere 探测 VS 17 实例。"""
    if platform.system() != "Windows":
        return False, None, "not-windows-host"
    prog_x86 = os.environ.get("ProgramFiles(x86)")
    vswhere = Path(prog_x86) / _VSWHERE_REL if prog_x86 else None
    if not vswhere or not vswhere.is_file():
        return False, None, None
    try:
        res = subprocess.run(
            [str(vswhere), "-latest", "-products", "*",
             "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
             "-property", "installationVersion"],
            capture_output=True, text=True, timeout=PROBE_TIMEOUT_S)
    except (subprocess.TimeoutExpired, OSError):
        return True, None, None
    version = res.stdout.strip().splitlines()[0] if res.stdout.strip() else None
    toolset = None
    if version and version.startswith("17"):
        toolset = "v143"
    return True, version, toolset


def _item(tool: str, required: str, observed, ok: bool,
          detail: str | None = None) -> dict:
    entry = {"tool": tool, "required": required, "observed": observed, "ok": ok}
    if detail:
        entry["detail"] = detail
    return entry


# ---------------------------------------------------------------- 节校验器 ----

def check_linux(policy_section: dict) -> list[dict]:
    """linux_hosted 节逐项探测（runner/architecture/gcc-14/clang-19/cmake/Ninja）。"""
    items: list[dict] = []
    runner = probe_hosted_linux_runner()
    runner_ok = (runner["image_os"] or "").lower().startswith("ubuntu24") \
        or (runner["os_release_id"] == "ubuntu"
            and (runner["os_release_version"] or "").startswith("24.04"))
    items.append(_item(
        "runner", policy_section.get("runner", "ubuntu-24.04"),
        runner, runner_ok,
        None if runner_ok else
        "非 GitHub hosted ubuntu-24.04 runner（预期在 hosted 上复验）"))

    arch = platform.machine().lower()
    arch_required = policy_section.get("architecture", "x86_64")
    items.append(_item("architecture", arch_required, platform.machine(),
                       arch in _LINUX_ARCH_OK,
                       None if arch in _LINUX_ARCH_OK else
                       f"观测 {platform.machine()}，非 x86_64/amd64"))

    required_primary = policy_section.get("primary_compiler", "gcc-14")
    present, text = probe_command_version(required_primary)
    major = _major_of(text)
    items.append(_item(
        required_primary, required_primary,
        {"present": present, "version_line": text},
        present and major is not None and major >= int(re.sub(r"\D.*", "", required_primary) or 0),
        None if (present and major is not None) else
        f"{required_primary} 不在 PATH 或版本不可解析（hosted ubuntu-24.04 预装；"
        "本地属预期缺失路径）"))

    required_secondary = policy_section.get("secondary_compiler", "clang-19")
    present, text = probe_command_version(required_secondary)
    major = _major_of(text)
    items.append(_item(
        required_secondary, required_secondary,
        {"present": present, "version_line": text},
        present and major is not None and major >= int(re.sub(r"\D.*", "", required_secondary) or 0),
        None if (present and major is not None) else
        f"{required_secondary} 不在 PATH 或版本不可解析"))

    required_cmake = policy_section.get("cmake", "3.31.12")
    present, text = probe_command_version("cmake")
    ver = _version_tuple(text)
    items.append(_item(
        "cmake", f">={required_cmake}",
        {"present": present, "version_line": text},
        present and ver is not None and ver >= _CMAKE_MIN,
        None if (present and ver is not None) else
        f"cmake 不在 PATH 或版本 < {required_cmake}"))

    generator = policy_section.get("generator", "Ninja")
    present, text = probe_command_version("ninja")
    items.append(_item(
        generator, "Ninja present",
        {"present": present, "version_line": text}, present,
        None if present else "ninja 不在 PATH（hosted ubuntu-24.04 预装）"))
    return items


def check_windows(policy_section: dict) -> list[dict]:
    """windows_hosted 节逐项探测（runner/architecture/VS 17/v143/cmake）。"""
    items: list[dict] = []
    runner = probe_runner()
    runner_ok = (runner["image_os"] or "").lower().startswith("win22")
    items.append(_item(
        "runner", policy_section.get("runner", "windows-2022"),
        runner, runner_ok,
        None if runner_ok else
        "非 GitHub hosted windows-2022 runner（预期在 hosted 上复验）"))

    arch = platform.machine().lower()
    arch_required = policy_section.get("architecture", "x64")
    arch_ok = arch in _WIN_ARCH_OK and platform.system() == "Windows"
    items.append(_item("architecture", arch_required, platform.machine(),
                       arch_ok,
                       None if arch_ok else
                       f"观测 {platform.machine()}（windows_hosted 要求 AMD64）"))

    vs_major_required = int(policy_section.get("visual_studio_major", 17))
    found, vs_version, toolset = probe_vs_instance()
    vs_major = int(vs_version.split(".")[0]) if vs_version else None
    items.append(_item(
        "visual_studio", f"VS {vs_major_required}.x",
        {"present": found, "installation_version": vs_version},
        found and vs_major is not None and vs_major == vs_major_required,
        None if found else
        "VS 实例不可达（vswhere 缺失或非 Windows host）"))

    required_toolset = policy_section.get("platform_toolset", "v143")
    toolset_ok = bool(found and toolset == required_toolset)
    items.append(_item(
        "platform_toolset", required_toolset,
        {"vs_version": vs_version, "toolset_hint": toolset}, toolset_ok,
        None if toolset_ok else
        f"未确认 {required_toolset} 工具集（需 VS 17 + VC.Tools.x86.x64 组件）"))

    required_cmake = policy_section.get("cmake", "3.31.12")
    present, text = probe_command_version("cmake")
    ver = _version_tuple(text)
    items.append(_item(
        "cmake", f">={required_cmake}",
        {"present": present, "version_line": text},
        present and ver is not None and ver >= _CMAKE_MIN,
        None if (present and ver is not None) else
        f"cmake 不在 PATH 或版本 < {required_cmake}"))
    return items


CHECKERS = {"linux": check_linux, "windows": check_windows}


# --------------------------------------------------------------------- CLI ----

def build_report(platform_name: str, policy_path: Path,
                 items: list[dict]) -> dict:
    failures = [it["tool"] for it in items if not it["ok"]]
    return {
        "schema_version": SCHEMA_VERSION,
        "task_id": TASK_ID,
        "generated_utc": utc_iso(),
        "platform": platform_name,
        "policy": str(policy_path),
        "items": items,
        "ok": not failures,
        "failures": failures,
    }


def _structured_stderr(report: dict) -> str:
    failures = [it for it in report["items"] if not it["ok"]]
    return json.dumps({
        "verdict": "FAIL",
        "platform": report["platform"],
        "failed_tools": [
            {"tool": it["tool"], "required": it["required"],
             "observed": it["observed"], "repair": it.get("detail") or
             "在 GitHub hosted runner 上复验或补齐该工具"}
            for it in failures],
    }, ensure_ascii=False)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="ci/bootstrap.py",
        description="V8-CI-007：hosted runner 工具链 bootstrap 门禁（逐项探测 policy 节）。")
    parser.add_argument("--policy", default=str(DEFAULT_POLICY),
                        help=f"toolchain policy JSON（默认 {DEFAULT_POLICY.name}）")
    parser.add_argument("--platform", required=True, choices=("linux", "windows"),
                        help="校验平台节（fatduck 属 V8-CI-008 域，不在本脚本）")
    parser.add_argument("--json", action="store_true",
                        help="stdout 输出完整 JSON 报告（三态均可机读）")
    args = parser.parse_args(argv)

    policy_path = Path(args.policy)
    if not policy_path.is_file():
        payload = {"verdict": "FAIL", "error": f"policy 文件不存在：{policy_path}"}
        print(json.dumps(payload, ensure_ascii=False), file=sys.stderr)
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        return _EXIT_CONTROLLED_FAIL
    try:
        policy = json.loads(policy_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        payload = {"verdict": "FAIL", "error": f"policy JSON 非法：{exc}"}
        print(json.dumps(payload, ensure_ascii=False), file=sys.stderr)
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        return _EXIT_CONTROLLED_FAIL
    section = policy.get(f"{args.platform}_hosted")
    if not isinstance(section, dict):
        payload = {"verdict": "FAIL",
                   "error": f"policy 缺少 {args.platform}_hosted 节"}
        print(json.dumps(payload, ensure_ascii=False), file=sys.stderr)
        if args.json:
            print(json.dumps(payload, ensure_ascii=False, indent=2))
        return _EXIT_CONTROLLED_FAIL

    items = CHECKERS[args.platform](section)
    report = build_report(args.platform, policy_path, items)

    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=2))
    if report["ok"]:
        print(f"bootstrap: PASS ({len(items)} items, platform={args.platform})",
              file=sys.stderr)
        return _EXIT_OK
    print(_structured_stderr(report), file=sys.stderr)
    return _EXIT_CONTROLLED_FAIL


if __name__ == "__main__":
    sys.exit(main())
