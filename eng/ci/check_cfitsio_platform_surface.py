#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_cfitsio_platform_surface.py — 第三方源"包含面完整性"静态核查器。

为什么需要它 (Windows 全量构建实测的两类缺陷):
  Linux 侧无法复现平台面 —— /usr/include 里恒有 pthread.h 与 zlib.h, 缺包含面
  也照样编过。要在 Linux 侧做成**可判定**的核查, 判据就不能依赖"能不能编过",
  只能依赖"目标声明面上到底接得到哪些包含目录"。本核查器即如此:
    ① 解析仓内 CMake 声明面 (变量展开 / include() / add_subdirectory() / foreach
        / 变量作用域还原), 求出每个目标的源集、包含面、编译定义;
    ② 对源集里的每个 TU 递归解析 #include (含 C 语义的同目录相对解析);
    ③ 判定合同头能否在"该 TU 可见的包含面"内解析 —— **不使用系统默认搜索路径**。

判据 (与 eng/cmake/cfitsio_platform.cmake 抬头的 I1/I2 同源):
  I1  目标源集含 vendored cfitsio 源清单中的 TU ⇒ 若其编译定义含 _REENTRANT,
      fitsio2.h 会打开 `#include <pthread.h>`; 该头在非 Windows 由 OS 提供,
      在 Windows/MSVC 下必须由**平台垫片**提供 ⇒ 目标必须经
      astrocs_cfitsio_apply_platform_shim() 拿到垫片包含面 (或等价显式包含面)。
  I2  目标源集含 `#include "zlib.h"` 的 TU ⇒ 必须经
      astrocs_cfitsio_apply_third_party_deps() 拿到 zlib 包含面
      (合同两条注入布局: <root>/include 与 <root>/lib/include)。
  I3  编入 vendored 第三方源的目标必须声明第三方告警隔离
      (astrocs_cfitsio_isolate_warnings() 或属性 ASTROCS_WARNINGS_OFF=1),
      否则同一批第三方 TU 在基准目标里安静、在重复点里把 C4206/C4267
      之类灌进项目告警面。
  I4  GCC 专有旗标 (`-fopenmp`) 必须落在按平台的门控块内
      (if(UNIX/WIN32/MSVC/APPLE/CMAKE_SYSTEM_NAME) ...); 收口处是
      eng/cmake/cfitsio_platform.cmake 的 astrocs_openmp_link_if_unix()。

退出码: 0=全绿; 1=判红(有目标接不到包含面); 2=输入不可用(清单/源目录缺失, 不静默放过)。

用法:
  python3 eng/ci/check_cfitsio_platform_surface.py             # 扫全仓
  python3 eng/ci/check_cfitsio_platform_surface.py --json      # 机器可读
  python3 eng/ci/check_cfitsio_platform_surface.py --self-test # 正例必绿 + 4 组负例必红

上游: docs/ASTROCS_DESIGN.md §8.4; ENGINEERING_SPEC.md §1/§8;
      docs/engineering/01_CHECKS.md §1; eng/cmake/cfitsio_platform.cmake 抬头。
