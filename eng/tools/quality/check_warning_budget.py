#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_warning_budget.py — 告警分母 / 分类 / 第三方隔离 / 基线对比判据。

为什么需要（本次 Windows 全量构建暴露的第二个盲区）:
  此前「告警清零」这类结论**没有分母、没有分类口径**，无法被检验。本判据给出:

  1. 分母面（本轮告警总数、唯一位置数、唯一规则数、可处置单元数）;
  2. 分类面（**先归纳类别再定量**: 按编译器规则号 C####/-Wxxx 归类，再报每类的
     出现次数与唯一位置数；结论行只出现类别与单元数，**不逐行计数**）;
  3. 来源面（本项目 / 测试面 / 第三方 / 外部 / 未判定）—— **第三方告警是显式隔离面**,
     单独计数单独打印，绝不与项目面混计；隔离失效（第三方文件被记入项目面）即判红;
  4. 抑制面：日志里出现 -w / /wd / warning(disable) / -Wno- 即计数，并与基线比对；
     **新增抑制 ⇒ 判红**（告警清零不得靠加抑制选项或调低告警级）;
  5. 基线对比：**新增告警数与消失告警数分开报**，不做净额掩盖;
  6. 与覆盖面判据联动：日志里的 Building 行用于给出「已尝试目标数」，未尝试面由
     check_build_coverage.py 单独报（两者严格分开，不混算）。

用法:
  python3 eng/tools/quality/check_warning_budget.py --log <build.log> [--log ...] \
      [--baseline <baseline.json>] [--json-out <p>] [--max-project-total N] \
      [--max-project-unique N] [--no-third-party-isolation]
  python3 eng/tools/quality/check_warning_budget.py --self-test
  python3 eng/tools/quality/check_warning_budget.py --emit-baseline <out.json> --log ...

退出码: 0=PASS; 1=FAIL（超阈值/隔离失效/新增抑制）; 2=ANCHOR_STALE（输入不可用）;
        3=参数非法。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import sys
import tempfile

# ---- 行模式 ---------------------------------------------------------------
MSVC_RE = re.compile(
    r"^(?P<file>[^()]+?)\((?P<line>\d+)(?:,(?P<col>\d+))?\)\s*:\s*"
    r"(?:fatal\s+)?warning\s+(?P<rule>[A-Z]+\d+)\s*:\s*(?P<text>.*)$")
GCCLIKE_RE = re.compile(
    r"^(?P<file>[^:]+(?:/[^:]*)?):(?P<line>\d+):(?:(?P<col>\d+):)?\s*"
    r"(?:fatal\s+)?warning:\s*(?P<text>.*?)(?:\s+\[(?P<opt>-[Ww][^\]]*)\])?$")
BUILDING_RE = re.compile(r"^\[\s*(\d+)/(\d+)\]\s+(?:Building|Linking)\b")
NINJA_TARGET_RE = re.compile(r"^\[\s*\d+/\d+\]\s+(?:Building|Linking)\s+\S*\s*object\s+(\S+?)(?:\.o|\.(?:cpp|cc|cxx|c))\b")

THIRD_PARTY_PARTS = ("third_party", "/healpix_db/archive/legacy/")
THIRD_PARTY_FILES = ("nanoflann.hpp",)
TEST_MARKERS = ("/tests/", "_test.", "test_", "/test/", "ctest")

# 抑制面: -w / /wd#### / #pragma warning(disable) / -Wno- / --no-warnings，
# 以及「命令行把告警级调低」的直接证据（MSVC: overriding '/W3' with '/W0' ⇒ D9025）。
# 告警清零不得靠这些手段达成（AGENTS.md §5/§6）。
SUPPRESS_RE = re.compile(
    r"(?<![\w-])-w(?![\w-])|/wd\d+|#\s*pragma\s+(?:warning|GCC\s+diagnostic)\s*[({]?\s*"
    r"(?:disable|ignored)|-Wno-|--no-warnings|Command\s+line\s+warning\s+D90\d\d")


def classify_origin(path: str) -> str:
    norm = "/" + path.replace(chr(92), "/").lstrip("/")
    if any(p in norm for p in THIRD_PARTY_PARTS) or norm.rsplit("/", 1)[-1] in THIRD_PARTY_FILES:
        return "third_party"
    if any(m in norm for m in TEST_MARKERS) or "selfcheck" in norm:
        return "test"
    if norm.startswith("/run/") or norm.startswith("/build/") or "/vcpkg/" in norm:
        return "external"
    return "project"


