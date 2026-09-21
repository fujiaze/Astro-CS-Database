#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""eng/ci/run_checks.py — 新规范机器检查入口（控制包任务 CI-001）。

权威依据
  - docs/ci/01_CHECKS.md §1（eng/ci/checks.json 唯一注册表；eng/ci/ 提供确定性执行器）、
    §5 运行方式（--all / --check <ID...> / --all --json-out <path>）；
  - docs/ci/CI_SPEC.md §4（P0/P1 必须 0）、§7（失败必须留可复现证据）；
  - ENGINEERING_SPEC.md §8（唯一注册表；确定性执行器；每项能绿能红）。

本入口的合同（与 eng/ci/run.py 并存：工作流暂仍调用 eng/ci/run.py，QA-001 之后再改绑定）
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
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

SCHEMA_VERSION = 1
RUNNER = "eng/ci/run_checks.py"
PROFILES = ("fast", "linux-main", "windows-main", "linux-deep", "fatduck")
ALLOWED_PLATFORM = ("any", "linux", "windows", "fatduck")

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_RUNNER_ERROR = 2

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
V_SKIP_PLATFORM = "SKIPPED(platform)"
V_SKIP_WAIVABLE = "SKIPPED(waivable)"
V_PREREQ = "FAIL(prerequisite)"
FAIL_VERDICTS = (V_FAIL, V_TIMEOUT, V_PREREQ)

# step 从父项继承的字段（缺失即继承；显式给出即覆盖）
INHERIT_FIELDS = ("command", "profiles", "platform", "timeout_seconds", "heavy",
                  "mutates_workspace", "outputs", "waivable",
                  "requires_monitor", "prerequisite_tools", "changed_paths",
                  "dirty_ignore_exact", "dirty_ignore_prefixes")


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
    result["stdout_tail"] = tail(stdout_s)
    result["stderr_tail"] = tail(stderr_s)
    result["stdout_lines"] = stdout_s.count("\n")
    result["stderr_lines"] = stderr_s.count("\n")
    result["exit_code"] = proc.returncode
    result["timed_out"] = timed_out
    if timed_out:
        return finish(V_TIMEOUT, f"超过登记超时 {step['timeout_seconds']}s 被终止")
    if proc.returncode is not None and proc.returncode < 0:
        result["signal"] = -proc.returncode
        return finish(V_FAIL, f"被信号终止：{-proc.returncode}")
    if proc.returncode == 0:
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


def select(registry: dict, *, mode: str, check_args: list[str], profile: str,
           platform: str) -> tuple[list[dict], dict]:
    steps, owner, dupes = index_steps(registry)
    if dupes:
        raise RunnerError(f"step id 在多处重复（收敛未完成）：{sorted(dupes)}")

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
        "profile": profile,
        "platform": platform,
        "requested": [],
        "profile_filter_applied": True,
        "entries": sorted({s["parent_id"] for s in selected}),
        "note": ("--all = 注册表全量（受 --profile 限定；默认 fast）。"
                 "linux-main 全量请显式 --profile linux-main。"),
    }
    return selected, selection


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="eng/ci/run_checks.py",
        description="AstroCS 新规范机器检查入口（CI-001）：确定性执行 eng/ci/checks.json。")
    g = p.add_mutually_exclusive_group(required=True)
    g.add_argument("--all", action="store_true",
                   help="注册表全量（受 --profile 限定；默认 fast）")
    g.add_argument("--check", action="append", nargs="+", default=[], metavar="ID",
                   help="显式检查 ID（注册项 ID 或其 step 旧 ID）；支持多值"
                        "（--check CHK-FMT CHK-UNIT，docs/ci/01_CHECKS.md §5）"
                        "与可重复（--check A --check B），两者可混用")
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


def main(argv: list[str] | None = None) -> int:
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
        # --check 多值 + 可重复：append+nargs 得到 list[list[str]]，此处展平；
        # 选择仍按注册表顺序（确定性），重复 ID 由 index 去重。
        check_args = [x for group in (args.check or []) for x in group]
        selected, selection = select(
            registry,
            mode="check" if check_args else "all",
            check_args=check_args,
            profile=args.profile,
            platform=platform,
        )
        registry_sha = sha256_file(registry_path)

        if args.plan_only:
            plan = {
                "schema_version": SCHEMA_VERSION,
                "runner": RUNNER,
                "mode": "plan-only",
                "registry": {"path": str(registry_path), "sha256": registry_sha,
                             "entry_count": len(registry["checks"])},
                "selection": selection,
                "selected_steps": [
                    {"id": s["id"], "parent_id": s["parent_id"],
                     "platform": s["platform"], "timeout_seconds": s["timeout_seconds"],
                     "profiles": s["profiles"], "command": s["command"]}
                    for s in selected],
                "selected_step_count": len(selected),
            }
            print(json.dumps(plan, ensure_ascii=False, indent=2))
            return EXIT_OK

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

        step_results = [execute_step(s, repo, run_root, platform) for s in selected]

        by_entry: dict[str, list[dict]] = {}
        order: list[str] = []
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
                verdict = V_TIMEOUT if any(s["verdict"] == V_TIMEOUT for s in subs) else V_FAIL
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

        counts = {V_PASS: 0, V_FAIL: 0, V_TIMEOUT: 0, V_SKIP_PLATFORM: 0,
                  V_SKIP_WAIVABLE: 0, V_PREREQ: 0}
        for s in step_results:
            counts[s["verdict"]] = counts.get(s["verdict"], 0) + 1
        failures = [s["id"] for s in step_results if s["verdict"] in FAIL_VERDICTS]
        verdict = "FAIL" if failures else "PASS"
        finished = utc_now()
        payload = {
            "schema_version": SCHEMA_VERSION,
            "runner": RUNNER,
            "generated_utc": utc_iso(finished),
            "registry": {"path": str(registry_path), "sha256": registry_sha,
                         "entry_count": len(registry["checks"]),
                         "step_count": len([s for s in index_steps(registry)[0]])},
            "selection": selection,
            "started_utc": utc_iso(started),
            "finished_utc": utc_iso(finished),
            "run_root": str(run_root),
            "summary": {
                "entries": len(entry_results),
                "steps": len(step_results),
                "passed": counts[V_PASS],
                "failed": counts[V_FAIL],
                "timeout": counts[V_TIMEOUT],
                "prerequisite_failed": counts[V_PREREQ],
                "skipped_platform": counts[V_SKIP_PLATFORM],
                "skipped_waivable": counts[V_SKIP_WAIVABLE],
                "duration_seconds": round(
                    (finished - started).total_seconds(), 3),
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
        print(f"verdict={verdict} entries={len(entry_results)} steps={len(step_results)} "
              f"pass={counts[V_PASS]} fail={counts[V_FAIL]} timeout={counts[V_TIMEOUT]} "
              f"prereq={counts[V_PREREQ]} skip_platform={counts[V_SKIP_PLATFORM]} "
              f"skip_waivable={counts[V_SKIP_WAIVABLE]}")
        print(f"registry_sha256={registry_sha}")
        if json_out is not None:
            print(f"json_out={json_out}")
        return EXIT_OK if verdict == "PASS" else EXIT_FAIL
    except RunnerError as exc:
        print(f"eng/ci/run_checks.py: {exc}", file=sys.stderr)
        return EXIT_RUNNER_ERROR


if __name__ == "__main__":
    sys.exit(main())
