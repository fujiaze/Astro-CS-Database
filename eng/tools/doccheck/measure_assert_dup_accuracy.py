#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""断言副本检测器 · 夹具准确率测量脚本。

对 assert_dup_fixtures.yaml 里每条夹具调 check_assert_dup.py（--format json，
subprocess，timeout 120 秒），打出逐条 PASS/FAIL 表与两条汇总数：

    正例命中率 = 实际命中的 must_hit 条数 / must_hit 总条数
    负例误报率 = 实际出现的 must_not_hit 条数 / must_not_hit 总条数

**退出码恒为 0。**
本脚本是**诊断工具，不是门禁**：它把计数摆出来供人读，不把 PASS/FAIL 变成阻断。
发现分歧不是错误；把分歧变成非零退出码才是。

**计数只作分母，判定由人做。**
must_hit / must_not_hit 的期望值全部由人读原文判定并写在夹具文件里，不由本脚本
或被测工具反推。命中率 100% 不等于「仓内无矛盾」——工具 banner 自己列了它抓不到
的形态（大小写混合类型名、被改写过的式子、同名不同义、恒真门）；负例误报率 0%
也不等于「无残留」，只等于「本夹具登记的那几条护栏没被突破」。

用法：
    python3 eng/tools/doccheck/measure_assert_dup_accuracy.py
    python3 eng/tools/doccheck/measure_assert_dup_accuracy.py --root .
    python3 eng/tools/doccheck/measure_assert_dup_accuracy.py --only POS-01 NEG-02
    python3 eng/tools/doccheck/measure_assert_dup_accuracy.py --json-out /tmp/acc.json

依赖：PyYAML（已实测 6.0.2 可用）。若不可用则自动退化为内嵌的极小 YAML 子集解析，
**不做 pip install**。
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "check_assert_dup.py")
FIXTURES = os.path.join(HERE, "assert_dup_fixtures.yaml")
TIMEOUT_S = 120

# 夹具文件与本测量脚本自身也在 eng/tools/doccheck/ 下，也在工具的扫描面内。
# 它们逐字包含全部查询串与全部命中行号，会把每一条查询的命中数抬高（实测
# `--num 1.482602218505602` 由 205 抬到 231），构成自污染。
# ⇒ 统一用 --exclude 把这两个文件挡在扫描面外。不排除就会得到无意义的数。
SELF_EXCLUDE = [
    "--exclude", "eng/tools/doccheck/assert_dup_fixtures.yaml",
    "--exclude", "eng/tools/doccheck/measure_assert_dup_accuracy.py",
]

# ── YAML 载入：PyYAML 优先，缺失时退化为内嵌子集解析 ────────────────────────
def load_fixtures(path):
    with open(path, "r", encoding="utf-8") as fh:
        text = fh.read()
    try:
        import yaml  # noqa: F401
        return yaml.safe_load(text), "PyYAML"
    except ImportError:
        return _mini_yaml(text), "内嵌解析（PyYAML 不可用）"


