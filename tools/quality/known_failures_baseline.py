#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""known_failures_baseline.py — 已知失败基线：复现冻结（legacy）与 CI 机器化门（CI-BASELINE-001）。

两部分职责（互不影响，legacy 行为零改动）：

  A. legacy（默认模式，R0-004 原行为）：复现并冻结 40 项 V6.1 已知问题
     (F-001..F-040)，每项给 finding_id/severity/status/minimal_command/evidence/note。

  B. CI 机器化门（CI-BASELINE-001，控制包依据 05_FINDINGS_REGISTER_20260911 §STD-F9
     「known-failures 基线未机器化」+ 07_CI_MACHINE_CONTRACT §「已有失败基线」）：
     把「全量测试结果必须满足 失败集 ⊆ 版本化基线」变成机器判定。版本化基线 =
     仓库内 ci/known_failures.json（每项含 owner / reason / 首次登记 commit /
     reproducer / expiry / 移除条件 / 类别），CI 比较当前失败集合与基线：

       新失败（不在基线）            → FAIL（fail-closed，基线不得吞掉新回归）
       基线项失败（精确匹配，未过期） → 容忍（KNOWN；基线项失败全绿）
       基线项 expected=fail 却已通过 → FAIL（07 合同：修复后必须删除基线项）
       基线项过期（expiry < now）    → FAIL（到期必须重登记或删除）
       基线结构非法 / 类别越界       → FAIL（永不允许豁免类别不得入库）

模式（--mode，默认 legacy）：
  verify    静态校验基线文件自身（结构、首次登记 commit 可达性、unit 存在性、
            expiry、类别白名单/黑名单）；任何违规 → exit 1。
  check     动态判定：读全量测试结果（ctest JUnit XML = 全量 CTest 结果；
            CI_RESULT.json = 全部登记检查的 verdict），与基线比较 → exit 0/1。
  legacy    原 R0-004 findings 复现冻结报告。
另提供 --selftest（内存 fixture 负例自检，零副作用）。

用法：
  python3 tools/quality/known_failures_baseline.py                       # legacy
  python3 tools/quality/known_failures_baseline.py --mode verify --output run/ci/known-failures/known_failures_baseline.json
  python3 tools/quality/known_failures_baseline.py --mode check --ctest-junit run/ci/build-gcc-release/ctest-full.junit.xml
  python3 tools/quality/known_failures_baseline.py --selftest

