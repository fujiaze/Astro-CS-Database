#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/check_platform_syslib_links.py — 「系统库链接面」平台判断唯一性核查器。

为什么需要它 (Windows 全量构建实测):
  正式平台 = Windows x64 (MSVC) / Linux amd64。libm / libpthread 只在类 UNIX 存在
  (Windows 的数学函数在 CRT 内、线程原语在 kernel32 内), zlib 在 Windows 侧只能来自
  ACS_ZLIB_LIBRARY。此前**同一判断**散落在 20 余处链接面, 形态有 if(UNIX) /
  if(NOT WIN32) / if(MSVC)…else() 三种口径, 另有一批**根本没写**平台判断 ⇒ Windows
  链接期 LNK1104 "cannot open file 'm.lib'"。撤除这些分支之后, 必须有**可执行判据**
  保证它们不会被重新写出来 —— 否则本次收敛只是把 20 处换成下一次的 20 处。
  Linux 侧无法用"能不能链接过"当判据 (系统 libm/libpthread/libz 恒在, 缺守卫也照样过),
  故本核查器只判**声明面**。

判据 (全部静态、可判定):
  R1  三个共用件 astrocs_platform_{math,pthread,zlib} 必须**恰好声明一次**, 且声明
      位置在根 CMakeLists.txt (唯一判定点)。
  R2  任何链接语句 (target_link_libraries / TARGET_LINK_LIBRARIES / list(APPEND
      *_LINK*) / set(*_LINK*) / find_library(*, m|z|pthread)) 内不得出现裸系统库名
      m / pthread / z —— 必须引用共用件。判定按**括号配平**覆盖整条语句: CMake 的链接
      语句可跨行, 命令名只在首行, 裸名常落在续行; 逐行匹配会漏判真实 Windows 站点
      (实测: lib/algorithms/psf/tests/p1psf/CMakeLists.txt:192 的裸 m 落在续行, 而
      p1psf_centroid_gate_test 正是冷构建 LNK1104 m.lib 的 11 个目标之一)。
  R3  任何平台条件块 (if(UNIX|WIN32|MSVC|MINGW|APPLE|NOT WIN32 …) … endif()) 内不得
      出现裸系统库名 (堵住"换个 if 再写一遍"的第二形态)。
  唯一允许出现裸名的地方 = 根 CMakeLists.txt 里由标记行围出的**共用件区块**
  (标记: '平台系统库唯一判定点' … 'END 平台系统库唯一判定点')。

用法:
  python3 eng/ci/check_platform_syslib_links.py              # 扫全仓, 0=全绿 1=判红
  python3 eng/ci/check_platform_syslib_links.py --self-test  # 4 正例必绿 + 7 负例必红
  python3 eng/ci/check_platform_syslib_links.py --json       # 机器可读
  python3 eng/ci/check_platform_syslib_links.py --root <dir> # 指定仓库根 (自检用)

上游: ENGINEERING_SPEC.md §1 (正式平台); 根 CMakeLists.txt 共用件区块注释;
      docs/engineering/01_CHECKS.md。先例: eng/ci/check_cfitsio_platform_surface.py。
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

SHARED = {"astrocs_platform_math": "m",
          "astrocs_platform_pthread": "pthread",
          "astrocs_platform_zlib": "z"}