def _mini_yaml(text):
    """只支持本夹具文件用到的子集：顶层 `fixtures:` 列表 + `- id:` 项 + 缩进标量。

    退路，不是通用解析器。存在意义：PyYAML 万一缺失时仍能跑，且**不 pip install**。
    `>-` 折叠标量按字面拼接（保留换行不影响断言，因为期望值全是单行路径串）。
    实现要点：列表项既可能是 `- id: X`（同行带键），也可能是纯列表续行
    `- ["--text", "a, b"]`（queries 的子查询）。后者含冒号/逗号时不能按 `k: v` 切。
    """
    items, cur, listkey, dictkey = [], None, None, None
    for raw in text.splitlines():
        if not raw.strip() or raw.strip().startswith("#"):
            continue
        if raw.strip() == "fixtures:":
            continue
        indent = len(raw) - len(raw.lstrip())
        s = raw.strip()

        if s.startswith("- ") and indent <= 4:
            # 新夹具项（顶层 `- id: X`）
            if listkey and cur is not None:
                cur.setdefault(listkey, []).append(_scalar(s[2:]))
                continue
            cur, listkey, dictkey = {}, None, None
            items.append(cur)
            k, _, v = s[2:].partition(":")
            cur[k.strip()] = _uncomment(v.strip())
            continue

        if cur is None:
            continue

        # 嵌套映射的条目：must_hit_layer 的 `"loc": "LAYER"`。
        # 只有「键是带引号的路径」才算本映射的条目；一旦出现 `note:` / 下一个键
        # （不带引号），立即退出映射态，否则会把整条 note 吞进映射里。
        body = s[2:] if s.startswith("- ") else s
        if dictkey and indent >= 6 and body[:1] in "\"'":
            # 用 rpartition：路径键本身含 `:`（如 "docs/.../PSF.md:85"），
            # 按第一个冒号切会把路径腰斩。
            k, _, v = body.rpartition(":")
            cur.setdefault(dictkey, {})[k.strip().strip("\"'")] = \
                _uncomment(v.strip()).strip("\"'")
            continue
        dictkey = None

        if s.startswith("- ") and listkey:
            cur.setdefault(listkey, []).append(_scalar(s[2:]))
            continue

        if ":" not in s:
            continue
        k, _, v = s.partition(":")
        k, v = k.strip(), _uncomment(v.strip())
        if v in (">-", ">", "|", "|-"):
            # 折叠标量：期望值不依赖它（note / human_ruling 仅供人读）。
            # **必须清 listkey/dictkey**，否则下一个 `- id: X` 会被当成上一个列表
            # 或映射的续行，把新夹具吞进上一条（第一版就漏了 NEG-02）。
            cur[k] = v
            listkey = dictkey = None
        elif v == "":
            if k == "must_hit_layer":
                cur[k] = {}
                dictkey, listkey = k, None
            else:
                cur[k] = []
                listkey, dictkey = k, None
        elif v.startswith("["):
            cur[k] = _tok(v)
            listkey = dictkey = None
        else:
            cur[k] = v
            listkey = dictkey = None
    return {"fixtures": _mini_fix(items)}


def _scalar(item):
    """列表里的一项：整体带引号的是**单个字符串**（路径），否则按查询串切词。"""
    t = _uncomment(item.strip()).rstrip()
    if len(t) >= 2 and t[0] == t[-1] and t[0] in "\"'":
        return t[1:-1]
    return _tok(t)


def _mini_fix(items):
    """把退路解析出的标量补成夹具结构，并抽出 queries。"""
    out = []
    for it in items:
        f = {
            "id": it.get("id", "?"),
            "kind": it.get("kind", "unknown"),
            "title": it.get("title", ""),
            "verified_by": it.get("verified_by", "待核"),
            "known_miss": it.get("known_miss") == "true",
            "expect": {
                "must_hit": it.get("must_hit") or [],
                "must_not_hit": it.get("must_not_hit") or [],
                "forbid_matched": it.get("forbid_matched") or [],
                "forbid_verdicts": it.get("forbid_verdicts") or [],
                "known_miss": it.get("known_miss") or [],
                "known_false_positive": it.get("known_false_positive") or [],
                "known_unrelated_hits": it.get("known_unrelated_hits") or [],
                "must_hit_layer": dict(it.get("must_hit_layer") or {}),
            },
        }
        qs = []
        if it.get("query"):
            q0 = it["query"]
            qs.append(_tok(q0) if isinstance(q0, str) else list(q0))
        for q in it.get("queries") or []:
            qs.append(_tok(q) if isinstance(q, str) else list(q))
        f["query"] = qs[0] if len(qs) == 1 else qs
        if it.get("expect_zero_hits_for_query"):
            f["expect"]["expect_zero_hits_for_query"] = int(
                it["expect_zero_hits_for_query"])
        out.append(f)
    return out


def _uncomment(s):
    """截掉行尾注释：引号外的 ` #` 之后全丢。引号内的 # 保留。"""
    quote = None
    for i, ch in enumerate(s):
        if quote:
            if ch == quote:
                quote = None
        elif ch in "\"'":
            quote = ch
        elif ch == "#" and (i == 0 or s[i - 1].isspace()):
            return s[:i]
    return s


