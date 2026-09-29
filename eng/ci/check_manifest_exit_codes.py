#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-MANIFEST-EXIT-CODES：manifest 面「合同行 → 实现行」退出码一致门（P-159 定一）。

依据（唯一权威链）:
  · docs/api/MANIFEST_VERIFY_V1.md §3 —— manifest verify 的**校验序 → 错误码**合同行
    （语法/schema=3 → status=="complete"(否则 8) → astrocs_version 一致(5) →
     重算 config/profile hash(3) → 逐 artifact 存在性(3)+sha256(8)+size(8) → 全过=0）；
  · docs/contracts/LOG_AND_ERROR_CONTRACT.md §5 —— 域→码表（IO→7 / INTEGRITY→8）；
  · lib/infrastructure/cli/exit_codes.h —— 码值语义唯一源（本门校验符号=数值绑定）。
  · 观测面定一条款（P-159「8 vs 7 定一」）：docs/plugins/infrastructure/21_observability.md
    与 docs/design/LOG_AND_ERROR_SYSTEM.md 必须写入**逐字相同**的
    「登记/写入失败 = exit 7（IO）；verify/完整性失败 = exit 8（INTEGRITY）」条款。

判据（fail-closed，锚点缺失即 FAIL，不静默通过）:
  C1 exit_codes.h 的**全部 11 个**退出码：符号=数值绑定逐个核对
     （OK=0/ARGS=2/INPUT=3/SCIENCE=4/BACKEND=5/COMPUTE=6/IO=7/INTEGRITY=8/
       CANCELLED=9/RESOURCE=10/INTERNAL=70）——**全覆盖，缺一即判红**；
     并附三条**不依赖本表副本**的结构判据（防「加码 / 删码 / 同值异名」绕过逐项核对）：
       C1a 枚举项数 = 11（少一项 = 删码，多一项 = 发明新码 → 判红）；
       C1b 11 个数值**两两互异**（把某码数值复制给另一符号 = 同值异名 → 判红）；
       C1c 头文件自述「11 个退出码」与实际枚举项数一致（自述漂移 → 判红）。
     数值本身属冻结面：本门**只核对既有绑定，不新增码、不改任何码值**。
  C2 commands.cpp 的 manifest verify（cmd_verify）里，每条 stderr 诊断锚之后**紧随**的
     return 必须是合同行规定的符号（例如 status!=complete 的锚之后必须是 INTEGRITY）；
  C3 MANIFEST_VERIFY_V1.md §3 含逐字合同行（顺序与码值同时被锁）；
  C4 两篇文档的 P-159 条款**逐字相同**且两码分立（7=登记/写入、8=verify/完整性）。

用法:
  python3 eng/ci/check_manifest_exit_codes.py [--root <dir>] [--json-out <file>] [--quiet]
  python3 eng/ci/check_manifest_exit_codes.py --self-test
  python3 eng/ci/check_manifest_exit_codes.py --fault-inject <name>   # 真仓只读注入 → 必须判红
exit 0 = 一致；exit 1 = 不一致（判红）；exit 2 = 输入不可用（fail-closed）。
本脚本只读，不写任何仓库文件（--json-out 除外；fixture/注入一律落在临时目录）。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent

REL = {
    "cmd": "lib/infrastructure/cli/commands.cpp",
    "exit": "lib/infrastructure/cli/exit_codes.h",
    "docv": "docs/api/MANIFEST_VERIFY_V1.md",
    "doca": "docs/plugins/infrastructure/21_observability.md",
    "docb": "docs/design/LOG_AND_ERROR_SYSTEM.md",
}

# 合同行（docs/api/MANIFEST_VERIFY_V1.md §3）逐字锚：顺序 + 码值一并锁定。
SEQ_ANCHOR = ("语法/schema(3)→status==\"complete\"(否则 8)→astrocs_version 与本机一致"
              "(5, 版本不同不可 verify)→重算 config/profile hash(3, 输入已变)→"
              "逐 artifact 存在性(3)+sha256(8)+size(8)→全部过→0")

# 代码锚：stderr 诊断串 → 该分支必须返回的符号（合同行 C2）。
# 顺序即文档 §3 校验序；同一分支多条诊断的取首条。
CODE_ANCHORS = [
    ("acsd: manifest malformed JSON", "INPUT"),
    ("acsd: not a v1 astrocs_run_manifest document", "INPUT"),
    ("acsd: run manifest status=", "INTEGRITY"),
    ("acsd: manifest was produced by version", "BACKEND"),
    ("acsd: phase3 manifest declares no phase3_output artifact", "INTEGRITY"),
    ("acsd: config input no longer readable", "INPUT"),
    ("acsd: config changed since the run", "INPUT"),
    ("acsd: artifact missing", "INPUT"),
    ("acsd: artifact sha256 mismatch", "INTEGRITY"),
    ("acsd: artifact size mismatch", "INTEGRITY"),
]
OK_ANCHOR = 'nlohmann::json out = {{"verify", "ok"}'

