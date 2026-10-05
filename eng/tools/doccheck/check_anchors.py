#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""锚点与指针校验器（check_anchors）——抓「引用指向不存在的对象」。

这个工具存在的原因

三轮文档审稿反复暴露同一类隐蔽缺陷：文档之间互相引用时，引用指着的对象并不存在。
这类缺陷分两级，量级相同、隐蔽程度递增：

    路径悬空（dangling_path）  被引用的文件或目录根本不存在。
    锚悬空  （dangling_anchor） 文件存在，但被引用的章节/条目在文件里不存在。

只报「引用无效」会把「路径修了锚没修」这种最隐蔽的一类埋掉：路径能打开了，
读的人以为找到了内容，其实停在错误的章节上。因此两级必须分开计数、分开报。

第三类最危险，本项目专门踩过

    节名真 · 文件错（section_wrong_file）
    引用写的节名**确实存在**，只是不在被指的那份文件里，而在另一份文件里。
    按「节名不存在」的常规改法去修，会把真节名换成假节名——**越修越坏**。
    所以这一类必须单独报，并且把「这个节名真正在哪」一并告诉人。

章节存在性怎么判

本项目已实测出现「第几节」在 A 文件存在、在 B 文件不存在的情形。因此本工具
**逐文件解析真实标题**，为每个目标文件单独建立章节号集合，再判定该引用在该文件里
是否存在。绝不使用「这个编号在仓库里某处出现过」当作存在的证据。

同名不同义

同名目标必须分开报，不能折叠成「重复引用」，也不能默认取第一个：

    ambiguous_target（同名多义）  同一路径后缀匹配到多个真实文件，工具不替人选。
    untracked_target（目标存在但未入版本控制）  磁盘上有、git 没跟踪——不是悬空，
                                 但会让任何「索引↔磁盘」类门禁在干净克隆上失真。

工具的定位（重要）

按新正本：测试是独立工具集、不是门禁。因此本工具

    * 默认退出码恒为 0，不产出阻塞退出码；
    * 不进默认构建，不挂任何 CI 必过项；
    * 不自动改任何文件；
    * 作用是帮人查——**判定由人做**。

计数只给分母。本项目已实测：以正则抓不到大小写混合的标识符，也抓不到不含该子串的行。
因此本工具的任何计数都不能当作通过依据；「没报」不等于「没问题」。
真实缺陷数量必然 ≥ 本工具报出的数量。

用法

    python3 eng/tools/doccheck/check_anchors.py                     # 人读报告
    python3 eng/tools/doccheck/check_anchors.py --json out.json     # 机器读
    python3 eng/tools/doccheck/check_anchors.py --only dangling_path
    python3 eng/tools/doccheck/check_anchors.py --limit 0            # 不限量
    python3 eng/tools/doccheck/check_anchors.py --selftest           # 自证

自证

    python3 eng/tools/doccheck/check_anchors.py --selftest
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

# --------------------------------------------------------------------------
# 扫描口径
#
# 沿用 docs/DOCUMENT_INDEX.yaml 自己声明的扫描口径（该文件「扫描口径（downstream）」一节：
# git 跟踪文件，排除 artifacts/、run/、build/、lib/third_party/、实验/、logs/）。
#
# 两处关键处理，都是本项目实测踩过的：
#
# 1) 解析用的「存在性全集」以**磁盘**为准，git 跟踪集只用来判「未入版本控制」。
#    原因：.gitignore 的 `build/` 规则会把 docs/engineering/build/ 整层排除出版本控制，
#    那 4 份一级正本磁盘上真实存在却 `git ls-files` 查不到。若拿 git 集当全集，
#    指向它们的每一条引用都会被误判成路径悬空。
#
# 2) 被排除目录一律不扫描（run/ 与 artifacts/ 下存着大量历史文档副本与证据快照），
#    但被排除目录**仍然可以被引用**，实验单元的 README 就是这一类。
# --------------------------------------------------------------------------

# 排除的是**顶层**过程/产物目录，不是任意同名段。
# ⚠ 踩过的坑：早先用「路径里任意一段命中排除名」，结果把
# docs/engineering/build/ 整层误杀——那里有 4 份登记为 ACTIVE_INORMATIVE 的一级正本。
EXCLUDE_TOP = (
    ".git", "artifacts", "run", "build", "out", "logs", "site", "node_modules",
)

# 这些名字在任何层级都排除（都是工具产物，不是人读件）
EXCLUDE_ANY = ("third_party", "__pycache__", ".pytest_cache", ".dsh-code-index")

# 默认扫描面：与 docs/DOCUMENT_INDEX.yaml 自己声明的扫描口径一致
# （该文件「扫描口径（downstream）」：git 跟踪文件，排除 artifacts/、run/、
#  build/、lib/third_party/、实验/、logs/）。
# 实验单元目录要扫请显式加 --with-experiments：实测该树下大面积是
# `file_audit_before.json` 这类**记录过去文件树**的证据快照，
# 它们记的就是已经不存在的路径，按引用扫会淹没信号。
SOURCE_SURFACES = ("docs", "eng", "lib", "testdata", "gaia")
EXTRA_SURFACES = ("实验",)

# 「§N → 章节名」批量替换留下的残渣指纹。
# 三轮记录点名的一类缺陷生成器：替换器把 § 换成「一节」，却没把编号换成
# 真实节名，于是产生语法上像节名、实际不存在的字符串。
#
# ⚠ 这几条必须写得很窄。早先用宽形态（任何空括注、任何「最高设计，」）
# 在全仓产出 15000+ 条假残渣——文档里 `main()`、`本文档，权威链` 遍地都是。
# 宁可漏掉，不可把真代码写法当缺陷。
RESIDUE_PATTERNS = (
    (re.compile(r"「」\s*一节"), "空节名锚「」：节名被批量替换吃光"),
    (re.compile(r"一节\s*[a-z](?![A-Za-z0-9])"), "「§a」被替换成「一节a」：编号未换成节名"),
    (re.compile(r"[本更新验]节\s*\.\d"), "「一节.N」：§N 被替换后编号残留"),
    (re.compile(r"[（(]\s*(?:最高设计|该文件|本文件|本文档)\s*[，、；：:]"),
     "空槽位：引用主体后直接跟标点，节名被吃光"),
    (re.compile(r"(?:最高设计|该文件|本文件|本文档)\s*[，、；：:]\s*$"),
     "空槽位：句末主体后无节名"),
)

TEXT_SUFFIXES = (".md", ".yaml", ".yml", ".txt")
CODE_SUFFIXES = (".json", ".py", ".c", ".h", ".cc", ".cpp", ".hpp", ".sh", ".cmake")

# 结构性引用字段名（机器源里指路的字段）
REF_KEYS = (
    "path", "doc", "doc_path", "document", "target", "target_path",
    "source", "source_path", "dest", "dest_path", "file", "file_path",
    "ref", "reference", "schema_path_reserved", "upstream", "authority",
    "owner", "separation", "design", "downstream",
)

