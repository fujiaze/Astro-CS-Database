#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AstroCS BLD-001 toolchain verifier (machine check).

Freezes the Windows x64 release toolchain per control-package
09_WINDOWS_TOOLCHAIN_LOCK.md and validates the repository's frozen
contract (packaging/schemas/preset-contract.json) against:

  * CMakePresets.json            -- the only formal Windows preset surface
  * packaging/windows/.vsconfig  -- the only VS Build Tools component list

Rules enforced here (any violation exits non-zero, i.e. FAIL fast):

  1. Contract file parses and is self-consistent (its consts are the
     single source of truth; drift anywhere fails).
  2. The formal Windows preset generator is EXACTLY "Visual Studio 17 2022"
     with architecture x64 and toolset v143 pinned to 14.44.35207.
  3. CMAKE_SYSTEM_VERSION is exactly 10.0.26100.0 and the DLL CRT policy
     (/MD /MDd) is applied; ACR is OFF on the formal path.
  4. Forbidden tokens (latest/evergreen, MinGW/MSYS, VS2026, v144/v145,
     Ninja, Win32/ARM/ARM64, /MT) never appear on the formal Windows
     surface (CMakePresets.json windows presets + packaging/windows/.vsconfig).
  5. The .vsconfig component list matches the frozen list exactly and does
     not add MFC/ATL/C++CLI/UWP/WinUI/Windows App SDK/ARM components.
  6. The Linux preset is light validation only and never overrides or
     hardcodes the Windows formal generator.

Pure stdlib (Python >= 3.10). Runs on the Linux control node AND on the
Windows build node (python 3.12.10); never invokes cmake/msvc itself.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from typing import Any, Dict, List, Optional, Tuple

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

CONTRACT_RELPATH = "packaging/schemas/preset-contract.json"
PRESETS_RELPATH = "CMakePresets.json"
VSCONFIG_RELPATH = "packaging/windows/.vsconfig"

# ---------------------------------------------------------------- helpers --

def _load_json(path: str) -> Any:
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def _norm(path: str) -> str:
    return path.replace("\\", "/")


# --------------------------------------------------------------- checks ----