BARE = {"m", "pthread", "z"}
START_MARK = "平台系统库唯一判定点"
END_MARK = "END 平台系统库唯一判定点"
# ── 构建输出识别: 按**内容**, 不按目录名 ─────────────────────────────────────
# 为什么必须内容判 (FINAL-07 实测的真实假红, 不是假想):
#   本仓的构建目录名**不固定**。根入口约定是 `build`, 但任务模板与多份隔离树脚本
#   默认 `-B b`, 另有人用 `out` / `cmake-build-debug` / `_build`。按目录名白名单
#   ⇒ 只要不叫 `build` 就**整体漏过**, 于是门去扫 CMake **自己生成**的探针文件。
#   实测 (`cmake -S . -B out` 后跑本核查器, 改前判红):
#     R2 out/CMakeFiles/3.31.6/CMakeCXXCompiler.cmake:91 链接语句含裸系统库名 'm'
#        set(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES "stdc++;m;gcc_s;gcc;c;gcc_s;gcc")
#   ⇒ **门在真实仓库上恒红** (rc=1), 且红的是生成物不是声明面。
# 内容判据 = CMake 构建树的定义性产物: 构建树根必有 CMakeCache.txt (cmake 的
#   构建缓存文件), 它只由 CMake 写入、源码树里不会有 ⇒ 按它识别与目录名无关。
#   这**只扩大对生成物的跳过**, 不触碰任何仓库声明面 (见下方 cmake_files 与
#   证据 run/FINAL-07/syslib-guard/evidence/)。
CMAKE_BUILD_CACHE = "CMakeCache.txt"
# 仍按名跳过的只有**非源码基础设施目录** (不是"猜测这是构建输出"):
#   .git         版本库元数据
#   node_modules 依赖安装树
#   run          过程产物区 (gitignore; ENGINEERING_SPEC.md §7; 非仓库声明面)
# 三者都不承载本仓声明面; 判据"只加严不放宽"针对的是**声明面**, 而把 gitignore
# 的过程产物区拉回扫描面只会在别的轮次留下陈旧 CMakeLists.txt 时造假红。
SKIP_DIRS = {".git", "node_modules", "run"}
# 排除的路径前缀（**前缀**匹配，不是全仓 third_party 通配）。
# 实测事实（2026-09-29 核对，注释按事实写，不按意图写）: 本仓真正的 vendored 第三方树
#   在 lib/infrastructure/aio/third_party/cfitsio/** 与
#   lib/infrastructure/pipeline/orchestrator/cpp/third_party/**，
#   **不匹配**这里的前缀 ⇒ 它们**仍在判据扫描面内**。
# 这不是遗漏而是承重: 本批正是靠它判出了 vendored cfitsio 自身的
#   FIND_LIBRARY(M_LIB m) / IF(MSVC OR MINGW) 第二处平台判断（HEAD 上 R2 判红，
#   已由本批改为共用件）。把排除放宽成 "**/third_party" 会把这条覆盖一起下放，
#   违反"判据只加严不下放"，故保持前缀口径不变。
SKIP_PATH_PARTS = {("lib", "third_party")}

# 词表只有 BARE 一处：此前这里又把 "(m|pthread|z)" 手抄了一遍，
# 改 BARE 词表零效果（死配置）。改为从 BARE 生成。
# ⚠ 交替必须是**捕获组**：负例路径用 `mm.group(1)` 取库名写进判词，
# 改成非捕获组会让该路径在自测面上抛 IndexError。
# 顺序无关：两侧都是完整词的边界断言。
BARE_RE = re.compile(r"(?<![A-Za-z0-9_./$-])("
                     + "|".join(sorted(BARE, key=len, reverse=True))
                     + r")(?![A-Za-z0-9_])")
LINK_STMT_RE = re.compile(r"(?:target_link_libraries|TARGET_LINK_LIBRARIES)\s*\(|"
                          r"list\s*\(\s*APPEND\s+\w*LINK\w*|"
                          r"set\s*\(\s*\w*LINK\w*|"
                          r"find_library\s*\([^)]*", re.I)
# find_library 的 NAMES 候选名列表 (如 NAMES z libz zlib zlibstatic zs) 是**库文件发现**,
# 不是链接决定: 链接决定仍必须走共用件。候选列表里出现 z/libz 属正常发现面 ⇒ 豁免。
NAMES_RE = re.compile(r"\bNAMES\b", re.I)
PLATFORM_IF_RE = re.compile(r"^\s*(if|elseif)\s*\(.*(UNIX|WIN32|MSVC|MINGW|APPLE).*\)\s*$", re.I)
IF_RE = re.compile(r"^\s*if\s*\(", re.I)
ENDIF_RE = re.compile(r"^\s*endif\s*\(", re.I)
# 括号配平用的上界 (防御性: 病态输入不得让扫描不终止)
MAX_LINK_SPAN_LINES = 200
_STR_SPAN_RE = re.compile(r'"[^"]*"')


def strip_comment(line: str) -> str:
    out = []
    in_str = False
    for i, ch in enumerate(line):
        if ch == '"':
            in_str = not in_str
        if ch == "#" and not in_str and (i == 0 or line[i - 1] in " \t"):
            break
        out.append(ch)
    return "".join(out)


def _paren_depth(line: str) -> int:
    """该行引入的净括号深度 (双引号串内的括号不计 —— 路径/正则字面量里常有括号)。"""
    return _STR_SPAN_RE.sub("", strip_comment(line)).count("(") - \
        _STR_SPAN_RE.sub("", strip_comment(line)).count(")")


