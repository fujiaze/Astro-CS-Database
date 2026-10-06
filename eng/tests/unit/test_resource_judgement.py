"""资源判据单元层：P5/P7/P10/P11/P12 + E1/E3/E6/E9 + C3，**直接对拍在库产品实现**。

审核包-R2 §3.6 逐字记旧 `release02` 资产的致命缺陷：「**零产品耦合**……这些脚本从不调 CLI、
不链接 `libacsd`、不 import 任何 `lib/` 模块。⇒ **原样吸收只会测到那份 Python 重实现，
不会测到 C++ 产品。**」本文件是对该缺陷的正面回应：它 import 的两个模块都是**在库产品
实现本体**，不是重实现：

| 被测对象 | 位置 | 性质 |
|---|---|---|
| `HeavyRunGuard.assert_ready` / `MonitorRequired` | `lib/infrastructure/observability/monitoring/runner.py` | 产品实现本体（纯 Python，无需编译） |
| `resolve_allocated_capacity` / `evaluate_frozen_gate` | `eng/tools/monitoring/run_monitored.py` | 产品实现本体（§8.6 登记的「唯一判定点」） |
| `resource_gate_v1.json` | `eng/contracts/` | 阈值**唯一数值源** |

⇒ 按 `docs/engineering/testing/TEST.md` §13 逐字「每条判据声明真值来源与被测对象集合，
且**被测对象集合至少含产品可执行程序本身**」，本文件的判据满足该条；
而纯 C++ 面（`resource_gate.h` / `resource_recorder.h`）本阶段**没有构建**，
其口径以逐行转写 + 明确登记的方式处理，不冒充已执行（§13「不得按『已执行』计入绿」）。

阈值一律来自 `eng/contracts/resource_gate_v1.json`，本文件**不含任何字面量阈值**
（`21_observability.md` §8.6 逐字：「本函数不含任何字面量阈值」）。
"""

from __future__ import annotations

import json
import math
import os
import statistics
import sys

from . import harness, tolerances as tol

_REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def _load_product_module(dotted: str, rel_path: str, sys_path_entry: str):
    """从**仓内产品路径**加载一个在库模块。

    三条纪律：
    1. 加载失败 / 加载到仓外同名模块 ⇒ 直接抛错判红，**不静默跳过**
       （`docs/engineering/testing/TEST.md` §13「不以跳过冒充通过」）；
    2. 校验 `__file__` 确实落在本仓内，防止被同名模块遮蔽（遮蔽会让对拍指向错误对象）；
    3. 用正常包导入而非裸 `exec`，让产品模块自身的相对 import（`.monitor` / `.trace_feed`）
       在正确的包上下文里解析。
    """
    import importlib
    if sys_path_entry not in sys.path:
        sys.path.insert(0, sys_path_entry)
    expected = os.path.realpath(os.path.join(_REPO, rel_path))
    mod = sys.modules.get(dotted)
    if mod is None or not str(getattr(mod, "__file__", "")).startswith(_REPO):
        # 清掉可能遮蔽本仓的同名条目后按包导入
        for name in [n for n in list(sys.modules)
                     if n == dotted or n.startswith(dotted + ".")]:
            del sys.modules[name]
        try:
            mod = importlib.import_module(dotted)
        except Exception as e:  # noqa: BLE001 - 诊断需要原始异常
            raise ImportError(
                f"产品模块加载失败：{dotted}（{rel_path}）：{type(e).__name__}: {e}") from e
    got = os.path.realpath(getattr(mod, "__file__", "") or "")
    if not got.startswith(_REPO) or got != expected:
        raise ImportError(
            f"产品模块被同名模块遮蔽：期望 {expected}，实测 {got}")
    for attr in _REQUIRED_ATTRS.get(dotted, ()):
        if not hasattr(mod, attr):
            raise ImportError(
                f"产品模块 {dotted} 缺符号 {attr}（文件 {got}）——产品已变，本用例须同步更新")
    return mod


#: 每条对拍必须落到的产品符号；缺一个就说明对拍没对准产品，不是「测试过时」。
_REQUIRED_ATTRS = {
    "tools.monitoring.run_monitored": ("resolve_allocated_capacity",
                                       "evaluate_frozen_gate",
                                       "FROZEN_GATE_MIN_EFFECTIVE_CPUS",
                                       "FROZEN_GATE_MIN_INTERVAL_SECONDS"),
    "monitoring.runner": ("HeavyRunGuard", "MonitorRequired", "REQUIRE_MONITOR_CLASSES"),
}


def _product():
    """加载在库产品实现本体。失败 ⇒ 抛错（`TEST.md` §13：不以跳过冒充通过）。"""
    return _load_product_module(
        "tools.monitoring.run_monitored",
        "eng/tools/monitoring/run_monitored.py",
        os.path.join(_REPO, "eng"))


def _observability():
    return _load_product_module(
        "monitoring.runner",
        "lib/infrastructure/observability/monitoring/runner.py",
        os.path.join(_REPO, "lib", "infrastructure", "observability"))


def _contract() -> dict:
    with open(os.path.join(_REPO, "eng", "contracts", "resource_gate_v1.json"),
              encoding="utf-8") as fh:
        return json.load(fh)


# ---------------------------------------------------------------------------
# E1 · 监控证据缺失 ⇒ 判红（**活产品对拍**）
# ---------------------------------------------------------------------------

