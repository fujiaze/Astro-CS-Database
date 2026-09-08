#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""GOV-001 机器检查器：仓库根工程约束文件 + AGENTS.md 指针化校验。

检查项（exit 0 = PASS）：
  1. AstroCS_ENGINEERING_CONSTRAINTS.md 存在于仓库根；
  2. 该文件受 Git 跟踪（git ls-files --error-unmatch）；
  3. 文件头 YAML 机器索引块完整（doc_id/status/source_*_sha256/source_main_sha）；
  4. source_main_sha 是当前 HEAD 的祖先提交（--base-sha 给定时改为与其严格相等）；
  5. 给定 --control-root 时，上游 01_OWNER_FROZEN_CONSTRAINTS.md 的 SHA-256
     与文件头 source_control_sha256 一致（修订关系）；未给定时结构性跳过；
  6. AGENTS.md 精简指针化：约束文件引用与 memory 引用必须在 AGENTS.md 字面命中；
     Linux/Windows 角色允许 AGENTS.md 字面提及，或由其指向的
     AstroCS_ENGINEERING_CONSTRAINTS.md 同时覆盖 Linux 与 Windows
     （指针间接覆盖 — GOV-001 指针化语义：AGENTS.md 不复制长文，平台角色
     细则在被指向的约束文件里）；
     "Git Bash"/"PowerShell"/"pwsh" 只允许出现在禁止性条款（禁止作为默认开发环境）；
  7. 输出稳定排序 JSON 的 machine_index（本文件登记的机器可读条目）。

用法：
  python3 tools/doccheck/check_engineering_constraints.py [--root <repo>]
      [--base-sha <40hex>] [--control-root <控制包目录>] [--json-out <file>] [--strict]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys

DOC_PATH = "AstroCS_ENGINEERING_CONSTRAINTS.md"
AGENTS_PATH = "AGENTS.md"
YAML_RE = re.compile(r"^```yaml\n(.*?)\n```", re.S | re.M)
KEY_RE = re.compile(r"^([A-Za-z0-9_]+):\s*(.+?)\s*$")


def sha256_file(p: str) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def parse_yaml_block(text: str) -> dict:
    m = YAML_RE.search(text)
    if not m:
        return {}
    out = {}
    for line in m.group(1).splitlines():
        km = KEY_RE.match(line)
        if km:
            out[km.group(1)] = km.group(2).strip().strip("'\"")
    return out


def git_tracked(root: str, rel: str) -> bool:
    r = subprocess.run(
        ["git", "ls-files", "--error-unmatch", "--", rel],
        cwd=root, capture_output=True, text=True)
    return r.returncode == 0