def _tok(s):
    """把 `["--text", "a, b"] # 说明` 或 `--text "a, b"` 切成 ['--text', 'a, b']。"""
    s = _uncomment(s.strip())
    if s.startswith("[") and s.endswith("]"):
        s = s[1:-1]
    out, cur, quote = [], "", None
    for ch in s:
        if quote:
            if ch == quote:
                quote = None
            else:
                cur += ch
            continue
        if ch in "\"'":
            quote = ch
        elif ch.isspace() or ch == ",":
            if cur:
                out.append(cur)
                cur = ""
        else:
            cur += ch
    if cur:
        out.append(cur)
    return out


def queries_of(fx):
    """夹具的查询列表。`query` 单条 / `queries` 多条子查询。"""
    q = fx.get("query")
    if q:
        return [q] if (isinstance(q, list) and q and isinstance(q[0], str)) else list(q)
    qs = fx.get("queries")
    if not qs:
        return []
    return [x if isinstance(x, list) else _tok(x) for x in qs]


# ── 调用被测工具 ────────────────────────────────────────────────────────────
def run_tool(query, root):
    """返回 (payload_dict, error_str)。error 非空表示这条查询没跑成。"""
    cmd = ([sys.executable, TOOL] + list(query) + SELF_EXCLUDE
           + ["--root", root, "--format", "json"])
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=TIMEOUT_S)
    except subprocess.TimeoutExpired:
        return None, "timeout(>%ds)" % TIMEOUT_S
    if proc.returncode != 0:
        return None, "exit=%d %s" % (proc.returncode,
                                    (proc.stderr or "").strip().splitlines()[-1:]
                                    and (proc.stderr or "").strip().splitlines()[-1]
                                    or "")
    try:
        return json.loads(proc.stdout), None
    except json.JSONDecodeError as exc:
        return None, "bad json: %s" % exc


def hit_keys(payload):
    return {"%s:%d" % (h["file"], h["line"]) for h in payload.get("命中", [])}


def matched_values(payload):
    return {str(h.get("matched", "")) for h in payload.get("命中", [])}


def verdicts(payload):
    return {str(h.get("verdict", "")) for h in payload.get("命中", [])}