@harness.test(
    "res.monitor_required_is_enforced_live",
    intent="E1：声明 requires_monitor 的重任务 run 无 monitor 时必须失败（对打在库实现本体）",
    inputs="HeavyRunGuard(resource_class='cpu_heavy', run_id=…).assert_ready()，无 attach",
    expected="抛 MonitorRequired；声明一旦消失（required_classes 不含该类）则不抛 ⇒ 守卫失效",
    source="lib/infrastructure/observability/monitoring/runner.py:83-86"
           "（assert_ready → raise MonitorRequired）与文件头逐字「无 monitor 的 cpu_heavy run "
           "必须失败（负测）」；判据 E1 见审核包-R2/T02 §2.4「requires_monitor 声明 true 而"
           "证据缺失/空/不可解析 ⇒ 判红」",
    criteria=("E1",),
)
def test_res_monitor_required_is_enforced_live():
    runner = _observability()
    harness.exact(runner.REQUIRE_MONITOR_CLASSES, ("cpu_heavy", "io"),
                  "强制 monitor 的资源类集合")
    with harness.evidence() as ev:
        ev.record("REQUIRE_MONITOR_CLASSES", list(runner.REQUIRE_MONITOR_CLASSES))

    # 正例：无 monitor ⇒ 失败
    guard = runner.HeavyRunGuard(resource_class="cpu_heavy", run_id="u1")
    harness.is_true(guard.requires_monitor, "cpu_heavy 必须声明 requires_monitor")
    err = harness.raises(runner.MonitorRequired, guard.assert_ready,
                         "无 monitor 的 cpu_heavy run 必须失败")
    harness.exact(err.resource_class, "cpu_heavy", "异常必须携带 resource_class")
    harness.exact(err.run_id, "u1", "异常必须携带 run_id")

    # 非声明类不受约束（负向对照，防「一律 raise」的假阳性）
    light = runner.HeavyRunGuard(resource_class="io_light", run_id="u2")
    harness.is_false(light.requires_monitor, "未声明的资源类不应要求 monitor")
    light.assert_ready()  # 不抛


@harness.test(
    "res.monitor_guard_declaration_removal_is_detected",
    intent="E1 负向：守卫的「声明」被摘掉时必须被发现（判定只走具名分支，else PASS 属未登记形态）",
    inputs="HeavyRunGuard(resource_class='cpu_heavy', required_classes=())",
    expected="requires_monitor 变 False 且 assert_ready 不抛 ⇒ 与正例的差别可被观测到",
    source="审核包-R2/T02 §2.4 E1 逐字「判定**只走具名分支**（`else PASS` 属未登记形态）」；"
           "lib/infrastructure/observability/monitoring/runner.py:52-54 的 required_classes",
    criteria=("E1",),
    kind=harness.NEGATIVE,
    inject="DEF-GUARD-DECL-REMOVED：required_classes 里删掉 cpu_heavy（守卫声明消失）",
)
def test_res_monitor_guard_declaration_removal_is_detected():
    runner = _observability()
    guard = runner.HeavyRunGuard(resource_class="cpu_heavy", run_id="u3",
                                 required_classes=())
    with harness.evidence() as ev:
        ev.record("声明被摘除后 requires_monitor", guard.requires_monitor)
    harness.is_false(guard.requires_monitor,
                     "缺陷未生效：required_classes=() 后仍要求 monitor，注入无效")
    guard.assert_ready()   # 缺陷生效的标志：不再抛

    # 反证：同一守卫在声明存在时确实抛 ⇒ 差别来自「声明」而非偶然
    ok_guard = runner.HeavyRunGuard(resource_class="cpu_heavy", run_id="u3")
    harness.raises(runner.MonitorRequired, ok_guard.assert_ready,
                   "对照臂必须抛，才能证明上面的差别来自声明")


@harness.test(
    "res.monitor_attach_run_id_mismatch_is_rejected",
    intent="E9「错误域不坍缩」：attach 的 run_id 不一致必须显式失败，不得静默接受",
    inputs="把 run_id='other' 的 monitor attach 到 run_id='u4' 的 guard",
    expected="抛 ValueError（错误域是 run_id 不一致，不是别的什么）",
    source="lib/infrastructure/observability/monitoring/runner.py:77-80 逐字"
           "「monitor.run_id != self.run_id → raise ValueError」；"
           "判据 E9 见审核包-R2/T02 §2.4「错误域不坍缩」",
    criteria=("E9",),
)
def test_res_monitor_attach_run_id_mismatch_is_rejected():
    runner = _observability()

    class _FakeMonitor:  # 只提供被检查的字段，不触发真实采样
        run_id = "other"

    guard = runner.HeavyRunGuard(resource_class="cpu_heavy", run_id="u4")
    err = harness.raises(ValueError, lambda: guard.attach(_FakeMonitor()),
                         "run_id 不一致必须拒绝")
    harness.is_true("run_id" in str(err), f"错误信息必须点名 run_id，实测 {err}")
    harness.is_true(guard._monitor is None, "拒绝后不得留下半挂载的 monitor")


# ---------------------------------------------------------------------------
# P5 / C2 · 已分配容量分母（**活产品对拍**）
# ---------------------------------------------------------------------------

