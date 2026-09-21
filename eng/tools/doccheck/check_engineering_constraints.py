#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-001 机器检查器：以 `ENGINEERING_SPEC.md` 为权威对象的工程约束门。

改造前（旧版）本检查器把仓库根旧文本 `AstroCS_ENGINEERING_CONSTRAINTS.md`
（已由 ROOT-007 删除）当权威对象，因此对当前树结构性 rc=1（GOV-001 基线）。
本版改以 `ENGINEERING_SPEC.md`（权威链第 3 级）为唯一权威对象：

  [1] `ENGINEERING_SPEC.md` 在位、受 Git 跟踪、含 §2/§3/§4/§5/§6/§7/§8 七节；
  [2] §7 根白名单可机械抽取：固定条目 + 其他固定目录 + gitignore 目录；
  [3] §7 要求存在的根条目必须齐备（缺一即红）；
  [4] §7 之外的顶层条目必须已登记（`eng/ci/root_manifest.json` 的
      registered_local_retention / ignored_patterns），未登记即红；
      -> 新增根条目须"先登记并经负责人确认"（§7 原文）由本项机器化；
  [5] 登记台账只减不增：`eng/ci/root_manifest.json` 的 allowed_files/allowed_dirs
      不得比 §7 更宽（逐条子集断言），且登记路径必须真实存在（禁悬空登记）。
      CI-003-E 收紧：悬空登记的唯一豁免 = **显式声明的归档迁移**
      （同名 + 声明的目标真实存在 + 若声明 sha256 则内容哈希必须一致）；
      旧判据「docs/archive 或 run/archive 下有同名文件即豁免」过宽（R-6 §3.6）；
  [6] `README.md` 权威入口：必须指向现行文档集（ASTROCS_DESIGN / AGENTS /
      ENGINEERING_SPEC / CONTROL_PACK_SPEC / docs/ci / docs/plugins /
      docs/design/UNIFIED_MODEL），且不得把已删除的旧治理对象写成上位权威；
  [7] 旧权威回归红灯：根权威面 + 工具面出现「旧文本 + 权威/必读/冻结」绑定即红；
  [8] 唯一权威入口：除 `ASTROCS_DESIGN.md` 的 §0 之外，活动文档不得自称
      "唯一最高权威/约束"（`docs/archive/**` 与带 ARCHIVED_NON_NORMATIVE 者豁免；
      **唯一行内豁免 = 逐字点名 ASTROCS_DESIGN.md**——「本文/本文件/本文档」等自指
      措辞不再豁免，R-6 §3.6 实测该豁免吃掉了最常见的中文自称写法）。
      扫描面读不到任何文件时 fail-closed 判红 unique_authority_scan_empty（§8）。

用法：
  python3 eng/tools/doccheck/check_engineering_constraints.py [--root <repo>]
      [--json-out <file>] [--quiet]
  python3 eng/tools/doccheck/check_engineering_constraints.py --self-test

--self-test（ENGINEERING_SPEC §8「可执行负例面」）：在 tempfile 造的 mini-repo 上跑
3 正例 + 8 负例（未登记根条目 / 台账比 §7 更宽 / 悬空登记 / 同名异内容不豁免 /
「本文档」自称唯一最高 / 旧权威对象同名重建 / 扫描面为空 fail-closed），
全部符合预期则自身 exit 0。
能红能绿历史证据（1 正例 + 8 负例）：run/PROJECT-GOVERNANCE-01/GOV-001/fixtures/
（runtime 验证脚本 verify_checkers.py；正例 = 真实仓库 rc=0）。
exit 0 = PASS；任一断言不成立 = exit 1（打印机器 JSON）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
SPEC = "ENGINEERING_SPEC.md"
MANIFEST = "eng/ci/root_manifest.json"

REQUIRED_SECTIONS = {
    2: "代码风格（注释/文档纪律）",
    3: "科学代码红线",
    4: "模块规范",
    5: "测试规范",
    6: "Git 与提交",
    7: "目录规范（根白名单）",
    8: "机器一致性检查（能红能绿）",
}
README_AUTHORITY = [
    "ASTROCS_DESIGN.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
    "docs/ci/", "docs/plugins/", "docs/design/UNIFIED_MODEL.md",
]
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


