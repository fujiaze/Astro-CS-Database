#!/usr/bin/env python3
"""合成全链层运行器 —— **报告器，不是裁判**。

`docs/engineering/testing/TEST.md` §9 逐字：「执行者是人：结论由人读对抗审核给出，
每条结论都附可复算的证据。**测试集不产出流水线判决**。」
`run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md` §4 逐字：
「测试集可以接入 CI，但**以非阻塞为常态**」。

⇒ **默认退出码恒为 `0`。** 红项以 `FAIL` 表打印，供人读；机器只读退出码时拿不到任何
「阻断」语义，这是刻意的。

`--exit-code` 是**开发期自查**开关（用它自证负例能红），它**不得**被接进 CI；
接 CI 的方式由 T12 统一设计，届时也只取 warn，不取阻塞。

用法：

    python3 -m eng.tests.synthetic.run_synthetic                  # 报告，退出码恒 0
    python3 -m eng.tests.synthetic.run_synthetic --exit-code      # 开发期自查：退出码 = 红项数
    python3 -m eng.tests.synthetic.run_synthetic --only p1        # 只跑 id 含该词的用例
    python3 -m eng.tests.synthetic.run_synthetic --verbose        # 打印每条用例的实测读数
    python3 -m eng.tests.synthetic.run_synthetic --list           # 只列元数据，不执行

骨架复用：注册表 / 断言 / 证据累积 / 非阻塞裁决复用 `eng.tests.unit.harness`
（**只读复用**，本层一个字都不写进 unit 层；理由与两条不变量见
`eng/tests/synthetic/__init__.py` 的 docstring）。容差冻结表是**本层自己的**
`tolerances.py`，与 unit 层**不同源**。

⚠ 本运行器**不产出判决**：`--exit-code` 之外的任何用法都恒返回 0。
"""

from __future__ import annotations

import argparse
import os
import sys

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _REPO_ROOT not in sys.path:
    sys.path.insert(0, _REPO_ROOT)

from eng.tests.unit import harness as _shared_harness  # noqa: E402
from eng.tests.synthetic import tolerances as _tol  # noqa: E402,F401

#: 本层自己的注册表视图。注册/执行/裁决全部委托给共享骨架，
#: 但**只扫本层**：discover 的包名写死，不接受调用方覆盖。
PACKAGE = "eng.tests.synthetic"


def discover() -> int:
    """导入本层所有 `test_*.py` 模块，触发注册。返回本次新注册的用例数。"""
    return _shared_harness.discover(PACKAGE)


def registered():
    """已注册的用例（来自共享骨架的注册表）。"""
    return _shared_harness.registered()


def verdict(results):
    """裁决词。本层恒为 `warn` —— 逐字引 `05` §4「以非阻塞为常态」。"""
    return _shared_harness.verdict(results)


def exit_code(results, non_blocking: bool = True) -> int:
    """退出码。默认恒 `0`；`non_blocking=False` 只在**开发期自查**时用。"""
    return _shared_harness.exit_code(results, non_blocking)


def _run_all():
    return _shared_harness.run_all()


def _run_subset(cases):
    """跑一个子集。复用骨架的逐例执行与证据挂载（`--only` 用）。"""
    import time
    import traceback

    from eng.tests.unit.harness import (CheckFailure, Evidence, Result,
                                        _current_evidence)
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
        except Exception as e:  # noqa: BLE001 - 用例内部错误也要报出来
            ok = False
            detail = f"未预期异常 {type(e).__name__}: {e}\n{traceback.format_exc()}"
        finally:
            dt = time.perf_counter() - t0
            _current_evidence.pop()
        out.append(Result(case=case, passed=ok, detail=detail, evidence=ev, seconds=dt))
    return out


def _print_header(cases) -> None:
    n = len(cases)
    print("=" * 78)
    print("ACSD 合成全链层测试集 · 报告（不是门禁，不裁决代码）")
    print("=" * 78)
    print(f"用例数: {n}   "
          f"正例 {sum(1 for c in cases if c.kind == _shared_harness.POSITIVE)}   "
          f"负例 {sum(1 for c in cases if c.kind == _shared_harness.NEGATIVE)}")
    print("-" * 78)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="合成全链层测试集报告器（非阻塞）")
    ap.add_argument("--exit-code", action="store_true",
                    help="开发期自查：退出码 = 红项数。不得接进 CI。")
    ap.add_argument("--only", default=None, help="只跑 id 含该子串的用例")
    ap.add_argument("--verbose", action="store_true", help="打印每条用例的实测读数")
    ap.add_argument("--list", action="store_true", help="只列出用例元数据，不执行")
    args = ap.parse_args(argv)

    discover()
    cases = registered()
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
            if c.kind == _shared_harness.NEGATIVE:
                print(f"    注入   : [{c.defect_id}] {c.inject}")
        return 0

    _print_header(cases)
    results = _run_all() if not args.only else _run_subset(cases)
    fails = [r for r in results if not r.passed]
    pos = [r for r in results if r.case.kind == _shared_harness.POSITIVE]
    neg = [r for r in results if r.case.kind == _shared_harness.NEGATIVE]

    for r in results:
        mark = "PASS" if r.passed else "FAIL"
        print(f"[{mark}] {r.case.id}  ({r.seconds * 1e3:.1f} ms)")
        if args.verbose or not r.passed:
            print(f"    意图 : {r.case.intent}")
            if r.case.kind == _shared_harness.NEGATIVE:
                print(f"    注入 : [{r.case.defect_id}] {r.case.inject}")
            if r.evidence.entries:
                print("    实测读数:")
                print(r.evidence.dump(indent="      "))
            if not r.passed:
                print(f"    红项 : {r.detail}")

    print("-" * 78)
    print(f"合计 {len(results)}  通过 {len(results) - len(fails)}  红 {len(fails)}"
          f"   （正例 {len(pos)} / 负例 {len(neg)}）")
    print(f"裁决词: {verdict(results)}（本层恒为 warn：05 §4「以非阻塞为常态」）")
    print("提醒：本层是工具不是裁判。红项是诊断，不是「不得合入」的结论。")
    print("数据依赖: 哈勃仿真臂读 testdata/HST_M16/（.gitignore:167 忽略，不在版本库）；")
    print("          读不到时判据显式失败并记为未执行，不以跳过冒充通过（TEST.md §13）。")
    if fails:
        print("\n红项清单:")
        for r in fails:
            print(f"  - {r.case.id}: {r.detail.splitlines()[0] if r.detail else ''}")

    if args.exit_code:
        return len(fails)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())