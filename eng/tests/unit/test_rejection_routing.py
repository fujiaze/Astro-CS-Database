"""排异路由单元测试集 —— 吸收审核包-R2/T02 §2.1 的 **S1 / S2 / S3 / S4** 四条判据。

**被测对象** = `eng/tests/unit/rejection_ref.py`（`lib/algorithms/coverage/src/rejection.cpp`
的逐行转写，见该文件头）。

**期望值来源（硬性纪律第 1 条）**：每条用例的 `expected` 只来自
① 科学正本 `docs/science/REJECTION.md` 的逐字条款，或 ② 由该条款的不等式直接推导的
解析分段函数；**不得**由 `rejection_ref.py` 的输出反推。故本文件里的枚举整数、
档位表、分段规则全部写成**字面常量 / 字面不等式**，与参考实现互不共享定义。

**兼容声明**：`docs/science/REJECTION.md` §14a 与 §5 逐字
`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`。本文件对对照档 `wbpp_2_9_1`
的一切断言都是对**本仓解析表**的断言，**不**宣称与 WBPP / PixInsight bit-exact。

**容差档**：路由返回值是枚举整数（方法 / 归一化 / 状态 / 逐样本原因），
按 `docs/engineering/testing/TEST.md` §4「元数据、…、索引、端口、**选择结果**」
取**精确一致**，全部走 `harness.exact`；冻结登记见 `tolerances.py`
（`rejection.routing` / `rejection.normalization`）。
"""

from __future__ import annotations

from . import harness
from . import rejection_ref as ref
from . import tolerances as tol

# 冻结容差（未登记即 import 期抛错，不许在用例里现编容差）
ROUTING_EXACT = tol.get("rejection.routing").value
NORMALIZATION_EXACT = tol.get("rejection.normalization").value

# ===========================================================================
# 正本常量 —— 逐字照抄 docs/science/REJECTION.md §12(:230-233)
#   「方法枚举共 11 项 + AUTO：NONE=0 / SIGMA=1 / WINSORIZED_SIGMA=2 /
#     AVERAGED_SIGMA=3 / LINEAR_FIT=4 / GENERALIZED_ESD=5 / RCR=6 / PERCENTILE=7 /
#     MEDIAN_SIGMA=8 / MINMAX=9 / AUTO=10 / EXTREME_VALUE_PRIOR_SIGMA=11」
# 数值绑定另见 `lib/algorithms/coverage/include/astro/phase2/rejection.h:46-62`（同一组值）。
# ===========================================================================

METHOD_NONE = 0
METHOD_SIGMA = 1
METHOD_WINSORIZED_SIGMA = 2
METHOD_AVERAGED_SIGMA = 3
METHOD_LINEAR_FIT = 4
METHOD_GENERALIZED_ESD = 5
METHOD_RCR = 6
METHOD_PERCENTILE = 7
METHOD_MEDIAN_SIGMA = 8
METHOD_MINMAX = 9
METHOD_AUTO = 10
METHOD_EXTREME_VALUE_PRIOR_SIGMA = 11

METHODS_ALL = (
    METHOD_NONE, METHOD_SIGMA, METHOD_WINSORIZED_SIGMA, METHOD_AVERAGED_SIGMA,
    METHOD_LINEAR_FIT, METHOD_GENERALIZED_ESD, METHOD_RCR, METHOD_PERCENTILE,
    METHOD_MEDIAN_SIGMA, METHOD_MINMAX, METHOD_AUTO, METHOD_EXTREME_VALUE_PRIOR_SIGMA,
)

# 归一化枚举（rejection.h:119-121；正本 §9(:208) 逐字 `normalization=MEDIAN_CENTER`）
NORMALIZE_NONE = 0
NORMALIZE_MEDIAN_CENTER = 1
NORMALIZE_MEDIAN_SCALE = 2

# 逐样本原因（rejection.h:99-103；正本 §2 与 §5「不做猜测，全接受」同义）
REASON_ACCEPTED = 0
REASON_REJECTED_LOW = 1
REASON_REJECTED_HIGH = 2
REASON_UNDERDETERMINED = 3

# 栈级状态（rejection.h:107-114；正本 §8 逐字 MIN_SAMPLES = 1）
STATUS_OK = 0
STATUS_MIN_SAMPLES = 1
STATUS_ALL_REJECTED = 2
STATUS_INVALID_INPUT = 3
STATUS_UNDERDETERMINED = 4
STATUS_INVALID_CONFIGURATION = 5
STATUS_INVALID_METHOD = 6

# profile 名（正本 §4(:53) 合法集 / §5(:62,:90,:98)）
PROFILE_PROD = "acsd_adaptive_pixel"      # 生产默认档
PROFILE_CTRL = "wbpp_2_9_1"               # 对照档
PROFILE_ALIAS = "wbpp_current"            # 历史 alias
PROFILE_TUNABLE = "acsd_adaptive"         # 可调档

# ===========================================================================
# 正本解析规则 —— 由 docs/science/REJECTION.md §5 的不等式直接转写
#   「生产默认 auto + profile = acsd_adaptive_pixel：
#       1 ≤ n ≤ 3 → none / 4 ≤ n ≤ 5 → percentile / n ≥ 6 → winsorized_sigma
#        （n ≥ 16 档同走 winsorized_sigma）」                          §5(:62-66)
#   「对照档 wbpp_2_9_1：n < 6 → percentile / 6 ≤ n ≤ 15 → winsorized_sigma /
#        n > 15 → linear_fit」                                          §5(:90-93)
# 这两个函数是**正本口径**，与被测实现无任何共享代码。
# ===========================================================================

PROD_SWEEP_DOMAIN = range(1, 65)   # 覆盖全部档界（3/4/5/6/15/16）及其两侧邻点


def canon_prod_method(n: int) -> int:
    """生产档 `acsd_adaptive_pixel` 的 AUTO 路由（正本 §5 生产档表）。"""
    if 1 <= n <= 3:
        return METHOD_NONE
    if 4 <= n <= 5:
        return METHOD_PERCENTILE
    if n >= 6:
        return METHOD_WINSORIZED_SIGMA
    raise ValueError(
        f"正本 §5 的生产档表域是 n ≥ 1；n={n} 不在表内（n=0 的 void 占位语义见正本 §8）")


def canon_ctrl_method(n: int) -> int:
    """对照档 `wbpp_2_9_1` 的 AUTO 路由（正本 §5 对照档表）。"""
    if n < 6:
        return METHOD_PERCENTILE
    if n <= 15:
        return METHOD_WINSORIZED_SIGMA
    return METHOD_LINEAR_FIT


# 正本 §5(:62-69) 逐行列出的档界点，生产档
PROD_BOUNDARY_POINTS = (
    (1, METHOD_NONE), (2, METHOD_NONE), (3, METHOD_NONE),
    (4, METHOD_PERCENTILE), (5, METHOD_PERCENTILE),
    (6, METHOD_WINSORIZED_SIGMA), (15, METHOD_WINSORIZED_SIGMA),
    (16, METHOD_WINSORIZED_SIGMA),
)

# 正本 §5(:90-93) 逐行列出的档界点，对照档（n=16/17/20 覆盖 `n > 15` 整段）
CTRL_BOUNDARY_POINTS = (
    (1, METHOD_PERCENTILE), (2, METHOD_PERCENTILE), (3, METHOD_PERCENTILE),
    (4, METHOD_PERCENTILE), (5, METHOD_PERCENTILE),
    (6, METHOD_WINSORIZED_SIGMA), (15, METHOD_WINSORIZED_SIGMA),
    (16, METHOD_LINEAR_FIT), (17, METHOD_LINEAR_FIT), (20, METHOD_LINEAR_FIT),
)

# 正本 §4(:45-48) 逐字：`underdetermined_n` 默认值
#   「`acsd_adaptive_pixel` ∧ `request=AUTO` ⇒ 3（生产默认档）；
#     `acsd_adaptive_pixel` ∧ `request=extreme_value_clip_prior_sigma` ⇒ 1；
#     其余 profile（`wbpp_2_9_1`/`wbpp_current`/`acsd_adaptive`）⇒ 2」
UNDET_PROD_AUTO = 3
UNDET_PROD_EXTREME = 1
UNDET_CTRL = 2

# 正本 §5(:105-111)「阈值冻结锚点」逐字
FROZEN_SIGMA = (4.0, 3.0, 8)
FROZEN_LINEAR_FIT = (5.0, 3.5, 8)
FROZEN_ESD = (0.05, 10)
FROZEN_PERCENTILE = (0.2, 0.1)
FROZEN_MINMAX = (1, 1, 4)
FROZEN_LARGE_SCALE = (0, 8, 2, 2)
#: `normalization_floor`：rejection.h:213 与 rejection.cpp:1241 的代码级冻结常量
#: （正本只冻结 §8a 的 `scale=|median|`，未给 floor 的数值 —— 如实标注，不假借正本）
FROZEN_NORMALIZATION_FLOOR = 1e-12

