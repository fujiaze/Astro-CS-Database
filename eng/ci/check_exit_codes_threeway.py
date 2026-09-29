#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-EXIT-CODES-THREEWAY —— 退出码面「权威表 ↔ 实现 ↔ 文档」**三方对账**元门。

立项依据（本门存在的理由，实测见 run/FINAL-07/exitcode-threeway/）:
  全仓此前**没有任何门**做退出码面的三方对账，只有**两两**对账:
    · CHK-MANIFEST-EXIT-CODES 的 C1 只守 11 码中的 6 个 → 另 5 码可静默漂移;
    · API-DOCS(eng/tools/check_api_docs.py) 的 [B] 是**单向**的「doc 码 ⇒ 头文件有定义」，
      且只取 EXIT_NAMES 的**键** ⇒ 门内表把 6 写成 EXEC（头文件真值 COMPUTE=6）
      这类**同值异名**永远不可见。
  后果：权威表 / 实现 / 文档 三者可各自漂移而无任何一道门红。

三方定义（本门的三个「向」）:
  A 权威表 = lib/infrastructure/cli/exit_codes.h 的 enum ExitCode（11 码，数值语义唯一源）
  B 实现   = ① lib/include/astrocs/core/contracts.h 的 enum class ExitCode 镜像表
               （代码里的**第二份数值副本**）
             ② lib/infrastructure/cli/protocol.h 的 is_frozen_exit_code_v1
               （**冻结域谓词**：实现侧自己认哪 11 个符号）
             ③ 各模块实际 return astrocs::SYM（实现真正**发出**的码）
  C 文档   = ① docs/engineering/CLI_PROTOCOL_V1.md §2（全 11 码，散文、只有数值无符号名）
             ② docs/engineering/LOG_AND_ERROR_CONTRACT.md §5（ErrorDomain→码 表，
               **带符号名**，8 行）

判据（每条都给出**分母**，不做「全通过」式报告）:
  T1 A 自洽   : 枚举项数 = 11；11 个值两两互异；头文件自述条数 = 实际项数
  T2 A ↔ B①  : 镜像表与权威表**双向**逐符号逐数值相同（缺/多/漂移都点名）
  T3 A ↔ B②  : 冻结域谓词引用的符号集 == 权威表符号集（双向；少了/多了都点名）
  T4 A ↔ B③  : 实现 return 的每个符号**必须**在权威表登记（未登记码 → 判红）
  T5 A ↔ C①  : 权威表 11 个值逐个在 CLI_PROTOCOL_V1 §2 在位
  T6 A ↔ C②  : 合同 §5 表里每个 值（符号） 的符号与数值必须与权威表相符
                （**同值异名 / 错值** → 判红）
  T7 覆盖缺口 : 权威表里没有在合同 §5 出现的码 —— **只报不判红**（域→码不是 1:1，
                是否补行属负责人判定），但必须点名 + 给分母。

fail-closed：任一必需输入缺失 / 解析不到项 / 段锚缺失 ⇒ 退出码 2，**不得判绿**。

用法:
  python3 eng/ci/check_exit_codes_threeway.py [--root <dir>] [--json-out <file>] [--quiet]
  python3 eng/ci/check_exit_codes_threeway.py --self-test
  python3 eng/ci/check_exit_codes_threeway.py --fault-inject <name>
exit 0 = 三方一致；exit 1 = 不一致（判红）；exit 2 = 输入不可用（fail-closed）。
只读；fixture/注入一律落临时目录，不写仓库。
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
    "auth": "lib/infrastructure/cli/exit_codes.h",
    "mirror": "lib/include/astrocs/core/contracts.h",
    "frozen": "lib/infrastructure/cli/protocol.h",
    "impl": "lib/infrastructure/cli/commands.cpp",
    "impl_dir": "lib/infrastructure/cli",
    "doc1": "docs/engineering/CLI_PROTOCOL_V1.md",
    "doc2": "docs/engineering/LOG_AND_ERROR_CONTRACT.md",
}
# T4 的实现面 = CLI 收敛面（lib/infrastructure/cli/ 整个目录），不是单个文件：
# 退出码在 parser.cpp / commands.cpp / main.cpp / runtime_client.cpp / subcommand.h /
# mode_gate.h 多个文件里发出，只扫 commands.cpp 会把分母做小（实测漏 3 个码）。
IMPL_SUFFIX = (".cpp", ".h", ".hpp", ".cc")