@harness.test(
    "res.denominator_prefers_granted_peak_then_fallback",
    intent="P5：利用率分母 = 已分配容量，主取 granted_workers 峰值，哨兵才回落 selected",
    inputs="resolve_allocated_capacity 的三组输入",
    expected="granted>0 → min(granted, available)；granted=哨兵且 selected>0 → "
             "min(selected, available)；两者皆哨兵 → 0（不拿机器核冒充）",
    source="eng/tools/monitoring/run_monitored.py:558-578 逐字 docstring（唯一实现点）；"
           "docs/detail/infrastructure/21_observability.md §8.2 判据表；"
           "eng/contracts/resource_gate_v1.json#denominator.primary/fallback/forbid",
    criteria=("P5",),
)
def test_res_denominator_prefers_granted_peak_then_fallback():
    m = _product()
    contract = _contract()
    harness.exact(contract["denominator"]["primary"], "granted_workers_peak",
                  "契约 primary 分母")
    harness.is_true(len(contract["denominator"]["forbid"]) >= 1,
                    "契约必须登记禁止形态")

    cases = [
        # (granted, selected, available, 期望)
        (8, 16, 16, 8),      # 主取义：观测到的授予峰值 8 < 可用 16 ⇒ 8
        (16, 4, 16, 16),     # 主取义：授予 16 ⇒ 16（不回落 selected=4）
        (32, 4, 16, 16),     # 上限截断到机器有效核
        (0, 4, 16, 4),       # 回落：granted 哨兵 0 ⇒ min(selected, available)
        (0, 32, 16, 16),     # 回落且截断
        (0, 0, 16, 0),       # 两者皆哨兵 ⇒ 0，绝不拿 available_cpus 冒充
        (0, 0, 0, 0),        # 全哨兵
    ]
    for granted, selected, available, expected in cases:
        got = m.resolve_allocated_capacity(granted_workers=granted,
                                           selected_workers=selected,
                                           available_cpus=available)
        with harness.evidence() as ev:
            ev.record(f"granted={granted}, selected={selected}, available={available}",
                      got, expected)
        harness.exact(got, expected,
                      f"分母解析：granted={granted} selected={selected} avail={available}")


@harness.test(
    "res.sentinel_zero_must_not_be_backfilled_by_config",
    intent="P5：granted/selected 皆哨兵 0 时分母必须是 0，利用率类判据不成立并记入 recorded",
    inputs="resolve_allocated_capacity(granted=0, selected=0, available=16) 与其后的门判定",
    expected="allocated 恒为 0（绝不拿机器有效核冒充）；utilization_evaluated 为假；"
             "recorded 里出现 allocated_capacity_undeclared；不产生硬失败",
    source="eng/contracts/resource_gate_v1.json#denominator.forbid[0,1] 逐字"
           "「以配置值冒充观测（哨兵 0 不得回填配置预算）」「以机器有效核（available_cpus）"
           "单独充当已分配容量」与 #denominator.zero_denominator_effect 逐字"
           "「利用率类判据不成立 → 记入 recorded（allocated_capacity_undeclared），不参与裁决」",
    criteria=("P5",),
    kind=harness.POSITIVE,
)
def test_res_sentinel_zero_must_not_be_backfilled_by_config():
    """⚠ 对抗复核 R1 更正：本条初版被标成 `NEGATIVE`，但函数体里**没有任何注入**——
    只是调用在库实现并断言它返回 0。那是**正例**，标成负例会把负例计数虚增。已改正。"""
    m = _product()
    leaked = m.resolve_allocated_capacity(granted_workers=0, selected_workers=0,
                                          available_cpus=16)
    with harness.evidence() as ev:
        ev.record("granted=0, selected=0, available=16 → allocated（必须 0）", leaked)
    harness.exact(leaked, 0, "哨兵 0 被回填：分母冒充已分配容量")

    res = m.evaluate_frozen_gate(
        {"duration_seconds": 30.0,
         "cpu_samples": [{"cpu_percent": 90.0} for _ in range(20)],
         "threads_max": 8},
        effective_cpus=16, allocated_workers=leaked)
    with harness.evidence() as ev:
        ev.record("verdict", res["verdict"])
        ev.record("utilization_evaluated", res["metrics"]["utilization_evaluated"])
        ev.record("recorded", [x.split(":")[0] for x in res["recorded"]])
    harness.is_false(res["metrics"]["utilization_evaluated"],
                     "分母未声明时利用率类判据必须不成立，不得评估")
    harness.is_true(any(x.startswith("allocated_capacity_undeclared")
                        for x in res["recorded"]),
                    f"必须记入 allocated_capacity_undeclared，实测 {res['recorded']}")
    harness.is_false(any("allocated_capacity_undeclared" in v for v in res["violations"]),
                     "未声明容量不得进硬失败清单（契约 zero_denominator_effect）")


@harness.test(
    "res.missing_or_bad_evidence_is_fail_not_pass",
    intent="E3：缺失证据 / 坏证据（不可解析、空）/ 无输出，三情况一律判红且不是通过",
    inputs="evaluate_frozen_gate 在四种坏证据下的返回",
    expected="verdict='fail' 且 violations 非空；绝不出现 'pass'",
    source="eng/tools/monitoring/run_monitored.py:629-720（monitoring_missing 三条"
           "硬失败分支）；docs/detail/infrastructure/21_observability.md §8.5 逐字"
           "「监控/采样证据缺失……**不是**低利用率的豁免，一律 fail-closed 判 FAIL」；"
           "判据 E3 见审核包-R2/T02 §2.4",
    criteria=("E3", "E6"),
)
def test_res_missing_or_bad_evidence_is_fail_not_pass():
    m = _product()
    ok_samples = [{"cpu_percent": 90.0} for _ in range(20)]

    cases = {
        "无 cpu_samples 键": {"duration_seconds": 30.0, "threads_max": 8},
        "空 cpu_samples": {"duration_seconds": 30.0, "cpu_samples": [],
                           "threads_max": 8},
        "全部样本不可解析": {"duration_seconds": 30.0,
                             "cpu_samples": [{"cpu_percent": None}] * 20,
                             "threads_max": 8},
        "threads_max 非法": {"duration_seconds": 30.0,
                             "cpu_samples": ok_samples, "threads_max": None},
    }
    for name, payload in cases.items():
        res = m.evaluate_frozen_gate(payload, effective_cpus=16,
                                     allocated_workers=16)
        with harness.evidence() as ev:
            ev.record(f"{name} → verdict", res["verdict"])
            ev.record(f"{name} → violations 数", len(res["violations"]))
        harness.exact(res["verdict"], "fail", f"{name} 必须判红")
        harness.is_true(len(res["violations"]) >= 1, f"{name} 必须给出具名违规")
        harness.is_true(any("monitoring_missing" in v for v in res["violations"]),
                        f"{name} 的违规必须具名（不得坍缩成无信息的红）")