"""
from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

SOURCES_MODULE_REL = "eng/cmake/cfitsio_sources.cmake"
PLATFORM_MODULE_REL = "eng/cmake/cfitsio_platform.cmake"
SHIM_DIR_REL = "eng/cmake/win32_pthread_shim"

SHIM_FUNC = "astrocs_cfitsio_apply_platform_shim"
DEPS_FUNC = "astrocs_cfitsio_apply_third_party_deps"
WARN_FUNC = "astrocs_cfitsio_isolate_warnings"
# 该函数名仅作**文档锚**保留（判据的豁免按**路径**判定，见 FLAG_HOME_REL；
# 下方常量此前零读，易被误以为改它能改豁免范围 —— 实际不会）。
OPENMP_FUNC = "astrocs_openmp_link_if_unix"
# 允许出现裸 `-fopenmp` 字面量而**不**要求同文件内平台门控的收口文件:
#   平台模块把所有"门控 + 旗标"收成 `astrocs_openmp_link_if_unix()` 一处。
# 其余文件里出现 `-fopenmp` 时, 判据是**该行必须落在按平台的门控块内**
# (同一文件、未被闭合的 if(UNIX ...) / if(WIN32 ...) / if(NOT MSVC ...) 祖先块)。
# 这就是本批"漏网旗标"的复发形态的反面: 实测形态 = 只判 OpenMP_CXX_FOUND 就
# `target_compile_options(... -fopenmp)`, 没有平台门控, MSVC 收到 GCC 旗标。
# 判据刻意只锁定这一个旗标 —— ISA 旗标 (-mavx2/-msse2) 的合法落点本就多于一处
# (AVX2/AVX512 两条 OBJECT 库 + 探测目标), 硬塞进来只会制造假红、逼人放宽判据。
FLAG_HOME_REL = PLATFORM_MODULE_REL
GCC_ONLY_FLAGS = ("-fopenmp",)
# 算作"平台门控"的块头判据 (出现在 if(...) 头里的子串)。
PLATFORM_GATES = ("UNIX", "WIN32", "MSVC", "APPLE", "CMAKE_SYSTEM_NAME")
# 向上回溯的行数上限 (防御性边界; 门控按 if/endif 配平判定, 不看邻近性)。
FLAG_GATE_LOOKBACK = 200

# ── 合同声明面 (改这里 = 改判据) ───────────────────────────────────────────
ZLIB_LAYOUTS = ("include", "lib/include")
PLATFORM_HEADERS = {"pthread.h": ("fitsio2.h", "_REENTRANT")}
DEP_HEADERS = ("zlib.h",)

VAR_RE = re.compile(r"\$\{([A-Za-z0-9_]+)\}")
INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^>"]+)[>"]', re.M)
BLOCK_START = {"if", "foreach", "while", "function", "macro"}
BLOCK_END = {"endif", "endforeach", "endwhile", "endfunction", "endmacro"}
STICKY = {"ASTROCS_CFITSIO_SOURCES", "CMAKE_SOURCE_DIR", "PROJECT_SOURCE_DIR",
          "ASTROCS_WIN32_PTHREAD_SHIM_DIR"}


class InputError(Exception):
    """输入不可用 —— 判 2, 不静默放过。"""


def _normpath(p) -> Path:
    """折叠 . / .. 段 (不要求路径存在), 等价 CMake get_filename_component(ABSOLUTE)。"""
    return Path(os.path.normpath(str(p)))


def is_file(p: Path) -> bool:
    """安全 is_file: 候选路径可能是超长拼接串 (未加引号的 LIST 展开), 没有 stat 资格。"""
    try:
        return Path(p).is_file()
    except OSError:
        return False


def is_dir(p: Path) -> bool:
    """安全 is_dir (同上)。"""
    try:
        return Path(p).is_dir()
    except OSError:
        return False


def split_cmake_args(text: str) -> list[str]:
    """按 CMake 规则切参数: 双引号成组 (去引号), 其余按空白。"""
    out, cur, in_q = [], [], False
    for ch in text:
        if ch == '"':
            in_q = not in_q
        elif ch.isspace() and not in_q:
            if cur:
                out.append("".join(cur))
                cur = []
        else:
            cur.append(ch)
    if cur:
        out.append("".join(cur))
    return out


def split_statements(text: str):
    """按 CMake 语义切语句: 只以括号深度==0 的闭括号结尾; 纯注释行丢弃。"""
    buf: list[str] = []
    depth = 0
    for raw in text.splitlines():
        line = raw.strip()
        if not buf and (not line or line.startswith("#")):
            continue
        if not buf and "#" in line:
            line = line.split("#", 1)[0].strip()
            if not line:
                continue
        buf.append(line)
        depth += line.count("(") - line.count(")")
        if depth <= 0 and buf:
            stmt = " ".join(buf)
            buf, depth = [], 0
            yield stmt
    if buf:
        yield " ".join(buf)


def as_call(stmt: str):
    m = re.match(r"^([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*$", stmt, re.S)
    return (m.group(1).lower(), m.group(2)) if m else (None, "")


class _Cursor:
    """语句游标: 支持"取到匹配的 endXXX 之前的子序列", 供 foreach 体复用。"""

    def __init__(self, statements: list[str]):
        self.items = statements
        self.i = 0

    def take_block(self) -> list[str]:
        """取出直到匹配 end 之前的语句 (处理嵌套)。"""
        body: list[str] = []
        nest = 0
        while self.i < len(self.items):
            stmt = self.items[self.i]
            self.i += 1
            name, _ = as_call(stmt)
            if name in BLOCK_START:
                nest += 1
                body.append(stmt)
            elif name in BLOCK_END:
                if nest == 0:
                    return body
                nest -= 1
                body.append(stmt)
            else:
                body.append(stmt)
        return body


class Model:
    """仓内 CMake 声明面的静态模型 (不跑工具链)。"""

    def __init__(self, root: Path):
        self.root = root
        self.var: dict[str, str | None] = {"CMAKE_SOURCE_DIR": str(root).replace("\\", "/")}
        self.include_dirs: dict[str, list[tuple[str, str]]] = {}
        self.defines: dict[str, list[str]] = {}
        self.explicit_sources: dict[str, list[str]] = {}
        self.func_calls: dict[str, set[str]] = {}
        # 目标属性 (set_target_properties ...) —— 第三方告警隔离就落在属性上。
        self.props: dict[str, dict[str, str]] = {}
        self.notes: list[str] = []
        self._read: set[Path] = set()
        self._read_cmake(root / "CMakeLists.txt", root)

    # ─────────────────────────────────────────────────────── 文件读取 ──
    def _read_cmake(self, path: Path, cur_dir: Path) -> None:
        if not is_file(path):
            return
        try:
            real = path.resolve()
        except OSError:
            return
        if real in self._read:
            return
        self._read.add(real)
        saved = {k: v for k, v in self.var.items() if k not in STICKY}
        sticky = {k: self.var[k] for k in STICKY if k in self.var}
        self.var["CMAKE_CURRENT_SOURCE_DIR"] = str(cur_dir).replace("\\", "/")
        try:
            stmts = list(split_statements(
                path.read_text(encoding="utf-8", errors="replace")))
            self._run(_Cursor(stmts), _Cursor(stmts).items, path.parent, cur_dir)
        finally:
            # 跨文件累积量: 除 STICKY 名单外, 任何"值里含 vendored 第三方源路径"的
            # 变量也不得被作用域还原抹掉 —— 重编译点就是这样把 60 个 TU 攒在
            # 局部变量里 (如 phase2 的 _P2_CFITSIO_ABS), 还原掉会让逐 TU 判定全部
            # 落空 = 假绿。判据是"值里出现该第三方源目录名", 与拼写方式无关。
            keep = {k: v for k, v in self.var.items()
                    if k in STICKY or (isinstance(v, str) and "cfitsio/" in v)}
            self.var.clear()
            self.var.update(saved)
            self.var.update(sticky)
            self.var.update(keep)

    def _run(self, cur: "_Cursor", _items, file_dir: Path, cur_dir: Path) -> None:
        while cur.i < len(cur.items):
            stmt = cur.items[cur.i]
            cur.i += 1
            name, body = as_call(stmt)
            if name is None:
                continue
            if name in BLOCK_START:
                block = cur.take_block()
                if name == "foreach":
                    self._foreach(body, block, file_dir, cur_dir)
                elif name == "if":
                    self._run(_Cursor(block), block, file_dir, cur_dir)
                # while/function/macro 体不在此展开 (定义时执行/不精确判定)
                continue
            if name in BLOCK_END or name in ("else", "elseif"):
                continue
            self._emit(name, body, file_dir, cur_dir)

    def _foreach(self, body: str, block: list[str], file_dir: Path, cur_dir: Path) -> None:
        parts = split_cmake_args(body)
        if not parts:
            return
        var_name = parts[0]
        items = [self._v(p) for p in parts[1:]]
        if var_name.upper() == "RANGE":
            try:
                lo, hi = int(self._v(parts[1])), int(self._v(parts[2]))
                items = [str(i) for i in range(lo, hi + 1)]
            except (IndexError, ValueError):
                return
        elif var_name.upper() == "IN" and len(parts) >= 3 and parts[2].upper() == "LISTS":
            var_name = parts[1]
            items = [self._v(p) for p in parts[3:]]
        elif var_name.upper() == "IN" and len(parts) >= 3 and parts[2].upper() == "ITEMS":
            var_name = parts[1]
            items = [self._v(p) for p in parts[3:]]
        flat: list[str] = []
        for it in items:
            flat.extend(it.split("\n"))
        old = self.var.get(var_name)
        for value in flat:
            self.var[var_name] = value
            self._run(_Cursor(block), block, file_dir, cur_dir)
        if old is None:
            self.var.pop(var_name, None)
        else:
            self.var[var_name] = old

    # ─────────────────────────────────────────────────── 变量/表达式 ──
    @staticmethod
    def _elements(text: str) -> list[str]:
        """把"未加引号的 CMake 列表展开结果"元素化 (换行分隔约定)。

        难点: CMake 用空白分隔列表元素, 而元素本身可以含空格 (本工作区路径
        "Astro CS Database" 就是), 展开后两种空白不可区分。本核查器取确定性约定:
        列表值一律以 ␠换行␠ 作为**元素边界**存储 (见 _emit 的 set/list 分支),
        元素内部才允许空格。任何解析步骤都不许把元素丢掉 —— 丢元素 = 少判 = 假绿。
        """
        return [e for e in (text or "").split("\n") if e.strip()]

    def _v(self, text: str) -> str:
        return self._expand(text, 0)

    def _expand(self, text: str, depth: int) -> str:
        if depth > 8 or "$" not in text:
            return text

        def sub(m):
            name = m.group(1)
            if name == "CMAKE_CURRENT_SOURCE_DIR":
                return self.var.get(name) or ""
            val = self.var.get(name)
            return self._expand(val or "", depth + 1)

        return VAR_RE.sub(sub, text)

    # ──────────────────────────────────────────────────────── 分派 ──
    def _emit(self, name: str, body: str, file_dir: Path, cur_dir: Path) -> None:
        v = self._v
        if name == "set":
            parts = split_cmake_args(body)
            if len(parts) >= 2:
                # 元素用 换行 分隔存储 (单个元素内部才允许空格, 见 _elements)。
                vals: list[str] = []
                for p_ in parts[1:]:
                    vals.extend(self._elements(v(p_)))
                self.var[parts[0]] = "\n".join(vals)
        elif name == "unset":
            parts = split_cmake_args(body)
            if parts:
                self.var.pop(parts[0], None)
        elif name == "get_filename_component":
            parts = split_cmake_args(body)
            if len(parts) >= 3:
                val, mode = v(parts[1]), parts[2].upper()
                if mode == "NAME":
                    self.var[parts[0]] = Path(val.rstrip("/")).name
                elif mode in ("ABSOLUTE", "REALPATH", "DIRECTORY"):
                    cand = Path(val)
                    if not cand.is_absolute():
                        cand = cur_dir / cand
                    self.var[parts[0]] = str(_normpath(cand)).replace("\\", "/")
                else:
                    self.var[parts[0]] = val
        elif name == "list":
            parts = split_cmake_args(body)
            if len(parts) >= 3 and parts[0].upper() == "APPEND":
                cur = self.var.get(parts[1]) or ""
                add: list[str] = []
                for p_ in parts[2:]:
                    add.extend(self._elements(v(p_)))
                self.var[parts[1]] = "\n".join([*self._elements(cur), *add]).strip()
        elif name == "string":
            parts = split_cmake_args(body)
            if len(parts) >= 4 and parts[0].upper() in ("APPEND", "CONCAT"):
                self.var[parts[1]] = "".join(v(p) for p in parts[3:])
        elif name == "include":
            args = split_cmake_args(body)
            skip_next = False
            for arg in args:
                if skip_next:
                    skip_next = False
                    continue
                if arg.upper() in ("OPTIONAL", "NO_POLICY_SCOPE"):
                    continue
                if arg.upper() == "RESULT_VARIABLE":
                    skip_next = True
                    continue
                target = v(arg)
                cand = Path(target)
                if not cand.is_absolute():
                    cand = cur_dir / target
                self._read_cmake(cand, cand.parent)
        elif name == "add_subdirectory":
            parts = split_cmake_args(body)
            if parts:
                sub = cur_dir / v(parts[0])
                keep = self.var.get("CMAKE_CURRENT_SOURCE_DIR")
                self._read_cmake(sub / "CMakeLists.txt", sub)
                self.var["CMAKE_CURRENT_SOURCE_DIR"] = keep
        elif name == "target_include_directories":
            self._put_scope(body, self.include_dirs, v)
        elif name == "target_compile_definitions":
            self._put_defs(body, v)
        elif name == "target_sources":
            self._put_sources(body, v)
        elif name in ("add_library", "add_executable"):
            self._put_add_target(body, v)
        elif name == "set_target_properties":
            # set_target_properties(<tgt> PROPERTIES <k> <v> [<k> <v> ...])
            args = split_cmake_args(body)
            if "PROPERTIES" in args:
                i = args.index("PROPERTIES")
                tgt = v(args[0])
                kv = [v(a) for a in args[i + 1:]]
                for j in range(0, len(kv) - 1, 2):
                    self.props.setdefault(tgt, {})[kv[j]] = kv[j + 1]
        elif name in (SHIM_FUNC, DEPS_FUNC, WARN_FUNC):
            for arg in split_cmake_args(body):
                if arg.startswith("$"):
                    continue
                self.func_calls.setdefault(v(arg), set()).add(name)

    @staticmethod
    def _tgt_args(body: str, v):
        parts = [v(p) for p in split_cmake_args(body)]
        return (parts[0], parts[1:]) if parts else ("", [])

    def _put_scope(self, body, sink, v) -> None:
        tgt, args = self._tgt_args(body, v)
        if not tgt:
            return
        scope = "PRIVATE"
        for a in args:
            if a.upper() in ("PRIVATE", "PUBLIC", "INTERFACE"):
                scope = a.upper()
                continue
            if a.upper() == "SYSTEM" or a.startswith("$<"):
                continue
            sink.setdefault(tgt, []).append((scope, a.replace("\\", "/")))

    def _put_defs(self, body, v) -> None:
        tgt, args = self._tgt_args(body, v)
        for a in args:
            if a.upper() in ("PRIVATE", "PUBLIC", "INTERFACE"):
                continue
            if a.startswith("-D"):
                a = a[2:]
            self.defines.setdefault(tgt, []).append(a)

    def _put_sources(self, body, v) -> None:
        tgt, args = self._tgt_args(body, v)
        for a in args:
            if a.upper() in ("PRIVATE", "PUBLIC", "INTERFACE") or a.startswith("$<"):
                continue
            self.explicit_sources.setdefault(tgt, []).append(a.replace("\\", "/"))

    def _put_add_target(self, body, v) -> None:
        tgt, args = self._tgt_args(body, v)
        skip = {"STATIC", "SHARED", "MODULE", "OBJECT", "INTERFACE", "EXCLUDE_FROM_ALL",
                "WIN32", "MACOSX_BUNDLE", "ALIAS", "UNKNOWN"}
        for a in args:
            if a.upper() in skip or a.startswith("$<"):
                continue
            self.explicit_sources.setdefault(tgt, []).append(a.replace("\\", "/"))

    # ──────────────────────────────────────────────────────── 查询 ──
    def _candidate_paths(self, raw: str):
        """候选路径: 未加引号的 ${LIST} 在 CMake 里按空白展开成多个路径, 而路径本身
        也可能含空格 (本工作区就是 "Astro CS Database")。两种形态都要认 —— 先整体试,
        再按空白切段逐个试, 否则含空格的工作区会把一整串当成一个不存在的文件 (假绿)。"""
        yield raw
        if "\n" in raw:
            for piece in raw.split("\n"):
                yield piece
        elif " " in raw:
            for piece in raw.split():
                yield piece

    def sources(self, tgt: str) -> list[Path]:
        out: list[Path] = []
        for raw in self.explicit_sources.get(tgt, []):
            for piece in self._candidate_paths(raw):
                cand = Path(piece)
                if not cand.is_absolute():
                    cand = self.root / piece
                cand = _normpath(cand)
                if is_file(cand) and cand not in out:
                    out.append(cand)
        return out

    def resolved_include_dirs(self, tgt: str) -> list[Path]:
        out: list[Path] = []
        for _scope, d in self.include_dirs.get(tgt, []):
            for piece in self._candidate_paths(d):
                cand = Path(piece)
                if not cand.is_absolute():
                    cand = self.root / piece
                cand = _normpath(cand)
                if is_dir(cand) and cand not in out:
                    out.append(cand)
        return out

    def has_func(self, tgt: str, func: str) -> bool:
        return func in self.func_calls.get(tgt, set())

    def warnings_off(self, tgt: str) -> bool:
        """目标是否声明了第三方告警隔离属性 (等价于调用点手写 set_target_properties)。"""
        return self.props.get(tgt, {}).get("ASTROCS_WARNINGS_OFF") == "1"


# ═════════════════════════════════════════════════ #include 递归解析 ══
def header_chain(src: Path, resolved: dict, seen: set) -> list[str]:
    """返回该 TU 递归解析到的合同头名 (只认 resolved 里登记的落点)。"""
    if src in seen or not is_file(src):
        return []
    seen.add(src)
    found: list[str] = []
    try:
        text = src.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []
    for delim, name in INCLUDE_RE.findall(text):
        base = Path(name).name
        if base in resolved:
            found.append(base)
            found.extend(header_chain(resolved[base], resolved, seen))
        elif delim == '"':
            cand = src.parent / name
            if is_file(cand):
                found.extend(header_chain(cand, resolved, seen))
    return found


# ═══════════════════════════════════════════════════════════════ 判据 ══
def _gcc_flag_offenders(root: Path) -> list[dict]:
    """I4: GCC 专有旗标只许出现在 FLAG_HOME_REL 这一个文件里。

    动机 (Windows 基线实测的漏网旗标): 同一件"按平台门控旗标"的规矩, 五处写了
    if(UNIX AND OpenMP_CXX_FOUND), 第六处只判 OpenMP_CXX_FOUND 就把 `-fopenmp`
    交给 cl.exe。凡"同一判断在多处各写一遍"就会漏 —— 所以判据封在"旗标字面量
    只许有一处落点"上, 落点本身即门控处。新增旗标必须在落点内写 (或者把落点
    改成更通用的机制), 不许在调用点再抄一份。
    """
    offenders: list[dict] = []
    home = (_normpath(root / FLAG_HOME_REL))
    for path in sorted(root.rglob("*")):
        if path.suffix not in (".txt", ".cmake"):
            continue
        if path.name != "CMakeLists.txt" and path.suffix != ".cmake":
            continue
        parts = path.relative_to(root).parts
        if parts[0] in {"build", "run", ".git"}:
            continue
        if not path.name == "CMakeLists.txt" and path.suffix != ".cmake":
            continue
        if _normpath(path) == home:
            continue
        try:
            raw = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = raw.splitlines()
        for n, line in enumerate(lines, 1):
            if line.lstrip().startswith("#"):
                continue
            hits = [f for f in GCC_ONLY_FLAGS if f in line]
            if not hits:
                continue
            if _gated_above(lines, n):
                continue
            for flag in hits:
                offenders.append({
                    "file": str(path.relative_to(root)).replace("\\", "/"),
                    "line": n, "flag": flag,
                    "reason": "未落在按平台的门控块内 (向上找不到未闭合的 "
                              "if(UNIX/WIN32/MSVC/APPLE/CMAKE_SYSTEM_NAME) 块头, "
                              "只找到 if(OpenMP_CXX_FOUND) 之类非平台条件)",
                    "text": line.strip()[:120]})
    return offenders


def _gated_above(lines: list[str], lineno: int) -> bool:
    """该行(1-based)是否处于按平台的门控块内 —— 沿闭括号配平向上找未闭合的 if()。

    只看"最近一个未闭合、且尚未被 endif 关掉"的 if 头: 若它就是平台门控, 判过;
    若它是别的 if (例如只有 OpenMP_CXX_FOUND), 继续向上找它的父块。
    """
    depth = 0
    for i in range(lineno - 2, max(-1, lineno - 2 - FLAG_GATE_LOOKBACK), -1):
        if i < 0:
            break
        code = lines[i].split("#", 1)[0].strip()
        if not code:
            continue
        if re.match(r"^endif\b", code):
            depth += 1
            continue
        m = re.match(r"^(if|while|foreach|function|macro)\b", code)
        if not m:
            continue
        if depth > 0:
            depth -= 1
            continue
        if m.group(1) != "if":
            return False
        return any(g in code for g in PLATFORM_GATES)
    return False


def check(root: Path) -> tuple[int, dict]:
    if not is_file(root / SOURCES_MODULE_REL):
        raise InputError(f"第三方源清单模块缺失: {SOURCES_MODULE_REL}")
    if not is_dir(root / "lib/infrastructure/aio/third_party/cfitsio"):
        raise InputError("vendored 第三方源目录缺失: lib/infrastructure/aio/third_party/cfitsio")

    model = Model(root)
    listed_raw = (model.var.get("ASTROCS_CFITSIO_SOURCES") or "").split()
    if not listed_raw:
        raise InputError("ASTROCS_CFITSIO_SOURCES 展开为空 —— 零命中不是空清单的理由")
    base_names = {Path(s).name for s in listed_raw}

    sites: list[dict] = []
    for tgt in sorted(model.explicit_sources):
        # 判据: ① 该 TU 在册的第三方源清单里 (按文件名, 因为重编译点会把它们拼成
        # 绝对路径或带前缀的相对路径); ② 它确实来自 vendored 目录 —— 路径前缀或
        # 祖先目录名命中都算, 免得把同名文件 (若有) 误判进来。
        cdir = (root / "lib/infrastructure/aio/third_party/cfitsio").resolve()
        tu = [s for s in model.sources(tgt)
              if s.name in base_names
              and (str(s).replace("\\", "/").endswith("/third_party/cfitsio/" + s.name)
                   or cdir in s.parents)]
        if not tu:
            continue
        defs = {d.split("=")[0].strip() for d in model.defines.get(tgt, [])}
        reentrant = "_REENTRANT" in defs
        dirs = model.resolved_include_dirs(tgt)
        resolved = {name: next((d / name for d in dirs if is_file(d / name)), None)
                    for name in list(PLATFORM_HEADERS) + list(DEP_HEADERS)}
        live = {k: v for k, v in resolved.items() if v is not None}

        zlib_consumers: list[str] = []
        pthread_consumers: list[str] = []
        for s in tu:
            try:
                raw = s.read_text(encoding="utf-8", errors="replace")
            except OSError:
                continue
            if re.search(r'#\s*include\s*"zlib\.h"', raw):
                zlib_consumers.append(s.name)
            chain = header_chain(s, live, set())
            if "pthread.h" in chain and reentrant:
                pthread_consumers.append(s.name)

        # I1: _REENTRANT 打开 fitsio2.h 的 pthread 分支 ⇒ 必须有垫片包含面
        needs_pthread = bool(pthread_consumers) or (
            reentrant and any("fitsio2.h" in s.read_text(encoding="utf-8", errors="replace")
                              for s in tu))
        pthread_ok = (not needs_pthread) or model.has_func(tgt, SHIM_FUNC) or \
            any(is_file(d / "pthread.h") for d in dirs)
        # I2: 有 zlib.h 消费点 ⇒ 必须有依赖包含面
        zlib_ok = (not zlib_consumers) or model.has_func(tgt, DEPS_FUNC) or \
            any(is_file(d / "zlib.h") for d in dirs)

        # I3: 第三方源的**告警隔离**必须与基准目标同口径 (属性 ASTROCS_WARNINGS_OFF
        #     退出项目 /W4 + 平台对应的告警级旗标)。少了它, 同一批第三方 TU 在
        #     基准目标里安静、在重复点里把 C4206/C4267 灌进项目告警面。
        warn_ok = model.has_func(tgt, WARN_FUNC) or model.warnings_off(tgt)

        sites.append({
            "target": tgt,
            "third_party_tu": len(tu),
            "reentrant": reentrant,
            "shim_call": model.has_func(tgt, SHIM_FUNC),
            "deps_call": model.has_func(tgt, DEPS_FUNC),
            "warn_call": model.has_func(tgt, WARN_FUNC),
            "warn_isolated": warn_ok,
            "pthread_consumers": sorted(set(pthread_consumers))[:3],
            "pthread_reachable": pthread_ok,
            "zlib_consumers": sorted(set(zlib_consumers)),
            "zlib_reachable": zlib_ok,
            "include_surface": [str(d) for d in dirs],
        })

    # I4: GCC 专有旗标只许在 FLAG_HOME_REL 一处出现 (平台门控与旗标同一处)。
    flag_offenders = _gcc_flag_offenders(root)

    red = [s["target"] for s in sites
           if (not s["pthread_reachable"]) or (not s["zlib_reachable"])
           or (not s["warn_isolated"])]
    payload = {
        "sites": sites,
        "listed_sources": len(listed_raw),
        "flag_home": FLAG_HOME_REL,
        "flag_offenders": flag_offenders,
        "platform_module_present": is_file(root / PLATFORM_MODULE_REL),
        "shim_dir_present": is_dir(root / SHIM_DIR_REL),
        "zlib_layouts_declared": list(ZLIB_LAYOUTS),
        "red_targets": red,
    }
    return (1 if red or flag_offenders else 0), payload


def _render(payload: dict) -> str:
    yn = lambda b: "是" if b else "否"
    lines = ["第三方源包含面核查 (I1 pthread 垫片 / I2 zlib 依赖包含面 / "
             "I3 第三方告警隔离 / I4 旗标的平台门控)",
             "  在册第三方源 %d 个 TU | 平台模块 %s | 垫片目录 %s | 重编译点 %d 个" % (
                 payload["listed_sources"],
                 "在" if payload["platform_module_present"] else "缺",
                 "在" if payload["shim_dir_present"] else "缺",
                 len(payload["sites"]))]
    for s in payload["sites"]:
        lines.append("  · %s: 第三方 TU %d | _REENTRANT=%s | pthread 可达=%s | "
                     "zlib 消费点=%s | zlib 可达=%s | 告警隔离=%s"
                     % (s["target"], s["third_party_tu"], yn(s["reentrant"]),
                        yn(s["pthread_reachable"]),
                        ",".join(s["zlib_consumers"]) or "无",
                        yn(s["zlib_reachable"]),
                        yn(s["warn_isolated"])))
    if payload["red_targets"]:
        lines.append("VERDICT=RED 判据不满足的目标: " + ", ".join(payload["red_targets"]))
    for off in payload["flag_offenders"]:
        lines.append("  ! 旗标落点违规: %s:%d 出现 %s | %s"
                     % (off["file"], off["line"], off["flag"], off["text"]))
    if payload["flag_offenders"]:
        lines.append("VERDICT=RED 存在未按平台门控的 GCC 专有旗标 (落点参 %s)"
                     % payload["flag_home"])
    if not payload["red_targets"] and not payload["flag_offenders"]:
        lines.append("VERDICT=GREEN 所有重编译点都接得到第三方源所需的平台/依赖包含面, "
                     "且 GCC 专有旗标均在平台门控内")
    return "\n".join(lines)


# ══════════════════════════════════════════════════════════ self-test ══
def _fixture(root: Path) -> None:
    """最小复刻: 第三方源清单 + 平台模块 + 一个 _REENTRANT 重编译点。"""
    (root / "eng/cmake/win32_pthread_shim").mkdir(parents=True)
    (root / "eng/cmake/win32_pthread_shim/pthread.h").write_text(
        "#pragma once\n", encoding="utf-8")
    third = root / "lib/infrastructure/aio/third_party/cfitsio"
    third.mkdir(parents=True)
    (third / "fitsio2.h").write_text(
        '#ifndef F2\n#define F2\n#ifdef _REENTRANT\n#include <pthread.h>\n#endif\n#endif\n',
        encoding="utf-8")
    (third / "zcompress.c").write_text(
        '#include "zlib.h"\nint z(void){return 0;}\n', encoding="utf-8")
    (third / "fitscore.c").write_text(
        '#include "fitsio2.h"\nint f(void){return 0;}\n', encoding="utf-8")
    (root / SOURCES_MODULE_REL).parent.mkdir(parents=True, exist_ok=True)
    (root / SOURCES_MODULE_REL).write_text(
        "set(ASTROCS_CFITSIO_SOURCES\n"
        "  lib/infrastructure/aio/third_party/cfitsio/fitscore.c\n"
        "  lib/infrastructure/aio/third_party/cfitsio/zcompress.c)\n",
        encoding="utf-8")
    (root / PLATFORM_MODULE_REL).write_text(
        "function(astrocs_cfitsio_apply_platform_shim tgt)\n"
        "endfunction()\n"
        "function(astrocs_cfitsio_apply_third_party_deps tgt)\n"
        "endfunction()\n", encoding="utf-8")


def _write_cmaked(root: Path, with_shim: bool, with_deps: bool,
                  with_warn: bool = True, raw_fopenmp: bool = False,
                  openmp_gated: bool = False) -> None:
    """写对方的 CMakeLists。raw_fopenmp/openmp_gated 用来注入旗标面负例。"""
    body = ["cmake_minimum_required(VERSION 3.16)",
            "project(fixture C)",
            "include(${CMAKE_SOURCE_DIR}/eng/cmake/cfitsio_sources.cmake)",
            "include(${CMAKE_SOURCE_DIR}/eng/cmake/cfitsio_platform.cmake)",
            "add_library(tgt STATIC ${ASTROCS_CFITSIO_SOURCES})",
            "target_compile_definitions(tgt PRIVATE _REENTRANT)"]
    if with_shim:
        body.append("astrocs_cfitsio_apply_platform_shim(tgt)")
    if with_deps:
        body.append("astrocs_cfitsio_apply_third_party_deps(tgt)")
    if with_warn:
        body.append("astrocs_cfitsio_isolate_warnings(tgt)")
    if raw_fopenmp and openmp_gated:
        # 落在门控块内的裸旗标 —— 形态合法 (判据 I4 不该误伤)
        body += ["if(UNIX AND OpenMP_CXX_FOUND)",
                 "  target_compile_options(tgt PRIVATE -fopenmp)",
                 "endif()"]
    elif raw_fopenmp:
        # 只有 OpenMP_CXX_FOUND 的裸旗标 —— 本批"漏网旗标"的确切形态, 必红
        body += ["if(OpenMP_CXX_FOUND)",
                 "  target_compile_options(tgt PRIVATE -fopenmp)",
                 "endif()"]
    (root / "CMakeLists.txt").write_text("\n".join(body) + "\n", encoding="utf-8")


def self_test() -> int:
    tmp = Path(tempfile.mkdtemp(prefix="acs_cfitsio_surface_"))
    bad: list[str] = []
    try:
        ok = tmp / "positive"
        _fixture(ok)
        _write_cmaked(ok, True, True)
        rc, payload = check(ok)
        if rc != 0 or len(payload["sites"]) != 1:
            bad.append("正例(两函数齐)未判绿或命中数不为 1: rc=%s sites=%d"
                       % (rc, len(payload["sites"])))

        neg1 = tmp / "neg1-no-shim"
        _fixture(neg1)
        _write_cmaked(neg1, with_shim=False, with_deps=True)
        rc, payload = check(neg1)
        if rc != 1 or "tgt" not in payload["red_targets"]:
            bad.append("负例1(缺 pthread 垫片)未判红: rc=%s red=%s"
                       % (rc, payload["red_targets"]))

        neg2 = tmp / "neg2-no-deps"
        _fixture(neg2)
        _write_cmaked(neg2, with_shim=True, with_deps=False)
        rc, payload = check(neg2)
        if rc != 1 or "tgt" not in payload["red_targets"]:
            bad.append("负例2(缺 zlib 依赖包含面)未判红: rc=%s red=%s"
                       % (rc, payload["red_targets"]))

        neg3 = tmp / "neg3-no-module"
        _fixture(neg3)
        (neg3 / SOURCES_MODULE_REL).unlink()
        _write_cmaked(neg3, True, True)
        try:
            check(neg3)
            bad.append("负例3(源清单模块缺失)未判输入不可用")
        except InputError:
            pass

        # 负例 5: 缺第三方告警隔离 (I3) ⇒ 同一批第三方源会在项目告警面里吵
        neg5 = tmp / "neg5-no-warn-isolation"
        _fixture(neg5)
        _write_cmaked(neg5, True, True, with_warn=False)
        rc, payload = check(neg5)
        if rc != 1 or "tgt" not in payload["red_targets"]:
            bad.append("负例5(缺第三方告警隔离)未判红: rc=%s red=%s"
                       % (rc, payload["red_targets"]))

        # 负例 6: 裸 -fopenmp 无平台门控 ⇒ 本批漏网旗标的复发形态, 必红
        neg6 = tmp / "neg6-ungated-fopenmp"
        _fixture(neg6)
        _write_cmaked(neg6, True, True, raw_fopenmp=True, openmp_gated=False)
        rc, payload = check(neg6)
        if rc != 1 or not payload["flag_offenders"]:
            bad.append("负例6(裸 -fopenmp 未平台门控)未判红: rc=%s offenders=%d"
                       % (rc, len(payload["flag_offenders"])))

        # 正例 2: 同样写裸 -fopenmp, 但落在 if(UNIX ...) 门控内 ⇒ 必须判绿
        pos2 = tmp / "positive2-gated-fopenmp"
        _fixture(pos2)
        _write_cmaked(pos2, True, True, raw_fopenmp=True, openmp_gated=True)
        rc, payload = check(pos2)
        if rc != 0 or payload["flag_offenders"]:
            bad.append("正例2(门控内的 -fopenmp)被误判红: rc=%s offenders=%d"
                       % (rc, len(payload["flag_offenders"])))

        # 负例 4: 真实仓故障注入 —— 抽掉 astrocs_cfitsio 的垫片注入调用 ⇒ 必红
        inj = tmp / "neg4-repo-injection"
        # 只搬"声明面"所需的少量文件 —— 本仓 testdata/gaia/artifacts 合计 >200GB,
        # 整仓 copytree 会拖垮磁盘与内存看门狗 (实测超时)。
        shutil.copytree(REPO, inj, ignore=shutil.ignore_patterns(
            ".git", "build", "run", "gaia", "testdata", "artifacts", "实验", "独立审计",
            "工程控制", "output", "logs", "*.a", "*.so", "*.o", "*.obj", "*.pdb"))
        cm = inj / "CMakeLists.txt"
        txt = cm.read_text(encoding="utf-8")
        marker = "  astrocs_cfitsio_apply_platform_shim(astrocs_cfitsio)"
        if marker not in txt:
            bad.append("负例4 前置失败: 真实仓 CMakeLists 里找不到垫片注入调用")
        else:
            cm.write_text(txt.replace(marker, "  # [SELF-TEST INJECTION] removed"),
                          encoding="utf-8")
            rc, payload = check(inj)
            if rc != 1 or "astrocs_cfitsio" not in payload["red_targets"]:
                bad.append("负例4(真实仓抽掉垫片)未判红: rc=%s red=%s"
                           % (rc, payload["red_targets"]))
        shutil.rmtree(inj, ignore_errors=True)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if bad:
        print("SELF-TEST FAIL:", file=sys.stderr)
        for b in bad:
            print("  - " + b, file=sys.stderr)
        return 1
    print("SELF-TEST OK: 正例 1/2 必绿 + 负例 1(缺垫片)/2(缺依赖包含面)/3(输入不可用)/"
          "4(真实仓抽垫片)/5(缺第三方告警隔离)/6(裸 -fopenmp 未门控) 必红"
          " —— 判据能绿能红且不误伤")
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="第三方源包含面完整性静态核查")
    ap.add_argument("--root", default=str(REPO))
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test()
    try:
        rc, payload = check(Path(args.root).resolve())
    except InputError as exc:
        print("CFITSIO_SURFACE_INPUT_UNAVAILABLE: %s" % exc, file=sys.stderr)
        return 2
    print(json.dumps(payload, ensure_ascii=False, indent=2) if args.json
          else _render(payload))
    return rc


if __name__ == "__main__":
    sys.exit(main())
