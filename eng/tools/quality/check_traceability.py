#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_traceability.py — V19R2 S5 追溯校验。

双向检查：
  code→contract→test：science/public 符号必须有契约 ID + 测试引用；
  contract→code→test：TRACEABILITY 行必须存在实现文件 + 测试文件。
输出 run/ci/quality/traceability_check.json（产物落 run/，不写受跟踪路径；ENGINEERING_SPEC §7/§8）。

枚举源与 fail-closed（规范依据 docs/ci/01_CHECKS.md §1、ENGINEERING_SPEC.md §10）：
  * 原实现用 `git ls-files` 取 "tracked 清单" 来判定 TRACEABILITY 行的实现文件是否
    属于仓库。该清单是**纯枚举源**：本文件声明的判据是 "行必须存在实现文件 + 测试文件"，
    并不要求 "被 git 跟踪"。实测后果（GITDECOUPLE-02，无 .git 镜像树）：
      - git 可执行文件不存在 ⇒ FileNotFoundError traceback（§1「不得 traceback」）；
      - git 在但非 git 树 ⇒ 清单为空 ⇒ 67/67 行被判 "implementation file not tracked"
        （rc=1，报错语义指向 "行断链" 而真因是 "枚举源不可用"，误导）。
  * 现判据：实现文件必须**存在于仓库根下的工作树**（确定性、不依赖 git），且不得越出
    仓库根；锚存活：REQUIRED_ROOTS 每个目录必须存在、lib/ 扫描面与 CSV 行数不得为空，
    失效以 `ANCHOR_STALE: <常量名> <路径>` 点名并 rc=2（fail-closed；§1
    「scanned == 0 ⇒ rc != 0」「锚失效不得 traceback、不得静默降级」）。

退出码：0 = PASS；1 = 判据违规（行断链 / 符号断链 / CSV 缺失或列不全）；
        2 = ANCHOR_STALE（锚失效 / 扫描面为空，fail-closed）。
