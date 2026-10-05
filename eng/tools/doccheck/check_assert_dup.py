#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""断言副本检测器（ACSD GOVERN-08 / T08）。

把「一处改动 → 全仓同步」从靠人记得变成机器可查：输入一条断言的特征
（公式片段、常数、判据句、标识符），输出它在全仓哪些文件出现、每处
所处文档层次、逐字是否一致，以及需要人裁决的分歧点。

**本工具是检查工具，不是判据。**
- 不产出阻塞退出码（除用法错误外恒为 0）；
- 不进默认构建，不被任何门禁调用；
- 不自动改任何文件（无 --fix，无写路径）；
- 计数只作分母，判定由人阅读上下文做出。

它抓不到的（本仓已实测踩过）：
- 大小写混合的类型名——`--text` 必然漏，须用 `--ident`；即便如此仍会漏
  （连字、希腊字母转写、Unicode 兼容字形）；
- 不含该子串的行——断言被改写过（换符号、换单位、换记法、拆成两句、
  只留结论不留公式）时逐字匹配完全看不见；
- 「同名不同义」的本质——工具只把分歧的上下文摆出来，不判是不是同义。
  本项目已多次因此误伤（把「登记册滞后」当成「重复副本」改回去、
  把有意的负例示范当成旧值残留）。
- 恒真门/恒红门（判据与被检量同源）——需要符号求值与别名追踪，不在本工具能力内。

用法（均可复跑）：
    python3 eng/tools/doccheck/check_assert_dup.py --text "w = SNR²/F_ref²"
    python3 eng/tools/doccheck/check_assert_dup.py --num 1.482602218505602
    python3 eng/tools/doccheck/check_assert_dup.py --ident frame_snr
    python3 eng/tools/doccheck/check_assert_dup.py --ident bilinear --prefix
    python3 eng/tools/doccheck/check_assert_dup.py --selftest
    python3 eng/tools/doccheck/check_assert_dup.py --text "…" --format json

退出码：0 = 正常（含「发现了分歧」）；2 = 用法错误或查询值非法；
1 = 仅内部崩溃。发现分歧不是错误。
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
import time
import unicodedata

# ---------------------------------------------------------------------------
# 文档层次：正本讲原则、细节讲实现、登记册讲索引
# ---------------------------------------------------------------------------
LAYERS = {
    "L0-DESIGN": (
        "L0 顶点", "docs/ACSD_DESIGN.md",
        "只写顶层结论：是什么、做到什么、顶层架构",
        "同层内最先出现的判据线索；冲突须负责人裁决",
    ),
    "L1-SCI": (
        "L1 正本·科学", "docs/science/**",
        "公式、常数、判据、推导",
        "科学量的正本落点；数值以本层为准",
    ),
    "L1-ENG": (
        "L1 正本·工程", "docs/engineering/**",
        "架构、行为合同、工程标准",
        "合同与键集合的正本落点",
    ),
    "L2-DETAIL": (
        "L2 细节", "docs/detail/**",
        "模块工作细节、数据对象与接口落地",
        "实现细节的落点；与正本同数须逐字一致",
    ),
    "R-INDEX": (
        "R 登记册·地图", "docs/DOCUMENT_INDEX.yaml",
        "全文档集唯一地图",
        "只应作索引；出现断言正文 = 职责疑似越界",
    ),
    "R-GOVERNANCE": (
        "R 登记册·治理台账", "docs/engineering/governance/**",
        "可追溯矩阵、待裁决台账、治理规范",
        "台账滞后是本仓高发形态：正文改了台账没改，或反过来",
    ),
    "R-GLOSSARY": (
        "R 登记册·术语", "docs/GLOSSARY.md",
        "术语与标度口径",
        "只应登记词条；同名不同义的高发面",
    ),
    "L3-CONTRACT": (
        "L3 合同面", "eng/contracts/**",
        "schema 与机器可读合同",
        "实现侧契约；与正本冲突以正本为准订正",
    ),
    "L3-CONFIG": (
        "L3 配置面", "eng/packaging/config/**",
        "机器可读配置与默认值、source_ref.quote 溯源锚",
        "配置锁的是冻结值；quote 与正本不同步是本仓实测的高发形态",
    ),
    "L3-CODE": (
        "L3 代码", "lib/**", "实现",
        "最低权威；与文档冲突按文档订正",
    ),
    "L3-TOOL": (
        "L3 工具", "eng/tools/**、eng/tests/**",
        "工程工具与测试集",
        "测试是独立工具集，不是门禁",
    ),
    "EXP": (
        "EXP 实验单元", "实验/**",
        "实验报告、复现脚本、裁决记录",
        "证据面；与正本冲突以正本为准",
    ),
    "GOV": (
        "GOV 审核包", "run/**",
        "审稿、订正、裁决过程记录",
        "默认不扫；过程记录里的旧值不是残留",
    ),
    "OTHER": ("其他", "（其余路径）", "—", "无层次约定，需人判"),
}

