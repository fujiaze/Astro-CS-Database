#!/usr/bin/env python3
"""AUD-402: production-only view of the four-side default candidates.

Drops test/tool/doc/fixture sites from four_side_tight.tsv so that what is left
is the set of places a *default* can actually live in the shipped code.
"""
import collections
import sys

PROD_DROP = ("/tests/", "/test/", "/tools/", "/fixtures/", "/oracle/", "/bench/",
             "/examples/", "/probe", "memory.md", "README", ".md:", ".txt:",
             "/p1cal/", "/p1cos/", "/p1drz/", "/p1noise/", "/p1phot/", "/p1hips/",
             "/integration/", "test_", "_test", "tests_core", "oracle.hpp")


def main():
    src = r"独立审计/证据/scan402\four_side_tight.tsv"
    dst = r"独立审计/证据/scan402\four_side_prod.tsv"
    groups = collections.defaultdict(list)
    total = 0
    for line in open(src, encoding="utf-8"):
        if line.startswith("key\t"):
            continue
        parts = line.rstrip("\n").split("\t")
        if len(parts) < 3:
            continue
        key, where, site = parts[:3]
        total += 1
        if any(d in where for d in PROD_DROP):
            continue
        groups[key].append((where, site))
    with open(dst, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("key\tsite_count\twhere\tsite\n")
        for k in sorted(groups):
            for w, s in groups[k]:
                fh.write(f"{k}\t{len(groups[k])}\t{w}\t{s}\n")
    print("tight rows:", total, "production-only rows:", sum(len(v) for v in groups.values()),
          "keys:", len(groups))
    if not groups:
        sys.exit(4)
    for k in (sys.argv[1:] or []):
        print("###", k)
        for w, s in groups.get(k, []):
            print("   ", w, "|", s)


if __name__ == "__main__":
    main()