"""

from __future__ import annotations

import csv
import json
import os
import re
import sys


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
# 产物落 run/（不写受跟踪的 reports/**；ENGINEERING_SPEC §7 产物落位 + §8 fail-closed）
DEFAULT_OUT = os.path.join("run", "ci", "quality", "traceability_check.json")

# 判据硬编码引用的仓库路径（锚存活；§1「锚存活」）。失效 ⇒ ANCHOR_STALE rc=2，点名。
REQUIRED_ROOTS = (
    ("lib", "符号/测试源扫描面（本检查器的 code→contract→test 面）"),
    ("docs", "TRACEABILITY.csv 所在文档面"),
)
CSV_REL = os.path.join("docs", "TRACEABILITY.csv")
REQUIRED_COLUMNS = ("requirement_id", "implementation_files", "test_files")
# lib/ 扫描面的剪枝排除面（与既有实现同集：构建区/依赖区/归档区/第三方）
LIB_EXCLUDE_DIR_NAMES = frozenset(
    ("build", "build2", "_deps", "archive", "third_party", "worktrees"))


class AnchorStale(Exception):
    """锚失效 / 扫描面为空 —— fail-closed，rc=2（§1 锚存活 + fail-closed）。"""


def _check_anchors() -> None:
    """REQUIRED_ROOTS 逐个存活检查；失效即点名（不 traceback、不静默降级）。"""
    for name, why in REQUIRED_ROOTS:
        p = os.path.join(ROOT, name)
        if not os.path.isdir(p):
            raise AnchorStale(f"REQUIRED_ROOTS[{name}] {p} 不存在（{why}）")


def _walk_error(exc: OSError) -> None:
    """遍历期错误（权限/IO）⇒ 扫描面可能静默缩小 —— fail-closed 判红，不静默跳过。"""
    raise AnchorStale(f"LIB_SCAN_FACE_WALK_ERROR: {exc}")


def _iter_lib_files(suffixes: tuple[str, ...]):
    """lib/ 下确定性枚举的源文件（剪枝排除面；不依赖 git）。

    返回按仓库相对 POSIX 路径排序的列表。枚举为空由调用方判 ANCHOR_STALE。
    """
    base = os.path.join(ROOT, "lib")
    found = []
    for dirpath, dirnames, filenames in os.walk(base, onerror=_walk_error,
                                               followlinks=False):
        dirnames[:] = sorted(d for d in dirnames if d not in LIB_EXCLUDE_DIR_NAMES)
        for name in sorted(filenames):
            if name.endswith(suffixes):
                found.append(os.path.relpath(os.path.join(dirpath, name), ROOT)
                             .replace(os.sep, "/"))
    return sorted(found)


def _impl_file_ok(rel: str) -> tuple[bool, str]:
    """实现文件是否属于仓库工作树（确定性判据，不依赖 git）。

    返回 (是否合格, 原因)。越出仓库根的相对路径一律不合格（防 ".." 逃逸）。
    """
    full = os.path.abspath(os.path.join(ROOT, rel))
    root_abs = os.path.abspath(ROOT)
    try:
        inside = os.path.commonpath([full, root_abs]) == root_abs
    except ValueError:  # 不同盘符（Windows）等
        inside = False
    if not inside:
        return False, "outside_repo_root"
    if not os.path.isfile(full):
        return False, "not_found"
    return True, "ok"


def main(argv: list[str] | None = None) -> int:
    import argparse
    ap = argparse.ArgumentParser(description="V19R2 S5 追溯校验")
    ap.add_argument("--out", default=DEFAULT_OUT,
                    help="JSON 证据输出路径（默认 run/ci/quality/traceability_check.json）")
    args = ap.parse_args(argv)

    # 锚存活先行（§1）：锚失效 ⇒ 点名 + rc=2，绝不带着空扫描面继续判绿。
    try:
        _check_anchors()
    except AnchorStale as exc:
        print(f"ANCHOR_STALE: {exc}", file=sys.stderr)
        return 2

    trace_path = os.path.join(ROOT, CSV_REL)
    if not os.path.isfile(trace_path):
        # fail-closed: 输入缺失判红，不得当「无违规」
        print(f"TRACEABILITY_FAIL: 输入缺失 {trace_path}", file=sys.stderr)
        return 1
    with open(trace_path, encoding="utf-8") as f:
        reader = csv.DictReader(f)
        columns = list(reader.fieldnames or [])
        trace = list(reader)
    missing_cols = [c for c in REQUIRED_COLUMNS if c not in columns]
    if missing_cols:
        # 列不全 ⇒ 判据不可执行（原来会 KeyError traceback，§1「不得 traceback」）
        print(f"TRACEABILITY_FAIL: {CSV_REL} 缺必需列 {missing_cols}（实际列={columns}）",
              file=sys.stderr)
        return 1
    if not trace:
        print(f"ANCHOR_STALE: TRACEABILITY_ROWS {trace_path} 解析出 0 行"
              f"（扫描面为空，fail-closed 拒绝恒真判绿）", file=sys.stderr)
        return 2

    rows_ok = 0
    broken = []
    impl_files_checked = 0
    for r in trace:
        impl = r["implementation_files"]
        tests = r["test_files"]
        if not impl or not tests:
            broken.append({"requirement": r["requirement_id"],
                           "reason": "missing implementation/test files"})
            continue
        bad = []
        for p in re.split(r"[;,\s]+", impl):
            if not p:
                continue
            impl_files_checked += 1
            ok, why = _impl_file_ok(p)
            if not ok:
                bad.append(f"{p} ({why})")
        if bad:
            broken.append({"requirement": r["requirement_id"],
                           "reason": "implementation file not in worktree: "
                                     + ", ".join(bad[:4])})
            continue
        rows_ok += 1

    # 抽样符号（科学关键）：code→contract→test
    symbols = [
        ("p2_upm_build", "upm.cpp"), ("p2_upm_save", "upm.cpp"),
        ("p2_upm_open", "upm.cpp"), ("p2_upm_calibrate_block", "upm.cpp"),
        ("p2_reject_stack", "rejection.cpp"),
        ("p2_integrate_pixel", "integrate.cpp"),
        # noise_model 内部函数经公共 API 测试覆盖（snr_noise_model_v1*）
        ("snr_noise_model_v1", "noise_model.cpp"),
        ("snr_phot_cal_quality", "noise_model.cpp"),
        ("p2_coverage_build", "coverage.cpp"),
        ("p2_sample_controls", "sampler.cpp"),
        # aio_upm 无独立单测名引用，经 p2_upm_save/open 公共 API 间接覆盖：
        # SaveOpenRoundtripAndHash / UpmPersist* 全链走 aio_upm_write_sparse。
        ("p2_upm_save", "upm.cpp"),
        ("p2_upm_open", "upm.cpp"),
        ("spherical_polygon_area", "spherical_overlap.cpp"),
    ]
    src_face = _iter_lib_files((".cpp", ".h", ".hpp"))
    test_face = _iter_lib_files(("_test.cpp", "gate.cpp", "test.cpp"))
    if not src_face:
        print(f"ANCHOR_STALE: LIB_SCAN_FACE {os.path.join(ROOT, 'lib')} 下未枚举到任何"
              f" .cpp/.h/.hpp（扫描面为空，fail-closed 拒绝恒真判绿）", file=sys.stderr)
        return 2

    sym_ok = 0
    sym_broken = []
    for sym, f in symbols:
        hit_src = False
        for rel in src_face:
            if os.path.basename(rel) != f:
                continue
            try:
                txt = open(os.path.join(ROOT, rel), encoding="utf-8",
                           errors="replace").read()
            except OSError:
                continue
            if re.search(r"\b" + re.escape(sym) + r"\b", txt):
                hit_src = True
                break
        if not hit_src:
            sym_broken.append({"symbol": sym, "reason": "not found in source"})
            continue
        # 测试引用
        test_ref = False
        indirect = {
            "p2_upm_save": "p2_upm_open",
            "p2_upm_open": "p2_upm_save",
        }
        needles = [sym] + ([indirect[sym]] if sym in indirect else [])
        for rel in test_face:
            try:
                txt = open(os.path.join(ROOT, rel), encoding="utf-8",
                           errors="replace").read()
            except OSError:
                continue
            if any(re.search(r"\b" + re.escape(n) + r"\b", txt) for n in needles):
                test_ref = True
                break
        if not test_ref:
            sym_broken.append({"symbol": sym, "reason": "no test reference"})
            continue
        sym_ok += 1

    out_path = args.out if os.path.isabs(args.out) else os.path.join(ROOT, args.out)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    out = {
        "traceability_rows": len(trace),
        "rows_ok": rows_ok,
        "rows_broken": len(broken),
        "broken_rows": broken,
        "sample_symbols": len(symbols),
        "sample_ok": sym_ok,
        "sample_broken": sym_broken,
        "TRACEABILITY_BROKEN": len(broken),
        # 扫描面留痕（fail-closed 可观测：任一面为 0 即已在上面判红）
        "scan_face": {
            "csv_rows": len(trace),
            "lib_src_files": len(src_face),
            "lib_test_files": len(test_face),
            "impl_files_checked": impl_files_checked,
            "enumeration": "os.walk(ROOT)+剪枝排除面（不依赖 git；GITDECOUPLE-02）",
        },
    }
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=1)
    print(f"traceability: rows={len(trace)} ok={rows_ok} broken={len(broken)} "
          f"symbols={sym_ok}/{len(symbols)} src_face={len(src_face)} "
          f"test_face={len(test_face)} out={os.path.relpath(out_path, ROOT)}")
    # 退出码按证据决定（旧版无条件 return 0 ⇒ waivable=false 却不可能红）
    if broken or sym_broken:
        print(f"TRACEABILITY_FAIL: broken_rows={len(broken)} "
              f"broken_symbols={len(sym_broken)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
