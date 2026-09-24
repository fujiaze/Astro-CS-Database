#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/run_checks.py — 新规范机器检查入口（控制包任务 CI-001）。

权威依据
  - docs/ci/01_CHECKS.md §1（eng/ci/checks.json 唯一注册表；eng/ci/ 提供确定性执行器）、
    §5 运行方式（--all / --check <ID...> / --all --json-out <path>）；
  - docs/ci/CI_SPEC.md §4（P0/P1 必须 0）、§7（失败必须留可复现证据）；
  - ENGINEERING_SPEC.md §8（唯一注册表；确定性执行器；每项能绿能红）。

本入口的合同（与 eng/ci/run.py 并存：工作流暂仍调用 eng/ci/run.py，QA-001 之后再改绑定）
  0. 范围（CI-INCREMENTAL；正本 docs/ci/CI_SPEC.md §2）：
       --changed（**默认**）= 影响面增量：改动集 = git diff --name-only <--base>
         ∪ git status --porcelain（未提交，含未跟踪），只跑 changed_paths 与之
         相交的 step；受影响的构建/测试 target 由构建图反查（eng/ci/incremental.py）。
       --all / --full = 整档全量（**必须显式**）。
       --check <ID...> = 点名（explicit）。
     结果 JSON 顶层 scope ∈ {changed, full, explicit}；禁止"看起来像全量其实不是"。
     fail-closed 三条（CI_SPEC.md §2.4）：
       a) 改动集含不匹配任何 changed_paths 的文件 ⇒ 判红 UNCOVERED_CHANGED_PATHS；
       b) 改动命中全局敏感面 ⇒ 自动升级全量并打印 escalated_to_full；
       c) 改动集非空而选中数为 0 ⇒ 判红 EMPTY_SELECTION（改动集为空 ⇒ no_changes，rc=0）。
     --explain 逐条打印选中/跳过与依据；--self-test 跑三条负例（必须判红）。
  1. 选择：--all（注册表全量，受 --profile 限定；默认 fast，与 eng/ci/run.py 的
     profile 语义一致）或 --check <ID...>（可给聚合项 ID，也可给其 step 的
     旧 ID；未登记的 ID 一律 runner error rc=2，绝不静默忽略）；
  2. 确定性：注册表顺序 = 执行顺序；串行执行；不使用随机数/并发；同一注册表
     与同一 profile 下选中集与执行序列完全可复现；
  3. 每项显式超时：timeout 只来自注册表（step.timeout_seconds 或父项
     timeout_seconds）；缺失/非法即 runner error rc=2（禁止无限等待）；
  4. 机器可读 JSON（--json-out，原子写）：含 run 元数据、注册表 sha256、
     选择信息、汇总（entries/steps/passed/failed/timeout/skipped/verdict）与
     逐项 rc、耗时、超时值、stdout/stderr 摘要；同时落 per-step 结果
     <run_root>/checks/<step-id>.json（与 eng/ci/run.py 同布局，供聚合型
     known-failures 门读取同 run 上游结果）；
  5. 退出码：0=全部 PASS（允许显式平台/waiver SKIP，均计数并落盘）、
     1=存在 FAIL/TIMEOUT、2=runner 配置/环境错误（注册表非法、ID 未登记、
     timeout 缺失等）。

聚合项（CI-001 ID 收敛）登记结构
  {
    "id": "CHK-UNIT", ...,
    "command": <steps[0].command 的等价副本，供旧消费者兼容>,
    "timeout_seconds": <各 step 超时之和>,
    "steps": [ {"id": "UT-API", "command": [...], "timeout_seconds": 300,
                "profiles": [...], "platform": "any", ...}, ... ]
  }
  每个 step 是自描述的最小检查单元（旧注册 ID 原样保留为 step id）；
  无 steps 的旧式单项注册表条目照旧执行（向后兼容，未收敛前即可用）。

用法
  python3 eng/ci/run_checks.py --all
  python3 eng/ci/run_checks.py --all --json-out run/PROJECT-GOVERNANCE-01/CI-001/logs/ci_result.json
  python3 eng/ci/run_checks.py --check CHK-MODULE-MANIFEST --check UT-API
  python3 eng/ci/run_checks.py --all --profile linux-main --plan-only
