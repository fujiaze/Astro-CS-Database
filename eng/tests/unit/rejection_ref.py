"""排异路由参考实现（被测口径）—— `lib/algorithms/coverage/src/rejection.cpp` 的逐行转写。

**它是什么**：排异规划层（`p2_reject_plan_resolve`）、内核闸
（`p2_reject_stack_ex` 的 UNDERDETERMINED 闸与方法×归一化合法性门）、compat
归一化耦合（`p2_reject_stack`）与适用性告警（`p2_rejection_applicability`）的
Python 转写。每一处分支都带被测 C++ 的 `file:line`，可逐行对回。

**它不是什么**（派单纪律第 1 条）：**不是期望值来源**。期望值只能来自
`docs/science/REJECTION.md`（科学正本）与可复算的解析推导；本文件的输出
**不得**被用来反推任何用例的 `expected`。

**兼容声明**：`docs/science/REJECTION.md` §14a 与 §5 逐字
`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED` —— 对照档 `wbpp_2_9_1` 的档界
只是**本仓解析表**（§5 逐字「本行只描述本仓解析表，不是对 WBPP 行为的转述」）。
本文件与本层任何用例都**不得**宣称与 WBPP / PixInsight bit-exact。

## 缺陷注入面（只用于负例；默认 `FAULTS` 为空 = 正确实现）

每个注入点对应产品 C++ 里的**一个决策行**，代号与报告逐条对应：

| 代号 | 注入的缺陷 | 被测 C++ 行 |
|---|---|---|
| `D1_SMALL_N_WBPP_TABLE` | 小 N 改采 WBPP 对称读法（`1 ≤ N ≤ 3` 也走 percentile） | `rejection.cpp:1142` |
| `D2_N16_LINEAR_FIT` | `N ≥ 16` 改回 `linear_fit`（撤掉 M3 改投） | `rejection.cpp:1169` |
| `D3_CTRL_N16_WINSORIZED` | 对照档 `n > 15` 也投 `winsorized_sigma`（两档塌成同一张表） | `rejection.cpp:1283` |
| `D4_CTRL_BOUNDARY_6_TO_5` | 对照档 `n < 6` 档的下界写成 `n < 5`（档界 6 被挪走） | `rejection.cpp:1281` |
| `D5_UNDET_DEFAULT_2_TO_1` | 非 pixel 档 `underdetermined_n` 默认 2 → 1 | `rejection.cpp:1231` |
| `D6_KERNEL_GATE_N_LT_1` | 内核闸条件 `n ≤ underdetermined_n` → `n < 1`（闸永不发火） | `rejection.cpp:2182` |
| `D7_COUPLING_ALL_NONE` | 归一化耦合改成「所有方法都 NONE」 | `rejection.cpp:2376` |
| `D8_PLAN_NORM_DEFAULT_NONE` | 规划层归一化默认值改成 NONE | `rejection.cpp:1239` |

注入**只改被测口径的行为**，不改任何判据：判据（期望值）始终来自正本。
"""

from __future__ import annotations

import math
from contextlib import contextmanager
from dataclasses import dataclass, replace
from typing import Iterator, Sequence, Tuple

# ===========================================================================
# 枚举与常量 —— rejection.h:45-121（逐行照抄枚举序号）
# ===========================================================================

# ---- 方法枚举（rejection.h:46-62；正本 docs/science/REJECTION.md §12 逐字给出同一组值）----
P2_REJECT_NONE = 0
P2_REJECT_SIGMA = 1
P2_REJECT_WINSORIZED_SIGMA = 2
P2_REJECT_AVERAGED_SIGMA = 3
P2_REJECT_LINEAR_FIT = 4
P2_REJECT_GENERALIZED_ESD = 5
P2_REJECT_RCR = 6
P2_REJECT_PERCENTILE = 7
P2_REJECT_MEDIAN_SIGMA = 8
P2_REJECT_MINMAX = 9
P2_REJECT_AUTO = 10
P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA = 11

