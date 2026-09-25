# -*- coding: utf-8 -*-
"""AUDIT-06 D4 prescan :: six-side default-value extractor (deterministic, read-only).

Usage:  python prescan_extract.py <repo-root>
Writes side_matrix.json + per-side dumps next to this script.
Never writes into the repository.  Never imports repository Python.
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else ".")

LIT = (r"(-?(?:0[xX][0-9a-fA-F]+|\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
       r"|true|false|null|\"[^\n\"]{0,60}\")")

S1_PATH = "eng/packaging/config/defaults.json"
S6_PATH = "eng/packaging/config/config_registry.json"
S2_GLOBS = ["eng/packaging/config/templates"]
S2_EXTRA = ["lib/infrastructure/pipeline/orchestrator/configs/stage1.template.json"]
S3_PATH = "lib/infrastructure/cli/session_commands.h"
# stage1.schema.json ships twice: the file and a C++ string copy in json_config.cpp
S3_EMBED = [("lib/infrastructure/pipeline/orchestrator/cpp/src/json_config.cpp",
             "stage1.schema")]
# config-class schemas only (input-contract surface).  Product/output schemas
# under eng/contracts/schemas/unified/ are deliberately OUT of the universe --
# see 说明 §漏检口径.
S4_FILES = [
    "eng/contracts/schemas/phase_config_normalize.schema.json",
    "eng/contracts/schemas/phase_config_mosaic.schema.json",
    "eng/contracts/schemas/phase_config_export.schema.json",
    "eng/contracts/schemas/cpu_profile.schema.json",
    "eng/contracts/schemas/hips_storage_form.schema.json",
    "eng/contracts/schemas/pipeline_block.schema.json",
    "eng/contracts/schemas/projection_registry.schema.json",
    "lib/infrastructure/pipeline/orchestrator/configs/stage1.schema.json",
]


def rp(p):
    return os.path.relpath(p, REPO).replace(os.sep, "/")


def load(path):
    return json.load(io.open(path, encoding="utf-8-sig"))


# ------------------------------------------------------------------ side 1
def side1():
    out = []
    doc = load(os.path.join(REPO, S1_PATH))
    for f in doc.get("fields") or []:
        out.append({
            "key": f["key"],
            "leaf": f["key"].split(".")[-1],
            "value": f.get("value"),
            "authority_status": f.get("authority_status"),
            "unit": f.get("unit"),
            "constraint": f.get("constraint"),
            "note": f.get("note"),
            "enum_target": f.get("enum_target"),
            "loc": "%s#%s" % (S1_PATH, f["key"]),
        })
    return out


# ------------------------------------------------------------------ side 2
def flatten(node, prefix, out, src):
    if isinstance(node, dict):
        for k, v in node.items():
            p = (prefix + "." + k) if prefix else k
            if isinstance(v, (dict,)):
                flatten(v, p, out, src)
            elif isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
                for i, item in enumerate(v):
                    flatten(item, p, out, src)          # blocks[] -> same key path
            else:
                out.append({"key": p, "leaf": p.split(".")[-1], "value": v, "loc": src})


def side2():
    out = []
    files = []
    for base in S2_GLOBS:
        d = os.path.join(REPO, base)
        if os.path.isdir(d):
            for fn in sorted(os.listdir(d)):
                if fn.endswith(".json"):
                    files.append("%s/%s" % (base, fn))
    files += S2_EXTRA
    for src in files:
        p = os.path.join(REPO, src)
        if not os.path.isfile(p):
            continue
        doc = load(p)
        tmp = []
        flatten(doc, "", tmp, src)
        for e in tmp:
            e["value"] = json.dumps(e["value"], ensure_ascii=False) \
                if isinstance(e["value"], (list, dict)) else e["value"]
        out.extend(tmp)
    return out


# ------------------------------------------------------------------ side 3
def c_string_at(text, i):
    """text[i] must be '"'.  returns (value, next_index)."""
    assert text[i] == '"'
    buf = []
    i += 1
    while i < len(text):
        c = text[i]
        if c == "\\":
            nxt = text[i + 1] if i + 1 < len(text) else ""
            buf.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(nxt, nxt))
            i += 2
            continue
        if c == '"':
            return "".join(buf), i + 1
        buf.append(c)
        i += 1
    raise ValueError("unterminated string")


def side3():
    """Parse config_fields(...) in session_commands.h (CLI --template / --help)."""
    path = os.path.join(REPO, S3_PATH)
    text = io.open(path, encoding="utf-8").read()
    lines = text.splitlines()
    out = []
    cur_session = None
    for m in re.finditer(r"static const std::vector<ConfigField>\s+(k\w+)\s*=\s*\{", text):
        start = m.end()
        sess = {"kNormalize": "normalize", "kMosaic": "mosaic",
                "kExport": "export"}.get(m.group(1), m.group(1))
        # scan entries until the closing "};"
        i = start
        while True:
            j = text.find("{\"", i)
            if j < 0:
                break
            close = text.find("};", i)
            if 0 <= close < j:
                break
            # key
            k, i2 = c_string_at(text, j + 1)
            # skip to comma
            while i2 < len(text) and text[i2] in " \t\n\r":
                i2 += 1
            assert text[i2] == ",", (k, text[i2:i2 + 20])
            i2 += 1
            while i2 < len(text) and text[i2] in " \t\n\r":
                i2 += 1
            if text[i2:i2 + 7] == "nullptr":
                val, i3 = None, i2 + 7
            elif text[i2] == '"':
                parts = []
                i3 = i2
                while True:
                    s, i3 = c_string_at(text, i3)
                    parts.append(s)
                    k2 = i3
                    while k2 < len(text) and text[k2] in " \t\n\r":
                        k2 += 1
                    if k2 < len(text) and text[k2] == '"':
                        i3 = k2
                        continue
                    break
                val = "".join(parts)
            else:
                raise ValueError(text[i2:i2 + 30])
            lineno = text.count("\n", 0, j) + 1
            out.append({"session": sess, "key": k, "leaf": k.split(".")[-1],
                        "template_value": val,
                        "loc": "%s:%d" % (S3_PATH, lineno)})
            i = i3
            # advance to next entry separator ','
            nxt_entry = text.find("{\"", i)
            nxt_end = text.find("};", i)
            if nxt_entry < 0 or (0 <= nxt_end < nxt_entry):
                break
        _ = cur_session
    # second-block overrides (they are what block #2 of the template shows)
    m = re.search(r"config_second_block_overrides\(\)\s*\{.*?k\s*=\s*\{(.*?)\};", text, re.S)
    if m:
        for mm in re.finditer(r"\{\s*\"([A-Za-z_][\w.]*)\"\s*,\s*\"", m.group(1)):
            key = mm.group(1)
            v, _ = c_string_at(m.group(1), mm.end() - 1)
            out.append({"session": "normalize", "key": key + " (block#2)",
                        "leaf": key, "template_value": v,
                        "loc": "%s#second_block_overrides" % S3_PATH})
    # expand nested template objects so that leaf keys (drizzle.pixfrac,
    # center.ra_deg, source.hips_dir, ...) surface on side 3 as well
    expanded = []
    for e in out:
        expanded.append(e)
        v = e.get("template_value")
        if isinstance(v, str) and v[:1] in "{[":
            try:
                doc = json.loads(v)
            except Exception:
                continue
            sub = []
            flatten(doc, e["key"].replace(" (block#2)", ""), sub, e["loc"])
            for s in sub:
                s["key"] = s["key"] + (" (block#2)" if "(block#2)" in e["key"] else "")
                s["session"] = e["session"]
                s["nested_of"] = e["key"]
                expanded.append(s)
    return expanded
PROBE = ("default", "enum", "minimum", "exclusiveMinimum", "maximum",
         "exclusiveMaximum", "const")
DESC_DEFAULT_RE = re.compile(
    r"(?:默认|缺省|default)\s*(?:值|取值)?\s*[=为:：（(]?\s*"
    r"(?:\"([A-Za-z_][\w.\-]*)\"|([A-Za-z_][\w.\-]{0,24})|(-?\d+(?:\.\d+)?))")


def walk_schema(node, chain, src, hits):
    if isinstance(node, dict):
        props = node.get("properties")
        if isinstance(props, dict):
            for name, sub in props.items():
                walk_schema(sub, chain + [name], src, hits)
        for kw in PROBE:
            if kw in node and chain:
                hits.append({"key": chain[-1], "leaf": chain[-1],
                             "chain": "/".join(chain), "kw": kw,
                             "value": json.dumps(node[kw], ensure_ascii=False),
                             "desc": (node.get("description") or "")[:900],
                             "loc": "%s#%s" % (src, "/".join(chain[-2:]))})
        # prose-declared default inside description (flagged separately)
        desc = node.get("description")
        if isinstance(desc, str) and chain and "default" not in node:
            m = DESC_DEFAULT_RE.search(desc)
            if m:
                v = m.group(1) or m.group(2) or m.group(3)
                hits.append({"key": chain[-1], "leaf": chain[-1],
                             "chain": "/".join(chain), "kw": "default(desc)",
                             "value": v, "loc": "%s#%s" % (src, "/".join(chain[-2:]))})
        for k, v in node.items():
            if k == "properties":
                continue
            if isinstance(v, (dict, list)):
                walk_schema(v, chain, src, hits)
    elif isinstance(node, list):
        for v in node:
            walk_schema(v, chain, src, hits)


def side4():
    out = []
    for src in S4_FILES:
        p = os.path.join(REPO, src)
        if not os.path.isfile(p):
            sys.stderr.write("MISSING schema %s\n" % src)
            continue
        hits = []
        walk_schema(load(p), [], src, hits)
        out.extend(hits)
    # schema / defaults literal JSON embedded as a C++ string in production code
    for rel, tag in S3_EMBED:
        p = os.path.join(REPO, rel)
        if not os.path.isfile(p):
            sys.stderr.write("MISSING embed %s\n" % rel)
            continue
        text = io.open(p, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r'"((?:[^"\\]|\\.){80,})"', text):
            blob = m.group(1)
            try:
                doc = json.loads(blob.replace("\\\"", "\""))
            except Exception:
                continue
            if not isinstance(doc, dict):
                continue
            hits = []
            walk_schema(doc, [], "%s (%s embedded)" % (rel, tag), hits)
            if hits:
                lineno = text.count("\n", 0, m.start()) + 1
                for h in hits:
                    h["loc"] = "%s:%d" % (h["loc"], lineno)
                out.extend(hits)
    return out


# ------------------------------------------------------------------ side 5
EXCL_DIR = ("third_party",)
TEST_HINT = re.compile(r"(^|/)tests?/|_test\.cpp$|^test_|/testkit/")
FUNC_HEAD = re.compile(
    r"^(?:template\s*<[^>]*>\s*)?[A-Za-z_][\w:<>,\*&\s~]*\s+([A-Za-z_~][\w]*)\s*\([^;{}]*\)\s*(?:const)?\s*(?:noexcept)?\s*(?:override)?\s*\{?\s*$")
MEMBER_ASSIGN = re.compile(r"([A-Za-z_][\w.]*(?:\.|->))?([a-z_][a-z0-9_]*)\s*=\s*%s\s*;" % LIT)
READ_ARG = re.compile(r"\b(?:value|get|get_or|at|value_or|opt|opt_|required|read|fetch)"
                      r"\w*\s*\(\s*\"([a-z_][a-z0-9_.]*)\"\s*,\s*%s" % LIT)
READ_ARG_LOOSE = re.compile(r"\"([a-z_][a-z0-9_.]*)\"\s*,\s*%s" % LIT)
# nlohmann "carrier[key].value(LIT, DFT)" / "j["k"] ? ... : DFT" reading idioms
BRACKET_VALUE = re.compile(r"\[\s*\"([a-z_][a-z0-9_.]*)\"\s*\]\s*"
                           r"\.?(?:value|get|get_or|to_)\w*\s*\(\s*[A-Za-z_][\w:<>]*"
                           r"\s*,\s*%s" % LIT)
BRACKET_ARG = re.compile(r"(?:value|get|get_or|opt|read|fetch)\w*\s*\(\s*\"?"
                         r"([a-z_][a-z0-9_.]*)\"?\s*\][^,()]{0,24},\s*%s" % LIT)
CONTAINS_TERNARY = re.compile(r"\"([a-z_][a-z0-9_.]*)\"\s*\)[^;?]*\?[^;:]*:\s*%s\s*;" % LIT)
IN_CLASS_INIT = re.compile(r"^\s*(?:const\s+|static\s+|constexpr\s+)*"
                           r"[A-Za-z_][\w:<>*&\[\] ]*\s+([a-z_][a-z0-9_]*)\s*"
                           r"(?:=\s*%s\s*;|\{\s*%s\s*\}\s*;)" % (LIT, LIT))


def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            i = n if j < 0 else j      # keep the terminating "\n" in the stream
            continue
        if c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            seg = text[i:j] if j >= 0 else text[i:]
            out.append("\n" * seg.count("\n"))
            i = n if j < 0 else j + 2
            continue
        if c in "\"'":
            q = c
            j = i + 1
            while j < n:
                if text[j] == "\\":
                    j += 2
                    continue
                if text[j] == q:
                    break
                j += 1 if text[j] != "\n" else 1
            out.append(text[i:min(j + 1, n)])
            i = j + 1
            continue
        out.append(c)
        i += 1
    return "".join(out)


STRUCT_OPEN = re.compile(r"^\s*(?:typedef\s+)?(?:struct|class)\s+([A-Za-z_]\w*)?\s*\{?\s*$")
STRUCT_CLOSE = re.compile(r"^\s*\}\s*([A-Za-z_]\w*)\s*;\s*$")


def side5(universe):
    prod, tests, allreads = [], [], []
    CARRIER = re.compile(r"(cfg|config|settings|opts|params|options|defaults|doc\[|"
                         r"j\[|json|request|input)", re.I)
    for root, dirs, files in os.walk(os.path.join(REPO, "lib")):
        dirs[:] = [d for d in dirs if d not in EXCL_DIR]
        for fn in sorted(files):
            if not fn.endswith((".cpp", ".h", ".hpp")):
                continue
            p = os.path.join(root, fn)
            rel = rp(p)
            is_test = bool(TEST_HINT.search(rel))
            text = strip_comments(io.open(p, encoding="utf-8", errors="replace").read())
            lines = text.splitlines()

            def emit(rec):
                (tests if is_test else prod).append(rec)

            # ---- (a) statement-level read points (default args / ternary / bracket)
            seen = set()
            pos = 0
            for chunk in text.split(";"):
                base_line = text.count("\n", 0, pos) + 1
                pos += len(chunk) + 1
                if len(chunk) > 600 or chunk.count("\n") > 8:
                    continue
                for rx, kind, gk, gv in (
                        (READ_ARG, "read-arg", 1, 2),
                        (BRACKET_VALUE, "read-arg[]", 1, 2),
                        (CONTAINS_TERNARY, "ternary-fallback", 1, 2)):
                    for m in rx.finditer(chunk):
                        key = m.group(gk)
                        leaf = key.split(".")[-1]
                        rec = {"key": key, "leaf": leaf, "kind": kind,
                               "value": m.group(gv),
                               "carrier_hint": bool(CARRIER.search(chunk)),
                               "loc": "%s:%d" % (rel, base_line +
                                                 chunk[:m.start()].count("\n"))}
                        if (rec["key"], rec["value"], rec["loc"]) in seen:
                            continue
                        seen.add((rec["key"], rec["value"], rec["loc"]))
                        allreads.append(rec)
                        if leaf not in universe:
                            continue
                        emit(rec)
            # ---- (b) in-class / struct member initialisers, with carrier name
            cur_struct = ""
            depth = 0
            for idx, line in enumerate(lines, start=1):
                so = STRUCT_OPEN.match(line)
                if so and line.rstrip().endswith("{"):
                    cur_struct = so.group(1) or cur_struct
                    depth = 1
                sc = STRUCT_CLOSE.match(line)
                if sc and cur_struct:
                    cur_struct = sc.group(1) or cur_struct
                    depth = 0
                m = IN_CLASS_INIT.match(line)
                if m and m.group(1) in universe:
                    emit({"key": m.group(1), "leaf": m.group(1),
                          "kind": "member-init", "value": m.group(2) or m.group(3),
                          "carrier": cur_struct, "loc": "%s:%d" % (rel, idx)})
            # ---- (c) member assignment on a config-ish receiver
            pos = 0
            for chunk in text.split(";"):
                base_line = text.count("\n", 0, pos) + 1
                pos += len(chunk) + 1
                if len(chunk) > 400 or chunk.count("\n") > 6:
                    continue
                for m in MEMBER_ASSIGN.finditer(chunk):
                    mm = re.match(r"([A-Za-z_][\w.\->]*?)(?:\.|->)([a-z_][a-z0-9_]*)"
                                  r"\s*=\s*(%s)\s*$" % LIT, m.group(0).strip())
                    if not mm:
                        mm = re.match(r"([a-z_][a-z0-9_]*)\s*=\s*(%s)\s*$" % LIT,
                                      m.group(0).strip())
                        if not mm:
                            continue
                        member, val, recv = mm.group(1), mm.group(2), None
                    else:
                        member, val, recv = mm.group(2), mm.group(3), mm.group(1)
                    if member not in universe:
                        continue
                    r = (recv or "").replace("->", ".").split(".")[-1]
                    cfg_like = re.search(
                        r"(cfg|config|settings|opts|params|options|defaults|grid|meta"
                        r"|stage1|sci_?cfg|ncfg|plan|req)", r, re.I)
                    if not cfg_like:
                        continue
                    emit({"key": member, "leaf": member,
                          "kind": "member-assign(%s)" % (r or "bare"),
                          "value": val, "carrier": cur_struct,
                          "loc": "%s:%d" % (rel, base_line + chunk[:m.start()].count("\n"))})
    return prod, tests, allreads


# ------------------------------------------------------------------ side 6
def side6():
    out = []
    doc = load(os.path.join(REPO, S6_PATH))
    for e in doc.get("plugin_knobs") or []:
        f = str(e.get("field") or "")
        # composite field labels ("rotation_deg + parity") carry several keys:
        # emit one record per identifier token, keeping the raw label for evidence
        toks = re.findall(r"[A-Za-z_][A-Za-z0-9_.]{2,}", f) or [f]
        for t in toks:
            if t in ("and", "or", "the"):
                continue
            out.append({"key": t, "leaf": t.split(".")[-1],
                        "field_raw": f,
                        "composite": len(toks) > 1,
                        "declared_default": e.get("declared_default"),
                        "registration": e.get("registration"),
                        "registered_at": e.get("registered_at"),
                        "registered_key": e.get("registered_key"),
                        "finding": e.get("finding"),
                        "conflict": e.get("conflict"),
                        "unit": e.get("unit"),
                        "owner_class": e.get("owner_class"),
                        "module": e.get("module"),
                        "note": e.get("note"),
                        "doc": e.get("doc"), "doc_line": e.get("line"),
                        "loc": "%s#%s:%s" % (S6_PATH, e.get("module"), e.get("line"))})
    return out


def norm_value(v):
    if v is None:
        return None
    if isinstance(v, str):
        s = v.strip()
        if s.startswith("{") or s.startswith("["):
            try:
                return norm_value(json.loads(s))
            except Exception:
                return s
        if s in ("true", "false"):
            return s
        try:
            return float(s)
        except ValueError:
            return s
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return float(v)
    if isinstance(v, list):
        return [norm_value(x) for x in v]
    return v


def main():
    s1, s2, s3, s4, s6 = side1(), side2(), side3(), side4(), side6()
    universe = set()
    for rec in s1 + s2 + s3 + s6:
        universe.add(rec["leaf"])
    for rec in s4:
        universe.add(rec["leaf"])
    universe |= {"smoothing_lambda"}
    # universe stays = registration faces; allreads also records config-carrier
    # reads whose key no registration face declares (J5b evidence)
    s5p, s5t, allreads = side5(universe)
    with io.open(os.path.join(HERE, "side5_reads_all.json"), "w",
                 encoding="utf-8") as f:
        json.dump(allreads, f, ensure_ascii=False, indent=1)

    def by_leaf(recs, leaf_field="leaf"):
        d = {}
        for r in recs:
            d.setdefault(r[leaf_field], []).append(r)
        return d

    matrix = {}
    for leaf in sorted(universe):
        matrix[leaf] = {
            "side1_defaults": by_leaf(s1).get(leaf, []),
            "side2_template": by_leaf(s2).get(leaf, []),
            "side3_cli": by_leaf(s3).get(leaf, []),
            "side4_schema": by_leaf(s4).get(leaf, []),
            "side5_code": by_leaf(s5p).get(leaf, []),
            "side6_registry": by_leaf(s6).get(leaf, []),
        }
    payload = {"repo": REPO, "universe_size": len(universe), "matrix": matrix}
    with io.open(os.path.join(HERE, "side_matrix.json"), "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, indent=1)
    with io.open(os.path.join(HERE, "side5_code_production.json"), "w", encoding="utf-8") as f:
        json.dump(s5p, f, ensure_ascii=False, indent=1)
    with io.open(os.path.join(HERE, "side5_code_tests.json"), "w", encoding="utf-8") as f:
        json.dump(s5t, f, ensure_ascii=False, indent=1)
    with io.open(os.path.join(HERE, "side5_reads_all.json"), "w", encoding="utf-8") as f:
        json.dump(allreads, f, ensure_ascii=False, indent=1)
    sys.stdout.write("universe=%d s1=%d s2=%d s3=%d s4=%d s5prod=%d s5test=%d "
                     "s5allreads=%d s6=%d\n" % (
                         len(universe), len(s1), len(s2), len(s3), len(s4),
                         len(s5p), len(s5t), len(allreads), len(s6)))
    keys_present = {}
    for leaf, cell in matrix.items():
        n = sum(1 for v in cell.values() if v)
        keys_present.setdefault(n, 0)
        keys_present[n] += 1
    sys.stdout.write("sides-present histogram: %s\n" % json.dumps(keys_present, sort_keys=True))


if __name__ == "__main__":
    main()
