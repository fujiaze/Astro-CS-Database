#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""frozen_gate_exit_gate.py —— 冻结门处置面一致性门（能红能绿）。

裁决对象 = eng/tools/acceptance/frozen_gate_inventory.json 这份盘点表本身。
它回答三个问题，且每条都能红：
  FG-01 每条冻结门是否**真的在跑**（CI 登记项 / 已注册 ctest 目标必须真实存在）
  FG-02 触发时**谁被通知**（通知面不得为空、不得为 none 而无登记面）
  FG-03 有没有**出口**（exit_kind=none 的门必须带处置方案 remediation）
  FG-04 声称在跑但只有 self-test / replay 登记的门，必须显式标 live_adjudication=false
  FG-05 CI 登记项的档位与可豁免性必须登记（档位决定"这一面在哪些档位被执行"）
  FG-06 「触发即中止」的门必须有制度化登记面（登记面为 none ⇒ 判红）

用法
----
  python3 eng/tools/acceptance/frozen_gate_exit_gate.py
  python3 eng/tools/acceptance/frozen_gate_exit_gate.py --self-test
  python3 eng/tools/acceptance/frozen_gate_exit_gate.py --json-out <out.json>

退出码: 0 判绿 / 1 判红 / 2 fail-closed（盘点表不可读或结构非法）
本文件不含任何科学阈值：阈值与冻结定义一字不动，只裁决"门有没有登记面与出口"。
"""
from __future__ import annotations

import argparse
import copy
import json
import pathlib
import re
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[2]
INVENTORY = HERE / "frozen_gate_inventory.json"
CHECKS = REPO / "eng/ci/checks.json"
EXIT_PASS, EXIT_RED, EXIT_FAILCLOSED = 0, 1, 2

EXIT_KINDS = {"stop_work_with_registration", "record_and_justify",
              "degrade_to_diagnostic", "waive", "none"}
NOTIFY_SURFACES = {"product_log", "ci_step", "run_artifact", "none"}
REG_SURFACES = {"tier_verdict_record", "run_manifest", "ci_artifact", "none"}
SELF_TEST_ONLY = ("SELFTEST", "REPLAY")


def _rule(fid, ok, detail):
    return {"rule": fid, "ok": bool(ok), "detail": detail}


def _load_ci():
    d = json.loads(CHECKS.read_text(encoding="utf-8"))
    ids, meta = set(), {}
    def walk(items):
        for s in items:
            ids.add(s.get("id"))
            meta[s.get("id")] = {
                "profiles": s.get("profiles") or [],
                "waivable": bool(s.get("waivable")),
                "platform": s.get("platform"),
                "steps": [st.get("id") for st in (s.get("steps") or [])],
            }
            walk(s.get("steps") or [])
    walk(d.get("checks") or [])
    return ids, meta


def _load_ctest(build_dir="build"):
    f = pathlib.Path(build_dir) / "CTestTestfile.cmake"
    if not f.is_file():
        return None
    try:
        out = subprocess.run(["ctest", "--test-dir", str(build_dir), "-N"],
                             capture_output=True, text=True, timeout=300).stdout
    except Exception:
        return None
    return set(re.findall(r"Test\s+#\d+:\s+(\S+)", out))


def adjudicate(inv, ci_ids, ci_meta, ctest_names, build_dir="build"):
    findings = []
    gates = inv.get("gates")
    if not isinstance(gates, list) or not gates:
        return {"verdict": "red", "findings": [
            _rule("FG-00", False, "盘点表无 gates 数组（门不可判）")], "n_red": 1}

    # FG-01 是否真的在跑
    bad, declared = [], []
    for g in gates:
        gid = g.get("gate_id")
        if g.get("runs_in_ci") is True:
            ev = g.get("ci_or_test_evidence") or []
            if not ev:
                bad.append("%s: 声称在 CI 跑但无任何 CI/测试证据 ID" % gid)
            for e in ev:
                if e in ci_ids:
                    continue
                if ctest_names and e in ctest_names:
                    continue
                bad.append("%s: 证据 ID %s 既不在 eng/ci/checks.json 也不在已注册 ctest 目标中"
                           % (gid, e))
        if g.get("runs_in_product") is False and g.get("runs_in_ci") is False \
                and g.get("live_adjudication") is False:
            # 三面全 false 只能以「已声明的缺口」存在：必须 open_gap=true 且带处置方案，
            # 否则就是「没人跑也没人认领」的静默冻结门 —— 判红。
            if g.get("open_gap") is not True:
                bad.append("%s: 三个在跑面全 false 却未标 open_gap=true（无人跑也无人认领）" % gid)
            elif not (g.get("remediation") or "").strip():
                bad.append("%s: 已声明缺口但无 remediation（缺口没有出口）" % gid)
            else:
                declared.append(gid)
    findings.append(_rule("FG-01", not bad, "; ".join(bad) if bad
                          else "声称在跑的门其 CI/测试证据全部可解析（CI %d 项）；"
                               "已声明的未运行缺口 %d 条：%s"
                               % (len(ci_ids), len(declared), ",".join(declared) or "无")))

    # FG-02 通知面
    bad = []
    for g in gates:
        gid = g.get("gate_id")
        ns = g.get("notification_surface")
        if ns not in NOTIFY_SURFACES:
            bad.append("%s: notification_surface 取值域外 %r" % (gid, ns))
        elif ns == "none" and not g.get("notified_detail"):
            bad.append("%s: 通知面为 none 却没有说明" % gid)
        elif ns != "none" and not g.get("notified_detail"):
            bad.append("%s: 通知面非 none 但未写明谁被通知（notified_detail 空）" % gid)
    findings.append(_rule("FG-02", not bad, "; ".join(bad) if bad
                          else "每条门的通知面已具名（product_log / ci_step / run_artifact）"))

    # FG-03 出口：无出口必须给处置方案
    bad = []
    for g in gates:
        gid = g.get("gate_id")
        ek = g.get("exit_kind")
        if ek not in EXIT_KINDS:
            bad.append("%s: exit_kind 取值域外 %r" % (gid, ek))
            continue
        if ek == "none" and not (g.get("remediation") or "").strip():
            bad.append("%s: 触发后无出口（exit_kind=none）且无 remediation（永久卡死）" % gid)
        if not (g.get("exit_detail") or "").strip():
            bad.append("%s: 缺 exit_detail（出口形态必须写清是停工还是登记）" % gid)
    n_no_exit = sum(1 for g in gates if g.get("exit_kind") == "none")
    findings.append(_rule("FG-03", not bad, "; ".join(bad) if bad
                          else "无出口的门 %d 条，全部带处置方案" % n_no_exit))

    # FG-04 只有 self-test / replay 登记的门必须显式标 live_adjudication=false
    bad = []
    for g in gates:
        gid = g.get("gate_id")
        ev = g.get("ci_or_test_evidence") or []
        if not ev or g.get("runs_in_ci") is not True:
            continue
        only_st = all(any(tag in e.upper() for tag in SELF_TEST_ONLY) for e in ev)
        if only_st and g.get("live_adjudication") is not False:
            bad.append("%s: 只有 self-test/replay 登记却未标 live_adjudication=false" % gid)
        if (not only_st) and g.get("live_adjudication") is False and g.get("remediation") is None:
            bad.append("%s: live_adjudication=false 但无 remediation" % gid)
    findings.append(_rule("FG-04", not bad, "; ".join(bad) if bad
                          else "「只有自证/回放、无当轮裁决」的门已逐条显式标注"))

    # FG-05 CI 档位与可豁免性必须登记（决定这一面在哪些档位被执行）
    bad = []
    for g in gates:
        gid = g.get("gate_id")
        for e in (g.get("ci_or_test_evidence") or []):
            if e not in ci_meta:
                continue
            m = ci_meta[e]
            if not m.get("profiles"):
                bad.append("%s/%s: 无档位归属（读不出这一面在哪些档位被执行）" % (gid, e))
            if m.get("waivable") and not (g.get("remediation") or "").strip():
                bad.append("%s/%s: 可豁免（waivable）却无处置方案" % (gid, e))
    findings.append(_rule("FG-05", not bad, "; ".join(bad) if bad
                          else "CI 登记项的档位归属与可豁免性已登记"))

    # FG-06 触发即中止的门必须有制度化登记面
    bad = []
    for g in gates:
        gid = g.get("gate_id")
        if g.get("exit_kind") != "stop_work_with_registration":
            continue
        rs = g.get("registration_surface")
        if rs not in REG_SURFACES:
            bad.append("%s: registration_surface 取值域外 %r" % (gid, rs))
        elif rs == "none":
            bad.append("%s: 触发即中止（停工）却无登记面 => 永久卡死且无出口" % gid)
    findings.append(_rule("FG-06", not bad, "; ".join(bad) if bad
                          else "停工型门全部带制度化登记面"))

    red = [f for f in findings if not f["ok"]]
    return {"verdict": "pass" if not red else "red", "findings": findings,
            "n_red": len(red), "n_gates": len(gates),
            "n_no_exit": n_no_exit, "declared_gaps": declared,
            "n_declared_gaps": len(declared)}


def _tiny_inventory(gate_overrides=None):
    base = {
        "schema": "acsd.frozen_gate_inventory.v1",
        "gates": [{
            "gate_id": "G-TEST", "title": "t", "family": "science_frozen_threshold",
            "scope": "s", "threshold_source": "docs/engineering/NUMERIC_STANDARD.md", "implementation": "lib/y.cpp",
            "runs_in_product": True, "runs_in_ci": True,
            "ci_or_test_evidence": ["CHK-UNIT"],
            "live_adjudication": True, "notification_surface": "ci_step",
            "notified_detail": "CI 步骤级红/绿", "exit_kind": "record_and_justify",
            "registration_surface": "ci_artifact", "exit_detail": "记录/裁决分离",
            "known_defect": None, "remediation": None,
        }],
    }
    for ov in (gate_overrides or []):
        base["gates"][0].update(ov)
    return base


NEG_CASES = [
    ("N1 声称在跑但证据 ID 解析不到",
     [{"ci_or_test_evidence": ["NO-SUCH-CHECK-ID"]}], "FG-01"),
    ("N2 三面全 false（既不在产品也不在 CI 也不实测）",
     [{"runs_in_product": False, "runs_in_ci": False, "live_adjudication": False}],
     "FG-01"),
    ("N3 通知面为 none 却不说明",
     [{"notification_surface": "none", "notified_detail": ""}], "FG-02"),
    ("N4 无出口且无处置方案（永久卡死）",
     [{"exit_kind": "none", "remediation": None}], "FG-03"),
    ("N5 只有 self-test 登记却宣称实测裁决",
     [{"ci_or_test_evidence": ["L2-FROZEN-GATE-SELFTEST"], "live_adjudication": True}],
     "FG-04"),
    ("N6 停工型门无登记面",
     [{"exit_kind": "stop_work_with_registration", "registration_surface": "none"}], "FG-06"),
]


def self_test(ci_ids, ci_meta, ctest_names, build_dir):
    lines = []
    good = _tiny_inventory()
    r = adjudicate(good, ci_ids, ci_meta, ctest_names, build_dir)
    ok = r["verdict"] == "pass"
    lines.append("[%s] S0 正例 => %s" % ("PASS" if ok else "FAIL", r["verdict"]))
    if not ok:
        for f in r["findings"]:
            if not f["ok"]:
                lines.append("        %s: %s" % (f["rule"], f["detail"]))
    n_ok = 0
    for name, ov, expect in NEG_CASES:
        inv = _tiny_inventory([copy.deepcopy(o) for o in ov])
        r2 = adjudicate(inv, ci_ids, ci_meta, ctest_names, build_dir)
        hit = [f["rule"] for f in r2["findings"] if not f["ok"]]
        good_case = r2["verdict"] == "red" and expect in hit
        n_ok += int(good_case)
        lines.append("[%s] %s => %s rules=%s 期望含 %s"
                     % ("PASS" if good_case else "FAIL", name, r2["verdict"], hit, expect))
    passed = ok and n_ok == len(NEG_CASES)
    lines.append("FG_SELFTEST_%s: 正例=%s 注入负例=%d/%d"
                 % ("PASS" if passed else "FAIL", "1/1" if ok else "0/1", n_ok, len(NEG_CASES)))
    print("\n".join(lines))
    return EXIT_PASS if passed else EXIT_RED


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="冻结门处置面一致性门（FG-01..FG-06）")
    ap.add_argument("--inventory", default=str(INVENTORY))
    ap.add_argument("--build-dir", default="build")
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    try:
        inv = json.loads(pathlib.Path(a.inventory).read_text(encoding="utf-8"))
    except Exception as exc:
        print("FG_FAILCLOSED: 盘点表不可读/不可解析: %s" % exc, file=sys.stderr)
        return EXIT_FAILCLOSED
    if not isinstance(inv, dict) or not inv.get("schema"):
        print("FG_FAILCLOSED: 盘点表缺 schema（门不可判）", file=sys.stderr)
        return EXIT_FAILCLOSED
    ci_ids, ci_meta = _load_ci()
    ctest_names = _load_ctest(a.build_dir)
    if ctest_names is None:
        print("FG_FAILCLOSED: 读不到已注册 ctest 目标（build/%s 或 ctest 不可用）" % a.build_dir,
              file=sys.stderr)
        return EXIT_FAILCLOSED
    if a.self_test:
        return self_test(ci_ids, ci_meta, ctest_names, a.build_dir)
    res = adjudicate(inv, ci_ids, ci_meta, ctest_names, a.build_dir)
    for f in res["findings"]:
        print("  [%s] %s: %s" % ("ok " if f["ok"] else "RED", f["rule"], f["detail"]))
    print("FROZEN_GATE_EXIT_%s: gates=%d 无出口=%d 已声明未运行缺口=%d red_rules=%d"
          % (res["verdict"].upper(), res["n_gates"], res["n_no_exit"],
             res.get("n_declared_gaps", 0), res["n_red"]))
    for gid in res.get("declared_gaps") or []:
        print("  GAP: %s 冻结门当前不在任何在跑面（已登记 + 有处置方案）" % gid)
    if a.json_out:
        pathlib.Path(a.json_out).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(a.json_out).write_text(
            json.dumps(res, ensure_ascii=False, indent=2), encoding="utf-8")
    return EXIT_PASS if res["verdict"] == "pass" else EXIT_RED


if __name__ == "__main__":
    raise SystemExit(main())