FROZEN_COUNT = 11                       # 冻结退出码条数（数值单源纪律，exit_codes.h 自述亦为 11）
SELF_DECL_RE = re.compile(r"本文件是\s*(\d+)\s*个退出码")
ENUM_BLOCK_RE = re.compile(r"enum\s+(?:class\s+)?ExitCode[^{]*\{(.*?)\}", re.S)
ENUM_ITEM_RE = re.compile(r"([A-Z_][A-Z0-9_]*)\s*=\s*(\d+)\b")
# 不要求同行有 ';'：多行 return（return astrocs::X\n    ;）在 cli/ 里很常见，
# 带上 ';' 会把分母做小（实测 89 处只认出 39 处）。
RET_RE = re.compile(r"return\s+astrocs::([A-Z_][A-Z0-9_]*)")
FROZEN_FN_RE = re.compile(
    r"is_frozen_exit_code_v1\s*\(\s*int\s+\w+\s*\)\s*\{(.*?)\n\s*\}", re.S)
# 合同 §5 表行： | \x60DOMAIN\x60 | 7（IO） | 依据 |
CONTRACT_ROW_RE = re.compile(
    r"^\s*\|\s*\x60([A-Z_][A-Z0-9_]*)\x60\s*\|\s*(\d+)\s*[（(]([A-Z_][A-Z0-9_]*)[）)]\s*\|",
    re.M)
BACKTICK = "\x60"

DOC1_SEC = ("## 2 ", "## 3 ")
DOC2_SEC = ("## 5. ", "## 6. ")


class InputMissing(Exception):
    """fail-closed 信号：输入缺失 / 解析不了。上层一律转退出码 2，绝不判绿。"""


def _read(root: pathlib.Path, key: str) -> str:
    p = root / REL[key]
    if not p.is_file():
        raise InputMissing("缺输入文件: %s" % p)
    return p.read_text(encoding="utf-8", errors="ignore")


def _section(text: str, start: str, end: str, what: str) -> str:
    i = text.find(start)
    if i < 0:
        raise InputMissing("%s 段锚缺失: %s" % (what, start))
    j = text.find(end, i + len(start))
    if j < 0:
        raise InputMissing("%s 段收尾锚缺失: %s" % (what, end))
    return text[i:j]


def parse_auth(text: str) -> dict:
    """A 权威表：exit_codes.h 的 enum ExitCode -> {符号: 数值}。"""
    m = ENUM_BLOCK_RE.search(text)
    if not m:
        raise InputMissing("exit_codes.h 解析不到 enum ExitCode 块")
    items = [(a, int(b)) for a, b in ENUM_ITEM_RE.findall(m.group(1))]
    if not items:
        raise InputMissing("exit_codes.h 的 enum ExitCode 块解析不到任何枚举项")
    return dict(items)


def parse_mirror(text: str) -> dict:
    """B1 实现镜像表：contracts.h 的 enum class ExitCode。"""
    m = ENUM_BLOCK_RE.search(text)
    if not m:
        raise InputMissing("contracts.h 解析不到 enum class ExitCode 块")
    items = [(a, int(b)) for a, b in ENUM_ITEM_RE.findall(m.group(1))]
    if not items:
        raise InputMissing("contracts.h 的 enum class ExitCode 块解析不到任何枚举项")
    return dict(items)


def parse_frozen(text: str) -> set:
    """B2 冻结域谓词 is_frozen_exit_code_v1 引用的符号集。"""
    m = FROZEN_FN_RE.search(text)
    if not m:
        raise InputMissing("protocol.h 解析不到 is_frozen_exit_code_v1 函数体")
    syms = set(re.findall(r"\b([A-Z_][A-Z0-9_]*)\b", m.group(1)))
    if not syms:
        raise InputMissing("is_frozen_exit_code_v1 函数体里解析不到任何符号")
    return syms