# 夹具：面亮度 ADU·sr⁻¹（正本 §3 的 values 标度）。
#   median = 1000 ⇒ percentile 判据带 = [-0.2·1000, +0.1·1000] = [-200, +100]
#   （正本 §8a:154 逐字 `v ∉ [median − plow·|median|, median + phigh·|median|]` ⇒ 判拒）
BAND_LO, BAND_HI = -200.0, 100.0
FIXTURE_N2 = (850.0, 1150.0)            # n=2：median=1000，work=(-150,+150)
FIXTURE_N3 = (850.0, 1000.0, 1150.0)   # n=3：median=1000，work=(-150,0,+150)
FIXTURE_N4 = (850.0, 1000.0, 1150.0, 1000.0)   # n=4：median=1000，work=(-150,0,+150,0)
#: 同族夹具按几何 n 取用（同一条判据带 [median−0.2·|median|, median+0.1·|median|]）
FIXTURE_BY_N = {1: (850.0,), 2: FIXTURE_N2, 3: FIXTURE_N3, 4: FIXTURE_N4}


# ===========================================================================
# 工具
# ===========================================================================

def resolve(profile: str, n: int, *, request: int = METHOD_AUTO,
            underdetermined_n: int = 0) -> tuple[int, "ref.Plan | None", str]:
    """走规划层解析；`rc != 0` 即判红（路由面不得静默失败）。"""
    rc, plan, err = ref.p2_reject_plan_resolve(
        ref.PlanRequest(request=request, nominal_contributors=n, profile=profile,
                        underdetermined_n=underdetermined_n))
    harness.is_true(rc == 0, f"{profile} n={n}: plan_resolve 必须 rc=0（err={err!r}）")
    return rc, plan, err


def method_at(profile: str, n: int) -> int:
    return resolve(profile, n)[1].method


def with_normalization(plan: "ref.Plan", normalization: int) -> "ref.Plan":
    """返回换掉归一化的 plan 副本（测试夹具；不改被测实现本身）。"""
    return ref.replace(plan, normalization=normalization)


def method_violation_table(profile: str, want_fn, ns, ev, label: str
                           ) -> tuple[list[tuple[int, int, int]], float]:
    """扫掠 AUTO 路由，逐点与**正本**期望比对；返回 (违例点, 最大超界量)。

    判据档 = 精确一致（`TEST.md` §4「选择结果」），故超界量 = |method − 正本期望|，
    门限 `ROUTING_EXACT`（= 0.0）。
    """
    violations: list[tuple[int, int, int]] = []
    worst = 0.0
    for n in ns:
        got = method_at(profile, n)
        want = want_fn(n)
        delta = float(abs(got - want))
        worst = max(worst, delta)
        if delta > ROUTING_EXACT:
            violations.append((n, got, want))
            ev.record(f"{label} n={n} 注入/实测 method", got,
                      note=f"正本期望 {want}，超界 {delta:g}")
        ev.record(f"{label} n={n} |method − 正本期望|", delta, ROUTING_EXACT,
                  note="精确档，0 = 落在正本档位表上")
    return violations, worst


# ===========================================================================
# S1 · 生产档路由（`acsd_n_map_method`）
# ===========================================================================

@harness.test(
    "rejection-S1-PROD-TABLE-POINTS",
    intent="生产档 acsd_adaptive_pixel 的 AUTO 路由表，在正本 §5 逐字列出的八个档界点上"
           "逐点给出方法枚举；并核验本层冻结容差取的是精确一致档。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, nominal_contributors ∈ {1,2,3,4,5,6,15,16}",
    expected="1,2,3 → NONE(0)；4,5 → PERCENTILE(7)；6,15,16 → WINSORIZED_SIGMA(2)；"
             "全部精确一致；冻结容差 rejection.routing = 0.0（精确档）",
    source="docs/science/REJECTION.md §5(:62-69) 生产档表逐字 + §12(:230-231) 枚举值；"
           "被测面 lib/algorithms/coverage/src/rejection.cpp:1150-1170",
    criteria=["S1"],
)
def s1_prod_table_points():
    with harness.evidence() as ev:
        harness.exact(ROUTING_EXACT, tol.EXACT,
                      f"冻结容差 rejection.routing 必须取精确档（TEST.md §4；tolerances.py:283）")
        ev.record("冻结容差 rejection.routing", ROUTING_EXACT, note="精确一致档 = 0.0")
        for n, want in PROD_BOUNDARY_POINTS:
            got = method_at(PROFILE_PROD, n)
            harness.exact(got, want, f"生产档 n={n} 的 AUTO 路由")
            ev.record(f"生产档 n={n} → method", got, note=f"正本 §5 期望 {want}")


@harness.test(
    "rejection-S1-PROD-SWEEP-RULE",
    intent="把正本 §5 的三段不等式写成解析分段函数，在 n∈[1,64] 稠密扫掠上逐点比对，"
           "使「1≤n≤3→none / 4≤n≤5→percentile / n≥6→winsorized_sigma（含 n≥16 同档）」"
           "成为可复算的域级断言，而不是七点抽查。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, nominal_contributors = 1..64",
    expected="每个 n 的 method 精确等于 canon_prod_method(n)（正本 §5 不等式的解析推导）；零违例",
    source="docs/science/REJECTION.md §5(:62-66) 三段不等式 + §9a(:260) 逐字"
           "「内置映射见 §5（1≤n≤3→none；4≤n≤5→percentile；n≥6→winsorized_sigma（n≥16→linear_fit 档改投 winsorized））」",
    criteria=["S1"],
)
def s1_prod_sweep_rule():
    with harness.evidence() as ev:
        violations, worst = method_violation_table(
            PROFILE_PROD, canon_prod_method, PROD_SWEEP_DOMAIN, ev, "生产档")
        ev.record("扫掠点数", len(list(PROD_SWEEP_DOMAIN)), note="域 [1,64] 覆盖全部档界 3/4/5/6/15/16")
        ev.record("最大超界量", worst, ROUTING_EXACT)
        harness.is_false(bool(violations), f"正本 §5 生产档表被违反：{violations}")


@harness.test(
    "rejection-S1-PROD-N0-VOID",
    intent="n = 0 的 void 像素占位语义：无候选栈 ⇒ 正本 §8 判 MIN_SAMPLES 且不做任何排异判定；"
           "规划层在该点的 method 取值按实现登记给出（本仓解析表，正本 §5 的表域不含 n=0）。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, nominal_contributors = 0；随后以 count=0 的栈过内核",
    expected="规划层 rc=0、method = PERCENTILE(7)（占位，永不进内核）；内核对 count=0 "
             "返回 status = MIN_SAMPLES(1)、零剔除",
    source="科学约束：docs/science/REJECTION.md §8(:148) 逐字「无候选（count==0）| MIN_SAMPLES（值 1）」"
           "与 §1(:10)「单帧无排异」；占位 method 取值出自 lib/algorithms/coverage/src/rejection.cpp:1128-1131 逐字"
           "「N = 0（void 像素占位，无候选栈）→ percentile（永不进 kernel）」——"
           "正本 §5 表的域是 n ≥ 1，此处如实标注该值不是正本条款",
    criteria=["S1"],
)
def s1_prod_n0_void():
    with harness.evidence() as ev:
        plan = resolve(PROFILE_PROD, 0)[1]
        harness.exact(plan.method, METHOD_PERCENTILE,
                      "n=0 占位 method（rejection.cpp:1131；正本未定义此点）")
        harness.exact(plan.nominal_n, 0, "plan.nominal_n 应记录几何 n")
        ev.record("n=0 规划层 method（占位）", plan.method)
        dec = ref.p2_reject_stack_ex((), plan)
        harness.exact(dec.status, STATUS_MIN_SAMPLES, "count=0 的栈状态（正本 §8）")
        harness.exact(dec.rejected_total, 0, "count=0 不得产出任何排异判定")
        harness.exact(len(dec.reasons), 0, "count=0 无逐样本判定")
        ev.record("count=0 内核 status", dec.status)
        ev.record("count=0 剔除数", float(dec.rejected_total), ROUTING_EXACT,
                  note="正本 §8：无候选栈不做排异判定")


@harness.test(
    "rejection-S1-PROD-THRESHOLD-INVARIANT",
    intent="正本 §7 阈值不变量「同 n 的 method 选择确定性一致」与 §11 确定性门："
           "同 (profile, request, n) 的重复调用与乱序扫掠必须给出同一 method；"
           "并用「观察到的 method 取值不是常量」证明该断言不是恒真。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO；n∈[1,2,3,4,5,6,15,16] 各调 3 次，"
           "外加一遍逆序扫掠",
    expected="每个 n 的 method 在重复调用与逆序扫掠下逐点恒等；且档界点上至少出现 3 个不同取值",
    source="docs/science/REJECTION.md §7(:132) 逐字「同 n 的 method 选择确定性一致"
           "（阈值表驱动，逐档继承 §5 冻结锚点）」+ §11(:222)「阈值不变量：同 n 的 plan.resolve 输出 method 确定性一致」",
    criteria=["S1"],
)
def s1_prod_threshold_invariant():
    with harness.evidence() as ev:
        first: dict[int, int] = {}
        for n, _ in PROD_BOUNDARY_POINTS:
            first[n] = method_at(PROFILE_PROD, n)
        for n in PROD_BOUNDARY_POINTS:                     # 逆序 + 重复
            for _ in range(2):
                got = method_at(PROFILE_PROD, n[0])
                harness.exact(got, first[n[0]], f"n={n[0]} 的 method 不确定")
        for n, want in PROD_BOUNDARY_POINTS:
            ev.record(f"n={n} 三次调用 method", first[n], note=f"正本 §5 期望 {want}")
        distinct = sorted(set(first.values()))
        ev.record("观察到的 method 取值集合", distinct)
        harness.is_true(len(distinct) >= 3,
                        f"路由在整个扫掠上取值不分化（只出现 {distinct}）⇒ 确定性断言退化为常量检查")


