#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""DOC-DESIGN-SECTION-REFS | 最高设计（ASTROCS_DESIGN.md）章节引用存在性门。

防的复发缺口（本门的存在理由）
  最高设计是权威链顶点，下级文档 / 机器合同 / 头文件注释大量以
  "docs/ASTROCS_DESIGN.md §<N[.N...]>" 的形式指向它的条款。**指向的节号
  是否真的存在，此前全仓无门**。实测（FINAL-07）：§3 只有 3.1/3.2/3.3，
  而写作 "ASTROCS_DESIGN.md §3.5" 的引用散布在
  eng/contracts/**、eng/packaging/config/**、eng/tools/quality/**、
  lib/include/**、lib/infrastructure/cli/**、eng/tests/**、
  docs/KNOWN_LIMITATIONS.md —— 一个**从不存在的节号**被机器合同引用，
  指向「内存 / CPU / 线程不设门」这条已生效的硬语义（真身在 §4.5 运行前预检）。
  条款号是给人读的门面：号错 ⇒ 读者按号回读必然落空，而 CI 全绿。

与既有门的分工（**不重复造已有的那部分**）
  * eng/tools/migration/check_md_links.py（DOC 迁移断链门）只认 markdown
    链接语法 [text](target) 的**路径**是否存在：其正则的字符类排除了空白，
    因此 "docs/ASTROCS_DESIGN.md §3.5" 这类「散文 / JSON / 注释里的裸引用」
    **整条不可见**；它还在 "#" 处切掉锚点，从不校验锚点本身。路径那面归它。
  * eng/tools/check_glossary.py G5 校验锚点存在性，但作用域只有
    docs/GLOSSARY.md 一张表的锚列，且其 "§([0-9]+[a-z]?)" 与
    "^#{1,6}\s+([0-9]+[a-z]?)" 两个字符类**都表达不了多级节号**：
    "### 3.5 x" 会被读成节号 "3"，"docs/ASTROCS_DESIGN.md §3.5" 这类整条锚不匹配。
  * **本门只补「章节号」这一面**：全仓扫 ASTROCS_DESIGN 引用，解析出节号，
    逐个核对目标文档的真实编号标题集合；路径不存在、路径写错、语义对不对
    **一律不判**（分别是上面那些门与人工复核的事）。

判据（任一 S 违规 ⇒ exit 1；输入不可用 ⇒ exit 2，fail-closed）
  S1 扫描面    遍历工作树（**含未跟踪的在制文件**），按扩展名白名单取文本文件；
               目录剪枝与证据面见下方常量，逐条有理由，不设豁免台账。
  S2 目标面    docs/ASTROCS_DESIGN.md 必须存在、可读、且解析出 >=1 个编号标题；
               否则 S2_TARGET_UNAVAILABLE / S2_NO_NUMBERED_HEADING（rc=2）。
               编号标题集合 = "^ {0,3}#{1,6}\s*([0-9]+(?:\.[0-9]+)*)" 的全体
               捕获（**多级节号必须能表达**，这是本门与 check_glossary 的关键差）。
  S3 引用存活  行内出现 ASTROCS_DESIGN[.md] 提及时，归属到它的 "§<N[.N...]>"
               必须命中 S2 的标题集合；未命中 ⇒ SECTION_REF_MISSING，逐条点名
               （文件:行号 + 原引用片段 + 该文档实际存在的同层节号）。
  S4 证据面    独立审计/、artifacts/ 下的同名引用**照扫照报**，但归入
               evidence_surface，不参与判红：AGENTS.md §目录落位速查写明
               「独立审计/ = 独立审计交付件（只读参照，不作构建 / 门禁输入）」
               「artifacts/ = 证据与产物」；改写审计留痕会篡改历史记录。
               该面命中数逐次打印并写进 JSON，**不得静默**。
  S5 扫描下限  归属到本门的 § 引用总数为 0 ⇒ 判红 rc=2
               （SCAN_FLOOR：解析器空转不得判绿；本门必须真在扫）。
  S6 自身排除  排除本文件自身（自检夹具里含故意的坏节号）；除此之外无任何排除。

解析规则（归属判定，保守到不误判、严格到不漏判）
  1. 逐行用一条**有序** token 正则切出四类 token：
     astrocs（ASTROCS_DESIGN 提及）> file（任何 <名>.<扩展名> 文件提及）>
     sec（§<N[.N...]>）> caps（全大写标识符，如 GAP_AUDIT / CLI-002）。
  2. 每个 sec 的归属**必须可绑定**（v2.1 改法；v1 的「左侧最近文档类 token」无界，
     同一行左侧出现过 ASTROCS_DESIGN 就把宪章号 / 裁决号 / 他合同节号全记到最高设计
     账上 ⇒ 实测 45 红里 36 条是这类误报）。绑定三条路径，逐条有理由：
     - 左向绑定：取左侧最近的文档类 token，跨度 ≤ BIND_GAP_MAX(32) 字符、不跨真句读
       （。；！？）、且 span 内不得夹带别的 §（夹带了就不是绑定，是另一句话）；
     - 链首继承：§A/§B/§C 链，链内 § 归属 = 链首归属，跨度只量「链首→归属词」这一段
       （否则链式里的死节号被无主化 —— 判据「看不见」不等于「不存在」）；
     - 右向倒装回退（owner=right）：左向不可绑定且本 § 是链首时，右侧 6 字符内的文档
       token 兜底（覆盖「§3.5 见 docs/ASTROCS_DESIGN.md」）；该节号若是**本文件自己**的
       编号标题则判 self，不得算到最高设计头上。
     归属结果是 astrocs ⇒ 进入 S3 判据；是 file / caps ⇒ 不是本门判据
     （例如 "GAP_AUDIT(RELEASE-02) §9.74" 里的 §9.74 属那份不存在的文档，
     不由本门判，但**照样打印**，供人工处置）；判不出（unbound）⇒ 不判红，
     按「本文件自有节号 / 他文档节号 / 全仓无此节号」分面登记打印，不静默。
  3. 编号命名空间消歧：§ 后 RULING_NS_AFTER(14) 字符内出现「裁决/决议/裁定/宪章/作废/
     废止/退役/条款 ID」时，该号属**宪章/裁决**命名空间（§9.73 = 裁决号，不是
     ASTROCS_DESIGN §9 的子节）⇒ 归 ruling_ns_refs 面逐条打印，不判红。
  4. 显式注销语境：§ 同行其后 RETRACT_AFTER(80) 字符内、或下一行行首 80 字符内出现
     RETRACT_MARKERS（已废止/作废/原引/不存在…）⇒ 归 retired_refs 面，不判红。
     词表已收窄：中性词「已按 / 历史 / 不再 / 冻结层 / 本文件属」**不是**注销标记
     （否则「已按 §4.5 实现」「历史沿革见 RELEASE_STATUS」等于给真死锚发免死金牌）。

用法
  python3 eng/tools/doccheck/check_design_section_refs.py [--root .] [--json-out F]
  python3 eng/tools/doccheck/check_design_section_refs.py --self-test
exit 0 = 全部存活；1 = 有失效引用；2 = 输入不可用 / 扫描面为空（fail-closed）。
--self-test 恒 0 = 内置正 / 负例全符合预期。

只读；仅 stdlib；无网络；输出按 (文件, 行号) 稳定排序，跨 cwd 复跑一致。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

# ── 常量（每条都有理由，不设豁免台账）──────────────────────────────
CHECK_ID = "DOC-DESIGN-SECTION-REFS-V2"
TARGET_REL = "docs/ASTROCS_DESIGN.md"
SELF_REL = "eng/tools/doccheck/check_design_section_refs.py"

# 目录剪枝：构建树 / 过程产物区 / 外部只读数据集 / 版本库 / 第三方 vendored。
# 依据：AGENTS.md §目录落位速查（run/ 过程产物不入库；gaia/、testdata/ 为外部
# 只读数据集；lib/third_party/ 为第三方依赖）。
PRUNE_DIRS = frozenset((
    ".git", "build", "run", "gaia", "testdata", "node_modules", "__pycache__",
    "lib/third_party",
))
# 证据面：照扫照报，不判红（依据见 docstring S4）。
EVIDENCE_PREFIXES = ("独立审计/", "artifacts/")

TEXT_EXTS = frozenset((
    ".md", ".json", ".py", ".cpp", ".cc", ".cxx", ".h", ".hpp", ".txt",
    ".yaml", ".yml", ".csv", ".cmake", ".ps1", ".sh", ".in", ".inc",
))
MAX_BYTES = 4 * 1024 * 1024

# ── v2 新增：绑定邻接判据（详见 判据修复方案.md）────────────
# 归属词与 § token 之间只允许出现「绑定分隔符」：不得夹带其它 § token，
# 不得跨句读/分读（。；！？），且跨度不得超限。否则该 § 的归属不可判定。
BIND_GAP_MAX = 32          # 左侧绑定最大跨度（字符）
RIGHT_FALLBACK_MAX = 6     # 右侧倒装回退最大跨度（字符）：覆盖「§3.5 见 docs/ASTROCS_DESIGN.md」
# v2.1 修：0xFF0C（，）**不是句读**。v2 把它当句读 ⇒ 「docs/ASTROCS_DESIGN.md，§3.5」
# 与「依据 …§10.2（…）」这类**真死锚**被无主化（实测 7/12 形态漏检，见 out/v2_falsifiability.json）。
# 只保留真句读：。；！？
SENTENCE_BREAK = frozenset(chr(0x3002) + chr(0xff1b) + chr(0xff01) + chr(0xff1f) + chr(10) + chr(13))
# 注销语境标记：出现即视为「显式标注的历史/已废止引用」，归报告面不判红。
RETRACT_MARKERS = (
    "已废止", "已作废", "作废", "已删除", "已死",
    "不存在", "原引", "原惯章", "原引用",
    "已注销",
)
# v2.1 修：**词表收窄** —— 删掉「已按 / 历史 / 冻结层 / 不再 / 本文件属」，它们是中性词：
#   「已按 §4.5 实现」「历史沿革见 RELEASE_STATUS」在仓内是常规写法，当注销标记用等于
#   给真死锚发免死金牌（v2 的宽词表把 E7 形态整条吞掉：真死锚的下一行写「历史沿革」
#   就把上一行豁免掉）。self-test N13 锁住这一条。
# v2.1 **保留**（更正副本里一处与实现矛盾的注释）：is_retracted 的第二段仍看
#   **下一行**前 RETRACT_AFTER 字符。它是「死锚自述」（类 D：该锚在现行设计文档里整条
#   不存在）唯一的抓手，删掉会让 eng/ci/check_mutation_gates.py:17 变红。实测：
#   仓内 5 条注销面条目中 1 条（就是这一行）只靠下一行命中（探针：
#   run/FINAL-07/docs-gate-v2/probe_retract_window.py，输出随交付落盘）。
#   残留取舍：真死锚若**下一行恰好含收窄词表里的词**（已废止/作废/原引/…）仍会被豁免 ——
#   这是设计取舍，豁免面 5 条已逐条 read 确认都是真注销自述，不是恒真门。
RETRACT_AFTER = 80         # 注销标记只认 § token **之后同一行**的最大字符距离
# 刻意不设「前向」窗口：前向窗口会把「原引…已废止；现行依据 = ASTROCS_DESIGN.md §3.5」
# 这类同行真死锚一起豁免掉（self-test N3c 就是锁这条的）。
  # 超大文件跳过并计数（fail-closed 靠 S5 兜底）

# S2：目标文档的编号标题。**多级**节号必须能表达（"### 4.5 x" -> "4.5"）。
# 顶层章写作 "## 4. normalize"、子节写作 "### 4.5 运行前预检"：
# 两种都要能收进集合（"§4" 指章、"§4.5" 指节）。因此**不能**在数字后再加
# "后面不是点" 的否定断言 —— 那会把 "## 4. xxx" 的 "4" 一并拒掉，
# 令 §0/§4/§7/§8/§9/§10/§11/§12/§13 这些**真实存在**的章引用全判红。
HEADING_NUM_RE = re.compile(r"^\s{0,3}#{1,6}\s*([0-9]+(?:\.[0-9]+)*)(?![0-9])")
# S3：行内 token 切分。**顺序即优先级**（astrocs 必须排在 file 之前，
# 否则 "docs/ASTROCS_DESIGN.md" 会先被 file 规则吃掉）。
_CJK = "\u4e00-\u9fff"
TOKEN_RE = re.compile(
    r"(?P<astrocs>[A-Za-z0-9_." + _CJK + r"/-]*ASTROCS_DESIGN(?:\.md)?)"
    r"|(?P<file>[A-Za-z0-9_." + _CJK + r"/-]*"
    r"\.(?:md|json|ya?ml|csv|txt|cmake|c|h|hpp|cpp|cc|cxx|py|ps1|sh|in|inc))"
    r"|(?P<sec>\u00a7\s*(?P<secnum>[0-9]+(?:\.[0-9]+)*))"
    r"|(?P<caps>\b[A-Z][A-Z0-9]*(?:_[A-Z0-9]+)+\b|\b[A-Z]{3,}[0-9]+\b)"
)


def target_section_numbers(repo):
    """S2：目标文档真实存在的编号标题集合。返回 (集合 | None)。"""
    path = os.path.join(repo, TARGET_REL)
    if not os.path.isfile(path):
        return None
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            text = fh.read()
    except OSError:
        return None
    nums = set()
    for ln in text.splitlines():
        m = HEADING_NUM_RE.match(ln)
        if m:
            nums.add(m.group(1))
    return nums


def iter_text_files(repo):
    """S1：遍历工作树（含未跟踪的在制文件）。产出 (rel_path, abspath)。"""
    out = []
    for dirpath, dirnames, filenames in os.walk(repo):
        dirnames[:] = sorted(d for d in dirnames if d not in PRUNE_DIRS)
        for fn in sorted(filenames):
            ext = os.path.splitext(fn)[1].lower()
            if ext not in TEXT_EXTS:
                continue
            ap = os.path.join(dirpath, fn)
            rel = os.path.relpath(ap, repo).replace(os.sep, "/")
            if rel == SELF_REL:
                continue
            out.append((rel, ap))
    return out


def _tok_slice(line, start, num):
    """返回紧邻 start 之前 / 之后的文档类 token（跳过 § 自身）。"""
    toks = []
    for m in TOKEN_RE.finditer(line):
        if m.group("sec") is not None:
            toks.append(("sec", m.group("sec"), m.start("sec"), m.group("secnum")))
            continue
        for kind in ("astrocs", "file", "caps"):
            if m.group(kind) is not None:
                toks.append((kind, m.group(kind), m.start(kind), None))
                break
    return toks


# v2.1 修：原判据还要求 span 内不含其它 § token，这等于宣布「链式引用不可归属」，
# 于是 "docs/ASTROCS_DESIGN.md §8.3/§9/§3.5" 里的死节号 §3.5 被无主化（实测漏检 E2/E3）。
# 链内归属改由 attribute() 的「链首继承」处理，跨度只量「链首 -> 归属词」这一段。
CHAIN_JOIN = "/、,，;；·•  　"
# v2.1 补：裁决号 / 宪章号是**另一套编号命名空间**（§9.73 = 裁决号，不是 ASTROCS_DESIGN §9 的子节）。
# 行文自己写了命名空间标记（「裁决 §9.73」「宪章 §10.5」「作废 §8.2」）时才认为宪章号。
# 被移走的条目**逐条打印**到 ruling_ns_refs 面，不静默。
RULING_NS_RE = re.compile(r'^.{0,6}?[（(]?\s*(?:裁决|决议|裁定|宪章|作废|废止|退役|条款\s*ID)')
RULING_NS_AFTER = 14   # § token 后多少字符内出现命名空间词


def _bindable(gap, toks, lo, hi, allow_sec=False):
    """gap 是否是「绑定跨度」：不跨句读，且长度在限内。

    allow_sec=True 时（本 § 是**链内继承者**，归属已由链首定下）允许 span 内有其它 §。
    否则一律拒绝：「左侧最近归属词与本 § 之间夹了另一个 §」不算绑定。
    这条就是 v2 中保留、v2.1 不能丢的那一条（它拦下的正是「§2.1 + §9.73」这类跨命号引用）。
    """
    if not allow_sec and any(t[0] == "sec" and lo <= t[2] < hi for t in toks):
        return False
    for ch in gap:
        if ch in SENTENCE_BREAK:
            return False
    return len(gap) <= BIND_GAP_MAX


def attribute(line, limit=32, right_limit=6):
    """v2.1 归属：§ token 认「紧邻绑定 / 链首继承」的文档类 token。

    返回 [(sec_text, sec_num, owner_kind, owner_text, col)]，owner_kind 取值：
      astrocs / file / caps      —— 绑定成功，按原分工判红或不判；
      right                      —— 短跨度倒装回退（计入不判红面）；
      unbound                    —— 行内无可信归属词，判定权交 evaluate()。
    """
    toks = _tok_slice(line, 0, 0)
    docs = [t for t in toks if t[0] in ("astrocs", "file", "caps")]
    out = []
    for i, t in enumerate(toks):
        kind, text, start, num = t
        if kind != "sec":
            continue
        # 链首继承：向前跳过链内相邻的 §
        j, head = i, t
        while j > 0 and toks[j - 1][0] == "sec":
            gp = line[toks[j - 1][2] + len(toks[j - 1][1]):toks[j][2]]
            if any(ch not in CHAIN_JOIN for ch in gp):
                break
            j -= 1
            head = toks[j]
        owner = None
        left = [d for d in docs if d[2] < head[2]]
        if left:
            d = left[-1]
            gap = line[d[2] + len(d[1]):head[2]]
            if _bindable(gap, toks, d[2] + len(d[1]), head[2], allow_sec=(j != i)):
                owner = d
        if owner is None and j == i:
            right = [d for d in docs if d[2] > start]
            if right:
                d = right[0]
                gap = line[start + len(text):d[2]]
                if _bindable(gap, toks, start + len(text), d[2]) and len(gap) <= right_limit:
                    out.append((text, num, "right", d[1], start))
                    continue
        if owner is not None and RULING_NS_RE.match(
                line[start + len(text):start + len(text) + RULING_NS_AFTER]):
            out.append((text, num, "ruling_ns",
                        line[start + len(text):start + len(text) + RULING_NS_AFTER], start))
            continue
        out.append((text, num, owner[0] if owner else "unbound",
                    owner[1] if owner else None, start))
    return out

def mentions_astrocs(text):
    """S3b 前置：文本里是否出现过 ASTROCS_DESIGN 提及（直接问 tokenizer，
    不走 attribute() —— 后者只回吐 sec token）。"""
    return any(m.group("astrocs") is not None for m in TOKEN_RE.finditer(text))


def is_evidence(rel):
    return any(rel == p.rstrip("/") or rel.startswith(p) for p in EVIDENCE_PREFIXES)


def evaluate(repo):
    """返回 (report_dict, rc)。rc: 0 pass / 1 violation / 2 input-unavailable。"""
    nums = target_section_numbers(repo)
    files = iter_text_files(repo)
    report = {
        "check": CHECK_ID,
        "target": TARGET_REL,
        "files_scanned": len(files),
        "files_skipped_large": 0,
        "target_sections": sorted(nums) if nums else [],
        "refs_checked": 0,
        "violations": [],
        "evidence_surface": [],
        "foreign_section_refs": [],
        "bare_section_refs": [],
        "retired_refs": [],
        "self_section_refs": [],
        "other_doc_section_refs": [],
        "unresolved_section_refs": [],
        "ruling_ns_refs": [],
    }
    if nums is None:
        report["error"] = "S2_TARGET_UNAVAILABLE: %s 不存在或不可读" % TARGET_REL
        return report, 2
    if not nums:
        report["error"] = "S2_NO_NUMBERED_HEADING: %s 解析出 0 个编号标题" % TARGET_REL
        return report, 2
    if not files:
        report["error"] = "S1_SCAN_SURFACE_EMPTY: 扫描面为 0 个文本文件（fail-closed）"
        return report, 2

    self_headings = {}

    def self_nums(rel):
        if rel not in self_headings:
            s = set()
            if rel.endswith(".md") and rel != TARGET_REL:
                try:
                    with open(os.path.join(repo, rel), encoding="utf-8",
                              errors="replace") as fh:
                        for ln in fh.read().splitlines():
                            hm = HEADING_NUM_RE.match(ln)
                            if hm:
                                s.add(hm.group(1))
                except OSError:
                    s = set()
            self_headings[rel] = s
        return self_headings[rel]

    known_elsewhere = set()
    for rel, ap in files:
        if rel == TARGET_REL or not rel.endswith(".md"):
            continue
        known_elsewhere |= self_nums(rel)

    def is_retracted(rel, lines, i, start):
        """§ token 是否落在显式注销语境里（v2.1：**只看同一行、只看它后面**）。

        v2 终版有 (b)「下一行行首 80 字符」窗口 —— 实测漏检：一条真死锚的**下一行**
        恰好写「历史沿革见 RELEASE_STATUS」就被整条豁免（out/v2_falsifiability.json E7）。
        跨行否定改为判红（保守方向）。
        """
        cur = lines[i - 1] if 0 < i <= len(lines) else ""  # i 是 1-based 行号
        tail = cur[start + 1:start + 1 + RETRACT_AFTER]
        for mk in RETRACT_MARKERS:
            if mk in tail:
                return True
        if i < len(lines):
            nxt = lines[i][:RETRACT_AFTER]
            for mk in RETRACT_MARKERS:
                if mk in nxt:
                    return True
        return False

    for rel, ap in files:
        try:
            if os.path.getsize(ap) > MAX_BYTES:
                report["files_skipped_large"] += 1
                continue
            with open(ap, encoding="utf-8", errors="replace") as fh:
                lines = fh.read().splitlines()
        except OSError:
            continue
        mentions = mentions_astrocs("\n".join(lines))
        for i, line in enumerate(lines, 1):
            for text, num, owner, owner_text, col in attribute(line):
                item = {
                    "file": rel, "line": i, "column": col + 1,
                    "ref": text.strip(), "section": num,
                    "owner": owner, "owner_text": (owner_text or "").strip(),
                    "context": line.strip()[:200],
                }
                # 自锚优先：右向倒装绑定时，若该节号是**本文件自己的**编号标题，
                # 判 owner=self（§1.4/§1.5 指本报告），不得算到 ASTROCS_DESIGN 头上。
                self_is = rel.endswith(".md") and num in self_nums(rel)
                bound_astrocs = (owner == "astrocs"
                                or (owner == "right" and owner_text
                                    and "ASTROCS_DESIGN" in owner_text
                                    and not self_is))
                if bound_astrocs:
                    report["refs_checked"] += 1
                if (bound_astrocs and num not in nums and not is_evidence(rel)
                        and is_retracted(rel, lines, i, col)):
                    item["rule"] = "RETIRED_REF_REPORT"
                    item["message"] = (
                        "%s:%d 引用 %s §%s（显式注销语境：已废止/作废/原引/不存在）"
                        % (rel, i, TARGET_REL, num))
                    report["retired_refs"].append(item)
                    continue
                if bound_astrocs:
                    if num in nums:
                        continue
                    item["rule"] = "SECTION_REF_MISSING"
                    item["message"] = (
                        "%s:%d 引用 %s §%s，但该文档没有此节号"
                        % (rel, i, TARGET_REL, num))
                    item["existing_same_top"] = sorted(
                        n for n in nums if n.split(".")[0] == num.split(".")[0])
                    if is_evidence(rel):
                        report["evidence_surface"].append(item)
                    else:
                        report["violations"].append(item)
                elif owner == "ruling_ns" and not is_evidence(rel):
                    item["rule"] = "RULING_NS_REPORT"
                    report["ruling_ns_refs"].append(item)
                elif owner in ("file", "caps") and not is_evidence(rel):
                    # 不判红但打印：该 sec 属另一份文档 / 标识符；若那份文档不存在，
                    # 属另一类悬空引用（人工处置面）。
                    report["foreign_section_refs"].append(item)
                elif (owner == "right" or owner == "unbound") and self_is:
                    report["self_section_refs"].append(item)
                elif owner == "unbound" and num in known_elsewhere:
                    report["other_doc_section_refs"].append(item)
                elif owner == "unbound":
                    item["rule"] = "UNRESOLVED_SECTION_REPORT"
                    item["message"] = (
                        "%s:%d 裸 §%s：行内无可信归属词，且该节号在全仓任何文档里都不存在"
                        % (rel, i, num))
                    report["unresolved_section_refs"].append(item)
                elif owner == "unbound" and mentions and not is_evidence(rel):
                    # S3b 报告面（不判红）：本行是裸 §，且本文件别处提过 ASTROCS_DESIGN。
                    # 「上文声明权威、正文用缩写」与「本文件自有章节号」机械上不可分，
                    # 兜底归属会造假红 ⇒ 只登记、只打印。
                    item["rule"] = "BARE_SECTION_REPORT"
                    item["message"] = (
                        "%s:%d 裸 §%s（行内无文档 token；本文件别处提过 ASTROCS_DESIGN）"
                        % (rel, i, num))
                    report["bare_section_refs"].append(item)

    if report["refs_checked"] == 0:
        report["error"] = (
            "S5_SCAN_FLOOR: 归属到 %s 的 § 引用总数为 0（解析器空转，不得判绿）"
            % TARGET_REL)
        return report, 2

    def _key(d):
        return (d["file"], d["line"], d["column"])

    report["violations"].sort(key=_key)
    report["evidence_surface"].sort(key=_key)
    report["foreign_section_refs"].sort(key=_key)
    report["bare_section_refs"].sort(key=_key)
    return report, (1 if report["violations"] else 0)


def _fmt(report, rc):
    out = []
    if report.get("error"):
        out.append("%s: %s" % (CHECK_ID, report["error"]))
    out.append(
        "扫描 %d 个文本文件（超大跳过 %d）；%s 编号标题 %d 个；本门判定引用 %d 条"
        % (report["files_scanned"], report["files_skipped_large"],
           TARGET_REL, len(report["target_sections"]), report["refs_checked"]))
    for v in report["violations"]:
        out.append("  [红] %s" % v["message"])
        out.append("       原文: %s" % v["context"])
        if v["existing_same_top"]:
            out.append("       同层实际存在: %s" % ", ".join(v["existing_same_top"]))
    if report["evidence_surface"]:
        out.append("  证据面（不判红：只读参照 / 证据留档）%d 条:"
                   % len(report["evidence_surface"]))
        for v in report["evidence_surface"]:
            out.append("       %s:%d %s §%s"
                       % (v["file"], v["line"], v["ref"], v["section"]))
    if report["bare_section_refs"]:
        out.append("  裸 § 报告面（不判红：行内无文档 token，人工判定归属）%d 条:"
                   % len(report["bare_section_refs"]))
        for v in report["bare_section_refs"]:
            out.append("       %s:%d §%s  %s"
                       % (v["file"], v["line"], v["section"], v["context"][:90]))
    for key, title in (("ruling_ns_refs", "宪章/裁决号面（不判红）"),
                       ("retired_refs", "注销引用面（不判红：显式标注已废止/死锚/原引）"),
                       ("self_section_refs", "本文件自身节号面（不判红：owner=self）"),
                       ("other_doc_section_refs", "其他文档节号面（不判红：owner=他文档）"),
                       ("unresolved_section_refs", "无主节号面（不判红：全仓无此节号）")):
        if report.get(key):
            out.append("  %s %d 条:" % (title, len(report[key])))
            for v in report[key][:400]:
                out.append("       %s:%d %s §%s"
                           % (v["file"], v["line"], v["ref"], v["section"]))
            if len(report[key]) > 400:
                out.append("       ... 其余 %d 条见 JSON" % (len(report[key]) - 400))
    if report["foreign_section_refs"]:
        out.append("  非本门判据的 § 引用（属其他文档 / 标识符，打印供人工处置）%d 条:"
                   % len(report["foreign_section_refs"]))
        for v in report["foreign_section_refs"]:
            out.append("       %s:%d %s §%s ← %s"
                       % (v["file"], v["line"], v["ref"], v["section"],
                          v["owner_text"]))
    out.append("== %s: %s（rc=%d）" % (CHECK_ID, "FAIL" if rc else "PASS", rc))
    return "\n".join(out)


def _write_fixture(root, design_sections, files):
    os.makedirs(os.path.join(root, "docs"), exist_ok=True)
    lines = ["# Fixture", ""]
    for s in design_sections:
        depth = s.count(".") + 2
        lines.append("%s %s Title" % ("#" * depth, s))
        lines.append("")
    with open(os.path.join(root, TARGET_REL), "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")
    for rel, body in files.items():
        ap = os.path.join(root, rel)
        parent = os.path.dirname(ap)
        if parent:
            os.makedirs(parent, exist_ok=True)
        with open(ap, "w", encoding="utf-8") as fh:
            fh.write(body)


def self_test():
    cases = []
    base = ["3.1", "3.2", "3.3", "4.5", "6.1", "6.2", "6.3", "8.3", "9", "12.5"]
    tmp = tempfile.mkdtemp(prefix="section-refs-selftest-")
    try:
        def run(name, sections, files, want_rc, want_ids=()):
            d = os.path.join(tmp, name)
            os.makedirs(d, exist_ok=True)
            _write_fixture(d, sections, files)
            rep, rc = evaluate(d)
            ids = sorted(v["rule"] for v in rep["violations"])
            ok = (rc == want_rc and all(i in ids for i in want_ids))
            cases.append([name, want_rc, rc, ids, ok, rep])
            if not ok:
                print("  MISMATCH %s want rc=%d got rc=%d ids=%s"
                      % (name, want_rc, rc, ids))
                print("       %s" % _fmt(rep, rc))
            return rep

        # N0 正例：指向真实存在的**多级**节号 ⇒ PASS
        run("N0_alive_multi", base,
            {"eng/c.json": '{"a": "docs/ASTROCS_DESIGN.md \u00a74.5 \u8fd0\u884c\u524d\u9884\u68c0"}'},
            0)
        # N1 负例（本题注入形态）：不存在的 \u00a73.5 ⇒ 红
        run("N1_dead_3_5", base,
            {"eng/c.json": '{"a": "docs/ASTROCS_DESIGN.md \u00a73.5\uff08\u9884\u68c0\u4e09\u7ea7\uff09"}'},
            1, ("SECTION_REF_MISSING",))
        # N2 无 .md 后缀形态同样判红（漏判防护）
        run("N2_no_md_suffix", base,
            {"lib/x.h": "// \u4f9d\u636e docs/ASTROCS_DESIGN \u00a73.5"},
            1, ("SECTION_REF_MISSING",))
        # N3 链式引用（v2.1 恢复 v1 口径）：链首 §8.3 绑定到 ASTROCS_DESIGN，
        # 链内 §9 / §3.5 **继承链首归属** ⇒ §3.5 不存在，必须判红。
        # v2 把预期改成 rc=0（「链内全部继承归属 = 31 条误报机制」），
        # 那是**把自测改成迎合实现**：v2 实测漏检 7/12真死锚形态，其中 2 条就是链式。
        rep = run("N3_chain_v2", base,
                  {"lib/x.cpp": "// docs/ASTROCS_DESIGN.md §8.3/§9/§3.5"},
                  1, ("SECTION_REF_MISSING",))
        assert len(rep["violations"]) == 1, \
            "链内死节应恰好 1 条红，实得 %d" % len(rep["violations"])
        # N3b 反向负例：死节号**紧邻**归属词时必须判红（防判据被放松成漏判）
        rep = run("N3b_chain_head_dead", base,
                  {"lib/x.cpp": "// docs/ASTROCS_DESIGN.md §3.5/§8.3/§9"},
                  1, ("SECTION_REF_MISSING",))
        assert len(rep["violations"]) == 1, \
            "紧邻死节号应恰好 1 条红，实得 %d" % len(rep["violations"])
        # N4 归属边界：\u00a712 \u5c5e ENGINEERING_SPEC.md\uff0c\u4e0d\u7b97\u672c\u95e8\u5224\u636e
        run("N4_other_doc", base,
            {"eng/c.json":
             '{"a": "docs/ASTROCS_DESIGN.md \u00a73.5 + ENGINEERING_SPEC.md \u00a712"}'},
            1, ("SECTION_REF_MISSING",))
        run("N4b_other_doc_alive", base,
            {"eng/c.json":
             '{"a": "docs/ASTROCS_DESIGN.md \u00a74.5 + ENGINEERING_SPEC.md \u00a712"}'},
            0)
        # N5 大写标识符边界：\u00a79.74 \u5c5e GAP_AUDIT\uff0c\u4e0d\u7b97\u672c\u95e8\u5224\u636e
        run("N5_caps_boundary", base,
            {"eng/contracts/g.json":
             '{"x": "GAP_AUDIT(RELEASE-02) \u00a79.74 \u88c1\u51b3 10 + '
             'docs/ASTROCS_DESIGN.md \u00a74.5"}'},
            0)
        # N6 倒装（\u00a7 \u5728\u524d\u3001\u63d0\u53ca\u5728\u540e\uff09\u4e5f\u80fd\u5f52\u5c5e
        run("N6_postfix_owner", base,
            {"docs/KNOWN_LIMITATIONS.md":
             "- \u89c1 \u00a73.5 docs/ASTROCS_DESIGN.md"},
            1, ("SECTION_REF_MISSING",))

        # ── v2 新增：六类误报形态的可执行负例（必须判绿）──────────
        # F1 同文档自身节号；右侧只有超限跨度，不应判给最高设计
        run("F1_self_own_section", base,
            {"docs/engineering/UNIFIED_OBJECTS.md":
             "## 31. V6 合同层数据合同\n\n> 语义权威 = 本 §31 正文（唯一权威链见 ASTROCS_DESIGN.md §4.5）。\n"},
            0)
        # F2 裁决号在行首、归属词在右侧且跨度超限 ⇒ 不判红
        run("F2_ruling_number_first", base,
            {"eng/tests/unit/t.cpp":
             "// 一律 RecordOnly（§9.74 裁决 10 + docs/ASTROCS_DESIGN §4.5）"},
            0)
        # F3 连字符条款 ID 应被认作文档类 token，其 § 归属该 ID
        run("F3_hyphen_clause_id", base,
            {"lib/x.cpp":
             "// 审计面（DATA-UNC-001 §30.1 规则 1 / docs/ASTROCS_DESIGN §3.1）"},
            0)
        # F4 中文文档名当归属词时，紧邻它的 § 不得被判给左侧的 ASTROCS_DESIGN
        run("F4_cjk_owner_word", base,
            {"eng/contracts/schemas/s.json":
             "'d': '（docs/ASTROCS_DESIGN.md §4.5；输出合同 §3.5：HiPS）'"},
            0)
        # F5 显式注销语境（死锚自述）⇒ 归注销面，不判红
        rep = run("F5_dead_anchor_narration", base,
                  {"eng/ci/g.json":
                   "  'h': '原引用 docs/ASTROCS_DESIGN.md §3.5:1「x」在现行设计文档中整条不存在，该锚已死。'"},
                  0)
        assert len(rep["retired_refs"]) == 1, \
            "注销面应报 1 条，实得 %d" % len(rep["retired_refs"])
        # F6 显式标注作废的旧宪章引用 ⇒ 不判红
        run("F6_retired_charter", base,
            {"docs/engineering/DOCUMENT_GOVERNANCE.md":
             "- （原引「宪章 §3.5/§18.2」已废止；现行 = ASTROCS_DESIGN.md §4.5）。"},
            0)
        # N3c 反向负例：注销标记不得掩盖同行的真死锚
        run("N3c_retired_marker_no_mask", base,
            {"docs/engineering/DOCUMENT_GOVERNANCE.md":
             "- 原引已废止；现行依据 = ASTROCS_DESIGN.md §3.5。"},
            1, ("SECTION_REF_MISSING",))

        # ── v2.1 补：四条「v2 漏检形态」的可执行反证（REPAIR-REPORT §1(b)(1)(3)(4)(6)）
        #     副本 patches/check_design_section_refs_v21.py 漏了本组，落地时补入；
        #     每条都锁一个机制，机制被改回去时本门立刻红（不是恒真门）。
        # N11 逗号不是句读：v2 的 SENTENCE_BREAK 含 0xFF0C（，），
        #     「docs/ASTROCS_DESIGN.md，§3.5」被判成跨句 ⇒ 真死锚被无主化
        rep = run("N11_comma_binding", base,
                  {"lib/x.cpp": "依据 docs/ASTROCS_DESIGN.md，§3.5 规定。"},
                  1, ("SECTION_REF_MISSING",))
        assert [v["section"] for v in rep["violations"]] == ["3.5"], \
            "N11 应恰好 1 条红且为 §3.5，实得 %s" % (
            [v["section"] for v in rep["violations"]],)
        # N12 长跨度散文 + 逗号：同上机制的长句形态（gap 里含逗号）
        rep = run("N12_long_gap", base,
                  {"lib/x.cpp":
                   "// 依据 docs/ASTROCS_DESIGN.md 的输出基数条款，详见 §3.5"},
                  1, ("SECTION_REF_MISSING",))
        assert [v["section"] for v in rep["violations"]] == ["3.5"], \
            "N12 应恰好 1 条红且为 §3.5，实得 %s" % (
            [v["section"] for v in rep["violations"]],)
        # N13 下一行不得发免死金牌：死锚 §3.5 的**下一行**写「历史沿革」。
        #     v2 的 RETRACT_MARKERS 含中性词「历史」+ 下一行 80 字符窗口 ⇒ 整条被吞。
        #     v2.1 收窄词表（历史/已按/不再/冻结层/本文件属 全部移出）⇒ 必须仍红。
        rep = run("N13_nextline_not_retract", base,
                  {"lib/x.cpp":
                   "// 依据 docs/ASTROCS_DESIGN.md §3.5。\n- 历史沿革见 RELEASE_STATUS。\n"},
                  1, ("SECTION_REF_MISSING",))
        assert len(rep["retired_refs"]) == 0, \
            "N13 不得进注销面，实得 %d 条" % len(rep["retired_refs"])
        # N14 宪章/裁决号消歧不得越权：行文**没有**「裁决/宪章」字样时，
        #     §9.73 就是一条指向最高设计的死锚，必须红（词表只给候选，不是缺陷豁免）。
        #     用 §4.5（base 中存在）当链首，使红恰好 1 条且就是 §9.73，机制被隔离。
        rep = run("N14_ruling_ns_not_mask", base,
                  {"lib/x.cpp": "// 权重口径 docs/ASTROCS_DESIGN §4.5 / §9.73"},
                  1, ("SECTION_REF_MISSING",))
        assert [v["section"] for v in rep["violations"]] == ["9.73"], \
            "N14 红应是 §9.73，实得 %s" % (
            [v["section"] for v in rep["violations"]],)
        assert not [v for v in rep["ruling_ns_refs"] if v["section"] == "9.73"], \
            "N14 的 §9.73 不得被 RULING_NS_RE 吃掉（无「裁决/宪章」字样）"
        # N15 裸 § 必须落到可打印的报告面（v1 的 `and True` 把这一面变死分支；
        #     v2.1 删掉 `and True`，但分支顺序上 unresolved 面在前，bare 面仍不命中）。
        #     夹具里另放一条**活的** ASTROCS_DESIGN 引用，避免 S5 扫描下限把本例打成 rc=2。
        #
        #     断言**收紧**（FINAL-07 docs-gate-v2 交付；实测依据，非推断）：
        #     原断言把四个面**相加**判非空，对本条要锁的死分支**无鉴别力** ——
        #     把 `elif owner == "unbound" and num in known_elsewhere:` 改回 v1 的
        #     `and True` 恒真 catch-all 后，裸 § 被并进 other_doc_section_refs，
        #     四个面之和仍是 2 ⇒ 原断言照样通过。实测 unresolved 2->0、other_doc 0->2
        #     （out/n11_n15_loadbearing.json 的 M4 变异体）。
        #     本夹具的 known_elsewhere 实为空（唯一 .md 是目标文档本身，被排除；
        #     note.md 无编号标题），故裸 § 必落 unresolved 面 ⇒ 直接钉死该面。
        rep = run("N15_bare_face_reachable", base,
                  {"eng/ci/note.md": "- 见 §3.5 与 §7。\n"
                   "- 权威链见 docs/ASTROCS_DESIGN.md §4.5。\n"},
                  0)
        assert len(rep["unresolved_section_refs"]) >= 1, \
            "N15：裸 § 必须落到 unresolved_section_refs，不得被恒真 catch-all 并进他面"
        assert not [v for v in rep["other_doc_section_refs"]
                    if v["section"] in ("3.5", "7")], \
            "N15：本夹具下 §3.5/§7 不得落 other_doc_section_refs（那正是死分支的表征）"
        # N7 证据面：照扫照报但不判红
        rep = run("N7_evidence_surface", base,
                  {"\u72ec\u7acb\u5ba1\u8ba1/x.md": "docs/ASTROCS_DESIGN.md \u00a73.5"},
                  0)
        assert len(rep["evidence_surface"]) == 1, "证据面应报 1 条"
        # N8 fail-closed：目标文档不存在 ⇒ rc=2
        d = os.path.join(tmp, "N8_target_missing")
        os.makedirs(os.path.join(d, "eng"), exist_ok=True)
        with open(os.path.join(d, "eng/c.json"), "w", encoding="utf-8") as fh:
            fh.write("{}")
        _rep, rc = evaluate(d)
        cases.append(["N8_target_missing", 2, rc, ["S2_TARGET_UNAVAILABLE"],
                      rc == 2, None])
        # N9 fail-closed：一个 \u00a7 引用都没有 ⇒ rc=2（解析器空转不得判绿）
        d = os.path.join(tmp, "N9_scan_floor")
        _write_fixture(d, base, {"eng/c.json": "{}"})
        _rep, rc = evaluate(d)
        cases.append(["N9_scan_floor", 2, rc, ["S5_SCAN_FLOOR"], rc == 2, None])
        # N10 目标文档无编号标题 ⇒ rc=2
        d = os.path.join(tmp, "N10_no_headings")
        _write_fixture(d, [], {"eng/c.json": '{"a":"docs/ASTROCS_DESIGN.md \u00a74.5"}'})
        _rep, rc = evaluate(d)
        cases.append(["N10_no_headings", 2, rc, ["S2_NO_NUMBERED_HEADING"],
                      rc == 2, None])
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    bad = [c for c in cases if not c[4]]
    for name, want, got, ids, ok, _rep in cases:
        print("  %-24s want rc=%d  got rc=%d  %s  %s"
              % (name, want, got, "OK" if ok else "MISMATCH", ids))
    print("== %s --self-test: %d 例，%d 例不符预期"
          % (CHECK_ID, len(cases), len(bad)))
    return 0 if not bad else 1


def main(argv=None):
    ap = argparse.ArgumentParser(description="最高设计章节引用存在性门")
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    repo = os.path.abspath(args.root)
    report, rc = evaluate(repo)
    print(_fmt(report, rc))
    if args.json_out:
        d = os.path.dirname(os.path.abspath(args.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            json.dump(report, fh, ensure_ascii=False, indent=2, sort_keys=True)
            fh.write("\n")
    return rc


if __name__ == "__main__":
    sys.exit(main())