# ── 判定 ────────────────────────────────────────────────────────────────────
def eval_fixture(fx, root):
    r = {
        "id": fx["id"], "kind": fx.get("kind", "?"),
        "title": fx.get("title", ""), "verified_by": fx.get("verified_by", "待核"),
        "queries": [], "union": set(), "errors": [],
        "missing_must_hit": [], "hit_must_not_hit": [],
        "matched_forbidden": [], "verdict_forbidden": [],
        "unexpected_hits": [], "known_miss_resolved": [],
        "known_fp_hit": [], "known_unrelated_hit": [],
        "layers": {}, "must_hit_by_query": {},
    }
    exp = fx.get("expect") or {}
    q_list = queries_of(fx)
    per_payloads = []
    sub_hits = []          # 每条子查询的命中集合，用于标注 must_hit 由哪条子查询命中

    for qi, q in enumerate(q_list):
        payload, err = run_tool(q, root)
        r["queries"].append({"query": q, "error": err})
        if err:
            r["errors"].append(err)
            sub_hits.append(set())
            continue
        s = payload.get("汇总", {})
        r["queries"][qi].update({
            "命中数": s.get("命中数", 0), "文件数": s.get("文件数", 0),
            "层数": s.get("层数", 0),
            "扫描文件数": (payload.get("口径") or {}).get("扫描文件数"),
            "耗时秒": (payload.get("口径") or {}).get("耗时秒"),
        })
        per_payloads.append(payload)
        sub_hits.append(hit_keys(payload))
        r["union"] |= sub_hits[-1]

    if exp.get("expect_zero_hits_for_query") is not None:
        idx = int(exp["expect_zero_hits_for_query"])
        if 0 <= idx < len(r["queries"]) and "命中数" in r["queries"][idx]:
            r["zero_hits_ok"] = (r["queries"][idx]["命中数"] == 0)
        else:
            r["zero_hits_ok"] = False
            r["errors"].append("子查询 %d 未跑成，无法判 0 命中" % idx)

    for loc in exp.get("must_hit") or []:
        if loc not in r["union"]:
            r["missing_must_hit"].append(loc)
            continue
        for payload in per_payloads:
            for h in payload.get("命中", []):
                if "%s:%d" % (h["file"], h["line"]) == loc:
                    r["layers"][loc] = h.get("layer")
        r["must_hit_by_query"][loc] = [
            i for i, keys in enumerate(sub_hits) if loc in keys]

    for loc, layer in (exp.get("must_hit_layer") or {}).items():
        got = r["layers"].get(loc)
        if got != layer:
            r.setdefault("layer_mismatch", []).append(
                {"loc": loc, "want": layer, "got": got})

    for loc in exp.get("must_not_hit") or []:
        if loc in r["union"]:
            r["hit_must_not_hit"].append(loc)

    # known_false_positive：人读判定为误报、但**已知工具会命中**的位置。
    # 它计入负例误报率（那是实测数），但**不**让夹具 FAIL——否则这条夹具
    # 永远红，红的却是「文档如实登记了一个已知误报面」，不是工具行为变了。
    for loc in exp.get("known_false_positive") or []:
        if loc in r["union"]:
            r["known_fp_hit"].append(loc)

    # known_unrelated_hits：命中了，但人读判定与本条断言无关（如工具自身 README 里的用法示例）。
    # 只登记与汇报，不计入任何分母、不判 FAIL。存在的意义是把「命中集合的全部成员」写全，
    # 免得下次有人把一处无关提及当成工具退化。
    for loc in exp.get("known_unrelated_hits") or []:
        if loc in r["union"]:
            r["known_unrelated_hit"].append(loc)

    all_matched = set()
    all_verd = set()
    for payload in per_payloads:
        all_matched |= matched_values(payload)
        all_verd |= verdicts(payload)
    for v in exp.get("forbid_matched") or []:
        if v in all_matched:
            r["matched_forbidden"].append(v)
    for v in exp.get("forbid_verdicts") or []:
        if v in all_verd:
            r["verdict_forbidden"].append(v)

    # known_miss：人读确认该处确有该断言、但工具抓不到。
    # 若它出现在命中里 ⇒ 已登记的限制消失了，报 FAIL 提醒撤掉标注。
    for loc in exp.get("known_miss") or []:
        if loc in r["union"]:
            r["known_miss_resolved"].append(loc)

    declared = set(exp.get("must_hit") or []) | set(exp.get("must_not_hit") or []) \
        | set(exp.get("known_miss") or []) | set(exp.get("known_false_positive") or []) \
        | set(exp.get("known_unrelated_hits") or []) \
        | set((exp.get("must_hit_layer") or {}).keys())
    r["unexpected_hits"] = sorted(r["union"] - declared)

    reasons = []
    if r["errors"]:
        reasons.append("查询未跑成(%s)" % r["errors"][0])
    if r["missing_must_hit"]:
        reasons.append("must_hit 漏 %d 条" % len(r["missing_must_hit"]))
    if r["hit_must_not_hit"]:
        reasons.append("must_not_hit 被误报 %d 条" % len(r["hit_must_not_hit"]))
    if r["matched_forbidden"]:
        reasons.append("禁值命中 %d 个" % len(r["matched_forbidden"]))
    if r["verdict_forbidden"]:
        reasons.append("误贴初判 %d 个" % len(r["verdict_forbidden"]))
    if r["known_miss_resolved"]:
        reasons.append("已登记的已知漏检消失了 %d 条" % len(r["known_miss_resolved"]))
    if r.get("layer_mismatch"):
        reasons.append("层归类不符 %d 条" % len(r["layer_mismatch"]))
    if r.get("zero_hits_ok") is False:
        reasons.append("期望 0 命中的子查询没跑成或非 0")

    r["reasons"] = reasons
    r["status"] = "PASS" if not reasons else "FAIL"
    return r


# ── 报告 ────────────────────────────────────────────────────────────────────
BANNER = """\
────────────────────────────────────────────────────────────────
【本脚本是诊断工具，不是门禁】
· 退出码恒为 0。PASS/FAIL 打在 stdout，不作为阻断条件。
· **计数只作分母，判定由人做。** 命中率 100% ≠ 仓内无矛盾；误报率 0% ≠ 无残留。
· 期望值全部来自 assert_dup_fixtures.yaml 里的人工判读，不由工具反推。
· 复跑不得换口径（--prefix-min / --tol / --include-run / --root），否则命中率不可比。
────────────────────────────────────────────────────────────────"""


def fmt_layer_mismatch(ms):
    return "；".join("%s 期望 %s 实得 %s" % (m["loc"], m["want"], m["got"])
                     for m in ms)


