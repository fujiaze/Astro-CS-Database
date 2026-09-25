#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Independent replication of the doc line-anchor regex surface (read-only).

Purpose: measure how many `path:line` anchors the shipped gate can actually see
versus how many exist in docs/**, using the gate's own regex components copied
verbatim from docs/algorithms/anchors/check_doc_line_anchors.py:56-62.
"""
import glob
import io
import os
import re
import sys

ROOT = r"F:\Astro dev\Astro CS Normalization Database"

EXTS = ("cpp", "cc", "cxx", "h", "hpp", "hh", "py", "sh", "ps1", "txt",
        "json", "yaml", "yml", "md", "cmake", "in")
_FILE = (r"(?<![\w./-])((?:[A-Za-z0-9_][A-Za-z0-9_.-]*/)*"
         r"[A-Za-z0-9_][A-Za-z0-9_.-]*\.(?:" + "|".join(EXTS) + r"))")
_ONE = r"[:#]L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?"
_CONT = r"(?:\s*[/,]\s*:?L?(\d+)(?:\s*[-\u2013]\s*L?(\d+))?)*"
ANCHOR_RE = re.compile(_FILE + _ONE + _CONT)
# widened variant: allow a closing backtick/quote between path and the colon
WIDE_RE = re.compile(_FILE + r"[`'\"]?" + _ONE + _CONT)

ARCHIVE_MARKERS = ("/archive/", "/legacy/", "/.git/", "/third_party/",
                   "/build/", "/out/", "/run/", "/worktrees/")

line_cache = {}


def nlines(rel):
    p = os.path.join(ROOT, rel.replace("/", os.sep))
    if p in line_cache:
        return line_cache[p]
    n = -1
    if os.path.isfile(p):
        with io.open(p, encoding="utf-8", errors="replace") as fh:
            n = sum(1 for _ in fh)
    line_cache[p] = n
    return n


def scan(pattern):
    docs = sorted(glob.glob(os.path.join(ROOT, pattern), recursive=True))
    seen_strict, seen_wide = set(), set()
    docs_with_strict, docs_with_wide = set(), set()
    wide_only_hits = []
    for d in docs:
        rel = os.path.relpath(d, ROOT).replace(os.sep, "/")
        if any(m in "/" + rel + "/" for m in ARCHIVE_MARKERS):
            continue
        with io.open(d, encoding="utf-8", errors="replace") as fh:
            for ln, text in enumerate(fh, 1):
                for m in ANCHOR_RE.finditer(text):
                    seen_strict.add((rel, ln, m.group(1), m.group(2)))
                    docs_with_strict.add(rel)
                for m in WIDE_RE.finditer(text):
                    seen_wide.add((rel, ln, m.group(1), m.group(2)))
                    docs_with_wide.add(rel)
                    if (rel, ln, m.group(1), m.group(2)) not in seen_strict:
                        wide_only_hits.append((rel, ln, m.group(1), m.group(2)))
    return (docs, seen_strict, seen_wide, docs_with_strict,
            docs_with_wide, wide_only_hits)


def main():
    (docs, strict, wide, dws, dww, wide_only) = scan("docs/**/*.md")
    print("docs_in_scope(md, non-archive)=%d" % len(docs))
    print("anchors_visible_to_shipped_regex=%d in %d docs" % (len(strict), len(dws)))
    print("anchors_under_widened_regex   =%d in %d docs" % (len(wide), len(dww)))
    print("newly_visible_if_widened      =%d" % len(wide_only))
    # how many of the newly visible are unresolvable / out of bounds?
    bad_path, oob = [], []
    for rel, ln, path, num in wide_only:
        n = nlines(path)
        if n < 0:
            # try unique basename resolution like C2
            base = os.path.basename(path)
            cand = [r for r in glob.glob(os.path.join(ROOT, "**", base), recursive=True)
                    if not any(m in "/" + os.path.relpath(r, ROOT).replace(os.sep, "/") + "/"
                               for m in ARCHIVE_MARKERS)]
            if len(cand) != 1:
                bad_path.append((rel, ln, path))
                continue
            n = sum(1 for _ in io.open(cand[0], encoding="utf-8", errors="replace"))
        if n >= 0 and num and int(num) > n:
            oob.append((rel, ln, path, num, n))
    print("newly_visible_unresolvable_path=%d" % len(bad_path))
    print("newly_visible_out_of_bounds    =%d" % len(oob))
    owner_strict = [a for a in strict if a[0].startswith("docs/owner/")]
    owner_wide = [a for a in wide if a[0].startswith("docs/owner/")]
    print("docs/owner anchors strict=%d wide=%d" % (len(owner_strict), len(owner_wide)))
    print("docs/owner newly visible:")
    for h in sorted(set(owner_wide) - set(owner_strict)):
        print("   %s:%d  %s :%s" % h)
    print("sample out-of-bounds (max 15):")
    for x in oob[:15]:
        print("   %s:%d %s line %s (file %d lines)" % x)


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    main()
