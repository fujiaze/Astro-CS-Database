#!/usr/bin/env python3
"""Clang-AST based API extraction + comparison vs API_CONTRACTS.csv (Control §9.1) v2.
Uses module defines from the actual Makefiles so guarded declarations are visible."""
import subprocess, csv, json, os, re, collections

REPO = "/home/lighthouse/Astro CS Database"
ROOT = open("/home/lighthouse/astrocs_audit_v2/CURRENT_ROOT.txt").read().strip()
OUT = os.path.join(ROOT, "package", "07_cross_layer")

rows = list(csv.DictReader(open(os.path.join(REPO, "docs/contracts/API_CONTRACTS.csv"), encoding="utf-8")))

AIO_DEFS = ["-DAIO_ENABLE_FITS","-DAIO_ENABLE_XISF","-DAIO_ENABLE_AHPX","-DAIO_ENABLE_HEALPIX",
            "-DAIO_ENABLE_COMPRESSOR","-DAIO_ENABLE_PIPELINE","-DHAS_ZSTD","-DHAS_LZ4"]

def flags_for(header):
    h = header.replace("\\", "/")
    incs = ["-I" + os.path.join(REPO, "lib/common")]
    defs = []
    if "astro_image_io" in h:
        incs += ["-I" + os.path.join(REPO, "lib/astro_image_io/include"),
                 "-I" + os.path.join(REPO, "lib/astro_image_io/src")]
        defs += AIO_DEFS
    if "phase2" in h:
        incs += ["-I" + os.path.join(REPO, "lib/phase2/include"),
                 "-I" + os.path.join(REPO, "lib/acr/include"),
                 "-I" + os.path.join(REPO, "lib/acr"),
                 "-I" + os.path.join(REPO, "lib/acr/backends/cuda/bridge"),
                 "-I" + os.path.join(REPO, "lib/acr/scheduler"),
                 "-I" + os.path.join(REPO, "lib/astro_image_io/include")]
    if "calibration" in h:
        incs += ["-I" + os.path.join(REPO, "lib/calibration/include"),
                 "-I" + os.path.join(REPO, "lib/calibration/cpp")]
    if "orchestrator" in h:
        incs += ["-I" + os.path.join(REPO, "lib/orchestrator/cpp/include")]
    return incs, defs

def extract_ast(header):
    path = os.path.join(REPO, header)
    if not os.path.isfile(path):
        return None, "header_missing"
    incs, defs = flags_for(header)
    cmd = ["clang", "-x", "c++", "-std=c++20"] + defs + incs + \
          ["-fsyntax-only", "-Xclang", "-ast-dump", path]
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if pr.returncode != 0 and not pr.stdout:
        return {}, "ast_error:" + pr.stderr[:120]
    syms = {}
    for line in pr.stdout.splitlines():
        m = re.match(r".*FunctionDecl.*?\s([A-Za-z_]\w*)\s'([^']*)'.*", line)
        if m:
            syms[m.group(1)] = m.group(2)
    return syms, "ok"

def norm_params(sig):
    if "(" not in sig:
        return ""
    inner = sig.split("(", 1)[1].rsplit(")", 1)[0]
    return re.sub(r"\s+", "", inner)

header_counts = collections.Counter(r["header"] for r in rows if r["header"])
sampled_headers = [h for h, c in header_counts.most_common(8) if c >= 2]
results = []
compared = 0
for hdr in sampled_headers:
    ast, status = extract_ast(hdr)
    table_rows = [r for r in rows if r["header"] == hdr]
    if status != "ok" or not ast:
        results.append({"header": hdr, "table_rows": len(table_rows), "ast_status": status,
                        "extracted_symbols": len(ast or {}), "matched": 0, "mismatched": 0,
                        "missing_in_ast": 0, "note": "ast_error_or_empty"})
        continue
    matched = mismatched = missing = 0
    mism = []
    for r in table_rows:
        sym = r["symbol"].strip()
        if sym not in ast:
            missing += 1
            continue
        rp = norm_params(r["full_signature"])
        ap = norm_params(ast[sym])
        if rp == ap:
            matched += 1
        else:
            mismatched += 1
            if len(mism) < 6:
                mism.append((sym, r["full_signature"], ast[sym]))
    compared += len(table_rows)
    results.append({"header": hdr, "table_rows": len(table_rows), "ast_status": status,
                    "extracted_symbols": len(ast), "matched": matched, "mismatched": mismatched,
                    "missing_in_ast": missing, "mismatch_examples": mism})

json.dump(results, open(os.path.join(OUT, "clang_ast_api_comparison.json"), "w"), indent=2, ensure_ascii=False)
print("== Clang AST API comparison (real AST, not regex) ==")
tot_m = tot_mm = tot_miss = 0
for r in results:
    print(f"{r['header']}: table={r['table_rows']} ast={r['extracted_symbols']} matched={r['matched']} mismatched={r['mismatched']} missing_in_ast={r['missing_in_ast']} status={r.get('ast_status')}")
    tot_m += r["matched"]; tot_mm += r["mismatched"]; tot_miss += r["missing_in_ast"]
    for (sym, t, a) in r.get("mismatch_examples", []):
        print(f"   MISMATCH {sym}:\n     table: {t}\n     ast:   {a}")
print("totals: matched=" + str(tot_m) + " mismatched=" + str(tot_mm) + " missing_in_ast=" + str(tot_miss) + " compared_rows=" + str(compared))
