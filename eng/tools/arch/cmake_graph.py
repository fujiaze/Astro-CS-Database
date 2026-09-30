#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""真实构建图读取器（根 CMake 图 / 生产闭包 / 安装面）—— 唯一实现。

从哪里读构建图、怎么读、怎么算生产闭包与安装面，收在这里供生成器与测试复用。
本模块只依赖标准库。"""

from __future__ import annotations

import hashlib
import json
import os
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

# 生产入口登记表（唯一源：eng/tools/arch/spec_named_impls.json 的 production_entry）。
def read_text(path, label=None) -> str:
    """读文本文件；失败时把路径带上，便于判据点名。"""
    try:
        with open(path, "r", encoding="utf-8") as handle:
            return handle.read()
    except OSError as exc:
        raise OSError("READ_FAILED: %s (%s)" % (label or path, exc)) from exc


def read_json(path, label=None):
    """读 JSON 文件；失败时把路径带上，便于判据点名。"""
    return json.loads(read_text(path, label))


ENTRY_REGISTRY = "eng/tools/arch/spec_named_impls.json"
DEFAULT_ENTRY_TARGET = "acsd"
# 安装规则唯一源（BLD-003：子目录 CMakeLists 禁止 install，根 CMake include 本文件）。
INSTALL_RULES = "eng/cmake/install_layout.cmake"

SOURCE_SUFFIXES = (".cpp", ".cc", ".cxx", ".c", ".h", ".hpp", ".hxx")
CMAKE_KEYWORDS = ("STATIC", "SHARED", "MODULE", "INTERFACE", "OBJECT", "ALIAS", "IMPORTED",
                  "WIN32", "MACOSX_BUNDLE", "EXCLUDE_FROM_ALL")
LINK_KEYWORDS = ("PUBLIC", "PRIVATE", "INTERFACE", "DEBUG", "OPTIMIZED", "GENERAL",
                 "LINK_PUBLIC", "LINK_PRIVATE", "LINK_INTERFACE_LIBRARIES")
_CMAKE_VAR_RE = re.compile(r"\$\{([A-Za-z0-9_]+)\}")
_VARIABLE_NAME_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_.\-]*$")
_TARGET_NAME_RE = _VARIABLE_NAME_RE


# --------------------------------------------------------------------- cmake graph ----
def _strip_cmake_comments(text: str) -> str:
    out = []
    for line in text.splitlines():
        cut = line.find("#")
        out.append(line[:cut] if cut >= 0 else line)
    return "\n".join(out)


def _commands(text: str, name: str):
    """产出 name(...) 的实参文本（括号配平；含跨行）。"""
    for m in re.finditer(r"(?<![A-Za-z0-9_])" + re.escape(name) + r"\s*\(", text):
        start = text.find("(", m.start())
        depth = 0
        for i in range(start, len(text)):
            if text[i] == "(":
                depth += 1
            elif text[i] == ")":
                depth -= 1
                if depth == 0:
                    yield text[start + 1:i]
                    break


def _tokens(body: str):
    return [t for t in re.split(r"[\s\n\r\t]+", body.strip()) if t]


def _expand(token: str, variables: dict, depth: int = 0):
    """展开 ${VAR}；未知变量返回 None（不猜）。"""
    if depth > 8:
        return None
    out = token
    for _ in range(8):
        m = _CMAKE_VAR_RE.search(out)
        if not m:
            return out
        key = m.group(1)
        value = variables.get(key)
        if value is None:
            return None
        out = out[:m.start()] + value + out[m.end():]
    return None


def _iter_reachable_cmake(repo: pathlib.Path):
    """自根 CMakeLists.txt 沿未注释 add_subdirectory 递归（只认根构建图）。"""
    root = repo / "CMakeLists.txt"
    if not root.is_file():
        raise gc.GateError("ANCHOR_MISSING: CMakeLists.txt（根构建图唯一事实源）")
    seen, queue = set(), [root]
    while queue:
        path = queue.pop(0)
        rel = path.relative_to(repo).as_posix()
        if rel in seen:
            continue
        seen.add(rel)
        yield path
        text = _strip_cmake_comments(read_text(path, rel))
        for body in _commands(text, "add_subdirectory"):
            toks = _tokens(body)
            if not toks:
                continue
            sub = toks[0].strip(chr(34))
            if "$" in sub:
                continue
            candidate = (path.parent / sub).resolve()
            try:
                candidate.relative_to(repo.resolve())
            except ValueError:
                continue
            cm = candidate / "CMakeLists.txt"
            if cm.is_file():
                queue.append(cm)


def _tokens_quoted(body: str):
    """引号感知分词：引号内空白不断词（CMake 语义）；引号被剥掉，不进 token 文本。"""
    import shlex
    lex = shlex.shlex(body, posix=True)
    lex.whitespace_split = True
    lex.commenters = ""
    return list(lex)


def _include_path(repo: pathlib.Path, cmake: pathlib.Path, body: str, variables: dict):
    """include(<路径>) 的仓库内绝对路径；不可解析/不在仓库内/非 .cmake ⇒ None（不猜）。

    只认字面路径或内建路径变量前缀（本解析器里它就是发起 include 的那个目录）。
    """
    toks = _tokens_quoted(body)
    if not toks:
        return None
    raw = toks[0].strip(chr(34))
    if ";" in raw or not raw.endswith(".cmake"):
        return None
    resolved = _expand(raw, variables)
    if resolved is None:
        return None
    candidate = pathlib.Path(resolved)
    if not candidate.is_absolute():
        candidate = cmake.parent / candidate
    candidate = pathlib.Path(os.path.realpath(candidate))
    try:
        candidate.relative_to(repo)
    except ValueError:
        return None
    return candidate if candidate.is_file() else None


def _collect_variables(repo: pathlib.Path, cmake: pathlib.Path, variables: dict,
                       unresolved: list, seen=None, depth: int = 0):
    """把本文件与递归 include 的仓库内 .cmake 模块里的 set() 并入 variables（就地更新）。

    递归深度有界（≤8）且按调用栈防环；带变量引用的 include 若解析不到仓库内文件，
    或深浅超限，记入 unresolved（禁止把「读不出来」当成「没有」）；无变量引用的
    include(Foo) 形态是 CMake 模块查找，本解析器无法解析，不记账也不猜。
    """
    seen = set() if seen is None else seen
    key = os.path.realpath(cmake)
    if key in seen:
        return
    seen.add(key)
    try:
        rel = cmake.relative_to(repo).as_posix()
    except ValueError:
        rel = cmake.as_posix()
    text = _strip_cmake_comments(read_text(cmake, rel))
    for body in _commands(text, "include"):
        inc = _include_path(repo, cmake, body, variables) if depth < 8 else None
        if inc is None:
            toks = _tokens_quoted(body)
            if toks and "$" in toks[0]:
                unresolved.append({"file": rel,
                                   "token": toks[0], "kind": "include"})
            continue
        _collect_variables(repo, inc, variables, unresolved, seen, depth + 1)
    for body in _commands(text, "set"):
        toks = _tokens_quoted(body)
        if (len(toks) < 2 or _CMAKE_VAR_RE.search(toks[0])
                or not _VARIABLE_NAME_RE.match(toks[0])):
            continue
        values = [_expand(t, variables) for t in toks[1:]]
        if any(v is None for v in values):
            continue  # 列表值里有解析不出的元素 ⇒ 整个 set 不猜（不部分写入，避免半真值）
        variables[toks[0]] = ";".join(values)  # CMake 列表语义：存储用分号连接


def _source_tokens(repo: pathlib.Path, cmake: pathlib.Path, tok: str, variables: dict):
    """把 add_library/add_executable 的一个实参展开为 0..n 个仓库内 source 相对路径。

    展开结果按 CMake 列表语义用分号拆分（set(VAR a b c) ⇒ 使用点变 3 个 source）；
    未知变量展开不出，或不在仓库内、后缀非源码 ⇒ 返回 []（不猜，由调用方计数）。
    """
    resolved = _expand(tok.strip(chr(34)), variables)
    if resolved is None:
        return []
    out = []
    for piece in str(resolved).split(";"):
        piece = piece.strip()
        if not piece:
            continue
        candidate = pathlib.Path(piece)
        if not candidate.is_absolute():
            candidate = cmake.parent / candidate
        try:
            relsrc = pathlib.Path(os.path.realpath(candidate)).relative_to(repo).as_posix()
        except ValueError:
            continue
        if relsrc.endswith(SOURCE_SUFFIXES):
            out.append(relsrc)
    return out


def parse_cmake_graph(repo: pathlib.Path):
    """返回 {targets, edges, unresolved}；targets[name] = {sources, file, kind}。

    变量表 = 内建路径变量 + 本文件与被 include 的仓库内 .cmake 模块里的 set()（递归有界）；
    set() 按 CMake 列表语义存为分号连接，使用点再拆成多个 source。解析不出的 source 实参
    与 include 进入 unresolved（调用方可据此 fail-closed），绝不猜。
    """
    repo = pathlib.Path(os.path.realpath(repo))
    targets: dict = {}
    edges: dict = {}
    unresolved: list = []
    for cmake in _iter_reachable_cmake(repo):
        rel = cmake.relative_to(repo).as_posix()
        text = _strip_cmake_comments(read_text(cmake, rel))
        variables = {
            "CMAKE_CURRENT_SOURCE_DIR": cmake.parent.as_posix(),
            "CMAKE_CURRENT_LIST_DIR": cmake.parent.as_posix(),
            "CMAKE_SOURCE_DIR": repo.as_posix(),
            "PROJECT_SOURCE_DIR": repo.as_posix(),
        }
        _collect_variables(repo, cmake, variables, unresolved)
        for kind in ("add_library", "add_executable"):
            for body in _commands(text, kind):
                toks = _tokens(body)
                if not toks:
                    continue
                name = toks[0].strip(chr(34))
                if "$" in name or not _TARGET_NAME_RE.match(name):
                    continue
                info = targets.setdefault(name, {"sources": set(), "file": rel, "kind": kind})
                for tok in toks[1:]:
                    if tok.upper() in CMAKE_KEYWORDS:
                        continue
                    found = _source_tokens(repo, cmake, tok, variables)
                    if not found:
                        if "$" in tok:
                            unresolved.append({"file": rel, "target": name,
                                               "token": tok, "kind": kind})
                        continue
                    info["sources"].update(found)
        for body in _commands(text, "target_link_libraries"):
            toks = _tokens(body)
            if not toks:
                continue
            name = toks[0].strip(chr(34))
            if "$" in name:
                continue
            deps = edges.setdefault(name, set())
            for tok in toks[1:]:
                tok = tok.strip(chr(34))
                if tok.upper() in LINK_KEYWORDS:
                    continue
                if "$" in tok or "::" in tok:
                    continue
                deps.add(tok)
    return {"targets": targets, "edges": edges,
            "unresolved": unresolved}
def production_closure(graph: dict, entry: str):
    targets = graph["targets"]
    if entry not in targets:
        raise gc.GateError("ANCHOR_STALE: 生产入口 target %r 不在根构建图" % entry)
    seen, stack = set(), [entry]
    while stack:
        node = stack.pop()
        if node in seen:
            continue
        seen.add(node)
        for dep in graph["edges"].get(node, ()):  # 未声明的名字 = 外部库，忽略
            if dep in targets and dep not in seen:
                stack.append(dep)
    if not seen:
        raise gc.GateError("ANCHOR_STALE: 生产闭包为空（禁止空转判绿）")
    return seen


# ------------------------------------------------------------------ 派生读取面 ----
def production_entry(repo: pathlib.Path, default: str = DEFAULT_ENTRY_TARGET) -> str:
    """生产入口 target 名，取自登记表（唯一源）。登记表缺失/为空 ⇒ GateError。"""
    repo = pathlib.Path(repo)
    doc = read_json(repo / ENTRY_REGISTRY, ENTRY_REGISTRY)
    if not isinstance(doc, dict):
        raise gc.GateError("ENTRY_REGISTRY_INVALID: %s 不是对象" % ENTRY_REGISTRY)
    entry = doc.get("production_entry") or default
    if not isinstance(entry, str) or not entry.strip():
        raise gc.GateError("ENTRY_REGISTRY_EMPTY: %s 的 production_entry 为空" % ENTRY_REGISTRY)
    return entry


def executable_targets(graph: dict) -> dict:
    """构建图里的可执行目标 {name: info}（kind == add_executable）。"""
    return {n: i for n, i in graph["targets"].items() if i.get("kind") == "add_executable"}


def library_targets(graph: dict) -> dict:
    """构建图里的库目标 {name: info}（kind == add_library）。"""
    return {n: i for n, i in graph["targets"].items() if i.get("kind") == "add_library"}


def source_digest(sources) -> str:
    """源集摘要：排序后逐行连接的 sha256 前 12 位十六进制。"""
    body = chr(10).join(sorted(sources))
    return hashlib.sha256(body.encode("utf-8")).hexdigest()[:12]


def source_fingerprint(sources) -> str:
    """源集指纹（写进文档表的形态）：source_digest 的 4-4-4 分组。

    分组只为与「git sha 字面量」的形态区分（CHK-DOC-HYGIENE D3d：7-64 位连续十六进制
    且含 a-f 视为过程痕迹）；摘要强度不变（仍是 sha256 前 12 位）。
    """
    d = source_digest(sources)
    return "%s-%s-%s" % (d[0:4], d[4:8], d[8:12])


def target_anchor(repo: pathlib.Path, name: str, info: dict) -> str:
    """返回 target 定义处的可复核锚 "<CMakeLists 相对路径>:<行号>"。

    登记面与文档表都要带「文件:行」判词（一页纸 S1 第 1 条）；解析器只记文件，
    行号在此按定义行文本定位（前缀匹配 kind(name，注释行不匹配）。找不到行号时
    退化为 "<路径>:?"（宁可显式缺行号，也不编造）。
    """
    repo = pathlib.Path(repo)
    rel = info.get("file")
    if not rel:
        return "?"
    text = read_text(repo / rel, rel)
    head = info.get("kind", "") + "("
    for lineno, line in enumerate(text.splitlines(), 1):
        stripped = line.lstrip()
        if not stripped.startswith(head):
            continue
        rest = stripped[len(head):].lstrip()
        if rest.startswith(name) and (len(rest) == len(name) or rest[len(name)] in " \t)"):
            return "%s:%d" % (rel, lineno)
    return "%s:?" % rel


def install_targets(repo: pathlib.Path):
    """安装面被安装的 target 名（含 foreach 列表变量展开）。

    返回 (targets:set, unresolved:list)。unresolved 非空表示安装规则里有解析不出来的
    变量目标 ⇒ 调用方必须 fail-closed（不得当成「没安装」而判绿）。
    解析复用本模块的 CMake 命令扫描器（与 eng/packaging/check_packaging_consistency.py
    C6 同一语义：只认字面名与 foreach 列表名）。
    """
    repo = pathlib.Path(repo)
    text = read_text(repo / INSTALL_RULES, INSTALL_RULES)
    code = chr(10).join(ln for ln in text.splitlines() if not ln.lstrip().startswith("#"))
    loop_vars = {}
    for body in _commands(code, "foreach"):
        toks = _tokens(body)
        if len(toks) >= 2:
            loop_vars[toks[0]] = [t for t in toks[1:] if t and not t.startswith("$")]
    stop = ("LIBRARY", "RUNTIME", "DESTINATION", "COMPONENT", "ARCHIVE", "OPTIONAL", "EXPORT")
    targets, unresolved = set(), []
    for body in _commands(code, "install"):
        toks = _tokens(body)
        if not toks or toks[0] != "TARGETS":
            continue
        for tok in toks[1:]:
            if tok.upper() in stop:
                break
            if tok.startswith("${") and tok.endswith("}"):
                names = loop_vars.get(tok[2:-1])
                if names is None:
                    unresolved.append(tok)
                else:
                    targets.update(names)
            elif tok.startswith("$"):
                unresolved.append(tok)
            else:
                targets.add(tok)
    targets = {t for t in targets if t and not t.startswith("$") and "/" not in t}
    if not targets:
        raise gc.GateError("ANCHOR_EMPTY: %s 未解析出任何 install(TARGETS)（禁止空转判绿）"
                           % INSTALL_RULES)
    return targets, sorted(set(unresolved))
