# -*- coding: utf-8 -*-
"""AUDIT-06 D4 prescan :: side-4 extractor (machine contracts / JSON schemas).

Read-only. Writes only into this prescan dir.
Collects every `default` / `enum` / `minimum` / `exclusiveMinimum` /
`maximum` / `exclusiveMaximum` / `const` occurrence in every tracked schema
under eng/contracts/ and docs/contracts/, keyed by JSON-pointer-like path.
"""
import json
import io
import os
import sys

REPO = sys.argv[1]
OUT = os.path.dirname(os.path.abspath(__file__))

KEYS = ("default", "enum", "minimum", "exclusiveMinimum",
        "maximum", "exclusiveMaximum", "const")


def walk(node, path, hits):
    if isinstance(node, dict):
        for k in KEYS:
            if k in node:
                hits.append((path, k, json.dumps(node[k], ensure_ascii=False)))
        for k, v in node.items():
            if isinstance(v, (dict, list)):
                walk(v, path + "/" + k, hits)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            walk(v, path + "[%d]" % i, hits)


def main():
    collected = {}
    for root, dirs, files in os.walk(os.path.join(REPO, "eng")):
        if os.sep + "third_party" in root:
            continue
        for fn in files:
            if not fn.endswith(".schema.json"):
                continue
            p = os.path.join(root, fn)
            try:
                d = json.load(io.open(p, encoding="utf-8"))
            except Exception as e:                       # noqa: BLE001
                sys.stderr.write("SKIP %s: %s\n" % (p, e))
                continue
            hits = []
            walk(d, "#", hits)
            collected[os.path.relpath(p, REPO).replace(os.sep, "/")] = hits
    with io.open(os.path.join(OUT, "side4_schema.json"), "w", encoding="utf-8") as f:
        json.dump(collected, f, ensure_ascii=False, indent=1)
    n = sum(len(v) for v in collected.values())
    sys.stdout.write("files=%d constraint-entries=%d\n" % (len(collected), n))
    for k in sorted(collected):
        sys.stdout.write("  %-70s %d\n" % (k, len(collected[k])))


if __name__ == "__main__":
    main()
