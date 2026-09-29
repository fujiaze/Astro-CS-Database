#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""内容锚（content anchor）共用的规范化、指纹与区分力判定。

规则正本 = docs/algorithms/anchors/ANCHOR_CONTRACT.md 第 9 节（唯一权威）；本模块是该规则的机器实现，
由 CFG-001 门（test_cfg001_contracts.py）与 eng/tests/config/check_cfg002_registry.py
（CFG002-01/04/09/11/12）共同引用——判据只有一份实现，禁止两处各写一套。

形态（9.1）：id（复合键路径或具名标记）/ path（被引文档）/ quote（被引规范文本本身）/ sha256（内容指纹）。
规范化（9.2）：N1 CRLF 折 LF；N2 逐行 strip；N3 行内空白折叠为单空格；N4 去空行；N5 以换行连接。
指纹（9.2）：sha256(norm_quote(quote)).hexdigest()[:16]。
区分力自检（9.3）：D1 引文规范长度 >= 2（无区分力 token 不能充当锚）；
D2 引文在目标文档规范化文本内恰出现一次；D4 value_text 必须落在引文内；D6 同一面内 id 不得指向两个不同目标。
"""
import hashlib
import os
import re

MIN_QUOTE_CHARS = 2
FINGERPRINT_ALGO = "sha256_16(norm_quote)"


def _collapse(line):
    return re.sub(r"\s+", " ", line.strip())


def norm_quote(quote):
    """引文规范化：逐行 strip + 行内空白折叠 + 去空行 + 换行连接（9.2）。"""
    if quote is None:
        return ""
    out = [_collapse(ln) for ln in quote.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    return "\n".join(x for x in out if x)


def norm_doc(text):
    """文档规范化（9.2）：与引文同一套规则；返回 (规范化全文, 行首偏移表)。"""
    raw = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    kept, starts, pos = [], [], 0
    for ln in raw:
        c = _collapse(ln)
        if not c:
            continue
        starts.append(pos)
        kept.append(c)
        pos += len(c) + 1
    return "\n".join(kept), starts


def fingerprint(quote):
    """内容指纹（9.3）：规范化引文的 sha256 前 16 位十六进制。"""
    return hashlib.sha256(norm_quote(quote).encode("utf-8")).hexdigest()[:16]


def anchor_matches(doc_text, quote):
    """引文在文档规范化文本内的全部命中（9.1 子串匹配）；返回起始行号列表（1 基）。"""
    q = norm_quote(quote)
    if not q:
        return []
    import bisect
    doc, starts = norm_doc(doc_text)
    hits, pos = [], 0
    while True:
        i = doc.find(q, pos)
        if i < 0:
            break
        hits.append(bisect.bisect_right(starts, i))
        pos = i + 1
    return hits


def judge(root, anchor, doc_text=None):
    """判一条内容锚：返回 (ok, detail)。不成立/无区分力/指纹不符一律判红。"""
    if not isinstance(anchor, dict):
        return False, "锚形态不是对象: %r" % (anchor,)
    aid, path, quote, sha = anchor.get("id"), anchor.get("path"), anchor.get("quote"), anchor.get("sha256")
    miss = [k for k in ("id", "path", "quote", "sha256") if not anchor.get(k)]
    if miss:
        return False, "锚 %r 缺字段 %s" % (aid or anchor, miss)
    for banned in ("line", "token_line", "ref_line"):
        if banned in anchor:
            return False, "锚 %s 内嵌行号字段 %r（9.3/D5：登记面与测试期望不内嵌行号）" % (aid, banned)
    q = norm_quote(quote)
    if len(q) < MIN_QUOTE_CHARS:
        return False, "锚 %s 的引文规范长度 %d < %d（无区分力 token，9.3/D1）" % (aid, len(q), MIN_QUOTE_CHARS)
    if fingerprint(quote) != sha:
        return False, "锚 %s 的内容指纹不符：登记 %s，重算 %s（引文已被改动，须复审锚，9.2/9.3-D3）" % (
            aid, sha, fingerprint(quote))
    if doc_text is None:
        p = os.path.join(root, path)
        if not os.path.isfile(p):
            return False, "锚 %s 的目标文档不存在: %s" % (aid, path)
        with open(p, encoding="utf-8") as fh:
            doc_text = fh.read()
    hits = anchor_matches(doc_text, quote)
    if not hits:
        return False, "锚 %s 的引文在 %s 内找不到（锚不成立，9.3/D2）" % (aid, path)
    if len(hits) > 1:
        return False, "锚 %s 的引文在 %s 内匹配 %d 处 %s（无区分力，9.3/D2）" % (aid, path, len(hits), hits[:6])
    return True, "line=%d" % hits[0]


def judge_value(root, anchor, value_text, label="anchor"):
    """内容锚 + 取值/字面量覆盖（9.3/D4；R-54 §2：锚必须锁住被引文本里的那个值，不是任意 token）。"""
    ok, detail = judge(root, anchor)
    if not ok:
        return False, detail
    if value_text is None:
        return True, detail
    if str(value_text) not in norm_quote(anchor.get("quote")):
        return False, ("锚 %s 的引文不含 value_text=%r（锚没锁住该值：换引文或改 value_text，"
                       "不得放宽判据）" % (anchor.get("id"), value_text))
    return True, detail


def judge_face(root, anchors, label="anchor"):
    """批量判一组锚；返回 (problems, lines) —— lines 为锚 id -> 实时行号（只用于报告）。

    面级判据 D6（9.3）：同一 id 在同一面出现两次而目标不同（path/quote 不同）即判红——
    同一个 id 只能有一个目标，否则「锚 id」就失去了区分力。
    """
    problems, lines, seen = [], {}, {}
    for a in anchors:
        ok, detail = judge(root, a)
        if not ok:
            problems.append("[%s] %s" % (label, detail))
            continue
        lines[a.get("id")] = detail
        sig = (a.get("path"), norm_quote(a.get("quote")))
        prev = seen.get(a.get("id"))
        if prev is not None and prev != sig:
            problems.append("[%s] 锚 id %r 在同一面指向两个不同目标（%s vs %s）（9.3/D6）"
                            % (label, a.get("id"), prev, sig))
        else:
            seen[a.get("id")] = sig
    return problems, lines


def row_fingerprint(text):
    """配置表行的内容指纹（9.3 用于行锚：登记面存 id + 指纹，不存行号）。"""
    return fingerprint(text)