# 冻结的 11 码「符号→数值」绑定表（数值属冻结面；本表只用于**核对**，不得改数值）。
# 2026-09-XX 覆盖面修复：本表此前只有 6 项（OK/ARGS/INPUT/BACKEND/IO/INTEGRITY），
#   SCIENCE/COMPUTE/CANCELLED/RESOURCE/INTERNAL 五码无任何门守护 ⇒ 可静默漂移。
#   现补齐为全部 11 项；并加 C1a/C1b/C1c 三条结构判据，使「加码 / 删码 / 同值异名」
#   即使绕过本表副本也被抓住（不依赖本表是否同步更新）。
EXIT_VALUES = {
    "OK": 0, "ARGS": 2, "INPUT": 3, "SCIENCE": 4, "BACKEND": 5, "COMPUTE": 6,
    "IO": 7, "INTEGRITY": 8, "CANCELLED": 9, "RESOURCE": 10, "INTERNAL": 70,
}
EXIT_COUNT = len(EXIT_VALUES)          # = 11，冻结条数
# 头文件自述的条数（用于 C1c：自述与实际项数必须一致）
EXIT_SELF_DECLARED_RE = re.compile(r"本文件是\s*(\d+)\s*个退出码")
# 头文件 enum 枚举项：SYM = NUM,  （只认顶格枚举行；注释行不含「= 数字,」形态）
ENUM_ITEM_RE = re.compile(r"^\s*([A-Z_][A-Z0-9_]*)\s*=\s*(\d+)\s*,", re.M)

CLAUSE_MARK = "manifest 面两码分立（P-159 定一"
CLAUSE_END = "（IO→7 / INTEGRITY→8）。"
CLAUSE_MUST_HAVE = ["= exit 7（IO）", "= exit 8", "MANIFEST_VERIFY_V1.md", "LOG_AND_ERROR_CONTRACT.md"]

RET_RE = re.compile(r"return\s+astrocs::([A-Z_]+)\s*;")


def _read(root: pathlib.Path, key: str) -> str:
    p = root / REL[key]
    if not p.is_file():
        raise SystemExit("[fail-closed] 缺输入文件: %s" % p)
    return p.read_text(encoding="utf-8", errors="ignore")


def _clause(text: str, src: str, fails: list) -> str:
    i = text.find(CLAUSE_MARK)
    if i < 0:
        fails.append("C4 %s: 未找到 P-159 条款锚「%s」" % (src, CLAUSE_MARK))
        return ""
    j = text.find(CLAUSE_END, i)
    if j < 0:
        fails.append("C4 %s: P-159 条款缺收尾锚「%s」" % (src, CLAUSE_END))
        return ""
    return text[i:j + len(CLAUSE_END)]