# --------------------------------------------------------------------------
# 章节号 / 标题规范化
# --------------------------------------------------------------------------

SEC_RE = re.compile(r"^(\d+(?:\.\d+)*[a-z]?)\.?(?:\s|$)")
APX_RE = re.compile(r"^附\s*录\s*([A-Z])\b")
HEADING_RE = re.compile(r"^(#{1,6})\s+(.*?)\s*$")

ANCHOR_RE = re.compile(r"§\s*([0-9]+(?:\.[0-9]+)*[a-z]?)")
CNANCHOR_RE = re.compile(r"第\s*([0-9]+(?:\.[0-9]+)*)\s*[章节]")

# `X.md`「节名」一节 —— 本仓实际在用的节名引用形态（docs 下 537 处）
NAME_REF_RE = re.compile(r"「([^」]{2,40})」\s*一节")
# 「节名」一节（不带路径，多为自指或上文承接）
BARE_NAME_REF_RE = re.compile(r"「([^」]{2,40})」\s*一节")

# §N（标题）—— DOCUMENT_INDEX.yaml 的 upstream 字段就是这个形态
ANCHOR_WITH_TITLE_RE = re.compile(r"§\s*([0-9]+(?:\.[0-9]+)*[a-z]?)\s*[（(]([^）)]{1,60})[）)]")

# --------------------------------------------------------------------------
# 路径抽取
# --------------------------------------------------------------------------

TICK_RE = re.compile(r"`([^`\n]{1,200})`")
BARE_RE = re.compile(
    r"(?<![\w/.-])((?:docs|eng|lib|实验|testdata|gaia)/[A-Za-z0-9_./+\-]*[A-Za-z0-9_+\-])"
)

PATH_TAIL_STRIP = "，。；：、,;:）)】]\"'` 　\t"
PATH_HEAD_STRIP = "【[（(\"'` 　\t"

# 明显不是仓内路径的形态
NOT_A_PATH = ("http://", "https://", "ftp://", "mailto:", "#")

# 分册代称：把文件路径藏起来的名字层，本项目实际在用
# 分册代称：把文件路径藏起来的名字层，本项目实际在用。
# ⚠ 实测：本仓出现的分册代称引用**全部**指向已重写掉的旧章节号（DATA_SEMANTICS
# 由 3414 行/§1–§31 改写为 300 行/§1–§7，路径全改了、章节号一个没改）。
# 所以这层不能当负例样例，它正是「路径修了锚没修」的主力形态。
NAMED_VOLUMES = {
    "science 分册的数据语义卷": "docs/science/unified/DATA_SEMANTICS.md",
    "数据语义卷": "docs/science/unified/DATA_SEMANTICS.md",
    "science 分册": None,
    "engineering 分册": None,
    "detail 分册": None,
}

# 文档代称（不带路径的缩写名 + §N）。本仓实测 `SCI §N` / `DATA_SEMANTICS §N`
# 各有上百处，是仅次于路径本身的第二大形态。
DOC_ALIASES = {
    "SCI": "docs/science/unified/SCIENCE_SCOPE.md",
    "DATA_SEMANTICS": "docs/science/unified/DATA_SEMANTICS.md",
    "CALIBRATION": "docs/science/calibration/CALIBRATION.md",
    "NOISE_SNR": "docs/science/noise_snr/NOISE_SNR.md",
    "最高设计": "docs/ACSD_DESIGN.md",
}

# 合同 / 条款 ID 代称（`ALG-CAL-001 §10`）：仓内 ID→文档的映射未成表，
# 因此不做存在性判定，只列为待人判读，避免伪装成悬空。
CLAUSE_ID_RE = re.compile(r"\b([A-Z]{2,6}-[A-Z0-9]+(?:-[A-Z0-9]+)*)\s*§\s*([0-9]+(?:\.[0-9]+)*[a-z]?)")

# 按设计登记「缺席」的字段：这些字段记的**就是**已经退役的东西，
# 目标不存在是登记意图，不是缺陷。不设这张表会报出几百条假悬空。
RETIREMENT_FIELDS = (
    ".modules.acr.files",
    ".modules",
    ".file_migrations",
    ".retired_knobs",
    ".legacy_paths",
    ".superseded_sections",
    ".test_only_oracles",
    ".migrated_from",
    ".retired",
    ".removed",
)

EXTERNAL_STD_MARKERS = (
    "Gaia", "DR3", "IVOA", "FITS", "标准", "文献", "论文", "RFC",
    "ISO", "IEEE", "外部", "上游", "官方", "NASA", "ESA",
)

# glob / 占位形态：按 DOCUMENT_GOVERNANCE.md 的 C1「不判」，只计数不作存在性判定
GLOB_CHARS = "*?[]{}"


def repo_universe(root):
    """返回 (磁盘存在全集, git 跟踪集, 来源说明, 全部目录集)。

    ⚠ 关键：**解析全集不做排除**。排除目录不扫描（那是「引用发起面」的事），
    但被排除目录里的文件仍然要能被引用解析到。run/、artifacts/、build/ 下存着
    大量历史副本与产物，文档确实会指向它们；若把它们排除出解析全集，
    每一条指向 run/... 的引用都会被误判成路径悬空。
    """
    on_disk, dirs = [], set()
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root).replace(os.sep, "/")
        if rel_dir == ".":
            rel_dir = ""
        else:
            dirs.add(rel_dir)
        for fn in filenames:
            rel = (rel_dir + "/" + fn) if rel_dir else fn
            on_disk.append(rel)
        # 记录每一级目录，供「目录引用」判定用
        if rel_dir:
            parts = rel_dir.split("/")
            for i in range(1, len(parts) + 1):
                dirs.add("/".join(parts[:i]))
    tracked = set()
    note = "os.walk（磁盘全集）"
    try:
        out = subprocess.run(
            ["git", "-c", "core.quotepath=false", "ls-files", "-z"],
            cwd=root, capture_output=True, check=True, timeout=180,
        )
        names = [n for n in out.stdout.decode("utf-8", "replace").split("\0") if n]
        if names:
            tracked = set(names)
            note = "os.walk（磁盘全集）+ git ls-files（跟踪集）"
    except Exception:
        pass
    return on_disk, tracked, note, dirs


def is_excluded(rel):
    parts = rel.split("/")
    if parts[0] in EXCLUDE_TOP:
        return True
    return any(p in EXCLUDE_ANY for p in parts)


# 仓内一级目录。判断一个 token 是不是「仓内路径」，先看它首段是不是这些之一。
REPO_TOPS = {
    "docs", "eng", "lib", "实验", "testdata", "gaia", "run", "artifacts",
    "build", "out", "logs", ".github",
}

# 看起来像文件的扩展名（相对路径 / 裸文件名判定用）
FILE_EXTS = (
    ".md", ".markdown", ".yaml", ".yml", ".json", ".py", ".txt",
    ".h", ".hpp", ".hh", ".c", ".cc", ".cpp", ".cxx", ".sh", ".cmake",
)

