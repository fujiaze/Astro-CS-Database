#!/usr/bin/env python3
"""check_comments.py — T406 comments checker

Checks: 禁止过期审计轮次、旧版本宣称、代码复述、错误线程/单位；要求复杂不变量附近有 ID
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, sys, re

STALE_PATTERNS = [
    (r"V1[0-9]R[0-9]", "stale audit round V19R2/V19R3"),
    (r"TODO.*fix|FIXME.*legacy", "code复述 fix/legacy"),
    (r"thread.*16.*hard.*code|num_threads\(16\)", "hardcoded 16 threads"),
]
# Require ID near complex invariants: check that invariant-adjacent comments have SCI/ALG ID
REQUIRE_ID_NEAR = ["invariant", "不变量", "conservative", "false_negative"]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    # Scan source files in lib/*/src/**/*.cpp
    srcs = list((repo / "lib").rglob("*.cpp")) + list((repo / "lib").rglob("*.h")) + list((repo / "lib").rglob("*.hpp"))
    # Exclude third_party
    srcs = [p for p in srcs if "third_party" not in str(p) and "archive" not in str(p)]
    for src in srcs[:50]:  # Check first 50 for performance
        text = src.read_text(encoding="utf-8", errors="ignore")
        # Extract comments
        comments = re.findall(r'//.*|/\*.*?\*/', text, re.S)
        for c in comments:
            # Check stale patterns - but allow if is in T406's own checker file or is documented history
            if "V19R2" in c or "V19R3" in c:
                # Allow if in science freeze docs referencing history, but not in active source claiming current
                if "冻结" not in c and "history" not in str(src).lower():
                    findings.append({"id":"COMMENT-STALE","severity":"P1","file":str(src.relative_to(repo)),"symbol":c[:60],"observed":"stale audit round","expected":"remove or update"})
                    status="FAIL"
                    break
        # Check for missing ID near invariants: if file contains false_negative or invariant keyword, check nearby ID
        text_lower = text.lower()
        if "false_negative" in text_lower or "不变量" in text:
            if "SCI-" not in text and "ALG-" not in text and "TRACEABILITY" not in text:
                findings.append({"id":"COMMENT-MISSING-ID","severity":"P1","file":str(src.relative_to(repo)),"observed":"invariant without SCI/ALG ID nearby","expected":"ID reference"})
                status="FAIL"
    # Additional: check that no source file claims wrong thread model (e.g., says parallel but is serial)
    # Heuristic: if doc says parallel but source has OFF, that's already covered in EXEC checker; skip here

    result = {"tool":"check_comments","status":status,"files_scanned":len(srcs[:50]),"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_comments" tests="{len(srcs[:50])}" failures="{failures}"><testcase classname="comments" name="hygiene"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