def scan_impl_returns(root: pathlib.Path) -> list:
    """B3 实现实际发出的码：[(符号, 仓内相对路径, 行号)]，逐处计数（分母用）。

    扫整个 CLI 收敛面 lib/infrastructure/cli/；退出码在多个文件里发出
    （commands.cpp / parser.cpp / main.cpp / runtime_client.cpp /
      subcommand.h / mode_gate.h），只扫 commands.cpp 会把分母做小。
    """
    d = root / REL["impl_dir"]
    if not d.is_dir():
        raise InputMissing("缺实现面目录: %s" % d)
    out = []
    for p in sorted(d.rglob("*")):
        if p.is_file() and p.suffix in IMPL_SUFFIX:
            text = p.read_text(encoding="utf-8", errors="ignore")
            rel = p.relative_to(root).as_posix()
            out += [(m.group(1), rel, text[:m.start()].count("\n") + 1)
                    for m in RET_RE.finditer(text)]
    if not out:
        raise InputMissing("实现面 %s 里解析不到任何 return astrocs::<SYM>" % d)
    return out


def parse_contract(text: str) -> list:
    """C2 合同 §5 域→码表：[(域, 值, 符号, 行号)]。"""
    sec = _section(text, DOC2_SEC[0], DOC2_SEC[1], "LOG_AND_ERROR_CONTRACT.md")
    rows = []
    for m in CONTRACT_ROW_RE.finditer(sec):
        rows.append((m.group(1), int(m.group(2)), m.group(3),
                     text[:m.start()].count("\n") + 1))
    if not rows:
        raise InputMissing("LOG_AND_ERROR_CONTRACT.md §5 域→码表解析不到任何行")
    return rows


def parse_cli_doc(text: str) -> str:
    """C1 CLI_PROTOCOL_V1 §2 正文。"""
    return _section(text, DOC1_SEC[0], DOC1_SEC[1], "CLI_PROTOCOL_V1.md")


