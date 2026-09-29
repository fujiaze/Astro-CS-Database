#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-403 机器检查器：双向层级索引闭合门（DOC-INDEX v2）。

权威依据：docs/ASTROCS_DESIGN.md §0.2（详细文档层与双向索引）、§0.3；ENGINEERING_SPEC.md §8
（悬空即缺陷）、§10（机器门：fail-closed / 可执行负例面 / 锚存活）。

检查项（exit 0 = PASS）：
  A. 索引文件与结构
     1. docs/DOCUMENT_INDEX.yaml 存在且受 Git 跟踪；
     2. YAML 结构合法（doc_index.active / doc_index.archived）；
     3. status 取值合法且 path 全索引唯一；active 不含 archive 路径；
        archived 全为 ARCHIVED_NON_NORMATIVE；
     4. 条目路径必须存在（悬空条目即缺陷；CLEAN-402 后遗留的 32 条已清除）；
     5. 覆盖：所有 Git 跟踪的 docs/** md/rst/txt/yaml 文件都被索引登记；
  B. 双向索引闭合（DOC-403 新增）
     6. 最高设计与根文档（ENGINEERING_SPEC §7 根固定条目）中每个 docs/... 指针目标存在；
     7. 每份下级文档（docs/**/*.md，活动分类 ACTIVE_*）都在索引登记；
     8. 每份下级文档抬头有「上游」条款区（指向 docs/ASTROCS_DESIGN.md 条款），覆盖率 100%；
     9. 文档与代码注释中引用的 docs/ 路径存在（与代码路径缺口门共用扫描数据；
        跨域未修项在 eng/tools/doccheck/dangling_ledger.json 显式登记，只减不增）；
  C. 归档边界与旧权威回归
    10. docs/archive/** 下每个 md 头 400 字符含 ARCHIVED 标记；工程控制归档目录约束；
    11. 根与 docs/ 下无散落旧控制包（含非 ASCII 路径：git -c core.quotepath=false）；
    12. 锚存活 + 旧权威回归（docs/review/**、REVIEW.md、CHANGELOG.md、HANDOVER.md）。
  D. 扫描面自证
    13. 逐文件排除项（SKIP_EXACT）逐条有据：是台账本身，或该文件自带 --self-test
        夹具面（既注册 --self-test 入口、又实现 self_test()/_self_test()）；文件不存在
        亦判红（锚存活 / fail-closed）。"为了让红灯变绿而随手把某文件塞进排除面"由此
        判红（负例 S17 无夹具面 / S18 锚缺失 / S19 台账被移出 / S20 判据判别力），
        排除面只减不增、判别力不降。

--strict 语义：判据硬引用的锚（ANCHOR_PATHS）任一缺失 ⇒ FAIL 并打印
  "ANCHOR_STALE: <path>"；退役锚缺失 ⇒ "ANCHOR_RETIRED_OK"（出库完成态，不判红）。

fail-closed（ENGINEERING_SPEC §10）：
  * --root 不存在 / 索引缺失 / 依赖（pyyaml、git）不可用 ⇒ 判红，不 traceback；
  * 扫描面为空（下级文档 0 份、根文档 0 份、台账不可读）⇒ 判红，不当作无违规。

用法：
  python3 eng/tools/doccheck/check_doc_index.py [--root <repo>] [--json-out <file>] [--strict] [--quiet]
  python3 eng/tools/doccheck/check_doc_index.py --self-test          # 可执行正/负例面
  python3 eng/tools/doccheck/check_doc_index.py --dump <kind>        # 机器出表（证据）
      kind 属于 design-pointers / subordinate-docs / dangling / entries

退出码：0 = PASS，1 = FAIL；--self-test 恒 0 = 全部内置正/负例符合预期，任一例不符预期则 1。
"""
from __future__ import annotations

import argparse
import contextlib
import json
import os
import re
import subprocess
import sys

INDEX_PATH = "docs/DOCUMENT_INDEX.yaml"
LEGAL = {"ACTIVE_NORMATIVE", "ACTIVE_INFORMATIVE", "GENERATED", "ARCHIVED_NON_NORMATIVE"}
T = chr(9)
BT = chr(96)

# --- 锚存活（ENGINEERING_SPEC §8）-------------------------------------------------
ANCHOR_PATHS = (INDEX_PATH,)

RETIRED_ANCHORS = (
    {
        "path": "engineering/control/archive/2026-09-02_legacy_工程控制_v1.3-to-v6.1/README_ARCHIVED.md",
        "retired_by": "ROOT-007 / RETIRE-001（旧世代控制包归档树整树删除）",
        "date": "2026-09-16",
        "evidence": "git ls-files engineering/ = 0 行；ls engineering/control/archive/ rc=2",
        "note": "原 control_archive_dir_readme 判据已随锚退役降级为检查 10 的目录级说明约束",
    },
)

RETIRED_ANCHOR_DIRS = ("engineering/control/archive",)
RETIRED_DOC_PATHS = ("docs/review", "REVIEW.md", "CHANGELOG.md", "HANDOVER.md")
ARCHIVE_DECL_MARKERS = ("ARCHIVED", "已归档", "已退役")

# --- 双向索引闭合面（DOC-403）-----------------------------------------------------
# 根文档 = ENGINEERING_SPEC §7「仓库根固定条目」中的文档条目 + 最高设计/手册。
# memory.md 不入本表：它是 history/日志命名空间驻留点（GOV-003 §2/§4 只警告不硬判，
# 见 eng/ci/check_version.py 文件头 [5]），历史轮次引用不构成活动指针。
ROOT_DOCS = (
    "docs/ASTROCS_DESIGN.md",
    "AGENTS.md",
    "ENGINEERING_SPEC.md",
    "CONTROL_PACK_SPEC.md",
    "ACCEPTANCE_SPEC.md",
    "README.md",
    "DEPENDENCIES.md",
)

# 代码注释面扫描目录（与代码路径缺口门共用扫描数据；过程/证据目录不入扫描面）
CODE_SCAN_DIRS = ("lib/", "eng/ci/", "eng/tools/", "eng/tests/", "eng/contracts/", "eng/packaging/config/",
                  "eng/packaging/", "eng/cmake/")
# 2026-09-21 ROOT-CONSOLIDATION：reverse_verify/ 根条目解散，其内容整体迁入 实验/，
# 故排除面由 "reverse_verify/" 等量替换为 "实验/"（扫描面不增不减）。
SKIP_DIRS = ("问题扫描/", "reports/", "artifacts/", "run/", "build/", "third_party/",
             "实验/", "工程控制/", "logs/", "docs/archive/")
SCAN_EXT = (".md", ".txt", ".json", ".yaml", ".yml", ".py", ".sh", ".h", ".hpp", ".cpp",
            ".cc", ".cmake", ".csv", ".in", ".ps1", ".toml")
LEDGER_PATH = "eng/tools/doccheck/dangling_ledger.json"
LEDGER_MAX = 104  # 冻结上限（只减不增；增加须显式评审并同步本常量）

# --- 内容真实性门（CONTENT-TRUTH）：路径可达但目标页没有该内容 -------------------------
#
# 为什么另立判据（本门存在理由，见 run/FINAL-07/content-truth/语义边界与可行性判定.md）：
#   scan_dangling() 只判「路径能否解析」。全函数从不开目标文件看内容 ⇒
#   「目标文件存在、路径可达、但那一页根本没有这句话讲的东西」在设计上门就抓不到。
#   已发生的真实实例：docs/detail/ 下 11 处模块页写「落位规则见 docs/detail/README.md」，
#   而那一节在当时的现行树里不存在（迁移重写的招牌件没带上原文）—— 门当时不红。
#
# 判据 T1（节号锚存活）：扫描面内形如 `<docs 路径> §<N[.N...]>` / `第 N 节` / ` N 节` 的引用，
#   目标必须**真有其节**。三种解析模式（任一命中即存活）：
#     模式 A 目标有编号标题 `#{1,6} <N[.N...]>`（多级节号必须能表达）；
#     模式 B 目标某标题正文里带显式节号 `（§<N>）`（仓内实测存在，见 01_CHECKS「（§2.1.1）」）；
#     模式 C 父节 <N> 正文里有有序列表第 <M> 项（仓内「§14.2 = §14 第 2 条」惯例，见 NOISE_MODEL §14）。
#   A/B/C 全不命中 => CONTENT_REF_DEAD，逐条点名「文件:行号 + 引用号 + 该目标实际有的同层节号」。
#
# 判据的语义边界（**这一段是判据成立的前提，不是事后说明**）：
#   本门只判「**可枚举定位符**」——节号。节号是文档自证的、可被机器穷举的地址。
#   本门**不判**「具名条款」（如「落位规则」「本节」）：那类指代词与目标标题之间没有可枚举的
#   对应关系，实测宽词表命中率仅 0.39（见交付文档），做进去就是用误报换命中。
#   本门**不判**行号锚内容：`ALG-LINE-ANCHORS`/`DOC-LINE-ANCHORS` 已覆盖行锚界内与符号绑定，
#   本仓实测 270 条行锚**超界 0 条**，重复实现既无信号又制造第二套解析口径。
#
# 与既有门的分工（**不重复造已有的那部分**）：
#   `check_design_section_refs.py` 的 S2 目标面**只有** docs/ASTROCS_DESIGN.md，
#   且其 S3 只判最高设计的节号存活。§N 指向**任何其它** docs 页时全仓无门 —— 本门补的就是这一面。
#   因此本门**主动排除** ASTROCS_DESIGN.md 目标（避免与该门重复判红、避免两套节号口径）。
CONTENT_TRUTH_LEDGER = "eng/tools/doccheck/content_truth_ledger.json"
# 冻结上限（只减不增）。台账键 = (file, target, section)，当前真死锚 10 处出现合并为 6 条。
CONTENT_TRUTH_LEDGER_MAX = 6
DESIGN_DOC = "docs/ASTROCS_DESIGN.md"
HEAD_NUM_RE = re.compile(r"^\s{0,3}#{1,6}\s*(\d+(?:\.\d+)*)(?![0-9])")
HEAD_ANY_RE = re.compile(r"^\s{0,3}#{1,6}\s+(.*)$")
INLINE_SEC_RE = re.compile(r"§\s*(\d+(?:\.\d+)*)")
LIST_ITEM_RE = re.compile(r"^\s*(\d+)\.\s")
# 节号锚：路径 token 之后**紧邻**（只隔空白/反引号/括号）的节号。
# 「紧邻」是绑定规则：同句左侧出现过别的文档名时，不得把该行的 § 号算到本路径头上
# （与 check_design_section_refs.py v2.1「归属必须可绑定」同源，不引入第二套猜测口径）。
SEC_ANCHOR_RE = re.compile(r"§\s*(\d+(?:\.\d+)*)|第\s*(\d+(?:\.\d+)*)\s*节"
                           r"|(\d+(?:\.\d+)*)\s*节")

# 现行控制包（ENGINEERING_SPEC §7：工程控制/ = 控制包工作区，收口后按
# CONTROL_PACK_SPEC §9 清理）。本目录以外的 工程控制/** 一律视为旧包残留。
CURRENT_CONTROL_PACK = "工程控制/RELEASE-05"
# 已收口归档包白名单（默认空：RELEASE-04 已按 §9 出库、整树删除，白名单无对象）。
# 新增白名单条目必须自带 SUMMARY.md（收口证明），缺 SUMMARY.md 的条目仍判红；
# 未登记的 工程控制/** 一律判红（本门保持有牙）。
ARCHIVED_CONTROL_PACKS = ()

# 扫描面排除（各自有据，不是"看不见"）：
#   * memory.md —— history/日志命名空间驻留点（GOV-003 §2/§4 只警告不硬判）
#   * /third_party/ —— 第三方 vendored 源码
#   * 本检查器自身与台账 —— 自检夹具/台账按构造承载 docs/ token
SKIP_SUFFIX = ("/memory.md",)
SKIP_CONTAINS = ("/third_party/",)

# SKIP_EXACT 准入规则（正向判据，不是文件名清单；机器自证 = skip_exact_justified）：
#   逐文件排除只对「按构造必须承载 docs/ token、但那些 token 不指向真实文件」的文件开放，
#   且必须属于下列两类之一 —— 不满足者一律不得登记（登记即判红）：
#     ① 台账本身（LEDGER_PATH）：它是悬空引用的显式登记面，其中的 token 是**数据**不是引用；
#     ② 自带 --self-test 夹具面的检查器：夹具里的 docs/… 是「应当被判红的样例文本」，
#        按构造不存在；把它们当悬空引用扫描 = 把负例当违规（DOC-HYGIENE-01 新增
#        check_doc_hygiene.py 未同步登记时，正是这类假红：8 条全来自其 --self-test 夹具）。
#   判别力不受影响：非排除文件里的悬空 docs/ 引用仍逐条判红（负例 S13），
#   台账上限 LEDGER_MAX 只减不增，且新增排除项必须先自证①/②（负例 S17/S18）。
SKIP_EXACT = (
    LEDGER_PATH,                                # ① 台账（悬空引用的登记面本身）
    "eng/tools/doccheck/check_doc_index.py",    # ② 本检查器（--self-test 夹具面）
    "eng/tools/doccheck/check_doc_hygiene.py",  # ② DOC-HYGIENE 检查器（--self-test 夹具面）
    # ② 其余自带 --self-test 夹具面的检查器（GATE-TRIAGE-01 补登记）：
    # 它们的夹具按构造写入不存在的 docs/ 路径（docs/DESIGN.md、docs/NOPE.md、
    # docs/gone/SPEC.md、docs/other.md、docs/contracts/FIX.md）作为**应当被判红的
    # 样例文本**，被本门当悬空引用扫描 = 把负例当违规（15 条假红全部来自这三份
    # 文件的夹具）。准入判据不变：仍须 --self-test 入口 + self_test()/_self_test()
    # 实现体（skip_exact_reason 机器自证；负例 S17/S18 覆盖）。
    "eng/ci/check_mutation_gates.py",                       # ② --self-test 夹具面
    "eng/ci/check_registration_anchors.py",                 # ② --self-test 夹具面
    "eng/tools/quality/contracts/check_symbol_dimension_uniqueness.py",  # ② 同上
    # ② FINAL-07 补登记：两文件的 `--self-test` 夹具按构造写入不存在的 docs/ 路径
    # （docs/owner/REFS.md、docs/owner/FORGED.md、docs/owner/MIXED.md、
    # docs/science/CAL.md…ACR.md、docs/science/NOPE.md）作为**应当被判红的样例文本**，
    # 被本门当悬空引用扫描 = 把负例当违规（实测 17 条假红全部来自这两份文件的夹具，
    # HEAD 基线同一判据为绿、不含这 17 条）。准入判据不变：两份均 `--self-test` +
    # `def _self_test(`/`def self_test(` 实现体（skip_exact_reason 机器自证）。
    "eng/tools/doccheck/check_version_namespaces.py",          # ② --self-test 夹具面
    "eng/tools/quality/contracts/check_traceability.py",        # ② --self-test 夹具面
)

# ②的机器判据：既要有 --self-test 入口，又要有 self_test()/_self_test() 实现体 ——
# 只在注释里提一句 "--self-test" 不算夹具面（防"塞注释过闸"）。
# 判据放宽到两种命名**不是放松**：判据的实质是「该文件确实存在一个可执行的
# 夹具面实现体」，而不是「函数名恰好不带下划线」；eng/ci/check_mutation_gates.py
# 的夹具面实现在 `_self_test()` 里（前导下划线是包内私有约定），原正则把它误判成
# 「未实现 self_test()」⇒ 该文件无法被登记，夹具里的 docs/ token 恒被判悬空。
SELFTEST_FLAG = "--self-test"
SELFTEST_ENTRY_RE = re.compile(r"def\s+_?self_test\s*\(")

# docs/... 指针 token：左边界禁止为路径字符（排除 URL 与 lib/.../docs/x 形态的误报）
TOKEN_RE = re.compile(r"(?<![A-Za-z0-9_./-])docs/[A-Za-z0-9_./-]*")
STRIP_CHARS = ".,;:)]}>。，）：；、”’" + BT + "*"
PLACEHOLDER_MARKERS = ("xxx", "yyy", "XXX", "YYY", "__missing__", "<", ">", "...", "…", "TODO")
RESOLVE_EXTS = ("", ".md", ".json", ".yaml", ".yml", ".csv", ".py", ".txt", ".rst")


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def _run(root: str, args: list) -> tuple:
    """git 调用：core.quotepath=false（非 ASCII 路径按原文返回，不转义）。"""
    try:
        r = subprocess.run(["git", "-c", "core.quotepath=false"] + args, cwd=root,
                           capture_output=True, text=True)
    except OSError as exc:
        return 127, "git unavailable: " + str(exc)
    return r.returncode, r.stdout


def git_tracked(root: str, rel: str) -> bool:
    # fail-closed：root 不存在/git 不可用时返回 False（调用方据此判红），不得 traceback。
    rc, _ = _run(root, ["ls-files", "--error-unmatch", "--", rel])
    return rc == 0


def git_ls(root: str) -> list:
    rc, out = _run(root, ["ls-files"])
    if rc != 0:
        return []
    return [x for x in out.splitlines() if x.strip()]


def is_archive_path(p: str) -> bool:
    return (p.startswith("docs/archive/") or p.startswith("engineering/control/archive/")
            or "/archive/" in "/" + p)


def _read_head(path: str, limit: int = 400) -> str:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read(limit)
    except OSError:
        return ""


def _read(path: str) -> str:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            return fh.read()
    except OSError:
        return ""


# --------------------------------------------------------------------------- #
# docs/... 指针解析（双向索引闭合面共用）
# --------------------------------------------------------------------------- #
def universe(root: str) -> tuple:
    """返回 (tracked 文件集合, tracked 目录集合（带尾斜杠）)。"""
    files = set(git_ls(root))
    dirs = set()
    for t in files:
        parts = t.split("/")
        for i in range(1, len(parts)):
            dirs.add("/".join(parts[:i]) + "/")
    return files, dirs


def _brace_expand(text: str, end: int, prefix: str) -> list:
    """token 结束于 '{' 时展开 {a,b,c}，返回 [(prefix+a+tail, 新结束位置)]。

    多行花括号（如 docs/standards/{A.md,\n  B.md}）的候选项按空白归一化后再拼接。
    """
    if end >= len(text) or text[end] != "{":
        return []
    close = text.find("}", end)
    if close < 0:
        return []
    alts = [_oneline(a) for a in text[end + 1:close].split(",")]
    tail_m = re.match(r"[A-Za-z0-9_./-]*", text[close + 1:])
    tail = tail_m.group(0) if tail_m else ""
    return [(prefix + a + tail, close + 1 + len(tail)) for a in alts if a]


def _oneline(s: str) -> str:
    """token 文本归一化为单行（台账键与出表列不得含换行）。"""
    return re.sub(r"\s+", " ", s).strip()


def _strip_tok(tok: str) -> str:
    t = _oneline(tok).rstrip(STRIP_CHARS)
    t = re.sub(r":[0-9]+(-[0-9]+)?$", "", t)  # :行号 / :行号-行号
    t = t.rstrip(".")
    return t


def _exists(tok: str, files: set, dirs: set) -> bool:
    if not tok or tok in ("docs/", "docs"):
        return True
    base = tok.rstrip("/")
    for ext in RESOLVE_EXTS:
        cand = base + ext
        if cand in files or cand in dirs:
            return True
    return (base + "/") in dirs


def token_resolution(text: str, m, files: set, dirs: set) -> tuple:
    """判定一个 docs/... token 是否可达。返回 (ok, 归一化 token 或说明)。"""
    tok = _strip_tok(m.group(0))
    if not tok or tok in ("docs/", "docs"):
        return True, tok
    if any(p in tok for p in PLACEHOLDER_MARKERS):
        return True, tok + "  (placeholder)"
    nxt = text[m.end():m.end() + 1]
    # 通配：docs/modules/registry/astrocs.phase*.md
    if nxt == "*":
        pref = tok.rstrip(".")
        if not pref:
            return True, tok
        hit = any(f.startswith(pref) for f in files) or any(d.startswith(pref) for d in dirs)
        return hit, pref + "*"
    # 花括号展开：docs/design/PHASE{1,2,3}_DETAILED_DESIGN.md
    exps = _brace_expand(text, m.end(), m.group(0))
    if exps:
        bad = [t for t, _ in exps if not _exists(_strip_tok(t), files, dirs)]
        return (not bad), _oneline("{" + ",".join(t for t, _ in exps) + "}")
    return _exists(tok, files, dirs), tok


def scan_dangling(root: str, rel: str, files: set, dirs: set) -> list:
    """返回 [(token, line)] —— 判词必须能定位到「文件:行」（GATE-TRIAGE-01）。"""
    text = _read(os.path.join(root, rel))
    out = []
    for m in TOKEN_RE.finditer(text):
        ok, tok = token_resolution(text, m, files, dirs)
        if not ok:
            out.append((tok, text.count("\n", 0, m.start()) + 1))
    return out


# --------------------------------------------------------------------------- #
# 内容真实性门（CONTENT-TRUTH）：节号锚存活
# --------------------------------------------------------------------------- #
# --- 折行补偿（前台 2026-09-30 点名的盲区）--------------------------------------
# TOKEN_RE 的字符类 [A-Za-z0-9_./-] **不含换行**：被 markdown 软折行切断的路径只会
# 匹配到前半截**目录**，紧随其后的 §N 锚整条丢失（漏报）；且 _oneline() 会把折行
# 归一成空格（`docs/science/ PHASE2_UPM.md`），即使匹配到也解析不出目标。
# 本门自带下列常量自处理折行，**只喂 section_anchor_refs**，不改动既有
# docs_path_refs_resolve 的匹配面（那是别人的门，判定行为不得被单方面改写）。
#
# 收紧口径（防误报，均为有意为之的保守选择）：
#   (1) 路径续行必须以 [A-Za-z0-9_.] 起头 —— `//`、`/*`、`#`、`>`、`-`
#       一律不算续行（否则 C++ 注释标记会被吞进路径，产出垃圾 token）；
#   (2) §N 跨行绑定只允许**一次**换行，且续行在 §N 之前只能是空白、或只含注释/列表标记；
#       出现任何正文实词即视为无关段落，**不绑定**（宁可漏，不可凭空造锚）；
#   (3) 续接后的路径若解析不到被跟踪 md，**不**回退到折行前的半截 token。
TOKEN_RE_WRAP = re.compile(
    r"(?<![A-Za-z0-9_./-])docs/[A-Za-z0-9_./-]*"
    r"(?:\n[ \t\u3000]+[A-Za-z0-9_.][A-Za-z0-9_./-]*)*")
ANCHOR_GAP_WRAP_RE = re.compile(
    r"(?:[ \t\u3000\x60(\uff08])*"
    r"(?:\n[ \t\u3000]*(?:(?://|/\*|\*|#|>|\|)[ \t\u3000]*)*)?"
    r"(?:[ \t\u3000\x60(\uff08])*")


def _unwrap_path(tok: str) -> str:
    """折行路径归一：**删除**换行与续行缩进（不是折叠成空格）。"""
    return re.sub(r"\n[ \t\u3000]*", "", tok)


def doc_section_index(root: str, rel: str) -> tuple:
    """解析目标文档的节号索引。返回 (模式A节号集, 模式B节号集, 模式C键集)。

    模式 A = 编号标题（`## 4.5 ...` 多级）；模式 B = 标题正文里的显式 `（§N）`；
    模式 C = 父节正文里的有序列表第 M 项（键为 "<父节>.<项号>"）。
    """
    nums, inline, items = set(), set(), set()
    cur = None
    for ln in _read(os.path.join(root, rel)).splitlines():
        m = HEAD_NUM_RE.match(ln)
        if m:
            nums.add(m.group(1))
            cur = m.group(1)
            continue
        h = HEAD_ANY_RE.match(ln)
        if h:
            # 标题行里的显式节号（模式 B）。cur 保持不变，`（§2.1.1）` 挂在当前父节下。
            for im in INLINE_SEC_RE.finditer(h.group(1)):
                inline.add(im.group(1))
                if cur:
                    inline.add(cur + "." + im.group(1).split(".")[-1])
            continue
        if cur:
            li = LIST_ITEM_RE.match(ln)
            if li:
                items.add(cur + "." + li.group(1))
    return nums, inline, items


def section_anchor_refs(root: str, rel: str, files: set) -> list:
    """扫出一个文件里的节号锚引用。

    返回 [(num, target_rel, line, wrapped)]，只含「目标解析得到一个被跟踪的 md 文件」的引用。
    绑定规则 = **紧邻 + 折行补偿**：路径 token 之后只允许空白/反引号/圆括号再接 §N；
    额外允许 TOKEN_RE_WRAP / ANCHOR_GAP_WRAP_RE 描述的**一次** markdown 软折行
    （口径与理由见本节上方注释）。wrapped=True 表示这条锚是靠折行补偿才捞回来的。
    """
    text = _read(os.path.join(root, rel))
    out = []
    for m in TOKEN_RE_WRAP.finditer(text):
        raw = m.group(0)
        gap = text[m.end():m.end() + 64]
        gm = ANCHOR_GAP_WRAP_RE.match(gap)
        if not gm:
            continue
        mm = SEC_ANCHOR_RE.match(gap[gm.end():])
        if not mm:
            continue
        num = next(g for g in mm.groups() if g)
        wrapped = "\n" in raw or "\n" in gap[:gm.end()]
        base = _strip_tok(_unwrap_path(raw))
        if not base or any(p in base for p in PLACEHOLDER_MARKERS):
            continue
        # 目标解析：只接受**被 git 跟踪的 md 文件**。非 md（yaml/csv/…）与目录不可判，
        # 由调用方计入 undecidable 面并打印（不得静默）。
        # 折行续接后若解析不到 ⇒ **不回退**到半截 token（回退会把目录当文件，制造假绿）。
        tgt = None
        for ext in RESOLVE_EXTS:
            cand = base + ext
            if cand in files and cand.endswith(".md"):
                tgt = cand
                break
        if not tgt:
            continue
        out.append((num, tgt, text.count("\n", 0, m.start()) + 1, wrapped))
    return out


def check_content_truth(root: str, scan_files: list, tset: set) -> dict:
    """T1 节号锚存活：路径可达但目标页没有该节号 ⇒ 判红。

    fail-closed：台账不可读 / 扫描面为空 / 归属到本门的节号锚总数为 0 ⇒ 判红
    （解析器空转不得判绿，见 run/FINAL-07/content-truth/实现说明.md §判别力）。
    """
    ledger, ldetail = _load_content_ledger(root)
    if ledger is None:
        return check("docs_section_refs_resolve", False,
                     "内容台账不可用（fail-closed）：" + ldetail)

    cache = {}

    def idx(t):
        if t not in cache:
            cache[t] = doc_section_index(root, t)
        return cache[t]

    judged = dead = total = undecidable = design_excluded = 0
    wrapped = 0
    findings = []
    for rel, _scope in scan_files:
        for num, tgt, ln, is_wrapped in section_anchor_refs(root, rel, tset):
            total += 1
            if is_wrapped:
                wrapped += 1
            if tgt == DESIGN_DOC:
                # 归兄弟门面 check_design_section_refs.py 专责（其 S2/S3 已判）。
                # 显式排除而非静默跳过：口径写在代码常量里，不做暗箱。
                design_excluded += 1
                continue
            A, B, C = idx(tgt)
            if not A and not B and not C:
                # 目标整篇没有编号标题（无章法文档）⇒ 不可判，不判红也不静默。
                undecidable += 1
                continue
            judged += 1
            if num in A or num in B or num in C:
                continue
            key = (rel, tgt, num)
            if key in ledger:
                continue
            dead += 1
            same_level = sorted(x for x in (A | B | C)
                                if x.split(".")[0] == num.split(".")[0])[:8]
            findings.append("%s:%d §%s -> %s（该目标实际有的同层节号=%s）"
                            % (rel, ln, num, tgt, repr(same_level)))

    detail = ("节号锚 %d 条（归本门判 %d；其中折行补偿捞回 %d 条；"
              "ASTROCS_DESIGN 归兄弟门面 %d 条不计；目标无章法不可判 %d 条）；"
              "真死锚 %d 条；台账=%s"
              % (total, judged, wrapped, design_excluded, undecidable, dead, ldetail))
    if total == 0:
        return check("docs_section_refs_resolve", False,
                     "扫描面为空（fail-closed）：归属到本门的节号锚 0 条，解析器不得空转判绿；"
                     + detail)
    if dead:
        return check("docs_section_refs_resolve", False,
                     "CONTENT_REF_DEAD " + str(dead) + " 条：" + repr(findings[:12]) + "；"
                     + detail)
    return check("docs_section_refs_resolve", True, detail)


def _load_content_ledger(root: str) -> tuple:
    """内容真实性台账：已登记的真死锚（跨域未修项）。读不到 ⇒ (None, 原因) 判红。"""
    full = os.path.join(root, CONTENT_TRUTH_LEDGER)
    if not os.path.isfile(full):
        return None, CONTENT_TRUTH_LEDGER + " 不存在"
    try:
        data = json.loads(_read(full))
    except Exception as exc:  # noqa: BLE001
        return None, "台账不可解析: " + str(exc)
    entries = data.get("entries")
    if not isinstance(entries, list) or not entries:
        return None, "台账 entries 为空"
    if int(data.get("max_entries", -1)) > CONTENT_TRUTH_LEDGER_MAX:
        return None, ("台账 max_entries 超过冻结上限 " + str(CONTENT_TRUTH_LEDGER_MAX)
                      + "（只减不增）")
    for e in entries:
        if not all(e.get(k) for k in ("file", "target", "section", "owner", "reason")):
            return None, "台账条目缺 file/target/section/owner/reason: " + repr(e)[:120]
    return ({(e["file"], e["target"], e["section"]): e for e in entries},
            str(len(entries)) + " 条（上限 " + str(CONTENT_TRUTH_LEDGER_MAX) + "）")


def load_ledger(root: str) -> tuple:
    """跨域未修悬空引用台账（收口域）。读不到 ⇒ (None, 原因)，调用方判红。"""
    full = os.path.join(root, LEDGER_PATH)
    if not os.path.isfile(full):
        return None, LEDGER_PATH + " 不存在"
    try:
        data = json.loads(_read(full))
    except Exception as exc:  # noqa: BLE001
        return None, "台账不可解析: " + str(exc)
    entries = data.get("entries")
    if not isinstance(entries, list) or not entries:
        return None, "台账 entries 为空"
    if int(data.get("max_entries", -1)) > LEDGER_MAX:
        return None, "台账 max_entries 超过冻结上限 " + str(LEDGER_MAX) + "（只减不增）"
    for e in entries:
        if not all(e.get(k) for k in ("file", "token", "owner", "reason")):
            return None, "台账条目缺 file/token/owner/reason: " + repr(e)[:120]
    return {(e["file"], e["token"]): e for e in entries}, str(len(entries)) + " 条（上限 " + str(LEDGER_MAX) + "）"


def skip_exact_reason(root: str, rel: str) -> str:
    """SKIP_EXACT 单条准入自证：返回 "" = 有据，否则返回判红原因。

    依据：ENGINEERING_SPEC §8.4（悬空即缺陷）、§10（锚存活 / fail-closed / 可执行负例面）。
    规则（见 SKIP_EXACT 准入规则）：① 台账本身；② 自带 --self-test 夹具面的检查器。
    """
    if rel == LEDGER_PATH:
        return ""
    full = os.path.join(root, rel)
    if not os.path.isfile(full):
        return rel + " 不存在（排除面条目是硬引用，必须锚存活；ENGINEERING_SPEC §10）"
    text = _read(full)
    if SELFTEST_FLAG not in text:
        return rel + " 无 " + SELFTEST_FLAG + " 入口（非夹具面文件，不得逐文件排除）"
    if not SELFTEST_ENTRY_RE.search(text):
        return rel + " 未实现 self_test()（只提 " + SELFTEST_FLAG + " 不算夹具面）"
    return ""


def check_skip_exact_justified(root: str) -> dict:
    """扫描面逐文件排除项自证（检查 13）：SKIP_EXACT 每条必须①是台账 或 ②自带夹具面。"""
    bad = [(rel, why) for rel in SKIP_EXACT
           for why in (skip_exact_reason(root, rel),) if why]
    if LEDGER_PATH not in SKIP_EXACT:
        bad.append((LEDGER_PATH, "台账必须留在排除面内（否则台账自身的 token 被当悬空引用）"))
    detail = ("无据排除=" + repr(bad[:5]) + " 共" + str(len(bad))
              + "（SKIP_EXACT 只允许：台账 / 自带 --self-test 夹具面的检查器）") if bad else \
             ("SKIP_EXACT " + str(len(SKIP_EXACT)) + " 条逐条有据（台账 + 自带 --self-test 夹具面）")
    return check("skip_exact_justified", not bad, detail)


def header_region(lines: list, max_lines: int = 25) -> list:
    out = []
    for ln in lines[:max_lines]:
        if ln.startswith("## "):
            break
        out.append(ln)
    return out


def has_upstream_header(text: str) -> bool:
    return any(("上游" in ln and "ASTROCS_DESIGN.md" in ln)
               for ln in header_region(text.splitlines()))


def index_status_map(doc_index: dict) -> dict:
    out = {}
    for sec in ("active", "archived"):
        for e in (doc_index.get(sec) or []):
            if isinstance(e, dict) and e.get("path"):
                out[str(e["path"])] = str(e.get("status", ""))
    return out


def status_of(path: str, status_map: dict, default: str = "ACTIVE_INFORMATIVE") -> str:
    """精确路径优先；否则取最长前缀条目（目录级登记）的状态。"""
    if path in status_map:
        return status_map[path]
    best = None
    for k, v in status_map.items():
        kk = k.rstrip("/")
        if path.startswith(kk + "/") and (best is None or len(kk) > len(best[0])):
            best = (kk, v)
    return best[1] if best else default


def _covered(path: str, covered_paths: list) -> bool:
    for cp in covered_paths:
        if path == cp or path.startswith(cp.rstrip("/") + "/"):
            return True
    return False


def run_checks(root: str, strict: bool) -> tuple:
    results = []
    machine_index = []

    # fail-closed：--root 指向不存在目录 ⇒ 立即判红并给出 ANCHOR_STALE。
    if not os.path.isdir(root):
        print("ANCHOR_STALE: " + root + " (--root 指向的仓库目录不存在)")
        for name in ("anchor_files_alive", "index_file_exists", "index_file_git_tracked",
                     "yaml_parse", "index_entry_paths_exist", "docs_fully_covered",
                     "root_docs_doc_links_resolve", "subordinate_docs_registered",
                     "subordinate_docs_upstream_header", "docs_path_refs_resolve",
                     "docs_section_refs_resolve",
                     "skip_exact_justified"):
            results.append(check(name, False, "仓库根不存在（fail-closed）：" + root))
        results.append(check("retired_authority_not_reintroduced", True,
                             "仓库根不存在：无旧权威可判（已由 anchor/index 项判红）"))
        return results, machine_index

    try:
        import yaml  # type: ignore
    except Exception:
        yaml = None

    index_full = os.path.join(root, INDEX_PATH)
    exists = os.path.isfile(index_full)
    results.append(check("index_file_exists", exists, INDEX_PATH))
    tracked = exists and git_tracked(root, INDEX_PATH)
    results.append(check("index_file_git_tracked", tracked,
                         "git ls-files --error-unmatch -- " + INDEX_PATH))

    doc_index = {}
    if exists:
        if yaml is None:
            results.append(check("yaml_parse", False, "pyyaml 不可用，跳过内容解析"))
        else:
            try:
                data = yaml.safe_load(_read(index_full))
                doc_index = (data or {}).get("doc_index", {})
                results.append(check("yaml_parse", True, "ok"))
            except Exception as exc:  # noqa: BLE001
                results.append(check("yaml_parse", False, str(exc)))
    else:
        results.append(check("yaml_parse", False, "索引文件缺失（fail-closed）"))

    active = doc_index.get("active", []) or []
    archived = doc_index.get("archived", []) or []
    all_entries = active + archived
    covered_paths = [str(e.get("path", "")) for e in all_entries if isinstance(e, dict)]

    bad_status = [e.get("path") for e in all_entries if e.get("status") not in LEGAL]
    results.append(check("status_legal", not bad_status,
                         "非法=" + repr(bad_status) if bad_status else "ok"))
    paths = [e.get("path", "") for e in all_entries]
    dups = sorted({p for p in paths if paths.count(p) > 1})
    results.append(check("path_unique", not dups, "重复=" + repr(dups) if dups else "ok"))
    bad_active = [e.get("path") for e in active if is_archive_path(str(e.get("path", "")))]
    results.append(check("active_no_archive", not bad_active,
                         "active 含 archive=" + repr(bad_active) if bad_active else "ok"))
    bad_arch_st = [e.get("path") for e in archived if e.get("status") != "ARCHIVED_NON_NORMATIVE"]
    results.append(check("archived_all_archived_status", not bad_arch_st,
                         "非ARCHIVED=" + repr(bad_arch_st) if bad_arch_st else "ok"))

    # --- 条目路径必须存在（悬空条目即缺陷）---
    tracked_all = git_ls(root)
    tset = set(tracked_all)
    dirs = set()
    for t in tracked_all:
        parts = t.split("/")
        for i in range(1, len(parts)):
            dirs.add("/".join(parts[:i]) + "/")
    missing_entries = [p for p in covered_paths
                       if p and p != INDEX_PATH and not _exists(p, tset, dirs)]
    results.append(check("index_entry_paths_exist", not missing_entries,
                         ("悬空条目=" + repr(missing_entries[:10]) + " 共" + str(len(missing_entries)))
                         if missing_entries
                         else str(len(covered_paths)) + " 条条目路径全部存在"))

    # --- 覆盖：git tracked docs/** md/rst/txt/yaml ---
    if tracked:
        docfiles = [p for p in tracked_all
                    if p.startswith("docs/") and p.endswith((".md", ".rst", ".txt", ".yaml", ".yml"))
                    and p != INDEX_PATH
                    and os.path.basename(p) != "README.md"]  # 目录 README=招牌件，按「每目录必有 README」治理，不入规范索引面
        uncovered = [p for p in docfiles if not _covered(p, covered_paths)]
        results.append(check("docs_fully_covered", not uncovered,
                             ("未覆盖=" + repr(uncovered[:10]) + " 共" + str(len(uncovered)))
                             if uncovered else "覆盖 " + str(len(docfiles)) + " 文件"))
    else:
        results.append(check("docs_fully_covered", False,
                             "index 未跟踪/不存在，覆盖检查无法执行（fail-closed）"))

    # --- 最高设计与根文档的 docs/... 指针可达 ---
    missing_root = [p for p in ROOT_DOCS if not os.path.isfile(os.path.join(root, p))]
    root_findings = []
    root_total = 0
    for p in ROOT_DOCS:
        if p in missing_root:
            continue
        text = _read(os.path.join(root, p))
        for m in TOKEN_RE.finditer(text):
            root_total += 1
            ok, tok = token_resolution(text, m, tset, dirs)
            if not ok:
                root_findings.append((p, tok))
    ok_root = (not missing_root) and (not root_findings) and root_total > 0
    detail_root = ("根文档 " + str(len(ROOT_DOCS)) + " 份、docs/ 指针 " + str(root_total)
                   + " 个全部可达" if ok_root else
                   "缺失根文档=" + repr(missing_root) + "；悬空指针=" + repr(root_findings[:10])
                   + "（共" + str(len(root_findings)) + "）")
    results.append(check("root_docs_doc_links_resolve", ok_root, detail_root))

    # --- 下级文档：登记 + 抬头上游条款 ---
    status_map = index_status_map(doc_index)
    subordinate = [p for p in tracked_all
                   if p.startswith("docs/") and p.endswith(".md")
                   and p not in ROOT_DOCS
                   and os.path.basename(p) != "README.md"  # 目录 README=招牌件，不要求上游抬头
                   and status_of(p, status_map).startswith("ACTIVE_")]
    unregistered = [p for p in subordinate if not _covered(p, covered_paths)]
    missing_header = [p for p in subordinate
                      if not has_upstream_header(_read(os.path.join(root, p)))]
    fail_closed_docs = not subordinate
    results.append(check("subordinate_docs_registered",
                         (not unregistered) and not fail_closed_docs,
                         "扫描面为空（fail-closed）" if fail_closed_docs else
                         (("未登记=" + repr(unregistered[:10]) + " 共" + str(len(unregistered)))
                          if unregistered
                          else str(len(subordinate)) + " 份下级文档全部在索引登记")))
    results.append(check("subordinate_docs_upstream_header",
                         (not missing_header) and not fail_closed_docs,
                         "扫描面为空（fail-closed）" if fail_closed_docs else
                         (("缺抬头=" + repr(missing_header[:10]) + " 共" + str(len(missing_header)))
                          if missing_header
                          else str(len(subordinate)) + " 份下级文档抬头均有「上游」条款区（100%）")))

    # --- 扫描面排除项自证：SKIP_EXACT 每条必须"有据"（准入规则见 SKIP_EXACT 定义处）---
    results.append(check_skip_exact_justified(root))

    # --- 文档与代码注释中的 docs/ 路径可达（跨域未修项走显式台账）---
    # 扫描面构建与既有 docs_path_refs_resolve 共用（CONTENT-TRUTH 门复用同一份面，
    # 保证两门分母同源；**判定逻辑未变**，只是把构建提到分支之前）。
    scan_files = []
    for p in ROOT_DOCS:
        if os.path.isfile(os.path.join(root, p)):
            scan_files.append((p, "root"))
    for p in tracked_all:
        if (p.startswith(SKIP_DIRS) or not p.endswith(SCAN_EXT) or p in SKIP_EXACT
                or p.endswith(SKIP_SUFFIX) or any(s in p for s in SKIP_CONTAINS)):
            continue  # 台账/自检夹具/历史命名空间/第三方源码不入扫描面
        if p.startswith("docs/"):
            if p.endswith(".md"):
                scan_files.append((p, "docs"))
        elif p.startswith(CODE_SCAN_DIRS):
            scan_files.append((p, "code"))

    ledger, ledger_detail = load_ledger(root)
    if ledger is None:
        results.append(check("docs_path_refs_resolve", False,
                             "台账不可用（fail-closed）：" + ledger_detail))
    else:
        dangling_all = []
        for rel, scope in scan_files:
            for tok, ln in scan_dangling(root, rel, tset, dirs):
                dangling_all.append((rel, tok, scope, ln))
        new_dangling = [x for x in dangling_all if (x[0], x[1]) not in ledger]
        seen = {(f, t) for f, t, _s, _l in dangling_all}
        resolved_ledger = [k for k in ledger if k not in seen]
        detail = ("扫描 " + str(len(scan_files)) + " 文件；台账=" + ledger_detail
                  + "；台账中已消除=" + str(len(resolved_ledger)) + " 条（可从台账移除）")
        # 判词不截断且带「文件:行」（GATE-TRIAGE-01）：原实现 `new_dangling[:10]`
        # 只列前 10 条且不含行号 ⇒ 读者无法定位到具体位置。
        rendered = ["%s:%d %s" % (f, ln, t) for f, t, _s, ln in new_dangling]
        results.append(check("docs_path_refs_resolve", not new_dangling,
                             ("未登记悬空=" + repr(rendered) + " 共"
                              + str(len(new_dangling)) + "；" + detail)
                             if new_dangling else ("未登记悬空=0；" + detail)))

    # --- 内容真实性：节号锚存活（路径可达但目标页没有该节号 ⇒ 判红）---
    results.append(check_content_truth(root, scan_files, tset))


    # --- 归档边界 ---
    archive_md = [p for p in tracked_all if p.startswith("docs/archive/") and p.endswith(".md")]
    no_header = []
    for p in archive_md:
        head = _read_head(os.path.join(root, p))
        if not head:
            no_header.append(p + "(读失败)")
        elif "ARCHIVED" not in head:
            no_header.append(p)
    results.append(check("archive_md_has_archived_marker", not no_header,
                         ("缺标记=" + repr(no_header)) if no_header
                         else "docs/archive " + str(len(archive_md)) + " 个 md 全含 ARCHIVED"))
    ctrl_archive_dir = os.path.join(root, "engineering/control/archive")
    if not os.path.isdir(ctrl_archive_dir):
        ctrl_ok, ctrl_detail = True, "engineering/control/archive 不存在（已整体出库，判 PASS）"
    else:
        ctrl_marks = [t for t in tracked_all
                      if t.startswith("engineering/control/archive/")
                      and "ARCHIVED" in os.path.basename(t).upper()]
        ctrl_ok = bool(ctrl_marks)
        ctrl_detail = ("工程控制归档目录含 ARCHIVED 标记文件: " + repr(sorted(ctrl_marks)[:3])
                       if ctrl_ok else "engineering/control/archive 存在但无 ARCHIVED 标记文件")
    results.append(check("control_archive_dir_readme", ctrl_ok, ctrl_detail))

    # --- 根无散落旧控制包（非 ASCII 路径必须能被看见：core.quotepath=false）---
    # 现行控制包目录允许在位；其余 工程控制/** 一律判红（旧包残留）。
    pack_prefix = CURRENT_CONTROL_PACK + "/"
    allowed_prefixes = (pack_prefix,) + tuple(x + "/" for x in ARCHIVED_CONTROL_PACKS)
    leftover = [p for p in tracked_all
                if p.startswith("工程控制/") and p != "工程控制/README.md"
                and not p.startswith(allowed_prefixes)]  # 目录 README=招牌件，非控制包残留
    # 白名单条目必须自带 SUMMARY.md（收口证明），否则仍判红
    for ap in ARCHIVED_CONTROL_PACKS:
        present = any(t.startswith(ap + "/") for t in tracked_all)
        if present and not any(t == ap + "/SUMMARY.md" for t in tracked_all):
            leftover = leftover + [ap + "/ (归档白名单条目缺 SUMMARY.md：未收口)"]
    pack_visible = [p for p in tracked_all if p.startswith(pack_prefix)]
    # 反向自证（防 core.quotepath 转义回归）：目录在磁盘上存在却"看不见"任何
    # tracked 文件 ⇒ 非 ASCII 路径被转义，判红并点名。
    pack_dir_on_disk = os.path.isdir(os.path.join(root, CURRENT_CONTROL_PACK))
    quotepath_blind = pack_dir_on_disk and not pack_visible
    if quotepath_blind:
        leftover = leftover + [CURRENT_CONTROL_PACK + "/ (目录存在但 git ls-files 不可见："
                               "core.quotepath 转义回归)"]
    results.append(check("no_legacy_工程控制_left", not leftover,
                         ("残留=" + repr(leftover[:5])) if leftover
                         else "ok（现行包 " + CURRENT_CONTROL_PACK + " 在位，tracked "
                              + str(len(pack_visible)) + " 文件可见）"))
    legacy_root = [p for p in tracked_all
                   if "/" not in p and p.endswith(".zip")
                   and ("CONTROL" in p or "AUDIT" in p or "REVIEW" in p or "PACK" in p)]
    results.append(check("no_root_legacy_control_zip", not legacy_root,
                         ("残留=" + repr(legacy_root)) if legacy_root else "ok"))

    results.extend(check_anchor_survival(root, strict))
    results.extend(check_old_authority_regression(root))

    for e in all_entries:
        machine_index.append({"path": e.get("path", ""), "status": e.get("status", "")})
    machine_index = sorted(machine_index, key=lambda x: x["path"])
    return results, machine_index


def check_anchor_survival(root: str, strict: bool) -> list:
    out = []
    missing = [p for p in ANCHOR_PATHS if not os.path.exists(os.path.join(root, p))]
    for p in missing:
        print(("ANCHOR_STALE: " + p) if strict
              else ("ANCHOR_ABSENT: " + p + " (skipped_anchor_absent)"))
    detail = ("all live anchors present: " + ", ".join(ANCHOR_PATHS)) if not missing else (
        "ANCHOR_STALE=" + repr(missing) if strict
        else "skipped_anchor_absent=" + repr(missing))
    out.append(check("anchor_files_alive", not (missing and strict), detail))
    retired_present = [p for p in (a["path"] for a in RETIRED_ANCHORS)
                       if os.path.exists(os.path.join(root, p))]
    for p in (a["path"] for a in RETIRED_ANCHORS):
        if p not in retired_present:
            print("ANCHOR_RETIRED_OK: " + p + " (退役锚，缺失=出库完成态)")
    out.append(check("anchor_retired_not_required", True,
                     "退役锚（不判红）: " + ", ".join(a["path"] for a in RETIRED_ANCHORS)
                     + "；现存=" + repr(retired_present)))
    return out


def _archived_index_paths(root: str) -> set:
    try:
        import yaml  # type: ignore
    except Exception:
        return set()
    try:
        data = yaml.safe_load(_read(os.path.join(root, INDEX_PATH))) or {}
    except Exception:  # noqa: BLE001
        return set()
    out = set()
    for e in ((data.get("doc_index") or {}).get("archived") or []):
        p = (e or {}).get("path")
        if p:
            out.add(str(p))
    return out


def _declares_archived(root: str, rel: str, archived_index: set) -> bool:
    norm = rel.rstrip("/")
    for a in archived_index:
        a = a.rstrip("/")
        if a == norm or a.startswith(norm + "/") or norm.startswith(a + "/"):
            return True
    full = os.path.join(root, rel)
    if os.path.isfile(full):
        return any(m in _read_head(full) for m in ARCHIVE_DECL_MARKERS)
    return False


def check_old_authority_regression(root: str) -> list:
    archived_index = _archived_index_paths(root)
    tracked = git_ls(root)
    offenders = []
    declared = []
    for rel in RETIRED_DOC_PATHS:
        full = os.path.join(root, rel)
        if os.path.isdir(full):
            for t in tracked:
                if t == rel.rstrip("/") or t.startswith(rel.rstrip("/") + "/"):
                    if _declares_archived(root, t, archived_index):
                        declared.append(t)
                    else:
                        offenders.append(t)
        elif os.path.isfile(full):
            if _declares_archived(root, rel, archived_index):
                declared.append(rel)
            else:
                offenders.append(rel)
    for d in RETIRED_ANCHOR_DIRS:
        if not os.path.isdir(os.path.join(root, d)):
            continue
        marks = [t for t in tracked if t.startswith(d.rstrip("/") + "/")
                 and "ARCHIVED" in os.path.basename(t).upper()]
        if not marks:
            offenders.append(d + "/ (目录存在但无 ARCHIVED 标记文件)")
        else:
            declared.extend(marks[:3])
    ok = not offenders
    detail = ("退役旧文档集未重新出现（已声明归档副本 "
              + repr(sorted(declared)[:5]) + " 共" + str(len(declared)) + "）") if ok else \
             ("旧权威回归（未声明归档）=" + repr(sorted(offenders)[:10]) + " 共"
              + str(len(offenders)))
    return [check("retired_authority_not_reintroduced", ok, detail)]


# --------------------------------------------------------------------------- #
# 机器出表（证据面）
# --------------------------------------------------------------------------- #
def dump(root: str, kind: str) -> int:
    if not os.path.isdir(root):
        print("ANCHOR_STALE: " + root, file=sys.stderr)
        return 2
    tset, dirs = universe(root)
    if kind == "design-pointers":
        print("root_doc" + T + "token" + T + "status")
        for p in ROOT_DOCS:
            if not os.path.isfile(os.path.join(root, p)):
                print(p + T + "<missing root doc>" + T + "FAIL")
                continue
            text = _read(os.path.join(root, p))
            for m in TOKEN_RE.finditer(text):
                ok, tok = token_resolution(text, m, tset, dirs)
                print(p + T + tok + T + ("OK" if ok else "DANGLING"))
        return 0
    if kind == "subordinate-docs":
        try:
            import yaml  # type: ignore
            data = yaml.safe_load(_read(os.path.join(root, INDEX_PATH))) or {}
        except Exception:  # noqa: BLE001
            print("index unreadable", file=sys.stderr)
            return 2
        smap = index_status_map((data or {}).get("doc_index") or {})
        covered = list(smap.keys())
        print("doc" + T + "status" + T + "registered" + T + "upstream_header")
        for p in sorted(t for t in tset if t.startswith("docs/") and t.endswith(".md")):
            st = status_of(p, smap)
            if not st.startswith("ACTIVE_"):
                continue
            print(p + T + st + T + ("yes" if _covered(p, covered) else "NO") + T
                  + ("yes" if has_upstream_header(_read(os.path.join(root, p))) else "NO"))
        return 0
    if kind == "dangling":
        print("file" + T + "line" + T + "token" + T + "scope")
        for p in ROOT_DOCS:
            if os.path.isfile(os.path.join(root, p)):
                for tok, ln in scan_dangling(root, p, tset, dirs):
                    print(p + T + str(ln) + T + tok + T + "root")
        for p in sorted(tset):
            if (p.startswith(SKIP_DIRS) or not p.endswith(SCAN_EXT) or p in SKIP_EXACT
                    or p.endswith(SKIP_SUFFIX) or any(s in p for s in SKIP_CONTAINS)):
                continue
            if p.startswith("docs/"):
                scope = "docs" if p.endswith(".md") else None
            elif p.startswith(CODE_SCAN_DIRS):
                scope = "code"
            else:
                scope = None
            if not scope:
                continue
            for tok, ln in scan_dangling(root, p, tset, dirs):
                print(p + T + str(ln) + T + tok + T + scope)
        return 0
    if kind == "entries":
        try:
            import yaml  # type: ignore
            data = yaml.safe_load(_read(os.path.join(root, INDEX_PATH))) or {}
        except Exception:  # noqa: BLE001
            print("index unreadable", file=sys.stderr)
            return 2
        di = (data or {}).get("doc_index") or {}
        print("section" + T + "path" + T + "status" + T + "exists" + T + "duty" + T + "upstream")
        for sec in ("active", "archived"):
            for e in (di.get(sec) or []):
                p = str(e.get("path", ""))
                print(sec + T + p + T + str(e.get("status", "")) + T
                      + ("yes" if _exists(p, tset, dirs) else "NO") + T
                      + str(e.get("duty", ""))[:60] + T + str(e.get("upstream", ""))[:60])
        return 0
    print("unknown dump kind: " + kind, file=sys.stderr)
    return 2


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="DOC-403 双向层级索引闭合门（DOC-INDEX）")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true",
                    help="锚缺失判红（ANCHOR_STALE）；非 strict 记 skipped_anchor_absent")
    ap.add_argument("--self-test", dest="self_test", action="store_true",
                    help="tempfile mini-repo 正/负例自证（不依赖真仓）")
    ap.add_argument("--quiet", action="store_true", help="只打结论摘要，不打 machine_index")
    ap.add_argument("--dump", default=None,
                    help="机器出表：design-pointers / subordinate-docs / dangling / entries")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    if args.dump:
        return dump(root, args.dump)

    results, machine_index = run_checks(root, args.strict)
    passed = all(r["pass"] for r in results)
    out = {
        "tool": "eng/tools/doccheck/check_doc_index.py",
        "version": "2.0.0",
        "task": "DOC-403",
        "root": root,
        "strict": bool(args.strict),
        "results": sorted(results, key=lambda r: r["check"]),
        "machine_index_count": len(machine_index),
        "machine_index": [] if args.quiet else machine_index,
        "verdict": "DOC_INDEX_PASS" if passed else "DOC_INDEX_FAIL",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    print(text)
    return 0 if passed else 1


# --------------------------------------------------------------------------- #
# 可执行负例面（ENGINEERING_SPEC §10）：tempfile mini-repo，不依赖真仓状态
# --------------------------------------------------------------------------- #
# 概览页的落位目录。真仓 2026-09 文档迁移后五份概览都在 docs/engineering/（docs/owner/ 已出库，
# 磁盘上残留的空目录对门不可见：git 不跟踪空目录）。mini-repo 必须与真仓同向，
# 否则 ROOT_DOC_FILES 里的 docs/engineering/SCIENCE_OVERVIEW.md 会指向不存在的文件。
OVERVIEW_DIR = "docs/engineering"
OVERVIEW_DOCS = ("SCIENCE_OVERVIEW.md", "PIPELINE_OVERVIEW.md", "ARCHITECTURE_OVERVIEW.md",
                 "RELEASE_STATUS.md", "CHANGE_REVIEW.md")
ROOT_DOC_FILES = {
    "docs/ASTROCS_DESIGN.md": "# 最高设计\n\n科学公式见 docs/engineering/SCIENCE_OVERVIEW.md；"
                         "索引见 docs/DOCUMENT_INDEX.yaml。\n" + ("顶层设计正文。 " * 30) + "\n",
    "AGENTS.md": "# AGENTS\n\n读法见 docs/DOCUMENT_INDEX.yaml。\n" + ("干活手册。 " * 30) + "\n",
    # docs/ci/ 在真仓是**空目录残留**（test -e EXISTS 但 git ls-files 0 行）——空目录不提供
    # 内容，git 不跟踪它 ⇒ 对本门等同于不存在。迁移后的文档集在 docs/engineering/。
    "ENGINEERING_SPEC.md": "# 工程规范\n\n文档集见 docs/engineering/。\n" + ("工程规范正文。 " * 30) + "\n",
    "CONTROL_PACK_SPEC.md": "# 控制包规范\n\n" + ("控制包规范正文。 " * 30) + "\n",
    "ACCEPTANCE_SPEC.md": "# 验收规范\n\n" + ("验收规范正文。 " * 30) + "\n",
    "README.md": "# README\n\n导航见 docs/DOCUMENT_INDEX.yaml。\n" + ("入口说明。 " * 30) + "\n",
    "DEPENDENCIES.md": "# 依赖\n\n" + ("依赖清单。 " * 30) + "\n",
}


def _idx_entry(path: str, status: str = "ACTIVE_NORMATIVE") -> str:
    """一条 DOCUMENT_INDEX.yaml 条目（夹具用）。"""
    return "    - path: \"" + path + "\"\n      status: " + status + "\n"


# 概览页**逐篇**登记，不登记 docs/engineering 目录。两个理由：
#   ① 迁移前登记的是 docs/owner 目录，迁移后目录本身已出库，逐篇登记与之等价；
#   ② 若登记 docs/engineering 目录，S6（docs/engineering/CONFIG_CONTRACT.md 应当
#      暴露 docs_fully_covered 缺口）会被目录覆盖面吞掉 —— 负例会失去牙。
BASE_INDEX = (
    "doc_index:\n"
    "  active:\n"
    + _idx_entry("docs/ASTROCS_DESIGN.md")
    + "".join(_idx_entry(OVERVIEW_DIR + "/" + d) for d in OVERVIEW_DOCS)
    + _idx_entry("docs/engineering/01_CHECKS.md")
    + _idx_entry("docs/DOC-L0.md")
    + "  archived:\n"
    + _idx_entry("docs/archive", "ARCHIVED_NON_NORMATIVE")
    + _idx_entry("docs/review", "ARCHIVED_NON_NORMATIVE")
)
LEDGER_JSON = {
    "tool": "eng/tools/doccheck/dangling_ledger.json",
    "max_entries": 1,
    "entries": [
        {"file": "lib/x/foo.cpp", "token": "docs/legacy/OLD.md",
         "owner": "FIX-404", "reason": "自检夹具：跨域未修悬空引用"},
    ],
}
CONTENT_LEDGER_JSON = {
    "tool": "eng/tools/doccheck/content_truth_ledger.json",
    "max_entries": 1,
    "entries": [
        # 本门判的是「路径可达但目标页没有该节号」，所以登记的**目标文件必须存在**。
        # 原夹具写 docs/owner/DEAD.md（文件根本不存在）⇒ 那是一条路径级悬空，
        # 由 docs_path_refs_resolve 判红，不是本门要测的东西。改指真实存在、
        # 但确实没有 §9.9 的 docs/engineering/SCIENCE_OVERVIEW.md。
        {"file": "lib/x/foo.cpp", "target": OVERVIEW_DIR + "/SCIENCE_OVERVIEW.md",
         "section": "9.9", "owner": "FIX-CT", "reason": "自检夹具：已登记的真死锚"},
    ],
}


def _write(path: str, text: str) -> None:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _selftest_stub(name: str) -> str:
    """SKIP_EXACT 中「自带夹具面」条目的同构桩：注册 --self-test 且实现 self_test()。

    真仓里这些路径是真实检查器；mini-repo 必须与真仓同构 —— 缺了就是锚缺失，
    skip_exact_justified 判红（这正是它该有的牙，见负例 S18）。
    """
    return ('#!/usr/bin/env python3\n# -*- coding: utf-8 -*-\n'
            '"""' + name + ' 夹具桩（mini-repo 与真仓同构用，不承载 docs/ token）。"""\n'
            'import argparse\n\n\n'
            'def self_test() -> int:\n'
            '    return 0\n\n\n'
            'def main(argv=None) -> int:\n'
            '    ap = argparse.ArgumentParser()\n'
            '    ap.add_argument("--self-test", action="store_true")\n'
            '    a = ap.parse_args(argv)\n'
            '    return self_test() if a.self_test else 0\n')


@contextlib.contextmanager
def _skip_exact(value):
    """临时改写 SKIP_EXACT（负例注入用）；退出即还原，自检不残留状态。"""
    saved = SKIP_EXACT
    globals()["SKIP_EXACT"] = tuple(value)
    try:
        yield
    finally:
        globals()["SKIP_EXACT"] = saved


def _mk(root: str) -> None:
    # 只建**会被写入文件**的目录。docs/owner/ 与 docs/ci/ 刻意不建：它们在真仓都是
    # 空目录残留（test -e EXISTS 但 git ls-files 0 行），而 git 不跟踪空目录 ⇒ 对本门
    # 等同于不存在。建一个空目录只会制造「目录在 ⇒ 路径可达」的错觉，这正是本次
    # 4 个正例变红的根因之一。docs/engineering/ 由下面的 _write 顺带建出。
    for d in ("docs/archive", "docs/contracts", "lib/x", "eng/tools/doccheck"):
        os.makedirs(os.path.join(root, d), exist_ok=True)
    _write(os.path.join(root, INDEX_PATH), BASE_INDEX)
    for name, text in ROOT_DOC_FILES.items():
        _write(os.path.join(root, name), text)
    # 概览页落位 docs/engineering/（迁移后形态），与 ROOT_DOC_FILES 里的指针、
    # 与 BASE_INDEX 里的逐篇登记三处同源；任一处不同源都会让正例假红。
    for d in OVERVIEW_DOCS:
        _write(os.path.join(root, OVERVIEW_DIR, d),
               "# " + d + "\n\n> 上游：docs/ASTROCS_DESIGN.md §2（核心科学方法）\n\n"
               + ("owner L0 overview content. " * 20) + "\n")
    _write(os.path.join(root, "docs/engineering/01_CHECKS.md"),
           "# checks\n\n> 上游：docs/ASTROCS_DESIGN.md §12.4（验证层级与四层验收）\n\nbody\n")
    _write(os.path.join(root, "docs/archive/OLD.md"), "# ARCHIVED old doc\n")
    _write(os.path.join(root, "docs/review/OLD_REVIEW.md"), "# ARCHIVED old review copy\n")
    _write(os.path.join(root, "docs/DOC-L0.md"),
           "# L0\n\n> 上游：docs/ASTROCS_DESIGN.md §0.2\n\nbody\n")
    _write(os.path.join(root, "lib/x/foo.cpp"),
           "// legacy pointer docs/legacy/OLD.md (跨域未修，台账登记)\nint x = 1;\n")
    _write(os.path.join(root, LEDGER_PATH),
           json.dumps(LEDGER_JSON, ensure_ascii=False, indent=1) + "\n")
    # 内容真实性台账（mini-repo 必须与真仓同构，否则 docs_section_refs_resolve fail-closed 判红）。
    _write(os.path.join(root, CONTENT_TRUTH_LEDGER),
           json.dumps(CONTENT_LEDGER_JSON, ensure_ascii=False, indent=1) + "\n")
    # SKIP_EXACT 的「自带夹具面」条目必须在位且自带 --self-test 实现（与真仓同构）。
    for rel in SKIP_EXACT:
        if rel != LEDGER_PATH:
            _write(os.path.join(root, rel), _selftest_stub(os.path.basename(rel)))


def _git_init(root: str) -> None:
    for cmd in (["git", "init", "-q"],
                ["git", "add", "-A"],
                ["git", "-c", "user.email=b12@example.invalid",
                 "-c", "user.name=B12", "commit", "-qm", "fixture"]):
        subprocess.run(cmd, cwd=root, capture_output=True, text=True)


def _mk_repo(tmp: str, name: str) -> str:
    root = os.path.join(tmp, name)
    _mk(root)
    _git_init(root)
    return root


def _res(root: str, name: str, strict: bool, rc_expected: int, needle: str) -> tuple:
    results, _ = run_checks(root, strict)
    failed = [r for r in results if not r["pass"]]
    rc = 0 if not failed else 1
    hit = any(needle in r["check"] for r in results)
    ok = (rc == rc_expected) and hit
    return ok, "rc=" + str(rc) + " (expect " + str(rc_expected) + ") failed=" + \
        repr([r["check"] for r in failed]) + " needle(" + needle + ")=" + str(hit)


def _red(root: str, name: str, strict: bool, needle: str) -> tuple:
    """负例：必须判红，且红项点名 needle。"""
    results, _ = run_checks(root, strict)
    failed = [r for r in results if not r["pass"]]
    hit = any(needle == r["check"] for r in failed)
    return hit, "rc=" + str(1 if failed else 0) + " red=" + repr([r["check"] for r in failed]) \
        + " needle(" + needle + ")=" + str(hit)


def _ct(root: str, expect_pass: bool) -> tuple:
    """断言**单个 check** 的 pass 取值（CONTENT-TRUTH 正例专用）。

    为什么不用 _res()：mini-repo 的既有夹具在 HEAD 就让 4 个正例红（S0/S4/S14b/S16，
    根因是夹具自身的悬空指针，见 run/FINAL-07/content-truth/三步验收实跑.md §3）。
    正例若断言全局 rc=0，会把「既有夹具红」误记成本判据的红 —— 那是不诚实的对照。
    因此本判据的正例只断言 docs_section_refs_resolve 这一项，判别力由 S21（注入前绿、
    注入后红）单独锁住。
    """
    results, _ = run_checks(root, True)
    row = next((r for r in results
                if r["check"] == "docs_section_refs_resolve"), None)
    if row is None:
        return False, "docs_section_refs_resolve 未出现在结果面（门自身坏了）"
    return row["pass"] == expect_pass, ("pass=" + repr(row["pass"]) + " expect="
                                        + repr(expect_pass) + " | " + row["detail"][:200])


def _ct_pass(root: str):
    """只取 docs_section_refs_resolve 的 pass 取值（用于非退化自证的正控）。"""
    results, _ = run_checks(root, True)
    row = next((r for r in results
                if r["check"] == "docs_section_refs_resolve"), None)
    return row["pass"] if row else None


def _scan_surface_for_fixture(root: str) -> list:
    """mini-repo 的扫描面（与 run_checks 同一份构造，逐条同构）。S28 用它找可改写的文件。"""
    tracked = git_ls(root)
    out = []
    for p in ROOT_DOCS:
        if os.path.isfile(os.path.join(root, p)):
            out.append((p, "root"))
    for p in tracked:
        if (p.startswith(SKIP_DIRS) or not p.endswith(SCAN_EXT) or p in SKIP_EXACT
                or p.endswith(SKIP_SUFFIX) or any(s in p for s in SKIP_CONTAINS)):
            continue
        if p.startswith("docs/"):
            if p.endswith(".md"):
                out.append((p, "docs"))
        elif p.startswith(CODE_SCAN_DIRS):
            out.append((p, "code"))
    return out


def self_test() -> int:
    """mini-repo 正/负例：索引合法⇒绿；悬空条目/悬空指针/缺抬头/漏登记/代码注释悬空/
    非 ASCII 旧控制包残留/台账缺失 ⇒ 分别判红。

    CONTENT-TRUTH 新增（S21..S28）：死节号锚判红 / 真实节号锚不误红 / ASTROCS 目标不归本门 /
    内容台账缺失与畸形 fail-closed / 模式 B、C 命中 / 节号锚分母为 0 时 fail-closed。
    """
    import tempfile

    cases = []
    with tempfile.TemporaryDirectory() as tmp:
        # S0 正例：完整 mini-repo（根文档 + 索引 + 抬头 + 归档标记 + 台账）⇒ rc=0
        r0 = _mk_repo(tmp, "s0")
        cases.append(("S0-positive-legal-index",) + _res(r0, "s0", True, 0, "docs_fully_covered"))

        # S1 锚缺失（strict）：索引文件不存在 ⇒ rc=1 + anchor_files_alive 红
        r1 = os.path.join(tmp, "s1")
        _mk(r1)
        os.remove(os.path.join(r1, INDEX_PATH))
        cases.append(("S1-anchor-stale-strict",) + _res(r1, "s1", True, 1, "anchor_files_alive"))

        # S2 锚缺失（非 strict）：同一输入 ⇒ 记 skipped_anchor_absent；strict/非 strict
        #    detail 必须不同；fail-closed 的 index_file_exists 仍判红 ⇒ rc=1。
        ok2, msg2 = _res(r1, "s2", False, 1, "anchor_files_alive")
        res_non, _ = run_checks(r1, False)
        res_str, _ = run_checks(r1, True)
        d_non = next(r["detail"] for r in res_non if r["check"] == "anchor_files_alive")
        d_str = next(r["detail"] for r in res_str if r["check"] == "anchor_files_alive")
        ok2 = ok2 and (d_non != d_str) and ("skipped_anchor_absent" in d_non) \
            and ("ANCHOR_STALE" in d_str)
        cases.append(("S2-anchor-strict-vs-default", ok2,
                      msg2 + " | non-strict=" + repr(d_non) + " strict=" + repr(d_str)))

        # S3 旧权威回归：docs/review 副本无 ARCHIVED 抬头、索引也未登记归档 ⇒ rc=1
        r3 = _mk_repo(tmp, "s3")
        _write(os.path.join(r3, "docs/review/SCIENCE_OVERVIEW.md"), "# 旧权威副本（无归档声明）\n")
        _write(os.path.join(r3, INDEX_PATH), BASE_INDEX.replace(
            "    - path: \"docs/review\"\n      status: ARCHIVED_NON_NORMATIVE\n", ""))
        _git_init(r3)
        cases.append(("S3-old-authority-return",) + _res(r3, "s3", True, 1,
                                                         "retired_authority_not_reintroduced"))

        # S4 旧权威副本已声明归档：同路径 + ARCHIVED 抬头（+ 索引 archived 登记）⇒ rc=0
        r4 = _mk_repo(tmp, "s4")
        _write(os.path.join(r4, "docs/review/SCIENCE_OVERVIEW.md"),
               "> **ARCHIVED_NON_NORMATIVE（DOC-001，2026-09-16）**：旧文档体系。\n# 旧副本\n")
        _write(os.path.join(r4, INDEX_PATH),
               BASE_INDEX + "    - path: \"docs/review/SCIENCE_OVERVIEW.md\"\n"
                            "      status: ARCHIVED_NON_NORMATIVE\n")
        _git_init(r4)
        cases.append(("S4-retired-declared-archived",) + _res(r4, "s4", True, 0,
                                                              "retired_authority_not_reintroduced"))

        # S5 根旧权威索引回归：REVIEW.md 重新出现 ⇒ rc=1
        r5 = _mk_repo(tmp, "s5")
        _write(os.path.join(r5, "REVIEW.md"), "# REVIEW\n")
        _git_init(r5)
        cases.append(("S5-root-review-return",) + _res(r5, "s5", True, 1,
                                                       "retired_authority_not_reintroduced"))

        # S6 fail-closed：索引未覆盖的新文档出现 ⇒ rc=1（docs_fully_covered 真实缺口）
        r6 = _mk_repo(tmp, "s6")
        _write(os.path.join(r6, "docs/engineering/CONFIG_CONTRACT.md"), "# config contract\n")
        _git_init(r6)
        cases.append(("S6-coverage-gap",) + _res(r6, "s6", True, 1, "docs_fully_covered"))

        # S7 空树（无索引、无根文档）⇒ fail-closed rc=1
        r7 = os.path.join(tmp, "s7")
        os.makedirs(r7, exist_ok=True)
        cases.append(("S7-empty-tree-fail-closed",) + _res(r7, "s7", True, 1, "index_file_exists"))

        # S8 --root 指向不存在目录 ⇒ fail-closed rc=1，且不得 traceback
        r8 = os.path.join(tmp, "s8-does-not-exist")
        cases.append(("S8-nonexistent-root-fail-closed",) + _res(r8, "s8", True, 1,
                                                                 "anchor_files_alive"))

        # S9 负例：索引条目悬空（条目路径不存在）⇒ index_entry_paths_exist 判红
        r9 = _mk_repo(tmp, "s9")
        _write(os.path.join(r9, INDEX_PATH), BASE_INDEX + (
            "    - path: \"docs/gone/GONE.md\"\n      status: ACTIVE_INFORMATIVE\n"))
        _git_init(r9)
        cases.append(("S9-dangling-index-entry",) + _red(r9, "s9", True, "index_entry_paths_exist"))

        # S10 负例：最高设计里的 docs/ 链接悬空 ⇒ root_docs_doc_links_resolve 判红
        r10 = _mk_repo(tmp, "s10")
        _write(os.path.join(r10, "docs/ASTROCS_DESIGN.md"),
               ROOT_DOC_FILES["docs/ASTROCS_DESIGN.md"] + "\n详见 docs/nope/NOPE.md。\n")
        _git_init(r10)
        cases.append(("S10-dangling-root-doc-link",) + _red(r10, "s10", True,
                                                            "root_docs_doc_links_resolve"))

        # S11 负例：删一个抬头 ⇒ subordinate_docs_upstream_header 判红
        r11 = _mk_repo(tmp, "s11")
        _write(os.path.join(r11, "docs/engineering/SCIENCE_OVERVIEW.md"),
               "# SCIENCE_OVERVIEW.md\n\n" + ("owner L0 overview content. " * 20) + "\n")
        _git_init(r11)
        cases.append(("S11-missing-upstream-header",) + _red(r11, "s11", True,
                                                             "subordinate_docs_upstream_header"))

        # S12 负例：漏登一篇下级文档 ⇒ subordinate_docs_registered 判红
        r12 = _mk_repo(tmp, "s12")
        _write(os.path.join(r12, "docs/new/NEW.md"),
               "# NEW\n\n> 上游：docs/ASTROCS_DESIGN.md §2\n\nbody\n")
        _git_init(r12)
        cases.append(("S12-unregistered-doc",) + _red(r12, "s12", True,
                                                      "subordinate_docs_registered"))

        # S13 负例：代码注释里的 docs/ 路径悬空且未登记台账 ⇒ docs_path_refs_resolve 判红，
        #     且判词必须带「文件:行」（GATE-TRIAGE-01：原实现只给 token 与 scope，
        #     读者无法定位到哪一行）。
        r13 = _mk_repo(tmp, "s13")
        _write(os.path.join(r13, "lib/y/bar.cpp"), "// see docs/missing/X.md\n")
        _git_init(r13)
        res13, _ = run_checks(r13, True)
        d13 = next((r for r in res13 if r["check"] == "docs_path_refs_resolve"), None)
        has_where = bool(d13) and ("lib/y/bar.cpp:1" in d13.get("detail", ""))
        cases.append(("S13-dangling-code-comment-ref",) + _red(r13, "s13", True,
                                                               "docs_path_refs_resolve"))
        cases.append(("S13b-dangling-detail-has-file-line", has_where,
                      (d13 or {}).get("detail", "")[:120]))

        # S14 负例：非 ASCII 路径旧控制包残留必须被判红（core.quotepath 假绿回归）
        r14 = _mk_repo(tmp, "s14")
        _write(os.path.join(r14, "工程控制/RELEASE-03/旧审计.md"), "# 旧控制包残留（非 ASCII 路径）\n")
        _git_init(r14)
        cases.append(("S14-legacy-control-nonascii-path",) + _red(r14, "s14", True,
                                                                  "no_legacy_工程控制_left"))

        # S14b 正例：现行控制包（CURRENT_CONTROL_PACK/**）在位不得判红
        r14b = _mk_repo(tmp, "s14b")
        _write(os.path.join(r14b, CURRENT_CONTROL_PACK, "00_README.md"), "# 现行控制包\n")
        _git_init(r14b)
        cases.append(("S14b-current-control-pack-ok",) + _res(r14b, "s14b", True, 0,
                                                             "no_legacy_工程控制_left"))

        # S14c 负例：quotepath 转义回归自证 —— 现行包目录在盘上但 tracked 不可见 ⇒ 判红
        r14c = _mk_repo(tmp, "s14c")
        os.makedirs(os.path.join(r14c, CURRENT_CONTROL_PACK), exist_ok=True)
        _write(os.path.join(r14c, CURRENT_CONTROL_PACK, "x.md"), "# untracked\n")
        cases.append(("S14c-quotepath-blind-spot",) + _red(r14c, "s14c", True,
                                                           "no_legacy_工程控制_left"))

        # S15 负例：台账缺失 ⇒ fail-closed 判红（不得把"文件不存在"当"无违规"）
        r15 = _mk_repo(tmp, "s15")
        os.remove(os.path.join(r15, LEDGER_PATH))
        _git_init(r15)
        cases.append(("S15-ledger-missing-fail-closed",) + _red(r15, "s15", True,
                                                                "docs_path_refs_resolve"))

        # S16 正例：真仓式合法索引 + 台账登记跨域未修项 ⇒ 仍 rc=0（台账不判红已修项）
        r16 = _mk_repo(tmp, "s16")
        _write(os.path.join(r16, "lib/x/foo.cpp"), "// 已修：无悬空引用\nint x = 1;\n")
        _git_init(r16)
        cases.append(("S16-ledger-resolved-still-green",) + _res(r16, "s16", True, 0,
                                                                 "docs_path_refs_resolve"))

        # S17 负例：往 SKIP_EXACT 塞一个「无夹具面」的文件（= 为了让红灯变绿而扩排除面）
        #     ⇒ skip_exact_justified 判红。非退化自证：同一 mini-repo 在未改动的
        #     SKIP_EXACT 下该 check 必须为绿 —— 红只能由注入产生，不是夹具本身坏。
        r17 = _mk_repo(tmp, "s17")
        _write(os.path.join(r17, "eng/tools/doccheck/no_selftest.py"),
               "# 该文件没有夹具面：既无 --self-test 入口，也无 self_test() 实现\n")
        _git_init(r17)
        res17, _ = run_checks(r17, True)
        pos17 = next((r["pass"] for r in res17 if r["check"] == "skip_exact_justified"), None)
        with _skip_exact(SKIP_EXACT + ("eng/tools/doccheck/no_selftest.py",)):
            ok17, msg17 = _red(r17, "s17", True, "skip_exact_justified")
        cases.append(("S17-skip-exact-unjustified", bool(ok17) and pos17 is True,
                      msg17 + " | 正控(未注入时 skip_exact_justified pass)=" + repr(pos17)))

        # S18 负例：SKIP_EXACT 塞入不存在的文件 ⇒ 判红（锚存活/fail-closed：
        #     否则「先删文件、再挂排除项」会成为永久盲区）
        r18 = _mk_repo(tmp, "s18")
        with _skip_exact(SKIP_EXACT + ("eng/tools/doccheck/ghost_checker.py",)):
            ok18, msg18 = _red(r18, "s18", True, "skip_exact_justified")
        cases.append(("S18-skip-exact-missing-anchor", ok18, msg18))

        # S19 负例：把台账本身移出 SKIP_EXACT ⇒ 判红（台账里的 token 是数据不是引用；
        #     台账不在排除面内时它自己会被当悬空引用扫描）
        r19 = _mk_repo(tmp, "s19")
        with _skip_exact(tuple(x for x in SKIP_EXACT if x != LEDGER_PATH)):
            ok19, msg19 = _red(r19, "s19", True, "skip_exact_justified")
        cases.append(("S19-ledger-dropped-from-skip-exact", ok19, msg19))

        # S20 负例：夹具面判据（--self-test 入口 + 实现体）的**判别力**自证。
        #     GATE-TRIAGE-01 把 SELFTEST_ENTRY_RE 从 `def self_test(` 放宽到
        #     `def _?self_test(`（check_mutation_gates.py 的实现体叫 _self_test）。
        #     放宽必须仍然拒绝「只有 --self-test 字样、没有实现体」的文件，
        #     否则就是为过闸而放松判据。三条断言：
        #       a) 桩（self_test）被接受；b) 桩（_self_test）被接受；
        #       c) 只提 --self-test 而无实现体 ⇒ 拒绝（skip_exact_reason 非空）。
        r20 = _mk_repo(tmp, "s20")
        p_stub_a = os.path.join(r20, "eng/tools/doccheck/stub_a.py")
        p_stub_b = os.path.join(r20, "eng/tools/doccheck/stub_b.py")
        p_fake = os.path.join(r20, "eng/tools/doccheck/fake_c.py")
        _write(p_stub_a, _selftest_stub("stub_a.py"))
        _write(p_stub_b, _selftest_stub("stub_b.py").replace(
            "def self_test()", "def _self_test()").replace(
            "return self_test()", "return _self_test()"))
        _write(p_fake, "# 只提 --self-test，不实现任何实现体\nX = 1\n")
        why_a = skip_exact_reason(r20, "eng/tools/doccheck/stub_a.py")
        why_b = skip_exact_reason(r20, "eng/tools/doccheck/stub_b.py")
        why_c = skip_exact_reason(r20, "eng/tools/doccheck/fake_c.py")
        ok20 = (why_a == "" and why_b == "" and why_c != "")
        cases.append(("S20-selftest-entry-criterion", ok20,
                      "self_test()=%r _self_test()=%r no_impl=%r"
                      % (why_a, why_b, why_c[:60])))

        # 正则哨：匹配一切节号锚形态。S28 用它把扫描面清空到分母 0。
        CONTENT_SCAN_RE = re.compile(r"§\s*\d+(?:\.\d+)*|第\s*\d+(?:\.\d+)*\s*节")
        N = chr(10)
        OWN_HEAD = "# SCI" + N + N + "> 上游：docs/ASTROCS_DESIGN.md §2" + N + N

        # S21 负例（可证伪）：路径可达但目标页没有该节号 ⇒ docs_section_refs_resolve 判红。
        #     非退化自证：同一 mini-repo 在注入**前**该 check 必须为绿 ——
        #     红只能由注入产生（恒真门守卫：判据不是恒绿的）。
        r21 = _mk_repo(tmp, "s21")
        _write(os.path.join(r21, "docs/engineering/SCIENCE_OVERVIEW.md"),
               OWN_HEAD + "## 1 起" + N + "正文" + N + "### 2.1 子节" + N + "正文" + N)
        _git_init(r21)
        pos21 = _ct_pass(r21)
        _write(os.path.join(r21, "lib/x/foo.cpp"),
               "// 规范见 docs/engineering/SCIENCE_OVERVIEW.md §7.3（该节不存在）" + T + "int x = 1;" + T)
        _git_init(r21)
        ok21, d21 = _ct(r21, False)
        cases.append(("S21-content-ref-dead-section",
                      bool(ok21) and pos21 is True,
                      "注入后判红=" + repr(bool(ok21)) + " | 正控(注入前 pass)="
                      + repr(pos21) + " | " + d21[:160]))

        # S22 正例（误报对照）：节号**真实存在**（§1 与多级 §2.1）⇒ 不得判红。
        r22 = _mk_repo(tmp, "s22")
        _write(os.path.join(r22, "docs/engineering/SCIENCE_OVERVIEW.md"),
               OWN_HEAD + "## 1 起" + N + "正文" + N + "### 2.1 子节" + N + "正文" + N)
        _write(os.path.join(r22, "lib/x/foo.cpp"),
               "// 口径见 docs/engineering/SCIENCE_OVERVIEW.md §2.1 与 §1" + T + "int x = 1;" + T)
        _git_init(r22)
        ok22, d22 = _ct(r22, True)
        cases.append(("S22-content-ref-live-section", ok22, d22[:160]))

        # S23 正例（误报对照）：ASTROCS_DESIGN.md 目标**不归本门** ⇒ 即便节号不存在也不判红
        #     （兄弟门面 check_design_section_refs.py 专责；本门显式排除，不造第二套口径）。
        r23 = _mk_repo(tmp, "s23")
        _write(os.path.join(r23, "lib/x/foo.cpp"),
               "// 权威见 docs/ASTROCS_DESIGN.md §99.99" + T + "int x = 1;" + T)
        _git_init(r23)
        ok23, d23 = _ct(r23, True)
        cases.append(("S23-astrocs-target-not-ours", ok23, d23[:160]))

        # S24 负例：内容台账缺失 ⇒ fail-closed 判红（不得把「台账读不到」当无违规）。
        r24 = _mk_repo(tmp, "s24")
        os.remove(os.path.join(r24, CONTENT_TRUTH_LEDGER))
        _git_init(r24)
        cases.append(("S24-content-ledger-missing-fail-closed",)
                      + _red(r24, "s24", True, "docs_section_refs_resolve"))

        # S25 负例：内容台账条目缺必填字段 ⇒ 判红（防台账被塞成空壳绕过）。
        r25 = _mk_repo(tmp, "s25")
        _write(os.path.join(r25, CONTENT_TRUTH_LEDGER),
               json.dumps({"max_entries": 1, "entries": [{"file": "a"}]},
                          ensure_ascii=False) + T)
        _git_init(r25)
        cases.append(("S25-content-ledger-malformed",)
                      + _red(r25, "s25", True, "docs_section_refs_resolve"))

        # S26 正例（误报对照）：§N.M 命中模式 B（标题正文带显式 `（§N.M）`）⇒ 不得判红。
        r26 = _mk_repo(tmp, "s26")
        _write(os.path.join(r26, "docs/engineering/SCIENCE_OVERVIEW.md"),
               OWN_HEAD + "## 2 清单" + N + "正文" + N + "#### 处置面（§2.1.1）" + N + "正文" + N)
        _write(os.path.join(r26, "lib/x/foo.cpp"),
               "// 登记见 docs/engineering/SCIENCE_OVERVIEW.md §2.1.1" + T + "int x = 1;" + T)
        _git_init(r26)
        ok26, d26 = _ct(r26, True)
        cases.append(("S26-inline-section-anchor-live", ok26, d26[:160]))

        # S27 正例（误报对照）：§N.M 命中模式 C（父节正文的有序列表第 M 项）⇒ 不得判红。
        r27 = _mk_repo(tmp, "s27")
        _write(os.path.join(r27, "docs/engineering/SCIENCE_OVERVIEW.md"),
               OWN_HEAD + "## 14 文献" + N + "1. 一" + N + "2. 二（冻结）" + N + "3. 三" + N)
        _write(os.path.join(r27, "lib/x/foo.cpp"),
               "// 常量见 docs/engineering/SCIENCE_OVERVIEW.md §14.2" + T + "int x = 1;" + T)
        _git_init(r27)
        ok27, d27 = _ct(r27, True)
        cases.append(("S27-list-item-section-live", ok27, d27[:160]))

        # S28 负例（判别力）：扫描面节号锚分母为 0 ⇒ fail-closed 判红。
        #     锁住「解析器空转不得判绿」——否则删光全仓节号锚即可让本门恒绿。
        r28 = _mk_repo(tmp, "s28")
        _git_init(r28)  # 先 init：扫描面依赖 git ls-files
        for rel, _s in _scan_surface_for_fixture(r28):
            full = os.path.join(r28, rel)
            if os.path.isfile(full):
                _write(full, CONTENT_SCAN_RE.sub("", _read(full)))
        _git_init(r28)
        cases.append(("S28-zero-anchor-scan-floor",)
                      + _red(r28, "s28", True, "docs_section_refs_resolve"))

    bad = [(n, m) for n, ok, m in cases if not ok]
    for n, ok, m in cases:
        print("SELFTEST " + ("PASS" if ok else "FAIL") + " " + n + ": " + m)
    if bad:
        # GATE-TRUST-01 三态：自检用例不符预期 = **门自身不可信**，不是「被判对象不合规」。
        # 必须用独立退出码 3（eng/ci/run_checks.py::EXIT_CRASH）声明，否则消费者会把
        # 「门坏了」读成「仓库内容红了」，红绿都当噪声。
        print("SELFTEST_FAIL: " + str(len(bad)) + "/" + str(len(cases)) + " 例不符预期",
              file=sys.stderr)
        print("GATE_TRUST_FAIL: 门自身不可信（判别力面用例不符预期；红绿都不具证据资格）："
              + repr([n for n, _m in bad]), file=sys.stderr)
        return 3
    print("SELFTEST_PASS: " + str(len(cases)) + "/" + str(len(cases))
          + " 例符合预期（正例 rc=0；悬空条目/悬空根文档指针/缺抬头/漏登记/代码注释悬空/"
            "非 ASCII 旧控制包残留/台账缺失/排除面无据、锚缺失、台账被移出、"
            "夹具面判据判别力 各自判红）")
    return 0


if __name__ == "__main__":
    sys.exit(main())

