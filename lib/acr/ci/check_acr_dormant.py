#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ACR-001 dormant build guard machine checker (SA-ACR-13).

Freezes the ACR dormant boundary required by
AstroCS_ENGINEERING_CONSTRAINTS.md §C.1/§C.2 and the ACR-001 task:

  验收 (tasks/04_CPU_RESOURCE_TASKS.md §ACR-001):
  - 默认/Release/Win install/product manifest 不构建、不链接、不加载
    ACR/CUDA；grep / CMake graph / PE imports / registry 均无生产依赖；
  - 显式 ASTROCS_ENABLE_ACR=ON 只能生成隔离实验 target，不得被 release
    preset 接收。

Static checks (source-tree level; no configure/build required):

  ACK-ACR-001  production CMake graph never add_subdirectory(lib/acr)
                (root CMakeLists.txt has no add_subdirectory(lib/acr)).
  ACK-ACR-002  lib/acr/CMakeLists.txt carries the standalone-only guard
                (CMAKE_SOURCE_DIR == CMAKE_CURRENT_SOURCE_DIR FATAL).
  ACK-ACR-003  release presets (win-msvc-17.14.39-x64 / linux-control) freeze
                ASTROCS_ENABLE_ACR=OFF; ASTROCS_ENABLE_ACR option default OFF.
  ACK-ACR-004  install surface is ACR/CUDA-free: cmake/install_layout.cmake,
                packaging/install-tree.contract.json,
                packaging/astrocs.product.json contain no ACR/CUDA entries
                (grep-based production-dependency zero-hit).
  ACK-ACR-005  production source tree (cli/, lib/core/, lib/phase1*,
                lib/phase2_session/, lib/phase3_session/, lib/backend_host/,
                runtime/, modules/, include/astrocs/) does not include/compile
                ACR headers (astro/compute, lib/acr/backends/cuda/bridge) —
                the phase2 legacy stub cuda_bridge_stub.cpp is excluded from
                the root product target list (BLD-002 source whitelist).
  ACK-ACR-006  registry/module loader rejects ACR modules in production
                (lib/core/src/module.cpp refuses astrocs.acr.*).
  ACK-ACR-007  product manifest units contain no ACR/CUDA unit (kind
                module/provider/runtime with acr/cuda in rel_path).

Negative self-tests (--selftest): each must FAIL when the violation is
introduced (guards against a vacuous checker).

Exit: 0 PASS, 1 FAIL (or selftest caught), 2 env error.
Pure stdlib (Python >= 3.10); runs on the Linux control node.
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

# production source trees that must never reference ACR headers/symbols
PROD_TREES = [
    "cli",
    "lib/core",
    "lib/phase1",
    "lib/phase1_session",
    "lib/phase2_session",
    "lib/phase3_session",
    "lib/backend_host",
    "runtime",
    "modules",
    "include/astrocs",
]
# legacy phase2 compatibility sources that legitimately include ACR bridge
# headers ONLY for standalone/module-tool builds (never part of the root
# product target list; excluded from the production compile whitelist).
LEGACY_ACR_SOURCES = [
    "lib/phase2/src/cuda_bridge_stub.cpp",
    "lib/phase2/src/acr_kernels.cpp",
    "lib/phase2/tools/stage2.cpp",
]
ACR_HEADER_MARKERS = [
    "astro/compute",
    "cuda_bridge_api.hpp",
    "acr_cuda_bridge.h",
    "lib/acr/backends/cuda/bridge",
]
ACR_SYMBOL_MARKERS = ["astro::compute", "kernel_registry", "device_executor", "try_append_cuda"]


