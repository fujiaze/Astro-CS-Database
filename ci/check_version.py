#!/usr/bin/env python3
"""ci/check_version.py — AstroCS 产品版本一致性 CI 快速门 (V81-ADOPT-006)。

用法:
  python3 ci/check_version.py --expected 0.11.0-alpha.2 [--root <repo_root>]
  (默认 root = 本脚本所在 ci/ 的上一级, 即仓库根; --root 仅用于对 /tmp fake 树
   做负向样例/自测, 脚本本身只读, 不写任何文件。)

检查规则 (写死, 无豁免开关; 与 docs/governance/VERSION_NAMESPACES.md GOV-003
"根 VERSION 唯一事实源/生成链禁止手抄" 合同一致):

  [1] expected 格式: 必须匹配 ^\\d+\\.\\d+\\.\\d+-alpha\\.\\d+$ (禁 stable/rc/beta)。
  [2] 根 VERSION 文件: strip 后必须完全 == expected (唯一事实源)。
  [3] 根 CMakeLists.txt project(): 统一后的写法 = 纯数字三元组, 规则为
      "project() 的 major.minor.patch 必须 == expected 去掉 -alpha.N 的基础号"
      (CMake project(VERSION) 语法只接受数字点分组件, 不接受 -alpha.N 后缀;
       alpha.N 只由根 VERSION 携带, 生成串 ASTROCS_VERSION_STRING 由
       file(READ VERSION) + git commit 拼装)。因此本检查不接受 "project() 显式
       写完整 expected" 的形态 —— 出现非数字后缀即 FAIL (会导致 CMake 配置错误)。
  [4] CLI 版本定义点 (实际形态: 无手抄常量, 全部由生成链注入):
      a) 根 CMakeLists.txt 必须 file(READ .../VERSION ASTROCS_BASE_VERSION);
      b) cli/CMakeLists.txt 必须 file(READ .../../VERSION BASE_VERSION);
      c) cli/version_generated.h.in 必须含注入点 @ASTROCS_VERSION_STRING@;
      d) 根与 cli 的 CMakeLists.txt 必须 configure_file 生成 version_generated.h;
      e) cli/**(.h/.hpp/.cpp/.cc/.in/.cmake/.txt) 与根 CMakeLists.txt 中出现的
         任何 X.Y.Z-alpha.N 字面量必须 == expected (漂移即 FAIL; 若出现与
         expected 相等的字面量, 值上 PASS, 但 detail 标注"手抄字面量, 建议迁移
         生成链")。
  [5] 活动文档统一 (GOV-003 硬判面 + 本任务收敛面): README.md / REVIEW.md /
      HANDOVER.md / docs/DOCUMENT_INDEX.yaml / docs/VERSIONING.md /
      docs/governance/** / docs/owner/** 中出现的任何 X.Y.Z-alpha.N 字面量必须
      == expected; 行级豁免仅限机器修订关系字段 (REV_FIELD: source_main_version/
      target_main_version/base_product_version/source_main_sha/base_main_sha/
      product_version/doc_version) —— 这些是"记录来源/基线"的机器字段, 不是
      当前值陈述。裸 X.Y.Z 三元组不扫 (FITS 4.0/HiPS 1.0/外部组件/占位属其他
      命名空间, 反误报口径同 GOV-003 §3 豁免表)。
      注意: CHANGELOG.md 与 memory.md 是 history/日志命名空间驻留点 (GOV-003
      §2/§4: 只警告不硬判), 不进本检查; AstroCS_ENGINEERING_CONSTRAINTS.md 是
      负责人冻结文件 (Agent 不可改), 其目标版本行由负责人修订, 也不进本检查。
  [6] 活动文档集合完整性: [5] 列出的固定文件必须存在 (缺文件 = FAIL, 防止
      "文件被移走后检查静默变空")。

输出: stdout 一份 JSON 摘要 (各检查项: 文件/行号/检测值/PASS|FAIL);
      任一 FAIL → exit 1, 全部 PASS → exit 0。脚本只读。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

SEMVER_ALPHA = re.compile(r"^\d+\.\d+\.\d+-alpha\.\d+$")
ALPHA_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)-alpha\.(\d+)(?![\w.])")
PROJECT_RE = re.compile(
    r"project\(\s*([A-Za-z0-9_\-]+)\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\s")
REV_FIELD = re.compile(
    r"^(source_main_version|target_main_version|base_product_version|"
    r"source_main_sha|base_main_sha|product_version|doc_version)\s*[:=]\s*")

DOC_SET_FILES = [
    "README.md", "REVIEW.md", "HANDOVER.md",
    "docs/DOCUMENT_INDEX.yaml", "docs/VERSIONING.md",
]
DOC_SET_DIRS = ["docs/governance", "docs/owner"]
DOC_SCAN_EXT = (".md", ".txt", ".json", ".yaml", ".yml", ".py", ".sh")
CLI_SCAN_EXT = (".h", ".hpp", ".cpp", ".cc", ".in", ".cmake", ".txt")


def add(checks: list, cid: str, ok: bool, file: str, line=None,
        detected="", detail="") -> None:
    checks.append({"id": cid, "pass": bool(ok), "file": file,
                   "line": line, "detected": detected, "detail": detail})


def read_text(path: str) -> str:
    with open(path, encoding="utf-8", errors="replace") as f:
        return f.read()


def base_of(expected: str) -> tuple:
    return tuple(int(x) for x in expected.split("-alpha.")[0].split("."))


def version_tuple(s: str) -> tuple:
    return tuple(int(x) for x in s.split("."))


def main() -> int:
    ap = argparse.ArgumentParser(description="AstroCS 产品版本一致性 CI 快速门")
    ap.add_argument("--expected", required=True,
                    help="期望产品版本, 如 0.11.0-alpha.2")
    ap.add_argument("--root", default=None,
                    help="仓库根 (默认: 脚本位置上一级; 只读)")
    args = ap.parse_args()
    root = os.path.abspath(args.root or os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    checks: list = []

    # [1] expected 格式
    add(checks, "expected_format", bool(SEMVER_ALPHA.match(args.expected)),
        "<argument>", None, args.expected,
        "必须形如 MAJOR.MINOR.PATCH-alpha.N (禁 stable/rc/beta)")
    if not SEMVER_ALPHA.match(args.expected):
        print(json.dumps({"tool": "ci/check_version.py", "expected": args.expected,
                          "root": root, "checks": checks,
                          "fail_count": 1, "pass_count": 0,
                          "verdict": "VERSION_CHECK_FAIL"},
                         ensure_ascii=False, indent=2))
        return 1
    exp_base = base_of(args.expected)

    # [2] 根 VERSION 文件
    vpath = os.path.join(root, "VERSION")
    if os.path.isfile(vpath):
        ver = read_text(vpath).strip()
        add(checks, "version_file", ver == args.expected, "VERSION", 1, ver,
            "根 VERSION 必须 == expected (唯一事实源)")
    else:
        add(checks, "version_file", False, "VERSION", None, "缺失",
            "根 VERSION 文件不存在")

    # [3] 根 CMakeLists.txt project() 数字三元组 == expected 基础号
    cml = os.path.join(root, "CMakeLists.txt")
    cml_text = read_text(cml) if os.path.isfile(cml) else ""
    proj_hit = None
    for i, ln in enumerate(cml_text.splitlines(), 1):
        m = PROJECT_RE.search(ln)
        if m:
            proj_hit = (i, m.group(1), m.group(2))
            break
    if proj_hit:
        i, name, pv = proj_hit
        ok = version_tuple(pv) == exp_base
        add(checks, "cmake_project_base", ok, "CMakeLists.txt", i,
            f"project({name} VERSION {pv})",
            "规则: project() 数字三元组 == expected 去 -alpha.N 的基础号 "
            "(CMake project() 不接受 alpha 后缀, 后缀由根 VERSION/生成链承载)")
    else:
        add(checks, "cmake_project_base", False, "CMakeLists.txt", None,
            "未找到 project(... VERSION ...)",
            "根 CMakeLists.txt 必须含唯一 project() 版本声明")

    # [4] CLI 生成链定义点
    def first_line(text: str, rx: re.Pattern):
        for i, ln in enumerate(text.splitlines(), 1):
            if rx.search(ln):
                return i, ln.strip()
        return None, None

    i, ln = first_line(cml_text, re.compile(
        r"file\(READ\s+\$\{CMAKE_CURRENT_SOURCE_DIR\}/VERSION\s+ASTROCS_BASE_VERSION"))
    add(checks, "chain_root_read_version", i is not None, "CMakeLists.txt", i,
        ln or "缺失", "根 CMakeLists.txt 必须 file(READ 根 VERSION) (禁止手抄)")
    cli_cml_path = os.path.join(root, "cli", "CMakeLists.txt")
    cli_cml = read_text(cli_cml_path) if os.path.isfile(cli_cml_path) else ""
    i, ln = first_line(cli_cml, re.compile(
        r"file\(READ\s+\$\{CMAKE_CURRENT_SOURCE_DIR\}/\.\./VERSION\s+BASE_VERSION"))
    add(checks, "chain_cli_read_version", i is not None, "cli/CMakeLists.txt", i,
        ln or "缺失", "cli/CMakeLists.txt 必须 file(READ 根 ../VERSION) (禁止手抄)")
    tpl_path = os.path.join(root, "cli", "version_generated.h.in")
    if os.path.isfile(tpl_path):
        tpl = read_text(tpl_path)
        i, ln = first_line(tpl, re.compile(r"@ASTROCS_VERSION_STRING@"))
        add(checks, "chain_template_placeholder", i is not None,
            "cli/version_generated.h.in", i, ln or "缺失",
            "版本注入模板必须含 @ASTROCS_VERSION_STRING@ 占位")
    else:
        add(checks, "chain_template_placeholder", False,
            "cli/version_generated.h.in", None, "缺失", "版本注入模板不存在")
    i, ln = first_line(cml_text, re.compile(
        r"configure_file\(cli/version_generated\.h\.in"))
    add(checks, "chain_configure_file_root", i is not None, "CMakeLists.txt", i,
        ln or "缺失", "根 CMakeLists.txt 必须 configure_file 生成 version_generated.h")
    i, ln = first_line(cli_cml, re.compile(
        r"configure_file\(version_generated\.h\.in"))
    add(checks, "chain_configure_file_cli", i is not None,
        "cli/CMakeLists.txt", i, ln or "缺失",
        "cli/CMakeLists.txt (兼容 target) 必须 configure_file 生成 version_generated.h")

    # [4e] cli/** 与根 CMakeLists.txt 的 alpha 字面量扫描
    lit_rows = []
    scan_targets = []
    cli_dir = os.path.join(root, "cli")
    if os.path.isdir(cli_dir):
        for dirpath, dirnames, filenames in os.walk(cli_dir):
            dirnames[:] = [x for x in dirnames if not x.startswith("__")]
            for fn in filenames:
                if fn.endswith(CLI_SCAN_EXT):
                    scan_targets.append(os.path.join(dirpath, fn))
    if os.path.isfile(cml):
        scan_targets.append(cml)
    for p in scan_targets:
        try:
            text = read_text(p)
        except OSError as exc:
            lit_rows.append((p, None, None, f"读取失败: {exc}"))
            continue
        for i, ln in enumerate(text.splitlines(), 1):
            for m in ALPHA_INLINE.finditer(ln):
                val = f"{m.group(1)}-alpha.{m.group(2)}"
                lit_rows.append((os.path.relpath(p, root), i, val, ln.strip()[:90]))
    bad = [(f, i, v, t) for (f, i, v, t) in lit_rows if v != args.expected]
    handcopy = [(f, i, v) for (f, i, v, t) in lit_rows if v == args.expected]
    add(checks, "literal_scan_cli_and_cmake", not bad,
        "cli/** + CMakeLists.txt",
        None if not bad else bad[0][1],
        "0 处 alpha 字面量 (生成链注入)" if not lit_rows
        else f"{len(lit_rows)} 处 (漂移 {len(bad)})",
        "规则: 任何 X.Y.Z-alpha.N 字面量必须 == expected"
        + (f"; 漂移: {bad[0][0]}:{bad[0][1]}={bad[0][2]}" if bad else "")
        + (f"; 手抄但同值 {len(handcopy)} 处 (建议迁移生成链)" if handcopy else ""))

    # [5] 活动文档 alpha 字面量统一
    doc_files = []
    for rel in DOC_SET_FILES:
        p = os.path.join(root, rel)
        if os.path.isfile(p):
            doc_files.append((rel, p))
    for d in DOC_SET_DIRS:
        base = os.path.join(root, d)
        if os.path.isdir(base):
            for dirpath, dirnames, filenames in os.walk(base):
                dirnames[:] = [x for x in dirnames if not x.startswith("__")]
                for fn in sorted(filenames):
                    if fn.endswith(DOC_SCAN_EXT):
                        p = os.path.join(dirpath, fn)
                        doc_files.append((os.path.relpath(p, root), p))
    drift, scanned = [], 0
    for rel, p in doc_files:
        scanned += 1
        for i, ln in enumerate(read_text(p).splitlines(), 1):
            if REV_FIELD.match(ln.strip()):
                continue  # 机器修订关系字段 (记录来源/基线), 非"当前值"陈述
            for m in ALPHA_INLINE.finditer(ln):
                val = f"{m.group(1)}-alpha.{m.group(2)}"
                if val != args.expected:
                    drift.append((rel, i, val, ln.strip()[:90]))
    add(checks, "doc_scan_active_docs", not drift, "README/REVIEW/HANDOVER/docs",
        None if not drift else drift[0][1],
        f"扫描 {scanned} 份活动文档" if not drift else
        f"{scanned} 份中 {len(drift)} 处漂移",
        "规则: 活动文档中任何 X.Y.Z-alpha.N 字面量必须 == expected"
        " (豁免仅 REV_FIELD 机器修订字段; CHANGELOG/memory 为 history/日志"
        " 命名空间不进本检查, 冻结约束文件由负责人修订)"
        + (f"; 首处漂移 {drift[0][0]}:{drift[0][1]}={drift[0][2]}" if drift else ""))

    # [6] 活动文档集合完整性
    missing = [rel for rel in DOC_SET_FILES
               if not os.path.isfile(os.path.join(root, rel))]
    missing += [d + "/" for d in DOC_SET_DIRS
                if not os.path.isdir(os.path.join(root, d))]
    add(checks, "doc_set_complete", not missing, "README/REVIEW/HANDOVER/docs",
        None, f"缺失: {missing}" if missing else f"{len(DOC_SET_FILES)} 文件 + "
        f"{len(DOC_SET_DIRS)} 目录齐全",
        "规则: [5] 的固定活动文档必须存在, 防止检查面被移空")

    fails = [c for c in checks if not c["pass"]]
    out = {
        "tool": "ci/check_version.py",
        "expected": args.expected,
        "root": root,
        "checks": checks,
        "fail_count": len(fails),
        "pass_count": len(checks) - len(fails),
        "verdict": "VERSION_CHECK_FAIL" if fails else "VERSION_CHECK_PASS",
    }
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
