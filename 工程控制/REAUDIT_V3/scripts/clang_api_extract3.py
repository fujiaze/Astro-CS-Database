#!/usr/bin/env python3
"""Clang-AST API comparison v3: parameter COUNT + pointer-level shape per param (names stripped)."""
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
    syms = {}
    for line in pr.stdout.splitlines():
        m = re.match(r".*FunctionDecl.*?\s([A-Za-z_]\w*)\s'([^']*)'.*", line)
        if m:
            syms[m.group(1)] = m.group(2)
    return syms, ("ok" if pr.returncode == 0 or syms else "ast_error")

def split_top(s):
    """split on top-level commas (depth 0)"""
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

def param_shape(sig):
    """return list of (n_ptrs, n_refs, const) per param from AST type string"""
    if "(" not in sig: return []
    inner = sig.split("(", 1)[1].rsplit(")", 1)[0]
    if inner.strip() == "": return []
    return [{"ptrs": p.count("*"), "refs": p.count("&"), "const": "const" in p} for p in split_top(inner)]

def table_params(sig):
    """extract param count from a full signature string like AIO_EXPORT int f(const char *path, int n);"""
    if "(" not in sig: return 0
    inner = sig.split("(", 1)[1].rsplit(")", 1)[0]
    if inner.strip() == "": return 0
    return len(split_top(inner))

header_counts = collections.Counter(r["header"] for r in rows if r["header"])
sampled_headers = [h for h, c in header_counts.most_common(8) if c >= 2]
results = []
tot_count_match = tot_count_mismatch = tot_missing = 0
for hdr in sampled_headers:
    ast, status = extract_ast(hdr)
    table_rows = [r for r in rows if r["header"] == hdr]
    count_match = count_mismatch = missing = 0
    examples = []
    for r in table_rows:
        sym = r["symbol"].strip()
        if sym not in ast:
            missing += 1
            continue
        tc = table_params(r["full_signature"])
        ap = param_shape(ast[sym])
        ac = len(ap)
        if tc == ac:
            count_match += 1
        else:
            count_mismatch += 1
            if len(examples) < 6:
                examples.append((sym, tc, ast[sym], r["full_signature"]))
    tot_count_match += count_match; tot_count_mismatch += count_mismatch; tot_missing += missing
    results.append({"header": hdr, "table_rows": len(table_rows), "ast_status": status,
                    "param_count_match": count_match, "param_count_mismatch": count_mismatch,
                    "missing_in_ast": missing, "mismatch_examples": examples})

json.dump(results, open(os.path.join(OUT, "clang_ast_api_comparison.json"), "w"), indent=2, ensure_ascii=False)
print("== Clang AST API comparison v3 (param count, real AST) ==")
for r in results:
    print(f"{r['header']}: table={r['table_rows']} count_match={r['param_count_match']} count_mismatch={r['param_count_mismatch']} missing_in_ast={r['missing_in_ast']}")
    for (sym, tc, a, t) in r.get("mismatch_examples", []):
        print(f"   COUNT_MISMATCH {sym}: table_count={tc} | ast={a} | table_sig={t}")
print("totals: count_match=" + str(tot_count_match) + " count_mismatch=" + str(tot_count_mismatch) + " missing_in_ast=" + str(tot_missing))