def read_text(p: pathlib.Path) -> str:
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def failures_for(repo: pathlib.Path) -> list[str]:
    fails: list[str] = []

    # ACK-ACR-001: root CMake never add_subdirectory(lib/acr)
    root_cmake = repo / "CMakeLists.txt"
    root_txt = read_text(root_cmake)
    if re.search(r"add_subdirectory\s*\(\s*lib/acr\b", root_txt) or \
       re.search(r"add_subdirectory\s*\(\s*\$\{CMAKE_CURRENT_SOURCE_DIR\}/lib/acr\b", root_txt):
        fails.append("ACK-ACR-001: root CMakeLists.txt add_subdirectory(lib/acr) — ACR in product graph")

    # ACK-ACR-002: lib/acr guard present
    acr_cmake = repo / "lib/acr/CMakeLists.txt"
    acr_txt = read_text(acr_cmake)
    if "CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR" not in acr_txt:
        fails.append("ACK-ACR-002: lib/acr/CMakeLists.txt standalone-only guard missing")
    if "FATAL_ERROR" not in acr_txt:
        fails.append("ACK-ACR-002: lib/acr/CMakeLists.txt guard must FATAL_ERROR on non-standalone")

    # ACK-ACR-003: option default OFF + release presets OFF
    opt_hits = [ln for ln in root_txt.splitlines()
                if re.search(r"option\s*\(\s*ASTROCS_ENABLE_ACR\b", ln)]
    if not opt_hits or not any(re.search(r"\bOFF\s*\)\s*$", ln) for ln in opt_hits):
        fails.append("ACK-ACR-003: ASTROCS_ENABLE_ACR option must default OFF in root CMakeLists.txt")
    presets = repo / "CMakePresets.json"
    pj = {}
    try:
        pj = json.loads(read_text(presets)) if presets.exists() else {}
    except json.JSONDecodeError:
        fails.append("ACK-ACR-003: CMakePresets.json unparseable")
    cps = pj.get("configurePresets", []) if isinstance(pj, dict) else []
    raw = {cp.get("name"): cp for cp in cps if isinstance(cp, dict)}
    for name in ("win-msvc-17.14.39-x64", "linux-control"):
        merged = {}
        seen = set()

        def walk(preset_name: str) -> None:
            p = raw.get(preset_name)
            if p is None or preset_name in seen:
                return
            seen.add(preset_name)
            inh = p.get("inherits")
            if isinstance(inh, str):
                inh = [inh]
            for parent in inh or []:
                walk(parent)
                for k, v in raw.get(parent, {}).items():
                    merged.setdefault(k, v)
            for k, v in p.items():
                merged[k] = v

        walk(name)
        if "name" not in merged:
            fails.append(f"ACK-ACR-003: preset '{name}' not found in CMakePresets.json")
            continue
        cv = merged.get("cacheVariables") or {}
        if str(cv.get("ASTROCS_ENABLE_ACR", "")).upper() not in ("OFF", "FALSE", "0"):
            fails.append(f"ACK-ACR-003: preset '{name}' must freeze ASTROCS_ENABLE_ACR=OFF")

    # ACK-ACR-004: install surface ACR/CUDA-free
    for rel in ("cmake/install_layout.cmake",
                "packaging/install-tree.contract.json",
                "packaging/astrocs.product.json"):
        p = repo / rel
        if not p.exists():
            fails.append(f"ACK-ACR-004: {rel} missing")
            continue
        txt = read_text(p)
        hits = [ln for ln in txt.splitlines() if re.search(r"acr|cuda", ln, re.I)]
        if hits:
            fails.append(f"ACK-ACR-004: ACR/CUDA reference in {rel}: {hits[:3]}")

    # ACK-ACR-005: production tree header inclusion zero-hit
    for tree in PROD_TREES:
        base = repo / tree
        if not base.is_dir():
            continue
        for p in sorted(base.rglob("*")):
            if not p.is_file():
                continue
            if p.suffix not in (".cpp", ".h", ".hpp", ".c", ".cc", ".cu"):
                continue
            rel = p.relative_to(repo).as_posix()
            if rel in LEGACY_ACR_SOURCES:
                continue
            txt = read_text(p)
            for marker in ACR_HEADER_MARKERS:
                if marker in txt:
                    fails.append(f"ACK-ACR-005: {rel} references ACR header marker '{marker}'")
                    break
            # only .cpp/.cc/.c sources carry symbol bodies
            if p.suffix in (".cpp", ".cc", ".c"):
                for marker in ACR_SYMBOL_MARKERS:
                    if marker in txt:
                        fails.append(f"ACK-ACR-005: {rel} references ACR symbol marker '{marker}'")
                        break

    # ACK-ACR-006: production module registry rejects ACR modules
    module_cpp = repo / "lib/core/src/module.cpp"
    if "astrocs.acr." not in read_text(module_cpp):
        fails.append("ACK-ACR-006: lib/core/src/module.cpp must reject astrocs.acr.* module registration")

    # ACK-ACR-007: product manifest has no ACR/CUDA unit
    prod = repo / "packaging/astrocs.product.json"
    try:
        data = json.loads(read_text(prod)) if prod.exists() else {}
    except json.JSONDecodeError:
        data = {}
        fails.append("ACK-ACR-007: astrocs.product.json unparseable")
    for unit in data.get("units", []) if isinstance(data, dict) else []:
        blob = json.dumps(unit, ensure_ascii=False).lower()
        if re.search(r"acr|cuda", blob):
            fails.append(f"ACK-ACR-007: product manifest unit contains ACR/CUDA: {unit.get('unit_id')}")

    return fails


