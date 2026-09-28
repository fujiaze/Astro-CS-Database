#!/usr/bin/env python3
"""GOV-003 版本命名空间机器检查器 (eng/tools/doccheck 系列)。

任务 GOV-003: 根 VERSION 设 0.11.0-alpha.1; product/module/ABI/data-schema/
doc-revision/history 五个版本命名空间; CMake/CLI/L0 从根 VERSION 生成产品
版本, 禁止手抄。

检查项 (exit 0 = PASS):
  1. 根 VERSION 存在且匹配 MAJOR.MINOR.PATCH-alpha.N (禁 stable/rc/beta);
  2. eng/tools/gen_version.py --json 输出 version 前缀 == 根 VERSION
     (生成链: CLI/打包从唯一源派生, 非手抄);
  3. 允许路径 active 文档/配置不含"另一个产品版本":
     - alpha 形态 X.Y.Z-alpha.N != 源 => FAIL;
     - 裸 X.Y.Z != 源基础号 => FAIL (豁免: 非产品版本命名空间与占位);
  4. 反误报: FITS 4.0 / HiPS 1.0 / ABI v1 / schema_version /
     DatabaseVersion / CFITSIO / X.Y.Z 占位 不被当作产品版本漂移;
     DOI/URL 引用标识符 (如 [DOI 10.1080/00401706.1977.10489534](https://doi.org/...))
     内的数字三元组属**引用标识符命名空间**, 不判 FAIL。该判定是**结构性**的
     (匹配位置落在标识符 token 跨度内), **不是行级关键词豁免** —— 同一行里落在
     标识符之外的版本字面量照旧判 FAIL (自测 N4 锁定该判别力);
  5. mutation 合同: 伪造产品版本字面量必须使本 checker FAIL;
  6. 他人路径遗留 (docs/VERSIONING.md、CMake project 字面量、eng/tests/ 硬编码、
     eng/tools/check_*.py 硬编码、DOCUMENT_INDEX base_product_version) 汇总为
     out_of_scope 列表输出, 不判 FAIL (集成协调项, 见 known_limits)。
  7. 扫描面约束 (GOV-003 判据面): 扫描目标一律按 repo 根解析, realpath 归一后用
     is_relative_to(repo) 校验; 逃出 repo 根的目标被**裁剪并点名** (rejected_paths)
     且判红 —— 既不静默计入, 也不静默丢弃。报告里的路径一律相对 repo 根
     (旧实现相对 eng/ 输出, 把仓库内文件显示成 '../memory.md', 看起来像扫描面逃出仓库)。
     扫描面为空 ⇒ 判红 (docs/ci/01_CHECKS.md §1: scanned==0 ⇒ rc!=0)。

用法:
  python3 eng/tools/doccheck/check_version_namespaces.py [--root <repo>] [--json-out <f>]
  python3 eng/tools/doccheck/check_version_namespaces.py --self-test   # P1/P2 + N1..N4
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from pathlib import Path

# 允许路径 active 扫描面 (GOV-003 owner 可改路径内的当前文档/配置)
# 硬判: 治理/规范/当前状态文档 — 任何其他产品版本/裸版本字面量 => FAIL。
SCAN_FILES = [
    "VERSION",
    "AGENTS.md",
    "AstroCS_ENGINEERING_CONSTRAINTS.md",
    "README.md",
    "REVIEW.md",
    "HANDOVER.md",
]
SCAN_DIRS = ["docs/owner"]
# warnings-only: 历史日志驻留点 (memory.md 是逐日操作日志, CHANGELOG.md 是
# history 命名空间合法驻留点; 其历史轮次/外部组件版本由 GOV-005 收敛,
# GOV-003 不硬判 FAIL, 只报告警告)。
LOG_FILES = ["memory.md", "CHANGELOG.md"]
ARCHIVE_HINT = ("/archive/", "ARCHIVED", "NON_NORMATIVE")

SEMVER_ALPHA = re.compile(r"^(\d+)\.(\d+)\.(\d+)-alpha\.(\d+)$")
ALPHA_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)-alpha\.(\d+)(?![\w.])")
BASE_INLINE = re.compile(r"(?<![\w.])(\d+\.\d+\.\d+)(?![\d.])")
PRERELEASE_BAD = re.compile(r"-(stable|rc|beta)\b", re.IGNORECASE)
# 引用标识符命名空间 (反误报 §4 同类, 结构性判定): DOI token 内的数字三元组是文献
# 标识符 (如 10.1080/00401706.1977.10489534、10.1093/biomet/26.4.404), 不是产品版本。
# 判据是"匹配位置是否落在标识符 token 跨度内", 因此同一行里标识符之外的版本字面量
# 照旧判 FAIL —— 判别力由 --self-test N4 锁定。
# 注: 该排除**只**作用于裸三元组 (BASE_INLINE); alpha 产品版本串 (ALPHA_INLINE) 不因
# 落在标识符内而被放过 —— 少一类放宽面, 判别力更强。
DOI_TOKEN = re.compile(r"\b10\.\d{4,9}/[^\s)\]}>,，。；、）】]+")

# 机器修订关系字段 (front matter/YAML), 非"当前产品版本"陈述
REV_FIELD = re.compile(r"^(source_main_version|target_main_version|base_product_version|source_main_sha|base_main_sha|product_version|doc_version)\s*[:=]\s*[\"']?\d+\.\d+\.\d+(-alpha\.\d+)?[\"']?")
CONTRACT_DOC_VERSION = re.compile(r"状态:\s*\w+\s+版本:\s*\d+(\.\d+)*")
# 非产品版本命名空间的行级豁免 (本命名空间合同 §1/§3): 这些 token 所在行的
# 数字三元组属于 FITS/HiPS/ABI/data-schema/外部组件/占位, 不得误报。
NAMESPACE_EXEMPT = (
    "hips_version", "hips 1.0", "hips 1.4", "hips_version=1.4",  # HiPS 格式版本
    "fits 4.0", "fits 4", "cfitsio", "fitsio", "fits 标准",       # FITS 格式/组件
    "abi v1", "abi_version", "acs_abi_version", "acs_artifact_abi",  # C ABI
    "schema_version", "cli_schema_version", "$schema",            # data-schema
    "databaseversion", "gaia 库",                                   # Gaia 库标识
    "module_version", "module.yaml",                                # module 命名空间
    "doc-revision", "doc_revision", "版本: ", "修订号",             # doc-revision
    "v6.1", "v19r", "v18r", "控制包", "轮次", "history", "archived",  # history
    "x.y.z", "major.minor.patch", "占位",
    "opencl", "driver", "g++", "gcc", "cmake", "ninja", "mingw", "msys2",
    "siril", "wbpp", "pcl", "rcr", "python", "clang", "healpix", "ivoa",
    "2026-", "2025-",                                           # 日期
    '"version":', '"版本":', "version: ", "版本号",
)


def check(name: str, ok: bool, detail: str) -> dict:
    return {"check": name, "pass": bool(ok), "detail": detail}


def root_version(root: str) -> str:
    p = os.path.join(root, "VERSION")
    with open(p, encoding="utf-8") as f:
        raw = f.read().strip()
    m = SEMVER_ALPHA.match(raw)
    if not m:
        raise SystemExit(f"VERSION 格式非法: {raw!r}")
    return raw


def _checker_repo_root() -> str:
    """检查器自身所在仓库根 (上溯 4 层: doccheck -> tools -> eng -> repo)。"""
    return os.path.dirname(os.path.dirname(os.path.dirname(
        os.path.dirname(os.path.abspath(__file__)))))


def within_repo(path: str, repo_root: str) -> bool:
    """扫描目标必须落在 repo 根内: realpath 归一后 is_relative_to(repo) 校验。

    lexical 越界 (`../x`) 与符号链接指向仓库外两种逃逸形态都在此被拦下。
    """
    real_path = Path(os.path.realpath(path))
    real_root = Path(os.path.realpath(repo_root))
    return real_path == real_root or real_path.is_relative_to(real_root)


def rel_display(path: str, repo_root: str | None = None) -> str:
    """报告用路径: repo 根内显示相对 repo 根, repo 根外回退绝对路径。

    旧实现固定相对 `<repo>/eng` 取相对路径, 把仓库内文件显示成 `../memory.md`,
    看起来像"扫描面逃出仓库"——那只是显示缺陷, 但会误导判据阅读者。
    """
    root = os.path.abspath(repo_root or _checker_repo_root())
    ap = os.path.abspath(path)
    if ap == root or ap.startswith(root + os.sep):
        return os.path.relpath(ap, root)
    return ap


def collect_scan_targets(root: str):
    """收集扫描目标并按 repo 根裁剪: (targets, rejected, absent)。

    targets  = realpath 落在 repo 根内的扫描文件 (参与判据);
    rejected = 解析到 repo 根之外的目标 —— **裁剪并点名**, 不静默计入,
               由 scan_surface_within_repo 判红 (fail-closed);
    absent   = 声明但当前不存在的路径 (登记留痕; 部分声明项按设计可缺, 不判红)。
    """
    targets: list[str] = []
    rejected: list[str] = []
    absent: list[str] = []

    def _add(path: str, declared: str) -> None:
        if not os.path.isfile(path):
            absent.append(declared)
            return
        if within_repo(path, root):
            targets.append(path)
            return
        rejected.append(f"{declared}: 解析到 repo 根之外 (realpath="
                        f"{os.path.realpath(path)}, repo={os.path.realpath(root)})")

    for declared in SCAN_FILES + LOG_FILES:
        _add(os.path.join(root, declared), declared)
    for declared in SCAN_DIRS:
        base = os.path.join(root, declared)
        if not os.path.isdir(base):
            absent.append(declared)
            continue
        if not within_repo(base, root):
            rejected.append(f"{declared}: 扫描目录解析到 repo 根之外 (realpath="
                            f"{os.path.realpath(base)}, repo={os.path.realpath(root)})")
            continue
        for dirpath, dirnames, filenames in os.walk(base):
            dirnames[:] = [x for x in dirnames if not x.startswith("__")]
            for fn in sorted(filenames):
                if fn.endswith((".md", ".txt", ".json", ".yaml", ".yml", ".py", ".sh")):
                    p = os.path.join(dirpath, fn)
                    _add(p, rel_display(p, root))
    return targets, rejected, absent


def iter_scan_files(root: str):
    """兼容入口: 只产出 repo 根内的扫描目标 (越界目标被裁剪, 见 collect_scan_targets)。"""
    targets, _rejected, _absent = collect_scan_targets(root)
    yield from targets


def is_archived_path(rel: str) -> bool:
    return any(h in rel for h in ("/archive/",))


def exempt_line(line: str) -> bool:
    low = line.lower()
    return any(k in low for k in NAMESPACE_EXEMPT)


def identifier_spans(line: str):
    """引用标识符 (DOI) 的字符跨度 —— 结构性判定, 不是行级关键词豁免。"""
    return [m.span() for m in DOI_TOKEN.finditer(line)]


def _in_spans(start: int, end: int, spans) -> bool:
    return any(s <= start and end <= e for s, e in spans)


def scan_file(path: str, base_num: str, alpha_n: int, errors: list, warnings: list,
              log_ok: bool = False, repo_root: str | None = None,
              notes: list | None = None):
    note_sink = notes if notes is not None else []
    rel = rel_display(path, repo_root)
    # 归档不扫描: history 命名空间只进 archive/CHANGELOG
    if is_archived_path(rel):
        return
    # 日志驻留点 (memory/CHANGELOG): 漂移只警告不 FAIL
    is_log = log_ok or os.path.basename(path) in ("memory.md", "CHANGELOG.md")
    sink_e, sink_w = errors, warnings
    if is_log:
        sink_e, sink_w = [], warnings
    with open(path, encoding="utf-8", errors="replace") as f:
        for i, line in enumerate(f, 1):
            if REV_FIELD.match(line.strip()):
                continue  # 机器修订关系字段(记录来源/基线), 非当前版本陈述
            if CONTRACT_DOC_VERSION.search(line):
                continue  # front matter 文档修订号 (doc-revision 命名空间)
            if PRERELEASE_BAD.search(line) and ALPHA_INLINE.search(line):
                m = ALPHA_INLINE.search(line)
                if m:
                    sink_e.append(
                        f"{rel}:{i}: 产品版本含禁止 prerelease: {line.strip()[:90]}")
            for m in ALPHA_INLINE.finditer(line):
                if m.group(1) != base_num or int(m.group(2)) != alpha_n:
                    # 其他 alpha 产品版本: history 轮次上下文允许例外
                    if exempt_line(line):
                        sink_w.append(f"{rel}:{i}: history/豁免行内 alpha 串 "
                                      f"{m.group(0)} (不判 FAIL): {line.strip()[:70]}")
                        continue
                    if is_log:
                        sink_w.append(f"{rel}:{i}: 日志内 alpha 串 {m.group(0)} "
                                      f"(warnings-only): {line.strip()[:70]}")
                        continue
                    sink_e.append(f"{rel}:{i}: 其他产品版本 {m.group(0)} != "
                                  f"{base_num}-alpha.{alpha_n}: {line.strip()[:90]}")
            if exempt_line(line):
                continue
            spans = identifier_spans(line)   # DOI 引用标识符跨度 (结构判定)
            for m in BASE_INLINE.finditer(line):
                if m.group(1) == base_num:
                    continue
                if _in_spans(m.start(), m.end(), spans):
                    # 引用标识符 (DOI/URL) 命名空间: 文献标识符, 非产品版本陈述。
                    # 记入 notes 而非静默丢弃 (审计可见), 且**只**排除跨度内的匹配 ——
                    # 同一行跨度外的版本字面量仍走下面的判红分支 (自测 N4)。
                    note_sink.append(
                        f"{rel}:{i}: 引用标识符命名空间内三元组 {m.group(1)} "
                        f"(DOI/URL 文献标识符, 非产品版本): {line.strip()[:70]}")
                    continue
                if is_log:
                    sink_w.append(f"{rel}:{i}: 日志内裸版本字面量 {m.group(1)} "
                                  f"(warnings-only): {line.strip()[:70]}")
                    continue
                sink_e.append(f"{rel}:{i}: 裸版本字面量 {m.group(1)} != "
                              f"{base_num}: {line.strip()[:90]}")


def _self_test() -> int:
    """可执行正/负例面（01_CHECKS §1「每项检查必须提供机器可执行负例入口」）。

    P1 真仓库（版本命名空间一致）⇒ 必须 rc=0（能绿）；
    P2 合成树命名空间一致（当前版本串 + DOI 引用）⇒ 扫描面必须判绿：active_scan 与
       scan_surface_within_repo 均 pass、errors 为空；DOI 三元组必须落进 notes
       （证明确实走了标识符命名空间判定，而不是判据退化不看了）；
    N1 非 git 树：本检查器 + gen_version.py + VERSION 复制到临时树
       （GIT_CEILING_DIRECTORIES 阻断向上找 .git）⇒ 必须 rc=1，gen_version_from_source
       的 detail 具名 GIT_UNAVAILABLE，且全输出零 "Traceback"（§1「不得 traceback」）；
    N2 伪造产品版本字面量写进 active 扫描面 ⇒ 必须 rc=1（mutation 合同的端到端面）；
    N3 越界路径负例：docs/owner/ 内放符号链接指向**仓库外**的漂移文件 ⇒ 该目标必须被
       裁剪并点名（rejected_paths + scan_surface_within_repo 判红），其漂移内容
       **不得**出现在 errors/stdout（不静默计入，也不静默丢弃）；同内容的仓库内文件由
       N2 证明仍会被抓 ⇒「被拒」不是「看不见」；
    N4 判别力负例（结构判定 != 行级豁免）：同一行内 DOI + 伪造产品版本 ⇒ 版本必须判红，
       DOI 三元组本身不得判红；只有 DOI 的行由 P2 证明不判红。
    """
    import shutil
    import tempfile
    problems: list[str] = []
    me = os.path.abspath(__file__)
    # me = <repo>/eng/tools/doccheck/check_version_namespaces.py ⇒ 上溯 4 层
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(me))))
    base = open(os.path.join(repo, "VERSION"), encoding="utf-8").read().strip()

    def _run(root: str, env=None):
        return subprocess.run([sys.executable, me, "--root", root],
                              capture_output=True, text=True, timeout=180, env=env)

    def _env(root: str) -> dict:
        # 阻断向上寻找 .git: 临时树必须是「非 git 树」, 结果不受调用点影响
        return dict(os.environ, GIT_CEILING_DIRECTORIES=root)

    def _doc(proc) -> dict:
        try:
            return json.loads(proc.stdout)
        except Exception:                                     # noqa: BLE001
            return {}

    def _res(doc: dict, name: str) -> dict:
        for row in doc.get("results", []):
            if row.get("check") == name:
                return row
        return {}

    def _git_init(root: str) -> None:
        """把临时树变成**真 git 仓**（夹具先例: check_doc_index.py::_git_init）。

        只有 git 面可用时, rc 才**由版本命名空间判据单独决定** —— 否则同树的
        GIT_UNAVAILABLE 降级面会把判红掩盖掉, 负例就退化成恒真。
        """
        for cmd in (["git", "init", "-q"],
                    ["git", "add", "-A"],
                    ["git", "-c", "user.email=gov003@example.invalid",
                     "-c", "user.name=GOV003", "commit", "-qm", "fixture"]):
            subprocess.run(cmd, cwd=root, capture_output=True, text=True, timeout=60)

    def _mk_tree(prefix: str, files: dict, git: bool = True) -> str:
        tree = tempfile.mkdtemp(prefix=prefix)
        os.makedirs(os.path.join(tree, "eng", "tools", "doccheck"))
        os.makedirs(os.path.join(tree, "docs", "owner"))
        shutil.copy2(me, os.path.join(tree, "eng", "tools", "doccheck",
                                      "check_version_namespaces.py"))
        shutil.copy2(os.path.join(repo, "eng", "tools", "gen_version.py"),
                     os.path.join(tree, "eng", "tools", "gen_version.py"))
        shutil.copy2(os.path.join(repo, "VERSION"), os.path.join(tree, "VERSION"))
        for rel, text in files.items():
            p = os.path.join(tree, rel)
            os.makedirs(os.path.dirname(p), exist_ok=True)
            with open(p, "w", encoding="utf-8") as f:
                f.write(text)
        if git:
            _git_init(tree)
        return tree

    p1 = _run(repo)
    if p1.returncode != 0:
        problems.append("P1 真仓库未判绿: rc=%d %s" % (p1.returncode, p1.stdout[:160]))

    # P2 正例（合成树, 命名空间一致）: 当前版本串 + 文献 DOI 引用
    doi_1 = "10.1080/00401706.1977.10489534"
    doi_2 = "10.1093/biomet/26.4.404"
    ver_line = ("产品版本唯一源：根 " + chr(96) + "VERSION" + chr(96) + " = " + chr(96)
                + base + chr(96) + "（生成串 " + chr(96) + base + "+g<commit12>"
                + chr(96) + "）。\n")
    refs = ("参考文献：[DOI %s](https://doi.org/%s)\n"
            "统计口径：[DOI %s](https://doi.org/%s)\n"
            % (doi_1, doi_1, doi_2, doi_2))
    tmp_p2 = _mk_tree("astrocs_vn_clean_", {"docs/owner/REFS.md": ver_line + refs})
    try:
        p2 = _run(tmp_p2, _env(tmp_p2))
        doc2 = _doc(p2)
        if p2.returncode != 0:
            problems.append("P2 命名空间一致的合成树未判绿: rc=%d %s"
                            % (p2.returncode, p2.stdout[:200]))
        if _res(doc2, "active_scan").get("pass") is not True:
            problems.append("P2 命名空间一致的合成树扫描面未判绿: %r"
                            % (_res(doc2, "active_scan"),))
        if doc2.get("errors"):
            problems.append("P2 命名空间一致的合成树出现漂移条目: %r"
                            % (doc2.get("errors")[:3],))
        if _res(doc2, "scan_surface_within_repo").get("pass") is not True:
            problems.append("P2 合成树扫描面越界: %r" % (doc2.get("rejected_paths"),))
        notes_rows = doc2.get("notes", [])
        notes2 = " || ".join(notes_rows)
        if doi_1 not in notes2 or doi_2 not in notes2:
            problems.append("P2 DOI 引用标识符未落进标识符命名空间 notes: %r"
                            % (notes2[:200],))
        # 报告路径必须相对 repo 根 (旧实现相对 eng/ ⇒ 显示成 ../docs/owner/... 的越界假象)
        if not notes_rows or not all(n.startswith("docs/owner/REFS.md:") for n in notes_rows):
            problems.append("P2 报告路径未相对 repo 根解析: %r" % (notes_rows[:3],))
    finally:
        shutil.rmtree(tmp_p2, ignore_errors=True)

    tmp = tempfile.mkdtemp(prefix="astrocs_vn_nogit_")
    try:
        os.makedirs(os.path.join(tmp, "eng", "tools", "doccheck"))
        shutil.copy2(me, os.path.join(tmp, "eng", "tools", "doccheck",
                                      "check_version_namespaces.py"))
        shutil.copy2(os.path.join(repo, "eng", "tools", "gen_version.py"),
                     os.path.join(tmp, "eng", "tools", "gen_version.py"))
        shutil.copy2(os.path.join(repo, "VERSION"), os.path.join(tmp, "VERSION"))
        env = dict(os.environ, GIT_CEILING_DIRECTORIES=tmp)
        n1 = _run(tmp, env)
        blob = n1.stdout + n1.stderr
        if n1.returncode == 0:
            problems.append("N1 非 git 树未判红（依赖不可用必须 fail-closed）")
        if "GIT_UNAVAILABLE" not in blob:
            problems.append("N1 未具名 GIT_UNAVAILABLE")
        if "Traceback" in blob:
            problems.append("N1 出现 traceback（§1 禁止）")
        try:
            doc = json.loads(n1.stdout)
        except Exception:                                     # noqa: BLE001
            doc = {}
        if doc.get("git_face", {}).get("available") is not False:
            problems.append("N1 git_face 未留痕 available=false: %r" % (doc.get("git_face"),))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # git 仓夹具: git 面可用时 rc 才由版本命名空间判据单独决定 (否则降级面掩盖判红)
    tmp2 = _mk_tree("astrocs_vn_forge_", {
        "docs/owner/FORGED.md": "发布产品版本 9.9.9-alpha.1 与 1.2.3 正式版\n"})
    try:
        n2 = _run(tmp2, _env(tmp2))
        doc_n2 = _doc(n2)
        if n2.returncode == 0:
            problems.append("N2 伪造产品版本字面量未被判红")
        # 判别力断言: 必须**具名**抓到伪造串 (只断言 rc!=0 会被同树 git 面判红掩盖)
        errs_n2 = "\n".join(doc_n2.get("errors", []))
        if "9.9.9-alpha.1" not in errs_n2 or "1.2.3" not in errs_n2:
            problems.append("N2 伪造串未进 errors（判据未命中）: %r" % (errs_n2[:200],))
        if "FORGED.md" not in errs_n2:
            problems.append("N2 漂移条目未点名仓库内文件 FORGED.md: %r" % (errs_n2[:200],))
    finally:
        shutil.rmtree(tmp2, ignore_errors=True)

    # N3 越界路径负例: 仓库外漂移文件经符号链接进入扫描面
    outside = tempfile.mkdtemp(prefix="astrocs_vn_outside_")
    tmp_n3 = _mk_tree("astrocs_vn_escape_", {})
    try:
        with open(os.path.join(outside, "drift.md"), "w", encoding="utf-8") as f:
            f.write("发布产品版本 9.9.9-alpha.1 与 1.2.3 正式版\n")
        os.symlink(os.path.join(outside, "drift.md"),
                   os.path.join(tmp_n3, "docs", "owner", "OUTSIDE.md"))
        n3 = _run(tmp_n3, _env(tmp_n3))
        doc3 = _doc(n3)
        if n3.returncode == 0:
            problems.append("N3 越界扫描目标未判红（必须 fail-closed）")
        if not any("OUTSIDE.md" in r for r in doc3.get("rejected_paths", [])):
            problems.append("N3 越界目标未被裁剪点名: %r" % (doc3.get("rejected_paths"),))
        if _res(doc3, "scan_surface_within_repo").get("pass") is not False:
            problems.append("N3 scan_surface_within_repo 未判红")
        if doc3.get("errors"):
            problems.append("N3 越界目标内容被静默计入扫描面: %r"
                            % (doc3.get("errors")[:3],))
        # 扫描面计数证明越界目标**未**被计入 (N3 树内只有 VERSION 一个合法目标;
        # 若被静默计入, 计数会是 2 且 errors 会带上漂移内容)
        detail3 = _res(doc3, "active_scan").get("detail", "")
        if not detail3.startswith("扫描 1 文件"):
            problems.append("N3 越界目标被计入扫描面 (active_scan detail=%r)" % (detail3,))
    finally:
        shutil.rmtree(tmp_n3, ignore_errors=True)
        shutil.rmtree(outside, ignore_errors=True)

    # N4 判别力负例: 同一行 DOI + 伪造产品版本 ⇒ 版本必须判红（结构判定 != 行级豁免）
    tmp_n4 = _mk_tree("astrocs_vn_doi_mix_", {"docs/owner/MIXED.md": (
        "参考文献：[DOI %s](https://doi.org/%s)\n"
        "发布产品版本 9.9.9-alpha.1，附 [DOI %s](https://doi.org/%s)\n"
        % (doi_1, doi_1, doi_2, doi_2))})
    try:
        n4 = _run(tmp_n4, _env(tmp_n4))
        doc4 = _doc(n4)
        if n4.returncode == 0:
            problems.append("N4 行内伪造产品版本未被判红")
        errs4 = "\n".join(doc4.get("errors", []))
        if "9.9.9-alpha.1" not in errs4:
            problems.append("N4 伪造产品版本未进 errors: %r" % (errs4[:200],))
        if ("裸版本字面量 %s" % doi_1.split("/")[1]) in errs4 or "裸版本字面量 26.4.404" in errs4:
            problems.append("N4 DOI 标识符被误判为产品版本漂移（判据退化）")
        notes4 = " || ".join(doc4.get("notes", []))
        if doi_2 not in notes4:
            problems.append("N4 行内 DOI 未走标识符命名空间判定: %r" % (notes4[:200],))
    finally:
        shutil.rmtree(tmp_n4, ignore_errors=True)

    for p in problems:
        print("  - %s" % p)
    print("SELF_TEST %s positives=2 negatives=4" % ("PASS" if not problems else "FAIL"))
    return 0 if not problems else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--self-test", action="store_true",
                    help="可执行正/负例面（P1/P2 判绿；非 git 树 / 伪造版本字面量 / "
                         "越界扫描路径 / DOI 行内夹带伪造版本 各负例必须判红）")
    args = ap.parse_args()
    if args.self_test:
        return _self_test()
    root = os.path.abspath(args.root)
    results: list[dict] = []
    errors: list[str] = []
    warnings: list[str] = []
    # git 面留痕（§1「显式降级 + 留痕」）：gen_version 的版本串只能从 git HEAD 派生，
    # git 不可用时该分量不可得 ⇒ 判红（不给结论），并在 JSON 里具名记录原因。
    git_face: dict = {"available": True, "detail": "git 工作树"}

    try:
        ver = root_version(root)
        base_num, alpha_n = re.match(r"^(\d+\.\d+\.\d+)-alpha\.(\d+)$", ver).groups()
        alpha_n = int(alpha_n)
        results.append(check("version_source_format", True, f"VERSION = {ver}"))
    except SystemExit as exc:
        print(exc)
        return 1

    # 生成链: gen_version.py 必须从 VERSION 派生, 输出前缀一致
    gen = os.path.join(root, "eng", "tools", "gen_version.py")
    gen_ok = False
    if os.path.isfile(gen):
        try:
            r = subprocess.run([sys.executable, gen, "--json"],
                               cwd=root, capture_output=True, text=True, timeout=60)
            if r.returncode == 0:
                rep = json.loads(r.stdout)
                if rep.get("version", "").startswith(ver + "+g") and rep.get("prerelease") == "alpha":
                    gen_ok = True
                    results.append(check("gen_version_from_source", True,
                                         f"gen_version --json version 前缀 = {ver}"))
                else:
                    results.append(check("gen_version_from_source", False,
                                         f"gen_version 输出前缀不符: {rep.get('version')!r}"))
            else:
                # 依赖不可用 vs 版本链不符必须分开命名（01_CHECKS §1「显式降级 + 点名」）：
                # gen_version 在非 git 树 / 无 git 时以 GIT_UNAVAILABLE + rc=2 显式降级；
                # 原实现把它的 traceback 文本原样塞进 detail，读起来像"版本不符"（语义错位）。
                why = (r.stderr.strip().splitlines() or [""])[0]
                if "GIT_UNAVAILABLE" in why:
                    git_face = {"available": False, "detail": why[:200]}
                    results.append(check("gen_version_from_source", False,
                                         f"依赖不可用（GIT_UNAVAILABLE，fail-closed rc={r.returncode}）: "
                                         f"{why[:160]}"))
                else:
                    results.append(check("gen_version_from_source", False,
                                         f"gen_version 退出 {r.returncode}: {r.stderr[:120]}"))
        except Exception as exc:  # noqa: BLE001
            results.append(check("gen_version_from_source", False, str(exc)))
    else:
        results.append(check("gen_version_from_source", False, "eng/tools/gen_version.py 缺失"))
    # 输出串形态断言 (机器可读, 供 TASK_RESULT evidence)
    try:
        r = subprocess.run([sys.executable, gen, "--json"], cwd=root,
                           capture_output=True, text=True, timeout=60)
        shape = json.loads(r.stdout)["version"]
        shape_ok = bool(re.match(rf"^{re.escape(base_num)}-alpha\.{alpha_n}\+g[0-9a-f]{{12}}(\.dirty)?$", shape))
        results.append(check("version_string_shape", shape_ok, f"生成串形态 {shape}"))
    except Exception:  # noqa: BLE001
        pass

    # active 允许路径扫描 (memory/CHANGELOG 日志驻留点 -> warnings-only)
    # 扫描面: 一律按 repo 根解析 + realpath is_relative_to(repo) 校验; 越界目标被裁剪
    # 并点名判红 (不静默计入); 空扫描面 fail-closed (01_CHECKS §1)。
    targets, rejected, absent = collect_scan_targets(root)
    scanned = 0
    notes: list[str] = []
    log_warn_before = len(warnings)
    for path in targets:
        scanned += 1
        scan_file(path, base_num, alpha_n, errors, warnings, repo_root=root, notes=notes)
    log_warnings = len(warnings) - log_warn_before
    if errors:
        scan_detail = f"扫描 {scanned} 文件; 漂移={len(errors)}"
    elif scanned == 0:
        scan_detail = ("扫描面为空 ⇒ fail-closed (docs/ci/01_CHECKS.md §1: "
                       "scanned==0 ⇒ rc!=0)")
    else:
        scan_detail = (f"扫描 {scanned} 文件 (允许路径 active + 日志驻留点), "
                       "无其他产品版本")
    results.append(check("active_scan", (not errors) and scanned > 0, scan_detail))
    results.append(check("log_warnings_only", True,
                         f"memory/CHANGELOG 等日志驻留点 {log_warnings} 条漂移仅警告 "
                         "(GOV-005 收敛对象, 不判 FAIL)"))
    results.append(check("scan_surface_within_repo", not rejected,
                         f"{scanned} 个扫描目标 realpath 均在 repo 根内"
                         if not rejected else
                         "越界扫描目标被裁剪并判红: " + "; ".join(rejected[:5])))
    results.append(check("namespace_notes", True,
                         f"引用标识符 (DOI/URL) 命名空间内三元组 {len(notes)} 条 "
                         "(文献标识符, 不判 FAIL) — 记于 JSON notes"))

    # 反误报: 豁免样本必须不报错 (FITS 4.0 / HiPS 1.0 / ABI v1)
    probe_lines = [
        "标准 IVOA HiPS 1.0 输出 (HiPS 格式版本非产品版本)",
        "FITS 4.0 规范与 CFITSIO 4.6.4 供应商版本",
        "ACS_ABI_VERSION_V1 = 1u (C ABI 命名空间, 非产品版本)",
        "schema_version = 1 (data-schema 命名空间, 非产品版本)",
        "DatabaseVersion=1.0.0 (Gaia 库标识)",
        "占位写法 X.Y.Z / MAJOR.MINOR.PATCH 不参与比较",
    ]
    false_pos = []
    for ln in probe_lines:
        for m in ALPHA_INLINE.finditer(ln):
            false_pos.append(f"alpha 误报: {ln[:60]}")
        if not exempt_line(ln):
            for m in BASE_INLINE.finditer(ln):
                if m.group(1) != base_num:
                    false_pos.append(f"裸三元组误报: {ln[:60]}")
    results.append(check("no_false_positive_fits_hips_abi", not false_pos,
                         "FITS 4.0/HiPS 1.0/ABI v1/schema/DatabaseVersion/占位 均不误报"
                         if not false_pos else "; ".join(false_pos[:5])))

    # mutation 合同: 伪造产品版本必须被抓
    import tempfile
    mutation_hit = False
    with tempfile.TemporaryDirectory() as td:
        fake = os.path.join(td, "FAKE.md")
        with open(fake, "w", encoding="utf-8") as f:
            f.write("发布产品版本 9.9.9-alpha.1 与 1.2.3 正式版\n")
        me: list[str] = []
        mw: list[str] = []
        scan_file(fake, base_num, alpha_n, me, mw)
        mutation_hit = any("9.9.9-alpha.1" in e or "1.2.3" in e for e in me)
    results.append(check("mutation_forged_version_fails", mutation_hit,
                         "伪造 9.9.9-alpha.1/1.2.3 被抓" if mutation_hit else "mutation 未命中!"))

    out_of_scope: list[str] = []
    # 他人路径遗留 (不判 FAIL; 前台集成/后续 GOV 任务协调)
    legacy = {
        "docs/VERSIONING.md": "VER-001 遗留: '当前冻结基线 0.10.0-alpha.2' (非允许路径, 需 GOV-005/前台收敛)",
        "CMakeLists.txt": "project(acsd VERSION 0.10.0) 字面量 (BLD-002 配合从 VERSION 生成; 主串已 file(READ) 生成)",
        "eng/tests/version/test_version_consistency.py": "test_01/05 硬编码 0.10.0-alpha.2 断言 (QA 配合更新)",
        "eng/tests/quality/test_linux_release.py": "assertIn('acsd 0.10.0') (QA 配合更新)",
        "eng/tests/backend/test_cpu_profile.py": "gen --version 0.10.0-alpha.2 (QA 配合更新)",
        "eng/tools/check_final_traceability.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "eng/tools/check_release_consistency.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "eng/tools/check_reproducible_build.py": "checker 硬编码 == 0.10.0-alpha.2 (QA/前台配合)",
        "eng/tools/make_linux_release.py": "回退串硬编码 0.10.0-alpha.2 (打包 owner 配合; 主路径读 VERSION)",
        "eng/tools/make_windows_release.py": "回退串硬编码 0.10.0-alpha.2 (打包 owner 配合; 主路径读 VERSION)",
        "docs/DOCUMENT_INDEX.yaml": "base_product_version 0.10.0-alpha.2 (GOV-002 基线修订字段; 需补登 docs/owner/RELEASE_STATUS.md §2)",
    }
    for p, why in legacy.items():
        if os.path.exists(os.path.join(root, p)):
            out_of_scope.append(f"{p}: {why}")

    results.append(check("known_legacy_reported", True,
                         f"登记 {len(out_of_scope)} 项他人路径遗留 (集成协调): "
                         + "; ".join(p.split(":")[0] for p in out_of_scope)))
    if warnings:
        results.append(check("warnings_note", True,
                             f"{len(warnings)} 条豁免/警告 (history 轮次引用, 不判 FAIL)"))

    passed = all(r["pass"] for r in results)
    out = {
        "tool": "eng/tools/doccheck/check_version_namespaces.py",
        "version": "1.0.0",
        "task": "GOV-003",
        "owner": "SA-GOV-01",
        "root": root,
        "product_version": ver,
        "results": sorted(results, key=lambda r: r["check"]),
        # 判红条目必须逐条可见 (旧输出只有 '漂移=N' 计数 + warnings 字段 ⇒ FAIL 到底由
        # 哪些条目构成在 JSON 里读不出来, 只能靠复算)。
        "errors": errors,
        "warnings": warnings,
        "notes": notes,
        "rejected_paths": rejected,
        "absent_declared_targets": absent,
        "out_of_scope_legacy": out_of_scope,
        "git_face": git_face,
        "verdict": "VERSION_NAMESPACES_PASS" if passed else "VERSION_NAMESPACES_FAIL",
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
