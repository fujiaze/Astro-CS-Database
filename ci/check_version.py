#!/usr/bin/env python3
"""ci/check_version.py — AstroCS 产品版本一致性 CI 快速门 (V81-ADOPT-006)。

用法:
  python3 ci/check_version.py --expected 0.11.0-alpha.2 [--root <repo_root>]
  python3 ci/check_version.py [--root <repo_root>]   # 缺省: expected 取自根 VERSION
  python3 ci/check_version.py --self-test            # 机器可执行负例面 (tempfile mini-repo)

检查规则 (写死, 无豁免开关; 与 docs/governance/VERSION_NAMESPACES.md GOV-003
"根 VERSION 唯一事实源/生成链禁止手抄" 合同一致):

  [0] 锚存活 (ENGINEERING_SPEC §8): 本文件硬编码引用的仓库路径常量在启动时逐条校验:
      a) ANCHORS 表 (文件/目录存在性): os.path.exists 必需, 且当 --root 恰为 git
         工作树根时追加 "git ls-files --error-unmatch <path>" 跟踪复核;
      b) REF_ANCHORS 表 (路径引用存活): 引用方文件中必须仍出现指向该路径的字面量
         (例: 根 CMakeLists.txt 的 configure_file(lib/infrastructure/cli/version_generated.h.in ...));
         目录迁移后引用被改指他处 ⇒ 本文件的绑定失效, 必须点名而非静默降级。
      任一失效 ⇒ stderr 打印 "ANCHOR_STALE: <常量名> <路径>" ⇒ exit 2 (fail-closed;
      不 traceback, 不静默通过)。摘要 JSON 里逐条给出 anchor_alive_*/ref_alive_*
      判据, 便于 CI 定位失效的常量与路径。
  [1] expected 格式: 必须匹配 ^\\d+\\.\\d+\\.\\d+-alpha\\.\\d+$ (禁 stable/rc/beta)。
      --expected 缺省时取根 VERSION 的 strip 值 (读不到 ⇒ 锚失效 ⇒ exit 2), 判定规则不变。
  [2] 根 VERSION 文件: strip 后必须完全 == expected (唯一事实源)。
  [3] 根 CMakeLists.txt project(): 统一后的写法 = 纯数字三元组, 规则为
      "project() 的 major.minor.patch 必须 == expected 去掉 -alpha.N 的基础号"
      (CMake project(VERSION) 语法只接受数字点分组件, 不接受 -alpha.N 后缀;
       alpha.N 只由根 VERSION 携带, 生成串 ASTROCS_VERSION_STRING 由
       file(READ VERSION) + git commit 拼装)。因此本检查不接受 "project() 显式
       写完整 expected" 的形态 —— 出现非数字后缀即 FAIL (会导致 CMake 配置错误)。
  [4] CLI 版本定义点 (实际形态: 无手抄常量, 全部由生成链注入):
      a) 根 CMakeLists.txt 必须 file(READ .../VERSION ASTROCS_BASE_VERSION);
      b) 根 CMakeLists.txt 必须 configure_file 生成 version_generated.h;
      c) lib/infrastructure/cli/version_generated.h.in 必须含注入点 @ASTROCS_VERSION_STRING@;
      d) **[W4-A3 退役] 原「cli/CMakeLists.txt 必须 file(READ ../VERSION)」与
         「cli/CMakeLists.txt 必须 configure_file」两条 chain 判据 + 两条
         CLI_CMAKE_* REF_ANCHORS 已删除**。该文件是 ROOT-008 迁移后遗留的旧 build
         文件(仍引用 main.cpp / lib/phase1/** / lib/backend_host/** 等已不存在路径),
         且其 configure_file 的输入 cli/version_generated.h.in **既不在 git 也不在
         盘上**(git ls-files 命中 0)。判据引用的目标已消失却仍判 PASS ⇒ 判据空转,
         正是 ENGINEERING_SPEC §8「锚存活」失效型实例。退役后"VERSION 不得删除"的
         硬依据改锚到根 CMakeLists.txt 的两条**真实**判据:
         chain_root_read_version + chain_configure_file_root
         (常量锚 ROOT_CMAKE_REL + CLI_TEMPLATE_REL)，见 ci/root_manifest.json。
      e) lib/infrastructure/cli/**(.h/.hpp/.cpp/.cc/.in/.cmake/.txt) 与根
         CMakeLists.txt 中出现的
         任何 X.Y.Z-alpha.N 字面量必须 == expected (漂移即 FAIL; 若出现与
         expected 相等的字面量, 值上 PASS, 但 detail 标注"手抄字面量, 建议迁移
         生成链")。
  [5] 活动文档统一 (GOV-003 硬判面 + 本任务收敛面): DOC_SET_FILES / DOC_SET_DIRS
      下的任何 X.Y.Z-alpha.N 字面量必须 == expected; 行级豁免仅限机器修订关系字段
      (REV_FIELD: source_main_version/target_main_version/base_product_version/
      source_main_sha/base_main_sha/product_version/doc_version) —— 这些是"记录来源/
      基线"的机器字段, 不是"当前值"陈述。裸 X.Y.Z 三元组不扫 (FITS 4.0/HiPS 1.0/
      外部组件/占位属其他命名空间, 反误报口径同 GOV-003 §3 豁免表)。
      注意: CHANGELOG.md 与 memory.md 是 history/日志命名空间驻留点 (GOV-003
      §2/§4: 只警告不硬判), 不进本检查; AstroCS_ENGINEERING_CONSTRAINTS.md 是
      负责人冻结文件 (Agent 不可改), 其目标版本行由负责人修订, 也不进本检查。
  [6] 活动文档集合完整性 (防移空): DOC_SET_FILES 的每个成员必须存在, DOC_SET_DIRS
      的每个目录必须存在且含 ≥1 个扫描成员; 缺一 ⇒ 该项 FAIL 并逐条点名。
      [5] doc_scan_active_docs 同样显式报缺 (缺成员 ⇒ FAIL + detail 点名),
      不再用 os.path.isfile 静默跳过 (静默跳过 = 扫描面悄悄缩小, 正是"防移空"
      条款自己要防的失效型; R-6 §3.2 C 实测)。

  [7] §12 门方向 (RELEASE-02 CI-HYGIENE 修复): 启动时先探测"版本信息面"是否存在。
      存在 ⇒ 校验一致性 ([1]~[6] 全部照旧, fail-closed);
      不存在 ⇒ 进入"Alpha 前无版本信息"模式 (version_absence_alpha_pre), 不判红。
      旧实现把"版本串必须存在"当硬门 ⇒ 版本信息缺席判 ANCHOR_STALE(exit 2),
      方向与 §12 相反 (CI 绿灯 = 必然违反 §12)。本修复只改门方向, 不改版本号。

输出: stdout 一份 JSON 摘要 (各检查项: 文件/行号/检测值/PASS|FAIL);
      锚失效 → exit 2; 任一 FAIL → exit 1; 全部 PASS → exit 0。脚本只读。
"""
from __future__ import annotations