METHOD_NAME = {
    P2_REJECT_NONE: "NONE",
    P2_REJECT_SIGMA: "SIGMA",
    P2_REJECT_WINSORIZED_SIGMA: "WINSORIZED_SIGMA",
    P2_REJECT_AVERAGED_SIGMA: "AVERAGED_SIGMA",
    P2_REJECT_LINEAR_FIT: "LINEAR_FIT",
    P2_REJECT_GENERALIZED_ESD: "GENERALIZED_ESD",
    P2_REJECT_RCR: "RCR",
    P2_REJECT_PERCENTILE: "PERCENTILE",
    P2_REJECT_MEDIAN_SIGMA: "MEDIAN_SIGMA",
    P2_REJECT_MINMAX: "MINMAX",
    P2_REJECT_AUTO: "AUTO",
    P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA: "EXTREME_VALUE_PRIOR_SIGMA",
}

# ---- 逐样本原因（rejection.h:99-103）----
P2_REASON_ACCEPTED = 0
P2_REASON_REJECTED_LOW = 1
P2_REASON_REJECTED_HIGH = 2
P2_REASON_UNDERDETERMINED = 3

# ---- 栈级状态（rejection.h:107-114）----
P2_STATUS_OK = 0
P2_STATUS_MIN_SAMPLES = 1
P2_STATUS_ALL_REJECTED = 2
P2_STATUS_INVALID_INPUT = 3
P2_STATUS_UNDERDETERMINED = 4
P2_STATUS_INVALID_CONFIGURATION = 5
P2_STATUS_INVALID_METHOD = 6
P2_STATUS_INTERNAL_ERROR = 7

# ---- 归一化（rejection.h:119-121）----
P2_NORMALIZE_NONE = 0
P2_NORMALIZE_MEDIAN_CENTER = 1
P2_NORMALIZE_MEDIAN_SCALE = 2

# ---- profile 名（rejection.h:81-95）----
P2_PROFILE_WBPP_2_9_1 = "wbpp_2_9_1"
P2_PROFILE_WBPP_CURRENT = "wbpp_current"      # = wbpp_2_9_1 alias
P2_PROFILE_ACSD_ADAPTIVE = "acsd_adaptive"
P2_PROFILE_ACSD_ADAPTIVE_PIXEL = "acsd_adaptive_pixel"

#: 合法 profile 集（rejection.cpp:1210-1213 的白名单）
LEGAL_PROFILES = (
    P2_PROFILE_WBPP_2_9_1,
    P2_PROFILE_WBPP_CURRENT,
    P2_PROFILE_ACSD_ADAPTIVE,
    P2_PROFILE_ACSD_ADAPTIVE_PIXEL,
)

# ===========================================================================
# 缺陷注入面
# ===========================================================================

D1_SMALL_N_WBPP_TABLE = "D1_SMALL_N_WBPP_TABLE"
D2_N16_LINEAR_FIT = "D2_N16_LINEAR_FIT"
D3_CTRL_N16_WINSORIZED = "D3_CTRL_N16_WINSORIZED"
D4_CTRL_BOUNDARY_6_TO_5 = "D4_CTRL_BOUNDARY_6_TO_5"
D5_UNDET_DEFAULT_2_TO_1 = "D5_UNDET_DEFAULT_2_TO_1"
D6_KERNEL_GATE_N_LT_1 = "D6_KERNEL_GATE_N_LT_1"
D7_COUPLING_ALL_NONE = "D7_COUPLING_ALL_NONE"
D8_PLAN_NORM_DEFAULT_NONE = "D8_PLAN_NORM_DEFAULT_NONE"

#: 当前生效的缺陷集合；空集 = 正确实现。负例用 `injected(...)` 临时置入。
FAULTS: set = set()