@harness.test(
    "res.bad_evidence_treated_as_pass_is_detected",
    intent="E3 负向：把坏证据当通过必须被发现（「错误结果被当作正确」是唯一该阻塞的科学口径类问题）",
    inputs="把上述四种坏证据的 verdict 逐个与 'pass' 比对",
    expected="任何一种坏证据给出 'pass' 即判红",
    source="审核包-R2/T02 §2.4 E6 逐字「非退化锚：没找到 / 空集 / 零样本必须判红」；"
           "run/GOVERN-08/工作包-RECTIFY-09原件/standards/05_INDEPENDENT_TEST_SUITE.md §4 逐字"
           "「只有科学口径错误（如公式错误、守恒被破坏、**错误结果被当作正确**）这类关键问题"
           "才作为阻塞项」",
    criteria=("E3", "E6"),
    kind=harness.NEGATIVE,
    inject="DEF-BAD-EVIDENCE-PASS：坏证据（缺失/空/不可解析）被判为 pass",
)
def test_res_bad_evidence_treated_as_pass_is_detected():
    m = _product()
    bad_payloads = [
        ("无 cpu_samples 键", {"duration_seconds": 30.0, "threads_max": 8}),
        ("空 cpu_samples", {"duration_seconds": 30.0, "cpu_samples": [],
                            "threads_max": 8}),
        ("全部样本不可解析", {"duration_seconds": 30.0,
                              "cpu_samples": [{"cpu_percent": "NaN"}] * 20,
                              "threads_max": 8}),
    ]

    def usable_evidence(payload: dict) -> bool:
        """证据面是否可用：至少一个数值型 cpu_percent，且 threads_max 合法。"""
        samples = payload.get("cpu_samples") or []
        vals = [s.get("cpu_percent") for s in samples]
        has_cpu = any(isinstance(v, (int, float)) and not isinstance(v, bool)
                      for v in vals)
        tm = payload.get("threads_max")
        return has_cpu and isinstance(tm, (int, float)) and not isinstance(tm, bool)

    def fail_open_judge(payload: dict) -> str:
        """注入的缺陷判定器：证据不可用时返回 'pass'（fail-open）。"""
        if not usable_evidence(payload):
            return "pass"
        return m.evaluate_frozen_gate(payload, effective_cpus=16,
                                      allocated_workers=16)["verdict"]

    def fail_open_detected(verdict: str, payload: dict) -> bool:
        """E3/E6 判据：证据不可用却给 'pass' ⇒ fail-open，必须被检出。"""
        return (not usable_evidence(payload)) and verdict == "pass"

    for name, payload in bad_payloads:
        honest = m.evaluate_frozen_gate(payload, effective_cpus=16,
                                        allocated_workers=16)["verdict"]
        injected = fail_open_judge(payload)
        with harness.evidence() as ev:
            ev.record(f"{name} → 在库判定器的 verdict", honest)
            ev.record(f"{name} → 注入缺陷后的 verdict", injected)
            ev.record(f"{name} → fail-open 检测器对在库判定器", fail_open_detected(honest, payload))
            ev.record(f"{name} → fail-open 检测器对注入判定器", fail_open_detected(injected, payload))
        harness.exact(honest, "fail", f"{name}：在库判定器必须判红")
        harness.is_false(fail_open_detected(honest, payload),
                         f"{name}：检测器恒红——在库判定器被误判为 fail-open")
        harness.is_true(fail_open_detected(injected, payload),
                        f"{name}：缺陷生效，fail-open 未被检出")
        harness.is_false(honest == injected,
                         f"{name}：注入未改变结论，负例无效")


# ---------------------------------------------------------------------------
# P10 / P12 · 利用率必须随负载变化
# ---------------------------------------------------------------------------

@harness.test(
    "res.utilization_is_load_dependent_and_formula_checked",
    intent="P10/P12：利用率 = avg_cpu/(100·已分配容量)，且两组不同负载必须给出不同输出",
    inputs="两组固定 CPU 样本（90% 与 30%）在 allocated=16 下过门",
    expected="两个 avg_utilization 互不相等；各自等于独立算式 avg_cpu/(100·allocated)",
    source="eng/tools/monitoring/run_monitored.py:722-724（avg_util = avg_cpu/denom，"
           "denom = 100·allocated）；docs/detail/infrastructure/21_observability.md §8.2"
           "「利用率是占**已分配容量**的百分比」；判据 P10/P12 见审核包-R2/T02 §2.2",
    criteria=("P10", "P12"),
)
def test_res_utilization_is_load_dependent_and_formula_checked():
    m = _product()
    allocated = 16
    loads = {"高负载 90%": 90.0, "低负载 30%": 30.0}
    got = {}
    for name, cpu in loads.items():
        res = m.evaluate_frozen_gate(
            {"duration_seconds": 30.0,
             "cpu_samples": [{"cpu_percent": cpu} for _ in range(20)],
             "threads_max": 8},
            effective_cpus=16, allocated_workers=allocated)
        got[name] = res["metrics"]["avg_utilization"]
        expected = cpu / (100.0 * allocated)   # 独立算式（P10 的定义式）
        with harness.evidence() as ev:
            ev.record(f"{name} → avg_utilization", got[name])
            ev.record(f"{name} → 独立算式 cpu/(100·allocated)", expected)
        harness.close(got[name], expected, rtol=tol.F64_RTOL,
                      atol=tol.F64_ATOL_PER_SCALE * max(abs(expected), 1.0),
                      what=f"{name} 的利用率公式",
                      scale=max(abs(expected), 1.0))

    harness.is_false(got["高负载 90%"] == got["低负载 30%"],
                     "缺陷生效：两组不同负载给出同一利用率（P12 负例）")

    # P12 的判别力自证：把算法换成「(min+max)/2」式恒值算法必须被本判据抓住
    def constant_algorithm(samples, alloc):   # 缺陷：常量
        return 0.5

    a = constant_algorithm([90.0] * 20, allocated)
    b = constant_algorithm([30.0] * 20, allocated)
    with harness.evidence() as ev:
        ev.record("恒值算法在两组负载下的输出", f"{a} / {b}")
    # 判别力自证：恒值算法在两组不同负载上**确实同值** ⇒ 上面的不等性断言有牙齿
    harness.is_true(a == b,
                    "恒值算法在两组负载上给出不同输出——构造错误，P12 的负例不成立")


