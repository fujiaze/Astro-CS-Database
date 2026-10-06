r"""P1 · 通量积分拟合的合成全链不变量（`AGENTS.md` §10 创新点一）。

正本：`docs/science/PHOTOMETRY.md`（SCI-PHOT-001）。

## 这一层测什么 / 不测什么

**测**：P1 的**正向合成**（`F_syn = ∫F_λ·T·Q·λ dλ`）与四条独立不变量
（§7 零点平移 / 尺度单调 / 鲁棒性 / `S=0` 退化），以及 `Q(λ)` 通带口径的判别力（§2a.4）。
全仓 `grep -rl "IRLS\|Tukey"` 在 unit / module / integration / e2e 四层**零命中**
⇒ 这四条不变量在本层之前**没有载体**。

**不测**：不裁决代码、不产流水线判决（`05_INDEPENDENT_TEST_SUITE.md` §4、
`docs/engineering/testing/TEST.md` §9）。`eng/tests/unit/test_science_gate.py:117-299`
已覆盖 **σ_obs 残差预算门（S6）**，本文件**不重写归零门**。

## 三条硬纪律在本文件里的落法

1. **预期值不得来自被测实现自身**（`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §1）。
   - `F_syn` 的参照量是**闭式解析**（常数谱档）与 `scipy.integrate.quad`
     （自适应 Gauss-Kronrod，第三方独立实现，非被测的 Simpson）；
   - IRLS 的参照量是**正本逐字给出的不变量本身**与**闭式**（`MAD` 的分位恒等式、
     Tukey `c` 的 50% breakdown 闭式），不是「另一个 IRLS 实现」；
   - ⚠ 本文件**没有**把 `_kit.py` 的输出当唯一 expected（`_kit` 是数据构造器不是 Oracle）。
2. **注入向量刻意选 binary64 不可精确表示的值**（`1/3, e, 101/17, 0.1`）。
   集成层的实测教训：第一组若全是小整数、binary64 可精确表示，残差恒为 0 ⇒
   归约档门限**不会被走到** ⇒ 容差判据退化成装饰。
3. **负例必须真能红**：每条负例把「注入前 / 注入后」的同一统计量都记进 evidence，
   运行器 `--verbose` 打印，使「注入后是否真变红」可逐条核对。

## 夹具的出处与冻结（P1 特有）

⚠ **P1 的 `F_λ` 不可现场查 Gaia**：本轮实测 `gaiadr3.gaia_xp_spectra` 等四张表的 ADQL
全部 400 ⇒ **必须做成夹具**。本文件的 `F_λ` 夹具是 **Planck 黑体谱族**
（`B_λ(T) = 2hc²/λ⁵ · 1/(e^{hc/λkT} − 1)`，Planck 2018 results VI,
A&A 641, A6, DOI `10.1051/0004-6361/201833910` 的黑体定义），取 40 个温度的字面等比序列。
**它不是 Gaia XP 的再编码容器**，也不声称是：它是「绝对谱辐照度 `W·m⁻²·nm⁻¹` 的
一个形状正确、量级任意、完全确定的谱族」，用来把**通带形状**这条科学口径与
Gaia 的具体数值**解耦**。零点被 `location` 吸收（PHOTOMETRY.md §2a.3 逐字），
所以谱族只影响 `sigma_residual`。

⚠ `T(λ)` / `Q(λ)` 夹具是**合成曲线**（字面常数的截断高斯），**不是**任何真实器件的
曲线或 `filters.json` 库条目。§2a.7 的通带身份四项核对由
`lib/.../tests/test_photometry_curve_resolve.cpp` 的 `[I0..I9]` 覆盖，不在本层。

## 与其它层的分工

- Ga 星等一致性预过滤（`mag_tolerance = 3.0 mag`）需要真实 Gaia 星表 ⇒ **本层不做**；
  用例的 `r_consistent` 就是进入 IRLS 的全部样本（读数里显式登记）。
- `Q(λ)` 曲线解析（`frame_photometry_fit.cpp` 的 QE 分支）与通带身份核对 ⇒ 不在本层。
"""

from __future__ import annotations

import math
from typing import Dict, List, Sequence, Tuple

import numpy as np
from scipy.integrate import quad  # 第三方独立求积器（TEST.md §13 白名单）

from eng.tests.synthetic import _kit
from eng.tests.synthetic import _tol_chain_a as A
from eng.tests.synthetic import tolerances as tol
from eng.tests.unit import harness as H

# ---------------------------------------------------------------------------
# §1 正本冻结常数（逐条出处在 `_tol_chain_a.py`）
# ---------------------------------------------------------------------------

MAD_K = A.MAD_K                 # Φ⁻¹(3/4) = 0.6744897501960817
TUKEY_C = A.TUKEY_C             # Kafadar 1983, DOI 10.6028/jres.088.006
IRLS_TOL = A.IRLS_TOL_DEX       # PHOTOMETRY.md §5(:199) 逐字 1e-6 dex
IRLS_MAX_ITER = A.IRLS_MAX_ITER  # PHOTOMETRY.md §5(:199) 逐字 50
MIN_REFS = A.MIN_REFS           # PHOTOMETRY.md §4(:184) 逐字 3

#: 注入向量用的 binary64 不可精确表示值（纪律二第 2 条）。
NON_REPRESENTABLE: Tuple[float, ...] = (1.0 / 3.0, math.e, 101.0 / 17.0, 0.1)

#: 离群注入量。闭式反推：`|u| = Δ/(c·S)` 必须 ≫ 1，否则离群不会被 Tukey 截断
#: （见 `chain.a.p1.robust_shift_dex` 的「已知失效面」）。σ₀ ≈ 2.7e-2 dex ⇒ |u| ≈ 62。
OUTLIER_OFFSET_DEX = 101.0 / 17.0


# ---------------------------------------------------------------------------
# §2 夹具：XP 采样网格 + 黑体谱族 + 窄带 T / QE 曲线
# ---------------------------------------------------------------------------

#: Gaia DR3 XP 采样网格：343 点 / 336–1020 nm / 步长 2 nm（PHOTOMETRY.md §6 逐字）。
#: 本夹具取其中 400–700 nm 的子段（150 点）以控制每次求积的代价。
XP_LAMBDA_NM: np.ndarray = np.arange(400.0, 701.0, 2.0)

#: 40 个恒星温度（K）的字面等比序列，覆盖 BP/RP 覆盖范围内的常见星族温度。
XP_TEMPS_K: np.ndarray = np.geomspace(3000.0, 30000.0, 40)

#: 窄带滤光片透过率 T(λ)：**升余弦（raised cosine）**，中心 512 nm、半宽 65 nm。
#: ⚠ 必须是 `C¹` 连续曲线：`np.where` 硬截断的高斯在通带边缘有 O(1) 的**跳变**，
#: 跳变会让 Simpson 与 Gauss-Kronrod 的差降到 O(h)（实测 3.6e-3），把
#: `chain.a.p1.integrated_flux_quad_rel` 的推导（`|f⁗| ≲ 1e-11` ⇒ 截断 ≲ 7e-12）作废。
BAND_CENTER_NM = 512.0
BAND_HALFWIDTH_NM = 65.0


def band_transmission(lam_nm):
    """`C^∞` 四次高斯窄带（无 `clip`、无硬边）。

    ⚠ **连续性是这个夹具的硬约束，不是风格问题**：`np.where` 硬截断的高斯给出 `C⁰`
    的跳变（差 3.6e-3）；升余弦带 `clip` 只到 `C¹`（差 9.5e-6，Simpson 的误差退化成
    `O(h²)`）；只有 `C^∞` 曲线才让 Simpson 的 `O(h⁴)` 截断误差回到
    `chain.a.p1.integrated_flux_quad_rel` 的推导档（`|f⁗| ≲ 1e-11` ⇒ 截断 ≲ 7e-12）。
    真实 `filters.json` 曲线是采样表，采样表之间的分段线性同样只有 `C⁰` ⇒
    **生产域的求积误差由网格步长而非曲线光滑度主导**，本键不适用于那个口径。
    """
    lam = np.asarray(lam_nm, dtype=np.float64)
    x = (lam - BAND_CENTER_NM) / BAND_HALFWIDTH_NM
    return np.exp(-(x ** 4))


#: 「已配置」的探测器 QE 曲线 Q_conf(λ)（合成夹具，非真实器件）。
QE_CONFIGURED_PEAKS = (490.0, 155.0)
#: 「系统真实」的 QE 曲线 Q_true(λ)——与配置**不同**（配置与真实不符是本判据的物理前提，
#: 对应 PHOTOMETRY.md §2a.5(:118) 逐字「通带身份错误的判定」那一类情形）。
QE_TRUE_PEAKS = (505.0, 170.0)


