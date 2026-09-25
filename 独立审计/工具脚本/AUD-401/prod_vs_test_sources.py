#!/usr/bin/env python3
"""AUD-401 step 3: split lib TUs by *production* build membership.

  PROD   : named by a listing stmt of a production target (add_library anywhere in
           the root closure; add_executable(acsd) and non-test executables)
  TEST   : named only by test/tool targets (name matches test/probe patterns or the
           declaring cmake file lives under a tests/ directory)
  ALONE  : named only by CMakeLists outside the root closure (standalone build trees)
  ORPHAN : not named by any listing stmt in the repo

Basename matching is used in addition to path resolution because the repo lists
sources through ${VAR}/../ prefixes (same rule as classify_sources.py).

Usage: python3 prod_vs_test_sources.py --root <repo> --closure <inv dir> --out <dir>
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from census_build_graph import statements, resolve_token, SRC_RE, LISTING_CMDS  # noqa: E402

TU_EXT = (".cpp", ".c", ".cc", ".cxx")
SKIP_KW = {"STATIC", "SHARED", "MODULE", "INTERFACE", "IMPORTED", "ALIAS",
           "EXCLUDE_FROM_ALL", "PRIVATE", "PUBLIC", "OBJECT", "WIN32",
           "HEADER_FILE_ONLY", "REQUIRED"}
TEST_NAME = re.compile(r"(^|_)test|test_|_tests?$|probe|bench|selftest|_gate$|^noop$|^echo$|fixture|interposer")


def is_test_target(name, cmakefile):
    p = cmakefile.replace("\\", "/").lower()
    if "/tests/" in "/" + p or p.startswith("eng/tests/") or "/test/" in p:
        return True
    if TEST_NAME.search(name.lower()):
        return True
    return False


def collect(cmake_files, root):
    """-> (listings: list of (kind, target, resolved_or_None, basename, where))"""
    listings = []
    for rel in cmake_files:
        f = root / rel
        if not f.is_file():
            continue
        base = f.parent
        cur_target = None
        for cmd, argtext, ln in statements(f.read_text(encoding="utf-8", errors="replace")):
            cl = cmd.lower()
            if cl not in LISTING_CMDS:
                continue
            toks = re.findall(r"[^\s()]+", argtext)
            if not toks:
                continue
            name = toks[0].strip("\"'")
            if cl == "add_library":
                kind = "lib"
            elif cl == "add_executable":
                kind = "exe"
            else:
                kind = cur_target or "lib"
            cur_target = name
            for tok in toks[1:] if cl in ("add_library", "add_executable") else toks:
                t = tok.strip("\"'")
                if t.upper() in SKIP_KW or not SRC_RE.search(t):
                    continue
                rp, rk = resolve_token(base, tok, root)
                b = os.path.basename(re.sub(r"\$\{[^}]*\}", "", t)).lstrip("/")
                listings.append((kind, name, rp if rk == "ok" else None, b, f"{rel}:{ln}"))
    return listings


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--closure", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    root = Path(args.root).resolve()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    closure = [l.split("\t")[0] for l in
               (Path(args.closure) / "graph_cmake_files.txt").read_text(encoding="utf-8").splitlines()
               if l.strip() and "${" not in l.split("\t")[0]]

    tracked = [t.replace("\\", "/") for t in subprocess.check_output(
        ["git", "-C", str(root), "ls-files"], text=True, errors="replace").splitlines()]
    all_cmake = [t for t in tracked if t.endswith("CMakeLists.txt") or t.endswith(".cmake")]

    cl_list = collect(closure, root)
    all_list = collect(all_cmake, root)

    def bucket_of(path):
        b = os.path.basename(path)
        prod_h, test_h, alone_h = [], [], []
        for kind, name, rp, bn, where in cl_list:
            if rp == path or bn == b:
                (test_h if is_test_target(name, where.split(":")[0]) else prod_h).append(f"{name}@{where}")
        if not prod_h and not test_h:
            for kind, name, rp, bn, where in all_list:
                if rp == path or bn == b:
                    alone_h.append(f"{name}@{where}")
        if prod_h:
            return "PROD", ";".join(sorted(set(prod_h))[:4])
        if test_h:
            return "TEST", ";".join(sorted(set(test_h))[:4])
        if alone_h:
            return "ALONE", ";".join(sorted(set(alone_h))[:4])
        return "ORPHAN", ""

    lib_tu = [t for t in tracked if t.startswith("lib/") and os.path.splitext(t)[1].lower() in TU_EXT]
    rows = [(path,) + bucket_of(path) for path in sorted(lib_tu)]
    (out / "source_prod_membership.tsv").write_text(
        "bucket\tpath\tfirst_targets\n" + "\n".join(f"{b}\t{p}\t{e}" for p, b, e in rows) + "\n",
        encoding="utf-8")

    cnt = {}
    for _p, b, _e in rows:
        cnt[b] = cnt.get(b, 0) + 1
    print(f"tracked lib TUs {len(lib_tu)}: PROD={cnt.get('PROD',0)} TEST-only={cnt.get('TEST',0)}"
          f" standalone-only={cnt.get('ALONE',0)} ORPHAN={cnt.get('ORPHAN',0)}")
    for tag in ("ORPHAN", "TEST"):
        print(f"\n=== {tag} ===")
        for p, b, e in rows:
            if b == tag and "/test" not in p:
                print(f"  {p}    [{e}]")
    # module-level rollup for TEST-only
    print("\n=== TEST-only TUs under lib/algorithms|infrastructure (non tests/ dir) ===")
    for p, b, e in rows:
        if b == "TEST" and "/tests/" not in p and "/test/" not in p:
            print(f"  {p}    [{e}]")


if __name__ == "__main__":
    sys.exit(main())