def check(root: pathlib.Path):
    fails, notes, gaps = [], [], []
    auth_txt = _read(root, "auth")
    auth = parse_auth(auth_txt)

    # ── T1 A 自洽 ────────────────────────────────────────────────────────
    byval = {}
    for s, v in auth.items():
        byval.setdefault(v, []).append(s)
    collide = {v: ns for v, ns in byval.items() if len(ns) > 1}
    for v in sorted(collide):
        fails.append("T1 A(权威表 exit_codes.h) 数值 %d 被多个符号占用: %s"
                     % (v, ", ".join(sorted(collide[v]))))
    msd = SELF_DECL_RE.search(auth_txt)
    if not msd:
        fails.append("T1 A 自述锚缺失「本文件是 N 个退出码」")
    elif int(msd.group(1)) != len(auth):
        fails.append("T1 A 自述「%s 个退出码」≠ 实际枚举 %d 项"
                     % (msd.group(1), len(auth)))
    if len(auth) != FROZEN_COUNT:
        fails.append("T1 A 权威表枚举 %d 项，要求 %d 项（%s）"
                     % (len(auth), FROZEN_COUNT,
                        "少项=删码" if len(auth) < FROZEN_COUNT else "多项=发明新码"))
    notes.append("T1 A 权威表: 项数 %d/%d, 唯一值 %d/%d, 自述条数=%s, 撞值组 %d"
                 % (len(auth), FROZEN_COUNT, len(byval), len(auth),
                    msd.group(1) if msd else "缺失", len(collide)))

    # ── T2 A ↔ B1 实现镜像表 ────────────────────────────────────────────
    mirror = parse_mirror(_read(root, "mirror"))
    only_a = sorted(set(auth) - set(mirror))
    only_b = sorted(set(mirror) - set(auth))
    diff_v = sorted(s for s in set(auth) & set(mirror) if auth[s] != mirror[s])
    for s in only_a:
        fails.append("T2 A→B1 缺 %s=%d：contracts.h 的 enum class ExitCode 未登记"
                     % (s, auth[s]))
    for s in only_b:
        fails.append("T2 B1→A 多 %s=%d：权威表 exit_codes.h 无此符号" % (s, mirror[s]))
    for s in diff_v:
        fails.append("T2 A↔B1 数值漂移 %s：权威表=%d，contracts.h 镜像表=%d"
                     % (s, auth[s], mirror[s]))
    notes.append("T2 A↔B1 镜像表: 一致 %d/%d（缺 %d / 多 %d / 漂移 %d）"
                 % (len(auth) - len(only_a) - len(diff_v), len(auth),
                    len(only_a), len(only_b), len(diff_v)))

    # ── T3 A ↔ B2 冻结域谓词 ────────────────────────────────────────────
    frozen = parse_frozen(_read(root, "frozen"))
    f_only_a = sorted(set(auth) - frozen)
    f_only_f = sorted(frozen - set(auth))
    for s in f_only_a:
        fails.append("T3 A→B2 权威表有 %s=%d，但 protocol.h 的 is_frozen_exit_code_v1 "
                     "未把它算进冻结域（运行事件面会把该码判为非法）" % (s, auth[s]))
    for s in f_only_f:
        fails.append("T3 B2→A is_frozen_exit_code_v1 引用了 %s，权威表无此符号" % s)
    notes.append("T3 A↔B2 冻结域谓词: 覆盖 %d/%d（未计入 %d / 多引用 %d）"
                 % (len(frozen & set(auth)), len(auth), len(f_only_a), len(f_only_f)))

    # ── T4 A ↔ B3 实现实际 return 的码 ──────────────────────────────────
    rets = scan_impl_returns(root)
    impl_syms = {s for s, _, _ in rets}
    unregistered = sorted(impl_syms - set(auth))
    for s in unregistered:
        where = ["%s:%d" % (f, ln) for x, f, ln in rets if x == s][:6]
        fails.append("T4 B3→A 码 %s 未在权威表登记：%s %d 处 %sreturn astrocs::%s%s"
                     % (s, "、".join(where), len(where), BACKTICK, s, BACKTICK))
    unused = sorted(set(auth) - impl_syms)
    notes.append("T4 A↔B3 实现 return: 引用 %d 处 / 覆盖 %d/%d 码"
                 "（未登记 %d；定义但实现未发出 %d）"
                 % (len(rets), len(impl_syms & set(auth)), len(auth),
                    len(unregistered), len(unused)))
    for s in unused:
        notes.append("     · %s=%d 已登记但实现面未发出（仅在冻结域谓词/权威表出现）"
                     % (s, auth[s]))

    # ── T5 A ↔ C1 CLI_PROTOCOL_V1 §2 ────────────────────────────────────
    cli_sec = parse_cli_doc(_read(root, "doc1"))
    found = set()
    for v in set(auth.values()):
        if re.search(r"(?<![\d.])%d(?![\d.])" % v, cli_sec):
            found.add(v)
    missing_doc1 = sorted(set(auth.values()) - found)
    for v in missing_doc1:
        sym = sorted(s for s, x in auth.items() if x == v)[0]
        fails.append("T5 A→C1 CLI_PROTOCOL_V1.md §2 未登记 退出码 %d（%s）" % (v, sym))
    notes.append("T5 A↔C1 CLI_PROTOCOL §2: 权威 %d/%d 码在位（缺 %d）"
                 % (len(found), len(auth), len(missing_doc1)))

    # ── T6 A ↔ C2 合同 §5 域→码表 ───────────────────────────────────────
    rows = parse_contract(_read(root, "doc2"))
    n_ok_t6 = 0
    for dom, val, sym, ln in rows:
        if sym not in auth:
            n_ok_t6 += 1
            fails.append("T6 C2→A LOG_AND_ERROR_CONTRACT.md:%d 域 %s 写的符号 %r "
                         "不在权威表（值 %d；同值权威符号=%s）——同值异名"
                         % (ln, dom, sym, val,
                            "/".join(sorted(s for s, x in auth.items() if x == val)) or "无"))
        elif auth[sym] != val:
            n_ok_t6 += 1
            fails.append("T6 C2↔A LOG_AND_ERROR_CONTRACT.md:%d 域 %s 写 %s=%d，权威表是 %d"
                         % (ln, dom, sym, val, auth[sym]))
        else:
            n_ok_t6 += 1
    notes.append("T6 A↔C2 合同 §5 域→码表: 一致 %d/%d 行（表行分母）" % (n_ok_t6, len(rows)))

    # ── T7 覆盖缺口（只报不判红）─────────────────────────────────────────
    doc2_syms = {s for _, _, s, _ in rows}
    miss_sym = sorted(set(auth) - doc2_syms)
    for s in miss_sym:
        gaps.append("T7 C2 覆盖缺口: 权威表 %s=%d 在合同 §5 域→码表里没有对应行"
                    "（域→码非 1:1，是否补行属负责人判定）" % (s, auth[s]))
    notes.append("T7 C2 覆盖: 合同表命中 %d/%d 码（%d 个码无对应域行）"
                 % (len(doc2_syms & set(auth)), len(auth), len(miss_sym)))
    return fails, notes, gaps