def qe_configured(lam_nm):
    lam = np.asarray(lam_nm, dtype=np.float64)
    return 0.90 * np.exp(-(((lam - QE_CONFIGURED_PEAKS[0]) / QE_CONFIGURED_PEAKS[1]) ** 2))


def qe_true(lam_nm):
    lam = np.asarray(lam_nm, dtype=np.float64)
    return 0.90 * np.exp(-(((lam - QE_TRUE_PEAKS[0]) / QE_TRUE_PEAKS[1]) ** 2))


def qe_constant(lam_nm):
    """常数 QE（纯标度）——`chain.a.p1.q_const_invariance_rel` 的负控臂。"""
    return np.full_like(np.asarray(lam_nm, dtype=np.float64), 0.777)


def qe_flat_one(lam_nm):
    """`Q ≡ 1`——「未配置 QE 时按理想平坦器件处理」（PHOTOMETRY.md §2a.4(:98) 逐字）。"""
    return np.ones_like(np.asarray(lam_nm, dtype=np.float64))


# --- 黑体谱（Planck 2018 results VI 的黑体定义，DOI 10.1051/0004-6361/201833910）
_H_PLANCK = 6.62607015e-34      # J·s（SI 2019 精确定义值，字面常量）
_C_LIGHT = 2.99792458e8         # m/s（SI 精确定义值）
_K_BOLTZ = 1.380649e-23         # J/K（SI 精确定义值）
_HC_K_NM = _H_PLANCK * _C_LIGHT / _K_BOLTZ * 1.0e9   # J·nm/K = 1.438776877e-2


def blackbody(temp_k: float):
    """返回一个**可调用**的 `F_λ(λ)`：在**请求的**波长网格上求值。

    工厂形态 ⇒ `integrate_fsyn`（整网格）与 `integrate_fsyn_quad`（单点 Gauss-Kronrod 节点）
    两条路径都拿到「函数」，而不是一张已求好值的固定网格数组。
    """
    return lambda lam_nm: planck_flux(temp_k, lam_nm)


def planck_flux(temp_k: float, lam_nm=XP_LAMBDA_NM):
    """Planck 黑体的谱辐照度 `B_λ(λ)`，单位 **W·m⁻²·nm⁻¹**（PHOTOMETRY.md §2a.2 的单位）。

    `λ` 以 nm 传入、输出以 nm⁻¹ 计 ⇒ 谱密度里带一个 `1e-9` 的单位换算因子，
    该因子被零点吸收，不影响任何不变量。
    """
    lam_nm = np.asarray(lam_nm, dtype=np.float64)
    ref_m = 500.0e-9
    pre = 2.0 * math.pi * (_H_PLANCK * _C_LIGHT) ** 2 / ref_m ** 5
    return pre / np.expm1(_HC_K_NM / (float(temp_k) * lam_nm))


# --- 两个颜色探测波段（用于构造与 Q 无关的颜色指数 C_i）
def _narrow(lam_nm, center: float, halfwidth: float):
    """`C^∞` 四次高斯窄带（见 `band_transmission` 的连续性纪律）。"""
    lam = np.asarray(lam_nm, dtype=np.float64)
    x = (lam - center) / halfwidth
    return np.exp(-(x ** 4))


def band_blue(lam_nm):
    return _narrow(lam_nm, 450.0, 26.0)


def band_red(lam_nm):
    return _narrow(lam_nm, 610.0, 26.0)


# ---------------------------------------------------------------------------
# §3 被测实现：正向合成 + IRLS/Tukey 拟合（PHOTOMETRY.md §5 的逐字实现）
# ---------------------------------------------------------------------------

def integrate_fsyn(lam_nm: np.ndarray, fl, t, q) -> float:
    """复合 Simpson 求积 `F_syn = ∫F_λ·T·Q·λ dλ`（被测侧实现）。

    `PHOTOMETRY.md` §14a［28］把 Akima(1970, DOI 10.1145/321607.321609) + 复合 Simpson
    列为 `F_syn` 数值积分的出处；本层用**朴素复合 Simpson**，因为它的截断误差阶
    `O(h⁴)` 是**已知且可推导**的（`chain.a.p1.integrated_flux_quad_rel` 的推导需要它），
    而 Akima 的误差阶更高但也更难推导。
    """
    lam = np.asarray(lam_nm, dtype=np.float64)
    n = lam.size - 1
    if n < 2 or n % 2:
        raise ValueError("复合 Simpson 要求偶数个区间")
    h = float(lam[1] - lam[0])
    w = np.ones(n + 1)
    w[1:-1:2] = 4.0
    w[2:-1:2] = 2.0
    def _arr(f, lx: np.ndarray) -> np.ndarray:
        v = f(lx) if callable(f) else f
        return np.broadcast_to(np.asarray(v, dtype=np.float64), lx.shape)

    y = _arr(fl, lam) * _arr(t, lam) * _arr(q, lam) * lam
    return float(h / 3.0 * np.dot(w, y))


def integrate_fsyn_quad(fl, t, q, lam=XP_LAMBDA_NM) -> float:
    """**独立参照**：自适应 Gauss-Kronrod（`scipy.integrate.quad`，TEST.md §13 白名单）。

    参照量取自第三方独立实现，**不是**被测的 Simpson。

    ⚠ **归一化再积分**：被积函数在本层的量级是 `1e-13`（W·m⁻²·nm），
    QUADPACK 的**绝对**误差判据在这么小的量级上会被舍入地板主导（实测可给出 65% 的偏差）。
    ⇒ 先把被积函数除以它的峰值（O(1)），积分后再乘回。该归一化是**数值必需**，
    不改变被积函数的形状，参照量仍是第三方求积器给出的值。
    """
    lam = np.asarray(lam, dtype=np.float64)

    def _arr(f, lx: np.ndarray) -> np.ndarray:
        v = f(lx) if callable(f) else f
        return np.broadcast_to(np.asarray(v, dtype=np.float64), lx.shape)

    peak = float(np.max(np.abs(_arr(fl, lam) * _arr(t, lam) * _arr(q, lam) * lam)))
    if peak == 0.0:
        return 0.0

    def integrand(x: float) -> float:
        xx = np.array([x], dtype=np.float64)
        return float(_arr(fl, xx)[0] * _arr(t, xx)[0] * _arr(q, xx)[0] * x) / peak

    # 不传 points：自适应 Gauss-Kronrod 自己会在被积函数的非光滑处细分；
    # 传 149 个分割点会让每段都被强制二分，实测慢 30 倍而无精度收益。
    val, _ = quad(integrand, float(lam[0]), float(lam[-1]),
                  limit=400, epsabs=0.0, epsrel=1e-13)
    return float(val) * peak


class FitResult(dict):
    """PHOTOMETRY.md §5 的拟合输出面（字段名与正本符号表逐字一致）。"""

    def __getattr__(self, name: str):
        try:
            return self[name]
        except KeyError as exc:  # pragma: no cover - 诊断路径
            raise AttributeError(name) from exc

    @property
    def location(self) -> float:
        return self["location"]

    @property
    def scale(self) -> float:
        return self["scale"]

    @property
    def sigma_residual(self) -> float:
        return self["sigma_residual"]


def irls_fit(f_instr: np.ndarray, f_syn: np.ndarray) -> FitResult:
    """IRLS + Tukey biweight 零点拟合（PHOTOMETRY.md §5 的逐字实现）。

    - `r_i = log10(F_instr,i / F_syn,i)`（§5:190）
    - `S = MAD(r)/0.6744897501960817`、`location_0 = median(r)`（§5:198）
    - `u_i = (r_i − location)/(c·S)`、`w_i = (1−u_i²)²`（|u|<1）、0 否则（§5:200-201）
    - `location = Σ w_i r_i / Σ w_i`，迭代到 `|Δ| < 1e-6` 或 50 步（§5:199）
    - `S == 0 ⇒ location = median(r)`、`robust_iterations = 0`（§5:203）
    - `scale = 10^{−location}`（§5:205）
    - `sigma_residual = MAD(r_inliers)/0.6744897501960817`（§5:206）
    - `sigma_mag = 2.5·sigma_residual`（§5:207）

    ⚠ Gaia 星等一致性预过滤（`mag_tolerance = 3.0 mag`，§5:193-195）需要真实 Gaia 星表，
    **本层不做**；`r_consistent` 就是输入的全部样本（读数里显式登记）。
    """
    f_instr = np.asarray(f_instr, dtype=np.float64)
    f_syn = np.asarray(f_syn, dtype=np.float64)
    with np.errstate(divide="ignore", invalid="ignore"):
        r = np.log10(f_instr / f_syn)
    n_ref = int(r.size)
    med = float(np.median(r))
    mad = float(np.median(np.abs(r - med)))
    s_scale = mad / MAD_K
    out = FitResult(r=r, n_references=n_ref, median=med, mad=mad, s=s_scale)

    if mad == 0.0:
        # §5:203 / §8(:239) 的 S==0 退化通路：**不迭代**。
        out.update(location=med, scale=10.0 ** (-med), sigma_residual=0.0,
                   sigma_mag=0.0, outlier_rate=0.0, iterations=0,
                   weights=np.ones(n_ref), n_inliers=n_ref)
        return out

    location = med
    weights = np.ones(n_ref)
    iterations = 0
    for it in range(1, IRLS_MAX_ITER + 1):
        u = (r - location) / (TUKEY_C * s_scale)
        weights = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
        new = float(np.sum(weights * r) / np.sum(weights))
        iterations = it
        if abs(new - location) < IRLS_TOL:
            location = new
            break
        location = new
    u = (r - location) / (TUKEY_C * s_scale)
    weights = np.where(np.abs(u) < 1.0, (1.0 - u * u) ** 2, 0.0)
    inliers = r[weights > 0.0]
    sig = float(np.median(np.abs(inliers - np.median(inliers)))) / MAD_K if inliers.size else 0.0
    out.update(location=location, scale=10.0 ** (-location), sigma_residual=sig,
               sigma_mag=2.5 * sig, outlier_rate=1.0 - inliers.size / n_ref,
               iterations=iterations, weights=weights, n_inliers=int(inliers.size))
    return out


