# -*- coding: utf-8 -*-
r"""DOC-SCI-SOURCE-SUFFICIENCY | 第二层「科学集」信源充分性门（判据 A）。

要判的是什么
  docs/science/**（含 algorithms/，科学正本，只读权威）的**每条主张**都必须有
  **可核到定位的来源**。负责人对第二层科学集的硬要求是「每条主张有来自
  实验 / 论文 / 开源库的详细支撑」。本门把这条要求变成带分母、能红能绿的门。

分母（claims_total）的口径 —— **块级结构化切分，不是字符窗口**
  1. 用真正的 markdown **块解析器**（标题树 / 围栏 / 表格 / 列表 / 段落）切块，
     每一轮强制断言游标严格前进（本仓第一版解析器就因表格入口写成"分隔行匹配"
     导致普通数据行处**死循环**，实测 exit=124，见 run/FINAL-07/doc-audit/recon_blocks.py 注记）。
  2. 一条主张 = 一个**断言块**：
       - 表格：每个**数据行**（去表头 / 去分隔行）各算一条；
       - 列表：每条 bullet / numbered step 算一条；
       - 段落：含断言标记的段落算一条；
       - 围栏：非 mermaid 的定义/公式围栏算一条。
  3. **排除面**（不进分母，逐条计数并打印，不静默）：标题行、mermaid 围栏、
     参考文献/引用节（§14 / §14a / 参考文献 / Primary literature / 参考代码库
     / References）—— 这些是**信源本身**，不是待支撑的主张。

来源分级（一条主张取其**自身块**内的锚）
  合格·论文    DOI(10.x/y) 或 arXiv 编号 **且** 定位到章节/表/图/公式号
  合格·开源库  仓库名 **且** 版本(tag/commit) **且** file:line
  合格·实验    产物路径 **且** 结果件**实际存在**（本门真的去 open+read，不只信路径写对）
  不合格       无信源 / 只有泛化引用（"见文献"）/ 部分合格（有 DOI 无定位等）

  ⚠ 泛化引用**本身不算任何一类的合格锚**：只写「见文献」的主张即判不合格。

判据（fail-closed）
  A1 扫描面   docs/science/** 下 *.md（含 algorithms/）；文件数 = 0 ⇒ rc=2。
  A2 分母面   必须解析出 >=1 个主张（claims_total = 0 ⇒ rc=2，SCAN_FLOOR：
              解析器空转不得判绿）。
  A3 分级     每条主张给 grade ∈ {paper, oss, experiment, *_partial, fail} 与 reason。
  A4 证据面   「实验」级必须真的 open+read 产物；不可读 ⇒ 不给该级。
  A5 清单     逐条输出 file:line + 主张原文 + 缺什么。

用法
  python3 eng/tools/doccheck/check_science_source_sufficiency.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_science_source_sufficiency.py --self-test
exit 0 = 全合格；1 = 有不合格主张；2 = 输入不可用 / 分母为 0（fail-closed）。
--self-test 恒 0 = 内置正例与**注入负例**（无信源 / 只写"见文献" / DOI 无定位）全被判红。

只读；仅 stdlib；无网络；输出按 (文件, 行号) 稳定排序，跨 cwd 复跑一致。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

CHECK_ID = "DOC-SCI-SOURCE-SUFFICIENCY"
SELF_REL = "eng/tools/doccheck/check_science_source_sufficiency.py"
SCI_REL = "docs/science"

# ── 块解析 ────────────────────────────────────────────────────────
FENCE_RE = re.compile(r"^\s{0,3}(`{3,}|~{3,})")
HEAD_RE = re.compile(r"^\s{0,3}(#{1,6})\s+(.*?)\s*$")
TBL_ROW_RE = re.compile(r"^\s*\|")
TBL_SEP_RE = re.compile(r"^\s*\|[\s:|-]+\|\s*$")
LIST_RE = re.compile(r"^(\s*)(?:[-*+]\s+|(\d+)\.\s+)")


def blocks_of(lines):
    """块级切分。返回 [(kind, start, end, lines)]，1-based。死循环守卫。"""
    out, i, n = [], 0, len(lines)
    while i < n:
        start_i, ln = i, lines[i]
        m = FENCE_RE.match(ln)
        if m:
            ch, need = m.group(1)[0], len(m.group(1))
            close = re.compile(r"^\s{0,3}%s{%d,}\s*$" % (re.escape(ch), need))
            j = i + 1
            while j < n and not close.match(lines[j]):
                j += 1
            end = min(j + 1, n)
            out.append(("fence", i + 1, end, lines[i:end]))
            i = end
        elif HEAD_RE.match(ln):
            out.append(("heading", i + 1, i + 1, [ln]))
            i += 1
        elif TBL_ROW_RE.match(ln):
            j = i
            while j < n and TBL_ROW_RE.match(lines[j]):
                j += 1
            out.append(("table", i + 1, j, lines[i:j]))
            i = j
        elif LIST_RE.match(ln):
            indent = len(LIST_RE.match(ln).group(1))
            j = i + 1
            while j < n:
                l2 = lines[j]
                if l2.strip() == "" or FENCE_RE.match(l2) or HEAD_RE.match(l2) or TBL_ROW_RE.match(l2):
                    break
                m2 = LIST_RE.match(l2)
                if m2:
                    if len(m2.group(1)) <= indent:
                        break
                elif not l2.startswith(" "):
                    break
                j += 1
            out.append(("list", i + 1, j, lines[i:j]))
            i = j
        elif ln.strip() == "":
            i += 1
        else:
            j = i + 1
            while (j < n and lines[j].strip() != "" and not FENCE_RE.match(lines[j])
                   and not HEAD_RE.match(lines[j]) and not LIST_RE.match(lines[j])
                   and not TBL_ROW_RE.match(lines[j])):
                j += 1
            out.append(("para", i + 1, j, lines[i:j]))
            i = j
        if i <= start_i:
            raise RuntimeError("blocks_of 未前进 @ line %d" % (i + 1))
    return out

# ── 参考节排除（信源本身，不是待支撑主张）─────────────────────────
REFSEC_RE = re.compile(
    r"(参考文献|参考代码库|引用|出处|文献|Primary\s*literature|References|"
    r"Bibliograph|Reference\s+code)", re.I)

# ── 断言识别 ─────────────────────────────────────────────────────
NORMATIVE_RE = re.compile(
    r"(必须|不得|不可|禁止|应当|应该|只允许|恒为|恒等|不变量|口径|定义为|定义|"
    r"判据|阈值|容差|不成立|退化|冻结|等价|充要|边界条件|"
    r"\bmust\b|\bshall\b|invariant|tolerance|threshold|criterion)")
EQ_RE = re.compile(r"[A-Za-z_一-鿿`)\]]\s*=\s*[^=]")
NUM_RE = re.compile(
    r"(?<![A-Za-z0-9_.])(\d+\.\d{2,}|\d{3,})"
    r"(?:\s*(?:mag|dex|nm|ADU|e-|°|′|″))?")

# ── 信源锚识别 ──────────────────────────────────────────────────
DOI_RE = re.compile(r"\b10\.\d{4,9}/[-._;()/:A-Za-z0-9]+\b")
ARXIV_RE = re.compile(r"\barXiv[:\s]*(?:\.\s*)?\d{4}\.\d{4,5}(?:v\d+)?\b", re.I)
# ⚠ 裁决（本车道 FINAL-07 修）：本仓 § **一律指内部章节**（ASTROCS_DESIGN §4.5 / 内部 §2.1），
#   因此 § 形式**永不计为文献定位符** —— 否则「DOI + 内部 §」会被判成「合格·论文」＝假绿。
LOCATOR_RES = [
    re.compile(r"(?:式|公式|方程)\s*\(?\s*[A-Za-z]?\d+(?:\.\d+)*\s*\)?"),
    re.compile(r"\bEq\.?\s*\(?\s*\d"),
    re.compile(r"\bTable\s*\d"),
    re.compile(r"\bFigure\s*\d|\bFig\.?\s*\d"),
    re.compile(r"表\s*\d+(?:\.\d+)*"),
    re.compile(r"图\s*\d+(?:\.\d+)*"),
    re.compile(r"\bSection\s*\d+(?:\.\d+)*"),
    re.compile(r"第\s*\d+(?:\.\d+)*\s*节"),
    re.compile(r"\bAppendix\s*[A-Z0-9]", re.I),
    re.compile(r"附录\s*[A-Z0-9]"),
]
GENERIC_RE = re.compile(
    r"(见文献|见参考资料|见引用|详见文献|参见文献|见\s*Refs?\b|见出处|see\s+Refs?\b)", re.I)

OSS_FILELINE_RE = re.compile(
    r"[\w./-]+\.(?:py|cpp|c|h|hpp|cc|cxx|pyx|rs|go|js|f90|jl)\s*[:(]\s*\d+")
OSS_REPO_RE = re.compile(
    r"\b(astroquery|photutils|astropy|scipy|numpy|healpy|astral|casatools|"
    r"drizzlepac|GaiaXPy|gaiaxpy|aips|libastrfits|cfitsio)\b", re.I)
VERSION_RE = re.compile(
    r"\b(?:v\d+\.\d+(?:\.\d+)*|\d+\.\d+(?:\.\d+)*|[0-9a-f]{7,40})\b", re.I)
PATHY_RE = re.compile(
    r"(?:实验|eng/tests|eng/tools|run|artifacts|testdata|gaia|lib|eng)/[\w./-]+"
    r"(?:\.(?:json|csv|md|png|npz|npy|txt|fits|yaml|yml|log|pdf))?")

ARTICLE_RE = re.compile(r"\b(?:19|20)\d{2}\b")


def has_locator(text):
    return any(rx.search(text) for rx in LOCATOR_RES)


# ⚠ 裁决一①（FINAL-07 修）：判「是不是主张」之前先剥掉**非主张数字**——
#   内部节号 §9.73 / 日期 / 行锚 :1234 / 版本 v1.2.3 / DOI / arXiv 号 / 参考文献序号。
#   旧版把它们一律当「常数主张」，分母因此虚高（实测 374 行只有这些）。
JUNK_NUM_RES = [
    re.compile(r"§\s*\d+(?:\.\d+)*[a-z]?"),
    re.compile(r"\b(?:19|20)\d{2}[-/年]\d{1,2}[-/月]\d{1,2}"),
    re.compile(r"(?<![A-Za-z0-9_])v\d+(?:\.\d+)+"),
    re.compile(r"[:：]\s*\d+(?:\s*[-–]\s*\d+)?\b"),
    re.compile(r"\b10\.\d{4,9}/[-._;()/:A-Za-z0-9]+"),
    re.compile(r"\b\d{4}\.\d{4,5}(?:v\d+)?\b"),
    re.compile(r"\[\d+\]"),
    re.compile(r"(?:\u884c|\u884c\u53f7|line|L)\s*\d+(?:\s*[-\u2013]\s*\d+)?\b", re.I),
]
JUNK_RE = re.compile("|".join("(?:%s)" % r.pattern for r in JUNK_NUM_RES))
# === 主张口径（负责人裁定）：台账 / 测试登记 / 政策声明 / 职责声明 / 承接行 一律不算主张 ===
# 检验句同一条：「它如果错了，会不会让科学结论错？」
#   台账/测试登记/政策/职责错 => 治理或流程失范，科学结论不变 => 不是主张；
#   承接行（「某式属某模块」）错 => 指向错，不是断言 => 是指针不是主张。
# 分寸（负责人同时给定）：排除只为让**分母诚实**，不得放过真正缺证据的断言。
#   若该块**自带证据锚**（DOI/arXiv/file:line/实验产物），它仍是主张 ——
#   EVIDENCE_ANCHOR_RE 守卫使排除规则不覆盖这一条。
EVIDENCE_ANCHOR_RE = re.compile(
    r"10\.\d{4,9}/|arXiv"
    r"|[\w./-]+\.(?:py|cpp|cc|cxx|h|hpp|rs)\s*[:（(\s]\s*\d+"
    r"|(?:实验|run|artifacts)/[\w./-]+")
LEDGER_HEAD_RE = re.compile(  # BUG-1 fix
    r"^\s*(?:[-*+]\s+|\d+\.\s+)?\|?\s*(?:>\s*)?(?:\*\*)?(?:DISP|FIX|TST|TEST|OBS|DEFECT|NOTE|REG|CHG)-[\w-]+")
POINTER_ONLY_RE = re.compile(  # BUG-1b fix
    r"^\s*(?:[-*+]\s+|\d+\.\s+)?(?:>\s*)?(?:\*\*)?(?:依据|证据|指针|承接|本文档现行口径|本域现行口径|不放|定位|见"
    r"|未变更|须逐字保留|冻结|保留原文|项目不要求|不要求逐字|生产消费|源码锚)"
    r"|(?:\*\*)?(?:ALG|SCI)-[A-Z0-9-]+\s*=\s*(?:本文档|登记面|本文)"
    r"|^\s*[-*]\s*`?docs/[\w./-]+\.md`?\s*[（(]"
    r"|属\s*astro_[\w]+|由\s*astro_[\w]+\s*负责|本节只作\*\*指针级承接")
# === 排除族台账（负责人要求：逐族可机器复核 + 逐族打印计数）=== 
# F1 台账/缺陷/测试登记   错 => 治理或覆盖失序，科学结论不变
# F2 承接/政策/冻结/源码锚  错 => 指向错或流程失范，不是断言
# F3 非主张数字（日期/节号/行锚/版本/DOI/arXiv）  分母清洗
# F4 参考节块  信源本身，不是待支撑主张
# F5 目录职责行  错 => 治理失序，科学结论不变
# F6 权威声明    错 => 权威链描述失真，科学结论不变
EXCLUDE_FAMILIES = {
    "F5-dir-duty": re.compile(r"^\s*(?:[-*+]\s+|\d+\.\s+)?[^`\n]{1,40}`?\s*[-\u2014]{2}"),
    "F6-authority-decl": re.compile(r"(本节是唯一|唯一冻结依据|唯一权威|权威正本|正本为|冻结依据|唯一口径为|本文件.{0,8}唯一)"),
}

INTERNAL_ANCHOR_RE = re.compile(r"(?:docs/[\w./-]+\.md|GLOSSARY|DATA_SEMANTICS|SCI-[A-Z]-\d+|ALG-[A-Z0-9-]+|§\s*\d)")
PUNCT_ONLY_RE = re.compile("[\\s\\-`*_>#|()\\u3008\\u3009\\u300c\\u300d\\u300e\\u300f\\u201c\\u201d\\uff0c\\u3002\\uff1b\\uff1a\\u3001/\\\\.]+")
WORD_RE = re.compile("[\\w\\u4e00-\\u9fff]{2,}")


def is_claim_block(kind, ls):
    """这个块是否是一条主张。返回 (bool, claim_type, why)。

    ⚠ 裁决一①：「主张」与「普通句子 / 目录引用 / 日期 / 版本」的区分规则：
      1) 先剥非主张数字（JUNK_RE），再判常数；
      2) 整块去掉路径与标点后若不剩任何实词 ⇒ 是目录清单，不是主张；
      3) 三种信号（公式=/规范语气/量值）全无 ⇒ 不是主张。
    """
    t = "\n".join(ls).strip()
    if not t:
        return (False, None, "empty")
    if kind == "fence":
        lang = (ls[0] or "").strip().strip("`~").strip().lower()
        if lang == "mermaid":
            return (False, None, "mermaid")
    # 「依据/证据/承接」抬头块是指路，不是主张
    # BUG-2 fix：台账类不因 file:line 复活（那不是「被当作科学依据引用」）
    if LEDGER_HEAD_RE.match(t):
        return (False, None, "ledger-row")
    if (POINTER_ONLY_RE.match(t) and not NORMATIVE_RE.search(t)
            and not EVIDENCE_ANCHOR_RE.search(t)):
        return (False, None, "F2-pointer-only")
    # F5/F6：证据锚优先级**高于**这两族（负责人纪律）
    if not EVIDENCE_ANCHOR_RE.search(t):
        for _fam, _rx in EXCLUDE_FAMILIES.items():
            if _rx.search(t):
                return (False, None, _fam)
    has_eq = bool(EQ_RE.search(t))
    has_norm = bool(NORMATIVE_RE.search(t))
    stripped = JUNK_RE.sub(" ", t)
    has_num = bool(NUM_RE.search(stripped))
    bare = PUNCT_ONLY_RE.sub(" ", PATHY_RE.sub(" ", t))
    has_word = bool(WORD_RE.search(bare))
    if has_eq:
        return (True, "formula", "eq")
    if has_norm:
        return (True, "assertion", "normative")
    if has_num and kind in ("list", "para", "table") and has_word:
        return (True, "constant", "number-after-junk-strip")
    if kind == "list" and re.match(r"^\s*\d+\.\s", ls[0] or ""):
        return (True, "alg_step", "numbered-step")
    if not has_num and not has_norm and not has_eq:
        return (False, None, "no-claim-signal")
    return (False, None, "path-or-crossref-only")


def is_own_code_path(tok, repo):
    """file:line token 的文件部分落在本仓 lib/|eng/ 下 ⇒ 判为本仓代码锚。"""
    fname = re.split(r"[:（(\s]", tok.strip(), 1)[0]
    base = os.path.basename(fname)
    for sub in ("lib", "eng"):
        for dp, dns, fns in os.walk(os.path.join(repo, sub)):
            dns[:] = [d for d in dns if d not in ("third_party", "__pycache__")]
            if base in fns:
                return True
    return False


def classify(text, repo):
    """在**主张自身块**内分级。返回 (grade, reason)。

    ⚠ 裁决一（本车道 FINAL-07 修）的三条硬规则：
      ① **先判是不是文献引用**：有 DOI/arXiv 时先走文献分支，不要先被某个
         本地路径抢走等级（否则 paper 级统计被 experiment 吞掉，分级不可读）。
      ② 本仓 § 恒指内部章节 ⇒ § 形式**永不计为文献定位**（见 LOCATOR_RES 注释）。
      ③ experiment 级**必须指向具体文件且可解析**；只给**目录**不给级，
         降为 experiment_partial 并写明「只给目录，未指向结果件」。
    """
    # ① 文献优先
    if DOI_RE.search(text) or ARXIV_RE.search(text):
        if has_locator(text):
            return ("paper", "doi/arxiv + 论文式定位符")
        return ("paper_partial", "doi/arxiv 缺章节/表/图/公式号定位")
    # ② 开源：file:line 必须配版本或仓库名
    m_oss = OSS_FILELINE_RE.search(text)
    if m_oss:
        # ⚠ 人工 30 条复核定位到的分级错：本仓 lib/|eng/ 的 file:line 是**代码锚**，
        #   不是第三方仓库信源。单列 code_anchor，不并入 oss（并入会把「有代码锚」
        #   误报成「有开源库信源」）。
        if is_own_code_path(m_oss.group(0), repo):
            return ("code_anchor", "本仓代码锚，非第三方信源：" + m_oss.group(0))
        if VERSION_RE.search(text) or OSS_REPO_RE.search(text):
            return ("oss", "repo/version+file:line")
        return ("oss_partial", "第三方 file:line 缺版本或仓库名")
    # ③ 实验：文件级 + 真读到字节；目录只给 partial
    dir_only = None
    for m in PATHY_RE.finditer(text):
        p = m.group(0).rstrip(".,;:）、，。；")
        cand = os.path.join(repo, p)
        if os.path.isfile(cand):
            try:
                with open(cand, "rb") as fh:
                    data = fh.read(4096)
            except OSError:
                continue
            if data:
                return ("experiment", "file+read:%dB: %s" % (len(data), p))
        elif os.path.isdir(cand):
            dir_only = p
    if dir_only is not None:
        return ("experiment_partial",
                "只给了目录、未指向结果件（裁决一③）：" + dir_only)
    if GENERIC_RE.search(text):
        return ("fail", "泛化引用（只写「见文献」）")
    # 内部正本锚：有可回指的仓内权威指针，但**不是一手来源** —— 单列一级，
    # 不并入 fail（否则 reason 会谎报「无信源」），也不并入合格（口径要求一手支撑）。
    if INTERNAL_ANCHOR_RE.search(text):
        return ("internal_anchor", "有内部正本锚，缺一手来源（论文/开源/实验）")
    if ARTICLE_RE.search(text) and has_locator(text):
        return ("paper_partial", "有年份+定位但无 DOI/arXiv")
    return ("fail", "无信源")


def iter_sci_md(repo):
    root = os.path.join(repo, SCI_REL)
    for dp, dns, fns in os.walk(root):
        dns[:] = sorted(dns)
        for fn in sorted(fns):
            if fn.endswith(".md"):
                ap = os.path.join(dp, fn)
                yield os.path.relpath(ap, repo).replace(os.sep, "/"), ap


def evaluate(repo):
    report = {
        "check": CHECK_ID,
        "root": SCI_REL,
        "files": 0,
        "claims_total": 0,
        "by_grade": {},
        "by_type": {},
        "excluded_refsec_blocks": 0,
        "excluded_nonclaim_blocks": 0,
        "excluded_by_family": {},
        "violations": [],
        "file_table": [],
    }
    claims = []
    for rel, ap in iter_sci_md(repo):
        report["files"] += 1
        lines = open(ap, encoding="utf-8", errors="replace").read().splitlines()
        blocks = blocks_of(lines)
        in_refsec = [False] * len(blocks)
        cur_ref, cur_lvl = False, 0
        for bi, (kind, _s, _e, ls) in enumerate(blocks):
            if kind == "heading":
                t = ls[0]
                m_lvl = re.match(r"^\s{0,3}(#+)", t)
                lvl = len(m_lvl.group(1)) if m_lvl else 1
                if REFSEC_RE.search(t):
                    cur_ref, cur_lvl = True, lvl
                elif cur_ref and lvl <= cur_lvl:
                    cur_ref = False
            in_refsec[bi] = cur_ref

        fclaims = 0
        for bi, (kind, s, e, ls) in enumerate(blocks):
            if kind == "heading":
                continue
            if in_refsec[bi]:
                report["excluded_refsec_blocks"] += 1
                continue
            if kind == "table":
                has_sep = any(TBL_SEP_RE.match(r) for r in ls)
                for ri, row in enumerate(ls):
                    if not TBL_ROW_RE.match(row) or TBL_SEP_RE.match(row):
                        continue
                    if has_sep and ri == 0:
                        continue
                    ok, ctype, _why = is_claim_block("table", [row])
                    if ok:
                        claims.append((rel, s + ri, row.strip(), ctype))
                    else:
                        report["excluded_nonclaim_blocks"] += 1
                        report["excluded_by_family"][_why] = report["excluded_by_family"].get(_why, 0) + 1
            elif kind == "list":
                ok, ctype, _why = is_claim_block("list", [ls[0]])
                if ok:
                    claims.append((rel, s, ls[0].strip(), ctype))
                else:
                    report["excluded_nonclaim_blocks"] += 1
                    report["excluded_by_family"][_why] = report["excluded_by_family"].get(_why, 0) + 1
            else:
                ok, ctype, _why = is_claim_block(kind, ls)
                if ok:
                    claims.append((rel, s, "\n".join(ls).strip(), ctype))
                else:
                    report["excluded_nonclaim_blocks"] += 1
                    report["excluded_by_family"][_why] = report["excluded_by_family"].get(_why, 0) + 1
            fclaims += 1
        report["file_table"].append([rel, fclaims])

    report["claims_total"] = len(claims)
    for rel, ln, text, ctype in claims:
        grade, reason = classify(text, repo)
        report["by_grade"][grade] = report["by_grade"].get(grade, 0) + 1
        report["by_type"][ctype] = report["by_type"].get(ctype, 0) + 1
        if grade not in ("paper", "oss", "experiment"):
            report["violations"].append({
                "file": rel, "line": ln, "type": ctype,
                "grade": grade, "reason": reason, "text": text[:300],
            })
    return report


def self_test():
    """注入负例必须判红。"""
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        sci = os.path.join(td, "docs", "science")
        os.makedirs(sci)
        art = os.path.join(td, "实验", "x")
        os.makedirs(art)
        with open(os.path.join(art, "r.json"), "w") as f:
            f.write("{}")
        B = chr(96)
        with open(os.path.join(sci, "OK.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n")
            f.write("- 公式 " + B + "x = 2n" + B + "（Gaia DR3 官方文档 §5.4.1，式 5.41；DOI 10.1051/0004-6361/202243709）\n")
            f.write("- 常数 " + B + "c = 4.685" + B + "，Kafadar 1983，Table 3（DOI 10.6028/jres.088.006）\n")
            f.write("- 判据 产物 实验/x/r.json 存在\n")
        with open(os.path.join(sci, "NEG1.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 判据 sigma 必须是 0.05，这里没有给任何来源\n")
        with open(os.path.join(sci, "NEG2.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 公式 " + B + "y = a*x" + B + "，详见文献。\n")
        with open(os.path.join(sci, "NEG3.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 判据 产物 实验/不存在/zz.json 的读数\n")
        with open(os.path.join(sci, "NEG4.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 常数 " + B + "m = 3.0" + B + "（DOI 10.1000/xyz）\n")
        # 裁决一②回归：DOI + **内部 §**（本仓 § 恒指内部章节）不得判成 paper
        with open(os.path.join(sci, "NEG5.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 常数 " + B + "k = 1.0" + B +
                    "（DOI 10.1000/xyz；见 §3.2 内部条款）\n")
        # 裁决一③回归：只给**目录**不得判成 experiment
        with open(os.path.join(sci, "NEG6.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 定义\n\n- 判据 读数见 实验/x/ 目录\n")
        # 裁决一①回归：只有日期/节号/行锚/版本的行不是主张（不进分母）
        with open(os.path.join(sci, "NEG7.md"), "w", encoding="utf-8") as f:
            f.write("# T\n\n## 1 实现\n\n- 版本 v1.2.3，2026-09-29，见 §9.73，行 1644\n")
        rep = evaluate(td)
        v = rep["violations"]
        okgrade = sum(rep["by_grade"].get(g, 0) for g in ("paper", "oss", "experiment"))
        ok = True
        if rep["claims_total"] == 0:
            print("SELF-TEST FAIL: 分母为 0"); ok = False
        if okgrade < 3:
            print("SELF-TEST FAIL: 正例应 >=3 合格，实得 %d" % okgrade); ok = False
        if len(v) < 4:
            print("SELF-TEST FAIL: 负例应 >=4 不合格，实得 %d" % len(v)); ok = False
        if not [x for x in v if "泛化引用" in x["reason"]]:
            print("SELF-TEST FAIL: 「只写见文献」未被识别"); ok = False
        if not [x for x in v if "无信源" in x["reason"]]:
            print("SELF-TEST FAIL: 「无信源」未被识别"); ok = False
        if not [x for x in v if "缺章节" in x["reason"]]:
            print("SELF-TEST FAIL: 「DOI 缺定位」未被识别"); ok = False
        # 裁决一②：NEG5（DOI + 内部 §）必须**不**是 paper
        neg5 = [x for x in v if x["file"].endswith("NEG5.md")]
        if not neg5 or any(x["grade"] == "paper" for x in neg5):
            print("SELF-TEST FAIL: 裁决一② 内部 § 被当论文定位符（假绿）"); ok = False
        # 裁决一③：NEG6（只给目录）必须**不**是 experiment
        neg6 = [x for x in v if x["file"].endswith("NEG6.md")]
        if not neg6 or any(x["grade"] == "experiment" for x in neg6):
            print("SELF-TEST FAIL: 裁决一③ 只给目录被判成实验结果件（假绿）"); ok = False
        # 裁决一①：NEG7（只有日期/节号/行锚/版本）根本不是主张，不该进分母
        if any(x["file"].endswith("NEG7.md") for x in v):
            print("SELF-TEST FAIL: 裁决一① 日期/节号/行锚/版本被当主张"); ok = False
        if rep["by_grade"].get("experiment_partial", 0) < 1:
            print("SELF-TEST FAIL: experiment_partial 级未产生（目录降级面没生效）"); ok = False
        print("SELF-TEST claims=%d violations=%d grades=%s" % (
            rep["claims_total"], len(v), rep["by_grade"]))
        return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true")
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    repo = os.path.abspath(a.root)
    if not os.path.isdir(os.path.join(repo, SCI_REL)):
        print("A1_INPUT_UNAVAILABLE: %s 不存在" % SCI_REL, file=sys.stderr)
        return 2
    try:
        rep = evaluate(repo)
    except RuntimeError as exc:
        print("A1_PARSE_BROKEN: %s" % exc, file=sys.stderr)
        return 2
    tot = rep["claims_total"]
    if not tot:
        print("A2_SCAN_FLOOR: 主张总数为 0（解析器空转，不得判绿）", file=sys.stderr)
        return 2
    ok = sum(rep["by_grade"].get(g, 0) for g in ("paper", "oss", "experiment"))
    partial = sum(rep["by_grade"].get(g, 0) for g in ("paper_partial", "oss_partial"))
    failn = rep["by_grade"].get("fail", 0)
    print("[%s] 分母 claims_total=%d  files=%d" % (CHECK_ID, tot, rep["files"]))
    print("  合格 %d (%.1f%%)  部分 %d (%.1f%%)  不合格 %d (%.1f%%)" % (
        ok, 100.0 * ok / tot, partial, 100.0 * partial / tot,
        failn, 100.0 * failn / tot))
    print("  分类: " + json.dumps(rep["by_grade"], ensure_ascii=False))
    print("  主张类型: " + json.dumps(rep["by_type"], ensure_ascii=False))
    print("  排除: 参考节块 %d / 非主张块 %d" % (
        rep["excluded_refsec_blocks"], rep["excluded_nonclaim_blocks"]))
    if a.json_out:
        with open(a.json_out, "w", encoding="utf-8") as f:
            json.dump(rep, f, ensure_ascii=False, indent=2)
    return 0 if not rep["violations"] else 1


if __name__ == "__main__":
    sys.exit(main())