class Verifier:
    def __init__(self, root: str) -> None:
        self.root = root
        self.failures: List[str] = []
        self.notes: List[str] = []

    def fail(self, msg: str) -> None:
        self.failures.append(msg)

    def note(self, msg: str) -> None:
        self.notes.append(msg)

    def check_contract_schema(self) -> Dict[str, Any]:
        """Load the frozen contract and sanity-check it (single source of truth)."""
        path = os.path.join(self.root, CONTRACT_RELPATH)
        try:
            contract = _load_json(path)
        except Exception as exc:  # pragma: no cover - defensive
            self.fail(f"contract file unreadable/invalid JSON: {exc}")
            return {}
        if not isinstance(contract, dict):
            self.fail("contract root must be an object")
            return {}
        if contract.get("contract_version") != 1:
            self.fail("contract_version must be 1")
        w = contract.get("windows")
        if not isinstance(w, dict):
            self.fail("contract.windows missing")
            return contract
        required_consts = [
            "formal_generator", "architecture", "toolset", "toolset_version",
            "compiler_family", "sdk_version", "cmake_version",
            "vs_buildtools_version", "vs_installationVersion",
            "crt_debug", "crt_release",
        ]
        for key in required_consts:
            if key not in w:
                self.fail(f"contract.windows.{key} missing")
        if "sdk_servicing_bundle" not in w:
            self.fail("contract.windows.sdk_servicing_bundle missing (frozen install bundle)")
        if "cpp_standard" not in w:
            self.fail("contract.windows.cpp_standard missing")
        fb = contract.get("forbidden")
        if not isinstance(fb, dict):
            self.fail("contract.forbidden missing")
        else:
            for group in ("evergreen", "mingw_msys", "vs2026", "future_toolset",
                          "ninja_generator", "non_x64_arch", "static_crt"):
                toks = fb.get(group)
                if not isinstance(toks, list) or not toks:
                    self.fail(f"contract.forbidden.{group} must be a non-empty array")
        pn = contract.get("preset_names")
        if not isinstance(pn, dict) or pn.get("windows_formal") != "win-msvc-17.14.39-x64" \
                or pn.get("linux_light") != "linux-control":
            self.fail("contract.preset_names mismatch (win-msvc-17.14.39-x64 / linux-control)")
        ll = contract.get("linux_light")
        if not isinstance(ll, dict):
            self.fail("contract.linux_light missing")
        return contract

    # -- formal preset extraction -----------------------------------------
    def find_configure_preset(self, presets: Dict[str, Any], name: str) -> Optional[Dict[str, Any]]:
        """Resolve a (possibly inheriting) configure preset to its merged dict."""
        raw = {p.get("name"): p for p in presets.get("configurePresets", []) if isinstance(p, dict)}
        target = raw.get(name)
        if target is None:
            return None
        # merge hidden ancestors in order (inherits may be str or list)
        merged: Dict[str, Any] = {}
        seen: set = set()

        def walk(p: Dict[str, Any]) -> None:
            inh = p.get("inherits")
            if isinstance(inh, str):
                inh = [inh]
            for parent_name in inh or []:
                parent = raw.get(parent_name)
                if parent is None or parent_name in seen:
                    continue
                seen.add(parent_name)
                walk(parent)
                for k, v in parent.items():
                    merged.setdefault(k, v)

        walk(target)
        for k, v in target.items():
            merged[k] = v
        return merged

    def check_presets(self, contract: Dict[str, Any]) -> None:
        path = os.path.join(self.root, PRESETS_RELPATH)
        try:
            presets = _load_json(path)
        except Exception as exc:
            self.fail(f"CMakePresets.json unreadable/invalid JSON: {exc}")
            return
        w = contract.get("windows", {})
        pn = contract.get("preset_names", {})
        win_name = pn.get("windows_formal", "win-msvc-17.14.39-x64")
        win = self.find_configure_preset(presets, win_name)

        # 1) formal preset exists
        if win is None:
            self.fail(f"configurePresets['{win_name}'] missing in CMakePresets.json")
            return
        self.note(f"formal windows preset '{win_name}' resolved (inherits merged)")

        # 2) generator / architecture / toolset exact
        gen = win.get("generator")
        if gen != w.get("formal_generator"):
            self.fail(f"formal generator must be '{w.get('formal_generator')}', got '{gen}'")
        arch = win.get("architecture")
        arch_val = arch.get("value") if isinstance(arch, dict) else arch
        if arch_val != w.get("architecture"):
            self.fail(f"formal architecture must be '{w.get('architecture')}', got '{arch_val}'")
        ts = win.get("toolset")
        ts_val = ts.get("value") if isinstance(ts, dict) else ts
        if not isinstance(ts_val, str):
            self.fail("formal toolset must be a string or object with value")
        else:
            parts = [p.strip() for p in ts_val.split(",")]
            if parts[0] != w.get("toolset"):
                self.fail(f"formal toolset family must be '{w.get('toolset')}', got '{parts[0]}'")
            kv = {}
            for part in parts[1:]:
                if "=" in part:
                    k, _, v = part.partition("=")
                    kv[k.strip()] = v.strip()
            if kv.get("host") != "x64":
                self.fail(f"formal toolset host must be x64, got '{kv.get('host')}'")
            if kv.get("version") != w.get("toolset_version"):
                self.fail(f"formal toolset version must be '{w.get('toolset_version')}', "
                          f"got '{kv.get('version')}'")

        # 3) cache variables
        cv = win.get("cacheVariables") or {}
        sv = cv.get("CMAKE_SYSTEM_VERSION")
        if sv != w.get("sdk_version"):
            self.fail(f"CMAKE_SYSTEM_VERSION must be '{w.get('sdk_version')}', got '{sv}'")
        rt = cv.get("CMAKE_MSVC_RUNTIME_LIBRARY")
        if not isinstance(rt, str) or "Debug>DLL" not in rt or "MultiThreaded" not in rt:
            self.fail(f"CMAKE_MSVC_RUNTIME_LIBRARY must be the DLL policy, got '{rt}'")
        acr = cv.get("ASTROCS_ENABLE_ACR")
        if str(acr).upper() not in ("OFF", "FALSE", "0"):
            self.fail(f"ASTROCS_ENABLE_ACR must be OFF on formal path, got '{acr}'")
        inst = cv.get("CMAKE_GENERATOR_INSTANCE")
        if not isinstance(inst, str) or not inst:
            self.fail("CMAKE_GENERATOR_INSTANCE missing (isolated VS install path required)")

        # 4) forbidden tokens on the windows preset CONFIG surface.
        #    Scan only actual configuration keys (generator/architecture/
        #    toolset/dirs/cacheVariables), never displayName/description
        #    prose, which legitimately documents exclusions.
        win_surface = self.config_surface(win)
        self.scan_forbidden("formal windows preset (config surface)", win_surface, contract)

    @staticmethod
    def config_surface(preset: Dict[str, Any]) -> str:
        """Serialize only the configuration-bearing keys of a preset."""
        keys = ["generator", "architecture", "toolset", "binaryDir",
                "installDir", "cacheVariables", "toolchainFile"]
        out: Dict[str, Any] = {}
        for k in keys:
            if k in preset:
                out[k] = preset[k]
        return json.dumps(out, ensure_ascii=False)

    def scan_forbidden(self, label: str, blob: str, contract: Dict[str, Any]) -> None:
        fb = contract.get("forbidden", {})
        for group, tokens in fb.items():
            for tok in tokens:
                if not isinstance(tok, str) or not tok:
                    continue
                # only token-ish matches (substring on the raw surface JSON)
                if tok.lower() in blob.lower():
                    self.fail(f"forbidden token '{tok}' (group={group}) found in {label}")

    def check_vsconfig(self, contract: Dict[str, Any]) -> None:
        """Compare .vsconfig components exactly against the frozen list.

        The frozen component list is embedded verbatim from
        09_WINDOWS_TOOLCHAIN_LOCK.md section 4 (the task spec).
        """
        path = os.path.join(self.root, VSCONFIG_RELPATH)
        try:
            cfg = _load_json(path)
        except Exception as exc:
            self.fail(f".vsconfig unreadable/invalid JSON: {exc}")
            return
        frozen = [
            "Microsoft.VisualStudio.Workload.VCTools",
            "Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64",
            "Microsoft.VisualStudio.Component.Windows11SDK.26100",
            "Microsoft.VisualStudio.Component.VC.ASAN",
            "Microsoft.VisualStudio.Component.TestTools.BuildTools",
            "Microsoft.VisualStudio.Component.VC.Llvm.Clang",
            "Microsoft.VisualStudio.Component.VC.Llvm.ClangToolset",
        ]
        got = cfg.get("components") if isinstance(cfg, dict) else None
        if not isinstance(got, list):
            self.fail(".vsconfig.components must be an array")
            return
        if cfg.get("version") != "1.0":
            self.fail(".vsconfig.version must be '1.0'")
        if got != frozen:
            missing = [c for c in frozen if c not in got]
            extra = [c for c in got if c not in frozen]
            if missing:
                self.fail(f".vsconfig missing frozen components: {missing}")
            if extra:
                self.fail(f".vsconfig has unapproved components: {extra}")
            if not missing and not extra:
                self.fail(".vsconfig components differ in order from the frozen list")
            return
        self.note(".vsconfig components exactly match the frozen 7-component list")

        # unapproved families guard (defense in depth, substring on ids)
        blob = json.dumps(got)
        unapproved = ["MFC", "ATL", "CppCLI", "UWP", "WinUI", "WindowsAppSDK",
                      "ARM64", "ARM.", ".ARM", "v144", "v145", "v143.mfc"]
        hits = [u for u in unapproved if u.lower() in blob.lower()]
        if hits:
            self.fail(f".vsconfig contains unapproved component families: {hits}")

    def check_linux_light(self, contract: Dict[str, Any]) -> None:
        """Linux preset is light validation only; never fixes the Windows generator."""
        path = os.path.join(self.root, PRESETS_RELPATH)
        presets = _load_json(path)
        pn = contract.get("preset_names", {})
        lname = pn.get("linux_light", "linux-control")
        linux = self.find_configure_preset(presets, lname)
        if linux is None:
            self.fail(f"configurePresets['{lname}'] missing")
            return
        gen = linux.get("generator")
        if gen and "Visual Studio" in str(gen):
            self.fail("linux preset must not use a Visual Studio generator")
        # never hardcode a forbidden generator on the Linux node either
        # (config surface only; prose describing the light role is not scanned)
        self.scan_forbidden(f"linux preset '{lname}' (config surface)",
                            self.config_surface(linux), contract)
        # sanity: linux binary dir distinct from windows binary dir
        if "win-msvc" in str(linux.get("binaryDir", "")):
            self.fail("linux preset binaryDir must be distinct from the windows build tree")
        self.note(f"linux preset '{lname}' present and light (no forbidden generator)")

    # -- whole-run ----------------------------------------------------------
    def run(self) -> Tuple[bool, List[str], List[str]]:
        contract = self.check_contract_schema()
        if contract:
            self.check_presets(contract)
            self.check_vsconfig(contract)
            self.check_linux_light(contract)
        else:
            self.fail("contract unusable; skipped preset/vsconfig/linux checks")
        ok = not self.failures
        return ok, self.failures, self.notes


def _main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="AstroCS BLD-001 toolchain verifier")
    ap.add_argument("--root", default=REPO_ROOT, help="repo root (default: derived)")
    ap.add_argument("--json", metavar="PATH", help="write machine-readable result JSON")
    args = ap.parse_args(argv)

    root = os.path.abspath(args.root)
    v = Verifier(root)
    ok, failures, notes = v.run()

    result = {
        "verifier": "cmake/toolchain/verify_toolchain.py",
        "verdict": "PASS" if ok else "FAIL",
        "root": root,
        "failures": failures,
        "notes": notes,
    }
    if args.json:
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(result, fh, ensure_ascii=False, indent=2)
    if ok:
        print("TOOLCHAIN_CONTRACT_PASS")
        for n in notes:
            print(f"  - {n}")
        return 0
    print("TOOLCHAIN_CONTRACT_FAIL", file=sys.stderr)
    for f in failures:
        print(f"  FAIL: {f}", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(_main())