@harness.test(
    "rejection-S1-PROD-NO-MINMAX",
    intent="T02 §2.1 S1 行逐字「min/max 不用于生产」：生产档 AUTO 在整个 n 域上"
           "不得产出 MINMAX，也不得因路由结果触发 fail-closed 守卫（rc 必须恒 0）。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, nominal_contributors = 0..64",
    expected="每个 n 的 method ≠ MINMAX(9)；每个 n 的 plan_resolve rc = 0",
    source="审核包-R2/T02-门禁退役与判据清单.md §2.1 S1 行逐字「min/max 不用于生产」；"
           "docs/detail/registry/acsd.phase2.reject.md:104-106 逐字「生产排异算法集 = none / percentile / "
           "winsorized / linear fit；min/max 极值法不用于生产」；被测面 rejection.cpp:1180-1192, :1288-1294",
    criteria=["S1"],
)
def s1_prod_no_minmax():
    with harness.evidence() as ev:
        seen: set[int] = set()
        for n in range(0, 65):
            rc, plan, err = ref.p2_reject_plan_resolve(
                ref.PlanRequest(request=METHOD_AUTO, nominal_contributors=n,
                                profile=PROFILE_PROD))
            harness.is_true(rc == 0, f"n={n}: 生产档 AUTO 解析必须成功（err={err!r}）")
            harness.is_false(plan.method == METHOD_MINMAX, f"n={n}: 生产档产出 MINMAX")
            harness.is_false(plan.method == METHOD_AUTO, f"n={n}: AUTO 不得残留进方法核（§12）")
            seen.add(plan.method)
        ev.record("生产档 n∈[0,64] 的 method 取值集合", sorted(seen),
                  note="不得含 MINMAX(9) / AUTO(10)")


# ===========================================================================
# S2 · 对照档 `wbpp_2_9_1`（与 S1 成对）
# ===========================================================================

@harness.test(
    "rejection-S2-CTRL-TABLE-POINTS",
    intent="对照档 wbpp_2_9_1 的 AUTO 路由表逐点断言（含 `n > 15 → linear_fit` 整段的三个点）。",
    inputs="profile=wbpp_2_9_1, request=AUTO, nominal_contributors ∈ {1,2,3,4,5,6,15,16,17,20}",
    expected="1..5 → PERCENTILE(7)；6,15 → WINSORIZED_SIGMA(2)；16,17,20 → LINEAR_FIT(4)；全部精确一致",
    source="docs/science/REJECTION.md §5(:90-93) 对照档表逐字；枚举值见同文件 §12(:230-231)；"
           "被测面 lib/algorithms/coverage/src/rejection.cpp:1276-1284",
    criteria=["S2"],
)
def s2_ctrl_table_points():
    with harness.evidence() as ev:
        for n, want in CTRL_BOUNDARY_POINTS:
            got = method_at(PROFILE_CTRL, n)
            harness.exact(got, want, f"对照档 n={n} 的 AUTO 路由")
            ev.record(f"对照档 n={n} → method", got, note=f"正本 §5 期望 {want}")


@harness.test(
    "rejection-S2-CTRL-SWEEP-RULE",
    intent="把正本 §5 对照档表的三段不等式写成解析分段函数，在 n∈[1,64] 上逐点比对，"
           "使档界 6 与 15 成为域级断言。",
    inputs="profile=wbpp_2_9_1, request=AUTO, nominal_contributors = 1..64",
    expected="每个 n 的 method 精确等于 canon_ctrl_method(n)；零违例",
    source="docs/science/REJECTION.md §5(:90-93) 逐字「n < 6 → percentile / 6 ≤ n ≤ 15 → "
           "winsorized_sigma / n > 15 → linear_fit」+ §4(:53) 合法 profile 集",
    criteria=["S2"],
)
def s2_ctrl_sweep_rule():
    with harness.evidence() as ev:
        violations, worst = method_violation_table(
            PROFILE_CTRL, canon_ctrl_method, PROD_SWEEP_DOMAIN, ev, "对照档")
        ev.record("最大超界量", worst, ROUTING_EXACT)
        harness.is_false(bool(violations), f"正本 §5 对照档表被违反：{violations}")


@harness.test(
    "rejection-S2-PAIR-PROD-VS-CTRL",
    intent="「拆开迁移等于没迁」的守卫：正本逐字「生产档 acsd_adaptive_pixel 的 n≥16 已改投 "
           "winsorized_sigma，本分支不动」⇒ n ≥ 16 上生产档与对照档**必须给出不同结果**；"
           "同时 4 ≤ n ≤ 15 两档同表（本表采纳 WBPP 的档界 6 / 15 两处）。",
    inputs="同一 n 同时解析 profile=acsd_adaptive_pixel 与 profile=wbpp_2_9_1，n ∈ [1,64]",
    expected="n ≥ 16：生产 = WINSORIZED_SIGMA(2) ≠ 对照 = LINEAR_FIT(4)（两点之差逐点非零）；"
             "4 ≤ n ≤ 15：两档逐点相等",
    source="docs/science/REJECTION.md §5(:66-69) 逐字「n ≥ 16 档同走 winsorized_sigma …… 对照档 "
           "wbpp_2_9_1 / acsd_adaptive 保持 linear_fit 档配置」+ §5(:76) 逐字「本表采纳 WBPP 的档界"
           "（6 / 15 两处）」+ §16(:339-340)；被测面 rejection.cpp:1270-1284",
    criteria=["S1", "S2"],
)
def s2_pair_prod_vs_ctrl():
    with harness.evidence() as ev:
        for n in (16, 17, 20, 32, 64):
            prod = method_at(PROFILE_PROD, n)
            ctrl = method_at(PROFILE_CTRL, n)
            harness.exact(prod, canon_prod_method(n), f"生产档 n={n}")
            harness.exact(ctrl, canon_ctrl_method(n), f"对照档 n={n}")
            gap = float(abs(prod - ctrl))
            harness.is_true(gap > ROUTING_EXACT,
                            f"n={n}: 生产档与对照档塌成同一结果（都 {prod}）⇒ 迁移被拆开等于没迁")
            ev.record(f"n={n} 两档 method 差 |prod − ctrl|", gap, ROUTING_EXACT,
                      note=f"生产 {prod} / 对照 {ctrl}，正本要求 n≥16 两档不同")
        for n in (4, 5, 6, 10, 15):
            prod = method_at(PROFILE_PROD, n)
            ctrl = method_at(PROFILE_CTRL, n)
            harness.exact(prod, ctrl, f"n={n}: 4..15 段两档应同表")
            ev.record(f"n={n} 两档 method 差 |prod − ctrl|", float(abs(prod - ctrl)),
                      ROUTING_EXACT, note="正本 §5：两档在 4..15 共用 WBPP 档界，应为 0")


@harness.test(
    "rejection-S2-ALIAS-SAME-TABLE",
    intent="历史 alias `wbpp_current` 与可调档 `acsd_adaptive` 必须与对照档解析面逐点同表，"
           "否则「对照基线」在不同调用点会悄悄换表。",
    inputs="profile ∈ {wbpp_2_9_1, wbpp_current, acsd_adaptive}，n ∈ [1,64]，request=AUTO",
    expected="同一 n 下三个 profile 的 method 逐点精确相等，且等于 canon_ctrl_method(n)",
    source="docs/science/REJECTION.md §4(:53) 逐字「`wbpp_current`（历史 alias，解析为 `wbpp_2_9_1`）」"
           "「`acsd_adaptive`（可调档，与对照档同阈）」+ §5(:98) 逐字「acsd_adaptive (tunable): 同阈但可配置 large_scale 等」",
    criteria=["S2"],
)
def s2_alias_same_table():
    with harness.evidence() as ev:
        for n in PROD_SWEEP_DOMAIN:
            base = method_at(PROFILE_CTRL, n)
            for profile in (PROFILE_ALIAS, PROFILE_TUNABLE):
                got = method_at(profile, n)
                harness.exact(got, base, f"n={n}: {profile} 与对照档解析面不一致")
            harness.exact(base, canon_ctrl_method(n), f"对照档 n={n}")
        ev.record("三个 profile 逐点比对范围", "n ∈ [1,64]",
                  note="wbpp_2_9_1 / wbpp_current / acsd_adaptive 三方同表")


# ===========================================================================
# S3 · 卫星线受控注入的**否定式**断言（单元层测的是决策函数，不是召回率）
# ===========================================================================

