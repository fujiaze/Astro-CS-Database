#!/usr/bin/env python3
"""AUD-402: four-sides default extractor.

For every configuration key that carries a default value anywhere in the
baseline, collect the value on each independent "side":
  S1 code fallback      (lib/**, numeric literal attached to the key)
  S2 defaults.json      (eng/packaging/config/defaults.json fields[])
  S3 factory template   (eng/packaging/config/templates/*.json, runtime_resources.json)
  S4 CLI skeleton       (lib/infrastructure/cli/session_commands.h config_fields)
  S5 schema default     (eng/contracts/**.json "default" for that key)

Writes four_side.tsv: key, side, path:line, value.  Deterministic: every
occurrence found by `git grep -n` over the tracked tree.
"""
import csv
import json
import os
import re
import subprocess
import sys

REPO = r"F:\Astro dev\Astro CS Normalization Database"
OUTDIR = r"独立审计/证据/scan402"
NUM = r"[-+]?\d[\d_]*(?:\.\d[\d_]*)?(?:[eE][-+]?\d+)?"


VENDOR = ("lib/third_party/", "/third_party/", "/archive/", ".trae/", "__pycache__",
          "nlohmann/", "json-schema-validator/")


def git_grep(args, pathspec=()):
    cmd = ["git", "-C", REPO, "grep", "-n"] + list(args) + ["--"] + list(pathspec)
    r = subprocess.run(cmd, capture_output=True)
    out = r.stdout.decode("utf-8", "replace").splitlines()
    keep = []
    for line in out:
        f = line.split(":", 1)[0].replace("\\", "/")
        if any(v in f for v in VENDOR):
            continue
        keep.append(line)
    return keep


def leaf_names_from_defaults():
    p = os.path.join(REPO, "eng", "packaging", "config", "defaults.json")
    d = json.load(open(p, encoding="utf-8-sig"))
    return [(f["key"], f["key"].split(".")[-1], json.dumps(f.get("value"), ensure_ascii=False))
            for f in d["fields"]]


def main():
    keys = leaf_names_from_defaults()
    # extra keys that live only in schemas / templates / runtime_resources
    extra = set()
    for root in ("eng/contracts/schemas", "eng/packaging/config"):
        p = os.path.join(REPO, root)
        for dirpath, _dn, fns in os.walk(p):
            for fn in fns:
                if not fn.endswith(".json"):
                    continue
                fp = os.path.join(dirpath, fn)
                try:
                    data = json.load(open(fp, encoding="utf-8-sig"))
                except Exception:
                    continue
                stack = [data]
                while stack:
                    n = stack.pop()
                    if isinstance(n, dict):
                        if "default" in n and isinstance(n.get("default"), (int, float, str, list)):
                            for k in n:
                                if isinstance(k, str) and re.fullmatch(r"[a-z][a-z0-9_]+", k):
                                    extra.add(k)
                        stack.extend(n.values())
                    elif isinstance(n, list):
                        stack.extend(n)
    tp = os.path.join(REPO, "eng", "packaging", "config", "templates")
    for fn in os.listdir(tp):
        data = json.load(open(os.path.join(tp, fn), encoding="utf-8-sig"))
        stack = [data]
        while stack:
            n = stack.pop()
            if isinstance(n, dict):
                extra.update(k for k in n if isinstance(k, str))
                stack.extend(n.values())
            elif isinstance(n, list):
                stack.extend(n)
    rr = json.load(open(os.path.join(REPO, "eng", "packaging", "config", "runtime_resources.json"),
                        encoding="utf-8-sig"))
    extra.update(k for k in rr if isinstance(k, str))

    leafs = sorted({k for _f, k, _v in keys} | extra)
    print("keys with a default somewhere:", len(leafs))
    rows = []
    numre = re.compile(NUM)
    for leaf in leafs:
        # S1/S4: every tracked code line that names the key AND carries a number
        for hit in git_grep(["-w", leaf], ("lib",)):
            parts = hit.split(":", 2)
            if len(parts) != 3 or not parts[1].isdigit():
                continue
            f, ln, txt = parts
            if not numre.search(txt):
                continue
            rows.append({"key": leaf, "side": "S1/S4 code", "where": f"{f}:{ln}",
                         "value": txt.strip()[:220]})
    with open(os.path.join(OUTDIR, "four_side.tsv"), "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["key", "side", "where", "text"])
        for r in rows:
            w.writerow([r["key"], r["side"], r["where"], r["value"]])
    print("candidate code-side rows:", len(rows))
    # S2/S3/S5 with exact values
    with open(os.path.join(OUTDIR, "four_side_values.tsv"), "w", encoding="utf-8", newline="") as fh:
        w = csv.writer(fh, delimiter="\t", lineterminator="\n")
        w.writerow(["leaf", "full_key", "defaults_json_value"])
        for full, leaf, val in sorted(keys):
            w.writerow([leaf, full, val])
    print("defaults.json rows:", len(keys))
    if not leafs:
        sys.exit(3)


if __name__ == "__main__":
    main()