LAYER_ORDER = [
    "L0-DESIGN", "L1-SCI", "L1-ENG", "L2-DETAIL", "R-INDEX", "R-GLOSSARY",
    "R-GOVERNANCE", "L3-CONTRACT", "L3-CONFIG", "L3-CODE", "L3-TOOL", "EXP", "GOV", "OTHER",
]

SCAN_EXTS = {".md", ".yaml", ".yml", ".json", ".cpp", ".hpp", ".h", ".cc",
             ".py", ".txt", ".cmake", ".in", ".toml"}

PRUNE_DIRS = {
    ".git", "build", "out", "node_modules", "__pycache__", ".pytest_cache",
    ".dsh-code-index", "site", ".venv", "venv", "dist", ".mypy_cache",
    "testdata", "artifacts", "gaia",
}

# 默认整目录不扫的路径前缀（过程记录/外部数据，非断言正本面）
DEFAULT_EXCLUDE_PREFIXES = ("run/",)

MAX_FILE_BYTES = 8 * 1024 * 1024


# ---------------------------------------------------------------------------
# 记法归一：只用来判「是不是同一个式子的不同写法」，不用来下结论
# ---------------------------------------------------------------------------
_SUPERSCRIPT = {
    "⁰": "0", "¹": "1", "²": "2", "³": "3", "⁴": "4",
    "⁵": "5", "⁶": "6", "⁷": "7", "⁸": "8", "⁹": "9",
    "⁻": "-", "⁺": "+",
}
_SUB = {"₀": "0", "₁": "1", "₂": "2", "₃": "3", "₄": "4",
        "₅": "5", "₆": "6", "₇": "7", "₈": "8", "₉": "9"}
_MATH_UNIFY = {
    "×": "*", "·": "*", "∗": "*", "⁃": "3",
    "√": "sqrt", "≠": "!=", "≈": "~=", "≤": "<=", "≥": ">=",
    "−": "-", "‐": "-", "–": "-", "—": "-", "─": "-",
    "′": "'", "‘": "'", "’": "'", "“": '"', "”": '"',
}


def normalize_formula(s: str) -> str:
    """压掉公式/记法的书写差异，只留语义骨架。

    归一后相同 =「同一个式子的不同写法」；归一后不同 =「需要人看」。
    """
    s = unicodedata.normalize("NFKC", s)
    out = []
    for ch in s:
        if ch in _SUPERSCRIPT:
            out.append("^" + _SUPERSCRIPT[ch])
        elif ch in _SUB:
            out.append("_" + _SUB[ch])
        elif ch in _MATH_UNIFY:
            out.append(_MATH_UNIFY[ch])
        else:
            out.append(ch)
    s = "".join(out)
    s = re.sub(r"sqrt\s*\(\s*([^()]*?)\s*\)", r"sqrt(\1)", s)
    return re.sub(r"\s+", "", s).strip().lower()


def fold_ident(s: str) -> str:
    """标识符折叠：只留字母数字并转小写。

    `FrameSnr` / `FRAMESNR` / `frame_snr` / `frameSnr` 折叠成同一个键。
    逐字匹配必然漏掉大小写混合的类型名，只有它才看得见。
    """
    s = unicodedata.normalize("NFKC", s)
    return re.sub(r"[^0-9a-z]", "", s.lower())


TOKEN_RE = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")
NUMBER_RE = re.compile(r"(?<![\w.])(\d+(?:\.\d+)?)(?![\w.])")
MD_HEADING_RE = re.compile(r"^(#{1,6})\s+(.*\S)\s*$")
COMMENTISH_RE = re.compile(r"^\s*(//|\*|/\*|#)")


def die_usage(msg):
    """用法/查询值错误：与 argparse 一致退 2，与内部崩溃的 1 区分开。"""
    sys.stderr.write("用法错误：%s\n" % msg)
    raise SystemExit(2)


def classify_layer(relpath: str) -> str:
    p = relpath.replace(os.sep, "/")
    if p == "docs/ACSD_DESIGN.md":
        return "L0-DESIGN"
    if p == "docs/DOCUMENT_INDEX.yaml":
        return "R-INDEX"
    if p == "docs/GLOSSARY.md":
        return "R-GLOSSARY"
    if p.startswith("docs/science/"):
        return "L1-SCI"
    if p.startswith("docs/engineering/governance/"):
        return "R-GOVERNANCE"
    if p.startswith("docs/engineering/"):
        return "L1-ENG"
    if p.startswith("docs/detail/"):
        return "L2-DETAIL"
    if p.startswith("eng/contracts/"):
        return "L3-CONTRACT"
    if p.startswith("eng/packaging/config/"):
        return "L3-CONFIG"
    if p.startswith("eng/tools/") or p.startswith("eng/tests/"):
        return "L3-TOOL"
    if p.startswith("lib/"):
        return "L3-CODE"
    if p.startswith("实验/"):
        return "EXP"
    if p.startswith("run/"):
        return "GOV"
    if p in ("AGENTS.md", "README.md", "docs/README.md"):
        return "L0-DESIGN"
    return "OTHER"