# 一律不是路径的字符：单位式（ADU/sr、ADU^2/sr^2）、公式、占位符、glob
NOT_PATH_CHARS = set("<>(){}[]^~\\|*=²³σμλΣΔΩ±≥≤≠→←")

# 截断符：反引号里常把一整句包进来（`X.md，最高设计`），路径只到第一个截断符。
# # 之后是 JSON-Pointer 片段，本工具不解析（见报告的盲区一节）。
TRUNC_CHARS = "，,；;（(：:#"


def classify_token(tok):
    """把候选 token 分类。

    返回 (kind, token)：
        'abs'  仓根相对（首段是已知一级目录）—— 解析失败即路径悬空
        'rel'  相对路径或裸文件名—— 解析不了不等于悬空，降为待人判读
        'none' 不是路径（单位式、公式、占位符、glob、散文片段）

    这一层是误报的主要闸门。本项目实测：不做这层区分时，`ADU/sr`、
    `sr^2/ADU^2`、`F_ref/σ_F(x,y)`、`<output_dir>/logs`、`variance/ivar`
    全被当成路径，一份文档能凭空产出几十条「路径悬空」。
    """
    if not tok:
        return ("none", None)
    t = tok.strip()
    for cut in TRUNC_CHARS:
        i = t.find(cut)
        if i > 0:
            t = t[:i]
    t = t.strip().strip(PATH_HEAD_STRIP).strip(PATH_TAIL_STRIP).strip()
    if not t or t in NOT_A_PATH:
        return ("none", None)
    if t.endswith("/"):
        t = t[:-1]
    if not t or any(c in NOT_PATH_CHARS for c in t):
        return ("none", None)
    if any(c.isspace() for c in t):
        return ("none", None)
    if t.startswith(("~", "$")) or t.startswith("_"):
        return ("none", None)
    if t.startswith(("../", "./")):
        return ("rel", t)
    first = t.split("/")[0]
    if first in REPO_TOPS:
        return ("abs", t)
    if t.endswith(FILE_EXTS):
        return ("rel", t)          # 裸文件名，如 `ACSD_DESIGN.md`
    # 其余相对形态：末段必须像个文件名，否则是单位式 / 公式 / 散文片段
    if "/" in t and not t.startswith("/"):
        last = t.rsplit("/", 1)[-1]
        if re.search(r"\.[A-Za-z0-9]{1,6}$", last):
            return ("rel", t)
    return ("none", None)


# --------------------------------------------------------------------------
# 仓库索引
# --------------------------------------------------------------------------


class RepoIndex:
    def __init__(self, root, universe, tracked, dirs):
        self.root = root
        self.universe = set(universe)
        self.tracked = tracked
        self.dirs = dirs
        # 后缀索引分两桶：正本 vs 历史副本。
        # ⚠ 踩过的坑：run/ 与 artifacts/ 下存着大量历史文档副本（bisect 树、
        # 归档快照）。不区分的话，docs/science/DATA_SEMANTICS.md 这种
        # 后缀会同时命中正本和几十份历史副本，全被误判成「同名多义」。
        self.by_suffix = defaultdict(list)
        for rel in sorted(self.universe):
            canonical = rel.split("/")[0] not in EXCLUDE_TOP
            parts = rel.split("/")
            for i in range(len(parts)):
                self.by_suffix["/".join(parts[i:])].append((canonical, rel))
        self._cache = {}

    def _is_dir(self, rel):
        return rel in self.dirs or os.path.isdir(os.path.join(self.root, rel))

    def _suffix_candidates(self, token):
        """后缀候选。

        只认**正本**。run/ 与 artifacts/ 下的历史副本不算目标：若一个裸后缀
        只在历史副本里出现，正本就是没了 —— 那是路径悬空，不是「同名多义」。
        """
        cands = self.by_suffix.get(token)
        if not cands:
            return []
        return sorted({r for can, r in cands if can})

    def resolve(self, token, src_rel):
        """解析路径 token。

        kind = 'exact' | 'relative' | 'ext' | 'dir' | 'suffix' | 'ambiguous' | 'missing'
        """
        if token in self._cache:
            return self._cache[token]
        r = self._resolve(token, src_rel)
        self._cache[token] = r
        return r

    def _resolve(self, token, src_rel):
        # 相对路径优先：文档里裸写的 `engineering/governance/X.md` 是**文档相对**，
        # 不是仓根相对。反过来做会把一批真引用误判成悬空。
        if token.startswith(("./", "../")) or (
            token.split("/")[0] not in REPO_TOPS
        ):
            base = os.path.dirname(src_rel)
            cand = os.path.normpath(os.path.join(base, token)).replace(os.sep, "/")
            if cand in self.universe:
                return ("relative", cand, [cand])
            if self._is_dir(cand):
                return ("dir", cand, [cand])

        if token in self.universe:
            return ("exact", token, [token])

        if "/" in token:
            base = os.path.dirname(src_rel)
            cand = os.path.normpath(os.path.join(base, token)).replace(os.sep, "/")
            if cand in self.universe:
                return ("relative", cand, [cand])

        for ext in (".md", ".yaml", ".yml", ".json"):
            if token + ext in self.universe:
                return ("ext", token + ext, [token + ext])

        cands = self._suffix_candidates(token)
        if cands:
            if len(cands) == 1:
                return ("suffix", cands[0], cands)
            return ("ambiguous", None, cands)

        if self._is_dir(token):
            return ("dir", token, [token])

        return ("missing", None, [])


# --------------------------------------------------------------------------
# 标题索引：逐文件建立章节集合 + 全仓节名反查表
# --------------------------------------------------------------------------


class HeadingIndex:
    def __init__(self, root):
        self.root = root
        self._cache = {}
        self.name_index = None   # 惰性：节名 -> [文件]

    def get(self, rel):
        if rel in self._cache:
            return self._cache[rel]
        secs, titles, apx = set(), {}, set()
        full = os.path.join(self.root, rel)
        try:
            with open(full, encoding="utf-8", errors="replace") as fh:
                in_fence = False
                for line in fh:
                    if line.lstrip().startswith("```"):
                        in_fence = not in_fence
                        continue
                    if in_fence:
                        continue
                    m = HEADING_RE.match(line.rstrip("\n"))
                    if not m:
                        continue
                    text = m.group(2).strip()
                    if not text or text.startswith(">"):
                        continue
                    a = APX_RE.match(text)
                    if a:
                        apx.add(a.group(1))
                        titles.setdefault("附录" + a.group(1), []).append(text)
                        continue
                    s = SEC_RE.match(text)
                    if s:
                        num = s.group(1)
                        secs.add(num)
                        rest = text[len(s.group(0)) :].strip()
                        titles.setdefault(num, []).append(rest or text)
                        # 节名也建反查：编号节去掉编号后是真实节名
                        if rest:
                            titles.setdefault("T:" + rest, []).append(text)
                    else:
                        titles.setdefault("T:" + text, []).append(text)
                        titles.setdefault(text, []).append(text)
        except OSError:
            pass
        rec = (secs, titles, apx)
        self._cache[rel] = rec
        return rec

    def has_section(self, rel, num):
        return num in self.get(rel)[0]

    def has_appendix(self, rel, letter):
        return letter in self.get(rel)[2]

    def has_title(self, rel, name):
        t = self.get(rel)[1]
        return name in t or ("T:" + name) in t

    def title_of(self, rel, num):
        got = self.get(rel)[1].get(num)
        return got[0] if got else None

    def _build_name_index(self):
        if self.name_index is not None:
            return self.name_index
        idx = defaultdict(list)
        for rel in self._cache:
            for key in self._cache[rel][1]:
                idx[key].append(rel)
        self.name_index = idx
        return idx

    def files_with_title(self, name):
        """这个节名在全仓哪些文件里真实存在——用于「节名真 · 文件错」判定。"""
        self._build_name_index()
        hits = set(self.name_index.get(name, [])) | set(
            self.name_index.get("T:" + name, [])
        )
        return sorted(hits)

    def ensure_indexed(self, rels):
        for r in rels:
            self.get(r)
        self.name_index = None