@harness.test(
    "rejection-S3-PROD-N1TO3-NO-CLAIM",
    intent="正本 §11(:221) 逐字「生产档 `n ≤ 3` 路由 `none` …… ⇒ 该域不宣称可剔」在生产档的"
           "落地：n ∈ {1,2,3} 上「路由 none」与「内核闸判 UNDERDETERMINED」两面同时成立。"
           "**判别力来源如实标注**：该域的零剔除由「路由 method=none」保证，所以真正区分"
           "「闸发了火」与「闸没发火」的量是 status（UNDERDETERMINED vs OK），不是剔除数；"
           "用同族夹具在 n=4（闸不发火 ⇒ status=OK 且真判出 1 个 REJECTED_HIGH）作旁证，"
           "证明该断言不是恒真。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, n ∈ {1,2,3} 与旁证 n=4；"
           "栈按几何 n 取同族夹具 FIXTURE_BY_N（判据带 [-200,+100]，median=1000）",
    expected="n ∈ {1,2,3}：method = NONE(0)、underdetermined_n = 3、内核 status = UNDERDETERMINED(4)、"
             "accepted = n、剔除数 = 0；旁证 n=4：status = OK(0)、REJECTED_HIGH 1 个",
    source="docs/science/REJECTION.md §11(:221) 卫星线注入门逐字「生产档 n ≤ 3 路由 none、"
           "对照档 n ≤ 2 由内核闸判 UNDERDETERMINED ⇒ 该域不宣称可剔」+ §5(:115-116) 逐字"
           "「UNDERDETERMINED (n ≤2) → 不做猜测，全接受」+ §4(:44-48) underdetermined_n 默认值；"
           "被测面 rejection.cpp:1154-1156, :1232-1235, :2182-2188",
    criteria=["S3", "S1"],
)
def s3_prod_n1to3_no_claim():
    with harness.evidence() as ev:
        for n in (1, 2, 3):
            plan = resolve(PROFILE_PROD, n)[1]
            harness.exact(plan.method, METHOD_NONE, f"生产档 n={n} 必须路由 NONE（正本 §5）")
            harness.exact(plan.underdetermined_n, UNDET_PROD_AUTO,
                          f"生产档 request=AUTO 的 underdetermined_n 默认（正本 §4）")
            values = FIXTURE_BY_N[n]
            dec = ref.p2_reject_stack_ex(values, plan)
            harness.exact(dec.status, STATUS_UNDERDETERMINED,
                          f"n={n}: 内核闸必须判 UNDERDETERMINED（不冒充排异成功）")
            harness.exact(dec.accepted_count, n, f"n={n}: 欠定域必须全接受")
            harness.exact(dec.rejected_total, 0, f"n={n}: 该域不得产出排异判定")
            harness.is_true(all(r == REASON_UNDERDETERMINED for r in dec.reasons),
                            f"n={n}: 逐样本原因必须全为 UNDERDETERMINED")
            ev.record(f"生产档 n={n} status / 剔除数", f"{dec.status} / {dec.rejected_total}",
                      note="正本 §11：该域不宣称可剔")
        # 判别力旁证：同族夹具在 n=4（闸不发火）上 status 变为 OK 且真判出剔除
        plan4 = resolve(PROFILE_PROD, 4)[1]
        dec4 = ref.p2_reject_stack_ex(FIXTURE_BY_N[4], plan4)
        harness.exact(dec4.status, STATUS_OK,
                      "旁证失败：n=4 的闸也没发火 ⇒ n≤3 的 UNDERDETERMINED 断言恒真、无牙齿")
        harness.exact(dec4.rejected_high, 1,
                      f"旁证失败：同族夹具在 n=4 上剔除数实测 {dec4.rejected_total}（期望 1）"
                      "⇒ 夹具本身不可判别，n≤3 的零剔除断言无牙齿")
        ev.record("旁证 n=4 status / 剔除数", f"{dec4.status} / {dec4.rejected_total}",
                  note="闸不发火 ⇒ OK + 真剔除，证明 n≤3 的闸判定非恒真")


@harness.test(
    "rejection-S3-CTRL-N2-NO-CLAIM",
    intent="正本 §11(:221) 逐字「对照档 `n ≤ 2` 由内核闸判 `UNDERDETERMINED` ⇒ 该域不宣称可剔」"
           "的**否定式**断言：n=2 的对照档栈不得产出任何排异判定。这条断言最容易退化为恒真，"
           "因此同批用同一族夹具证明它**不是**恒真 —— n=3 时闸不发火、同一条 percentile 判据带"
           "真的判出 1 个 REJECTED_HIGH。",
    inputs="profile=wbpp_2_9_1, request=AUTO；n=2 栈 FIXTURE_N2=(850,1150)，"
           "n=3 旁证栈 FIXTURE_N3=(850,1000,1150)；判据带 [-200,+100]（median=1000）",
    expected="n=2：method = PERCENTILE(7)、underdetermined_n = 2、status = UNDERDETERMINED(4)、"
             "accepted = 2、剔除数 = 0；旁证 n=3：status = OK(0)、剔除数 = 1",
    source="docs/science/REJECTION.md §11(:221) + §4(:51) 逐字「对照档 n ≤ 2 由内核闸判 UNDERDETERMINED」"
           "与 §4(:45-48)「其余 profile ⇒ 2」+ §7(:134) 逐字「UNDERDETERMINED 单调性：n ≤2 恒 UNDERDETERMINED，"
           "不做剔除（recall=0 显式）」+ §8a(:154) 判据带；被测面 rejection.cpp:1275-1284, :2182-2188, :1876-1897",
    criteria=["S3", "S2"],
)
def s3_ctrl_n2_no_claim():
    with harness.evidence() as ev:
        plan2 = resolve(PROFILE_CTRL, 2)[1]
        harness.exact(plan2.method, METHOD_PERCENTILE,
                      "对照档 n=2 的路由取值（正本 §4(:51) 逐字 method=7）")
        harness.exact(plan2.underdetermined_n, UNDET_CTRL,
                      "对照档 underdetermined_n 默认（正本 §4(:45-48)）")
        dec2 = ref.p2_reject_stack_ex(FIXTURE_N2, plan2)
        harness.exact(dec2.status, STATUS_UNDERDETERMINED, "n=2 必须由内核闸判 UNDERDETERMINED")
        harness.exact(dec2.accepted_count, 2, "n=2 全接受")
        harness.exact(dec2.rejected_total, 0,
                      "n=2 不得产出任何排异判定（正本 §11：该域不宣称可剔）")
        ev.record("n=2 status / 剔除数", f"{dec2.status} / {dec2.rejected_total}",
                  ROUTING_EXACT, note="正本 §11：n≤2 不宣称可剔 ⇒ 剔除数必须为 0")
        # 非退化旁证：同族夹具在 n=3 上闸不发火，判据带真的判出 1 个 REJECTED_HIGH
        plan3 = resolve(PROFILE_CTRL, 3)[1]
        dec3 = ref.p2_reject_stack_ex(FIXTURE_N3, plan3)
        harness.exact(dec3.status, STATUS_OK, "旁证失败：n=3 的闸也发火了")
        harness.exact(dec3.rejected_high, 1,
                      f"旁证失败：n=3 的剔除数实测 {dec3.rejected_total}，"
                      "期望 1（+150 > +0.1·1000）⇒ n=2 的零剔除断言恒真、无牙齿")
        ev.record("旁证 n=3 status / 剔除数", f"{dec3.status} / {dec3.rejected_total}",
                  note="同族夹具真的能判出剔除 ⇒ n=2 的零剔除不是恒真")


# ===========================================================================
# S3 负例 · 内核闸的两处决策行
# ===========================================================================

@harness.test(
    "rejection-S3-NEG-UNDET-DEFAULT-2-TO-1",
    intent="注入缺陷 D5：把非 pixel 档 underdetermined_n 默认从 2 降到 1（正是被测注释"
           "「若要强制 percentile 排异需把本默认降到 2」描述的那一处决策落地）。"
           "独立判据 = 正本 §4 的默认值 + §11 的「n ≤ 2 不宣称可剔」否定式断言。",
    inputs="注入 D5；profile=wbpp_2_9_1, request=AUTO, n=2，栈 FIXTURE_N2=(850,1150)",
    expected="判据必须变红：plan.underdetermined_n 实测 1（正本 §4 要求 2，超界 1）；"
             "内核在 n=2 上产出 REJECTED_HIGH 1 个（正本要求 0，超界 1）",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1231（默认值 2）；"
           "判据来源 docs/science/REJECTION.md §4(:45-48)「其余 profile ⇒ 2」+ §11(:221)",
    criteria=["S3", "S2"],
    kind=harness.NEGATIVE,
    defect_id=ref.D5_UNDET_DEFAULT_2_TO_1,
    inject="非 pixel 档 underdetermined_n 默认值 2 → 1（rejection.cpp:1231）",
)
def s3_neg_undet_default_2_to_1():
    with harness.evidence() as ev:
        with ref.injected(ref.D5_UNDET_DEFAULT_2_TO_1):
            plan = resolve(PROFILE_CTRL, 2)[1]
            dec = ref.p2_reject_stack_ex(FIXTURE_N2, plan)
        ev.record("注入后 plan.underdetermined_n（n=2 对照档）", plan.underdetermined_n,
                  note="正本 §4 要求 2")
        viol_undet = float(abs(plan.underdetermined_n - UNDET_CTRL))
        ev.record("判据违背量 |underdetermined_n − 正本默认|", viol_undet, ROUTING_EXACT,
                  note=f"注入行 {ref.DEFECT_LINE[ref.D5_UNDET_DEFAULT_2_TO_1]}")
        ev.record("注入后 n=2 内核 status / 剔除数", f"{dec.status} / {dec.rejected_total}",
                  note="正本 §11：n≤2 由闸判 UNDERDETERMINED、剔除数 0")
        viol_rej = float(dec.rejected_total)
        ev.record("判据违背量 n=2 剔除数 − 0", viol_rej, ROUTING_EXACT,
                  note="该域从「不宣称可剔」变成「宣称可剔」")
        harness.is_true(viol_undet > ROUTING_EXACT or viol_rej > ROUTING_EXACT,
                        "注入 D5 后判据仍未变红 ⇒ 这条负例没有牙齿")


