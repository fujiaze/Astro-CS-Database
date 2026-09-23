#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""TEST-INDEX-LIVE | test_index.csv 登记项 vs **实测** 一致性门（审计 TEST-INDEX-LIVE-001）。

为什么另立本门（根因）
  `eng/ci/check_test_index.py`（PY-TEST-INDEX-VERDICTS，已注册）只判「聚合标注是否
  **诚实/自洽**」（PASS ⇔ failed==0∧errored==0、全 skip 不得记 PASS、计数非 NA 等），
  它**不判登记项与实测是否一致**：CSV 靠手维护（无生成器），`measured_utc` 一旦
  过期，聚合面就会长期停留在旧快照上（实证：eng/tests/config 记 PASS/72/0/0 而实测
  failures=3；eng/tests/quality 记 HEAVY/NA 而实测 153 用例 149s OK；
  eng/tests/version 记 PASS/31 而实测 36 用例 failures=2）。
  本门把「登记 = 实测」机器化，与上述聚合自洽门**互补**，不重复其判据。

判据（任一 W 违规 ⇒ exit 1）
  W0 fail-closed   CSV 缺必需列 / 无可执行行 / 套件超时 / unittest 汇总解析不到
                   ⇒ 判红（禁止把「跑不起来」当「无违规」）；`--max-wall-seconds`
                   预算耗尽仍有未覆盖行 ⇒ 判红（未覆盖不得静默）。
  W1 verdict      实测 (failed+errored)==0 ⇔ 登记 verdict==PASS；>0 ⇔ FAIL/ENV_FAIL；
                   登记 HOSTED/HEAVY/SCRIPT/SKIP_ONLY 的行**不实跑**（其语义是「未本地
                   执行」），但若其计数列非 NA ⇒ 判红（与聚合门 R4 同口径，防伪装）。
  W2 counts       cases/failed/errored/skipped 必须与实测**逐项相等**。
  W3 wall_magnitude 登记 `measured_wall_s` 与实测墙钟**量级一致**：max/min > 3.0
                   且绝对差 > 2.0 s ⇒ 判红（秒级套件的噪声不判红；量级漂移判红）。

用法
  python3 eng/tools/doccheck/check_test_index_live.py                # 实跑全部 local_runnable 行
  python3 eng/tools/doccheck/check_test_index_live.py --suites config,api
  python3 eng/tools/doccheck/check_test_index_live.py --dry-run
  python3 eng/tools/doccheck/check_test_index_live.py --self-test    # tempfile 夹具正/负例
exit 0 = PASS；1 = 判据违规；2 = 输入不可用（fail-closed）。