DEFECT_LINE = {
    D1_SMALL_N_WBPP_TABLE: "rejection.cpp:1142",
    D2_N16_LINEAR_FIT: "rejection.cpp:1169",
    D3_CTRL_N16_WINSORIZED: "rejection.cpp:1283",
    D4_CTRL_BOUNDARY_6_TO_5: "rejection.cpp:1281",
    D5_UNDET_DEFAULT_2_TO_1: "rejection.cpp:1231",
    D6_KERNEL_GATE_N_LT_1: "rejection.cpp:2182",
    D7_COUPLING_ALL_NONE: "rejection.cpp:2376",
    D8_PLAN_NORM_DEFAULT_NONE: "rejection.cpp:1239",
}


@contextmanager
def injected(*defect_ids: str) -> Iterator[None]:
    """在块内注入具名缺陷；退出后恢复原状（不跨用例泄漏）。"""
    saved = set(FAULTS)
    FAULTS.clear()
    FAULTS.update(defect_ids)
    try:
        yield
    finally:
        FAULTS.clear()
        FAULTS.update(saved)


def _fault(defect_id: str) -> bool:
    return defect_id in FAULTS


# ===========================================================================
# 结构体 —— rejection.h:129-244
# ===========================================================================

@dataclass(frozen=True)
class SigmaParams:
    """rejection.h:129-140（P2SigmaParams）。"""

    lower_sigma: float = 4.0
    upper_sigma: float = 3.0
    max_iterations: int = 8


@dataclass(frozen=True)
class LinearFitParams:
    """rejection.h:142-146（P2LinearFitParams）。"""

    lower: float = 5.0
    upper: float = 3.5
    max_iterations: int = 8


@dataclass(frozen=True)
class EsdParams:
    """rejection.h:148-151（P2EsdParams）。"""

    alpha: float = 0.05
    max_outliers: int = 10


@dataclass(frozen=True)
class PercentileParams:
    """rejection.h:153-157（P2PercentileParams）。"""

    low_fraction: float = 0.2
    high_fraction: float = 0.1


@dataclass(frozen=True)
class MinmaxParams:
    """rejection.h:159-163（P2MinmaxParams）。"""

    reject_low_count: int = 1
    reject_high_count: int = 1
    min_kept: int = 4


@dataclass(frozen=True)
class LargeScaleParams:
    """rejection.h:165-172（P2LargeScaleParams）。"""

    enabled: int = 0
    min_structure_pixels: int = 8
    low_grow_radius_pixels: int = 2
    high_grow_radius_pixels: int = 2


@dataclass(frozen=True)
class ExtremePriorParams:
    """rejection.h:199-205（P2ExtremeValuePriorSigmaParams）。"""

    alpha: float = 0.05
    prior_sigma: float = 0.0            # 0 = 未提供 → kernel fail-closed
    prior_sky: float = math.nan         # NaN = 未提供
    center_mode: int = 1


@dataclass(frozen=True)
class Plan:
    """`P2RejectionPlan`（rejection.h:208-229）；默认值来自 plan_resolve 的赋值。"""

    method: int
    minimum_n: int
    underdetermined_n: int
    normalization: int
    normalization_floor: float
    sigma: SigmaParams
    winsorized: SigmaParams
    averaged: SigmaParams
    median_sigma: SigmaParams
    linear_fit: LinearFitParams
    esd: EsdParams
    percentile: PercentileParams
    minmax: MinmaxParams
    large_scale: LargeScaleParams
    extreme_prior: ExtremePriorParams
    nominal_n: int


@dataclass(frozen=True)
class PlanRequest:
    """`P2RejectionPlanRequest`（rejection.h:232-244）。"""

    request: int
    nominal_contributors: int
    profile: str = P2_PROFILE_WBPP_2_9_1
    underdetermined_n: int = 0          # 0 = 按 profile 默认（:1236）


@dataclass(frozen=True)
class Decision:
    """`P2RejectionDecision` 的本层所需字段。"""

    status: int
    reasons: Tuple[int, ...]
    accepted_count: int
    rejected_low: int = 0
    rejected_high: int = 0
    iterations: int = 0

    @property
    def rejected_total(self) -> int:
        return self.rejected_low + self.rejected_high


