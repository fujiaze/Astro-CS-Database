#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUDIT-06 / D4 复算第二路：同一键多侧默认值 **全量机械抽取**（第一步：只抽取，不判读）。

六侧：
  S1 defaults.json   eng/packaging/config/defaults.json fields[].value (+authority_status)
  S2 出厂模板         eng/packaging/config/templates/*.json（现行 blocks[] 形态；取不到显式标"该侧无此项"）
  S3 CLI 生成骨架     lib/infrastructure/cli/session_commands.h 字符串聚合内嵌 JSON（先解出 JSON 再取键）
  S4 机器合同/schema   eng/contracts/**.json（含 schemas/**）＋ orchestrator/configs/*.schema.json
                     的 default / enum / minimum / exclusiveMinimum / maximum / const
  S5 代码兜底         lib/** 读键点缺省实参 value("k",d) / contains("k")?:d / 成员初始化 / 具名常量
  S6 登记册           eng/packaging/config/config_registry.json plugin_knobs[].declared_default (+note 断言)

严格只读；不 import 仓库模块；PYTHONDONTWRITEBYTECODE=1 + python -B。
"""
import io
import json
import os
import re
import subprocess
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO = r"F:/Astro dev/Astro CS Normalization Database"
RAW = r"独立审计/证据"
HERE = os.path.dirname(os.path.abspath(__file__))
OUTCSV = os.path.join(RAW, "D4-多侧默认值全表.csv")

SIDES = ["S1_defaults.json", "S2_出厂模板", "S3_CLI骨架", "S4_合同schema", "S5_代码兜底", "S6_登记册"]

GENERIC = {
    "mode", "name", "path", "type", "value", "enabled", "unit", "units", "scale", "key",
    "data", "size", "form", "order", "label", "id", "min", "max", "center", "radius",
    "width", "height", "count", "level", "index", "kernel", "sigma", "alpha", "beta",
    "density", "spacing", "version", "format", "precision", "ordering", "nested",
    "source", "target", "output", "inputs", "params", "config", "items", "dir", "file",
    "method", "kind", "state", "status", "step", "text", "tol", "tolerance", "factor",
    "weight", "weights", "dtype", "bitpix", "n", "m", "k", "r", "s", "t", "x0", "y0",
    "x1", "y1", "low", "high", "lower", "upper", "a", "b", "c", "d", "e", "f", "g", "p",
}
STRUCT = {
    "properties", "items", "$defs", "patternProperties", "additionalProperties",
    "then", "else", "allOf", "oneOf", "anyOf", "prefixItems", "contains", "not",
    "propertyNames", "$ref", "definitions", "required", "type", "description",
    "examples", "title", "$schema", "$id", "enum", "default", "const", "if",
}
STOP_KEYS = set(STRUCT) | {"true", "false", "null"}


def rel(p):
    return os.path.relpath(p, REPO).replace("\\", "/")


def read(path):
    with io.open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def tracked(prefixes):
    out = subprocess.run(["git", "-C", REPO, "ls-files", "-z"] + list(prefixes),
                         capture_output=True)
    if out.returncode != 0:
        raise SystemExit("git ls-files failed: " +
                         out.stderr.decode("utf-8", "replace"))
    return [s.decode("utf-8") for s in out.stdout.split(b"\0") if s]


def jnum(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if v is None:
        return "null"
    if isinstance(v, (int, float)):
        return repr(v)
    if isinstance(v, (list, tuple)):
        return "[" + ",".join(jnum(x) for x in v) + "]"
    return json.dumps(v, ensure_ascii=False)


RECORDS = []
S5_DUMP = []


def add(side, ckey, rawpath, value, site, extra=""):
    if ckey is None:
        return
    RECORDS.append({"side": side, "ckey": ckey, "rawpath": rawpath,
                    "value": str(value), "site": site, "extra": extra})


def canon_of(segs):
    segs = [s for s in segs if s and s not in STRUCT]
    segs = [s for s in segs if not s.startswith("[")]
    if not segs:
        return None, None
    leaf = segs[-1].lower()
    if leaf in GENERIC and len(segs) >= 2:
        return segs[-2].lower() + "." + leaf, ".".join(s.lower() for s in segs)
    return leaf, ".".join(s.lower() for s in segs)


# ══════════════════ S1 ══════════════════
def side1():
    p = os.path.join(REPO, "eng/packaging/config/defaults.json")
    d = json.loads(read(p))
    for f in d.get("fields", []):
        k = f.get("key")
        if not k:
            continue
        ck, sem = canon_of(k.split("."))
        add("S1_defaults.json", ck, sem if sem else k, jnum(f.get("value")),
            "%s#fields[key=%s]" % (rel(p), k),
            "authority_status=%s" % f.get("authority_status"))


# ══════════════════ S2 ══════════════════
def side2():
    tdir = os.path.join(REPO, "eng/packaging/config/templates")
    for fn in sorted(os.listdir(tdir)):
        if not fn.endswith(".json"):
            continue
        p = os.path.join(tdir, fn)
        d = json.loads(read(p))
        phase = fn.replace(".phase_config.json", "")

        def rec(node, segs):
            ck, sem = canon_of(segs)
            if ck is None:
                return
            v = jnum(node)
            ph = isinstance(node, str) and (node.startswith("path/to/") or
                                            node in ("", ".", "[]"))
            add("S2_出厂模板", ck, sem, v, "%s#%s" % (rel(p), ".".join(segs)),
                "phase=%s%s" % (phase, "；示例占位值" if ph else ""))

        def emit(node, segs):
            if isinstance(node, dict):
                for k, v in node.items():
                    emit(v, segs + [k])
            elif isinstance(node, list):
                if node and all(isinstance(x, dict) for x in node):
                    for x in node:
                        emit(x, segs + ["[]"])
                else:
                    rec(node, segs)
            else:
                rec(node, segs)

        emit(d, [])


# ══════════════════ C++ 文本工具 ══════════════════
def strip_cpp_comments(src):
    out = []
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in "\"'":
            q = c
            j = i + 1
            while j < n:
                if src[j] == "\\":
                    j += 2
                    continue
                if src[j] == q:
                    j += 1
                    break
                j += 1
            out.append(src[i:j])
            i = j
            continue
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append("\n")
            continue
        out.append(c)
        i += 1
    return "".join(out)


def split_top(text):
    entries = []
    depth = 0
    start = None
    i, n = 0, len(text)
    in_str = None
    while i < n:
        c = text[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == in_str:
                in_str = None
            i += 1
            continue
        if c in "\"'":
            in_str = c
            i += 1
            continue
        if c == "{":
            depth += 1
            if depth == 1:
                start = i
        elif c == "}":
            depth -= 1
            if depth == 0 and start is not None:
                entries.append(text[start + 1:i])
                start = None
        i += 1
    return entries


def tokens(entry):
    out = []
    i, n = 0, len(entry)
    while i < n:
        c = entry[i]
        if c.isspace():
            i += 1
            continue
        if c in "\"'":
            q = c
            j = i + 1
            buf = c
            while j < n:
                if entry[j] == "\\":
                    buf += entry[j:j + 2]
                    j += 2
                    continue
                if entry[j] == q:
                    buf += q
                    j += 1
                    break
                buf += entry[j]
                j += 1
            out.append(("str", buf))
            i = j
            continue
        m = re.match(r"[A-Za-z_]\w*", entry[i:])
        if m:
            out.append(("id", m.group(0)))
            i += len(m.group(0))
            continue
        i += 1
    return out


def unquote(s):
    s = s[1:-1]
    return (s.replace('\\"', '"').replace("\\'", "'").replace("\\\\", "\\")
             .replace("\\n", " ").replace("\\t", " "))


# ══════════════════ S3 ══════════════════
def brace_body(src, open_idx):
    depth = 0
    j = open_idx
    in_str = None
    while j < len(src):
        c = src[j]
        if in_str:
            if c == "\\":
                j += 2
                continue
            if c == in_str:
                in_str = None
            j += 1
            continue
        if c in "\"'":
            in_str = c
            j += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return src[open_idx + 1:j]
        j += 1
    return ""


def expand(obj, path, site, tag):
    ck, sem = canon_of(path)
    if ck is None:
        return
    add("S3_CLI骨架", ck, sem, jnum(obj), site, tag)
    if isinstance(obj, dict):
        for k, v in obj.items():
            expand(v, path + [k], site, tag)


def side3():
    p = os.path.join(REPO, "lib/infrastructure/cli/session_commands.h")
    src = strip_cpp_comments(read(p))
    for vname, sess in (("kNormalize", "normalize"), ("kMosaic", "mosaic"),
                        ("kExport", "export")):
        m = re.search(r"std::vector<\s*ConfigField\s*>\s*" + vname + r"\s*=\s*\{", src)
        if not m:
            continue
        body = brace_body(src, m.end() - 1)
        for entry in split_top(body):
            toks = tokens(entry)
            strs = [t[1] for t in toks if t[0] == "str"]
            if not strs:
                continue
            key = unquote(strs[0])
            jt = None
            idx = 0
            for kind, val in toks:
                if kind != "str":
                    continue
                if idx == 0:
                    idx = 1
                    continue
                jt = unquote(val)
                break
            if idx == 1:
                # 第二成员若是 nullptr 标识符 ⇒ 不进模板
                first_str_seen = False
                is_null = False
                for kind, val in toks:
                    if kind == "str":
                        if not first_str_seen:
                            first_str_seen = True
                            continue
                        break
                    if kind == "id" and val == "nullptr" and first_str_seen:
                        is_null = True
                        break
                if is_null:
                    jt = None
            ck, sem = canon_of(key.split("."))
            site = "%s#config_fields(%s).%s" % (rel(p), sess, key)
            if jt is None:
                add("S3_CLI骨架", ck, sem,
                    "该侧无此项(键名列进 --help；json==nullptr ⇒ 不写入模板)",
                    site, sess)
                continue
            try:
                obj = json.loads(jt)
            except Exception as e:
                add("S3_CLI骨架", ck, sem, "内嵌JSON解析失败:%s" % e, site, sess)
                continue
            expand(obj, key.split("."), site, sess)
    m = re.search(r"config_second_block_overrides\(\)[^=]*=\s*\{", src)
    if m:
        body = brace_body(src, m.end() - 1)
        for entry in split_top(body):
            strs = [unquote(t[1]) for t in tokens(entry) if t[0] == "str"]
            if len(strs) < 2:
                continue
            key, jt = strs[0], strs[1]
            try:
                obj = json.loads(jt)
            except Exception:
                continue
            expand(obj, key.split("."),
                   "%s#config_second_block_overrides.%s" % (rel(p), key),
                   "normalize第2块覆盖")


# ══════════════════ S4 ══════════════════
def schema_files():
    fs = [f for f in tracked(["eng/contracts"]) if f.endswith(".json")]
    fs += [f for f in tracked(["lib/infrastructure/pipeline/orchestrator/configs"])
           if f.endswith(".schema.json")]
    return sorted(set(fs))


def walk_schema(node, segs, cb):
    if isinstance(node, dict):
        hit = {k: node[k] for k in ("default", "enum", "minimum", "exclusiveMinimum",
                                    "maximum", "exclusiveMaximum", "const") if k in node}
        if hit:
            cb(segs, hit)
        for k, v in node.items():
            if k in ("default", "enum", "minimum", "exclusiveMinimum", "maximum",
                     "exclusiveMaximum", "const", "description", "title", "examples",
                     "$comment", "comment"):
                continue
            if isinstance(v, dict):
                if k in ("properties", "patternProperties", "$defs", "definitions",
                         "propertyNames"):
                    for kk, vv in v.items():
                        walk_schema(vv, segs + [kk], cb)
                else:
                    walk_schema(v, segs + [k], cb)
            elif isinstance(v, list) and k in ("anyOf", "oneOf", "allOf"):
                for vv in v:
                    walk_schema(vv, segs, cb)
    elif isinstance(node, list):
        for vv in node:
            walk_schema(vv, segs, cb)


def side4():
    for rf in schema_files():
        p = os.path.join(REPO, rf.replace("/", os.sep))
        try:
            d = json.loads(read(p))
        except Exception:
            continue

        def cb(segs, payload, rf=rf):
            ck, sem = canon_of(segs)
            if ck is None:
                return
            txt = ";".join("%s=%s" % (k, jnum(payload[k])) for k in
                           ("default", "const", "enum", "minimum", "exclusiveMinimum",
                            "maximum", "exclusiveMaximum") if k in payload)
            add("S4_合同schema", ck, sem, txt, "%s#/%s" % (rf, "/".join(segs)), "")

        walk_schema(d, [], cb)


# ══════════════════ S5 ══════════════════
CODE_SKIP = re.compile(r"(^|/)third_party/|(^|/)tests?/|(^|/)test_|_test\.[ch]|\.md$|"
                       r"(^|/)memory\.md$|(^|/)README|(^|/)docs/")

NUM = r"[-+]?(?:0x[0-9a-fA-F]+|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][-+]?\d+)?)[fFuUlL]*"
STR_LIT = r'"[^"]*"'
CHAR_LIT = r"'.'|'\\.'"
CONST_ID = r"(?:[A-Z][A-Z0-9_]{2,}|k[A-Za-z0-9_]{2,}|[A-Z][A-Za-z0-9]*(?:_[A-Za-z0-9]+)+)"
SIMPLE = (r"(?:" + NUM + r"|true|false|null|nullptr|std::" + CONST_ID +
          r"|" + STR_LIT + r"|" + CHAR_LIT + r"|" + CONST_ID +
          r"(?:::" + CONST_ID + r")*(?:<[^<>]{1,30}>)?(?:::\w+)?"
          r"|" + CONST_ID + r"\(\))")
EXPR_RE = re.compile(r"^\s*(?:" + SIMPLE + r")(?:\s*[-+*/]\s*(?:" + SIMPLE + r"))*\s*$")