# ---------------------------------------------------------------------------
# 文件遍历
# ---------------------------------------------------------------------------
def iter_files(root: str, include_run: bool, extra_exclude=()):
    root = os.path.abspath(root)
    excl = list(extra_exclude) + (list(DEFAULT_EXCLUDE_PREFIXES)
                                  if not include_run else [])
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root).replace(os.sep, "/")
        if rel_dir == ".":
            rel_dir = ""
        dirnames[:] = sorted(
            d for d in dirnames if d not in PRUNE_DIRS and not d.startswith(".git"))
        for fn in sorted(filenames):
            rel = (rel_dir + "/" + fn) if rel_dir else fn
            if os.path.splitext(fn)[1].lower() not in SCAN_EXTS:
                continue
            if any(rel == pre or rel.startswith(pre) for pre in excl):
                continue
            full = os.path.join(dirpath, fn)
            try:
                if os.path.getsize(full) > MAX_FILE_BYTES:
                    continue
            except OSError:
                continue
            yield rel, full


def read_text(full: str):
    try:
        with open(full, "r", encoding="utf-8", errors="replace") as fh:
            return fh.read()
    except OSError:
        return ""


def nearest_section(lines, idx):
    for i in range(idx, -1, -1):
        m = MD_HEADING_RE.match(lines[i])
        if m:
            return "%s %s" % (m.group(1), m.group(2))
        if COMMENTISH_RE.match(lines[i]) and i < idx - 40:
            return lines[i].strip()[:80]
    return ""


def clip(s: str, width=110):
    s = s.replace("\t", " ")
    s = re.sub(r"\s+", " ", s).strip()
    return s if len(s) <= width else s[: width - 1] + "…"


def make_hit(mode, rel, lineno, layer, section, excerpt, matched, extra=None):
    h = {
        "mode": mode, "file": rel, "line": lineno, "layer": layer,
        "layer_label": LAYERS[layer][0], "layer_path": LAYERS[layer][1],
        "section": section, "excerpt": clip(excerpt), "matched": matched,
    }
    if extra:
        h.update(extra)
    return h


# ---------------------------------------------------------------------------
# 四种查询模式
# ---------------------------------------------------------------------------
def scan_text(root, needle, include_run, extra_exclude):
    """逐字子串。零歧义，但只看得见逐字相同的那部分。"""
    hits = []
    nb = needle.encode("utf-8")
    for rel, full in iter_files(root, include_run, extra_exclude):
        try:
            with open(full, "rb") as fh:
                if nb not in fh.read():
                    continue
        except OSError:
            continue
        lines = read_text(full).splitlines()
        layer = classify_layer(rel)
        sec = None
        for i, line in enumerate(lines):
            if needle not in line:
                if sec is None or i % 64 == 0:
                    sec = nearest_section(lines, i)
                continue
            occ = 0
            pos = line.find(needle)
            while pos != -1:
                occ += 1
                hits.append(make_hit("text", rel, i + 1, layer,
                                     sec or nearest_section(lines, i),
                                     line, needle, {"occ": occ}))
                pos = line.find(needle, pos + len(needle))
    return hits


def scan_regex(root, pattern, include_run, extra_exclude):
    """正则锚点。只作定位，不作判定依据——本仓已实测正则漏检。"""
    hits = []
    try:
        rx = re.compile(pattern)
    except re.error as exc:
        die_usage("正则不合法：%s" % exc)
    for rel, full in iter_files(root, include_run, extra_exclude):
        text = read_text(full)
        if not rx.search(text):
            continue
        lines = text.splitlines()
        layer = classify_layer(rel)
        sec = None
        for i, line in enumerate(lines):
            ms = rx.findall(line)
            if not ms:
                continue
            if sec is None or i % 64 == 0:
                sec = nearest_section(lines, i)
            got = ms[0] if isinstance(ms[0], str) else ms[0][0]
            hits.append(make_hit("regex", rel, i + 1, layer,
                                 sec or nearest_section(lines, i), line, got))
    return hits


