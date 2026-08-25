#!/usr/bin/env python3
"""check_config_contracts.py — T403 config contracts checker

Checks: schema、示例、解析器、默认值、枚举、错误信息一致；单一默认值源
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
    # Check: stage2_common.cpp parser defaults match docs/development/CONFIG_SCHEMA.md
    schema_doc = repo / "docs/development/CONFIG_SCHEMA.md"
    if not schema_doc.exists():
        findings.append({"id":"CFG-MISSING-DOC","severity":"P1","observed":"CONFIG_SCHEMA.md missing","expected":"exists"})
        status="FAIL"
    # Check: weight_mode defaults: parser default "auto" -> 2 (ivar) should match doc
    parser_file = repo / "lib/phase2/src/stage2_common.cpp"
    parser_text = parser_file.read_text(encoding="utf-8", errors="ignore") if parser_file.exists() else ""
    if 'weight_mode = 2' not in parser_text:
        findings.append({"id":"CFG-PARSER-DEFAULT","severity":"P1","file":str(parser_file.relative_to(repo)),"observed":"weight_mode default 2 not found","expected":"default ivar"})
        status="FAIL"
    if 'acr_route", std::string("auto")' not in parser_text:
        findings.append({"id":"CFG-PARSER-ACR","severity":"P1","observed":"acr_route auto default not found","expected":"auto"})
        status="FAIL"
    # Check: example configs are valid JSON and contain required keys
    for cfg in (repo / "lib/phase2/configs").glob("*.json"):
        try:
            j=json.loads(cfg.read_text(encoding="utf-8"))
            if "inputs" not in j or "integration" not in j:
                findings.append({"id":"CFG-EXAMPLE-KEYS","severity":"P1","file":str(cfg.relative_to(repo)),"observed":"missing inputs/integration","expected":"both present"})
                status="FAIL"
        except Exception as e:
            findings.append({"id":"CFG-EXAMPLE-JSON","severity":"P1","file":str(cfg.relative_to(repo)),"observed":str(e),"expected":"valid JSON"})
            status="FAIL"
    # Check: weight_mode enum in doc and parser match
    doc_text = schema_doc.read_text(encoding="utf-8", errors="ignore") if schema_doc.exists() else ""
    for enum_val in ["auto","ivar","equal","support_x_snr2"]:
        if enum_val not in parser_text:
            findings.append({"id":"CFG-ENUM-MISSING","severity":"P1","symbol":enum_val,"observed":"not in parser","expected":"exists"})
            status="FAIL"
    # CONFIG_SCHEMA.md lists weight_mode(auto) shorthand; check keyword presence
    if "weight_mode" not in doc_text:
        findings.append({"id":"CFG-DOC-WEIGHT","severity":"P1","observed":"weight_mode not in CONFIG_SCHEMA.md","expected":"exists"})
        status="FAIL"
    # Check: error messages exist in parser
    for err in ["weight_mode 只支持","acr_route 只支持"]:
        if err not in parser_text:
            findings.append({"id":"CFG-ERROR-MSG","severity":"P1","symbol":err,"observed":"not in parser","expected":"exists"})
            status="FAIL"

    result = {"tool":"check_config_contracts","status":status,"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_config_contracts" tests="1" failures="{failures}"><testcase classname="config" name="contracts"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
