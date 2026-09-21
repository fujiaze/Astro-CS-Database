#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CHK-NO-WEIGHT-MODE：详细文档层「权重模式」整套作废门（DOC-201 / §9.73 A44）。

权威依据
  - `工程控制/RELEASE-02/GAP_AUDIT.md` §9.73 裁决 A44（负责人逐字：不存在「权重模式」，
    全程都是 SNR；权重只在阶段二消费 SNR 时按该天位像素对应帧集合现场算出）；
  - `ASTROCS_DESIGN.md` §2.1（总纲：全程只有 SNR，不存在「权重模式」这个概念；
    禁止任何「权重模式 / 权重档位 / 默认权重模式 / mode0·mode1·mode2」的键名、枚举、配置项或产物）；
  - `ENGINEERING_SPEC.md` §8（每项检查有正例与负例、能红能绿；fail-closed；锚存活）；
  - `工程控制/RELEASE-03/tasks/DOC-201.md`（验收门 1/2 + 「新增负例 ⇒ 检查器判红」）。

判据（确定性，与 `grep -rn` 同口径）
  R1  ASCII 键族：`docs/**` 与 `README.md` 中**每一行**含 `weight_mode` 的行，
      必须同行含留痕串「已按 §9.73 A44 作废」；否则判红。
      （历史/归档/冻结层允许保留原文 + 同行留痕；活文档层应当无该串。）
  R2  中文概念：同一扫描面中每一行含「权重模式」的行，必须同行含「不存在」或「已作废」。
  R3  fail-closed：`docs/` 或 `README.md` 缺失 ⇒ rc=2（禁止把「文件不存在」当「无违规」）。

用法
  python3 eng/ci/check_no_weight_mode.py                 # 扫真实仓库，rc=0 全绿
  python3 eng/ci/check_no_weight_mode.py --json-out F    # 机器可读结果（原子写）
  python3 eng/ci/check_no_weight_mode.py --self-test     # 临时目录正例/负例，证明能红能绿
退出码：0 PASS；1 FAIL；2 输入不可用（fail-closed）。
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile

ASCII_TOKEN = "weight_mode"
ASCII_MARK = "已按 §9.73 A44 作废"
ZH_TOKEN = "权重模式"
ZH_MARKS = ("不存在", "已作废")
SCAN_DIR = "docs"
SCAN_FILE = "README.md"
SKIP_DIRS = {".git", "build", "run", "__pycache__", ".venv", "node_modules"}
TEXT_EXT = {".md", ".json", ".yaml", ".yml", ".csv", ".txt", ".py", ".rst"}


def iter_scan_files(root):
    """扫描面 = docs/**（跳过重型目录）+ 仓库根 README.md。确定性顺序。"""
    files = []
    docs = os.path.join(root, SCAN_DIR)
    if not os.path.isdir(docs):
        return None
    for dirpath, dirnames, filenames in os.walk(docs):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        for name in sorted(filenames):
            if os.path.splitext(name)[1].lower() in TEXT_EXT:
                files.append(os.path.join(dirpath, name))
    readme = os.path.join(root, SCAN_FILE)
    if not os.path.isfile(readme):
        return None
    files.append(readme)
    return files


def scan(root):
    """返回 (findings, scanned_files, scanned_lines)。findings 为稳定排序的 dict 列表。"""
    files = iter_scan_files(root)
    if files is None:
        return None, 0, 0
    findings, lines_total = [], 0
    for path in files:
        try:
            with open(path, encoding="utf-8", errors="replace") as fh:
                lines = fh.read().split("\n")
        except OSError:
            return None, 0, 0
        rel = os.path.relpath(path, root).replace(os.sep, "/")
        for idx, line in enumerate(lines, start=1):
            lines_total += 1
            if ASCII_TOKEN in line and ASCII_MARK not in line:
                findings.append({"rule": "R1-ASCII-KEY", "file": rel, "line": idx,
                                 "observed": line.strip()[:160],
                                 "expected": "同行含「%s」留痕（§9.73 A44）" % ASCII_MARK})
            if ZH_TOKEN in line and not any(m in line for m in ZH_MARKS):
                findings.append({"rule": "R2-ZH-CONCEPT", "file": rel, "line": idx,
                                 "observed": line.strip()[:160],
                                 "expected": "同行含「不存在」或「已作废」留痕（§9.73 A44）"})
    findings.sort(key=lambda f: (f["file"], f["line"], f["rule"]))
    return findings, len(files), lines_total


