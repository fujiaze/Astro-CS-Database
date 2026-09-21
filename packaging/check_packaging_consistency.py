#!/usr/bin/env python3
"""AstroCS 打包面一致性检查器（W5-PKG-001）

职责（单一入口，全部 fail-closed）：核对「产品清单 / 安装树合同 / 依赖锁 / 安装规则 /
构建图声明的外部依赖」四处登记面是否同源一致，并在负例注入下必须判红。

判据（每条独立成 finding 码）：
  C1 VERSION-SINGLE-SOURCE  packaging/ 内登记面的版本串必须逐字等于根 VERSION；
                            schema 不得携带写死的 alpha 版本常量（陈旧第二事实源）。
  C2 TREE-CLOSURE           安装树合同 units[*].install_path 全集必须逐字等于
                            「产品清单 units[*].rel_path ∪ packaging/licenses/* ∪
                            packaging/schemas/*.json ∪ {astrocs.product.json}」
                            （漏列=合同少登记实际进包文件；多列=合同登记不存在的文件）。
  C3 PLATFORM-PARITY        Linux 清单与 Windows 清单模板的 unit 集合必须同构
                            （unit_id/kind/module_id/abi_version/status 全等；
                            rel_path 仅允许 .so <-> .dll 与 lib 前缀差异）。
  C4 LOCK-VS-TREE           依赖锁 production_dependencies[*].vendored_path 必须存在；
                            file_sha256 / license_file_sha256 必须与实文件一致；
                            aggregate_sha256 必须按声明的算法可复算；
                            tracked_files 必须与 git 实树一致。
  C5 LOCK-VS-CMAKE          根 CMakeLists 链接的外部库名必须全部在锁内登记；
                            production 依赖必须显式声明引用面（referenced_by）或
                            显式登记为 UNREFERENCED + 理由，不得静默多列。
  C6 RULES-VS-CONTRACT      eng/cmake/install_layout.cmake 的 install(TARGETS ...) 目标
                            必须与合同 unit 一一对应；schemas/licenses 不得按目录通配
                            安装（白名单原则）。
  C7 LICENSE-REGISTRY       production 依赖必须有随包许可文本且已在安装树合同登记；
                            system 依赖必须有 license 字段；合同 license 单元必须实存；
                            test-only oracle 的消费者路径必须实存。

用法:
  python3 packaging/check_packaging_consistency.py --root <repo> [--json-out P]
  python3 packaging/check_packaging_consistency.py --root <repo> --self-test

退出码: 0 = 全部一致; 1 = 发现不一致; 2 = 输入不可用 (fail-closed, 不静默通过)。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

VERSION_FILE = "VERSION"
CONTRACT = "packaging/install-tree.contract.json"
LINUX_MANIFEST = "packaging/astrocs.product.json"
WIN_TEMPLATE = "eng/cmake/astrocs.product.windows.json.in"
LOCK = "packaging/dependency-lock.json"
SCHEMA_DIR = "packaging/schemas"
LICENSE_DIR = "packaging/licenses"
INSTALL_RULES = "eng/cmake/install_layout.cmake"
CFITSIO_SOURCES = "eng/cmake/cfitsio_sources.cmake"
ROOT_CMAKE = "CMakeLists.txt"
MANIFEST_BASENAME = "astrocs.product.json"
DL_LIBS_TOKEN = "$" + "{CMAKE_DL_LIBS}"

# 版本常量形态: 出现在 packaging/schemas/** 即陈旧第二事实源
VERSION_LITERAL_RE = re.compile(r"\b\d+\.\d+\.\d+-alpha\.\d+\b")
# 根 CMakeLists 里允许出现的外部（非本仓库 target）链接库名
EXTERNAL_LIB_TOKENS = {
    "gsl", "gslcblas", "z", "zstd", "lz4", "m", "dl", "pthread",
    "kernel32", "user32",
}
# 依赖锁里代表「系统线程/OpenMP/数学库」的等价登记名
TOKEN_ALIASES = {
    "Threads::Threads": ("threads", "pthread"),
    "OpenMP::OpenMP_CXX": ("openmp",),
    DL_LIBS_TOKEN: ("libdl", "dl"),
    "m": ("libm", " m"),
    "z": ("zlib", " z"),
}


class InputUnavailable(Exception):
    """输入缺失/不可解析 —— 必须 fail-closed。"""


def read_text(root: Path, rel: str) -> str:
    p = root / rel
    if not p.is_file():
        raise InputUnavailable(f"ANCHOR_STALE: {rel} 不存在")
    return p.read_text(encoding="utf-8", errors="strict")


def load_json(root: Path, rel: str) -> dict:
    try:
        return json.loads(read_text(root, rel))
    except json.JSONDecodeError as e:
        raise InputUnavailable(f"{rel} 不是合法 JSON: {e}") from e


def sha256_file(p: Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git_blob_sha1(p: Path) -> str:
    data = p.read_bytes()
    return hashlib.sha1(b"blob %d\0" % len(data) + data).hexdigest()


def strip_prose(path_str: str) -> str:
    """消费者条目允许带人读后缀（如 'compare_astrometry.py (real 模式)'）。"""
    return re.sub(r"\s*[（(].*$", "", path_str.strip())


def list_tracked(root: Path, rel_dir: str) -> list:
    try:
        out = subprocess.run(
            ["git", "ls-files", "-z", "--", rel_dir], cwd=str(root),
            capture_output=True, check=True)
    except (OSError, subprocess.CalledProcessError) as e:
        raise InputUnavailable(f"ANCHOR_STALE: git ls-files {rel_dir} 不可用: {e}") from e
    return sorted(x for x in out.stdout.decode("utf-8").split("\0") if x)


def parse_cfitsio_sources(root: Path) -> list:
    text = read_text(root, CFITSIO_SOURCES)
    srcs = re.findall(r"^\s+(\S+\.c)\s*$", text, re.M)
    if not srcs:
        raise InputUnavailable(f"ANCHOR_STALE: {CFITSIO_SOURCES} 未解析出生产源")
    return srcs


def aggregate_sha256(root: Path, algo: str, sources: list) -> str:
    if algo == "sha256(concat(sorted(git-blob-sha1 of sources)))":
        joined = "".join(sorted(git_blob_sha1(root / s) for s in sources))
    elif algo == "sha256(concat(sorted(file-sha256 of sources)))":
        joined = "".join(sorted(sha256_file(root / s) for s in sources))
    else:
        raise InputUnavailable(f"未知 aggregate_sha256_algo: {algo!r} (fail-closed)")
    return hashlib.sha256(joined.encode("ascii")).hexdigest()


# ── C1 版本单源 ────────────────────────────────────────────────────────────────
def check_version_single_source(root: Path, findings: list):
    ver = read_text(root, VERSION_FILE).strip()
    if not ver:
        raise InputUnavailable("VERSION 为空")
    for rel, key in ((LINUX_MANIFEST, "product_version"),
                     (CONTRACT, "target_version"),
                     (LOCK, "target_version")):
        got = load_json(root, rel).get(key)
        if got != ver:
            findings.append(("C1", f"{rel}:{key}={got!r} != VERSION {ver!r}"))
    sdir = root / SCHEMA_DIR
    if not sdir.is_dir():
        raise InputUnavailable(f"ANCHOR_STALE: {SCHEMA_DIR} 不存在")
    for p in sorted(sdir.glob("*.json")):
        text = p.read_text(encoding="utf-8")
        for lineno, line in enumerate(text.splitlines(), 1):
            for m in VERSION_LITERAL_RE.findall(line):
                findings.append(("C1", f"{p.relative_to(root)}:{lineno} 写死版本常量 "
                                      f"{m!r} (应只校验形态, 值唯一源=根 VERSION)"))


# ── 安装树相对路径 → 仓库源路径（licenses/schemas/manifest 三类为只读复制）──
def install_source_path(root: Path, rel: str) -> Path:
    if rel.startswith("licenses/"):
        return root / LICENSE_DIR / rel.split("/", 1)[1]
    if rel.startswith("schemas/"):
        return root / SCHEMA_DIR / rel.split("/", 1)[1]
    if rel == MANIFEST_BASENAME:
        return root / LINUX_MANIFEST
    return root / rel


# ── C2 安装树闭包 ─────────────────────────────────────────────────────────────
def expected_tree_files(root: Path) -> set:
    manifest = load_json(root, LINUX_MANIFEST)
    ex = {u["rel_path"] for u in manifest.get("units", [])}
    for d, prefix in ((LICENSE_DIR, "licenses"), (SCHEMA_DIR, "schemas")):
        dd = root / d
        if not dd.is_dir():
            raise InputUnavailable(f"ANCHOR_STALE: {d} 不存在")
        ex |= {f"{prefix}/{p.name}" for p in dd.iterdir() if p.is_file()}
    ex.add(MANIFEST_BASENAME)
    return ex


def check_tree_closure(root: Path, findings: list):
    contract = load_json(root, CONTRACT)
    units = contract.get("units")
    if not units:
        raise InputUnavailable(f"{CONTRACT} 无 units (fail-closed)")
    declared = {u["install_path"] for u in units}
    expected = expected_tree_files(root)
    for rel in sorted(declared - expected):
        findings.append(("C2", f"合同多列 {rel}（不在实际进包集内）"))
    for rel in sorted(expected - declared):
        findings.append(("C2", f"合同漏列 {rel}（实际进包但未登记白名单）"))


# ── C3 双平台清单同构 ─────────────────────────────────────────────────────────
def _win_manifest(root: Path) -> dict:
    text = read_text(root, WIN_TEMPLATE)
    text = text.replace("@ASTROCS_BASE_VERSION@", "0.0.0").replace(
        "@ASTROCS_GIT_COMMIT@", "0" * 40)
    return json.loads(text)


def _norm_rel(rel: str) -> str:
    base = rel.replace("\\", "/")
    if base.startswith("lib") and base.endswith(".so"):
        base = base[3:]
    return re.sub(r"\.(so|dll|exe)$", "", base)


def check_platform_parity(root: Path, findings: list):
    lin = {u["unit_id"]: u for u in load_json(root, LINUX_MANIFEST).get("units", [])}
    win = {u["unit_id"]: u for u in _win_manifest(root).get("units", [])}
    if not lin or not win:
        raise InputUnavailable("平台清单 units 为空 (fail-closed)")
    for uid in sorted(set(lin) - set(win)):
        findings.append(("C3", f"{uid} 仅 Linux 清单登记（Windows 漏列）"))
    for uid in sorted(set(win) - set(lin)):
        findings.append(("C3", f"{uid} 仅 Windows 清单登记（Linux 漏列）"))
    for uid in sorted(set(lin) & set(win)):
        a, b = lin[uid], win[uid]
        for key in ("kind", "module_id", "abi_version"):
            if a.get(key) != b.get(key):
                findings.append(("C3", f"{uid}.{key}: linux={a.get(key)!r} "
                                      f"windows={b.get(key)!r} 不一致"))
        if a.get("status") != b.get("status"):
            findings.append(("C3", f"{uid}.status: linux={a.get('status')!r} "
                                  f"windows={b.get('status')!r} 不一致（同源同一提交）"))
        if _norm_rel(a["rel_path"]) != _norm_rel(b["rel_path"]):
            findings.append(("C3", f"{uid}.rel_path: {a['rel_path']} <-> "
                                  f"{b['rel_path']} 非同构"))


# ── C4 依赖锁 ↔ 实树 ─────────────────────────────────────────────────────────
def check_lock_vs_tree(root: Path, findings: list, allow_missing_git: bool = False,
                       notes: list = None):
    lock = load_json(root, LOCK)
    prod = lock.get("production_dependencies") or []
    if not prod:
        raise InputUnavailable("dependency-lock.production_dependencies 为空 (fail-closed)")
    cfitsio_srcs = parse_cfitsio_sources(root)
    for dep in prod:
        name = dep.get("name", "?")
        vp = dep.get("vendored_path")
        if not vp:
            findings.append(("C4", f"{name} 缺 vendored_path"))
            continue
        target = root / vp
        if not target.exists():
            findings.append(("C4", f"{name} vendored_path 不存在: {vp}"))
            continue
        claimed = dep.get("file_sha256")
        hash_of = dep.get("file_sha256_of")
        if claimed:
            fp = root / (hash_of or vp)
            if not fp.is_file():
                findings.append(("C4", f"{name} file_sha256_of 不存在: {hash_of}"))
            elif sha256_file(fp) != claimed:
                findings.append(("C4", f"{name} file_sha256 不匹配: {hash_of}"))
        agg = dep.get("aggregate_sha256")
        if agg:
            algo = dep.get("aggregate_sha256_algo")
            if not algo:
                findings.append(("C4", f"{name} 有 aggregate_sha256 但缺 "
                                      f"aggregate_sha256_algo（不可复算）"))
            else:
                got = aggregate_sha256(root, algo, cfitsio_srcs)
                if got != agg:
                    findings.append(("C4", f"{name} aggregate_sha256 复算不符: "
                                          f"got {got[:16]}… want {agg[:16]}…"))
        lf, lfh = dep.get("license_file"), dep.get("license_file_sha256")
        if lf:
            if not (root / lf).is_file():
                findings.append(("C4", f"{name} license_file 不存在: {lf}"))
            elif lfh and sha256_file(root / lf) != lfh:
                findings.append(("C4", f"{name} license_file_sha256 不匹配: {lf}"))
        elif lfh:
            findings.append(("C4", f"{name} 有 license_file_sha256 但无 license_file 路径"))
        tf = dep.get("tracked_files")
        if tf is not None:
            try:
                actual = len(list_tracked(root, vp))
            except InputUnavailable:
                # 自测夹具是 /tmp 非 git 仓库: 该项在自测中显式跳过（真树强制判红）
                if not allow_missing_git:
                    raise
                if notes is not None:
                    notes.append(f"{name} tracked_files 自测跳过（夹具非 git 仓库）")
                actual = tf
            if actual != tf:
                findings.append(("C4", f"{name} tracked_files={tf} 与实树 {actual} 不符"))
        for entry in dep.get("referenced_by") or []:
            if not (root / entry).is_file():
                findings.append(("C4", f"{name} referenced_by 不存在: {entry}"))


# ── C5 依赖锁 ↔ CMake 实际依赖 ───────────────────────────────────────────────
def _declared_source_roots(root: Path) -> list:
    paths = [root / ROOT_CMAKE, root / "cli" / "CMakeLists.txt"]
    paths.extend(sorted((root / "eng" / "cmake").glob("*")))
    paths.extend(sorted(root.glob("lib/*/CMakeLists.txt")))
    paths.extend(sorted(root.glob("lib/*/*/CMakeLists.txt")))
    return [p for p in paths if p.is_file()]


def _linked_external_tokens(cmake_text: str) -> set:
    linked = set()
    for m in re.finditer(r"target_link_libraries\(([^)]*)\)", cmake_text, re.S):
        for token in re.sub(r"\s+", " ", m.group(1)).split(" "):
            tok = token.strip()
            if not tok or tok in ("PRIVATE", "PUBLIC", "INTERFACE") or tok.startswith("astrocs"):
                continue
            bare = re.sub(r"^\$\{|\}$", "", tok)
            if tok in EXTERNAL_LIB_TOKENS or tok in (DL_LIBS_TOKEN,
                                                     "Threads::Threads",
                                                     "OpenMP::OpenMP_CXX"):
                linked.add(tok)
            elif bare in EXTERNAL_LIB_TOKENS or bare in ("CMAKE_DL_LIBS",):
                linked.add(tok if tok in (DL_LIBS_TOKEN,) else bare)
    return linked


def check_lock_vs_cmake(root: Path, findings: list):
    lock_text = read_text(root, LOCK)
    for tok in sorted(_linked_external_tokens(read_text(root, ROOT_CMAKE))):
        names = TOKEN_ALIASES.get(tok, (tok,))
        if not any(n.lower() in lock_text.lower() for n in names):
            findings.append(("C5", f"CMake 链接的外部库 {tok!r} 在依赖锁零登记（锁漏列）"))

    src_texts = {}
    for p in _declared_source_roots(root):
        src_texts[p] = p.read_text(encoding="utf-8", errors="ignore")
    for src_dir in ("lib", "include", "cli"):
        d = root / src_dir
        if not d.is_dir():
            continue
        for p in d.rglob("*"):
            if len(src_texts) >= 4000:
                break
            if p.is_file() and p.suffix in (".c", ".cc", ".cpp", ".h", ".hpp"):
                try:
                    src_texts[p] = p.read_text(encoding="utf-8", errors="ignore")
                except OSError:
                    continue

    for dep in load_json(root, LOCK).get("production_dependencies") or []:
        name = dep.get("name", "?")
        token = dep.get("referenced_token")
        refs = dep.get("referenced_by")
        status = dep.get("reference_status")
        if status == "UNREFERENCED":
            if not (dep.get("unreferenced_reason") or "").strip():
                findings.append(("C5", f"{name} 登记 UNREFERENCED 但缺 unreferenced_reason"))
            continue
        if not refs:
            findings.append(("C5", f"{name} 未声明 referenced_by/reference_status"
                                  f"（生产依赖引用面不可判）"))
            continue
        if not token:
            findings.append(("C5", f"{name} 缺 referenced_token（引用面不可判）"))
            continue
        if not any(token in t for t in src_texts.values()):
            findings.append(("C5", f"{name} 声明 production 但全树零引用 token "
                                  f"{token!r}（多列）"))


# ── C6 安装规则 ↔ 合同 ───────────────────────────────────────────────────────
def _strip_cmake_comments(text: str) -> str:
    """只看指令, 不看注释（注释里出现 FILES_MATCHING 是说明而非规则）。"""
    return "\n".join(ln for ln in text.splitlines()
                     if not ln.lstrip().startswith("#"))


def check_rules_vs_contract(root: Path, findings: list):
    rules = read_text(root, INSTALL_RULES)
    rules_code = _strip_cmake_comments(rules)
    root_cmake = _strip_cmake_comments(read_text(root, ROOT_CMAKE))
    if "install_layout.cmake" not in root_cmake:
        findings.append(("C6", f"{ROOT_CMAKE} 未 include {INSTALL_RULES}（锚失效）"))
    if "FILES_MATCHING" in rules_code:
        findings.append(("C6", f"{INSTALL_RULES} 用目录通配安装（FILES_MATCHING），"
                              f"违反白名单原则：安装集必须逐文件枚举"))
    targets = set()
    loop_vars = {}
    for m in re.finditer(r"foreach\(\s*(\w+)\s+([^)]*)", rules_code, re.S):
        var, names = m.group(1), m.group(2).split()
        loop_vars[var] = [n for n in names if n and not n.startswith("$")]
    for m in re.finditer(r"install\(TARGETS\s+([^\n]*)", rules_code):
        for tok in re.split(r"[\s)]+", m.group(1)):
            tok = tok.strip()
            if not tok or tok in ("LIBRARY", "RUNTIME", "DESTINATION", "COMPONENT",
                                  "ARCHIVE", "OPTIONAL", "EXPORT"):
                break
            var = re.fullmatch(r"\$\{(\w+)\}", tok)
            if var:
                targets.update(loop_vars.get(var.group(1), []))
            else:
                targets.add(tok)
    targets = {t for t in targets if t and not t.startswith("$") and "/" not in t}
    contract = load_json(root, CONTRACT)
    unit_kinds = {"exe", "runtime", "io", "module", "provider"}
    unit_paths = {u["install_path"]: u for u in contract.get("units", [])
                  if u.get("kind") in unit_kinds}
    bases = {Path(p).name for p in unit_paths}
    for tgt in sorted(targets):
        cands = {tgt, f"{tgt}.so", f"lib{tgt}.so", f"{tgt}.dll", f"lib{tgt}.dll",
                 f"{tgt}.exe"}
        if not (cands & bases):
            findings.append(("C6", f"install(TARGETS {tgt}) 未在合同登记对应 install_path"))
    for path in sorted(unit_paths):
        stem = re.sub(r"\.(so|dll|exe)$", "", Path(path).name)
        if stem.startswith("lib"):
            stem = stem[3:]
        if stem not in targets:
            findings.append(("C6", f"合同登记 {path} 但安装规则无对应 install(TARGETS "
                                  f"{stem})"))


# ── C7 许可登记面 ────────────────────────────────────────────────────────────
def check_license_registry(root: Path, findings: list):
    lock = load_json(root, LOCK)
    contract = load_json(root, CONTRACT)
    lic_units = {u["install_path"] for u in contract.get("units", [])
                 if u.get("kind") == "license"}
    for rel in sorted(lic_units):
        if not install_source_path(root, rel).is_file():
            findings.append(("C7", f"合同登记许可单元 {rel} 实文件不存在"))
    for dep in lock.get("production_dependencies") or []:
        name = dep.get("name", "?")
        if not (dep.get("license") or "").strip():
            findings.append(("C7", f"生产依赖 {name} 缺 license 字段"))
        lf = dep.get("license_file")
        if not lf:
            findings.append(("C7", f"生产依赖 {name} 无随包许可文本登记（license_file）"))
        elif f"licenses/{Path(lf).name}" not in lic_units:
            findings.append(("C7", f"生产依赖 {name} 的许可文本 {lf} 未登记进安装树合同"))
    for dep in lock.get("system_dependencies") or []:
        if not (dep.get("license") or "").strip():
            findings.append(("C7", f"系统依赖 {dep.get('name','?')} 缺 license 字段"))
    for dep in lock.get("test_only_oracles") or []:
        for c in dep.get("consumers") or []:
            c = strip_prose(c)
            if any(ch in c for ch in "*?"):
                if not list(root.glob(c)):
                    findings.append(("C7", f"test-only oracle {dep.get('name')} 消费者 "
                                          f"glob 零命中: {c}"))
            elif not (root / c).exists():
                findings.append(("C7", f"test-only oracle {dep.get('name')} 消费者不存在: {c}"))


CHECKS = (
    ("C1", check_version_single_source),
    ("C2", check_tree_closure),
    ("C3", check_platform_parity),
    ("C4", check_lock_vs_tree),
    ("C5", check_lock_vs_cmake),
    ("C6", check_rules_vs_contract),
    ("C7", check_license_registry),
)


def evaluate(root: Path, allow_missing_git: bool = False, notes: list = None) -> list:
    findings = []
    for code, fn in CHECKS:
        if code == "C4":
            fn(root, findings, allow_missing_git, notes)
        else:
            fn(root, findings)
    return findings


# ── 负例注入自测（ENGINEERING_SPEC §8 可执行负例面）─────────────────────────
SELFTEST_DIRS = [CONTRACT.rsplit("/", 1)[0], SCHEMA_DIR, LICENSE_DIR, "cmake", "cli"]


def _link_or_copy(s, d):
    """copytree copy_function: 同盘硬链接（大 vendored 树零拷贝）, 跨盘回退复制。"""
    try:
        os.link(s, d)
    except OSError:
        shutil.copy2(s, d)


def _copy(src: Path, dest: Path, repo: Path):
    rel = src.relative_to(repo)
    out = dest / rel
    out.parent.mkdir(parents=True, exist_ok=True)
    try:
        os.link(src, out)          # 同盘硬链接: 大 vendored 树零拷贝
    except OSError:
        shutil.copy2(src, out)


def _sandbox_lock_consumers(repo: Path):
    """夹具需带的依赖锁消费者/被证物路径（从锁自身解析, 不写死清单）。"""
    try:
        lock = json.loads((repo / LOCK).read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    out = []
    for dep in lock.get("production_dependencies") or []:
        if dep.get("vendored_path"):
            out.append(dep["vendored_path"])
        if dep.get("license_file"):
            out.append(dep["license_file"])
    for dep in lock.get("test_only_oracles") or []:
        out.extend(dep.get("consumers") or [])
    return out


def build_sandbox(repo: Path, dest: Path):
    shutil.copy2(repo / VERSION_FILE, dest / VERSION_FILE)
    shutil.copy2(repo / ROOT_CMAKE, dest / ROOT_CMAKE)
    copied = []
    for rel in sorted(SELFTEST_DIRS, key=len):
        # 祖先目录已整树复制则跳过（packaging 覆盖 packaging/schemas 等）
        if any(rel == c or rel.startswith(c + "/") for c in copied):
            continue
        src = repo / rel
        if src.is_dir():
            shutil.copytree(src, dest / rel,
                            ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
                            copy_function=_link_or_copy)
            copied.append(rel)
        else:
            (dest / rel).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dest / rel)
            copied.append(rel)
    for pat in ("lib/*/CMakeLists.txt", "lib/*/*/CMakeLists.txt", "include/**/*"):
        for src in sorted(repo.glob(pat)):
            if src.is_file():
                _copy(src, dest, repo)
    for entry in _sandbox_lock_consumers(repo):
        entry = strip_prose(entry)
        cands = sorted(repo.glob(entry)) if any(ch in entry for ch in "*?") else [repo / entry]
        for src in cands:
            if src.is_file():
                _copy(src, dest, repo)
            elif src.is_dir():
                shutil.copytree(src, dest / src.relative_to(repo),
                                ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
                                copy_function=_link_or_copy, dirs_exist_ok=True)


def _mutate_json(path: Path, fn):
    d = json.loads(path.read_text(encoding="utf-8"))
    fn(d)
    path.write_text(json.dumps(d, ensure_ascii=False, indent=2), encoding="utf-8")


def _inject_status_flip(r: Path):
    t = read_text(r, WIN_TEMPLATE)
    assert '"status": "SKELETON"' in t, "夹具无 SKELETON 状态可翻转"
    (r / WIN_TEMPLATE).write_text(
        t.replace('"status": "SKELETON"', '"status": "IMPLEMENTED"', 1), encoding="utf-8")


def _inject_wildcard(r: Path):
    t = read_text(r, INSTALL_RULES)
    (r / INSTALL_RULES).write_text(
        'install(DIRECTORY x FILES_MATCHING PATTERN "*.json")\n' + t, encoding="utf-8")


def self_test(repo: Path) -> int:
    ok = True
    cases = (
        ("positive (未注入)", lambda r: None, None),
        ("C1 manifest 版本漂移", lambda r: _mutate_json(
            r / LINUX_MANIFEST,
            lambda d: d.__setitem__("product_version", "9.9.9")), "C1"),
        ("C1 schema 写死版本常量", lambda r: (r / SCHEMA_DIR / "astrocs-product.schema.json")
            .write_text(read_text(r, SCHEMA_DIR + "/astrocs-product.schema.json")
                        .replace('"type": "string"', '"const": "0.11.0-alpha.1"', 1),
                        encoding="utf-8"), "C1"),
        ("C2 合同漏列（删一个 schema 单元）", lambda r: _mutate_json(
            r / CONTRACT, lambda d: d.__setitem__(
                "units", [u for u in d["units"]
                          if u["install_path"] != "schemas/preset-contract.json"])), "C2"),
        ("C2 合同多列（登记不存在的文件）", lambda r: _mutate_json(
            r / CONTRACT, lambda d: d["units"].append(
                {"unit_id": "SCHEMA-GHOST", "kind": "schema",
                 "install_path": "schemas/ghost.schema.json", "required": False})), "C2"),
        ("C3 平台状态漂移", _inject_status_flip, "C3"),
        ("C4 vendored_path 失效", lambda r: _mutate_json(
            r / LOCK, lambda d: d["production_dependencies"][0].__setitem__(
                "vendored_path", "lib/does_not_exist/third_party/cfitsio")), "C4"),
        ("C4 聚合哈希不符", lambda r: _mutate_json(
            r / LOCK, lambda d: d["production_dependencies"][0].__setitem__(
                "aggregate_sha256", "0" * 64)), "C4"),
        ("C5 CMake 外部库漏登记", lambda r: _mutate_json(
            r / LOCK, lambda d: d.__setitem__(
                "system_dependencies",
                [x for x in d["system_dependencies"]
                 if "gsl" not in x["name"].lower()])), "C5"),
        ("C6 目录通配安装回归", _inject_wildcard, "C6"),
        ("C7 许可文本缺失", lambda r: (r / LICENSE_DIR / "nlohmann_json.MIT.txt").unlink(),
         "C7"),
    )
    with tempfile.TemporaryDirectory(prefix="astrocs-pkg-selftest-") as tmp:
        base = Path(tmp)
        for case, mutate, expect in cases:
            work = base / re.sub(r"[^A-Za-z0-9]+", "_", case)
            work.mkdir()
            notes = []
            build_sandbox(repo, work)
            try:
                mutate(work)
                got = evaluate(work, allow_missing_git=True, notes=notes)
            except InputUnavailable as e:
                print(f"SELFTEST FAIL {case}: 注入后输入不可用 {e}", file=sys.stderr)
                ok = False
                continue
            codes = {c for c, _ in got}
            if expect is None:
                good = not got
                detail = "应全绿" if good else f"误报 {sorted(codes)}"
                if notes:
                    detail += f" [跳过: {'; '.join(notes)}]"
            else:
                good = expect in codes
                detail = f"应命中 {expect}, 实得 {sorted(codes)}"
            print(("SELFTEST PASS " if good else "SELFTEST FAIL ") + f"{case}: {detail}")
            ok = ok and good
    print("SELFTEST " + ("PASS (全部注入均判红, 正例判绿)" if ok else "FAIL"))
    return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser(description="AstroCS 打包面一致性检查器 (W5-PKG-001)")
    ap.add_argument("--root", default="", help="仓库根 (默认由脚本位置推导)")
    ap.add_argument("--json-out", default="", help="可选 JSON 报告输出路径")
    ap.add_argument("--self-test", action="store_true",
                    help="负例注入自测（机器可执行负例面）")
    args = ap.parse_args()

    repo = Path(args.root).resolve() if args.root else Path(__file__).resolve().parent.parent
    if not (repo / VERSION_FILE).is_file():
        print(f"ANCHOR_STALE: {VERSION_FILE} 不存在于 {repo}", file=sys.stderr)
        return 2
    if args.self_test:
        return self_test(repo)
    try:
        findings = evaluate(repo)
    except InputUnavailable as e:
        print(f"PKG_CONSISTENCY FAIL (fail-closed): {e}", file=sys.stderr)
        return 2
    report = {
        "checker": "PKG_CONSISTENCY",
        "root": str(repo),
        "finding_count": len(findings),
        "findings": [{"code": c, "detail": d} for c, d in findings],
        "verdict": "PASS" if not findings else "FAIL",
    }
    if args.json_out:
        out = Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                       encoding="utf-8")
    if findings:
        for code, detail in findings:
            print(f"{code} {detail}", file=sys.stderr)
        print(f"PKG_CONSISTENCY FAIL: {len(findings)} 处不一致")
        return 1
    print("PKG_CONSISTENCY PASS: 版本单源/安装树闭包/双平台同构/依赖锁↔实树↔CMake/"
          "安装规则↔合同/许可登记 全部一致")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