def lit_only(expr):
    """只保留字面量/具名常量形（排除成员访问、函数调用取真值、迭代器表达式）。"""
    expr = expr.strip().rstrip(",;").strip()
    expr = re.sub(r"//.*$", "", expr).strip()
    if not expr or len(expr) > 70:
        return None
    if "." in expr and not re.search(r"\d\.\d", expr):
        # 允许 std::numeric_limits<T>::max() 一类
        if not expr.startswith("std::") and "::" not in expr:
            return None
    return expr if EXPR_RE.match(expr) else None


def const_variants(cname):
    norm = re.sub(r"^k(?=[A-Z0-9_])", "", cname)
    norm = re.sub(r"[_\-]?DEFAULT\b", "", norm, flags=re.I)
    norm = re.sub(r"\bDEFAULT[_\-]?", "", norm, flags=re.I)
    parts = [p.lower() for p in re.split(r"(?<!^)(?=[A-Z])|_", norm) if p]
    return "_".join(parts)


def code_files():
    return [f for f in tracked(["lib"])
            if f.endswith((".h", ".hpp", ".cpp", ".cc", ".in")) and not CODE_SKIP.search(f)]


def alt(keys):
    return "|".join(re.escape(k) for k in sorted(keys, key=len, reverse=True))


def side5(scan_keys, keyparent):
    keys = {k for k in scan_keys if k not in STOP_KEYS and len(k) >= 3}
    A = alt(keys)
    rx_quoted = re.compile(r'"(' + A + r')"\s*,\s*([^;\n\)]{1,90})')
    rx_member = re.compile(r'(?<![A-Za-z0-9_])(?P<recv>(?:[A-Za-z_]\w*(?:\.|->))*)(' + A +
                           r')\s*=\s*(?!=)\s*([^;\n\{]{1,90})')
    rx_ctor = re.compile(r'(?<![.\w])(' + A + r')\s*\(\s*([^)\n]{1,60})\)\s*(?:,|:|\{|\n)')
    rx_tern = re.compile(r'contains\(\s*"(' + A + r')"\s*\)(.{0,300}?)\?\s*(.{1,140}?)\s*:\s*(.{1,90}?)\s*[;\n]',
                         re.S)
    rx_const = re.compile(r'(?:constexpr|const)\b[^=;\n]{0,80}?\b([A-Za-z_]\w*)\s*=\s*([^;]{1,80});')
    rx_define = re.compile(r'#\s*define\s+([A-Za-z_]\w*)\s+(\S[^\n]{0,80})')
    # 常量名 → 键
    c2k = {}
    for k in keys:
        for kk in (k, k.replace(".", "_")):
            c2k.setdefault(const_variants(kk), kk)
    files = code_files()
    side5_lines = 0
    GLOBAL = {}  # key -> literal -> [(kind, site)]

    def gpush(k, expr, site, kind):
        GLOBAL.setdefault(k, {}).setdefault(expr, []).append((kind, site))

    for rf in files:
        p = os.path.join(REPO, rf.replace("/", os.sep))
        try:
            raw = read(p)
        except Exception:
            continue
        src = strip_cpp_comments(raw)
        lines = src.split("\n")
        side5_lines += len(lines)
        found = {}

        def push(k, expr, ln, kind):
            e = lit_only(expr)
            if e is None:
                return
            found.setdefault(k, []).append((kind, e, "%s:%d" % (rf, ln)))

        for ln, line in enumerate(lines, 1):
            if not line.strip():
                continue
            for m in rx_quoted.finditer(line):
                push(m.group(1), m.group(2), ln, 'value("k",d)')
            for m in rx_member.finditer(line):
                form = ("成员/赋值 %sk=" % (m.group(1) or "")) if m.group(1) else "声明/赋值 k="
                push(m.group(2), m.group(3), ln, form)
            for m in rx_ctor.finditer(line):
                push(m.group(1), m.group(2), ln, '初始化列表 k(…)')
            for m in rx_const.finditer(line):
                cv = const_variants(m.group(1))
                k = c2k.get(cv) or c2k.get(cv.replace("_", ""))
                if k:
                    push(k, m.group(2), ln, "具名常量 %s" % m.group(1))
            for m in rx_define.finditer(line):
                cv = const_variants(m.group(1))
                k = c2k.get(cv) or c2k.get(cv.replace("_", ""))
                if k:
                    push(k, m.group(2), ln, "#define %s" % m.group(1))
        for m in rx_tern.finditer(src):
            ln = src[:m.start()].count("\n") + 1
            push(m.group(1), m.group(4), ln, 'contains("k")?x:d')
        par_of = keyparent
        for k, hits in found.items():
            par = par_of.get(k)
            for kind, expr, site in hits:
                if k in GENERIC and par:
                    try:
                        ln2 = int(site.rsplit(":", 1)[1])
                        ctx = "\n".join(lines[max(0, ln2 - 9):ln2 + 8])
                    except Exception:
                        ctx = ""
                    if not re.search(r'\b' + re.escape(par) + r'\b', ctx, re.I):
                        continue
                gpush(k, expr, site, kind)

    nrec = 0
    for k in sorted(GLOBAL):
        byval = GLOBAL[k]
        parts = []
        for v, sites in sorted(byval.items(),
                               key=lambda x: (-len(x[1]), x[0]))[:14]:
            u = sorted({s for _, s in sites})
            kinds = ",".join(sorted({kk for kk, _ in sites}))
            parts.append("%s ×%d (%s) @%s" % (v, len(sites), kinds, "; ".join(u[:4])))
        tot = sum(len(s) for s in byval.values())
        more = " …(本键另有 %d 种字面量，见 side5_位点.log)" % (len(byval) - 14) \
            if len(byval) > 14 else ""
        allsites = sorted({s for ss in byval.values() for s, _ in ss})
        add("S5_代码兜底", k, "",
            " ## ".join(parts) + more,
            "%d 处 / %d 个位点（前 4: %s）；全量见 side5_位点.log" %
            (tot, len(allsites), "; ".join(allsites[:4])),
            "")
        nrec += 1
    S5_DUMP.extend((k, v, kk, s) for k, d in GLOBAL.items() for v, ss in d.items()
                   for kk, s in ss)
    return side5_lines, nrec


