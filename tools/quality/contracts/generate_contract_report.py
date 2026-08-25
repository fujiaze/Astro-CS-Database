#!/usr/bin/env python3
"""generate_contract_report.py — T410 report generator

Checks: 汇总 JSON/JUnit/HTML 或 Markdown，不自行改变 verdict
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
Supports: --repo, --out-json, --out-junit, --out-md
"""
import argparse, json, pathlib, sys, hashlib

TOOLS = [
    "check_traceability",
    "check_api_contracts",
    "check_doc_symbols",
    "check_config_contracts",
    "check_execution_contracts",
    "check_science_units",
    "check_comments",
    "check_forbidden_patterns",
    "check_build_graph",
    "check_test_contracts",
]

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    ap.add_argument("--out-md", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    results = []
    overall = "PASS"
    for tool in TOOLS:
        script = repo / f"tools/quality/contracts/{tool}.py"
        if not script.exists():
            results.append({"tool":tool,"status":"MISSING","passed":False})
            overall="FAIL"
            continue
        import subprocess
        try:
            out = subprocess.run([sys.executable, str(script), "--repo", str(repo)], capture_output=True, text=True, timeout=30)
            try:
                data = json.loads(out.stdout.strip().split("\n")[-1])
                status = data.get("status","UNKNOWN")
            except:
                status = "PASS" if out.returncode==0 else "FAIL"
            passed = status=="PASS" and out.returncode==0
            if not passed:
                overall="FAIL"
            results.append({"tool":tool,"status":status,"passed":passed,"returncode":out.returncode})
        except Exception as e:
            results.append({"tool":tool,"status":"ERROR","error":str(e),"passed":False})
            overall="FAIL"
    report = {"tool":"generate_contract_report","status":overall,"passed": overall=="PASS","results":results}
    # Byte-stable: sort keys, no timestamp
    js = json.dumps(report, indent=2, ensure_ascii=False, sort_keys=True)
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(js, encoding="utf-8")
    else:
        print(js)
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([r for r in results if not r["passed"]])
        junit = f'<testsuite name="generate_contract_report" tests="{len(results)}" failures="{failures}"><testcase classname="report" name="summary"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    if args.out_md:
        pathlib.Path(args.out_md).parent.mkdir(parents=True, exist_ok=True)
        md = f"# Contract Report\n\nOverall: {overall}\n\n" + "\n".join(f"- {r['tool']}: {r['status']}" for r in results)
        pathlib.Path(args.out_md).write_text(md, encoding="utf-8")
    # Stability: same input -> same bytes (no timestamp in report, only results)
    return 0 if overall=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
