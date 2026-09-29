#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""check_ci_profile_coverage.py — CI 档位覆盖闭包门（CI-PROFILE-COVERAGE-CLOSURE）。

问题形态（自测层判据被 CI 整体漏掉）:
    checks.json 里 5 条 self-test 判据只挂 profiles=["fast"]，是全库**唯一**零持久档
    挂载的 self-test（29/34 条同类都挂 linux-main）。而 eng/ci/run.py:480 的唯一顶层
    选择点是严格成员判定
        candidates = [c for c in registry["checks"] if profile in c["profiles"]]
    —— **零继承展开**；eng/ci/run_checks.py:123 的 PROFILE_INHERITS 只在**已派发判据
    的子步**上生效（且只对 steps 存在者有意义）。
    后果：这 5 条是叶判据（steps == 0），没有任何子步可供继承生效 ⇒ 任何 CI 腿都选不到
    它们。判据的鉴别力分两层：被测门（绿只证明「当前没报错」）＋证明它有鉴别力的
    self-test 门。self-test 整体缺席 ⇒ **CI 绿灯不证明判据有鉴别力**。

判据口径（可执行，纯登记面，不碰执行器）:
  1. 档位阶梯**从单一来源 import** eng/ci/run_checks.py 的 PROFILE_INHERITS /
     PROFILE_GATING_LANES（不复制常量，避免第二套路径语义——那正是本门不碰 run.py
     的原因：改执行器须先把它提到两入口共用的单一来源，属更大的治理面）。
  2. 对每个宿主持久档 P，闭包 closure(P) = {P} ∪ PROFILE_INHERITS[P]。
     「按继承闭包应被派发」= 该判据 profiles ∩ closure(P) ≠ ∅。
  3. 「登记表实际挂载」= 该判据 profiles ∩ PROFILE_GATING_LANES ≠ ∅。
  4. **判红条件（并集层）**：应被闭包覆盖、却**没有被任何**持久档挂载 ⇒ 判红并**逐个点名**，
     同时打印是哪些档的闭包要求覆盖它、它当前实际挂了什么。
  5. 同阶梯档的「未直接挂载但闭包要求」**不判红**（那是执行器继承的职责；例如 94 条
     fast,linux-main,windows-main 不直接挂 linux-deep 是正常的），逐档明细只作
     INFO 打印。判红面只认第 4 条。
  6. fail-closed：注册表缺失 / 不可解析 / 缺 checks 键 / 档位阶梯模块不可 import
     ⇒ rc=2（输入不可用），绝不静默放行。

用法:
  python3 eng/tools/quality/check_ci_profile_coverage.py
  python3 eng/tools/quality/check_ci_profile_coverage.py --registry <path>   # 隔离树复跑
  python3 eng/tools/quality/check_ci_profile_coverage.py --json-out <path>
  python3 eng/tools/quality/check_ci_profile_coverage.py --list              # 打印红项
  python3 eng/tools/quality/check_ci_profile_coverage.py --self-test         # 含注入负例

退出码: 0=PASS（闭包覆盖完整）; 1=FAIL（有判据未被任何持久档覆盖，逐条点名）;
        2=输入不可用（fail-closed）; 3=参数非法。