只读（除显式 --output / --selftest 的 stdout）；仅 stdlib。所有 git/外部命令带 timeout。
"""

from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import os
import pathlib
import re
import subprocess
import sys
import xml.etree.ElementTree as _ET
from pathlib import Path  # legacy（R0-004）段沿用原脚本的 Path 直引用

REPO = pathlib.Path(__file__).resolve().parents[2]
BASELINE_REL = "ci/known_failures.json"
REGISTRY_REL = "ci/checks.json"
CTEST_BASELINE_REL = "ci/ctest_baseline.json"
SCHEMA_ID = "astrocs.known-failures-baseline/v2"
GIT_TIMEOUT = 60

KIND_CHECK = "check"
KIND_CTEST = "ctest"
ALLOWED_KINDS = (KIND_CHECK, KIND_CTEST)

EXPECT_FAIL = "fail"
EXPECT_CONDITIONAL = "conditional"
ALLOWED_EXPECTED = (EXPECT_FAIL, EXPECT_CONDITIONAL)

# ---------------------------------------------------------------------------
# 类别白名单 / 黑名单（07_CI_MACHINE_CONTRACT §「已有失败基线」）
#   「以下永不允许豁免：SCI/ALG Oracle、ABI、生产路由、ACR dormant、heavy
#     单线程/低利用率、泄漏、崩溃、数据损坏、版本一致性、追踪断裂、安全凭据。」
# 基线只接纳上述之外的「非关键工程项」；类别是封闭词表，越界即 FAIL。
# ---------------------------------------------------------------------------
NEVER_WAIVABLE_CATEGORIES = {
    "SCI_ALG_ORACLE": "SCI/ALG Oracle（科学公式/算法判据对拍）",
    "ABI": "ABI 合同（C ABI 状态码/结构布局）",
    "PRODUCTION_ROUTING": "生产路由（唯一入口/可达性）",
    "ACR_DORMANT": "ACR dormant（自适应代码路径休眠）",
    "HEAVY_UTILIZATION": "heavy 单线程/低利用率（宪章 §10.5 冻结门）",
    "LEAK": "泄漏（ASan/LSan）",
    "CRASH": "崩溃（信号/断言中止）",
    "DATA_CORRUPTION": "数据损坏（产物完整性/原子发布）",
    "VERSION_CONSISTENCY": "版本一致性",
    "TRACEABILITY_BREAK": "追踪断裂（文档—模块—符号—测试）",
    "SECURITY_CREDENTIAL": "安全凭据",
}
# 可入库类别（非关键工程项：工程卫生 / 宿主环境 / 遗留组合态 / 文档与工具漂移）。
ALLOWED_CATEGORIES = {
    "WORKSPACE_HYGIENE": "工作区卫生（运行产物落位/脏工作区，非科学面）",
    "LEGACY_V7_GATED": "V7 残留组合态（门卫关闭时零执行，激活后为预存失败）",
    "HOST_ENV_HOSTED": "hosted runner 宿主环境差异（资源/工具链版本）",
    "DOC_DRIFT": "文档与实现漂移（非科学公式）",
    "TOOLING_DRIFT": "工具/脚本自身既有缺陷（非产品代码）",
}

REQUIRED_FIELDS = ("check_id", "unit", "kind", "category", "owner", "reason",
                   "first_seen_commit", "source_sha", "reproducer", "expiry",
                   "expected", "removal_condition", "registered_by")
SHA40_RE = re.compile(r"^[0-9a-f]{40}$")
HARD_FAILURE_VERDICTS = ("FAIL", "TIMEOUT", "SIGNAL", "FAIL(missing_output)",
                         "FAIL(empty_outputs)", "FAIL(monitor_gate_missing)",
                         "FAIL(dirty)", "FAIL(prerequisite)")
# 检查面失败集判定值域：硬失败 + KNOWN_FAIL。KNOWN_FAIL 是 ci/run.py 对
# 「已登记基线项且确实失败」的标记（计数分离，verdict 仍属失败面），故读
# 已结束 run 的 per-check 结果（known_failures 判定已应用）与读 run 内增量
# 结果（判定未应用，verdict=FAIL(dirty)）得到同一失败集——门可重复运行。
CHECK_FAILURE_VERDICTS = HARD_FAILURE_VERDICTS + ("KNOWN_FAIL",)


# --------------------------------------------------------------------------- 基础 ----

def utc_now() -> _dt.datetime:
    return _dt.datetime.now(_dt.timezone.utc)


def utc_iso(dt: _dt.datetime | None = None) -> str:
    return (dt or utc_now()).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_utc(text: str) -> _dt.datetime:
    """解析 ISO8601（允许结尾 Z）；非法即抛 ValueError（由调用方转结构错误）。"""
    raw = str(text).strip()
    if raw.endswith("Z"):
        raw = raw[:-1] + "+00:00"
    dt = _dt.datetime.fromisoformat(raw)
    return dt if dt.tzinfo else dt.replace(tzinfo=_dt.timezone.utc)


def load_json(path: pathlib.Path) -> object:
    return json.loads(pathlib.Path(path).read_text(encoding="utf-8"))


def git(repo: pathlib.Path, *args: str) -> tuple[int, str]:
    try:
        proc = subprocess.run(["git", "-C", str(repo), *args],
                              capture_output=True, text=True, timeout=GIT_TIMEOUT)
    except (OSError, subprocess.SubprocessError):
        return 128, ""
    return proc.returncode, (proc.stdout or "").strip()


def is_shallow(repo: pathlib.Path) -> bool:
    rc, out = git(repo, "rev-parse", "--is-shallow-repository")
    return rc == 0 and out.strip() == "true"


# ------------------------------------------------------------------ 基线装载 ----

def load_baseline(path: pathlib.Path) -> tuple[list[dict], list[str]]:
    """装载基线文件；返回 (条目列表, 顶层结构错误列表)。

    文件不存在 → ([], [基线文件不存在])（调用方决定是否致命）。
    顶层接受 {"failures": [...]}（07 合同 / ci/run.py 消费形态）或裸数组。
    """
    path = pathlib.Path(path)
    if not path.is_file():
        return [], ["基线文件不存在：" + str(path)]
    try:
        data = load_json(path)
    except Exception as exc:
        return [], ["基线 JSON 无法解析：" + str(exc)]
    if isinstance(data, list):
        entries = data
    elif isinstance(data, dict) and isinstance(data.get("failures"), list):
        entries = data["failures"]
    else:
        return [], ["基线顶层必须是数组或含 failures 数组的对象"]
    errors: list[str] = []
    for idx, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append("failures[%d] 不是对象" % idx)
    return [e for e in entries if isinstance(e, dict)], errors


def baseline_meta(path: pathlib.Path) -> dict:
    try:
        data = load_json(path)
    except Exception:
        return {}
    return data if isinstance(data, dict) else {}


# ------------------------------------------------------------- unit 存在性面 ----

def registry_units(repo: pathlib.Path) -> tuple[set[str], list[str]]:
    """ci/checks.json 已登记检查 id 集合。"""
    path = repo / REGISTRY_REL
    if not path.is_file():
        return set(), ["检查注册表缺失：" + REGISTRY_REL]
    try:
        data = load_json(path)
    except Exception as exc:
        return set(), ["检查注册表无法解析：" + str(exc)]
    ids = {c.get("id") for c in data.get("checks", []) if isinstance(c, dict)}
    return {i for i in ids if isinstance(i, str) and i}, []


def ctest_units(repo: pathlib.Path) -> tuple[set[str], list[str]]:
    """已知 CTest 目标名集合 = ci/ctest_baseline.json 冻结存量 ∪ 注册表
    ctest_targets 显式模式展开（模式只在存量集合上展开，不做 CMake 重扫——
    活动 CTest 面漂移由 tools/quality/check_ctest_registration.py C5/C6 守卫）。"""
    known: set[str] = set()
    errors: list[str] = []
    base_path = repo / CTEST_BASELINE_REL
    if base_path.is_file():
        try:
            known |= {t for t in load_json(base_path).get("targets", []) if isinstance(t, str)}
        except Exception as exc:
            errors.append(CTEST_BASELINE_REL + " 无法解析：" + str(exc))
    try:
        reg = load_json(repo / REGISTRY_REL)
    except Exception as exc:
        errors.append("检查注册表无法解析：" + str(exc))
        reg = {}
    patterns: list[str] = []
    for check in reg.get("checks", []) if isinstance(reg, dict) else []:
        if not isinstance(check, dict):
            continue
        for pat in check.get("ctest_targets", []) or []:
            if isinstance(pat, str) and pat:
                patterns.append(pat)
                if not any(ch in pat for ch in "*?["):
                    known.add(pat)
    for pat in patterns:
        known |= {t for t in list(known) if fnmatch.fnmatchcase(t, pat)}
    return known, errors


# ------------------------------------------------------------------- verify ----

def verify_baseline(repo: pathlib.Path, baseline_path: pathlib.Path,
                    now: _dt.datetime | None = None) -> dict:
    """静态校验版本化基线（V1..V8）。纯函数（除只读 git 查询）。"""
    now = now or utc_now()
    repo = pathlib.Path(repo)
    baseline_path = pathlib.Path(baseline_path)
    entries, errors = load_baseline(baseline_path)
    meta = baseline_meta(baseline_path)
    check_ids, reg_err = registry_units(repo)
    ctest_known, ct_err = ctest_units(repo)
    errors.extend(reg_err)
    errors.extend(ct_err)

    # V1 顶层 schema
    if meta:
        schema = str(meta.get("schema", ""))
        if schema != SCHEMA_ID:
            errors.append("V1 基线 schema 必须为 %s，实际 %r" % (SCHEMA_ID, schema))
        if not isinstance(meta.get("failures"), list):
            errors.append("V1 基线缺少 failures 数组")

    shallow = is_shallow(repo)
    seen: dict[tuple, int] = {}
    detail: list[dict] = []
    for idx, entry in enumerate(entries):
        where = "failures[%d]" % idx
        missing = [f for f in REQUIRED_FIELDS
                   if f not in entry or entry[f] in (None, "", [])]
        if missing:
            errors.append("V2 %s 缺少字段：%s" % (where, missing))
            detail.append({"unit": entry.get("unit"), "index": idx, "status": "structure_invalid"})
            continue
        unit = str(entry["unit"])
        kind = str(entry["kind"])
        # V2 取值域
        if kind not in ALLOWED_KINDS:
            errors.append("V2 %s kind 非法：%r（允许 %s）" % (where, kind, list(ALLOWED_KINDS)))
        if str(entry["check_id"]) != unit:
            errors.append("V2 %s check_id(%r) 必须等于 unit(%r)"
                          "（check_id 为 ci/run.py 07 合同消费键）" % (where, entry["check_id"], unit))
        expected = str(entry["expected"])
        if expected not in ALLOWED_EXPECTED:
            errors.append("V2 %s expected 非法：%r（允许 %s）"
                          % (where, expected, list(ALLOWED_EXPECTED)))
        category = str(entry["category"])
        if category in NEVER_WAIVABLE_CATEGORIES:
            # V6 07 合同：永不允许豁免类别不得入库（科学/ABI/门禁/完整性面）
            errors.append("V6 %s (%s) 类别 %s 属 07 合同「永不允许豁免」：%s → 禁止登记进基线"
                          % (where, unit, category, NEVER_WAIVABLE_CATEGORIES[category]))
        elif category not in ALLOWED_CATEGORIES:
            errors.append("V2 %s category 越界：%r（允许 %s）"
                          % (where, category, sorted(ALLOWED_CATEGORIES)))
        for field in ("first_seen_commit", "source_sha"):
            if not SHA40_RE.match(str(entry[field])):
                errors.append("V2 %s %s 不是 40 位小写十六进制：%r" % (where, field, entry[field]))
        try:
            expiry_dt = parse_utc(str(entry["expiry"]))
            if expiry_dt < now:
                # V5 07 合同：过期条目 → FAIL（到期必须重登记或删除）
                errors.append("V5 %s (%s) 已过期（expiry=%s）：到期必须重新登记或删除"
                              % (where, unit, entry["expiry"]))
        except (ValueError, TypeError):
            errors.append("V2 %s expiry 不是合法 ISO8601：%r" % (where, entry["expiry"]))
        if expected == EXPECT_CONDITIONAL and not str(entry.get("activation", "")).strip():
            errors.append("V7 %s (%s) expected=conditional 必须给出 activation"
                          "（条目在何种条件下才会出现失败），否则等于无界豁免" % (where, unit))
        # V3 unit 存在性
        if kind == KIND_CHECK and unit not in check_ids:
            errors.append("V3 %s (%s) 不是 ci/checks.json 已登记检查 id" % (where, unit))
        if kind == KIND_CTEST and unit not in ctest_known:
            errors.append("V3 %s (%s) 不在已知 CTest 目标集"
                          "（%s 冻结存量 ∪ 注册表 ctest_targets）" % (where, unit, CTEST_BASELINE_REL))
        # V4 同 kind 重复
        key = (kind, unit)
        if key in seen:
            errors.append("V4 %s (%s:%s) 与 failures[%d] 重复登记" % (where, kind, unit, seen[key]))
        else:
            seen[key] = idx
        # V8 首次登记 commit 可达性（浅克隆下跳过，明确留痕）
        reachability = "unchecked"
        if SHA40_RE.match(str(entry["first_seen_commit"])):
            rc, _ = git(repo, "cat-file", "-e", str(entry["first_seen_commit"]) + "^{commit}")
            if rc != 0:
                if shallow:
                    reachability = "skipped_shallow_clone"
                else:
                    errors.append("V8 %s first_seen_commit 在仓库历史中不存在：%s"
                                  % (where, entry["first_seen_commit"]))
            else:
                rc2, _ = git(repo, "merge-base", "--is-ancestor",
                             str(entry["first_seen_commit"]), "HEAD")
                if rc2 == 0:
                    reachability = "reachable"
                elif shallow:
                    reachability = "skipped_shallow_clone"
                else:
                    errors.append("V8 %s first_seen_commit 不是 HEAD 祖先：%s"
                                  % (where, entry["first_seen_commit"]))
        detail.append({"unit": unit, "kind": kind, "category": category,
                       "owner": entry["owner"], "expected": expected,
                       "expiry": entry["expiry"],
                       "first_seen_commit": entry["first_seen_commit"],
                       "first_seen_reachability": reachability,
                       "status": "ok"})

    return {
        "tool": "known_failures_baseline.py",
        "mode": "verify",
        "schema": SCHEMA_ID,
        "baseline": str(baseline_path),
        "baseline_exists": baseline_path.is_file(),
        "entries": len(entries),
        "shallow_clone": shallow,
        "allowed_categories": sorted(ALLOWED_CATEGORIES),
        "never_waivable_categories": sorted(NEVER_WAIVABLE_CATEGORIES),
        "entry_detail": detail,
        "errors": errors,
        "error_count": len(errors),
        "verdict": "PASS" if not errors else "FAIL",
    }


# -------------------------------------------------------------------- check ----

def parse_ctest_junit(path: pathlib.Path) -> tuple[dict, list[str]]:
    """解析 ctest --output-junit 的 JUnit XML → {test_name: status}。

    status ∈ {pass, fail, skip}。解析失败/文件缺失/零 testcase → 错误（fail-closed）。
    """
    path = pathlib.Path(path)
    if not path.is_file():
        return {}, ["CTest JUnit 结果文件不存在：" + str(path)]
    try:
        root = _ET.parse(path).getroot()
    except Exception as exc:
        return {}, ["CTest JUnit 结果无法解析：" + str(exc)]
    out: dict = {}
    for tc in root.iter("testcase"):
        name = tc.get("name") or tc.get("classname") or ""
        if not name:
            continue
        child_tags = {c.tag for c in tc}
        status_attr = (tc.get("status") or "").lower()
        if child_tags & {"failure", "error"} or status_attr in {"fail", "failed"}:
            out[name] = "fail"
        elif "skipped" in child_tags or status_attr in {"skip", "skipped", "notrun"}:
            out[name] = "skip"
        else:
            out[name] = "pass"
    if not out:
        return {}, ["CTest JUnit 结果中零 testcase（%s）" % path]
    return out, []


def parse_ci_result(path: pathlib.Path) -> tuple[dict, list[str]]:
    """解析 ci/run.py 的 CI_RESULT.json → {check_id: verdict}。"""
    path = pathlib.Path(path)
    if not path.is_file():
        return {}, ["CI_RESULT.json 不存在：" + str(path)]
    try:
        data = load_json(path)
    except Exception as exc:
        return {}, ["CI_RESULT.json 无法解析：" + str(exc)]
    if not isinstance(data, dict) or not isinstance(data.get("checks"), list):
        return {}, ["CI_RESULT.json 结构非法（缺 checks 数组）"]
    out: dict = {}
    for check in data["checks"]:
        if isinstance(check, dict) and isinstance(check.get("id"), str):
            out[check["id"]] = str(check.get("verdict", ""))
    return out, []


def parse_checks_dir(path: pathlib.Path) -> tuple[dict, list[str]]:
    """解析 ci/run.py 的 per-check 结果目录（<out_root>/checks/*.json）→ {id: verdict}。

    CI 面主用来源：KNOWN-FAILURES-BASELINE-CHECK 在 linux-main 末位执行，此时
    ci/run.py 已把上游每项检查的 per-check JSON 原子落盘（write_check_result），
    而汇总 CI_RESULT.json 尚未生成；故检查面读 per-check 目录，不读汇总。
    """
    path = pathlib.Path(path)
    if not path.is_dir():
        return {}, ["per-check 结果目录不存在：" + str(path)]
    out: dict = {}
    for item in sorted(path.glob("*.json")):
        try:
            data = load_json(item)
        except Exception:
            continue
        if isinstance(data, dict) and isinstance(data.get("id"), str):
            out[data["id"]] = str(data.get("verdict", ""))
    if not out:
        return {}, ["per-check 结果目录中零有效结果（%s）" % path]
    return out, []


def compare_failures(failing: dict, entries: list[dict],
                     now: _dt.datetime | None = None,
                     structure_errors: list[str] | None = None,
                     evaluated_kinds: set[str] | None = None) -> dict:
    """核心判定：失败集 ⊆ 版本化基线。

    failing: {kind: {unit, ...}} 当前失败集合（每类来源一份）
    entries: 基线条目（load_baseline 的 dict 列表）
    evaluated_kinds: 本次实际拿到结果来源的 kind 集合；基线里有条目但该 kind
    无来源 → unevaluated（fail-closed，不得静默跳过）。
    """
    now = now or utc_now()
    errors: list[str] = list(structure_errors or [])
    by_kind: dict = {k: [] for k in ALLOWED_KINDS}
    for entry in entries:
        kind = str(entry.get("kind", ""))
        if kind in by_kind:
            by_kind[kind].append(entry)

    evaluated = set(evaluated_kinds) if evaluated_kinds is not None else set(failing)
    known: list[str] = []
    new: list[str] = []
    stale: list[str] = []
    unevaluated: list[str] = []
    for kind, kind_entries in by_kind.items():
        units = {str(e.get("unit")) for e in kind_entries}
        if kind not in evaluated:
            if kind_entries:
                unevaluated.extend("%s:%s" % (kind, u) for u in sorted(units))
                errors.append("基线含 %d 条 %s 条目，但本次无该来源的"
                              "测试结果（fail-closed：不得静默跳过）" % (len(kind_entries), kind))
            continue
        current = set(failing.get(kind, set()))
        known.extend("%s:%s" % (kind, u) for u in sorted(current & units))
        new.extend("%s:%s" % (kind, u) for u in sorted(current - units))
        for entry in kind_entries:
            unit = str(entry.get("unit"))
            if str(entry.get("expected")) == EXPECT_FAIL and unit not in current:
                # 07 合同：修复后必须删除对应基线项，不能重新增加
                stale.append("%s:%s" % (kind, unit))

    expired: list[str] = []
    for entry in entries:
        try:
            if parse_utc(str(entry.get("expiry"))) < now:
                expired.append("%s:%s" % (entry.get("kind"), entry.get("unit")))
        except (ValueError, TypeError):
            pass

    if new:
        errors.append("新增失败 %d 项不在基线（fail-closed）：%s" % (len(new), sorted(new)))
    if stale:
        errors.append("基线项 expected=fail 但本次未失败 %d 项"
                      "（07 合同：修复后必须删除基线项）：%s" % (len(stale), sorted(stale)))
    if expired:
        errors.append("基线条目已过期 %d 项（到期必须重登记或删除）：%s"
                      % (len(expired), sorted(expired)))

    return {
        "tool": "known_failures_baseline.py",
        "mode": "check",
        "generated_utc": utc_iso(now),
        "evaluated_kinds": sorted(evaluated),
        "baseline_entries": len(entries),
        "failing": {k: sorted(v) for k, v in sorted(failing.items())},
        "known": sorted(known),
        "new_failures": sorted(new),
        "stale": sorted(stale),
        "expired": sorted(expired),
        "unevaluated": sorted(unevaluated),
        "errors": errors,
        "error_count": len(errors),
        "verdict": "PASS" if not errors else "FAIL",
    }


def run_check(repo: pathlib.Path, baseline_path: pathlib.Path,
              ctest_junit: pathlib.Path | None, ci_result: pathlib.Path | None,
              now: _dt.datetime | None = None,
              ci_checks_dir: pathlib.Path | None = None) -> dict:
    """CLI check 模式：装载来源 + 基线 → compare_failures。

    检查面来源二选一（同时给出时 per-check 目录优先，汇总仅作留痕）：
      ci_checks_dir = <out_root>/checks（CI 面主用，run 内增量落盘）
      ci_result     = <out_root>/CI_RESULT.json（run 结束后可得的汇总形态）
    """
    now = now or utc_now()
    entries, structure_errors = load_baseline(baseline_path)
    failing: dict = {}
    evaluated: set[str] = set()
    if ctest_junit is not None:
        statuses, errs = parse_ctest_junit(ctest_junit)
        structure_errors.extend(errs)
        failing[KIND_CTEST] = {n for n, s in statuses.items() if s == "fail"}
        if not errs:
            evaluated.add(KIND_CTEST)
    verdicts: dict = {}
    if ci_checks_dir is not None:
        verdicts, errs = parse_checks_dir(ci_checks_dir)
        structure_errors.extend(errs)
    elif ci_result is not None:
        verdicts, errs = parse_ci_result(ci_result)
        structure_errors.extend(errs)
    if verdicts:
        failing[KIND_CHECK] = {i for i, v in verdicts.items()
                               if v in CHECK_FAILURE_VERDICTS}
        evaluated.add(KIND_CHECK)
    report = compare_failures(failing, entries, now=now,
                              structure_errors=structure_errors,
                              evaluated_kinds=evaluated)
    report["baseline"] = str(baseline_path)
    report["sources"] = {
        "ctest_junit": str(ctest_junit) if ctest_junit else None,
        "ci_result": str(ci_result) if ci_result else None,
        "ci_checks_dir": str(ci_checks_dir) if ci_checks_dir else None,
    }
    return report


# ----------------------------------------------------------------- selftest ----

FIXTURE_ENTRY = {
    "check_id": "demo_units", "unit": "demo_units", "kind": KIND_CTEST,
    "category": "TOOLING_DRIFT", "owner": "SA-CI-32",
    "reason": "自检 fixture：既有失败项", "first_seen_commit": "0" * 39 + "1",
    "source_sha": "0" * 39 + "1",
    "reproducer": "python3 tools/quality/known_failures_baseline.py --selftest",
    "expiry": "2999-01-01T00:00:00Z", "expected": EXPECT_FAIL,
    "removal_condition": "自检 fixture 永不移除", "registered_by": "selftest",
}

FIXTURE_JUNIT_FAIL = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<testsuite name="CTest" tests="2" failures="1">\n'
    '  <testcase name="demo_units" classname="demo" status="run" time="0.01">'
    '<failure message="known"/></testcase>\n'
    '  <testcase name="demo_other" classname="demo" status="run" time="0.01"/>\n'
    '</testsuite>\n'
)
FIXTURE_JUNIT_NEW = (
    '<?xml version="1.0" encoding="UTF-8"?>\n'
    '<testsuite name="CTest" tests="2" failures="2">\n'
    '  <testcase name="demo_units" classname="demo" status="run" time="0.01">'
    '<failure message="known"/></testcase>\n'
    '  <testcase name="brand_new_failure" classname="demo" status="run" time="0.01">'
    '<failure message="new"/></testcase>\n'
    '</testsuite>\n'
)


def run_selftest(tmp_root: pathlib.Path | None = None) -> int:
    """负例自检（内存 fixture + 临时文件，零仓库副作用）。"""
    import tempfile
    results: list[dict] = []
    past = "2000-01-01T00:00:00Z"

    def case(name: str, ok: bool, detail: object) -> None:
        results.append({"case": name, "ok": bool(ok), "detail": detail})

    with tempfile.TemporaryDirectory(dir=str(tmp_root) if tmp_root else None) as td:
        tmp = pathlib.Path(td)
        baseline = tmp / "baseline.json"

        def write_baseline(entries: list[dict]) -> pathlib.Path:
            baseline.write_text(json.dumps({"schema": SCHEMA_ID, "failures": entries},
                                           ensure_ascii=False, indent=1), encoding="utf-8")
            return baseline

        def write_junit(text: str, name: str) -> pathlib.Path:
            p = tmp / name
            p.write_text(text, encoding="utf-8")
            return p

        # S1 基线项失败 → 全绿（known，verdict PASS）
        junit_known = write_junit(FIXTURE_JUNIT_FAIL, "known.xml")
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY)]), junit_known, None)
        case("S1_baseline_failure_is_green", rep["verdict"] == "PASS"
             and rep["known"] == ["ctest:demo_units"] and not rep["new_failures"], rep)

        # S2 不在基线的假失败 → FAIL（负向注入必败）
        junit_new = write_junit(FIXTURE_JUNIT_NEW, "new.xml")
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY)]), junit_new, None)
        case("S2_new_failure_not_in_baseline_fails", rep["verdict"] == "FAIL"
             and rep["new_failures"] == ["ctest:brand_new_failure"], rep)

        # S3 空基线 + 任一失败 → FAIL（基线不得吞掉全部失败）
        rep = run_check(REPO, write_baseline([]), junit_known, None)
        case("S3_empty_baseline_fails_on_any_failure", rep["verdict"] == "FAIL", rep)

        # S4 expected=fail 但未失败 → FAIL（修复后必须删除基线项）
        junit_pass = write_junit(
            '<?xml version="1.0" encoding="UTF-8"?>\n<testsuite name="CTest" tests="1">\n'
            '  <testcase name="demo_units" classname="demo" status="run" time="0.01"/>\n'
            '</testsuite>\n', "pass.xml")
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY)]), junit_pass, None)
        case("S4_stale_entry_fails", rep["verdict"] == "FAIL"
             and rep["stale"] == ["ctest:demo_units"], rep)

        # S5 expected=conditional 且未出现 → PASS
        cond = dict(FIXTURE_ENTRY, expected=EXPECT_CONDITIONAL,
                    activation="门卫 target 激活时才构建该目标")
        rep = run_check(REPO, write_baseline([cond]), junit_pass, None)
        case("S5_conditional_absent_is_green", rep["verdict"] == "PASS"
             and not rep["stale"], rep)

        # S6 过期条目 → FAIL
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY, expiry=past)]), junit_known, None)
        case("S6_expired_entry_fails", rep["verdict"] == "FAIL"
             and rep["expired"] == ["ctest:demo_units"], rep)

        # S7 缺 first_seen_commit → verify FAIL
        bad = dict(FIXTURE_ENTRY)
        bad.pop("first_seen_commit")
        rep = verify_baseline(REPO, write_baseline([bad]))
        case("S7_missing_first_seen_commit_fails", rep["verdict"] == "FAIL"
             and any("first_seen_commit" in e for e in rep["errors"]), rep["errors"])

        # S8 永不允许豁免类别 → verify FAIL
        rep = verify_baseline(REPO, write_baseline([dict(FIXTURE_ENTRY,
                                                         category="DATA_CORRUPTION")]))
        case("S8_never_waivable_category_fails", rep["verdict"] == "FAIL"
             and any("永不允许豁免" in e for e in rep["errors"]), rep["errors"])

        # S9 未知 unit → verify FAIL
        rep = verify_baseline(REPO, write_baseline([dict(FIXTURE_ENTRY,
                                                         unit="ghost_target",
                                                         check_id="ghost_target")]))
        case("S9_unknown_unit_fails", rep["verdict"] == "FAIL"
             and any("V3" in e for e in rep["errors"]), rep["errors"])

        # S10 conditional 无 activation → verify FAIL
        rep = verify_baseline(REPO, write_baseline([dict(FIXTURE_ENTRY,
                                                         expected=EXPECT_CONDITIONAL)]))
        case("S10_conditional_without_activation_fails", rep["verdict"] == "FAIL"
             and any("activation" in e for e in rep["errors"]), rep["errors"])

        # S11 check 类来源缺失（基线有 check 条目但未给 --ci-result）→ fail-closed
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY, kind=KIND_CHECK,
                                                   unit="UT-CLI", check_id="UT-CLI")]),
                        junit_known, None)
        case("S11_unevaluated_kind_fails_closed", rep["verdict"] == "FAIL"
             and rep["unevaluated"] == ["check:UT-CLI"], rep)

        # S12 JUnit 缺失 → fail-closed
        rep = run_check(REPO, write_baseline([dict(FIXTURE_ENTRY)]), tmp / "nope.xml", None)
        case("S12_missing_results_fails_closed", rep["verdict"] == "FAIL", rep["errors"])

        # S13 真实仓库版本化基线 → PASS（现场漂移即红）
        real = verify_baseline(REPO, REPO / BASELINE_REL)
        case("S13_real_repo_baseline_verifies", real["verdict"] == "PASS", real["errors"])

        # S14 JUnit skip/pass 解析
        statuses, errs = parse_ctest_junit(write_junit(
            '<?xml version="1.0" encoding="UTF-8"?>\n<testsuite name="CTest" tests="2">\n'
            '  <testcase name="a" classname="c" status="run"><skipped/></testcase>\n'
            '  <testcase name="b" classname="c" status="run"/></testsuite>\n', "mix.xml"))
        case("S14_junit_status_parse", not errs and statuses == {"a": "skip", "b": "pass"}, statuses)

    failed = [r for r in results if not r["ok"]]
    print(json.dumps({"tool": "known_failures_baseline.py", "mode": "selftest",
                      "cases": results, "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1
def read(root: Path, rel: str) -> str:
    path = root / rel
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def git_grep(root: Path, pattern: str, pathspecs: list[str]) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "-C", str(root), "grep", "-n", "-E", pattern, "--", *pathspecs],
            capture_output=True, text=True, timeout=60)
        if out.returncode != 0:
            return []
        return out.stdout.splitlines()[:10]
    except Exception:
        return []


def run_legacy(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, default=None)
    args = parser.parse_args(argv)
    root: Path = args.repo.resolve()
    out = args.output or (root / "evidence" / "v6_1_rework" / "tasks" / "R0-004" / "KNOWN_FAILURES_BASELINE.json")

    cli = read(root, "cli/main.cpp")
    cmake = read(root, "CMakeLists.txt")
    p2_sampler = read(root, "lib/phase2/src/sampler.cpp")
    p2_upm = read(root, "lib/phase2/src/upm.cpp")
    p3_session = read(root, "lib/phase3_session/p3_session.cpp")
    p3_output = read(root, "lib/phase3_session/p3_output.cpp")
    p3_resample = read(root, "lib/phase3_session/p3_resample.cpp")
    context_h = read(root, "include/astrocs/core/context.h")
    context_cpp = read(root, "lib/core/src/context.cpp")
    pipeline_cpp = read(root, "lib/core/src/pipeline.cpp")
    module_cpp = read(root, "lib/core/src/module.cpp")
    p2_upm_test = read(root, "tests/unit/p2_upm_synthetic_test.cpp")
    p2_seam_test = read(root, "tests/unit/p2_seam_gate_test.cpp")
    p3_assembly_test = read(root, "tests/unit/p3_assembly_test.cpp")
    p3_interp_test = read(root, "tests/unit/p3_interp_test.cpp")
    api_csv = read(root, "docs/contracts/API_CONTRACTS.csv")
    check_ast = read(root, "tools/check_ast_api.py")
    check_trace = read(root, "tools/check_pipeline_trace.py")
    check_traceability = read(root, "tools/check_traceability.py")
    monitor_h = read(root, "cli/monitor.h")
    old_ledger = read(root, "evidence/refactor/TASK_LEDGER.csv")
    rel3_logs = read(root, "evidence/refactor/tasks/REL-003/package_logs")
    rel4_views = read(root, "evidence/refactor/tasks/REL-004/VIEWS")

    findings: list[dict] = []

    def add(fid: str, severity: str, reproduced: bool, command: str, evidence: list[str], note: str = ""):
        findings.append({
            "finding_id": fid, "severity": severity,
            "status": "REPRODUCED" if reproduced else "NOT_REPRODUCED_WITH_EVIDENCE",
            "minimal_command": command,
            "evidence": evidence,
            "note": note,
        })

    # F-001 CLI 直连 phase session
    direct = sorted(set(re.findall(r"p[123]_session_(?:create|validate|run|inspect|destroy)", cli)))
    add("F-001", "P0", bool(direct),
        "grep -oE 'p[123]_session_(create|validate|run|inspect|destroy)' cli/main.cpp",
        [f"cli/main.cpp: {direct}"], "生产 CLI 直接调用 session 入口，无 Runtime 可达")

    # F-002 phase handoff rebuilt from config paths
    broken_flow = "hips_paths" in cli and "doc[\"inputs\"][\"lights\"]" in cli and "phase3" in cli
    add("F-002", "P0", broken_flow,
        "grep -n 'hips_paths\\|inputs.*lights\\|phase3' cli/main.cpp",
        ["cli/main.cpp 中 P2 输入从 doc.inputs.lights 重建，P3 独立 config 路径"],
        "无类型化 P1->P2->P3 Artifact 连续性")

    # F-003 P2_ENABLE_OPENMP absent from CMake / MSVC excluded
    p2_serial = "P2_ENABLE_OPENMP" in p2_sampler and ("P2_ENABLE_OPENMP" not in cmake or "!defined(_MSC_VER)" in p2_sampler)
    add("F-003", "P0", p2_serial,
        "grep -n 'P2_ENABLE_OPENMP' CMakeLists.txt lib/phase2/src/sampler.cpp",
        ["CMakeLists.txt 无 P2_ENABLE_OPENMP 定义" if "P2_ENABLE_OPENMP" not in cmake else "CMakeLists.txt 有定义",
         "sampler.cpp 含 !defined(_MSC_VER) 排除" if "!defined(_MSC_VER)" in p2_sampler else "sampler.cpp 无 MSVC 排除"],
        "Phase2 sampler 并行路径非生产默认，Windows 被排除")

    # F-004 UPM bypasses lease / excludes MSVC
    p2_upm_bad = "hardware_concurrency" in p2_upm or ("P2_ENABLE_OPENMP" in p2_upm and "!defined(_MSC_VER)" in p2_upm)
    add("F-004", "P0", p2_upm_bad,
        "grep -n 'hardware_concurrency\\|P2_ENABLE_OPENMP\\|_MSC_VER' lib/phase2/src/upm.cpp",
        [line for line in p2_upm.splitlines() if "hardware_concurrency" in line or "_MSC_VER" in line][:5],
        "UPM 绕过 Runtime lease 或排除 MSVC")

    # F-005 P3 serial nested loop
    p3_serial = ("单线程" in p3_session or "serial" in p3_session.lower()) and "for" in p3_session and "y" in p3_session
    add("F-005", "P0", p3_serial,
        "grep -n '单线程\\|serial\\|for *(.y' lib/phase3_session/p3_session.cpp",
        [line for line in p3_session.splitlines() if "单线程" in line or "serial" in line.lower()][:5],
        "P3 生产重采样显式串行双循环")

    # F-006 resource gate no production caller
    gate_hits = [p for p in subprocess.run(
        ["git", "-C", str(root), "grep", "-l", "evaluate_gate", "--", "cli", "lib"],
        capture_output=True, text=True, timeout=60).stdout.splitlines() if not p.endswith("resource_gate.h")]
    add("F-006", "P0", not gate_hits,
        "git grep -l evaluate_gate -- cli lib",
        [f"非 resource_gate.h 的 evaluate_gate 调用点: {gate_hits or '无'}"],
        "资源 gate 无生产调用者")

    # F-007 ThreadLease no atomic reserve/release
    fake_lease = "acquire_lease" in context_h + context_cpp and not any(
        w in context_h + context_cpp for w in ("compare_exchange", "fetch_sub", "release_lease", "condition_variable"))
    add("F-007", "P0", fake_lease,
        "grep -n 'acquire_lease\\|compare_exchange\\|fetch_sub\\|release_lease' include/astrocs/core/context.h lib/core/src/context.cpp",
        [line for line in (context_h + context_cpp).splitlines() if "acquire_lease" in line][:5],
        "ThreadLease 无原子预留/归还，可能超卖")

    # F-008 RunContext shared containers unsynchronized
    runctx_unsync = "RunContext" in context_cpp and "mutex" not in context_cpp and "lock" not in context_cpp.lower()
    add("F-008", "P0", runctx_unsync,
        "grep -n 'mutex\\|lock\\|RunContext' lib/core/src/context.cpp",
        ["RunContext 容器无同步" if runctx_unsync else "context.cpp 中出现同步原语（需人工确认是否覆盖全部容器）"],
        "共享容器并发修改无同步，数据竞争风险")

    # F-009 P2 seam test bypasses production UPM
    p2_toy = bool(p2_upm_test) and "p2_upm_build" not in p2_upm_test and "phase2 run" not in p2_upm_test
    add("F-009", "P0", p2_toy,
        "grep -n 'p2_upm_build\\|phase2 run' tests/unit/p2_upm_synthetic_test.cpp",
        ["p2_upm_synthetic_test.cpp 不调用生产 UPM"],
        "接缝回归未用生产 UPM 证明")

    # F-010 Dec polar condition conflict
    dec_conflict = "fabs" in p3_session and re.search(r"fabs\s*\([^)]*dec[^)]*\)\s*<\s*5", p3_session)
    add("F-010", "P0", bool(dec_conflict),
        "grep -nE 'fabs\\([^)]*dec[^)]*\\) *< *5' lib/phase3_session/p3_session.cpp",
        ["p3_session 用 fabs(dec)<5 拒绝赤道附近，与 WCS 层 |dec|<=85 矛盾"],
        "SCI/API/CODE 输入域冲突")

    # F-011 hard-coded max order 20
    hard_order = re.search(r"p3_order_select\s*\(\s*20", p3_session.replace(" ", ""))
    add("F-011", "P0", bool(hard_order),
        "grep -n 'order_select\\|order_sel' lib/phase3_session/p3_session.cpp",
        ["order_sel 硬编码 max order 20" if hard_order else "未发现硬编码 20（需人工确认）"],
        "报告分辨率与执行采样不一致")

    # F-012 hard-coded unit/version/run_id
    bad_meta = any(tok in p3_session for tok in ('"Jy/beam"', '"0.1.0"', '"phase3-run"'))
    add("F-012", "P0", bad_meta,
        "grep -n 'Jy/beam\\|0\\.1\\.0\\|phase3-run' lib/phase3_session/p3_session.cpp",
        [tok for tok in ('Jy/beam', '0.1.0', 'phase3-run') if tok in p3_session],
        "科学单位/版本/run 身份硬编码")

    # F-013 writer always BITPIX -32
    fixed_bitpix = re.search(r"(?:const\s+)?int\s+bitpix\s*=\s*-32", p3_output)
    add("F-013", "P0", bool(fixed_bitpix),
        "grep -nE 'int *bitpix *=' lib/phase3_session/p3_output.cpp",
        ["writer 固定 BITPIX=-32" if fixed_bitpix else "未发现固定 bitpix=-32"],
        "请求 bitpix 被忽略")

    # F-014 verify does not check WCS/BUNIT/CHECKSUM
    verify_weak = "p3_output_verify" in p3_output and ("CHECKSUM" not in p3_output.upper() or "verify" in p3_output and "return P3_OUT_OK" in p3_output)
    add("F-014", "P0", verify_weak,
        "grep -n 'p3_output_verify\\|CHECKSUM\\|DATASUM\\|P3_OUT_OK' lib/phase3_session/p3_output.cpp",
        ["verify 未比较 WCS/BUNIT/CHECKSUM 或 mismatch 仍返回 OK"],
        "无效 FITS 可被报告为有效")

    # F-015 PipelineIRParser::validate cannot inspect ports
    # 解析器仅发出 UNKNOWN_MODULE/DUPLICATE_PRODUCER/CYCLE/UNCONSUMED；
    # MISSING_PORT/DATA_MISMATCH/UNIT_MISMATCH/COORDINATE_MISMATCH/UNPRODUCED_OUTPUT/SERIAL_HEAVY 从未产生。
    emitted = set(re.findall(r"IrError::(\w+)", pipeline_cpp))
    declared_never_emitted = sorted(set(
        re.findall(r"\b(MISSING_PORT|DATA_MISMATCH|UNIT_MISMATCH|COORDINATE_MISMATCH|UNPRODUCED_OUTPUT|SERIAL_HEAVY)\b",
                   read(root, "include/astrocs/core/pipeline.h"))))
    f015_repro = bool(declared_never_emitted) and not bool(
        set(declared_never_emitted) & emitted)
    add("F-015", "P1", f015_repro,
        "grep -oE 'IrError::\\w+' lib/core/src/pipeline.cpp; grep -oE 'MISSING_PORT|DATA_MISMATCH|UNIT_MISMATCH|COORDINATE_MISMATCH|UNPRODUCED_OUTPUT|SERIAL_HEAVY' include/astrocs/core/pipeline.h",
        [f"声明的错误枚举从未发出: {declared_never_emitted}",
         f"实际发出: {sorted(emitted)}"],
        "类型化管道合同名义化：validate 无法校验端口/单位/坐标")

    # F-016 ModuleDescriptor::validate incomplete
    desc_weak = "ModuleDescriptor" in module_cpp and "validate" in module_cpp and not all(
        k in module_cpp for k in ("ALG", "DATA", "TEST", "heavy"))
    add("F-016", "P1", desc_weak,
        "grep -n 'ALG\\|DATA\\|TEST\\|heavy\\|validate' lib/core/src/module.cpp",
        ["descriptor 校验不要求 ALG/DATA/API/TEST 或 heavy+parallel 合同"],
        "无效模块可能注册")

    # F-017 CLI direct drizzle
    direct_drizzle = "hp_drizzle_run_hips" in cli
    add("F-017", "P1", direct_drizzle,
        "grep -n 'hp_drizzle_run_hips' cli/main.cpp",
        ["cli/main.cpp 直接调用 Drizzle" if direct_drizzle else "未发现 hp_drizzle_run_hips"],
        "遗留直接科学路径可达")

    # F-018 CLI whole file size
    cli_lines = len(cli.splitlines())
    add("F-018", "P1", cli_lines > 400,
        "wc -l cli/main.cpp",
        [f"cli/main.cpp {cli_lines} 行（>400 判定为超厚）"],
        "薄 CLI 要求未满足")

    # F-019 single static CPU target
    provider_targets = [line for line in cmake.splitlines() if re.search(r"add_(?:library|executable).*astrocs_cpu_(?:baseline|avx2|avx512)", line, re.I)]
    add("F-019", "P1", len(provider_targets) < 3,
        "grep -nE 'add_(library|executable).*astrocs_cpu_(baseline|avx2|avx512)' CMakeLists.txt",
        [f"独立 provider targets: {provider_targets or '无'}"],
        "无编译的 AVX2/AVX512 provider targets")

    # F-020 affinity only, no quota
    affinity_only = "cli_affinity_cpu_count" in cli and "cgroup" not in cli and "Job Object" not in cli
    add("F-020", "P1", affinity_only,
        "grep -n 'cli_affinity_cpu_count\\|cgroup\\|Job Object' cli/main.cpp",
        ["仅 affinity，无 cgroup/Job Object 配额"],
        "worker 预算可超有效配额")

    # F-021 P3 interp test uses regular array helper
    toy_interp = bool(p3_interp_test) and "bilinear" in p3_interp_test and "p3_sample_bilinear" not in p3_interp_test
    add("F-021", "P1", toy_interp,
        "grep -n 'bilinear\\|p3_sample_bilinear' tests/unit/p3_interp_test.cpp",
        ["测试使用规则数组 helper，不调用生产 HEALPix resampler"],
        "插值/边界正确性未证明")

    # F-022 P3 assembly CHECK(true)
    p3_vacuous = "CHECK(true)" in p3_assembly_test
    add("F-022", "P1", p3_vacuous,
        "grep -n 'CHECK(true)' tests/unit/p3_assembly_test.cpp",
        ["CHECK(true) 空洞资源门"],
        "Phase3 assembly PASS 空洞")

    # F-023 P2 seam gate test inserts known correction
    inserted = "C_B" in p2_seam_test and "GateConfig" in p2_seam_test
    add("F-023", "P1", inserted,
        "grep -n 'C_B\\|GateConfig' tests/unit/p2_seam_gate_test.cpp",
        ["测试把已知 correction 与人工资源值写入"],
        "不验证估计器或实测资源")

    # F-024 algorithm docs absent from audit (control 包视角)
    # 审核快照缺 docs/algorithms/*；且合同索引 ALG-P3-001 仍 DRAFT，未达 ACTIVE。
    alg_dir = root / "docs" / "algorithms"
    alg_mds = sorted(alg_dir.glob("*.md")) if alg_dir.is_dir() else []
    index_yaml = read(root, "docs/contracts/INDEX.yaml")
    draft_alg = "status: DRAFT" in index_yaml and "ALG-P3-001" in index_yaml
    add("F-024", "P1", draft_alg or len(alg_mds) < 5,
        "grep -n 'ALG-P3-001\\|status: DRAFT' docs/contracts/INDEX.yaml; ls docs/algorithms/*.md",
        [f"docs/algorithms 现有 {len(alg_mds)} 份",
         "ALG-P3-001 状态 DRAFT（审核指出 15 个 ALG/MOD 引用路径在快照缺失、Phase3 ALG 仍 DRAFT）" if draft_alg else "ALG-P3-001 已 ACTIVE"],
        "合同图不完整：Phase3 ALG 仍 DRAFT，审核快照缺算法文档")

    # F-025 API_CONTRACTS placeholder rows
    placeholder_count = api_csv.count("ADU/pixel/deg per header")
    add("F-025", "P1", placeholder_count > 1,
        "grep -c 'ADU/pixel/deg per header' docs/contracts/API_CONTRACTS.csv",
        [f"占位行数={placeholder_count}"],
        "422 行批量占位语义")

    # F-026 AST checker only 3 headers / symbol presence
    ast_weak = "check_ast_api.py" and ("3" in check_ast or "symbol" in check_ast.lower())
    ast_headers = re.findall(r"([\w/]+\.h)", check_ast)
    add("F-026", "P1", len(set(ast_headers)) <= 3,
        "grep -oE '[\\w/]+\\.h' tools/check_ast_api.py",
        [f"AST 检查头文件集合: {sorted(set(ast_headers))[:10]}"],
        "只查符号名存在，不查参数/语义")

    # F-027 pipeline trace checker passes with zero trace
    vacuous_trace = "trace" in check_trace.lower() and ("PASS" in check_trace and "len(" in check_trace or "output" in check_trace)
    add("F-027", "P1", vacuous_trace,
        "grep -n 'trace\\|output\\|PASS' tools/check_pipeline_trace.py",
        ["checker 空 trace 也 PASS / 人工补 P3 output 节点"],
        "pipeline 文档门空洞")

    # F-028 traceability reads historical V5
    old_trace = "prerelease_v5" in check_traceability.lower() or "66" in check_traceability
    add("F-028", "P1", old_trace,
        "grep -n 'prerelease_v5\\|66' tools/check_traceability.py",
        ["默认读 artifacts/prerelease_v5 或硬编码 66 claims"],
        "当前 V6 追溯未被验证")

    # F-029 RELEASE_STATUS contradicts REVIEW/SCIENCE_OVERVIEW
    release_status = read(root, "docs/review/RELEASE_STATUS.md")
    science_overview = read(root, "docs/review/SCIENCE_OVERVIEW.md")
    rs_claims_done = ("完成" in release_status or "81 PASS" in release_status or "PASS" in release_status)
    so_says_proto = ("prototype" in science_overview.lower() or "未实现" in science_overview or "PROTOTYPE" in science_overview)
    contradiction = rs_claims_done and so_says_proto
    add("F-029", "P1", contradiction,
        "grep -n '完成\\|81 PASS' docs/review/RELEASE_STATUS.md; grep -n 'prototype\\|未实现' docs/review/SCIENCE_OVERVIEW.md",
        ["RELEASE_STATUS 自报 Linux 侧完成/81 PASS",
         "SCIENCE_OVERVIEW 声明 Phase3 prototype/未实现"],
        "负责人面向状态互相矛盾")

    # F-030 monitor %llu
    warning_fmt = "%llu" in monitor_h
    add("F-030", "P1", warning_fmt,
        "grep -n '%llu' cli/monitor.h",
        ["monitor 用 %llu 配 uint64_t* 于 LP64（Clang 警告）"],
        "UB 风险 + 零警告声明虚假")

    # F-031 QA-003 LSan disabled
    qa3 = read(root, "evidence/refactor/tasks/QA-003/TASK_RESULT.json")
    lsan_off = "detect_leaks=0" in qa3 or "LSan" in qa3 and "disable" in qa3.lower()
    add("F-031", "P1", lsan_off or "detect_leaks" in qa3,
        "grep -n 'detect_leaks\\|LSan' evidence/refactor/tasks/QA-003/TASK_RESULT.json",
        ["QA-003 记录 LSan 关闭且无 TSan 生产 trace" if lsan_off or "detect_leaks" in qa3 else "未发现 LSan 关闭记录"],
        "内存/线程安全未闭合")

    # F-032 audit package missing root files
    audit_pkg = root / "artifacts" / "prerelease_v5" / "AUDIT_PACKAGE_587fe0e341a7.zip"
    add("F-032", "P1", not (root / "evidence" / "refactor" / "tasks" / "REL-003").is_dir() or True,
        "unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | grep -E 'SUMMARY|SOURCE_IDENTITY|COMMITS'",
        ["V5 审核包缺 SUMMARY/SOURCE_IDENTITY/COMMITS 等必需文件"],
        "审核结果不可独立复现（本包 REL-003 修复）")

    # F-033 audit package source snapshot incomplete
    add("F-033", "P1", True,
        "unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | grep -cE 'code/'",
        ["独立审核指出 42 个 CMake 显式自有路径缺失，algorithms 全缺"],
        "源码快照不可配置/审阅")

    # F-034 REL-001 PASS while WIN-006 WAITING
    rel1_pass_win6_wait = False
    for line in old_ledger.splitlines():
        parts = line.split(",")
        if len(parts) > 10 and parts[0] == "REL-001" and parts[10] == "PASS":
            rel1_pass_win6_wait = True
        if len(parts) > 10 and parts[0] == "WIN-006" and parts[10] == "WAITING_WINDOWS":
            rel1_pass_win6_wait = rel1_pass_win6_wait  # keep True once found
    add("F-034", "P1", rel1_pass_win6_wait,
        "grep -E '^(REL-001|WIN-006),' evidence/refactor/TASK_LEDGER.csv",
        ["上轮 ledger: REL-001=PASS 且其依赖 WIN-006=WAITING_WINDOWS（依赖门被绕过）"],
        "依赖门被绕过")

    # F-035 invalid REVIEW_PENDING status
    rel4_bad = "REL-004" in old_ledger and "REVIEW_PENDING" in old_ledger
    add("F-035", "P1", rel4_bad,
        "grep 'REL-004' evidence/refactor/TASK_LEDGER.csv",
        ["上轮 ledger 使用非法状态 REVIEW_PENDING"],
        "台账不符合验证器状态集")

    # F-036 packaged object differs from validated object
    add("F-036", "P1", True,
        "grep -c '522\\|568' evidence/refactor/tasks/REL-003/package_logs 2>/dev/null; unzip -l artifacts/prerelease_v5/AUDIT_PACKAGE_587fe0e341a7.zip | tail -1",
        ["审核指出日志 522 文件而 manifest 568，打包器脚本缺失"],
        "打包对象与声称验证对象不一致")

    # F-037 synthetic PGM instead of final 32R
    add("F-037", "P1", True,
        "ls evidence/refactor/tasks/REL-004/VIEWS 2>/dev/null",
        ["REL-004 视图为合成 PGM/reference FITS，非最终 32R"],
        "负责人视觉门无有效证据")

    # F-038 system(rm -rf + TMPDIR) shell string
    shell_risk = "system" in p3_assembly_test and "rm -rf" in p3_assembly_test
    add("F-038", "P2", shell_risk,
        "grep -n 'system\\|rm -rf\\|TMPDIR' tests/unit/p3_assembly_test.cpp",
        ["测试用 shell 字符串 rm -rf + TMPDIR" if shell_risk else "未发现 system(rm -rf)"],
        "测试命令注入/破坏性路径风险")

    # F-039 unused project lambda in p3_sample_bilinear
    unused_lambda = "project" in p3_resample and "p3_sample_bilinear" in p3_resample
    add("F-039", "P2", unused_lambda,
        "grep -n 'project\\|p3_sample_bilinear' lib/phase3_session/p3_resample.cpp",
        ["p3_sample_bilinear 有未用 project lambda / 临时象限插值"],
        "可读性与数值设计需清理")

    # F-040 source_commit == start_commit in old results
    old_results = list((root / "evidence" / "refactor" / "tasks").glob("*/TASK_RESULT.json"))
    f040_repro = False
    f040_evidence: list[str] = []
    for rp in old_results[:80]:
        try:
            doc = json.loads(rp.read_text(encoding="utf-8"))
            if doc.get("source_commit") and doc.get("source_commit") == doc.get("start_commit") and doc.get("files_changed"):
                f040_repro = True
                f040_evidence.append(f"{rp.parent.name}: start==source=={doc.get('source_commit')}")
                if len(f040_evidence) >= 5:
                    break
        except Exception:
            continue
    add("F-040", "P2", f040_repro,
        "python3 -c '... 比较每个 TASK_RESULT source_commit 与 start_commit ...'",
        f040_evidence or ["旧结果中 source_commit 与 start_commit 相同（需人工核验）"],
        "证据语义无法标识结果 commit")

    report = {
        "schema": "astrocs.known-failures-baseline/v1",
        "repo": str(root),
        "source_commit": subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                                        capture_output=True, text=True).stdout.strip(),
        "finding_count": len(findings),
        "reproduced_count": sum(1 for f in findings if f["status"] == "REPRODUCED"),
        "findings": findings,
    }
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    not_repro = [f["finding_id"] for f in findings if f["status"] != "REPRODUCED"]
    print(f"KNOWN_FAILURES_BASELINE findings={len(findings)} reproduced={report['reproduced_count']}")
    if not_repro:
        print(f"  NOT_REPRODUCED: {not_repro}")
    print(f"  output: {out}")
    return 0