def run(root, json_out=None):
    findings, nfiles, nlines = scan(root)
    if findings is None:
        print("CHK-NO-WEIGHT-MODE_FAIL: 扫描面不可用（缺 %s/ 或 %s）—— fail-closed"
              % (SCAN_DIR, SCAN_FILE), file=sys.stderr)
        return 2
    result = {
        "tool": "check_no_weight_mode",
        "authority": ["GAP_AUDIT.md §9.73 A44", "ASTROCS_DESIGN.md §2.1",
                      "工程控制/RELEASE-03/tasks/DOC-201.md"],
        "root": os.path.abspath(root),
        "files_scanned": nfiles,
        "lines_scanned": nlines,
        "findings": findings,
        "passed": not findings,
    }
    if json_out:
        os.makedirs(os.path.dirname(os.path.abspath(json_out)) or ".", exist_ok=True)
        tmp = json_out + ".tmp"
        with open(tmp, "w", encoding="utf-8") as fh:
            json.dump(result, fh, ensure_ascii=False, indent=2, sort_keys=True)
            fh.write("\n")
        os.replace(tmp, json_out)
    if findings:
        print("CHK-NO-WEIGHT-MODE_FAIL: %d 处无留痕命中（§9.73 A44）" % len(findings))
        for f in findings[:20]:
            print("  [%s] %s:%d  %s" % (f["rule"], f["file"], f["line"], f["observed"]))
        if len(findings) > 20:
            print("  ... 其余 %d 处见 --json-out" % (len(findings) - 20))
        return 1
    print("CHK-NO-WEIGHT-MODE_PASS: files=%d lines=%d 无未留痕命中（§9.73 A44）"
          % (nfiles, nlines))
    return 0


def _write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as fh:
        fh.write(text)


def self_test():
    """临时目录正例/负例：证明本检查器能红能绿（ENGINEERING_SPEC §8）。"""
    tmp = tempfile.mkdtemp(prefix="chk-no-weight-mode-")
    cases, problems = [], []
    try:
        # 正例（绿）：冻结层留痕 + 活文档层无该串
        green = os.path.join(tmp, "green")
        _write(os.path.join(green, "docs", "frozen", "a.md"),
               "# 历史件\nweight_mode = 2（已按 §9.73 A44 作废：该概念不存在）\n")
        _write(os.path.join(green, "docs", "live", "b.md"),
               "# 活文档\n权重 = 阶段二按像素集合现场算的派生量\n")
        _write(os.path.join(green, "README.md"), "# 仓库\n")
        rc = run(green)
        cases.append(("green_clean", rc, 0))
        # 负例（红）：一处裸 weight_mode
        red = os.path.join(tmp, "red")
        _write(os.path.join(red, "docs", "live", "b.md"),
               "# 活文档\n| weight_mode | 2 |\n")
        _write(os.path.join(red, "README.md"), "# 仓库\n")
        rc = run(red)
        cases.append(("red_bare_weight_mode", rc, 1))
        # 负例（红）：一处裸「权重模式」
        red2 = os.path.join(tmp, "red2")
        _write(os.path.join(red2, "docs", "live", "c.md"),
               "# 活文档\nPhase2 生产权重模式 = point_information\n")
        _write(os.path.join(red2, "README.md"), "# 仓库\n")
        rc = run(red2)
        cases.append(("red_bare_zh_concept", rc, 1))
        # 负例（红）：留痕串被删（仅剩「已作废」泛词不含 §9.73 A44）
        red3 = os.path.join(tmp, "red3")
        _write(os.path.join(red3, "docs", "frozen", "d.md"),
               "# 历史件\nweight_mode = 2（已作废）\n")
        _write(os.path.join(red3, "README.md"), "# 仓库\n")
        rc = run(red3)
        cases.append(("red_marker_without_a44", rc, 1))
        # fail-closed：缺 docs/
        bad = os.path.join(tmp, "failclosed")
        _write(os.path.join(bad, "README.md"), "# 仓库\n")
        rc = run(bad)
        cases.append(("failclosed_missing_docs", rc, 2))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    for name, got, want in cases:
        ok = got == want
        print("SELFTEST_%s %s (rc=%d want=%d)" % ("PASS" if ok else "FAIL", name, got, want))
        if not ok:
            problems.append(name)
    if problems:
        print("SELF_TEST FAIL cases=%d problems=%s" % (len(cases), problems))
        return 1
    print("SELF_TEST PASS cases=%d" % len(cases))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="§9.73 A44「权重模式」作废门（DOC-201）")
    ap.add_argument("--root", default=".", help="仓库根（默认 .）")
    ap.add_argument("--json-out", default=None, help="机器可读结果输出路径（原子写）")
    ap.add_argument("--self-test", action="store_true", help="临时目录正例/负例自检")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    return run(args.root, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