# --------------------------------------------------------------------------
# 引用抽取
# --------------------------------------------------------------------------


def iter_candidates(text):
    """产出 (raw, start, len)。反引号优先，裸路径补充（代码注释无反引号）。"""
    out, seen = [], set()
    for m in TICK_RE.finditer(text):
        # 用组 1 的起点：m.start() 指向开引号，会把 token 位置整体前移 1，
        # 导致 anchors_after 切出的尾巴错位、章节锚永远匹配不上。
        if m.group(1) not in seen:
            seen.add(m.group(1))
            out.append((m.group(1), m.start(1), len(m.group(1))))
    for m in BARE_RE.finditer(text):
        tok = m.group(1)
        if tok not in seen:
            seen.add(tok)
            out.append((tok, m.start(1), len(tok)))
    return out


def anchors_after(text, start, length):
    """取紧跟该路径之后的章节锚集合。

    覆盖三种形态：
        `X.md` §2.2            单锚
        `X.md` §4.2（标题）、§12.1（标题）   多锚
        `X.md` 附录 A（术语）
    返回 [(raw, number_or_None, title_or_None), ...]
    """
    tail = text[start + length : start + length + 160]
    # token 后面常跟着收尾定界符（反引号、右括号、引号），先跳过再判锚
    tail = tail.lstrip("`\"'）)」』]】 \t　")
    res = []
    pos = 0
    while True:
        rest = tail[pos:]
        m = re.match(r"§\s*([0-9]+(?:\.[0-9]+)*[a-z]?)(?:\s*[（(]([^）)]{1,60})[）)])?", rest)
        if m:
            res.append(("§" + m.group(1), m.group(1), m.group(2)))
            pos += m.end()
            continue
        m = re.match(r"附\s*录\s*([A-Z])(?:\s*[（(]([^）)]{1,60})[）)])?", rest)
        if m:
            res.append(("附录" + m.group(1), None, m.group(2)))
            res[-1] = ("附录" + m.group(1), "APX:" + m.group(1), m.group(2))
            pos += m.end()
            continue
        m = re.match(r"[、,，;；/]?\s*(?=[、,，;；/]|$)", rest)
        if m and m.end() > 0 and rest[: m.end()].strip("、,，;；/ \t"):
            pos += m.end()
            continue
        break
    return res


def name_refs_in(line):
    """取 `X.md`「节名」一节 这类节名引用。返回 [(name, start, end)]。"""
    return [(m.group(1), m.start(), m.end()) for m in NAME_REF_RE.finditer(line)]


# --------------------------------------------------------------------------
# 发现
# --------------------------------------------------------------------------

CLASSES = {
    "dangling_path": "路径悬空：被引用的文件/目录不存在",
    "dangling_anchor": "锚悬空：文件存在，但被引用的章节不在该文件里",
    "section_wrong_file": "节名真 · 文件错：节名真实存在，但不在被指的那份文件里",
    "anchor_title_mismatch": "锚题不符：节号命中但括注标题与该节标题不一致",
    "quote_not_found": "引文锚失配：文件存在，但登记的引文在目标里查不到",
    "ambiguous_target": "同名多义：路径后缀或节名匹配到多个真实目标，工具不替人选",
    "untracked_target": "目标存在但未入版本控制：磁盘有、git 无（不是悬空，但会让门禁在干净克隆上失真）",
    "unresolved_relative": "相对/裸名路径解析不到：可能相对于正文另行声明的目录，不判悬空",
    "replacement_residue": "替换残渣：「§N→节名」批量替换留下的语法像节名、实际不存在的串",
    "unresolved_bare_anchor": "裸章节锚：§N 未带路径，自指/外部/代称三者不可区分",
    "clause_id_anchor": "条款 ID 锚：`ALG-CAL-001 §10`。ID→文档映射未成表，不判存在性",
    "external_ref": "外部引用：指向仓外标准/文献，仓内不可判定（仅列出，不判缺陷）",
}

SOFT = ("unresolved_bare_anchor", "external_ref", "unresolved_relative",
        "clause_id_anchor", "untracked_target")
ADVISORY = ("ambiguous_target", "anchor_title_mismatch", "replacement_residue")


class Finding:
    __slots__ = ("cls", "src", "line", "raw", "token", "target", "detail", "cands")

    def __init__(self, cls, src, line, raw, token, target, detail, cands=None):
        self.cls, self.src, self.line, self.raw = cls, src, line, raw
        self.token, self.target, self.detail = token, target, detail
        self.cands = cands or []

    def key(self):
        return (self.cls, self.src, self.line, self.token, self.target or "")

    def as_dict(self):
        return {
            "class": self.cls,
            "meaning": CLASSES.get(self.cls, ""),
            "source_file": self.src,
            "line": self.line,
            "raw_reference": self.raw,
            "token": self.token,
            "resolved_target": self.target,
            "detail": self.detail,
            "candidates": self.cands,
        }


def looks_external(before):
    win = before[-40:]
    return any(mk in win for mk in EXTERNAL_STD_MARKERS)


def normalize_title(s):
    if s is None:
        return None
    s = s.strip().strip("「」『』\"' 　")
    s = re.sub(r"\s+", "", s)
    return s


def looks_like_title(s):
    """括注到底像不像「节标题」。

    散文里 `§1（该节真实存在）` 的括注是注释，不是标题断言；对它做逐字比对
    只会产出误报。因此先做形态闸门：过长、含句读、像整句话的一律不判。
    """
    if not s:
        return False
    t = s.strip()
    if len(t) < 2 or len(t) > 30:
        return False
    if any(c in t for c in "。，；：！？、,;:?!"):
        return False
    return True


def title_compatible(a, b):
    """括注标题与真实节标题是否相容：完全相同，或一方是另一方的前缀/包含。"""
    na, nb = normalize_title(a), normalize_title(b)
    if not na or not nb:
        return True
    if na == nb:
        return True
    return na in nb or nb in na


# --------------------------------------------------------------------------
# 主扫描
# --------------------------------------------------------------------------