# ===========================================================================
# 方法注册表 —— rejection.cpp:983-1007
# ===========================================================================

def method_minimum_n(method: int) -> int:
    """rejection.cpp:983-1000。"""
    if method in (P2_REJECT_SIGMA, P2_REJECT_WINSORIZED_SIGMA, P2_REJECT_AVERAGED_SIGMA,
                  P2_REJECT_GENERALIZED_ESD, P2_REJECT_RCR, P2_REJECT_MEDIAN_SIGMA):
        return 3
    if method == P2_REJECT_LINEAR_FIT:
        return 4
    if method == P2_REJECT_PERCENTILE:
        return 2
    if method == P2_REJECT_MINMAX:
        return 3
    if method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA:
        return 2
    return 0                            # NONE 与未知都落这里（:985/:998）


def method_is_explicit(method: int) -> bool:
    """rejection.cpp:1003-1007：AUTO(10) 与越界一律不进 kernel。"""
    return P2_REJECT_NONE <= method <= P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA and method != P2_REJECT_AUTO


# ===========================================================================
# 唯一显式决策点 + AUTO 路由表 —— rejection.cpp:1114-1192
# ===========================================================================

class _PixelSmallNPolicy:
    """rejection.cpp:1140 的 enum class PixelSmallNPolicy。"""

    kWbppTable = "kWbppTable"          # 对称读法（未采用）
    kConservativeNone = "kConservativeNone"  # 定案值


def k_pixel_small_n_policy() -> str:
    """rejection.cpp:1141-1142 的 `kPixelSmallNPolicy`（改判只改本行的取值）。"""
    if _fault(D1_SMALL_N_WBPP_TABLE):
        return _PixelSmallNPolicy.kWbppTable
    return _PixelSmallNPolicy.kConservativeNone


def p2_rejection_percentile_band_min_n() -> int:
    """rejection.cpp:1144-1148。4 = N ≤ 3 走保守 none（当前生效）；1 = WBPP 对称读法。"""
    return 1 if k_pixel_small_n_policy() == _PixelSmallNPolicy.kWbppTable else 4


def acsd_n_map_method(n: int) -> int:
    """rejection.cpp:1150-1170 `acsd_n_map_method`（生产档逐像素映射）。"""
    if (k_pixel_small_n_policy() == _PixelSmallNPolicy.kConservativeNone
            and 1 <= n <= 3):
        return P2_REJECT_NONE           # :1154-1156
    if n < 6:
        return P2_REJECT_PERCENTILE     # :1157（含 n = 0 的 void 占位）
    if n <= 15:
        return P2_REJECT_WINSORIZED_SIGMA   # :1158
    if _fault(D2_N16_LINEAR_FIT):
        return P2_REJECT_LINEAR_FIT    # 注入：撤回 M3 改投
    return P2_REJECT_WINSORIZED_SIGMA   # :1169（M3：n ≥ 16 同走 winsorized_sigma）


def auto_method_forbidden(method: int, n: int, pixel_profile: bool) -> bool:
    """rejection.cpp:1180-1192：AUTO 解析命中 min/max 或「非保守点的 NONE」⇒ fail-closed。"""
    if method == P2_REJECT_MINMAX:
        return True
    if method == P2_REJECT_NONE:
        return not (pixel_profile
                    and k_pixel_small_n_policy() == _PixelSmallNPolicy.kConservativeNone
                    and 1 <= n <= 3)
    return False


# ===========================================================================
# 规划层解析 —— rejection.cpp:1195-1304 `p2_reject_plan_resolve`
# ===========================================================================

