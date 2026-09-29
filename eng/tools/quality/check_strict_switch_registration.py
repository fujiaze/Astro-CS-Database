#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""STRICT-SWITCH-REG-001 判据「最严口径开关」登记门（第一步：字段可选，但有该类开关者必声明）

依据（负责人裁决一，分两步走的**第一步**）：
  凡判据脚本声明了 strict / verbose / 全量(full) / --from- 前缀类开关，
  其登记项就**必须**声明最严口径开关名（字段 strict_switches，**本步为可选字段**），
  且**登记命令必须真的传它**；缺声明、或声明了但没传 ⇒ 判红。
  第二步（后续）再评估是否把该字段转必填。本门不预设转必填的时点。

设计要点：
  * 字段是可选的，但对「有该类开关的脚本」**等效必填** —— 这正是本步的力度。
  * 双向核对：声明的开关必须是脚本真有的（防拼写漂移），且必须出现在命令里（防只声明不传）。
  * fail-closed：扫描面为空 / 注册表不可用 ⇒ exit 2。
  * 零 git 写、只读。

已知前置依赖（**本门不代做，须前台串行**）：
  eng/ci/checks.schema.json 的 check/step items 均为 additionalProperties:false，
  故新增 strict_switches 字段必须**先扩该 schema**，否则 validate_registry.py 会判红。
  两个文件都属登记面，由前台与本门同批入库。

用法：
  python3 check_strict_switch_registration.py [--root <dir>] [--json-out <f>] [--report-only]
  python3 check_strict_switch_registration.py --self-test
退出码：0 通过；1 判红；2 输入不可用/门自身错误。
--report-only 只打印不改判（用于入库前普查，不阻塞 CI）。
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

REL_REGISTRY = "eng/ci/checks.json"
REL_SCHEMA = "eng/ci/checks.schema.json"
SCRIPTISH = re.compile(r"^[\w./+-]+\.(py|sh|ps1|bat)$")
INREPO_PREFIX = ("eng/", "lib/", "scripts/", "tools/", "docs/")
# 纪律点名的四类开关
STRICT_RE = re.compile(r"^--(strict|strict[a-z0-9-]*|verbose|verbose[a-z0-9-]*"
                       r"|full[a-z0-9-]*|from-[a-z0-9-]+)$")
OPT_RE = re.compile(r"add_argument\(\s*[\"'](--[a-z0-9][\w-]*)[\"']")
QUOTE_RE = re.compile(r"[\"'](--[a-z][\w-]{2,})[\"']")
FIELD = "strict_switches"
NOTE_FIELD = "strict_switch_note"

# 派发器自身（不是判据）：其开关是「选谁跑 / 跑多全」，不是该判据的口径。
# **排除必须可见**：本门把被排除的脚本打进 notes，不静默跳过。
DISPATCHERS = {
    "eng/ci/run_checks.py": "CI 判据派发器（--full 是选执行面，不是判据口径）",
    "eng/ci/run.py": "CI 唯一入口（--profile 选档，不是判据口径）",
}


def _fail_closed(msg: str):
    print("[fail-closed] %s" % msg, file=sys.stderr)
    raise SystemExit(2)


def cmds_of(entry):
    yield entry.get("command") or []
    for s in entry.get("steps") or []:
        yield s.get("command") or []


def scripts_referenced(entry):
    for cmd in cmds_of(entry):
        for tok in cmd or []:
            if (not tok.startswith("-") and SCRIPTISH.match(tok)
                    and tok.startswith(INREPO_PREFIX)):
                yield tok


def strict_switches_of(root: pathlib.Path, script_rel: str):
    p = root / script_rel
    if not p.is_file():
        return None            # 脚本不存在 => 归 INV-REG-001，本门不重复判红
    try:
        txt = p.read_text(encoding="utf-8", errors="ignore")
    except Exception:          # noqa: BLE001
        return None
    names = set(OPT_RE.findall(txt)) | set(QUOTE_RE.findall(txt))
    return sorted(n for n in names if STRICT_RE.match(n))


