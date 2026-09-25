#!/usr/bin/env python3
"""AUD-402: narrow the four-side candidate list to real default-assignment sites.

Input : four_side.tsv (key, side, where, full line text)  -- from four_side.py
Output: four_side_tight.tsv (key, where, matched-site-value)
A hit requires the key name to sit *immediately* next to a number in one of the
shapes a default actually takes in this codebase:
   "key": N   "key" = N   "key", N   key = N   key: N   key{N}   key{ N }
"""
import collections
import re
import sys

NUM = r"[-+]?\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][-+]?\d+)?"
SHAPES = [
    r'"%s"\s*[:=]\s*' + NUM,
    r'"%s"\s*,\s*' + NUM,
    r'\b%s\s*(?:=|\+=|:|\{\{)\s*' + NUM,
    r'\b%s\{\s*' + NUM,
    r'\b%s\s*\(\s*' + NUM,
]


def main(src="four_side.tsv", dst="four_side_tight.tsv"):
    rows = [l.rstrip("\n").split("\t") for l in open(src, encoding="utf-8")][1:]
    out = []
    perkey = collections.defaultdict(list)
    for rec in rows:
        if len(rec) < 4:
            continue
        key, _side, where, txt = rec[:4]
        for sh in SHAPES:
            m = re.search(sh % key, txt)
            if m:
                out.append((key, where, m.group(0).strip()))
                perkey[key].append((where, m.group(0).strip()))
                break
    with open(dst, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("key\twhere\tsite\n")
        for r in out:
            fh.write("\t".join(r) + "\n")
    print("tight hits:", len(out), "keys with >=1 tight hit:", len(perkey))
    allkeys = {r[0] for r in rows}
    miss = sorted(allkeys - set(perkey))
    print("keys with NO tight hit:", len(miss))
    print(",".join(miss))
    if not out:
        sys.exit(4)


if __name__ == "__main__":
    main()
