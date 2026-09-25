#!/usr/bin/env python3
"""AUD-401 step 2 (v2): classify every tracked lib TU by build-graph membership.

Listing statements = add_library / add_executable / set / target_sources, with
comment lines stripped.  A source file is "named" if its *path* resolves into the
listing, OR its *basename* appears in a listing argument (the repo lists many
sources through ${VAR}/... and ../... prefixes, which are not path-resolvable
without configuring CMake).

Buckets
  A  named by a listing stmt inside the root add_subdirectory closure
     (sub-tag A-exact when the resolved relative path matched)
  B  named only by CMakeLists outside the closure (standalone build trees: acr,
     hips_browser, cfitsio, per-module standalone guards, ...)
  C  named by NO listing statement in any tracked CMakeLists -> true orphan TU

Usage: python3 classify_sources.py --root <repo> --closure <inv dir> --out <dir>
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from census_build_graph import statements, resolve_token, SRC_RE, LISTING_CMDS  # noqa: E402

SKIP_KW = {"STATIC", "SHARED", "MODULE", "INTERFACE", "IMPORTED", "ALIAS",
           "EXCLUDE_FROM_ALL", "PRIVATE", "PUBLIC", "OBJECT", "WIN32",
           "HEADER_FILE_ONLY", "REQUIRED"}
TU_EXT = (".cpp", ".c", ".cc", ".cxx")


def listing_hits(cmake_files, root):
    """returns (resolved_paths dict, per-file basename dict)"""
    resolved, basenames = {}, {}
    for rel in cmake_files:
        f = root / rel
        if not f.is_file():
            continue
        base = f.parent
        for cmd, argtext, ln in statements(f.read_text(encoding="utf-8", errors="replace")):
            if cmd.lower() not in LISTING_CMDS:
                continue
            for tok in re.findall(r"[^\s()]+", argtext):
                t = tok.strip("\"'")
                if t.upper() in SKIP_KW:
                    continue
                if not SRC_RE.search(t):
                    continue
                rp, kind = resolve_token(base, tok, root)
                b = os.path.basename(re.sub(r"\$\{[^}]*\}", "", t)).lstrip("/")
                if b:
                    basenames.setdefault(rel, set()).add(b)
                    basenames.setdefault("_all", set()).add(b)
                if kind == "ok" and rp.endswith(tuple(TU_EXT)):
                    resolved.setdefault(rp, []).append(f"{rel}:{ln}:{cmd}")
    return resolved, basenames


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--closure", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    root = Path(args.root).resolve()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    closure = []
    for l in (Path(args.closure) / "graph_cmake_files.txt").read_text(encoding="utf-8").splitlines():
        if not l.strip():
            continue
        rel = l.split("\t")[0]
        if "${" in rel:
            continue
        closure.append(rel)

    tracked = subprocess.check_output(["git", "-C", str(root), "ls-files"],
                                      text=True, errors="replace").splitlines()
    tracked = [t.replace("\\", "/") for t in tracked]
    all_cmake = [t for t in tracked if t.endswith("CMakeLists.txt") or t.endswith(".cmake")]

    res_c, bn_c = listing_hits(closure, root)
    res_a, bn_a = listing_hits(all_cmake, root)

    lib_tu = [t for t in tracked if t.startswith("lib/")
              and os.path.splitext(t)[1].lower() in TU_EXT]

    rows = []
    for p in sorted(lib_tu):
        b = os.path.basename(p)
        in_c = (p in res_c) or (b in bn_c.get("_all", set()))
        in_a = (p in res_a) or (b in bn_a.get("_all", set()))
        tag = "A-exact" if p in res_c else "A-basename"
        if in_c and in_a:
            bucket = "A"
            ev = ";".join(res_c.get(p, [])) or f"basename in closure listings ({tag})"
        elif in_a:
            bucket = "B"
            ev = ";".join(res_a.get(p, [])) or "basename only in non-closure listings"
        else:
            bucket = "C"
            ev = ""
        rows.append((bucket, p, ev))

    (out / "source_build_membership.tsv").write_text(
        "bucket\tpath\tevidence\n" + "\n".join(f"{b}\t{p}\t{e}" for b, p, e in rows) + "\n",
        encoding="utf-8")

    cnt = {}
    for b, _p, _e in rows:
        cnt[b] = cnt.get(b, 0) + 1
    print(f"tracked lib TUs {len(lib_tu)}: A(closure)={cnt.get('A',0)} "
          f"B(non-closure cmake only)={cnt.get('B',0)} C(no cmake anywhere)={cnt.get('C',0)}")
    print("\n=== C: no CMake target anywhere compiles this TU ===")
    for b, p, _e in rows:
        if b == "C":
            print("  " + p)
    print("\n=== B: outside root production closure ===")
    groups = {}
    for b, p, _e in rows:
        if b == "B":
            groups[os.path.dirname(p)] = groups.get(os.path.dirname(p), 0) + 1
    for k in sorted(groups, key=lambda x: -groups[x]):
        print(f"  {groups[k]:4d}  {k}")


if __name__ == "__main__":
    sys.exit(main())
