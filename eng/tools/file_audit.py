#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/tools/file_audit.py —— 文件审计（分子与分母由**同一条命令**产出）。

存在理由（结论真实性缺口）
  审查节点原话：`tool_missing=true` 与 `coverage_ok=true` 并存、**分母 713 全仓无定义**、
  `eng/tools/file_audit` 缺失。三者同一根因：**覆盖度结论的分子与分母来自不同来源**
  —— 分母是一次口头目标（"700+"／"713"），分子是另一支 `find+wc` 替代统计，工具本身
  又不存在，于是结论无法被任何一条命令复算。

判据（S2-B「判完成」第 2 条）
  **每个分母与分子由同一命令产出**。本工具因此：
    1. 用**一条** `git ls-files -z` 同时取得分子与分母（同一命令、同一快照），
       并把该命令原文写进产物 `source_command`，供任何第三方逐字复跑；
    2. 分母**有定义**且定义随产物落盘（`denominator_definition`），不使用口头阈值；
    3. 无法执行该命令（不在 git 工作树 / git 不可用）⇒ **exit 2 且不写产物**，
       fail-closed；绝不退回 `find+wc` 之类的第二来源（那正是本工具要消灭的形态）。

分母口径（本文件即定义正本）
  分母 = `shipping_files`：`git ls-files` 中属于**出货语料**的已跟踪文件
         （前缀 lib/ eng/ docs/ 实验/，或根级清单 SHIPPING_EXACT 中的条目）。
         第三方、testdata、gaia、artifacts、run、工程控制 等**不属于**出货语料。
  分子 = `standard_scanned`：上述文件中扩展名落在 SCANNED_EXT 内的那些
         （即"标准扫描器真的会读"的文件）。`unscanned` 列出未覆盖项，使分子可核对。

快照（可复算性的输入面）
  分子与分母依赖"已跟踪文件全集"这一**随提交漂移**的输入，故产物必须登记输入快照：
  source_revision（产出时 HEAD）与 source_snapshot_sha256（该路径全集的指纹）。
  复算 = `--at-revision <source_revision>`：按该提交树重建同一输入，计数与指纹必须
  逐位相等。旧口径（"产物 vs. 现时工作树计数"）把**快照漂移**与**数字被改写**混为
  一谈：任何一次新增出货语料文件的提交都会让结论瞬间变红，而门分不清这是漂移还是
  造假；新口径对两者分别判定（漂移 ⇒ 按登记快照仍可复算；改写 ⇒ 计数或指纹不符即红）。

用法
  python3 eng/tools/file_audit.py [--root DIR] [--json-out PATH] [--quiet]
  python3 eng/tools/file_audit.py --at-revision <sha> [--json-out PATH] [--quiet]
退出码 0 = 产出审计；2 = 前置命令不可用/快照不可解析（fail-closed，不退回第二来源）。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
from collections import Counter

SOURCE_COMMAND = ["git", "ls-files", "-z"]
REVISION_COMMAND = ["git", "rev-parse"]

# 输入快照（复算的输入面，见模块头「快照」一节）
TREE_COMMAND = ["git", "ls-tree", "-r", "-z", "--name-only"]
SNAPSHOT_DEFINITION = (
    "快照 = 一条 git 命令给出的**已跟踪路径全集**；source_snapshot_sha256 = "
    "sha256(路径排序后以 \"\\n\" 连接)。产出时快照 = 产出时点的索引/工作树"
    "（source_command = git ls-files -z，source_revision = 产出时 HEAD）；"
    "复算时快照 = source_revision 的提交树（git ls-tree -r -z --name-only <rev>，"
    "即 file_audit.py --at-revision <rev>）。索引干净时两者给出同一路径全集 ⇒ "
    "计数与指纹可逐位复算；复算必须按 source_revision 重建输入，而不是按现时工作树。"
)

SHIPPING_PREFIXES = ("lib/", "eng/", "docs/", "实验/")
SHIPPING_EXACT = (
    "CMakeLists.txt", "CMakePresets.json", "VERSION", "README.md", "AGENTS.md",
    "docs/ASTROCS_DESIGN.md", "ENGINEERING_SPEC.md", "ACCEPTANCE_SPEC.md",
    "CONTROL_PACK_SPEC.md", "DEPENDENCIES.md", "memory.md",
)
SCANNED_EXT = (
    ".py", ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inc",
    ".md", ".rst", ".txt", ".json", ".jsonl", ".yaml", ".yml", ".toml", ".ini",
    ".cmake", ".sh", ".ps1", ".bat", ".csv", ".fits", ".xisf",
)

DENOMINATOR_DEFINITION = (
    "分母 = source_command 的 stdout（NUL 分隔）中，路径满足："
    "(a) 以 lib/ eng/ docs/ 实验/ 之一开头，或 (b) 恰为根级出货清单条目。"
    "分母 = 满足条件的已跟踪文件数；分子 = 其中扩展名 ∈ SCANNED_EXT 者。"
    "分子与分母同一次 git ls-files 调用、同一快照，故比值可由该命令复算。"
)