import argparse
import io
import json
import os
import re
import subprocess
import sys
import tempfile

SEMVER_ALPHA = re.compile(r"^\d+\.\d+\.\d+-alpha\.\d+$")
ALPHA_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)-alpha\.(\d+)(?![\w.])")
PROJECT_RE = re.compile(
    r"project\(\s*([A-Za-z0-9_\-]+)\s+VERSION\s+([0-9]+(?:\.[0-9]+){0,3})\s")
REV_FIELD = re.compile(
    r"^(source_main_version|target_main_version|base_product_version|"
    r"source_main_sha|base_main_sha|product_version|doc_version)\s*[:=]\s*")

# ---- 硬编码仓库路径常量 (锚存活合同; 常量集中于此, 不散落) --------------------
VERSION_REL = "VERSION"
ROOT_CMAKE_REL = "CMakeLists.txt"
# ROOT-008（2026-09-16）目录迁移：cli/** → lib/infrastructure/cli/**（CMake 源树 + 版本模板）。
# CLI_DIR_REL 的扫描面必须随迁，否则 [7] 字面量扫描静默漏扫迁移后的 CLI 源码。
CLI_DIR_REL = "lib/infrastructure/cli"
# [W4-A3 退役] 原 CLI_CMAKE_REL = "cli/CMakeLists.txt" 已删除。
# 退役理由（ENGINEERING_SPEC §8「锚存活」实例）: 该常量支撑的 3 条判据
# (anchor_alive_CLI_CMAKE_REL / chain_cli_read_version / chain_configure_file_cli)
# 与 2 条 REF_ANCHORS(CLI_TEMPLATE_REF_CLI / CLI_BASE_VERSION_READ) 判的是**死对象**:
# 文件内 configure_file 的输入 cli/version_generated.h.in 已从版本库消失
# (`git ls-files cli/version_generated.h.in` = 0 命中，盘上亦不存在)，
# 而链式判据只匹配文件内**字符串**、从不校验被引用目标是否存活 ⇒ 恒判 PASS。
# 「VERSION 不得删除」的硬依据改锚到根 CMakeLists.txt 的真实判据
# (chain_root_read_version :51 + chain_configure_file_root :62)，见 ci/root_manifest.json。
CLI_TEMPLATE_REL = "lib/infrastructure/cli/version_generated.h.in"

# [5]/[6] 现行活动文档集 (逐条绑定, 与 GOV-003 硬判面一致; 迁移时同步更新本表)。
DOC_SET_FILES = [
    "README.md",
    "docs/DOCUMENT_INDEX.yaml",
    "docs/VERSIONING.md",
    "docs/governance/VERSION_NAMESPACES.md",
    "docs/owner/ARCHITECTURE_OVERVIEW.md",
    "docs/owner/CHANGE_REVIEW.md",
    "docs/owner/PIPELINE_OVERVIEW.md",
    "docs/owner/PROJECT_SPEC.md",
    "docs/owner/RELEASE_STATUS.md",
    "docs/owner/SCIENCE_OVERVIEW.md",
]
DOC_SET_DIRS = ["docs/governance", "docs/owner"]
DOC_SCAN_EXT = (".md", ".txt", ".json", ".yaml", ".yml", ".py", ".sh")
CLI_SCAN_EXT = (".h", ".hpp", ".cpp", ".cc", ".in", ".cmake", ".txt")

# 锚表: (常量名, 仓库相对路径)。DOC_SET_DIRS 目录锚额外要求"非空"(见 [6])。
ANCHORS = (
    [("VERSION_REL", VERSION_REL),
     ("ROOT_CMAKE_REL", ROOT_CMAKE_REL),
     ("CLI_DIR_REL", CLI_DIR_REL),
     ("CLI_TEMPLATE_REL", CLI_TEMPLATE_REL)]
    + [("DOC_SET_FILES[%d]" % i, rel) for i, rel in enumerate(DOC_SET_FILES)]
    + [("DOC_SET_DIRS[%d]" % i, d) for i, d in enumerate(DOC_SET_DIRS)]
)

# CMake 变量引用字面量 (拆写, 避免在工具链源码里出现未转义的插值序列)。
CSD = "$" + "{CMAKE_CURRENT_SOURCE_DIR}"