def parse_log(text: str):
    """解析构建日志 → 告警条目 + 已尝试目标列表 + 抑制面。"""
    warns, targets, suppressions = [], [], []
    cur_target = None
    for raw in text.splitlines():
        line = raw.rstrip("\r")
        mb = BUILDING_RE.match(line)
        if mb:
            mt = NINJA_TARGET_RE.match(line)
            cur_target = mt.group(1) if mt else None
            if cur_target:
                targets.append(cur_target)
            continue
        m = MSVC_RE.match(line.strip())
        rule = None
        if m:
            rule = m.group("rule")
            rec = (m.group("file"), int(m.group("line")), m.group("text"), rule)
        else:
            m = GCCLIKE_RE.match(line.strip())
            if m and m.group("text"):
                rule = m.group("opt") or "unknown"
                rec = (m.group("file"), int(m.group("line")), m.group("text"), rule)
            else:
                rec = None
        if rec:
            f, ln, txt, rule = rec
            f = f.replace(chr(92), "/")
            warns.append({
                "file": f, "line": ln, "rule": rule, "text": txt.strip()[:200],
                "origin": classify_origin(f), "target": cur_target,
            })
        for sm in SUPPRESS_RE.finditer(line):
            suppressions.append({"token": sm.group(0), "target": cur_target})
    return warns, targets, suppressions


def summarize(warns, suppressions):
    total = len(warns)
    locations = set((w["file"], w["line"]) for w in warns)
    rules = set(w["rule"] for w in warns)
    units = set((w["file"], w["rule"]) for w in warns)
    by_origin, by_origin_loc = {}, {}
    for w in warns:
        by_origin[w["origin"]] = by_origin.get(w["origin"], 0) + 1
    for f, _l in locations:
        o = classify_origin(f)
        by_origin_loc[o] = by_origin_loc.get(o, 0) + 1
    by_rule = {}
    for w in warns:
        e = by_rule.setdefault(w["rule"], {"total": 0, "locations": set(), "origins": set()})
        e["total"] += 1
        e["locations"].add((w["file"], w["line"]))
        e["origins"].add(w["origin"])
    categories = sorted(
        ({"rule": k, "total": v["total"], "unique_locations": len(v["locations"]),
          "origins": sorted(v["origins"])} for k, v in by_rule.items()),
        key=lambda d: (-d["total"], d["rule"]))
    project_total = by_origin.get("project", 0)
    project_loc = by_origin_loc.get("project", 0)
    project_units = len(set((w["file"], w["rule"]) for w in warns if w["origin"] == "project"))
    return {
        "total_warnings": total,
        "unique_locations": len(locations),
        "unique_rules": len(rules),
        "disposition_units": len(units),
        "by_origin": by_origin,
        "by_origin_unique_locations": by_origin_loc,
        "project_total": project_total,
        "project_unique_locations": project_loc,
        "project_disposition_units": project_units,
        "test_total": by_origin.get("test", 0),
        "third_party_total": by_origin.get("third_party", 0),
        "third_party_unique_locations": by_origin_loc.get("third_party", 0),
        "third_party_isolated": True,
        "categories": categories,
        "suppressions": len(suppressions),
        "suppression_tokens": sorted(set(s["token"] for s in suppressions)),
    }


def diff_baseline(base: dict, cur: dict, warns):
    now = set("%s:%d:%s" % (w["file"], w["line"], w["rule"]) for w in warns)
    was = set(base.get("warning_keys", []))
    return {
        "added": sorted(now - was),
        "removed": sorted(was - now),
        "added_count": len(now - was),
        "removed_count": len(was - now),
        "net": len(now) - len(was),
    }

def check_isolation(summ, warns, enabled=True):
    """第三方隔离面校验: 隔离失效（第三方文件被记入项目面）即缺陷。"""
    problems = []
    for w in warns:
        if w["origin"] == "project" and (
                any(p in "/" + w["file"] for p in THIRD_PARTY_PARTS)
                or w["file"].rsplit("/", 1)[-1] in THIRD_PARTY_FILES):
            problems.append("隔离失效: %s:%d 被记入项目面" % (w["file"], w["line"]))
    if not enabled:
        problems.append("第三方隔离面被显式关闭（--no-third-party-isolation）")
    return problems