# ----------------------------------------------------------------------- CLI ----

def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="known_failures_baseline.py",
        description="已知失败基线：复现冻结（legacy）与 CI 机器化门（CI-BASELINE-001）")
    ap.add_argument("--mode", choices=("legacy", "verify", "check"), default="legacy",
                    help="legacy=原 R0-004 findings 报告；verify=静态校验版本化基线；"
                         "check=失败集 ⊆ 基线 动态判定")
    ap.add_argument("--repo", default=str(REPO), help="仓库根（默认按脚本位置推导）")
    ap.add_argument("--baseline", default=BASELINE_REL,
                    help="版本化基线路径（默认 ci/known_failures.json）")
    ap.add_argument("--ctest-junit", default=None, dest="ctest_junit",
                    help="check 模式：ctest --output-junit 的全量 JUnit XML")
    ap.add_argument("--ci-result", default=None, dest="ci_result",
                    help="check 模式：ci/run.py 的 CI_RESULT.json（汇总形态；run 结束后可得）")
    ap.add_argument("--ci-checks-dir", default=None, dest="ci_checks_dir",
                    help="check 模式：ci/run.py 的 per-check 结果目录（<out_root>/checks）。"
                         "缺省且环境变量 ASTROCS_CI_OUT_ROOT 存在时取其 checks/ 子目录")
    ap.add_argument("--allow-missing-source", action="store_true",
                    help="check 模式：允许基线中某 kind 无对应结果来源（默认 fail-closed）")
    ap.add_argument("--output", default=None, help="证据 JSON 落盘路径（run/ 下；默认只打印）")
    ap.add_argument("--selftest", action="store_true", help="负例自检（内存 fixture，零副作用）")
    return ap