def scan_ident(root, name, include_run, extra_exclude, prefix=False):
    """标识符折叠匹配：抓大小写混合 / 下划线风格不同的同一类型名。

    --prefix 时把「折叠后互为前缀」也算命中，用于抓 `bilinear` 与
    `bilinear_4quad` 这类包含关系（精确相等比较必漏）。
    """
    hits = []
    target = fold_ident(name)
    if not target:
        die_usage("--ident 折叠后为空：%r" % name)
    for rel, full in iter_files(root, include_run, extra_exclude):
        text = read_text(full)
        if not text:
            continue
        lines = text.splitlines()
        layer = classify_layer(rel)
        sec = None
        for i, line in enumerate(lines):
            toks = TOKEN_RE.findall(line)
            if not toks:
                continue
            hit_tok = None
            for tok in toks:
                f = fold_ident(tok)
                if not f:
                    continue
                if f == target:
                    hit_tok = tok
                    break
                if prefix and (f.startswith(target) or target.startswith(f)) \
                        and min(len(f), len(target)) >= 4:
                    hit_tok = tok
                    break
            if hit_tok is None:
                continue
            if sec is None or i % 64 == 0:
                sec = nearest_section(lines, i)
            hits.append(make_hit(
                "ident", rel, i + 1, layer, sec or nearest_section(lines, i),
                line, hit_tok,
                {"fold_target": target,
                 "relation": "折叠同形" if fold_ident(hit_tok) == target
                 else "折叠前缀包含"}))
    return hits


def _common_prefix_len(a: str, b: str) -> int:
    k = 0
    while k < len(a) and k < len(b) and a[k] == b[k]:
        k += 1
    return k


def scan_num(root, value, include_run, extra_exclude, prefix_min=5, tol=1e-3):
    """数值近邻：抓「同一常数在别处留着旧值 / 截断值 / 近似值」。

    给定真值 V，列出全仓与 V「像同一个量」的数 W：
      - 有效数字公共前缀 ≥ prefix_min 位，**且公共前缀已越过候选数的整数部
        至少 2 位**（否则 `1482.63` 会被 `1.4826` 的前缀误吸），相对差 < 0.5
        → 同前缀·不同精度（ULP 差、截断值都落在这里）；
      - 公共前缀 ≥3 位且 |V-W|/|V| < tol → 同量级·疑似旧值。
    两条都带量级护栏：`1482.63` 相对 `1.4826` 差 1000 倍，不是精度变体。

    这是**分母**：命中可能是刻意的近似标注、有意的负例示范或历史记录，
    必须读上下文再判。本仓实测：`211034.6` 会被本模式命中，但它是
    docs/science/algorithms/DRIZZLE_GEOMETRY.md 里**故意写的反例**
    （说明抄错值会怎样），不是残留。
    """
    hits = []
    v_str = str(value).strip()
    try:
        v_f = float(v_str)
    except ValueError:
        die_usage("数值不合法：%s" % value)
    v_digits = v_str.replace(".", "")
    v_int = v_str.split(".")[0]
    exact = 0
    for rel, full in iter_files(root, include_run, extra_exclude):
        text = read_text(full)
        if not text:
            continue
        lines = text.splitlines()
        layer = classify_layer(rel)
        sec = None
        for i, line in enumerate(lines):
            for m in NUMBER_RE.finditer(line):
                num_str = m.group(1)
                if num_str == v_str:
                    exact += 1          # 正本本身不进邻居表，另计
                    continue
                try:
                    num_f = float(num_str)
                except ValueError:
                    continue
                if v_f == 0:
                    continue
                rel_diff = abs(num_f - v_f) / abs(v_f)
                d_digits = num_str.replace(".", "")
                d_int_len = len(num_str.split(".")[0])
                k = _common_prefix_len(v_digits, d_digits)
                same_int = d_int_len == len(v_int) and num_str.split(".")[0] == v_int
                past_int = k - d_int_len   # 公共前缀越过候选数整数部的位数
                kind = None
                if k >= prefix_min and past_int >= 2 and rel_diff < 0.5:
                    kind = "同前缀·不同精度"
                elif k >= 3 and rel_diff < tol:
                    kind = "同量级·疑似旧值"
                elif same_int and len(v_int) >= 4 and rel_diff < 1e-2:
                    kind = "同整数部·疑似旧值"
                if kind is None:
                    continue
                if sec is None or i % 64 == 0:
                    sec = nearest_section(lines, i)
                hits.append(make_hit(
                    "num", rel, i + 1, layer, sec or nearest_section(lines, i),
                    line, num_str,
                    {"target_value": v_str, "rel_diff": rel_diff,
                     "rel_diff_pct": "%.2e" % rel_diff,
                     "shared_prefix": k, "past_int": past_int,
                     "prev_line": clip(lines[i - 1], 90) if i else "",
                     "neighbor_kind": kind}))
    # 正本自身的位置不进上表（否则每次都把自己算成"分歧"），
    # 但必须报出来——只看邻居会以为真值只存在于别处。
    hits.append({"mode": "num-canonical", "file": "(本仓)", "line": 0,
                 "layer": "OTHER", "layer_label": "计数项", "layer_path": "—",
                 "section": "", "excerpt": "", "matched": v_str,
                 "verdict": "正本自身出现次数",
                 "reasons": ["目标值 %s 逐字出现 %d 次（这些位置不列在邻居表里）；"
                             "要看见它们的位置请另跑 --text %r" % (v_str, exact, v_str)]})
    return hits


