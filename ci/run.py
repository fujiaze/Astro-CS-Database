#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AstroCS 统一 CI 执行器（控制包任务 V8-CI-002，owner=SA-CI-32）。

职责（tasks/02_CI_TASKS.md「V8-CI-002」+ 07_CI_MACHINE_CONTRACT.md）：
  - 唯一 CI 入口：CI YAML 只调用 ``python3 ci/run.py --profile ...``，不复制业务命令。
  - 从唯一注册表 ``ci/checks.json`` 按 profile / --check / --focus / --changed-from
    （经 ci/impact_map.json）选择检查，逐项按登记 command 与 timeout 执行。
  - 捕获 exit code / stdout / stderr（logs/<check-id>.log，zstd 可用时附 .log.zst）。
  - mutates_workspace=false 的检查执行前后对比 ``git status --porcelain=v1
    --untracked-files=all``，出现新增/修改即 FAIL(dirty)（工作区纯净性，07 合同）。
  - 生成 per-check 结果 ``checks/<check-id>.json`` 与汇总 ``CI_RESULT.json``；
    verdict 一律由检查结果计算，禁止手填 PASS（伪造/陈旧结果一律重算覆盖）。
  - 支持 ``ci/known_failures.json`` 基线：过期/新增失败/计数增加/签名变化均 FAIL，
    精确匹配的已知失败标 KNOWN_FAIL（verdict 仍 FAIL，仅计数分离）。

设计约束：
  - 仅 stdlib；不依赖 pytest/jsonschema（schema 校验用内置最小子集校验器）。
  - 检查命令以 argv 数组执行（永不 shell=True），杜绝 shell 注入。
  - 所有外部命令均带 timeout；runner 自身退出码：0=PASS/FATDUCK_PENDING，
    1=FAIL，2=runner 配置或环境错误（registry 非法、ref 无效等）。