def _git(root, cmd):
    """跑一条 git 命令；失败即抛（调用方转 exit 2，fail-closed）。"""
    r = subprocess.run(cmd, cwd=root, capture_output=True)
    if r.returncode != 0:
        raise RuntimeError("%s 失败 rc=%d: %s"
                           % (" ".join(cmd), r.returncode,
                              r.stderr.decode("utf-8", "replace")[:300]))
    return r.stdout.decode("utf-8", "surrogateescape")


def tracked_files(root):
    """一条命令取全量已跟踪文件（产出路径）。"""
    return [p for p in _git(root, SOURCE_COMMAND).split("\0") if p]


def tracked_files_at(root, revision):
    """按提交树取同一路径全集（复算路径）：索引干净时与 tracked_files 集合相同。"""
    return [p for p in _git(root, TREE_COMMAND + [revision]).split("\0") if p]


def head_revision(root):
    """产出时 HEAD（不可判定时 None ⇒ 产物显式登记为不可复算，不静默编造）。"""
    r = subprocess.run(REVISION_COMMAND + ["HEAD"], cwd=root, capture_output=True)
    if r.returncode != 0:
        return None
    rev = r.stdout.decode("utf-8", "replace").strip()
    return rev or None


def resolve_revision(root, revision):
    """把 rev 解析为提交 sha；不可解析即抛（fail-closed，不退回现时工作树）。"""
    r = subprocess.run(REVISION_COMMAND + [revision + "^{commit}"], cwd=root,
                       capture_output=True)
    if r.returncode != 0:
        raise RuntimeError("git rev-parse %s^{commit} 失败 rc=%d: %s"
                           % (revision, r.returncode,
                              r.stderr.decode("utf-8", "replace")[:300]))
    return r.stdout.decode("utf-8", "replace").strip()


def snapshot_sha256(paths):
    """输入快照指纹：路径集合（排序）的 sha256 —— 与取数所用命令无关，只反映输入全集。"""
    return hashlib.sha256("\n".join(sorted(paths)).encode("utf-8")).hexdigest()


def is_shipping(path):
    return path.startswith(SHIPPING_PREFIXES) or path in SHIPPING_EXACT


def audit(root, at_revision=""):
    if at_revision:
        revision = resolve_revision(root, at_revision)
        files = tracked_files_at(root, revision)
    else:
        files = tracked_files(root)
        revision = head_revision(root)
    shipping = sorted(p for p in files if is_shipping(p))
    scanned = [p for p in shipping if os.path.splitext(p)[1].lower() in SCANNED_EXT]
    unscanned = sorted(p for p in shipping if p not in set(scanned))
    ext = Counter(os.path.splitext(p)[1].lower() or "<none>" for p in shipping)
    return {
        "tool": "file_audit",
        "schema_version": 1,
        "root": os.path.abspath(root),
        "source_command": SOURCE_COMMAND,
        "source_command_text": " ".join(SOURCE_COMMAND),
        "denominator_definition": DENOMINATOR_DEFINITION,
        "source_revision": revision,
        "source_snapshot_sha256": snapshot_sha256(files),
        "snapshot_definition": SNAPSHOT_DEFINITION,
        "counts": {
            "tracked_total": len(files),
            "shipping_total": len(shipping),
            "standard_scanned": len(scanned),
            "coverage": (len(scanned) / len(shipping)) if shipping else 0.0,
        },
        "by_extension": dict(sorted(ext.items())),
        "unscanned": unscanned,
        "produced_by": "eng/tools/file_audit.py",
    }


def main(argv=None):
    ap = argparse.ArgumentParser(description="文件审计（分子/分母同源）")
    ap.add_argument("--root", default=os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))
    ap.add_argument("--json-out", default="")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--at-revision", default="",
                    help="按该提交树复算（输入快照 = source_revision；缺省 = 现时索引/工作树）")
    a = ap.parse_args(argv)
    try:
        out = audit(a.root, a.at_revision)
    except (OSError, RuntimeError) as exc:
        print("FILE_AUDIT_ERROR: %s（fail-closed，不退回第二来源）" % exc, file=sys.stderr)
        return 2
    if a.json_out:
        d = os.path.dirname(os.path.abspath(a.json_out))
        if d:
            os.makedirs(d, exist_ok=True)
        with open(a.json_out, "w", encoding="utf-8") as f:
            json.dump(out, f, ensure_ascii=False, indent=1, sort_keys=True)
        print("REPORT_WRITTEN %s" % a.json_out)
    if not a.quiet:
        print(json.dumps(out, ensure_ascii=False, indent=1, sort_keys=True))
    else:
        c = out["counts"]
        print("FILE_AUDIT ok shipping=%d scanned=%d coverage=%.4f cmd=%s"
              % (c["shipping_total"], c["standard_scanned"], c["coverage"],
                 out["source_command_text"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