def scan(root, surfaces):
    universe, tracked, note, dirs = repo_universe(root)
    index = RepoIndex(root, universe, tracked, dirs)
    heads = HeadingIndex(root)

    # 先把可解析的 markdown 文件全部建索引，供「节名真 · 文件错」反查
    md_files = [r for r in universe if r.lower().endswith((".md", ".markdown"))]
    heads.ensure_indexed(md_files)

    findings, seen = [], set()
    stats = defaultdict(int)

    def add(f):
        k = f.key()
        if k in seen:
            return
        seen.add(k)
        findings.append(f)

    scanned = 0
    self_rel = os.path.relpath(os.path.abspath(__file__), root).replace(os.sep, "/")
    for rel in sorted(universe):
        if is_excluded(rel):
            continue
        if rel == self_rel:
            stats["self_skipped"] += 1
            continue      # 工具不扫自己：自证夹具里的假路径会把自己读成悬空
        if surfaces:
            top = rel.split("/")[0]
            if top not in surfaces:
                # 仓库根文件：只收 .md。CMakeLists.txt / CMakePresets.json 里的
                # 路径串是构建规则不是文档指针，混进来会淹没真缺陷。
                if "/" in rel or not rel.lower().endswith((".md", ".markdown")):
                    continue
        suffix = os.path.splitext(rel)[1].lower()
        if suffix not in TEXT_SUFFIXES and suffix not in CODE_SUFFIXES:
            continue
        try:
            with open(os.path.join(root, rel), encoding="utf-8", errors="replace") as fh:
                text = fh.read()
        except OSError:
            continue
        scanned += 1
        stats["files_scanned"] += 1
        lines = text.splitlines()
        is_md = suffix in (".md", ".markdown")

        # ================= 逐行：路径引用 =================
        for lineno, line in enumerate(lines, 1):
            if line.lstrip().startswith("```"):
                continue

            # 替换残渣指纹：「§N→节名」批量替换的产物
            for rx, why in RESIDUE_PATTERNS:
                rm = rx.search(line)
                if rm:
                    add(Finding("replacement_residue", rel, lineno,
                                line.strip()[:200], rm.group(0), None, why))

            # 分册代称：science 分册的数据语义卷 §13.4
            for vol, tgt in NAMED_VOLUMES.items():
                p = line.find(vol)
                if p < 0:
                    continue
                if tgt is None:
                    add(Finding("unresolved_bare_anchor", rel, lineno,
                                line.strip()[:200], vol, None,
                                "分册代称「%s」指向上层目录多卷，工具不猜具体文件" % vol))
                    continue
                am = ANCHOR_RE.search(line[p + len(vol):])
                if am:
                    num = am.group(1)
                    if not heads.has_section(tgt, num):
                        where = heads.files_with_title(heads.title_of(tgt, "") or "") if False else []
                        add(Finding("dangling_anchor", rel, lineno,
                                    line.strip()[:200], vol, tgt,
                                    "分册代称→%s，该文件无章节 §%s" % (tgt, num)))
                    else:
                        stats["resolved_named_volume"] += 1

            # 路径 token
            for raw, start, length in iter_candidates(line):
                ckind, token = classify_token(raw)
                if ckind == "none":
                    continue

                kind, resolved, cands = index.resolve(token, rel)

                if kind == "missing":
                    if ckind == "rel":
                        # 相对路径 / 裸文件名解析不到 ≠ 悬空：它可能相对于
                        # 正文里另行声明的目录。降为待人判读，不计入悬空。
                        add(Finding("unresolved_relative", rel, lineno,
                                    raw[:200], token, None,
                                    "相对/裸名路径解析不到：可能相对于正文另行声明的目录"))
                        continue
                    add(Finding("dangling_path", rel, lineno,
                                (raw + " " + line[start + length: start + length + 24]).strip()[:200],
                                token, None, "路径不存在：%s" % token))
                    continue

                if kind == "ambiguous":
                    add(Finding("ambiguous_target", rel, lineno, raw, token, None,
                                "路径后缀匹配到 %d 个真实文件" % len(cands), cands))
                    continue

                # 目标存在但未入版本控制
                if resolved not in tracked and not resolved.endswith("/"):
                    stats["untracked_targets_seen"] += 1

                # 路径通了，再判锚
                for araw, num, atitle in anchors_after(line, start, length):
                    if not num:
                        continue
                    if resolved and os.path.isdir(os.path.join(root, resolved)):
                        stats["anchor_on_dir"] += 1
                        continue
                    if not resolved.lower().endswith((".md", ".markdown")):
                        stats["anchor_on_code"] += 1
                        continue
                    if num.startswith("APX:"):
                        if heads.has_appendix(resolved, num[4:]):
                            stats["resolved_anchor"] += 1
                        else:
                            add(Finding("dangling_anchor", rel, lineno,
                                        "%s %s" % (raw, araw), token, resolved,
                                        "文件存在，但无「附录 %s」" % num[4:]))
                        continue
                    if heads.has_section(resolved, num):
                        stats["resolved_anchor"] += 1
                        real = heads.title_of(resolved, num)
                        # 标题逐字比对只在「条款号 + 条款标题」是合同义务的地方生效：
                        # 索引 upstream 字段与文档抬头「上游：」区（DOCUMENT_GOVERNANCE C4）。
                        # 散文里的括注是注释，判它只会误报。
                        contractual = suffix in (".yaml", ".yml") or line.lstrip().lstrip(
                            ">").lstrip().startswith("上游")
                        if (contractual and looks_like_title(atitle)
                                and not title_compatible(atitle, real)):
                            add(Finding("anchor_title_mismatch", rel, lineno,
                                        "%s %s（%s）" % (raw, araw, atitle),
                                        token, resolved,
                                        "节号 §%s 存在，但括注标题与真实标题不一致：括注「%s」/ 真实「%s」"
                                        % (num, atitle, real)))
                    else:
                        add(Finding("dangling_anchor", rel, lineno,
                                    "%s %s" % (raw, araw), token, resolved,
                                    "文件存在，但无章节 §%s%s" % (num, _near(resolved, num, heads))))

            # 文档代称 + 章节锚：`SCI §3a`、`DATA_SEMANTICS §4a`
            for alias, tgt in DOC_ALIASES.items():
                pat = r"(?<![\w/])" + re.escape(alias) + r"\s*§\s*([0-9]+(?:\.[0-9]+)*[a-z]?)"
                for am in re.finditer(pat, line):
                    num = am.group(1)
                    if heads.has_section(tgt, num):
                        stats["resolved_alias_anchor"] += 1
                    else:
                        add(Finding("dangling_anchor", rel, lineno,
                                    line[am.start(): am.end()][:200], alias, tgt,
                                    "代称「%s」→ %s，该文件无章节 §%s%s"
                                    % (alias, tgt, num, _near(tgt, num, heads))))

            # 条款 ID 锚：`ALG-CAL-001 §10`。ID→文档映射未成表，不判存在性。
            for cm in CLAUSE_ID_RE.finditer(line):
                stats["clause_id_anchor"] += 1
                add(Finding("clause_id_anchor", rel, lineno,
                            cm.group(0)[:120], cm.group(1), None,
                            "合同/条款 ID 锚：ID→文档映射未成表，工具不判存在性"))

            # `X.md`「节名」一节
            if is_md:
                for name, ns, ne in name_refs_in(line):
                    if looks_external(line[:ns]):
                        stats["external_name_ref"] += 1
                        continue
                    # 该节名引用前文最近的一条路径 token
                    before = line[:ns]
                    owner = None
                    for raw, start, length in iter_candidates(before):
                        _ck, t = classify_token(raw)
                        if t:
                            k, resolved, cands = index.resolve(t, rel)
                            if resolved and resolved.lower().endswith((".md", ".markdown")):
                                owner = (t, resolved)
                    if owner is None:
                        add(Finding("unresolved_bare_anchor", rel, lineno,
                                    line.strip()[:200], "「%s」一节" % name, None,
                                    "节名引用未带可解析路径，自指或上文承接"))
                        continue
                    tok, tgt = owner
                    if heads.has_title(tgt, name):
                        stats["resolved_name_ref"] += 1
                        continue
                    # 关键分支：节名真·文件错。按「节名不存在」的常规改法去修，
                    # 会把真节名换成假节名——越修越坏。
                    where = [f for f in heads.files_with_title(name) if f != tgt]
                    if len(where) == 1:
                        add(Finding("section_wrong_file", rel, lineno,
                                    line.strip()[:200], "「%s」一节" % name, tgt,
                                    "节名「%s」在被指文件 %s 中不存在，但在 %s 中真实存在"
                                    % (name, tgt, where[0]), where))
                    elif len(where) > 1:
                        add(Finding("ambiguous_target", rel, lineno,
                                    line.strip()[:200], "「%s」一节" % name, tgt,
                                    "节名「%s」在 %d 个真实文件里都是标题：不唯一，工具不替人选"
                                    % (name, len(where)), where))
                    else:
                        add(Finding("dangling_anchor", rel, lineno,
                                    line.strip()[:200], "「%s」一节" % name, tgt,
                                    "文件存在，且全仓未找到同名节"))

        # ================= 结构化引用（JSON / YAML） =================
        # JSON 同时被上面的逐行扫描覆盖过；这里只补「字段名语境」，
        # 同一文件同一 token 已报过就跳过，避免一处缺陷报两遍。
        emitted = {(f.cls, f.token) for f in findings if f.src == rel}
        structured = []
        if suffix == ".json":
            structured = _json_refs(text)
        elif suffix in (".yaml", ".yml"):
            structured = _yaml_refs(lines)
        for val, key, jpath in structured:
            if any(jpath.startswith(rf) or (rf + ".") in jpath for rf in RETIREMENT_FIELDS):
                stats["retirement_fields_skipped"] += 1
                continue
            for piece in re.split(r"[、,，;；\s]+", val):
                _ck, token = classify_token(piece)
                if token is None:
                    continue
                kind, resolved, cands = index.resolve(token, rel)
                if kind == "missing":
                    cls = "dangling_path" if _ck == "abs" else "unresolved_relative"
                elif kind == "ambiguous":
                    cls = "ambiguous_target"
                else:
                    stats["resolved_structured"] += 1
                    if resolved not in tracked:
                        stats["untracked_targets_seen"] += 1
                    continue
                if (cls, token) in emitted:
                    continue
                lno = _line_of(text, piece)
                add(Finding(cls, rel, lno,
                            "%s: %s" % (key, piece)[:200], token, None,
                            "结构化字段 %s 指向%s" % (key, "不存在的路径" if cls == "dangling_path"
                                                     else "%d 个候选" % len(cands)),
                            cands))
                emitted.add((cls, token))

        # ================= 内容锚：引文是否还在目标里 =================
        # 本仓最高精度的一面：defaults.json 的 source_ref 逐条登记
        # {id, path, quote, sha256, value_text}，引文是目标文件的逐字连续子串。
        # 引文查不到 = 锚悬空（文件在、内容不在），零误报、可逐条复跑。
        if suffix == ".json":
            for rec in _quote_anchors(text):
                p, quote = rec["path"], rec["quote"]
                if not p or not quote:
                    stats["quote_anchor_incomplete"] += 1
                    continue
                k, resolved, cands = index.resolve(p, rel)
                if k == "missing":
                    continue      # 路径缺失已由 dangling_path 报过
                if k == "ambiguous":
                    continue
                try:
                    with open(os.path.join(root, resolved), encoding="utf-8") as fh:
                        body = fh.read()
                except OSError:
                    continue
                if quote in body:
                    stats["quote_anchor_ok"] += 1
                else:
                    stats["quote_anchor_broken"] += 1
                    add(Finding("quote_not_found", rel, _line_of(text, quote[:40]),
                                quote[:120], p, resolved,
                                "文件存在，但登记引文在目标内查不到（%d 字）" % len(quote)))

        # ================= 裸章节锚分类 =================
        if suffix in TEXT_SUFFIXES:
            for lineno, line in enumerate(lines, 1):
                if line.lstrip().startswith("```"):
                    continue
                for m in ANCHOR_RE.finditer(line):
                    before = line[: m.start()]
                    tail = before.rstrip()
                    if re.search(r"[`）)】\w][\w./+-]{1,}$", tail[-45:]) and (
                            "`" in tail[-45:] or "/" in tail[-45:] or ")" in tail[-10:]):
                        continue    # 已由「路径 + §锚」那一路判定过
                    if looks_external(before):
                        stats["external_anchor"] += 1
                        continue
                    add(Finding("unresolved_bare_anchor", rel, lineno,
                                line.strip()[:200], "§" + m.group(1), None,
                                "裸章节锚未带路径：自指/仓外标准/分册代称三者不可区分"))

    # 未入版本控制的目标：按目标聚合，单独一类
    _collect_untracked(findings, add, universe, tracked, root)

    return findings, stats, note, index, heads, scanned


