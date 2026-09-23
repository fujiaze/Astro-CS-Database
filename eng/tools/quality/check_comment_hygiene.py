#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_comment_hygiene.py — V19R2 一次性审计脚本（**已退役，非门禁**）。

退役判定依据（先判定再处置；不是"只改退出码了事"）
  1) 判据面已有**在册**门承接：eng/ci/checks.json 的 CHK-STALE-DOC 含步骤
     CON-COMMENTS → eng/tools/quality/contracts/check_comments.py
     （实测 800 文件扫描 + --self-test 5 例正负例全 OK + rc 0/1/2/3）。
     本脚本在 eng/ci/checks.json 与 docs/ci/01_CHECKS.md 中**零引用**；同簇
     build_v19r2_package.py / v19r3_* / v19r4_* / update_audit_status.py 互相
     引用且均无现行消费方 ⇒ 属 V19R2/R3/R4 时代的一次性审计工件。
  2) 判据退化（真仓库实测 171 条命中 / 71 文件 / 815 文件扫描）：
     FORBIDDEN 的 V[0-9]+ 与 R[0-9]+ 无法区分"已死开发轮次"与**现行合同词表**。实例：
       - lib/algorithms/coverage/include/astro/phase2/coverage.h:65 的
         "// V6 目标态：..." 中 V6 是**冻结的产品族合同名**（eng/contracts/data/v6/、
         weight_modes.production 等），不是开发轮次；
       - lib/infrastructure/aio/include/aio_hips.h:250 的 "V1 signal 子产品" 是
         aio_hips 的现行子产品版本。
     实测 171 条与在册门的 V1[0-9]R[0-9] 模式**重叠 0 条**：两门判据面不重合，
     而本脚本的真阳性（控制包任务标记 / bug 狩猎轮次 / 04_CPU_RESOURCE_TASKS 过程
     痕迹）与上述假阳性混在同一份命中清单里 ⇒ 直接注册会在 lib/ 立刻造出 171 处
     红，其中含大量误判：不是"能红能绿"，是"恒红且噪声"。
  3) 产物落点违规：原实现默认写 reports/v19r2/**，而 reports/ 不在
     ENGINEERING_SPEC §7 根目录白名单内。实测：跑一次本脚本即让
     CHK-ROOT-CLEAN（P0）判红（violations: unregistered_root_entry: reports）。
  4) 真正属于该判据面的欠账已分类留痕（不随退役消失）：见
     run/DEFECT-REPRO-01/COMMENT_HYGIENE_INVENTORY.md —— 按"可机械剥离的轮次标记"
     与"需裁决的现行词表假阳性"分类，并列出在册门当前**不覆盖**的类别，
     供负责人决定是否并入 CON-COMMENTS 的判据（ENGINEERING_SPEC §9：有长期价值的
     结论并入正式文档，其余删除，拿不准的列清单上呈）。

退役契约（docs/ci/01_CHECKS.md §2.1「检查器退役与预留」）
  * 无参调用：打印退役标识并 exit 2；
  * 原实现保留为 legacy_scan()，经 --legacy-scan 复跑（保留可复跑性）；
  * 产物默认落 run/comment-hygiene/（run/ 是 §7 登记的 gitignore 临时产物区），
    **不再**写 reports/**；也可用 --out-dir 指定；
  * --strict：命中即 rc=1（审计口径的能红能绿）；默认 rc=0 只表示"扫描完成"。
退出码：0 = --legacy-scan 扫描完成且未给 --strict；1 = --strict 下有命中；
        2 = 退役（无参调用）/ ANCHOR_STALE（扫描锚失效，fail-closed）；
        3 = 用法错误。
"""

from __future__ import annotations

import argparse
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

# 默认产物目录（§7：run/ 是登记的临时产物区；reports/ 不在白名单内，禁止再写）
DEFAULT_OUT_DIR_REL = os.path.join("run", "comment-hygiene")

RETIREMENT_MARKER = (
    "RETIRED: check_comment_hygiene.py 是 V19R2 一次性审计脚本，未注册进 "
    "eng/ci/checks.json；注释纪律的在册门是 CHK-STALE-DOC 的步骤 CON-COMMENTS "
    "(eng/tools/quality/contracts/check_comments.py)。"
)


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
    except Exception:  # noqa: BLE001
        pass
    cwd = os.getcwd()
    if os.path.isdir(os.path.join(cwd, "docs")) and os.path.isdir(os.path.join(cwd, "lib")):
        return cwd
    return os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


ROOT = _deduce_root()

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


def legacy_scan(root: str, out_dir: str, strict: bool) -> int:
    """原 V19R2 扫描实现（保留可复跑性；产物落 out_dir，不落 reports/）。"""
    try:
        files = scan_face(root)
    except AnchorStale as exc:
        print(f"ANCHOR_STALE: {exc}", file=sys.stderr)
        return 2
    violations: list[dict] = []
    for p in files:
        path = os.path.join(root, p)
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
    os.makedirs(os.path.join(out_dir, "evidence", "quality"), exist_ok=True)
    with open(os.path.join(out_dir, "evidence", "quality",
                           "comment_check.json"), "w", encoding="utf-8") as f:
        json.dump({"files_scanned": len(files),
                   "violation_lines": len(violations),
                   "retired": True,
                   "replacement_gate": "CHK-STALE-DOC/CON-COMMENTS "
                                       "(eng/tools/quality/contracts/check_comments.py)",
                   "scan_face": {
                       "root": SCAN_ROOT_REL,
                       "enumeration": "os.walk(ROOT/lib)+剪枝排除面（不依赖 git；GITDECOUPLE-02）",
                   },
                   "violations": violations}, f, ensure_ascii=False, indent=1)
    by_file: dict[str, int] = {}
    for v in violations:
        by_file[v["file"]] = by_file.get(v["file"], 0) + 1
    md = ["# Comment Hygiene Report (V19R2 legacy, RETIRED)",
          "",
          "- 本报告来自**已退役**的 V19R2 一次性审计脚本；在册门 = CHK-STALE-DOC/CON-COMMENTS。",
          f"- files scanned: {len(files)}",
          f"- violation lines: {len(violations)}",
          f"- files with violations: {len(by_file)}", ""]
    for p, n in sorted(by_file.items(), key=lambda kv: -kv[1]):
        md.append(f"- {n:4d}  {p}")
    with open(os.path.join(out_dir, "comment_hygiene.md"), "w",
              encoding="utf-8") as f:
        f.write("\n".join(md) + "\n")
    print(f"comment hygiene (legacy, retired): scanned={len(files)} "
          f"violations={len(violations)} files={len(by_file)} out={out_dir}")
    # 审计口径：--strict 下命中即判红（能红能绿）；默认 rc=0 只表示"扫描完成"。
    return 1 if (strict and violations) else 0


def _self_test() -> int:
    """可执行正/负例面：退役契约 + 产物落点 + 锚存活。"""
    import shutil
    import subprocess
    import tempfile

    problems: list[str] = []
    me = os.path.abspath(__file__)

    # P0 退役契约：无参调用 ⇒ 退役标识 + exit 2
    r = subprocess.run([sys.executable, me], capture_output=True, text=True, timeout=120)
    if r.returncode != 2:
        problems.append("无参调用期望 rc=2（§2.1 退役契约），实得 %d" % r.returncode)
    if "RETIRED" not in (r.stdout + r.stderr):
        problems.append("无参调用缺退役标识 RETIRED")

    # P1 复跑面：--legacy-scan --out-dir <tmp> ⇒ rc=0 且产物落在指定目录
    tmp = tempfile.mkdtemp(prefix="astrocs_ch_")
    try:
        out = os.path.join(tmp, "out")
        r = subprocess.run([sys.executable, me, "--legacy-scan", "--out-dir", out],
                           capture_output=True, text=True, timeout=600)
        if r.returncode != 0:
            problems.append("--legacy-scan 期望 rc=0，实得 %d: %s" % (r.returncode, r.stderr[:160]))
        if not os.path.isfile(os.path.join(out, "evidence", "quality", "comment_check.json")):
            problems.append("--legacy-scan 未在 --out-dir 下产出 comment_check.json")
        # 产物落点：真仓库根下不得出现 reports/
        if os.path.isdir(os.path.join(ROOT, "reports")):
            problems.append("跑完 --legacy-scan 后仓库根出现 reports/（§7 白名单外）")
        # N1 能红：--strict 在真仓库（171 命中）必须 rc=1
        r2 = subprocess.run([sys.executable, me, "--legacy-scan", "--strict",
                             "--out-dir", os.path.join(tmp, "out2")],
                            capture_output=True, text=True, timeout=600)
        if r2.returncode != 1:
            problems.append("--strict 有命中时期望 rc=1，实得 %d" % r2.returncode)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # N2 锚失效：--root 指向无 lib/ 的空树 ⇒ ANCHOR_STALE + rc=2（不得静默判绿）
    empty = tempfile.mkdtemp(prefix="astrocs_ch_empty_")
    try:
        r = subprocess.run([sys.executable, me, "--legacy-scan", "--root", empty,
                            "--out-dir", os.path.join(empty, "out")],
                           capture_output=True, text=True, timeout=120)
        if r.returncode != 2 or "ANCHOR_STALE" not in r.stderr:
            problems.append("空扫描根期望 ANCHOR_STALE + rc=2，实得 rc=%d stderr=%r"
                            % (r.returncode, r.stderr[:160]))
    finally:
        shutil.rmtree(empty, ignore_errors=True)

    for p in problems:
        print("  - %s" % p)
    print("SELF_TEST %s positives=2 negatives=2" % ("PASS" if not problems else "FAIL"))
    return 0 if not problems else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--legacy-scan", action="store_true", dest="legacy_scan",
                    help="复跑退役前的原扫描实现（报告型；--strict 时命中即 rc=1）")
    ap.add_argument("--strict", action="store_true",
                    help="审计口径：有命中即 rc=1（默认 rc=0 仅表示扫描完成）")
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--out-dir", default=None,
                    help="产物目录（默认 run/comment-hygiene/；禁止写 reports/**）")
    ap.add_argument("--self-test", action="store_true", dest="self_test")
    args = ap.parse_args(argv)

    if args.self_test:
        return _self_test()
    if not args.legacy_scan:
        # §2.1：退役检查器无参调用时打印退役标识并 exit 2
        print(RETIREMENT_MARKER, file=sys.stderr)
        print("  复跑原实现: --legacy-scan [--strict] [--out-dir DIR]", file=sys.stderr)
        return 2
    root = os.path.abspath(args.root)
    out_dir = os.path.abspath(args.out_dir) if args.out_dir else os.path.join(root, DEFAULT_OUT_DIR_REL)
    return legacy_scan(root, out_dir, args.strict)


if __name__ == "__main__":
    raise SystemExit(main())