def read(path: str) -> str:
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def section(text: str, num: int) -> str:
    m = re.search(r"^##\s*%d\.[^\n]*\n(.*?)(?=^##\s|\Z)" % num, text, re.S | re.M)
    return m.group(1) if m else ""


def git_tracked(root: str, rel: str) -> bool:
    r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                       cwd=root, capture_output=True, text=True)
    return r.returncode == 0


def spec_whitelist(sec7: str):
    """从 §7 代码块机械抽取 (顶层文件, 顶层目录, gitignore 目录)。"""
    m = re.search(r"```text\n(.*?)```", sec7, re.S)
    body = m.group(1) if m else sec7
    files, dirs, ignored = set(), set(), set()
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith(("lib/", "├", "│", "└")):
            continue
        if line.startswith("其他固定目录："):
            # 该行按空白列出目录；中文词是说明性标签（报告/证据/打包/工程），只取 ASCII 目录名
            for tok in line[len("其他固定目录："):].split():
                tok = tok.strip().rstrip("/")
                if re.fullmatch(r"[A-Za-z0-9_.\-]+", tok):
                    dirs.add(tok)
            continue
        # 行内中文说明/编号引用（如"（程序根全局配置：…）"、"; ASTROCS_DESIGN.md §3.3"）不是条目
        line = re.split(r"[（;；]", line, maxsplit=1)[0]
        for tok in re.split(r"[\s/]+", line):
            tok = tok.strip()
            if not tok:
                continue
            if "." in tok:
                files.add(tok)
            elif re.fullmatch(r"[A-Za-z0-9_\-]+", tok):
                dirs.add(tok)
    # §7 正文点名："run/（gitignore：临时产物/日志） logs/（gitignore）"
    for name in re.findall(r"([A-Za-z0-9_\-]+)/（[^）]*gitignore", sec7):
        ignored.add(name)
    # §7 正文点名的条目：lib/（树形图目标源码根）、工程控制/（控制包落点）、
    # reports/ 的中文标签"报告"、artifacts/ 的中文标签"证据"、packaging/ 的"打包"、
    # engineering/ 的"工程"——中文标签是说明不是路径，不登记。
    if re.search(r"^lib/", sec7, re.M):
        dirs.add("lib")
    # CJK 目录名（工程控制/、实验/）不匹配上面的 ASCII token 正则，必须显式登记；
    # 这两个目录是 §7 代码块内逐字点名的固定条目，显式登记不是放宽判据。
    # 2026-09-21 ROOT-CONSOLIDATION：experiments 侧的 reverse_verify/ 根条目已解散，
    # 其内容迁入 实验/，故 实验/ 的显式登记与原先对 reverse_verify 的登记等价。
    dirs |= {"run", "logs", "工程控制", "实验"}
    dirs -= ignored
    return files, dirs, ignored


def legacy_hits(text: str, path: str) -> list:
    bad = []
    for i, line in enumerate(text.splitlines(), 1):
        if not any(t in line for t in LEGACY_TOKENS):
            continue
        if any(m in line for m in NON_BINDING_MARKERS):
            continue
        if any(m in line for m in BINDING_MARKERS):
            bad.append({"file": path, "line": i, "text": line.strip()[:160]})
    return bad