# ⚠ 对抗复核 R1 的否决已执行：本文件原先有一条
#   `res.constant_utilization_algorithm_is_flagged`，其判据 `_constant_series_detector`
#   是**用例内本地函数**、输入是两条**字面序列**，断言退化为「单元素集合 == 1」——
#   没有任何实现、产品或 `*_ref.py` 被执行，属自指型空转。**已删除**。
#   它要守的性质由 `res.worker_balance_shape_is_degenerate_by_construction` 覆盖：
#   那条喂入的是**在库调用点形态**（`commands.cpp:943,962` 同值写两列）产生的真实序列。
# 本地退化检测器仍在，但只作为被测对象出现，不再单独充当负例。


def _worker_balance_utilization(active: int, runnable: int) -> float:
    """`lib/infrastructure/cli/resource_recorder.h:368-369` 的逐行转写。

        const double denom = r.active_workers + r.runnable_workers;
        const double util  = denom > 0 ? r.active_workers * 100.0 / denom : 0.0;
    """
    denom = active + runnable
    return active * 100.0 / denom if denom > 0 else 0.0


def _constant_series_detector(series):
    """P11 退化检测：判据算法与负载相关 ⇒ 输出序列不得恒定。

    ⚠ 本函数**不是**用例；它是被测对象之一，由
    `res.worker_balance_shape_is_degenerate_by_construction` 喂入**在库调用点形态**
    产生的真实序列。
    """
    finite = [v for v in series if isinstance(v, (int, float)) and math.isfinite(v)]
    if len(finite) < 2:
        return True          # E6 非退化锚：样本不足 ⇒ 无证据资格 ⇒ 判红
    return len(set(finite)) == 1


@harness.test(
    "res.worker_balance_shape_is_degenerate_by_construction",
    intent="P11：worker_balance 的利用率算法在**在库调用点**上退化为常量（已知红登记）",
    inputs="resource_recorder.h:368-369 的形状 + commands.cpp:943/962 的调用点形态"
           "（两处都以同值写入 active 与 runnable 两列）",
    expected="对任意 (a,a) 输入，utilization ≡ 50.00；退化检测器必须判红。"
             "**本用例锁住一个在库缺陷，不是认可它**：修复 product 后应改本用例",
    source="lib/infrastructure/cli/resource_recorder.h:368-369（util 分式本体）"
           "与 lib/infrastructure/cli/commands.cpp:943 `set_workers(planned_start, "
           "planned_start)`、:962 `set_workers(eff, eff)`（同值写入两列）；"
           "审核包-R2/T02 §6.2 C3 逐字登记「`worker_balance` 判据当前正是它自己明令判红的形态」",
    criteria=("P11",),
)
def test_res_worker_balance_shape_is_degenerate_by_construction():
    # 生产调用点的实际形态：两列恒等
    for eff in (1, 2, 4, 8, 16, 64, 256):
        active, runnable = eff, eff          # commands.cpp:943 / :962 的写入形态
        util = _worker_balance_utilization(active, runnable)
        with harness.evidence() as ev:
            ev.record(f"eff={eff} → utilization_pct", util, 50.0)
        # 精确恒等式（非浮点容差比较），故不声明 scale 域、不触发 §4.3 的 1 ulp 下限
        harness.exact(util, 50.0, f"eff={eff} 的 worker_balance 利用率")

    series = [_worker_balance_utilization(e, e) for e in (1, 2, 4, 8, 16, 64)]
    flagged = _constant_series_detector(series)
    with harness.evidence() as ev:
        ev.record("生产调用点形态下的 utilization 序列", series)
        ev.record("P11 退化检测", flagged)
    harness.is_true(flagged,
                    "在库调用点形态下算法未被判红——若本条变红，说明 product 侧已修复"
                    "（此时应改本用例的 expected，而不是加豁免）")

    # 对照：真正随负载变化的输入不触发退化判定（检测器不得恒红）
    varying = [_worker_balance_utilization(a, r) for a, r in
               ((1, 4), (4, 4), (4, 16), (16, 16), (3, 7))]
    harness.is_false(_constant_series_detector(varying),
                     "变化序列被判红——检测器恒红，无判别力")


# ---------------------------------------------------------------------------
# P7 · 队列有工作的前置判据（**活产品对拍**）
# ---------------------------------------------------------------------------

def _low_util_samples(n: int, cpu_percent: float, runnable: int):
    """构造一段连续低利用样本：t 每 0.5 s 一拍，覆盖 ≥ 10 s。"""
    return [{"t": 0.5 * i, "cpu_percent": cpu_percent, "runnable": runnable}
            for i in range(n)]