def p2_reject_plan_resolve(req: PlanRequest) -> Tuple[int, "Plan | None", str]:
    """返回 `(rc, plan, err)`；`rc == 0` 时 `plan` 非空。"""
    if (req.request < P2_REJECT_NONE
            or req.request > P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA
            or (req.request != P2_REJECT_AUTO and not method_is_explicit(req.request))):
        return 1, None, "p2_reject_plan_resolve: request out of range"   # :1202-1207
    profile = req.profile if req.profile is not None else P2_PROFILE_WBPP_2_9_1  # :1209
    if profile not in LEGAL_PROFILES:
        return 1, None, "p2_reject_plan_resolve: profile 不合法"        # :1210-1218
    pixel_profile = (profile == P2_PROFILE_ACSD_ADAPTIVE_PIXEL)          # :1220

    # ---- underdetermined_n 默认（:1231-1238）----
    undet_default = 2                                                     # :1231
    if pixel_profile:                                                     # :1232-1235
        undet_default = 1 if req.request == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA else 3
    if _fault(D5_UNDET_DEFAULT_2_TO_1) and not pixel_profile:              # 注入点
        undet_default = 1
    underdetermined_n = (req.underdetermined_n if req.underdetermined_n > 0
                         else undet_default)                              # :1236-1238

    # ---- 归一化与冻结阈值（:1239-1264）----
    normalization = (P2_NORMALIZE_NONE if _fault(D8_PLAN_NORM_DEFAULT_NONE)
                     else P2_NORMALIZE_MEDIAN_CENTER)                     # :1239
    normalization_floor = 1e-12                                           # :1241

    # ---- AUTO 解析（:1267-1285）----
    method = req.request
    if method == P2_REJECT_AUTO:
        n = req.nominal_contributors
        if pixel_profile:
            method = acsd_n_map_method(n)                                 # :1270-1274
        else:
            # 对照档冻结表（:1276-1284）
            ctrl_lo = 5 if _fault(D4_CTRL_BOUNDARY_6_TO_5) else 6         # 注入点
            if n < ctrl_lo:
                method = P2_REJECT_PERCENTILE                             # :1281
            elif n <= 15:
                method = P2_REJECT_WINSORIZED_SIGMA                       # :1282
            else:
                method = (P2_REJECT_WINSORIZED_SIGMA
                          if _fault(D3_CTRL_N16_WINSORIZED)
                          else P2_REJECT_LINEAR_FIT)                      # :1283 / 注入点

    # ---- 生产路径守卫（:1288-1294）----
    if (req.request == P2_REJECT_AUTO
            and auto_method_forbidden(method, req.nominal_contributors, pixel_profile)):
        return 1, None, "p2_reject_plan_resolve: AUTO 路由命中禁用方法"

    # ---- extreme_prior 强制 NONE（:1300-1301）----
    if method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA:
        normalization = P2_NORMALIZE_NONE

    plan = Plan(
        method=method,
        minimum_n=method_minimum_n(method),                               # :1296
        underdetermined_n=underdetermined_n,
        normalization=normalization,
        normalization_floor=normalization_floor,
        sigma=SigmaParams(4.0, 3.0, 8),                                  # :1242
        winsorized=SigmaParams(4.0, 3.0, 8),                             # :1243-1244
        averaged=SigmaParams(4.0, 3.0, 8),                               # :1245-1246
        median_sigma=SigmaParams(4.0, 3.0, 8),                           # :1251-1252
        linear_fit=LinearFitParams(5.0, 3.5, 8),                         # :1247-1248
        esd=EsdParams(0.05, 10),                                         # :1249
        percentile=PercentileParams(0.2, 0.1),                           # :1250
        minmax=MinmaxParams(1, 1, 4),                                    # :1253-1254
        large_scale=LargeScaleParams(0, 8, 2, 2),                        # :1256-1259
        extreme_prior=ExtremePriorParams(),                              # :1260-1264
        nominal_n=req.nominal_contributors,                              # :1265
    )
    return 0, plan, ""


def p2_reject_plan_resolve_n(nominal_n: int, req: PlanRequest
                             ) -> Tuple[int, "Plan | None", str]:
    """rejection.cpp:1306-1317：几何 n 覆盖 request 的 nominal_contributors。"""
    return p2_reject_plan_resolve(replace(req, nominal_contributors=nominal_n))