def check(root: pathlib.Path):
    fails: list = []
    notes: list = []
    ex = _read(root, "exit")
    for sym, val in sorted(EXIT_VALUES.items()):
        m = re.search(r"\b%s\s*=\s*(\d+)" % sym, ex)
        if not m:
            fails.append("C1 exit_codes.h 未声明 %s" % sym)
        elif int(m.group(1)) != val:
            fails.append("C1 exit_codes.h %s=%s，合同要求 %d" % (sym, m.group(1), val))
    notes.append("C1 exit_codes.h 符号-数值绑定 %d/%d 项（全 %d 码逐个核对）"
                 % (len(EXIT_VALUES), len(EXIT_VALUES), EXIT_COUNT))

    # ── C1a/C1b/C1c：结构判据（不依赖本表副本是否同步更新） ─────────────────
    enum_items = [(m.group(1), int(m.group(2))) for m in ENUM_ITEM_RE.finditer(ex)]
    enum_names = [s for s, _ in enum_items]
    if len(enum_items) != EXIT_COUNT:
        dup = sorted({s for s in enum_names if enum_names.count(s) > 1})
        fails.append("C1a exit_codes.h 枚举项 %d 项，要求恰好 %d 项%s"
                     % (len(enum_items), EXIT_COUNT,
                        "（重复符号: %s）" % ", ".join(dup) if dup else
                        "（少项=删码 / 多项=发明新码）"))
    else:
        notes.append("C1a exit_codes.h 枚举项 %d/%d 项（无删码 / 无新码）"
                     % (len(enum_items), EXIT_COUNT))
    byval = {}
    for s, v in enum_items:
        byval.setdefault(v, []).append(s)
    collide = {v: ns for v, ns in byval.items() if len(ns) > 1}
    if collide:
        for v in sorted(collide):
            fails.append("C1b exit_codes.h 数值 %d 被多个符号占用（同值异名/同值多名）: %s"
                         % (v, ", ".join(collide[v])))
    else:
        notes.append("C1b exit_codes.h 数值两两互异 %d/%d" % (len(byval), len(enum_items)))
    msd = EXIT_SELF_DECLARED_RE.search(ex)
    if not msd:
        fails.append("C1c exit_codes.h 缺自述锚「本文件是 N 个退出码」（自述不可核对 → fail-closed）")
    elif int(msd.group(1)) != len(enum_items):
        fails.append("C1c exit_codes.h 自述「%s 个退出码」与实际枚举项数 %d 不一致"
                     % (msd.group(1), len(enum_items)))
    else:
        notes.append("C1c 头文件自述条数与实际项数一致（%d）" % len(enum_items))

    cmd = _read(root, "cmd")
    hits = 0
    for msg, want in CODE_ANCHORS:
        k = cmd.find(msg)
        if k < 0:
            fails.append("C2 commands.cpp 缺诊断锚「%s」（合同行不可核对）" % msg)
            continue
        m = RET_RE.search(cmd, k)
        if not m:
            fails.append("C2 「%s」之后 %d 字符内无 return astrocs::…" % (msg, 4000))
            continue
        got = m.group(1)
        if got != want:
            fails.append("C2 「%s」→ 实现返回 %s，合同行要求 %s" % (msg, got, want))
        else:
            hits += 1
    k = cmd.find(OK_ANCHOR)
    if k < 0:
        fails.append("C2 commands.cpp 缺成功锚「%s」" % OK_ANCHOR)
    else:
        m = RET_RE.search(cmd, k)
        if not m or m.group(1) != "OK":
            fails.append("C2 成功路径未返回 astrocs::OK")
        else:
            hits += 1
    notes.append("C2 代码锚逐条符号一致 %d/%d" % (hits, len(CODE_ANCHORS) + 1))

    dv = _read(root, "docv")
    if SEQ_ANCHOR not in dv:
        fails.append("C3 MANIFEST_VERIFY_V1.md §3 缺逐字合同行（校验序/码值被改动或缺失）")
    else:
        notes.append("C3 合同行逐字在位")

    ca = _clause(_read(root, "doca"), "21_observability.md", fails)
    cb = _clause(_read(root, "docb"), "LOG_AND_ERROR_SYSTEM.md", fails)
    if ca and cb:
        na = re.sub(r"\s+", "", ca)
        nb = re.sub(r"\s+", "", cb)
        if na != nb:
            fails.append("C4 两文档 P-159 条款不逐字相同（定一被破坏）")
        else:
            for must in CLAUSE_MUST_HAVE:
                if must not in ca:
                    fails.append("C4 P-159 条款缺要件「%s」" % must)
            notes.append("C4 两文档条款逐字一致（%d 字符）" % len(na))
    return fails, notes


# ── 可执行正/负例面 ─────────────────────────────────────────────────────────
MUTANTS = {
    # 名称 -> (相对路径键, 旧串, 新串)：注入即必须判红
    "doc-clause-missing": ("doca", "manifest 面两码分立（P-159 定一", "manifest 面（措辞已删）"),
    "doc-clause-diverged": ("docb", "不匹配等）= exit 8", "不匹配等）= exit 7"),
    "code-status-to-input": ("cmd", "return astrocs::INPUT;\n", "return astrocs::INTEGRITY;\n"),
    "doc-seq-code-wrong": ("docv", "(否则 8)", "(否则 3)"),
    "exit-value-drift": ("exit", "INTEGRITY     = 8", "INTEGRITY     = 7"),
}

# 正则替换型注入（C1 覆盖面修复的鉴别力面）：对 **11 个码逐个**注入数值漂移。
# 修复前只有 6 个码有判据 ⇒ 这 11 例里有 5 例（SCIENCE/COMPUTE/CANCELLED/
# RESOURCE/INTERNAL）注入后 rc=0（零鉴别力）。修复后 11 例必须**全部**判红。
DRIFT_VALUE = 42
VALUE_DRIFT_SYMS = tuple(sorted(EXIT_VALUES))          # 11 个，逐个注入