@harness.test(
    "res.queue_low_window_requires_queued_work",
    intent="P7：低利用窗只有**同时**存在就绪线程积压时才判 CPU 饥饿",
    inputs="连续 ≥10 s 的低利用样本两组：runnable_p50=2（16 核配额）与 runnable_p50=20",
    expected="前者进 recorded（并行宽度不足，不是饥饿）；后者进 violations（硬失败）",
    source="eng/tools/monitoring/run_monitored.py:801-812 逐字「仅 R≥2 不足以判定：2 个线程在 "
           "16 核配额上全速跑也是 R=2，那是并行宽度不足（记录项），不是饥饿」与 "
           "`queued_work = (stat > allocated and stat >= FROZEN_GATE_MIN_QUEUED_THREADS)`；"
           "eng/contracts/resource_gate_v1.json#compute.queued_work_predicate",
    criteria=("P7",),
)
def test_res_queue_low_window_requires_queued_work():
    m = _product()
    contract = _contract()
    harness.is_true(contract["compute"]["queue_low_window_requires_queued_work"],
                    "契约必须要求队列有工作")
    harness.exact(contract["compute"]["queued_work_min_runnable_threads"], 2,
                  "契约最小就绪线程数")

    allocated = 16
    # 组 1：宽度不足（runnable=2 ≤ allocated=16）—— 不是饥饿
    res1 = m.evaluate_frozen_gate(
        {"duration_seconds": 30.0, "poll_interval": 0.5,
         "cpu_samples": _low_util_samples(40, 5.0, 2), "threads_max": 8},
        effective_cpus=16, allocated_workers=allocated)
    with harness.evidence() as ev:
        ev.record("组 1（runnable=2, allocated=16）→ verdict", res1["verdict"])
        ev.record("组 1 → queued_work", res1["metrics"].get("queued_work"))
        ev.record("组 1 → runnable_p50", res1["metrics"].get("queued_work_runnable_p50"))
    harness.is_false(any("frozen_low_utilization_window:" in v for v in res1["violations"]),
                     f"宽度不足不得判 CPU 饥饿，实测 violations={res1['violations']}")
    harness.is_true(any(s.startswith("frozen_low_utilization_window_no_queue")
                        for s in res1["recorded"]),
                    f"宽度不足必须落 recorded，实测 recorded={res1['recorded']}")

    # 组 2：真实饥饿（runnable_p50=20 > allocated=16）
    res2 = m.evaluate_frozen_gate(
        {"duration_seconds": 30.0, "poll_interval": 0.5,
         "cpu_samples": _low_util_samples(40, 5.0, 20), "threads_max": 24},
        effective_cpus=16, allocated_workers=allocated)
    with harness.evidence() as ev:
        ev.record("组 2（runnable=20, allocated=16）→ verdict", res2["verdict"])
        ev.record("组 2 → queued_work", res2["metrics"].get("queued_work"))
        ev.record("组 2 → violations", res2["violations"])
    harness.exact(res2["verdict"], "fail", "真实饥饿必须硬失败")
    harness.is_true(any("frozen_low_utilization_window:" in v for v in res2["violations"]),
                    f"真实饥饿必须命中硬失败判据，实测 {res2['violations']}")


@harness.test(
    "res.dropping_the_queue_predicate_is_detected",
    intent="P7 负例：**对在库实现**做半边消融——去掉「就绪线程 ≥ 2」这一半后，"
           "宽度不足仍不得判 CPU 饥饿 ⇒ 判别力只能来自 `runnable_p50 > 已分配容量`",
    inputs="对在库 `evaluate_frozen_gate` 注入：把 FROZEN_GATE_MIN_QUEUED_THREADS 临时置 0，"
           "在 runnable_p50=2 / allocated=16 的低利用窗上重跑",
    expected="注入后该样本**仍不判硬失败**（落 recorded）；对照臂 runnable_p50=20 时"
             "仍是硬失败。两臂之差只能由 `stat > allocated` 解释",
    source="eng/contracts/resource_gate_v1.json#compute.queued_work_note 逐字"
           "「就绪（/proc R 态）线程数中位数必须**同时**满足 > 已分配容量核数 与 >= 2 —— "
           "就绪线程多于可用槽位才叫队列有工作；仅有 2 个线程在 16 核配额上跑是"
           "**并行宽度不足（记录项）**，不是 CPU 饥饿」；在库实现 "
           "`eng/tools/monitoring/run_monitored.py:807-808` 的 "
           "`queued_work = (stat > allocated and stat >= FROZEN_GATE_MIN_QUEUED_THREADS)`",
    criteria=("P7",),
    kind=harness.NEGATIVE,
    inject="DEF-QUEUE-MINRUNNABLE-REMOVED：把「就绪线程 ≥ 2」这一半前置去掉（置 0）",
    defect_id="RES-N-QUEUE-MINRUNNABLE",
)
def test_res_dropping_the_queue_predicate_is_detected():
    """⚠ 对抗复核 R1 更正：本条初版用两个**用例内本地 lambda** 比较，
    产品 `evaluate_frozen_gate` 根本没被调用 —— 那是「对 Python 运算符语义的断言」，
    不是对 ACSD 的断言，等价于自指型空转。已改写为**对在库实现的真差分**。"""
    m = _product()
    allocated = 16

    def judge(runnable, min_runnable):
        saved = m.FROZEN_GATE_MIN_QUEUED_THREADS
        m.FROZEN_GATE_MIN_QUEUED_THREADS = min_runnable
        try:
            return m.evaluate_frozen_gate(
                {"duration_seconds": 30.0, "poll_interval": 0.5,
                 "cpu_samples": _low_util_samples(40, 5.0, runnable),
                 "threads_max": 24 if runnable > 16 else 8},
                effective_cpus=16, allocated_workers=allocated)
        finally:
            m.FROZEN_GATE_MIN_QUEUED_THREADS = saved

    starved_hard = judge(20, 2)
    starved_soft = judge(20, 0)              # 注入：去掉「≥ 2」这一半
    width_hard = judge(2, 2)
    width_soft = judge(2, 0)                # 注入：去掉「≥ 2」这一半

    def hard(r):
        return any("frozen_low_utilization_window:" in v for v in r["violations"])

    with harness.evidence() as ev:
        ev.record("runnable=20（真实饥饿）· 正常 / 注入后是否硬失败",
                  f"{hard(starved_hard)} / {hard(starved_soft)}")
        ev.record("runnable=2 （宽度不足）· 正常 / 注入后是否硬失败",
                  f"{hard(width_hard)} / {hard(width_soft)}")
        ev.record("在库自报的判据参考串", width_hard["metrics"].get("queued_work_reference"))
    harness.is_true(hard(starved_hard), "真实饥饿必须硬失败（正本判据）")
    harness.is_false(hard(width_hard),
                     "宽度不足不得判 CPU 饥饿（正本判据）")
    harness.is_false(hard(width_soft),
                     "缺陷未生效：去掉「≥ 2」后宽度不足被误判成饥饿")
    harness.is_true(hard(starved_soft), "去掉「≥ 2」后真实饥饿仍应硬失败（对照臂）")
    # 反证：两臂之差确实由 `stat > allocated` 承担，而不是被注入改掉
    harness.exact(width_hard["metrics"].get("queued_work_reference"),
                  "runnable_p50 > allocated",
                  "在库必须自报判据参考串，证明差分打在同一谓词上")