def check(root: pathlib.Path, report_only: bool = False):
    reg = root / REL_REGISTRY
    if not reg.is_file():
        _fail_closed("缺注册表：%s" % reg)
    try:
        data = json.loads(reg.read_text(encoding="utf-8"))
    except Exception as e:     # noqa: BLE001
        _fail_closed("注册表不可解析：%s" % e)
    checks = data.get("checks")
    if not isinstance(checks, list) or not checks:
        _fail_closed("注册表 checks 为空或缺失")

    notes, errors, scanned_scripts = [], [], set()
    excluded_hit = {}
    for c in checks:
        cid = c.get("id", "<无 id>")
        per_script = {}
        for s in scripts_referenced(c):
            if s in per_script:
                continue
            if not s.endswith(".py"):
                continue
            if s in DISPATCHERS:
                excluded_hit[s] = excluded_hit.get(s, 0) + 1
                continue
            ss = strict_switches_of(root, s)
            if ss is None:
                continue
            scanned_scripts.add(s)
            if ss:
                per_script[s] = ss
        if not per_script:
            continue
        declared = c.get(FIELD)
        passed = set()
        for cmd in cmds_of(c):
            passed |= {t for t in cmd if isinstance(t, str) and t.startswith("--")}
        for s, ss in sorted(per_script.items()):
            if declared is None:
                errors.append("%s 脚本 %s 声明了最严口径开关 %s，但登记项**没有** %s 字段"
                              % (cid, s, ss, FIELD))
                continue
            if not isinstance(declared, list):
                errors.append("%s 的 %s 必须是数组" % (cid, FIELD))
                continue
            if not declared:
                # 显式声明「本脚本没有最严口径开关（默认即最严）」是一等答案，
                # 但必须附理由字段，否则等于没答。**禁止空数组蒙混。**
                if not str(c.get(NOTE_FIELD) or "").strip():
                    errors.append("%s 的 %s 为空且未给 %s 理由；"
                                  "「默认即最严」是可接受的答案，但必须写清理由"
                                  % (cid, FIELD, NOTE_FIELD))
                continue
            unknown = [x for x in declared if x not in ss]
            if unknown:
                errors.append("%s 的 %s 声明了脚本 %s **没有**的开关 %s（拼写漂移）"
                              % (cid, FIELD, s, unknown))
            missing = [x for x in ss if x not in declared]
            if missing:
                errors.append("%s 的 %s 未覆盖脚本 %s 的最严口径开关 %s"
                              % (cid, FIELD, s, missing))
            notpassed = [x for x in declared if x not in passed]
            if notpassed:
                errors.append("%s 声明了 %s=%s，但登记命令**没有传**"
                              % (cid, FIELD, notpassed))

    if not scanned_scripts:
        _fail_closed("未扫到任何判据脚本（拒绝把空扫描面当通过）")
    notes.append("登记判据 %d 条；扫到含 strict/verbose/全量/--from- 开关的脚本 %d 个"
                 % (len(checks), len(scanned_scripts)))
    if excluded_hit:
        notes.append("已排除派发器（非判据，排除可见）：%s"
                     % "; ".join("%s 被 %d 条判据引用 —— %s"
                                 % (k, v, DISPATCHERS[k]) for k, v in sorted(excluded_hit.items())))
    if report_only:
        notes.append("report-only：判红 %d 条，仅报告不判红" % len(errors))
        return errors, notes
    return errors, notes