# 结构型注入：加码 / 删码 / 同值异名 / 自述漂移 —— 不依赖本表副本即可被抓。
# kind: "add-dup" 追加新符号并复制 COMPUTE 的数值；"drop" 删整行；
#       "dup-value" 改成与既有码撞值；"self-declared" 改自述条数。
STRUCT_MUTANTS = {
    "exit-add-duplicate-code": ("add-dup", "COMPUTE", None),
    "exit-drop-code": ("drop", "SCIENCE", None),
    "exit-drop-code-cancelled": ("drop", "CANCELLED", None),
    "exit-drop-code-internal": ("drop", "INTERNAL", None),
    "exit-dup-value": ("dup-value", "COMPUTE", 7),
    "exit-self-declared-drift": ("self-declared", None, 12),
}

# 每条注入**必须由哪条判据**抓住（自检按判据代号断言，避免"抓住但抓错地方"）。
MUTANT_EXPECT = {
    "exit-add-duplicate-code": "C1a",     # 加码（复制 COMPUTE 数值）→ 项数判据
    "exit-drop-code": "C1",               # 删码 → 未声明
    "exit-drop-code-cancelled": "C1",
    "exit-drop-code-internal": "C1",
    "exit-dup-value": "C1b",             # 撞值（COMPUTE 改成 IO 的 7）→ 互异性判据
    "exit-self-declared-drift": "C1c",    # 自述漂移 → 自述判据
}


def expect_token(name: str) -> str | None:
    """该注入应由哪条判据抓住；None = 不指定。"""
    if name in MUTANT_EXPECT:
        return MUTANT_EXPECT[name]
    if name.startswith("exit-value-drift-"):
        return "C1 "
    return None


def _literal_mutate(text: str, mutation: str, keym: str, key: str) -> str:
    """字面替换型注入（MUTANTS 表）。只对 keym 命中的那个输入文件生效。"""
    if keym != key:
        return text
    _, old, new = MUTANTS[mutation]
    # code-status-to-input 用唯一锚（status!=complete 分支）定向替换
    if mutation == "code-status-to-input":
        anchor = "acsd: run manifest status="
        k = text.find(anchor)
        if k < 0:
            raise SystemExit("[fail-closed] 注入锚缺失: %s" % anchor)
        m = RET_RE.search(text, k)
        if not m:
            raise SystemExit("[fail-closed] 注入锚后无 return")
        return text[:m.start()] + "return astrocs::INPUT;" + text[m.end():]
    n = text.count(old)
    if n != 1:
        raise SystemExit("[fail-closed] 注入锚命中 %d 次（要求恰好 1）: %s"
                         % (n, old[:40]))
    return text.replace(old, new)


def _apply_mutation(text: str, key: str, mutation: str) -> str:
    """把一次注入施加到某个输入文件上（分派三种注入形态）。"""
    if mutation.startswith("exit-value-drift-"):
        if key == "exit":
            return _drift_exit_value(text, mutation[len("exit-value-drift-"):], DRIFT_VALUE)
        return text
    if mutation in STRUCT_MUTANTS:
        if key == "exit":
            return _struct_mutate_exit(text, mutation)
        return text
    return _literal_mutate(text, mutation, MUTANTS[mutation][0], key)


def _fixture(dst: pathlib.Path, mutation: str | None = None):
    for key, rel in REL.items():
        src = REPO / rel
        if not src.is_file():
            raise SystemExit("[fail-closed] 缺真仓输入: %s" % src)
        tgt = dst / rel
        tgt.parent.mkdir(parents=True, exist_ok=True)
        text = src.read_text(encoding="utf-8", errors="ignore")
        if mutation:
            text = _apply_mutation(text, key, mutation)
        tgt.write_text(text, encoding="utf-8")


def all_mutants():
    """全部注入名 = 字面替换型 + 结构型 + 11 例逐码数值漂移。"""
    names = list(MUTANTS) + list(STRUCT_MUTANTS)
    names += ["exit-value-drift-" + s for s in VALUE_DRIFT_SYMS]
    return names


def _drift_exit_value(text: str, sym: str, newval: int) -> str:
    """把 exit_codes.h 里符号 sym 的枚举值改成 newval（正则在临时夹具上跑，不碰真仓）。"""
    pat = re.compile(r"^(\s*%s\s*=\s*)\d+" % re.escape(sym), re.M)
    if not pat.search(text):
        raise SystemExit("[fail-closed] 数值漂移锚缺失: %s" % sym)
    return pat.sub(lambda m: m.group(1) + str(newval), text, count=1)


