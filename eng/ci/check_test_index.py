#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_test_index.py — Python 测试聚合判据门 (RELEASE-02 CI-HYGIENE)。

缺陷背景 (DESIGN-CONFORMANCE SUB-D-35):
  eng/tests/test_index.csv 是 eng/tests/** Python 套件的聚合登记面。旧版只有自由文本
  notes, 无机器 verdict 列 ⇒ 含 errors/failures/skip 的套件与"通过"无法机器区分,
  聚合结论只能人工转述 ⇒ "Python 测试绿灯"不可信 (多套件含 errors/failures/skip
  仍被当作通过)。

本门把"聚合判据"机器化 (fail-closed, 逐行判据见 R1~R7):
  R1 列齐全: path,type,runner_command,requires_compiler,local_runnable,
     hosted_only,verdict,cases,failed,errored,skipped,notes;
  R2 path 唯一, 且 eng/tests/config 必须登记 (与 CFG002-07 同口径);
  R3 verdict ∈ 封闭词表 {PASS,FAIL,ENV_FAIL,SKIP_ONLY,HOSTED,HEAVY,SCRIPT};
  R4 unittest 套件: PASS ⇔ failed==0 ∧ errored==0; FAIL/ENV_FAIL ⇒ failed+errored>0;
     HOSTED/HEAVY/SCRIPT/SKIP_ONLY 用 NA 计数 (未本地执行, 不得伪装成 0);
  R5 全 skip 不得记 PASS: cases>0 ∧ skipped==cases ∧ failed==0 ∧ errored==0
     ⇒ 必须记 SKIP_ONLY (判红), 不得以 PASS 充数;
  R6 任何 errors/failures/skip 必须落 notes (不得静默);
  R7 SKIP_ONLY 行存在 ⇒ 判红 (未验证不得当通过)。

退出码:
  0 = 所有行 verdict 与计数自洽且无 SKIP_ONLY (聚合 verdict 仍会打印);
  1 = 任一判据违反, 或存在 SKIP_ONLY;
  2 = 输入缺失/CSV 无法解析。
  **本门只判"聚合标注是否诚实/自洽", 不重复判套件执行面** —— 套件执行由
  eng/ci/checks.json 的 CHK-UNIT UT-* 步骤负责; 两者互补: UT-* 判红执行, 本门
  保证聚合面不会把红标成绿。若聚合 verdict=FAIL/ENV_FAIL, 摘要会显式给出。

用法:
  python3 eng/ci/check_test_index.py
  python3 eng/ci/check_test_index.py --csv eng/tests/test_index.csv
  python3 eng/ci/check_test_index.py --self-test      # tempfile 夹具正/负例
只读 (--self-test 只写 tempfile); 仅 stdlib。
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DEFAULT_CSV = "eng/tests/test_index.csv"
REQUIRED_COLUMNS = ("path", "type", "runner_command", "requires_compiler",
                    "local_runnable", "hosted_only", "verdict", "cases",
                    "failed", "errored", "skipped", "notes")
VERDICTS = ("PASS", "FAIL", "ENV_FAIL", "SKIP_ONLY", "HOSTED", "HEAVY", "SCRIPT")
# 非绿 verdict: 聚合层不得视为通过 (但仍如实标注, 不是"判据违反")
NON_GREEN = ("FAIL", "ENV_FAIL", "SKIP_ONLY")
INT_FIELDS = ("cases", "failed", "errored", "skipped")


def _int_or_none(v):
    if v is None:
        return None
    s = str(v).strip()
    if s == "" or s.upper() == "NA":
        return None
    try:
        return int(s)
    except ValueError:
        return "INVALID"


def validate(csv_text: str) -> tuple:
    """返回 (problems[list[str]], rows[list[dict]], aggregate[str])。"""
    problems: list[str] = []
    if csv_text.startswith("\ufeff"):
        csv_text = csv_text[1:]
    reader = csv.DictReader(io.StringIO(csv_text))
    header = reader.fieldnames or []
    missing_cols = [c for c in REQUIRED_COLUMNS if c not in header]
    if missing_cols:
        return (["R1 缺列: %s" % missing_cols], [], "UNKNOWN")
    rows = list(reader)
    if not rows:
        return (["R1 CSV 无数据行"], [], "UNKNOWN")

    seen = set()
    worst_rank = -1
    rank = {"PASS": 0, "HOSTED": 1, "HEAVY": 1, "SCRIPT": 1,
            "ENV_FAIL": 2, "SKIP_ONLY": 3, "FAIL": 4}
    worst = "PASS"
    for n, row in enumerate(rows, 2):
        path = (row.get("path") or "").strip()
        if not path:
            problems.append("R2 行 %d: path 为空" % n)
            continue
        if path in seen:
            problems.append("R2 行 %d: path 重复登记 %s" % (n, path))
        seen.add(path)
        verdict = (row.get("verdict") or "").strip().upper()
        notes = (row.get("notes") or "").strip()
        typ = (row.get("type") or "").strip()
        if verdict not in VERDICTS:
            problems.append("R3 行 %d (%s): verdict=%r 不在词表 %s"
                            % (n, path, row.get("verdict"), list(VERDICTS)))
            continue
        if rank[verdict] > worst_rank:
            worst_rank = rank[verdict]
            worst = verdict
        vals = {f: _int_or_none(row.get(f)) for f in INT_FIELDS}
        for f, v in vals.items():
            if v == "INVALID":
                problems.append("R4 行 %d (%s): %s 非整数且非 NA: %r"
                                % (n, path, f, row.get(f)))
        cases, failed, errored, skipped = (vals["cases"], vals["failed"],
                                           vals["errored"], vals["skipped"])
        is_unittest = (typ == "unittest")
        if verdict in ("PASS", "FAIL", "ENV_FAIL"):
            if not is_unittest:
                problems.append("R4 行 %d (%s): type=%s 不得用 verdict=%s"
                                % (n, path, typ, verdict))
                continue
            if None in (cases, failed, errored, skipped):
                problems.append("R4 行 %d (%s): verdict=%s 必须给整数 "
                                "cases/failed/errored/skipped (未执行用 "
                                "HOSTED/HEAVY/SCRIPT)" % (n, path, verdict))
                continue
            if verdict == "PASS":
                if failed or errored:
                    problems.append("R4 行 %d (%s): verdict=PASS 但 "
                                    "failed=%d errored=%d —— 含 errors/failures "
                                    "不得记通过" % (n, path, failed, errored))
                if cases > 0 and skipped == cases:
                    problems.append("R5 行 %d (%s): verdict=PASS 但全部 %d 例 "
                                    "被 skip —— 必须记 SKIP_ONLY, 不得充数"
                                    % (n, path, cases))
            else:  # FAIL / ENV_FAIL
                if not (failed or errored):
                    problems.append("R4 行 %d (%s): verdict=%s 但 failed=0 "
                                    "errored=0 —— 判据不符" % (n, path, verdict))
        else:  # HOSTED/HEAVY/SCRIPT/SKIP_ONLY
            if verdict == "SKIP_ONLY":
                if None in (cases, skipped) or not (cases and skipped == cases):
                    problems.append("R6 行 %d (%s): SKIP_ONLY 必须给 "
                                    "cases==skipped>0" % (n, path))
            elif any(v not in (None,) for v in (cases, failed, errored, skipped)):
                problems.append("R4 行 %d (%s): verdict=%s 未本地执行, 计数应为 NA"
                                % (n, path, verdict))
        if (failed or errored or skipped) and not notes:
            problems.append("R6 行 %d (%s): 存在 errors/failures/skip 但 notes 为空 "
                            "—— 不得静默" % (n, path))
    if "eng/tests/config" not in seen:
        problems.append("R2: eng/tests/test_index.csv 未登记 eng/tests/config (CFG002-07 口径)")
    return problems, rows, worst


def run(csv_path: str) -> int:
    if not os.path.isfile(csv_path):
        print(json.dumps({"tool": "eng/ci/check_test_index.py", "csv": csv_path,
                          "verdict": "INPUT_MISSING", "exit_code": 2},
                         ensure_ascii=False, indent=2))
        return 2
    with open(csv_path, encoding="utf-8") as fh:
        text = fh.read()
    problems, rows, aggregate = validate(text)
    skip_only = [r.get("path") for r in rows
                 if (r.get("verdict") or "").strip().upper() == "SKIP_ONLY"]
    red = bool(problems) or bool(skip_only)
    out = {
        "tool": "eng/ci/check_test_index.py",
        "csv": csv_path,
        "row_count": len(rows),
        "aggregate_verdict": aggregate,
        "aggregate_is_green": aggregate == "PASS",
        "skip_only_rows": skip_only,
        "problems": problems,
        "exit_code": 1 if red else 0,
        "verdict": ("TEST_INDEX_RED" if red else "TEST_INDEX_CONSISTENT"),
        "rows": [{"path": r.get("path"), "verdict": r.get("verdict"),
                  "cases": r.get("cases"), "failed": r.get("failed"),
                  "errored": r.get("errored"), "skipped": r.get("skipped")}
                 for r in rows],
    }
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return out["exit_code"]


# ---- 自测 (tempfile 夹具; 不读不写本仓库) -----------------------------------
HDR = ("path,type,runner_command,requires_compiler,local_runnable,hosted_only,"
       "verdict,cases,failed,errored,skipped,notes\n")


def _row(path, typ, verdict, cases="NA", failed="NA", errored="NA",
         skipped="NA", notes="ok", comp="false", local="true", hosted="false"):
    return ('%s,%s,"python3 -m unittest",%s,%s,%s,%s,%s,%s,%s,%s,"%s"\n'
            % (path, typ, comp, local, hosted, verdict, cases, failed,
               errored, skipped, notes))


def _fixture(extra_rows="", drop_config=False):
    body = HDR
    if not drop_config:
        body += _row("eng/tests/config", "unittest", "PASS", 10, 0, 0, 0)
    body += extra_rows
    return body


def self_test() -> int:
    cases = []

    def add(name, text, want_rc, want_token=None):
        problems, rows, agg = validate(text)
        skip_only = any((r.get("verdict") or "").upper() == "SKIP_ONLY" for r in rows)
        rc = 1 if (problems or skip_only) else 0
        ok = (rc == want_rc)
        blob = " ".join(problems) + " " + agg
        if ok and want_token and want_token not in blob:
            ok = False
        cases.append((name, rc, want_rc, ok, blob[:160]))

    add("pos_consistent", _fixture(
        _row("eng/tests/api", "unittest", "FAIL", 39, 0, 2, 0, "errors=2")), 0)
    add("pos_hosted", _fixture(
        _row("eng/tests/unit", "ctest", "HOSTED", "NA", "NA", "NA", "NA", "hosted")), 0)
    add("neg_pass_with_failure", _fixture(
        _row("eng/tests/api", "unittest", "PASS", 39, 1, 0, 0, "x")), 1,
        "不得记通过")
    add("neg_fail_without_counts", _fixture(
        _row("eng/tests/api", "unittest", "FAIL", 39, 0, 0, 0, "x")), 1, "判据不符")
    add("neg_all_skip_masquerade", _fixture(
        _row("eng/tests/api", "unittest", "PASS", 5, 0, 0, 5, "x")), 1, "SKIP_ONLY")
    add("neg_unknown_verdict", _fixture(
        _row("eng/tests/api", "unittest", "GREEN", 5, 0, 0, 0, "x")), 1, "不在词表")
    add("neg_duplicate_path", _fixture(
        _row("eng/tests/config", "unittest", "PASS", 10, 0, 0, 0)), 1, "重复")
    add("neg_skip_only_row", _fixture(
        _row("eng/tests/api", "unittest", "SKIP_ONLY", 5, 0, 0, 5, "all skip")), 1,
        "SKIP_ONLY")
    add("neg_silent_skips", _fixture(
        _row("eng/tests/api", "unittest", "PASS", 5, 0, 0, 3, "")), 1, "不得静默")
    # 缺列
    problems, rows, agg = validate("path,type\ntests/config,unittest\n")
    cases.append(("neg_missing_columns", 1 if problems else 0, 1,
                  bool(problems), "缺列"))
    # 缺 eng/tests/config
    add("neg_missing_config_row", _fixture(
        _row("eng/tests/api", "unittest", "PASS", 5, 0, 0, 0), drop_config=True),
        1, "未登记 eng/tests/config")

    ok = True
    for name, rc, want, good, blob in cases:
        ok = ok and good
        print("[selftest] %-28s rc=%-2d want=%-2d %s"
              % (name, rc, want, "OK" if good else "MISMATCH"))
        if not good:
            print("[selftest]   %s" % blob)
    print("[selftest] %d cases, %s" % (len(cases), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="Python 测试聚合判据门")
    ap.add_argument("--csv", default=DEFAULT_CSV)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    path = args.csv if os.path.isabs(args.csv) else os.path.join(REPO, args.csv)
    return run(path)


if __name__ == "__main__":
    sys.exit(main())
