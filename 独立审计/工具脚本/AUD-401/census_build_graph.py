#!/usr/bin/env python3
"""AUD-401: deterministic CMake build-graph census (read-only, no build executed).

Method
  1. BFS the root CMakeLists.txt `add_subdirectory()` closure (commented-out
     lines are ignored) -> "graph cmake files".
  2. For each graph cmake file, split into *statements* (command name + arg
     text up to the balanced closing paren), ignoring `#` comment lines and
     trailing `#` comments.
  3. From statements whose command is one of
        add_library / add_executable / set / target_sources
     collect every argument token that looks like a file path
     (.cpp/.c/.cc/.cxx/.h/.hpp/.in/.in).  Tokens starting with '$' that are not
     ${CMAKE_CURRENT_SOURCE_DIR}/... style are recorded verbatim as "unresolved".
  4. Compare against `git ls-files lib/**` -> sources never named by a
     compile-listing statement anywhere in the graph.

Outputs (in --out):
  graph_cmake_files.txt   closure + the line that pulled each one in
  graph_targets.txt       target -> source list (resolved repo-relative)
  graph_sources.txt       every repo-relative source named by a listing stmt
  graph_other_cmds_hits.txt  source-ish tokens named by NON-listing stmts
  uncovered_cpp.txt / uncovered_hdr.txt   tracked lib sources with no listing hit

Usage: python3 census_build_graph.py --root "<repo>" --out "<dir>"
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

SRC_RE = re.compile(r"\.(cpp|c|cc|cxx|h|hpp|in)$", re.I)
LISTING_CMDS = {"add_library", "add_executable", "set", "target_sources"}
NON_LISTING_CMDS = {"target_include_directories", "target_link_libraries", "install",
                    "add_test", "add_custom_command", "add_custom_target",
                    "target_compile_definitions", "set_tests_properties",
                    "add_dependencies", "source_group", "configure_file", "file"}
TOKEN_RE = re.compile(r"[^\s()]+")


def read_text(p: Path):
    try:
        return p.read_text(encoding="utf-8", errors="replace")
    except Exception:
        return ""


def statements(text):
    """Yield (command, arg_text, start_line_no). Ignores comment-only lines and
    strips trailing comments not inside quotes."""
    out_lines = []
    for ln in text.splitlines():
        s = ln
        # strip trailing '#' comments unless inside quotes (crude but adequate here)
        if s.lstrip().startswith("#"):
            out_lines.append("")
            continue
        q = False
        cut = -1
        for i, ch in enumerate(s):
            if ch == '"':
                q = not q
            elif ch == "#" and not q:
                cut = i
                break
        out_lines.append(s if cut < 0 else s[:cut])
    joined = "\n".join(out_lines)
    # walk for command( ... )
    i, n, lineno = 0, len(joined), 1
    results = []
    line_of = [1] * (n + 2)
    cur = 1
    for k, ch in enumerate(joined):
        line_of[k] = cur
        if ch == "\n":
            cur += 1
    pat = re.compile(r"([A-Za-z_][\w:]*)\s*\(")
    pos = 0
    while True:
        m = pat.search(joined, pos)
        if not m:
            break
        cmd = m.group(1)
        depth = 1
        j = m.end()
        q = False
        while j < n and depth > 0:
            ch = joined[j]
            if ch == '"':
                q = not q
            elif not q:
                if ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
            j += 1
        args = joined[m.end():j - 1]
        results.append((cmd, args, line_of[m.start()]))
        pos = m.end() if m.end() > pos else pos + 1
    return results


def resolve_token(base_dir: Path, tok: str, root: Path):
    tok = tok.strip().strip("\"'")
    if not tok:
        return None, "empty"
    if tok.startswith("${CMAKE_CURRENT_SOURCE_DIR}/") or tok.startswith("${CMAKE_CURRENT_LIST_DIR}/"):
        for pre in ("${CMAKE_CURRENT_SOURCE_DIR}/", "${CMAKE_CURRENT_LIST_DIR}/"):
            if tok.startswith(pre):
                tok = tok[len(pre):]
        tok = re.sub(r"\$\{[^}]*\}", "VAR", tok)
    elif tok.startswith("${") or tok.startswith("$<"):
        return None, "unresolved"
    if os.path.isabs(tok):
        return None, "abs"
    p = (base_dir / tok)
    rp = os.path.relpath(p, root).replace("\\", "/")
    return rp, "ok"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    root = Path(args.root).resolve()
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # 1) closure
    seen, queue, order = set(), [("CMakeLists.txt", "(root)")], []
    while queue:
        rel, why = queue.pop(0)
        if rel in seen:
            continue
        seen.add(rel)
        order.append((rel, why))
        cur = root / rel
        if not cur.is_file():
            continue
        for cmd, argtext, ln in statements(read_text(cur)):
            if cmd.lower() != "add_subdirectory":
                continue
            for tok in TOKEN_RE.findall(argtext):
                if tok.startswith("${") or tok.startswith("$<"):
                    continue
                tgt = (cur.parent / tok.strip("\"'"))
                relsub = os.path.relpath(tgt, root).replace("\\", "/") + "/CMakeLists.txt"
                if relsub not in seen:
                    queue.append((relsub, f"{rel}:{ln}: add_subdirectory({tok})"))

    # 2) listing statements
    sources, targets, other_hits, unresolved = {}, {}, {}, set()
    for rel, _why in order:
        cur = root / rel
        if not cur.is_file():
            continue
        base = cur.parent
        for cmd, argtext, ln in statements(read_text(cur)):
            cl = cmd.lower()
            toks = TOKEN_RE.findall(argtext)
            if cl in LISTING_CMDS:
                srcs = []
                name = toks[0] if toks else ""
                for tok in toks[1:]:
                    if tok.upper() in {"STATIC", "SHARED", "MODULE", "INTERFACE", "IMPORTED",
                                       "ALIAS", "EXCLUDE_FROM_ALL", "WIN32", "OBJECT",
                                       "HEADER_FILE_ONLY", "PRIVATE", "PUBLIC", "INTERFACE"}:
                        continue
                    if not SRC_RE.search(tok.strip("\"'")):
                        continue
                    rp, kind = resolve_token(base, tok, root)
                    if kind == "ok":
                        sources.setdefault(rp, []).append(f"{rel}:{ln}:{cmd}:{tok}")
                        srcs.append(rp)
                    elif kind == "unresolved":
                        unresolved.add(f"{rel}:{ln}:{cmd}:{tok}")
                if cl in ("add_library", "add_executable"):
                    targets[name] = (rel, ln, srcs)
            elif cl in NON_LISTING_CMDS:
                for tok in toks:
                    t = tok.strip("\"'")
                    if SRC_RE.search(t) and ("/" in t or "\\" in t):
                        rp, kind = resolve_token(base, tok, root)
                        if kind == "ok":
                            other_hits.setdefault(rp, []).append(f"{rel}:{ln}:{cmd}")

    # 3) tracked sources
    tracked = subprocess.check_output(["git", "-C", str(root), "ls-files", "lib"],
                                      text=True, errors="replace").splitlines()
    tracked = [t.replace("\\", "/") for t in tracked]
    lib_cpp = [t for t in tracked if SRC_RE.search(t) and os.path.splitext(t)[1].lower() in (".cpp", ".c", ".cc", ".cxx")]
    lib_hdr = [t for t in tracked if SRC_RE.search(t) and os.path.splitext(t)[1].lower() in (".h", ".hpp", ".in")]

    def hit_set(items):
        cov, part = [], []
        for p in items:
            if p in sources:
                cov.append(p)
            elif p in other_hits:
                part.append(p)
            else:
                part.append(p)
        return cov, part

    unc_cpp = [p for p in lib_cpp if p not in sources]
    unc_hdr = [p for p in lib_hdr if p not in sources]
    only_other = [p for p in unc_cpp + unc_hdr if p in other_hits]

    (out / "graph_cmake_files.txt").write_text(
        "\n".join(f"{r}\t{w}" for r, w in order) + "\n", encoding="utf-8")
    (out / "graph_targets.txt").write_text("\n".join(
        f"{n}\t{v[0]}:{v[1]}\t{len(v[2])}\t" + ";".join(v[2]) for n, v in sorted(targets.items())) + "\n",
        encoding="utf-8")
    (out / "graph_sources.txt").write_text("\n".join(
        f"{k}\t" + " | ".join(v) for k, v in sorted(sources.items())) + "\n", encoding="utf-8")
    (out / "graph_other_cmds_hits.txt").write_text("\n".join(
        f"{k}\t" + " | ".join(v) for k, v in sorted(other_hits.items())) + "\n", encoding="utf-8")
    (out / "unresolved_tokens.txt").write_text("\n".join(sorted(unresolved)) + "\n", encoding="utf-8")
    (out / "uncovered_cpp.txt").write_text("\n".join(sorted(unc_cpp)) + "\n", encoding="utf-8")
    (out / "uncovered_hdr.txt").write_text("\n".join(sorted(unc_hdr)) + "\n", encoding="utf-8")

    print(f"closure cmake files        : {len(order)}")
    print(f"targets in closure         : {len(targets)}")
    print(f"sources named by listing   : {len(sources)}")
    print(f"tracked lib .cpp/.c        : {len(lib_cpp)}   NOT named: {len(unc_cpp)}")
    print(f"tracked lib .h/.hpp/.in    : {len(lib_hdr)}   NOT named: {len(unc_hdr)}")
    print(f"  of the unnamed, seen only in non-listing cmds: {len(only_other)}")
    print("targets:", ", ".join(sorted(targets)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