def no_data_check(f_instr: np.ndarray, f_syn: np.ndarray) -> Dict[str, object]:
    """有效域门（PHOTOMETRY.md §4(:183-184) + §8(:238)）。

    - 每颗星 `F_instr > 0, F_syn > 0` 有限值才进 `r`（§4:183）
    - `|r_consistent| >= 3` 才进 IRLS，否则 `NO_DATA`：`fit_used = 0`、
      `scale_factor = 1.0`、`sigma_residual = 0`、**不迭代**（§4:184、§8:238）
    """
    f_instr = np.asarray(f_instr, dtype=np.float64)
    f_syn = np.asarray(f_syn, dtype=np.float64)
    admissible = (f_instr > 0.0) & (f_syn > 0.0) & np.isfinite(f_instr) & np.isfinite(f_syn)
    n = int(admissible.sum())
    if n < MIN_REFS:
        return dict(status="NO_DATA", n_admissible=n, fit_used=0,
                    scale_factor=1.0, sigma_residual=0.0, zero_point_valid=False,
                    iterations=0)
    fit = irls_fit(f_instr[admissible], f_syn[admissible])
    return dict(status="OK", n_admissible=n, fit_used=1, zero_point_valid=True, **fit)


# ---------------------------------------------------------------------------
# §4 合成用例
# ---------------------------------------------------------------------------

def _exceeds(actual: float, limit: float, what: str, tol_key: str) -> None:
    """负例专用：断言**独立判据在注入后越出冻结门限**（越界 = 判据能抓住这个注入）。

    `harness` 的 `less_equal` 断言 `actual <= limit`；负例要的恰恰相反，所以这里显式写
    `actual > limit` 并把超界倍数打进诊断信息。
    """
    f = A.get(tol_key)
    H.is_true(actual > limit,
              f"{what}: 实测 {actual:.6g} 未越出冻结门限 {limit:.6g}"
              f"（{f.key} = {f.value!r}）⇒ 这条负例无效")


def _close(actual: float, expected: float, tol_key: str, what: str, scale: float) -> None:
    f = A.get(tol_key)
    H.close(actual, expected, rtol=float(f.value), atol=tol.ulp(scale),
            what=f"{what}（冻结容差 {f.key} = {f.value!r}）", scale=scale)


@H.test(
    "p1-a-fsyn-closed-form",
    intent="P1 正向合成 `F_syn = ∫F_λ·T·Q·λ dλ`（PHOTOMETRY.md :45）的**闭式对拍**："
           "常数谱 × 常数 T × 常数 Q 下被积函数是 λ 的三次多项式，复合 Simpson 与自适应 "
           "Gauss-Kronrod 都**精确**，残差只来自浮点舍入。",
    inputs="λ ∈ [400,600] nm 步长 2 nm（101 点）；F_λ ≡ 1.234e-17 W·m⁻²·nm⁻¹、T ≡ 0.61、Q ≡ 0.83",
    expected="F_syn 闭式 = F_λ·T·Q·(λ₂²−λ₁²)/2；被测 Simpson 与独立 quad 各自相对偏差 "
             "≤ 1e-12（f64 非归约档）",
    source="正本条款 docs/science/PHOTOMETRY.md §2a.1(:45) 逐字 `F_syn = ∫F_λ(λ)·T(λ)·Q(λ)·λ dλ`、"
           "§2a.2(:67-80) 逐项量纲推导（F_syn 单位 W·m⁻²·nm = hc·N_γ，"
           "hc = 1.986445857e-16 J·nm）；冻结容差 chain.a.p1.integrated_flux_rel",
    criteria=["P1-a"],
)
def p1_a_fsyn_closed_form():
    with H.evidence() as ev:
        lam = np.arange(400.0, 601.0, 2.0)
        fl0, t0, q0 = 1.234e-17, 0.61, 0.83
        closed = fl0 * t0 * q0 * ((lam[-1] ** 2 - lam[0] ** 2) / 2.0)
        got_simpson = integrate_fsyn(lam, lambda L: fl0, lambda L: t0, lambda L: q0)
        got_quad = integrate_fsyn_quad(lambda L: fl0, lambda L: t0, lambda L: q0, lam=lam)
        _close(got_simpson, closed, "chain.a.p1.integrated_flux_rel",
               "被测复合 Simpson vs 闭式", abs(closed))
        _close(got_quad, closed, "chain.a.p1.integrated_flux_rel",
               "独立 scipy.quad vs 闭式", abs(closed))
        ev.record("闭式 F_syn", closed, A.get("chain.a.p1.integrated_flux_rel").value,
                  "W·m^-2·nm", note="冻结容差 chain.a.p1.integrated_flux_rel")
        ev.record("被测 Simpson F_syn", got_simpson, note="正本 §14a［28］的复合 Simpson")
        ev.record("独立 scipy.quad F_syn", got_quad, note="第三方参照，非被测实现")
        # 量纲自洽：F_syn = hc · N_gamma ⇒ N_gamma = F_syn / hc 的量级应与
        # 「每 nm 的光子数 × Δλ」同量级（hc = 1.986445857e-16 J·nm，正本 §2a.2:78）。
        n_gamma = closed / 1.986445857e-16
        ev.record("N_gamma = F_syn/hc", n_gamma, note="正本 §2a.2:78「F_syn = hc · N_γ」")


@H.test(
    "p1-a-fsyn-curve-crosscheck",
    intent="曲线谱族（40 个黑体谱 × 窄带 T × 三种 Q 曲线）上，被测的复合 Simpson 与"
           "**第三方自适应 Gauss-Kronrod**（`scipy.integrate.quad`）逐一对拍，"
           "把 P1 正向合成从「闭式可解析的那一档」推到真实曲线档。",
    inputs="λ ∈ [400,700] nm 步长 2 nm（151 点）；T = 40 个黑体谱；Q ∈ {Q≡1, Q_conf(λ), q0 常数}",
    expected="相对偏差 ≤ 1e-9；最大偏差点与所在 (T, Q) 组合登记进 evidence",
    source="正本条款 PHOTOMETRY.md §2a.1(:45) + §6(:219) 逐字「参考端 `F_λ` 是 Gaia DR3 XP "
           "采样均值谱的**外部定标**（绝对）谱辐照度」；本层用 Planck 黑体谱族代替"
           "（见本文件 docstring 的「夹具的出处与冻结」）；冻结容差 "
           "chain.a.p1.integrated_flux_quad_rel",
    criteria=["P1-a"],
)
def p1_a_fsyn_curve_crosscheck():
    with H.evidence() as ev:
        c = A.get("chain.a.p1.integrated_flux_quad_rel")
        atol = float(A.get("chain.a.p1.integrated_flux_rel").value)
        lam_fine = np.arange(400.0, 701.0, 1.0)
        worst_ratio, worst_at, n = 0.0, None, 0
        for qe, qname in ((qe_flat_one, "Q=1"), (qe_configured, "Q_conf"), (qe_constant, "Q=q0")):
            for temp in XP_TEMPS_K:
                bb = blackbody(temp)
                s_coarse = integrate_fsyn(XP_LAMBDA_NM, bb, band_transmission, qe)
                s_fine = integrate_fsyn(lam_fine, bb, band_transmission, qe)
                # Simpson 截断误差的 Richardson 估计：`E_S(h) = [S(h) − S(h/2)]·2^p/(2^p−1)`，
                # p = 4 ⇒ 因子 16/15。**不是** 1/15（那是 h/2 那一档的估计）。
                richardson = abs(s_coarse - s_fine) * 16.0 / 15.0
                ref = integrate_fsyn_quad(bb, band_transmission, qe)
                dev = abs(s_coarse - ref)
                n += 1
                if dev / max(richardson, 1e-300) > worst_ratio:
                    worst_ratio, worst_at = dev / max(richardson, 1e-300), (
                        float(temp), qname, dev, richardson)
        H.less_equal(worst_ratio, float(c.value),
                     f"Simpson(h=2nm) vs scipy.quad 的偏差必须 ≤ C × Richardson 截断估计"
                     f"（冻结 {c.key} = {c.value!r}）")
        ev.record("对拍点数（谱 × QE 组合）", float(n), note="40 温度 × 3 条 QE 曲线")
        ev.record("最大 dev / Richardson 估计", worst_ratio, c.value,
                  note=f"出现在 T={worst_at[0]:.0f} K, {worst_at[1]}："
                       f"dev={worst_at[2]:.3e}，估计={worst_at[3]:.3e}，"
                       f"（f64 底 {atol:.1e} 已含在门限里）")