"""
from __future__ import annotations

import argparse
import copy
import importlib.util
import json
import pathlib
import sys
import tempfile

REPO = pathlib.Path(__file__).resolve().parent.parent.parent.parent
REGISTRY = "eng/ci/checks.json"
RUN_CHECKS = "eng/ci/run_checks.py"

EXIT_OK = 0
EXIT_FAIL = 1
EXIT_INPUT = 2
EXIT_ARGS = 3


def load_ladder(run_checks_path: pathlib.Path):
    """从 eng/ci/run_checks.py 单一来源取档位阶梯（不复制常量）。"""
    if not run_checks_path.is_file():
        raise FileNotFoundError("档位阶梯模块不存在: %s" % run_checks_path)
    spec = importlib.util.spec_from_file_location("_acsd_run_checks_ladder",
                                                  run_checks_path)
    if spec is None or spec.loader is None:
        raise ImportError("档位阶梯模块不可加载: %s" % run_checks_path)
    mod = importlib.util.module_from_spec(spec)
    try:
        spec.loader.exec_module(mod)
    except Exception as exc:  # noqa: BLE001 — import 面任一异常都按 fail-closed
        raise ImportError("档位阶梯模块 import 失败: %s" % exc) from exc
    inherits = getattr(mod, "PROFILE_INHERITS", None)
    lanes = getattr(mod, "PROFILE_GATING_LANES", None)
    if not isinstance(inherits, dict) or not isinstance(lanes, (tuple, list)) or not lanes:
        raise ImportError("档位阶梯模块未导出 PROFILE_INHERITS/PROFILE_GATING_LANES")
    return {str(p): tuple(str(x) for x in inherits.get(p, ())) for p in lanes}, tuple(str(x) for x in lanes)


def load_registry(registry_path: pathlib.Path) -> dict:
    if not registry_path.is_file():
        raise FileNotFoundError("注册表不存在: %s" % registry_path)
    try:
        data = json.loads(registry_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise ValueError("注册表不可解析: %s" % exc) from exc
    if not isinstance(data, dict) or not isinstance(data.get("checks"), list):
        raise ValueError("注册表缺 checks 数组或结构非法")
    for c in data["checks"]:
        if not isinstance(c, dict) or not isinstance(c.get("id"), str):
            raise ValueError("注册表条目缺 id 字段或结构非法")
        profs = c.get("profiles")
        if not isinstance(profs, list) or not profs:
            raise ValueError("注册表条目 profiles 缺失/为空: %r" % c.get("id"))
    return data


def evaluate(registry: dict, inherits: dict, lanes: tuple) -> dict:
    """算出闭包应覆盖面与实际挂载面之差。"""
    closures = {p: {p} | set(inherits.get(p, ())) for p in lanes}
    covered: set = set()
    expected: dict = {}
    for c in registry["checks"]:
        cid = c["id"]
        profs = set(c["profiles"])
        if profs & set(lanes):
            covered.add(cid)
        for lane, clo in closures.items():
            if profs & clo:
                expected.setdefault(cid, []).append(lane)
    uncovered = sorted(cid for cid in expected if cid not in covered)
    prof_of = {c["id"]: list(c["profiles"]) for c in registry["checks"]}
    findings = [
        {
            "id": cid,
            "profiles": prof_of[cid],
            "required_by_lanes": expected[cid],
        }
        for cid in uncovered
    ]
    per_lane = {
        lane: {
            "closure": sorted(closures[lane]),
            "expected_count": sum(1 for cid in expected if lane in expected[cid]),
            "mounted_count": sum(1 for c in registry["checks"] if lane in c["profiles"]),
        }
        for lane in lanes
    }
    return {
        "total_checks": len(registry["checks"]),
        "lanes": list(lanes),
        "closures": {k: sorted(v) for k, v in closures.items()},
        "per_lane": per_lane,
        "covered_count": len(covered),
        "expected_count": len(expected),
        "uncovered_count": len(uncovered),
        "uncovered": findings,
    }


def _emit(report: dict, list_mode: bool, json_out: str) -> None:
    if list_mode:
        for f in report["uncovered"]:
            print("UNCOVERED %s profiles=%s required_by=%s"
                  % (f["id"], f["profiles"], f["required_by_lanes"]))
    print("[INFO] lanes=%s" % ",".join(report["lanes"]))
    for lane, st in report["per_lane"].items():
        print("[INFO] lane=%-12s closure=%-46s expected=%-4d mounted=%d"
              % (lane, ",".join(st["closure"]), st["expected_count"], st["mounted_count"]))
    print("[INFO] total_checks=%d covered_by_persistent_tier=%d expected_by_closure=%d uncovered=%d"
          % (report["total_checks"], report["covered_count"],
             report["expected_count"], report["uncovered_count"]))
    for f in report["uncovered"]:
        print("CI_PROFILE_COVERAGE_FAIL: %s 未被任何宿主持久档挂载 "
              "(profiles=%s)；按档位闭包它应被 %s 覆盖"
              % (f["id"], f["profiles"], ",".join(f["required_by_lanes"])))
    if report["uncovered_count"]:
        print("CI_PROFILE_COVERAGE_FAIL: %d 条判据应被 CI 档覆盖却无任何持久档挂载 —— "
              "补 profiles（登记面修法），不要改执行器" % report["uncovered_count"])
    else:
        print("CI_PROFILE_COVERAGE_PASS: 档位闭包覆盖完整")
    if json_out:
        p = pathlib.Path(json_out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")


def gate(registry_path: pathlib.Path, run_checks_path: pathlib.Path,
         list_mode: bool, json_out: str) -> int:
    try:
        registry = load_registry(registry_path)
        inherits, lanes = load_ladder(run_checks_path)
    except (FileNotFoundError, ValueError, ImportError) as exc:
        print("CI_PROFILE_COVERAGE_INPUT_ERROR: %s" % exc, file=sys.stderr)
        return EXIT_INPUT
    report = evaluate(registry, inherits, lanes)
    report["registry"] = str(registry_path)
    _emit(report, list_mode, json_out)
    return EXIT_FAIL if report["uncovered_count"] else EXIT_OK


# ─────────────────────────────── 自测面（隔离语料，不依赖仓库现状）───────────────

def _write_reg(path: pathlib.Path, data: dict) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")


def _synthetic_registry(ids_with_profiles: dict) -> dict:
    return {
        "checks": [
            {
                "id": cid, "profiles": list(profs), "platform": "any",
                "command": ["python3", "-c", "pass"], "timeout_seconds": 60,
                "heavy": False, "mutates_workspace": False, "outputs": [],
                "waivable": False, "changed_paths": ["**"], "requires_monitor": False,
            }
            for cid, profs in ids_with_profiles.items()
        ]
    }


def _self_test(registry_path: pathlib.Path, run_checks_path: pathlib.Path) -> int:
    cases: list = []

    def rec(name: str, want: int, got: int, note: str = "") -> None:
        cases.append((name, want, got, note))

    inherits, lanes = load_ladder(run_checks_path)
    with tempfile.TemporaryDirectory(prefix="ci_prof_cov_") as td:
        tmp = pathlib.Path(td)

        # T1 绿：全部判据都挂在宿主持久档上 ⇒ 必须 PASS
        reg1 = tmp / "green.json"
        _write_reg(reg1, _synthetic_registry({
            "A-FAST-AND-TIERS": ["fast", "linux-main", "windows-main"],
            "B-LINUX-ONLY": ["linux-main"],
            "C-DEEP": ["linux-deep", "prerelease"],
        }))
        rec("T1_green_synthetic_all_tiers", EXIT_OK,
            gate(reg1, run_checks_path, False, ""))

        # T2 红：唯一一条只挂 fast ⇒ 必须判红且**点名**
        reg2 = tmp / "red_one.json"
        _write_reg(reg2, _synthetic_registry({
            "A-FAST-AND-TIERS": ["fast", "linux-main", "windows-main"],
            "B-FAST-ONLY-LEAF": ["fast"],
        }))
        rc = gate(reg2, run_checks_path, True, "")
        rec("T2_red_synthetic_fast_only", EXIT_FAIL, rc)

        # T3 红：两条只挂 fast ⇒ 必须判红且**两条都点名**（证明是逐条点名不是"至少一条"）
        reg3 = tmp / "red_two.json"
        _write_reg(reg3, _synthetic_registry({
            "A-FAST-AND-TIERS": ["fast", "linux-main"],
            "B-FAST-ONLY-1": ["fast"],
            "C-FAST-ONLY-2": ["fast"],
        }))
        rc = gate(reg3, run_checks_path, True, "")
        rec("T3_red_synthetic_two_fast_only", EXIT_FAIL, rc)

        # T4 绿：真注册表**原样副本** ⇒ 必须 PASS（本门对当前登记面的回归断言）
        real = load_registry(registry_path)
        reg4 = tmp / "real_green.json"
        _write_reg(reg4, real)
        rec("T4_green_real_registry_copy", EXIT_OK,
            gate(reg4, run_checks_path, False, ""))

        # T5 注入负例（合同要求）：把真注册表里某条判据的 profiles 改成只挂 fast ⇒ 必须判红。
        #    注入在隔离树副本上做，**绝不触碰仓库里的真实登记**。
        inj = copy.deepcopy(real)
        victims = sorted(c["id"] for c in inj["checks"]
                         if set(c["profiles"]) & set(lanes))
        if not victims:
            rec("T5_inject_fast_only", EXIT_INPUT, EXIT_FAIL, "真注册表无任何持久档判据，无法注入")
        else:
            victim = victims[0]
            for c in inj["checks"]:
                if c["id"] == victim:
                    c["profiles"] = ["fast"]
            reg5 = tmp / "real_injected.json"
            _write_reg(reg5, inj)
            rc = gate(reg5, run_checks_path, True, "")
            rec("T5_inject_fast_only", EXIT_FAIL, rc,
                "injected_victim=%s" % victim)

        # T6 fail-closed：注册表不存在 ⇒ rc=2
        rec("T6_input_missing_registry", EXIT_INPUT,
            gate(tmp / "does_not_exist.json", run_checks_path, False, ""))

        # T7 fail-closed：注册表不可解析 ⇒ rc=2
        bad = tmp / "bad.json"
        bad.write_text("{ not json", encoding="utf-8")
        rec("T7_input_malformed_registry", EXIT_INPUT,
            gate(bad, run_checks_path, False, ""))

        # T8 fail-closed：缺 checks 键 ⇒ rc=2
        nokeys = tmp / "nokeys.json"
        _write_reg(nokeys, {"version": 1})
        rec("T8_input_missing_checks_key", EXIT_INPUT,
            gate(nokeys, run_checks_path, False, ""))

        # T9 fail-closed：档位阶梯模块不可 import ⇒ rc=2（证明不静默用硬编码常量兜底）
        rec("T9_input_ladder_module_missing", EXIT_INPUT,
            gate(reg1, tmp / "no_such_run_checks.py", False, ""))

    ok = True
    for name, want, got, note in cases:
        passed = want == got
        ok = ok and passed
        print("  [SELFTEST] %-40s expect=rc%d got=rc%d %s%s"
              % (name, want, got, "PASS" if passed else "FAIL",
                 (" (%s)" % note) if note else ""))
    if ok:
        print("CI_PROFILE_COVERAGE_SELFTEST_PASS: %d/%d case(s)" % (len(cases), len(cases)))
        return EXIT_OK
    print("CI_PROFILE_COVERAGE_SELFTEST_FAIL: %d/%d case(s)" %
          (sum(1 for c in cases if c[1] != c[2]), len(cases)))
    return EXIT_FAIL


def main(argv: list | None = None) -> int:
    ap = argparse.ArgumentParser(
        prog="eng/tools/quality/check_ci_profile_coverage.py",
        description="CI 档位覆盖闭包门：断言每条应被档位闭包覆盖的判据都被宿主持久档挂载。")
    ap.add_argument("--registry", default=None,
                    help="注册表路径（默认 eng/ci/checks.json；供隔离树复跑）")
    ap.add_argument("--run-checks", default=None, dest="run_checks",
                    help="档位阶梯单一来源模块（默认 eng/ci/run_checks.py）")
    ap.add_argument("--json-out", default="", dest="json_out",
                    help="机器可读判词输出路径")
    ap.add_argument("--list", action="store_true", dest="list_mode",
                    help="只打印判红条目（逐条点名）")
    ap.add_argument("--self-test", action="store_true", dest="self_test",
                    help="隔离语料正例/负例 + 注入负例（改副本，不碰真登记）")
    args = ap.parse_args(argv)

    registry_path = (pathlib.Path(args.registry) if args.registry
                     else REPO / REGISTRY)
    run_checks_path = (pathlib.Path(args.run_checks) if args.run_checks
                       else REPO / RUN_CHECKS)
    if args.self_test:
        try:
            return _self_test(registry_path, run_checks_path)
        except (FileNotFoundError, ValueError, ImportError) as exc:
            print("CI_PROFILE_COVERAGE_INPUT_ERROR: %s" % exc, file=sys.stderr)
            return EXIT_INPUT
    return gate(registry_path, run_checks_path, args.list_mode, args.json_out)


if __name__ == "__main__":
    sys.exit(main())