# 引用存活表: (常量名, 被引用路径, 引用方文件, 必须在引用方出现的字面量正则)。
# 目录迁移 (ROOT-008 等) 后引用被改指他处 ⇒ ANCHOR_STALE, 不静默降级为普通 FAIL。
REF_ANCHORS = [
    ("CLI_TEMPLATE_REF_ROOT", CLI_TEMPLATE_REL, ROOT_CMAKE_REL,
     re.compile(r"configure_file\(lib/infrastructure/cli/version_generated\.h\.in")),
    # [W4-A3 退役] CLI_TEMPLATE_REF_CLI / CLI_BASE_VERSION_READ 已删除：
    # 二者判的是 cli/CMakeLists.txt 内的字符串，而被引用目标
    # (cli/version_generated.h.in) 已不在版本库 ⇒ 判据空转（§8 锚存活失效型）。
]

# §12 版本纪律修复（RELEASE-02 CI-HYGIENE）：
#   设计 §12「Alpha 之前：程序与代码中不包含任何版本信息」。
#   旧实现把「版本串必须存在且一致」当硬门 ⇒ 版本信息不存在时判红（ANCHOR_STALE），
#   方向与 §12 相反（CI 绿灯 = 必然违反 §12）。新判据：
#     · 版本信息存在  ⇒ 校验一致性（[1]~[6] 全部照旧，fail-closed）；
#     · 版本信息不存在 ⇒ 不得判红（本文件的版本锚/生成链判据无对象，显式进入
#       "Alpha 前无版本信息" 模式，仍校验非版本锚与文档集完整性，防移空）。
#   注意：本修复只改门方向，不改任何版本号；版本信息当前仍存在于仓库，
#   其去留属负责人裁决（DC-718 / 控制包 P0-15），不在本包范围。
VERSION_ANCHOR_NAMES = ("VERSION_REL", "CLI_TEMPLATE_REL")

GIT_TIMEOUT_S = 30


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


# ---- [0] 锚存活 -------------------------------------------------------------
def git_toplevel(root: str):
    """root 为 git 工作树根时返回其真实路径, 否则 None (非 git 树 ⇒ 跳过跟踪复核)。"""
    try:
        p = subprocess.run(["git", "-C", root, "rev-parse", "--show-toplevel"],
                           capture_output=True, text=True, timeout=GIT_TIMEOUT_S)
    except Exception:
        return None
    if p.returncode != 0:
        return None
    top = os.path.realpath(p.stdout.strip())
    return top if top == os.path.realpath(root) else None


def git_tracked(root: str, rel: str):
    """(True|False|None, 说明): None = git 不可用/无法判定。"""
    try:
        p = subprocess.run(["git", "-C", root, "ls-files", "--error-unmatch",
                            "--", rel], capture_output=True, text=True,
                           timeout=GIT_TIMEOUT_S)
    except Exception as exc:
        return None, "git 调用失败: %s" % exc
    if p.returncode == 0:
        return True, "ls-files --error-unmatch rc=0"
    last = (p.stderr.strip().splitlines() or [""])[-1]
    return False, "ls-files --error-unmatch rc=%d (%s)" % (p.returncode, last[:120])


def anchor_status(root: str, rel: str) -> tuple:
    """(存活?, 说明)。双查: os.path.exists 必需 + (git 树根时) 跟踪复核。"""
    full = os.path.join(root, rel)
    if not os.path.exists(full):
        return False, "路径不存在 (os.path.exists=False)"
    if git_toplevel(root) is None:
        return True, "存在 (root 非 git 工作树根, 未做跟踪复核)"
    ok, why = git_tracked(root, rel)
    if ok is False:
        return False, "存在但 git 未跟踪: %s" % why
    if ok is None:
        return True, "存在 (git 不可用, 未做跟踪复核: %s)" % why
    return True, "存在 + git 跟踪 (%s)" % why


def check_anchors(root: str, skip=()) -> tuple:
    """返回 (checks, stale_lines)。stale 非空 ⇒ fail-closed exit 2。

    skip: 需跳过的锚常量名集合。§12 修复后仅"版本信息不存在"模式用它跳过
    版本专用锚 (VERSION_REL/CLI_TEMPLATE_REL) —— 这些锚的判据对象本就不该存在,
    跳过是"无对象"而非"放宽判据"; 非版本锚 (CMake/CLI 源树/文档集) 照旧 fail-closed。
    """
    checks, stale = [], []
    for name, rel in ANCHORS:
        if name in skip:
            add(checks, "anchor_alive_%s" % name, True, rel, None, "SKIPPED(无版本信息)",
                "§12 修复: 版本信息不存在 ⇒ 版本专用锚无判据对象, 显式跳过并留痕")
            continue
        ok, detail = anchor_status(root, rel)
        add(checks, "anchor_alive_%s" % name, ok, rel, None,
            "存活" if ok else "ANCHOR_STALE",
            "锚存活合同 (ENGINEERING_SPEC §8): %s" % detail)
        if not ok:
            stale.append("ANCHOR_STALE: %s %s" % (name, rel))
    return checks, stale


def ref_anchor_status(root: str, ref: str, holder: str, rx: re.Pattern) -> tuple:
    """引用存活: holder 文件中必须仍出现指向 ref 的字面量。"""
    hfull = os.path.join(root, holder)
    if not os.path.isfile(hfull):
        return False, "引用方文件不存在: %s" % holder
    for ln in read_text(hfull).splitlines():
        if rx.search(ln):
            return True, "引用在位: %s:%s" % (holder, ln.strip()[:100])
    return False, ("引用失效: %s 中未出现指向 %s 的字面量 (绑定可能已随目录迁移改指他处)"
                   % (holder, ref))


