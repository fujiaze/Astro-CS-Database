#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_gaia_zlib_configure_contract.py — CI-WIN-001 回归门 (module-local)。

权威:
  - 裁决 R-12: lib/gaia_xpsd_client 段与根 CMakeLists astrocs_cfitsio 段 ZLIB
    消费口径必须对齐 —— 同时消费 ACS_ZLIB_ROOT 与 ACS_ZLIB_LIB, 不得放宽
    REQUIRED;
  - 裁决 R-13: 环境/工具链配置缺陷按配置修, 不得降级门禁 / 不得改 waivable;
  - 宪章 §15.2 (Windows 正式工具链 VS2022/MSVC v143)、§14.4 fail-fast;
  - FD-R1-011: e6254d4d 判定"已修复"与实际不符 (只消费 root, 未消费 LIB)。
    前台实测 WIN-BUILD-RELEASE configure 原文:
      Could NOT find ZLIB (missing: ZLIB_LIBRARY) (found version "1.3.2")

做法 (本地同构, 不需 Windows 主机、不需真实 zlib 库、与宿主是否装 zlib 无关):
  把生产文件 lib/gaia_xpsd_client/CMakeLists.txt 逐字节复制进临时源树, 仅在其
  cmake_minimum_required 之后插入一行 project() (该文件是 add_subdirectory
  子目录文件, 自身无 project(); 其余内容零改动), 并用占位 .c 补齐 add_library
  的源文件 (configure 阶段不编译、不链接)。随后逐场景真实执行
  `cmake -S <tmp> -B <tmp>/build`, 断言退出码与 CMake 输出的
  `Found ZLIB: <lib> (found version "<ver>")` 落点。

  fake root = <tmp>/zlib-root/{include/zlib.h, lib/<INJECTED_LIB>}, 其 zlib.h
  声明 ZLIB_VERSION "1.3.2" (与 CI 报出的版本一致), 故 "库/版本落点" 可判别
  消费的是 fake root 还是宿主系统 zlib (本机系统 zlib = 1.3.1)。

场景 (S2/S2b/S4 判别性; S1/S3 守"基线不变"):
  S1  env 全空                -> 系统兜底, 落点不在任何 fake root 下 (基线不变)
  S2  有效 root + LIB 注入     -> rc=0, 库 == <root>/lib/<注入名>, 版本 == fake
                                 root 的 1.3.2 (root 与 LIB 两个 env 都被消费;
                                 注入名刻意取 FindZLIB NAMES 之外, 复刻 vcpkg
                                 x64-windows-static-md 静态产物 zs.lib)
  S2b 有效 root + 无 LIB 注入  -> configure 必败 (missing: ZLIB_LIBRARY):
                                 REQUIRED 仍在, 未降级为可选依赖
  S3  无效 root + LIB 注入     -> 与 S1 逐字段同口径 (退回系统搜索, 基线不变)
  S4  故障注入                -> 临时副本剥掉 ACS_ZLIB_LIB 消费块后 S2 场景必败
                                 且逐字复刻 CI 原文 (missing: ZLIB_LIBRARY /
                                 found version "1.3.2") —— 证明本门对 FD-R1-011
                                 型回退可判别 (先红后绿)
  S5  静态口径                -> find_package(ZLIB REQUIRED) 仍在且未被改成
                                 QUIET/可选依赖

S2/S2b/S4 传 -DCMAKE_IGNORE_PATH=<宿主隐式搜索目录> (由一次探针 configure 现场
读取 CMAKE_C_IMPLICIT_{INCLUDE,LINK}_DIRECTORIES 得到), 把搜索面收敛到 fake
root, 使断言不依赖宿主是否装有 zlib; S1/S3 故意不收敛, 以验证系统兜底本身。