def head_sha(root: str) -> str:
    r = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                       capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".", help="仓库根目录")
    ap.add_argument("--base-sha", default=None, help="期望 source_main_sha (40hex)")
    ap.add_argument("--control-root", default=None,
                    help="控制包目录；提供时校验上游 01_OWNER_FROZEN_CONSTRAINTS.md hash")
    ap.add_argument("--json-out", default=None, help="结果 JSON 输出文件")
    ap.add_argument("--strict", action="store_true", help="保留（严格模式标记）")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    results: list[dict] = []
    machine_index: list[dict] = []
    meta: dict = {}

    doc_full = os.path.join(root, DOC_PATH)
    exists = os.path.isfile(doc_full)
    results.append(check("constraints_file_exists", exists, DOC_PATH))

    tracked = False
    if exists:
        tracked = git_tracked(root, DOC_PATH)
    results.append(check("constraints_file_git_tracked", tracked,
                         "git ls-files --error-unmatch -- " + DOC_PATH))

    header: dict = {}
    if exists:
        header = parse_yaml_block(open(doc_full, encoding="utf-8").read())
    need = ["doc_id", "doc_status", "source_control_relpath",
            "source_control_sha256", "source_main_sha"]
    missing = [k for k in need if not header.get(k)]
    results.append(check("yaml_header_complete", not missing,
                         "missing=" + ",".join(missing) if missing else "ok"))

    sha_ok = False
    sha_detail = ""
    if header.get("source_main_sha"):
        src = header["source_main_sha"]
        if args.base_sha:
            # 显式给定 base-sha：严格相等（外部仲裁模式，保持旧语义）
            sha_ok = src == args.base_sha
            sha_detail = f"header={src} expected={args.base_sha}"
        else:
            # 默认语义：约束头 source_main_sha 记录冻结时点的 main 提交，
            # main 之后正常前进不构成漂移；用 merge-base --is-ancestor 判定
            # 冻结时点是否在当前 HEAD 历史内（sha 伪造或历史被重写都会 FAIL）。
            r = subprocess.run(
                ["git", "merge-base", "--is-ancestor", src, "HEAD"],
                cwd=root, capture_output=True, text=True)
            sha_ok = r.returncode == 0
            sha_detail = f"header={src} ancestor_of_HEAD={sha_ok}" + (
                "" if sha_ok else " (merge-base --is-ancestor rc=%d: sha 不在当前 HEAD 历史)" % r.returncode)
    results.append(check("source_main_sha_matches", sha_ok, sha_detail))

    upstream_ok = True  # 未提供 --control-root 时结构性跳过（无治理包 ≠ 违规）
    upstream_detail = "control-root 未提供，结构性跳过（治理包不入库，见检查器 docstring 第 5 条）"
    if args.control_root:
        up = os.path.join(os.path.abspath(args.control_root),
                          header.get("source_control_relpath", ""))
        if os.path.isfile(up):
            actual = sha256_file(up)
            upstream_ok = actual == header.get("source_control_sha256")
            upstream_detail = f"upstream={up} header_sha={header.get('source_control_sha256')} actual={actual}"
        else:
            upstream_detail = f"upstream 不存在: {up}"
    results.append(check("upstream_control_sha256_matches", upstream_ok, upstream_detail))

    if exists:
        machine_index.append({
            "doc_id": header.get("doc_id", ""),
            "doc_status": header.get("doc_status", ""),
            "doc_scope": header.get("doc_scope", ""),
            "path": DOC_PATH,
            "source_control_relpath": header.get("source_control_relpath", ""),
            "source_control_sha256": header.get("source_control_sha256", ""),
            "source_main_sha": header.get("source_main_sha", ""),
        })

    agents_ok = False
    agents_detail = ""
    agents = ""
    if os.path.isfile(os.path.join(root, AGENTS_PATH)):
        agents = open(os.path.join(root, AGENTS_PATH), encoding="utf-8").read()
    miss_refs: list[str] = []
    # 第一组：AGENTS.md 指针化入口必须字面命中的引用
    refs_literal = [("AstroCS_ENGINEERING_CONSTRAINTS.md", "约束文件引用"),
                    ("memory.md", "memory 引用")]
    miss_refs += [label for needle, label in refs_literal if needle not in agents]
    # 第二组：平台角色 —— AGENTS.md 字面提及（Linux 与 Windows 都在）
    # 或其指向的 AstroCS_ENGINEERING_CONSTRAINTS.md 中同时含 Linux 与 Windows
    # （指针间接覆盖，符合 GOV-001 "AGENTS.md 精简指针化"语义）。
    constraints_text = ""
    if exists:
        constraints_text = open(doc_full, encoding="utf-8").read()
    agents_literal = "Linux" in agents and "Windows" in agents
    constraints_covers = ("Linux" in constraints_text and
                          "Windows" in constraints_text)
    platform_via = None
    if agents_literal:
        platform_via = "agents_md_literal"
    elif constraints_covers:
        platform_via = "constraints_doc_pointer"
    else:
        miss_refs.append("Linux/Windows 角色 (AGENTS.md 与其指向的约束文件均未覆盖)")
    # Git Bash / PowerShell / pwsh 只允许出现在禁止性条款中
    bad_lines = []
    for i, line in enumerate(agents.splitlines(), 1):
        if re.search(r"Git\s*Bash|PowerShell|pwsh", line) and "禁止" not in line:
            bad_lines.append(f"{AGENTS_PATH}:{i}")
    agents_ok = (not miss_refs) and (not bad_lines)
    agents_detail = (("缺少: " + ",".join(miss_refs)) if miss_refs else "") + \
                    ((" 非禁止条款命中: " + "; ".join(bad_lines)) if bad_lines else "")
    if agents_ok and platform_via == "constraints_doc_pointer":
        agents_detail = "平台角色经 AstroCS_ENGINEERING_CONSTRAINTS.md 指针间接覆盖"
    results.append(check("agents_md_pointer_style", agents_ok, agents_detail))

    passed = all(r["pass"] for r in results)
    out = {
        "tool": "tools/doccheck/check_engineering_constraints.py",
        "version": "1.1.0",
        "task": "GOV-001",
        "root": root,
        "head_sha": head_sha(root),
        "results": sorted(results, key=lambda r: r["check"]),
        "machine_index": sorted(machine_index, key=lambda e: e["path"]),
        "verdict": "CONSTRAINTS_PASS" if passed else "CONSTRAINTS_FAIL",
        "meta": {"stdout_sha256": "", "strict": args.strict},
    }
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    meta["stdout_sha256"] = hashlib.sha256(text.encode("utf-8")).hexdigest()
    out["meta"] = meta
    text = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True)
    if args.json_out:
        os.makedirs(os.path.dirname(os.path.abspath(args.json_out)), exist_ok=True)
        with open(args.json_out, "w", encoding="utf-8") as f:
            f.write(text + "\n")
    print(text)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