def check_ref_anchors(root: str) -> tuple:
    """返回 (checks, stale_lines)。路径引用失效 ⇒ fail-closed exit 2。"""
    checks, stale = [], []
    for name, ref, holder, rx in REF_ANCHORS:
        ok, detail = ref_anchor_status(root, ref, holder, rx)
        add(checks, "ref_alive_%s" % name, ok, holder, None,
            "存活" if ok else "ANCHOR_STALE",
            "路径引用存活合同 (ENGINEERING_SPEC §8): %s" % detail)
        if not ok:
            stale.append("ANCHOR_STALE: %s %s" % (name, ref))
    return checks, stale


# ---- [5]/[6] 活动文档集扫描面 ------------------------------------------------
def doc_scan_set(root: str) -> tuple:
    """返回 (doc_files[(rel, full)], missing_members[rel], empty_dirs[dir])。

    DOC_SET_FILES 成员缺失不再静默跳过 —— 由调用方判 FAIL 并点名 [6]/[5]。
    DOC_SET_DIRS 目录缺失或扫描成员数为 0 同样显式登记 (防"目录空了但看着通过")。
    目录内成员与显式成员按 realpath 去重 (显式绑定 + 递归扫描并存时只扫一次)。
    """
    doc_files, missing, empty = [], [], []
    seen = set()

    def push(rel: str, full: str) -> None:
        key = os.path.realpath(full)
        if key in seen:
            return
        seen.add(key)
        doc_files.append((rel, full))

    for rel in DOC_SET_FILES:
        full = os.path.join(root, rel)
        if os.path.isfile(full):
            push(rel, full)
        else:
            missing.append(rel)
    for d in DOC_SET_DIRS:
        base = os.path.join(root, d)
        if not os.path.isdir(base):
            empty.append(d + "/ (目录缺失)")
            continue
        n = 0
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if not x.startswith("__")]
            for fn in sorted(filenames):
                if fn.endswith(DOC_SCAN_EXT):
                    full = os.path.join(dirpath, fn)
                    push(os.path.relpath(full, root), full)
                    n += 1
        if n == 0:
            empty.append(d + "/ (0 个扫描成员)")
    return doc_files, missing, empty


# ---- §12 版本信息存在性探测 (门方向修复的核心) ------------------------------
def _alpha_hit(text: str):
    """返回首处非豁免 alpha 字面量 (行号, 'X.Y.Z-alpha.N'), 无则 (None, None)。"""
    for i, ln in enumerate(text.splitlines(), 1):
        if REV_FIELD.match(ln.strip()):
            continue  # 机器修订字段记录"来源/基线", 不是"当前版本信息"陈述
        m = ALPHA_INLINE.search(ln)
        if m:
            return i, "%s-alpha.%s" % (m.group(1), m.group(2))
    return None, None


def detect_version_presence(root: str) -> tuple:
    """(present, evidence): 仓库里是否存在任何"版本信息面"。

    判据对齐 ASTROCS_DESIGN §12「Alpha 之前：程序与代码中不包含任何版本信息」：
      · 根 VERSION 文件非空;
      · lib/infrastructure/cli/** 或根 CMakeLists.txt 出现 alpha 字面量;
      · 根 CMakeLists.txt 的 project(... VERSION ...) 数字三元组 (版本基础设施);
      · 根 CMakeLists.txt 的 file(READ .../VERSION ...) 生成链;
      · CLI 版本注入模板含 @ASTROCS_VERSION_STRING@ 占位;
      · 活动文档集 (DOC_SET_FILES/DIRS) 出现非豁免 alpha 字面量。
    任一命中 ⇒ present=True ⇒ 走一致性校验; 全不命中 ⇒ absence 模式, 不判红。
    只读; 不 import 被测模块。
    """
    evidence = []
    vpath = os.path.join(root, VERSION_REL)
    if os.path.isfile(vpath):
        try:
            raw = read_text(vpath).strip()
        except OSError:
            raw = ""
        if raw:
            evidence.append("%s 非空: %r" % (VERSION_REL, raw[:80]))

    cml = os.path.join(root, ROOT_CMAKE_REL)
    cml_text = read_text(cml) if os.path.isfile(cml) else ""
    if cml_text:
        m = PROJECT_RE.search(cml_text)
        if m:
            evidence.append("%s project(%s VERSION %s)" % (ROOT_CMAKE_REL, m.group(1), m.group(2)))
        if re.search(r"file\(READ\s+\$\{CMAKE_CURRENT_SOURCE_DIR\}/VERSION\s+"
                     r"ASTROCS_BASE_VERSION", cml_text):
            evidence.append("%s 含 file(READ VERSION ASTROCS_BASE_VERSION) 生成链" % ROOT_CMAKE_REL)

    cli_dir = os.path.join(root, CLI_DIR_REL)
    if os.path.isdir(cli_dir):
        for dirpath, dirnames, filenames in os.walk(cli_dir):
            dirnames[:] = [x for x in dirnames if not x.startswith("__")]
            for fn in sorted(filenames):
                if not fn.endswith(CLI_SCAN_EXT):
                    continue
                full = os.path.join(dirpath, fn)
                try:
                    i, val = _alpha_hit(read_text(full))
                except OSError:
                    continue
                if val:
                    evidence.append("%s:%s %s" % (os.path.relpath(full, root), i, val))
    if cml_text:
        i, val = _alpha_hit(cml_text)
        if val:
            evidence.append("%s:%s %s" % (ROOT_CMAKE_REL, i, val))

    tpl = os.path.join(root, CLI_TEMPLATE_REL)
    if os.path.isfile(tpl):
        try:
            if "@ASTROCS_VERSION_STRING@" in read_text(tpl):
                evidence.append("%s 含 @ASTROCS_VERSION_STRING@ 注入占位" % CLI_TEMPLATE_REL)
        except OSError:
            pass

    doc_files, _, _ = doc_scan_set(root)
    for rel, p in doc_files:
        try:
            i, val = _alpha_hit(read_text(p))
        except OSError:
            continue
        if val:
            evidence.append("%s:%s %s" % (rel, i, val))

    return bool(evidence), evidence


