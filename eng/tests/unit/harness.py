"""单元层极小测试骨架（纯标准库，零第三方依赖）。

**它是什么**：一个带元数据的用例注册表 + 执行器 + 诊断输出。

**它不是什么**（`docs/engineering/testing/TEST.md` §9 逐字「测试集不产出流水线判决」、
`standards/05_INDEPENDENT_TEST_SUITE.md` §4 逐字「以非阻塞为常态」）：

- 不产出流水线判决。默认退出码恒为 `0`；
- 不裁决代码。红项是**暴露给人看**的诊断，不是准入结论；
- 不做覆盖门、不做分档、不接 CI。

## 正例 / 负例的判定语义（本骨架的关键约定）

| 类型 | 语义 | 变红意味着 |
|---|---|---|
| `positive` | 正确实现必须满足由**独立来源**给出的期望值 | 被测口径被改坏，或期望值被篡改 |
| `negative` | 向被测实现注入一个具名缺陷，**断言独立判据能把它抓住** | **判据没有牙齿**——注入后仍然通过，说明这条判据恒绿 |

⇒ 负例「变红」的正确含义是：**注入缺陷后判据给出超界的读数**（而不是用例失败）。
每条负例都把实测违例量记进 `evidence`，由运行器打印，使「注入后是否真的变红」可被逐条核对。

`docs/engineering/testing/TEST.md` §5 逐字：「负例必须可红：正确实现给绿，注入缺陷给红。」
`standards/05_INDEPENDENT_TEST_SUITE.md` §1 逐字：「测试是工具不是裁判……不用空断言充数。」
`TEST.md` §2 逐字：「每个度量具备非退化判据：真值无效应时度量必须归零或报警。**恒真的比较没有证据资格**。」
"""

from __future__ import annotations

import importlib
import math
import pkgutil
import time
import traceback
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Sequence

from . import tolerances as tol

# ---------------------------------------------------------------------------
# 用例元数据
# ---------------------------------------------------------------------------

POSITIVE = "positive"
NEGATIVE = "negative"

_REGISTRY: List["TestCase"] = []


@dataclass(frozen=True)
class TestCase:
    """一条用例的元数据。每条都必须能说出：意图、输入、预期结果、来源依据。"""

    id: str
    intent: str
    inputs: str
    expected: str
    source: str            # 来源依据：正本条款 / 判据编号，出处可逐行核对
    criteria: Sequence[str]  # 吸收自审核包-R2/T02 判据清单的编号
    kind: str              # POSITIVE | NEGATIVE
    func: Callable[[], None]
    inject: str = ""       # negative 专用：注入的缺陷是什么
    defect_id: str = ""    # negative 专用：缺陷代号，便于报告逐条登记


def test(case_id: str, *, intent: str, inputs: str, expected: str,
         source: str, criteria: Sequence[str] = (), kind: str = POSITIVE,
         inject: str = "", defect_id: str = ""):
    """注册一条用例。

    参数全部必填（除 `criteria`）：**不提供意图与来源的用例不得进本测试集**，
    这是 `standards/05_INDEPENDENT_TEST_SUITE.md` §1「不用空断言充数」的直接落法。
    """
    if kind not in (POSITIVE, NEGATIVE):
        raise ValueError(f"kind 只能是 {POSITIVE!r} 或 {NEGATIVE!r}")
    if kind == NEGATIVE and not inject:
        raise ValueError(f"负例 {case_id} 必须写明 inject（注入的缺陷是什么）")
    if not source:
        raise ValueError(f"用例 {case_id} 必须写明 source（来源依据）")

    def deco(fn: Callable[[], None]) -> Callable[[], None]:
        if any(c.id == case_id for c in _REGISTRY):
            raise ValueError(f"用例 id 重复: {case_id}")
        _REGISTRY.append(TestCase(
            id=case_id, intent=intent, inputs=inputs, expected=expected,
            source=source, criteria=tuple(criteria), kind=kind, func=fn,
            inject=inject, defect_id=defect_id or case_id,
        ))
        return fn
    return deco


# ---------------------------------------------------------------------------
# 证据累积：负例把实测违例量写在这里，运行器打印
# ---------------------------------------------------------------------------