def _line_of(text, piece):
    for i, l in enumerate(text.splitlines(), 1):
        if piece in l:
            return i
    return 0


def _near(rel, num, heads):
    secs = heads.get(rel)[0]
    if not secs:
        return ""
    base = num.rstrip("abcdefghijklmnopqrstuvwxyz")
    near = sorted(s for s in secs if s.startswith(base) or base.startswith(s))
    return ("；该文件相近章节：" + "、".join(near[:6])) if near else ""


def _json_refs(text):
    try:
        data = json.loads(text)
    except Exception:
        return []
    out = []
    stack = [(data, "")]
    while stack:
        node, jpath = stack.pop()
        if isinstance(node, dict):
            for k, v in node.items():
                if k in REF_KEYS and isinstance(v, str):
                    out.append((v, k, jpath + "." + k))
                stack.append((v, jpath + "." + str(k)))
        elif isinstance(node, list):
            for i, v in enumerate(node):
                stack.append((v, jpath + "[%d]" % i))
    return out


def _quote_anchors(text):
    """取 {path, quote} 成对出现的内容锚登记项。"""
    try:
        data = json.loads(text)
    except Exception:
        return []
    out = []

    def walk(node):
        if isinstance(node, dict):
            if isinstance(node.get("path"), str) and isinstance(node.get("quote"), str):
                out.append({"path": node["path"], "quote": node["quote"]})
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)

    walk(data)
    return out


