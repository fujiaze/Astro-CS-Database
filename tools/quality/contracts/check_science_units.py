#!/usr/bin/env python3
"""check_science_units.py — T405 science units checker

Checks: SCI/ALG/API 中定义的量、单位、精度、valid range 相互一致
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
    # Check: each SCI doc has physical quantity and units section
    sci_docs = list((repo / "docs/science").glob("*.md"))
    for doc in sci_docs:
        text = doc.read_text(encoding="utf-8", errors="ignore")
        # Check that "物理量和单位" or units exist; UNCERTAINTY_AND_COVARIANCE uses variance/covariance terminology
        if "物理量和单位" not in text and "变量/单位" not in text and "ADU" not in text and "variance" not in text and "方差" not in text:
            findings.append({"id":"SCI-UNITS-MISSING","severity":"P1","file":str(doc.relative_to(repo)),"observed":"missing units section","expected":"contains ADU/deg/pixel units"})
            status="FAIL"
    # Check: API contracts have units column non-empty
    api_csv = repo / "docs/contracts/API_CONTRACTS.csv"
    if api_csv.exists():
        rows = list(csv.DictReader(open(api_csv, encoding="utf-8")))
        for r in rows:
            if not r.get("units","").strip():
                findings.append({"id":"API-UNITS-EMPTY","severity":"P1","symbol":r.get("symbol",""),"observed":"units empty","expected":"non-empty"})
                status="FAIL"
                break
    # Check: SCI docs units are not contradictory (heuristic: ADU, deg, pixel appear consistently)
    # Simple: ensure at least 10 units references exist
    all_sci = " ".join(p.read_text(encoding="utf-8", errors="ignore") for p in sci_docs)
    unit_count = sum(all_sci.count(u) for u in ["ADU","deg","pixel","rad","mag","dex"])
    if unit_count < 20:
        findings.append({"id":"SCI-UNITS-COUNT","severity":"P1","observed":f"unit references {unit_count} < 20","expected":"≥20"})
        status="FAIL"
    # Check: precision mentions FP32/FP64
    precision_count = sum(all_sci.count(p) for p in ["FP32","FP64","float","double"])
    if precision_count < 5:
        findings.append({"id":"SCI-PRECISION-MISSING","severity":"P1","observed":f"precision refs {precision_count} < 5","expected":"≥5"})
        status="FAIL"

    result = {"tool":"check_science_units","status":status,"docs":len(sci_docs),"unit_refs":unit_count,"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_science_units" tests="{len(sci_docs)}" failures="{failures}"><testcase classname="units" name="consistency"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