# ── 可执行正/负例面 ─────────────────────────────────────────────────────────
# 每个注入 = (键, 旧串, 新串) 或 (键, None, callable)；注入到临时夹具上跑，必须判红。
MUTANTS = {
    # 负例 1：实现多出未登记码（T4）——正是「典型受害面」：模块实际发码但表里没有
    "impl-unregistered-code":
        ("impl", None, lambda t: t.replace(
            "return astrocs::OK;", "return astrocs::GHOST_UNREGISTERED;", 1)),
    # 负例 2：文档写错名字（T6）——同值异名那一类
    "contract-doc-wrong-name":
        ("doc2", "| 7（IO） | I/O 失败", "| 7（EXEC） | I/O 失败"),
    # 负例 3：权威表删码（T1+T2+T3）
    "authority-drop-code":
        ("auth", None, lambda t: "\n".join(
            l for l in t.split("\n") if not re.match(r"^\s*SCIENCE\s*=\s*\d+", l))),
    # 负例 4：实现镜像表数值漂移（T2）
    "mirror-value-drift": ("mirror", "COMPUTE = 6,", "COMPUTE = 61,"),
    # 负例 5：冻结域谓词漏一码（T3）
    "frozen-domain-narrow": ("frozen", "c == COMPUTE || ", ""),
    # 负例 6：CLI 协议文档漏登记一个码（T5）
    "cli-doc-drop-code": ("doc1", "/ 70 未分类内部错误", "/ 未分类内部错误"),
    # 负例 7：权威表造同值（复制 COMPUTE 的数值给 SCIENCE）（T1）
    "authority-dup-value":
        ("auth", None, lambda t: t.replace("SCIENCE       = 4,", "SCIENCE       = 6,", 1)),
    # 负例 8：权威表加一个新码（T1 项数）
    "authority-add-code":
        ("auth", None, lambda t: t.replace(
            "INTERNAL      = 70,", "INTERNAL      = 70,\n    BRAND_NEW    = 71,", 1)),
}
MUTANT_EXPECT = {
    "impl-unregistered-code": "T4",
    "contract-doc-wrong-name": "T6",
    "authority-drop-code": "T1",
    "mirror-value-drift": "T2",
    "frozen-domain-narrow": "T3",
    "cli-doc-drop-code": "T5",
    "authority-dup-value": "T1",
    "authority-add-code": "T1",
}


def _apply(text: str, mutation: str) -> str:
    _, old, new = MUTANTS[mutation]
    if callable(new):
        out = new(text)
        if out == text:
            raise SystemExit("[fail-closed] 注入锚未命中: %s" % mutation)
        return out
    n = text.count(old)
    if n != 1:
        raise SystemExit("[fail-closed] 注入锚命中 %d 次（要求恰好 1）: %s -> %s"
                         % (n, old[:50], mutation))
    return text.replace(old, new)


def _fixture(dst: pathlib.Path, mutation=None, drop_key=None, blank_key=None):
    for key, rel in REL.items():
        src = REPO / rel
        if src.is_dir():
            continue          # impl_dir 只是 T4 的扫描根，不是要复制的单文件输入
        if not src.is_file():
            raise SystemExit("[fail-closed] 缺真仓输入: %s" % src)
        tgt = dst / rel
        tgt.parent.mkdir(parents=True, exist_ok=True)
        text = src.read_text(encoding="utf-8", errors="ignore")
        if drop_key == key:
            continue                     # 制造「输入缺失」
        if blank_key == key:
            text = "// 本注入把该文件清空，制造「解析不了」\n"
        elif mutation and MUTANTS[mutation][0] == key:
            text = _apply(text, mutation)
        tgt.write_text(text, encoding="utf-8")


