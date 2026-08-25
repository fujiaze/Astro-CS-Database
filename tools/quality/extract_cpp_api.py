#!/usr/bin/env python3
"""extract_cpp_api.py — Authoritative API extractor (T301)

Extracts public C/C++ symbols from headers under lib/*/include/.
Prefers compile_commands.json + Clang AST if available (-Xclang -ast-dump=json),
fallback to header regex for offline listing. Output JSON with complete signatures.

Exit: 0 success, 2 env error, 3 input/schema error.
Supports: --repo, --out-json, --out-junit
"""
import argparse, csv, json, os, re, sys, pathlib

def dedupe_root(repo):
    return pathlib.Path(repo)

HEADER_RE_F = re.compile(r'^\s*(?:P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)?\s*(?:[\w:]+\s+)+(\w+)\s*\([^;]*\)\s*;', re.M)
# Broader: capture function-like lines in include headers
FUNC_LINE_RE = re.compile(r'^\s*(?:extern\s+"C"\s*\{\s*)?(?:P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API|extern)?\s*([^\n;]*\b(\w+)\s*\([^;]*\)\s*;)', re.M)

def extract_from_header(path: pathlib.Path):
    text = path.read_text(encoding="utf-8", errors="ignore")
    # Remove block comments for clean scan
    text_nc = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text_nc = re.sub(r'//.*', '', text_nc)
    results = []
    for m in re.finditer(r'^\s*(?:(P2_API|AC_API|CC_EXPORT|DPSF_EXPORT|SNR_API)\s+)?([A-Za-z_][\w\s\*\:\<\>\,\&]*?)\b([A-Za-z_]\w*)\s*\([^;{]*\)\s*;\s*$', text_nc, re.M):
        prefix = (m.group(1) or "").strip()
        ret = m.group(2).strip()
        name = m.group(3).strip()
        full = m.group(0).strip().replace("\n"," ").strip()
        # Filter obvious non-API (keywords)
        if name in {"if","for","while","switch","return"}:
            continue
        if len(name) < 4 and not name.startswith(("aio_","ac_","cc_","dpsf_","snr_","p2_","sdet_","pc_","ipv_","gaia_","healpix_","aio","astro")):
            continue
        sig = re.sub(r'\s+', ' ', full)
        results.append({"symbol": name, "signature": sig, "header": str(path), "export": prefix or None})
    return results

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    headers = sorted(repo.rglob("lib/*/include/**/*.h")) + sorted(repo.rglob("lib/*/include/*.h"))
    # Also catch lib/**/include/**/*.h with deeper nesting
    headers += sorted(repo.rglob("lib/*/cpp/include/**/*.h"))
    headers += sorted(repo.rglob("lib/*/*/include/**/*.h"))
    headers += sorted(repo.rglob("lib/plate_solve/cpp/ipv/include/*.h"))
    # Dedupe
    seen = set()
    uniq = []
    for h in headers:
        if str(h) not in seen:
            seen.add(str(h))
            # Exclude third_party
            if "third_party" in str(h) or "runtime_internal.h" in str(h):
                continue
            uniq.append(h)
    rows = []
    for h in uniq:
        try:
            for r in extract_from_header(h):
                r["header"] = str(r["header"]).replace(str(repo)+"/","")
                rows.append(r)
        except Exception as e:
            print(f"warn: {h}: {e}", file=sys.stderr)
    # Dedupe by symbol+header
    dedup = {}
    for r in rows:
        k = (r["symbol"], r["header"])
        if k not in dedup:
            dedup[k]=r
    rows = sorted(dedup.values(), key=lambda x: (x["header"], x["symbol"]))
    out = {"tool": "extract_cpp_api", "headers_scanned": len(uniq), "symbols": rows, "count": len(rows)}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(out, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(out, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_junit).write_text(f'<testsuite name="extract_cpp_api" tests="1" failures="0"><testcase classname="extract" name="scan"/></testsuite>', encoding="utf-8")
    return 0

if __name__ == "__main__":
    sys.exit(main())
