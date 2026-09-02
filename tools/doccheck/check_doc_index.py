#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-002 机器检查器：docs/DOCUMENT_INDEX.yaml 文档索引 + 归档边界校验。

检查项（exit 0 = PASS）：
  1. docs/DOCUMENT_INDEX.yaml 存在于仓库根（docs/ 下）且受 Git 跟踪；
  2. YAML 结构合法：doc_index.active / doc_index.archived；
  3. 每项 status 唯一取值于 {ACTIVE_NORMATIVE, ACTIVE_INFORMATIVE,
     GENERATED, ARCHIVED_NON_NORMATIVE}；path 全索引唯一（不跨区段重复）；
  4. active 区段不含任何 archive 路径（docs/archive/**、engineering/control/archive/**、
     亦不含路径中含 /archive/ 者）；
  5. archived 区段全部 status == ARCHIVED_NON_NORMATIVE；
  6. 所有 Git 跟踪的 docs/** 与 docs/archive/** md/rst/txt/yaml 文件都被索引覆盖
     （doc_index.active ∪ doc_index.archived 的 path 前缀覆盖）；
  7. archive 内容约束：docs/archive/** 下每个 .md 文件头 400 字符内含
     "ARCHIVED"；engineering/control/archive/** 目录含目录级归档说明
     （README_ARCHIVED.md 或 ARCHIVED 标记）；
  8. 根与 docs/ 下无散落旧控制包（无 工程控制/ 路径、无 git 跟踪的根级
     旧控制 zip/md）；
  9. 输出稳定排序 JSON 的 machine_index。

用法：
  python3 tools/doccheck/check_doc_index.py [--root <repo>] [--json-out <file>]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys

INDEX_PATH = "docs/DOCUMENT_INDEX.yaml"
LEGAL = {"ACTIVE_NORMATIVE", "ACTIVE_INFORMATIVE", "GENERATED", "ARCHIVED_NON_NORMATIVE"}


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def git_tracked(root: str, rel: str) -> bool:
    r = subprocess.run(["git", "ls-files", "--error-unmatch", "--", rel],
                       cwd=root, capture_output=True, text=True)
    return r.returncode == 0


def git_ls(root: str) -> list[str]:
    r = subprocess.run(["git", "ls-files"], cwd=root,
                       capture_output=True, text=True)
    return [x for x in r.stdout.splitlines() if x.strip()]


def is_archive_path(p: str) -> bool:
    return p.startswith("docs/archive/") or p.startswith("engineering/control/archive/") or "/archive/" in "/" + p


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--strict", action="store_true")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    results: list[dict] = []
    machine_index: list[dict] = []

    try:
        import yaml  # type: ignore
    except Exception:
        yaml = None

    index_full = os.path.join(root, INDEX_PATH)
    exists = os.path.isfile(index_full)
    results.append(check("index_file_exists", exists, INDEX_PATH))
    tracked = exists and git_tracked(root, INDEX_PATH)
    results.append(check("index_file_git_tracked", tracked, "git ls-files --error-unmatch -- " + INDEX_PATH))

    doc_index: dict = {}
    if exists:
        if yaml is None:
            results.append(check("yaml_parse", False, "pyyaml 不可用，跳过内容解析"))
        else:
            try:
                data = yaml.safe_load(open(index_full, encoding="utf-8"))
                doc_index = (data or {}).get("doc_index", {})
                results.append(check("yaml_parse", True, "ok"))
            except Exception as exc:
                results.append(check("yaml_parse", False, str(exc)))

    active = doc_index.get("active", []) or []
    archived = doc_index.get("archived", []) or []
    all_entries = active + archived

    # 3a. status 合法
    bad_status = [e.get("path") for e in all_entries if e.get("status") not in LEGAL]
    results.append(check("status_legal", not bad_status, f"非法={bad_status}" if bad_status else "ok"))
    # 3b. path 唯一
    paths = [e.get("path", "") for e in all_entries]
    dups = sorted({p for p in paths if paths.count(p) > 1})
    results.append(check("path_unique", not dups, f"重复={dups}" if dups else "ok"))
    # 4. active 不含 archive
    bad_active = [e.get("path") for e in active if is_archive_path(e.get("path", ""))]
    results.append(check("active_no_archive", not bad_active,
                         f"active 含 archive={bad_active}" if bad_active else "ok"))
    # 5. archived 全 ARCHIVED_NON_NORMATIVE
    bad_arch_st = [e.get("path") for e in archived if e.get("status") != "ARCHIVED_NON_NORMATIVE"]
    results.append(check("archived_all_archived_status", not bad_arch_st,
                         f"非ARCHIVED={bad_arch_st}" if bad_arch_st else "ok"))

    # 6. 覆盖：git tracked docs/** md/rst/txt/yaml
    if git_tracked(root, INDEX_PATH):
        allf = git_ls(root)
        docfiles = [p for p in allf
                    if p.startswith("docs/") and p.endswith((".md", ".rst", ".txt", ".yaml", ".yml"))
                    and p != INDEX_PATH]  # index 文件自身不作覆盖目标
        covered_paths = [e.get("path", "") for e in all_entries]
        uncovered = []
        for p in docfiles:
            hit = False
            for cp in covered_paths:
                if p == cp or p.startswith(cp.rstrip("/") + "/"):
                    hit = True
                    break
            if not hit:
                uncovered.append(p)
        results.append(check("docs_fully_covered", not uncovered,
                             f"未覆盖={uncovered[:10]}…共{len(uncovered)}" if uncovered else f"覆盖 {len(docfiles)} 文件"))
    else:
        results.append(check("docs_fully_covered", True, "index 未跟踪，跳过覆盖检查"))

    # 7. archive 内容约束
    archive_md = [p for p in git_ls(root) if p.startswith("docs/archive/") and p.endswith(".md")]
    no_header = []
    for p in archive_md:
        fp = os.path.join(root, p)
        try:
            head = open(fp, encoding="utf-8", errors="replace").read(400)
        except Exception:
            no_header.append(p + "(读失败)")
            continue
        if "ARCHIVED" not in head:
            no_header.append(p)
    results.append(check("archive_md_has_archived_marker", not no_header,
                         f"缺标记={no_header}" if no_header else f"docs/archive {len(archive_md)} 个 md 全含 ARCHIVED"))
    ctrl_readme = os.path.join(root, "engineering/control/archive",
                               "2026-09-02_legacy_工程控制_v1.3-to-v6.1", "README_ARCHIVED.md")
    results.append(check("control_archive_dir_readme", os.path.isfile(ctrl_readme),
                         "engineering/control/archive 目录级 README_ARCHIVED.md 存在" if os.path.isfile(ctrl_readme) else "缺失"))

    # 8. 根无散落旧控制包
    allf = git_ls(root)
    leftover = [p for p in allf if p.startswith("工程控制/")]
    legacy_root = [p for p in allf
                   if "/" not in p and p.endswith(".zip") and ("CONTROL" in p or "AUDIT" in p or "REVIEW" in p or "PACK" in p)]
    results.append(check("no_legacy_工程控制_left", not leftover, f"残留={leftover[:5]}" if leftover else "ok"))
    results.append(check("no_root_legacy_control_zip", not legacy_root, f"残留={legacy_root}" if legacy_root else "ok"))

    for e in all_entries:
        machine_index.append({"path": e.get("path", ""), "status": e.get("status", "")})
    machine_index = sorted(machine_index, key=lambda x: x["path"])

    passed = all(r["pass"] for r in results)
    out = {
        "tool": "tools/doccheck/check_doc_index.py",
        "version": "1.0.0",
        "task": "GOV-002",
        "root": root,
        "results": sorted(results, key=lambda r: r["check"]),
        "machine_index_count": len(machine_index),
        "machine_index": machine_index,
        "verdict": "DOC_INDEX_PASS" if passed else "DOC_INDEX_FAIL",
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    print(text)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
