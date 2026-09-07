#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ci_windows_driver.py — V8-CI-006 windows-main hosted 检查驱动（owner=SA-CI-32）。

控制包依据（02_CI_TASKS.md V8-CI-006「GitHub Windows」）：
  - windows-2022 / VS 2022 v143 / Win10 minimum target / ACR OFF，全部经
    CMakePresets.json 既有 preset ``win-msvc-17.14.39-x64``（base-msvc 冻结
    generator/toolset/SDK/C++17/ACR OFF；preset 自带 Windows 主机 condition，
    只在 Windows 主机生效）承载，本驱动不另设工具链事实源；
  - configure/build/test/install/package 全链；RelWithDebInfo（build/test
    preset ``win-rel``）；install 规则唯一源 = cmake/install_layout.cmake
    （BLD-003 白名单 install 树），本驱动用 ``cmake --install --prefix
    <candidate dir>`` 把白名单树落到 CI 产物目录（CMakeLists.txt:8 禁止
    正式 install——candidate 是 CI 产物目录而非系统安装）；
  - candidate 内含 BUILD_PROVENANCE.json / SOURCE_MANIFEST.json /
    SHA256SUMS；不含源码、测试数据、build cache；
  - 验证所有 DLL 名称、导出 ABI、加载、注册、CLI 入口、合成 Oracle。
    二进制级行为（dumpbin /EXPORTS、DLL 加载、astrocs.exe 运行）只在
    Windows 主机真跑（hosted）；非 Windows 主机逐项登记
    ``hosted-only`` 状态与 hosted 预期，绝不伪造 PASS。

阶段语义（--stages 逗号选择，按 canonical 顺序执行）：
  configure : cmake --preset win-msvc-17.14.39-x64（binaryDir 由 preset 决定）
  build     : cmake --build <build_dir> --config RelWithDebInfo --parallel
              （并行度经 NUMBER_OF_PROCESSORS/os.cpu_count() 探测，不硬编码）
  test      : ctest --preset win-rel --output-junit <junit>（CTest 单测集）
  install   : cmake --install <build_dir> --config RelWithDebInfo
              --prefix <candidate>（BLD-003 白名单树）
  package   : 组装 candidate（install 树 + 文档白名单 + 三清单 + 排除规则）
              并执行 candidate 校验（verify_candidate）

退出码（受控，不依赖 traceback）：
  0=成功；1=--stages 解析失败；2=目录校验失败（仓库外/candidate 不存在）；
  3=依赖工具缺失（cmake 不在 PATH）；5=candidate 校验失败（Windows 主机
  executed 项 FAIL）；6=package 组装失败；124=阶段超时；其他=首个失败
  cmake/ctest 步骤的原始退出码。