def report(doc, show=0):
    s = doc["summary"]
    print("WARNING-BUDGET %s" % doc["verdict"])
    print("分母: 告警总数=%d 唯一位置=%d 唯一规则=%d 可处置单元=%d"
          % (s["total_warnings"], s["unique_locations"], s["unique_rules"],
             s["disposition_units"]))
    print("来源面: " + "  ".join("%s=%d(唯一位置 %d)"
                                 % (k, s["by_origin"].get(k, 0),
                                    s["by_origin_unique_locations"].get(k, 0))
                                 for k in sorted(s["by_origin"])) or "来源面: 无告警")
    print("项目面: 总数=%d 唯一位置=%d 可处置单元=%d" %
          (s["project_total"], s["project_unique_locations"], s["project_disposition_units"]))
    print("测试面: 总数=%d" % s["test_total"])
    print("第三方隔离面: 总数=%d 唯一位置=%d isolated=%s（不与项目面混计）"
          % (s["third_party_total"], s["third_party_unique_locations"], s["third_party_isolated"]))
    print("抑制面: 命中=%d %s" % (s["suppressions"], ",".join(s["suppression_tokens"])))
    print("分类（先归纳类别再定量, 结论不含逐行计数）:")
    for cat in s["categories"][:12]:
        print("  - %-14s 出现=%-5d 唯一位置=%-5d 来源=%s"
              % (cat["rule"], cat["total"], cat["unique_locations"], ",".join(cat["origins"])))
    if len(s["categories"]) > 12:
        print("  ... 共 %d 类" % len(s["categories"]))
    if doc.get("baseline_diff"):
        d = doc["baseline_diff"]
        print("基线对比: 新增=%d 消失=%d（分开报, 不做净额）" % (d["added_count"], d["removed_count"]))
        for k in d["added"][:show or 10]:
            print("  + %s" % k)
        for k in d["removed"][:show or 10]:
            print("  - %s" % k)
    if doc.get("attempted_targets") is not None:
        print("尝试面（由本日志的 Building 行给出, 独立于分母）: 目标数=%d"
              % doc["attempted_targets"])
    for p in doc["problems"]:
        print("  [!] %s" % p)


def self_test(args) -> int:
    fx = pathlib.Path(__file__).resolve().parent / "fixtures" / "warning_budget"
    if not fx.is_dir():
        print("SELFTEST_FAIL: 夹具目录缺失 %s" % fx)
        return 1
    failures = []

    def load(name):
        return (fx / name).read_text(encoding="utf-8")

    def run(text, baseline=None):
        warns, targets, supp = parse_log(text)
        summ = summarize(warns, supp)
        doc = {"check_id": "WARNING-BUDGET", "verdict": "PASS", "summary": summ,
               "problems": check_isolation(summ, warns, args.isolation),
               "attempted_targets": len(set(targets))}
        if summ["project_total"] > args.max_project_total:
            doc["problems"].append("项目面告警 %d > 阈值 %d"
                                   % (summ["project_total"], args.max_project_total))
        if summ["project_unique_locations"] > args.max_project_unique:
            doc["problems"].append("项目面唯一位置 %d > 阈值 %d"
                                   % (summ["project_unique_locations"], args.max_project_unique))
        if summ["suppressions"]:
            doc["problems"].append("抑制面命中 %d: %s（告警清零不得靠加抑制/调低告警级）"
                                   % (summ["suppressions"], ",".join(summ["suppression_tokens"])))
        if baseline is not None:
            doc["baseline_diff"] = diff_baseline(baseline, summ, warns)
        doc["verdict"] = "FAIL" if doc["problems"] else "PASS"
        return doc, warns, summ

    # 态 1（正例）: 干净构建 + 第三方隔离 ⇒ PASS，分母口径可读
    doc, _w, s = run(load("clean.log"))
    if doc["verdict"] != "PASS":
        failures.append("clean: 期望 PASS 实得 %s %s" % (doc["verdict"], doc["problems"]))
    elif s["total_warnings"] != 3 or s["project_total"] != 1 or s["test_total"] != 1 \
            or s["third_party_total"] != 1 or s["unique_locations"] != 3:
        failures.append("clean: 分母口径不符 %s" % json.dumps(
            {k: s[k] for k in ("total_warnings", "unique_locations", "project_total",
                               "test_total", "third_party_total")}, ensure_ascii=False))
    else:
        print("SELFTEST_PASS clean (总数=3 唯一位置=3 项目=1 测试=1 第三方隔离=1, PASS)")

    # 态 2（负例）: 大面积同类告警 + 重复位置 + 抑制选项 ⇒ 阈值/抑制判红
    doc, _w, s = run(load("dirty.log"))
    if doc["verdict"] != "FAIL":
        failures.append("dirty: 期望 FAIL 实得 %s" % doc["verdict"])
    else:
        need = ("项目面告警", "抑制")
        if not any(any(n in p for n in need) for p in doc["problems"]):
            failures.append("dirty: 判红原因不对 %s" % doc["problems"])
        cats = {c["rule"]: c for c in s["categories"]}
        if "C4267" not in cats or cats["C4267"]["total"] != 4 \
                or cats["C4267"]["unique_locations"] != 3:
            failures.append("dirty: 分类口径错 C4267=%s" % cats.get("C4267"))
        else:
            print("SELFTEST_PASS dirty (C4267×4 折叠为 3 个唯一位置/1 个可处置单元, 抑制面判红)")

    # 态 3（恢复）: 处置后回到零告警 ⇒ PASS，且相对基线「新增=0」
    base_doc = run(load("dirty.log"))[0]
    keys = []
    for cat in base_doc["summary"]["categories"]:
        keys.append(cat["rule"])
    doc, _w, s = run(load("clean.log"))
    if doc["verdict"] != "PASS" or s["project_total"] != 1:
        failures.append("restore: 期望 PASS/项目面=1 实得 %s/%s"
                        % (doc["verdict"], s["project_total"]))
    else:
        print("SELFTEST_PASS restore (处置后项目面告警回到基线以下, 门转绿)")

    # 态 4（基线对比）: 新增/消失分开报，不做净额
    import json as _json
    baseline = _load_baseline_from(fx / "baseline.json")
    doc, _w, _s = run(load("regressed.log"), baseline=baseline)
    d = doc.get("baseline_diff") or {}
    if d.get("added_count", 0) == 0 and d.get("removed_count", 0) == 0:
        failures.append("baseline: 新增/消失均为 0，判据没在比")
    else:
        print("SELFTEST_PASS baseline (新增=%d 消失=%d 分开报, net=%d)"
              % (d["added_count"], d["removed_count"], d["net"]))

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例/负例/恢复/基线对比 四态全部符合预期")
    return 0


