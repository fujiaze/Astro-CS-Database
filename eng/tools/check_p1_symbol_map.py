#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""P1-001: old-symbol 映射完整性校验（**已退役保留，非门禁**；同族口径见
eng/tools/check_p2_symbol_map.py）。

根因（原实现为何死掉）
  原 MAP = docs/refactor/P1_SYMBOL_MAP.md —— 该路径已失效：
    - docs/refactor/ 整目录已被 GOV-002（b7b2dea7「docs(governance): GOV-002 归档非当前
      工程文档」）删除；本基线 `git log --oneline --diff-filter=D -- docs/refactor/`
      只有这一条。
  后果：退役前的实现在**模块级**无条件 read_text，任何 import/直跑都在模块执行期抛裸
        FileNotFoundError，判据主体（规则 1–4）永不到达；且本脚本从未在 eng/ci/checks.json
        注册（该注册表对本文件零引用），映射面长期无门。

处置口径：**不要把 MAP_REL 改成新路径。**
  把一个已失效的门改写成新路径，等于把失效伪装成有效 —— 这比留着失效更危险，因为它会让
  门禁报告「通过」。判据原文 run/FINAL-07/lead-01/self_check_after_migration.md:97-107
  （S7 失效门禁不得被伪装成有效门），其中 :101 对本文件逐字点名：读
  `docs/refactor/P1_SYMBOL_MAP.md`，该目录已不存在 ⇒ **仍标注失效**。
  同族对照：eng/tools/check_p2_symbol_map.py:5-10 逐字写着「原 MAP = docs/refactor/
  P2_SYMBOL_MAP.md，SRC = lib/phase2/src —— 两条路径均已失效」。同族两条门应当长得一样。
  故本文件的 MAP_REL **故意保留失效路径字面量**，由 R1 锚存活判据具名点名，而不是被悄悄
  换成一个能读通的新路径。谁想「顺手修好」，先看这一段。

判据（任一 R 违规 ⇒ exit 1；输入缺失 ⇒ exit 2，fail-closed）
  R1 anchor_alive     MAP_REL / HDR_REL / INDEX_REL 三条硬编码路径必须存在，失效时报
                      ANCHOR_STALE: <常量名> <路径> 并点名（不 traceback、不静默）。
  R2 api_mapped       p1_session.h 每个导出 API 必须在映射表中被映射。
  R3 sci_registered   映射表中每个 SCI-* 必须登记于 docs/contracts/INDEX.yaml。
  R4 module_id_ok     映射表中每个 module-ID 须符合 `astrocs.phase1.<snake_case>` 命名。
  R5 blockers_zero    映射表须登记「未映射旧能力: **0**」；未映射旧能力 = blocker。
  （R2–R5 即退役前的原判据，逐条未改，保留在 legacy_check() 内。）

不覆盖（如实声明）
  * 映射表本体已随 GOV-002 删除，且**内容未迁移到别处**（本轮落位表
    run/FINAL-07/doc-migration/s3_placement_v5.csv 共 277 行，其中 P1_SYMBOL_MAP /
    SYMBOL_MAP / docs/refactor 命中均为 0 次）⇒ 「映射表内容是否仍正确」本门**不再保证**。
    引用该映射表的其他文档也只写「已删，见 git 历史」（lib/phase1_session/README.md:14、
    :205、:266），无任何一处声称内容已迁至新落点。
  * R4 的正则与其自身的 `re.match` 同源，结构上恒真（退役前即如此）——本单按「判据未改」
    原样保留，不在这里偷偷放松或收紧。
  * 现行 phase1 模块映射由在册门 CHK-MODULE-MANIFEST（eng/tools/quality/check_module_map.py）
    承担；本门不承担该面。
  * 登记面冲突（如实记录，本单不裁决）：docs/engineering/01_CHECKS.md:213-214 仍把本文件
    列为「不是检查器（无判据、无退出码语义）」，与本文件本体的判据 + 退出码语义不符。

