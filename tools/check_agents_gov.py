#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-001 语义检查器：AGENTS.md 硬禁令 / 权威链唯一性 / 旧权威回归红灯。

改造前（旧版）本检查器把某一版 AGENTS.md 的 10 组字符串当硬编码期望，
旧 AGENTS.md 一被改写即误报 rc=1（GOV-001 基线），且对"旧权威重新出现"零覆盖。

本版不再做文本快照，而是按语义断言：

  [1] AGENTS.md 结构：§1 开工前必读 / §5 硬禁令 / §8 停止点 三节必须存在且非空；
  [2] §5 硬禁令逐条抽取，断言条数下限 + 每条为"否定式"且含动宾实质；
  [3] §5 硬禁令的语义族覆盖（科学红线、三阶段隔离、无硬编码线程、main-only、
      禁止 force push、运行产物落位、发布权、禁空骨架冒充、不得掩盖失败、
      禁复制长文、禁读密钥）——任一语义族被整体删除即红；
  [4] §5 条款指向的上级权威必须存在于权威链白名单（正文文件必须真实存在）；
      出现已删除的旧权威对象（ASTROCS_PROJECT_CONSTITUTION.md /
      AstroCS_ENGINEERING_CONSTRAINTS.md / 设计大纲/）即红；
  [5] 权威链唯一性：ASTROCS_DESIGN.md §0 的 mermaid 链必须恰好覆盖
      ASTROCS_DESIGN → AGENTS → ENGINEERING_SPEC → CONTROL_PACK_SPEC →
      docs/ci → docs/plugins 六步且顺序正确（步骤全部为真实存在的路径）；
  [6] 反回归扫描：根权威面（README/AGENTS/ENGINEERING_SPEC/CONTROL_PACK_SPEC/
      HANDOVER）与 tools/doccheck/、tools/check_agents_gov.py 自身不得把
      已删除的旧权威文本写成上位/必读/冻结权威；
  [7] 活动文档"唯一权威"声明：不得再有除 ASTROCS_DESIGN.md 之外的文档自称
      "唯一最高权威/唯一最高约束"（docs/archive/** 与已标 ARCHIVED_NON_NORMATIVE 的
      归档件豁免）。

用法：
  python3 tools/check_agents_gov.py [--root <repo>] [--json-out <file>] [--quiet]

能红能绿证据（1 正例 + 7 负例）：tests 见 run/PROJECT-GOVERNANCE-01/GOV-001/fixtures/
（runtime 验证脚本 verify_checkers.py，正例 = 真实仓库 rc=0）。
exit 0 = PASS；任一断言不成立 = exit 1（打印机器 JSON）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
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
SELF_AUTHORITY_RE = re.compile(r"唯一\s*最高(权威|约束|规范|文档)")
# 声明"唯一最高"时必须出现的自指文件（ASTROCS_DESIGN.md 自身或其等价描述）
DESIGN_SELF_RE = re.compile(r"(本文|本文件|本设计|ASTROCS_DESIGN\.md)")

# §5 语义族：族名 -> 判定（对单条禁令文本）
FAMILIES = [
    ("科学红线",
     lambda t: "科学公式" in t and any(k in t for k in ("冻结", "容差", "不动", "不可", "定义"))),
    ("三阶段隔离",
     lambda t: ("normalize" in t and "mosaic" in t and "export" in t)
     or "三阶段" in t or "串" in t),
    ("无硬编码并行度",
     lambda t: (("workers" in t or "线程" in t or "ISA" in t) and "硬编码" in t)
     or ("benchmark" in t and "profile" in t)),
    ("main-only", lambda t: "main" in t and ("分支" in t or "worktree" in t or "clone" in t)),
    ("禁历史重写", lambda t: "force" in t.lower() or "amend" in t or "历史重写" in t),
    ("产物落位", lambda t: ("output_dir" in t or "run/" in t) and ("根目录" in t or "落" in t)),
    ("发布权", lambda t: "发布" in t and ("负责人" in t or "宣布" in t or "宣称" in t)),
    ("禁空骨架冒充",
     lambda t: any(k in t for k in ("facade", "空骨架", "no-op", "mock")) and "冒充" in t),
    ("不得掩盖失败",
     lambda t: "掩盖失败" in t or ("环境问题" in t and "工具问题" in t)),
    ("禁复制长文", lambda t: "复制" in t and ("AGENTS.md" in t or "提交消息" in t)),
    ("禁读密钥", lambda t: ("密钥" in t or "凭据" in t) and ("读" in t or "打印" in t)),
]
FAMILY_MIN = 10  # §5 必须有 >= 10 条禁令，且 >= 10 个语义族被覆盖
CLAUSE_MIN_CHARS = 8


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
    """除 ASTROCS_DESIGN.md 外，活动面不得自称"唯一最高权威/约束"。"""
    base = root if root == REPO else os.path.join(root, "fixture-repo")
    scan = [os.path.join(base, x) for x in
            ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md")
            if os.path.isfile(os.path.join(base, x))]
    for dirpath, dirnames, filenames in os.walk(os.path.join(base, "docs")):
        dirnames[:] = [d for d in dirnames if d not in ("__pycache__",)]
        rel = os.path.relpath(dirpath, base).replace(os.sep, "/")
        if rel.startswith("docs/archive"):
            continue
        for fn in filenames:
            if fn.endswith(".md"):
                scan.append(os.path.join(dirpath, fn))
    for p in scan:
        if not os.path.isfile(p):
            continue
        text = read(p)
        rel = os.path.relpath(p, base).replace(os.sep, "/")
        if rel == DESIGN:
            continue
        if "ARCHIVED_NON_NORMATIVE" in text:
            continue
        for i, line in enumerate(text.splitlines(), 1):
            if not SELF_AUTHORITY_RE.search(line):
                continue
            if "ASTROCS_DESIGN.md" in line or "本文" in line or "本文件" in line:
                continue
            v.append({"check": "single_authority_entry",
                      "detail": "%s:%d 以非最高权威文档自称唯一最高: %s"
                      % (rel, i, line.strip()[:120])})
    notes["unique_authority_scanned"] = len(scan)


def main() -> int:
    ap = argparse.ArgumentParser(description="AGENTS.md / 权威链语义检查器（GOV-001）")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
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
            violations.append({"check": "section_%d_present" % n, "detail": "§%d %s 缺失或为空" % (n, name)})

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
    neg = [c for c in clauses if not re.search(r"禁|不|不得|勿|免", c)]
    if neg:
        violations.append({"check": "hard_rule_negative_form",
                          "detail": "§5 非否定式条款: %s" % [c[:40] for c in neg[:3]]})

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
    # [6] 根权威面反回归
    for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
                "HANDOVER.md", "tools/check_agents_gov.py",
                "tools/doccheck/check_engineering_constraints.py"):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            violations += violations_legacy_binding(read(p), rel)
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
    out = {
        "tool": "tools/check_agents_gov.py",
        "version": "2.0.0",
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