@dataclass
class Evidence:
    """一次执行中累积的可核对读数。"""

    entries: List[str] = field(default_factory=list)

    def record(self, key: str, value: Any, tol_value: Any = None,
               unit: str = "", note: str = "") -> None:
        line = f"{key} = {value!r}"
        if unit:
            line += f" {unit}"
        if tol_value is not None:
            line += f"  (门限 {tol_value!r}"
            line += f", 余量 {margin(value, tol_value):.3g}×)" if isinstance(value, float) else ")"
        if note:
            line += f"  [{note}]"
        self.entries.append(line)

    def dump(self, indent: str = "      ") -> str:
        return "\n".join(indent + e for e in self.entries)


def margin(value: float, limit: float) -> float:
    """超界倍数。|value| / |limit|；limit 为 0 时返回 `inf`（判红）。"""
    if limit == 0.0:
        return math.inf if value != 0.0 else 0.0
    return abs(value) / abs(limit)


# ---------------------------------------------------------------------------
# 断言（全部带诊断信息，不裸 assert）
# ---------------------------------------------------------------------------

class CheckFailure(AssertionError):
    """判据未满足。**不是**流水线判决，只是把红项喊出来。"""


def _fmt(x: Any, n: int = 12) -> str:
    if isinstance(x, float):
        return f"{x:.{n}g}"
    return repr(x)


def close(actual: float, expected: float, *, rtol: float, atol: float,
          what: str = "", scale: Optional[float] = None) -> None:
    """浮点比较，判据 = `|actual − expected| ≤ atol + rtol·|expected|`。

    `scale` 显式给出被比较量的量级域（`TEST.md` §4.3：`atol ≥ 1 ulp(scale)` 才可判）。
    """
    if scale is not None and atol < tol.ulp(scale):
        raise CheckFailure(
            f"{what}: atol={atol:.3g} < 1 ulp(scale={_fmt(scale)})={tol.ulp(scale):.3g} "
            f"⇒ 绝对容差不可满足（TEST.md §4.3），必须改用同值的相对形式")
    diff = abs(actual - expected)
    limit = atol + rtol * abs(expected)
    if not math.isfinite(actual) or not math.isfinite(expected):
        raise CheckFailure(
            f"{what}: 非有限值参与比较（actual={actual!r} expected={expected!r}）。"
            f"TEST.md §4.4：非有限值要么位置精确一致要么判错，不存在「落在容差内通过」的中间态")
    if diff > limit:
        raise CheckFailure(
            f"{what}: |{_fmt(actual)} − {_fmt(expected)}| = {_fmt(diff)} > "
            f"atol+rtol·|expected| = {_fmt(atol)}+{_fmt(rtol)}·{_fmt(abs(expected))} = {_fmt(limit)}"
            + (f" (scale={_fmt(scale)})" if scale is not None else ""))


def exact(actual: Any, expected: Any, what: str = "") -> None:
    """精确一致档（`TEST.md` §4：元数据、掩膜、计数、索引、端口、选择结果）。"""
    if actual != expected:
        raise CheckFailure(f"{what}: 期望精确 {expected!r}，实测 {actual!r}")


def is_true(cond: bool, what: str) -> None:
    if not cond:
        raise CheckFailure(f"{what}: 条件不成立")


def is_false(cond: bool, what: str) -> None:
    if cond:
        raise CheckFailure(f"{what}: 条件成立，但正本要求不成立")


def less_equal(actual: float, limit: float, what: str = "") -> None:
    if not (actual <= limit):
        raise CheckFailure(f"{what}: {_fmt(actual)} > 门限 {_fmt(limit)}（超界 {margin(actual, limit):.3g}×）")


def raises(exc_type, fn: Callable[[], Any], what: str = "") -> BaseException:
    """断言显式失败面。**不返哨兵、不静默改写**（HEALPIX_MAPPING「非法输入的显式失败面」）。"""
    try:
        fn()
    except exc_type as e:
        return e
    except BaseException as e:  # noqa: BLE001 - 诊断需要区分异常种类
        raise CheckFailure(f"{what}: 期望 {exc_type.__name__}，实测 {type(e).__name__}: {e}") from None
    raise CheckFailure(f"{what}: 期望抛出 {exc_type.__name__}，实际正常返回")


# ---------------------------------------------------------------------------
# 执行
# ---------------------------------------------------------------------------

@dataclass
class Result:
    case: TestCase
    passed: bool
    detail: str = ""
    evidence: Evidence = field(default_factory=Evidence)
    seconds: float = 0.0


