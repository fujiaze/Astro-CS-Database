"""科学判据的「就地即正本」四条 + 测光双边界门（S6）。

本文件吸收审核包-R2 §2.1 里**删掉即永久消失**的那几条：

| 判据 | 清单里的逐字定位 | 本文件的落法 |
|---|---|---|
| **S6** | 「P1 测光双边界门」 | 判定序性质测试 + 两个**已被撤回的恒真门**做负例 |
| **S14** | 「优化前后 science 输出 hash/数值等价」 | 逐成员等价判定器 + 正负夹具对 |
| **S15** | `PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED` | 禁用宣称扫描器 + 注入宣称的反例 |
| **S16** | 「真实帧只报 `observed_rejection_rate`，不得称 false reject」 | 字段命名判据扫描器 + 违规样本反例 |

⚠️ S14 的**完整形态**（优化前后跑同一产品两遍比 hash）需要构建与两次运行，
本阶段无构建 ⇒ 本文件只测**判定器本身**并如实登记执行面未落位。
"""

from __future__ import annotations

import hashlib
import math
import os
import re
import tempfile

from . import harness

_UNIT_DIR = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.abspath(os.path.join(_UNIT_DIR, "..", "..", ".."))

# --- S6 冻结常数（来源：实验/photometric-magnitude/README.md §2.2-2.3 逐字）----
#: Rousseeuw & Croux 1993 的 MAD→σ 因子（README §2.2 逐字 0.6744897501960817）
MAD_TO_SIGMA = 0.6744897501960817
#: `sigma_obs = 2.5·MAD/0.6744897501960817`（README §2.2）
SIGMA_OBS_SCALE = 2.5
#: `1.166 = √1.361`，MAD 尺度估计量的高斯渐近标准化方差（Rousseeuw & Croux 1993 Table 2）
SIGMA_EST_REL_SD = 1.166
#: 3σ 抽样涨落包络的倍数
ENVELOPE_K = 3.0


def mad_sigma_bound_factor(n: int) -> float:
    """`rho_lo(n) = 1 − 3·1.166/√n`（README §2.3 逐字）。

    `n ≤ (3·1.166)²` 时该值 ≤ 0 ⇒ 3σ **下**包络在数学上不存在。
    """
    return 1.0 - ENVELOPE_K * SIGMA_EST_REL_SD / math.sqrt(n)


