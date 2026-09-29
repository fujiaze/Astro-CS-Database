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
    "### 3.5 x" 会被读成节号 "3"，"docs/X.md §3.5" 整条锚不匹配。
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
  2. 每个 sec 归属到**左侧最近的文档类 token**（astrocs / file / caps）：
     - 左侧找不到 ⇒ 右侧就近找一次（覆盖「§3.5 见 docs/ASTROCS_DESIGN.md」倒装）；
     - 归属结果是 astrocs ⇒ 进入 S3 判据；是 file / caps ⇒ 不是本门判据
       （例如 "GAP_AUDIT(RELEASE-02) §9.74" 里的 §9.74 属那份不存在的文档，
       不由本门判，但**照样打印**，供人工处置）。
  3. 链式引用（§3.5/§6.3、§8.3/§9/§3.5、§6.2（…）、§3.5）逐 token 各判各的：
     sec 之间不算边界。

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
CHECK_ID = "DOC-DESIGN-SECTION-REFS"
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
MAX_BYTES = 4 * 1024 * 1024  # 超大文件跳过并计数（fail-closed 靠 S5 兜底）

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


def attribute(line):
    """把行内每个 sec token 归属到最近的文档类 token。

    返回 [(sec_text, sec_num, owner_kind, owner_text, col)]；owner 为 None 表示
    行内左右都没有文档 token（**裸 §**）。裸 § 一律**不判**，只进 S3b 报告面：
    实测仓内「文件头声明权威、��文写裸 §」与「本文档自己的 §3.5」两种形态在
    同一行面上不可机械区分（check_agents_gov.py、实验/** 的裸 § 指各自文档），
    按「本文件提过 ASTROCS_DESIGN」兜底归属会一次制造 1400+ 假红 —— 那是判据
    缺陷不是缺陷本身。故只登记、只打印，判定留给人工。
    """
    toks = []
    for m in TOKEN_RE.finditer(line):
        if m.group("sec") is not None:
            toks.append(("sec", m.group("sec"), m.start("sec"), m.group("secnum")))
            continue
        for kind in ("astrocs", "file", "caps"):
            if m.group(kind) is not None:
                toks.append((kind, m.group(kind), m.start(kind), None))
                break
    docs = [t for t in toks if t[0] in ("astrocs", "file", "caps")]
    out = []
    for kind, text, start, num in toks:
        if kind != "sec":
            continue
        left = [d for d in docs if d[2] < start]
        right = [d for d in docs if d[2] > start]
        owner = left[-1] if left else (right[0] if right else None)
        out.append((text, num, owner[0] if owner else None,
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
                if owner == "astrocs":
                    report["refs_checked"] += 1
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
                elif owner in ("file", "caps") and not is_evidence(rel):
                    # 不判红但打印：该 sec 属另一份文档 / 标识符；若那份文档不存在，
                    # 属另一类悬空引用（人工处置面）。
                    report["foreign_section_refs"].append(item)
                elif owner is None and mentions and not is_evidence(rel):
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
        # N3 链式引用逐 token 判：\u00a78.3/\u00a79 \u6d3b\u3001\u00a73.5 \u6b7b ⇒ \u6070\u597d 1 \u6761\u7ea2
        rep = run("N3_chain", base,
                  {"lib/x.cpp": "// docs/ASTROCS_DESIGN.md \u00a78.3/\u00a79/\u00a73.5"},
                  1, ("SECTION_REF_MISSING",))
        assert len(rep["violations"]) == 1, \
            "链式引用应恰好 1 条红，实得 %d" % len(rep["violations"])
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