"""
from __future__ import annotations

import argparse
import datetime as _dt
import hashlib
import importlib.util
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import incremental as _inc  # noqa: E402  (eng/ci/incremental.py：范围计算正本)
import monitor_evidence as _mon_ev  # noqa: E402  (eng/ci/monitor_evidence.py：证据判定单一实现点)

SCHEMA_VERSION = 1
RUNNER = "eng/ci/run_checks.py"
# prerelease：负责人手动触发的一次性重步骤档（CI_SPEC.md §2.6）。真实数据 E2E、
# L2 性能、sanitizer、coverage、nwoker/invariant 只在此档，不得留在常跑档。
PROFILES = ("fast", "integration", "linux-main", "windows-main",
            "linux-deep", "prerelease", "fatduck")
ALLOWED_PLATFORM = ("any", "linux", "windows", "fatduck")

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_RUNNER_ERROR = 2
# GATE-TRIAGE-01：**崩溃换独立退出码**。检查器抛未捕获异常（Traceback）时它
# 没有给出 verdict —— 这与「检查器给出 FAIL 判词」是两件不同的事：前者说明
# 门本身不可信（红绿都可能是噪声），后者说明被判对象不合规。
# 旧行为把两者都记 rc=1 ⇒ 无法区分「门崩」与「判红」（人工直跑时尤其致命）。
EXIT_CRASH = 3

# ctest SKIP_RETURN_CODE 同语义（77）：宿主能力门（如 AVX-512 不可用）以 77
# 表示"合同化跳过"，不是失败。与 eng/ci/run.py 的 SKIP_EXIT_CODE 同合同（W4-A3：
# 两入口对同一退出码的判定必须一致，否则同一 step 在两个入口一红一绿）。
# **只在执行单元 waivable=true 时被承认**；非 waivable 的 77 = 自我豁免 ⇒ FAIL。
SKIP_EXIT_CODE = 77

TAIL_LIMIT = 4000  # per-step stdout/stderr 摘要上限（字符）
DEFAULT_RUN_ROOT = "run/ci/run-checks"

V_PASS = "PASS"
V_FAIL = "FAIL"
V_TIMEOUT = "TIMEOUT"
# 三态之三：检查器崩（未捕获异常 / 无 verdict）。与 V_FAIL 严格区分。
V_CRASH = "CRASH"
V_SKIP_PLATFORM = "SKIPPED(platform)"
V_SKIP_WAIVABLE = "SKIPPED(waivable)"
V_PREREQ = "FAIL(prerequisite)"
V_REUSED = "PASS(reused_fingerprint)"
V_SCOPE = "FAIL(scope)"
# GATE-501（D-12 / ENGINEERING_SPEC §10 fail-closed）：exit 0 只是必要条件，
# 不是充分条件。执行单元还必须兑现内容级证据面：
#   V_MISSING_OUTPUT  登记 outputs 在执行后不存在（"文件不存在按无违规通过"= 假绿）
#   V_EMPTY_OUTPUT    outputs 为空且 stdout/stderr 全空（静默失败不可发现）
#   V_GATE_MISSING    requires_monitor=true 却无监控证据 / 证据违反 L2 冻结判据
V_MISSING_OUTPUT = "FAIL(missing_output)"
V_EMPTY_OUTPUT = "FAIL(empty_outputs)"
V_GATE_MISSING = "FAIL(monitor_gate_missing)"
FAIL_VERDICTS = (V_FAIL, V_TIMEOUT, V_PREREQ, V_SCOPE,
                 V_MISSING_OUTPUT, V_EMPTY_OUTPUT, V_GATE_MISSING, V_CRASH)

# outputs 为空且按设计静默成功的执行单元（显式登记，不设全局兜底）；
# 新增检查不得进入本表（新防线要求留痕或登记 waivable）。
SILENT_OK_UNITS = frozenset({
    "API-DOCS",        # eng/tools/check_api_docs.py：rc=0 静默成功
    "UNIT-CLOSURE",    # eng/tools/check_unit_closure.py：rc=0 静默成功
})

# fail-closed 判据 ID（CI_SPEC.md §2.4）；命中即判红，不允许静默跳过。
SCOPE_UNCOVERED = "UNCOVERED_CHANGED_PATHS"
SCOPE_EMPTY = "EMPTY_SELECTION"
SCOPE_BUDGET = "BUDGET_EXCEEDED"

DEFAULT_INCREMENTAL_BUDGET_SECONDS = 120

# step 从父项继承的字段（缺失即继承；显式给出即覆盖）
INHERIT_FIELDS = ("command", "profiles", "platform", "timeout_seconds", "heavy",
                  "mutates_workspace", "outputs", "waivable",
                  "requires_monitor", "prerequisite_tools", "changed_paths",
                  "dirty_ignore_exact", "dirty_ignore_prefixes", "fingerprint")


class RunnerError(Exception):
    """runner 自身配置/环境错误（rc=2），与检查失败（rc=1）严格区分。"""


def utc_now() -> _dt.datetime:
    return _dt.datetime.now(_dt.timezone.utc)


def utc_iso(dt: _dt.datetime) -> str:
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 16), b""):
            h.update(chunk)
    return h.hexdigest()


def git_head(repo: Path) -> str:
    """当前 HEAD SHA（指纹输入之一）；不可用时返回 'unknown'（不阻断执行）。"""
    try:
        return subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(repo),
                              stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                              timeout=60).stdout.decode("utf-8", "replace").strip() or "unknown"
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def current_platform(override: str) -> str:
    if override != "auto":
        return override
    if sys.platform.startswith("win"):
        return "windows"
    return "linux"


def load_registry(path: Path) -> dict:
    if not path.is_file():
        raise RunnerError(f"注册表不存在：{path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # noqa: BLE001 - 任何解析失败都是 runner error
        raise RunnerError(f"注册表无法解析：{path}: {exc}") from exc
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        raise RunnerError(f"注册表顶层非法（需 schema_version=1 的对象）：{path}")
    checks = data.get("checks")
    if not isinstance(checks, list) or not checks:
        raise RunnerError(f"注册表 checks 必须是非空数组：{path}")
    seen: set[str] = set()
    for i, c in enumerate(checks):
        if not isinstance(c, dict) or not isinstance(c.get("id"), str) or not c["id"]:
            raise RunnerError(f"checks[{i}] 缺 id")
        if c["id"] in seen:
            raise RunnerError(f"重复 id：{c['id']}")
        seen.add(c["id"])
    return data


def expand_steps(entry: dict, *, where: str) -> list[dict]:
    """把注册项展开为自描述 step 列表（无 steps 时即该项自身）。"""
    raw_steps = entry.get("steps")
    if raw_steps is None:
        raw_steps = [{}]
    if not isinstance(raw_steps, list) or not raw_steps:
        raise RunnerError(f"{where}: steps 必须是非空数组")
    steps: list[dict] = []
    for j, raw in enumerate(raw_steps):
        if not isinstance(raw, dict):
            raise RunnerError(f"{where}.steps[{j}] 必须是对象")
        merged: dict = {}
        for field in INHERIT_FIELDS:
            if field in raw:
                merged[field] = raw[field]
            elif field in entry:
                merged[field] = entry[field]
        sid = raw.get("id", entry["id"])
        if not isinstance(sid, str) or not sid:
            raise RunnerError(f"{where}.steps[{j}]: id 非法")
        merged["id"] = sid
        merged["parent_id"] = entry["id"]
        cmd = merged.get("command")
        if not isinstance(cmd, list) or not cmd or not all(isinstance(x, str) and x for x in cmd):
            raise RunnerError(f"{where}.steps[{j}] ({sid}): command 必须是非空字符串数组")
        to = merged.get("timeout_seconds")
        if not isinstance(to, int) or isinstance(to, bool) or to < 1:
            raise RunnerError(
                f"{where}.steps[{j}] ({sid}): timeout_seconds 必须显式给出且为 >=1 的整数"
                "（禁止无超时执行）")
        prof = merged.get("profiles")
        if not isinstance(prof, list) or not prof or any(p not in PROFILES for p in prof):
            raise RunnerError(f"{where}.steps[{j}] ({sid}): profiles 非法 {prof!r}")
        plat = merged.get("platform", "any")
        if plat not in ALLOWED_PLATFORM:
            raise RunnerError(f"{where}.steps[{j}] ({sid}): platform 非法 {plat!r}")
        merged["platform"] = plat
        steps.append(merged)
    ids = [s["id"] for s in steps]
    if len(set(ids)) != len(ids):
        raise RunnerError(f"{where}: step id 重复 {ids}")
    return steps


def index_steps(registry: dict) -> tuple[list[dict], dict[str, str], dict[str, dict]]:
    """展开全注册表；返回 (按注册表顺序的 step 列表, step_id→parent_id, 冲突表)。"""
    steps: list[dict] = []
    owner: dict[str, str] = {}
    dupes: dict[str, str] = {}
    for c in registry["checks"]:
        entry_steps = expand_steps(c, where=c["id"])
        for s in entry_steps:
            if s["id"] in owner:
                dupes[s["id"]] = owner[s["id"]]
            owner[s["id"]] = c["id"]
            steps.append(s)
    return steps, owner, dupes


def probe_module(mod: str) -> tuple[bool, str | None]:
    try:
        found = importlib.util.find_spec(mod) is not None
    except (ImportError, ValueError):
        found = False
    return (True, None) if found else (False, f"python 模块缺失: {mod}")


def probe_prerequisite(step: dict) -> tuple[bool, str | None]:
    """prerequisite_tools 探测：<exe> 经 shutil.which；<exe>:<module> 探 python 模块。"""
    for tool in step.get("prerequisite_tools", []) or []:
        if ":" in tool:
            exe, mod = tool.split(":", 1)
            ok, reason = probe_module(mod)
            if not ok:
                return ok, reason
        else:
            if shutil.which(tool) is None:
                return False, f"外部工具缺失: {tool}"
    return True, None


def tail(text: str, limit: int = TAIL_LIMIT) -> str:
    return text if len(text) <= limit else text[-limit:]


def _terminate(process: subprocess.Popen) -> None:
    try:
        if os.name == "posix":
            import errno
            try:
                os.killpg(os.getpgid(process.pid), signal.SIGKILL)
                return
            except OSError as exc:
                if exc.errno != errno.ESRCH:
                    process.kill()
        else:  # pragma: no cover - Windows 路径
            process.kill()
    except Exception:  # noqa: BLE001
        try:
            process.kill()
        except OSError:
            pass


def evidence_verdict(step: dict, repo: Path, stdout_tail: str,
                    stderr_tail: str) -> tuple[str, str] | None:
    """exit 0 之后的内容级证据判定（fail-closed；None = 证据齐备）。

    三重证据面（ENGINEERING_SPEC §10 / docs/ci/CI_SPEC.md §9）：
      1. 登记 outputs 必须存在（缺失 ⇒ FAIL(missing_output)）——"文件不存在
         按无违规通过"是假绿；
      2. outputs 为空且 stdout/stderr 全空 ⇒ FAIL(empty_outputs)（静默失败
         不可发现；显式登记的 SILENT_OK_UNITS 除外）；
      3. requires_monitor=true ⇒ 必须兑现监控证据，证据含 frozen_gate 时四条
         L2 冻结判据按 fail-closed 复核（违规 ⇒ FAIL(monitor_gate_missing)）。
    """
    missing = [rel for rel in (step.get("outputs") or [])
               if not _mon_ev.path_exists(repo, str(rel))]
    if missing:
        return (V_MISSING_OUTPUT, f"exit 0 但登记输出缺失：{missing}")
    if (not step.get("outputs") and not step.get("waivable")
            and step["id"] not in SILENT_OK_UNITS
            and not stdout_tail and not stderr_tail):
        return (V_EMPTY_OUTPUT,
                "exit 0 且 stdout/stderr 均为空：空 outputs 单元无任何内容级证据"
                "（静默失败不可发现）；如架构上必须静默，请登记 waivable 或留痕")
    gap = _mon_ev.monitor_evidence_gap(step, repo)
    if gap is not None:
        return (V_GATE_MISSING, gap)
    return None


# --------------------------------------------------------------- 门崩识别 ----
# 判据（保守，避免误伤）：输出里出现 Python 未捕获异常的 Traceback 头，且**没有**
# 任何结构化 verdict 标记 ⇒ 该执行单元**没有给出判定**，记 CRASH（独立退出码 3）。
# 为什么不是「有 Traceback 就算崩」：unittest 的失败报告里也有 Traceback，
# 那种情况检查器（测试驱动器）**确实**给出了 FAIL 判定，必须留在 FAIL 档。
# 崩溃证据全文照录（不截断）：崩溃的第一现场就是栈帧，截断等于让读者无法定位。
CRASH_TRACEBACK_RE = re.compile(r"Traceback \(most recent call last\)")
VERDICT_TOKEN_RE = re.compile(
    r"(?:verdict|VERDICT|FAIL|PASS|_FAIL|_PASS|_RED|_OK|_GREEN)\s*[:=]")
CRASH_TAIL_LIMIT = 20000  # 崩溃证据留 20k 字符（远大于 TAIL_LIMIT，够放完整栈）


def crash_site(blob: str) -> str:
    """从 Traceback 文本里抽「最后一帧的文件:行 + 异常行」，供判词直接定位。"""
    lines = [ln.rstrip() for ln in blob.splitlines()]
    frames = [ln.strip() for ln in lines if ln.strip().startswith("File \"")]
    exc = ""
    for ln in reversed(lines):
        s = ln.strip()
        if not s or s.startswith(("File \"", "Traceback", "[")):
            continue
        if re.match(r"^[A-Za-z_][\w.]*(?:Error|Exception|Warning|Interrupt|Exit)\b", s) \
                or re.match(r"^[A-Za-z_][\w.]*:\s", s):
            exc = s
            break
    site = frames[-1] if frames else "(无栈帧)"
    return "%s | %s" % (site[:400], exc[:400])


def looks_like_crash(stdout_s: str, stderr_s: str) -> bool:
    """True = 检查器抛了未捕获异常且没给 verdict（门崩，不是判红）。"""
    blob = stdout_s + "\n" + stderr_s
    if not CRASH_TRACEBACK_RE.search(blob):
        return False
    # 结构化 verdict 标记（JSON "verdict": "FAIL" / TOOL_FAIL: / ...）⇒ 门有判定
    return not VERDICT_TOKEN_RE.search(blob)


def execute_step(step: dict, repo: Path, run_root: Path, platform: str) -> dict:
    sid = step["id"]
    started = utc_now()
    result: dict = {
        "id": sid,
        "parent_id": step.get("parent_id"),
        "command": list(step["command"]),
        "timeout_seconds": int(step["timeout_seconds"]),
        "platform": step["platform"],
        "profiles": list(step["profiles"]),
        "waivable": bool(step.get("waivable", False)),
        "heavy": bool(step.get("heavy", False)),
        "mutates_workspace": bool(step.get("mutates_workspace", False)),
        "requires_monitor": bool(step.get("requires_monitor", False)),
        "outputs": list(step.get("outputs", []) or []),
        "started_utc": utc_iso(started),
        "finished_utc": None,
        "duration_seconds": 0.0,
        "exit_code": None,
        "timed_out": False,
        "signal": None,
        "stdout_tail": "",
        "stderr_tail": "",
        "stdout_lines": 0,
        "stderr_lines": 0,
        "reason": None,
        "fingerprint": None,
        "verdict": None,
    }

    def finish(verdict: str, reason: str | None = None) -> dict:
        finished = utc_now()
        result["finished_utc"] = utc_iso(finished)
        result["duration_seconds"] = round((finished - started).total_seconds(), 3)
        result["verdict"] = verdict
        result["reason"] = reason
        write_per_step_result(run_root, result)
        return result

    if step["platform"] != "any" and step["platform"] != platform:
        return finish(V_SKIP_PLATFORM,
                      f"platform 登记={step['platform']} 运行={platform}（显式跳过并计数）")

    ok, reason = probe_prerequisite(step)
    if not ok:
        if step.get("waivable"):
            return finish(V_SKIP_WAIVABLE, f"prerequisite 未满足（waivable）：{reason}")
        return finish(V_PREREQ, f"prerequisite 未满足：{reason}")

    # 输入指纹缓存（CI_SPEC.md §2.6）：prerelease 重步骤"只跑一次"。
    # 指纹命中 ⇒ 跳过执行并复用归档（打印 reused_fingerprint）；指纹不可用/写入失败
    # ⇒ 照常执行（fail-closed：宁可多跑，不得因缓存故障而跳过判定）。
    fp_cfg = step.get("fingerprint")
    fp = None
    if isinstance(fp_cfg, dict):
        try:
            fp = _inc.input_fingerprint(
                repo,
                commit=fp_cfg.get("commit") or git_head(repo),
                config_paths=fp_cfg.get("config", []),
                data_paths=fp_cfg.get("data", []),
                workers=fp_cfg.get("workers"),
                extra=fp_cfg.get("extra"),
            )
            archive = repo / str(fp_cfg.get("archive", ""))
            if _inc.fingerprint_hit(archive, fp):
                # fail-closed：指纹命中只说明"输入相同"，复用前还必须确认归档证据
                # 仍在（登记 outputs / 监控证据齐备）；归档缺失则照常执行，不得
                # 以"复用"为名跳过判定。
                reuse_gap = evidence_verdict(
                    step, repo, f"reused_fingerprint {fp['sha256'][:16]}", "")
                if reuse_gap is None:
                    result["fingerprint"] = fp["sha256"]
                    return finish(V_REUSED,
                                  f"reused_fingerprint {fp['sha256'][:16]}：输入指纹命中，"
                                  f"复用归档 {fp_cfg.get('archive')}")
                result["fingerprint_error"] = (
                    f"指纹命中但归档证据缺失（{reuse_gap[1]}）→ 照常执行")
        except Exception as exc:  # noqa: BLE001 - 缓存故障不得导致跳过
            fp = None
            result["fingerprint_error"] = f"指纹计算失败（照常执行）：{exc}"

    env = dict(os.environ)
    env.setdefault("PYTHONIOENCODING", "utf-8")
    env.setdefault("PYTHONUTF8", "1")
    env["ASTROCS_CI_CHECK_ID"] = sid
    env["ASTROCS_CI_OUT_ROOT"] = str(run_root)

    try:
        proc = subprocess.Popen(
            step["command"], cwd=str(repo), env=env,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            shell=False, start_new_session=(os.name == "posix"),
        )
    except FileNotFoundError as exc:
        return finish(V_PREREQ, f"无法启动命令：{exc}")

    timed_out = False
    try:
        stdout_b, stderr_b = proc.communicate(timeout=step["timeout_seconds"])
    except subprocess.TimeoutExpired:
        timed_out = True
        _terminate(proc)
        stdout_b, stderr_b = proc.communicate()

    stdout_s = stdout_b.decode("utf-8", "replace")
    stderr_s = stderr_b.decode("utf-8", "replace")
    crash = looks_like_crash(stdout_s, stderr_s)
    # 崩溃证据**不按 TAIL_LIMIT 截断**（GATE-TRIAGE-01）：栈帧就是第一现场。
    result["stdout_tail"] = tail(stdout_s, CRASH_TAIL_LIMIT if crash else TAIL_LIMIT)
    result["stderr_tail"] = tail(stderr_s, CRASH_TAIL_LIMIT if crash else TAIL_LIMIT)
    result["stdout_lines"] = stdout_s.count("\n")
    result["stderr_lines"] = stderr_s.count("\n")
    result["exit_code"] = proc.returncode
    result["timed_out"] = timed_out
    result["crash"] = crash
    if crash and proc.returncode != 0:
        # 门崩：检查器没有给出 verdict。判词直接给出可定位的栈尾（文件名:行号）。
        return finish(V_CRASH, "检查器抛未捕获异常（无 verdict，门不可信）："
                               + crash_site(stdout_s + "\n" + stderr_s))
    if timed_out:
        return finish(V_TIMEOUT, f"超过登记超时 {step['timeout_seconds']}s 被终止")
    if proc.returncode is not None and proc.returncode < 0:
        result["signal"] = -proc.returncode
        return finish(V_FAIL, f"被信号终止：{-proc.returncode}")
    if proc.returncode == 0:
        if fp is not None and isinstance(fp_cfg, dict):
            try:
                _inc.write_fingerprint(repo / str(fp_cfg.get("archive", "")), fp)
                result["fingerprint"] = fp["sha256"]
            except OSError as exc:
                result["fingerprint_error"] = f"指纹写入失败：{exc}"
        # GATE-501：exit 0 不是充分条件——必须兑现内容级证据面（fail-closed）
        gap = evidence_verdict(step, repo, result["stdout_tail"], result["stderr_tail"])
        if gap is not None:
            return finish(gap[0], gap[1])
        return finish(V_PASS)
    if proc.returncode == SKIP_EXIT_CODE:
        # 合同化 skip（ctest SKIP_RETURN_CODE=77 同语义）**只对 waivable 执行单元有效**：
        # 非 waivable 的单元以 77 退出 = 自我豁免（fail-open），任何检查器都能
        # 用 sys.exit(77) 让整门变绿 ⇒ 按 FAIL 记（ENGINEERING_SPEC §8 fail-closed：
        # 不得把"未执行/未判定"当通过）。口径与 eng/ci/run.py 的 SKIP_EXIT_CODE 分支一致
        # （W4-A3：两入口对同一退出码的判定必须一致）。
        if step.get("waivable"):
            return finish(V_SKIP_WAIVABLE,
                          f"命令以 SKIP 退出码 {SKIP_EXIT_CODE} 结束"
                          "（ctest SKIP_RETURN_CODE 同语义, 如宿主能力 gate）")
        return finish(V_FAIL,
                      f"非 waivable 执行单元以 SKIP 退出码 {SKIP_EXIT_CODE} 结束："
                      "合同化 SKIP 仅适用 waivable 单元（fail-closed）")
    return finish(V_FAIL, f"exit code {proc.returncode}")


def write_per_step_result(run_root: Path, result: dict) -> None:
    """per-step 结果原子落盘（<run_root>/checks/<id>.json），供聚合门读同 run 上游。"""
    checks_dir = run_root / "checks"
    checks_dir.mkdir(parents=True, exist_ok=True)
    target = checks_dir / f"{result['id']}.json"
    tmp = target.with_suffix(".json.tmp")
    tmp.write_text(json.dumps(result, ensure_ascii=False, indent=2, sort_keys=True),
                   encoding="utf-8")
    os.replace(tmp, target)


# ---------------------------------------------------------------------------
# 调度器（CI-INCREMENTAL：独占道 / 并行道 / 末位屏障）
# ---------------------------------------------------------------------------
# 设计依据（docs/ci/CI_SPEC.md §2.5 超时预算 + 本任务"少跑不需要跑的、不放松判据"）：
#   1. 独占道：**命令真的起了资源监控**的 step（CPU 利用率测量不能被并发污染）；
#      以及 **mutates_workspace=true 且声明了 run/ 之外的产物**的 step（真写跟踪树）。
#   2. 并行道：其余 step，并发度默认 min(8, cpu_count)，--jobs 覆盖；--serial 一键回串行。
#   3. 写冲突：两个 step 的**资源键**相交则不得重叠执行。资源键 = 声明 outputs
#      ∪ --build-dir/--test-dir（CTest 会在该目录写 Testing/Temporary/*）
#      ∪ --work-dir。
#   4. 末位屏障：reads_run_results=true 的 step（聚合门读同 run 上游结果）排在最后串行。
# 注意：requires_monitor 本身**不**作为独占依据 —— 见 run_checks.py 头部说明与
#   eng/ci/run.py:109 monitor_gate_requested（只有命令请求 --gate-required 才有判定，
#   只有命令起 resource_monitor.py 才有采样，才存在"测量被污染"这回事）。
MONITOR_CMDS = ("resource_monitor.py", "run_monitored.py")


def step_is_exclusive(step: dict) -> tuple:
    """返回 (是否独占, 原因)。判据见本节头部说明。"""
    cmd = " ".join(step.get("command") or [])
    for marker in MONITOR_CMDS:
        if marker in cmd:
            return True, f"exclusive:命令起资源监控（{marker}），CPU 测量须独占"
    outside = [o for o in (step.get("outputs") or []) if not o.startswith("run/")]
    if step.get("mutates_workspace") and outside:
        return True, ("exclusive:mutates_workspace=true 且声明跟踪树产物 "
                      + ",".join(outside[:2]))
    return False, "parallel"


def _opt_values(cmd: list, names) -> list:
    out = []
    for i, tok in enumerate(cmd):
        for name in names:
            if tok == name and i + 1 < len(cmd):
                out.append(cmd[i + 1])
            elif tok.startswith(name + "="):
                out.append(tok.split("=", 1)[1])
    return out


def step_resource_keys(step: dict) -> set:
    keys = {"output:" + o for o in (step.get("outputs") or [])}
    cmd = step.get("command") or []
    for v in _opt_values(cmd, ("--build-dir", "--test-dir")):
        keys.add("builddir:" + v)
    for v in _opt_values(cmd, ("--work-dir",)):
        keys.add("workdir:" + v)
    return keys


def plan_lanes(steps: list) -> list:
    """静态分道（--explain 用；不执行）。"""
    rows = []
    for s in steps:
        if s.get("reads_run_results"):
            rows.append({"id": s["id"], "lane": "barrier",
                         "reason": "reads_run_results：聚合门读同 run 上游结果，末位串行"})
            continue
        excl, why = step_is_exclusive(s)
        rows.append({"id": s["id"], "lane": "exclusive" if excl else "parallel",
                     "reason": why,
                     "resource_keys": sorted(step_resource_keys(s))[:6]})
    return rows


def run_scheduled(steps: list, repo: Path, run_root: Path, platform: str, *,
                  jobs: int, serial: bool, executor=None) -> tuple:
    """按独占/并行/屏障三道执行；返回 (按注册表顺序的结果列表, 调度计划)。

    executor 可注入（--self-test 用假执行体证明"独占不被并发""写冲突被串行化"）。
    """
    run_one = executor or execute_step
    order = [s["id"] for s in steps]
    barrier = [s for s in steps if s.get("reads_run_results")]
    normal = [s for s in steps if not s.get("reads_run_results")]
    plan = {"mode": "serial" if (serial or jobs <= 1) else "parallel",
            "jobs": max(1, jobs), "serial_requested": bool(serial),
            "exclusive": [], "parallel": [], "serialized_by_resource": [],
            "barrier": [s["id"] for s in barrier]}
    results: dict = {}

    if plan["mode"] == "serial":
        for s in normal + barrier:
            results[s["id"]] = run_one(s, repo, run_root, platform)
        return [results[i] for i in order], plan

    import concurrent.futures as _cf
    pool = _cf.ThreadPoolExecutor(max_workers=plan["jobs"])
    inflight: dict = {}    # future -> (step, resource_keys)
    key_owner: dict = {}   # resource key -> {future}

    def collect(fut) -> None:
        step, keys = inflight.pop(fut)
        for k in keys:
            owners = key_owner.get(k)
            if owners is not None:
                owners.discard(fut)
        results[step["id"]] = fut.result()

    def drain() -> None:
        for fut in list(inflight):
            collect(fut)

    def wait_for_keys(keys) -> bool:
        """等所有与 keys 相交的在飞 step 结束；返回是否真的等过（=被串行化）。"""
        waited = False
        while True:
            conflicts = set()
            for k in keys:
                conflicts |= key_owner.get(k, set())
            if not conflicts:
                return waited
            waited = True
            done, _pending = _cf.wait(conflicts,
                                      return_when=_cf.FIRST_COMPLETED)
            for fut in done:
                if fut in inflight:
                    collect(fut)

    try:
        for s in normal:
            excl, why = step_is_exclusive(s)
            if excl:
                plan["exclusive"].append({"id": s["id"], "reason": why})
                drain()                      # 独占：先等所有在飞并行 step 结束
                results[s["id"]] = run_one(s, repo, run_root, platform)
                continue
            keys = step_resource_keys(s)
            blocked = wait_for_keys(keys)
            if blocked:
                plan["serialized_by_resource"].append(
                    {"id": s["id"], "keys": sorted(keys)[:4]})
            plan["parallel"].append(s["id"])
            fut = pool.submit(run_one, s, repo, run_root, platform)
            inflight[fut] = (s, keys)
            for k in keys:
                key_owner.setdefault(k, set()).add(fut)
        drain()
    finally:
        pool.shutdown(wait=True)

    for s in barrier:
        results[s["id"]] = run_one(s, repo, run_root, platform)
    return [results[i] for i in order], plan


def scheduler_self_test() -> list:
    """调度器负例面：证明"独占不被并发""写冲突被串行化""屏障最后跑"。

    用假执行体记录每个 step 的 [start, end) 区间，直接判区间是否相交。
    """
    import threading
    import time as _time
    cases: list = []
    lock = threading.Lock()
    spans: dict = {}

    def make_executor(sleep_s: float = 0.25):
        def _exec(step, _repo, _run_root, _platform):
            sid = step["id"]
            with lock:
                spans.setdefault(sid, []).append([_time.monotonic(), None])
                idx = len(spans[sid]) - 1
            _time.sleep(sleep_s)
            with lock:
                spans[sid][idx][1] = _time.monotonic()
            return {"id": sid, "parent_id": step.get("parent_id"), "verdict": V_PASS,
                    "exit_code": 0, "duration_seconds": sleep_s, "timeout_seconds": 1,
                    "platform": "linux", "profiles": ["fast"], "waivable": False,
                    "heavy": False, "mutates_workspace": False, "requires_monitor": False,
                    "outputs": [], "started_utc": "", "finished_utc": "", "timed_out": False,
                    "signal": None, "stdout_tail": "", "stderr_tail": "", "stdout_lines": 0,
                    "stderr_lines": 0, "fingerprint": None, "reason": None}
        return _exec

    def overlap(a, b) -> bool:
        return not (a[1] <= b[0] or b[1] <= a[0])

    def mk(sid, **kw):
        base = {"id": sid, "parent_id": "SELF-TEST", "command": ["python3", "-c", "pass"],
                "timeout_seconds": 1, "profiles": ["fast"], "platform": "any",
                "outputs": [], "waivable": False, "heavy": False,
                "mutates_workspace": False, "requires_monitor": False}
        base.update(kw)
        return base

    # S1 写冲突：两个 step 的 outputs 相交 ⇒ 必须串行（不得重叠）
    spans.clear()
    a = mk("S1-A", outputs=["run/ci/selftest/shared.json"])
    b = mk("S1-B", outputs=["run/ci/selftest/shared.json"])
    _res, plan = run_scheduled([a, b], Path("."), Path("."), "linux", jobs=4,
                               serial=False, executor=make_executor())
    ok = (len(spans.get("S1-A", [])) == 1 and len(spans.get("S1-B", [])) == 1
          and not overlap(spans["S1-A"][0], spans["S1-B"][0]))
    cases.append({"case": "S1_outputs_conflict_serialized", "ok": ok,
                  "serialized": plan["serialized_by_resource"],
                  "note": "outputs 相交的两个 step 区间不得重叠"})

    # S2 独占道：起资源监控的 step 不得与任何并行 step 重叠
    spans.clear()
    mon = mk("S2-MON", command=["python3", "eng/ci/resource_monitor.py", "--timeout", "1",
                                "--output", "run/ci/selftest/mon.json", "--",
                                "python3", "-c", "pass"])
    p1 = mk("S2-P1", outputs=["run/ci/selftest/p1.json"])
    p2 = mk("S2-P2", outputs=["run/ci/selftest/p2.json"])
    _res, plan = run_scheduled([p1, mon, p2], Path("."), Path("."), "linux", jobs=4,
                               serial=False, executor=make_executor())
    mon_span = spans["S2-MON"][0]
    bad = [sid for sid in ("S2-P1", "S2-P2")
           for sp in spans.get(sid, []) if overlap(mon_span, sp)]
    cases.append({"case": "S2_exclusive_lane_no_overlap", "ok": not bad,
                  "exclusive": plan["exclusive"], "overlapped": bad,
                  "note": "resource_monitor 步骤与并行步骤区间不得重叠"})

    # S3 屏障：reads_run_results=true 的 step 必须在所有其它 step 结束之后才开始
    spans.clear()
    n1 = mk("S3-N1", outputs=["run/ci/selftest/n1.json"])
    n2 = mk("S3-N2", outputs=["run/ci/selftest/n2.json"])
    bar = mk("S3-BAR", reads_run_results=True)
    _res, plan = run_scheduled([n1, n2, bar], Path("."), Path("."), "linux", jobs=4,
                               serial=False, executor=make_executor())
    bar_start = spans["S3-BAR"][0][0]
    early = [sid for sid in ("S3-N1", "S3-N2")
             if any(sp[1] > bar_start for sp in spans.get(sid, []))]
    cases.append({"case": "S3_barrier_runs_last", "ok": not early,
                  "barrier": plan["barrier"], "violations": early,
                  "note": "聚合门不得与上游 step 并发"})

    # S5 分道判据基于"命令是否真的起监控"，不基于 requires_monitor 标志：
    #    requires_monitor=False 但命令起 resource_monitor.py ⇒ 仍必须进独占道（被拒入并行道）
    spans.clear()
    mon2 = mk("S5-MON2", requires_monitor=False,
              command=["python3", "eng/ci/resource_monitor.py", "--timeout", "1",
                       "--output", "run/ci/selftest/mon2.json", "--",
                       "python3", "-c", "pass"])
    q1 = mk("S5-Q1", outputs=["run/ci/selftest/q1.json"])
    q2 = mk("S5-Q2", outputs=["run/ci/selftest/q2.json"])
    _res, plan = run_scheduled([q1, mon2, q2], Path("."), Path("."), "linux", jobs=4,
                               serial=False, executor=make_executor())
    lane = {r["id"]: r["lane"] for r in plan_lanes([q1, mon2, q2])}
    span = spans["S5-MON2"][0]
    bad = [sid for sid in ("S5-Q1", "S5-Q2")
           for sp in spans.get(sid, []) if overlap(span, sp)]
    cases.append({"case": "S5_monitor_step_refused_from_parallel_lane",
                  "ok": lane.get("S5-MON2") == "exclusive" and not bad,
                  "lanes": lane, "overlapped": bad,
                  "note": "分道看命令（真的起了监控），不看 requires_monitor 标志"})

    # S4 结果顺序仍按注册表顺序（确定性）
    spans.clear()
    seq = [mk(f"S4-{i}", outputs=[f"run/ci/selftest/s4-{i}.json"]) for i in range(5)]
    res, _plan = run_scheduled(seq, Path("."), Path("."), "linux", jobs=4,
                               serial=False, executor=make_executor(0.05))
    cases.append({"case": "S4_result_order_deterministic",
                  "ok": [r["id"] for r in res] == [s["id"] for s in seq],
                  "order": [r["id"] for r in res]})

    # S6 并行/串行等价性（G2-9 未完成项）：同一 step 集在 --serial 与 --jobs=4
    #    下必须给出**逐项相同**的结果序列（id 顺序 + verdict），即并行化不改变
    #    判定结果，只改变墙钟。
    spans.clear()
    mix = [mk("S6-A", outputs=["run/ci/selftest/s6-a.json"]),
           mk("S6-MON", command=["python3", "eng/ci/resource_monitor.py", "--timeout", "1",
                                 "--output", "run/ci/selftest/s6-mon.json", "--",
                                 "python3", "-c", "pass"]),
           mk("S6-B", outputs=["run/ci/selftest/s6-b.json"]),
           mk("S6-BAR", reads_run_results=True),
           mk("S6-C", outputs=["run/ci/selftest/s6-c.json"])]
    serial_res, _p1 = run_scheduled(mix, Path("."), Path("."), "linux", jobs=1,
                                    serial=True, executor=make_executor(0.05))
    par_res, _p2 = run_scheduled(mix, Path("."), Path("."), "linux", jobs=4,
                                 serial=False, executor=make_executor(0.05))
    key = lambda rs: [(r["id"], r["verdict"]) for r in rs]
    cases.append({"case": "S6_serial_parallel_equivalence",
                  "ok": key(serial_res) == key(par_res) and len(par_res) == len(mix),
                  "serial": key(serial_res), "parallel": key(par_res),
                  "note": "并行化只改墙钟，不改逐项判定与结果顺序"})
    return cases


def scope_failures(meta: dict, selected: list) -> list:
    """fail-closed 判据（CI_SPEC.md §2.4）。返回命中的判据 ID 列表（空 = 不判红）。"""
    codes: list = []
    if meta.get("scope") != "changed":
        return codes
    if meta.get("no_changes"):
        return codes
    if meta.get("uncovered_paths"):
        codes.append(SCOPE_UNCOVERED)
    if not selected:
        codes.append(SCOPE_EMPTY)
    return codes


def select_changed(steps: list, *, profile: str, change: dict, repo: Path,
                   match=None, graph=None) -> tuple:
    """增量选择（CI_SPEC.md §2.3）。返回 (selected_steps, selection_meta)。

    match(path, patterns) 可注入（--self-test 的"选择器恒空"负例面）；默认用
    incremental.first_match（glob 语义见 CI_SPEC.md §2.3）。
    """
    matcher = match or _inc.first_match
    changed = list(change.get("changed_files") or [])
    candidates = [s for s in steps if profile in s["profiles"]]
    esc = _inc.escalation_reasons(changed)
    meta = {
        "mode": "changed",
        "scope": "changed",
        "profile": profile,
        "requested": [],
        "profile_filter_applied": True,
        "base_ref": change.get("base_ref"),
        "injected_change_set": bool(change.get("injected")),
        "injected_from": change.get("injected_from"),
        "changed_files": changed,
        "changed_file_count": len(changed),
        "no_changes": not changed,
        "escalated_to_full": esc,
        "uncovered_paths": [],
        "candidate_steps": len(candidates),
        "build_graph": {"used": False, "reason": "未加载"},
        "per_step": [],
        "note": ("--changed = 影响面增量（CI_SPEC.md §2.3）：只跑 changed_paths 与改动集"
                 "相交的 step；受影响的构建/测试 target 由构建图反查。"),
    }
    if esc:
        meta["scope"] = "full"
        for s in candidates:
            meta["per_step"].append({"id": s["id"], "parent_id": s["parent_id"],
                                     "selected": True, "basis": "escalated_to_full"})
        return list(candidates), meta
    if not changed:
        return [], meta

    glob_hits: dict = {}
    for s in candidates:
        pats = s.get("changed_paths") or []
        hits = {}
        for p in changed:
            m = matcher(p, pats)
            if m:
                hits[p] = m
        if hits:
            glob_hits[s["id"]] = hits

    # 构建图反查（CI_SPEC.md §2.3）：只缩"跑 ctest 且声明 ctest_targets"的 step；
    # 改动文件不在构建图中 / 图不可用 ⇒ 保持选中（fail-closed，绝不缩范围）。
    narrowed: dict = {}
    if graph is not None and getattr(graph, "available", False):
        lib_changed = [p for p in changed if p.startswith("lib/")]
        if not lib_changed:
            meta["build_graph"] = {"used": False, "reason": "改动集不含 lib/** 路径"}
        elif not graph.affected_outputs(lib_changed):
            meta["build_graph"] = {"used": False,
                                   "reason": "改动文件不在构建图中（保持全选，fail-closed）"}
        else:
            affected = graph.affected_tests(lib_changed)
            meta["build_graph"] = {"used": True, "reason": None,
                                   "affected_test_count": len(affected),
                                   "affected_tests": sorted(affected)[:200]}
            by_id = {s["id"]: s for s in candidates}
            for sid, hits in glob_hits.items():
                s = by_id[sid]
                targets = s.get("ctest_targets") or []
                if not targets or not is_ctest_step(s):
                    continue
                if any(not p.startswith("lib/") for p in hits.values()):
                    continue  # 命中的是非 lib 路径 ⇒ 归因明确，不缩
                if not _inc.expand_target_globs(targets, affected):
                    narrowed[sid] = sorted(affected)[:5]
    else:
        meta["build_graph"] = {"used": False,
                               "reason": (graph.reason if graph is not None else "构建图未加载")}

    selected: list = []
    for s in candidates:
        sid = s["id"]
        if sid not in glob_hits:
            meta["per_step"].append({"id": sid, "parent_id": s["parent_id"],
                                     "selected": False, "basis": "changed_paths 未命中"})
            continue
        if sid in narrowed:
            meta["per_step"].append({"id": sid, "parent_id": s["parent_id"], "selected": False,
                                     "basis": "build_graph:无受影响 target",
                                     "affected_tests_sample": narrowed[sid]})
            continue
        meta["per_step"].append({
            "id": sid, "parent_id": s["parent_id"], "selected": True,
            "basis": "changed_paths:" + ",".join(sorted(set(glob_hits[sid].values()))),
            "matched_files": sorted(glob_hits[sid])})
        selected.append(s)
    meta["uncovered_paths"] = _inc.uncovered_paths(changed, _inc.coverage_patterns(candidates))
    return selected, meta


CTEST_STEP_MARKERS = ("ctest-target", "ctest-full", "ctest --test-dir", "-R ^")


def is_ctest_step(step: dict) -> bool:
    cmd = " ".join(step.get("command") or [])
    return any(m in cmd for m in CTEST_STEP_MARKERS)


def select(registry: dict, *, mode: str, check_args: list, profile: str, platform: str,
           change: dict | None = None, repo: Path | None = None, graph=None) -> tuple:
    steps, owner, dupes = index_steps(registry)
    if dupes:
        raise RunnerError(f"step id 在多处重复（收敛未完成）：{sorted(dupes)}")

    if mode == "changed":
        if change is None:
            raise RunnerError("mode=changed 需要改动集")
        return select_changed(steps, profile=profile, change=change, repo=repo, graph=graph)

    if mode == "check":
        if not check_args:
            raise RunnerError("--check 需要至少一个 ID")
        by_entry = {c["id"]: c for c in registry["checks"]}
        by_step = {s["id"]: s for s in steps}
        unknown = [x for x in check_args if x not in by_entry and x not in by_step]
        if unknown:
            raise RunnerError(f"--check 指定了未登记的 ID（既不匹配注册项也不匹配 step）：{unknown}")
        wanted_entries = {x for x in check_args if x in by_entry}
        wanted_steps = {x for x in check_args if x in by_step}
        selected = [s for s in steps
                    if s["parent_id"] in wanted_entries or s["id"] in wanted_steps]
        selection = {
            "mode": "explicit",
            "scope": "explicit",
            "profile": profile,
            "platform": platform,
            "requested": list(check_args),
            "profile_filter_applied": False,
            "entries": sorted({s["parent_id"] for s in selected}),
        }
        return selected, selection

    selected = [s for s in steps if profile in s["profiles"]]
    selection = {
        "mode": "all",
        "scope": "full",
        "profile": profile,
        "platform": platform,
        "requested": [],
        "profile_filter_applied": True,
        "entries": sorted({s["parent_id"] for s in selected}),
        "note": ("--all = 注册表整档全量（受 --profile 限定；默认 fast）。"
                 "linux-main 全量请显式 --profile linux-main。"),
    }
    return selected, selection


def explain_lines(selection: dict, selected: list) -> list:
    """--explain：逐条"选中/跳过 + 依据"，供人工复核（CI_SPEC.md §2.3）。"""
    out = []
    out.append(f"scope={selection.get('scope')} profile={selection.get('profile')} "
               f"base_ref={selection.get('base_ref')} "
               f"changed_files={selection.get('changed_file_count')}")
    if selection.get("injected_change_set"):
        out.append(f"!! injected_change_set from {selection.get('injected_from')}"
                   "（注入集，非 git 推导；仅用于自检/可复现实验）")
    if selection.get("no_changes"):
        out.append("no_changes：改动集为空 ⇒ 不跑任何检查（rc=0，CI_SPEC.md §2.4-3）")
        return out
    for r in selection.get("escalated_to_full") or []:
        out.append(f"escalated_to_full: {r['path']} 命中敏感面 {r['sensitive_pattern']}")
    for p in selection.get("uncovered_paths") or []:
        out.append(f"UNCOVERED_CHANGED_PATHS: {p}"
                   "（注册表覆盖缺口：请补 changed_paths 或显式全量）")
    bg = selection.get("build_graph") or {}
    out.append(f"build_graph: used={bg.get('used')} "
               f"affected_tests={bg.get('affected_test_count')} reason={bg.get('reason')}")
    for row in selection.get("per_step") or []:
        mark = "SELECT " if row.get("selected") else "skip   "
        extra = ""
        if row.get("matched_files"):
            extra = " files=" + ",".join(row["matched_files"][:3])
        if row.get("affected_tests_sample"):
            extra = " affected_sample=" + ",".join(row["affected_tests_sample"][:3])
        out.append(f"  {mark} {row['id']:38s} [{row.get('parent_id')}] {row.get('basis')}{extra}")
    out.append(f"selected_steps={len(selected)}")
    return out


def crash_tristate_self_test() -> list:
    """三态（PASS/FAIL/CRASH）+ 崩溃独立退出码的可执行正/负例面（GATE-TRIAGE-01）。

    负例（必须命中）：检查器抛未捕获异常且不给 verdict ⇒ 该 step 记 V_CRASH、
    条目记 V_CRASH、runner 退出码 EXIT_CRASH(3)，判词带「文件:行」。
    正例（不得误伤）：① 检查器给出结构化 FAIL 判词（即使输出里有 Traceback，
    例如 unittest 失败报告）⇒ 记 V_FAIL、退出码 EXIT_FAIL(1)；
    ② 全绿 ⇒ V_PASS、退出码 EXIT_OK(0)。
    """
    import tempfile as _tempfile
    cases: list = []
    with _tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "eng" / "ci").mkdir(parents=True, exist_ok=True)

        def reg(steps):
            doc = {"schema_version": 1, "checks": [
                {"id": "SELFTEST-TRISTATE", "profiles": ["fast"], "platform": "any",
                 "command": steps[0]["command"], "timeout_seconds": 60,
                 "heavy": False, "mutates_workspace": False, "outputs": [],
                 "waivable": False, "requires_monitor": False, "steps": steps}]}
            p = root / "eng" / "ci" / "checks.json"
            p.write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
            return p

        def step(code):
            return {"id": "SELFTEST-TRISTATE", "command": [sys.executable, "-c", code],
                    "timeout_seconds": 60, "platform": "any", "profiles": ["fast"],
                    "waivable": False, "heavy": False, "mutates_workspace": False,
                    "requires_monitor": False, "outputs": []}

        crash_code = ("import sys\n"
                      "def boom():\n"
                      "    raise ValueError('selftest injected crash')\n"
                      "boom()\n")
        fail_with_traceback = (
            "import sys, traceback\n"
            "try:\n"
            "    raise AssertionError('assertion')\n"
            "except AssertionError:\n"
            "    traceback.print_exc()\n"
            "print('SELFTEST_TOOL_FAIL: verdict=FAIL')\n"
            "sys.exit(1)\n")
        green_code = "print('SELFTEST_TOOL_PASS: verdict=PASS')\n"

        for name, code, want_verdict, want_rc in (
                ("crash_uncaught_exception", crash_code, V_CRASH, EXIT_CRASH),
                ("fail_with_traceback_stays_fail", fail_with_traceback, V_FAIL, EXIT_FAIL),
                ("green_stays_pass", green_code, V_PASS, EXIT_OK)):
            reg([step(code)])
            out_json = root / (name + ".json")
            rc = main(["--check", "SELFTEST-TRISTATE", "--registry",
                       str(root / "eng" / "ci" / "checks.json"),
                       "--repo-root", str(root), "--run-root", str(root / "rr"),
                       "--json-out", str(out_json), "--quiet"])
            payload = json.loads(out_json.read_text(encoding="utf-8"))
            entry = payload["checks"][0]
            got_verdict = entry["verdict"]
            ok = (got_verdict == want_verdict and rc == want_rc)
            detail = ""
            if want_verdict == V_CRASH:
                # 判词必须带「文件:行」（崩溃第一现场可定位）且不截断
                detail = entry["steps"][0].get("reason") or ""
                ok = ok and (".py\", line " in detail or ":" in detail)
                ok = ok and "selftest injected crash" in (
                    entry["steps"][0].get("stderr_tail") or "")
            cases.append({"case": "tristate_" + name, "ok": bool(ok),
                          "verdict": got_verdict, "want": want_verdict,
                          "rc": rc, "want_rc": want_rc, "reason": detail[:160]})
    return cases


def evidence_verdict_self_test() -> list:
    """证据面 fail-closed 自测（GATE-501）：缺失证据 / 坏证据 / 无输出三注入必红。

    直接驱动 execute_step 使用的同一判定函数 evidence_verdict（单一实现点），
    在临时目录内构造 6 个用例（4 红 2 绿），不写主工作区。
    """
    import shutil as _shutil
    import tempfile as _tempfile
    cases: list = []
    good_metrics = {"effective_cpus": 16, "allocated": 16, "interval_seconds": 60.0,
                    "avg_utilization": 0.95, "p50_utilization": 0.97,
                    "sample_pass_fraction": 1.0, "max_low_window_seconds": 0.0,
                    "utilization_evaluated": True}
    bad_metrics = dict(good_metrics, avg_utilization=0.245275, p50_utilization=0.038825,
                       sample_pass_fraction=0.069272, max_low_window_seconds=142.049,
                       interval_seconds=389.278)
    repo_root = Path(__file__).resolve().parents[2]
    with _tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "eng" / "contracts").mkdir(parents=True, exist_ok=True)
        _shutil.copy2(repo_root / "eng" / "contracts" / "resource_gate_v1.json",
                      root / "eng" / "contracts" / "resource_gate_v1.json")
        mon = root / "run" / "ci" / "monitor" / "SELFTEST.json"
        mon.parent.mkdir(parents=True, exist_ok=True)

        def mk(sid, **kw):
            base = {"id": sid, "command": ["python3", "-c", "pass"], "outputs": [],
                    "waivable": False, "requires_monitor": False, "heavy": False,
                    "mutates_workspace": False, "platform": "any", "profiles": ["fast"],
                    "timeout_seconds": 60}
            base.update(kw)
            return base

        def expect(name, step, stdout_tail, want):
            got = evidence_verdict(step, root, stdout_tail, "")
            got_verdict = got[0] if got else V_PASS
            cases.append({"case": name, "ok": got_verdict == want,
                          "got": got_verdict, "want": want,
                          "reason": (got[1] if got else "")[:160]})

        # N4 缺失证据：登记输出不存在 ⇒ 判红
        expect("N4_missing_declared_output",
               mk("E-N4", outputs=["run/ci/monitor/ABSENT.json"]), "ok", V_MISSING_OUTPUT)
        # N5 无输出：outputs 为空且 stdout/stderr 全空 ⇒ 判红
        expect("N5_empty_outputs_silent", mk("E-N5"), "", V_EMPTY_OUTPUT)
        # N6 缺失证据：requires_monitor 但无监控证据 ⇒ 判红
        expect("N6_monitor_evidence_missing",
               mk("E-N6", requires_monitor=True), "ok", V_GATE_MISSING)
        # N7 坏证据：证据存在但四条 L2 冻结判据违规 ⇒ 判红
        mon.write_text(json.dumps(
            {"duration_seconds": 389.278, "poll_interval": 0.2,
             "cpu_samples": [{"t": 0.0, "cpu_percent": 100.0}],
             "frozen_gate": {"verdict": "pass", "violations": [], "recorded": [],
                             "metrics": bad_metrics}}, ensure_ascii=False),
            encoding="utf-8")
        expect("N7_frozen_gate_violation",
               mk("E-N7", requires_monitor=True, outputs=["run/ci/monitor/SELFTEST.json"]),
               "ok", V_GATE_MISSING)
        # P3 正例：合规监控证据 ⇒ 绿
        mon.write_text(json.dumps(
            {"duration_seconds": 60.0, "poll_interval": 0.2,
             "cpu_samples": [{"t": 0.0, "cpu_percent": 100.0}],
             "frozen_gate": {"verdict": "pass", "violations": [], "recorded": [],
                             "metrics": good_metrics}}, ensure_ascii=False),
            encoding="utf-8")
        expect("P3_compliant_evidence_green",
               mk("E-P3", requires_monitor=True, outputs=["run/ci/monitor/SELFTEST.json"]),
               "ok", V_PASS)
        # P4 正例：无 requires_monitor、无 outputs 但有留痕 ⇒ 绿
        expect("P4_plain_step_with_stdout_green", mk("E-P4"), "ok", V_PASS)
    return cases


def run_self_test(repo: Path, registry: dict, *, profile: str, platform: str) -> int:
    """fail-closed 负例面（CI_SPEC.md §2.4 末段）：必须能红，且正例能绿。"""
    steps, _owner, _dupes = index_steps(registry)
    cases: list = []

    def case(name, change, *, match=None, expect_scope=None, expect_fail=None, expect_ok=False):
        selected, meta = select_changed(steps, profile=profile, change=change, repo=repo,
                                        match=match, graph=None)
        codes = scope_failures(meta, selected)
        ok = True
        if expect_fail is not None:
            ok = ok and expect_fail in codes
        if expect_ok:
            ok = ok and not codes and bool(selected)
        if expect_scope is not None:
            ok = ok and meta.get("scope") == expect_scope
        cases.append({"case": name, "ok": ok, "scope": meta.get("scope"),
                      "selected_steps": len(selected), "failures": codes,
                      "uncovered_sample": (meta.get("uncovered_paths") or [])[:3],
                      "escalated_sample": [r["path"] for r in
                                           (meta.get("escalated_to_full") or [])][:3]})
        return ok

    def injected(paths, tag):
        return {"base_ref": None, "changed_files": list(paths), "committed_changed": [],
                "uncommitted_changed": [], "injected": True, "injected_from": f"self-test:{tag}"}

    # N1 改动落在未覆盖路径 ⇒ 判红
    case("N1_uncovered_changed_path", injected(["__selftest__/not_covered.bin"], "N1"),
         expect_fail=SCOPE_UNCOVERED)
    # N2 改动命中全局敏感面 ⇒ 升级整档全量
    case("N2_sensitive_escalation", injected(["eng/ci/checks.json"], "N2"),
         expect_scope="full")
    # N3 选择器被改成恒空 ⇒ 判红
    case("N3_selector_always_empty", injected(["docs/ci/CI_SPEC.md"], "N3"),
         match=lambda _p, _pats: None, expect_fail=SCOPE_EMPTY)
    # P1 正例：正常改动 ⇒ 有选中且不判红
    case("P1_covered_change_green", injected(["docs/ci/CI_SPEC.md"], "P1"), expect_ok=True)
    # P2 正例：确实无改动 ⇒ no_changes，不误判红
    case("P2_no_changes_green", injected([], "P2"), expect_scope="changed")

    cases.extend(scheduler_self_test())
    cases.extend(evidence_verdict_self_test())
    cases.extend(crash_tristate_self_test())
    passed = sum(1 for c in cases if c["ok"])
    for c in cases:
        if 'scope' in c:
            print(f"[{'PASS' if c['ok'] else 'FAIL'}] {c['case']}: scope={c['scope']} "
                  f"selected={c['selected_steps']} failures={c['failures']} "
                  f"uncovered={c['uncovered_sample']} escalated={c['escalated_sample']}")
        else:
            print(f"[{'PASS' if c['ok'] else 'FAIL'}] {c['case']}: "
                  f"{json.dumps({k: v for k, v in c.items() if k not in ('case', 'ok')}, ensure_ascii=False)}")
    print(f"self_test: cases={len(cases)} passed={passed} failed={len(cases) - passed}")
    return EXIT_OK if passed == len(cases) else EXIT_FAIL


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="eng/ci/run_checks.py",
        description="ACSD 新规范机器检查入口（CI-001）：确定性执行 eng/ci/checks.json。"
                    "默认范围 = --changed（影响面增量，CI_SPEC.md §2.1）；全量必须显式 --all。")
    g = p.add_mutually_exclusive_group(required=False)
    g.add_argument("--changed", action="store_true",
                   help="影响面增量（**默认**）：改动集 = git diff --name-only <--base> "
                        "∪ git status --porcelain（含未跟踪）")
    g.add_argument("--all", "--full", action="store_true", dest="all",
                   help="注册表整档全量（受 --profile 限定；默认 fast）——必须显式给出")
    g.add_argument("--check", action="append", nargs="+", default=[], metavar="ID",
                   help="显式检查 ID（注册项 ID 或其 step 旧 ID）；支持多值"
                        "（--check CHK-FMT CHK-UNIT，docs/ci/01_CHECKS.md §5）"
                        "与可重复（--check A --check B），两者可混用")
    p.add_argument("--base", default="HEAD", metavar="REF",
                   help="增量基线 ref（默认 HEAD）；改动集 = 该 ref..工作树")
    p.add_argument("--changed-paths-from", default=None, metavar="FILE",
                   help="注入改动集（每行一个路径；仅用于 --self-test 负例与可复现实验，"
                        "结果 JSON 会带 injected_change_set=true）")
    p.add_argument("--explain", action="store_true",
                   help="逐条打印选中/跳过 + 依据（不执行）；fail-closed 命中时 rc=1")
    p.add_argument("--self-test", action="store_true", dest="self_test",
                   help="跑 fail-closed 负例/正例面（未覆盖路径 / 敏感面升级 / 恒空选择器 + 2 正例）")
    p.add_argument("--jobs", type=int, default=None, metavar="N",
                   help="并行道并发度（默认 min(8, cpu_count)）；独占道与屏障不受影响")
    p.add_argument("--serial", action="store_true",
                   help="一键回到串行执行（逃生开关；用于等价性对照）")
    p.add_argument("--budget-seconds", type=int, default=DEFAULT_INCREMENTAL_BUDGET_SECONDS,
                   metavar="N",
                   help=f"增量档总预算（默认 {DEFAULT_INCREMENTAL_BUDGET_SECONDS} s；0=不限）；"
                        "超出即判红并提示应拆分")
    p.add_argument("--build-dir", default="build", metavar="DIR",
                   help="构建图反查用的构建目录（默认 build）")
    p.add_argument("--profile", choices=PROFILES, default="fast",
                   help="profile 选择（默认 fast；--check 时不做 profile 过滤）")
    p.add_argument("--json-out", default=None, metavar="PATH",
                   help="机器可读 JSON 输出路径（原子写；目录自动创建）")
    p.add_argument("--plan-only", action="store_true",
                   help="只打印选中序列（不执行、不写 per-step 结果）")
    p.add_argument("--registry", default=None, help="注册表路径覆盖（默认 eng/ci/checks.json）")
    p.add_argument("--repo-root", default=None, help="仓库根覆盖（默认由脚本位置推导）")
    p.add_argument("--platform", default="auto", choices=("auto", "linux", "windows", "fatduck"),
                   help="运行平台覆盖（默认自动检测）")
    p.add_argument("--run-root", default=None,
                   help=f"per-step 结果根目录（默认 --json-out 所在目录，否则 {DEFAULT_RUN_ROOT}/<utc>）")
    p.add_argument("--quiet", action="store_true", help="只打印汇总行")
    return p


def main(argv: list | None = None) -> int:
    args = build_parser().parse_args(argv)
    started = utc_now()
    repo = Path(args.repo_root).resolve() if args.repo_root else \
        Path(__file__).resolve().parent.parent.parent
    registry_path = Path(args.registry) if args.registry else repo / "eng" / "ci" / "checks.json"
    if not registry_path.is_absolute():
        registry_path = (Path.cwd() / registry_path).resolve()

    try:
        registry = load_registry(registry_path)
        platform = current_platform(args.platform)
        if args.self_test:
            return run_self_test(repo, registry, profile=args.profile, platform=platform)

        # --check 多值 + 可重复：append+nargs 得到 list[list[str]]，此处展平；
        # 选择仍按注册表顺序（确定性），重复 ID 由 index 去重。
        check_args = [x for group in (args.check or []) for x in group]
        mode = "check" if check_args else ("all" if args.all else "changed")

        change = None
        graph = None
        if mode == "changed":
            if args.changed_paths_from:
                change = _inc.change_set_from_file(Path(args.changed_paths_from))
            else:
                change = _inc.change_set(repo, args.base)
            graph = _inc.BuildGraph(repo / args.build_dir, repo).load()

        selected, selection = select(registry, mode=mode, check_args=check_args,
                                     profile=args.profile, platform=platform,
                                     change=change, repo=repo, graph=graph)
        scope = selection.get("scope", "full" if mode == "all" else "explicit")
        registry_sha = sha256_file(registry_path)
        all_steps = index_steps(registry)[0]
        integration_pending = [s["id"] for s in all_steps if "integration" in s["profiles"]]
        integration_not_run = (args.profile == "fast" and bool(integration_pending))
        fails = scope_failures(selection, selected)

        if args.explain:
            for line in explain_lines(selection, selected):
                print(line)
            print(f"scheduler: mode={'serial' if args.serial else 'parallel'} "
                  f"jobs={args.jobs or min(8, os.cpu_count() or 1)}")
            for row in plan_lanes(selected):
                extra = (" keys=" + ",".join(row["resource_keys"])
                         if row.get("resource_keys") else "")
                print(f"  lane={row['lane']:9s} {row['id']:38s} {row['reason']}{extra}")
            if fails:
                print(f"scope_verdict=FAIL failures={fails}")
                return EXIT_FAIL
            print("scope_verdict=PASS")
            return EXIT_OK

        if args.plan_only:
            plan = {
                "schema_version": SCHEMA_VERSION,
                "runner": RUNNER,
                "mode": "plan-only",
                "scope": scope,
                "registry": {"path": str(registry_path), "sha256": registry_sha,
                             "entry_count": len(registry["checks"])},
                "selection": selection,
                "selected_steps": [
                    {"id": s["id"], "parent_id": s["parent_id"],
                     "platform": s["platform"], "timeout_seconds": s["timeout_seconds"],
                     "profiles": s["profiles"], "command": s["command"]}
                    for s in selected],
                "selected_step_count": len(selected),
                "scope_failures": fails,
                "integration_not_run": integration_not_run,
                "scheduler": plan_lanes(selected),
            }
            print(json.dumps(plan, ensure_ascii=False, indent=2))
            return EXIT_FAIL if fails else EXIT_OK

        if args.json_out:
            json_out = Path(args.json_out)
            if not json_out.is_absolute():
                json_out = repo / json_out
        else:
            json_out = None
        if args.run_root:
            run_root = Path(args.run_root)
            if not run_root.is_absolute():
                run_root = repo / run_root
        elif json_out is not None:
            run_root = json_out.parent
        elif os.environ.get("ASTROCS_CI_OUT_ROOT"):
            # 被 eng/ci/run.py 派发（聚合项 command = run_checks.py --check <ID>）时继承
            # 同 run 证据根：per-step 结果与上游 run.py 结果落同一 <out_root>/checks/，
            # 聚合型 known-failures 门才能读到整轮上游结果。
            run_root = Path(os.environ["ASTROCS_CI_OUT_ROOT"])
        else:
            run_root = repo / DEFAULT_RUN_ROOT / started.strftime("%Y%m%dT%H%M%SZ")
        run_root.mkdir(parents=True, exist_ok=True)

        # fail-closed：命中即判红且**不执行**（CI_SPEC.md §2.4）——避免用"跑了一半"
        # 的假绿掩盖"有改动没人判"。
        scope_entries: list = []
        if fails:
            why = []
            if SCOPE_UNCOVERED in fails:
                why.append("改动集含不匹配任何注册检查 changed_paths 的文件"
                           "（注册表覆盖缺口，请补 changed_paths 或显式全量）：")
                why += [f"    - {p}" for p in selection["uncovered_paths"]]
            if SCOPE_EMPTY in fails:
                why.append("改动集非空但选中检查数为 0（选择器退化，fail-closed）")
            for line in why:
                print(line, file=sys.stderr)
            scope_entries.append({
                "id": "SCOPE-FAIL-CLOSED",
                "verdict": V_SCOPE,
                "rc": EXIT_FAIL,
                "duration_seconds": 0.0,
                "timeout_seconds": 0,
                "steps": [{
                    "id": "SCOPE-FAIL-CLOSED", "parent_id": "SCOPE-FAIL-CLOSED",
                    "command": [], "timeout_seconds": 0, "platform": platform,
                    "profiles": [args.profile], "waivable": False, "heavy": False,
                    "mutates_workspace": False, "requires_monitor": False, "outputs": [],
                    "started_utc": utc_iso(started), "finished_utc": utc_iso(utc_now()),
                    "duration_seconds": 0.0, "exit_code": EXIT_FAIL, "timed_out": False,
                    "signal": None, "stdout_tail": "", "stderr_tail": "",
                    "stdout_lines": 0, "stderr_lines": 0, "fingerprint": None,
                    "reason": f"fail-closed: {','.join(fails)}", "verdict": V_SCOPE,
                }],
            })

        jobs = args.jobs if args.jobs and args.jobs > 0 else min(8, os.cpu_count() or 1)
        step_results, sched_plan = run_scheduled(
            selected, repo, run_root, platform, jobs=jobs, serial=args.serial)

        by_entry: dict = {}
        order: list = []
        for s in step_results:
            pid = s["parent_id"] or s["id"]
            if pid not in by_entry:
                by_entry[pid] = []
                order.append(pid)
            by_entry[pid].append(s)

        entry_results = []
        for pid in order:
            subs = by_entry[pid]
            bad = [s for s in subs if s["verdict"] in FAIL_VERDICTS]
            if bad:
                # 三态传播：崩溃优先于判红/超时（门崩 ⇒ 该条目的红绿都不可信，
                # 必须让消费者一眼看出「这不是内容不合规」）。
                verdict = V_CRASH if any(s["verdict"] == V_CRASH for s in subs) else \
                    (V_TIMEOUT if any(s["verdict"] == V_TIMEOUT for s in subs) else
                     (V_SCOPE if any(s["verdict"] == V_SCOPE for s in subs) else V_FAIL))
            elif subs and all(s["verdict"] == V_SKIP_PLATFORM for s in subs):
                verdict = V_SKIP_PLATFORM
            elif subs and all(s["verdict"] == V_SKIP_WAIVABLE for s in subs):
                verdict = V_SKIP_WAIVABLE
            else:
                verdict = V_PASS
            entry_results.append({
                "id": pid,
                "verdict": verdict,
                "rc": next((s["exit_code"] for s in subs if s["exit_code"]), 0),
                "duration_seconds": round(sum(s["duration_seconds"] for s in subs), 3),
                "timeout_seconds": sum(s["timeout_seconds"] for s in subs),
                "steps": subs,
            })

        counts = {V_PASS: 0, V_FAIL: 0, V_TIMEOUT: 0, V_CRASH: 0, V_SKIP_PLATFORM: 0,
                  V_SKIP_WAIVABLE: 0, V_PREREQ: 0, V_SCOPE: 0, V_REUSED: 0}
        for s in step_results:
            counts[s["verdict"]] = counts.get(s["verdict"], 0) + 1
        failures = [s["id"] for s in step_results if s["verdict"] in FAIL_VERDICTS]
        finished = utc_now()
        elapsed = round((finished - started).total_seconds(), 3)

        # 增量档总预算（CI_SPEC.md §2.5）：超出即判红并提示"应拆分"。
        budget_exceeded = False
        if scope == "changed" and args.budget_seconds and not fails \
                and elapsed > args.budget_seconds:
            budget_exceeded = True
            scope_entries.append({
                "id": "SCOPE-BUDGET",
                "verdict": V_SCOPE,
                "rc": EXIT_FAIL,
                "duration_seconds": elapsed,
                "timeout_seconds": args.budget_seconds,
                "steps": [{
                    "id": "SCOPE-BUDGET", "parent_id": "SCOPE-BUDGET",
                    "command": [], "timeout_seconds": args.budget_seconds, "platform": platform,
                    "profiles": [args.profile], "waivable": False, "heavy": False,
                    "mutates_workspace": False, "requires_monitor": False, "outputs": [],
                    "started_utc": utc_iso(started), "finished_utc": utc_iso(finished),
                    "duration_seconds": elapsed, "exit_code": EXIT_FAIL, "timed_out": False,
                    "signal": None, "stdout_tail": "", "stderr_tail": "",
                    "stdout_lines": 0, "stderr_lines": 0, "fingerprint": None,
                    "reason": (f"{SCOPE_BUDGET}: 增量档实测 {elapsed}s > 预算 "
                               f"{args.budget_seconds}s —— 应拆分（把重步骤移入 integration/"
                               "prerelease 档，或收窄 changed_paths）"),
                    "verdict": V_SCOPE,
                }],
            })
            failures = failures + ["SCOPE-BUDGET"]
            counts[V_SCOPE] = counts.get(V_SCOPE, 0) + 1

        entry_results = scope_entries + entry_results
        verdict = "FAIL" if failures else "PASS"
        if counts[V_CRASH]:
            verdict = V_CRASH
        payload = {
            "schema_version": SCHEMA_VERSION,
            "runner": RUNNER,
            "scope": scope,
            "generated_utc": utc_iso(finished),
            "registry": {"path": str(registry_path), "sha256": registry_sha,
                         "entry_count": len(registry["checks"]),
                         "step_count": len(index_steps(registry)[0])},
            "selection": selection,
            "integration_not_run": integration_not_run,
            "scheduler": sched_plan,
            "integration_pending_steps": sorted(integration_pending),
            "budget_seconds": args.budget_seconds if scope == "changed" else None,
            "budget_exceeded": budget_exceeded,
            "started_utc": utc_iso(started),
            "finished_utc": utc_iso(finished),
            "run_root": str(run_root),
            "summary": {
                "entries": len(entry_results),
                "steps": len(step_results),
                "passed": counts[V_PASS],
                "failed": counts[V_FAIL],
                "timeout": counts[V_TIMEOUT],
                "crashed": counts[V_CRASH],
                "prerequisite_failed": counts[V_PREREQ],
                "skipped_platform": counts[V_SKIP_PLATFORM],
                "skipped_waivable": counts[V_SKIP_WAIVABLE],
                "scope_failed": counts[V_SCOPE],
                "reused_fingerprint": counts[V_REUSED],
                "duration_seconds": elapsed,
                "verdict": verdict,
                "failures": failures,
            },
            "checks": entry_results,
        }
        if json_out is not None:
            json_out.parent.mkdir(parents=True, exist_ok=True)
            tmp = json_out.with_suffix(json_out.suffix + ".tmp")
            tmp.write_text(json.dumps(payload, ensure_ascii=False, indent=2),
                           encoding="utf-8")
            os.replace(tmp, json_out)

        if not args.quiet:
            for e in entry_results:
                line = (f"{e['id']:32s} {e['verdict']:20s} "
                        f"steps={len(e['steps'])} rc={e['rc']} "
                        f"t={e['duration_seconds']:.2f}s/{e['timeout_seconds']}s")
                print(line)
                for s in e["steps"]:
                    if s["verdict"] != V_PASS:
                        print(f"    - {s['id']}: {s['verdict']} rc={s['exit_code']} "
                              f"{s['reason'] or ''}")
        if scope == "changed" and selection.get("no_changes"):
            print("no_changes")
        if budget_exceeded:
            print(f"BUDGET_EXCEEDED: 增量档实测 {elapsed}s > 预算 {args.budget_seconds}s"
                  " —— 应拆分（CI_SPEC.md §2.5）")
        print(f"scheduler mode={sched_plan['mode']} jobs={sched_plan['jobs']} "
              f"exclusive={len(sched_plan['exclusive'])} parallel={len(sched_plan['parallel'])} "
              f"serialized_by_resource={len(sched_plan['serialized_by_resource'])} "
              f"barrier={len(sched_plan['barrier'])}")
        print(f"scope={scope} verdict={verdict} entries={len(entry_results)} "
              f"steps={len(step_results)} "
              f"pass={counts[V_PASS]} fail={counts[V_FAIL]} timeout={counts[V_TIMEOUT]} "
              f"crash={counts[V_CRASH]} "
              f"prereq={counts[V_PREREQ]} skip_platform={counts[V_SKIP_PLATFORM]} "
              f"skip_waivable={counts[V_SKIP_WAIVABLE]} scope_failed={counts[V_SCOPE]}")
        if integration_not_run:
            print(f"integration_not_run: fast 档不含 {len(integration_pending)} 个 integration "
                  "步骤；提交前必须另跑 python3 eng/ci/run_checks.py --all --profile integration"
                  "（CI_SPEC.md §2.6）")
        print(f"registry_sha256={registry_sha}")
        if json_out is not None:
            print(f"json_out={json_out}")
        # 三态退出码（GATE-TRIAGE-01）：崩溃换独立退出码 3，与判红（1）区分。
        if counts[V_CRASH]:
            return EXIT_CRASH
        return EXIT_OK if verdict == "PASS" else EXIT_FAIL
    except RunnerError as exc:
        print(f"eng/ci/run_checks.py: {exc}", file=sys.stderr)
        return EXIT_RUNNER_ERROR
    except _inc.IncrementalError as exc:
        print(f"eng/ci/run_checks.py: 增量范围计算失败：{exc}", file=sys.stderr)
        return EXIT_RUNNER_ERROR


if __name__ == "__main__":
    sys.exit(main())