保留契约（docs/engineering/01_CHECKS.md §2.1「检查器保留项与预留项」:191-192）
  * 无参调用：打印保留标识并 exit 2；
  * 原实现保留在 legacy_check()，经 --legacy-check 复跑（判据未改，能红能绿）；
  * 本脚本默认不写任何产物文件（不落仓库根）。
  注：同族多个工具的 docstring 写的是 §2.1「检查器**退役**与预留」；该小节在现行
  docs/engineering/01_CHECKS.md:189 的标题已是「检查器**保留**项与预留项」，本文件按现行
  标题书写。锚存活口径同 §1:14-15「traceback 与静默降级一律按失败处理」。

用法
  python3 eng/tools/check_p1_symbol_map.py --legacy-check [--root DIR]
  python3 eng/tools/check_p1_symbol_map.py --self-test
退出码：0 = --legacy-check 判据通过（判据对象已删 ⇒ 真仓内实际不可达）；1 = 判据违规/锚失效；
        2 = 已退役保留（无参调用）/ 输入不可用（fail-closed）；3 = 用法错误。
可证伪性：默认（无参）调用**恒为非零**。--self-test 的 PASS 只描述「本工具的契约行为
        符合预期」，**不代表 P1-001 通过**，也不代表映射面已闭合。

只读；仅 stdlib；输出稳定排序（无时间戳）。
"""
from __future__ import annotations

import argparse
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# ── 判据锚（docs/engineering/01_CHECKS.md §1:14 锚存活） ────────────────────────────
# MAP_REL 是**已失效的路径**，故意保持字面量不变；见文件头「处置口径」。
MAP_REL = "docs/refactor/P1_SYMBOL_MAP.md"
HDR_REL = "lib/phase1_session/p1_session.h"
INDEX_REL = "docs/contracts/INDEX.yaml"

BLOCKER_MARK = "未映射旧能力: **0**"
_API_RE = re.compile(r"acs_status\s+(p1_session_\w+)\s*\(")
_SCI_RE = re.compile(r"\b(SCI-[A-Z0-9-]+)\b")
_MOD_RE = re.compile(r"`astrocs\.phase1\.[a-z_]+`")

RETIREMENT_MARKER = (
    "RETIRED: check_p1_symbol_map.py（P1-001）未注册进 eng/ci/checks.json；"
    "判据对象 docs/refactor/P1_SYMBOL_MAP.md 已随 GOV-002（b7b2dea7）归档清理删除，"
    "内容未迁移到本轮落位表；不要把该路径改成新路径（S7：失效门禁不得被伪装成有效门）；"
    "复跑原判据: --legacy-check"
)


def read_text(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def check_root(root):
    """R1 锚存活 + R2–R5 原判据（退役前逐条未改）。返回 (verdict, errors, counts)。"""
    root = os.path.abspath(root)
    errors = []
    counts = {"apis": 0, "scis": 0, "mods": 0}

    # R1 锚存活（fail-closed，逐条点名，不 traceback）
    paths = (("MAP_REL", MAP_REL), ("HDR_REL", HDR_REL), ("INDEX_REL", INDEX_REL))
    for const, rel in paths:
        if not os.path.exists(os.path.join(root, rel)):
            errors.append(("R1_anchor_alive",
                           "ANCHOR_STALE: %s %s" % (const, rel)))
    if errors:
        return "FAIL", errors, counts

    map_text = read_text(os.path.join(root, MAP_REL))
    hdr_text = read_text(os.path.join(root, HDR_REL))
    index_text = read_text(os.path.join(root, INDEX_REL))

    # R2 p1_session.h 导出 API 全部被映射
    api_re = _API_RE.findall(hdr_text)
    counts["apis"] = len(api_re)
    for api in api_re:
        if api not in map_text:
            errors.append(("R2_api_mapped", "unmapped old symbol: %s" % api))

    # R3 映射表 SCI-* 登记
    sci_used = sorted(set(_SCI_RE.findall(map_text)))
    counts["scis"] = len(sci_used)
    for sci in sci_used:
        if sci not in index_text:
            errors.append(("R3_sci_registered",
                           "SCI not registered in INDEX.yaml: %s" % sci))

    # R4 映射表 module-ID 命名（正则与下方 re.match 同源，结构上恒真 —— 退役前即如此，未改）
    mods = _MOD_RE.findall(map_text)
    counts["mods"] = len(mods)
    for m in mods:
        if not re.match(r"`astrocs\.phase1\.[a-z_]+`", m):
            errors.append(("R4_module_id_ok", "bad module id: %s" % m))

    # R5 blocker 检查
    if BLOCKER_MARK not in map_text:
        errors.append(("R5_blockers_zero", "mapping doc claims nonzero unmapped blockers"))

    errors.sort(key=lambda e: (e[0], e[1]))
    return ("FAIL" if errors else "PASS"), errors, counts


def emit(verdict, errors, counts):
    if verdict != "PASS":
        by_code = {}
        for code, _ in errors:
            by_code[code] = by_code.get(code, 0) + 1
        print("P1-001_MAP_VIOLATION: %d findings (by_code=%s)"
              % (len(errors), ",".join("%s:%d" % kv for kv in sorted(by_code.items()))))
        for code, detail in errors:
            print("  [%s] %s" % (code, detail))
        return 1
    print("P1-001_PASS: %d old APIs mapped, %d SCI registered, %d modules, blockers=0"
          % (counts["apis"], counts["scis"], counts["mods"]))
    return 0


def legacy_check(root):
    """退役前的原判定实现（判据未改）。"""
    if not os.path.isdir(root):
        print("P1-001_MAP_VIOLATION: ANCHOR_STALE: --root %s" % root)
        return 2
    verdict, errors, counts = check_root(root)
    return emit(verdict, errors, counts)


# ------------------------------------------------------------------ fixtures
BT = chr(96)
_HDR = ("#pragma once\n"
        "acs_status p1_session_init(phase1_session_t* s);\n"
        "acs_status p1_session_step(phase1_session_t* s);\n"
        "acs_status p1_session_finish(phase1_session_t* s);\n")
_FIX_MAP = ("# P1-001 映射表\n\n"
            "| old symbol | new module | SCI |\n|---|---|---|\n"
            "| " + BT + "p1_session_init" + BT + " | " + BT + "astrocs.phase1.session" + BT
            + " | SCI-P1-001 |\n"
            "| " + BT + "p1_session_step" + BT + " | " + BT + "astrocs.phase1.session" + BT
            + " | SCI-P1-001 |\n"
            "| " + BT + "p1_session_finish" + BT + " | " + BT + "astrocs.phase1.session" + BT
            + " | SCI-P1-001 |\n\n未映射旧能力: **0**\n")
_FIX_INDEX = "SCI-P1-001: phase1 session\n"


def _write(root, rel, text):
    p = os.path.join(root, rel)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as fh:
        fh.write(text)


def _mk_fixture():
    root = tempfile.mkdtemp(prefix="p1map_selftest_")
    _write(root, MAP_REL, _FIX_MAP)
    _write(root, HDR_REL, _HDR)
    _write(root, INDEX_REL, _FIX_INDEX)
    return root


def legacy_self_test():
    """退役前的原夹具自检（判据未改；6 组：正例 1 + 负例 4 + 恢复 1）。"""
    fails = []
    checks = []

    def expect(name, root, want, code=None):
        verdict, errors, _ = check_root(root)
        codes = {e[0] for e in errors}
        ok = verdict == want and (code is None or code in codes)
        checks.append((name, verdict, sorted(codes), ok))
        if not ok:
            fails.append("%s: 期望 %s/%s 实得 %s/%s"
                         % (name, want, code, verdict, sorted(codes)))

    root = _mk_fixture()
    try:
        expect("N0 正例（映射闭合）", root, "PASS")
        _write(root, MAP_REL, _FIX_MAP.replace("p1_session_step", "p1_session_other"))
        expect("N1 导出 API 未映射", root, "FAIL", "R2_api_mapped")
        _write(root, MAP_REL, _FIX_MAP)
        _write(root, INDEX_REL, "SCI-OTHER-999: nothing\n")
        expect("N2 SCI 未登记", root, "FAIL", "R3_sci_registered")
        _write(root, INDEX_REL, _FIX_INDEX)
        _write(root, MAP_REL, _FIX_MAP.replace(BLOCKER_MARK, "未映射旧能力: **2**"))
        expect("N3 映射表登记非零 blocker", root, "FAIL", "R5_blockers_zero")
        _write(root, MAP_REL, _FIX_MAP)
        expect("N3' 恢复后回绿", root, "PASS")
        os.remove(os.path.join(root, MAP_REL))
        expect("N4 MAP 锚失效（具名点名）", root, "FAIL", "R1_anchor_alive")
    finally:
        shutil.rmtree(root, ignore_errors=True)

    for name, verdict, codes, ok in checks:
        print("  %-32s verdict=%-4s codes=%-34s %s"
              % (name, verdict, ",".join(codes) or "-", "OK" if ok else "**FAIL**"))
    if fails:
        print("P1_SYMBOL_MAP_SELFTEST_FAIL:")
        for f in fails:
            print("  " + f)
        return 1
    print("P1_SYMBOL_MAP_SELFTEST_PASS: %d 组（正例 1 + 负例 4 + 恢复 1）全部符合预期" % len(checks))
    return 0


def self_test():
    """可执行正/负例面：保留契约（§2.1:191-192）+ 原判据的 6 组夹具自检。"""
    import subprocess

    problems = []
    cases = 0

    def check_case(name, rc, want_rc, blob, token):
        nonlocal cases
        cases += 1
        ok = rc == want_rc and token in blob
        print("  SELFTEST_%s %-34s rc=%d want_rc=%d token=%r"
              % ("PASS" if ok else "FAIL", name, rc, want_rc, token))
        if not ok:
            problems.append("%s: rc=%d(want %d) token=%r missing" % (name, rc, want_rc, token))

    me = os.path.abspath(__file__)

    def run_cli(extra):
        return subprocess.run([sys.executable, me] + extra,
                              capture_output=True, text=True, timeout=300)

    # P0：默认无参调用 ⇒ 打印保留标识并 exit 2（可证伪性：绝不为 0）
    r = run_cli([])
    blob = r.stdout + r.stderr
    check_case("P0_retired_noarg", r.returncode, 2, blob, "RETIRED")
    if "ANCHOR_STALE: MAP_REL" in blob:
        problems.append("P0_retired_noarg: 无参调用不应报锚失效（那是 legacy 的事）")
        print("  SELFTEST_FAIL P0_retired_noarg_should_not_probe_anchor")

    # N1：真仓 --legacy-check ⇒ 锚失效具名点名 + 非零（不 traceback）
    r = run_cli(["--legacy-check", "--root", REPO])
    blob = r.stdout + r.stderr
    check_case("N1_legacy_anchor_stale_named", r.returncode, 1, blob,
               "ANCHOR_STALE: MAP_REL")
    if "Traceback" in blob:
        problems.append("N1: 出现 traceback，违反 §1:14「traceback 一律按失败处理」")
        print("  SELFTEST_FAIL N1_traceback_present")

    # N2：--root 不存在 ⇒ rc=2
    r = run_cli(["--legacy-check", "--root", os.path.join(REPO, "no_such_root")])
    check_case("N2_root_missing_red", r.returncode, 2, r.stdout + r.stderr,
               "ANCHOR_STALE: --root")

    # 原判据的 6 组夹具自检（判据未改）
    cases += 1
    rc = legacy_self_test()
    if rc != 0:
        problems.append("legacy_self_test rc=%d" % rc)
    print("  SELFTEST_%s %-34s rc=%d"
          % ("PASS" if rc == 0 else "FAIL", "P3_legacy_fixtures", rc))

    print("SELF_TEST %s cases=%d problems=%d"
          % ("PASS" if not problems else "FAIL", cases, len(problems)))
    for p in problems:
        print("  - " + p)
    return 0 if not problems else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="P1-001 old-symbol 映射完整性校验（已退役保留）")
    ap.add_argument("--legacy-check", action="store_true", dest="legacy_check",
                    help="复跑退役前的原判定实现（判据未改）")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    if not args.legacy_check:
        # §2.1:191-192：无参调用打印保留标识并 exit 2
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原判据: --legacy-check [--root DIR]", file=sys.stderr)
        return 2
    return legacy_check(args.root)


if __name__ == "__main__":
    try:
        sys.exit(main())
    except SystemExit:
        raise
    except Exception as exc:  # noqa: BLE001
        print("TOOLING_FAILURE: %r" % (exc,), file=sys.stderr)
        sys.exit(3)