@harness.test(
    "rejection-S3-NEG-KERNEL-GATE-CONDITION",
    intent="注入缺陷 D6：把内核闸条件从 `n ≤ underdetermined_n` 改成 `n < 1`（闸永不发火），"
           "而 underdetermined_n 的**取值保持正确的 2**。这条把「闸条件本身」与「默认值」"
           "两处决策行分开判，避免只测默认值而漏掉闸条件的回归。",
    inputs="注入 D6；profile=wbpp_2_9_1, request=AUTO, n=2（underdetermined_n 正确 = 2），"
           "栈 FIXTURE_N2=(850,1150)",
    expected="判据必须变红：内核在 n=2 上产出 REJECTED_HIGH 1 个（正本 §11 要求 0），"
             "status 由 UNDERDETERMINED 变为 OK",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:2182（`n <= plan->underdetermined_n`）；"
           "判据来源 docs/science/REJECTION.md §4(:44) 逐字「n <= underdetermined_n 或 n < minimum_n ⇒ UNDERDETERMINED」"
           "+ §11(:221)",
    criteria=["S3"],
    kind=harness.NEGATIVE,
    defect_id=ref.D6_KERNEL_GATE_N_LT_1,
    inject="内核闸条件 n ≤ underdetermined_n → n < 1（rejection.cpp:2182）",
)
def s3_neg_kernel_gate_condition():
    with harness.evidence() as ev:
        with ref.injected(ref.D6_KERNEL_GATE_N_LT_1):
            plan = resolve(PROFILE_CTRL, 2)[1]
            dec = ref.p2_reject_stack_ex(FIXTURE_N2, plan)
        harness.exact(plan.underdetermined_n, UNDET_CTRL,
                      "前提：本注入只动闸条件，默认值应保持正确值 2")
        ev.record("注入后 n=2 内核 status", dec.status,
                  note="正本 §4：n ≤ underdetermined_n ⇒ UNDERDETERMINED(4)")
        ev.record("注入后 n=2 剔除数", float(dec.rejected_total), ROUTING_EXACT,
                  note="正本 §11：n≤2 不宣称可剔 ⇒ 剔除数必须为 0")
        viol = float(dec.rejected_total)
        harness.is_true(viol > ROUTING_EXACT,
                        "注入 D6 后判据仍未变红 ⇒ 这条负例没有牙齿")


# ===========================================================================
# S4 · 归一化默认值 + 耦合不变量
# ===========================================================================

@harness.test(
    "rejection-S4-NORMALIZATION-DEFAULT",
    intent="正本 §9(:208) 逐字「归一化默认 `acsd_median_center_v1`（`normalization=MEDIAN_CENTER`）」："
           "两档 AUTO 与显式方法解析出的 plan 归一化都必须是 MEDIAN_CENTER(1)；"
           "唯一的例外是显式 opt-in 的 extreme_value_clip_prior_sigma（按原始值域比绝对 prior_sky）。",
    inputs="profile ∈ {acsd_adaptive_pixel, wbpp_2_9_1} × n ∈ {4,16,20} 的 AUTO 解析；"
           "外加 request ∈ {NONE, SIGMA, WINSORIZED_SIGMA, LINEAR_FIT, PERCENTILE, RCR} 的显式解析；"
           "以及 request=EXTREME_VALUE_PRIOR_SIGMA 的显式解析",
    expected="除 extreme_value_clip_prior_sigma 外全部 = MEDIAN_CENTER(1)，精确一致；"
             "extreme_value_clip_prior_sigma = NONE(0)",
    source="docs/science/REJECTION.md §9(:208) 归一化默认逐字；例外项见 "
           "lib/algorithms/coverage/include/astro/phase2/rejection.h:197-198 逐字"
           "「normalization 必须 = P2_NORMALIZE_NONE（本方法在原始 calibrated 值域直接比较绝对 prior_sky）」"
           "与 rejection.cpp:1300-1301；被测面 rejection.cpp:1239",
    criteria=["S4"],
)
def s4_normalization_default():
    with harness.evidence() as ev:
        cases: list[tuple[str, int, int, int]] = []
        for profile in (PROFILE_PROD, PROFILE_CTRL):
            for n in (4, 16, 20):
                cases.append((f"AUTO {profile} n={n}", METHOD_AUTO, n, NORMALIZE_MEDIAN_CENTER))
        for request in (METHOD_NONE, METHOD_SIGMA, METHOD_WINSORIZED_SIGMA, METHOD_LINEAR_FIT,
                        METHOD_PERCENTILE, METHOD_RCR):
            cases.append((f"显式 request={request}", request, 10, NORMALIZE_MEDIAN_CENTER))
        cases.append(("显式 extreme_value_clip_prior_sigma", METHOD_EXTREME_VALUE_PRIOR_SIGMA,
                      4, NORMALIZE_NONE))
        for label, request, n, want in cases:
            plan = resolve(PROFILE_PROD, n, request=request)[1]
            harness.exact(plan.normalization, want, f"{label} 的归一化默认值")
            ev.record(f"{label} → normalization", plan.normalization,
                      NORMALIZATION_EXACT, note=f"正本 §9 要求 {want}")


@harness.test(
    "rejection-S4-FROZEN-ANCHORS",
    intent="正本 §5(:105-111)「阈值冻结锚点」逐字的那张表整表断言：percentile low 0.2 / high 0.1"
           "是本单元必须覆盖的冻结阈值常量；同时把同段其余锚点一并锁住，避免只锁两个数"
           "而让其余锚点悄悄漂移。normalization_floor = 1e-12 按**代码级冻结常量**标注"
           "（正本只冻结 §8a 的 scale=|median|，未给 floor 数值）。",
    inputs="profile=acsd_adaptive_pixel, request=AUTO, n=16（任意档都返回同一张默认锚点表）",
    expected="sigma/winsorized/averaged = 4.0/3.0/8；linear_fit = 5.0/3.5/8；"
             "esd = 0.05/10；percentile = 0.2/0.1；minmax = 1/1/4；large_scale = 0/8/2/2；"
             "normalization_floor = 1e-12；全部精确一致",
    source="docs/science/REJECTION.md §5(:105-111)「阈值冻结锚点 (SCI-REJ / ALG-REJ-001..008)」逐字；"
           "normalization_floor 见 lib/algorithms/coverage/include/astro/phase2/rejection.h:213 逐字"
           "「MEDIAN_SCALE 的最小 |median|（默认 1e-12）」与 rejection.cpp:1241；被测面 rejection.cpp:1241-1259",
    criteria=["S4"],
)
def s4_frozen_anchors():
    with harness.evidence() as ev:
        plan = resolve(PROFILE_PROD, 16)[1]
        for name in ("sigma", "winsorized", "averaged", "median_sigma"):
            got = (getattr(plan, name).lower_sigma, getattr(plan, name).upper_sigma,
                   getattr(plan, name).max_iterations)
            harness.exact(got, FROZEN_SIGMA, f"{name} 的冻结锚点（正本 §5）")
            ev.record(f"{name} lower/upper/max_iter", got, note="正本 §5: sigma/winsorized/averaged: 4.0/3.0/8")
        got = (plan.linear_fit.lower, plan.linear_fit.upper, plan.linear_fit.max_iterations)
        harness.exact(got, FROZEN_LINEAR_FIT, "linear_fit 的冻结锚点（正本 §5）")
        ev.record("linear_fit lower/upper/max_iter", got, note="正本 §5: linear_fit: 5.0/3.5/8")
        got = (plan.esd.alpha, plan.esd.max_outliers)
        harness.exact(got, FROZEN_ESD, "ESD 的冻结锚点（正本 §5）")
        ev.record("esd alpha/max_outliers", got, note="正本 §5: ESD: alpha 0.05 / max_outliers 10")
        got = (plan.percentile.low_fraction, plan.percentile.high_fraction)
        harness.exact(got, FROZEN_PERCENTILE, "percentile 的冻结阈值（正本 §5）")
        ev.record("percentile low/high_fraction", got, note="正本 §5 与 §8a:154: plow=0.2 / phigh=0.1")
        got = (plan.minmax.reject_low_count, plan.minmax.reject_high_count, plan.minmax.min_kept)
        harness.exact(got, FROZEN_MINMAX, "minmax 的冻结锚点（正本 §5）")
        ev.record("minmax reject_low/reject_high/min_kept", got, note="正本 §5: minmax: 1/1/4")
        got = (plan.large_scale.enabled, plan.large_scale.min_structure_pixels,
               plan.large_scale.low_grow_radius_pixels, plan.large_scale.high_grow_radius_pixels)
        harness.exact(got, FROZEN_LARGE_SCALE, "large_scale 的冻结锚点（正本 §5）")
        ev.record("large_scale enabled/min_structure/low/high_radius", got,
                  note="正本 §5: large_scale 默认关闭 0/8/2/2")
        harness.exact(plan.normalization_floor, FROZEN_NORMALIZATION_FLOOR,
                      "normalization_floor 的冻结常量（rejection.h:213）")
        ev.record("normalization_floor", plan.normalization_floor,
                  note="代码级冻结常量；正本只冻结 §8a 的 scale=|median|")