def self_test() -> int:
    bad = 0
    names = sorted(MUTANTS)
    with tempfile.TemporaryDirectory(prefix="threeway_selftest_") as td:
        base = pathlib.Path(td)
        _fixture(base)
        try:
            fails, notes, gaps = check(base)
        except InputMissing as exc:
            print("SELFTEST 正例 FAIL-closed: %s" % exc)
            return 1
        if fails:
            bad += 1
            print("SELFTEST 正例 FAIL: %s" % fails[:3])
        else:
            print("SELFTEST 正例 PASS（干净夹具必须绿）  负例分母 %d 例 + fail-closed 2 例"
                  % len(names))
            print("         （注意：以下分母是**夹具**读数 —— 夹具只复制 commands.cpp，"
                  "T4 的实现面分母因此小于真仓；真仓读数请直接跑本门）")
            for n in notes:
                print("         | %s" % n)
            for g in gaps:
                print("         ~ %s" % g)
        for name in names:
            root = base.parent / (base.name + "_" + name)
            root.mkdir(parents=True, exist_ok=True)
            _fixture(root, name)
            try:
                f2, _, _ = check(root)
            except InputMissing as exc:
                bad += 1
                print("SELFTEST 负例 %-24s FAIL（注入导致 fail-closed 而非判红: %s）"
                      % (name, exc))
            else:
                want = MUTANT_EXPECT[name]
                if f2 and any(l.startswith(want) for l in f2):
                    print("SELFTEST 负例 %-24s PASS [%s] 判红: %s"
                          % (name, want, f2[0][:74]))
                elif not f2:
                    bad += 1
                    print("SELFTEST 负例 %-24s FAIL（未被抓住 = 零鉴别力）" % name)
                else:
                    bad += 1
                    print("SELFTEST 负例 %-24s FAIL（抓错判据: 期望 %s / 实际 %s）"
                          % (name, want, "; ".join(l[:32] for l in f2)))
            shutil.rmtree(root, ignore_errors=True)
        # fail-closed：不仅要在进程内抛 InputMissing，还必须**进程真的退 2**。
        # 这里用子进程跑 main()，直接断言 returncode == 2（判绿=0 一律算失败）。
        import subprocess
        for label, kw in (("input-missing", dict(drop_key="auth")),
                          ("unparsable", dict(blank_key="auth"))):
            root = base.parent / (base.name + "_" + label)
            root.mkdir(parents=True, exist_ok=True)
            _fixture(root, **kw)
            try:
                check(root)
                inproc = "未抛"
            except InputMissing as exc:
                inproc = "抛 InputMissing"
            proc = subprocess.run(
                [sys.executable, str(pathlib.Path(__file__).resolve()),
                 "--root", str(root), "--quiet"],
                capture_output=True, text=True)
            ok = (proc.returncode == 2 and inproc != "未抛")
            if not ok:
                bad += 1
            print("SELFTEST fail-closed %-14s %s（进程 rc=%d，期望 2；%s）"
                  % (label, "PASS" if ok else "FAIL", proc.returncode, inproc))
            shutil.rmtree(root, ignore_errors=True)
    print("SELFTEST_%s" % ("FAIL" if bad else "PASS"))
    return 1 if bad else 0


def fault_inject(name: str) -> int:
    if name not in MUTANTS:
        print("未知注入名，可选: %s" % ", ".join(sorted(MUTANTS)))
        return 2
    with tempfile.TemporaryDirectory(prefix="threeway_inject_") as td:
        root = pathlib.Path(td)
        _fixture(root, name)
        try:
            fails, _, _ = check(root)
        except InputMissing as exc:
            print("FAULT-INJECT %s: fail-closed 退 2 -> %s" % (name, exc))
            return 0
    if not fails:
        print("FAULT-INJECT %s: 未判红（门已退化）" % name)
        return 1
    print("FAULT-INJECT %s: 判红 OK -> %s" % (name, fails[0]))
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="退出码面三方对账元门")
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
    try:
        fails, notes, gaps = check(root)
    except InputMissing as exc:
        print("[fail-closed] %s" % exc, file=sys.stderr)
        print("EXIT_CODES_THREEWAY_INPUT_MISSING: %s" % exc, file=sys.stderr)
        return 2
    if a.json_out:
        out = pathlib.Path(a.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(
            {"check": "CHK-EXIT-CODES-THREEWAY", "root": str(root),
             "notes": notes, "gaps": gaps, "errors": fails, "pass": not fails},
            ensure_ascii=False, indent=1), encoding="utf-8")
    if not a.quiet:
        for n in notes:
            print("[ok] " + n)
        for g in gaps:
            print("[gap] " + g)
    if fails:
        for f in fails:
            print("[FAIL] " + f, file=sys.stderr)
        print("EXIT_CODES_THREEWAY_FAIL errors=%d" % len(fails))
        return 1
    print("EXIT_CODES_THREEWAY_PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