# ---------------------------------------------------------------------------
# 初判：只给依据，不下判决
# ---------------------------------------------------------------------------
VERDICT_HELP = {
    "唯一正本候选": "全仓只此一处，且落在正本层。仍需人确认它确实该是正本。",
    "重复副本·逐字一致": "多处逐字相同。是同一断言的多个副本——但「是否允许存在副本」要人判。",
    "重复副本·记法不同": "归一后相同、逐字不同（空格/根号/上下标/乘号）。属书写分叉，不必然是矛盾。",
    "数值分歧·需人裁": "同一锚点在不同处带的数值不同。可能是旧值残留、精度截断，也可能是有意的近似或负例示范。",
    "同名不同义·候选": "同层多处对同一断言的表述在锚点之外有实质差异。**工具只摆差异，不判是不是同义**——本项目已多次因此误伤。",
    "单一层内命中": "只落在一个层次里。跨层一致性无从判断；不等于没问题。",
    "登记册含断言正文": "登记册（DOCUMENT_INDEX.yaml / GLOSSARY.md / governance 台账）里出现断言正文而不只是索引，疑似职责越界或台账滞后。",
    "强度词·同段见限定词": "该句用了全称强度词，且同段内找到了限定词。工具不判这是否充分。",
    "强度词·同段未见限定词": "该句用了「唯一/恒/严格/必然」这类全称强度词，但同段没找到任何限定词。本仓实测这类句常与紧邻的另一句互斥（改表不改 bullet）。是否真缺限定由人判。",
}


def span_text(h):
    lo, hi = h.get("window_lines", [0, 0])
    return "%d–%d" % (lo, hi)


def annotate(hits, modes):
    layers_seen = sorted({h["layer"] for h in hits},
                         key=lambda k: LAYER_ORDER.index(k))
    n_layers = len(layers_seen)
    forms = {normalize_formula(h.get("matched", "")) for h in hits}
    registry_layers = {"R-INDEX", "R-GLOSSARY", "R-GOVERNANCE"}

    for h in hits:
        if h.get("mode") == "num-canonical":
            continue          # 正本自身已自带 verdict，不改写
        if "strength" in modes:
            q = h.get("qualifiers_in_window") or []
            if q:
                h["verdict"] = "强度词·同段见限定词"
                h["reasons"] = ["强度词 %s；同段(%s行内)见限定词：%s" % (
                    "、".join(h.get("strength_words", [])), span_text(h), "、".join(q))]
            else:
                h["verdict"] = "强度词·同段未见限定词"
                h["reasons"] = ["强度词 %s；同段 %s 行内未见任何限定词——"
                                "是否真缺限定由人判" % (
                                    "、".join(h.get("strength_words", [])), span_text(h))]
        elif "num" in modes:
            h["verdict"] = "数值分歧·需人裁"
            h["reasons"] = ["数值近邻：%s；与 %s 相对差 %s" % (
                h.get("neighbor_kind", ""), h.get("target_value", ""),
                h.get("rel_diff_pct", "")),
                "命中不等于矛盾：近似标注、负例示范、历史记录都会命中"]
        elif h["layer"] in registry_layers:
            h["verdict"] = "登记册含断言正文"
            h["reasons"] = ["登记册层职责是索引/词条；此处出现断言正文需确认是否越界或台账滞后"]
        elif len(hits) == 1:
            h["verdict"] = "唯一正本候选"
            h["reasons"] = ["全仓仅此一处命中"]
        elif n_layers == 1:
            h["verdict"] = "单一层内命中"
            h["reasons"] = ["命中集中在 %s 一层内" % LAYERS[h["layer"]][1]]
        elif len(forms) > 1:
            h["verdict"] = "同名不同义·候选"
            h["reasons"] = ["归一后形态 %d 种，锚点之外表述有实质差异；"
                            "须逐处读上下文确认是不是同义" % len(forms)]
        else:
            h["verdict"] = "重复副本·逐字一致"
            h["reasons"] = ["跨 %d 层出现且归一后形态唯一：%s" % (
                n_layers, "、".join(LAYERS[k][1] for k in layers_seen))]
    return hits


STRENGTH_WORDS = ("唯一", "恒", "严格", "必然", "一律", "必须", "总是",
                  "全部", "零", "无例外")
STRENGTH_QUALIFIERS = ("仅当", "仅在", "高斯", "等权", "行归一", "跨叶",
                       "在…下", "前提下", "成立域", "适用域", "若", "当且仅当",
                       "非定理", "充分条件", "不构成", "不含", "除外")