def mad(x):
    """中位绝对偏差。"""
    xs = sorted(x)
    m = xs[len(xs) // 2] if len(xs) % 2 else 0.5 * (xs[len(xs) // 2 - 1]
                                                    + xs[len(xs) // 2])
    return sorted(abs(v - m) for v in xs)[len(xs) // 2] if len(xs) % 2 else \
        0.5 * (sorted(abs(v - m) for v in xs)[len(xs) // 2 - 1]
               + sorted(abs(v - m) for v in xs)[len(xs) // 2])


def sigma_obs(residuals) -> float:
    """`sigma_obs = 2.5·MAD(r_inliers)/0.6744897501960817`。"""
    return SIGMA_OBS_SCALE * mad(residuals) / MAD_TO_SIGMA


def sigma_ceiling(budget_terms, n: int) -> float:
    """`sigma_ceiling = (1 + 3·1.166/√n)·sqrt(Σ 预算项²)`。"""
    return (1.0 + ENVELOPE_K * SIGMA_EST_REL_SD / math.sqrt(n)) * \
        math.sqrt(sum(t * t for t in budget_terms))


def sigma_floor(sigma_fit, n: int, *, clamp: bool = False) -> float:
    """`sigma_floor = (1 − 3·1.166/√n)·σ_fit`。

    `clamp=True` 是**已被撤回**的写法（README §2.3「订正 P1-M07」逐字：
    「历史建议『下界取 `max(rho_lo, 0)·σ_fit`』**已撤回**——clamp 后下界恒为 0 而
    `σ_obs ≥ 0` 恒真 ⇒ 下界恒不触发，比原缺陷更隐蔽，属**恒真门（无证据资格）**」）。
    保留该分支**只作为负例**。
    """
    lo = mad_sigma_bound_factor(n)
    if clamp:
        lo = max(lo, 0.0)
    return lo * sigma_fit


ABOVE_CEILING = "ABOVE_CEILING"
BELOW_FLOOR = "BELOW_FLOOR"
LOWER_BOUND_UNDEFINED = "LOWER_BOUND_UNDEFINED"
PASS = "PASS"


def adjudicate(obs: float, floor: float, ceil: float, scope: str) -> str:
    """判定序（README §2.3 逐字）：

        sigma_obs > sigma_ceiling → ABOVE_CEILING;
        scope == upper_only        → LOWER_BOUND_UNDEFINED;
        sigma_obs < sigma_floor    → BELOW_FLOOR;
        否则                         → PASS

    ⇒ **upper_only 永不记 PASS**（README 逐字「（**不记 PASS**）」）。
    """
    if obs > ceil:
        return ABOVE_CEILING
    if scope == "upper_only":
        return LOWER_BOUND_UNDEFINED
    if obs < floor:
        return BELOW_FLOOR
    return PASS


def gate_scope(n: int) -> str:
    """`"two_sided" if rho_lo > 0 else "upper_only"`（README §2.3 逐字）。"""
    return "two_sided" if mad_sigma_bound_factor(n) > 0 else "upper_only"


# ---------------------------------------------------------------------------
# S6 · 可证伪点与判定序
# ---------------------------------------------------------------------------

@harness.test(
    "s6.scope_degrades_exactly_at_rho_lo_zero",
    intent="S6：n ≤ (3·1.166)² 时 3σ 下包络在数学上不存在，作用域必须降级为 upper_only",
    inputs="n = 10, 12, 13, 20, 50 的作用域与 rho_lo",
    expected="n ≤ 12 → upper_only；n ≥ 13 → two_sided；分界 (3·1.166)² = 12.2364…",
    source="实验/photometric-magnitude/README.md §2.3「判据作用域（订正 P1-M07）」逐字"
           "「`rho_lo = 1 − 3·1.166/√n ≤ 0` ⟺ `n ≤ (3·1.166)² = 12.236` 时 3σ **下**包络"
           "在数学上不存在」与 §2.3 判定序 `gate_scope = \"two_sided\" if rho_lo > 0 else "
           "\"upper_only\"`",
    criteria=("S6",),
)
def test_s6_scope_degrades_exactly_at_rho_lo_zero():
    pivot = (ENVELOPE_K * SIGMA_EST_REL_SD) ** 2
    with harness.evidence() as ev:
        ev.record("(3·1.166)²（分界 n）", pivot)
        ev.record("n=12 → rho_lo", mad_sigma_bound_factor(12), note="应 < 0")
        ev.record("n=13 → rho_lo", mad_sigma_bound_factor(13), note="应 > 0")
    harness.is_false(mad_sigma_bound_factor(12) > 0, "n=12 的 rho_lo 必须 ≤ 0")
    harness.is_true(mad_sigma_bound_factor(13) > 0, "n=13 的 rho_lo 必须 > 0")
    # 纯比值比较，未声明 scale 域 ⇒ 不触发 §4.3 的 1 ulp 绝对容差下限
    harness.close(pivot, 12.2364, rtol=1e-4, atol=0.0, what="分界 n 的解析值")
    for n, expect in ((10, "upper_only"), (12, "upper_only"), (13, "two_sided"),
                      (20, "two_sided"), (50, "two_sided")):
        with harness.evidence() as ev:
            ev.record(f"n={n} → gate_scope", gate_scope(n), expect)
        harness.exact(gate_scope(n), expect, f"n={n} 的作用域")


@harness.test(
    "s6.upper_only_scope_never_records_pass",
    intent="S6：作用域降级后下限不可判，**不得记 PASS**（否则是无证据资格的恒真通过）",
    inputs="n = 10（upper_only）、sigma_obs 取 0 与取 ceil 上下两侧",
    expected="三种 obs 一律不返回 PASS；只返回 ABOVE_CEILING 或 LOWER_BOUND_UNDEFINED",
    source="实验/photometric-magnitude/README.md §2.3 逐字「作用域降级 `upper_only`、"
           "状态词 `LOWER_BOUND_UNDEFINED`（**不记 PASS**）」",
    criteria=("S6",),
)
def test_s6_upper_only_scope_never_records_pass():
    n = 10
    ceil = sigma_ceiling([0.003], n)
    floor_undefined = sigma_floor(0.01, n)
    for obs in (0.0, ceil - 1e-12, ceil, ceil + 1e-9):
        verdict = adjudicate(obs, floor_undefined, ceil, gate_scope(n))
        with harness.evidence() as ev:
            ev.record(f"n={n} upper_only, sigma_obs={obs!r} →", verdict)
        harness.is_false(verdict == PASS,
                         f"upper_only 作用域不得记 PASS（obs={obs!r}）")
    harness.exact(adjudicate(ceil + 1e-6, floor_undefined, ceil, gate_scope(n)),
                  ABOVE_CEILING, "超上界必须 ABOVE_CEILING（该判定先于作用域）")
    harness.exact(adjudicate(0.0, floor_undefined, ceil, gate_scope(n)),
                  LOWER_BOUND_UNDEFINED, "作用域降级时下限不可判")


@harness.test(
    "s6.retracted_floor_clamp_is_a_tautological_gate",
    intent="S6 负例：被撤回的 `max(rho_lo,0)` 写法把下界恒压成 0 ⇒ 下界永不触发（恒真门）",
    inputs="n = 10（rho_lo < 0）与 n = 50（rho_lo > 0），对比未撤回与 clamp 两种下界",
    expected="clamp 后 n=10 的 floor ≡ 0 且 `sigma_obs ≥ 0` 恒真 ⇒ 下界判据恒不触发；"
             "未撤回写法在 n=50 时下界为正、有判别力",
    source="实验/photometric-magnitude/README.md §2.3「订正 P1-M07」逐字「历史建议『下界取 "
           "`max(rho_lo, 0)·σ_fit`』**已撤回**——clamp 后下界恒为 0 而 `σ_obs ≥ 0` 恒真 ⇒ "
           "下界恒不触发，比原缺陷更隐蔽，属**恒真门（无证据资格）**」；"
           "docs/engineering/testing/TEST.md §2 逐字「恒真的比较没有证据资格」",
    criteria=("S6",),
    kind=harness.NEGATIVE,
    inject="DEF-FLOOR-CLAMP：sigma_floor 取 max(rho_lo,0)·σ_fit（已撤回的写法）",
)
def test_s6_retracted_floor_clamp_is_a_tautological_gate():
    sigma_fit = 0.01
    n = 10                     # rho_lo < 0：3σ 下包络在数学上不存在

    honest_scope = gate_scope(n)                       # "upper_only"
    injected_scope = "two_sided"                       # ← 缺陷：不降级
    obs = 0.0                                          # 真值无效应 ⇒ σ_obs = 0

    # 注入的缺陷管线 = 「clamp 下界」+「不降级作用域」。两者必须一起注入：
    # 只注入 clamp 时作用域仍会降级，判定词变成 LOWER_BOUND_UNDEFINED，
    # 恒真效应被作用域降级掩盖住 —— 这正是 README 逐字说的「比原缺陷更隐蔽」。
    floor_injected = sigma_floor(sigma_fit, n, clamp=True)
    ceil = sigma_ceiling([0.002, 0.001, 0.0007], n)
    v_honest = adjudicate(obs, sigma_floor(sigma_fit, n), ceil, honest_scope)
    v_injected = adjudicate(obs, floor_injected, ceil, injected_scope)
    with harness.evidence() as ev:
        ev.record("n=10 的 rho_lo", mad_sigma_bound_factor(n), note="< 0 ⇒ 下包络不存在")
        ev.record("未撤回作用域 / 注入作用域", f"{honest_scope} / {injected_scope}")
        ev.record("未撤回下界 / clamp 下界",
                  f"{sigma_floor(sigma_fit, n)} / {floor_injected}")
        ev.record("真值无效应（σ_obs=0）的判定（未撤回 / 注入）",
                  f"{v_honest} / {v_injected}")
    harness.exact(v_honest, LOWER_BOUND_UNDEFINED,
                  "诚实口径：下界不存在，必须报 LOWER_BOUND_UNDEFINED")
    harness.exact(floor_injected, 0.0, "clamp 后下界必须恒为 0")
    harness.exact(v_injected, PASS,
                  "缺陷生效的标志：clamp + 不降级 ⇒ 真值无效应被判 PASS（恒真门）")

    # 恒真性：clamp 下界对任意 σ_obs ≥ 0 都不触发（与注入值无关）
    triggers = [adjudicate(x, floor_injected, 1e9, injected_scope) == BELOW_FLOOR
                for x in (0.0, 1e-9, 1e-3, 1.0)]
    with harness.evidence() as ev:
        ev.record("clamp 下界在四个不同 σ_obs 上触发 BELOW_FLOOR 吗", triggers)
    harness.is_false(any(triggers), "clamp 下界不该有任何触发点")

    # 反证：n=50（rho_lo > 0）时 clamp 不起作用 ⇒ 差别来自 rho_lo < 0 这个域
    harness.exact(sigma_floor(sigma_fit, 50, clamp=True), sigma_floor(sigma_fit, 50),
                  "n=50 时 clamp 不得改变下界（rho_lo>0）")


@harness.test(
    "s6.self_referential_sigma_flat_makes_the_ceiling_undecidable",
    intent="S6 负例：σ_flat 若取被测残差自身（`delta_after_m`），上界随注入同步膨胀 ⇒ 恒 PASS",
    inputs="同一批通量，分别用「自指口径」与「独立口径」算 sigma_ceiling",
    expected="自指口径下注入逐星散度使上界同步膨胀、判定恒 PASS（无判别力）；"
             "独立口径下同一注入判红",
    source="实验/photometric-magnitude/README.md §2.3「σ_flat 必须独立于被测样本"
           "（订正 P1-B01）」逐字「历史实现取 `calibrate()['delta_after_m']`——那是**同一批星在"
           "多项式拟合之后**的残差散度，即 `sigma_obs` 自身的函数 ⇒ 上界随被测统计量同步膨胀、"
           "判据失去判别力（真实帧占上界方差 **47.6%**）」，并逐字「自指口径下注入逐星散度使 "
           "`sigma_ceiling` 同步膨胀，判定**恒 PASS（无判别力）**；独立口径下同一注入在 "
           "0.05 mag 处**判红**」",
    criteria=("S6",),
    kind=harness.NEGATIVE,
    inject="DEF-SIGMA-FLAT-SELFREF：预算项 σ_flat 取自被测样本自身的拟合残差散度",
)
def test_s6_self_referential_sigma_flat_makes_the_ceiling_undecidable():
    n = 50
    sigma_fit = 0.01
    other_budget = [0.002, 0.0015, 0.001]      # 与 σ_flat 无关的其余预算项
    sigma_flat_independent = 0.0007             # README §2.3 ② 的仓内约定常数
    injected_scatter = 0.05                    # 注入的逐星乘性残差散度（README §2.3 复现点）

    def run(sigma_flat):
        ceil = sigma_ceiling(other_budget + [sigma_flat], n)
        floor = sigma_floor(sigma_fit, n)
        return ceil, floor, adjudicate(injected_scatter, floor, ceil, gate_scope(n))

    ceil_ind, floor_ind, v_ind = run(sigma_flat_independent)
    ceil_self, floor_self, v_self = run(injected_scatter)   # ← 缺陷：σ_flat 取被测统计量自身

    with harness.evidence() as ev:
        ev.record("独立口径 σ_flat / sigma_ceiling", f"{sigma_flat_independent} / {ceil_ind}")
        ev.record("自指口径 σ_flat / sigma_ceiling", f"{injected_scatter} / {ceil_self}")
        ev.record("上界膨胀倍数", ceil_self / ceil_ind)
        ev.record("注入 0.05 mag 时的判定（独立 / 自指）", f"{v_ind} / {v_self}")
    harness.is_true(ceil_self > ceil_ind, "缺陷未生效：自指口径的上界没有膨胀")
    harness.is_false(v_ind == PASS,
                     "独立口径必须抓得住 0.05 mag 的注入（README §2.3 逐字「独立口径下同一注入在 "
                     "0.05 mag 处**判红**」）")
    harness.is_true(v_ind in (BELOW_FLOOR, ABOVE_CEILING),
                    f"独立口径必须给出区分性判定，实测 {v_ind}")
    harness.exact(v_self, PASS,
                  "缺陷生效的标志：自指口径对同一注入判 PASS（恒真、无判别力）")
    harness.is_false(v_self == v_ind, "缺陷未生效：两种口径同值")
    # 恒真性：自指口径下把注入散度放大 100 倍，上界同步放大 ⇒ 仍恒 PASS
    _, _, v_100 = run(injected_scatter * 100)
    with harness.evidence() as ev:
        ev.record("注入放大 100 倍后的自指口径判定", v_100)
    harness.exact(v_100, PASS, "自指口径必须对任意注入都 PASS（恒真门）")


@harness.test(
    "s6.truth_no_effect_must_not_pass",
    intent="S6 可证伪点 ①：真值无效应时 sigma_obs = 0 且**判红**（不得 PASS）",
    inputs="n = 50、恒等注入（残差恒为 0）、独立口径预算",
    expected="sigma_obs 逐位为 0；判定为 BELOW_FLOOR，不得 PASS",
    source="实验/photometric-magnitude/README.md §1「可证伪点」第 1 条逐字「真值无效应"
           "（`F_instr = inject_scale·F_syn` 精确、`m≡1`、无噪声）时，`sigma_obs` 必须为 0 "
           "且判据判红」；同 §1 第 2 条「注入**乘性空间残差**时 `sigma_obs` 必须单调上升并在"
           "足够大时判红」",
    criteria=("S6",),
)
def test_s6_truth_no_effect_must_not_pass():
    n = 50
    obs = sigma_obs([0.0] * 41)
    ceil = sigma_ceiling([0.002, 0.0015, 0.001, 0.0007], n)
    floor = sigma_floor(0.01, n)
    verdict = adjudicate(obs, floor, ceil, gate_scope(n))
    with harness.evidence() as ev:
        ev.record("sigma_obs（41 个零残差）", obs)
        ev.record("sigma_floor / sigma_ceiling", f"{floor} / {ceil}")
        ev.record("判定", verdict)
    harness.exact(obs, 0.0, "真值无效应时 sigma_obs 必须逐位为 0")
    harness.is_false(verdict == PASS, "可证伪点 ① 被否证：真值无效应时判据判了 PASS")
    harness.exact(verdict, BELOW_FLOOR, "真值无效应必须判 BELOW_FLOOR")


# ---------------------------------------------------------------------------
# S14 · 优化前后 science 输出等价（判定器 + 夹具对）
# ---------------------------------------------------------------------------

def _digest_tree(root: str):
    """逐成员内容散列（相对路径 → sha256）。空目录返回空 dict。"""
    out = {}
    for dirpath, _dirs, files in os.walk(root):
        for fn in sorted(files):
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, root).replace(os.sep, "/")
            with open(full, "rb") as fh:
                out[rel] = hashlib.sha256(fh.read()).hexdigest()
    return out


def output_diff(a: dict, b: dict):
    """返回 (缺失, 新增, 内容不同) 三张清单；空 ⇒ 逐成员等价。"""
    ka, kb = set(a), set(b)
    return sorted(ka - kb), sorted(kb - ka), sorted(
        k for k in (ka & kb) if a[k] != b[k])


@harness.test(
    "s14.output_equivalence_holds_for_identical_trees",
    intent="S14 正例：同一 science 输出的两次落盘，逐成员内容散列必须逐项相等",
    inputs="临时目录里两份逐字节相同的产物树（各 3 个成员）",
    expected="output_diff 三张清单全空 ⇒ 等价",
    source="审核包-R2/T02 判据清单 §2.1 S14 行逐字「**优化前后 science 输出 hash/数值等价**」"
           "（就地即正本，删除即永久消失）；来源文档 BASELINE 的回归基线语义；"
           "docs/engineering/testing/TEST.md §8 逐字「引用未变更模块的既有产物时，必须以构建指纹"
           "（内容散列）证明同源」",
    criteria=("S14",),
)
def test_s14_output_equivalence_holds_for_identical_trees():
    with tempfile.TemporaryDirectory() as tmp:
        for side in ("before", "after"):
            d = os.path.join(tmp, side)
            os.makedirs(os.path.join(d, "sub"))
            for rel, body in (("signal.fits", b"SIG"), ("support.fits", b"SUP"),
                              ("sub/manifest.json", b"{}")):
                with open(os.path.join(d, rel), "wb") as fh:
                    fh.write(body)
        a = _digest_tree(os.path.join(tmp, "before"))
        b = _digest_tree(os.path.join(tmp, "after"))
        missing, added, changed = output_diff(a, b)
        with harness.evidence() as ev:
            ev.record("成员数", len(a))
            ev.record("缺失 / 新增 / 内容不同",
                      f"{missing} / {added} / {changed}")
        harness.exact(len(a), 3, "夹具成员数")
        harness.is_false(missing or added or changed, "等价的两棵树给出非空差异")


@harness.test(
    "s14.silent_product_change_is_detected",
    intent="S14 负例：优化把某个 science 输出的**数值**改了，逐成员散列必须抓到并点名",
    inputs="after 树里 signal.fits 少 1 字节（模拟数值漂移）、support.fits 被删、"
           "多出一个新成员",
    expected="三类差异各自被点名；差异清单非空即判红（不因「差异很小」而放过）",
    source="审核包-R2/T02 §2.1 S14；docs/engineering/testing/TEST.md §8 逐字「受影响验证范围由"
           "改动集算出，**只扩大不缩小**」",
    criteria=("S14",),
    kind=harness.NEGATIVE,
    inject="DEF-OUTPUT-DRIFT：优化改写了 science 输出的一个成员、并删一个、增一个",
)
def test_s14_silent_product_change_is_detected():
    with tempfile.TemporaryDirectory() as tmp:
        d0, d1 = os.path.join(tmp, "before"), os.path.join(tmp, "after")
        for d in (d0, d1):
            os.makedirs(os.path.join(d, "sub"))
        with open(os.path.join(d0, "signal.fits"), "wb") as fh:
            fh.write(b"SIG")                       # 正确
        with open(os.path.join(d1, "signal.fits"), "wb") as fh:   # 注入：数值漂移
            fh.write(b"SIE")
        for d in (d0, d1):
            with open(os.path.join(d, "sub/manifest.json"), "wb") as fh:
                fh.write(b"{}")
        with open(os.path.join(d0, "support.fits"), "wb") as fh:   # 注入：成员丢失
            fh.write(b"SUP")
        with open(os.path.join(d1, "extra.json"), "wb") as fh:    # 注入：多出新成员
            fh.write(b"{}")

        missing, added, changed = output_diff(_digest_tree(d0), _digest_tree(d1))
        with harness.evidence() as ev:
            ev.record("缺失成员", missing)
            ev.record("新增成员", added)
            ev.record("内容不同成员", changed)
        harness.exact(missing, ["support.fits"], "成员丢失必须被点名")
        harness.exact(added, ["extra.json"], "新增成员必须被点名")
        harness.exact(changed, ["signal.fits"], "内容漂移必须被点名")

        # 反证：三者都不是空跑出来的
        harness.is_false(not (missing or added or changed),
                         "缺陷未生效：等价判定器没有抓到任何差异")


# ---------------------------------------------------------------------------
# S15 · 禁止宣称与 PixInsight/WBPP bit-exact
# ---------------------------------------------------------------------------

#: 违禁宣称形态（S15 正本：`PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED`，
#: WBPP profile 只提供 routing 政策映射）。
#:
#: ⚠️ **口径收窄的教训**：初版把「逐位一致 / 完全相同 / 位相同」一律当违禁，
#: 结果把测试集内部的自洽断言（「两个 CD 的 |det| 必须逐位相同」、「c_jp 必须逐位相同」）
#: 也判成红 —— 那是 S15 **没有**禁止的东西。判据禁的是**对第三方产品**宣称 bit-exact，
#: 不是禁一切逐位相等表述。故判据必须**同时**出现逐位等价词与具名第三方产品。
_THIRD_PARTY_PRODUCT = re.compile(
    r"(pixinsight|wbpp|drizzlepac|\bccdstack\b|astrodrizzle|"
    r"imageintegration|pixinsight\u7684|imageintegration)", re.IGNORECASE)
_EXACT_WORD = re.compile(
    r"(bit[-\s]?exact|\u9010\u4f4d\u4e00\u81f4|\u9010\u4f4d\u76f8\u540c|"
    r"identical\s+to|\bexact\s+match)", re.IGNORECASE)
#: 「否定 / 废止 / 不得」等语境不构成宣称。
_NEGATION = ("NOT_CLAIMED", "\u4e0d\u5f97", "\u4e0d\u5ba3\u79f0", "\u5df2\u5e9f\u6b62",
             "\u5df2\u64a4\u56de", "\u975e", "\u4e0d\u662f", "\u7981", "not claimed",
             "never claims", "does not claim")


def claim_scanner_violations(text: str, context: str = ""):
    """返回违禁宣称行；空列表 = 合规。

    命中条件 = 同一行同时出现「逐位/bit 等价词」与「具名第三方产品」。
    只出现其中之一不算违禁（内部自洽断言、单纯的第三方提及都合法）。
    """
    out = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw
        # 先剥掉 markdown 强调标记再判否定语境：`**不**宣称` 规范化成 `不宣称`，
        # 否则会被当成正面宣称（这正是本扫描器第一轮实测到的假阳性）。
        normalized = line.replace("*", "").replace("`", "")
        if not (_EXACT_WORD.search(normalized) and _THIRD_PARTY_PRODUCT.search(normalized)):
            continue
        if any(k in normalized for k in _NEGATION):
            continue
        out.append((lineno, context, line.strip()[:120]))
    return out


@harness.test(
    "s15.no_exact_compatibility_claim_in_the_test_suite",
    intent="S15：测试集内不得出现与 PixInsight/WBPP bit-exact 的宣称（profile 只给 routing 政策映射）",
    inputs="eng/tests/unit/ 下全部 *.py 与 *.md 的逐行扫描",
    expected="违禁宣称 0 处；引用「NOT_CLAIDED / 不得 / 已废止」的历史叙事不计",
    source="审核包-R2/T02 判据清单 §2.1 S15 行逐字「`PIXINSIGHT_EXACT_COMPATIBILITY = "
           "NOT_CLAIMED`（WBPP profile 仅提供 routing 政策映射）」，并标为「⚠ **就地即正本**……"
           "非可执行判据 ⇒ 迁为**文档约束**（禁止任何材料宣称 bit-exact）」",
    criteria=("S15",),
)
def test_s15_no_exact_compatibility_claim_in_the_test_suite():
    # 扫描器自身文件把违禁模式作为**正则字面量数据**写在源码里，它不是宣称；
    # 把它算进去就是「用检测器自己的词表把自己判红」的无意义循环。
    self_file = os.path.basename(__file__)
    hits = []
    scanned = 0
    for fn in sorted(os.listdir(_UNIT_DIR)):
        if not fn.endswith((".py", ".md")) or fn == self_file:
            continue
        scanned += 1
        with open(os.path.join(_UNIT_DIR, fn), encoding="utf-8") as fh:
            hits.extend(claim_scanner_violations(fh.read(), context=fn))
    with harness.evidence() as ev:
        ev.record("扫描文件数", scanned)
        ev.record("违禁宣称处数", len(hits))
        ev.record("命中明细", hits[:5] or "无")
    harness.is_false(bool(hits), f"测试集内出现 bit-exact 宣称：{hits}")


@harness.test(
    "s15.exact_compatibility_claim_is_detected",
    intent="S15 负例：注入一条「与 PixInsight 逐位一致」的宣称，扫描器必须抓到",
    inputs="两段文本：合规的「不宣称 bit-exact」vs 违规的「与 PixInsight 逐位一致」",
    expected="合规段 0 命中、违规段 ≥1 命中",
    source="审核包-R2/T02 §2.1 S15 行；docs/engineering/testing/TEST.md §2 逐字"
           "「恒真的比较没有证据资格」（扫描器本身必须对拍出两臂判定互不相同）",
    criteria=("S15",),
    kind=harness.NEGATIVE,
    inject="DEF-EXACT-CLAIM：在材料里宣称与 PixInsight/WBPP bit-exact",
)
def test_s15_exact_compatibility_claim_is_detected():
    compliant = ("PIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED；"
                 "本测试集不得宣称与 PixInsight 逐位一致。")
    violating = "结论：本实现与 PixInsight 逐位一致，可作为逐位回归基线。"
    mixed = ("# 合规声明\nPIXINSIGHT_EXACT_COMPATIBILITY = NOT_CLAIMED\n"
             "# 但下面这句是宣称\n本实现与 WBPP bit-exact。\n")
    with harness.evidence() as ev:
        ev.record("合规段命中数", len(claim_scanner_violations(compliant)))
        ev.record("违规段命中数", len(claim_scanner_violations(violating)))
        ev.record("混合段命中行", [h[0] for h in claim_scanner_violations(mixed)])
    harness.exact(claim_scanner_violations(compliant), [], "合规段不得命中")
    harness.is_true(len(claim_scanner_violations(violating)) >= 1,
                    "缺陷未生效：违规宣称未被扫描器抓到")
    harness.is_true(len(claim_scanner_violations(mixed)) >= 1,
                    "缺陷未生效：混合段里的违规行未被抓到")


# ---------------------------------------------------------------------------
# S16 · 两率必须是不同字段名
# ---------------------------------------------------------------------------

OBSERVED_FIELD = "observed_rejection_rate"
FALSE_FIELD = "false_reject_rate"
#: 判据的机械形式：同一「率」的**名字**只能选一个，且真实帧侧只准 observed。
_RATE_NAME = re.compile(r"\b([A-Za-z_]*rejection_rate|[A-Za-z_]*reject_rate)\b")


def naming_violations(source_text: str):
    """找出「把真实帧排异率命名为 false reject」的字段名用法。

    机械口径（须与人读口径一致，故保守）：只在**同时**出现真值字段 `false_reject_rate`
    且其上下文含真实帧线索（`real` / `真实` / `observed`）时判红。
    受控注入场（`satellite` / `inject` / `clean sample`）不在此判据内——
    S16 逐字限定的就是**真实帧**。
    """
    out = []
    for lineno, line in enumerate(source_text.splitlines(), 1):
        if FALSE_FIELD not in line:
            continue
        low = line.lower()
        if any(k in low for k in ("real", "真实", "observed")):
            out.append((lineno, line.strip()[:120]))
    return out


@harness.test(
    "s16.real_frame_rate_must_not_be_named_false_reject",
    intent="S16：真实帧只报 `observed_rejection_rate`，不得称 false reject（两率必须不同字段名）",
    inputs="受控注入侧的 `clean_false_reject_rate`（合规）vs 真实帧侧的 `false_reject_rate`（违规）",
    expected="受控注入侧不判红；真实帧侧判红并点名行号",
    source="审核包-R2/T02 判据清单 §2.1 S16 行逐字「真实 16 帧只报 `observed_rejection_rate`，"
           "**不得称 false reject**」，处置逐字「⚠ **就地即正本**……迁为**命名约束**：两率必须是"
           "不同字段名」；正本出处 `docs/science/REJECTION.md` 的卫星线/真实帧两率口径",
    criteria=("S16",),
)
def test_s16_real_frame_rate_must_not_be_named_false_reject():
    controlled = 'out["clean_false_reject_rate"] = clean_rej / max(1, n_clean)'
    real_frame = ('r["false_reject_rate"] = rejected / total  '
                  '# 真实 16 帧')
    real_frame_en = 'payload["false_reject_rate"]  # observed on real frames'
    with harness.evidence() as ev:
        ev.record("受控注入侧命中数", len(naming_violations(controlled)))
        ev.record("真实帧侧（中文线索）命中数", len(naming_violations(real_frame)))
        ev.record("真实帧侧（英文线索）命中数", len(naming_violations(real_frame_en)))
    harness.exact(naming_violations(controlled), [],
                  "受控注入侧的 false reject 命名不在 S16 判据内")
    harness.is_true(len(naming_violations(real_frame)) >= 1,
                    "缺陷未生效：真实帧侧的 false reject 字段名未被抓到")
    harness.is_true(len(naming_violations(real_frame_en)) >= 1,
                    "缺陷未生效：英文线索的真实帧违规未被抓到")


@harness.test(
    "s16.two_rates_must_be_distinct_field_names",
    intent="S16 负向：把两率压进同一个字段名（判据力归零）必须被检出",
    inputs="一段文本同时定义 observed 与 false 两率，却只用一个共享字段名承载",
    expected="字段名去重后只剩一个 ⇒ 判红（两率不可区分）",
    source="审核包-R2/T02 §2.1 S16 处置列逐字「迁为**命名约束**：**两率必须是不同字段名**」",
    criteria=("S16",),
    kind=harness.NEGATIVE,
    inject="DEF-RATE-COLLAPSE：observed 与 false 两率共用同一个字段名",
)
def test_s16_two_rates_must_be_distinct_field_names():
    payload = ('{"rejection_rate": 0.031}   '
               '# 真值同时是 observed 与 false —— 字段名塌成一个')
    names = _RATE_NAME.findall(payload)
    with harness.evidence() as ev:
        ev.record("抽取到的率字段名", names)
        ev.record("去重后字段名数", len(set(names)))
    harness.exact(len(set(names)), 1, "夹具必须只含一个字段名（缺陷形态）")
    harness.is_false(OBSERVED_FIELD in payload,
                     "缺陷未生效：文本仍带 observed 字段名")
    harness.is_false(FALSE_FIELD in payload,
                     "缺陷未生效：文本仍带 false 字段名")
    harness.is_true(len(set(names)) < 2,
                    "缺陷未生效：两率字段名未被压成一个")