def q_list_of(r):
    return [m.get("query") for m in r.get("queries", [])]


def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="measure_assert_dup_accuracy.py",
        description="断言副本检测器夹具准确率测量（诊断，非门禁；退出码恒 0）")
    ap.add_argument("--root", default=".", help="仓根（默认当前目录）")
    ap.add_argument("--fixtures", default=FIXTURES, help="夹具文件路径")
    ap.add_argument("--only", nargs="*", default=None,
                    help="只跑这些夹具 id（可多个）")
    ap.add_argument("--json-out", default=None, help="把结构化结果另存到该路径")
    args = ap.parse_args(argv)

    root = os.path.abspath(args.root)
    data, loader = load_fixtures(args.fixtures)
    fixtures = data.get("fixtures") or []

    if args.only:
        want = set(args.only)
        fixtures = [f for f in fixtures if f.get("id") in want]
        if not fixtures:
            print("没有 id 命中 --only %s；可用 id：%s"
                  % (sorted(want), [f.get("id") for f in data.get("fixtures") or []]))

    print(BANNER)
    print("夹具文件：%s" % os.path.abspath(args.fixtures))
    print("被测工具：%s" % TOOL)
    print("仓根    ：%s" % root)
    print("YAML    ：%s" % loader)
    print("夹具条数：%d（正例 %d / 负例 %d）"
          % (len(fixtures),
             sum(1 for f in fixtures if f.get("kind") == "positive"),
             sum(1 for f in fixtures if f.get("kind") == "negative")))
    print("每条查询 timeout=%ds；查询总数 %d"
          % (TIMEOUT_S, sum(max(1, len(queries_of(f))) for f in fixtures)))
    print("-" * 72)
    print()

    results = [eval_fixture(f, root) for f in fixtures]

    print("【逐条 PASS/FAIL 表】")
    print("%-8s %-9s %-4s %-9s %s" % ("ID", "类别", "结果", "命中数", "判定依据"))
    print("-" * 72)
    for r in results:
        hits = sum(m.get("命中数", 0) for m in r["queries"] if "命中数" in m)
        flag = "（已知漏检）" if any(
            (f.get("expect") or {}).get("known_miss")
            for f in fixtures if f.get("id") == r["id"]) else ""
        print("%-8s %-9s %-4s %-9s %s%s"
              % (r["id"], r["kind"], r["status"], hits,
                 ("；".join(r["reasons"]) if r["reasons"] else "全部期望满足"), flag))
        for q in r["queries"]:
            cmd = " ".join(q["query"]) if q.get("query") else "—"
            if q.get("error"):
                print("         └ 查询 %-46s  %s" % (cmd, q["error"]))
            else:
                print("         └ 查询 %-46s  命中=%s 文件=%s 层=%s 扫描=%s 耗时=%ss"
                      % (cmd, q["命中数"], q["文件数"], q["层数"],
                         q["扫描文件数"], q["耗时秒"]))
        for loc in r["missing_must_hit"]:
            print("         └ 漏命中（应出现未见）：%s" % loc)
        if len(q_list_of(r)) > 1:
            for loc, byq in sorted(r["must_hit_by_query"].items()):
                tags = ",".join("子查询%d" % i for i in byq)
                lay = r["layers"].get(loc, "—")
                print("         └ must_hit 由 %s 命中：%s  [层 %s]" % (tags, loc, lay))
        for loc in r["hit_must_not_hit"]:
            print("         └ 误报（不应出现却出现）：%s" % loc)
        for v in r["matched_forbidden"]:
            print("         └ 禁值被命中：%r" % v)
        for v in r["verdict_forbidden"]:
            print("         └ 被误贴初判：%r" % v)
        for loc in r["known_miss_resolved"]:
            print("         └ 已登记的已知漏检消失了（该撤掉标注）：%s" % loc)
        for loc in r["known_fp_hit"]:
            print("         └ 已登记的误报（预期内，计入误报率，不判 FAIL）：%s" % loc)
        for loc in r["known_unrelated_hit"]:
            print("         └ 已登记的无关提及（不计入任何分母，不判 FAIL）：%s" % loc)
        if r.get("layer_mismatch"):
            print("         └ 层归类不符：%s" % fmt_layer_mismatch(r["layer_mismatch"]))
    print("-" * 72)
    print()

    pos = [r for r in results if r["kind"] == "positive"]
    neg = [r for r in results if r["kind"] == "negative"]

    pos_exp = sum(len((_fx(r["id"], fixtures).get("expect") or {}).get("must_hit") or [])
                  for r in pos)
    pos_got = pos_exp - sum(len(r["missing_must_hit"]) for r in pos)
    neg_exp = sum(len((_fx(r["id"], fixtures).get("expect") or {}).get("must_not_hit") or [])
                  for r in neg)
    neg_bad = sum(len(r["hit_must_not_hit"]) for r in neg)
    kfp_exp = sum(len((_fx(r["id"], fixtures).get("expect") or {})
                      .get("known_false_positive") or []) for r in neg)
    kfp_hit = sum(len(r["known_fp_hit"]) for r in neg)

    print("【两条汇总数】")
    print("  正例命中率（must_hit）    ：%d / %d = %s"
          % (pos_got, pos_exp,
             ("%.1f%%" % (100.0 * pos_got / pos_exp)) if pos_exp else "—（无分母）"))
    fp_num, fp_den = neg_bad + kfp_hit, neg_exp + kfp_exp
    print("  负例误报率                ：%d / %d = %s"
          % (fp_num, fp_den, ("%.1f%%" % (100.0 * fp_num / fp_den)) if fp_den else "—（无分母）"))
    print("      其中 must_not_hit 被突破 ：%d / %d %s"
          % (neg_bad, neg_exp, "（护栏失守，判 FAIL）" if neg_bad else "（护栏全部守住）"))
    print("      其中 known_false_positive ：%d / %d %s"
          % (kfp_hit, kfp_exp, "（已登记的固有误报面，预期内）" if kfp_exp else "（无登记项）"))
    print()
    print("  分母口径：正例分母只数 must_hit 条数；负例分母数 must_not_hit +")
    print("  known_false_positive 条数。forbid_matched / forbid_verdicts /")
    print("  expect_zero_hits / known_miss 是独立断言，逐条 PASS/FAIL 里体现，")
    print("  **不进上面两个分母**——它们量的是护栏与已登记限制，不是召回。")
    print()

    n_pass = sum(1 for r in results if r["status"] == "PASS")
    print("【总计】PASS %d / FAIL %d（共 %d 条）" % (n_pass, len(results) - n_pass, len(results)))
    print()

    print("【读法 · 别把这两行数当结论】")
    print("  · 正例命中率只说「夹具登记的那些形态，工具抓到了没有」。")
    print("    抓不到时先分清是工具的错还是夹具写错了，逐条看上面的判定依据列。")
    print("  · 负例误报率只说「夹具登记的那几条护栏没被突破」。")
    print("    负例本来就是人读判定为非矛盾的，命中不等于误报——")
    print("    NEG-02 就是命中 44 处、误报 0 处的典型：命中全是故意写的反例。")
    print("  · 命中 0 处不等于无矛盾；命中 N 处不等于 N 个矛盾。两种方向的误读都发生过。")
    print("  · 工具抓不到的形态（大小写混合类型名、被改写过的式子、同名不同义、")
    print("    恒真门/恒红门）不进入任何一条分母。已实测的限制记在 NEG-04。")
    print("  · 退出码恒 0：这份表是给人读的，不是给 CI 阻断的。")

    if args.json_out:
        out = []
        for r in results:
            r = dict(r)
            r.pop("union", None)
            out.append(r)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump({
                "定位": "诊断工具；退出码恒 0；计数只作分母，判定由人做",
                "仓根": root, "夹具文件": os.path.abspath(args.fixtures),
                "YAML 载入": loader,
                "正例命中率": {"命中": pos_got, "应命中": pos_exp},
                "负例误报率": {"误报": fp_num, "登记": fp_den,
                              "must_not_hit被突破": neg_bad,
                              "known_false_positive": kfp_hit},
                "结果": out,
            }, fh, ensure_ascii=False, indent=2)
        print()
        print("结构化结果已存：%s" % os.path.abspath(args.json_out))

    return 0


def _fx(fid, fixtures):
    for f in fixtures:
        if f.get("id") == fid:
            return f
    return {}


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)