def scan_strength(root, word, include_run, extra_exclude, span=2):
    """强度词 + 限定词共现扫描。

    本仓实测形态：正本同一文件里既有「X 一律以 schema 为准」，又有新写的
    「本列是 X 的唯一正本」；判据表说 ①③ 无 enforcement 键、紧邻 bullet 却说
    「①③ 任一违约即判红」。这类矛盾的机器信号是**强度词出现而同段没有限定词**。
    工具只报「同段未见限定词」这一事实，是否真缺限定由人判。
    """
    hits = []
    needle = word or "恒"
    for rel, full in iter_files(root, include_run, extra_exclude):
        lines = read_text(full).splitlines()
        layer = classify_layer(rel)
        sec = None
        for i, line in enumerate(lines):
            if needle not in line:
                continue
            lo, hi = max(0, i - span), min(len(lines), i + span + 1)
            window = " ".join(lines[lo:hi])
            quals = [q for q in STRENGTH_QUALIFIERS if q in window]
            strs = [w for w in STRENGTH_WORDS if w in line]
            if sec is None:
                sec = nearest_section(lines, i)
            hits.append(make_hit(
                "strength", rel, i + 1, layer, sec, line, needle,
                {"strength_words": strs or [needle],
                 "qualifiers_in_window": quals,
                 "window_lines": [lo + 1, hi]}))
    return hits


def summarize(hits):
    if not hits:
        return {"命中数": 0, "文件数": 0, "层数": 0}
    lc = {}
    for h in hits:
        lc[h["layer"]] = lc.get(h["layer"], 0) + 1
    vd = {}
    for h in hits:
        vd[h["verdict"]] = vd.get(h["verdict"], 0) + 1
    # 同文件内的重复出现距离：本仓实测「改表不改紧邻 bullet」矛盾只隔 7–20 行
    proximity = []
    by_file = {}
    for h in hits:
        by_file.setdefault(h["file"], []).append(h["line"])
    for f, lines in sorted(by_file.items()):
        ls = sorted(set(lines))
        if len(ls) < 2:
            continue
        gaps = [ls[i + 1] - ls[i] for i in range(len(ls) - 1)]
        proximity.append({"file": f, "处数": len(ls),
                          "最近距离": min(gaps), "最大距离": max(gaps)})
    proximity.sort(key=lambda x: x["最近距离"])
    return {
        "命中数": len(hits),
        "文件数": len({h["file"] for h in hits}),
        "层数": len(lc),
        "按层分布": {LAYERS[k][0] + "（" + LAYERS[k][1] + "）": v
                 for k, v in sorted(lc.items(),
                                    key=lambda kv: LAYER_ORDER.index(kv[0]))},
        "按初判分布": vd,
        "同文件内重复_按最近距离排序": proximity[:12],
    }


def build_caliber(args, modes, files_scanned, elapsed):
    """口径块：任何计数离开它就不可比。

    第三轮审稿判定「收敛度量不可信」的直接原因就是把不同口径的数配成
    before/after（全树改前 709 × 子集改后 46）。本块把搜索根、文件过滤、
    行形态过滤、查询原文、大小写策略、计数单位、工作树指纹全部钉死。
    """
    import hashlib
    try:
        with open(os.path.abspath(__file__), "rb") as fh:
            self_md5 = hashlib.md5(fh.read()).hexdigest()
    except OSError:
        self_md5 = "(不可读)"
    return {
        "搜索根": os.path.abspath(args.root),
        "查询模式": list(modes) + (["prefix"] if getattr(args, "prefix", False) else []),
        "查询原文": {m: repr(getattr(args, m)) for m in modes},
        "窗口行数": getattr(args, "span", None),
        "前缀阈值": getattr(args, "prefix_min", None),
        "相对差阈值": getattr(args, "tol", None),
        "文件类型过滤": "、".join(sorted(SCAN_EXTS)),
        "剪掉目录": "、".join(sorted(PRUNE_DIRS)),
        "额外排除": list(args.exclude) or ["（无）"],
        "run_面": "含" if args.include_run else "不含 run/（审核过程记录）",
        "大小写策略": "--ident 折叠转小写；--text/--regex/--num 区分大小写",
        "行形态过滤": "无（逐行全文匹配；表格行、代码注释行、YAML 行一视同仁）",
        "计数单位": "命中处数（一行内多次出现按多次计）；文件数、层数另计",
        "扫描文件数": files_scanned,
        "工具自身md5": self_md5,
        "耗时秒": round(elapsed, 2),
        "禁止": "本口径下的数字不得与其它口径/其它轮次的数字配成 before/after；"
                "扫描文件数会随交付件落盘漂移，须连同本块一起引用",
    }


BANNER = """\
────────────────────────────────────────────────────────────────
【本工具是检查工具，不是判据】
· 退出码恒为 0（发现分歧不是错误）；不进默认构建；不自动改任何文件。
· 计数只作分母。命中与否不构成通过与否——判定必须来自阅读。
· 逐字匹配看不见：大小写混合的类型名、被改写过的式子（换符号/换单位/
  换记法/拆句/只留结论不留公式）里不含该子串的那些行。
  抓大小写混合类型名用 --ident；抓前缀包含关系（bilinear ⊂ bilinear_4quad）
  再加 --prefix。即便如此仍会漏（连字、希腊字母转写、Unicode 兼容字形）。
· 「同名不同义」工具只把分歧摆出来，不判是不是同义；判成重复而误伤，
  本项目已实测发生过。
· 数值近邻（--num）尤其易误报：刻意的近似标注、有意的负例示范、历史
  记录都会命中。逐条读上下文再动。
· 恒真门/恒红门（判据与被检量同源）不在本工具能力内，需要符号求值。
· 版本控制盲区：本工具扫的是**工作树**。`docs/engineering/build/` 被
  `.gitignore` 的 `build/` 规则排除（git ls-files 为 0），干净克隆上整目录
  不存在——只在工作树扫会把「已如实登记」误判为已闭合。此项本工具不判定。
────────────────────────────────────────────────────────────────\
"""


