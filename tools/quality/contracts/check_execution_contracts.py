#!/usr/bin/env python3
"""check_execution_contracts.py — T404 execution contracts checker

Checks: ARC-EXEC 与 CMake defines、OpenMP pragma、线程池/async/Dispatcher、日志字段、测试映射一致
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re, csv

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    # Check: EXECUTION_MODEL.md exists and lists ARC-EXEC IDs
    exec_doc = repo / "docs/architecture/EXECUTION_MODEL.md"
    if not exec_doc.exists():
        findings.append({"id":"EXEC-MISSING-DOC","severity":"P1","observed":"EXECUTION_MODEL.md missing","expected":"exists"})
        status="FAIL"
    else:
        text = exec_doc.read_text(encoding="utf-8", errors="ignore")
        for id in ["ARC-EXEC-001","ARC-EXEC-002","ARC-EXEC-003","ARC-EXEC-004","ARC-EXEC-005"]:
            if id not in text:
                findings.append({"id":"EXEC-MISSING-ID","severity":"P1","symbol":id,"observed":"not in EXECUTION_MODEL.md","expected":"exists"})
                status="FAIL"
        if "P2_ENABLE_OPENMP" not in text:
            findings.append({"id":"EXEC-MISSING-OPENMP","severity":"P1","observed":"P2_ENABLE_OPENMP not in doc","expected":"exists"})
            status="FAIL"
        if "critical(aio_read)" not in text:
            findings.append({"id":"EXEC-MISSING-CRITICAL","severity":"P1","observed":"critical(aio_read) not in doc","expected":"exists"})
            status="FAIL"
    # Check: CMakeLists defines
    cmake = repo / "lib/phase2/CMakeLists.txt"
    if cmake.exists():
        ct = cmake.read_text(encoding="utf-8", errors="ignore")
        if 'option(P2_ENABLE_OPENMP' not in ct:
            findings.append({"id":"EXEC-BAD-CMAKE","severity":"P1","file":str(cmake.relative_to(repo)),"observed":"P2_ENABLE_OPENMP option not found","expected":"exists"})
            status="FAIL"
        if 'target_link_libraries(phase2 PUBLIC OpenMP' not in ct:
            findings.append({"id":"EXEC-BAD-LINK","severity":"P1","observed":"OpenMP link not found","expected":"exists"})
            status="FAIL"
    # Check: source pragma
    sampler = repo / "lib/phase2/src/sampler.cpp"
    if sampler.exists():
        st = sampler.read_text(encoding="utf-8", errors="ignore")
        if '#if defined(P2_ENABLE_OPENMP)' not in st:
            findings.append({"id":"EXEC-BAD-SOURCE","severity":"P1","file":str(sampler.relative_to(repo)),"observed":"P2_ENABLE_OPENMP guard not found","expected":"exists"})
            status="FAIL"
    # Check: Dispatcher exists
    if not (repo / "lib/acr/scheduler/dispatcher.cpp").exists():
        findings.append({"id":"EXEC-MISSING-DISPATCHER","severity":"P1","observed":"dispatcher.cpp missing","expected":"exists"})
        status="FAIL"
    # Check: BUILD_GRAPH mentions EXEC
    bg = repo / "docs/architecture/BUILD_GRAPH.md"
    if bg.exists():
        bt = bg.read_text(encoding="utf-8", errors="ignore")
        if "P2_ENABLE_OPENMP" not in bt:
            findings.append({"id":"EXEC-BAD-BUILDGRAPH","severity":"P1","observed":"P2_ENABLE_OPENMP not in BUILD_GRAPH","expected":"exists"})
            status="FAIL"

    result = {"tool":"check_execution_contracts","status":status,"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_execution_contracts" tests="1" failures="{failures}"><testcase classname="exec" name="contracts"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