def _yaml_refs(lines):
    out = []
    for i, line in enumerate(lines, 1):
        s = line.strip()
        if s.startswith("#"):
            continue
        m = re.match(r"^-\s*([a-z_]+)\s*:\s*(.+)$", s) or re.match(r"^([a-z_]+)\s*:\s*(.+)$", s)
        if not m:
            continue
        key, val = m.group(1), m.group(2).strip().strip('"').strip("'")
        if key in REF_KEYS and val:
            out.append((val, key, "." + key))
    return out


def _collect_untracked(findings, add, universe, tracked, root):
    """磁盘上存在、但 git 未跟踪的文件：不是悬空路径，但要让登记面与门禁看见它。

    git 不可用时一律不报——不知道跟踪状态就说「未入版本控制」是编造。
    """
    if not tracked:
        return
    untracked = sorted(r for r in universe if r not in tracked and not is_excluded(r))
    for rel in untracked:
        if rel.lower().endswith((".md", ".markdown", ".yaml", ".yml")):
            add(Finding("untracked_target", rel, 0, rel, rel, rel,
                        "磁盘存在但未入版本控制：任何「索引↔磁盘」类门禁在干净克隆上会漏掉它"))


# --------------------------------------------------------------------------
# 自证
# --------------------------------------------------------------------------

SELFTEST_TREE = {
    "docs/t_doc.md": (
        "# 顶层文档\n\n"
        "## 1 存在的一节\n\n正文。\n\n"
        "## 2.1 存在的一小节\n\n正文。\n\n"
        "## 4b 字母后缀一节\n\n正文。\n\n"
        "## 附录 A. 术语\n\n正文。\n\n"
        "【负例·应判通过】有效引用：`docs/t_other.md` §1（该节真实存在）；\n"
        "自指：`docs/t_doc.md` §2.1；无扩展名：`docs/t_other`；目录：`docs/t_sub/`；\n"
        "字母后缀锚：`docs/t_doc.md` §4b。\n\n"
        "【正例·路径悬空】指向不存在的文件：`docs/t_gone.md`。\n\n"
        "【正例·锚悬空】文件在但节不在：`docs/t_other.md` §7。\n\n"
        "【正例·节名真·文件错】`docs/t_wrong.md`「甲节名」一节 —— 该节名真实存在，但在别的文件。\n\n"
        "【正例·同名多义】两个真实文件同名：`x.md`。\n\n"
        "【负例·节名引用有效】`docs/t_other.md`「甲节」一节。\n\n"
        "【外部引用·不判缺陷】Gaia DR3 文档 §5.4.1 式 5.41。\n"
    ),
    "docs/t_other.md": "# 别的文档\n\n## 1 甲节\n\n正文。\n\n## 3 丙节\n\n正文。\n",
    "docs/t_right.md": "# 正确承载者\n\n## 7 甲节名\n\n正文。\n",
    "docs/t_wrong.md": "# 错误承载者\n\n## 2 别的节\n\n正文。\n",
    "docs/t_sub/README.md": "# 子目录招牌件\n",
    "docs/t_ambig_a/x.md": "# 甲文件同名\n\n## 1 节甲\n",
    "docs/t_ambig_b/x.md": "# 乙文件同名\n\n## 9 节乙\n",
    "eng/contracts/t.json": (
        "{\n"
        '  "target": "docs/t_other.md",\n'
        '  "source": "docs/t_gone.md",\n'
        '  "path": "docs/t_other.md"\n'
        "}\n"
    ),
}

SELFTEST_EXPECT = {
    "dangling_path": [("docs/t_gone.md", "docs/t_doc.md"),
                      ("docs/t_gone.md", "eng/contracts/t.json")],
    "dangling_anchor": [("docs/t_other.md", "docs/t_doc.md")],
    "ambiguous_target": [("x.md", "docs/t_doc.md")],
    "section_wrong_file": [("「甲节名」一节", "docs/t_doc.md")],
}

SELFTEST_CLEAN_MARKERS = ["【负例·应判通过】", "自指：", "字母后缀锚：", "【负例·节名引用有效】"]
SELFTEST_CLEAN_FILES = ["docs/t_other.md", "docs/t_sub/README.md",
                        "docs/t_ambig_a/x.md", "docs/t_ambig_b/x.md",
                        "docs/t_right.md"]


