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
  C1 exit_codes.h 中 OK/ARGS/INPUT/BACKEND/IO/INTEGRITY 的数值 = 0/2/3/5/7/8；
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

EXIT_VALUES = {"OK": 0, "ARGS": 2, "INPUT": 3, "BACKEND": 5, "IO": 7, "INTEGRITY": 8}

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
    notes.append("C1 exit_codes.h 符号-数值绑定 %d 项" % len(EXIT_VALUES))

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
    "code-status-to-input": ("cmd", 'return astrocs::INPUT;\n', 'return astrocs::INTEGRITY;\n'),
    "doc-seq-code-wrong": ("docv", "(否则 8)", "(否则 3)"),
    "exit-value-drift": ("exit", "INTEGRITY     = 8", "INTEGRITY     = 7"),
}


def _fixture(dst: pathlib.Path, mutation: str | None = None):
    for key, rel in REL.items():
        src = REPO / rel
        if not src.is_file():
            raise SystemExit("[fail-closed] 缺真仓输入: %s" % src)
        tgt = dst / rel
        tgt.parent.mkdir(parents=True, exist_ok=True)
        text = src.read_text(encoding="utf-8", errors="ignore")
        if mutation:
            keym, old, new = MUTANTS[mutation]
            if keym == key:
                # code-status-to-input 用唯一锚（status!=complete 分支）定向替换
                if mutation == "code-status-to-input":
                    anchor = "acsd: run manifest status="
                    k = text.find(anchor)
                    if k < 0:
                        raise SystemExit("[fail-closed] 注入锚缺失: %s" % anchor)
                    m = RET_RE.search(text, k)
                    if not m:
                        raise SystemExit("[fail-closed] 注入锚后无 return")
                    text = text[:m.start()] + "return astrocs::INPUT;" + text[m.end():]
                else:
                    n = text.count(old)
                    if n != 1:
                        raise SystemExit("[fail-closed] 注入锚命中 %d 次（要求恰好 1）: %s"
                                         % (n, old[:40]))
                    text = text.replace(old, new)
        tgt.write_text(text, encoding="utf-8")


def self_test() -> int:
    bad = 0
    with tempfile.TemporaryDirectory(prefix="p159_selftest_") as td:
        base = pathlib.Path(td)
        _fixture(base)
        fails, _ = check(base)
        if fails:
            bad += 1
            print("SELFTEST 正例 FAIL: %s" % fails[:3])
        else:
            print("SELFTEST 正例 PASS（干净夹具必须绿）")
        for name in MUTANTS:
            root = base.parent / (base.name + "_" + name)
            root.mkdir(parents=True, exist_ok=True)
            _fixture(root, name)
            f2, _ = check(root)
            if f2:
                print("SELFTEST 负例 %s PASS（判红: %s）" % (name, f2[0][:80]))
            else:
                bad += 1
                print("SELFTEST 负例 %s FAIL（注入未被抓住 = 零鉴别力）" % name)
            shutil.rmtree(root, ignore_errors=True)
    print("SELFTEST_%s" % ("FAIL" if bad else "PASS"))
    return 1 if bad else 0


def fault_inject(name: str) -> int:
    if name not in MUTANTS:
        print("未知注入名，可选: %s" % ", ".join(sorted(MUTANTS)))
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
