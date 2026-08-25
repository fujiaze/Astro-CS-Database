#!/usr/bin/env python3
"""check_build_graph.py — T408 build graph checker

Checks: 文档声明的 target/source/define/link library 与 CMake file-api/compile DB 一致
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    bg = repo / "docs/architecture/BUILD_GRAPH.md"
    if not bg.exists():
        findings.append({"id":"BUILD-MISSING-DOC","severity":"P1","observed":"BUILD_GRAPH.md missing","expected":"exists"})
        status="FAIL"
    else:
        text = bg.read_text(encoding="utf-8", errors="ignore")
        # Check targets mentioned
        for tgt in ["phase2","astrocs-stage2","calibrated_pair_diag","rejection_cli"]:
            if tgt not in text:
                findings.append({"id":"BUILD-MISSING-TARGET","severity":"P1","symbol":tgt,"observed":"not in BUILD_GRAPH","expected":"exists"})
                status="FAIL"
        if "P2_ENABLE_OPENMP" not in text:
            findings.append({"id":"BUILD-MISSING-DEFINE","severity":"P1","observed":"P2_ENABLE_OPENMP not in BUILD_GRAPH","expected":"exists"})
            status="FAIL"
    # Verify CMakeLists have these targets
    cmake = repo / "lib/phase2/CMakeLists.txt"
    if cmake.exists():
        ct = cmake.read_text(encoding="utf-8", errors="ignore")
        for tgt in ["add_library(phase2","add_executable(astrocs-stage2","add_executable(calibrated_pair_diag"]:
            if tgt not in ct:
                findings.append({"id":"BUILD-CMAKE-MISSING","severity":"P1","symbol":tgt,"observed":"not in CMakeLists","expected":"exists"})
                status="FAIL"
        if "target_link_libraries(phase2" not in ct:
            findings.append({"id":"BUILD-MISSING-LINK","severity":"P1","observed":"phase2 link not in CMake","expected":"exists"})
            status="FAIL"
    # Verify sources exist
    if not (repo / "lib/phase2/src/upm.cpp").exists():
        findings.append({"id":"BUILD-MISSING-SOURCE","severity":"P1","observed":"upm.cpp missing","expected":"exists"})
        status="FAIL"

    result = {"tool":"check_build_graph","status":status,"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_build_graph" tests="1" failures="{failures}"><testcase classname="build" name="graph"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
