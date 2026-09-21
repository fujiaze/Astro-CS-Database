#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-001 语义检查器：AGENTS.md 硬禁令 / 权威链唯一性 / 旧权威回归红灯。

改造前（旧版）本检查器把某一版 AGENTS.md 的 10 组字符串当硬编码期望，
旧 AGENTS.md 一被改写即误报 rc=1（GOV-001 基线），且对"旧权威重新出现"零覆盖。

本版不再做文本快照，而是按语义断言：

  [1] AGENTS.md 结构：§1 开工前必读 / §5 硬禁令 / §8 停止点 三节必须存在且非空；
  [2] §5 硬禁令逐条抽取，断言条数下限 + 每条为"否定式/硬约束式"且含动宾实质；
  [3] §5 硬禁令的语义族覆盖（科学红线、三阶段隔离、无硬编码线程、main-only、
      禁止 force push、运行产物落位、发布权、禁空骨架冒充、不得掩盖失败、
      禁复制长文、禁读密钥）——任一语义族被整体删除即红；
  [3b] 语义族**反转检测**（CI-003-E 增补；R-6 §3.5 漏报）：某条 §5 条款命中语义族、
      却同时被写成反转/松绑措辞（如「科学公式与默认容差不再冻结，可按需调整」）
      ⇒ 判红，check=family_reversal:<族名>；否定语境（「不得解除冻结」）不误红；
  [4] §5 条款指向的上级权威必须存在于权威链白名单（正文文件必须真实存在）；
      出现已删除的旧权威对象（ASTROCS_PROJECT_CONSTITUTION.md /
      AstroCS_ENGINEERING_CONSTRAINTS.md / 设计大纲/）即红；
  [5] 权威链唯一性：ASTROCS_DESIGN.md §0 的 mermaid 链必须恰好覆盖
      ASTROCS_DESIGN → AGENTS → ENGINEERING_SPEC → CONTROL_PACK_SPEC →
      docs/ci → docs/plugins 六步且顺序正确（步骤全部为真实存在的路径）；
  [6] 反回归扫描：根权威面（README/AGENTS/ENGINEERING_SPEC/CONTROL_PACK_SPEC/
      HANDOVER）与 eng/tools/doccheck/、eng/tools/check_agents_gov.py 自身不得把
      已删除的旧权威文本写成上位/必读/冻结权威；
  [7] 活动文档"唯一权威"声明：不得再有除 ASTROCS_DESIGN.md 之外的文档自称
      "唯一最高权威/唯一最高约束"（docs/archive/** 与已标 ARCHIVED_NON_NORMATIVE 的
      归档件豁免；**唯一行内豁免 = 逐字点名 ASTROCS_DESIGN.md**）。
      扫描面 = <root> 自身（CI-003-E 修正：旧版在 --root 指向副本时把 base 换成
      不存在的 <root>/fixture-repo，scanned=0 ⇒ 假绿）；扫描面读不到任何文件时
      fail-closed 判红 unique_authority_scan_empty（ENGINEERING_SPEC §8）。

用法：
  python3 eng/tools/check_agents_gov.py [--root <repo>] [--json-out <file>] [--quiet]
  python3 eng/tools/check_agents_gov.py --self-test

--self-test（ENGINEERING_SPEC §8「可执行负例面」）：在 tempfile 造的 mini-repo 上跑
4 正例 + 6 负例（§5 科学红线反转 / 语义族整条删除 / 「本文档」自称唯一最高 /
旧权威回归 / 旧权威对象同名重建 / 扫描面为空 fail-closed），全部符合预期则自身 exit 0。
能红能绿历史证据（1 正例 + 7 负例）：run/PROJECT-GOVERNANCE-01/GOV-001/fixtures/
（runtime 验证脚本 verify_checkers.py，正例 = 真实仓库 rc=0）。
exit 0 = PASS；任一断言不成立 = exit 1（打印机器 JSON）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AGENTS = "AGENTS.md"
DESIGN = "ASTROCS_DESIGN.md"

# ---- 权威链（ASTROCS_DESIGN.md §0）与"下级可引用"白名单 --------------------
AUTHORITY_CHAIN = ["ASTROCS_DESIGN.md", "AGENTS.md", "ENGINEERING_SPEC.md",
                   "CONTROL_PACK_SPEC.md", "docs/ci", "docs/plugins"]
CITED_ALLOWED = {
    "ASTROCS_DESIGN.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
    "docs/ci", "docs/plugins", "docs/science", "docs/algorithms",
    "docs/design/UNIFIED_MODEL.md", "docs/GLOSSARY.md", "memory.md", "REPO",
}
# 已由 ROOT-007 删除 / 整体出库的旧治理对象：任何"上位权威/必读/冻结"写法都算回归
LEGACY_TOKENS = ["ASTROCS_PROJECT_CONSTITUTION.md", "AstroCS_ENGINEERING_CONSTRAINTS.md",
                 "设计大纲/"]
BINDING_MARKERS = ["权威", "上位", "最高", "FROZEN", "ACTIVE_NORMATIVE", "必读",
                   "冻结", "约束", "依据", "遵循", "按"]
NON_BINDING_MARKERS = ["ARCHIVED_NON_NORMATIVE", "ARCHIVED", "已删", "已删除", "已归档",
                       "已出库", "降级", "历史参照", "非权威", "不再作为", "复原",
                       "前身", "曾", "历史"]
# 重建对象本体用的强绑定措辞（不含"按/遵循"这类过宽词；对象身份由文件名给出，正文不必重复）
LEGACY_BINDING_STRONG = ["权威", "上位", "最高", "FROZEN", "ACTIVE_NORMATIVE", "必读",
                         "冻结", "约束", "依据"]
SELF_AUTHORITY_RE = re.compile(r"唯一\s*最高(权威|约束|规范|文档)")
# 声明"唯一最高"时必须出现的自指文件（ASTROCS_DESIGN.md 自身或其等价描述）
DESIGN_SELF_RE = re.compile(r"(本文|本文件|本设计|ASTROCS_DESIGN\.md)")

# §5 语义族：族名 -> 判定（对单条禁令文本）。
# CI-003-E：三处同义改写曾误红（R-6 §3.5）——密钥/三阶段/空骨架的判据词表按"动作语义"
# 扩面，而不是要求某一版文本的关键词原样出现。
FAMILIES = [
    ("科学红线",
     lambda t: "科学公式" in t and any(k in t for k in ("冻结", "容差", "不动", "不可", "定义"))),
    ("三阶段隔离",
     lambda t: ("normalize" in t and "mosaic" in t and "export" in t)
     or "三阶段" in t or "串" in t
     or "三个命令" in t or "相互独立" in t or "独立运行" in t
     or "不得串联" in t or "互不依赖" in t),
    ("无硬编码并行度",
     lambda t: (("workers" in t or "线程" in t or "ISA" in t) and "硬编码" in t)
     or ("benchmark" in t and "profile" in t)),
    ("main-only", lambda t: "main" in t and ("分支" in t or "worktree" in t or "clone" in t)),
    ("禁历史重写", lambda t: "force" in t.lower() or "amend" in t or "历史重写" in t),
    ("产物落位", lambda t: ("output_dir" in t or "run/" in t) and ("根目录" in t or "落" in t)),
    ("发布权", lambda t: "发布" in t and ("负责人" in t or "宣布" in t or "宣称" in t)),
    ("禁空骨架冒充",
     lambda t: any(k in t for k in ("facade", "空骨架", "no-op", "mock", "桩函数", "桩",
                                    "占位实现", "占位", "stub", "假实现")) and "冒充" in t),
    ("不得掩盖失败",
     lambda t: "掩盖失败" in t or ("环境问题" in t and "工具问题" in t)),
    ("禁复制长文", lambda t: "复制" in t and ("AGENTS.md" in t or "提交消息" in t)),
    ("禁读密钥",
     lambda t: ("密钥" in t or "凭据" in t)
     and any(k in t for k in ("读", "打印", "查看", "输出", "泄露", "泄漏", "暴露",
                              "记录", "复制", "上传", "落盘", "回显"))),
]
FAMILY_PRED = dict(FAMILIES)
FAMILY_MIN = 10  # §5 必须有 >= 10 条禁令，且 >= 10 个语义族被覆盖
CLAUSE_MIN_CHARS = 8
# 硬约束式（否定式）措辞：§5 条款必须是禁令/硬约束，不是愿望句。
# CI-003-E：「三个命令必须保持相互独立」与「不得串联」同义（R-6 §3.5 判为同义改写），
# 故把等价隔离措辞计入，避免同义改写继续误红。
NEG_FORM_RE = re.compile(r"禁|不|勿|免|独立|隔离|互斥")

# ---- 语义族反转/松绑检测（CI-003-E）----------------------------------------
# 命中语义族 + 命中反转措辞 ⇒ 判红。反例（R-6 §3.5 实测旧版静默 rc=0）：
#   「科学公式与默认容差不再冻结，可按需调整」——直接违反 ENGINEERING_SPEC §3 科学红线。
# 否定语境（「不得解除冻结」「不可按需调整」）是**禁止松绑**的写法，不判红（见 _forbidden_hit）。
FAMILY_FORBIDDEN = [
    ("科学红线",
     re.compile(r"不再冻结|解除冻结|允许修改|可以修改|可按需调整|可自由调整|随意调整|"
                r"不再不可修改|不再不可变更|无需冻结|不必冻结|"
                r"科学公式[^；。]{0,12}(?:允许|可以|可)[^；。]{0,6}(?:修改|调整|变更)"),
     "ENGINEERING_SPEC.md §3 科学代码红线：科学公式/默认容差/冻结定义不得被反转或松绑"),
    ("main-only",
     re.compile(r"(?:允许|可以|均可|可)[^；。]{0,4}在\s*main\s*(?:之)?外[^；。]{0,6}"
                r"(?:开|建|拉)?(?:分支|worktree|clone)"
                r"|在\s*main\s*外[^；。]{0,6}(?:开发|提交)"),
     "ENGINEERING_SPEC.md §6 / AGENTS.md §5：只 main 开发，分支/worktree/clone 不得松绑"),
    ("发布权",
     re.compile(r"(?:任何人|人人|均可|都可以|无需|不必|不需要)[^；。]{0,10}(?:宣布)?发布"
                r"|(?:无需|不必|不需要)[^；。]{0,8}负责人[^；。]{0,6}(?:确认|批准|决定)"),
     "AGENTS.md §5：最终发布决定权只归负责人，不得让渡"),
]
# 反转措辞前面的否定语境（「不得解除冻结」不是松绑，是禁止松绑）
NEG_GUARD_RE = re.compile(r"(?:禁止|不得|不许|严禁|切勿|勿|免|不|无)[^，。；、）]{0,3}$")


def read(path: str) -> str:
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def section(text: str, num: int) -> str:
    """取 ## <num>. 标题到下一个同级标题之间的正文。"""
    m = re.search(r"^##\s*%d\.[^\n]*\n(.*?)(?=^##\s|\Z)" % num, text, re.S | re.M)
    return m.group(1) if m else ""


def bullets(block: str) -> list:
    out = []
    for line in block.splitlines():
        s = line.strip()
        if s.startswith(("- ", "* ")):
            out.append(s[2:].strip())
    return out


def violations_legacy_binding(text: str, path: str) -> list:
    """文本行是否把已删除旧权威写成上位权威/必读/冻结对象。"""
    bad = []
    for i, line in enumerate(text.splitlines(), 1):
        if not any(tok in line for tok in LEGACY_TOKENS):
            continue
        if any(m in line for m in NON_BINDING_MARKERS):
            continue
        if any(m in line for m in BINDING_MARKERS):
            bad.append({"file": path, "line": i, "text": line.strip()[:160]})
    return bad


def violations_legacy_object_body(text: str, path: str) -> list:
    """根下被**重建**的已删除旧治理对象（文件名即旧对象名）：本体正文出现绑定措辞即红。

    与 violations_legacy_binding 的差别：那条判据要求**行内出现旧对象名**（用于扫描别的
    文档对旧对象的引用）；重建对象的本体不必重复自己的文件名——R-6 §3.6 实测
    「本文件为 … 权威，属上位必读的 FROZEN 依据」旧版 rc=0（该文件内容根本没被读）。
    带 ARCHIVED/已删除/历史参照等非绑定标记的行豁免（归档件仍可保留原文）。
    """
    bad = []
    for i, line in enumerate(text.splitlines(), 1):
        if any(m in line for m in NON_BINDING_MARKERS):
            continue
        if any(m in line for m in LEGACY_BINDING_STRONG):
            bad.append({"check": "legacy_authority_regression",
                        "detail": "%s:%d 已删除旧治理对象被重建并写成绑定权威: %s"
                        % (path, i, line.strip()[:120]),
                        "file": path, "line": i, "text": line.strip()[:160]})
    return bad


def label_to_authority(label: str) -> str:
    """§0 mermaid 节点标签 -> 权威链名（别名表；未知标签返回空串 => 判红）。"""
    if "最高设计" in label:
        return "ASTROCS_DESIGN.md"
    if "AGENTS" in label:
        return "AGENTS.md"
    if "工程规范" in label or "ENGINEERING_SPEC" in label:
        return "ENGINEERING_SPEC.md"
    if "控制包规范" in label or "CONTROL_PACK_SPEC" in label:
        return "CONTROL_PACK_SPEC.md"
    if "CI 规范" in label or "docs/ci" in label:
        return "docs/ci"
    if "插件文档" in label or "docs/plugins" in label:
        return "docs/plugins"
    return ""


def forbidden_hit(text: str, pat: re.Pattern):
    """返回第一个**非否定语境**的反转匹配；全是否定语境则返回 None。"""
    for m in pat.finditer(text):
        if NEG_GUARD_RE.search(text[max(0, m.start() - 8):m.start()]):
            continue
        return m
    return None


def check_authority_chain(root: str, v: list, notes: dict) -> None:
    p = os.path.join(root, DESIGN)
    if not os.path.isfile(p):
        v.append({"check": "design_authority_declaration", "detail": "缺 " + DESIGN})
        return
    text = read(p)
    sec0 = section(text, 0)
    if not sec0.strip():
        v.append({"check": "design_authority_declaration", "detail": "§0 权威声明缺失或为空"})
        return
    if "以本文为准" not in sec0:
        v.append({"check": "design_authority_declaration",
                  "detail": "§0 未声明「以本文为准」（最高权威不可让渡）"})
    # 权威链顺序：按 §0 mermaid 权威节点标签在本节中的出现次序
    labels = re.findall(r'^\s*([A-Za-z][A-Za-z0-9]*)\["([^"]+)"\]', sec0, re.M)
    mapped = []
    for _node, label in labels:
        name = label_to_authority(label)
        if name and name not in mapped:
            mapped.append(name)
    got = [n for n in mapped if n in AUTHORITY_CHAIN]
    notes["authority_chain"] = got
    if got != AUTHORITY_CHAIN:
        v.append({"check": "authority_chain_order",
                  "detail": "§0 权威链应为 %s，实测 %s"
                  % (" > ".join(AUTHORITY_CHAIN), " > ".join(got) or "(空)")})
    # 链上"可直接定位"的文件必须真实存在（docs/ci、docs/plugins 为目录）
    missing_paths = [n for n in AUTHORITY_CHAIN
                     if not os.path.exists(os.path.join(root, n))]
    if missing_paths:
        v.append({"check": "authority_chain_paths_exist",
                  "detail": "权威链指向不存在的路径: %s" % ",".join(missing_paths)})


def check_unique_authority(root: str, v: list, notes: dict) -> None:
    """除 ASTROCS_DESIGN.md 外，活动面不得自称"唯一最高权威/约束"。

    扫描面**总是** root 自身（CI-003-E：旧版 `base = root if root == REPO else
    os.path.join(root, "fixture-repo")` 让 --root 副本的扫描面指向不存在的
    <root>/fixture-repo ⇒ scanned=0 ⇒ 假绿，R-6 §3.5 实测）。
    扫描面一个文件都读不到时 fail-closed 判红（ENGINEERING_SPEC §8：不得把
    「文件不存在」当「无违规」）。
    """
    base = root
    # CI-003（2026-09-16）：memory.md 纳入扫描面。它是 §0 权威链相邻的根条目
    # （ENGINEERING_SPEC §7 固定条目）且承载历史教训，旧版不在面内 ⇒ 在其中写
    # 「本记录为仓库唯一最高权威」两门都看不见（R-6 §3.5 记的"空触发"另一半）。
    scan = [os.path.join(base, x) for x in
            ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
             "memory.md")
            if os.path.isfile(os.path.join(base, x))]
    for dirpath, dirnames, filenames in os.walk(os.path.join(base, "docs")):
        dirnames[:] = [d for d in dirnames if d not in ("__pycache__",)]
        rel = os.path.relpath(dirpath, base).replace(os.sep, "/")
        if rel.startswith("docs/archive"):
            continue
        for fn in filenames:
            if fn.endswith(".md"):
                scan.append(os.path.join(dirpath, fn))
    scanned = 0
    for p in scan:
        if not os.path.isfile(p):
            continue
        text = read(p)
        scanned += 1
        rel = os.path.relpath(p, base).replace(os.sep, "/")
        if rel == DESIGN:
            continue
        # CI-003-B（2026-09-16）：豁免粒度由「整文件」改为「行级」。
        # 旧判据 "ARCHIVED_NON_NORMATIVE" in text 的触发条件是**提到**该字样而不是
        # **自称归档** ⇒ memory.md 只是在第 8/12/73 行说「别的东西已归档」就被整文件跳过，
        # 「memory.md 自称唯一最高权威」永远判不出来（假绿）；与 R-6 §3.5 的「空触发」
        # 同一失效型。现行判据：只有**该行自身**带非绑定标记才跳过该行。
        for i, line in enumerate(text.splitlines(), 1):
            if any(m in line for m in NON_BINDING_MARKERS):
                continue
            if not SELF_AUTHORITY_RE.search(line):
                continue
            # 唯一行内豁免：逐字点名现行最高设计 ASTROCS_DESIGN.md。
            # 「本文/本文件/本报告」等自指措辞不再豁免（CI-003-E；R-6 §3.6 实测该豁免
            # 吃掉了最常见的中文自称写法「本文档是唯一最高权威」）。
            if "ASTROCS_DESIGN.md" in line:
                continue
            v.append({"check": "single_authority_entry",
                      "detail": "%s:%d 以非最高权威文档自称唯一最高: %s"
                      % (rel, i, line.strip()[:120])})
    notes["unique_authority_scanned"] = scanned
    notes["unique_authority_scan_face"] = len(scan)
    if scanned == 0:
        v.append({"check": "unique_authority_scan_empty",
                  "detail": "唯一权威扫描面为空（%s 下既无 4 个根文档也无 docs/*.md）；"
                            "按 ENGINEERING_SPEC §8 fail-closed 判红" % root})


def run_check(root: str):
    """对 root 执行全部断言，返回 (violations, notes, checks)。CLI 与 --self-test 共用。"""
    root = os.path.abspath(root)
    violations: list = []
    notes: dict = {}
    checks: list = []

    apath = os.path.join(root, AGENTS)
    if not os.path.isfile(apath):
        violations.append({"check": "agents_exists", "detail": "缺 " + AGENTS})
        text = ""
    else:
        text = read(apath)

    # [1] 结构
    secs = {n: section(text, n) for n in (1, 5, 8)}
    for n, name in ((1, "开工前必读"), (5, "硬禁令"), (8, "停止点")):
        ok = bool(secs[n].strip())
        checks.append({"check": "section_%d_present" % n, "pass": ok, "detail": name})
        if not ok:
            violations.append({"check": "section_%d_present" % n,
                              "detail": "§%d %s 缺失或为空" % (n, name)})

    # [2] 硬禁令逐条抽取
    clause_list = bullets(secs[5])
    clauses = [c for c in clause_list if len(c) >= CLAUSE_MIN_CHARS]
    notes["hard_rule_count"] = len(clauses)
    if len(clauses) < FAMILY_MIN:
        violations.append({"check": "hard_rule_count",
                          "detail": "§5 硬禁令条数 %d < %d" % (len(clauses), FAMILY_MIN)})
    empties = [c for c in clause_list if len(c) < CLAUSE_MIN_CHARS]
    if empties:
        violations.append({"check": "hard_rule_nonempty",
                          "detail": "§5 存在空/占位禁令: %s" % empties[:3]})
    neg = [c for c in clauses if not NEG_FORM_RE.search(c)]
    if neg:
        violations.append({"check": "hard_rule_negative_form",
                          "detail": "§5 非否定式/硬约束式条款: %s" % [c[:40] for c in neg[:3]]})

    # [3] 语义族覆盖
    covered = []
    for name, pred in FAMILIES:
        hit = [c for c in clauses if pred(c)]
        if hit:
            covered.append(name)
        else:
            violations.append({"check": "family:" + name,
                              "detail": "§5 语义族「%s」在硬禁令中无对应条款" % name})
    notes["families_covered"] = covered

    # [3b] 语义族反转/松绑检测（CI-003-E）
    for c in clauses:
        for fam, pat, why in FAMILY_FORBIDDEN:
            if not FAMILY_PRED[fam](c):
                continue
            if not forbidden_hit(c, pat):
                continue
            violations.append({"check": "family_reversal:" + fam,
                              "detail": "§5 语义族「%s」被改写成反转/松绑措辞: %s —— %s"
                              % (fam, c[:70], why)})
            break

    # [4] 条款内上级权威引用合法性 + [6] 反回归（AGENTS.md 自身）
    for c in clauses:
        for ref in re.findall(r"`([^`]+)`", c):
            base = ref.split()[0].rstrip("/")
            if base.endswith((".md", ".json", ".yaml")) or "/" in base:
                if base not in CITED_ALLOWED and not base.startswith("docs/"):
                    violations.append({"check": "clause_authority_ref",
                                      "detail": "§5 条款引用未知权威: %s" % base})
    violations += violations_legacy_binding(text, AGENTS)

    # [5] 权威链
    check_authority_chain(root, violations, notes)
    # [6] 根权威面反回归（含根下被**重建**的旧治理对象本体——CI-003-E：旧版只扫描固定
    # 文件名，把已删对象同名重建到根下再写满约束性措辞也从不被读；对象名见 LEGACY_TOKENS）
    for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
                "memory.md", "HANDOVER.md", "eng/tools/check_agents_gov.py",
                "eng/tools/doccheck/check_engineering_constraints.py"):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            violations += violations_legacy_binding(read(p), rel)
    for name in sorted(os.listdir(root)):
        if name not in LEGACY_TOKENS or not name.endswith(".md"):
            continue
        p = os.path.join(root, name)
        if os.path.isfile(p):
            violations += violations_legacy_object_body(read(p), name)
    # [7] 唯一权威入口
    check_unique_authority(root, violations, notes)

    # 去重（同 check+detail 只报一次）
    seen = set()
    uniq = []
    for x in violations:
        x.setdefault("check", "legacy_authority_regression")
        x.setdefault("detail", "")
        key = (x["check"], x["detail"])
        if key in seen:
            continue
        seen.add(key)
        uniq.append(x)
    return uniq, notes, checks


# ---- 可执行负例面：--self-test（ENGINEERING_SPEC §8）------------------------
SELFTEST_AGENTS_MD = """# AGENTS.md（mini-repo 夹具）

## 1. 开工前必读

1. ASTROCS_DESIGN.md；

---

## 5. 硬禁令（违反即回退）

- **不动科学公式、默认容差、SCI/ALG 冻结定义**（除非最高设计/文档集变更流程批准）；
- **不串三阶段**：禁止把 normalize/mosaic/export 隐式串成一次运行；
- **不硬编码线程/ISA/block**：由 benchmark 生成的 profile 决定，禁止写死 workers；
- **不在 main 外开分支/worktree/额外 clone**；只 main 原子提交；
- **不 force push / amend / 历史重写**；
- **不把运行产物散落根目录**：一切输出落 output_dir 或 run/，不入库；
- **不宣布发布**：只有负责人可作最终发布决定；
- **不用 facade/空骨架/no-op 冒充实现完成**；
- **不以「环境问题/工具问题」掩盖失败**；必须给出可复现证据；
- **不复制文档长文进 AGENTS.md 或提交消息**；
- **不读取/打印密钥与凭据**（如 Fatduck 密钥只允许 -i 路径引用）。

---

## 8. 何时必须停下来问负责人

- 权限/数据/环境缺失导致任务无法推进；
"""

SELFTEST_DESIGN_MD = """# ASTROCS_DESIGN.md（mini-repo 夹具）

## 0. 权威链

本设计为项目最高权威，与其他文档冲突时以本文为准。

```mermaid
flowchart TD
    D["① 本文 最高设计"]
    A["② AGENTS.md 机器干活手册"]
    E["③ 工程规范 ENGINEERING_SPEC"]
    C["④ 控制包规范 CONTROL_PACK_SPEC"]
    CI["⑤ CI 规范 docs/ci/"]
    P["⑥ 插件文档 docs/plugins/"]
    D --> A & E & C & CI & P
```

---

## 1. 定位

夹具：仅用于 --self-test。
"""


# 根下被重建的已删除旧治理对象本体（负例：写成绑定权威 / 正例：标明历史参照）
SELFTEST_LEGACY_BINDING = """# AstroCS 工程约束

> 本文件为 AstroCS 工程约束权威，属上位必读的 FROZEN 依据。
"""

SELFTEST_LEGACY_ARCHIVED = """# AstroCS 工程约束（已删除，历史参照）

> 本文件不再作为现行权威，已归档保留作历史参照。
"""


def _write(path: str, text: str) -> None:
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def _edit(path: str, old: str, new: str) -> None:
    text = open(path, encoding="utf-8").read()
    if old not in text:
        raise AssertionError("self-test 夹具锚点缺失: %r" % old[:60])
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text.replace(old, new, 1))


def _mini_repo(base: str) -> None:
    """造一个 AGENTS-GOV 应 rc=0 的 mini-repo（含§1/§5/§8、权威链六步与 4 个根文档）。"""
    os.makedirs(os.path.join(base, "docs", "ci"), exist_ok=True)
    os.makedirs(os.path.join(base, "docs", "plugins"), exist_ok=True)
    _write(os.path.join(base, AGENTS), SELFTEST_AGENTS_MD)
    _write(os.path.join(base, DESIGN), SELFTEST_DESIGN_MD)
    for rel, text in (("README.md", "# README（mini-repo 夹具）\n\n- ASTROCS_DESIGN.md\n"),
                      ("ENGINEERING_SPEC.md", "# ENGINEERING_SPEC（mini-repo 夹具）\n"),
                      ("CONTROL_PACK_SPEC.md", "# CONTROL_PACK_SPEC（mini-repo 夹具）\n")):
        _write(os.path.join(base, rel), text)


def self_test() -> int:
    tmp = tempfile.mkdtemp(prefix="agents-gov-selftest-")
    results = []

    def case(name: str, expect: str, root: str, want=None) -> None:
        v, notes, _checks = run_check(root)
        codes = sorted({x["check"] for x in v})
        if expect == "pass":
            ok = not v
            detail = "violations=%d%s" % (len(v), "" if not v else " codes=" + ",".join(codes))
        else:
            ok = want in codes
            detail = "codes=" + (",".join(codes) or "(空)")
        results.append({"case": name, "expect": expect, "want": want,
                        "rc": 0 if not v else 1, "ok": ok, "detail": detail,
                        "unique_authority_scanned": notes.get("unique_authority_scanned"),
                        "families_covered": len(notes.get("families_covered") or [])})

    try:
        base = os.path.join(tmp, "base")
        _mini_repo(base)
        case("P1 mini-repo 基线（正例）", "pass", base)

        # P2：R-6 §3.5 三处同义改写（旧版全误红）——改后必须仍绿
        p2 = os.path.join(tmp, "p2-synonym")
        _mini_repo(p2)
        _edit(os.path.join(p2, AGENTS),
              "- **不读取/打印密钥与凭据**（如 Fatduck 密钥只允许 -i 路径引用）。",
              "- **禁止查看或输出任何凭据与密钥**；")
        _edit(os.path.join(p2, AGENTS),
              "- **不串三阶段**：禁止把 normalize/mosaic/export 隐式串成一次运行；",
              "- **三个命令必须保持相互独立**；")
        _edit(os.path.join(p2, AGENTS),
              "- **不用 facade/空骨架/no-op 冒充实现完成**；",
              "- **禁止以桩函数或占位实现冒充完成**；")
        case("P2 同义改写（密钥/三阶段/空骨架）仍绿", "pass", p2)

        # P3：禁止松绑的否定语境不误红（「不得解除冻结」「不可随意调整」）
        p3 = os.path.join(tmp, "p3-negated")
        _mini_repo(p3)
        _edit(os.path.join(p3, AGENTS),
              "- **不动科学公式、默认容差、SCI/ALG 冻结定义**（除非最高设计/文档集变更流程批准）；",
              "- **不得解除冻结**：科学公式与默认容差不得按需调整，也不可随意调整；")
        case("P3 否定语境（禁止松绑）不误红", "pass", p3)

        # N1：科学红线被改写成反转/松绑（R-6 §3.5 实测旧版 rc=0 的漏报）
        n1 = os.path.join(tmp, "n1-reversal")
        _mini_repo(n1)
        _edit(os.path.join(n1, AGENTS),
              "- **不动科学公式、默认容差、SCI/ALG 冻结定义**（除非最高设计/文档集变更流程批准）；",
              "- **科学公式与默认容差不再冻结，可按需调整**；")
        case("N1 §5 科学红线反转（漏报修复）", "fail", n1, "family_reversal:科学红线")

        # N2：语义族整条删除 ⇒ family 缺失仍红
        n2 = os.path.join(tmp, "n2-family-gone")
        _mini_repo(n2)
        _edit(os.path.join(n2, AGENTS),
              "- **不读取/打印密钥与凭据**（如 Fatduck 密钥只允许 -i 路径引用）。\n",
              "")
        case("N2 §5 语义族「禁读密钥」整条删除", "fail", n2, "family:禁读密钥")

        # N3：「本文档」自称唯一最高（R-6 §3.6 实测被「本文」豁免漏掉的措辞）
        n3 = os.path.join(tmp, "n3-self-authority")
        _mini_repo(n3)
        _write(os.path.join(n3, "docs", "NOTE.md"),
               "# 探针\n\n本文档是仓库唯一最高权威，其余文档均从属。\n")
        case("N3 「本文档」自称唯一最高（继承漏报修复）", "fail", n3,
             "single_authority_entry")

        # N4：旧权威回归
        n4 = os.path.join(tmp, "n4-legacy")
        _mini_repo(n4)
        _edit(os.path.join(n4, "README.md"), "# README（mini-repo 夹具）\n",
              "# README（mini-repo 夹具）\n\n> 根 ASTROCS_PROJECT_CONSTITUTION.md 是"
              "最高权威与必读上位依据。\n")
        case("N4 旧权威重新作为上位权威出现", "fail", n4, "legacy_authority_regression")

        # N5：扫描面为空 ⇒ fail-closed 判红（ENGINEERING_SPEC §8）
        n5 = os.path.join(tmp, "n5-empty-face")
        _mini_repo(n5)
        for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md"):
            os.remove(os.path.join(n5, rel))
        case("N5 唯一权威扫描面为空 ⇒ fail-closed", "fail", n5,
             "unique_authority_scan_empty")

        # N6：已删除的旧治理对象被同名重建到根，本体写成绑定权威（R-6 §3.6 实测旧版 rc=0）
        n6 = os.path.join(tmp, "n6-legacy-object")
        _mini_repo(n6)
        _write(os.path.join(n6, "AstroCS_ENGINEERING_CONSTRAINTS.md"), SELFTEST_LEGACY_BINDING)
        case("N6 已删旧治理对象同名重建到根（本体写成绑定权威）", "fail", n6,
             "legacy_authority_regression")

        # P4：同名重建但正文标明「已删除/历史参照」⇒ 不误红
        p4 = os.path.join(tmp, "p4-legacy-archived")
        _mini_repo(p4)
        _write(os.path.join(p4, "AstroCS_ENGINEERING_CONSTRAINTS.md"), SELFTEST_LEGACY_ARCHIVED)
        case("P4 同名重建但正文标明历史参照（正例）", "pass", p4)

        ok = all(r["ok"] for r in results)
        report = {"tool": "eng/tools/check_agents_gov.py", "mode": "self-test",
                  "positive_cases": sum(1 for r in results if r["expect"] == "pass"),
                  "negative_cases": sum(1 for r in results if r["expect"] == "fail"),
                  "all_pass": ok, "results": results}
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main() -> int:
    ap = argparse.ArgumentParser(description="AGENTS.md / 权威链语义检查器（GOV-001）")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--self-test", action="store_true",
                    help="跑内置 mini-repo 正/负例（可执行负例面，ENGINEERING_SPEC §8）")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root)
    uniq, notes, checks = run_check(root)
    out = {
        "tool": "eng/tools/check_agents_gov.py",
        "version": "2.1.0",
        "task": "GOV-001",
        "root": root,
        "checks": checks,
        "notes": notes,
        "violations": uniq,
        "verdict": "GOV_CHECK_PASS" if not uniq else "GOV_CHECK_FAIL",
    }
    text_out = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text_out + chr(10))
    if not args.quiet:
        print(text_out)
    return 0 if not uniq else 1


if __name__ == "__main__":
    sys.exit(main())