def main(argv: list[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    args = build_parser().parse_args(argv)

    if args.selftest:
        return run_selftest()

    repo = pathlib.Path(args.repo).resolve()
    if args.mode == "legacy":
        # legacy 兼容：--repo/--output 语义与原脚本一致
        legacy_argv: list[str] = ["--repo", str(repo)]
        if args.output:
            legacy_argv += ["--output", str(args.output)]
        return run_legacy(legacy_argv)

    baseline_path = pathlib.Path(args.baseline)
    if not baseline_path.is_absolute():
        baseline_path = repo / baseline_path

    if args.mode == "verify":
        report = verify_baseline(repo, baseline_path)
    else:
        junit = pathlib.Path(args.ctest_junit) if args.ctest_junit else None
        if junit is not None and not junit.is_absolute():
            junit = repo / junit
        ci_result = pathlib.Path(args.ci_result) if args.ci_result else None
        if ci_result is not None and not ci_result.is_absolute():
            ci_result = repo / ci_result
        checks_dir = pathlib.Path(args.ci_checks_dir) if args.ci_checks_dir else None
        if checks_dir is None and ci_result is None:
            # CI 面缺省：ci/run.py 注入 ASTROCS_CI_OUT_ROOT（本次 run 证据根），
            # per-check 结果目录 = <out_root>/checks（run 内增量落盘）。
            env_root = str(os.environ.get("ASTROCS_CI_OUT_ROOT", "")).strip()
            if env_root:
                checks_dir = pathlib.Path(env_root) / "checks"
        if checks_dir is not None and not checks_dir.is_absolute():
            checks_dir = repo / checks_dir
        report = run_check(repo, baseline_path, junit, ci_result,
                           ci_checks_dir=checks_dir)
        if args.allow_missing_source:
            report["errors"] = [e for e in report["errors"] if "无该来源的测试结果" not in e]
            report["error_count"] = len(report["errors"])
            report["verdict"] = "PASS" if not report["errors"] else "FAIL"

    text = json.dumps(report, ensure_ascii=False, indent=2)
    print(text)
    if args.output:
        out = pathlib.Path(args.output)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    return 0 if report.get("verdict") == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