def _struct_mutate_exit(text: str, mutation: str) -> str:
    """结构型注入（加码 / 删码 / 撞值 / 自述漂移）。"""
    kind, arg, extra = STRUCT_MUTANTS[mutation]
    if kind == "add-dup":
        if "BRAND_NEW" in text:
            raise SystemExit("[fail-closed] BRAND_NEW 已存在")
        return text.replace(
            "    INTERNAL      = 70,",
            "    INTERNAL      = 70,\n    BRAND_NEW    = 6,   // 注入: 复制 COMPUTE 的数值",
            1)
    if kind == "drop":
        lines = text.split("\n")
        keep = [l for l in lines if not re.match(r"^\s*%s\s*=\s*\d+" % re.escape(arg), l)]
        if len(keep) == len(lines):
            raise SystemExit("[fail-closed] 删码锚缺失: %s" % arg)
        return "\n".join(keep)
    if kind == "dup-value":
        return _drift_exit_value(text, arg, extra)
    if kind == "self-declared":
        pat = EXIT_SELF_DECLARED_RE
        if not pat.search(text):
            raise SystemExit("[fail-closed] 自述锚缺失")
        return pat.sub(lambda m: "本文件是 %d 个退出码" % extra, text, count=1)
    raise SystemExit("[fail-closed] 未知结构注入 kind: %s" % kind)


def self_test() -> int:
    """正例 + 全部负例（含 11 例逐码数值漂移）必须给出逐例判词，不做「全通过」式汇总。"""
    bad = 0
    names = all_mutants()
    with tempfile.TemporaryDirectory(prefix="p159_selftest_") as td:
        base = pathlib.Path(td)
        _fixture(base)
        fails, notes = check(base)
        if fails:
            bad += 1
            print("SELFTEST 正例 FAIL: %s" % fails[:3])
        else:
            print("SELFTEST 正例 PASS（干净夹具必须绿）  分母：负例 %d 例" % len(names))
            for n in notes:
                print("         | %s" % n)
        n_drift = 0
        for name in names:
            root = base.parent / (base.name + "_" + name)
            root.mkdir(parents=True, exist_ok=True)
            _fixture(root, name)
            f2, _ = check(root)
            tag = "（逐码漂移）" if name.startswith("exit-value-drift-") else ""
            n_drift += name.startswith("exit-value-drift-")
            want = expect_token(name)
            if f2 and (want is None or any(l.startswith(want) for l in f2)):
                print("SELFTEST 负例 %-28s PASS%s [%s] 判红: %s"
                      % (name, tag, want or "-", f2[0][:70]))
            elif not f2:
                bad += 1
                print("SELFTEST 负例 %-28s FAIL（注入未被抓住 = 零鉴别力）" % name)
            else:
                bad += 1
                print("SELFTEST 负例 %-28s FAIL（判红但抓错判据: 期望 %s / 实际 %s）"
                      % (name, want, "; ".join(l[:40] for l in f2)))
            shutil.rmtree(root, ignore_errors=True)
    print("SELFTEST 逐码数值漂移 %d/%d 例判红" % (n_drift, len(VALUE_DRIFT_SYMS)))
    print("SELFTEST_%s" % ("FAIL" if bad else "PASS"))
    return 1 if bad else 0


def fault_inject(name: str) -> int:
    names = all_mutants()
    if name not in names:
        print("未知注入名，可选: %s" % ", ".join(sorted(names)))
        return 2
    with tempfile.TemporaryDirectory(prefix="p159_inject_") as td:
        root = pathlib.Path(td)
        _fixture(root, name)
        fails, _ = check(root)
    if not fails:
        print("FAULT-INJECT %s: 未判红（门已退化）" % name)
        return 1
    print("FAULT-INJECT %s: 判红 OK -> %s" % (name, fails[0]))
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="manifest 面退出码一致门（P-159）")
    ap.add_argument("--root", default=str(REPO))
    ap.add_argument("--json-out")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--fault-inject", metavar="NAME")
    a = ap.parse_args(argv)
    if a.self_test:
        return self_test()
    if a.fault_inject:
        return fault_inject(a.fault_inject)
    root = pathlib.Path(a.root)
    fails, notes = check(root)
    if a.json_out:
        out = pathlib.Path(a.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps({"check": "CHK-MANIFEST-EXIT-CODES", "root": str(root),
                                   "notes": notes, "errors": fails,
                                   "pass": not fails}, ensure_ascii=False, indent=1),
                       encoding="utf-8")
    if not a.quiet:
        for n in notes:
            print("[ok] " + n)
    if fails:
        for f in fails:
            print("[FAIL] " + f, file=sys.stderr)
        print("MANIFEST_EXIT_CODES_FAIL errors=%d" % len(fails))
        return 1
    print("MANIFEST_EXIT_CODES_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