# ===========================================================================
# compat adapter 的归一化耦合 —— rejection.cpp:2374-2378 `p2_reject_stack`
# ===========================================================================

def compat_normalization_for(method: int) -> int:
    """rejection.cpp:2376-2378。

    逐字：「compat 保持旧行为：sigma 类方法 shift-invariant → NONE 等价；
    **percentile 必须 MEDIAN_CENTER（|median| 尺度，负值安全）**」。
    """
    if method == P2_REJECT_PERCENTILE and not _fault(D7_COUPLING_ALL_NONE):
        return P2_NORMALIZE_MEDIAN_CENTER
    return P2_NORMALIZE_NONE


# ===========================================================================
# 内核 —— rejection.cpp:2103-2316 `p2_reject_stack_ex`（本层用到的分支逐行转写）
# ===========================================================================

def _all_undetermined(n: int) -> Tuple[int, ...]:
    return tuple([P2_REASON_UNDERDETERMINED] * n)


def _undetermined_status(n: int) -> Decision:
    """整栈全接受 + P2_STATUS_UNDERDETERMINED（:2183-2187 的早退面）。"""
    return Decision(status=P2_STATUS_UNDERDETERMINED, reasons=_all_undetermined(n),
                    accepted_count=n)


def _invalid_configuration(n: int) -> Decision:
    """整栈免检 + P2_STATUS_INVALID_CONFIGURATION（:2147-2151 的早退面）。"""
    return Decision(status=P2_STATUS_INVALID_CONFIGURATION, reasons=_all_undetermined(n),
                    accepted_count=n)


def scratch_median(values: Sequence[float]) -> float:
    """rejection.cpp:1061-1069。偶数 n 取 `0.5·(v[n/2] + max(v[0..n/2))`；排序后等价。"""
    n = len(values)
    if n == 0:
        return 0.0
    v = sorted(values)
    mid = n // 2
    if n % 2 == 1:
        return v[mid]
    return 0.5 * (v[mid] + v[mid - 1])


def method_thresholds_finite(method: int, plan: Plan) -> bool:
    """rejection.cpp:2072-2099。"""
    if method in (P2_REJECT_SIGMA, P2_REJECT_WINSORIZED_SIGMA, P2_REJECT_AVERAGED_SIGMA,
                  P2_REJECT_MEDIAN_SIGMA):
        s = plan.sigma
        if method == P2_REJECT_WINSORIZED_SIGMA:
            s = plan.winsorized
        elif method == P2_REJECT_AVERAGED_SIGMA:
            s = plan.averaged
        elif method == P2_REJECT_MEDIAN_SIGMA:
            s = plan.median_sigma
        return math.isfinite(s.lower_sigma) and math.isfinite(s.upper_sigma)
    if method == P2_REJECT_LINEAR_FIT:
        return math.isfinite(plan.linear_fit.lower) and math.isfinite(plan.linear_fit.upper)
    if method == P2_REJECT_PERCENTILE:
        return math.isfinite(plan.percentile.low_fraction) and math.isfinite(plan.percentile.high_fraction)
    if method == P2_REJECT_GENERALIZED_ESD:
        return math.isfinite(plan.esd.alpha)
    return True    # NONE / RCR / MINMAX / 未知（:2093-2097）


def extreme_prior_valid(prior_sigma: float, prior_sky: float, center_mode: int) -> bool:
    """rejection.cpp:2192 的判定面（rejection.h:195-196 逐字：先验 σ 非有限或 ≤0 ⇒ 无效）。"""
    return math.isfinite(prior_sigma) and prior_sigma > 0.0


def reject_none_impl(n: int) -> Tuple[int, ...]:
    """rejection.cpp:1532-1534：全 ACCEPTED。"""
    return tuple([P2_REASON_ACCEPTED] * n)