# ── 自测面：临时目录 fixture + 假 git 不需要（本门不查跟踪集）────────────────
def self_test() -> int:
    print("STRICT-SWITCH-REG-001 self-test")
    reg_plain = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-OK", "command": ["python3", "eng/ci/a.py", "--strict"],
         "strict_switches": ["--strict"]}]})
    reg_nodecl = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-NODECL", "command": ["python3", "eng/ci/a.py", "--strict"]}]})
    reg_empty = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-EMPTY", "command": ["python3", "eng/ci/a.py"],
         "strict_switches": []}]})
    reg_nopass = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-NOPASS", "command": ["python3", "eng/ci/a.py"],
         "strict_switches": ["--strict"]}]})
    reg_typo = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-TYPO", "command": ["python3", "eng/ci/a.py", "--stirct"],
         "strict_switches": ["--stirct"]}]})
    reg_noswitch = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-NOSW", "command": ["python3", "eng/ci/b.py"]}]})
    reg_empty_ok = json.dumps({"schema_version": 1, "checks": [
        {"id": "T-EMPTYOK", "command": ["python3", "eng/ci/a.py"],
         "strict_switches": [],
         "strict_switch_note": "脚本唯一同类开关 --from-x 是输入选择器，默认即最严"}]})
    script_a = 'ap.add_argument("--strict", action="store_true")\n'
    script_b = 'ap.add_argument("--json-out", default=None)\n'

    def case(name, reg, files, expect_rc):
        with tempfile.TemporaryDirectory(prefix="ssreg_") as td:
            root = pathlib.Path(td)
            (root / REL_REGISTRY).parent.mkdir(parents=True, exist_ok=True)
            (root / REL_REGISTRY).write_text(reg, encoding="utf-8")
            for rel, content in files.items():
                p = root / rel
                p.parent.mkdir(parents=True, exist_ok=True)
                p.write_text(content, encoding="utf-8")
            try:
                errors, notes = check(root)
                rc = 1 if errors else 0
                detail = errors[:1]
            except SystemExit as e:
                rc, detail = int(e.code or 2), []
        ok = rc == expect_rc
        print("  %-44s expect=%d got=%d %s %s"
              % (name, expect_rc, rc, "PASS" if ok else "FAIL",
                 ("| " + detail[0][:60]) if detail else ""))
        return ok

    bad = 0
    if not case("N0 正例：声明+都传了", reg_plain, {"eng/ci/a.py": script_a}, 0):
        bad += 1
    if not case("N1 负例：有开关但未声明字段", reg_nodecl, {"eng/ci/a.py": script_a}, 1):
        bad += 1
    if not case("N2 负例：字段声明为空", reg_empty, {"eng/ci/a.py": script_a}, 1):
        bad += 1
    if not case("N3 负例：声明了但命令没传", reg_nopass, {"eng/ci/a.py": script_a}, 1):
        bad += 1
    if not case("N4 负例：声明了脚本没有的开关", reg_typo, {"eng/ci/a.py": script_a}, 1):
        bad += 1
    if not case("N5 正例：脚本无该类开关（不需字段）", reg_noswitch,
                {"eng/ci/b.py": script_b}, 0):
        bad += 1
    if not case("N6 正例：声明为空但给了理由（默认即最严）", reg_empty_ok,
                {"eng/ci/a.py": 'ap.add_argument("--from-src", default=None)\n'}, 0):
        bad += 1
    print("SELFTEST_%s" % ("FAIL" if bad else "PASS"))
    return 1 if bad else 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="判据最严口径开关登记门")
    ap.add_argument("--root", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--json-out")
    ap.add_argument("--report-only", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    root = pathlib.Path(a.root)
    errors, notes = check(root, report_only=a.report_only)
    if a.json_out:
        out = pathlib.Path(a.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"check": "STRICT-SWITCH-REG-001", "root": str(root),
                                   "notes": notes, "errors": errors,
                                   "pass": not errors, "report_only": a.report_only},
                                  ensure_ascii=False, indent=1), encoding="utf-8")
    for n in notes:
        print("[ok] %s" % n)
    if errors:
        for e in errors:
            print("[REPORT] %s" % e if a.report_only else ("[FAIL] %s" % e), file=sys.stderr)
        print("STRICT_SWITCH_REG_%s errors=%d"
              % ("REPORT" if a.report_only else "FAIL", len(errors)))
        return 0 if a.report_only else 1
    print("STRICT_SWITCH_REG_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
