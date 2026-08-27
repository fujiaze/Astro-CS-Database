#!/usr/bin/env python3
"""Full 422-row Clang-AST API comparison (Control §9.1): all 43 headers, param-count + missing."""
import subprocess, csv, json, os, re, collections, sys

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
        incs += ["-I" + os.path.join(REPO, "lib/orchestrator/cpp/include"),
                 "-I" + os.path.join(REPO, "lib/orchestrator/cpp/third_party/json-schema-validator")]
    if "snr_estimator" in h:
        incs += ["-I" + os.path.join(REPO, "lib/snr_estimator/cpp/include"),
                 "-I" + os.path.join(REPO, "lib/common/include")]
    if "plate_solve" in h:
        incs += ["-I" + os.path.join(REPO, "lib/plate_solve/cpp/ipv/include"),
                 "-I" + os.path.join(REPO, "lib/astro_image_io/include")]
    if "star_detector" in h:
        incs += ["-I" + os.path.join(REPO, "lib/star_detector/include"),
                 "-I" + os.path.join(REPO, "lib/astro_image_io/include")]
    if "dynamic_psf" in h:
        incs += ["-I" + os.path.join(REPO, "lib/dynamic_psf/include")]
    if "photometric_calib" in h:
        incs += ["-I" + os.path.join(REPO, "lib/photometric_calib/cpp/include")]
    return incs, defs

def extract_ast(header):
    path = os.path.join(REPO, header)
    if not os.path.isfile(path):
        return None, "header_missing"
    incs, defs = flags_for(header)
    cmd = ["clang", "-x", "c++", "-std=c++20", "-w"] + defs + incs + \
          ["-fsyntax-only", "-Xclang", "-ast-dump", path]
    pr = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    syms = {}
    for line in pr.stdout.splitlines():
        m = re.match(r".*(?:FunctionDecl|CXXMethodDecl).*?\s([A-Za-z_]\w*)\s'([^']*)'.*", line)
        if m:
            syms[m.group(1)] = m.group(2)
    if syms:
        return syms, "ok"
    return syms, "ast_error:" + (pr.stderr[:100].replace("\n", " ") if pr.stderr else "no output")

def split_top(s):
    out, depth, cur = [], 0, ""
    for ch in s:
        if ch in "([<": depth += 1
        elif ch in ")]>": depth -= 1
        if ch == "," and depth == 0:
            out.append(cur); cur = ""
        else:
            cur += ch
    if cur.strip(): out.append(cur)
    return [p.strip() for p in out if p.strip()]

def table_param_count(sig):
    if "(" not in sig: return None
    inner = sig.split("(", 1)[1].rsplit(")", 1)[0]
    if inner.strip() in ("", "void"): return 0
    return len(split_top(inner))

def ast_param_count(sig):
    if "(" not in sig: return None
    inner = sig.split("(", 1)[1].rsplit(")", 1)[0]
    if inner.strip() == "": return 0
    return len(split_top(inner))

headers = sorted(set(r["header"] for r in rows if r["header"]))
per_header = collections.defaultdict(list)
for r in rows:
    if r["header"]:
        per_header[r["header"]].append(r)

results = []
tot_match = tot_mm = tot_missing = tot_ast_ok_rows = 0
for hdr in headers:
    ast, status = extract_ast(hdr)
    tr = per_header[hdr]
    count_match = count_mismatch = missing = 0
    ex = []
    for r in tr:
        sym = r["symbol"].strip()
        if sym not in ast:
            missing += 1
            continue
        tc = table_param_count(r["full_signature"])
        ac = ast_param_count(ast[sym])
        if tc == ac:
            count_match += 1
        else:
            count_mismatch += 1
            if len(ex) < 3:
                ex.append((sym, tc, ast[sym], r["full_signature"]))
    tot_match += count_match; tot_mm += count_mismatch; tot_missing += missing
    tot_ast_ok_rows += count_match + count_mismatch
    results.append({"header": hdr, "table_rows": len(tr), "ast_status": status,
                    "param_count_match": count_match, "param_count_mismatch": count_mismatch,
                    "missing_in_ast": missing, "mismatch_examples": ex})

json.dump(results, open(os.path.join(OUT, "clang_ast_api_comparison_full.json"), "w"), indent=2, ensure_ascii=False)
print("== FULL Clang-AST API comparison (422 rows) ==")
for r in results:
    print(f"{r['header']}: rows={r['table_rows']} match={r['param_count_match']} mm={r['param_count_mismatch']} missing={r['missing_in_ast']} status={r['ast_status'][:40]}")
print("TOTALS: count_match=" + str(tot_match) + " count_mismatch=" + str(tot_mm) + " missing_in_ast=" + str(tot_missing) + " ast_resolved_rows=" + str(tot_ast_ok_rows))