def run_checks(repo: pathlib.Path) -> int:
    repo = repo.resolve()
    fails = failures_for(repo)
    if fails:
        print("ACR_DORMANT_FAIL")
        for f in fails:
            print("  - " + f)
        return 1
    print("ACR_DORMANT_PASS: production build/install/manifest/registry "
          "ACR/CUDA-free (ACK-ACR-001..007); ACR experimental tree standalone-only")
    return 0


def selftest() -> int:
    """Negative self-tests: inject each violation and require FAIL.

    Exercises the checker against synthetic repos so a vacuous checker
    (one that always passes) is caught.
    """
    import tempfile
    cases = []

    def mkrepo(violation: str) -> pathlib.Path:
        td = pathlib.Path(tempfile.mkdtemp(prefix="acr_off_st_"))
        (td / "CMakeLists.txt").write_text(
            "option(ASTROCS_ENABLE_ACR \"dormant\" OFF)\n", encoding="utf-8")
        (td / "CMakePresets.json").write_text(
            json.dumps({"configurePresets": [
                {"name": "win-msvc-17.14.39-x64",
                 "cacheVariables": {"ASTROCS_ENABLE_ACR": "OFF"}},
                {"name": "linux-control",
                 "cacheVariables": {"ASTROCS_ENABLE_ACR": "OFF"}},
            ]}), encoding="utf-8")
        (td / "cmake").mkdir()
        (td / "cmake/install_layout.cmake").write_text("", encoding="utf-8")
        (td / "packaging").mkdir()
        (td / "packaging/install-tree.contract.json").write_text("{}", encoding="utf-8")
        (td / "packaging/astrocs.product.json").write_text(
            json.dumps({"units": []}), encoding="utf-8")
        (td / "lib").mkdir()
        (td / "lib/acr").mkdir(parents=True)
        (td / "lib/acr/CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.24)\n", encoding="utf-8")
        (td / "lib/core").mkdir(parents=True)
        (td / "lib/core/src").mkdir()
        (td / "lib/core/src/module.cpp").write_text(
            "// rejects astrocs.acr.*\n", encoding="utf-8")
        if violation == "add_subdir":
            (td / "CMakeLists.txt").write_text(
                "option(ASTROCS_ENABLE_ACR \"dormant\" OFF)\n"
                "add_subdirectory(lib/acr)\n", encoding="utf-8")
        elif violation == "no_guard":
            pass  # lib/acr/CMakeLists.txt without guard (default above)
        elif violation == "preset_on":
            (td / "CMakePresets.json").write_text(
                json.dumps({"configurePresets": [
                    {"name": "win-msvc-17.14.39-x64",
                     "cacheVariables": {"ASTROCS_ENABLE_ACR": "ON"}},
                    {"name": "linux-control",
                     "cacheVariables": {"ASTROCS_ENABLE_ACR": "OFF"}},
                ]}), encoding="utf-8")
        elif violation == "install_acr":
            (td / "cmake/install_layout.cmake").write_text(
                "install(TARGETS acr_fake ...)\n", encoding="utf-8")
        elif violation == "prod_include":
            (td / "cli").mkdir()
            (td / "cli/main.cpp").write_text(
                '#include "astro/compute/acr.hpp"\nint main(){return 0;}\n', encoding="utf-8")
        elif violation == "registry_open":
            (td / "lib/core/src/module.cpp").write_text(
                "// registry without acr rejection\n", encoding="utf-8")
        elif violation == "manifest_acr":
            (td / "packaging/astrocs.product.json").write_text(
                json.dumps({"units": [{"unit_id": "MOD-ACR", "kind": "module",
                                       "rel_path": "modules/acr.so",
                                       "abi_version": 1, "status": "SKELETON"}]}),
                encoding="utf-8")
        cases.append((violation, td))
        return td

    expected_fail = {
        "add_subdir": ["ACK-ACR-001"],
        "no_guard": ["ACK-ACR-002"],
        "preset_on": ["ACK-ACR-003"],
        "install_acr": ["ACK-ACR-004"],
        "prod_include": ["ACK-ACR-005"],
        "registry_open": ["ACK-ACR-006"],
        "manifest_acr": ["ACK-ACR-007"],
    }
    bad = 0
    for name, ids in expected_fail.items():
        td = mkrepo(name)
        fails = failures_for(td)
        missing = [i for i in ids if not any(f.startswith(i) for f in fails)]
        if missing:
            print(f"SELFTEST_FAIL: violation '{name}' not caught (missing {missing}); got {fails}")
            bad += 1
        else:
            print(f"SELFTEST_PASS: violation '{name}' caught ({ids[0]})")
        import shutil
        shutil.rmtree(td, ignore_errors=True)
    return 1 if bad else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--repo", default=".", type=pathlib.Path)
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    return run_checks(args.repo)


if __name__ == "__main__":
    raise SystemExit(main())