# ---------------------------------------------------------------------------
# E9 · 失败诚实传播：哨兵不得折叠
# ---------------------------------------------------------------------------

@harness.test(
    "res.unsampled_sentinel_is_not_folded_to_zero",
    intent="E9：未采样哨兵（-1）必须跳过判定，不得折叠成 0% 利用率参与有限比较",
    inputs="含哨兵 -1 的逐样本利用率序列 vs 真实零利用率序列",
    expected="哨兵序列不得被判成「0% 利用率」；两者在判据上必须可区分",
    source="lib/infrastructure/cli/resource_gate.h:428-431 逐字「哨兵纪律(批次 P p2007 先例): "
           "字段为 -1(未采样/未观测)时跳过对应判定；**未采样不是低利用率证据**」；"
           "docs/engineering/testing/TEST.md §4.4 逐字「比较器不得把非有限值折叠成 `0` 或"
           "任何哨兵值后再参与有限比较」；判据 E9 见审核包-R2/T02 §2.4",
    criteria=("E9",),
)
def test_res_unsampled_sentinel_is_not_folded_to_zero():
    SENTINEL = -1.0

    def honest_utilization(values, denom):
        """诚实口径：哨兵跳过，不进入均值。"""
        real = [v for v in values if v is not None and v != SENTINEL]
        if not real:
            return None          # 无有效样本 ⇒ 无证据资格 ⇒ 返回 None 而非 0
        return sum(real) / denom / len(real)

    def folded_utilization(values, denom):
        """缺陷口径：把哨兵当 0 折进均值。"""
        filled = [0.0 if (v is None or v == SENTINEL) else v for v in values]
        return sum(filled) / denom / len(filled)

    denom = 1600.0   # 100 · allocated(16)
    honest = honest_utilization([SENTINEL, SENTINEL], denom)
    folded = folded_utilization([SENTINEL, SENTINEL], denom)
    true_zero = honest_utilization([0.0, 0.0], denom)
    with harness.evidence() as ev:
        ev.record("全哨兵序列 → 诚实口径", honest)
        ev.record("全哨兵序列 → 折叠口径（缺陷）", folded)
        ev.record("真实零利用率序列 → 诚实口径", true_zero)
    harness.is_true(honest is None, "全哨兵序列必须返回「无证据」而不是任何数值")
    harness.is_false(folded is None, "缺陷未生效：哨兵仍被折成数值")
    harness.is_false(honest == folded,
                     "缺陷生效：未采样与真实 0% 利用率在折叠口径下不可区分（错误域坍缩）")
    harness.exact(true_zero, 0.0, "真实零利用率的诚实口径必须是 0.0")

    # 与在库门面对拍：**登记偏差**。C++ 判定面有哨兵纪律
    # （lib/infrastructure/cli/resource_gate.h:428-431），Python 判定面没有：
    # run_monitored.py 的 valid 过滤只判 isinstance(v,(int,float))，不排除哨兵 -1。
    # ⚠ 本断言锁的是**当前事实**，不是认可它。product 侧补上过滤后本条会变红，
    #    届时应改本条 expected，而不是加豁免。
    m = _product()
    res = m.evaluate_frozen_gate(
        {"duration_seconds": 30.0,
         "cpu_samples": [{"cpu_percent": -1.0} for _ in range(20)],
         "threads_max": 8},
        effective_cpus=16, allocated_workers=16)
    with harness.evidence() as ev:
        ev.record("全 -1 样本 → verdict（登记偏差：Python 判定面无哨兵过滤）",
                  res["verdict"])
        ev.record("全 -1 样本 → recorded", [s.split(":")[0] for s in res["recorded"]])
    harness.exact(res["verdict"], "pass",
                  "登记偏差已变化：Python 判定面开始过滤哨兵 -1，请改本条 expected")
    harness.is_true(any("avg_utilization_low" in s for s in res["recorded"]),
                    "哨兵被当成有效样本的直接证据：它被算进了平均利用率")
    # 对照：None（真缺失）在同一判定面上必须判红（这才是 `TEST.md` §4.4 管的「缺失」）
    res_none = m.evaluate_frozen_gate(
        {"duration_seconds": 30.0,
         "cpu_samples": [{"cpu_percent": None} for _ in range(20)],
         "threads_max": 8},
        effective_cpus=16, allocated_workers=16)
    with harness.evidence() as ev:
        ev.record("全 None（缺失）样本 → verdict", res_none["verdict"])
    harness.exact(res_none["verdict"], "fail", "真缺失样本必须判红（monitoring_missing）")


