#!/usr/bin/env python3
"""Clang-AST based API extraction + comparison vs API_CONTRACTS.csv (Control §9.1).
Real Clang AST (not regex). Reports signature match/mismatch for sampled headers."""
import subprocess, csv, json, os, re, collections

REPO = "/home/lighthouse/Astro CS Database"
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "07_cross_layer")

rows = list(csv.DictReader(open(os.path.join(REPO, "docs/contracts/API_CONTRACTS.csv"), encoding="utf-8")))

# header -> include list (minimal for the sampled API headers)
def includes_for(header):
    h = header.replace("\\", "/")
    if "aio_" in h and ("astro_image_io" in h):
        return ["-Ilib/astro_image_io/include", "-Ilib/astro_image_io/src", "-Ilib/common"]
    if "phase2" in h:
        return ["-Ilib/phase2/include", "-Ilib/acr/include", "-Ilib/acr",
                "-Ilib/acr/backends/cuda/bridge", "-Ilib/acr/scheduler",
                "-Ilib/astro_image_io/include", "-Ilib/common"]
    if "calibration" in h:
        return ["-Ilib/calibration/include", "-Ilib/calibration/cpp", "-Ilib/common"]
    return ["-Ilib/common"]

def extract_ast(header):
    """Return {symbol: real_signature} from clang AST dump."""
    path = os.path.join(REPO, header)
    if not os.path.isfile(path):
        return None, "header_missing"
    cmd = ["clang", "-x", "c++", "-std=c++20"] + includes_for(header) + \
          ["-fsyntax-only", "-Xclang", "-ast-dump", path]
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    syms = {}
    for line in pr.stdout.splitlines():
        m = re.match(r"FunctionDecl.*?\s([A-Za-z_]\w*)\s'([^']*)'", line)
        if m:
            syms[m.group(1)] = m.group(2)
    return syms, "ok"

# sample headers (those with >=2 rows in the table)
header_counts = collections.Counter(r["header"] for r in rows if r["header"])
sampled_headers = [h for h, c in header_counts.most_common(6) if c >= 2]

results = []
compared = 0
for hdr in sampled_headers:
    ast, status = extract_ast(hdr)
    table_rows = [r for r in rows if r["header"] == hdr]
    if status != "ok" or not ast:
        results.append({"header": hdr, "table_rows": len(table_rows),
                        "ast_status": status, "extracted_symbols": len(ast or {}),
                        "matched": 0, "mismatched": 0, "missing_in_ast": 0,
                        "note": "header_missing_or_ast_error"})
        continue
    matched = mismatched = missing = 0
    mismatches = []
    for r in table_rows:
        sym = r["symbol"].strip()
        if sym not in ast:
            missing += 1
            continue
        real = ast[sym]
        # normalize: strip return-type detail differences (keep param count/order/types is hard from text dump)
        # compare the parameter list portion
        rp = r["full_signature"].split("(", 1)[1].rsplit(")", 1)[0] if "(" in r["full_signature"] else ""
        ap = real.split("(", 1)[1].rsplit(")", 1)[0] if "(" in real else ""
        # strip whitespace
        rp_n = re.sub(r"\s+", "", rp)
        ap_n = re.sub(r"\s+", "", ap)
        if rp_n == ap_n:
            matched += 1
        else:
            mismatched += 1
            mismatches.append((sym, r["full_signature"], real))
    compared += len(table_rows)
    results.append({"header": hdr, "table_rows": len(table_rows),
                    "ast_status": status, "extracted_symbols": len(ast),
                    "matched": matched, "mismatched": mismatched, "missing_in_ast": missing,
                    "mismatch_examples": mismatches[:5]})

json.dump(results, open(os.path.join(OUT, "clang_ast_api_comparison.json"), "w"), indent=2, ensure_ascii=False)
print("== Clang AST API comparison ==")
for r in results:
    print(f"{r['header']}: table={r['table_rows']} ast={r['extracted_symbols']} matched={r['matched']} mismatched={r['mismatched']} missing_in_ast={r['missing_in_ast']}")
    for (sym, t, a) in r.get("mismatch_examples", []):
        print(f"   MISMATCH {sym}:\n     table: {t}\n     ast:   {a}")
print("compared rows:", compared)