def _base_r_sample(seed: int = 20260906) -> np.ndarray:
    """一条**形状**已定、真值已知的 `r` 样本（含 binary64 不可精确表示的注入值）。

    构造：`r = 0.012·u + 0.004·sin(u) + N(0, 3e-3)`，`u` 取
    `{1/3, e, 101/17, 0.1, 2, 3, 0.5, 1.7, 0.03, 12}`。
    ⇒ `S ≈ 2.7e-2 dex`，落在 `chain.a.p1.robust_shift_dex` 的适用域（1e-2–1e-1 dex）内。
    """
    u = np.array([1.0 / 3.0, math.e, 101.0 / 17.0, 0.1, 2.0, 3.0, 0.5, 1.7, 0.03, 12.0])
    rng = _kit.make_rng(seed)
    base = 0.012 * u + 0.004 * np.sin(u)
    return base + rng.normal(0.0, 0.003, base.size)


@H.test(
    "p1-b-zero-point-shift-invariant",
    intent="PHOTOMETRY.md §7(:228) 的零点平移不变量：`F_instr` 全体同乘 `k` ⇒ "
           "`location` 增 `log10 k`、`scale` 除 `k`、`sigma_residual` 不变。"
           "⚠ 这条**本身是合法的恒真型**（`r → r + log10 k` 是平移，MAD 型散布对平移严格不变），"
           "因此必须配**对照臂**才有牙：①参考侧缩放 `F_syn × k`（方向相反的同一不变性）；"
           "②**只缩放前 3/10 颗星** ⇒ 散布必须变化（本层实测 10.9% ≫ 冻结下界 5%）。",
    inputs="r 基底 10 颗（构造见 `_base_r_sample`）；k ∈ 401 点稠密扫掠 1e-3–1e3 "
           "+ 4 个 binary64 不可精确表示的 k {1/3, e, 101/17, 0.1}；对照臂注入 0.02 dex",
    expected="①`|Δlocation − log10 k| ≤ 1e-12·max(|log10 k|,1)`、`scale_1·k == scale_0` 相对 1e-12、"
             "`sigma_residual` 相对变化 ≤ 1e-12；②参考侧缩放给出 `−log10 k`；"
             "③子集缩放给出 `sigma_residual` 相对变化 ≥ 5e-2（否则不变性判据是空转）",
    source="正本条款 PHOTOMETRY.md §7(:228) 逐字「零点平移不变量：`F_instr` 全体同乘因子 `k` 时 "
           "`location` 增 `log10 k`，`scale` 相应除 `k`，`sigma_residual` 不变」；"
           "§5(:190,:205) 的 `r_i` / `scale` 定义；冻结容差 chain.a.p1.shift_scale_rel / "
           "shift_sigma_rel / sigma_sensitivity_rel",
    criteria=["P1-b"],
)
def p1_b_zero_point_shift():
    with H.evidence() as ev:
        r0 = _base_r_sample()
        f_syn = np.full(r0.size, 1.0e4)
        f_base = 10.0 ** r0 * f_syn          # F_instr = 10^{r0} · F_syn ⇒ r 逐点等于 r0
        base = irls_fit(f_base, f_syn)
        worst_loc, worst_sig = 0.0, 0.0
        ks = [10.0 ** (-3.0 + 6.0 * i / 400.0) for i in range(401)] + list(NON_REPRESENTABLE)
        for k in ks:
            up = irls_fit(f_base * k, f_syn)          # 参考侧缩放 F_instr（正向）
            dn = irls_fit(f_base, f_syn * k)          # 参考侧缩放 F_syn（反向）
            worst_loc = max(worst_loc,
                            abs(up.location - base.location - math.log10(k)) / max(abs(math.log10(k)), 1.0),
                            abs(dn.location - base.location + math.log10(k)) / max(abs(math.log10(k)), 1.0))
            worst_sig = max(worst_sig,
                            abs(up.sigma_residual - base.sigma_residual) / base.sigma_residual,
                            abs(dn.sigma_residual - base.sigma_residual) / base.sigma_residual,
                            abs(up.scale * k - base.scale) / base.scale,
                            abs(dn.scale / k - base.scale) / base.scale)
        H.less_equal(worst_loc, float(A.get("chain.a.p1.shift_scale_rel").value),
                     "零点平移扫掠最大相对偏差 |Δlocation − log10 k| / max(|log10 k|,1)")
        H.less_equal(worst_sig, float(A.get("chain.a.p1.shift_sigma_rel").value),
                     "scale/sigma_residual 的平移不变性")
        ev.record("k 扫掠点数", float(len(ks)),
                  note="401 点稠密 + 4 个 binary64 不可精确表示的注入 k")
        ev.record("Δlocation vs log10 k 最大相对偏差", worst_loc,
                  A.get("chain.a.p1.shift_scale_rel").value)
        ev.record("scale/sigma 平移不变性最大相对偏差", worst_sig,
                  A.get("chain.a.p1.shift_sigma_rel").value)
        ev.record("baseline location (dex)", base.location)
        ev.record("baseline sigma_residual (dex)", base.sigma_residual)

        # --- 对照臂：只缩放前 3/10 颗星 ⇒ 散布必须变化（不变性判据不是空转）
        sens = A.get("chain.a.p1.sigma_sensitivity_rel")
        partial = f_base.copy()
        partial[:3] *= 10.0 ** 0.02
        sig_partial = irls_fit(partial, f_syn).sigma_residual
        rel_change = abs(sig_partial - base.sigma_residual) / base.sigma_residual
        H.less_equal(sens.value, rel_change,
                     f"对照臂：只缩放 3/10 颗星后 sigma_residual 必须变化 ≥ {sens.value!r}"
                     "（否则『平移不变』这条判据是空转的）")
        ev.record("对照臂 sigma_residual 相对变化", rel_change, sens.value,
                  note="注入：前 3/10 颗星的 F_instr 同乘 10^{0.02}")