@harness.test(
    "rejection-S4-PCTL-COUPLING-PLAN",
    intent="S4 的耦合约束「`percentile ⟹ MEDIAN_CENTER`」在规划/compat 面逐方法断言："
           "percentile 分支必须给 MEDIAN_CENTER，其余方法给 NONE。用非 percentile 方法的"
           "取值作非退化旁证，证明这不是一个常量函数。",
    inputs="对全部 12 个方法枚举值取 compat 归一化耦合的返回值",
    expected="method = PERCENTILE(7) ⇒ MEDIAN_CENTER(1)；其余 11 个方法 ⇒ NONE(0)；"
             "逐点精确一致；且两支都非空（存在 PERCENTILE 与非 PERCENTILE 两类）",
    source="T02 §2.1 S4 行逐字「耦合约束 percentile ⟹ MEDIAN_CENTER」；"
           "被测面 lib/algorithms/coverage/src/rejection.cpp:2374-2378 逐字"
           "「compat 保持旧行为：sigma 类方法 shift-invariant → NONE 等价；"
           "percentile 必须 MEDIAN_CENTER（|median| 尺度，负值安全）」；正本 §8a(:156-157) 同向",
    criteria=["S4"],
)
def s4_pctl_coupling_plan():
    with harness.evidence() as ev:
        seen: set[int] = set()
        for method in METHODS_ALL:
            got = ref.compat_normalization_for(method)
            want = NORMALIZE_MEDIAN_CENTER if method == METHOD_PERCENTILE else NORMALIZE_NONE
            harness.exact(got, want, f"method={method} 的 compat 归一化耦合")
            seen.add(got)
            ev.record(f"method {method} → compat normalization", got,
                      NORMALIZATION_EXACT, note=f"耦合要求 {want}")
        harness.is_true(NORMALIZE_MEDIAN_CENTER in seen and NORMALIZE_NONE in seen,
                        "耦合函数退化成常量 ⇒ 断言无牙齿")


@harness.test(
    "rejection-S4-PCTL-COUPLING-KERNEL",
    intent="正本 §8a(:156-157) 逐字「`normalization=MEDIAN_CENTER` **由内核强制**（`rejection.cpp`）」："
           "手工构造 method=percentile + normalization=NONE 的 plan，内核必须显式 fail-closed"
           "（INVALID_CONFIGURATION + 整栈免检），不得静默在错误工作域上跑判据。"
           "同批用 MEDIAN_CENTER 的同族栈作旁证（真判出 1 个 REJECTED_HIGH），证明该门不是恒真。",
    inputs="手工 plan（method=PERCENTILE），分别置 normalization ∈ {NONE, MEDIAN_CENTER}；栈 FIXTURE_N3",
    expected="NONE ⇒ status = INVALID_CONFIGURATION(5)、剔除数 0、逐样本原因全 UNDERDETERMINED；"
             "MEDIAN_CENTER ⇒ status = OK(0)、REJECTED_HIGH 1 个",
    source="docs/science/REJECTION.md §8a(:156-157)「工作域 = v − median，normalization=MEDIAN_CENTER 由内核强制」"
           "+ §8(:149)「配置非法 (method/profile) | INVALID_CONFIGURATION/INVALID_METHOD」；"
           "被测面 lib/algorithms/coverage/src/rejection.cpp:2143-2152",
    criteria=["S4"],
)
def s4_pctl_coupling_kernel():
    with harness.evidence() as ev:
        base = resolve(PROFILE_CTRL, 3)[1]
        bad = with_normalization(base, NORMALIZE_NONE)
        dec_bad = ref.p2_reject_stack_ex(FIXTURE_N3, bad)
        harness.exact(dec_bad.status, STATUS_INVALID_CONFIGURATION,
                      "percentile + NONE 必须被内核 fail-closed（正本 §8a）")
        harness.exact(dec_bad.rejected_total, 0, "非法组合不得产出任何排异判定")
        harness.is_true(all(r == REASON_UNDERDETERMINED for r in dec_bad.reasons),
                        "非法组合必须整栈免检（逐样本原因全为 UNDERDETERMINED）")
        ev.record("percentile + NONE → status / 剔除数",
                  f"{dec_bad.status} / {dec_bad.rejected_total}",
                  note="正本 §8a：归一化由内核强制")
        dec_ok = ref.p2_reject_stack_ex(FIXTURE_N3, base)
        harness.exact(dec_ok.status, STATUS_OK, "旁证失败：MEDIAN_CENTER 分支也没跑成")
        harness.exact(dec_ok.rejected_high, 1,
                      f"旁证失败：同族栈在合法组合下剔除数实测 {dec_ok.rejected_total}（期望 1）"
                      "⇒ 内核门恒真、无牙齿")
        ev.record("percentile + MEDIAN_CENTER → status / 剔除数",
                  f"{dec_ok.status} / {dec_ok.rejected_total}",
                  note="旁证：同一判据带真能判出剔除")


@harness.test(
    "rejection-S4-ROUTING-ADMISSIBLE",
    intent="路由面与内核面的合成不变量：AUTO 解析出的每个 plan 都必须是内核**可接受**的组合"
           "（方法×归一化合法 + 方法显式）。正本 §8a 的「内核强制」+ §12 的「AUTO 只在规划层"
           "解析、永不进方法核」共同蕴含该不变量；违反它意味着某个 n 会静默走进 INVALID_CONFIGURATION。",
    inputs="profile ∈ {acsd_adaptive_pixel, wbpp_2_9_1, wbpp_current, acsd_adaptive} × n ∈ [0,64]，request=AUTO",
    expected="每个解析结果都通过 kernel_method_x_normalization_legal，且 method 显式（非 AUTO）",
    source="docs/science/REJECTION.md §8a(:156-157) 归一化由内核强制 + §12(:232-233) 逐字"
           "「AUTO 只在规划层解析、永不进方法核」+ §4(:53) 合法 profile 集；"
           "被测面 rejection.cpp:2143-2168, :2195-2207",
    criteria=["S4", "S1", "S2"],
)
def s4_routing_admissible():
    with harness.evidence() as ev:
        illegal: list[tuple[str, int, int, int]] = []
        for profile in (PROFILE_PROD, PROFILE_CTRL, PROFILE_ALIAS, PROFILE_TUNABLE):
            for n in range(0, 65):
                plan = resolve(profile, n)[1]
                if not ref.kernel_method_x_normalization_legal(plan.method, plan.normalization):
                    illegal.append((profile, n, plan.method, plan.normalization))
                harness.is_true(ref.method_is_explicit(plan.method),
                                f"{profile} n={n}: 解析结果含 AUTO（正本 §12）")
        harness.is_false(bool(illegal), f"AUTO 解析面产出内核非法组合：{illegal}")
        ev.record("扫描组合数", 4 * 65, note="4 profile × n∈[0,64]")
        ev.record("内核非法组合数", float(len(illegal)), 0.0, note="必须为 0")


@harness.test(
    "rejection-S4-UNDET-DEFAULT-MATRIX",
    intent="正本 §4(:45-48) 把 `underdetermined_n` 的默认值写成 **profile × request 的二维表**："
           "`acsd_adaptive_pixel` ∧ AUTO ⇒ 3、同档 ∧ extreme_value_clip_prior_sigma ⇒ 1、"
           "其余 profile ⇒ 2，且「调用方显式传 `underdetermined_n>0` 时以显式值为准」。"
           "只测其中一维会让「按 request 分档」这一半无人看守。",
    inputs="profile ∈ {acsd_adaptive_pixel, wbpp_2_9_1, wbpp_current, acsd_adaptive} × "
           "request ∈ {AUTO, EXTREME_VALUE_PRIOR_SIGMA}，underdetermined_n=0（走默认）；"
           "外加 pixel ∧ request=AUTO ∧ 显式 underdetermined_n ∈ {1,5} 的覆盖点",
    expected="pixel: AUTO ⇒ 3、EXTREME ⇒ 1；其余三 profile: AUTO ⇒ 2、EXTREME ⇒ 2；"
             "显式传值时以显式值为准（1 与 5）；全部精确一致",
    source="docs/science/REJECTION.md §4(:44-48) 逐字「`underdetermined_n` 的默认值由 profile 与 request 决定"
           "（rejection.cpp，实测）：`acsd_adaptive_pixel` ∧ `request=AUTO` ⇒ 3（生产默认档）；"
           "`acsd_adaptive_pixel` ∧ `request=extreme_value_clip_prior_sigma` ⇒ 1；"
           "其余 profile（`wbpp_2_9_1`/`wbpp_current`/`acsd_adaptive`）⇒ 2。"
           "调用方显式传 `underdetermined_n>0` 时以显式值为准」；"
           "被测面 lib/algorithms/coverage/src/rejection.cpp:1222-1238（注释 :1226-1229 逐字"
           "「此后该值与路由档**解耦**」——本用例断言的是默认值表本身，不预设该解耦的任何后果）",
    criteria=["S4", "S3", "S1", "S2"],
)
def s4_undet_default_matrix():
    with harness.evidence() as ev:
        expect = {
            (PROFILE_PROD, METHOD_AUTO): UNDET_PROD_AUTO,
            (PROFILE_PROD, METHOD_EXTREME_VALUE_PRIOR_SIGMA): UNDET_PROD_EXTREME,
            (PROFILE_CTRL, METHOD_AUTO): UNDET_CTRL,
            (PROFILE_CTRL, METHOD_EXTREME_VALUE_PRIOR_SIGMA): UNDET_CTRL,
            (PROFILE_ALIAS, METHOD_AUTO): UNDET_CTRL,
            (PROFILE_ALIAS, METHOD_EXTREME_VALUE_PRIOR_SIGMA): UNDET_CTRL,
            (PROFILE_TUNABLE, METHOD_AUTO): UNDET_CTRL,
            (PROFILE_TUNABLE, METHOD_EXTREME_VALUE_PRIOR_SIGMA): UNDET_CTRL,
        }
        for (profile, request), want in expect.items():
            for n in (2, 6):
                plan = resolve(profile, n, request=request)[1]
                harness.exact(plan.underdetermined_n, want,
                              f"{profile} ∧ request={request} ∧ n={n} 的 underdetermined_n 默认")
                ev.record(f"{profile} ∧ request={request} → underdetermined_n",
                          plan.underdetermined_n, note=f"正本 §4 要求 {want}")
        for explicit in (1, 5):
            plan = resolve(PROFILE_PROD, 6, request=METHOD_AUTO,
                           underdetermined_n=explicit)[1]
            harness.exact(plan.underdetermined_n, explicit,
                          "显式传值时以显式值为准（正本 §4）")
            ev.record("pixel ∧ AUTO ∧ 显式传 underdetermined_n", plan.underdetermined_n,
                      note="正本 §4：显式值优先")