只读仓库（--self-test 只写 tempfile）；仅 stdlib；输出稳定排序（无时间戳）。
"""
from __future__ import annotations

import argparse
import csv
import io
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_CSV = "eng/tests/test_index.csv"
REQUIRED_COLUMNS = ("path", "type", "verdict", "cases", "failed", "errored",
                    "skipped", "local_runnable", "measured_wall_s")
RUNNABLE_VERDICTS = ("PASS", "FAIL", "ENV_FAIL")
NON_RUN_VERDICTS = ("HOSTED", "HEAVY", "SCRIPT", "SKIP_ONLY")
RAN_RE = re.compile(r"Ran (\d+) tests? in ([0-9.]+)s")
FAILED_RE = re.compile(r"FAILED \(([^)]*)\)")
OK_RE = re.compile(r"^OK(?: \(([^)]*)\))?\s*$", re.M)


def _int_or_none(v):
    s = ("" if v is None else str(v)).strip()
    if s == "" or s.upper() == "NA":
        return None
    try:
        return int(s)
    except ValueError:
        return "INVALID"


def _float_or_none(v):
    s = ("" if v is None else str(v)).strip()
    if s == "" or s.upper() == "NA":
        return None
    try:
        return float(s)
    except ValueError:
        return "INVALID"


def parse_unittest_output(text):
    """从 unittest 输出解析 (cases, failed, errored, skipped, wall_s)；解析不到返回 None。"""
    m = RAN_RE.search(text)
    if not m:
        return None
    cases, wall = int(m.group(1)), float(m.group(2))
    failed = errored = skipped = 0
    fm = FAILED_RE.search(text)
    if fm:
        body = fm.group(1)
        for key, pat in (("failures", r"failures=(\d+)"), ("errors", r"errors=(\d+)"),
                         ("skipped", r"skipped=(\d+)")):
            km = re.search(pat, body)
            if km:
                if key == "failures":
                    failed = int(km.group(1))
                elif key == "errors":
                    errored = int(km.group(1))
                else:
                    skipped = int(km.group(1))
    else:
        om = OK_RE.search(text)
        if om and om.group(1):
            km = re.search(r"skipped=(\d+)", om.group(1))
            if km:
                skipped = int(km.group(1))
    return cases, failed, errored, skipped, wall


def run_suite(repo, rel, timeout_s):
    cmd = [sys.executable, "-B", "-m", "unittest", "discover", "-s", rel, "-t", rel]
    t0 = time.monotonic()
    try:
        proc = subprocess.run(cmd, cwd=repo, capture_output=True, text=True,
                              timeout=timeout_s)
    except subprocess.TimeoutExpired:
        return None, "TIMEOUT(>%ds)" % timeout_s, time.monotonic() - t0
    wall = time.monotonic() - t0
    parsed = parse_unittest_output(proc.stdout + "\n" + proc.stderr)
    if parsed is None:
        return None, "UNPARSED(rc=%d)" % proc.returncode, wall
    return parsed, None, wall


def check_rows(repo, rows, suites=None, exclude=None, timeout_s=600, budget_s=1800.0,
               dry_run=False):
    """返回 (problems[list[str]], measured[list[dict]], excluded[list[str]])。

    exclude 是**显式**排除面（如写副作用的套件）：被排除行不实跑，但会在结果里
    逐行点名（不静默跳过）。"""
    problems: list[str] = []
    measured: list[dict] = []
    excluded: list[str] = []
    spent = 0.0
    for row in rows:
        path = (row.get("path") or "").strip()
        verdict = (row.get("verdict") or "").strip().upper()
        typ = (row.get("type") or "").strip()
        local = (row.get("local_runnable") or "").strip().lower() == "true"
        counts = {f: _int_or_none(row.get(f)) for f in ("cases", "failed", "errored", "skipped")}
        wall_reg = _float_or_none(row.get("measured_wall_s"))
        for f, v in counts.items():
            if v == "INVALID":
                problems.append("W0 %s: %s 非整数且非 NA: %r" % (path, f, row.get(f)))
        if verdict in NON_RUN_VERDICTS:
            if any(v is not None for v in counts.values()):
                problems.append("W1 %s: verdict=%s 未本地执行, 计数必须为 NA" % (path, verdict))
            continue
        if verdict not in RUNNABLE_VERDICTS:
            problems.append("W1 %s: verdict=%r 不在可跑词表 %s"
                            % (path, row.get("verdict"), list(RUNNABLE_VERDICTS)))
            continue
        if not local or typ != "unittest":
            problems.append("W1 %s: verdict=%s 但 local_runnable=%s/type=%s —— 不可跑却记可跑结论"
                            % (path, verdict, row.get("local_runnable"), typ))
            continue
        if suites and path.rsplit("/", 1)[-1] not in suites:
            continue
        if exclude and path.rsplit("/", 1)[-1] in exclude:
            excluded.append("%s（显式 --exclude-suites：%s）" % (path, verdict))
            continue
        if dry_run:
            measured.append({"path": path, "dry_run": True})
            continue
        if spent > budget_s:
            problems.append("W0 %s: --max-wall-seconds 预算 %.0fs 已耗尽, 未覆盖"
                            % (path, budget_s))
            continue
        parsed, err, wall = run_suite(repo, path, timeout_s)
        spent += wall
        if parsed is None:
            problems.append("W0 %s: %s" % (path, err))
            continue
        cases, failed, errored, skipped, inner = parsed
        rec = {"path": path, "verdict": verdict, "cases": cases, "failed": failed,
               "errored": errored, "skipped": skipped, "wall_s": round(wall, 2),
               "wall_registered_s": wall_reg}
        measured.append(rec)
        green = (failed == 0 and errored == 0)
        if verdict == "PASS" and not green:
            problems.append("W1 %s: 登记 PASS 但实测 failed=%d errored=%d"
                            % (path, failed, errored))
        if verdict in ("FAIL", "ENV_FAIL") and green:
            problems.append("W1 %s: 登记 %s 但实测全绿（登记过期）" % (path, verdict))
        for f, got in (("cases", cases), ("failed", failed), ("errored", errored),
                       ("skipped", skipped)):
            want = counts[f]
            if want is None:
                problems.append("W2 %s: %s 登记为 NA 但实测 %d（可跑行不得记 NA）"
                                % (path, f, got))
            elif want != got:
                problems.append("W2 %s: %s 登记 %d / 实测 %d" % (path, f, want, got))
        if wall_reg is None:
            problems.append("W2 %s: measured_wall_s 缺失（可跑行必须登记实测墙钟）" % path)
        elif wall_reg != "INVALID":
            lo, hi = min(wall, wall_reg), max(wall, wall_reg)
            if lo > 0 and hi / lo > 3.0 and (hi - lo) > 2.0:
                problems.append("W3 %s: 耗时量级漂移 登记 %.2fs / 实测 %.2fs"
                                % (path, wall_reg, wall))
    return problems, measured, excluded


def load_rows(csv_path):
    with open(csv_path, encoding="utf-8") as fh:
        text = fh.read()
    if text.startswith("\ufeff"):
        text = text[1:]
    reader = csv.DictReader(io.StringIO(text))
    header = reader.fieldnames or []
    missing = [c for c in REQUIRED_COLUMNS if c not in header]
    return list(reader), missing


def run(csv_path, suites, exclude, timeout_s, budget_s, dry_run, json_out):
    if not os.path.isfile(csv_path):
        print(json.dumps({"tool": "check_test_index_live", "verdict": "INPUT_MISSING",
                          "csv": csv_path, "exit_code": 2}, ensure_ascii=False, indent=2))
        return 2
    rows, missing = load_rows(csv_path)
    if missing:
        print(json.dumps({"tool": "check_test_index_live", "verdict": "CSV_MISSING_COLUMNS",
                          "missing": missing, "exit_code": 2}, ensure_ascii=False, indent=2))
        return 2
    runnable = [r for r in rows
                if (r.get("verdict") or "").strip().upper() in RUNNABLE_VERDICTS]
    if not runnable:
        print(json.dumps({"tool": "check_test_index_live", "verdict": "NO_RUNNABLE_ROWS",
                          "exit_code": 2}, ensure_ascii=False, indent=2))
        return 2
    problems, measured, excluded = check_rows(REPO, rows, suites, exclude, timeout_s,
                                              budget_s, dry_run)
    out = {"tool": "eng/tools/doccheck/check_test_index_live.py",
           "csv": csv_path, "runnable_rows": len(runnable),
           "excluded_rows": excluded,
           "measured_rows": len([m for m in measured if not m.get("dry_run")]),
           "dry_run": bool(dry_run),
           "problems": problems,
           "exit_code": 1 if problems else 0,
           "verdict": "TEST_INDEX_LIVE_RED" if problems else "TEST_INDEX_LIVE_CONSISTENT",
           "measured": measured}
    if json_out:
        parent = os.path.dirname(os.path.abspath(json_out))
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(json_out, "w", encoding="utf-8") as fh:
            json.dump(out, fh, ensure_ascii=False, indent=1, sort_keys=True)
            fh.write("\n")
    for e in excluded:
        print("[excluded] %s" % e)
    if problems:
        print("TEST_INDEX_LIVE_RED: %d 条" % len(problems))
        for p in problems:
            print("  " + p)
        return 1
    print("TEST_INDEX_LIVE_CONSISTENT: %d 可跑行全部与实测一致（显式排除 %d 行）"
          % (out["measured_rows"], len(excluded)))
    return 0


# ---------------------------------------------------------------- self-test
_FIXTURE_PASS = "import unittest\n\n\nclass T(unittest.TestCase):\n    def test_ok(self):\n        self.assertTrue(True)\n"
_FIXTURE_FAIL = "import unittest\n\n\nclass T(unittest.TestCase):\n    def test_bad(self):\n        self.assertEqual(1, 2)\n"
_HDR = ("path,type,runner_command,requires_compiler,local_runnable,hosted_only,"
        "verdict,cases,failed,errored,skipped,measured_utc,measured_wall_s,notes\n")


def _fixture_row(path, verdict, cases="NA", failed="NA", errored="NA", skipped="NA",
                 wall="NA", local="true", typ="unittest"):
    return ('%s,%s,"python3 -m unittest",false,%s,false,%s,%s,%s,%s,%s,2026-01-01,%s,"x"\n'
            % (path, typ, local, verdict, cases, failed, errored, skipped, wall))


def self_test():
    checks = []
    root = tempfile.mkdtemp(prefix="test_index_live_selftest_")
    try:
        for name, body in (("t_pass", _FIXTURE_PASS), ("t_fail", _FIXTURE_FAIL)):
            d = os.path.join(root, name)
            os.makedirs(d, exist_ok=True)
            with open(os.path.join(d, "test_x.py"), "w", encoding="utf-8") as fh:
                fh.write(body)
        # 实测基线（供正例使用）：实跑一次拿真实计数与墙钟
        p_ok, err_ok, w_ok = run_suite(root, "t_pass", 120)
        p_bad, err_bad, w_bad = run_suite(root, "t_fail", 120)
        if p_ok is None or p_bad is None:
            print("SELFTEST FAILED: 夹具套件不可执行 (%s/%s)" % (err_ok, err_bad))
            return 1

        def build(rows):
            return list(csv.DictReader(io.StringIO(_HDR + "".join(rows))))

        def expect(name, rows, want_rc, token=None):
            problems, _, _ = check_rows(root, build(rows), timeout_s=120, budget_s=600)
            rc = 1 if problems else 0
            blob = " ".join(problems)
            ok = (rc == want_rc) and (token is None or token in blob)
            checks.append((name, rc, want_rc, ok, blob[:150]))

        good_pass = _fixture_row("t_pass", "PASS", p_ok[0], 0, 0, p_ok[3], "%.2f" % w_ok)
        good_fail = _fixture_row("t_fail", "FAIL", p_bad[0], p_bad[1], p_bad[2], p_bad[3],
                                 "%.2f" % w_bad)
        expect("pos_consistent", [good_pass, good_fail], 0)
        expect("neg_pass_but_red", [_fixture_row("t_fail", "PASS", 1, 0, 0, 0, "0.1")],
               1, "登记 PASS 但实测")
        expect("neg_fail_but_green", [_fixture_row("t_pass", "FAIL", 1, 1, 0, 0, "0.1")],
               1, "登记 FAIL 但实测全绿")
        expect("neg_count_drift", [_fixture_row("t_pass", "PASS", 7, 0, 0, 0, "%.2f" % w_ok)],
               1, "cases 登记 7 / 实测")
        expect("neg_wall_magnitude", [_fixture_row("t_pass", "PASS", p_ok[0], 0, 0, 0, "600")],
               1, "耗时量级漂移")
        expect("neg_hosted_with_counts",
               [_fixture_row("t_pass", "HOSTED", 5, 0, 0, 0, "NA", local="false")],
               1, "未本地执行, 计数必须为 NA")
        expect("neg_runnable_but_not_local",
               [_fixture_row("t_pass", "PASS", 1, 0, 0, 0, "0.1", local="false")],
               1, "不可跑却记可跑结论")
        expect("neg_missing_wall", [_fixture_row("t_pass", "PASS", p_ok[0], 0, 0, 0, "NA")],
               1, "measured_wall_s 缺失")
        expect("neg_unrunnable_target",
               [_fixture_row("t_no_such_dir", "PASS", 1, 0, 0, 0, "0.1")],
               1, "W0")
        # W0：无可跑行
        problems, _, _ = check_rows(root, build([_fixture_row("t_pass", "HOSTED", local="false")]),
                                    timeout_s=120)
        checks.append(("pos_all_nonrun_rows", 1 if problems else 0, 0, not problems, "跳过"))
    finally:
        shutil.rmtree(root, ignore_errors=True)
    ok = True
    for name, rc, want, good, blob in checks:
        ok = ok and good
        print("[selftest] %-30s rc=%-2d want=%-2d %s"
              % (name, rc, want, "OK" if good else "MISMATCH"))
        if not good:
            print("[selftest]   %s" % blob)
    print("[selftest] %d cases, %s" % (len(checks), "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="test_index.csv 登记 vs 实测一致性门")
    ap.add_argument("--csv", default=DEFAULT_CSV)
    ap.add_argument("--suites", default=None, help="逗号分隔的套件目录名白名单")
    ap.add_argument("--exclude-suites", default=None,
                    help="显式排除的套件目录名（如写副作用套件）；排除行会逐行点名")
    ap.add_argument("--timeout-seconds", type=int, default=600)
    ap.add_argument("--max-wall-seconds", type=float, default=1800.0)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    suites = set(s.strip() for s in args.suites.split(",") if s.strip()) if args.suites else None
    exclude = set(s.strip() for s in args.exclude_suites.split(",") if s.strip()) \
        if args.exclude_suites else None
    csv_path = args.csv if os.path.isabs(args.csv) else os.path.join(REPO, args.csv)
    return run(csv_path, suites, exclude, args.timeout_seconds, args.max_wall_seconds,
               args.dry_run, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