def legacy_object_body_hits(text: str, path: str) -> list:
    """根下被**重建**的已删除旧治理对象（文件名即旧对象名）：本体正文出现绑定措辞即红。

    legacy_hits 要求**行内出现旧对象名**（用于扫描别的文档对旧对象的引用）；重建对象的
    本体不必重复自己的文件名——R-6 §3.6 实测「本文件为 … 权威，属上位必读的 FROZEN
    依据」旧版 rc=0（内容根本没被读）。带 ARCHIVED/已删除/历史参照等标记的行豁免。
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


def sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def relocated(entry: dict, root: str):
    """登记路径在根下不存在时，是否属于**已显式声明的归档迁移**。

    CI-003-E（2026-09-16）收紧。旧判据「`docs/archive` 或 `run/archive` 下有同名文件
    即豁免」过宽：任何同名文件——哪怕内容毫不相关——都能让悬空登记**永不判红**
    （R-6 §3.6 实测：`registered_local_retention` 里的 `HANDOVER.md` 就是靠
    `docs/archive/HANDOVER.md` 这一条同名关系免红的）。

    现行判据（fail-closed，ENGINEERING_SPEC §8「不得把『文件不存在』当『无违规』」）：
      1) 迁移目标必须被**显式声明**：条目字段 `relocated_to`/`archived_to`，或条目
         `note` 内逐字点名 `docs/archive/<basename>` / `run/archive/<basename>`；
         同名（basename 一致）是硬条件——"恰好有个同名文件"不再豁免；
      2) 声明的目标必须是真实存在的**普通文件**；
      3) 条目若声明了 `sha256`/`relocated_sha256`，归档件内容哈希必须与之一致
         （「同名」必须升级为「同一内容」；不一致 ⇒ 仍判悬空）。
    残余边界（如实登记）：条目只声明路径、未声明 sha256 时，本判据无法发现"归档件被
    替换成同名异内容"；彻底关闭需要登记面携带 sha256，而 `eng/ci/root_manifest.json`
    不在本任务文件域。

    返回 (是否豁免, 原因串)。
    """
    p = str(entry.get("path") or "")
    base = os.path.basename(p)
    if not base:
        return False, "登记条目缺 path"
    declared = []
    for key in ("relocated_to", "archived_to"):
        v = entry.get(key)
        if isinstance(v, str) and v.strip():
            declared.append(v.strip().replace("\\", "/").lstrip("./"))
    note = str(entry.get("note") or "")
    for cand in ("docs/archive/" + base, "run/archive/" + base):
        if cand in note:
            declared.append(cand)
    want = entry.get("sha256") or entry.get("relocated_sha256")
    for cand in declared:
        if os.path.basename(cand) != base:
            continue
        full = os.path.join(root, cand)
        if not os.path.isfile(full):
            continue
        if want:
            got = sha256_file(full)
            if got.lower() != str(want).lower():
                return False, "声明的 sha256 不一致: %s" % cand
        return True, "显式声明且存在: %s" % cand
    return False, "无显式归档声明（仅同名不算）: %s" % base


def run_check(root: str):
    """对 root 执行全部断言，返回 (violations, notes, checks)。CLI 与 --self-test 共用。"""
    root = os.path.abspath(root)
    checks, violations, notes = [], [], {}

    spec_path = os.path.join(root, SPEC)
    if not os.path.isfile(spec_path):
        violations.append({"check": "spec_exists", "detail": "缺 " + SPEC})
        spec = ""
    else:
        spec = read(spec_path)
        # 夹具树（非 git 工作树）时"受 Git 跟踪"结构性不可判——真实仓库必须判。
        if os.path.isdir(os.path.join(root, ".git")):
            tracked = git_tracked(root, SPEC)
            checks.append({"check": "spec_git_tracked", "pass": tracked, "detail": SPEC})
            if not tracked:
                violations.append({"check": "spec_git_tracked", "detail": SPEC + " 未被 git 跟踪"})

    secs = {n: section(spec, n) for n in REQUIRED_SECTIONS}
    for n, name in REQUIRED_SECTIONS.items():
        ok = bool(secs[n].strip())
        checks.append({"check": "spec_section_%d" % n, "pass": ok, "detail": name})
        if not ok:
            violations.append({"check": "spec_section_%d" % n,
                              "detail": "§%d %s 缺失或为空" % (n, name)})

    files, dirs, ignored = spec_whitelist(secs[7])
    notes["spec7_files"] = sorted(files)
    notes["spec7_dirs"] = sorted(dirs)
    notes["spec7_gitignored"] = sorted(ignored)
    if not files or not dirs:
        violations.append({"check": "spec7_parse", "detail": "§7 白名单无法机械抽取"})

    present = sorted(os.listdir(root))
    notes["top_level_entries"] = len(present)

    man_path = os.path.join(root, MANIFEST)
    man = {}
    if os.path.isfile(man_path):
        man = json.loads(read(man_path))
    else:
        violations.append({"check": "manifest_exists", "detail": "缺 " + MANIFEST})

    # [3] §7 要求存在的条目
    missing_required = [n for n in sorted(files) if n not in present]
    missing_dirs = [n for n in sorted(dirs) if n not in present or not os.path.isdir(os.path.join(root, n))]
    if missing_required:
        violations.append({"check": "spec7_required_files_present",
                          "detail": "§7 要求存在但缺失的文件: %s" % ",".join(missing_required)})
    if missing_dirs:
        violations.append({"check": "spec7_required_dirs_present",
                          "detail": "§7 要求存在但缺失的目录: %s" % ",".join(missing_dirs)})

    # [4] 未登记条目
    entries = man.get("registered_local_retention", []) or []
    registered = {e.get("path") for e in entries}
    tolerated = set(man.get("ignored_patterns", []) or [])
    implicit = set(man.get("implicit_entries", []) or [])
    allowed = set(man.get("allowed_files", []) or []) | set(man.get("allowed_dirs", []) or [])
    pending_owner = set(man.get("gov001_pending_owner_root_entries", []) or [])
    unregistered = []
    for name in present:
        if name in allowed or name in implicit or name in ignored or name in tolerated:
            continue
        if any(re.fullmatch(p.replace("*", ".*"), name) for p in
               man.get("runtime_product_patterns", []) or []):
            unregistered.append((name, "runtime_product_at_root"))
            continue
        if name in registered or name in pending_owner:
            continue
        unregistered.append((name, "unregistered_root_entry"))
    for name, why in unregistered:
        violations.append({"check": why,
                          "detail": "顶层条目 %s 不在 §7 白名单且未登记（§7：新根条目须先登记并经负责人确认）" % name})

    # [5] 台账只减不增 + 登记不悬空
    man_extra = set(man.get("gov001_pending_owner_root_entries", []) or [])
    wider = sorted(allowed - (files | dirs | ignored | man_extra))
    if wider:
        violations.append({"check": "manifest_not_wider_than_spec7",
                          "detail": "eng/ci/root_manifest.json 白名单比 §7 更宽: %s" % ",".join(wider)})

    dangling, relocation = [], []
    for e in entries:
        p = e.get("path")
        if not p or p in present:
            continue
        ok, why = relocated(e, root)
        relocation.append({"path": p, "relocated": ok, "reason": why})
        if not ok:
            dangling.append(p)
    dangling = sorted(set(dangling))
    if dangling:
        violations.append({"check": "manifest_no_dangling_registration",
                          "detail": "登记台账悬空（路径不存在且无显式归档声明）: %s" % ",".join(dangling)})
    notes["registered_pending_owner"] = sorted(x for x in registered if x)
    notes["registered_relocation"] = relocation

    # [6] README 权威入口
    rp = os.path.join(root, "README.md")
    if not os.path.isfile(rp):
        violations.append({"check": "readme_exists", "detail": "缺 README.md"})
    else:
        rtext = read(rp)
        missing_entry = [p for p in README_AUTHORITY if p not in rtext]
        if missing_entry:
            violations.append({"check": "readme_authority_entries",
                              "detail": "README 权威入口缺: %s" % ",".join(missing_entry)})
        for p in README_AUTHORITY:
            fp = os.path.join(root, p)
            if not os.path.exists(fp):
                violations.append({"check": "readme_authority_path_exists",
                                  "detail": "README 权威入口指向不存在的路径: %s" % p})
        violations += legacy_hits(rtext, "README.md")

    # [7] 旧权威回归红灯（根权威面 + 工具面 + 根下被重建的旧权威对象本体）
    for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
                "memory.md", "ASTROCS_DESIGN.md", "eng/tools/check_agents_gov.py",
                "eng/tools/doccheck/check_engineering_constraints.py"):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            violations += legacy_hits(read(p), rel)
    # CI-003-E：根下若被**重建**了已删除的旧治理对象（对象名见 LEGACY_TOKENS），其本体
    # 也必须被读。R-6 §3.6 实测：把已删对象同名重建到根并登记进 ignored_patterns ⇒
    # 旧版 rc=0（该文件从不被任何判据读取）。
    for name in sorted(os.listdir(root)):
        if name not in LEGACY_TOKENS or not name.endswith(".md"):
            continue
        p = os.path.join(root, name)
        if os.path.isfile(p):
            violations += legacy_object_body_hits(read(p), name)

    # [8] 唯一权威入口
    # CI-003（2026-09-16）：memory.md 纳入唯一权威扫描面（与 AGENTS-GOV [7] 同步）。
    scan = ["README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
            "memory.md"]
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, "docs")):
        dirnames[:] = [d for d in dirnames if d != "__pycache__"]
        rel = os.path.relpath(dirpath, root).replace(os.sep, "/")
        if rel.startswith("docs/archive"):
            continue
        for fn in filenames:
            if fn.endswith(".md"):
                scan.append(os.path.join(rel, fn))
    scanned = 0
    for rel in scan:
        p = os.path.join(root, rel)
        if not os.path.isfile(p):
            continue
        t = read(p)
        scanned += 1
        if rel == "ASTROCS_DESIGN.md":
            continue
        # CI-003-B（2026-09-16）：豁免粒度由「整文件」改为「行级」（与 AGENTS-GOV 同步）。
        # 旧判据只看文件内**是否提到** ARCHIVED_NON_NORMATIVE，不看是否**自称归档**
        # ⇒ memory.md 的「自称唯一最高权威」被整文件豁免吃掉（假绿）。
        for i, line in enumerate(t.splitlines(), 1):
            if any(m in line for m in NON_BINDING_MARKERS):
                continue
            # 唯一行内豁免：逐字点名现行最高设计 ASTROCS_DESIGN.md（CI-003-E；
            # 「本文/本文件/本文档/本报告」等自指措辞不再豁免）。
            if SELF_AUTHORITY_RE.search(line) and "ASTROCS_DESIGN.md" not in line:
                violations.append({"check": "single_authority_entry",
                                  "detail": "%s:%d 以非最高权威文档自称唯一最高: %s"
                                  % (rel, i, line.strip()[:120])})
    notes["unique_authority_scanned"] = scanned
    notes["unique_authority_scan_face"] = len(scan)
    if scanned == 0:
        violations.append({"check": "unique_authority_scan_empty",
                          "detail": "唯一权威扫描面为空（%s 下既无根文档也无 docs/*.md）；"
                                    "按 ENGINEERING_SPEC §8 fail-closed 判红" % root})

    seen, uniq = set(), []
    for x in violations:
        x.setdefault("check", "constraints_violation")
        x.setdefault("detail", "")
        key = (x["check"], x["detail"])
        if key in seen:
            continue
        seen.add(key)
        uniq.append(x)
    return uniq, notes, checks


# ---- 可执行负例面：--self-test（ENGINEERING_SPEC §8）------------------------
SELFTEST_SPEC = """# ENGINEERING_SPEC（mini-repo 夹具）