def link_stmt_spans(lines) -> list:
    """按括号配平求出每条链接语句覆盖的行区间 (1-based, 闭区间)。

    为什么必须按**语句**而不是按行判 (2026-09-29 修复的真实假绿):
      CMake 的链接语句可跨行, 续行里没有命令名, 逐行匹配 LINK_STMT_RE 于是把
      续行漏在判据之外。实测漏网站点 = Windows 冷构建 LNK1104
      "cannot open file 'm.lib'" 的 11 个目标之一的声明面:
        lib/algorithms/psf/tests/p1psf/CMakeLists.txt:191-192
          target_link_libraries(p1psf_centroid_gate_test PRIVATE
            m ${ASTROCS_OMP_TARGET})            <- 裸 m 落在续行
        eng/tests/unit/p2_samp/CMakeLists.txt:37-43 同形态 (pthread m z 落在续行)
      修复前判据对这两处判绿 (findings=0), 而 Windows 冷构建对
      p1psf_centroid_gate_test 实报 LNK1104 ⇒ 那是恒绿面, 不是判据力。
    返回 [(start, end, exempt_by_names)]; exempt_by_names = 该语句含 find_library
    的 NAMES 候选面 (库文件**发现**面, 豁免; 但只豁免**本语句**, 不豁免同文件别处)。
    """
    spans = []
    i = 0
    n = len(lines)
    while i < n:
        code = strip_comment(lines[i])
        if not LINK_STMT_RE.search(code):
            i += 1
            continue
        depth = _paren_depth(code)
        j = i
        while depth > 0 and j + 1 < n and (j + 1 - i) < MAX_LINK_SPAN_LINES:
            j += 1
            depth += _paren_depth(lines[j])
        if depth > 0:
            # 病态输入 (括号不配平): 只判开头那一行, 不吞掉整个文件。
            j = i
        body = "\n".join(strip_comment(lines[k]) for k in range(i, j + 1))
        spans.append((i + 1, j + 1, bool(NAMES_RE.search(body))))
        i = j + 1
    return spans


def cmake_files(root: Path):
    """产出仓库内全部 .cmake / CMakeLists.txt **声明面**文件 (已排序)。

    跳过两类, 两类都不是仓库声明面:
      1) 非源码基础设施目录 (SKIP_DIRS, 按名: .git / node_modules / run);
      2) CMake 构建树 —— 按**内容**判: 树根含 CMakeCache.txt 则整棵子树剪掉
         (含 FetchContent 的 _deps/、编译器探针 CMakeCXXCompiler.cmake 等)。
    第 2 类就地剪枝, 顺带避免把构建树上万个文件逐个 stat —— 这也正是改前
    `rglob("*")` 在 `build/` 上要遍历全部生成物的原因。
    """
    root = Path(root)
    for dirpath, dirnames, filenames in os.walk(root):
        d = Path(dirpath)
        # (1) 非源码基础设施目录
        dirnames[:] = [n for n in sorted(dirnames) if n not in SKIP_DIRS]
        # (2) CMake 构建树: 按内容 (与目录名无关)
        if CMAKE_BUILD_CACHE in filenames:
            dirnames[:] = []
            continue
        for fn in sorted(filenames):
            p = d / fn
            if fn != "CMakeLists.txt" and p.suffix.lower() != ".cmake":
                continue
            rel = p.relative_to(root)
            if any(rel.parts[: len(sp)] == sp for sp in SKIP_PATH_PARTS):
                continue
            yield p


def shared_region(lines):
    """根 CMakeLists 中共用件区块的行号集合 (1-based)。"""
    region, inside = set(), False
    for i, l in enumerate(lines, 1):
        if START_MARK in l:
            inside = True
        if inside:
            region.add(i)
        if END_MARK in l:
            inside = False
    return region


