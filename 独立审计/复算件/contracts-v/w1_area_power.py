"""W1 evidence collector (read-only).

For every tracked file under eng/contracts, docs/contracts, lib, report each
literal occurrence of `pixel_area_power` in a JSON-schema-ish value position and
classify the constraint it imposes (enum / const / type-only).
No repo code is imported; files are read as raw text.
"""
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")

REPO = r"F:\Astro dev\Astro CS Normalization Database"


def tracked(prefix):
    out = subprocess.run(
        ["git", "-C", REPO, "ls-files", "--", prefix],
        capture_output=True, text=True, encoding="utf-8", errors="replace")
    return [l for l in out.stdout.splitlines() if l.strip()]


def grab_object(text, start):
    """text[start] is the '{' that opens an object; return (obj_str, end_idx)."""
    depth = 0
    i = start
    in_str = False
    esc = False
    while i < len(text):
        c = text[i]
        if in_str:
            if esc:
                esc = False
            elif c == "\\":
                esc = True
            elif c == '"':
                in_str = False
        else:
            if c == '"':
                in_str = True
            elif c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    return text[start:i + 1], i + 1
        i += 1
    return None, start


def line_of(text, idx):
    return text.count("\n", 0, idx) + 1


KEY_RE = re.compile(r'"pixel_area_power"\s*:\s*\{')
ENUM_RE = re.compile(r'"enum"\s*:\s*(\[[^\]]*\])', re.S)
CONST_RE = re.compile(r'"const"\s*:\s*(-?\d+)')
TYPE_RE = re.compile(r'"type"\s*:\s*"(integer|number)"')

rows = []
for prefix in ("eng/contracts", "docs/contracts"):
    for rel in tracked(prefix):
        ap = os.path.join(REPO, rel.replace("/", os.sep))
        if not os.path.isfile(ap):
            continue
        text = io.open(ap, encoding="utf-8", errors="replace").read()
        for m in KEY_RE.finditer(text):
            body, _ = grab_object(text, m.end() - 1)
            if body is None:
                continue
            try:
                node = json.loads(body)
            except Exception:
                node = {}
            kind = []
            if "enum" in node:
                kind.append("enum=" + json.dumps(node["enum"]))
            if "const" in node:
                kind.append("const=" + json.dumps(node["const"]))
            if "type" in node and not kind:
                kind.append("type=" + str(node["type"]))
            if not kind:
                kind.append("other:" + body[:60].replace("\n", " "))
            rows.append((rel, line_of(text, m.start()), "; ".join(kind)))

print("== schema-side pixel_area_power value constraints (eng/contracts, docs/contracts) ==")
for r in sorted(set(rows)):
    print("%-72s %5d  %s" % r)
print("total schema-constraint sites:", len(rows))
print("sites with enum:", sum(1 for r in rows if "enum=" in r[2]))
print("distinct enum value sets:", sorted({r[2] for r in rows if "enum=" in r[2]}))