def print_report(hits, args, modes, query_desc, caliber):
    print(BANNER)
    print("查询：%s" % query_desc)
    print("根目录：%s" % os.path.abspath(args.root))
    print("扫描范围：%s" % ("含 run/ 审核过程记录" if args.include_run
                          else "不含 run/（审核过程记录，不是断言正本面）"))
    print("扫描文件数：%d" % caliber["扫描文件数"])
    print()
    print("── 口径（离开它，计数不可比；禁止跨口径配 before/after）")
    for k in ("查询模式", "文件类型过滤", "剪掉目录", "额外排除", "run_面",
              "大小写策略", "行形态过滤", "计数单位", "扫描文件数"):
        print("   %-12s %s" % (k, caliber[k]))
    print()
    if not hits:
        print("命中 0 处。")
        print("注意：命中 0 ≠ 无矛盾。逐字/正则匹配抓不到不含该子串的行，")
        print("      也抓不到大小写混合的类型名——读上节限制后换 --ident/--num 再试。")
        return

    order = {k: i for i, k in enumerate(LAYER_ORDER)}
    hits.sort(key=lambda h: (order.get(h["layer"], 99), h["file"], h["line"]))
    cur = None
    for h in hits:
        if h["layer"] != cur:
            cur = h["layer"]
            lab, path, duty, _ = LAYERS[cur]
            print("── %s  %s" % (lab, path))
            print("   该层该讲：%s" % duty)
        print("  %s:%d  § %s" % (h["file"], h["line"], h["section"] or "—"))
        print("      命中：%s" % clip(h.get("matched", ""), 100))
        print("      上下文：%s" % h["excerpt"])
        print("      初判：%s" % h.get("verdict", "—"))
        for r in h.get("reasons", []):
            print("        依据：%s" % r)
        print()

    prox = summarize(hits)["同文件内重复_按最近距离排序"]
    if prox:
        print("── 同文件内多次命中（按最近距离升序；距离越近越值得先读——"
              "本仓实测「改表不改紧邻 bullet」这类矛盾就出现在相邻几行内）")
        for p in prox[:8]:
            print("   %-58s %d 处，最近相距 %d 行" % (
                p["file"], p["处数"], p["最近距离"]))
        print()
    print("─" * 64)
    print("初判含义对照（初判≠判决，逐条读上下文后由人裁定）：")
    seen = []
    for h in hits:
        v = h.get("verdict", "")
        if v and v not in seen:
            seen.append(v)
    for v in seen:
        print("  · %s —— %s" % (v, VERDICT_HELP.get(v, "")))
    print()
    print("复跑本条查询：")
    print("  " + " ".join(rebuild_cmd(args, modes)))
    print("  （分母口径：命中 %d 处 / %d 个文件 / %d 个层次）" % (
        len(hits), len({h["file"] for h in hits}),
        len({h["layer"] for h in hits})))


def rebuild_cmd(args, modes):
    p = ["python3 eng/tools/doccheck/check_assert_dup.py"]
    for m in modes:
        p += ["--" + m, repr(getattr(args, m))]
    if getattr(args, "prefix", False):
        p += ["--prefix"]
    if "strength" in modes:
        p += ["--span", str(args.span)]
    p += ["--root", args.root]
    if args.include_run:
        p += ["--include-run"]
    for e in args.exclude:
        p += ["--exclude", e]
    return p