# ===========================================================================
# S1 / S2 负例 · 路由表的两处决策行
# ===========================================================================

@harness.test(
    "rejection-S1-NEG-SMALL-N-PERCENTILE",
    intent="注入缺陷 D1：把 `1 ≤ n ≤ 3` 的保守 none 改回 WBPP 对称读法（kWbppTable，"
           "即被测代码自己标注的「未采用」分支）。正本 §5 与 §16 明确小 N 段取保守读法，"
           "该注入必须被 S1 的表驱动判据抓住。",
    inputs="注入 D1；profile=acsd_adaptive_pixel, request=AUTO, n ∈ {1,2,3}（另附 n=4,5 对照）",
    expected="判据必须变红：n ∈ {1,2,3} 的 method 实测 PERCENTILE(7)、正本要求 NONE(0)，"
             "逐点超界 7；n ∈ {4,5} 不受影响（超界 0）",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1140-1147（kWbppTable 分支）；"
           "判据来源 docs/science/REJECTION.md §5(:63) 逐字「1 ≤ n ≤ 3 → none …… 依据：低电平强制 percentile 有损」"
           "+ §16(:333-337)「小 N 档位取舍的依据 …… 生产档取 1 ≤ N ≤ 3 → none（保守读法）」",
    criteria=["S1", "S3"],
    kind=harness.NEGATIVE,
    defect_id=ref.D1_SMALL_N_WBPP_TABLE,
    inject="小 N 保守读法 kConservativeNone → WBPP 对称读法 kWbppTable（rejection.cpp:1142）",
)
def s1_neg_small_n_percentile():
    with harness.evidence() as ev:
        with ref.injected(ref.D1_SMALL_N_WBPP_TABLE):
            for n in (1, 2, 3):
                got = method_at(PROFILE_PROD, n)
                want = canon_prod_method(n)
                delta = float(abs(got - want))
                harness.is_true(delta > ROUTING_EXACT,
                                f"注入 D1 后 n={n} 仍未被正本判据抓住（method={got}）⇒ 无牙齿")
                ev.record(f"n={n} 注入后 method", got,
                          note=f"正本 §5/§16 期望 {want}（保守读法 none）")
                ev.record(f"n={n} 判据违背量 |method − 正本期望|", delta, ROUTING_EXACT,
                          note=f"注入行 {ref.DEFECT_LINE[ref.D1_SMALL_N_WBPP_TABLE]}")
            for n in (4, 5):
                got = method_at(PROFILE_PROD, n)
                harness.exact(got, canon_prod_method(n), f"n={n} 不应受该注入影响")
                ev.record(f"n={n} 对照 method", got, note="注入只应改 1..3 段")
        ev.record("受影响点数", 3.0, note="n=1,2,3")


@harness.test(
    "rejection-S1-NEG-N16-LINEAR-FIT",
    intent="注入缺陷 D2：把 `n ≥ 16` 改回 `linear_fit`（撤掉 M3 改投 winsorized_sigma）。"
           "正本 §5(:66-69) 明写该档已改投 winsorized_sigma，注入必须在 n=16 上判红。",
    inputs="注入 D2；profile=acsd_adaptive_pixel, request=AUTO, n ∈ {16,17,20,32}",
    expected="判据必须变红：每个 n 的 method 实测 LINEAR_FIT(4)、正本要求 WINSORIZED_SIGMA(2)，超界 2",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1169；"
           "判据来源 docs/science/REJECTION.md §5(:65-69) 逐字「n ≥ 6 → winsorized_sigma …… "
           "（n ≥ 16 档同走 winsorized_sigma）」+ §9a(:260)",
    criteria=["S1"],
    kind=harness.NEGATIVE,
    defect_id=ref.D2_N16_LINEAR_FIT,
    inject="n ≥ 16 档 winsorized_sigma → linear_fit（rejection.cpp:1169，M3 改投被撤回）",
)
def s1_neg_n16_linear_fit():
    with harness.evidence() as ev:
        with ref.injected(ref.D2_N16_LINEAR_FIT):
            for n in (16, 17, 20, 32):
                got = method_at(PROFILE_PROD, n)
                want = canon_prod_method(n)
                delta = float(abs(got - want))
                harness.is_true(delta > ROUTING_EXACT,
                                f"注入 D2 后 n={n} 仍未被正本判据抓住（method={got}）⇒ 无牙齿")
                ev.record(f"n={n} 注入后 method", got,
                          note=f"正本 §5 期望 {want}（n≥16 档改投 winsorized_sigma）")
                ev.record(f"n={n} 判据违背量 |method − 正本期望|", delta, ROUTING_EXACT,
                          note=f"注入行 {ref.DEFECT_LINE[ref.D2_N16_LINEAR_FIT]}")


@harness.test(
    "rejection-S2-NEG-COLLAPSE-CONTROL",
    intent="注入缺陷 D3：把对照档 `n > 15` 也改成 winsorized_sigma（两档塌成同一张表）。"
           "独立判据 = 正本 §5 的成对口径「生产档 n≥16 已改投 winsorized_sigma，本分支不动」"
           "⇒ 两档在 n ≥ 16 上必须给出不同结果。",
    inputs="注入 D3；profile=acsd_adaptive_pixel 与 wbpp_2_9_1 同批解析，n ∈ {16,17,20,32}",
    expected="判据必须变红：|prod − ctrl| 实测 0（两档同为 WINSORIZED_SIGMA），"
             "正本要求逐点非零（应 ≥ 1）",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1283；"
           "判据来源 docs/science/REJECTION.md §5(:66-69) + §5(:90-93) + §14(:271) 逐字"
           "「对照档 wbpp_2_9_1 的 auto 路由与阈值表为本仓解析表 …… 其 n > 15 档取 linear_fit」",
    criteria=["S2", "S1"],
    kind=harness.NEGATIVE,
    defect_id=ref.D3_CTRL_N16_WINSORIZED,
    inject="对照档 n > 15 档 linear_fit → winsorized_sigma（rejection.cpp:1283，两档塌成同一张表）",
)
def s2_neg_collapse_control():
    with harness.evidence() as ev:
        with ref.injected(ref.D3_CTRL_N16_WINSORIZED):
            for n in (16, 17, 20, 32):
                prod = method_at(PROFILE_PROD, n)
                ctrl = method_at(PROFILE_CTRL, n)
                gap = float(abs(prod - ctrl))
                # 独立判据 = 正本 §5 的成对口径「n ≥ 16 两档必须给出不同结果」。
                # 注入 D3 后该判据必须判红（gap 归零）⇒ 断言它**确实**判红。
                harness.is_false(gap > ROUTING_EXACT,
                                 f"注入 D3 后 n={n} 的成对判据仍未变红（两档差 {gap}）⇒ 无牙齿")
                harness.exact(prod, canon_prod_method(n), f"生产档 n={n} 不应受该注入影响")
                ev.record(f"n={n} 两档 method", f"prod={prod} / ctrl={ctrl}",
                          note="正本 §5：n≥16 两档必须不同")
                ev.record(f"n={n} 成对判据违背量（1 − |prod − ctrl|，门限 0）",
                          max(0.0, 1.0 - gap), ROUTING_EXACT,
                          note=f"枚举取不同值 ⇒ 最小差 1；实测差 {gap:g} ⇒ 违背 "
                               f"{max(0.0, 1.0 - gap):g}（注入行 "
                               f"{ref.DEFECT_LINE[ref.D3_CTRL_N16_WINSORIZED]}）")


