"""W2 evidence collector (read-only).

Reproduces, with my own independent scan, what the
`eng/ci/check_registration_anchors.py` R1/R2 gate would and would not see:

  * walk every tracked JSON under eng/contracts and eng/ci
  * for every string value that is *exactly* a repo path (or starts with one of
    the gate's SRC_TOP / RUNTIME_TOP roots), classify by
      - key name in the gate's LIVE_KEYS set  -> gate-visible candidate
      - whether the referenced path exists in the tracked tree
      - whether the key path carries a HISTORY_KEY_MARK token (gate exempts it)
  * separately: every string anywhere that merely *mentions* a retired control
    pack path (工程控制/RELEASE-0x, run/**) inside prose values -> gate-invisible.

Nothing is imported from the repository; the LIVE_KEYS / HISTORY_KEY_MARK /
SAMPLE_PATH_MARK sets are re-typed from the checker's source as read at
HEAD=c8f64e9a (lines 60-80 of eng/ci/check_registration_anchors.py).
"""
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8")

REPO = r"F:\Astro dev\Astro CS Normalization Database"

SRC_TOP = ("lib/", "eng/", "docs/", "testdata/", "gaia/", "\u5b9e\u9a8c/", "\u5de5\u7a0b\u63a7\u5236/")
RUNTIME_TOP = ("run/", "dist/", "build/")
TEMPLATE_MARK = ("path/to", "%s", "<", ">", "$", "__no_such", "...")
LIVE_KEYS = {
    "path", "doc_ref", "schema_ref", "canonical_schema", "schema", "source", "target",
    "file", "ref", "contract", "authority", "anchors", "anchor", "implementations",
    "implementation", "spec", "doc", "schema_path", "registry", "matrix", "ledger",
    "evidence",
}
HISTORY_KEY_MARK = ("migration", "migrated", "history", "historical", "former",
                    "previous", "from", "old", "retired", "superseded", "proposal",
                    "at_base_commit", "gone", "deleted")
SAMPLE_PATH_MARK = ("/examples/", "/negative/", "/fixtures/", "/proposals/", "/templates/")

PATH_FILE_RE = re.compile(r"^[\w./\u4e00-\u9fff-]+\.[A-Za-z0-9]+$")
PATH_DIR_RE = re.compile(r"^[\w/\u4e00-\u9fff-]+/$")


def tracked(prefix=None):
    cmd = ["git", "-c", "core.quotePath=false", "-C", REPO, "ls-files"]
    if prefix:
        cmd += ["--", prefix]
    out = subprocess.run(cmd, capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    return [l for l in out.stdout.splitlines() if l.strip()]


def iter_strings(node, path, parent_key, sink):
    if isinstance(node, dict):
        for k, v in node.items():
            iter_strings(v, path + "/" + str(k), str(k), sink)
    elif isinstance(node, list):
        for i, v in enumerate(node):
            iter_strings(v, path + "[%d]" % i, parent_key, sink)
    elif isinstance(node, str):
        sink.append((path, parent_key, node))


tracked_set = set(tracked(""))


def exists(rel):
    return rel in tracked_set


gate_visible_missing = []
prose_retired_refs = []
retired_prefixes = ("\u5de5\u7a0b\u63a7\u5236/RELEASE-01", "\u5de5\u7a0b\u63a7\u5236/RELEASE-02",
                    "\u5de5\u7a0b\u63a7\u5236/RELEASE-03", "\u5de5\u7a0b\u63a7\u5236/RELEASE-04",
                    "\u5de5\u7a0b\u63a7\u5236/PROJECT-GOVERNANCE-01")
RUN_REF_RE = re.compile(r"(^|[\s(（\"'`=/])run/[A-Za-z0-9_.\-/]")

for prefix in ("eng/contracts", "eng/ci"):
    for rel in tracked(prefix):
        if not rel.endswith(".json"):
            continue
        posix = "/" + rel + "/"
        if any(m in posix for m in SAMPLE_PATH_MARK):
            continue
        ap = os.path.join(REPO, rel.replace("/", os.sep))
        try:
            doc = json.load(io.open(ap, encoding="utf-8"))
        except Exception as exc:
            print("UNPARSEABLE %s: %s" % (rel, exc))
            continue
        strings = []
        iter_strings(doc, "$", None, strings)
        for keypath, key, value in strings:
            # (1) gate-visible candidates: exact path-shaped values under a LIVE_KEY
            shaped = bool(PATH_FILE_RE.match(value) or PATH_DIR_RE.match(value))
            if shaped and not any(m in value for m in TEMPLATE_MARK):
                if key in LIVE_KEYS and not any(m in keypath.lower() for m in HISTORY_KEY_MARK):
                    if value.startswith(RUNTIME_TOP) or value.startswith(SRC_TOP):
                        if not exists(value):
                            gate_visible_missing.append((rel, keypath, key, value))
            # (2) prose mentions of retired control packs / run/ (any key)
            if any(p in value for p in retired_prefixes) or RUN_REF_RE.search(value):
                prose_retired_refs.append((rel, keypath, key, value[:120]))

print("== (1) gate-visible (R1/R2) missing path-shaped refs under LIVE_KEYS ==")
for r in gate_visible_missing:
    print("  %s  %s  [%s]  -> %s" % r)
print("  count:", len(gate_visible_missing))

print()
print("== (2) prose mentions of retired control-pack / run/ paths in eng/contracts+eng/ci JSON ==")
files = {}
for rel, keypath, key, val in prose_retired_refs:
    files.setdefault(rel, []).append((keypath, key))
for rel in sorted(files):
    print("  %-62s %3d site(s)" % (rel, len(files[rel])))
print("  total sites:", len(prose_retired_refs), " files:", len(files))