# ---------------------------------------------------------------------------
# 自检：固定夹具，口径公开
# ---------------------------------------------------------------------------
def selftest(root):
    """跑内置夹具，把实测命中数与初判打在屏幕上。

    给人看的诊断表，不是门禁：恒退出 0，通过与否由读表的人判。
    """
    print(BANNER)
    print("自检：内置夹具（查询取自本仓真实文本）")
    print("根目录：%s" % os.path.abspath(root))
    print("-" * 72)
    cases = [
        ("权重换算式跨层副本", "--text 'w = SNR²/F_ref²'",
         lambda: scan_text(root, "w = SNR²/F_ref²", False, ())),
        ("MAD 常数的 ULP 邻居（真阳性：PSF.md 用了 1.4826022185056023）",
         "--num 1.482602218505602",
         lambda: scan_num(root, "1.482602218505602", False, ())),
        ("大小写混合类型名（逐字抓不到，折叠抓得到）", "--ident frame_snr",
         lambda: scan_ident(root, "frame_snr", False, ())),
        ("标识符前缀包含（bilinear ⊂ bilinear_4quad）",
         "--ident bilinear --prefix",
         lambda: scan_ident(root, "bilinear", False, (), prefix=True)),
    ]
    for desc, cmd, fn in cases:
        hits = annotate(fn(), ("text",))
        s = summarize(hits)
        layers = sorted({h["layer"] for h in hits},
                        key=lambda k: LAYER_ORDER.index(k))
        print("夹具：%s" % desc)
        print("  查询：%s" % cmd)
        print("  实测：命中=%d 文件=%d 层=%d" % (
            s["命中数"], s["文件数"], s["层数"]))
        print("  层次：%s" % ("、".join(LAYERS[k][0] for k in layers) or "—"))
        print("  初判分布：%s" % (s.get("按初判分布") or "—"))
        for h in hits[:4]:
            print("    例：%s:%d  %s" % (h["file"], h["line"],
                                     clip(h.get("matched", ""), 56)))
        print("-" * 72)
    print("读法：命中数是分母。初判分布说明工具把分歧摆在了哪些类别上；")
    print("     「是不是矛盾」由你读上下文后判定。")


# ---------------------------------------------------------------------------
def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="check_assert_dup.py",
        description="断言副本检测器（检查工具，非判据；退出码恒 0）")
    ap.add_argument("--text", help="逐字子串（公式片段/判据句）")
    ap.add_argument("--regex", help="正则锚点；只作定位，不作判定依据")
    ap.add_argument("--ident", help="标识符折叠匹配，抓大小写混合的类型名")
    ap.add_argument("--num", help="数值近邻，抓同一常数的旧值/截断值/近似值")
    ap.add_argument("--prefix", action="store_true",
                    help="--ident 额外把折叠后互为前缀的标识符算命中")
    ap.add_argument("--strength", help="强度词（同段未见限定词则标出）；缺省扫「恒」")
    ap.add_argument("--span", type=int, default=2,
                    help="--strength 的上下文窗口行数（默认 2）")
    ap.add_argument("--root", default=".", help="仓根（默认当前目录）")
    ap.add_argument("--include-run", action="store_true",
                    help="连 run/ 审核过程记录一起扫（噪声很大，默认关）")
    ap.add_argument("--exclude", action="append", default=[],
                    help="额外排除的路径前缀，可重复")
    ap.add_argument("--prefix-min", type=int, default=5,
                    help="--num 同前缀位数阈值（默认 5）")
    ap.add_argument("--tol", type=float, default=1e-3,
                    help="--num 同量级相对差阈值（默认 1e-3）")
    ap.add_argument("--format", choices=("text", "json"), default="text")
    ap.add_argument("--selftest", action="store_true",
                    help="跑内置夹具诊断表（恒退出 0）")
    args = ap.parse_args(argv)

    if args.selftest:
        selftest(args.root)
        return 0

    modes = tuple(k for k in ("text", "regex", "ident", "num", "strength")
                  if getattr(args, k) is not None)
    if not modes:
        ap.error("至少给一个查询：--text / --regex / --ident / --num / "
                 "--strength / --selftest")

    root = os.path.abspath(args.root)
    if not os.path.isdir(root):
        ap.error("根目录不存在：%s" % root)
    t0 = time.time()
    args.files_scanned = sum(
        1 for _ in iter_files(root, args.include_run, tuple(args.exclude)))

    hits = []
    if args.text is not None:
        hits += scan_text(root, args.text, args.include_run, tuple(args.exclude))
    if args.regex is not None:
        hits += scan_regex(root, args.regex, args.include_run, tuple(args.exclude))
    if args.ident is not None:
        hits += scan_ident(root, args.ident, args.include_run,
                           tuple(args.exclude), prefix=args.prefix)
    if args.num is not None:
        hits += scan_num(root, args.num, args.include_run, tuple(args.exclude),
                         prefix_min=args.prefix_min, tol=args.tol)
    if args.strength is not None:
        hits += scan_strength(root, args.strength, args.include_run,
                              tuple(args.exclude), span=args.span)

    hits = annotate(hits, modes)
    elapsed = time.time() - t0
    desc = "; ".join("%s=%r" % (m, getattr(args, m)) for m in modes)
    if getattr(args, "prefix", False):
        desc += "; prefix=True"
    caliber = build_caliber(args, modes, args.files_scanned, elapsed)

    if args.format == "json":
        print(json.dumps({
            "工具": "check_assert_dup.py",
            "定位": "检查工具，非判据；计数为分母，判定由人阅读做出",
            "查询": desc, "根目录": root,
            "口径": caliber,
            "汇总": summarize(hits), "命中": hits,
        }, ensure_ascii=False, indent=2))
    else:
        print_report(hits, args, modes, desc, caliber)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)