_current_evidence: List[Evidence] = []


class evidence:
    """上下文管理器：块内 `record()` 的读数挂到**当前执行的用例**上。

    ⚠ 关键实现约束（本层踩过的坑）：**必须复用栈顶那个由运行器附加的对象**，
    不能另建一个再压栈。否则 `record()` 写进的是个随即被弹出的临时对象，
    而 `Result.evidence` 仍为空 ⇒ `--verbose` 读数全丢，负例的「实测超界读数」
    根本打不出来（负例有效性将无从核对）。
    只有在没有运行器上下文（如直接被 pytest 调用）时才自建并压栈。
    """

    _owned = False

    def __enter__(self) -> Evidence:
        if _current_evidence:
            self._owned = False
            return _current_evidence[-1]
        self._owned = True
        ev = Evidence()
        _current_evidence.append(ev)
        return ev

    def __exit__(self, *exc) -> None:
        if self._owned and _current_evidence:
            _current_evidence.pop()


def _evidence() -> Evidence:
    return _current_evidence[-1] if _current_evidence else Evidence()


def discover(package_name: str = "eng.tests.unit") -> int:
    """导入本包下所有 `test_*.py` 模块，触发注册。返回本次新注册的用例数。

    这里**不接受**「文件里写了用例但没注册进来」的情况——S17 元口径
    「写在产物里无人读 ≠ 在跑」的最小落法：文件必须在导入期注册。
    """
    before = len(_REGISTRY)
    pkg = importlib.import_module(package_name)
    for mod in pkgutil.iter_modules(pkg.__path__):
        if mod.name.startswith("test_") and mod.ispkg:
            raise RuntimeError(
                f"{mod.name} 是包；测试文件必须是模块（避免 __init__ 里的空断言面）")
        if mod.name.startswith("test_"):
            importlib.import_module(f"{package_name}.{mod.name}")
    return len(_REGISTRY) - before


def registered() -> List[TestCase]:
    return list(_REGISTRY)


# ---------------------------------------------------------------------------
# 裁决语义：测试是工具不是裁判
# ---------------------------------------------------------------------------

#: 非阻塞模式的唯一裁决词（`standards/05_INDEPENDENT_TEST_SUITE.md` §4「以非阻塞为常态」）。
VERDICT_WARN = "warn"


def verdict(results: Sequence["Result"]) -> str:
    """返回裁决词。**本层恒为 `warn`**——即使全部红项。

    规范 §4 逐字：「测试集可以接入 CI，但以非阻塞为常态：默认报告结果与警告（warn），
    不因为重构中的中间状态频繁阻断。」§1 逐字：「不作为门禁去约束代码」。

    ⇒ 这里**故意**不存在返回 `red` 的分支：一条想造出门禁的红路径的测试，必须变红。
    """
    return VERDICT_WARN


def exit_code(results: Sequence["Result"], non_blocking: bool = True) -> int:
    """退出码。默认恒 `0`；`non_blocking=False` 只在**开发期自查**时用。

    `docs/engineering/testing/TEST.md` §9 逐字：「测试集不产出流水线判决。」
    """
    if non_blocking:
        return 0
    return sum(1 for r in results if not r.passed)


def run_all(predicate=None) -> List[Result]:
    """执行注册表里的全部用例（或满足 `predicate` 的子集）。

    `predicate` 存在的理由：元判据 `meta.every_registered_case_is_executed`
    若无条件跑全表，会把其它判据面的蒙特卡洛/密集域用例一并拉进来，
    单这一条就要跑几分钟。元判据要验的是「登记 ↔ 执行」的映射，不是各用例的结论，
    因此允许它自选子集。
    """
    cases = [c for c in _REGISTRY if predicate is None or predicate(c)]
    results: List[Result] = []
    for case in cases:
        ev = Evidence()
        _current_evidence.append(ev)
        t0 = time.perf_counter()
        try:
            case.func()
            passed, detail = True, ""
        except CheckFailure as e:
            passed, detail = False, str(e)
        except Exception as e:  # noqa: BLE001 - 用例内部错误也要报出来
            passed = False
            detail = f"未预期异常 {type(e).__name__}: {e}\n{traceback.format_exc()}"
        finally:
            dt = time.perf_counter() - t0
            _current_evidence.pop()
        results.append(Result(case=case, passed=passed, detail=detail,
                              evidence=ev, seconds=dt))
    return results