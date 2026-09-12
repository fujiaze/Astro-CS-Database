#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ctest_registration.py — CTest 目标 → CI 检查项注册闭包校验器（CI-REG-002）。

控制包依据：05_FINDINGS_REGISTER_20260911.md STD-F7「本轮新增测试目标未被 CI 显式登记」
处置 4「validators 增加负向检查（发现未注册的新 add_test 目标即 FAIL）」。

契约（fail-closed，任一 C 规则违规 → exit 1）：
  C1  扫描活动 CTest 面全部 CMake 源（名为 CMakeLists.txt 或 *.cmake），解析
      add_test(NAME <target> ...) 目标名；出现非 NAME 形式 add_test 亦判违规
      （无法静态枚举 = 无法证明已注册）。扫描排除 run/ build/ out/ artifacts/
      .git/ third_party/，以及路径含 archive/superseded 的归档旧树（AGENTS.md 目录规范）。
  C2  每个目标必须被「显式注册」覆盖，二者其一：
        (a) ci/checks.json 中某检查项的 ctest_targets 模式（精确名或 glob）匹配；或
        (b) ci/ctest_baseline.json 冻结存量清单命中（CI-REG-002 建立时点一次性收编的
            存量目标）。
  C3  未覆盖目标 → FAIL，逐条输出 target <- source（新增/改名测试必须同提交注册）。
  C4  反向完整性：ctest_targets 模式匹配不到任何现存目标 → FAIL（陈旧注册）。
  C5  基线漂移：ctest_baseline.json 中已不存在于 CMake 源的目标 → FAIL
      （删除测试必须同提交收缩基线，防基线无限膨胀）。
  C6  防「登记但未真跑」：无 glob 字符的 ctest_targets 名必须出现在该检查项 command
      的某个参数里（如 ctest -R ^p1001_real_nodes$），否则 FAIL。

负例（--selftest，全部在内存 fixture 上跑，零副作用）：
  S1  新增未注册 add_test → FAIL（C3）；
  S2  同目标由 ctest_targets 显式登记且 command 携带 → PASS；
  S3  同目标由冻结基线覆盖 → PASS；
  S4  ctest_targets 指向不存在的目标 → FAIL（C4）；
  S5  ctest_targets 精确名不在 command 中 → FAIL（C6）；
  S6  基线含已消失目标 → FAIL（C5）；
  S7  主仓库真实三件套（源/注册表/基线）→ PASS（回归保护，现场漂移即红）。

用法:
  python3 tools/quality/check_ctest_registration.py                     # 校验（CI 检查面）
  python3 tools/quality/check_ctest_registration.py --output run/ci/... # 同时落证据 JSON
  python3 tools/quality/check_ctest_registration.py --write-baseline    # 维护面：重算存量基线
  python3 tools/quality/check_ctest_registration.py --selftest          # 负例自检

只读（除 --write-baseline/--output 显式请求）；仅 stdlib。
"""
from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import pathlib
import re
import subprocess
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
REGISTRY_REL = "ci/checks.json"
BASELINE_REL = "ci/ctest_baseline.json"
SCHEMA_VERSION = 1

# 扫描排除（AGENTS.md 目录规范：run/ 临时面、build/out 构建面、artifacts 证据面，
# 归档控制包旧树不是活动 CTest 面）。
SKIP_DIR_NAMES = {
    "run", "build", "out", "artifacts", ".git", "third_party", "node_modules",
    ".venv", "__pycache__", "BASS DR3", "AstroCS.wiki",
}
SKIP_PATH_SUBSTR = ("archive", "superseded")

ADD_TEST_NAME_RE = re.compile(r"add_test\s*\(\s*NAME\s+([^\s()#]+)")
ADD_TEST_ANY_RE = re.compile(r"(?<![A-Za-z0-9_.])add_test\s*\(")
GLOB_CHARS = "*?["


def _utc_now() -> str:
    return _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


# --------------------------------------------------------------------------- 扫描 ----

def _is_skipped(rel: pathlib.Path) -> bool:
    if set(rel.parts) & SKIP_DIR_NAMES:
        return True
    low = str(rel).replace("\\", "/")
    return any(s in low for s in SKIP_PATH_SUBSTR)


def discover_sources(repo: pathlib.Path) -> list[pathlib.Path]:
    """活动 CTest 面 CMake 源：CMakeLists.txt 与 *.cmake（排除归档/构建/运行面）。"""
    found: set[pathlib.Path] = set()
    for pattern in ("CMakeLists.txt", "*.cmake"):
        for path in repo.rglob(pattern):
            if _is_skipped(path.relative_to(repo)):
                continue
            found.add(path)
    return sorted(found)


def _visible_text(text: str) -> str:
    """剥离整行注释（行首 # 行），避免注释里的 add_test 伪目标。"""
    return "\n".join(l for l in text.splitlines() if not l.lstrip().startswith("#"))


