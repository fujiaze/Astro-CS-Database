# -*- coding: utf-8 -*-
"""AUDIT-06 D4 prescan :: prose-claim checker (登记即错 / 声称的登记点不成立).

Scans the *text* of every registration face (defaults.json note+constraint,
schema descriptions, config_registry notes) for claims of the form

  A. "<file>.phase_config.json"（… 同值 …）  -- the named factory template must
     actually carry the key the claim hangs on
  B. "defaults.json#<dotted.key>" / "defaults.json 的 <key>"   -- the referenced
     key must exist and, when a number is asserted (同值 N), must equal N
  C. "数值默认未冻结 / pending_authority" claims about a defaults.json key --
     the key's real authority_status is compared

Deterministic and recomputable; each finding carries the claiming text verbatim.
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(sys.argv[1])


def flat(node, prefix, out):
    if isinstance(node, dict):
        for k, v in node.items():
            p = prefix + "." + k if prefix else k
            if isinstance(v, dict):
                flat(v, p, out)
            elif isinstance(v, list) and v and all(isinstance(x, dict) for x in v):
                for x in v:
                    flat(x, p, out)
            else:
                out[p] = v
    return out


def main():
    mat = json.load(io.open(os.path.join(HERE, "side_matrix.json"),
                            encoding="utf-8"))["matrix"]
    defaults = json.load(io.open(os.path.join(
        REPO, "eng/packaging/config/defaults.json"), encoding="utf-8"))
    dby = {f["key"]: f for f in defaults["fields"]}
    tmpl = {}
    for fn in sorted(os.listdir(os.path.join(
            REPO, "eng/packaging/config/templates"))):
        if fn.endswith(".json"):
            doc = json.load(io.open(os.path.join(
                REPO, "eng/packaging/config/templates", fn), encoding="utf-8"))
            tmpl[fn] = flat(doc, "", {})

    findings = []
    TPL_RE = re.compile(r"templates/([A-Za-z0-9_.\-]+\.json)")
    DEF_RE = re.compile(r"defaults\.json#(?:fields\[\]\.key=)?([a-z_][a-z0-9_.]*)")
    SAME_RE = re.compile(r"同值\s*(?:[一二三四五六七八九十]+处)?\s*(\d+(?:\.\d+)?)")
    PEND_RE = re.compile(r"(pending_authority|数值默认未冻结|未冻结)")

    def scan(leaf, side, loc, text, own_value):
        if not text:
            return
        for m in TPL_RE.finditer(text):
            fn = m.group(1)
            sent = text[max(0, m.start() - 160):m.end() + 200]
            if fn not in tmpl:
                findings.append({"leaf": leaf, "side": side, "loc": loc,
                                 "claim": "引用模板 %s" % fn,
                                 "actual": "该模板文件不存在",
                                 "text": sent.replace("\n", " ")[:200]})
                continue
            hit = [k for k in tmpl[fn] if k.split(".")[-1] == leaf]
            if not hit:
                findings.append({"leaf": leaf, "side": side, "loc": loc,
                                 "claim": "文本声称本键登记于模板 %s" % fn,
                                 "actual": "%s 的键面（含 blocks[] 展开）无 %s"
                                           % (fn, leaf),
                                 "text": sent.replace("\n", " ")[:200]})
            else:
                tv = tmpl[fn][hit[0]]
                sm = SAME_RE.search(sent)
                if sm and tv is not None and str(tv) != sm.group(1) \
                        and (isinstance(tv, str) or repr(tv) != sm.group(1)):
                    findings.append({"leaf": leaf, "side": side, "loc": loc,
                                     "claim": "文本声称三处同值 %s" % sm.group(1),
                                     "actual": "模板 %s#%s 实测 %r" % (fn, hit[0], tv),
                                     "text": sent.replace("\n", " ")[:200]})
        for m in DEF_RE.finditer(text):
            key = m.group(1).rstrip("）)，,。")
            sent = text[max(0, m.start() - 160):m.end() + 220]
            if key not in dby:
                findings.append({"leaf": leaf, "side": side, "loc": loc,
                                 "claim": "文本引用 defaults.json#%s" % key,
                                 "actual": "defaults.json 无该键",
                                 "text": sent.replace("\n", " ")[:200]})
                continue
            val = dby[key].get("value")
            sm = SAME_RE.search(sent)
            if sm and str(val) != sm.group(1):
                findings.append({"leaf": leaf, "side": side, "loc": loc,
                                 "claim": "文本声称同值 %s" % sm.group(1),
                                 "actual": "defaults.json#%s 实测 value=%r"
                                           % (key, val),
                                 "text": sent.replace("\n", " ")[:200]})
            if PEND_RE.search(sent) and dby[key].get("authority_status") != \
                    "pending_authority":
                findings.append({"leaf": leaf, "side": side, "loc": loc,
                                 "claim": "文本称该键 pending_authority / 数值未冻结",
                                 "actual": "defaults.json#%s 实测 authority_status=%s、"
                                           "value=%r" % (key, dby[key].get(
                                               "authority_status"), val),
                                 "text": sent.replace("\n", " ")[:200]})

    for leaf, cell in mat.items():
        for r in cell["side1_defaults"]:
            scan(leaf, "侧1", r["loc"],
                 (r.get("constraint") or "") + " " + (r.get("note") or ""),
                 r.get("value"))
        for r in cell["side4_schema"]:
            if r.get("kw") in ("default", "default(desc)"):
                scan(leaf, "侧4", r["loc"], r.get("desc") or "", r.get("value"))
        for r in cell["side6_registry"]:
            scan(leaf, "侧6", r["loc"], r.get("note") or "",
                 r.get("declared_default"))
    json.dump(findings, io.open(os.path.join(HERE, "claims.json"), "w",
                                encoding="utf-8"), ensure_ascii=False, indent=1)
    with io.open(os.path.join(HERE, "claims.txt"), "w", encoding="utf-8") as f:
        for x in findings:
            f.write("%-28s %s %s\n   声称：%s\n   实测：%s\n   原文：%s\n" % (
                x["leaf"], x["side"], x["loc"], x["claim"], x["actual"], x["text"]))
    sys.stdout.write("claims=%d findings=%d\n" % (0, len(findings)))


if __name__ == "__main__":
    main()