def selftest():
    import shutil
    import tempfile

    base = tempfile.mkdtemp(prefix="acsd_anchor_selftest_")
    try:
        for rel, content in SELFTEST_TREE.items():
            p = os.path.join(base, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as fh:
                fh.write(content)

        findings, stats, _n, _i, _h, _s = scan(base, ())
        pairs = {(f.cls, f.token, f.src) for f in findings}
        ok, bad = 0, []

        for cls, wants in SELFTEST_EXPECT.items():
            for tok, src in wants:
                if (cls, tok, src) in pairs:
                    ok += 1
                else:
                    bad.append("漏报：期望 %s（token=%s）在 %s" % (cls, tok, src))

        doc_lines = SELFTEST_TREE["docs/t_doc.md"].splitlines()
        clean = {i for i, l in enumerate(doc_lines, 1)
                 for mk in SELFTEST_CLEAN_MARKERS if mk in l}
        hit = [(f.cls, f.line) for f in findings
               if f.src == "docs/t_doc.md" and f.line in clean
               and f.cls not in SOFT]
        if hit:
            bad.append("误报：负例行 %s 上出现判定 %s" % (sorted(clean), hit))
        else:
            ok += 1

        fhit = [(f.cls, f.line) for f in findings
                if f.src in SELFTEST_CLEAN_FILES and f.cls not in SOFT]
        if fhit:
            bad.append("误报：干净文件上出现发现 %s" % fhit)
        else:
            ok += 1

        bad_tokens = {"§5.4.1"}
        wrong = [(f.cls, f.token) for f in findings
                 if f.cls in ("dangling_path", "dangling_anchor") and f.token in bad_tokens]
        if wrong:
            bad.append("误报：外部标准引用被判成仓内悬空 %s" % wrong)
        else:
            ok += 1

        return ok, bad, findings, stats
    finally:
        shutil.rmtree(base, ignore_errors=True)


# --------------------------------------------------------------------------
# 报告
# --------------------------------------------------------------------------


ORDER = ("dangling_path", "dangling_anchor", "section_wrong_file",
         "anchor_title_mismatch", "ambiguous_target", "untracked_target")


def render_text(findings, stats, note, scanned, limit):
    out = []
    A = out.append
    by = defaultdict(list)
    for f in findings:
        by[f.cls].append(f)

    A("=" * 78)
    A("锚点与指针校验器（check_anchors）")
    A("=" * 78)
    A("")
    A("本工具是查问题的工具，不是判据。默认退出码恒为 0，不阻塞任何构建。")
    A("下面的数字是分母参考，不是通过/不通过的依据；判定由人做。")
    A("")
    A("【扫描口径】")
    A("  文件清单     : %s" % note)
    A("  实际扫描文件 : %d" % scanned)
    A("  扫描面       : %s + 仓库根文件" % ", ".join(SOURCE_SURFACES))
    A("  排除目录     : %s" % ", ".join(EXCLUDE_TOP))
    A("  说明         : 排除目录不扫描，但仍然可以被引用")
    A("                 存在性以【磁盘】为准；git 未跟踪的目标单列 untracked_target，")
    A("                 因为 .gitignore 的 build/ 会让 docs/engineering/build/ 整层脱离")
    A("                 版本控制——拿 git 集当全集会把真引用误判成悬空。")
    A("")
    A("【硬限制：计数只给分母，判定必须来自阅读】")
    A("  本项目已实测：以正则抓不到大小写混合的标识符，也抓不到不含该子串的行。")
    A("  因此本工具报出的条数是【真实缺陷数的下界】，不是上界，也不是全集。")
    A("  「没报出来」不等于「没有问题」。")
    A("")
    A("【已核实可解析的量（分母）】")
    for k in sorted(stats):
        if k != "files_scanned":
            A("  %-26s %d" % (k, stats[k]))
    A("")
    A("【发现汇总】")
    for cls in ORDER:
        A("  %-24s %6d    %s" % (cls, len(by.get(cls, [])), CLASSES.get(cls, "")))
    for cls in SOFT:
        A("  %-24s %6d    %s（不判缺陷）" % (cls, len(by.get(cls, [])), CLASSES[cls]))
    A("")
    A("  硬判定合计（路径悬空 + 锚悬空 + 节名真·文件错）= %d"
      % (len(by.get("dangling_path", [])) + len(by.get("dangling_anchor", []))
         + len(by.get("section_wrong_file", []))))
    A("")
    for cls in ORDER:
        items = by.get(cls, [])
        A("-" * 78)
        A("【%s】%d 条 —— %s" % (cls, len(items), CLASSES.get(cls, "")))
        A("-" * 78)
        if not items:
            A("  （本轮无）")
            A("")
            continue
        for f in (items if limit <= 0 else items[:limit]):
            A("  %s:%d" % (f.src, f.line))
            A("      引用原文 : %s" % f.raw)
            A("      token    : %s" % f.token)
            A("      判定     : %s" % f.detail)
            if f.cands:
                A("      候选     : %s" % ", ".join(f.cands[:8]))
                if len(f.cands) > 8:
                    A("                 …共 %d 个" % len(f.cands))
            A("")
        if limit > 0 and len(items) > limit:
            A("  …另有 %d 条未显示（--limit 控制）" % (len(items) - limit))
            A("")
    for cls in SOFT:
        items = by.get(cls, [])
        A("-" * 78)
        A("【%s】%d 条 —— %s" % (cls, len(items), CLASSES[cls]))
        A("-" * 78)
        for f in (items[:10] if limit > 0 else items):
            A("  %s:%d  %s" % (f.src, f.line, f.raw[:90]))
        if limit > 0 and len(items) > 10:
            A("  …另有 %d 条未显示" % (len(items) - 10))
        A("")
    return "\n".join(out)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="锚点与指针校验器：抓指向不存在的文档/章节/文件的引用。"
                    "查问题用，不是门禁；默认退出码恒为 0。")
    ap.add_argument("--root", default=".", help="仓库根（默认当前目录）")
    ap.add_argument("--json", metavar="PATH", help="把结构化结果写到该文件")
    ap.add_argument("--only", action="append", help="只看某一类（可重复）：%s" % ", ".join(CLASSES))
    ap.add_argument("--limit", type=int, default=40, help="每类最多显示几条（0=不限）")
    ap.add_argument("--quiet", action="store_true", help="只输出汇总")
    ap.add_argument("--no-external", action="store_true", help="不列外部引用")
    ap.add_argument("--with-experiments", action="store_true",
                    help="额外扫 实验/（默认不扫：与 DOCUMENT_INDEX 自订扫描口径一致，"
                         "且该树大半是记录过去文件树的证据快照）")
    ap.add_argument("--fail-on-findings", action="store_true",
                    help="可选：发现问题时返回 1（默认不启用，本工具不是门禁）")
    ap.add_argument("--selftest", action="store_true", help="跑内置正负例自证")
    args = ap.parse_args(argv)

    if args.selftest:
        ok, bad, findings, stats = selftest()
        print("自证：%d 项通过，%d 项失败" % (ok, len(bad)))
        for b in bad:
            print("  " + b)
        print("自证树内发现：" + ", ".join(
            "%s=%d" % (c, sum(1 for f in findings if f.cls == c))
            for c in sorted(set(f.cls for f in findings))))
        print("说明：自证只验工具在这组合成正负例上的行为，不代表全仓准确率。")
        return 0 if not bad else 1

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        print("仓库根不存在：%s" % root, file=sys.stderr)
        return 0

    surfaces = SOURCE_SURFACES + (EXTRA_SURFACES if args.with_experiments else ())
    findings, stats, note, _idx, _heads, scanned = scan(root, surfaces)
    if args.no_external:
        findings = [f for f in findings if f.cls != "external_ref"]
    if args.only:
        want = set(args.only)
        findings = [f for f in findings if f.cls in want]

    by = defaultdict(int)
    for f in findings:
        by[f.cls] += 1

    if args.quiet:
        print("扫描 %d 文件" % scanned)
        for c in sorted(by):
            print("  %-24s %d" % (c, by[c]))
    else:
        print(render_text(findings, stats, note, scanned, args.limit))

    print("\n提示：本工具不判对错。路径悬空与锚悬空必须分开看——")
    print("      「路径修了锚没修」只会落在锚悬空一类里；")
    print("      「节名真·文件错」按常规改法修会把真节名换成假节名。")

    if args.json:
        payload = {
            "tool": "check_anchors",
            "role": "查问题用；不是判据；默认退出码 0",
            "root": root,
            "scan_source": note,
            "files_scanned": scanned,
            "excluded_dirs": list(EXCLUDE_TOP),
            "scan_surfaces": list(SOURCE_SURFACES),
            "limits": [
                "计数只给分母，判定必须来自阅读",
                "以正则抓不到大小写混合标识符，也抓不到不含该子串的行",
                "报出条数是真实缺陷数的下界，不是全集",
                "裸 §N（无路径）三类不可区分，只列为待人判读",
            ],
            "summary": dict(by),
            "findings": [f.as_dict() for f in findings],
        }
        with open(args.json, "w", encoding="utf-8") as fh:
            json.dump(payload, fh, ensure_ascii=False, indent=2)
        print("结构化结果已写：%s" % args.json)

    if args.fail_on_findings and findings:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())