def check(root: Path):
    findings = []
    root_cml = root / "CMakeLists.txt"
    if not root_cml.is_file():
        return [{"rule": "R0", "where": "CMakeLists.txt", "line": 0,
                 "msg": "仓库根 CMakeLists.txt 不存在"}]
    root_lines = root_cml.read_text(encoding="utf-8", errors="replace").split("\n")
    region = shared_region(root_lines)
    if not region:
        findings.append({"rule": "R1", "where": "CMakeLists.txt", "line": 0,
                         "msg": "未找到共用件区块标记 " + START_MARK})
    # R1: 共用件声明唯一且在根
    for name in SHARED:
        hits = []
        for p in cmake_files(root):
            for i, l in enumerate(p.read_text(encoding="utf-8", errors="replace").split("\n"), 1):
                if re.search(r"add_library\s*\(\s*" + re.escape(name) + r"\b", l, re.I):
                    hits.append((p.relative_to(root).as_posix(), i))
        if len(hits) != 1:
            findings.append({"rule": "R1", "where": "repo", "line": 0,
                             "msg": f"{name}: add_library 声明 {len(hits)} 次 (要求恰好 1 次) -> {hits}"})
        elif hits[0][0] != "CMakeLists.txt":
            findings.append({"rule": "R1", "where": hits[0][0], "line": hits[0][1],
                             "msg": f"{name} 不在根 CMakeLists.txt 声明 (唯一判定点)"})
    # R2/R3
    for p in cmake_files(root):
        rel = p.relative_to(root).as_posix()
        lines = p.read_text(encoding="utf-8", errors="replace").split("\n")
        allowed = region if rel == "CMakeLists.txt" else set()
        # 链接语句覆盖行 (按括号配平, 含**跨行**续行 —— 见 link_stmt_spans 抬头)
        covered, names_exempt = set(), set()
        for a, b, by_names in link_stmt_spans(lines):
            for k in range(a, b + 1):
                covered.add(k)
                if by_names:
                    names_exempt.add(k)
        depth_platform = 0
        for i, raw in enumerate(lines, 1):
            code = strip_comment(raw)
            if PLATFORM_IF_RE.match(code):
                depth_platform += 1
            if i in allowed:
                if ENDIF_RE.match(code):
                    depth_platform = max(0, depth_platform - 1)
                continue
            in_link = (LINK_STMT_RE.search(code) is not None) or (i in covered)
            if in_link and i not in names_exempt:
                for mm in BARE_RE.finditer(code):
                    findings.append({"rule": "R2", "where": rel, "line": i,
                                     "msg": f"链接语句含裸系统库名 '{mm.group(1)}' (应引用共用件 "
                                            + "/".join(SHARED) + ")", "text": code.strip()[:160]})
            elif depth_platform > 0:
                for mm in BARE_RE.finditer(code):
                    findings.append({"rule": "R3", "where": rel, "line": i,
                                     "msg": f"平台条件块内含裸系统库名 '{mm.group(1)}'", "text": code.strip()[:160]})
            if IF_RE.match(code):
                pass
            elif ENDIF_RE.match(code):
                depth_platform = max(0, depth_platform - 1)
    return findings

# ── 自检: 4 正例必绿 + 7 负例必红 ────────────────────────────────────────────
FAKE_ROOT = """# fake root
project(fake CXX)
# ── 平台系统库唯一判定点 (test) ──
add_library(astrocs_platform_math INTERFACE)
add_library(astrocs_platform_pthread INTERFACE)
add_library(astrocs_platform_zlib INTERFACE)
if(WIN32)
  # windows: empty payload
else()
  target_link_libraries(astrocs_platform_math INTERFACE m)
  target_link_libraries(astrocs_platform_pthread INTERFACE pthread)
  target_link_libraries(astrocs_platform_zlib INTERFACE z)
endif()
# ── END 平台系统库唯一判定点 ──
add_subdirectory(sub)
"""
FAKE_SUB = {
  "green": "add_executable(t t.cpp)\ntarget_link_libraries(t PRIVATE astrocs_platform_math astrocs_platform_zlib)\n",
  "bare": "add_executable(t t.cpp)\ntarget_link_libraries(t PRIVATE astrocs_platform_math m)\n",
  "guarded": "add_executable(t t.cpp)\nif(UNIX)\n  target_link_libraries(t PRIVATE m)\nendif()\n",
  "listappend": "set(X_LINK_LIBS Threads::Threads)\nif(UNIX)\n  list(APPEND X_LINK_LIBS z)\nendif()\ntarget_link_libraries(t PRIVATE X_LINK_LIBS)\n",
  # 跨行形态: 裸名落在**续行** (命令名在上一行) —— 2026-09-29 实测的真实假绿形态,
  # 逐行匹配的旧判据对它 findings=0。必须能判红, 否则该恒绿面无法被排除。
  "multiline": "add_executable(t t.cpp)\ntarget_link_libraries(t PRIVATE\n  m ${ASTROCS_OMP_TARGET})\n",
  # 跨行 + 共用件 => 同一形态必须判绿 (证明不是"见多行就红")
  "multiline_green": "add_executable(t t.cpp)\ntarget_link_libraries(t PRIVATE\n  astrocs_platform_math ${ASTROCS_OMP_TARGET})\n",
  # find_library 的 NAMES 候选面跨行 => 仍是**发现面**, 豁免 (防误伤)
  "names_multiline": "find_library(Z NAMES\n  z libz zlib\n  PATHS /usr/lib)\n",
}


def _mk(tmp: Path, root_text: str, sub_text: str):
    (tmp / "sub").mkdir(parents=True, exist_ok=True)
    (tmp / "CMakeLists.txt").write_text(root_text, encoding="utf-8")
    (tmp / "sub" / "CMakeLists.txt").write_text(sub_text, encoding="utf-8")


