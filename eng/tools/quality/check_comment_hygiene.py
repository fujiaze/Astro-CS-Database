#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_comment_hygiene.py — V19R2 comment-hygiene gate.

Scans first-party production C/C++ sources for forbidden history markers:
  V[0-9]+ / R[0-9]+ / MICROFIX / 控制包 / 审计 / 骨架版本 / 号计划
with whitelist: FITS/HiPS/protocol/scientific model versions and
lowercase vendor versions (astrocs-upm-v2, NoiseWeightModelV1).

Output: reports/v19r2/evidence/quality/comment_check.json and
reports/v19r2/comment_hygiene.md.

扫描面与 fail-closed（规范依据 docs/ci/01_CHECKS.md §1、ENGINEERING_SPEC.md §10）：
  * 原实现用 `git ls-files` 取 tracked 清单再筛 lib/ 前缀 —— 该清单是**纯枚举源**，
    判据本身（"首方生产源不得含历史标记"）与 git 无关。实测后果（GITDECOUPLE-02，
    无 .git 镜像树）：git 在但非 git 树 ⇒ 清单为空 ⇒ scanned=0 而 **rc=0**
    （假绿，违反 §1「scanned == 0 ⇒ rc != 0」）；git 可执行文件不存在 ⇒
    FileNotFoundError traceback（§1「不得 traceback」）。
  * 现枚举源 = lib/ 工作树 os.walk + 剪枝排除面（确定性、不依赖 git），
    生产树 / 镜像树 / 归档树走**同一条代码路径**；实测真仓库枚举 815 个源文件，
    与 `git ls-files` 筛选结果逐元素相等。
  * 锚存活：`lib/` 必须存在、枚举面不得为空，失效以
    `ANCHOR_STALE: <常量名> <路径>` 点名并 rc=2（fail-closed）。