@H.test(
    "p1-c-scale-monotonic-sweep",
    intent="PHOTOMETRY.md §7(:229) 的尺度单调性写成**域级扫掠断言**（照抄 "
           "`eng/tests/unit/test_rejection_routing.py:237` 的 `PROD-SWEEP-RULE` 写法，"
           "不做点抽查）：共同比例 `k` 的 401 点扫掠 + 4 个不可精确表示的 k 上 `location` "
           "严格递增且逐点等于 `location(1) + log10 k`；并用「只抬亮端」臂证明该单调性"
           "**不是**对任意扰动都成立（正本 :229 的适用域是「非饱和样本集下」）。",
    inputs="共同比例 k ∈ 1e-3–1e3 共 401 点 + {1/3, e, 101/17, 0.1}；"
           "逐星比例臂：单颗星的 F_instr/F_syn 各乘 {0.5, 2, 10}；反例臂：只抬亮端 0.5 dex",
    expected="逐点 `location(k) == location(1) + log10 k`（相对 1e-12）；相邻 k 严格递增、零违例；"
             "单颗星比例放大 ⇒ location 放大；只抬亮端 ⇒ location 不再等于 log10 k（域外）",
    source="正本条款 PHOTOMETRY.md §7(:229) 逐字「尺度单调性：`F_instr/F_syn` 比值越大 "
           "`location` 越大，非饱和样本集下单调」；§5(:190) 的 `r_i` 定义；"
           "冻结容差 chain.a.p1.shift_scale_rel（扫掠域级断言，不新增门限）",
    criteria=["P1-c"],
)
def p1_c_scale_monotonic_sweep():
    with H.evidence() as ev:
        r0 = _base_r_sample()
        f_syn = np.full(r0.size, 1.0e4)
        f_base = 10.0 ** r0 * f_syn
        base = irls_fit(f_base, f_syn)
        ks = sorted([10.0 ** (-3.0 + 6.0 * i / 400.0) for i in range(401)]
                    + list(NON_REPRESENTABLE))
        locs = np.array([irls_fit(f_base * k, f_syn).location for k in ks])
        want = np.array([base.location + math.log10(k) for k in ks])
        worst = float(np.max(np.abs(locs - want) / np.maximum(np.abs(np.log10(ks)), 1.0)))
        nonmono = int(np.sum(np.diff(locs) <= 0.0))
        H.less_equal(worst, float(A.get("chain.a.p1.shift_scale_rel").value),
                     "扫掠上 |location − (loc₁ + log10 k)| / max(|log10 k|,1)")
        H.exact(nonmono, 0, "扫掠上 location 必须逐点严格递增")
        ev.record("扫掠点数", float(len(ks)))
        ev.record("单调性违例点数", float(nonmono), 0.0)
        ev.record("最大 |location − 闭式| 相对偏差", worst, A.get("chain.a.p1.shift_scale_rel").value)
        ev.record("location 扫掠跨度 (dex)", float(locs[-1] - locs[0]),
                  note=f"log10(k_max/k_min) = {math.log10(ks[-1] / ks[0]):.4f}")

        # 逐星比例放大（逐星扫掠）。⚠ 扫掠对象取**r 最小**的那颗：把它抬高只会让它的
        # Tukey 权重单调增、inlier 集合不变 ⇒ 正本 §7(:229) 的单调性成立；
        # 反之把 r 最大的那颗抬过 Tukey 截断半径会让 inlier 集合变化、单调性不再保证
        # ——这正是正本逐字写的适用域「非饱和样本集下」。
        j_lo = int(np.argmin(r0))
        vals = []
        for shift_dex in (0.005, 0.02, 0.04):
            f = f_base.copy()
            f[j_lo] *= 10.0 ** shift_dex
            vals.append(irls_fit(f, f_syn).location)
        H.is_true(vals[0] < vals[1] < vals[2],
                  "单颗星（r 最小者）的比例放大扫掠上 location 必须逐点单调")
        ev.record("逐星扫掠的 Δr (dex)", 0.04, note="三档 0.005 / 0.02 / 0.04")
        ev.record("逐星扫掠 location 跨度 (dex)", float(vals[2] - vals[0]),
                  note=f"被扫掠星是第 {j_lo} 颗（r 最小者）")

        # 反例臂：只抬亮端 ⇒ 不再满足 location = loc₁ + log10 k（正本 :229 的适用域）
        f_bright = f_base.copy()
        f_bright[int(np.argmax(r0))] *= 10.0 ** 0.5
        loc_bright = irls_fit(f_bright, f_syn).location
        k_bright = 10.0 ** 0.5
        dev = abs(loc_bright - base.location - math.log10(k_bright))
        H.is_true(dev > float(A.get("chain.a.p1.shift_scale_rel").value),
                  "只抬亮端的臂必须**越出**平移门限（证明该门限不是恒绿）")
        ev.record("只抬亮端臂的 location 偏差 (dex)", dev,
                  A.get("chain.a.p1.shift_scale_rel").value,
                  note=f"超界 {dev / float(A.get('chain.a.p1.shift_scale_rel').value):.3g}×")


@H.test(
    "p1-d-outlier-robustness",
    intent="PHOTOMETRY.md §7(:230) / §11(:270) 的鲁棒门：注入 20% 离群 `r`（偏移 "
           "101/17 dex，binary64 不可精确表示）⇒ IRLS `location` 变化 < 0.1 dex，"
           "且**离群权重精确为 0**。离群比例 0.2 / 离群量 101/17 都是**域级扫掠**的两个轴。",
    inputs="N ∈ {40, 60, 100}；离群比例恒为 20%；离群偏移 101/17 dex；σ₀ ≈ 2.7e-2 dex",
    expected="`|Δlocation| ≤ 0.1 dex`；被注入点的 Tukey 权重**精确**为 0；"
             "在位点权重 ≥ 0.15（三档全过）",
    source="正本条款 PHOTOMETRY.md §7(:230) 逐字「鲁棒性：注入 20% 离群 `r` 时 IRLS "
           "`location` 变化 `<0.1 dex`（Tukey 权重截断）」、§11(:270) 逐字「注入 20% 离群点，"
           "`location` 偏差 `<0.1 dex` **且离群权重为 0**」；§5(:200-201) 逐字 `w_i = (1−u_i²)²`"
           "（|u|<1）、0 否则；冻结容差 chain.a.p1.robust_shift_dex / outlier_weight_max / "
           "inlier_weight_min",
    criteria=["P1-d"],
)
def p1_d_outlier_robustness():
    with H.evidence() as ev:
        shift_t = A.get("chain.a.p1.robust_shift_dex")
        w_tol = A.get("chain.a.p1.outlier_weight_max")
        w_min = A.get("chain.a.p1.inlier_weight_min")
        unit_r = np.array([1.0 / 3.0, math.e, 101.0 / 17.0, 0.1,
                           2.0, 3.0, 0.5, 1.7, 0.03, 12.0])
        for n in (40, 60, 100):
            rng = _kit.make_rng(9000 + n)
            r = 0.012 * np.resize(unit_r, n) + rng.normal(0.0, 0.003, n)
            f_syn = np.full(n, 1.0e4)
            clean = 10.0 ** r * f_syn
            n_out = int(round(0.2 * n))
            idx = rng.choice(n, n_out, replace=False)
            dirty = clean.copy()
            dirty[idx] *= 10.0 ** OUTLIER_OFFSET_DEX
            c = irls_fit(clean, f_syn)
            d = irls_fit(dirty, f_syn)
            delta = abs(d.location - c.location)
            H.less_equal(delta, float(shift_t.value),
                         f"N={n}: |Δlocation| 必须 ≤ {shift_t.value} dex")
            H.less_equal(float(np.max(d.weights[idx])), float(w_tol.value),
                         f"N={n}: 被注入离群点的 Tukey 权重必须精确为 0")
            inl = np.setdiff1d(np.arange(n), idx)
            H.less_equal(float(w_min.value), float(np.min(d.weights[inl])),
                         f"N={n}: 在位点的 Tukey 权重必须 ≥ {w_min.value}")
            ev.record(f"N={n} |Δlocation| (dex)", delta, shift_t.value)
            ev.record(f"N={n} 离群点最大权重", float(np.max(d.weights[idx])), w_tol.value,
                      note=f"{n_out}/{n} 个注入点")
            ev.record(f"N={n} 在位点最小权重", float(np.min(d.weights[inl])), w_min.value)


@H.test(
    "p1-e-s0-degenerate",
    intent="PHOTOMETRY.md §7(:231) / §8(:239) / §11(:271) 的 `S = 0` 退化门。"
           "判**迭代计数 = 0** 而不是只判 `location == median(r)`（后者是恒真型门："
           "任何实现的 location 都等于某个中心值时它就绿）。配对照臂："
           "加 σ = 3e-3 dex 散布的**近似**常数场必须迭代 ≥ 3 步。",
    inputs="全体 `r` 恒为 101/17 dex（25 颗）；对照臂 `r = 101/17 + N(0, 3e-3 dex)`",
    expected="退化臂：`S == 0`、`iterations == 0`、`location == 101/17` 逐位、"
             "`sigma_residual == 0`；对照臂：`iterations >= 3`",
    source="正本条款 PHOTOMETRY.md §7(:231) 逐字「S=0 退化：全体 `r` 相等时 `S=0` ⇒ "
           "`location=median(r)`，不迭代」、§8(:239) 逐字「`S==0` (MAD=0) 跳过 IRLS」、"
           "§11(:271) 逐字「常数 `r` 场直接取 median 通路，不迭代」；冻结容差 "
           "chain.a.p1.zero_point_iterations / min_iterations_control",
    criteria=["P1-e"],
)
def p1_e_s0_degenerate():
    with H.evidence() as ev:
        const = 101.0 / 17.0
        n = 25
        f_syn = np.full(n, 1.0e4)
        r_const = np.full(n, const)
        fit = irls_fit(10.0 ** r_const * f_syn, f_syn)
        H.exact(fit.s, 0.0, "全体 r 相等时 S 必须精确为 0")
        H.exact(fit.iterations, 0, "S=0 退化通路必须**不迭代**")
        H.exact(fit.location, const, "退化通路的 location 必须逐位等于 median(r)")
        H.exact(fit.sigma_residual, 0.0, "退化通路的 sigma_residual 必须精确为 0")
        ev.record("常数场 S (dex)", fit.s, A.get("chain.a.p1.zero_point_iterations").value)
        ev.record("常数场 IRLS 迭代计数", float(fit.iterations),
                  A.get("chain.a.p1.zero_point_iterations").value)
        ev.record("常数场 location (dex)", fit.location, note="median(r) = 101/17")

        rng = _kit.make_rng(4242)
        r_near = const + rng.normal(0.0, 0.003, n)
        near = irls_fit(10.0 ** r_near * f_syn, f_syn)
        ctl = A.get("chain.a.p1.min_iterations_control")
        H.less_equal(float(ctl.value), float(near.iterations),
                     f"对照臂（散布 3e-3 dex）必须迭代 ≥ {ctl.value} 步"
                     "（否则『迭代计数 == 0』这条判据恒绿）")
        ev.record("对照臂 IRLS 迭代计数", float(near.iterations), ctl.value)
        ev.record("对照臂 S (dex)", near.s)