# ══════════════════ S6 ══════════════════
DEFAULT_ASSERT = re.compile(
    r'(默认值?|缺省|恒为?|取|冻结)\s*[:：=＝]?\s*'
    r'(0\.\d+|1\.0|\d+(?:\.\d+)?|"[^"]{1,30}"|[a-z_][a-z0-9_]{2,24})')


def side6():
    p = os.path.join(REPO, "eng/packaging/config/config_registry.json")
    d = json.loads(read(p))
    for i, row in enumerate(d.get("plugin_knobs", [])):
        note = row.get("note") or ""
        asserts = "; ".join("".join(g) for g in DEFAULT_ASSERT.findall(note))[:300]
        for src_key, tag in ((row.get("registered_key"), "registered_key"),
                             (row.get("field"), "field")):
            if not src_key:
                continue
            segs = [s for s in re.split(r"[.\[\]]+", str(src_key).replace("blocks[]", ""))
                    if s and not s.isdigit()]
            ck, sem = canon_of(segs)
            if ck is None:
                continue
            txt = "declared_default=%s" % jnum(row.get("declared_default"))
            if asserts:
                txt += " ｜note默认值断言: " + asserts
            add("S6_登记册", ck, sem, txt,
                "%s#plugin_knobs[%d] %s=%s (doc %s:%s, module %s, finding=%s)" %
                (rel(p), i, tag, src_key, row.get("doc"), row.get("line"),
                 row.get("module"), row.get("finding")),
                "registered_at=%s" % (row.get("registered_at") or "-"))
            if tag == "registered_key" and row.get("field") and row.get("declared_default") is not None:
                break