退出码：0 = 扫描完成（结论与命中数一并打印，见文件末注释）；2 = ANCHOR_STALE
（锚失效 / 扫描面为空，fail-closed）。
"""

from __future__ import annotations

import json
import os
import re
import sys

# 扫描面锚（§1 锚存活）：硬编码引用的仓库路径必须存在。
SCAN_ROOT_REL = "lib"
# 剪枝排除面（与既有实现同集：构建区/依赖区/归档区/第三方/CMake 生成区）
EXCLUDE_DIR_NAMES = frozenset(
    ("build", "build2", "_deps", "CMakeFiles", "archive", "third_party", "worktrees"))
SOURCE_SUFFIXES = (".cpp", ".h", ".hpp", ".c", ".cc", ".hh")


def _deduce_root() -> str:
    # auto-deduce project root: walk up until docs/ and lib/ found (Linux-portable)
    try:
        p = os.path.abspath(__file__)
        cur = os.path.dirname(p)
        for _ in range(5):
            if os.path.isdir(os.path.join(cur, "docs")) and os.path.isdir(os.path.join(cur, "lib")):
                return cur
            parent = os.path.dirname(cur)
            if parent == cur:
                break
            cur = parent
    except Exception:
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

ROOT = _deduce_root()
OUT = os.path.join(ROOT, "reports", "v19r2")

FORBIDDEN = re.compile(
    r"(?<![A-Za-z0-9_-])(?:V\d+(?:\.\d+)?(?:-[A-Za-z0-9]+)?|R\d+|"
    r"MICROFIX)(?![A-Za-z0-9_-])|控制包|审计|骨架版本|号计划")

# Formal/scientific version contexts that are allowed.
WHITELIST = re.compile(
    r"(?:FITS|HiPS|IVOA|schema|protocol|astrocs-upm-v\d+|"
    r"NoiseWeightModelV1|model_hash|format|astrocs-stage2|"
    r"astrocs_adaptive|wbpp_\d+_\d+_\d+|upm_v\d+|hiss_v\d+)",
    re.IGNORECASE)


class AnchorStale(Exception):
    """锚失效 / 扫描面为空 —— fail-closed，rc=2（§1 锚存活 + fail-closed）。"""


def _walk_error(exc: OSError) -> None:
    """遍历期错误（权限/IO）⇒ 扫描面可能静默缩小 —— fail-closed 判红，不静默跳过。"""
    raise AnchorStale(f"LIB_SCAN_FACE_WALK_ERROR: {exc}")


def scan_face(root: str) -> list[str]:
    """lib/ 下首方生产源（仓库相对 POSIX 路径，确定性枚举，不依赖 git）。

    锚失效 / 枚举为空 ⇒ AnchorStale（点名），绝不返回空集
    （空扫描 = 恒真假绿，§1「scanned == 0 ⇒ rc != 0」）。
    """
    base = os.path.join(root, SCAN_ROOT_REL)
    if not os.path.isdir(base):
        raise AnchorStale(f"SCAN_ROOT_REL {base} 不存在（首方生产源扫描面）")
    found: list[str] = []
    for dirpath, dirnames, filenames in os.walk(base, onerror=_walk_error,
                                               followlinks=False):
        dirnames[:] = sorted(d for d in dirnames if d not in EXCLUDE_DIR_NAMES)
        for name in sorted(filenames):
            if name.endswith(SOURCE_SUFFIXES):
                found.append(os.path.relpath(os.path.join(dirpath, name), root)
                             .replace(os.sep, "/"))
    if not found:
        raise AnchorStale(
            f"LIB_SCAN_FACE_EMPTY: {base} 下未枚举到任何首方生产源"
            f"（后缀 {SOURCE_SUFFIXES}）—— fail-closed 拒绝空扫描")
    return sorted(found)


def line_is_allowed(line: str) -> bool:
    # 整行是 formal version / protocol 声明时允许
    return bool(WHITELIST.search(line))


def main() -> int:
    try:
        files = scan_face(ROOT)
    except AnchorStale as exc:
        print(f"ANCHOR_STALE: {exc}", file=sys.stderr)
        return 2
    violations: list[dict] = []
    for p in files:
        path = os.path.join(ROOT, p)
        try:
            with open(path, encoding="utf-8", errors="replace") as f:
                lines = f.readlines()
        except OSError:
            continue
        for i, raw in enumerate(lines, 1):
            if "//" not in raw:
                continue
            if line_is_allowed(raw):
                continue
            for m in FORBIDDEN.finditer(raw):
                violations.append({
                    "file": p, "line": i, "match": m.group(0),
                    "text": raw.strip()[:160],
                })
                break  # 每行记一次即可
    os.makedirs(os.path.join(OUT, "evidence", "quality"), exist_ok=True)
    with open(os.path.join(OUT, "evidence", "quality",
                           "comment_check.json"), "w", encoding="utf-8") as f:
        json.dump({"files_scanned": len(files),
                   "violation_lines": len(violations),
                   "scan_face": {
                       "root": SCAN_ROOT_REL,
                       "enumeration": "os.walk(ROOT/lib)+剪枝排除面（不依赖 git；GITDECOUPLE-02）",
                   },
                   "violations": violations}, f, ensure_ascii=False, indent=1)
    by_file: dict[str, int] = {}
    for v in violations:
        by_file[v["file"]] = by_file.get(v["file"], 0) + 1
    md = ["# Comment Hygiene Report (V19R2)",
          "", f"- files scanned: {len(files)}",
          f"- violation lines: {len(violations)}",
          f"- files with violations: {len(by_file)}", ""]
    for p, n in sorted(by_file.items(), key=lambda kv: -kv[1]):
        md.append(f"- {n:4d}  {p}")
    with open(os.path.join(OUT, "comment_hygiene.md"), "w",
              encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    print(f"comment hygiene: scanned={len(files)} "
          f"violations={len(violations)} files={len(by_file)}")
    # 退出码：本工具是 V19R2 时代的**报告型**扫描器（未登记进 eng/ci/checks.json），
    # 命中数只打印不参与 rc —— 该口径是历史行为，GITDECOUPLE-02 未改动它
    # （改 rc 会让真仓库从"恒绿"变成"新红"，属另一项需要裁决的处置）。
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