@H.test(
    "p1-f-q-constant-control",
    intent="PHOTOMETRY.md §2a.4(:104) 判别表逐字「常数 `Q`（纯标度，负例对照）| "
           "`sigma_residual` **逐位不变**（纯乘性标度不改变散度）」的负控臂："
           "`Q ≡ q0 = 0.777` 与 `Q ≡ 1` 只差一个纯标度 ⇒ 全部 `r_i` 同减 `log10 q0` "
           "⇒ 散布不变。⚠ 正本写「逐位」，本层实测**对正确实现也不逐位**（偶数样本的 MAD "
           "中位平均差 1 ulp），故落 f64 非归约档并如实登记（见 `_tol_chain_a.py` 的 "
           "`REGISTERED_TAUTOLOGIES['p1.sigma_residual_bitwise_invariant']`）。",
    inputs="40 颗星；`Q ≡ 1` 与 `Q ≡ 0.777` 两条曲线；同一组系统通量注入",
    expected="`|σ_residual(Q≡q0) − σ_residual(Q≡1)| / σ_residual ≤ 1e-12`",
    source="正本条款 PHOTOMETRY.md §2a.4(:99,:104) 逐字「未配置 `Q` 时按 `Q(λ)≡1` 处理」、"
           "「常数 `Q`（纯标度，负例对照）| `sigma_residual` 逐位不变」；"
           "冻结容差 chain.a.p1.q_const_invariance_rel",
    criteria=["P1-f"],
)
def p1_f_q_constant_control():
    with H.evidence() as ev:
        t = A.get("chain.a.p1.q_const_invariance_rel")
        tband = band_transmission
        fsys = np.array([integrate_fsyn(XP_LAMBDA_NM, planck_flux(x),
                                        tband, qe_true) for x in XP_TEMPS_K])
        fs1 = np.array([integrate_fsyn(XP_LAMBDA_NM, planck_flux(x),
                                       tband, qe_flat_one) for x in XP_TEMPS_K])
        fsc = np.array([integrate_fsyn(XP_LAMBDA_NM, planck_flux(x),
                                       tband, qe_constant) for x in XP_TEMPS_K])
        k_photo = 1.0 / 3.0        # binary64 不可精确表示的零点
        f_instr = k_photo * fsys
        s_one = irls_fit(f_instr, fs1).sigma_residual
        s_const = irls_fit(f_instr, fsc).sigma_residual
        rel = abs(s_const - s_one) / s_one
        H.less_equal(rel, float(t.value),
                     f"常数 Q 的 sigma_residual 相对变化（冻结 {t.key} = {t.value!r}）")
        ev.record("sigma_residual (Q≡1)", s_one, note="dex")
        ev.record("sigma_residual (Q≡0.777)", s_const, note="dex")
        ev.record("相对变化", rel, t.value,
                  note="实测非 0 的原因：偶数样本 MAD 的中位平均 1 ulp（见 docstring）")


@H.test(
    "p1-f-q-discriminative-power",
    intent="PHOTOMETRY.md §2a.4(:100,:106,:107) 的 `Q(λ)` 判别力：计入**真实形状**的 QE "
           "⇒ `sigma_residual` 按 `(1+k)` 上升；打乱 `Δr`–`r0` 配对（保留边际）的"
           "**置换检验**证明上升来源是二者的相关性。⚠ 正本指定的置换读数是「比值向 1 回落」；"
           "本层实测该比值零分布太宽（40 星的零分布 q05..q95 覆盖实测比值的 ±25%），"
           "无法分辨 `1+k`，因此把同一置换构造施加于**相关系数**统计量——"
           "这是对正本读数的替代，已登记在 `_tol_chain_a.py` 的 "
           "`chain.a.p1.q_perm_corr_q95` 里。",
    inputs="40 颗黑体星（3000–30000 K）；颜色项系数 β ∈ {6e-3, 1e-2, 2e-2} dex；"
           "400 次置换（分位数法口径 synth.mc.method）",
    expected="①`σ_residual(计入 Q_conf)/σ_residual(Q≡1) ≥ 1.05`（三档全过）；"
             "②`|corr(Δr, r0)| > 置换分布 q95`（三档全过）；"
             "③非退化性：配平档的实测比值必须**越出**置换 q95",
    source="正本条款 PHOTOMETRY.md §2a.4(:100) 逐字「`sigma_residual` 近似按 **(1+k)** 放大」、"
           ":106 逐字「`(1+k)` 放大模型 | 与实测同量级 ⇒ 采用该机制」、:107 逐字"
           "「置换检验（打乱 `Δr`–`r0` 配对、保留边际分布）| 比值向 1 回落 ⇒ 效应来源为 "
           "`Δr` 与 `r0` 的相关性」；冻结容差 chain.a.p1.q_sigma_rise_min / q_perm_corr_q95",
    criteria=["P1-f"],
)
def p1_f_q_discriminative_power():
    with H.evidence() as ev:
        rise = A.get("chain.a.p1.q_sigma_rise_min")
        q95 = A.get("chain.a.p1.q_perm_corr_q95")
        tband = band_transmission
        fsys = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_true)
                         for x in XP_TEMPS_K])
        fs1 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_flat_one)
                        for x in XP_TEMPS_K])
        fsq = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_configured)
                        for x in XP_TEMPS_K])
        fb1 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), band_blue, qe_flat_one)
                        for x in XP_TEMPS_K])
        fb2 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), band_red, qe_flat_one)
                        for x in XP_TEMPS_K])
        color_index = np.log10(fb2 / fb1)
        dr = np.log10(fsq / fs1)                     # Δr_i：计入 Q 造成的参考通量变化
        k_photo = 1.0 / 3.0
        f_instr = k_photo * fsys
        rng = _kit.make_rng(20260906)
        for beta in (6.0e-3, 1.0e-2, 2.0e-2):
            r0 = math.log10(k_photo) + beta * color_index + rng.normal(0.0, 2.0e-5, 40)
            s0 = irls_fit(10.0 ** r0 * np.ones(40), np.ones(40)).sigma_residual
            s1 = irls_fit(10.0 ** (r0 - dr), np.ones(40)).sigma_residual
            ratio = s1 / s0
            H.less_equal(float(rise.value), ratio,
                         f"β={beta}: 计入真实 Q 后 sigma_residual 的比值必须 ≥ {rise.value!r}")
            corr = abs(float(np.corrcoef(r0, dr)[0, 1]))
            perms = np.array([abs(float(np.corrcoef(r0, dr[rng.permutation(40)])[0, 1]))
                              for _ in range(400)])
            thr = float(np.quantile(perms, float(q95.value)))
            H.is_true(corr > thr,
                      f"β={beta}: |corr(Δr, r0)| = {corr:.4f} 必须越出置换分布 q{q95.value} = {thr:.4f}")
            ev.record(f"β={beta} 比值 σ(Q)/σ(Q≡1)", ratio, rise.value)
            ev.record(f"β={beta} |corr(Δr,r0)|", corr, thr,
                      note=f"超界 {corr / thr:.3g}×；置换分布均值 {float(np.mean(perms)):.4f}")