证据: 设 GAIA_ZLIB_CONTRACT_EVIDENCE_DIR=<dir> 时逐场景落盘 argv/cwd/rc/
stdout/stderr (<dir>/<tag>.log) 与汇总 <dir>/scenarios.json。
GAIA_ZLIB_CONTRACT_SOURCE=<file> 可指向任一历史版本同名文件 (CI-WIN-001 复现
旧版 RED 留证用; 默认 = 生产文件)。
"""
from __future__ import annotations

import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
MODULE_CMAKE = REPO / "lib" / "gaia_xpsd_client" / "CMakeLists.txt"
CONFIGURE_TIMEOUT = 600
FAKE_VERSION = "1.3.2"          # 与 CI 报出 (found version "1.3.2") 一致
ZLIB_HEADER = f'#define ZLIB_VERSION "{FAKE_VERSION}"\n'
# 复刻 hosted runner "Provide zlib (vcpkg)" 步注入的确切库文件名: 不在 FindZLIB
# 的 NAMES(z zlib zdll zlib1 zlibstatic zlibwapi zlibvc zlibstat) 内, 只能靠
# ACS_ZLIB_LIB 直连命中 —— 与 vcpkg 静态产物 zs.lib 同型。
INJECTED_LIB = "zs.lib" if platform.system() == "Windows" else "libzs.so"
# 故障注入: 剥掉 ACS_ZLIB_LIB 消费块 (if(...ACS_ZLIB_LIB...)/set/endif 三行)
ACS_LIB_BLOCK_RE = re.compile(
    r"\n[ \t]*if\(NOT \"\$ENV\{ACS_ZLIB_LIB\}\" STREQUAL.*?\n[ \t]*endif\(\)",
    re.S)
FOUND_RE = re.compile(r'Found ZLIB: (\S+) \(found version "([^"]*)"\)')
NOTFOUND_RE = re.compile(
    r'Could NOT find ZLIB \(missing: ([^)]*)\)(?: \(found version "([^"]*)"\))?')
_FALLBACK_IGNORE = ("/usr/lib", "/usr/local/lib", "/usr/include", "/usr/local/include",
                    "/lib", "/lib64", "/usr/lib64", "/opt/local/lib", "/opt/local/include")


def _clean_env(**overrides: str | None) -> dict:
    """受控子进程环境: 清掉所有会影响 FindZLIB 的继承项, 再按场景注入。"""
    env = dict(os.environ)
    for key in ("ACS_ZLIB_ROOT", "ACS_ZLIB_LIB", "ZLIB_ROOT", "ZLIB_LIBRARY",
                "ZLIB_LIBRARY_RELEASE", "ZLIB_LIBRARY_DEBUG", "ZLIB_INCLUDE_DIR",
                "ZLIB_DIR", "CMAKE_PREFIX_PATH", "CMAKE_LIBRARY_PATH",
                "CMAKE_INCLUDE_PATH", "CMAKE_FRAMEWORK_PATH", "CMAKE_APPBUNDLE_PATH"):
        env.pop(key, None)
    for key, value in overrides.items():
        if value is None:
            env.pop(key, None)
        else:
            env[key] = value
    return env


def _cache_value(build_dir: Path, name: str) -> str | None:
    """读 CMakeCache.txt 中 <name>:<TYPE>=<value> (取最后一次出现)。"""
    cache = build_dir / "CMakeCache.txt"
    if not cache.is_file():
        return None
    found = None
    for line in cache.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith(name + ":"):
            found = line.split("=", 1)[1] if "=" in line else ""
    return found


def _found_zlib(log: str) -> tuple[str | None, str | None]:
    """解析 cmake 输出中的 zlib 落点 (成功) 或缺失项 (失败)。"""
    ok = FOUND_RE.search(log)
    if ok:
        return ok.group(1), ok.group(2)
    bad = NOTFOUND_RE.search(log)
    if bad:
        return None, bad.group(2)
    return None, None


def _system_ignore_paths(work: Path) -> list[str]:
    """现场探测宿主的隐式 include/link 目录 (隔离系统搜索面, 不硬编码路径)。"""
    src = work / "probe-src"
    src.mkdir(parents=True, exist_ok=True)
    (src / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\n"
        "project(probe C)\n"
        'message(STATUS "PROBE_INC=${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}")\n'
        'message(STATUS "PROBE_LNK=${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}")\n',
        encoding="utf-8")
    build = work / "probe-build"
    try:
        res = subprocess.run(["cmake", "-S", str(src), "-B", str(build)],
                             cwd=str(REPO), env=_clean_env(), capture_output=True,
                             text=True, timeout=CONFIGURE_TIMEOUT)
    except (OSError, subprocess.SubprocessError):
        return list(_FALLBACK_IGNORE)
    dirs: set[str] = set()
    for line in res.stdout.splitlines():
        for prefix in ("PROBE_INC=", "PROBE_LNK="):
            if prefix in line:
                dirs.update(d for d in line.split(prefix, 1)[1].split(";")
                            if d.startswith("/"))
    # 隐式目录的父目录一并忽略 (find_path 走 PATH_SUFFIXES include; 部分发行版
    # 把 /usr/lib/<triplet> 作为隐式目录, /usr 本身仍在系统前缀里)。
    for d in list(dirs):
        parts = Path(d).parts
        if len(parts) > 2:
            dirs.add(str(Path(*parts[:-1])))
    return sorted(dirs) or list(_FALLBACK_IGNORE)


def _write_fake_zlib_root(base: Path) -> Path:
    """伪造 zlib 安装树: <root>/include/zlib.h + <root>/lib/<INJECTED_LIB>。"""
    root = base / "zlib-root"
    (root / "include").mkdir(parents=True, exist_ok=True)
    (root / "lib").mkdir(parents=True, exist_ok=True)
    (root / "include" / "zlib.h").write_text(ZLIB_HEADER, encoding="utf-8")
    (root / "lib" / INJECTED_LIB).write_bytes(b"!<arch>\nACS_ZLIB_LIB fake\n")
    return root


def _make_project(base: Path, source_text: str) -> Path:
    """临时源树: 生产文件逐字节复制 + 仅插入 project() 一行 + 占位源文件。"""
    src = base / "src"
    (src / "src").mkdir(parents=True, exist_ok=True)
    (src / "include").mkdir(parents=True, exist_ok=True)
    for name in ("gaia_client.c", "module_entry.c"):
        (src / "src" / name).write_text(
            "/* CI-WIN-001 configure-contract placeholder (never compiled) */\n",
            encoding="utf-8")
    lines = source_text.splitlines(keepends=True)
    index = next((i for i, ln in enumerate(lines)
                  if ln.strip().startswith("cmake_minimum_required")), None)
    if index is None:
        raise AssertionError(
            "lib/gaia_xpsd_client/CMakeLists.txt 缺 cmake_minimum_required, "
            "无法补齐顶层上下文")
    lines.insert(index + 1, "project(astrocs_gaia_zlib_contract C)\n")
    (src / "CMakeLists.txt").write_text("".join(lines), encoding="utf-8")
    return src


class GaiaZlibConfigureContract(unittest.TestCase):
    """gaia 段 ZLIB 消费口径 —— 真实 configure 级回归门。"""

    @classmethod
    def setUpClass(cls):
        cls.source = Path(os.environ.get("GAIA_ZLIB_CONTRACT_SOURCE",
                                         str(MODULE_CMAKE)))
        if not cls.source.is_file():
            raise AssertionError(f"源文件不存在: {cls.source}")
        cls.source_text = cls.source.read_text(encoding="utf-8")
        cls.evidence_dir = os.environ.get("GAIA_ZLIB_CONTRACT_EVIDENCE_DIR")
        if cls.evidence_dir:
            Path(cls.evidence_dir).mkdir(parents=True, exist_ok=True)
        if shutil.which("cmake") is None:
            raise AssertionError("cmake 不在 PATH: 本检查的 prerequisite 缺失")
        cls._tmp = Path(tempfile.mkdtemp(prefix="gaia_zlib_contract_"))
        cls._records: list[dict] = []
        cls._ignore = _system_ignore_paths(cls._tmp)
        cls._isolate = ["-DCMAKE_IGNORE_PATH=" + ";".join(cls._ignore)]

    @classmethod
    def tearDownClass(cls):
        if cls.evidence_dir:
            out = Path(cls.evidence_dir) / "scenarios.json"
            out.write_text(json.dumps({
                "source": str(cls.source),
                "injected_lib": INJECTED_LIB,
                "fake_version": FAKE_VERSION,
                "system_ignore_paths": cls._ignore,
                "cmake": subprocess.run(["cmake", "--version"], capture_output=True,
                                        text=True, timeout=60).stdout.splitlines()[:1],
                "scenarios": cls._records,
            }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        shutil.rmtree(cls._tmp, ignore_errors=True)

    # ------------------------------------------------------------ 场景执行 ----
    def _configure(self, tag: str, source_text: str, env: dict,
                   extra_args: list[str]) -> tuple[int, Path, str]:
        base = Path(self._tmp) / tag
        base.mkdir(parents=True, exist_ok=True)
        src = _make_project(base, source_text)
        build = base / "build"
        argv = ["cmake", "-S", str(src), "-B", str(build)] + extra_args
        try:
            res = subprocess.run(argv, cwd=str(REPO), env=env, capture_output=True,
                                 text=True, timeout=CONFIGURE_TIMEOUT)
            rc, out, err = res.returncode, res.stdout, res.stderr
        except subprocess.TimeoutExpired as exc:
            rc = 124
            out = (exc.stdout or b"").decode("utf-8", "replace") if isinstance(
                exc.stdout, bytes) else (exc.stdout or "")
            err = ((exc.stderr or b"").decode("utf-8", "replace") if isinstance(
                exc.stderr, bytes) else (exc.stderr or "")) + "\nTIMEOUT"
        log = out + err
        lib, ver = _found_zlib(log)
        record = {"tag": tag, "argv": argv, "cwd": str(REPO), "rc": rc,
                  "zlib_lib": lib, "zlib_version": ver,
                  "ZLIB_LIBRARY_cache": _cache_value(build, "ZLIB_LIBRARY"),
                  "ZLIB_INCLUDE_DIR_cache": _cache_value(build, "ZLIB_INCLUDE_DIR"),
                  "ACS_ZLIB_ROOT_cache": _cache_value(build, "ACS_ZLIB_ROOT"),
                  "cmake_install_cmake": (build / "cmake_install.cmake").is_file(),
                  "stdout": out, "stderr": err}
        self._records.append(record)
        if self.evidence_dir:
            (Path(self.evidence_dir) / f"{tag}.log").write_text(
                f"# tag: {tag}\n# argv: {argv}\n# cwd: {REPO}\n# rc: {rc}\n"
                f"# zlib_lib: {lib}\n# zlib_version: {ver}\n"
                f"# ZLIB_LIBRARY(cache): {record['ZLIB_LIBRARY_cache']}\n"
                f"# ZLIB_INCLUDE_DIR(cache): {record['ZLIB_INCLUDE_DIR_cache']}\n"
                f"# cmake_install.cmake: {record['cmake_install_cmake']}\n"
                f"\n===== stdout =====\n{out}\n===== stderr =====\n{err}\n",
                encoding="utf-8")
        return rc, build, log

    def _record(self, tag: str) -> dict:
        return next(r for r in self._records if r["tag"] == tag)

    # ---------------------------------------------------------------- 用例 ----
    def test_s1_env_empty_system_fallback_baseline(self):
        """S1: env 全空 -> 系统兜底, 基线不变 (不落在任何 fake root 下)。"""
        fake = _write_fake_zlib_root(Path(self._tmp) / "s1")
        rc, _build, log = self._configure("S1-env-empty", self.source_text,
                                          _clean_env(), [])
        lib, _ver = _found_zlib(log)
        if rc == 0:
            self.assertIsNotNone(lib, f"S1 rc=0 但未报出 zlib 落点\n{log}")
            self.assertFalse(str(lib).startswith(str(fake)),
                             f"S1 命中了 fake root (env 为空不得消费 ACS): {lib}")
            self.assertTrue(os.path.exists(str(lib)),
                            f"S1 选中库文件不存在: {lib}")
        else:
            self.assertIn("missing: ZLIB_LIBRARY", log,
                          f"S1 失败原因非 zlib 缺失 (宿主无系统 zlib 时应报此项)\n{log}")

    def test_s2_valid_root_with_injected_lib_succeeds(self):
        """S2: 有效 root + ACS_ZLIB_LIB 注入 -> rc=0 且两个 env 都被消费。"""
        fake = _write_fake_zlib_root(Path(self._tmp) / "s2")
        env = _clean_env(ACS_ZLIB_ROOT=str(fake), ACS_ZLIB_LIB=INJECTED_LIB)
        rc, build, log = self._configure("S2-root+lib", self.source_text, env,
                                         self._isolate)
        self.assertEqual(rc, 0,
                         f"S2 configure 必败: ACS_ZLIB_LIB 未被消费\n{log}")
        self.assertEqual(_found_zlib(log)[0], str(fake / "lib" / INJECTED_LIB),
                         f"S2 zlib 库未落在 ACS_ZLIB_LIB 注入的文件上\n{log}")
        self.assertEqual(_found_zlib(log)[1], FAKE_VERSION,
                         f"S2 未用 ACS_ZLIB_ROOT/include 的头 (版本应为 fake "
                         f"{FAKE_VERSION})\n{log}")
        self.assertEqual(_cache_value(build, "ZLIB_LIBRARY"),
                         str(fake / "lib" / INJECTED_LIB),
                         f"S2 ZLIB_LIBRARY 缓存项不是注入库\n{log}")
        self.assertEqual(_cache_value(build, "ZLIB_INCLUDE_DIR"),
                         str(fake / "include"),
                         f"S2 ZLIB_INCLUDE_DIR 未落在 ACS_ZLIB_ROOT/include\n{log}")
        self.assertTrue((build / "cmake_install.cmake").is_file(),
                        "S2 configure 未产出 cmake_install.cmake "
                        "(WIN-PACKAGE-CANDIDATE install 阶段前提)\n" + log)

    def test_s2b_valid_root_without_injected_lib_fails_closed(self):
        """S2b: 有效 root 但无 ACS_ZLIB_LIB -> 必败 (REQUIRED 仍在)。"""
        fake = _write_fake_zlib_root(Path(self._tmp) / "s2b")
        env = _clean_env(ACS_ZLIB_ROOT=str(fake))
        rc, _build, log = self._configure("S2b-root-only", self.source_text, env,
                                          self._isolate)
        self.assertNotEqual(
            rc, 0, f"zlib 未找到却 configure 成功 (REQUIRED 被放宽?)\n{log}")
        self.assertIn("Could NOT find ZLIB", log, f"S2b 未复刻 CI 报错\n{log}")
        self.assertIn("missing: ZLIB_LIBRARY", log,
                      f"S2b 未复刻 CI 原文 missing: ZLIB_LIBRARY\n{log}")

    def test_s3_invalid_root_falls_back_to_system(self):
        """S3: 无效 root (+LIB 注入) -> 与 S1 逐字段同口径。"""
        base = Path(self._tmp) / "s3"
        base.mkdir(parents=True, exist_ok=True)
        bogus = base / "does-not-exist-root"
        env = _clean_env(ACS_ZLIB_ROOT=str(bogus), ACS_ZLIB_LIB=INJECTED_LIB)
        rc, _build, log = self._configure("S3-invalid-root", self.source_text, env, [])
        s1 = self._record("S1-env-empty")
        self.assertEqual(rc, s1["rc"],
                         f"S3 与 S1 退出码不同 (无效 root 不得改变基线)\n{log}")
        self.assertEqual(_found_zlib(log)[0], s1["zlib_lib"],
                         f"S3 与 S1 选中的库不同 (无效 root 不得伪命中)\n{log}")
        self.assertEqual(_found_zlib(log)[1], s1["zlib_version"],
                         f"S3 与 S1 版本不同 (无效 root 不得改变基线)\n{log}")
        if rc == 0:
            lib = _found_zlib(log)[0]
            self.assertFalse(str(lib).startswith(str(bogus)),
                             f"S3 命中了不存在的 root: {lib}")
            self.assertTrue(os.path.exists(str(lib)),
                            f"S3 选中库文件不存在: {lib}")

    def test_s4_fault_injection_without_acs_zlib_lib_consumption(self):
        """S4: 剥掉 ACS_ZLIB_LIB 消费块 (= e6254d4d 旧版) -> S2 场景必败。"""
        mutated, count = ACS_LIB_BLOCK_RE.subn("", self.source_text, count=1)
        if count == 0:
            # 源本身不含该消费块 (e6254d4d 旧版 = FD-R1-011 的形态): 直接以源
            # 为注入体, 后续断言必败 —— 这正是 CI-WIN-001 的先红证据。
            mutated = self.source_text
        # 注释里仍会提到 ACS_ZLIB_LIB / STREQUAL (历史说明), 故只断言可执行
        # 语句本身已消失; 若消费块存在却未被正则匹配, 说明文件结构漂移。
        self.assertNotIn("set(ZLIB_LIBRARY", mutated,
                         "ACS_ZLIB_LIB 消费块未被正则匹配: 生产文件结构已漂移, "
                         "本门必须随之更新 (不得静默跳过)")
        fake = _write_fake_zlib_root(Path(self._tmp) / "s4")
        env = _clean_env(ACS_ZLIB_ROOT=str(fake), ACS_ZLIB_LIB=INJECTED_LIB)
        rc, _build, log = self._configure("S4-fault-injection", mutated, env,
                                          self._isolate)
        self.assertNotEqual(rc, 0,
                            f"故障注入未被检出: 未消费 ACS_ZLIB_LIB 也 configure "
                            f"成功\n{log}")
        self.assertIn(f'Could NOT find ZLIB (missing: ZLIB_LIBRARY) '
                      f'(found version "{FAKE_VERSION}")', log,
                      f"S4 未逐字复刻 CI 原文 (missing: ZLIB_LIBRARY / "
                      f"found version {FAKE_VERSION})\n{log}")

    def test_s5_required_package_still_required(self):
        """S5: find_package(ZLIB REQUIRED) 仍在, 未被改成 QUIET/可选。"""
        self.assertIn("find_package(ZLIB REQUIRED)", self.source_text,
                      "gaia 段的 find_package(ZLIB REQUIRED) 被摘除/放宽")
        self.assertNotRegex(
            self.source_text,
            r"find_package\(ZLIB[^)]*(QUIET|OPTIONAL_COMPONENTS)",
            "gaia 段的 zlib 被降级为可选依赖 (裁决 R-12/R-13 禁止)")


if __name__ == "__main__":
    unittest.main(verbosity=2)