def reject_percentile_impl(work: Sequence[float], prm: PercentileParams,
                           orig_median: float) -> Tuple[int, ...]:
    """rejection.cpp:1876-1897。判据带 `|median|` 尺度，**负值安全**（先取绝对值）。"""
    plow = abs(prm.low_fraction)
    phigh = abs(prm.high_fraction)
    scale = abs(orig_median)
    out = []
    for v in work:
        if v < -scale * plow:
            out.append(P2_REASON_REJECTED_LOW)
        elif v > scale * phigh:
            out.append(P2_REASON_REJECTED_HIGH)
        else:
            out.append(P2_REASON_ACCEPTED)
    return tuple(out)


def kernel_underdetermined_gate(n: int, plan: Plan) -> bool:
    """rejection.cpp:2179-2182 的闸条件本身（`True` = 判 UNDERDETERMINED 全接受）。

    正本 docs/science/REJECTION.md §4 逐字：「`n <= underdetermined_n` 或
    `n < minimum_n` ⇒ `UNDERDETERMINED`」。
    """
    min_n = plan.minimum_n if plan.minimum_n > 0 else 0
    if _fault(D6_KERNEL_GATE_N_LT_1):
        return n < 1                      # 注入：闸永不发火
    return n <= plan.underdetermined_n or (min_n > 0 and n < min_n)


def kernel_method_x_normalization_legal(method: int, norm: int) -> bool:
    """rejection.cpp:2143-2168 的方法×归一化合法性门。

    正本 docs/science/REJECTION.md §8a 逐字：`normalization=MEDIAN_CENTER`
    **由内核强制**（percentile）。
    """
    if method == P2_REJECT_PERCENTILE and norm != P2_NORMALIZE_MEDIAN_CENTER:
        return False
    if method == P2_REJECT_RCR and norm != P2_NORMALIZE_NONE:
        return False
    if method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA and norm != P2_NORMALIZE_NONE:
        return False
    return True


def p2_reject_stack_ex(values: Sequence[float], plan: Plan) -> Decision:
    """rejection.cpp:2103-2316。本层只走到 NONE / PERCENTILE 两个核，其余显式失败。"""
    n = len(values)
    if not method_is_explicit(plan.method):                 # :2107-2120
        return Decision(status=P2_STATUS_INVALID_METHOD, reasons=_all_undetermined(n),
                        accepted_count=n)
    if n == 0:                                              # :2125
        return Decision(status=P2_STATUS_MIN_SAMPLES, reasons=(), accepted_count=0)
    if any(not math.isfinite(v) for v in values):           # :2132-2141
        return Decision(status=P2_STATUS_INVALID_INPUT, reasons=_all_undetermined(n),
                        accepted_count=n)
    if not kernel_method_x_normalization_legal(plan.method, plan.normalization):
        return _invalid_configuration(n)                    # :2145-2168
    if not method_thresholds_finite(plan.method, plan):     # :2171-2177
        return _invalid_configuration(n)
    if kernel_underdetermined_gate(n, plan):                 # :2182-2188
        return _undetermined_status(n)
    if plan.method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA:  # :2192-2199（fail-closed）
        if not extreme_prior_valid(plan.extreme_prior.prior_sigma,
                                   plan.extreme_prior.prior_sky,
                                   plan.extreme_prior.center_mode):
            return _undetermined_status(n)

    # ---- 归一化（:2207-2220）----
    work = list(values)
    orig_median = 0.0
    if plan.normalization != P2_NORMALIZE_NONE:
        orig_median = scratch_median(work)
        if plan.normalization == P2_NORMALIZE_MEDIAN_CENTER:
            work = [v - orig_median for v in work]
        else:                                               # MEDIAN_SCALE
            scale = max(abs(orig_median), plan.normalization_floor)
            work = [v / scale for v in work]

    # ---- 方法核（:2223-2276）----
    iterations = 0
    if plan.method == P2_REJECT_NONE:
        reasons = reject_none_impl(n)
    elif plan.method == P2_REJECT_PERCENTILE:
        reasons = reject_percentile_impl(work, plan.percentile, orig_median)
        iterations = 1
    else:
        raise NotImplementedError(
            f"本层只转写 NONE / PERCENTILE 两个核；method="
            f"{METHOD_NAME.get(plan.method, plan.method)} 未转写（rejection.cpp:2223-2276）")

    accepted = sum(1 for r in reasons if r == P2_REASON_ACCEPTED)
    rej_low = sum(1 for r in reasons if r == P2_REASON_REJECTED_LOW)
    rej_high = sum(1 for r in reasons if r == P2_REASON_REJECTED_HIGH)
    any_underdetermined = any(r == P2_REASON_UNDERDETERMINED for r in reasons)
    if any_underdetermined:
        accepted += sum(1 for r in reasons if r == P2_REASON_UNDERDETERMINED)

    status = (P2_STATUS_UNDERDETERMINED if any_underdetermined else P2_STATUS_OK)
    if accepted == 0:                                       # :2300-2310
        if n <= 4:
            reasons = _all_undetermined(n)
            return Decision(status=P2_STATUS_UNDERDETERMINED, reasons=reasons,
                            accepted_count=n, iterations=iterations)
        status = P2_STATUS_ALL_REJECTED
    return Decision(status=status, reasons=tuple(reasons), accepted_count=accepted,
                    rejected_low=rej_low, rejected_high=rej_high, iterations=iterations)