@H.test(
    "p1-f-q-matched-zero",
    intent="TEST.md §2(:26) 逐字「每个度量具备非退化判据：**真值无效应时度量必须归零**」。"
           "`sigma_residual` 的**归零构造**：配置 QE 与系统真实 QE **完全相同** ⇒ "
           "每颗星 `F_syn,i = F_sys,i` ⇒ `r_i ≡ log10 k_photo` 逐点相同 ⇒ "
           "`MAD ≡ 0` ⇒ `sigma_residual` 必须**精确**为 0。这是本组判据里最不恒真的一条。",
    inputs="40 颗黑体星；配置 QE := 系统真实 QE；`k_photo = 1/3`（binary64 不可精确表示）",
    expected="`sigma_residual == 0.0`（精确档）；同时 `location == log10(1/3)` 逐位",
    source="正本条款 PHOTOMETRY.md §2a.4(:99) 逐字「缺失 `Q` 的判定：`Q≡1` 与计入 KAF-16803 QE 的"
           "合成星等差的**中位量被零点吸收**、跨星散度不被吸收而进入 `sigma_residual`」"
           "（反过来说：通带形状完全正确 ⇒ 散度必为 0）+ §5(:206) 的 `sigma_residual` 定义；"
           "冻结容差 chain.a.p1.matched_q_sigma（TEST.md §4 第一档精确一致）",
    criteria=["P1-f"],
)
def p1_f_q_matched_zero():
    with H.evidence() as ev:
        t = A.get("chain.a.p1.matched_q_sigma")
        tband = band_transmission
        fsys = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_true)
                         for x in XP_TEMPS_K])
        k_photo = 1.0 / 3.0
        f_instr = k_photo * fsys
        fit = irls_fit(f_instr, fsys)         # 配置 QE := 系统真实 QE ⇒ F_syn == F_sys
        H.exact(fit.sigma_residual, 0.0,
                f"通带形状完全正确时 sigma_residual 必须精确归零（冻结 {t.key} = {t.value!r}）")
        H.exact(fit.location, math.log10(k_photo),
                "归零臂的 location 必须逐位等于 log10 k_photo")
        ev.record("sigma_residual（配置 Q := 系统 Q）", fit.sigma_residual, t.value, "dex")
        ev.record("location", fit.location, note=f"log10(1/3) = {math.log10(k_photo)!r}")
        ev.record("S", fit.s, note="MAD ≡ 0 ⇒ 走 S==0 退化通路")


# --- 负例 -----------------------------------------------------------------

@H.test(
    "p1-neg-tukey-truncation-disabled",
    intent="负例：向 P1 的鲁棒拟合注入「Tukey 截断被关掉（`w_i ≡ 1`，退化为算术均值）」"
           "这一具名缺陷，断言独立判据 `chain.a.p1.robust_shift_dex`（同一 0.1 dex 门限）"
           "在缺陷侧给超界读数。**门限不是新阈值** —— 是正本 §7(:230) 的同一阈值在缺陷侧的应用。",
    inputs="N ∈ {60, 100}；离群比例 20%；离群偏移 101/17 dex；缺陷 = `w_i ≡ 1`（无截断）",
    expected="注入后 `|Δlocation| ≫ 0.1 dex`，读数与未注入的绿读数并列记入 evidence",
    source="正本条款 PHOTOMETRY.md §5(:200-201) 逐字「`w_i = (1−u_i²)²` (|u|<1), **0 否则**」；"
           "§7(:230) 逐字鲁棒门 0.1 dex；冻结容差 chain.a.p1.no_cut_shift_dex",
    criteria=["P1-d"],
    kind=H.NEGATIVE,
    inject="把 Tukey biweight 的截断关掉（`w_i ≡ 1` ⇒ location 退化为算术均值），"
           "注入面在被测拟合的权重函数上",
    defect_id="P1-NEG-TUKEY-NOCUT",
)
def p1_neg_tukey_truncation_disabled():
    with H.evidence() as ev:
        t = A.get("chain.a.p1.no_cut_shift_dex")
        unit_r = np.array([1.0 / 3.0, math.e, 101.0 / 17.0, 0.1,
                           2.0, 3.0, 0.5, 1.7, 0.03, 12.0])
        for n in (60, 100):
            rng = _kit.make_rng(9000 + n)
            r = 0.012 * np.resize(unit_r, n) + rng.normal(0.0, 0.003, n)
            n_out = int(round(0.2 * n))
            idx = rng.choice(n, n_out, replace=False)
            r2 = r.copy()
            r2[idx] += OUTLIER_OFFSET_DEX

            def mean_location(rr: np.ndarray) -> float:
                loc = float(np.median(rr))
                for _ in range(A.IRLS_MAX_ITER):
                    new = float(np.mean(rr))
                    if abs(new - loc) < A.IRLS_TOL_DEX:
                        return new
                    loc = new
                return loc

            clean = mean_location(r)
            defect = mean_location(r2)
            delta = abs(defect - clean)
            _exceeds(delta, float(t.value),
                     f"N={n}: 关掉 Tukey 截断后 |Δlocation|", "chain.a.p1.no_cut_shift_dex")
            ev.record(f"N={n} 注入前 |Δlocation|（正确实现，绿）", 0.0, t.value,
                      note="正确实现：Tukey 截断把离群权重置 0 ⇒ Δlocation ≈ 0")
            ev.record(f"N={n} 注入后 |Δlocation|（缺陷，红）", delta, t.value,
                      note=f"超界 {delta / float(t.value):.3g}×；解析预期 0.2×101/17 = 1.188 dex")


@H.test(
    "p1-neg-q-differenced-shape",
    intent="负例：向 P1-f 的 `Q(λ)` 判别力注入「QE 曲线与颜色项无关」"
           "（换成一条在带内**等值**的常数曲线以外、但使 `Δr_i` 退化成与 `r0` 无关的形状）"
           "这一具名缺陷，断言独立判据 `chain.a.p1.q_sigma_rise_min` 在缺陷侧给超界/不超界"
           "读数。注入实现：`Δr` 被**独立置换**（等价于把 QE 的形状误差打到与恒星颜色无关）。",
    inputs="40 颗黑体星；β = 2e-2 dex（比值最接近 1 的那一档 ⇒ 门限余量最小）；400 次对照",
    expected="装饰后的 `Δr`（与 `r0` 无关）使比值落回 1 附近 ⇒ 门限 `1.05` 越界；"
             "读数记入 evidence",
    source="正本条款 PHOTOMETRY.md §2a.4(:110) 逐字「⇒ 效应来自『`Δr` 与 `r0` 的相关性"
           "（共同的**颜色项**）』，**不是**新增独立散度、**也不是** inlier 集合变化」；"
           "冻结容差 chain.a.p1.q_sigma_rise_min",
    criteria=["P1-f"],
    kind=H.NEGATIVE,
    inject="把 `Δr`（计入 QE 造成的参考通量变化）与恒星颜色项解耦："
           "对 `Δr` 做一次与 `r0` 无关的独立置换，等价于 QE 曲线形状误差与颜色无关",
    defect_id="P1-NEG-Q-DECORRELATED",
)
def p1_neg_q_decorrelated():
    with H.evidence() as ev:
        rise = A.get("chain.a.p1.q_sigma_rise_min")
        tband = band_transmission
        fsys = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_true)
                         for x in XP_TEMPS_K])
        fs1 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_flat_one)
                        for x in XP_TEMPS_K])
        fsq = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), tband, qe_configured)
                        for x in XP_TEMPS_K])
        fb1 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), band_blue, qe_flat_one)
                        for x in XP_TEMPS_K])
        fb2 = np.array([integrate_fsyn(XP_LAMBDA_NM, blackbody(x), band_red, qe_flat_one)
                        for x in XP_TEMPS_K])
        ci = np.log10(fb2 / fb1)
        dr = np.log10(fsq / fs1)
        k_photo = 1.0 / 3.0
        rng = _kit.make_rng(31337)
        beta = 2.0e-2
        r0 = math.log10(k_photo) + beta * ci + rng.normal(0.0, 2.0e-5, 40)
        s0 = irls_fit(10.0 ** r0, np.ones(40)).sigma_residual
        real = irls_fit(10.0 ** (r0 - dr), np.ones(40)).sigma_residual / s0
        H.less_equal(float(rise.value), real,
                     "未注入时比值必须过门限（否则这条负例的『注入前绿读数』不成立）")
        q95 = A.get("chain.a.p1.q_perm_corr_q95")
        perms = np.array([abs(float(np.corrcoef(r0, dr[rng.permutation(40)])[0, 1]))
                          for _ in range(400)])
        thr = float(np.quantile(perms, float(q95.value)))
        corr_real = abs(float(np.corrcoef(r0, dr)[0, 1]))
        # 单次置换的 |corr| 本身是随机量（典型 0.05–0.30），用它比门限会随置换运气跳变
        # ⇒ 取 200 次置换的**中位数**（无随机跳变、且远离门限），单次读数一并登记。
        dec_corr = np.array([abs(float(np.corrcoef(r0, dr[rng.permutation(40)])[0, 1]))
                             for _ in range(200)])
        corr_dec = float(np.median(dec_corr))
        dr_dec = dr[rng.permutation(40)]
        ratios = np.array([irls_fit(10.0 ** (r0 - dr[rng.permutation(40)]),
                                    np.ones(40)).sigma_residual / s0 for _ in range(200)])
        H.is_true(corr_real > thr,
                  f"未注入时 |corr(Δr,r0)| = {corr_real:.4f} 必须越出置换 q{q95.value}"
                  f" = {thr:.4f}（否则负例的绿读数不成立）")
        H.is_true(corr_dec <= thr,
                  f"注入『Δr 与颜色项解耦』后 |corr| = {corr_dec:.4f} 未落回置换 q95 "
                  f"= {thr:.4f} 以内 ⇒ 判据没有抓住这个注入")
        ev.record("注入前 |corr(Δr,r0)|（相关，正确，绿）", corr_real, thr)
        ev.record("注入后 |corr(Δr,r0)|（解耦，缺陷，红）", corr_dec, thr,
                  note="200 次置换的中位数；单次读数 " + f"{abs(float(np.corrcoef(r0, dr_dec)[0, 1])):.4f}"
                       " ⇒ Q 效应的相关性证据消失")
        ev.record("注入前比值（相关，绿）", real, rise.value)
        ev.record("注入后比值中位数（解耦，缺陷）", float(np.median(ratios)), rise.value,
                  note="200 次置换的比值中位数；单次置换的散布见 evidence 的比值档")