def report(checks: list, root: str, expected, expected_source: str,
           stale: list) -> int:
    fails = [c for c in checks if not c["pass"]]
    code = 2 if stale else (1 if fails else 0)
    out = {
        "tool": "ci/check_version.py",
        "root": root,
        "expected": expected,
        "expected_source": expected_source,
        "anchor_stale": stale,
        "checks": checks,
        "fail_count": len(fails),
        "pass_count": len(checks) - len(fails),
        "exit_code": code,
        "verdict": ("ANCHOR_STALE" if stale else
                    ("VERSION_CHECK_FAIL" if fails else "VERSION_CHECK_PASS")),
    }
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return code


def main() -> int:
    ap = argparse.ArgumentParser(description="AstroCS 产品版本一致性 CI 快速门")
    ap.add_argument("--expected", default=None,
                    help="期望产品版本, 如 0.11.0-alpha.2 (缺省: 取根 VERSION 的 strip 值)")
    ap.add_argument("--root", default=None,
                    help="仓库根 (默认: 脚本位置上一级; 只读)")
    ap.add_argument("--self-test", action="store_true",
                    help="mini-repo 自测: 正例必绿 + 负例必红 (不读不写本仓)")
    args = ap.parse_args()
    if args.self_test:
        return self_test()
    root = os.path.abspath(args.root or os.path.dirname(
        os.path.dirname(os.path.abspath(__file__))))
    checks: list = []

    # §12 门方向修复（RELEASE-02 CI-HYGIENE）: 先探测"版本信息面"是否存在。
    #   存在  ⇒ 走下方 [1]~[6] 一致性校验 (fail-closed, 与旧行为等价);
    #   不存在 ⇒ absence 模式: 版本专用锚/生成链判据无对象, 显式跳过并留痕,
    #            仍校验非版本锚 + 文档集完整性 (防移空), 然后 PASS —— 不判红。
    # 修复前方向相反: 版本不存在 ⇒ anchor_alive_VERSION_REL 判 ANCHOR_STALE(exit 2),
    # 即"CI 绿灯 = 必然违反 §12"。本修复只改门方向, 不改任何版本号。
    present, presence_evidence = detect_version_presence(root)
    if not present:
        anchor_checks, stale = check_anchors(root, skip=VERSION_ANCHOR_NAMES)
        checks.extend(anchor_checks)
        for line in stale:
            sys.stderr.write(line + "\n")
        doc_files, doc_missing, doc_empty_dirs = doc_scan_set(root)
        add(checks, "version_absence_alpha_pre", not stale, "<version-surface>", None,
            "无版本信息",
            "ASTROCS_DESIGN §12: Alpha 前程序与代码中不含任何版本信息; "
            "本模式判据 = 版本信息不存在 ⇒ 不判红 "
            "(存在则转入 [1]~[6] 一致性校验, 由 version_presence_detected 留痕)")
        add(checks, "doc_set_complete", not doc_missing and not doc_empty_dirs,
            "README/docs/governance/docs/owner", None,
            "缺失: %s" % doc_missing if doc_missing else
            ("扫描面缺口: %s" % doc_empty_dirs if doc_empty_dirs else
             "%d 文件 + %d 目录齐全" % (len(DOC_SET_FILES), len(DOC_SET_DIRS))),
            "规则: §12 absence 模式仍须保证非版本面判据存活 (防移空), "
            "不因版本信息缺席而缩小扫描面")
        return report(checks, root, "", "absent (§12 Alpha 前无版本信息)", stale)

    # [0] 锚存活前置断言 (fail-closed; 不 traceback, 不静默通过)
    anchor_checks, stale = check_anchors(root)
    ref_checks, ref_stale = check_ref_anchors(root)
    checks.extend(anchor_checks)
    checks.extend(ref_checks)
    stale.extend(ref_stale)
    for line in stale:
        sys.stderr.write(line + "\n")
    add(checks, "version_presence_detected", True, "<version-surface>", None,
        "%d 处版本信息" % len(presence_evidence),
        "§12 门方向: 版本信息存在 ⇒ 校验一致性 (非「必须存在」); 证据: %s"
        % "; ".join(presence_evidence[:6]))

    # expected 解析: 显式 --expected 优先; 缺省时取根 VERSION (唯一事实源)
    expected_source = "--expected" if args.expected is not None else VERSION_REL
    expected = args.expected
    if expected is None:
        vpath = os.path.join(root, VERSION_REL)
        if os.path.isfile(vpath):
            try:
                expected = read_text(vpath).strip()
                expected_source = "%s (--expected 缺省)" % VERSION_REL
            except OSError as exc:
                expected = ""
                expected_source = "%s (读取失败: %s)" % (VERSION_REL, exc)
        else:
            expected = ""
            expected_source = "%s (缺失)" % VERSION_REL

    # [1] expected 格式
    add(checks, "expected_format", bool(SEMVER_ALPHA.match(expected or "")),
        "<argument|VERSION>", None, expected or "",
        "必须形如 MAJOR.MINOR.PATCH-alpha.N (禁 stable/rc/beta); 来源=%s"
        % expected_source)
    if not SEMVER_ALPHA.match(expected or ""):
        return report(checks, root, expected, expected_source, stale)
    exp_base = base_of(expected)

    # [2] 根 VERSION 文件
    vpath = os.path.join(root, VERSION_REL)
    if os.path.isfile(vpath):
        ver = read_text(vpath).strip()
        add(checks, "version_file", ver == expected, VERSION_REL, 1, ver,
            "根 VERSION 必须 == expected (唯一事实源)"
            + ("; 注意: expected 亦取自 VERSION (缺省模式), 本项为自比较, "
               "独立判据为 [3]/[4]" if args.expected is None else ""))
    else:
        add(checks, "version_file", False, VERSION_REL, None, "缺失",
            "根 VERSION 文件不存在")

    # [3] 根 CMakeLists.txt project() 数字三元组 == expected 基础号
    cml = os.path.join(root, ROOT_CMAKE_REL)
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
        add(checks, "cmake_project_base", ok, ROOT_CMAKE_REL, i,
            "project(%s VERSION %s)" % (name, pv),
            "规则: project() 数字三元组 == expected 去 -alpha.N 的基础号 "
            "(CMake project() 不接受 alpha 后缀, 后缀由根 VERSION/生成链承载)")
    else:
        add(checks, "cmake_project_base", False, ROOT_CMAKE_REL, None,
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
    add(checks, "chain_root_read_version", i is not None, ROOT_CMAKE_REL, i,
        ln or "缺失", "根 CMakeLists.txt 必须 file(READ 根 VERSION) (禁止手抄)")
    # [W4-A3 退役] chain_cli_read_version / chain_configure_file_cli 已删除：
    # 判据输入 cli/CMakeLists.txt 的 configure_file 目标 cli/version_generated.h.in
    # 已不在版本库，判据只匹配字符串 ⇒ 恒 PASS（§8 锚存活失效型，实测见
    # run/PROJECT-GOVERNANCE-01/W4-A3/logs/CLI003_before_false_green.log）。
    tpl_path = os.path.join(root, CLI_TEMPLATE_REL)
    if os.path.isfile(tpl_path):
        tpl = read_text(tpl_path)
        i, ln = first_line(tpl, re.compile(r"@ASTROCS_VERSION_STRING@"))
        add(checks, "chain_template_placeholder", i is not None,
            CLI_TEMPLATE_REL, i, ln or "缺失",
            "版本注入模板必须含 @ASTROCS_VERSION_STRING@ 占位")
    else:
        add(checks, "chain_template_placeholder", False,
            CLI_TEMPLATE_REL, None, "缺失",
            "版本注入模板不存在 (锚失效时见 anchor_alive_CLI_TEMPLATE_REL)")
    i, ln = first_line(cml_text, re.compile(
        r"configure_file\(lib/infrastructure/cli/version_generated\.h\.in"))
    add(checks, "chain_configure_file_root", i is not None, ROOT_CMAKE_REL, i,
        ln or "缺失", "根 CMakeLists.txt 必须 configure_file 生成 version_generated.h")

    # [4e] cli/** 与根 CMakeLists.txt 的 alpha 字面量扫描
    lit_rows = []
    scan_targets = []
    cli_dir = os.path.join(root, CLI_DIR_REL)
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
            lit_rows.append((p, None, None, "读取失败: %s" % exc))
            continue
        for i, ln in enumerate(text.splitlines(), 1):
            for m in ALPHA_INLINE.finditer(ln):
                val = "%s-alpha.%s" % (m.group(1), m.group(2))
                lit_rows.append((os.path.relpath(p, root), i, val, ln.strip()[:90]))
    bad = [(f, i, v, t) for (f, i, v, t) in lit_rows
           if v is not None and v != expected]
    handcopy = [(f, i, v) for (f, i, v, t) in lit_rows if v == expected]
    unread = [(f, t) for (f, i, v, t) in lit_rows if v is None]
    add(checks, "literal_scan_cli_and_cmake", not bad and not unread,
        "cli/** + CMakeLists.txt",
        None if not bad else bad[0][1],
        "0 处 alpha 字面量 (生成链注入)" if not lit_rows
        else "%d 处 (漂移 %d)" % (len(lit_rows), len(bad)),
        "规则: 任何 X.Y.Z-alpha.N 字面量必须 == expected"
        + ("; 漂移: %s:%s=%s" % (bad[0][0], bad[0][1], bad[0][2]) if bad else "")
        + ("; 读取失败: %s" % unread if unread else "")
        + ("; 手抄但同值 %d 处 (建议迁移生成链)" % len(handcopy) if handcopy else ""))

    # [5] 活动文档 alpha 字面量统一 + 扫描面显式报缺 (防移空)
    doc_files, doc_missing, doc_empty_dirs = doc_scan_set(root)
    drift, scanned = [], 0
    for rel, p in doc_files:
        scanned += 1
        for i, ln in enumerate(read_text(p).splitlines(), 1):
            if REV_FIELD.match(ln.strip()):
                continue  # 机器修订关系字段 (记录来源/基线), 非"当前值"陈述
            for m in ALPHA_INLINE.finditer(ln):
                val = "%s-alpha.%s" % (m.group(1), m.group(2))
                if val != expected:
                    drift.append((rel, i, val, ln.strip()[:90]))
    shrink = doc_missing or doc_empty_dirs
    add(checks, "doc_scan_active_docs", not drift and not shrink,
        "README/docs/governance/docs/owner",
        None if not drift else drift[0][1],
        ("扫描 %d 份活动文档" % scanned if not shrink else
         "扫描 %d 份活动文档; 扫描面缺口: 缺成员=%s 空目录=%s"
         % (scanned, doc_missing, doc_empty_dirs)) if not drift else
        "%d 份中 %d 处漂移" % (scanned, len(drift)),
        "规则: 活动文档中任何 X.Y.Z-alpha.N 字面量必须 == expected"
        " (豁免仅 REV_FIELD 机器修订字段; CHANGELOG/memory 为 history/日志"
        " 命名空间不进本检查, 冻结约束文件由负责人修订). 防移空: DOC_SET_FILES "
        "成员缺失或 DOC_SET_DIRS 扫描成员为 0 时本项判 FAIL 并点名"
        " (禁止 isfile 静默跳过导致扫描面悄悄缩小)"
        + ("; 缺成员=%s" % doc_missing if doc_missing else "")
        + ("; 空目录=%s" % doc_empty_dirs if doc_empty_dirs else "")
        + ("; 首处漂移 %s:%s=%s" % (drift[0][0], drift[0][1], drift[0][2])
           if drift else ""))

    # [6] 活动文档集合完整性
    missing = doc_missing[:]
    missing += [d + "/" for d in DOC_SET_DIRS
                if not os.path.isdir(os.path.join(root, d))]
    add(checks, "doc_set_complete", not missing and not doc_empty_dirs,
        "README/docs/governance/docs/owner",
        None, "缺失: %s" % missing if missing else
        ("扫描面缺口: %s" % doc_empty_dirs if doc_empty_dirs else
         "%d 文件 + %d 目录齐全" % (len(DOC_SET_FILES), len(DOC_SET_DIRS))),
        "规则: [5] 的固定活动文档/目录必须存在且非空, 防止检查面被移空"
        " (缺一即 FAIL 并逐条点名)")

    return report(checks, root, expected, expected_source, stale)


# ---- 可执行负例面: tempfile mini-repo ---------------------------------------
ROOT_CMAKE_TMPL = """cmake_minimum_required(VERSION 3.24)
project(astrocs VERSION %(base)s LANGUAGES C CXX)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/VERSION ASTROCS_BASE_VERSION)
configure_file(lib/infrastructure/cli/version_generated.h.in ${CMAKE_CURRENT_SOURCE_DIR}/version_generated.h @ONLY)
"""
# [W4-A3 退役] 原 CLI_CMAKE_TMPL（cli/CMakeLists.txt 最小夹具）已随两条 chain 判据
# 一并删除：夹具为死判据提供输入，留着就是“给不存在的目标造在场证明”。
TEMPLATE_TMPL = """#pragma once
#define ASTROCS_VERSION_STRING "@ASTROCS_VERSION_STRING@"
"""
DOC_TMPL = "# %s\n\ndoc_version: 0.10.0-alpha.1\n"
# §12 absence 夹具: 无 VERSION / 无 project VERSION / 无生成链 / 无 alpha 字面量。
ROOT_CMAKE_NO_VERSION_TMPL = """cmake_minimum_required(VERSION 3.24)
project(astrocs LANGUAGES C CXX)
"""
NO_VERSION_DOC_TMPL = "# %s\n\n版本信息面: 无 (Alpha 前, ASTROCS_DESIGN §12)\n"


def _mini_repo(root: str, *, expected: str = "0.11.0-alpha.2", omit=(),
               extra_files=None, readme_body=None, version_free=False) -> None:
    if version_free:
        files = {
            ROOT_CMAKE_REL: ROOT_CMAKE_NO_VERSION_TMPL,
            # 非版本锚 (CLI_DIR_REL) 仍须存活, 但不得引入版本字面量。
            "lib/infrastructure/cli/placeholder.txt": "// 无版本信息面\n",
            "README.md": readme_body if readme_body is not None
            else NO_VERSION_DOC_TMPL % "README",
        }
    else:
        base = ".".join(expected.split("-alpha.")[0].split("."))
        files = {
            VERSION_REL: expected + "\n",
            ROOT_CMAKE_REL: ROOT_CMAKE_TMPL % {"base": base},
            CLI_TEMPLATE_REL: TEMPLATE_TMPL,
            "README.md": readme_body if readme_body is not None
            else DOC_TMPL % "README",
        }
    doc_tmpl = NO_VERSION_DOC_TMPL if version_free else DOC_TMPL
    for rel in DOC_SET_FILES:
        if rel == "README.md":
            continue
        files[rel] = doc_tmpl % os.path.basename(rel)
    for d in DOC_SET_DIRS:
        files.setdefault(os.path.join(d, "placeholder.md"), doc_tmpl % d)
    files.update(extra_files or {})
    for rel in omit:
        files.pop(rel, None)
    for rel, body in files.items():
        full = os.path.join(root, rel)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w", encoding="utf-8") as fh:
            fh.write(body)


def _run_mini(root: str, expected=None) -> tuple:
    """进程内以给定 argv 跑 main(); 返回 (exit_code, stdout+stderr 全文)。

    stdout (JSON 摘要) 一并捕获: 自测只打印 [selftest] 结论行, 不把每例摘要刷屏;
    断言目标 = 全文, 既覆盖 stderr 的 ANCHOR_STALE 行, 也覆盖 JSON 里的判据明细。
    """
    argv = ["--root", root]
    if expected is not None:
        argv += ["--expected", expected]
    old_argv, old_out, old_err = sys.argv, sys.stdout, sys.stderr
    buf = io.StringIO()
    try:
        sys.argv = ["check_version.py"] + argv
        sys.stdout = buf
        sys.stderr = buf
        code = main()
    finally:
        sys.argv, sys.stdout, sys.stderr = old_argv, old_out, old_err
    return code, buf.getvalue()


def self_test() -> int:
    """正例必绿 + 负例必红 (tempfile mini-repo; 不读不写本仓库)。"""
    expected = "0.11.0-alpha.2"
    cases = []

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        pos = os.path.join(td, "pos")
        os.makedirs(pos)
        _mini_repo(pos, expected=expected)
        code, err = _run_mini(pos, expected)
        cases.append(("pos_explicit_expected", code, err, 0, None))
        code, err = _run_mini(pos)
        cases.append(("pos_expected_from_version", code, err, 0, None))

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-doc-member")
        os.makedirs(root)
        _mini_repo(root, expected=expected, omit=("docs/owner/PIPELINE_OVERVIEW.md",))
        code, err = _run_mini(root, expected)
        cases.append(("neg_doc_member_missing", code, err, 2,
                      "doc_scan_active_docs|docs/owner/PIPELINE_OVERVIEW.md"))

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-cli-drift")
        os.makedirs(root)
        # ROOT-008 迁移后 CLI 字面量扫描面 = lib/infrastructure/cli/（CLI_DIR_REL）
        _mini_repo(root, expected=expected, extra_files={
            "lib/infrastructure/cli/drift.cpp": 'const char* kVersion = "0.9.9-alpha.1";\n'})
        code, err = _run_mini(root, expected)
        cases.append(("neg_cli_version_drift", code, err, 1,
                      "literal_scan_cli_and_cmake|0.9.9-alpha.1"))

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-doc-drift")
        os.makedirs(root)
        _mini_repo(root, expected=expected, readme_body=(
            "# README\n\n- product_version: 0.10.0-alpha.9\n\n当前版本 0.10.0-alpha.9\n"))
        code, err = _run_mini(root, expected)
        cases.append(("neg_doc_version_drift", code, err, 1,
                      "doc_scan_active_docs|0.10.0-alpha.9"))

    # [W4-A3] 换行拆写 ${，避免工具链源码出现未转义插值序列
    D = "$"

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-anchor-stale")
        os.makedirs(root)
        _mini_repo(root, expected=expected, omit=(CLI_TEMPLATE_REL,))
        code, err = _run_mini(root, expected)
        cases.append(("neg_anchor_stale", code, err, 2,
                      "ANCHOR_STALE: CLI_TEMPLATE_REL " + CLI_TEMPLATE_REL))

    # [W4-A3] 改锚后的「VERSION 不得删除」硬依据必须可判红：根 CMakeLists.txt 缺
    # file(READ …/VERSION ASTROCS_BASE_VERSION) ⇒ rc=1 且点名 chain_root_read_version。
    # 若这条也空转，退役 cli 判据后会出现"无人守 VERSION"的盲区。
    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-root-version-read")
        os.makedirs(root)
        _mini_repo(root, expected=expected, extra_files={
            ROOT_CMAKE_REL: ("cmake_minimum_required(VERSION 3.24)\n"
                             "project(astrocs VERSION 0.11.0 LANGUAGES C CXX)\n"
                             "configure_file(lib/infrastructure/cli/version_generated.h.in "
                             + D + "{CMAKE_CURRENT_SOURCE_DIR}/version_generated.h @ONLY)\n")})
        code, err = _run_mini(root, expected)
        cases.append(("neg_root_version_read_missing", code, err, 1,
                      "chain_root_read_version"))

    # §12 门方向修复的正/负例（RELEASE-02 CI-HYGIENE）:
    #   pos: 完全没有版本信息 ⇒ rc=0（旧实现因 anchor_alive_VERSION_REL 判 ANCHOR_STALE
    #        exit 2, 方向与 §12 相反）;
    #   pos: 完全无版本信息 + 显式 --expected ⇒ 仍 rc=0（显式 expected 不得强制版本存在）;
    #   neg: 无 VERSION 但 CLI 出现 alpha 字面量 ⇒ 判红（版本信息存在即必须一致,
    #        fail-closed; 此处 VERSION 锚同时失效故为 ANCHOR_STALE exit 2）。
    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "pos-absence")
        os.makedirs(root)
        _mini_repo(root, version_free=True)
        code, err = _run_mini(root)
        cases.append(("pos_absence_no_version", code, err, 0,
                      "VERSION_CHECK_PASS|version_absence_alpha_pre"))
        code, err = _run_mini(root, expected)
        cases.append(("pos_absence_explicit_expected", code, err, 0,
                      "VERSION_CHECK_PASS"))

    with tempfile.TemporaryDirectory(prefix="cv-selftest-") as td:
        root = os.path.join(td, "neg-absence-literal")
        os.makedirs(root)
        _mini_repo(root, version_free=True, extra_files={
            "lib/infrastructure/cli/drift.cpp": 'const char* kVersion = "0.9.9-alpha.1";\n'})
        code, err = _run_mini(root)
        cases.append(("neg_absence_literal_present", code, err, 2,
                      "ANCHOR_STALE: VERSION_REL"))

    ok = True
    for name, code, err, want_rc, want_text in cases:
        good = (code == want_rc)
        if good and want_text:
            for token in want_text.split("|"):
                if token not in err:
                    good = False
        ok = ok and good
        print("[selftest] %-28s rc=%-2d want=%-2d %s"
              % (name, code, want_rc, "OK" if good else "MISMATCH"))
        if not good and want_text:
            print("[selftest]   输出应包含: %s" % want_text)
    print("[selftest] %d cases, %s" % (len(cases),
                                       "ALL OK" if ok else "FAILED"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