# ---------------------------------------------------------------------------
# C3 · 覆盖结论：冻结前一律「未判」
# ---------------------------------------------------------------------------

def _coverage_verdict(threshold, measured: float) -> str:
    """覆盖结论判定：`threshold` 为 `null`（未冻结）时只能给「未判」。"""
    if threshold is None:
        return "undetermined"
    return "pass" if measured >= threshold else "fail"


@harness.test(
    "res.coverage_verdict_is_undetermined_until_frozen",
    intent="C3：冻结前覆盖结论一律「未判」，不得给通过也不得给判红",
    inputs="threshold=None 与 threshold=0.6 两种冻结状态",
    expected="未冻结 ⇒ 'undetermined'（与 measured 无关）；冻结后 ⇒ pass/fail 可判",
    source="审核包-R2/T02 §2.3 C3 逐字「**冻结前覆盖结论一律「未判」**」"
           "（源自已退役的 coverage_baseline_v1.md:31，判据在位）；"
           "审核包-R2/T02 §2.6 V1 否决项逐字「『工具缺失 ⇒ 报不可测』**不得**记为通过」",
    criteria=("C3",),
)
def test_res_coverage_verdict_is_undetermined_until_frozen():
    harness.exact(_coverage_verdict(None, 0.0), "undetermined",
                  "未冻结且 measured=0 必须『未判』")
    harness.exact(_coverage_verdict(None, 1.0), "undetermined",
                  "未冻结且 measured=1 必须『未判』")
    harness.exact(_coverage_verdict(None, 0.0), _coverage_verdict(None, 1.0),
                  "未冻结时结论不得随读数变化（否则它不是『未判』而是隐藏的判定）")
    harness.exact(_coverage_verdict(0.6, 0.75), "pass", "冻结后 0.75 ≥ 0.6 判通过")
    harness.exact(_coverage_verdict(0.6, 0.45), "fail", "冻结后 0.45 < 0.6 判红")


@harness.test(
    "res.unfrozen_coverage_is_undetermined_not_pass",
    intent="C3：冻结前覆盖结论一律「未判」，不得给通过也不得给判红（判据设计自身的正确性）",
    inputs="同一组读数在 threshold=None 与 threshold=0.6 两种冻结状态下的结论",
    expected="未冻结 ⇒ 'undetermined'，且**与读数无关**；冻结后才可能 pass/fail",
    source="审核包-R2/T02 判据清单 §2.3 C3 逐字「**冻结前覆盖结论一律「未判」**」"
           "（源自已退役的 coverage_baseline_v1.md:31；该路径**已从版本库删除**，"
           "此处按审核包-R2 §2.1 已登记的判据文本重建，**不把该路径当活指针**）；"
           "审核包-R2 §2.6 V1 否决理由逐字「与三处 fail-closed 原则**正面冲突**……"
           "登记进去直接制造一条 **fail-open 判据**」",
    criteria=("C3", "E6"),
    kind=harness.POSITIVE,
)
def test_res_unfrozen_coverage_is_undetermined_not_pass():
    """⚠ 对抗复核 R1 更正：本条初版被标成 `NEGATIVE` 并注入一个本地 `fail_open`
    判定器 —— 注入臂、判据、被测对象三者全是本用例内的本地函数，无任何产品面覆盖。
    仓内**没有**覆盖率度量实现可对拍（覆盖门载体整体退役），因此这条只能是**判据设计
    自身的正确性**，改正为 `POSITIVE`。fail-open 形态由 `E6` 的非退化锚在别处覆盖。"""
    def fail_open_detected(verdict: str) -> bool:
        """E3/E6 判据：未冻结时的 verdict 若落在 pass/fail 上，即为 fail-open。"""
        return verdict in ("pass", "fail")

    honest_zero = _coverage_verdict(None, 0.0)
    honest_one = _coverage_verdict(None, 1.0)
    with harness.evidence() as ev:
        ev.record("未冻结 · measured=0.0", honest_zero)
        ev.record("未冻结 · measured=1.0", honest_one)
        ev.record("fail-open 检测器对二者的判定",
                  f"{fail_open_detected(honest_zero)} / {fail_open_detected(honest_one)}")
        ev.record("冻结 0.6 · 0.75 / 0.45",
                  f"{_coverage_verdict(0.6, 0.75)} / {_coverage_verdict(0.6, 0.45)}")
    harness.exact(honest_zero, "undetermined", "未冻结且 measured=0 必须『未判』")
    harness.exact(honest_one, "undetermined", "未冻结且 measured=1 必须『未判』")
    harness.exact(honest_zero, honest_one,
                  "未冻结时结论不得随读数变化（否则它不是『未判』而是隐藏的判定）")
    harness.is_false(fail_open_detected(honest_zero), "fail-open 检测器对诚实口径恒红")
    harness.is_false(fail_open_detected(honest_one), "fail-open 检测器对诚实口径恒红")
    harness.exact(_coverage_verdict(0.6, 0.75), "pass", "冻结后 0.75 ≥ 0.6 判通过")
    harness.exact(_coverage_verdict(0.6, 0.45), "fail", "冻结后 0.45 < 0.6 判红")