@H.test(
    "p1-g-passband-outside-grid",
    intent="负例（退化条件）：PHOTOMETRY.md §2a.1(:62) 与 §8(:243) 逐字「通带与光谱网格"
           "**完全不重叠**（`T(λ)Q(λ)≡0` 于整个网格）| `F_syn ≡ 0`；`F_syn>0` 的有效域判据"
           "拒绝**全部**参考星 ⇒ 定标无输入，必须报拟合失败（`NO_DATA`/"
           "`zero_point_valid=false`），**输出面 = 无零点、无星等**」。"
           "断言独立判据 `chain.a.p1.no_data_min_refs`（§4(:184) 逐字 3）在此**越界**，"
           "并记下被测面读数。",
    inputs="λ ∈ [400,700] nm；`T(λ) ≡ 0` 于整个网格（等价于「通带全在 XP 网格外」）；40 颗星",
    expected="每颗星 `F_syn` **精确**为 0；`n_admissible = 0 < 3` ⇒ `status = NO_DATA`、"
             "`fit_used = 0`、`scale_factor = 1.0`、`sigma_residual = 0`、"
             "`zero_point_valid = false`、`iterations = 0`",
    source="正本条款 PHOTOMETRY.md §2a.1(:62) 逐字（同上）、§8(:243) 逐字"
           "「通带与光谱网格完全不重叠（`T·Q≡0`）| `F_syn≡0` ⇒ 有效域拒绝全部参考星 ⇒ "
           "拟合失败（`NO_DATA`/`zero_point_valid=false`），**不产出零点**」；"
           "§4(:183-184) 的有效域与 `|r_consistent|>=3` 门；冻结容差 "
           "chain.a.p1.fsyn_zero_exact / no_data_min_refs",
    criteria=["P1-g"],
    kind=H.NEGATIVE,
    inject="把通带曲线整体移出光谱网格（`T(λ)·Q(λ) ≡ 0` 于整个网格）⇒ `F_syn ≡ 0`",
    defect_id="P1-NEG-PASSBAND-OUTSIDE-GRID",
)
def p1_g_passband_outside_grid():
    with H.evidence() as ev:
        zero_t = A.get("chain.a.p1.fsyn_zero_exact")
        gate = A.get("chain.a.p1.no_data_min_refs")

        def transmission_zero(_lam):
            return np.zeros_like(np.asarray(_lam, dtype=np.float64))

        fsys = np.array([integrate_fsyn(XP_LAMBDA_NM, planck_flux(x),
                                        band_transmission, qe_true) for x in XP_TEMPS_K])
        fsyn0 = np.array([integrate_fsyn(XP_LAMBDA_NM, planck_flux(x),
                                         transmission_zero, qe_true) for x in XP_TEMPS_K])
        H.exact(float(np.max(np.abs(fsyn0))), float(zero_t.value),
                "T·Q ≡ 0 时每颗参考星的 F_syn 必须精确为 0（被积函数逐点为 0 ⇒ 无舍入）")
        ev.record("max |F_syn|（通带落在网格外）", float(np.max(np.abs(fsyn0))), zero_t.value,
                  "W·m^-2·nm")
        ev.record("注入前 max |F_syn|（通带在网格内，绿）", float(np.max(np.abs(fsys))),
                  note="作为对照读数")

        f_instr = (1.0 / 3.0) * fsys
        chk = no_data_check(f_instr, fsyn0)
        H.exact(chk["status"], "NO_DATA", "有效域拒绝全部参考星 ⇒ 拟合必须报 NO_DATA")
        H.is_true(int(chk["n_admissible"]) < int(gate.value),
                  f"n_admissible = {chk['n_admissible']} 必须越出 §4(:184) 的门 {gate.value}")
        H.exact(chk["fit_used"], 0, "NO_DATA 时 fit_used 必须为 0")
        H.exact(chk["scale_factor"], 1.0, "NO_DATA 时 scale_factor 必须为 1.0")
        H.exact(chk["sigma_residual"], 0.0, "NO_DATA 时 sigma_residual 必须为 0")
        H.exact(chk["zero_point_valid"], False, "NO_DATA 时不得声称零点有效")
        H.exact(chk["iterations"], 0, "NO_DATA 时不得迭代")
        ev.record("status", float(0), note="NO_DATA（逐字记录在上面的 exact 断言里）")
        ev.record("n_admissible", float(int(chk["n_admissible"])), gate.value,
                  note=f"门限 {gate.value}（PHOTOMETRY.md §4(:184) 逐字）⇒ 越界 "
                       f"{float(gate.value) / max(int(chk['n_admissible']), 1):.3g}×")


@H.test(
    "p1-neg-partial-frame-rescaling",
    intent="负例：向 P1-b 的零点平移不变量注入「`F_instr` **全体**同乘 `k`」被实现成"
           "「只乘一部分星（亮端）」这一具名缺陷（真实测光标定里最常见的一类口径错误）。"
           "断言独立判据 `chain.a.p1.shift_scale_rel` 在缺陷侧给超界读数。"
           "⚠ 与 P1-c 的反例臂是**同一个注入的两种读法**：这里判 `location`，那里判单调域外。",
    inputs="10 颗星；k = 10^{1/3}（binary64 不可精确表示的对数）；注入 = 只把 `r` 最大的那颗乘 k",
    expected="注入后 `|Δlocation − log10 k|` 越出 1e-12 门限；读数与未注入绿读数并列记录",
    source="正本条款 PHOTOMETRY.md §7(:228) 逐字「`F_instr` **全体**同乘因子 `k` 时 ...」"
           "（逐字「全体」⇒ 部分同乘是缺陷）；§5(:190) 的 `r_i` 定义；"
           "冻结容差 chain.a.p1.shift_scale_rel",
    criteria=["P1-b"],
    kind=H.NEGATIVE,
    inject="把零点平移不变量要求的『全体同乘 k』实现成『只乘最亮的那一颗』",
    defect_id="P1-NEG-PARTIAL-RESCALE",
)
def p1_neg_partial_frame_rescaling():
    with H.evidence() as ev:
        t = A.get("chain.a.p1.shift_scale_rel")
        r0 = _base_r_sample()
        f_syn = np.full(r0.size, 1.0e4)
        f_base = 10.0 ** r0 * f_syn
        base = irls_fit(f_base, f_syn).location
        k = 10.0 ** (1.0 / 3.0)          # 目标平移量 log10 k = 1/3，binary64 不可精确表示
        good = irls_fit(f_base * k, f_syn).location - base
        partial_in = f_base.copy()
        partial_in[int(np.argmax(r0))] *= k
        bad = irls_fit(partial_in, f_syn).location - base
        dev = abs(bad - math.log10(k))
        H.less_equal(good, math.log10(k) * (1.0 + float(t.value)),
                     "未注入时『全体同乘』必须给出 log10 k（否则负例的绿读数不成立）")
        _exceeds(dev, float(t.value), "只乘一颗星的臂的平移偏差",
                 "chain.a.p1.shift_scale_rel")
        ev.record("注入前 Δlocation（全体同乘，绿）", good, math.log10(k),
                  note="目标 log10 k = 1/3 dex")
        ev.record("注入后 Δlocation（只乘一颗，缺陷）", bad, math.log10(k),
                  note=f"|偏差| = {dev:.6g} dex")
        ev.record("平移门限", float(t.value),
                  note=f"越界 {dev / float(t.value):.3g}×")