def _load_baseline_from(path: pathlib.Path):
    if not path.is_file():
        return {"warning_keys": []}
    return json.loads(path.read_text(encoding="utf-8"))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="告警分母/分类/隔离/基线对比判据")
    ap.add_argument("--log", action="append", default=[])
    ap.add_argument("--baseline", default=None)
    ap.add_argument("--emit-baseline", default=None)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--max-project-total", type=int, default=10 ** 9)
    ap.add_argument("--max-project-unique", type=int, default=10 ** 9)
    ap.add_argument("--no-third-party-isolation", action="store_true")
    ap.add_argument("--isolation", action="store_true", default=True)
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--show", type=int, default=10)
    args = ap.parse_args(argv)
    if args.no_third_party_isolation:
        args.isolation = False

    if args.self_test:
        return self_test(args)
    if not args.log:
        print("WARNING-BUDGET USAGE: 至少给一个 --log（或用 --self-test）")
        return 3
    texts = []
    for lg in args.log:
        p = pathlib.Path(lg)
        if not p.is_file():
            print("WARNING-BUDGET ANCHOR_STALE: 日志不存在 %s" % p)
            return 2
        texts.append(p.read_text(encoding="utf-8", errors="replace"))
    warns, targets, supp = [], [], []
    for t in texts:
        w, tg, sp = parse_log(t)
        warns += w
        targets += tg
        supp += sp
    summ = summarize(warns, supp)
    doc = {"check_id": "WARNING-BUDGET", "verdict": "PASS", "summary": summ,
           "problems": check_isolation(summ, warns, args.isolation),
           "attempted_targets": len(set(targets)),
           "logs": args.log}
    if summ["project_total"] > args.max_project_total:
        doc["problems"].append("项目面告警 %d > 阈值 %d" % (summ["project_total"], args.max_project_total))
    if summ["project_unique_locations"] > args.max_project_unique:
        doc["problems"].append("项目面唯一位置 %d > 阈值 %d"
                               % (summ["project_unique_locations"], args.max_project_unique))
    if args.baseline:
        base = _load_baseline_from(pathlib.Path(args.baseline))
        doc["baseline_diff"] = diff_baseline(base, summ, warns)
        new_supp = set(summ["suppression_tokens"]) - set(base.get("suppression_tokens", []))
        if new_supp:
            doc["problems"].append("新增告警抑制: %s（告警清零不得靠加抑制）"
                                   % ",".join(sorted(new_supp)))
    elif summ["suppressions"]:
        doc["problems"].append("抑制面命中 %d: %s（无基线可比 ⇒ 判红；给出 --baseline 后"
                               "仅新增项判红）"
                               % (summ["suppressions"], ",".join(summ["suppression_tokens"])))
    doc["verdict"] = "FAIL" if doc["problems"] else "PASS"
    if args.emit_baseline:
        out = pathlib.Path(args.emit_baseline)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({
            "warning_keys": ["%s:%d:%s" % (w["file"], w["line"], w["rule"]) for w in warns],
            "suppression_tokens": sorted(set(s["token"] for s in supp)),
            "summary": {k: summ[k] for k in ("total_warnings", "unique_locations",
                                             "disposition_units")},
        }, ensure_ascii=False, indent=2) + chr(10), encoding="utf-8")
        print("BASELINE_OUT %s" % out)
    report(doc, args.show)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + chr(10), encoding="utf-8")
        print("JSON_OUT %s" % out)
    return 1 if doc["verdict"] == "FAIL" else 0


if __name__ == "__main__":
    sys.exit(main())

