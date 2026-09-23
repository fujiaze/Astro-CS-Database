#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_test_index.py — Python 测试聚合判据门 (RELEASE-02 CI-HYGIENE)。

缺陷背景 (DESIGN-CONFORMANCE SUB-D-35):
  eng/tests/test_index.csv 是 eng/tests/** Python 套件的聚合登记面。旧版只有自由文本
  notes, 无机器 verdict 列 ⇒ 含 errors/failures/skip 的套件与"通过"无法机器区分,
  聚合结论只能人工转述 ⇒ "Python 测试绿灯"不可信 (多套件含 errors/failures/skip
  仍被当作通过)。

本门把"聚合判据"机器化 (fail-closed, 逐行判据见 R1~R8):
  R1 列齐全: path,type,runner_command,requires_compiler,local_runnable,
     hosted_only,verdict,cases,failed,errored,skipped,notes;
  R2 path 唯一, 且 eng/tests/config 必须登记 (与 CFG002-07 同口径);
  R3 verdict ∈ 封闭词表 {PASS,FAIL,ENV_FAIL,SKIP_ONLY,HOSTED,HEAVY,SCRIPT};
  R4 unittest 套件: PASS ⇔ failed==0 ∧ errored==0; FAIL/ENV_FAIL ⇒ failed+errored>0;
     HOSTED/HEAVY/SCRIPT/SKIP_ONLY 用 NA 计数 (未本地执行, 不得伪装成 0);
  R5 全 skip 不得记 PASS: cases>0 ∧ skipped==cases ∧ failed==0 ∧ errored==0
     ⇒ 必须记 SKIP_ONLY (判红), 不得以 PASS 充数;
  R6 任何 errors/failures/skip 必须落 notes (不得静默);
  R7 SKIP_ONLY 行存在 ⇒ 判红 (未验证不得当通过);
  R8 缺口登记册棘轮 (只减不增): eng/packaging/config/config_registry.json 的
     index_ownership.test_index_known_unregistered 每条必须**仍有对象**
     —— 目录存在, 且仍未登记进 test_index.csv; 失效项 ⇒ 判红并要求删除登记。
     依据 ENGINEERING_SPEC §10 / docs/ci/01_CHECKS.md §1「锚存活」「fail-closed」;
     与 run_manifest_schema_deviations.json、traceability_warn_baseline.json 同款。
     登记册缺失/不可解析/为空 ⇒ 判红 (fail-closed, 不得把"解析不到"当"无违规")。

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
# 缺口登记册（test_index_known_unregistered 的宿主）
DEFAULT_REGISTRY = "eng/packaging/config/config_registry.json"
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


def check_known_unregistered(repo: str, rows, registry_path: str = None) -> list[str]:
    """R8（棘轮，只减不增）：缺口登记册的每个登记项必须**仍有对象**。

    规范依据：ENGINEERING_SPEC §10「检查器在输入缺失、路径不存在…时判红」、
    docs/ci/01_CHECKS.md §1「锚存活」「fail-closed」。

    为什么需要：eng/packaging/config/config_registry.json 的
    index_ownership.test_index_known_unregistered 是"已知未登记缺口"的台账。
    其消费方 eng/tests/config/check_cfg002_registry.py 只闭合
    「实际缺口 ⊆ 清单」一个方向；另一个方向（清单里的条目是否还是缺口）它只
    计算 stale 并打印，明写"已登记项残留只提示，不判红" ⇒ 死登记项可以永久
    静默累积（实测：eng/tests/gaia_zlib 目录在真仓库根本不存在）。
    本判据补上「清单 ⊆ 实际缺口」方向，两侧闭合；与
    run_manifest_schema_deviations.json / traceability_warn_baseline.json 的
    棘轮同款：条目失效 ⇒ 判红并要求删除登记，不得当只增不减的豁免表。

    只判"登记项是否仍有对象"（目录存在 + 仍未登记），**不**重复枚举实际缺口集合
    —— 那一侧的口径唯一源是 CFG-002，避免第二套枚举语义（ENGINEERING_SPEC §10）。
    """
    problems: list[str] = []
    reg_file = registry_path or os.path.join(repo, DEFAULT_REGISTRY)
    if not os.path.isfile(reg_file):
        return ["R8 缺口登记册缺失（fail-closed）: %s" % reg_file]
    try:
        with open(reg_file, encoding="utf-8") as fh:
            doc = json.load(fh)
        known = doc["index_ownership"]["test_index_known_unregistered"]
    except (OSError, ValueError, KeyError, TypeError) as exc:
        return ["R8 缺口登记册不可解析（fail-closed）: %s: %s" % (reg_file, exc)]
    if not isinstance(known, list) or not known:
        return ["R8 缺口登记册为空或非数组（fail-closed，不得把「解析不到」当「无违规」）"]
    registered = {(r.get("path") or "").strip() for r in rows}
    for entry in known:
        rel = str(entry).strip()
        if not rel:
            problems.append("R8 登记项为空串（无效登记）")
            continue
        if not os.path.isdir(os.path.join(repo, rel)):
            problems.append(
                "R8 登记项已无对象（目录不存在，须删除登记）: %s" % rel)
            continue
        if any(p == rel or p.startswith(rel.rstrip("/") + "/") for p in registered):
            problems.append(
                "R8 登记项已失效（该目录已登记进 test_index.csv，缺口已闭合，"
                "须删除登记）: %s" % rel)
    return problems


def run(csv_path: str, registry_path: str = None, repo: str = None) -> int:
    if not os.path.isfile(csv_path):
        print(json.dumps({"tool": "eng/ci/check_test_index.py", "csv": csv_path,
                          "verdict": "INPUT_MISSING", "exit_code": 2},
                         ensure_ascii=False, indent=2))
        return 2
    with open(csv_path, encoding="utf-8") as fh:
        text = fh.read()
    problems, rows, aggregate = validate(text)
    # R8 的对象是"仓库里的目录"，故 root 用 REPO（不随 --csv 漂移）；
    # --registry 只是给夹具/自测用的登记册路径覆盖。
    problems = problems + check_known_unregistered(repo or REPO, rows, registry_path)
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

    # ---- R8 缺口登记册棘轮（登记项必须仍有对象）------------------------------
    import shutil
    import tempfile

    def add_r8(name, known, make_dirs, want_rc, want_token=None, registry="auto"):
        tmp = tempfile.mkdtemp(prefix="astrocs_ti_r8_")
        try:
            for d in make_dirs:
                os.makedirs(os.path.join(tmp, d), exist_ok=True)
            if registry == "auto":
                os.makedirs(os.path.join(tmp, "eng", "packaging", "config"), exist_ok=True)
                reg = os.path.join(tmp, "eng", "packaging", "config", "config_registry.json")
                with open(reg, "w", encoding="utf-8") as fh:
                    json.dump({"index_ownership":
                               {"test_index_known_unregistered": known}}, fh)
            else:
                reg = os.path.join(tmp, registry)
            problems, rows, _agg = validate(_fixture())
            problems = problems + check_known_unregistered(tmp, rows, reg)
            rc = 1 if problems else 0
            blob = " ".join(problems)
            ok = (rc == want_rc) and (want_token is None or want_token in blob)
            cases.append((name, rc, want_rc, ok, blob[:160]))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    add_r8("pos_known_unregistered_live", ["eng/tests/oracle"], ["eng/tests/oracle"], 0)
    add_r8("neg_known_dir_missing", ["eng/tests/gaia_zlib"], [], 1, "已无对象")
    add_r8("neg_known_already_registered", ["eng/tests/config"], ["eng/tests/config"], 1, "已失效")
    add_r8("neg_known_registry_missing", ["eng/tests/oracle"], ["eng/tests/oracle"], 1,
           "登记册缺失", registry="eng/packaging/config/NO_SUCH.json")

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
    ap.add_argument("--registry", default=None,
                    help="缺口登记册路径覆盖（默认 eng/packaging/config/config_registry.json）")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    path = args.csv if os.path.isabs(args.csv) else os.path.join(REPO, args.csv)
    return run(path, args.registry)


if __name__ == "__main__":
    sys.exit(main())