def parse_targets(sources: dict) -> tuple:
    """返回 (target -> 仓库相对源路径, 结构违规列表)。"""
    targets: dict = {}
    errors: list = []
    for rel, raw in sorted(sources.items()):
        text = _visible_text(raw)
        names = ADD_TEST_NAME_RE.findall(text)
        total = len(ADD_TEST_ANY_RE.findall(text))
        if total != len(names):
            errors.append(
                "C1 %s: add_test 调用 %d 处但 NAME 形式仅 %d 处"
                "（非 NAME 形式无法静态枚举 → fail-closed）" % (rel, total, len(names)))
        for name in names:
            name = name.strip().strip('"')
            if not name:
                continue
            prev = targets.get(name)
            if prev is not None and prev != rel:
                errors.append("C1 目标 %s 在多个源中重复注册：%s 与 %s" % (name, prev, rel))
                continue
            targets[name] = rel
    return targets, errors


# -------------------------------------------------------------------- 注册表/基线 ----

def load_json(path: pathlib.Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def registry_patterns(registry: dict) -> list:
    """返回 [(check_id, pattern, command)]。"""
    out: list = []
    for check in registry.get("checks", []):
        if not isinstance(check, dict):
            continue
        for pat in check.get("ctest_targets", []) or []:
            if isinstance(pat, str) and pat:
                out.append((check["id"], pat, list(check.get("command", []))))
    return out


def baseline_targets(baseline: dict) -> set:
    return {t for t in baseline.get("targets", []) if isinstance(t, str) and t}


# ------------------------------------------------------------------------- 判定 ----

def evaluate(targets: dict, registry: dict, baseline: dict) -> dict:
    """核心判定（纯函数，供主流程与 --selftest 共用）。"""
    errors: list = []
    patterns = registry_patterns(registry)
    base = baseline_targets(baseline)

    explicit: dict = {}
    dangling: list = []
    not_in_command: list = []
    for cid, pat, command in patterns:
        hits = [t for t in targets if fnmatch.fnmatchcase(t, pat)]
        if not hits:
            dangling.append("%s:%s" % (cid, pat))
        for target in hits:
            explicit.setdefault(target, cid)
        if not any(ch in pat for ch in GLOB_CHARS):
            if pat not in "\n".join(command):
                not_in_command.append("%s:%s" % (cid, pat))

    registered = set(explicit) | base
    unregistered = sorted(t for t in targets if t not in registered)
    baseline_only = sorted(t for t in targets if t in base and t not in explicit)
    stale_baseline = sorted(base - set(targets))

    if unregistered:
        errors.append(
            "C3 未注册的 add_test 目标 %d 个（新增/改名测试必须同提交在 ci/checks.json "
            "登记显式检查项，或经 ci/ctest_baseline.json 冻结收编）：" % len(unregistered))
        for target in unregistered:
            errors.append("C3   %s <- %s" % (target, targets[target]))
    for item in dangling:
        errors.append("C4 ctest_targets 模式匹配不到任何现存目标（陈旧注册）：%s" % item)
    for item in stale_baseline:
        errors.append(
            "C5 ctest_baseline.json 目标已不在 CMake 源中（删除测试须同步收缩基线）：%s" % item)
    for item in not_in_command:
        errors.append(
            "C6 ctest_targets 精确名未出现在该检查 command 中（登记但未真跑）：%s" % item)

    return {
        "targets_total": len(targets),
        "sources_total": len(set(targets.values())),
        "registered_explicit": sorted(explicit),
        "registered_baseline_only": baseline_only,
        "baseline_total": len(base),
        "unregistered": unregistered,
        "stale_baseline": stale_baseline,
        "dangling_patterns": sorted(dangling),
        "pattern_not_in_command": sorted(not_in_command),
        "errors": errors,
    }


# --------------------------------------------------------------------- 真实数据面 ----

def collect_real(repo: pathlib.Path) -> tuple:
    sources = {}
    for path in discover_sources(repo):
        rel = str(path.relative_to(repo)).replace("\\", "/")
        sources[rel] = path.read_text(encoding="utf-8", errors="replace")
    return parse_targets(sources)


def write_baseline(repo: pathlib.Path, targets: dict, explicit: set, previous: dict = None) -> pathlib.Path:
    """重算存量基线：现存目标 − 显式登记目标（显式登记者不进基线）。"""
    entries = sorted(t for t in targets if t not in explicit)
    try:
        sha = subprocess.run(["git", "-C", str(repo), "rev-parse", "HEAD"],
                             capture_output=True, text=True, timeout=30).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        sha = ""
    data = {
        "schema_version": SCHEMA_VERSION,
        "purpose": ("CI-REG-002 存量 CTest 目标冻结清单：建立时点已存在的 add_test 目标"
                    "一次性收编（避免 150+ 存量目标逐个登记）；此后新增/改名目标必须显式"
                    "登记 ci/checks.json 的 ctest_targets，删除目标必须同步收缩本清单"
                    "（tools/quality/check_ctest_registration.py C5 fail-closed）。"
                    "维护命令：--write-baseline。"),
        "base_commit": sha,
        "generated_utc": _utc_now(),
        "sources": sorted(set(targets.values())),
        "targets": entries,
    }
    path = repo / BASELINE_REL
    if previous is not None and previous.get("targets") == entries:
        return path
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return path


# ------------------------------------------------------------------------- 自检 ----

FIXTURE_CMAKE = (
    "add_executable(demo_test demo_test.cpp)\n"
    "add_test(NAME demo_units COMMAND demo_test units)\n"
    "add_test(NAME brand_new_target COMMAND demo_test new)\n"
)
FIXTURE_CMAKE_STALE = "add_test(NAME demo_units COMMAND demo_test units)\n"
FIXTURE_CMAKE_ONE = "add_test(NAME demo_units COMMAND demo_test units)\n"


def _fixture_registry(target=None, command_name=None) -> dict:
    checks = [{
        "id": "DEMO-BASE", "profiles": ["fast"], "platform": "any",
        "command": ["python3", "tools/quality/check_ctest_registration.py"],
        "timeout_seconds": 60, "heavy": False, "mutates_workspace": False,
        "outputs": [], "waivable": False,
    }]
    if target is not None:
        checks.append({
            "id": "DEMO-CTEST", "profiles": ["linux-main"], "platform": "any",
            "command": ["ctest", "--test-dir", "run/ci/build", "-R",
                        "^%s$" % (command_name or target), "--output-on-failure"],
            "timeout_seconds": 300, "heavy": False, "mutates_workspace": False,
            "outputs": [], "waivable": False, "ctest_targets": [target],
        })
    return {"schema_version": 1, "checks": checks}


def run_selftest() -> int:
    results: list = []

    def case(name: str, sources: dict, registry: dict, baseline: dict, expect_pass: bool) -> None:
        targets, structural = parse_targets(sources)
        verdict = evaluate(targets, registry, baseline)
        errs = structural + verdict["errors"]
        ok = (not errs) if expect_pass else bool(errs)
        results.append({"case": name, "expect": "PASS" if expect_pass else "FAIL",
                        "actual": "PASS" if not errs else "FAIL", "ok": ok, "errors": errs})

    src = {"CMakeLists.txt": FIXTURE_CMAKE}
    empty_reg = _fixture_registry()
    case("S1_unregistered_new_target", src, empty_reg, {"targets": []}, False)
    case("S2_explicit_registration", {"CMakeLists.txt": FIXTURE_CMAKE_ONE},
         _fixture_registry("demo_units"), {"targets": []}, True)
    case("S3_baseline_covered", src, empty_reg,
         {"targets": ["demo_units", "brand_new_target"]}, True)
    case("S4_dangling_pattern", src, _fixture_registry("ghost_target"), {"targets": []}, False)
    case("S5_pattern_not_in_command", src, _fixture_registry("demo_units", "other_name"),
         {"targets": []}, False)
    case("S6_stale_baseline", {"CMakeLists.txt": FIXTURE_CMAKE_STALE}, empty_reg,
         {"targets": ["removed_target"]}, False)

    targets, structural = collect_real(REPO)
    verdict = evaluate(targets, load_json(REPO / REGISTRY_REL),
                       load_json(REPO / BASELINE_REL))
    errs = structural + verdict["errors"]
    results.append({"case": "S7_real_repo", "expect": "PASS",
                    "actual": "PASS" if not errs else "FAIL", "ok": not errs, "errors": errs})

    failed = [r for r in results if not r["ok"]]
    print(json.dumps({"tool": "check_ctest_registration.py", "mode": "selftest",
                      "cases": results, "failed": len(failed),
                      "verdict": "PASS" if not failed else "FAIL"},
                     ensure_ascii=False, indent=2))
    return 0 if not failed else 1


# --------------------------------------------------------------------------- CLI ----

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="CTest 目标 → CI 注册闭包校验（CI-REG-002）")
    ap.add_argument("--repo", default=str(REPO))
    ap.add_argument("--registry", default=REGISTRY_REL)
    ap.add_argument("--baseline", default=BASELINE_REL)
    ap.add_argument("--output", default=None, help="证据 JSON 落盘路径（run/ 下）")
    ap.add_argument("--write-baseline", action="store_true",
                    help="维护面：按当前源码重算 ci/ctest_baseline.json（CI 不调用）")
    ap.add_argument("--selftest", action="store_true", help="负例自检（内存 fixture）")
    args = ap.parse_args(argv)

    if args.selftest:
        return run_selftest()

    repo = pathlib.Path(args.repo).resolve()
    targets, structural = collect_real(repo)
    registry = load_json(repo / args.registry)
    baseline_path = repo / args.baseline
    baseline = load_json(baseline_path) if baseline_path.is_file() else {"targets": []}

    if args.write_baseline:
        explicit = {t for _cid, pat, _cmd in registry_patterns(registry)
                    for t in targets if fnmatch.fnmatchcase(t, pat)}
        path = write_baseline(repo, targets, explicit, previous=baseline)
        print(json.dumps({"tool": "check_ctest_registration.py", "mode": "write-baseline",
                          "baseline": str(path.relative_to(repo)).replace("\\", "/"),
                          "targets": len(targets), "explicit": len(explicit),
                          "baseline_entries": len(targets) - len(explicit),
                          "verdict": "PASS"}, ensure_ascii=False, indent=2))
        return 0

    verdict = evaluate(targets, registry, baseline)
    errors = structural + verdict["errors"]
    summary = {
        "tool": "check_ctest_registration.py",
        "rule": "CI-REG-002 / STD-F7 处置 4",
        "generated_utc": _utc_now(),
        "registry": args.registry,
        "baseline": args.baseline,
        "baseline_base_commit": baseline.get("base_commit", ""),
        "targets_total": verdict["targets_total"],
        "sources_total": verdict["sources_total"],
        "registered_explicit": verdict["registered_explicit"],
        "registered_baseline_only_count": len(verdict["registered_baseline_only"]),
        "unregistered": verdict["unregistered"],
        "stale_baseline": verdict["stale_baseline"],
        "dangling_patterns": verdict["dangling_patterns"],
        "pattern_not_in_command": verdict["pattern_not_in_command"],
        "error_count": len(errors),
        "errors": errors,
        "verdict": "PASS" if not errors else "FAIL",
    }
    text = json.dumps(summary, ensure_ascii=False, indent=2)
    if args.output:
        out = pathlib.Path(args.output)
        if not out.is_absolute():
            out = repo / out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    # 人类可读摘要恒落 stdout（run.py V_EMPTY_OUTPUT 防线：非 waivable 检查必须留痕）
    print("[check_ctest_registration] targets=%d sources=%d explicit=%d baseline_only=%d "
          "verdict=%s" % (verdict["targets_total"], verdict["sources_total"],
                          len(verdict["registered_explicit"]),
                          len(verdict["registered_baseline_only"]), summary["verdict"]))
    for line in errors:
        print(line)
    print(text)
    return 0 if not errors else 1


if __name__ == "__main__":
    sys.exit(main())