# ===========================================================================
# 适用性告警 —— rejection.cpp:2663-2714 `p2_rejection_applicability`
# ===========================================================================

def p2_rejection_applicability(method: int, nominal_n: int) -> str:
    """返回告警码；空串 = 无告警。

    正本 docs/science/REJECTION.md §5 逐字「显式指定算法时 `16≤n<20` linear_fit
    由调用方发 WARN」；合法性窗口表见 docs/detail/registry/acsd.phase2.reject.md
    「显式指定的合法性窗口」（percentile 仅 ≤ 8 帧、linear fit 建议 ≥ 20）。
    """
    if method == P2_REJECT_NONE:
        return "W_NONE"
    if method == P2_REJECT_MINMAX:
        return "W_MINMAX"
    if method == P2_REJECT_PERCENTILE:
        if nominal_n > 8:
            return "W_PCT_GT8"
        if nominal_n <= 4:
            return "W_PCT_N4_FALLBACK"
        return ""
    if method == P2_REJECT_SIGMA:
        return "W_SIGMA_RANGE" if (nominal_n < 8 or nominal_n > 15) else ""
    if method == P2_REJECT_WINSORIZED_SIGMA:
        # rejection.cpp:2683-2686 是两个**独立** if：第二个可覆盖第一个，
        # 故 n ≤ 4 最终落 W_NR_LE4（不是 W_WINS_LT8）。
        code = ""
        if nominal_n < 8:
            code = "W_WINS_LT8"
        if nominal_n <= 4:
            code = "W_NR_LE4"
        return code
    if method == P2_REJECT_MEDIAN_SIGMA:
        return "W_NR_LE4" if nominal_n <= 4 else ""
    if method == P2_REJECT_LINEAR_FIT:
        if nominal_n < 8:
            return "W_LF_LT8"
        if nominal_n < 20:
            return "W_LF_LT20"
        return ""
    if method == P2_REJECT_AVERAGED_SIGMA:
        return "W_AVG_RANGE" if (nominal_n < 8 or nominal_n > 10) else ""
    if method == P2_REJECT_GENERALIZED_ESD:
        return "W_ESD_LT25" if nominal_n < 25 else ""
    if method == P2_REJECT_RCR:
        return "W_RCR_LT15" if nominal_n < 15 else ""
    if method == P2_REJECT_EXTREME_VALUE_PRIOR_SIGMA:
        return ""
    return "W_UNKNOWN_METHOD"