# ══════════════════ 合并 ══════════════════
def merge_alias(groups):
    parent = {g: g for g in groups}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        ra, rb = find(a), find(b)
        if ra != rb:
            parent[rb] = ra

    names = sorted(groups)
    norm = {g: g.replace(".", "_") for g in names}
    byn = {}
    for g in names:
        byn.setdefault(norm[g], []).append(g)
    for _, gs in byn.items():
        for g in gs[1:]:
            union(gs[0], g)
    for a in names:
        for b in names:
            if a == b:
                continue
            na, nb = norm[a], norm[b]
            long_, short_ = (na, nb) if len(na) >= len(nb) else (nb, na)
            if len(short_) >= 6 and long_.endswith("_" + short_):
                if short_.split("_")[-1] not in GENERIC:
                    union(a, b)
    members = {}
    for g in names:
        members.setdefault(find(g), []).append(g)
    # 组名取最短成员（同义键合并后以裸键名为准，避免 algorithm_drizzle_pixfrac 顶掉 pixfrac）
    root_of = {}
    for _, ms in members.items():
        best = sorted(ms, key=lambda x: (len(x), x))[0]
        for g in ms:
            root_of[g] = best
    return root_of, members


def main():
    side1()
    side2()
    side3()
    side4()
    side6()

    keyparent = {}
    for r in RECORDS:
        segs = [s for s in re.split(r"[.\[\]]+", r["rawpath"]) if s and s not in STRUCT]
        if len(segs) >= 2 and segs[-1].lower() in GENERIC:
            keyparent.setdefault(segs[-1].lower(), segs[-2].lower())
    scan_keys = set()
    for r in RECORDS:
        scan_keys.add(r["ckey"].split(".")[-1])
    scan_keys = {k for k in scan_keys if k and k not in STOP_KEYS and len(k) >= 3}
    nlines, nsites = side5(scan_keys, keyparent)

    groups = {r["ckey"] for r in RECORDS}
    root_of, members = merge_alias(groups)
    for r in RECORDS:
        r["group"] = root_of.get(r["ckey"], r["ckey"])

    bygroup = {}
    for r in RECORDS:
        bygroup.setdefault(r["group"], []).append(r)

    import csv
    os.makedirs(RAW, exist_ok=True)
    rows = []
    for g in sorted(bygroup):
        recs = bygroup[g]
        raws = sorted({r["rawpath"] for r in recs if r["rawpath"]})
        fams = sorted({r["rawpath"].split(".")[0] for r in recs if r["rawpath"]} - {""})
        cells = []
        for side in SIDES:
            sub = [r for r in recs if r["side"] == side]
            if not sub:
                cells.append("该侧无此项")
                continue
            vals = {}
            for r in sub:
                vals.setdefault(r["value"], []).append((r["site"], r["extra"]))
            parts = []
            for v, ses in sorted(vals.items(), key=lambda x: -len(x[1]))[:8]:
                sv = " ; ".join(dict.fromkeys(s for s, _ in ses))
                if len(sv) > 500:
                    sv = sv[:500] + "…"
                ex = ses[0][1] or ""
                parts.append("值[%s] @%s%s" % (v[:1500], sv, (" [%s]" % ex) if ex else ""))
            more = " (本侧另有 %d 种取值)" % (len(vals) - 8) if len(vals) > 8 else ""
            cells.append(" ||| ".join(parts) + more)
        rows.append([g, "、".join(fams), "；".join(raws[:8])] + cells)

    hdr = ["键", "键族", "语义路径样本"] + SIDES
    with io.open(OUTCSV, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(hdr)
        for r in rows:
            w.writerow(r)

    with io.open(os.path.join(HERE, "side5_位点.log"), "w", encoding="utf-8") as f:
        f.write("键\t字面量\t形态\t位点\n")
        for k, v, kind, site in sorted(S5_DUMP):
            f.write("%s\t%s\t%s\t%s\n" % (k, v, kind, site))
    with io.open(os.path.join(HERE, "records.tsv"), "w", encoding="utf-8") as f:
        f.write("group\tside\tckey\trawpath\tvalue\tsite\n")
        for r in RECORDS:
            f.write("\t".join(str(r[k]).replace("\t", " ") for k in
                              ("group", "side", "ckey", "rawpath", "value", "site")) + "\n")

    # ── 正例控制 ──
    g0 = root_of.get("pixfrac", "pixfrac")
    ctl = [r for r in RECORDS if r["group"] == g0]
    got = {}
    for r in ctl:
        got.setdefault(r["side"], set()).add(r["value"])
    print("=== 正例控制 drizzle.pixfrac（组名 %s，%d 条记录）===" % (g0, len(ctl)))
    ok = True
    for side in SIDES:
        has = side in got
        ok = ok and has
        print("  %-16s %s" % (side, ("命中 %d 种" % len(got[side])) if has else "缺"))
        for v in sorted(got.get(side, []))[:3]:
            print("        %s" % v[:260])
    allv = " ".join(v for s in got.values() for v in s)
    has08 = "0.8" in allv
    has10 = ("1.0" in allv) or re.search(r'enum=\[0,1\]|;1,|="1"|=1\b', allv) is not None
    print("  含 0.8: %s ; 含 1.0: %s ; 六侧齐全: %s" % (has08, has10, ok))
    verdict = "PASS" if (ok and has08 and has10) else "FAIL —— 抽取口径失效，必须先修脚本"
    print("  控制判定: %s" % verdict)
    pr = [r for r in rows if r[0] == g0]
    if pr:
        print("  pixfrac 行原文（列 4..9 为六侧）：")
        for i, side in enumerate(SIDES):
            print("   [%s] %s" % (side, pr[0][3 + i][:600]))

    print("\n扫描代码文件行数: %d ; S5 生成记录: %d" % (nlines, nsites))
    print("总键数(组): %d ; 总抽取记录: %d" % (len(rows), len(RECORDS)))
    for side in SIDES:
        n = sum(1 for r in RECORDS if r["side"] == side)
        kg = len({r["group"] for r in RECORDS if r["side"] == side})
        print("  %-16s 记录 %5d 覆盖键 %5d" % (side, n, kg))
    six = sum(1 for g, rs in bygroup.items() if len({r["side"] for r in rs}) == 6)
    print("六侧齐全键数: %d (%.1f%% of %d)" % (six, 100.0 * six / max(1, len(rows)), len(rows)))
    print("输出: %s （数据行 %d）" % (OUTCSV, len(rows)))
    return 0 if verdict == "PASS" else 2


if __name__ == "__main__":
    sys.exit(main())
