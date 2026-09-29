#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_build_coverage.py — 「全量构建」可检验性判据（登记/已尝试/未尝试）。

为什么需要: 「登记 338 个目标、已尝试 177 个」这类数字若无判据，「全量构建」只是
一句话。本判据把三个数字变成**可复跑、可判红**的结论，并把两件事**严格分开**:

  * 未尝试面 (never_attempted): 登记了但一次都没被派发过的目标 —— 覆盖率问题;
  * 真失败面 (failed): 被派发过但编译/链接失败的目标 —— 正确性问题;
  两者不合并、不互相抵偿（52.4% 的尝试率 + 122 个错误，不等于「其余 47.6% 无问题」）。

输入（三种，等价语义）:
  1) --coverage <json>   : {registered: [...], attempted: [...]}（Windows 腿直接产出）;
  2) --registry <json> + --log <build.log> : 登记表 + 从日志 Building/Linking 行抽尝试面;
  3) --registry-from-cmake <repo>          : 登记表由根 CMake 图（eng/ci/cmake_graph.py）现算。

收口条件: 未尝试数为零，且构建走到最后一个登记目标（尝试序列是登记序列的前缀）。
平台面判定权在 Windows 腿；本判据在 Linux 树上用夹具自证（--self-test）。

退出码: 0=PASS; 1=FAIL（有未尝试或真失败）; 2=ANCHOR_STALE; 3=参数非法。
"""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys

BUILDING_RE = re.compile(r"^\[\s*(\d+)/(\d+)\]\s+(?:Building|Linking)\b")
NINJA_OBJ_RE = re.compile(r"^\[\s*\d+/\d+\]\s+(?:Building|Linking)\s+\S*\s*object\s+(\S+?)(?:\.o|\.(?:cpp|cc|cxx|c))\b")
MSBUILD_TARGET_RE = re.compile(r"^(\S+)\.vcxproj\b")
ERROR_RE = re.compile(r"\berror\s+[A-Z]+\d+|:\s*error:|\berror:\s")


def parse_log(text: str):
    """抽: 尝试面（按出现顺序去重）、报错面、总步数上界。"""
    attempted, failed, total_hint = [], [], None
    seen = set()
    for raw in text.splitlines():
        line = raw.rstrip("\r")
        mb = BUILDING_RE.match(line)
        if mb:
            total_hint = int(mb.group(2))
            mo = NINJA_OBJ_RE.match(line)
            if mo:
                name = mo.group(1)
                if name and name not in seen:
                    seen.add(name)
                    attempted.append(name)
            continue
        mm = MSBUILD_TARGET_RE.match(line.strip())
        if mm:
            name = mm.group(1)
            if name not in seen:
                seen.add(name)
                attempted.append(name)
            continue
        if ERROR_RE.search(line):
            failed.append(line.strip()[:200])
    return attempted, failed, total_hint


def compute(registered, attempted, failed, total_hint=None):
    reg = list(dict.fromkeys(registered))
    att = list(dict.fromkeys(attempted))
    reg_set, att_set = set(reg), set(att)
    never = [t for t in reg if t not in att_set]
    extra = [t for t in att if t not in reg_set]      # 尝试了但不在登记表 ⇒ 登记表缺口
    # 「走到最后一个登记目标」: 尝试序列必须是登记序列的前缀
    order = {t: i for i, t in enumerate(reg)}
    att_ordered = [t for t in att if t in order]
    att_idx = [order[t] for t in att_ordered]
    prefix_end = -1
    for i, idx in enumerate(att_idx):
        if idx == i:
            prefix_end = i
        else:
            break
    complete = (len(never) == 0 and len(att_set) >= len(reg_set))
    last_registered_attempted = bool(reg) and reg[-1] in att_set
    return {
        "registered": len(reg),
        "attempted": len(att_set),
        "never_attempted": len(never),
        "coverage_pct": round(100.0 * len(att_set) / len(reg), 2) if reg else 0.0,
        "never_attempted_names": never,
        "attempted_not_registered": extra,
        "failed_lines": len(failed),
        "failed_sample": failed[:10],
        "reached_last_target": last_registered_attempted,
        "stopped_at_index": prefix_end,
        "log_total_steps_hint": total_hint,
        "complete": complete,
    }


def report(doc):
    s = doc["summary"]
    print("BUILD-COVERAGE %s" % doc["verdict"])
    print("登记目标数=%d  已尝试=%d  **未尝试=%d**  覆盖率=%.2f%%"
          % (s["registered"], s["attempted"], s["never_attempted"], s["coverage_pct"]))
    print("走到最后一个登记目标: %s（收口必要条件）" % ("是" if s["reached_last_target"] else "否"))
    print("—— 未尝试面（覆盖率问题, 与真失败严格分开）: %d 个" % s["never_attempted"])
    for t in s["never_attempted_names"][:20]:
        print("    未尝试: %s" % t)
    if len(s["never_attempted_names"]) > 20:
        print("    ... 共 %d 个" % len(s["never_attempted_names"]))
    print("—— 真失败面（正确性问题, 不与未尝试合并计数）: %d 条错误行" % s["failed_lines"])
    for t in s["failed_sample"][:5]:
        print("    失败: %s" % t)
    if s["attempted_not_registered"]:
        print("—— 登记表缺口: %d 个目标被构建但未登记 %s"
              % (len(s["attempted_not_registered"]), s["attempted_not_registered"][:5]))
    for p in doc["problems"]:
        print("  [!] %s" % p)


def _registered_from_cmake(repo: pathlib.Path):
    import importlib.util
    p = repo / "eng" / "ci" / "cmake_graph.py"
    if not p.is_file():
        raise FileNotFoundError(str(p))
    sys.path.insert(0, str(p.parent))
    spec = importlib.util.spec_from_file_location("cmake_graph", p)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    graph = mod.parse_cmake_graph(repo)
    targets = graph.get("targets") or {}
    # 登记面 = 根 CMake 图里的全部本地可构建目标（可执行 + 库）; 别名/导入目标不计
    # kind 形如 add_executable/add_library; 别名与导入目标不在本仓 CMakeLists 内定义, 不计
    return [n for n, info in sorted(targets.items())
            if (info or {}).get("file") and (info or {}).get("kind") in
            ("add_executable", "add_library")]

def self_test(args) -> int:
    fx = pathlib.Path(__file__).resolve().parent / "fixtures" / "build_coverage"
    if not fx.is_dir():
        print("SELFTEST_FAIL: 夹具目录缺失 %s" % fx)
        return 1
    failures = []

    def load(name):
        return json.loads((fx / name).read_text(encoding="utf-8"))

    def run(cov):
        attempted, failed, hint = parse_log(cov.get("log", ""))
        reg = cov.get("registered") or []
        att = cov.get("attempted") if cov.get("attempted") is not None else attempted
        s = compute(reg, att, failed, hint)
        problems = []
        if s["never_attempted"]:
            problems.append("未尝试面非空: %d 个目标从未被派发" % s["never_attempted"])
        if s["failed_lines"] and not args.allow_failures:
            problems.append("真失败面非空: %d 条错误行" % s["failed_lines"])
        if not s["reached_last_target"]:
            problems.append("未走到最后一个登记目标（构建在中途停止派发）")
        if s["attempted_not_registered"]:
            problems.append("登记表缺口: %d 个" % len(s["attempted_not_registered"]))
        doc = {"check_id": "BUILD-COVERAGE", "verdict": "FAIL" if problems else "PASS",
               "summary": s, "problems": problems}
        return doc

    # 态 1（正例）: 全量走到最后一个目标, 无未尝试, 无错误
    doc = run(load("complete.json"))
    s = doc["summary"]
    if doc["verdict"] != "PASS" or s["never_attempted"] != 0 or s["attempted"] != s["registered"]:
        failures.append("complete: 期望 PASS/未尝试=0/尝试==登记 实得 %s/%s/%s"
                        % (doc["verdict"], s["never_attempted"], s["attempted"]))
    else:
        print("SELFTEST_PASS complete (登记=%d 尝试=%d 未尝试=0 走到最后目标=%s)"
              % (s["registered"], s["attempted"], s["reached_last_target"]))

    # 态 2（负例）: 遇错即停派发 ⇒ 未尝试面非空, 且与真失败分开报
    doc = run(load("partial.json"))
    s = doc["summary"]
    if doc["verdict"] != "FAIL":
        failures.append("partial: 期望 FAIL 实得 %s" % doc["verdict"])
    elif s["never_attempted"] == 0 or s["failed_lines"] == 0:
        failures.append("partial: 未尝试/真失败两面没分开报 %s" % s)
    else:
        print("SELFTEST_PASS partial (登记=%d 尝试=%d 未尝试=%d 真失败=%d 走到最后=%s, FAIL)"
              % (s["registered"], s["attempted"], s["never_attempted"],
                 s["failed_lines"], s["reached_last_target"]))

    # 态 3（恢复）: 处置后未尝试归零 ⇒ 门转绿
    doc = run(load("restored.json"))
    s = doc["summary"]
    if doc["verdict"] != "PASS" or s["never_attempted"] != 0:
        failures.append("restored: 期望 PASS/未尝试=0 实得 %s/%s"
                        % (doc["verdict"], s["never_attempted"]))
    else:
        print("SELFTEST_PASS restored (未尝试归零, 门转绿)")

    # 态 4（登记表缺口）: 被构建但未登记 ⇒ 判红（登记面必须覆盖构建面）
    doc = run(load("gap.json"))
    if doc["verdict"] != "FAIL" or not doc["summary"]["attempted_not_registered"]:
        failures.append("gap: 期望 FAIL/报出缺口 实得 %s" % doc["verdict"])
    else:
        print("SELFTEST_PASS gap (登记表缺口 %d 个被判红)"
              % len(doc["summary"]["attempted_not_registered"]))

    # 态 5（真仓现算）: 根 CMake 图能给出登记面（Linux 树自证）
    if args.repo:
        try:
            names = _registered_from_cmake(pathlib.Path(args.repo))
            if not names:
                failures.append("cmake: 根 CMake 图解析出 0 个目标（登记表为空 ⇒ fail-closed）")
            else:
                print("SELFTEST_PASS cmake (登记面=%d 个目标, 来自根 CMake 图)" % len(names))
        except Exception as exc:  # noqa: BLE001
            failures.append("cmake: 解析失败 %s" % exc)

    if failures:
        print("SELFTEST_FAIL:")
        for f in failures:
            print("  " + f)
        return 1
    print("SELFTEST_PASS: 正例/负例/恢复/缺口/真仓 五态全部符合预期")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="全量构建覆盖面判据")
    ap.add_argument("--coverage", default=None, help="JSON: {registered:[], attempted:[], log}")
    ap.add_argument("--registry", default=None, help="JSON: {targets:[]}")
    ap.add_argument("--registry-from-cmake", default=None, help="仓库根, 用根 CMake 图现算")
    ap.add_argument("--log", action="append", default=[])
    ap.add_argument("--json-out", default=None)
    ap.add_argument("--allow-failures", action="store_true",
                    help="只报真失败面不判红（未尝试面永远判红）")
    ap.add_argument("--repo", default=str(pathlib.Path(__file__).resolve().parents[3]))
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    if args.self_test:
        return self_test(args)

    registered, attempted, log_text = [], [], ""
    if args.coverage:
        p = pathlib.Path(args.coverage)
        if not p.is_file():
            print("BUILD-COVERAGE ANCHOR_STALE: %s 不存在" % p)
            return 2
        doc_in = json.loads(p.read_text(encoding="utf-8"))
        registered = doc_in.get("registered", [])
        attempted = doc_in.get("attempted", [])
        log_text += doc_in.get("log", "")
    if args.registry:
        p = pathlib.Path(args.registry)
        if not p.is_file():
            print("BUILD-COVERAGE ANCHOR_STALE: %s 不存在" % p)
            return 2
        registered += json.loads(p.read_text(encoding="utf-8")).get("targets", [])
    if args.registry_from_cmake:
        try:
            registered += _registered_from_cmake(pathlib.Path(args.registry_from_cmake))
        except Exception as exc:  # noqa: BLE001
            print("BUILD-COVERAGE ANCHOR_STALE: CMake 图解析失败 %s" % exc)
            return 2
    for lg in args.log:
        p = pathlib.Path(lg)
        if not p.is_file():
            print("BUILD-COVERAGE ANCHOR_STALE: %s 不存在" % p)
            return 2
        log_text += p.read_text(encoding="utf-8", errors="replace")

    if not registered:
        print("BUILD-COVERAGE ANCHOR_STALE: 登记面为空（无登记表/无 CMake 图/无 --coverage）")
        return 2
    att_from_log, failed, hint = parse_log(log_text)
    attempted = list(attempted) + att_from_log
    s = compute(registered, attempted, failed, hint)
    problems = []
    if s["never_attempted"]:
        problems.append("未尝试面非空: %d 个登记目标从未被派发（覆盖率未收口）"
                        % s["never_attempted"])
    if s["failed_lines"] and not args.allow_failures:
        problems.append("真失败面非空: %d 条错误行（与未尝试面分开报）" % s["failed_lines"])
    if not s["reached_last_target"]:
        problems.append("未走到最后一个登记目标")
    if s["attempted_not_registered"]:
        problems.append("登记表缺口: %d 个被构建目标未登记" % len(s["attempted_not_registered"]))
    doc = {"check_id": "BUILD-COVERAGE", "verdict": "FAIL" if problems else "PASS",
           "summary": s, "problems": problems}
    report(doc)
    if args.json_out:
        out = pathlib.Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + chr(10), encoding="utf-8")
        print("JSON_OUT %s" % out)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())