## 2. 代码风格

- 遵循 .clang-format 与 .editorconfig。

## 3. 科学代码红线（最高优先级）

- 科学公式与默认容差不得随意修改。

## 4. 模块规范

- 每模块必备 README.md 与 module.yaml。

## 5. 测试规范

- 每模块必备单测、合同测试与负例。

## 6. Git 与提交

- 只 main 开发；禁止分支/worktree/额外 clone。

## 7. 目录规范（强制）

```text
仓库根固定条目：
README.md / AGENTS.md / ASTROCS_DESIGN.md / ENGINEERING_SPEC.md /
CONTROL_PACK_SPEC.md / memory.md / CMakeLists.txt

lib/
├── algorithms/
└── infrastructure/

其他固定目录：docs/ eng/tools/ eng/ci/
run/（gitignore：临时产物/日志）  logs/（gitignore）
```

## 8. 机器一致性检查

- 每项检查须有正例与负例（能红能绿）。
"""

SELFTEST_README = """# README（mini-repo 夹具）

权威入口（现行文档集）：

- ASTROCS_DESIGN.md（最高设计）
- AGENTS.md（干活纪律）
- ENGINEERING_SPEC.md（工程规范）
- CONTROL_PACK_SPEC.md（控制包规范）
- docs/ci/（CI 规范）
- docs/plugins/（插件文档）
- docs/design/UNIFIED_MODEL.md（统一数据模型）
"""

SELFTEST_FILES = ["README.md", "AGENTS.md", "ASTROCS_DESIGN.md", "ENGINEERING_SPEC.md",
                  "CONTROL_PACK_SPEC.md", "memory.md", "CMakeLists.txt"]
SELFTEST_DIRS = ["docs", "tools", "ci", "工程控制", "lib"]


def selftest_manifest(allowed_files=None, allowed_dirs=None, retention=None,
                      ignored_patterns=None) -> dict:
    return {
        "schema_version": 1,
        "spec": "ENGINEERING_SPEC.md §7（mini-repo 夹具）",
        "allowed_files": list(SELFTEST_FILES) if allowed_files is None else allowed_files,
        "allowed_dirs": list(SELFTEST_DIRS) if allowed_dirs is None else allowed_dirs,
        "implicit_entries": [".git"],
        "runtime_product_patterns": ["astrocs_run_*.json"],
        "ignored_patterns": list(ignored_patterns or []),
        "registered_local_retention": retention or [],
        "gov001_pending_owner_root_entries": [],
    }


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


def _mini_repo(base: str, manifest: dict = None) -> None:
    """造一个 ENG-CONSTRAINTS 应 rc=0 的 mini-repo（§7 白名单 + 台账 + README 权威入口）。"""
    for d in SELFTEST_DIRS:
        os.makedirs(os.path.join(base, d), exist_ok=True)
    os.makedirs(os.path.join(base, "docs", "ci"), exist_ok=True)
    os.makedirs(os.path.join(base, "docs", "plugins"), exist_ok=True)
    _write(os.path.join(base, SPEC), SELFTEST_SPEC)
    _write(os.path.join(base, "README.md"), SELFTEST_README)
    _write(os.path.join(base, "AGENTS.md"), "# AGENTS（mini-repo 夹具）\n\n- 只 main 开发。\n")
    _write(os.path.join(base, "ASTROCS_DESIGN.md"), "# ASTROCS_DESIGN（mini-repo 夹具）\n")
    _write(os.path.join(base, "CONTROL_PACK_SPEC.md"), "# CONTROL_PACK_SPEC（mini-repo 夹具）\n")
    _write(os.path.join(base, "memory.md"), "# memory（mini-repo 夹具）\n")
    _write(os.path.join(base, "CMakeLists.txt"), "# mini-repo 夹具\n")
    _write(os.path.join(base, "docs", "design", "UNIFIED_MODEL.md"),
           "# UNIFIED_MODEL（mini-repo 夹具）\n")
    _write(os.path.join(base, MANIFEST),
           json.dumps(manifest if manifest is not None else selftest_manifest(),
                      ensure_ascii=False, indent=2) + chr(10))


def self_test() -> int:
    tmp = tempfile.mkdtemp(prefix="eng-constraints-selftest-")
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
                        "unique_authority_scanned": notes.get("unique_authority_scanned")})

    try:
        # P1：基线正例
        p1 = os.path.join(tmp, "p1-base")
        _mini_repo(p1)
        case("P1 mini-repo 基线（正例）", "pass", p1)

        # P2：悬空登记的**显式归档声明**正例（同名 + 声明 + sha256 一致 ⇒ 不判悬空）
        arc = _archive_probe(tmp, "p2-relocated", "GONE_OK.md", "归档内容", declare_hash=True)
        case("P2 悬空登记已显式声明归档且哈希一致（正例）", "pass", arc)

        # N1：新建未登记根条目
        n1 = os.path.join(tmp, "n1-new-root")
        _mini_repo(n1)
        _write(os.path.join(n1, "NEW_ROOT_JUNK.bin"), "x")
        case("N1 根目录新增未登记条目", "fail", n1, "unregistered_root_entry")

        # N2：台账 allowed_files 比 §7 宽
        n2 = os.path.join(tmp, "n2-wider")
        _mini_repo(n2, selftest_manifest(allowed_files=list(SELFTEST_FILES) + ["EXTRA_ROOT.md"]))
        case("N2 台账白名单比 §7 更宽", "fail", n2, "manifest_not_wider_than_spec7")

        # N3：悬空登记（不存在、无归档同名）
        n3 = os.path.join(tmp, "n3-dangling")
        _mini_repo(n3, selftest_manifest(retention=[{"path": "GONE.md", "note": "保留（登记）"}]))
        case("N3 悬空登记（不存在且无归档）", "fail", n3, "manifest_no_dangling_registration")

        # N3b：同名归档件存在但条目未显式声明 ⇒ 仍判悬空（R-6 §3.6 过宽豁免的负例）
        n3b = os.path.join(tmp, "n3b-same-name")
        _mini_repo(n3b, selftest_manifest(retention=[{"path": "GONE2.md", "note": "保留（登记）"}]))
        _write(os.path.join(n3b, "docs", "archive", "GONE2.md"), "内容毫不相关的同名文件\n")
        case("N3b 同名归档件存在但未显式声明 ⇒ 仍悬空（收紧点）", "fail", n3b,
             "manifest_no_dangling_registration")

        # N3c：显式声明了 sha256 但归档件内容不一致 ⇒ 判悬空
        arc2 = _archive_probe(tmp, "n3c-hash-mismatch", "GONE_BAD.md", "被替换的内容",
                              declare_hash=False)
        case("N3c 声明 sha256 与归档件不一致 ⇒ 悬空", "fail", arc2,
             "manifest_no_dangling_registration")

        # N4：「本文档」自称唯一最高（R-6 §3.6 实测被「本文」豁免漏掉的措辞）
        n4 = os.path.join(tmp, "n4-self-authority")
        _mini_repo(n4)
        _write(os.path.join(n4, "docs", "NOTE.md"),
               "# 探针\n\n本文档是仓库唯一最高权威，其余文档均从属。\n")
        case("N4 「本文档」自称唯一最高（继承漏报修复）", "fail", n4, "single_authority_entry")

        # N5：扫描面为空 ⇒ fail-closed
        # CI-003：memory.md 已纳入 [8] 扫描面，清空夹具必须一并移除它，
        # 否则 scanned=1 而 fail-closed 不触发（本用例正是为了守住该机制）。
        n5 = os.path.join(tmp, "n5-empty-face")
        _mini_repo(n5)
        for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
                    "memory.md"):
            os.remove(os.path.join(n5, rel))
        os.remove(os.path.join(n5, "docs", "design", "UNIFIED_MODEL.md"))
        case("N5 唯一权威扫描面为空 ⇒ fail-closed", "fail", n5, "unique_authority_scan_empty")

        # N6：已删除旧治理对象同名重建到根（登记进 ignored_patterns 后旧版 rc=0，内容从不被读）
        legacy_ign = ["AstroCS_ENGINEERING_CONSTRAINTS.md"]
        n6 = os.path.join(tmp, "n6-legacy-object")
        _mini_repo(n6, selftest_manifest(ignored_patterns=legacy_ign))
        _write(os.path.join(n6, "AstroCS_ENGINEERING_CONSTRAINTS.md"), SELFTEST_LEGACY_BINDING)
        case("N6 已删旧治理对象同名重建到根（本体写成绑定权威）", "fail", n6,
             "legacy_authority_regression")

        # P3：同名重建但正文标明「已删除/历史参照」⇒ 不误红
        p3 = os.path.join(tmp, "p3-legacy-archived")
        _mini_repo(p3, selftest_manifest(ignored_patterns=legacy_ign))
        _write(os.path.join(p3, "AstroCS_ENGINEERING_CONSTRAINTS.md"), SELFTEST_LEGACY_ARCHIVED)
        case("P3 同名重建但正文标明历史参照（正例）", "pass", p3)

        ok = all(r["ok"] for r in results)
        report = {"tool": "eng/tools/doccheck/check_engineering_constraints.py", "mode": "self-test",
                  "positive_cases": sum(1 for r in results if r["expect"] == "pass"),
                  "negative_cases": sum(1 for r in results if r["expect"] == "fail"),
                  "all_pass": ok, "results": results}
        print(json.dumps(report, ensure_ascii=False, indent=2, sort_keys=True))
        return 0 if ok else 1
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def _archive_probe(tmp: str, tag: str, name: str, body: str, declare_hash: bool) -> str:
    """造「登记条目显式声明归档目标」的 mini-repo；declare_hash=False 时声明一个错哈希。"""
    root = os.path.join(tmp, tag)
    content = body
    archive_rel = "docs/archive/" + name
    _mini_repo(root, selftest_manifest(retention=[{
        "path": name,
        "class": "F2",
        "owner_task": "GOV-001",
        "relocated_to": archive_rel,
        "note": "已归档（self-test 夹具）",
        "sha256": hashlib.sha256(content.encode("utf-8")).hexdigest() if declare_hash
        else "0" * 64,
    }]))
    _write(os.path.join(root, archive_rel), content)
    return root


def main() -> int:
    ap = argparse.ArgumentParser(description="工程约束门（以 ENGINEERING_SPEC.md 为权威）")
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
        "tool": "eng/tools/doccheck/check_engineering_constraints.py",
        "version": "2.1.0",
        "task": "GOV-001",
        "root": root,
        "authority_object": SPEC,
        "checks": checks,
        "notes": notes,
        "violations": uniq,
        "verdict": "CONSTRAINTS_PASS" if not uniq else "CONSTRAINTS_FAIL",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as fh:
            fh.write(text + chr(10))
    if not args.quiet:
        print(text)
    return 0 if not uniq else 1


if __name__ == "__main__":
    sys.exit(main())