def self_test(repo_root: Path, verbose: bool) -> int:
    cases = []
    with tempfile.TemporaryDirectory() as td:
        base = Path(td)
        # P1: 合法形态 (引用共用件) 必绿
        d = base / "p1"; _mk(d, FAKE_ROOT, FAKE_SUB["green"])
        cases.append(("P1 引用共用件", d, 0))
        # N1: 裸 m 必红 (R2)
        d = base / "n1"; _mk(d, FAKE_ROOT, FAKE_SUB["bare"])
        cases.append(("N1 裸 m", d, 1))
        # N2: 换 if(UNIX) 再写一遍必红 (R2)
        d = base / "n2"; _mk(d, FAKE_ROOT, FAKE_SUB["guarded"])
        cases.append(("N2 if(UNIX)+裸 m", d, 1))
        # N3: list(APPEND ...LINK... z) 必红 (R2)
        d = base / "n3"; _mk(d, FAKE_ROOT, FAKE_SUB["listappend"])
        cases.append(("N3 list(APPEND ... z)", d, 1))
        # N4: 共用件被第二次声明必红 (R1)
        d = base / "n4"; _mk(d, FAKE_ROOT,
                             "add_library(astrocs_platform_math INTERFACE)\n" + FAKE_SUB["green"])
        cases.append(("N4 共用件重复声明", d, 1))
        # N5: 共用件区块标记缺失 (判断被搬走) 必红 (R1)
        d = base / "n5"
        _mk(d, FAKE_ROOT.replace("─ 平台系统库唯一判定点 (test) ─", "moved")
                     .replace("─ END 平台系统库唯一判定点 ─", "moved-end"),
            FAKE_SUB["green"])
        cases.append(("N5 唯一判定点标记缺失", d, 1))
        # N6: 裸 m 落在**续行** (跨行链接语句) 必红 —— 真实假绿形态 (见 link_stmt_spans)
        d = base / "n6"; _mk(d, FAKE_ROOT, FAKE_SUB["multiline"])
        cases.append(("N6 跨行链接语句续行裸 m", d, 1))
        # N7: set(*_LINK*) 跨行, 裸名在续行 => 必红
        d = base / "n7"
        _mk(d, FAKE_ROOT,
            "add_executable(t t.cpp)\nset(T_LINK_LIBS\n  astrocs_platform_math\n  z)\n")
        cases.append(("N7 set 跨行续行裸 z", d, 1))
        # P2: 跨行 + 共用件 => 必绿 (不得"见多行就红")
        d = base / "p2"; _mk(d, FAKE_ROOT, FAKE_SUB["multiline_green"])
        cases.append(("P2 跨行链接语句引用共用件", d, 0))
        # P3: find_library(... NAMES ...) 跨行候选面 => 必绿 (不得误伤发现面)
        d = base / "p3"; _mk(d, FAKE_ROOT, FAKE_SUB["names_multiline"])
        cases.append(("P3 find_library NAMES 跨行候选面", d, 0))
        # P0: 真实仓库必绿
        cases.append(("P0 真实仓库", repo_root, 0))

        ok = True
        for label, root, want in cases:
            f = check(root)
            got = 1 if f else 0
            flag = "OK " if got == want else "!! "
            print(f"  {flag}{label}: 期望{'红' if want else '绿'} 实测{'红' if got else '绿'}"
                  f" (findings={len(f)})")
            if got != want:
                ok = False
                for x in f[:4]:
                    print(f"       {x['rule']} {x['where']}:{x['line']} {x['msg']}")
        print("== self-test " + ("PASS (正例全绿 / 负例全红)" if ok else "FAIL") + " ==")
        return 0 if ok else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=None, help="仓库根 (默认 = 本脚本所在仓)")
    ap.add_argument("--self-test", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    repo_root = Path(a.root).resolve() if a.root else Path(__file__).resolve().parents[2]
    if a.self_test:
        return self_test(repo_root, not a.quiet)
    f = check(repo_root)
    if a.json:
        print(json.dumps({"findings": f, "count": len(f)}, ensure_ascii=False, indent=2))
    elif f:
        print(f"PLATFORM-SYSLIB-LINKS: 判红 ({len(f)} 条)")
        for x in f:
            print(f"  {x['rule']} {x['where']}:{x['line']} {x['msg']}")
            if x.get("text"):
                print(f"      {x['text']}")
    elif not a.quiet:
        print("PLATFORM-SYSLIB-LINKS: 全绿 (共用件唯一声明; 全仓链接面无裸系统库名)")
    return 1 if f else 0


if __name__ == "__main__":
    sys.exit(main())
