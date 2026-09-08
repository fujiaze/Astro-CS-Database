#!/usr/bin/env python3
"""check_api_contracts.py — T401 API contracts checker

Checks: API 文档完整签名与 AST 一致；参数顺序、类型、const/noexcept/linkage/导出宏
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
Supports: --repo, --out-json, --out-junit
"""
import argparse, csv, json, pathlib, sys, re

def normalize_sig(s): return re.sub(r'\s+', ' ', (s or "").strip())

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    api_csv = repo / "docs/contracts/API_CONTRACTS.csv"
    inv_json = repo / "docs/architecture/api_inventory.json"  # optional cached extract
    # Load API contracts
    if not api_csv.exists():
        print(f"FAIL: missing {api_csv}", file=sys.stderr); return 3
    rows = list(csv.DictReader(open(api_csv, encoding="utf-8")))
    # Load AST via extractor
    sys.path.insert(0, str(repo / "tools/quality"))
    try:
        import extract_cpp_api
        # Call extractor logic to get symbols
        import importlib.util
        spec = importlib.util.spec_from_file_location("extract_mod", str(repo / "tools/quality/extract_cpp_api.py"))
        mod = importlib.util.module_from_spec(spec)
        # Instead directly run extractor via subprocess to get json
        import subprocess, json as js
        out = subprocess.check_output([sys.executable, str(repo / "tools/quality/extract_cpp_api.py"), "--repo", str(repo)], text=True)
        ast = js.loads(out)
        # 同名 symbol 多条(如 CodecRegistry::instance 与 Logger::instance,
        # 或两个 typedef 均名为 void)——dict 后写覆盖前写会把 CSV 记录的另一条
        # 判成 API-SIG-TEXT。按 (symbol, header) 保留全部实测, 查找时按 CSV
        # 声明的 header 精确匹配, 再退化到同名任一条。
        ast_syms = {}
        for r in ast.get("symbols", []):
            ast_syms.setdefault(r["symbol"], []).append(r)
    except Exception as e:
        print(f"ENV error loading AST: {e}", file=sys.stderr); return 2
    findings = []
    status = "PASS"
    # Required columns in API_CONTRACTS
    required = ["symbol","full_signature","header","linkage"]
    for c in required:
        if c not in rows[0]:
            print(f"FAIL: missing column {c}", file=sys.stderr); return 3
    # 真实签名比对 (口径: 归一化文本全等 + 参数个数一致):
    #   - 归一化(空白折叠)后全等 => 通过;
    #   - 参数个数不同           => API-SIG-ARITY (P1);
    #   - 参数个数相同但文本不同 => API-SIG-TEXT (P1);
    #   - AST 实测无该符号签名   => API-SIG-NOREF (P1)。
    # 覆盖口径: docs/contracts/API_CONTRACTS.csv 全部行 vs extract_cpp_api.py
    # 头文件实测(含 include/lib 头文件), 覆盖率 = rows 中实际比对的行数比例。
    def _arity(sig_text: str):
        m = re.match(r".*?\b\w[\w:]*\s*\((.*)\)", normalize_sig(sig_text))
        if not m:
            return None
        inner = m.group(1).strip()
        if inner in ("", "void"):
            return 0
        depth, n = 0, 1
        for ch in inner:
            if ch in "(<[":
                depth += 1
            elif ch in ")>]":
                depth -= 1
            elif ch == "," and depth == 0:
                n += 1
        return n

    sig_base = None
    for i, r in enumerate(rows, start=2):
        sym = r["symbol"].strip()
        sig = r["full_signature"].strip()
        hdr = r["header"].strip()
        if not sym:
            findings.append({"id":"API-EMPTY-SYM","severity":"P1","file":str(api_csv),"line":i,"observed":"empty symbol","expected":"non-empty"})
            status="FAIL"
            continue
        if not sig:
            findings.append({"id":"API-EMPTY-SIG","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"observed":"empty signature","expected":"non-empty"})
            status="FAIL"
            continue
        # Check AST existence
        if sym not in ast_syms:
            # Allow if symbol is composite with :: (e.g., class method) - strip prefix
            base = sym.split("::")[-1]
            if base not in ast_syms and sym not in [k.split("::")[-1] for k in ast_syms]:
                findings.append({"id":"API-MISSING-AST","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"header":hdr,"observed":"symbol not in AST extract","expected":"exists in include headers"})
                status="FAIL"
                continue
        # Locate the actually-extracted signature in headers
        # 匹配序: 同名且同 header 文件 → 同名任一条签名相等 → 同名首条
        # (等价旧 dict 行为)。CSV header 列是权威定位, 消重名覆盖误判。
        sig_base = None
        ast_sig = None
        if sym in ast_syms:
            same_hdr = [r for r in ast_syms[sym] if r.get("header") == hdr]
            if same_hdr:
                ast_sig = same_hdr[0].get("signature")
            elif any(normalize_sig(sig) == normalize_sig(r.get("signature")) for r in ast_syms[sym]):
                ast_sig = next(r.get("signature") for r in ast_syms[sym]
                               if normalize_sig(sig) == normalize_sig(r.get("signature")))
            else:
                ast_sig = ast_syms[sym][0].get("signature")
        else:
            sig_base = sym.split("::")[-1]
            if sig_base in ast_syms:
                ast_sig = ast_syms[sig_base][0].get("signature")
        if not ast_sig:
            findings.append({"id":"API-SIG-NOREF","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"header":hdr,"observed":"no measured signature in headers","expected":"extractable signature"})
            status="FAIL"
            continue
        if normalize_sig(sig) != normalize_sig(ast_sig):
            # 文本不同: 用参数个数区分"结构性不同"与"仅表述不同"
            ka, kb = _arity(sig), _arity(ast_sig)
            if ka is None or kb is None:
                findings.append({"id":"API-SIG-UNPARSABLE","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"observed":f"arity unparsable csv={ka} ast={kb}","expected":"parsable signature"})
                status="FAIL"
            elif ka != kb:
                findings.append({"id":"API-SIG-ARITY","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"observed":f"param count csv={ka} vs header={kb}","expected":"matching signature"})
                status="FAIL"
            else:
                findings.append({"id":"API-SIG-TEXT","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"observed":f"csv={normalize_sig(sig)[:120]}","expected":f"header={normalize_sig(ast_sig)[:120]}"})
                status="FAIL"
        # Check header exists
        if hdr and not (repo / hdr).exists():
            findings.append({"id":"API-BAD-HEADER","severity":"P1","file":str(api_csv),"line":i,"symbol":sym,"header":hdr,"observed":"header not found","expected":"exists"})
            status="FAIL"
    # Coverage: ensure API_CONTRACTS count ≈ api_inventory count
    if len(rows) < 300:
        findings.append({"id":"API-COUNT-LOW","severity":"P1","symbol":"count","observed":f"{len(rows)} < 300","expected":"≥300"})
        status="FAIL"

    result = {"tool":"check_api_contracts","status":status,"rows":len(rows),"ast_symbols":len(ast_syms),"findings":findings,"passed": status=="PASS",
              "coverage":{"compared_rows": len(rows) - sum(1 for f in findings if f["id"] in ("API-EMPTY-SYM","API-EMPTY-SIG","API-MISSING-AST")), "basis": "normalized-text + arity vs extract_cpp_api.py header measurement"}}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_api_contracts" tests="{len(rows)}" failures="{failures}"><testcase classname="api" name="signatures"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