硬性纪律：所有子进程 argv 数组 + shell=False + 逐阶段 timeout；
JSON summary（阶段/退出码/时长/产物路径）；仅 stdlib。
"""
from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import threading
import zipfile
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

__all__ = [
    "main", "run_step", "parse_stages", "stage_plan", "build_candidate",
    "verify_candidate", "expected_windows_artifacts", "prune_candidate",
    "build_dir_from_preset", "EXCLUDE_PATTERNS", "DOC_WHITELIST",
    "pack_candidate_zip", "collect_error_lines", "ERROR_LINE_RE",
    "STAGE_LOG_TEMPLATE",
]

# ---------------------------------------------------------------------- 常量 ----

TASK_ID = "V8-CI-006"

DEFAULT_PRESET = "win-msvc-17.14.39-x64"   # configure preset（Windows 主机 condition）
DEFAULT_TEST_PRESET = "win-rel"            # ctest preset（RelWithDebInfo）
BUILD_CONFIG = "RelWithDebInfo"            # build/test/install 共用配置

DEFAULT_BUILD_DIR = "build/win-msvc-17.14.39-x64"  # preset binaryDir 的仓库相对形态
DEFAULT_CANDIDATE = "run/ci/win-candidate"         # candidate 目录（run/ 全局 ignore）
DEFAULT_JUNIT = "run/ci/win-test-junit.xml"
DEFAULT_OUTPUT = "run/ci/win-driver-summary.json"
# V8-CI-007 连带：--zip 打包输出（candidate zip 交 ci/validate_candidate.py 校验）
DEFAULT_ZIP = "artifacts/candidate/AstroCS-candidate.zip"

# 逐阶段默认 timeout 秒（逐阶段超时纪律；checks.json 的外层 timeout 更大）
STAGE_TIMEOUTS = {
    "configure": 900,
    "build": 2400,
    "test": 1800,
    "install": 600,
    "package": 600,
}
STAGE_ORDER = ("configure", "build", "test", "install", "package")

# V8-CI-010 F-R4-01：MSVC/MSBuild/链接器错误行过滤窗口。--parallel N 下
# error 行可能被后续 warning/link 行冲出 output_tail 的固定 25 行窗口，
# build 失败归属无法从 summary 定位；失败（exit≠0）时按此正则全量过滤
# 收集 error_lines（保序，cap 50 行防 warning 风暴），output_tail 语义不变。
ERROR_LINE_RE = re.compile(
    r"error C\d+|error LNK\d+|fatal error|: error |/error MSB\d+|LINK : fatal",
    re.IGNORECASE)
ERROR_LINES_CAP = 50

# F-R4-03：逐阶段全量 tee 日志（相对仓库根）。run 5e457d425fc8 实证：build
# exit 1 时 output_tail 25 行只见成功链接行（astrocs.vcxproj -> ...astrocs.exe），
# 真实 error（astrocs_io C1189 / io_reentrant_test LNK1104）仅靠 error_lines
# 幸存；全量日志 win-stage-<name>.log 保证下一轮可完整还原失败上下文。
STAGE_LOG_TEMPLATE = "run/ci/win-stage-{name}.log"


def collect_error_lines(lines: list[str]) -> list[str]:
    """按编译错误关键词过滤收集错误行（保序；cap ERROR_LINES_CAP 行）。"""
    hits = [ln for ln in lines if ERROR_LINE_RE.search(ln)]
    return hits[:ERROR_LINES_CAP]

# candidate 排除规则（build cache / CTest 日志 / 源码 / 测试数据；fnmatch 全路径匹配）
EXCLUDE_PATTERNS = (
    "CMakeCache.txt", "CMakeFiles/*", "CMakeFiles", "CTestTestfile.cmake",
    "Testing/*", "Testing", "install_manifest.txt", "*.log", "*.ilk",
    "Makefile", "cmake_install.cmake", "*.obj", "*.lib", "*.exp",
)
# 源码/测试数据后缀（防御性：install 白名单树本不含，出现即剔除并登记）
EXCLUDE_SUFFIXES = (
    ".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx", ".inl",
    ".py", ".pyc", ".rst", ".md.orig", ".sh",
)
EXCLUDE_DIR_PARTS = ("CMakeFiles", "Testing", "testdata", ".dSYM")
# 文档白名单（install 树之外的唯一附加物：(仓库相对源, candidate 内目标名)）
DOC_WHITELIST = (("README.md", "README.txt"),)
MANIFEST_NAMES = ("BUILD_PROVENANCE.json", "SOURCE_MANIFEST.json", "SHA256SUMS")

CONTRACT_REL = "packaging/install-tree.contract.json"
PRESETS_REL = "CMakePresets.json"


# ---------------------------------------------------------------------- 基础 ----

def utc_iso() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def _resolve(rel: str) -> Path:
    p = Path(rel)
    return p if p.is_absolute() else REPO / p


def _ensure_inside_repo(rel: str, opt: str) -> Path:
    p = _resolve(rel).resolve(strict=False)
    try:
        p.relative_to(REPO.resolve())
    except ValueError:
        raise SystemExit(f"ci_windows_driver: --{opt} 必须位于仓库内（{REPO}），实际 {rel}")
    return p


def _append_summary(out_file: Path | None, summary: dict) -> None:
    if out_file:
        out_file.parent.mkdir(parents=True, exist_ok=True)
        out_file.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                            encoding="utf-8")


def detect_jobs() -> int | None:
    """探测并行度（不硬编码：Windows NUMBER_OF_PROCESSORS / os.cpu_count）。"""
    env_jobs = os.environ.get("NUMBER_OF_PROCESSORS")
    if env_jobs and env_jobs.isdigit() and int(env_jobs) > 0:
        return int(env_jobs)
    n = os.cpu_count()
    return n if isinstance(n, int) and n > 0 else None


def run_step(argv: list[str], *, timeout: int, cwd: Path | None = None,
             env: dict[str, str] | None = None,
             log_path: Path | None = None) -> dict:
    """执行一步：argv 数组、shell=False、超时杀进程组；返回结果 dict。

    Windows 超时先 taskkill /F /T（杀进程树）再回退 proc.kill()。
    F-R4-03：log_path 提供时把合并输出全量 tee 到该文件（UTF-8, errors=replace），
    读线程边读边落盘并 flush——进程崩溃或超时被杀也保留已产出部分日志；
    tee 自身失败（OSError）不阻断主流程，只丢日志。
    """
    logfh = None
    if log_path is not None:
        try:
            log_path.parent.mkdir(parents=True, exist_ok=True)
            logfh = log_path.open("w", encoding="utf-8", errors="replace")
            logfh.write(f"$ {' '.join(argv)}\n")
        except OSError:
            logfh = None
    try:
        proc = subprocess.Popen(
            argv, cwd=str(cwd or REPO), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=False,
            start_new_session=(os.name == "posix"),
        )
    except FileNotFoundError:
        if logfh is not None:
            logfh.write(f"可执行不存在：{argv[0]}\n")
            logfh.close()
        return {"argv": argv, "exit_code": 127, "timed_out": False,
                "output_tail": f"可执行不存在：{argv[0]}", "error_lines": []}
    lines: list[str] = []

    def _pump() -> None:
        # 单读线程持续排空管道（避免 communicate 与大输出互锁）并 tee 落盘。
        assert proc.stdout is not None
        for raw in iter(proc.stdout.readline, b""):
            text = raw.decode("utf-8", "replace").rstrip("\r\n")
            lines.append(text)
            if logfh is not None:
                logfh.write(text + "\n")
                logfh.flush()

    pump = threading.Thread(target=_pump, daemon=True)
    pump.start()
    try:
        proc.wait(timeout=timeout)
        exit_code, timed_out = proc.returncode, False
    except subprocess.TimeoutExpired:
        timed_out = True
        if os.name == "nt":
            try:
                subprocess.run(["taskkill", "/F", "/T", "/PID", str(proc.pid)],
                               capture_output=True, timeout=15)
            except Exception:
                proc.kill()
        else:
            proc.kill()
        proc.wait()
        exit_code = 124
    pump.join(timeout=10)
    if logfh is not None:
        if timed_out:
            logfh.write(f"\n[ci_windows_driver] TIMED_OUT after {timeout}s\n")
        logfh.close()
    try:
        if proc.stdout is not None:
            proc.stdout.close()
    except Exception:
        pass
    tail = lines[-25:] if len(lines) > 25 else lines
    # F-R4-01：失败时额外收集编译/链接错误行（成功恒为 []，schema 兼容）
    error_lines = collect_error_lines(lines) if exit_code != 0 else []
    result = {"argv": argv, "exit_code": exit_code, "timed_out": timed_out,
              "output_tail": "\n".join(tail), "error_lines": error_lines}
    if log_path is not None:
        result["log_path"] = str(log_path)
    return result


def parse_stages(spec: str) -> list[str]:
    """解析 --stages 逗号列表：canonical 去重保序、拒绝未知阶段。"""
    if not spec or not spec.strip():
        raise ValueError("--stages 不能为空")
    seen: list[str] = []
    for tok in spec.split(","):
        tok = tok.strip().lower()
        if not tok:
            continue
        if tok not in STAGE_ORDER:
            raise ValueError(f"未知阶段：{tok!r}（可用：{'/'.join(STAGE_ORDER)}）")
        if tok not in seen:
            seen.append(tok)
    if not seen:
        raise ValueError("--stages 未选出任何阶段")
    return sorted(seen, key=STAGE_ORDER.index)


def build_dir_from_preset(preset: str, presets_path: Path | None = None) -> str:
    """从 CMakePresets.json 读取 configure preset binaryDir（单一事实源）。

    展开 ${sourceDir} 为仓库相对形态（'build/win-msvc-17.14.39-x64'）；
    preset 文件不可读时回退 DEFAULT_BUILD_DIR。
    """
    path = presets_path or (REPO / PRESETS_REL)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        for p in data.get("configurePresets", []):
            if p.get("name") == preset and not p.get("hidden"):
                bindir = str(p.get("binaryDir", ""))
                rel = bindir.replace("${sourceDir}", "").strip("/")
                return rel or DEFAULT_BUILD_DIR
    except Exception:
        pass
    return DEFAULT_BUILD_DIR


def _toolchain_from_presets(presets_path: Path | None = None) -> dict:
    """探测工具链冻结面（CMakePresets.json vendor.windows_formal；缺失置 null）。"""
    path = presets_path or (REPO / PRESETS_REL)
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
        vendor = data.get("vendor", {})
        for key, val in vendor.items():
            if key.endswith("/toolchain/1") or key == "astrocs.org/toolchain/1":
                return val.get("windows_formal", {})
    except Exception:
        pass
    return {}


def probe_tool(name: str) -> str | None:
    return shutil.which(name)


def cmake_version(exe: str = "cmake") -> str | None:
    """探测 cmake 版本（cmake --version 首行；探测所得，不硬编码）。"""
    res = run_step([exe, "--version"], timeout=30)
    if res["exit_code"] != 0:
        return None
    first = res["output_tail"].splitlines()[0] if res["output_tail"] else ""
    m = re.search(r"([0-9]+\.[0-9]+\.[0-9]+[^\s]*)", first)
    return m.group(1) if m else (first or None)


def msvc_compiler_probe() -> dict:
    """cl.exe 可达性探测（hosted vs devcmd 才有；受限环境登记 null）。"""
    exe = probe_tool("cl")
    if not exe:
        return {"cl_exe": None, "cl_version": None}
    res = run_step(["cl"], timeout=30)
    m = re.search(r"Version ([0-9.]+)", res["output_tail"])
    return {"cl_exe": exe, "cl_version": m.group(1) if m else None}


# ------------------------------------------------------------------ 阶段计划 ----

def stage_plan(stages: list[str], *, preset: str = DEFAULT_PRESET,
               build_dir: str = DEFAULT_BUILD_DIR,
               candidate_dir: str = DEFAULT_CANDIDATE,
               junit: str = DEFAULT_JUNIT,
               test_preset: str = DEFAULT_TEST_PRESET) -> list[dict]:
    """构造逐阶段计划：[{name, timeout, argv, kind}]（纯函数，单测直接断言）。"""
    jobs = detect_jobs()
    plan: list[dict] = []
    for stage in stages:
        timeout = STAGE_TIMEOUTS[stage]
        if stage == "configure":
            plan.append({"name": stage, "timeout": timeout,
                         "argv": ["cmake", "--preset", preset]})
        elif stage == "build":
            argv = ["cmake", "--build", build_dir, "--config", BUILD_CONFIG]
            if jobs:
                argv += ["--parallel", str(jobs)]  # 探测所得并行度
            plan.append({"name": stage, "timeout": timeout, "argv": argv})
        elif stage == "test":
            # --output-on-failure: 失败用例的 stderr/断言细节进 stage tee 日志
            # 与 junit（run 5e457d425fc8 的 7 failed 无断言细节可查，即缺此项）。
            # WIN-TEST-UNIT missing_output 根因: --output-junit 路径相对 ctest
            # 进程 cwd（=REPO），ctest 不自建父目录——run/ci/ 不存在时 junit
            # 写出失败且 ctest 仍 exit 0。driver 在起 test 前显式 mkdir 父目录
            # （与 _append_summary 的 mkdir 同口径）。
            junit_path = _resolve(junit)
            junit_path.parent.mkdir(parents=True, exist_ok=True)
            plan.append({"name": stage, "timeout": timeout,
                         "argv": ["ctest", "--preset", test_preset,
                                  "--output-on-failure",
                                  "--output-junit", str(junit_path)]})
        elif stage == "install":
            plan.append({"name": stage, "timeout": timeout,
                         "argv": ["cmake", "--install", build_dir,
                                  "--config", BUILD_CONFIG,
                                  "--prefix", candidate_dir]})
        elif stage == "package":
            plan.append({"name": stage, "timeout": timeout, "argv": [],
                         "kind": "python-internal"})
    return plan


def _is_excluded(rel_posix: str) -> bool:
    """candidate 排除规则（build cache/CTest 日志/源码/测试数据）。"""
    parts = rel_posix.split("/")
    name = parts[-1]
    if any(fnmatch.fnmatch(rel_posix, pat) or fnmatch.fnmatch(name, pat)
           for pat in EXCLUDE_PATTERNS):
        return True
    if any(name.lower().endswith(sfx) for sfx in EXCLUDE_SUFFIXES):
        return True
    if any(p in EXCLUDE_DIR_PARTS for p in parts[:-1]):
        return True
    return False


def prune_candidate(candidate: Path) -> list[str]:
    """对 candidate 执行排除规则（删除并返回被剔除的相对路径清单）。"""
    removed: list[str] = []
    for path in sorted(candidate.rglob("*")):
        rel = path.relative_to(candidate).as_posix()
        if path.is_file() and _is_excluded(rel):
            path.unlink()
            removed.append(rel)
    # 空目录收尾（Testing/、CMakeFiles/ 等）
    for path in sorted((p for p in candidate.rglob("*") if p.is_dir()),
                       key=lambda p: len(p.parts), reverse=True):
        try:
            path.rmdir()
        except OSError:
            pass
    return removed


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def git_ls_files(repo: Path) -> list[str]:
    """git ls-files（只读枚举 tracked 源文件清单；超时 60s）。"""
    res = run_step(["git", "ls-files"], timeout=60, cwd=repo)
    if res["exit_code"] != 0:
        return []
    return [ln for ln in res["output_tail"].splitlines() if ln.strip()]


def git_head_sha(repo: Path) -> str | None:
    res = run_step(["git", "rev-parse", "HEAD"], timeout=30, cwd=repo)
    if res["exit_code"] != 0:
        return None
    return res["output_tail"].strip() or None


def git_dirty_count(repo: Path) -> int:
    res = run_step(["git", "status", "--porcelain"], timeout=30, cwd=repo)
    if res["exit_code"] != 0:
        return -1
    return sum(1 for ln in res["output_tail"].splitlines() if ln.strip())


def detect_host() -> dict:
    """host 探测（OS palette；全部运行时所得，不硬编码）。"""
    runner = {}
    for k in ("RUNNER_OS", "GITHUB_ACTIONS", "GITHUB_WORKFLOW", "ImageOS"):
        v = os.environ.get(k)
        if v is not None:
            runner[k] = v
    return {
        "os": {
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
        },
        "runner": runner,
        "python": platform.python_version(),
    }


def build_provenance(*, source_sha: str | None, dirty_count: int,
                     preset: str, test_preset: str, build_dir: str,
                     candidate_dir: str, cmake_ver: str | None,
                     toolchain: dict, jobs: int | None) -> dict:
    """BUILD_PROVENANCE.json 内容（host/toolchain 探测所得，不硬编码）。"""
    return {
        "schema_version": 1,
        "task_id": TASK_ID,
        "generator": "tools/quality/ci_windows_driver.py",
        "built_utc": utc_iso(),
        "source_sha": source_sha,
        "source_dirty_entries": dirty_count,
        "preset": {
            "configure": preset,
            "test": test_preset,
            "build_config": BUILD_CONFIG,
        },
        "host": detect_host(),
        "toolchain": {
            "cmake_version": cmake_ver,
            "preset_formal": toolchain,
            "compiler": msvc_compiler_probe(),
        },
        "jobs": jobs,
        "build_dir": build_dir,
        "candidate_dir": candidate_dir,
        "acr_enabled": False,
    }


def source_manifest(source_sha: str | None, files: list[str], repo: Path) -> dict:
    """SOURCE_MANIFEST.json 内容：源文件路径+SHA256 清单（只列清单不复制源码）。"""
    entries = []
    for rel in sorted(set(files)):
        path = repo / rel
        if path.is_file():
            entries.append({"path": rel, "sha256": _sha256_file(path)})
    return {
        "schema_version": 1,
        "task_id": TASK_ID,
        "generated_utc": utc_iso(),
        "source_sha": source_sha,
        "algorithm": "sha256",
        "file_count": len(entries),
        "files": entries,
    }


def sha256_sums(candidate: Path) -> tuple[Path, int]:
    """生成 SHA256SUMS：candidate 内全部产物文件（<hex>  <relpath>）。"""
    files = sorted(p for p in candidate.rglob("*")
                   if p.is_file() and p.name != "SHA256SUMS")
    lines = [f"{_sha256_file(p)}  {p.relative_to(candidate).as_posix()}"
             for p in files]
    out = candidate / "SHA256SUMS"
    out.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return out, len(lines)


def expected_windows_artifacts(contract_path: Path | None = None) -> list[str]:
    """从 install-tree contract 推导 Windows 形态期望产物（ARC-001 dll_units）。

    映射规则（单一事实源 = packaging/install-tree.contract.json，不硬编码清单）：
      kind=exe  → astrocs.exe（Windows CLI 入口，astrocs 目标 OUTPUT_NAME）；
      *.so      → 同目录同名 *.dll（无 lib 前缀：MSVC/CMake 对 Windows DLL
                  不加前缀，见 install_layout.cmake「Windows 正式形态」注释——
                  astrocs_runtime.dll / astrocs_io.dll / modules/astrocs_noop.dll /
                  providers/astrocs_cpu_baseline.dll）；
      其余（licenses/schemas/manifest）原样保留。
    """
    path = contract_path or (REPO / CONTRACT_REL)
    data = json.loads(path.read_text(encoding="utf-8"))
    out: list[str] = []
    for unit in data.get("units", []):
        rel = str(unit.get("install_path", ""))
        if not rel:
            continue
        p = Path(rel)
        if unit.get("kind") == "exe":
            out.append((p.parent / (p.name + ".exe")).as_posix())
        elif p.name.endswith(".so"):
            stem = p.name[:-3]
            if stem.startswith("lib"):
                stem = stem[3:]
            out.append((p.parent / (stem + ".dll")).as_posix())
        else:
            out.append(rel)
    return sorted(set(out))


# --------------------------------------------------------- candidate 校验 ----

def _verify_pair(name: str, ok: bool | None, detail: str, *, executed: bool) -> dict:
    return {"item": name, "executed": executed, "verdict": ok, "detail": detail}


def verify_candidate(candidate: Path, *, run_binaries: bool | None = None,
                     contract_path: Path | None = None) -> dict:
    """candidate 校验：DLL 名称 / 导出 ABI / 加载 / 注册 / CLI 入口 / 合成 Oracle。

    run_binaries=False 强制 hosted-only 登记（非 Windows 主机）；
    None 时按平台自动判定（sys.platform == "win32"）。
    verdict 语义：true=PASS（仅 executed 项）、false=FAIL、None=hosted-only
    （本机受限，登记产物与 hosted 预期；绝不伪造 PASS）。
    """
    if run_binaries is None:
        run_binaries = sys.platform.startswith("win")
    checks: list[dict] = []

    # 1) DLL/EXE 名称符合 install_layout/ARC-001（install-tree contract 推导）。
    #    hosted-only（非 Windows 宿主）时 candidate 本身不存在，一律登记 None。
    expected = expected_windows_artifacts(contract_path)
    missing = [rel for rel in expected
               if not (candidate / rel).is_file()]
    if run_binaries:
        checks.append(_verify_pair(
            "dll_names_install_layout", not missing,
            (f"期望产物（contract 推导 Windows 形态）{len(expected)} 项；缺失 {missing}"
             if missing else f"期望产物 {len(expected)} 项全部就位：{expected}"),
            executed=True))
    else:
        checks.append(_verify_pair(
            "dll_names_install_layout", None,
            f"contract 推导期望产物 {len(expected)} 项；本机不落盘，"
            "install 形态结构校验在 hosted windows-2022 执行", executed=False))
    for manifest in MANIFEST_NAMES:
        checks.append(_verify_pair(
            f"manifest_{manifest}", (candidate / manifest).is_file()
            if run_binaries else None,
            f"candidate 根 {manifest} 存在性",
            executed=run_binaries))

    exe = candidate / "astrocs.exe"
    if not run_binaries:
        # 非 Windows 主机：二进制级行为逐项登记 hosted-only 预期
        hosted_note = "hosted windows-2022 才真跑（本机 Linux 无法执行 Windows 二进制）"
        for item, detail in (
            ("exported_abi_dumpbin",
             "dumpbin /EXPORTS astrocs_runtime.dll|astrocs_io.dll|"
             "modules/astrocs_noop.dll|providers/astrocs_cpu_baseline.dll "
             "非空符号表，且 astrocs_runtime 导出 acs_artifact_*、"
             "astrocs_io 导出 acs_fio_*（头文件冻结 C ABI）"),
            ("dll_load_and_register",
             "astrocs.exe modules list / modules verify 退出 0：平台 DLL 同目录"
             "加载 + noop 模块经 loader 注册（WIN-* hosted 端到端）"),
            ("cli_entry", "astrocs.exe version --json 退出 0 且 JSON 含 name=astrocs"),
            ("synthetic_oracle",
             "astrocs.exe selftest 退出 0（CLI 内置合成自检）；CTest 全合成矩阵"
             "由 WIN-TEST-UNIT 阶段 ctest --preset win-rel 承担"),
        ):
            checks.append(_verify_pair(item, None, f"{detail}；{hosted_note}",
                                       executed=False))
        return {"executed": run_binaries, "checks": checks,
                "expected_artifacts": expected}

    # ---- Windows 主机：真跑 ----
    dumpbin = probe_tool("dumpbin")
    for dll in ("astrocs_runtime.dll", "astrocs_io.dll",
                "modules/astrocs_noop.dll", "providers/astrocs_cpu_baseline.dll"):
        path = candidate / dll
        if not path.is_file() or not dumpbin:
            checks.append(_verify_pair(
                f"exported_abi:{dll}", None,
                f"dumpbin={'可用' if dumpbin else '不可达'}；产物存在={path.is_file()}",
                executed=False))
            continue
        res = run_step([dumpbin, "/EXPORTS", str(path)],
                       timeout=120, cwd=candidate)
        exports = [ln.strip() for ln in res["output_tail"].splitlines()
                   if re.match(r"^\s+\d+\s+[0-9A-Fa-f]{8}", ln)]
        ok = res["exit_code"] == 0 and bool(exports)
        checks.append(_verify_pair(
            f"exported_abi:{dll}", ok,
            f"dumpbin /EXPORTS 退出 {res['exit_code']}，导出符号 {len(exports)} 个",
            executed=True))

    if exe.is_file():
        env = dict(os.environ)
        env["PATH"] = str(candidate) + os.pathsep + env.get("PATH", "")
        for item, argv, expect_json in (
            ("dll_load_and_register:modules_list",
             [str(exe), "modules", "list"], False),
            ("dll_load_and_register:modules_verify",
             [str(exe), "modules", "verify"], False),
            ("cli_entry:version_json", [str(exe), "version", "--json"], True),
            ("synthetic_oracle:selftest", [str(exe), "selftest"], False),
        ):
            res = run_step(argv, timeout=180, cwd=candidate, env=env)
            ok = res["exit_code"] == 0
            if ok and expect_json:
                # version --json 返回 {"schema_version":"1","name":"astrocs",
                # "version":...}: "name" 键的值才是 astrocs。旧写法
                # "astrocs" in dict 检查的是键而非值, 恒假 → cli_entry 必 FAIL。
                try:
                    doc = json.loads(res["output_tail"].splitlines()[-1])
                    ok = doc.get("name") == "astrocs" and bool(doc.get("version"))
                except Exception:
                    ok = False
            checks.append(_verify_pair(item, ok,
                                       f"argv={argv} exit={res['exit_code']}",
                                       executed=True))
    else:
        checks.append(_verify_pair("cli_entry", False,
                                   "candidate/astrocs.exe 缺失", executed=True))
    failed = [c["item"] for c in checks if c["executed"] and c["verdict"] is False]
    return {"executed": run_binaries, "checks": checks,
            "expected_artifacts": expected, "failed": failed}


# ----------------------------------------------------------- package 组装 ----

def pack_candidate_zip(candidate: Path, zip_out: Path) -> dict:
    """V8-CI-007 连带：把组装完成的 candidate 目录打包为 zip。

    - zip 根 = candidate 根（BUILD_PROVENANCE.json / SOURCE_MANIFEST.json /
      SHA256SUMS / 产物树均在 zip 根下）；
    - 成员名统一 posix 相对路径、sorted 排序、固定时间戳（1980-01-01），
      使同内容 candidate 产出字节可复现的 zip（SHA 可比对）；
    - ZIP_DEFLATED 压缩；文件读取后即写，不驻留内存全量。
    """
    if not candidate.is_dir():
        raise SystemExit(f"ci_windows_driver: candidate 目录不存在，无法打包：{candidate}")
    members = sorted(p for p in candidate.rglob("*") if p.is_file())
    zip_out.parent.mkdir(parents=True, exist_ok=True)
    if zip_out.exists():
        zip_out.unlink()
    with zipfile.ZipFile(zip_out, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for path in members:
            arcname = path.relative_to(candidate).as_posix()
            info = zipfile.ZipInfo(arcname, date_time=(1980, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            zf.writestr(info, path.read_bytes())
    return {
        "zip": str(zip_out),
        "file_count": len(members),
        "bytes": zip_out.stat().st_size,
    }


def build_candidate(candidate: Path, *, source_repo: Path | None = None,
                    preset: str = DEFAULT_PRESET, test_preset: str = DEFAULT_TEST_PRESET,
                    build_dir: str = DEFAULT_BUILD_DIR,
                    cmake_ver: str | None = None,
                    run_binaries: bool | None = None) -> dict:
    """组装 candidate（排除规则 + 文档白名单 + 三清单）并校验。

    语义（V8-CI-006 规格）：
      - candidate = install 树（BLD-003 白名单，由 install 阶段落盘）+ 文档白名单；
      - 排除：源码文件、测试数据、build cache（CMakeCache/CMakeFiles/CTest 日志等）；
      - BUILD_PROVENANCE.json（host/toolchain 探测所得）、
        SOURCE_MANIFEST.json（git ls-files 源文件路径+SHA256，只列清单不复制）、
        SHA256SUMS（candidate 全部产物文件）。
    """
    repo = source_repo or REPO
    if not candidate.is_dir():
        raise SystemExit(f"ci_windows_driver: candidate 目录不存在（先跑 install 阶段）：{candidate}")
    removed = prune_candidate(candidate)
    copied_docs: list[str] = []
    for src_rel, dst_name in DOC_WHITELIST:
        src = repo / src_rel
        if src.is_file():
            shutil.copyfile(src, candidate / dst_name)
            copied_docs.append(dst_name)

    source_sha = git_head_sha(repo)
    provenance = build_provenance(
        source_sha=source_sha, dirty_count=git_dirty_count(repo),
        preset=preset, test_preset=test_preset, build_dir=build_dir,
        candidate_dir=str(candidate), cmake_ver=cmake_ver or cmake_version(),
        toolchain=_toolchain_from_presets(), jobs=detect_jobs())
    (candidate / "BUILD_PROVENANCE.json").write_text(
        json.dumps(provenance, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    manifest = source_manifest(source_sha, git_ls_files(repo), repo)
    (candidate / "SOURCE_MANIFEST.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    sums_path, sums_count = sha256_sums(candidate)
    artifact_files = sorted(p.relative_to(candidate).as_posix()
                            for p in candidate.rglob("*") if p.is_file())
    verification = verify_candidate(candidate, run_binaries=run_binaries)
    return {
        "candidate_dir": str(candidate),
        "artifact_files": artifact_files,
        "artifact_file_count": len(artifact_files),
        "pruned_by_exclude_rules": removed,
        "docs_whitelisted": copied_docs,
        "manifests": {
            "BUILD_PROVENANCE.json": "written",
            "SOURCE_MANIFEST.json": f"{manifest['file_count']} tracked files",
            "SHA256SUMS": f"{sums_count} entries",
        },
        "verification": verification,
    }


# --------------------------------------------------------------------- CLI ----

def _run_stages(stages: list[str], plan: list[dict], *, output: Path | None,
                candidate: Path, build_dir: str, preset: str, test_preset: str,
                cmake_ver: str | None) -> tuple[int, dict]:
    summary = {
        "driver": "ci_windows_driver.py", "task_id": TASK_ID,
        "generated_utc": utc_iso(),
        "stages_requested": stages, "stages": [], "exit_code": 0,
        "cmake": {"present": True, "version": cmake_ver},
    }
    rc = 0
    for step in plan:
        print(f"[ci_windows_driver] {step['name']}: {' '.join(step['argv'])}"
              if step["argv"] else f"[ci_windows_driver] {step['name']}: (python-internal)",
              flush=True)
        if step.get("kind") == "python-internal":
            t0 = datetime.now(timezone.utc)
            try:
                pkg = build_candidate(candidate, preset=preset,
                                      test_preset=test_preset,
                                      build_dir=build_dir, cmake_ver=cmake_ver)
                stage_res = {"name": "package", "argv": [], "exit_code": 0,
                             "timed_out": False,
                             "timeout": step["timeout"],
                             "duration_seconds": round(
                                 (datetime.now(timezone.utc) - t0).total_seconds(), 3),
                             "candidate": pkg}
                verification = pkg.get("verification", {})
                failed = [c["item"] for c in verification.get("checks", [])
                          if c["executed"] and c["verdict"] is False]
                if failed:
                    stage_res["exit_code"] = 5
                    stage_res["failed_verification"] = failed
                    # 失败时逐项带 detail（dumpbin 退出码/符号数/exe 输出尾），
                    # 否则 summary 只见 item 名无法离线定位根因。
                    stage_res["verification_details"] = [
                        c for c in verification.get("checks", [])
                        if c.get("executed") and c.get("verdict") is False]
                summary["candidate"] = {
                    "dir": pkg["candidate_dir"],
                    "file_count": pkg["artifact_file_count"],
                    "manifests": pkg["manifests"],
                }
            except SystemExit as exc:
                stage_res = {"name": "package", "argv": [], "exit_code": 6,
                             "timed_out": False, "timeout": step["timeout"],
                             "output_tail": str(exc)}
        else:
            # F-R4-03：子进程阶段全量 tee 到 run/ci/win-stage-<name>.log，
            # summary 记录 log 路径（相对仓库根）供 artifact 上传与离线诊断。
            log_rel = STAGE_LOG_TEMPLATE.format(name=step["name"])
            res = run_step(step["argv"], timeout=step["timeout"],
                           log_path=_resolve(log_rel))
            stage_res = {"name": step["name"], "argv": step["argv"],
                         "exit_code": res["exit_code"],
                         "timed_out": res["timed_out"],
                         "timeout": step["timeout"],
                         "output_tail": res["output_tail"],
                         "error_lines": res["error_lines"],
                         "log": log_rel}
        summary["stages"].append(stage_res)
        if stage_res["exit_code"] != 0:
            rc = stage_res["exit_code"]
            summary["exit_code"] = rc
            summary["failed_stage"] = step["name"]
            break
    _append_summary(output, summary)
    return rc, summary


def build_parser() -> argparse.ArgumentParser:
    ap = argparse.ArgumentParser(
        prog="ci_windows_driver.py",
        description="windows-main hosted 检查驱动（V8-CI-006）")
    ap.add_argument("--stages", default=",".join(STAGE_ORDER),
                    help="逗号分隔阶段集（configure/build/test/install/package）")
    ap.add_argument("--preset", default=DEFAULT_PRESET,
                    help="configure preset（默认冻结 preset，仅 Windows 主机生效）")
    ap.add_argument("--test-preset", dest="test_preset", default=DEFAULT_TEST_PRESET)
    ap.add_argument("--build-dir", default=None,
                    help="默认从 CMakePresets.json binaryDir 推导（单一事实源）")
    ap.add_argument("--candidate-dir", dest="candidate_dir", default=DEFAULT_CANDIDATE)
    ap.add_argument("--junit", default=DEFAULT_JUNIT)
    # V8-CI-007 连带：package 成功后把 candidate 打包为 zip（validate_candidate 输入）
    ap.add_argument("--zip", dest="zip_out", nargs="?", const=DEFAULT_ZIP,
                    default=None,
                    help=f"package 成功后打包 candidate 为 zip "
                         f"（默认 {DEFAULT_ZIP}；不传 = 只出目录）")
    ap.add_argument("--output", default=DEFAULT_OUTPUT, help="JSON summary 路径")
    ap.add_argument("--no-summary", action="store_true")
    ap.add_argument("--require-tools", nargs="*", default=["cmake"],
                    help="前置工具 PATH 探测（缺失 exit 3；--require-tools 空=跳过）")
    return ap


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        stages = parse_stages(args.stages)
    except ValueError as exc:
        print(f"ci_windows_driver: {exc}", file=sys.stderr)
        return 1

    build_dir = (build_dir_from_preset(args.preset) if args.build_dir is None
                 else args.build_dir)
    try:
        candidate = _ensure_inside_repo(args.candidate_dir, "candidate-dir")
        build = _ensure_inside_repo(build_dir, "build-dir")
    except SystemExit as exc:
        print(str(exc), file=sys.stderr)
        return 2

    missing = [t for t in (args.require_tools or []) if probe_tool(t) is None]
    if missing:
        print(json.dumps({
            "driver": "ci_windows_driver.py", "task_id": TASK_ID,
            "error": "依赖工具不在 PATH（受控报错，参照 V8-CI-005 deep 驱动模式）",
            "missing_tools": missing,
            "hosted_note": "windows-main 检查在 GitHub windows-2022 runner 上真跑；"
                           "本机 Linux 由 run.py probe_prerequisite → SKIPPED(waivable) 拦截",
        }, ensure_ascii=False), file=sys.stderr)
        return 3

    cmake_ver = cmake_version()
    plan = stage_plan(stages, preset=args.preset, build_dir=str(build),
                      candidate_dir=str(candidate), junit=args.junit,
                      test_preset=args.test_preset)
    output = None if args.no_summary else _resolve(args.output)
    rc, _summary = _run_stages(stages, plan, output=output, candidate=candidate,
                               build_dir=str(build), preset=args.preset,
                               test_preset=args.test_preset, cmake_ver=cmake_ver)
    # V8-CI-007 连带：package 成功且请求 --zip 时打包 candidate
    if args.zip_out and rc == 0 and "package" in stages:
        zip_path = _ensure_inside_repo(args.zip_out, "zip")
        packed = pack_candidate_zip(candidate, zip_path)
        print(json.dumps({"driver": "ci_windows_driver.py", "task_id": TASK_ID,
                          "candidate_zip": packed["zip"],
                          "zip_file_count": packed["file_count"],
                          "zip_bytes": packed["bytes"]}, ensure_ascii=False))
    return rc


if __name__ == "__main__":
    sys.exit(main())
