#!/usr/bin/env python3
"""单元层运行器 —— **报告器，不是裁判**。

`docs/engineering/testing/TEST.md` §9 逐字：「执行者是人：结论由人读对抗审核给出，
每条结论都附可复算的证据。**测试集不产出流水线判决**。」
`standards/05_INDEPENDENT_TEST_SUITE.md` §4 逐字：「测试集可以接入 CI，但**以非阻塞
为常态**」。

⇒ **默认退出码恒为 `0`。** 红项以 `FAIL` 表打印，供人读；机器只读退出码时拿不到任何
「阻断」语义，这是刻意的。

`--exit-code` 是**开发期自查**开关（本单用它自证负例能红），它**不得**被接进 CI；
接 CI 的方式由 T12 统一设计，届时也只取 warn，不取阻塞。

用法：

    python3 -m eng.tests.unit.run_unit                 # 报告，退出码恒 0
    python3 -m eng.tests.unit.run_unit --exit-code     # 开发期自查，退出码 = 红项数
    python3 -m eng.tests.unit.run_unit --only drizzle  # 只跑 id 含该词的用例
    python3 -m eng.tests.unit.run_unit --verbose       # 打印每条用例的实测读数
"""

from __future__ import annotations

import argparse
import os
import sys

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

from eng.tests.unit import harness  # noqa: E402


def _print_header(cases) -> None:
    n = len(cases)
    print("=" * 78)
    print("ACSD 单元层测试集 · 报告（不是门禁，不裁决代码）")
    print("=" * 78)
    print(f"用例数: {n}   "
          f"正例 {sum(1 for c in cases if c.kind == harness.POSITIVE)}   "
          f"负例 {sum(1 for c in cases if c.kind == harness.NEGATIVE)}")
    print("-" * 78)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="单元层测试集报告器（非阻塞）")
    ap.add_argument("--exit-code", action="store_true",
                    help="开发期自查：退出码 = 红项数。不得接进 CI。")
    ap.add_argument("--only", default=None, help="只跑 id 含该子串的用例")
    ap.add_argument("--verbose", action="store_true", help="打印每条用例的实测读数")
    ap.add_argument("--list", action="store_true", help="只列出用例元数据，不执行")
    args = ap.parse_args(argv)

    harness.discover()
    cases = harness.registered()
    if args.only:
        cases = [c for c in cases if args.only in c.id]
    if not cases:
        print("没有匹配的用例。", file=sys.stderr)
        return 0

    if args.list:
        for c in cases:
            print(f"[{c.kind:8s}] {c.id}")
            print(f"    意图   : {c.intent}")
            print(f"    输入   : {c.inputs}")
            print(f"    预期   : {c.expected}")
            print(f"    来源   : {c.source}")
            print(f"    判据   : {', '.join(c.criteria) or '—'}")
            if c.kind == harness.NEGATIVE:
                print(f"    注入   : [{c.defect_id}] {c.inject}")
        return 0

    _print_header(cases)
    results = harness.run_all() if not args.only else _run_subset(cases)
    fails = [r for r in results if not r.passed]
    pos = [r for r in results if r.case.kind == harness.POSITIVE]
    neg = [r for r in results if r.case.kind == harness.NEGATIVE]

    for r in results:
        mark = "PASS" if r.passed else "FAIL"
        print(f"[{mark}] {r.case.id}  ({r.seconds * 1e3:.1f} ms)")
        if args.verbose or not r.passed:
            print(f"    意图 : {r.case.intent}")
            if r.case.kind == harness.NEGATIVE:
                print(f"    注入 : [{r.case.defect_id}] {r.case.inject}")
            if r.evidence.entries:
                print("    实测读数:")
                print(r.evidence.dump(indent="      "))
            if not r.passed:
                print(f"    红项 : {r.detail}")

    print("-" * 78)
    print(f"合计 {len(results)}  通过 {len(results) - len(fails)}  红 {len(fails)}"
          f"   （正例 {len(pos)} / 负例 {len(neg)}）")
    print("提醒：本层是工具不是裁判。红项是诊断，不是「不得合入」的结论。")
    if fails:
        print(f"\n红项清单:")
        for r in fails:
            print(f"  - {r.case.id}: {r.detail.splitlines()[0] if r.detail else ''}")

    if args.exit_code:
        return len(fails)
    return 0


def _run_subset(cases):
    import time
    from eng.tests.unit.harness import CheckFailure, Evidence, _current_evidence, Result
    import traceback
    out = []
    for case in cases:
        ev = Evidence()
        _current_evidence.append(ev)
        t0 = time.perf_counter()
        try:
            case.func()
            ok, detail = True, ""
        except CheckFailure as e:
            ok, detail = False, str(e)
        except Exception as e:  # noqa: BLE001
            ok = False
            detail = f"未预期异常 {type(e).__name__}: {e}\n{traceback.format_exc()}"
        finally:
            dt = time.perf_counter() - t0
            _current_evidence.pop()
        out.append(Result(case=case, passed=ok, detail=detail, evidence=ev, seconds=dt))
    return out


if __name__ == "__main__":
    raise SystemExit(main())