"""

from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import importlib.util
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid
from pathlib import Path

SCHEMA_VERSION = 1
PROFILES = ("fast", "linux-main", "windows-main", "linux-deep", "fatduck")

EXIT_OK = 0            # verdict PASS / FATDUCK_PENDING
EXIT_FAIL = 1          # 任一检查失败（含 KNOWN_FAIL，计数分离但 verdict 仍 FAIL）
EXIT_RUNNER_ERROR = 2  # runner 自身配置/环境错误（registry 非法、ref 无效、schema 违规）

GIT_TIMEOUT = 60          # runner 内部 git 命令超时（秒）
ZSTD_TIMEOUT = 60         # zstd 压缩超时（秒）
DEFAULT_LOG_LIMIT = 8 * 1024 * 1024  # 单检查日志截断上限（字节），原始文件全量保留
TAIL_LIMIT = 4000         # per-check JSON 内嵌 stdout/stderr 尾部上限（字节）
MONITOR_SCRIPT = "ci/resource_monitor.py"  # V8-CI-003 产物；heavy 项依赖其存在

# per-check verdict 值域（全部由 runner 计算，不接受外部输入）
V_PASS = "PASS"
V_FAIL = "FAIL"                        # 非零退出
V_TIMEOUT = "TIMEOUT"                  # 超过登记 timeout_seconds 被 kill
V_SIGNAL = "SIGNAL"                    # 被信号终止（非 timeout 路径）
V_MISSING_OUTPUT = "FAIL(missing_output)"
V_DIRTY = "FAIL(dirty)"
V_PREREQ = "FAIL(prerequisite)"
V_KNOWN = "KNOWN_FAIL"
V_SKIP_WAIVABLE = "SKIPPED(waivable)"
V_SKIP_PLATFORM = "SKIPPED(waivable)"  # platform 不匹配且 waivable=true 时复用该标记

HARD_FAILURE_VERDICTS = (V_FAIL, V_TIMEOUT, V_SIGNAL, V_MISSING_OUTPUT, V_DIRTY, V_PREREQ)


class RunnerError(Exception):
    """runner 自身配置/环境错误 → 退出码 2。"""


# ---------------------------------------------------------------------------
# 基础工具
# ---------------------------------------------------------------------------

def utc_now() -> _dt.datetime:
    """返回带时区的当前 UTC 时间。"""
    return _dt.datetime.now(_dt.timezone.utc)


def utc_iso(dt: _dt.datetime) -> str:
    """ISO8601 UTC 字符串（秒级精度，Z 后缀）。"""
    return dt.astimezone(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def parse_utc(value: str) -> _dt.datetime:
    """解析 ISO8601 UTC（容忍 Z / 偏移 / 无时区）。"""
    text = value.strip()
    if text.endswith("Z"):
        text = text[:-1] + "+00:00"
    dt = _dt.datetime.fromisoformat(text)
    if dt.tzinfo is None:
        dt = dt.replace(tzinfo=_dt.timezone.utc)
    return dt.astimezone(_dt.timezone.utc)


def run_cmd(argv: list[str], *, cwd: Path | None = None, timeout: float,
            env: dict[str, str] | None = None) -> subprocess.CompletedProcess:
    """运行外部命令（统一 timeout；纪律要求：所有外部命令带 timeout）。"""
    return subprocess.run(
        argv, cwd=str(cwd) if cwd else None, env=env,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=timeout,
    )


def git(repo: Path, *args: str, timeout: float = GIT_TIMEOUT) -> str:
    """在仓库内执行 git 命令并返回 stdout 文本。

    core.quotepath=off：porcelain/diff 的非 ASCII 路径按 UTF-8 直出，
    避免 C-quote 转义破坏路径解析（对 rev-parse 等无影响）。
    """
    try:
        proc = run_cmd(
            ["git", "-c", "core.quotepath=off", "-C", str(repo), *args], timeout=timeout
        )
    except subprocess.TimeoutExpired as exc:  # pragma: no cover - 防御分支
        raise RunnerError(f"git {' '.join(args)} 超时（{timeout}s）") from exc
    except FileNotFoundError as exc:
        raise RunnerError("git 可执行文件不存在") from exc
    if proc.returncode != 0:
        raise RunnerError(
            "git {} 失败（exit {}）：{}".format(
                " ".join(args), proc.returncode, proc.stderr.decode("utf-8", "replace").strip()
            )
        )
    return proc.stdout.decode("utf-8", "replace")


# ---------------------------------------------------------------------------
# 内置 JSON Schema 最小子集校验器（本机无 jsonschema 库；覆盖控制包 schema 所需关键字）
# ---------------------------------------------------------------------------

_TYPE_MAP = {
    "object": dict,
    "array": list,
    "string": str,
    "boolean": bool,
    "null": type(None),
}


def validate_against_schema(instance, schema: dict, path: str = "$") -> list[str]:
    """对 instance 按 JSON Schema 子集校验，返回错误列表（空列表=通过）。

    支持：type/const/enum/pattern/required/properties/additionalProperties/
    items/minItems/minimum。pattern 遵循 JSON Schema 语义（re.search）。
    """
    errors: list[str] = []

    def err(msg: str) -> None:
        errors.append(f"{path}: {msg}")

    expected = schema.get("type")
    if isinstance(expected, str):
        if expected == "integer":
            if not isinstance(instance, int) or isinstance(instance, bool):
                err(f"期望 integer，实际 {type(instance).__name__}")
                return errors
            if "minimum" in schema and instance < schema["minimum"]:
                err(f"值 {instance} 小于 minimum {schema['minimum']}")
        elif expected == "number":
            if not isinstance(instance, (int, float)) or isinstance(instance, bool):
                err(f"期望 number，实际 {type(instance).__name__}")
                return errors
        else:
            py_type = _TYPE_MAP.get(expected)
            if py_type is None:
                err(f"校验器不支持的 type：{expected}")
            elif not isinstance(instance, py_type):
                err(f"期望 {expected}，实际 {type(instance).__name__}")
                return errors
    if "const" in schema and instance != schema["const"]:
        err(f"期望 const {schema['const']!r}，实际 {instance!r}")
    if "enum" in schema and instance not in schema["enum"]:
        err(f"值 {instance!r} 不在枚举 {schema['enum']!r} 内")
    if isinstance(instance, str) and "pattern" in schema:
        if re.search(schema["pattern"], instance) is None:
            err(f"值 {instance!r} 不匹配 pattern {schema['pattern']!r}")
    if isinstance(instance, dict):
        for key in schema.get("required", []):
            if key not in instance:
                err(f"缺少 required 字段 '{key}'")
        props = schema.get("properties", {})
        addl = schema.get("additionalProperties", True)
        for key, value in instance.items():
            if key in props:
                errors.extend(validate_against_schema(value, props[key], f"{path}.{key}"))
            elif addl is False:
                err(f"出现 schema 未允许的字段 '{key}'")
            elif isinstance(addl, dict):
                errors.extend(validate_against_schema(value, addl, f"{path}.{key}"))
    if isinstance(instance, list):
        min_items = schema.get("minItems")
        if min_items is not None and len(instance) < min_items:
            err(f"数组长度 {len(instance)} 小于 minItems {min_items}")
        items = schema.get("items")
        if isinstance(items, dict):
            for idx, value in enumerate(instance):
                errors.extend(validate_against_schema(value, items, f"{path}[{idx}]"))
    return errors


def load_json(path: Path) -> dict:
    """加载 JSON 文件（文件缺失/解析失败 → RunnerError）。"""
    try:
        text = path.read_text(encoding="utf-8")
    except FileNotFoundError as exc:
        raise RunnerError(f"文件不存在：{path}") from exc
    except OSError as exc:
        raise RunnerError(f"无法读取 {path}：{exc}") from exc
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise RunnerError(f"JSON 解析失败 {path}：{exc}") from exc


# ---------------------------------------------------------------------------
# 注册表 / known_failures / impact_map
# ---------------------------------------------------------------------------

def load_registry(repo: Path, registry_path: Path) -> dict:
    """加载并校验检查注册表（结构对齐控制包 ci/checks.schema.json）。"""
    registry = load_json(registry_path)
    schema_path = repo / "ci" / "checks.schema.json"
    if schema_path.is_file():
        errors = validate_against_schema(registry, load_json(schema_path))
        if errors:
            raise RunnerError("ci/checks.json 不符合 ci/checks.schema.json：\n  " + "\n  ".join(errors))
    else:
        # schema 副本缺失时退回最小内置校验，仍保证核心字段齐全。
        if registry.get("schema_version") != SCHEMA_VERSION:
            raise RunnerError("ci/checks.json schema_version 必须为 1")
        if not isinstance(registry.get("checks"), list) or not registry["checks"]:
            raise RunnerError("ci/checks.json 缺少非空 checks 数组")
        for idx, check in enumerate(registry.get("checks", [])):
            missing = [k for k in ("id", "profiles", "platform", "command", "timeout_seconds",
                                   "heavy", "mutates_workspace", "outputs", "waivable")
                       if k not in check]
            if missing:
                raise RunnerError(f"checks[{idx}] 缺少字段：{missing}")
            if not isinstance(check["command"], list) or not check["command"]:
                raise RunnerError(f"checks[{idx}] command 必须是非空 argv 数组")

    checks: dict[str, dict] = {}
    duplicates: list[str] = []
    for check in registry.get("checks", []):
        cid = check["id"]
        if cid in checks:
            duplicates.append(cid)
        checks[cid] = check
    if duplicates:
        raise RunnerError(f"ci/checks.json 存在重复检查 ID（未登记项不得注册）：{sorted(set(duplicates))}")
    registry["_by_id"] = checks
    return registry


def load_known_failures(path: Path) -> tuple[list[dict], list[str]]:
    """加载 ci/known_failures.json；文件不存在 → ([], [])（跳过基线逻辑）。

    返回 (条目列表, 结构错误列表)。条目结构：
    {check_id, owner, reproducer, source_sha, expiry, reason[, expected_count, signature]}
    """
    if not path.is_file():
        return [], []
    data = load_json(path)
    if isinstance(data, list):
        entries = data
    elif isinstance(data, dict) and isinstance(data.get("failures"), list):
        entries = data["failures"]
    else:
        return [], ["known_failures.json 顶层必须是数组或含 failures 数组的对象"]

    required = ("check_id", "owner", "reproducer", "source_sha", "expiry", "reason")
    errors: list[str] = []
    cleaned: list[dict] = []
    for idx, entry in enumerate(entries):
        if not isinstance(entry, dict):
            errors.append(f"failures[{idx}] 不是对象")
            continue
        missing = [k for k in required if k not in entry or entry[k] in (None, "")]
        if missing:
            errors.append(f"failures[{idx}] 缺少字段：{missing}")
            continue
        if not re.fullmatch(r"[0-9a-f]{40}", str(entry["source_sha"])):
            errors.append(f"failures[{idx}] source_sha 不是 40 位十六进制")
        try:
            parse_utc(str(entry["expiry"]))
        except (ValueError, TypeError):
            errors.append(f"failures[{idx}] expiry 不是合法 ISO8601 时间：{entry['expiry']!r}")
        cleaned.append(entry)
    return cleaned, errors


def load_impact_map(path: Path) -> dict:
    """加载 changed path → checks 映射（不存在时返回空映射）。"""
    if not path.is_file():
        return {"rules": [], "fallback": []}
    data = load_json(path)
    if not isinstance(data.get("rules"), list):
        raise RunnerError("ci/impact_map.json 缺少 rules 数组")
    data.setdefault("fallback", [])
    return data


# ---------------------------------------------------------------------------
# 检查选择
# ---------------------------------------------------------------------------

def current_platform(platform_arg: str) -> str:
    """当前运行平台（--platform 覆盖；fatduck 由 harness 显式声明）。"""
    if platform_arg != "auto":
        return platform_arg
    if os.environ.get("ASTROCS_FATDUCK") == "1":
        return "fatduck"
    return "windows" if sys.platform.startswith("win") else "linux"


def _match_prefix(path: str, pattern: str) -> bool:
    """changed_paths/impact 路径前缀匹配（'cli/**' 视作目录前缀）。"""
    base = pattern.split("/**")[0].rstrip("/")
    target = path.split("/**")[0].rstrip("/")
    if not base:
        return False
    return target == base or target.startswith(base + "/") or base.startswith(target + "/") or target == base


def parse_porcelain_paths(status_text: str) -> list[str]:
    """解析 git status --porcelain 输出为路径列表（处理重命名与引号）。"""
    paths: list[str] = []
    for line in status_text.splitlines():
        if len(line) < 4:
            continue
        entry = line[3:]
        if " -> " in entry:
            entry = entry.split(" -> ", 1)[1]
        if entry.startswith('"') and entry.endswith('"'):
            entry = entry[1:-1]
        paths.append(entry)
    return paths


def changed_paths_from(repo: Path, ref: str) -> list[str]:
    """--changed-from ref：ref..HEAD 差异 ∪ 当前工作区未提交变更。"""
    git(repo, "rev-parse", "--verify", "--quiet", f"{ref}^{{commit}}")
    committed = git(repo, "diff", "--name-only", ref, "HEAD")
    try:
        status = git(repo, "status", "--porcelain=v1", "--untracked-files=all")
    except RunnerError:
        status = ""
    paths = [p for p in committed.splitlines() if p.strip()]
    paths.extend(parse_porcelain_paths(status))
    return sorted({p.replace("\\", "/") for p in paths if p.strip()})


def select_checks(registry: dict, *, profile: str, check_args: list[str], focus: str | None,
                  changed_from: str | None, impact: dict, repo: Path) -> tuple[list[dict], dict]:
    """按 CLI 选择条件筛出检查列表；返回 (checks, selection 元数据)。"""
    by_id: dict[str, dict] = registry["_by_id"]
    selected_by: dict[str, str] = {}

    if check_args:
        unknown = [cid for cid in check_args if cid not in by_id]
        if unknown:
            raise RunnerError(f"--check 指定了未登记的检查 ID：{unknown}")
        selected = [by_id[cid] for cid in check_args]
        for cid in check_args:
            selected_by[cid] = "explicit --check"
        return selected, {"mode": "explicit", "selected_by": selected_by}

    candidates = [c for c in registry["checks"] if profile in c["profiles"]]
    for check in candidates:
        selected_by[check["id"]] = f"profile:{profile}"

    changed_files: list[str] = []
    if changed_from:
        changed_files = changed_paths_from(repo, changed_from)
        matched_ids: set[str] = set()
        unmatched = []
        for path in changed_files:
            hit = False
            for rule in impact.get("rules", []):
                if any(_match_prefix(path, pat) for pat in rule.get("paths", [])):
                    hit = True
                    matched_ids.update(rule.get("checks", []))
            if not hit:
                unmatched.append(path)
        mapped = matched_ids | set(impact.get("fallback", []) if unmatched else [])
        allowed = {c["id"] for c in candidates} & mapped
        candidates = [c for c in candidates if c["id"] in allowed]
        for cid in list(selected_by):
            if cid not in allowed:
                selected_by.pop(cid, None)

    if focus:
        focused: list[dict] = []
        if focus in by_id:
            focused = [by_id[focus]]
        else:
            for check in candidates:
                if any(_match_prefix(focus, pat) for pat in check.get("changed_paths", [])):
                    focused.append(check)
        keep = {c["id"] for c in focused}
        candidates = focused
        for cid in list(selected_by):
            if cid not in keep:
                selected_by.pop(cid, None)

    meta = {
        "mode": "profile" if not changed_from and not focus else "filtered",
        "selected_by": selected_by,
        "changed_files": changed_files,
    }
    return candidates, meta


# ---------------------------------------------------------------------------
# prerequisite 探测（本机/本环境无法运行 → waivable 跳过，否则 FAIL(prerequisite)）
# ---------------------------------------------------------------------------

def _script_exists(repo: Path, rel: str) -> bool:
    return (repo / rel).exists()


def probe_prerequisite(check: dict, repo: Path, platform: str) -> tuple[bool, str | None]:
    """轻量前置条件探测；返回 (ok, reason)。

    覆盖：command[0] 可执行性、``python3 -m <mod>`` 模块存在性、
    command 中引用的仓库相对路径存在性（排除登记 outputs）、
    requires_monitor 检查统一监控包装器（V8-CI-003 产物）是否存在、
    platform 适用性。
    """
    command = check["command"]
    exe = command[0]
    if "/" in exe or "\\" in exe:
        if not _script_exists(repo, exe):
            return False, f"command 可执行文件不存在：{exe}"
    elif shutil.which(exe) is None:
        return False, f"command 可执行文件不在 PATH：{exe}"

    outputs = set(check.get("outputs", []))
    for idx, arg in enumerate(command):
        if idx == 0 or arg.startswith("-"):
            continue
        if idx >= 1 and command[idx - 1] == "-m":
            # python3 -m <module>：子进程以 cwd=repo 运行，仓库内模块按文件布局
            # （a/b/c.py、a/b/c/__init__.py 或 namespace 包目录）即可定位；
            # 仓库外模块（stdlib/第三方）才退回 runner 进程的 find_spec。
            rel_mod = arg.replace(".", "/")
            module_locatable = (
                _script_exists(repo, rel_mod + ".py")
                or _script_exists(repo, rel_mod + "/__init__.py")
                or (repo / rel_mod).is_dir()
                or importlib.util.find_spec(arg) is not None
            )
            if not module_locatable:
                return False, f"python -m 模块不存在：{arg}"
            continue
        if " " in arg or "\t" in arg:
            continue  # 含空白的参数视为字面值（如 -c 代码串），不做路径探测
        if "/" not in arg and "\\" not in arg:
            continue
        if arg in outputs or arg.startswith("/"):
            continue  # 登记输出路径运行时才生成；绝对路径属环境特定
        if not _script_exists(repo, arg):
            return False, f"command 引用的仓库路径不存在：{arg}"

    if check.get("requires_monitor") and not _script_exists(repo, MONITOR_SCRIPT):
        return False, f"heavy 检查要求统一监控包装器（{MONITOR_SCRIPT}，V8-CI-003）未就绪"

    if check["platform"] != "any" and check["platform"] != platform:
        return False, f"登记平台为 {check['platform']}，当前运行平台为 {platform}（platform_mismatch）"
    return True, None


# ---------------------------------------------------------------------------
# 执行单个检查
# ---------------------------------------------------------------------------

def snapshot_status(repo: Path) -> dict[str, tuple[str, int, int]]:
    """工作区快照：path → (porcelain XY, mtime_ns, size)。

    porcelain 只能发现新增条目；对已 dirty 的路径叠加 mtime/size，
    才能检出「检查前已修改的文件被再次修改」。
    """
    text = git(repo, "status", "--porcelain=v1", "--untracked-files=all")
    snapshot: dict[str, tuple[str, int, int]] = {}
    for line in text.splitlines():
        if len(line) < 4:
            continue
        xy = line[:2]
        entry = line[3:]
        if " -> " in entry:
            entry = entry.split(" -> ", 1)[1]
        if entry.startswith('"') and entry.endswith('"'):
            entry = entry[1:-1]
        try:
            st = os.stat(repo / entry)
            snapshot[entry] = (xy, st.st_mtime_ns, st.st_size)
        except OSError:
            snapshot[entry] = (xy, 0, 0)  # 已删除文件
    return snapshot


def _is_ignored(path: str, ignore_prefixes: list[str], ignore_exact: set[str]) -> bool:
    if path in ignore_exact:
        return True
    return any(path == p or path.startswith(p) for p in ignore_prefixes)


def detect_dirty(before: dict, after: dict, ignore_prefixes: list[str],
                 ignore_exact: set[str]) -> list[str]:
    """工作区纯净性对比（07 合同）：新增/修改即违规；忽略 runner 产物与登记 outputs。"""
    violations: list[str] = []
    for path, entry in after.items():
        if _is_ignored(path, ignore_prefixes, ignore_exact):
            continue
        old = before.get(path)
        if old is None:
            violations.append(path)
            continue
        xy_old, m_old, s_old = old
        xy_new, m_new, s_new = entry
        if xy_new != xy_old:
            violations.append(path)  # 状态漂移：新增修改/暂存化/tracked 文件被删除（含 M→D）
        elif (m_new, s_new) != (m_old, s_old) and xy_new.strip() in {"M", "A", "??"}:
            violations.append(path)  # 已修改/新增文件内容再变化
    return sorted(set(violations))


def _terminate(process: subprocess.Popen) -> None:
    """终止检查进程树（POSIX 杀进程组；Windows 退回 kill）。"""
    try:
        if os.name == "posix":
            import errno
            try:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                return
            except OSError as exc:  # 进程组已退出
                if exc.errno != errno.ESRCH:
                    process.kill()
        else:  # pragma: no cover - Windows 路径
            process.kill()
    except Exception:
        try:
            process.kill()
        except OSError:
            pass


def _tail(data: bytes, limit: int = TAIL_LIMIT) -> str:
    text = data.decode("utf-8", "replace")
    if len(text) <= limit:
        return text
    return text[-limit:]


def execute_check(check: dict, repo: Path, out_root: Path, platform: str,
                  strict_workspace: bool) -> dict:
    """执行一项检查并返回 per-check 结果 dict（verdict 全部由执行结果计算）。"""
    cid = check["id"]
    started = utc_now()
    result: dict = {
        "schema_version": SCHEMA_VERSION,
        "id": cid,
        "command": check["command"],
        "timeout_seconds": check["timeout_seconds"],
        "heavy": check["heavy"],
        "requires_monitor": check.get("requires_monitor", False),
        "waivable": check["waivable"],
        "mutates_workspace": check["mutates_workspace"],
        "platform_registered": check["platform"],
        "platform_runtime": platform,
        "exit_code": None,
        "timed_out": False,
        "signal": None,
        "stdout_tail": "",
        "stderr_tail": "",
        "log": None,
        "log_zst": None,
        "started_utc": utc_iso(started),
        "finished_utc": None,
        "duration_seconds": 0.0,
        "dirty": {"checked": False, "violations": []},
        "outputs_expected": check["outputs"],
        "outputs_missing": [],
        "prerequisite": {"checked": True, "ok": True, "reason": None},
        "verdict": None,
        "reason": None,
    }

    ok, reason = probe_prerequisite(check, repo, platform)
    if not ok:
        finished = utc_now()
        result["finished_utc"] = utc_iso(finished)
        result["duration_seconds"] = round((finished - started).total_seconds(), 3)
        result["prerequisite"] = {"checked": True, "ok": False, "reason": reason}
        if check["waivable"]:
            result["verdict"] = V_SKIP_WAIVABLE
            result["reason"] = f"prerequisite 未满足（waivable）：{reason}"
        else:
            result["verdict"] = V_PREREQ
            result["reason"] = f"prerequisite 未满足：{reason}"
        return result

    logs_dir = out_root / "logs"
    checks_dir = out_root / "checks"
    logs_dir.mkdir(parents=True, exist_ok=True)
    checks_dir.mkdir(parents=True, exist_ok=True)

    ignore_prefixes: list[str] = []
    if strict_workspace:
        try:
            rel_out = out_root.resolve().relative_to(repo.resolve())
            ignore_prefixes = [rel_out.as_posix() + "/"]
        except ValueError:
            ignore_prefixes = []  # 输出目录在仓库外时无需忽略自身产物
    ignore_exact = set(check["outputs"]) if strict_workspace else set()
    dirty_checked = strict_workspace and not check["mutates_workspace"]
    before = snapshot_status(repo) if dirty_checked else {}

    env = dict(os.environ)
    env.setdefault("PYTHONIOENCODING", "utf-8")
    env["ASTROCS_CI_CHECK_ID"] = cid
    timed_out = False
    stdout_b, stderr_b = b"", b""
    t0 = time.monotonic()
    try:
        proc = subprocess.Popen(
            check["command"], cwd=str(repo), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            shell=False, start_new_session=(os.name == "posix"),
        )
    except FileNotFoundError as exc:
        finished = utc_now()
        result["finished_utc"] = utc_iso(finished)
        result["duration_seconds"] = round((finished - started).total_seconds(), 3)
        result["verdict"] = V_PREREQ
        result["reason"] = f"无法启动命令：{exc}"
        return result

    try:
        stdout_b, stderr_b = proc.communicate(timeout=check["timeout_seconds"])
    except subprocess.TimeoutExpired:
        timed_out = True
        _terminate(proc)
        try:
            stdout_b, stderr_b = proc.communicate(timeout=30)
        except subprocess.TimeoutExpired:  # pragma: no cover - 极端残留
            stdout_b, stderr_b = b"", b""
    duration = time.monotonic() - t0
    finished = utc_now()
    returncode = proc.returncode

    result["exit_code"] = returncode
    result["timed_out"] = timed_out
    result["finished_utc"] = utc_iso(finished)
    result["duration_seconds"] = round(duration, 3)
    result["stdout_tail"] = _tail(stdout_b)
    result["stderr_tail"] = _tail(stderr_b)
    if returncode is not None and returncode < 0:
        result["signal"] = -returncode

    # 日志：分节写 stdout/stderr（全量，超限截断并标注）
    limit = DEFAULT_LOG_LIMIT
    header = (
        f"# check_id: {cid}\n"
        f"# command: {json.dumps(check['command'], ensure_ascii=False)}\n"
        f"# started_utc: {result['started_utc']}\n"
        f"# timeout_seconds: {check['timeout_seconds']}\n"
    )
    stdout_full, stdout_trunc = stdout_b, False
    stderr_full, stderr_trunc = stderr_b, False
    if len(stdout_full) > limit:
        stdout_full, stdout_trunc = stdout_full[-limit:], True
    if len(stderr_full) > limit:
        stderr_full, stderr_trunc = stderr_full[-limit:], True
    log_text = (
        header
        + f"# exit_code: {returncode}\n# timed_out: {timed_out}\n"
        + ("===== STDOUT =====\n" if not stdout_trunc else "===== STDOUT (尾部截断) =====\n")
        + stdout_full.decode("utf-8", "replace")
        + ("\n" if stdout_full else "")
        + ("===== STDERR =====\n" if not stderr_trunc else "===== STDERR (尾部截断) =====\n")
        + stderr_full.decode("utf-8", "replace")
    )
    try:
        log_path = logs_dir / f"{cid}.log"
        log_path.write_text(log_text, encoding="utf-8")
        result["log"] = log_path.relative_to(out_root).as_posix()
        if stdout_trunc or stderr_trunc:
            result["reason"] = "日志超过截断上限，已保留尾部（原始压缩日志保留 14 天）"
    except OSError as exc:  # pragma: no cover
        result["reason"] = f"日志写入失败：{exc}"

    # zstd 可用时附加压缩日志（07 合同 logs/<check-id>.log.zst 形态）
    zstd = shutil.which("zstd")
    if zstd and log_path.is_file():
        try:
            run_cmd([zstd, "-q", "-f", str(log_path)], timeout=ZSTD_TIMEOUT)
            zst = log_path.with_suffix(log_path.suffix + ".zst")
            if zst.is_file():
                result["log_zst"] = zst.relative_to(out_root).as_posix()
        except subprocess.TimeoutExpired:  # pragma: no cover
            pass

    if dirty_checked:
        after = snapshot_status(repo)
        violations = detect_dirty(before, after, ignore_prefixes, ignore_exact)
        result["dirty"] = {"checked": True, "violations": violations}
    else:
        result["dirty"] = {"checked": False, "violations": []}

    # ---- verdict 判定（顺序固定，全部由证据计算，无手填入口）----
    if timed_out:
        result["verdict"] = V_TIMEOUT
        result["reason"] = result["reason"] or f"超过登记 timeout {check['timeout_seconds']}s，进程已被终止"
    elif returncode is not None and returncode < 0 and not timed_out:
        result["verdict"] = V_SIGNAL
        result["reason"] = result["reason"] or f"进程被信号终止：{-returncode}"
    elif result["dirty"]["violations"]:
        result["verdict"] = V_DIRTY
        result["reason"] = result["reason"] or "mutates_workspace=false 的检查修改了工作区"
    elif returncode != 0:
        result["verdict"] = V_FAIL
        result["reason"] = result["reason"] or f"命令非零退出：{returncode}"
    else:
        missing = [rel for rel in check["outputs"] if not _script_exists(repo, rel)]
        result["outputs_missing"] = missing
        if missing:
            result["verdict"] = V_MISSING_OUTPUT
            result["reason"] = f"exit 0 但登记输出缺失：{missing}"
        else:
            result["verdict"] = V_PASS
    return result


# ---------------------------------------------------------------------------
# known_failures 基线比较（07 合同）
# ---------------------------------------------------------------------------

def failure_signature(log_path: Path | None) -> str | None:
    """失败输出签名：日志内容 sha256 前 16 位。"""
    if log_path is None or not Path(log_path).is_file():
        return None
    import hashlib
    return hashlib.sha256(Path(log_path).read_bytes()).hexdigest()[:16]


def apply_known_failures(entries: list[dict], structure_errors: list[str],
                         check_results: list[dict], now: _dt.datetime,
                         out_root: Path) -> tuple[list[dict], dict]:
    """比较当前失败集合与基线。返回 (更新后的 per-check 列表, known_failures 摘要)。

    规则：结构错误/条目过期/新增失败/计数增加/签名变化 → 维持 FAIL；
    精确匹配（check_id 相等且未过期）→ verdict 置 KNOWN_FAIL（计数分离，总 verdict 仍 FAIL）。
    """
    summary: dict = {
        "enabled": bool(entries) or bool(structure_errors),
        "matched": [],
        "new_failures": [],
        "expired": [],
        "invalid": list(structure_errors),
        "signature_changed": [],
        "count_increased": [],
    }
    if not entries and not structure_errors:
        return check_results, summary

    by_id: dict[str, list[dict]] = {}
    for entry in entries:
        by_id.setdefault(str(entry["check_id"]), []).append(entry)

    for entry in entries:
        if parse_utc(str(entry["expiry"])) < now:
            summary["expired"].append({
                "check_id": str(entry["check_id"]), "expiry": str(entry["expiry"]),
                "owner": str(entry["owner"]),
            })

    for result in check_results:
        cid = result["id"]
        if result["verdict"] not in HARD_FAILURE_VERDICTS:
            continue
        matches = by_id.get(cid, [])
        if not matches:
            summary["new_failures"].append(cid)
            continue
        active = [e for e in matches if parse_utc(str(e["expiry"])) >= now]
        if not active:
            continue  # 全部过期已在 expired 中报告，失败保持 FAIL
        result["verdict"] = V_KNOWN
        result["known_failure"] = {
            "owner": active[0]["owner"],
            "reproducer": active[0]["reproducer"],
            "source_sha": active[0]["source_sha"],
            "reason": active[0]["reason"],
        }
        summary["matched"].append(cid)
        if len(active) > 1:
            expected_counts = [int(e.get("expected_count", 1)) for e in active]
            if sum(expected_counts) < len(active):
                summary["count_increased"].append(cid)
        signature = active[0].get("signature")
        log_rel = result.get("log")
        if signature and log_rel:
            # 日志写入失败等异常路径下 log 为空，签名无从比较，保持 KNOWN_FAIL。
            log_abs = Path(log_rel)
            if not log_abs.is_absolute():
                log_abs = out_root / log_abs
            actual = failure_signature(log_abs)
            if actual is not None and actual != signature:
                summary["signature_changed"].append(cid)
                result["verdict"] = V_FAIL
                result["reason"] = "失败输出签名与 known_failures 基线不一致（signature_changed）"
    return check_results, summary


# ---------------------------------------------------------------------------
# CI_RESULT 组装与 schema 自校验
# ---------------------------------------------------------------------------

def build_ci_result(*, repo: Path, profile: str, selected_meta: dict, check_results: list[dict],
                    started: _dt.datetime, strict_workspace: bool, changed_from: str | None,
                    known_summary: dict) -> dict:
    finished = utc_now()
    executed = [r for r in check_results if r["verdict"] != V_SKIP_WAIVABLE]
    hard_fail = [r for r in executed if r["verdict"] in HARD_FAILURE_VERDICTS]
    known_fail = [r for r in executed if r["verdict"] == V_KNOWN]
    passed = [r for r in executed if r["verdict"] == V_PASS]
    skipped = [r for r in check_results if r["verdict"] == V_SKIP_WAIVABLE]

    if profile == "fatduck" and not executed:
        verdict = "FATDUCK_PENDING"
        verdict_reason = "fatduck profile 无本机可执行检查，等待 Fatduck harness 结果"
    elif not executed and not known_summary.get("expired"):
        verdict = "FAIL"
        verdict_reason = "未选中任何可执行检查（no_checks_selected），不允许空集 PASS"
    elif hard_fail or known_fail or known_summary.get("expired") or known_summary.get("invalid"):
        verdict = "FAIL"
        verdict_reason = None
    elif passed and not skipped and not known_fail:
        verdict = "PASS"
        verdict_reason = None
    elif passed and skipped:
        verdict = "PASS"
        verdict_reason = f"全部非 waivable 检查 PASS；{len(skipped)} 项 SKIPPED(waivable)"
    else:
        verdict = "FAIL"
        verdict_reason = "无可判定结果"

    result = {
        "schema_version": SCHEMA_VERSION,
        "generated_by": "ci/run.py (V8-CI-002)",
        "source_sha": git(repo, "rev-parse", "HEAD").strip(),
        "profile": profile,
        "run_id": selected_meta.get("run_id", ""),
        "started_utc": utc_iso(started),
        "finished_utc": utc_iso(finished),
        "strict_workspace": strict_workspace,
        "changed_from": changed_from,
        "selection": {
            "mode": selected_meta.get("mode"),
            "selected_by": selected_meta.get("selected_by", {}),
            "changed_files_count": len(selected_meta.get("changed_files", [])),
        },
        "checks": [
            {
                "id": r["id"],
                "verdict": r["verdict"],
                "exit_code": r["exit_code"],
                "duration_seconds": r["duration_seconds"],
                "timed_out": r.get("timed_out", False),
                "signal": r.get("signal"),
                "waivable": r["waivable"],
                "dirty_checked": r["dirty"]["checked"],
                "dirty_violations": r["dirty"]["violations"],
                "reason": r.get("reason"),
                "log": r.get("log"),
            }
            for r in check_results
        ],
        "summary": {
            "total": len(check_results),
            "pass": len(passed),
            "fail": len([r for r in executed if r["verdict"] in HARD_FAILURE_VERDICTS]),
            "known_fail": len(known_fail),
            "skipped_waivable": len(skipped),
            "fail_detail": {v: len([r for r in executed if r["verdict"] == v])
                            for v in HARD_FAILURE_VERDICTS
                            if any(r["verdict"] == v for r in executed)},
        },
        "known_failures": known_summary,
        "verdict": verdict,
    }
    if verdict_reason:
        result["verdict_reason"] = verdict_reason
    return result


def write_outputs(out_root: Path, ci_result: dict, check_results: list[dict],
                  result_schema: dict) -> None:
    """写 per-check JSON、CI_RESULT.json（写前按控制包 ci_result.schema.json 自校验）。"""
    checks_dir = out_root / "checks"
    checks_dir.mkdir(parents=True, exist_ok=True)
    for r in check_results:
        path = checks_dir / f"{r['id']}.json"
        tmp = path.with_suffix(".json.tmp")
        tmp.write_text(json.dumps(r, ensure_ascii=False, indent=2, sort_keys=True), encoding="utf-8")
        tmp.replace(path)  # 原子覆盖：陈旧/伪造结果一律重算覆盖

    errors = validate_against_schema(ci_result, result_schema)
    if errors:
        raise RunnerError("生成的 CI_RESULT.json 不符合 ci/ci_result.schema.json：\n  "
                          + "\n  ".join(errors))
    path = out_root / "CI_RESULT.json"
    tmp = path.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(ci_result, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
                   encoding="utf-8")
    tmp.replace(path)


# ---------------------------------------------------------------------------
# CLI 主流程
# ---------------------------------------------------------------------------

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="ci/run.py",
        description="AstroCS 统一 CI 执行器：CI YAML 的唯一入口（V8-CI-002）。",
    )
    parser.add_argument("--profile", choices=PROFILES, default="fast",
                        help="检查 profile（默认 fast）")
    parser.add_argument("--check", action="append", default=[],
                        help="显式指定检查 ID（可多次；优先于 profile 选择）")
    parser.add_argument("--focus", default=None,
                        help="聚焦 TASK-ID 或路径前缀（按检查 id 或 changed_paths 前缀过滤）")
    parser.add_argument("--changed-from", default=None, dest="changed_from",
                        help="以 <ref>..HEAD 及工作区未提交变更经 ci/impact_map.json 过滤")
    parser.add_argument("--plan-only", action="store_true",
                        help="只打印选中检查清单 JSON，不执行（hosted 预检/windows profile 验证）")
    parser.add_argument("--output-root", default=None, dest="output_root",
                        help="结果输出目录（默认 artifacts/ci/<sha12>/<run-id>/）")
    parser.add_argument("--strict-workspace", default=True,
                        type=lambda v: str(v).strip().lower() not in {"0", "false", "no"},
                        help="mutates_workspace=false 检查执行前后对比 git status（默认开启）")
    parser.add_argument("--platform", default="auto",
                        choices=("auto", "linux", "windows", "fatduck"),
                        help="运行平台覆盖（默认自动检测；fatduck harness 显式声明）")
    parser.add_argument("--registry", default=None,
                        help="注册表路径覆盖（默认 ci/checks.json；供单元测试注入 fixture）")
    parser.add_argument("--impact-map", default=None, dest="impact_map",
                        help="impact_map 路径覆盖（默认 ci/impact_map.json；供单元测试注入）")
    parser.add_argument("--repo-root", default=None, dest="repo_root",
                        help="仓库根覆盖（默认由脚本位置推导；供单元测试注入 /tmp fixture 仓库）")
    parser.add_argument("--known-failures", default=None, dest="known_failures",
                        help="known_failures 基线路径覆盖（默认 ci/known_failures.json）")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    started = utc_now()

    repo = Path(args.repo_root).resolve() if args.repo_root else \
        Path(__file__).resolve().parent.parent
    if not (repo / ".git").exists() and os.environ.get("ASTROCS_CI_ALLOW_NO_GIT") != "1":
        print(f"ci/run.py：仓库根解析失败（{repo} 无 .git）", file=sys.stderr)
        return EXIT_RUNNER_ERROR

    try:
        source_sha = git(repo, "rev-parse", "HEAD").strip()
        if not re.fullmatch(r"[0-9a-f]{40}", source_sha):
            raise RunnerError(f"HEAD SHA 异常：{source_sha!r}")

        registry_path = Path(args.registry) if args.registry else repo / "ci" / "checks.json"
        registry = load_registry(repo, registry_path)
        impact = load_impact_map(
            Path(args.impact_map) if args.impact_map else repo / "ci" / "impact_map.json"
        )
        selected, selected_meta = select_checks(
            registry, profile=args.profile, check_args=list(args.check), focus=args.focus,
            changed_from=args.changed_from, impact=impact, repo=repo,
        )
        selected_meta["run_id"] = (
            utc_now().strftime("%Y%m%dT%H%M%SZ") + "-" + uuid.uuid4().hex[:8]
        )

        if args.plan_only:
            plan = {
                "schema_version": SCHEMA_VERSION,
                "mode": "plan-only",
                "profile": args.profile,
                "source_sha": source_sha,
                "platform_runtime": current_platform(args.platform),
                "changed_from": args.changed_from,
                "focus": args.focus,
                "selection_mode": selected_meta["mode"],
                "selected_count": len(selected),
                "checks": [
                    {
                        "id": c["id"],
                        "platform": c["platform"],
                        "timeout_seconds": c["timeout_seconds"],
                        "heavy": c["heavy"],
                        "mutates_workspace": c["mutates_workspace"],
                        "waivable": c["waivable"],
                        "requires_monitor": c.get("requires_monitor", False),
                        "selected_by": selected_meta["selected_by"].get(c["id"]),
                        "command": c["command"],
                    }
                    for c in selected
                ],
            }
            print(json.dumps(plan, ensure_ascii=False, indent=2))
            return EXIT_OK

        if args.output_root:
            out_root = Path(args.output_root)
            if not out_root.is_absolute():
                out_root = repo / out_root
        else:
            out_root = repo / "artifacts" / "ci" / source_sha[:12] / selected_meta["run_id"]
        out_root.mkdir(parents=True, exist_ok=True)
        (out_root / "logs").mkdir(parents=True, exist_ok=True)
        (out_root / "checks").mkdir(parents=True, exist_ok=True)

        platform = current_platform(args.platform)
        known_path = (Path(args.known_failures) if args.known_failures
                      else repo / "ci" / "known_failures.json")
        kf_entries, kf_errors = load_known_failures(known_path)

        check_results = [
            execute_check(check, repo, out_root, platform, args.strict_workspace)
            for check in selected
        ]
        check_results, known_summary = apply_known_failures(
            kf_entries, kf_errors, check_results, utc_now(), out_root
        )
        known_summary["baseline_path"] = (str(known_path) if known_path.is_file() else None)

        ci_result = build_ci_result(
            repo=repo, profile=args.profile, selected_meta=selected_meta,
            check_results=check_results, started=started,
            strict_workspace=args.strict_workspace, changed_from=args.changed_from,
            known_summary=known_summary,
        )

        result_schema_path = repo / "ci" / "ci_result.schema.json"
        if result_schema_path.is_file():
            result_schema = load_json(result_schema_path)
        else:
            raise RunnerError("ci/ci_result.schema.json 缺失（控制包结果 schema 必须对齐在位）")
        write_outputs(out_root, ci_result, check_results, result_schema)

        summary_line = (
            f"verdict={ci_result['verdict']} total={ci_result['summary']['total']} "
            f"pass={ci_result['summary']['pass']} fail={ci_result['summary']['fail']} "
            f"known_fail={ci_result['summary']['known_fail']} "
            f"skipped_waivable={ci_result['summary']['skipped_waivable']}"
        )
        print(summary_line)
        print(f"result: {out_root / 'CI_RESULT.json'}")
        return EXIT_OK if ci_result["verdict"] in ("PASS", "FATDUCK_PENDING") else EXIT_FAIL
    except RunnerError as exc:
        print(f"ci/run.py：{exc}", file=sys.stderr)
        return EXIT_RUNNER_ERROR


if __name__ == "__main__":
    sys.exit(main())
