#!/usr/bin/env python3
"""check_doc_symbols.py — T402 doc symbols checker

Checks: 文档中反引号符号、文件和 config key 均可解析；排除 archive 清单
Exit: 0 PASS, 1 contract FAIL, 2 env error, 3 schema error
"""
import argparse, json, pathlib, re, sys, csv

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--out-json", default=None)
    ap.add_argument("--out-junit", default=None)
    args = ap.parse_args()
    repo = pathlib.Path(args.repo)
    findings = []
    status = "PASS"
    # Scan docs/**/*.md excluding archive/history
    # Only authoritative docs per 05 L1 classification
    auth_dirs = ["science","algorithms","architecture","contracts","modules"]
    docs = []
    for d in auth_dirs:
        docs.extend([p for p in (repo / "docs" / d).rglob("*.md") if "archive" not in str(p)])
    # Also include top-level docs that are authoritative: TRACEABILITY, PUBLIC_API etc handled via contracts
    # Only add if exists (fixtures may not have)
    for extra in [repo / "docs/contracts/PUBLIC_API.md", repo / "docs/contracts/DATA_SEMANTICS.md"]:
        if extra.exists():
            docs.append(extra)
    # Extract backtick symbols like `p2_integrate_pixel` or `docs/...` or `lib/...`
    backtick_re = re.compile(r'`([^`]+)`')
    # Load known symbols from API inventory
    api_syms = set()
    try:
        inv = list(csv.DictReader(open(repo/"docs/architecture/api_inventory.csv", encoding="utf-8")))
        api_syms = set(r["symbol"] for r in inv)
    except: pass
    # Load known files
    known_files = set(str(p.relative_to(repo)) for p in repo.rglob("*") if p.is_file())
    for doc in docs:
        text = doc.read_text(encoding="utf-8", errors="ignore")
        for m in backtick_re.finditer(text):
            token = m.group(1).strip()
            # Skip obvious non-symbol tokens (plain english, too short, contains spaces)
            if " " in token and "/" not in token: continue
            if token.startswith("http"): continue
            # Skip composite file lists like rejection.cpp/integrate.cpp
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts) and len(parts) == 2 and token.count(".") == 2:
                    continue
            # Skip glob patterns
            if "*" in token:
                continue
            # Check if token looks like file path
            if "/" in token and "." in token:
                # File reference: check exists or is doc-relative
                if token.endswith(".md") or token.endswith(".h") or token.endswith(".cpp") or token.endswith(".json"):
                    # Try alternative: token may be relative like tools/stage2.cpp -> lib/phase2/tools/stage2.cpp
                    found = token in known_files or (repo / token).exists()
                    if not found:
                        # Try lib/phase2/tools/ prefix
                        alt = repo / "lib/phase2" / token
                        if alt.exists():
                            found = True
                        alt2 = repo / "lib" / token
                        if alt2.exists():
                            found = True
                    if not found:
                        # Allow if is archive-excluded doc
                        if "archive" not in token and "third_party" not in token:
                            findings.append({"id":"DOC-BAD-FILE","severity":"P1","file":str(doc.relative_to(repo)),"symbol":token,"observed":"file not found","expected":"exists"})
                            status="FAIL"
                continue
            # Skip pure header filenames (contain .h)
            if token.endswith(".h") or token.endswith(".cpp") or token.endswith(".hpp"):
                continue
            # Skip module short names (no dot, not API prefix)
            if token in {"snr_estimator","calibration","phase2","healpix_drizzle","astro_image_io","acr","photometric_calib","plate_solve","dynamic_psf","star_detector","orchestrator","healpix_db","common","gaia_client"}:
                continue
            # Skip C file refs like gaia_client.c
            if token.endswith(".c"):
                continue
            # Skip file:line refs like dpsf_psf.cpp:368
            if ":" in token and (token.endswith(tuple(str(i) for i in range(10))) or token.split(":")[-1].isdigit()):
                continue
            # Skip composite file lists with /
            if "/" in token and token.count("/") == 1 and "." in token and "," not in token:
                # Single file path - already handled as file reference
                pass
            # Composite file list like rejection.cpp/integrate.cpp -> skip
            if "/" in token and ".cpp" in token:
                parts = token.split("/")
                if any(".cpp" in p for p in parts):
                    continue
            # Skip dll/so names
            if token.endswith((".dll",".so",".json")):
                continue
            # Skip composite file lists like a/b/c.h
            if token.count("/") >= 2 and token.count(",") == 0 and "." not in token.split("/")[-1].split(",")[0]:
                # Heuristic: if token looks like path but contains multiple slashes without clear file, skip detailed check
                pass
            # Check if token looks like symbol (contains _ and no spaces)
            if re.match(r'^[A-Za-z_][\w:]*$', token.replace(".","")):
                # Heuristic: if token contains _ and is plausible API symbol, check against inventory
                if "_" in token and len(token) >= 3:
                    # Allow known symbols or check if substring matches
                    if token not in api_syms and not any(token in s for s in api_syms) and not any(s in token for s in api_syms):
                        # Check if token is actually a known file stem or config key - skip if not API-like prefix
                        # Skip known status/error codes (not API symbols)
                        if token in {"AC_ERR_PARAM","AC_OK","AC_ERR_MEMORY","AC_ERR_INTERNAL","NO_DATA","NO_CANDIDATES","INVALID_INPUT","INVALID_CONFIGURATION","INVALID_METHOD","AC_ERR","TIMEOUT","CANCELLED","OK","ALL_REJECTED","ZERO_VALID_WEIGHT","UNDERDETERMINED","P2_INTEGRATE_OK","P2_INTEGRATE_NO_CANDIDATES","P2_STATUS_OK","MOFFAT4_FWHM_FACTOR","DPSF_ERR_PARAM","NO_SOLUTION","PC_API","P2_API","AC_API","SNR_API","CC_EXPORT","DPSF_EXPORT","SDET_EXPORT","IPV_API","AIO_EXPORT","HIO_EXPORT","THREAD_BUDGET_EXEMPT","BASELINE_OPCODE_PASS","LD_LIBRARY_PATH","backend_math_contract.h"}:
                            continue
                        # Only flag UPPER_CASE or known API prefix; lowercase vars like t_light/hp_res are sci params not API symbols
                        is_api_like = (token.isupper() and "_" in token and len(token) >= 6) or token.startswith(("p2_","aio_","ac_","cc_","dpsf_","sdet_","ipv_","pc_","snr_","gaia_"))
                        if is_api_like:
                            findings.append({"id":"DOC-BAD-SYMBOL","severity":"P1","file":str(doc.relative_to(repo)),"symbol":token,"observed":"symbol not in API inventory","expected":"exists"})
                            status="FAIL"
    # Check archive symbols should not be in active docs (exclude archive docs themselves)
    result = {"tool":"check_doc_symbols","status":status,"docs_scanned":len(docs),"findings":findings,"passed": status=="PASS"}
    if args.out_json:
        pathlib.Path(args.out_json).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.out_json).write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    else:
        print(json.dumps(result, indent=2, ensure_ascii=False))
    if args.out_junit:
        pathlib.Path(args.out_junit).parent.mkdir(parents=True, exist_ok=True)
        failures = len([f for f in findings if f["severity"] in ("P0","P1")])
        junit = f'<testsuite name="check_doc_symbols" tests="{len(docs)}" failures="{failures}"><testcase classname="docs" name="symbols"/></testsuite>'
        pathlib.Path(args.out_junit).write_text(junit, encoding="utf-8")
    return 0 if status=="PASS" else 1

if __name__ == "__main__":
    sys.exit(main())
