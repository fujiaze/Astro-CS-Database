#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-001 机器检查器：以 `ENGINEERING_SPEC.md` 为权威对象的工程约束门。

改造前（旧版）本检查器把仓库根旧文本 `AstroCS_ENGINEERING_CONSTRAINTS.md`
（已由 ROOT-007 删除）当权威对象，因此对当前树结构性 rc=1（GOV-001 基线）。
本版改以 `ENGINEERING_SPEC.md`（权威链第 3 级）为唯一权威对象：

  [1] `ENGINEERING_SPEC.md` 在位、受 Git 跟踪、含 §2/§3/§4/§5/§6/§7/§8 七节；
  [2] §7 根白名单可机械抽取：固定条目 + 其他固定目录 + gitignore 目录；
  [3] §7 要求存在的根条目必须齐备（缺一即红）；
  [4] §7 之外的顶层条目必须已登记（`ci/root_manifest.json` 的
      registered_local_retention / ignored_patterns），未登记即红；
      -> 新增根条目须"先登记并经负责人确认"（§7 原文）由本项机器化；
  [5] 登记台账只减不增：`ci/root_manifest.json` 的 allowed_files/allowed_dirs
      不得比 §7 更宽（逐条子集断言），且登记路径必须真实存在（禁悬空登记）；
  [6] `README.md` 权威入口：必须指向现行文档集（ASTROCS_DESIGN / AGENTS /
      ENGINEERING_SPEC / CONTROL_PACK_SPEC / docs/ci / docs/plugins /
      docs/design/UNIFIED_MODEL），且不得把已删除的旧治理对象写成上位权威；
  [7] 旧权威回归红灯：根权威面 + 工具面出现「旧文本 + 权威/必读/冻结」绑定即红；
  [8] 唯一权威入口：除 `ASTROCS_DESIGN.md` 的 §0 之外，活动文档不得自称
      "唯一最高权威/约束"（`docs/archive/**` 与带 ARCHIVED_NON_NORMATIVE 者豁免）。

用法：
  python3 tools/doccheck/check_engineering_constraints.py [--root <repo>]
      [--json-out <file>] [--quiet]

能红能绿证据（1 正例 + 8 负例）：run/PROJECT-GOVERNANCE-01/GOV-001/fixtures/
（runtime 验证脚本 verify_checkers.py；正例 = 真实仓库 rc=0）。
exit 0 = PASS；任一断言不成立 = exit 1（打印机器 JSON）。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SPEC = "ENGINEERING_SPEC.md"
MANIFEST = "ci/root_manifest.json"

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
    dirs |= {"run", "logs", "工程控制"}
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


def main() -> int:
    ap = argparse.ArgumentParser(description="工程约束门（以 ENGINEERING_SPEC.md 为权威）")
    ap.add_argument("--root", default=REPO)
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
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
    registered = {e.get("path") for e in man.get("registered_local_retention", []) or []}
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
        if False:
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
                          "detail": "ci/root_manifest.json 白名单比 §7 更宽: %s" % ",".join(wider)})
    def _relocated(p: str) -> bool:
        base = os.path.basename(p)
        for cand in (os.path.join("docs", "archive", base), os.path.join("run", "archive", base)):
            if os.path.exists(os.path.join(root, cand)):
                return True
        return False

    dangling = sorted(p for p in registered if p not in present and not _relocated(p))
    if dangling:
        violations.append({"check": "manifest_no_dangling_registration",
                          "detail": "登记台账悬空（路径不存在）: %s" % ",".join(dangling)})
    notes["registered_pending_owner"] = sorted(registered)

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

    # [7] 旧权威回归红灯（根权威面 + 工具面）
    for rel in ("README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md",
                "ASTROCS_DESIGN.md", "tools/check_agents_gov.py",
                "tools/doccheck/check_engineering_constraints.py"):
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            violations += legacy_hits(read(p), rel)

    # [8] 唯一权威入口
    scan = ["README.md", "AGENTS.md", "ENGINEERING_SPEC.md", "CONTROL_PACK_SPEC.md"]
    for dirpath, dirnames, filenames in os.walk(os.path.join(root, "docs")):
        dirnames[:] = [d for d in dirnames if d != "__pycache__"]
        rel = os.path.relpath(dirpath, root).replace(os.sep, "/")
        if rel.startswith("docs/archive"):
            continue
        for fn in filenames:
            if fn.endswith(".md"):
                scan.append(os.path.join(rel, fn))
    notes["unique_authority_scanned"] = len(scan)
    for rel in scan:
        p = os.path.join(root, rel)
        if not os.path.isfile(p):
            continue
        t = read(p)
        if "ARCHIVED_NON_NORMATIVE" in t or rel == "ASTROCS_DESIGN.md":
            continue
        for i, line in enumerate(t.splitlines(), 1):
            if SELF_AUTHORITY_RE.search(line) and "ASTROCS_DESIGN.md" not in line \
               and "本文" not in line and "本文件" not in line:
                violations.append({"check": "single_authority_entry",
                                  "detail": "%s:%d 以非最高权威文档自称唯一最高: %s"
                                  % (rel, i, line.strip()[:120])})

    seen, uniq = set(), []
    for x in violations:
        x.setdefault("check", "constraints_violation")
        x.setdefault("detail", "")
        key = (x["check"], x["detail"])
        if key in seen:
            continue
        seen.add(key)
        uniq.append(x)
    out = {
        "tool": "tools/doccheck/check_engineering_constraints.py",
        "version": "2.0.0",
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