@harness.test(
    "rejection-S2-NEG-CONTROL-BOUNDARY-6",
    intent="注入缺陷 D4：把对照档 `n < 6` 档的下界写成 `n < 5`（档界 6 被挪走），"
           "使 n=5 从 percentile 变成 winsorized_sigma。对照档表的判据必须抓住它——"
           "注意这条缺陷**不**触发成对发散判据（两档在 n=5 上会一起变），"
           "所以对照档表自身的逐点断言必须独立有牙齿。",
    inputs="注入 D4；profile=wbpp_2_9_1, request=AUTO, n ∈ {4,5,6}",
    expected="判据必须变红：n=5 实测 WINSORIZED_SIGMA(2)、正本要求 PERCENTILE(7)，超界 5；"
             "n=4 与 n=6 不受影响（超界 0）",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1281；"
           "判据来源 docs/science/REJECTION.md §5(:91-92) 逐字「n < 6 → percentile …… "
           "6 ≤ n ≤ 15 → winsorized_sigma」+ §5(:76)「本表采纳 WBPP 的档界（6 / 15 两处）」",
    criteria=["S2"],
    kind=harness.NEGATIVE,
    defect_id=ref.D4_CTRL_BOUNDARY_6_TO_5,
    inject="对照档 n < 6 档下界写成 n < 5（rejection.cpp:1281，档界 6 → 5）",
)
def s2_neg_control_boundary_6():
    with harness.evidence() as ev:
        with ref.injected(ref.D4_CTRL_BOUNDARY_6_TO_5):
            for n in (4, 5, 6):
                got = method_at(PROFILE_CTRL, n)
                want = canon_ctrl_method(n)
                delta = float(abs(got - want))
                ev.record(f"n={n} 注入后 method", got,
                          note=f"正本 §5 对照档表期望 {want}")
                ev.record(f"n={n} 判据违背量 |method − 正本期望|", delta, ROUTING_EXACT,
                          note=f"注入行 {ref.DEFECT_LINE[ref.D4_CTRL_BOUNDARY_6_TO_5]}")
                if n == 5:
                    harness.is_true(delta > ROUTING_EXACT,
                                    f"注入 D4 后 n=5 仍未被抓住（method={got}）⇒ 无牙齿")
                else:
                    # 记录「注入被限制在单一档界」这一事实：n=4/6 未受影响
                    harness.exact(delta, 0.0, f"n={n} 不应受该注入影响")
                    ev.record(f"n={n} 定位读数", "未受影响（违背量 0）",
                              note="注入只挪动档界 6 ⇒ 只有 n=5 被改")


# ===========================================================================
# S4 负例 · 归一化的两处决策行
# ===========================================================================

@harness.test(
    "rejection-S4-NEG-COUPLING-ALL-NONE",
    intent="注入缺陷 D7（T02 §2.1 S4 行点名「`:2375` 的耦合最易被回归破坏」）：把归一化耦合改成"
           "「所有方法都 NONE」（`plan.normalization = P2_NORMALIZE_NONE` 无条件赋值）。"
           "独立判据 = 正本 §8a 的耦合不变量 `percentile ⟹ MEDIAN_CENTER`。",
    inputs="注入 D7；对 method = PERCENTILE 取 compat 归一化耦合返回值",
    expected="判据必须变红：耦合返回 NONE(0)、正本要求 MEDIAN_CENTER(1)，超界 1",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:2376-2378；"
           "判据来源 T02 §2.1 S4 行 + docs/science/REJECTION.md §8a(:154-157)",
    criteria=["S4"],
    kind=harness.NEGATIVE,
    defect_id=ref.D7_COUPLING_ALL_NONE,
    inject="归一化耦合改成所有方法都 NONE（rejection.cpp:2376）",
)
def s4_neg_coupling_all_none():
    with harness.evidence() as ev:
        with ref.injected(ref.D7_COUPLING_ALL_NONE):
            got = ref.compat_normalization_for(METHOD_PERCENTILE)
            want = NORMALIZE_MEDIAN_CENTER
            delta = float(abs(got - want))
            harness.is_true(delta > NORMALIZATION_EXACT,
                            f"注入 D7 后 percentile 分支仍未变红（normalization={got}）⇒ 无牙齿")
            ev.record("注入后 percentile 的 compat normalization", got,
                      NORMALIZATION_EXACT, note="正本 §8a：percentile 必须 MEDIAN_CENTER")
            ev.record("判据违背量 |normalization − 正本期望|", delta, NORMALIZATION_EXACT,
                      note=f"注入行 {ref.DEFECT_LINE[ref.D7_COUPLING_ALL_NONE]}")
            # 二阶后果：同一缺陷下，内核的门会 fail-closed（不是静默错判）
            plan = resolve(PROFILE_CTRL, 5)[1]
            plan = with_normalization(plan, got)
            dec = ref.p2_reject_stack_ex(FIXTURE_N3, plan)
            ev.record("同缺陷下内核 status", dec.status,
                      note="percentile + NONE ⇒ INVALID_CONFIGURATION(5)（正本 §8a 内核强制）")


@harness.test(
    "rejection-S4-NEG-PLAN-NORM-DEFAULT-NONE",
    intent="注入缺陷 D8：把规划层归一化默认值从 MEDIAN_CENTER 改成 NONE（rejection.cpp:1239）。"
           "独立判据 = 正本 §9 的归一化默认值 + S4 的路由 admissibility 不变量；"
           "两条判据同批给出超界读数。",
    inputs="注入 D8；profile=acsd_adaptive_pixel, request=AUTO, n=4（路由到 percentile）",
    expected="判据必须变红：plan.normalization 实测 NONE(0)、正本 §9 要求 MEDIAN_CENTER(1)，超界 1；"
             "且该组合被内核判为非法（INVALID_CONFIGURATION）",
    source="注入点 lib/algorithms/coverage/src/rejection.cpp:1239；"
           "判据来源 docs/science/REJECTION.md §9(:208)「归一化默认 acsd_median_center_v1」"
           "+ §8a(:156-157) 内核强制；被测面 rejection.cpp:1239, :2145-2152",
    criteria=["S4", "S1"],
    kind=harness.NEGATIVE,
    defect_id=ref.D8_PLAN_NORM_DEFAULT_NONE,
    inject="规划层归一化默认值 MEDIAN_CENTER → NONE（rejection.cpp:1239）",
)
def s4_neg_plan_norm_default_none():
    with harness.evidence() as ev:
        with ref.injected(ref.D8_PLAN_NORM_DEFAULT_NONE):
            plan = resolve(PROFILE_PROD, 4)[1]
            want = NORMALIZE_MEDIAN_CENTER
            delta = float(abs(plan.normalization - want))
            harness.is_true(delta > NORMALIZATION_EXACT,
                            f"注入 D8 后归一化默认值仍未变红（{plan.normalization}）⇒ 无牙齿")
            harness.exact(plan.method, METHOD_PERCENTILE, "前提：n=4 应路由 percentile")
            ev.record("注入后 plan.normalization", plan.normalization,
                      NORMALIZATION_EXACT, note="正本 §9 要求 1 (MEDIAN_CENTER)")
            ev.record("判据违背量 |normalization − 正本默认值|", delta, NORMALIZATION_EXACT,
                      note=f"注入行 {ref.DEFECT_LINE[ref.D8_PLAN_NORM_DEFAULT_NONE]}")
            legal = ref.kernel_method_x_normalization_legal(plan.method, plan.normalization)
            harness.is_false(legal, "注入 D8 后路由结果仍被内核判为合法 ⇒ admissibility 判据无牙齿")
            dec = ref.p2_reject_stack_ex(FIXTURE_N3, plan)
            harness.exact(dec.status, STATUS_INVALID_CONFIGURATION,
                          "同缺陷下内核必须 fail-closed（不得静默在错误工作域上判）")
            ev.record("同缺陷下内核 status", dec.status, note="正本 §8a：percentile 必须 MEDIAN_CENTER")


# ===========================================================================
# 附加（正本 §5「显式指定算法时 16≤n<20 linear_fit 由调用方发 WARN」）
# ===========================================================================

@harness.test(
    "rejection-S5-EXPLICIT-LINEAR-FIT-WARN",
    intent="正本 §5(:89) 逐字「显式指定算法时 `16≤n<20` linear_fit 由调用方发 WARN」在适用性"
           "面的落地：16..19 必发告警、n ≥ 20 不发。这是 §5 档位表里唯一一条**显式请求**分支的"
           "冻结口径（正本 §9a(:259)「显式指定算法时 16≤n<20 linear_fit 由调用方发 WARN」）。",
    inputs="p2_rejection_applicability(method=LINEAR_FIT, nominal_n ∈ {8,15,16,17,18,19,20,25})",
    expected="n ∈ [16,19] 告警码非空；n = 20 与 n = 25 告警码为空串",
    source="docs/science/REJECTION.md §5(:89) 逐字「显式指定算法时 16≤n<20 linear_fit 由调用方发 WARN」；"
           "合法性窗口表见 docs/detail/registry/acsd.phase2.reject.md:110-119 逐字"
           "「显式指定的合法性窗口（同 WBPP 2.5.9 rejectionIsGood()，只告警、不硬阻断）…… "
           "linear fit 建议 ≥ 20」；被测面 lib/algorithms/coverage/src/rejection.cpp:2690-2695",
    criteria=["S1", "S2"],
)
def s5_explicit_linear_fit_warn():
    with harness.evidence() as ev:
        for n in (8, 15, 16, 17, 18, 19, 20, 25):
            code = ref.p2_rejection_applicability(METHOD_LINEAR_FIT, n)
            ev.record(f"linear_fit n={n} 告警码", code or "（无）")
            if 16 <= n < 20:
                harness.is_true(code != "", f"n={n}: 正本 §5 要求 linear_fit 发 WARN，实测无告警")
            if n >= 20:
                harness.exact(code, "", f"n={n}: 合法性窗口（建议 ≥ 20 帧）应无告警")