r"""P2 · 跨帧绝对信噪比的合成全链不变量（`AGENTS.md` §10 创新点二）。

正本：`docs/science/noise_snr/NOISE_SNR.md`（SCI-NOISE）。

## 这一层测什么 / 不测什么

**测**：P2 的**绝对信噪比定义面**（方差面已被
`eng/tests/integration/test_variance_propagation.py` 的 A7/A8/A9 + B10–B14 覆盖 ⇒
**不重写**）：`k_photo` 整除消去、天光只进噪声项、`σ_i²` 三项组装、`m_5` 闭式、
控制点方差、信息量判据、跨帧可比性硬约束。

**不测**：不裁决代码、不产流水线判决（`05_INDEPENDENT_TEST_SUITE.md` §4、
`docs/engineering/testing/TEST.md` §9）。

## ⚠ 三条黑名单（本文件刻意不写的判据）

1. **`Σ_k SNR_k² = F0²/Var(F̂)`**（NOISE_SNR.md §3.4:353-359）正本逐字「对任何可达输入
   恒成立，**结构上不可能给出判决**」。⇒ 本文件只写正本指定的那一条替代判据
   （§3.4:359 逐字「承载证据的判据是**点源信息量**：`Var(F_hat) = 1/sum_k W_k` 与实测通量
   散度对拍，参照量取自权重之外，因而有鉴别力」），**参照量是定种子蒙特卡洛实测的通量散度**。
2. **`σ_F := F_ref/SNR` 取逆**（NOISE_SNR.md §3.3:278 逐字「任何把 `k_photo,k` 直接当作
   前向亮度因子的写法方向相反」）⇒ 不用它当参照量。P2-a 的第二参照量取**蒙特卡洛实测散度**。
3. **`w = SNR²/F_ref,k²`**（NOISE_SNR.md §3.4:336-341 逐字这是唯一真实的失效面）⇒
   只作为**负例注入**出现（`p2-neg-wrong-normalization`），不作为正例。

## ⚠ 天光纪律（04 §2.2 / NOISE_SNR.md §3.3:295,306）

逐字「**分子只有源**：天光只经散粒噪声进入分母。把天光电平或观测电平计入分子，
就得到一个随天光上升的量，与信噪比的定义相反」。

⇒ 本文件里天光**只**以两种形态出现：① 逐像素方差里的散粒项 `S_sky/g`；
② 定种子蒙特卡洛采样时的 Poisson 入射率。**没有任何一条正例**把天光放进分子；
「天光进分子」只作为**负例注入**出现（`p2-neg-sky-in-numerator`），并断言
单调下降判据给它超界读数。

## ⚠ `k_photo` 的物理解释（本文件的核心建模决定）

`k_photo,k` 是**帧面 ADU 与统一测光坐标系之间的乘性标度**（NOISE_SNR.md §3.3:270 逐字
「`k_photo,k` 是帧 `k` 的**测光标度因子**（帧面 ADU 与统一测光坐标系之间的乘性标度，
`F_sys ≡ k_photo,k · F_adu`）」）。⇒ **同一个物理源在不同帧上换 `k_photo`，
它在 ADU 域的读数与噪声同步缩放**：`F_adu,k = F_sys/k_photo,k`、
`σ_i²(frame k) = σ_i²(frame 1)/k_photo,k²`。

⚠ **本层实测踩到并否决的另一种读法**：若把 `k_photo` 只作用在 `F_ref` 上而不缩放
`σ_F^{frame}`，则 `SNR ∝ 1/k_photo`，看起来「整除消去」被推翻。**那不是消去被推翻，
是帧的建模错了**（真实帧换了测光标度后 ADU 电平与噪声一起变）。本文件的所有
`k_photo` 用例都按上面的同步缩放构造，并在 evidence 里记下 `W_k` 对 `k_photo` 的独立性。

## 夹具

全部为**字面常量**的解析夹具（ADU 标度）。逐源最优提取权重用归一化高斯轮廓
（`Σ P = 1`，NOISE_SNR.md §3.3:252 逐字「`P_i` 是归一 PSF 轮廓（`sum_i P_i = 1`）」）。
蒙特卡洛重复次数 `R = 200` 取本层统一冻结值（骨架表 `synth.mc.replicates`），
分位数法口径见 `synth.mc.method`。`testdata/HST_M16/` 不在本文件任何判据的输入面上
（P2 的输入是**解析已知**的通量/背景/噪声参数），因此本文件不抛 `TemplateUnavailable`。
"""

from __future__ import annotations

import math
from typing import Dict, List, Sequence, Tuple

import numpy as np

from eng.tests.synthetic import _kit
from eng.tests.synthetic import _tol_chain_a as A
from eng.tests.synthetic import tolerances as tol
from eng.tests.unit import harness as H

R_REPLICATES = int(tol.get("synth.mc.replicates").value)   # 200
Z95 = tol.Z95
F64_RTOL = tol.F64_RTOL                                      # 1e-12（骨架表 §1 通用档）

# ---------------------------------------------------------------------------
# §1 字面夹具（每条都在用例的 `source` 里登记出处）
# ---------------------------------------------------------------------------

#: 参考星在帧面的通量（ADU，k_photo = 1 档）。冻结字面量，域 1e3–1e5 ADU。
F_SRC_ADU = 1.0e4
#: 增益（e⁻/ADU）与读噪（e⁻ rms）：与 `_kit.HST_GAIN_E_PER_ADU_FALLBACK = 1.5` 同值；
#: 本层取**帧面**口径的 `RN = 25 e⁻`，使 `(RN/g)² = 277.8 ADU²` 占解析方差的
#: 几个百分点以上 —— `chain.a.p2.var_double_count_min` 的推导要求这一占比够大，
#: 否则双重计数的负例不可触发。
GAIN_E_PER_ADU = 1.5
READ_NOISE_E = 25.0
DARK_ADU = 20.0
#: PSF：像素数与 FWHM（像元）。⚠ `_kit` 的坑：离散核峰值 ≠ `1/(2πσ²)`（FWHM<2 时低 10%）
#: ⇒ 本文件**不判核峰值**，只用闭式可判的归一性 `Σ P = 1`。
FWHM_PX = 3.0
N_PX = 400
#: 测光标度因子 `k_photo` 的跨帧取值（3.5 倍跨度）。
K_PHOTO_SET: Tuple[float, ...] = (0.8, 1.0, 1.35, 1.7, 2.1, 2.8)
#: 天光扫掠：每 decade 5 档的 21 点对数扫掠，域 1e3–1e7 ADU/px。
SKY_DECADES = (3.0, 7.0)
SKY_STEPS = 21
#: 天光受限（渐近）子域的门：天光必须同时压过暗流+读噪与源散粒项 100 倍以上。
SKY_LIMITED_FACTOR = 100.0
#: 跨帧可比性臂的固定天光（ADU/px）与参考电平扫掠跨度（dex）。
ARM_SKY_ADU = 1.0e8
ARM_FLUX_DECADES = (-1.0, 1.0)
ARM_STEPS = 11
#: 参考星等档与合成零点（NOISE_SNR.md §3.3:261-267 的 `m_ref` / `ZP_syn`）。
M_REF = 18.0
ZP_SYN = 26.4899
#: 方差面蒙特卡洛的抽样次数与每档像元数（两点差分 → 1 自由度）。
VAR_N_DRAW = 64
VAR_N_PX = 400


def sky_sweep() -> np.ndarray:
    return np.logspace(SKY_DECADES[0], SKY_DECADES[1], SKY_STEPS)


def gauss_profile(n: int, fwhm: float) -> np.ndarray:
    """归一化高斯 PSF 轮廓 `P`（`Σ P_i = 1`，NOISE_SNR.md §3.3:252 逐字）。"""
    sigma = float(fwhm) / 2.3548200450309493
    i = np.arange(n, dtype=np.float64) - n // 2
    k = np.exp(-0.5 * (i / sigma) ** 2)
    return k / k.sum()


# ---------------------------------------------------------------------------
# §2 被测实现：逐像素方差 / σ_F / SNR / m_5 / 信息量（NOISE_SNR.md §3.3–§3.4）
# ---------------------------------------------------------------------------

def assemble_variance(f_adu: np.ndarray, p: np.ndarray, sky_adu: float,
                      *, mode: str = "shot_noise_only",
                      rn_e: float = READ_NOISE_E, gain: float = GAIN_E_PER_ADU,
                      dark_adu: float = DARK_ADU) -> np.ndarray:
    r"""逐像素方差面（ADU²）。

    `shot_noise_only`（NOISE_SNR.md §3.3:290 逐字「只含天光与暗流的散粒噪声，
    此时叠加 `(RN/g)²`」）：

    ```text
    sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F * P_i / g
    ```

    `empirical_total_rms`（:291 逐字「是经验总均方根、已含读噪噪声，此时**不再叠加**
    `(RN/g)²`」）：

    ```text
    sigma_i^2 = sigma_sky^2 + F * P_i / g
    ```

    `sigma_sky²` 口径：天光与暗流在 ADU 标度上逐像元入射 ⇒ 泊松方差 `= 入射 ADU`
    （NOISE_SNR.md §2.2 的随机方差项表；电子域 `Var = λ`，ADU 域除以 `g` 已并入 `F/g`）。
    """
    p = np.asarray(p, dtype=np.float64)
    var = float(sky_adu) + float(dark_adu) + float(f_adu) * p / float(gain)
    if mode == "shot_noise_only":
        var = var + (float(rn_e) / float(gain)) ** 2
    elif mode != "empirical_total_rms":
        raise ValueError(f"未知的天空项语义模式 {mode!r}；NOISE_SNR.md §3.3:288 要求显式二选一")
    return var


def sigma_F_frame(p: np.ndarray, var: np.ndarray) -> float:
    """帧面通量不确定度 `σ_F = (Σ_i P_i²/σ_i²)^{−1/2}`
    （NOISE_SNR.md §3.3:255 逐字 `sigma_F^{-2} = sum_i P_i^2 / sigma_i^2`）。单位 ADU。"""
    return float(1.0 / math.sqrt(float(np.sum(np.asarray(p) ** 2 / np.asarray(var)))))


def frame_variance_at_k(f_adu: float, p: np.ndarray, sky_adu: float, k_photo: float) -> np.ndarray:
    """`k_photo` 帧的逐像素方差面：ADU 读数与噪声同步缩放（本文件 docstring 的「核心建模决定」）。

    每一项（天光、暗流、读噪、源散粒）都是 ADU 域的量，换 `k_photo` 后全部除
    `k_photo,k` ⇒ 整个方差面 `× 1/k_photo,k²`。
    ⚠ 只缩放天光与源、漏掉暗流与读噪，是本层实测踩到并否决的写法（它会让
    `σ_F^{frame,k}` 不再按 `1/k_photo` 缩放，「整除消去」看起来被推翻——那不是消去被推翻，
    是帧的建模错了）。
    """
    return assemble_variance(f_adu, p, sky_adu) / float(k_photo) ** 2


def frame_variance_k_scope_misspecified(f_adu: np.ndarray, p: np.ndarray,
                                        sky_adu: float, k_photo: float) -> np.ndarray:
    """**缺陷臂**：`k_photo` 只作用在**源散粒项**上，天光/暗流/读噪不随 `k_photo` 缩放。

    ```text
    var_i(k) = S_sky + DARK + (RN/g)^2 + F*P_i/(g*k^2)
    ```

    这正是本文件 docstring「⚠ 本层实测踩到并否决的另一种读法」描述的**错建模**：
    真实帧换了测光标度后 ADU 电平与噪声**一起**变，只缩放源项会让
    `σ_F^{frame,k}` 不再按 `1/k_photo` 缩放 —— 「整除消去」看起来被推翻，
    **那不是消去被推翻，是帧的建模错了**。

    它的闭式（可复算，与本函数逐位一致到 1.4e-16）：
    `SNR_k = F0·sqrt(Σ_i P_i²/(k²·base + S_i))`、
    `W_k = Σ_i P_i²/(k²·base + S_i)`，其中 `base = S_sky + DARK + (RN/g)²`、
    `S_i = F·P_i/g`。**跨帧散布在这个错建模下恒不为 0**（源项占比越大越接近
    `std(k)/mean(k) = 0.41728`）⇒ 它是 P2-a 的 ②③ 判别力来源。
    """
    base = (float(sky_adu) + float(DARK_ADU)
            + (float(READ_NOISE_E) / float(GAIN_E_PER_ADU)) ** 2)
    src = np.asarray(f_adu, dtype=np.float64) * np.asarray(p, dtype=np.float64) \
        / (float(GAIN_E_PER_ADU) * float(k_photo) ** 2)
    return base + src


def snr_k_misspecified_closed_form(p: np.ndarray, sky_adu: float,
                                   f0: float, k_photo: float) -> float:
    """`frame_variance_k_scope_misspecified` 的 **闭式解析** SNR
    （闭式与实现逐位一致到 1.4e-16；用作该缺陷臂的**独立**参照量）。

    ```text
    SNR_k = F0·sqrt(Σ_i P_i²/(k²·base + S_i)),  base = S_sky+DARK+(RN/g)², S_i = F·P_i/g
    ```

    ⚠ 分子是 **`F0`（公共锚）不是 `F_ref,k`**：`F_ref,k = F0/k` 已经并进 `k²·base` 那一项，
    写成 `F_ref,k·sqrt(Σ P²/(k²base+S))` 会多带一个 `1/k`。
    """
    base = (float(sky_adu) + float(DARK_ADU)
            + (float(READ_NOISE_E) / float(GAIN_E_PER_ADU)) ** 2)
    s_i = np.asarray(p, dtype=np.float64) * float(F_SRC_ADU) / float(GAIN_E_PER_ADU)
    k = float(k_photo)
    return float(f0) * math.sqrt(float(np.sum(
        np.asarray(p, dtype=np.float64) ** 2 / (k * k * base + s_i))))


def f_ref(k_photo: float, f0: float) -> float:
    """参考通量 `F_ref,k = F0 / k_photo,k`（NOISE_SNR.md §3.3:267 逐字）。"""
    return float(f0) / float(k_photo)


def snr_frame(f_ref_adu: float, sigma_f_adu: float) -> float:
    """帧级绝对信噪比 `SNR = F_ref / σ_F`（NOISE_SNR.md §3.3:256 逐字）。"""
    return float(f_ref_adu) / float(sigma_f_adu)


def snr_systemic(k_photo: float, f0: float, sigma_f_frame: float) -> float:
    """同一 SNR 的**统一测光坐标系**写法 `SNR = F0 / σ_F^{sys,k}`，
    `σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}`（NOISE_SNR.md §3.3:273-276 逐字）。

    ⚠ 这与 `snr_frame` 是**同一个量的两种写法**（代数恒等），正本 :278 逐字指出
    `k_photo,k` 在其中整除消去 ⇒ 单独判它**恒真**。本文件把它写成「两条写法一致」的
    辅助断言，判别力放在 `p2-a` 的**第二个不同口径的参照量**（蒙特卡洛实测通量散度）上。
    """
    return float(f0) / (float(k_photo) * float(sigma_f_frame))


def zp_frame(k_photo: float, zp_syn: float = ZP_SYN) -> float:
    """帧测光零点 `ZP_k = ZP_syn - 2.5 * log10( k_photo,k )`（NOISE_SNR.md §3.3:264 逐字）。"""
    return float(zp_syn) - 2.5 * math.log10(float(k_photo))


def m5(k_photo: float, sigma_f_ref: float, zp_syn: float = ZP_SYN) -> float:
    """5σ 点源深度 `m_5 = ZP_k - 2.5 * log10( 5 * sigma_F^{frame}(ref) )`
    （NOISE_SNR.md §3.3:311 逐字）。"""
    return zp_frame(k_photo, zp_syn) - 2.5 * math.log10(5.0 * float(sigma_f_ref))


def snr_from_m5(m5_mag: float, k_photo: float, sigma_f_ref: float,
                zp_syn: float = ZP_SYN) -> float:
    """由 `m_5` 反解通量再取 SNR（NOISE_SNR.md §3.3:314 逐字「`m_5` 由信噪比定义与星等定义
    直接导出：令 `SNR = 5` 解出 `F` 再换算星等」）。"""
    f_adu = 10.0 ** ((zp_frame(k_photo, zp_syn) - float(m5_mag)) / 2.5)
    return f_adu / float(sigma_f_ref)


def f0_anchor(m_ref: float = M_REF, zp_syn: float = ZP_SYN) -> float:
    """物理公共锚 `F0 = 10^{−0.4·(m_ref − ZP_syn)}`（NOISE_SNR.md §3.3:266 逐字，与帧无关）。"""
    return 10.0 ** (-0.4 * (float(m_ref) - float(zp_syn)))


def frame_weight(p: np.ndarray, var: np.ndarray, k_photo: float) -> float:
    """点源信息量 `W_k = P^T C^{-1} P / k_photo,k²`（NOISE_SNR.md §3.4:347 逐字）。"""
    return float(np.sum(np.asarray(p) ** 2 / np.asarray(var))) / float(k_photo) ** 2


def simulate_fhat(rng: np.random.Generator, f_sys: float,
                  frames: Sequence[Tuple[np.ndarray, np.ndarray, float]]) -> float:
    """按 `d_k = (1/k_photo,k)·F_sys,k·P_k + n_k`、`n_k ~ N(0, C_k)`
    （NOISE_SNR.md §3.3:343 逐字）算 `Q_k = (1/k_photo,k)·P_k^T C_k^{-1} d_k`（:346 逐字）
    并返回 `Q_k` 之和（`F̂ = ΣQ/ΣW` 的分子）。"""
    num = 0.0
    for p, var, k_photo in frames:
        d = (1.0 / float(k_photo)) * float(f_sys) * np.asarray(p) + rng.normal(
            0.0, np.sqrt(np.asarray(var)))
        num += (1.0 / float(k_photo)) * float(np.dot(np.asarray(p), d / np.asarray(var)))
    return num


def _exceeds(actual: float, limit: float, what: str, tol_key: str) -> None:
    try:
        f = A.get(tol_key)
    except KeyError:
        f = tol.get(tol_key)          # 骨架表的键（量级冻结的占位）
    H.is_true(actual > limit,
              f"{what}: 实测 {actual:.6g} 未越出冻结门限 {limit:.6g}"
              f"（{f.key} = {f.value!r}）⇒ 这条负例无效")


# ---------------------------------------------------------------------------
# §3 正例
# ---------------------------------------------------------------------------

@H.test(
    "p2-a-kphoto-cancels-in-snr",
    intent="NOISE_SNR.md §3.3(:267,:273-278) 的核心代数：`F_ref,k = F0/k_photo,k`、"
           "`σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}` ⇒ `k_photo` 在 "
           "`SNR = F_ref,k/σ_F^{frame,k} = F0/σ_F^{sys,k}` 中**整除消去** ⇒ SNR 对测光标度"
           "**无条件不变**（正本 :278 逐字「跨帧可比性是这条代数恒等式的推论，"
           "不依赖任何附加假设）。"
           "⚠⚠ **本条的三条恒真/夹具恒等必须逐条标明（A/C 类，TAUTOLOGY_REGISTER §0）**："
           "①「两条代数写法同值」是**同源恒等**；"
           "②「跨帧 SNR 散布 ≤ 1e-12」与 ③「逐帧 `W_k` 散布 ≤ 1e-12」在**给定夹具 "
           "`frame_variance_at_k`（把整个方差面除 `k²`）下是构造级恒真**——"
           "`σ_F^{frame,k} ≡ σ_F^{frame,1}/k` 逐位成立 ⇒ `SNR_k ≡ F0/σ_F^{frame,1}`、"
           "`W_k ≡ 常数`，**任何输入都不能让它们变红**。"
           "⇒ 本条据此重写：②③ 改成**作用域敏感性**双臂判据（绿臂 = 同步缩放 ⇒ 散布 ≤ "
           "f64 非归约档；红臂 = `k_photo` 只作用于源散粒项的错建模 ⇒ 散布必须越出冻结下界 "
           "`chain.a.p2.kscope_spread_min`，并与闭式解析对拍）。判别力在红臂与 ④。",
    inputs="6 帧 `k_photo ∈ {0.8, 1.0, 1.35, 1.7, 2.1, 2.8}`（3.5 倍跨度）、"
           "`S_sky = 1e4 ADU/px`、400 像元 PSF；绿臂 = ADU 读数与噪声**同步**缩放"
           "（见 docstring 的「k_photo 的物理解释」）；"
           "红臂（错建模）= `k_photo` **只作用在源散粒项**、天光/暗流/读噪不缩放；"
           "R = 200 次定种子重复",
    expected="①每帧的 `F_ref/σ_F^{frame}` 与 `F0/σ_F^{sys}` 在 f64 非归约档一致"
             "（**同源恒等的对照臂**，不承担判别力）；"
             "②绿臂：同步缩放下逐帧 `SNR` 相对散布 ≤ 1e-12、`W_k` 散布 ≤ 1e-12"
             "（**夹具构造级恒真**，已标注）；"
             "③红臂（判别力）：错建模下两个散布都 **≥ 1e-1**，且逐帧 `SNR` 与闭式 "
             "`F0·√(ΣP²/(k²·base+S_i))` 相对一致到 1e-12；"
             "④并合 GLS 的 `σ̂(F̂)` 与 `√(1/Σ_k W_k)` 相对偏差 ≤ 1.1e-1"
             "（**第二个不同口径的参照量**）",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:267,:270,:273-278) 与 "
           "§3.4(:343-351)（统一线性模型 `d_k = (1/k_photo,k)·F_sys,k·P_k + n_k`、`Q_k`、"
           "`W_k`、`F_hat = sum_k Q_k / sum_k W_k`、`Var(F_hat) = 1/sum_k W_k`）；"
           "统计口径分位数法见骨架表 synth.mc.method；"
           "冻结容差 chain.a.p2.information_rel（② 绿臂用骨架表 F64_RTOL = 1e-12 通用档；"
           "**③ 红臂用新增冻结 chain.a.p2.kscope_spread_min**）",
    criteria=["P2-a", "P2-f"],
)
def p2_a_kphoto_cancels():
    with H.evidence() as ev:
        f0 = f0_anchor()
        p = gauss_profile(N_PX, FWHM_PX)
        sky = 1.0e4
        frames = [(p, frame_variance_at_k(F_SRC_ADU, p, sky, k), k) for k in K_PHOTO_SET]
        ws = [frame_weight(pp, vv, k) for pp, vv, k in frames]
        sigma_pred = math.sqrt(1.0 / sum(ws))

        snrs, snr_sys, worst_two, w_spread = [], [], 0.0, 0.0
        for (pp, vv, k) in frames:
            sf = sigma_F_frame(pp, vv)
            a = snr_frame(f_ref(k, f0), sf)
            b = snr_systemic(k, f0, sf)
            worst_two = max(worst_two, abs(a - b) / a)
            snrs.append(a)
            snr_sys.append(b)
        H.less_equal(worst_two, F64_RTOL,
                     "两条代数写法的相对差（同源恒等的对照臂，见 docstring 的标注）")
        spread = float(np.std(snrs) / np.mean(snrs))
        H.less_equal(spread, F64_RTOL,
                     "整除消去 ⇒ 跨帧 SNR 逐帧相同（绿臂：夹具构造级恒真，已标注）")
        w_spread = float(np.std(ws) / np.mean(ws))
        H.less_equal(w_spread, F64_RTOL,
                     "纯测光标度重标定不改变信息量 ⇒ `W_k` 逐帧相同（绿臂：同上）")

        # ---- 红臂（判别力）：`k_photo` 的**作用域**错建模 -------------------------
        # 绿臂之所以恒真，是因为夹具把整个方差面除 `k²`。一旦 `k_photo` 只作用在源散粒项
        # （天光/暗流/读噪不缩放），`σ_F^{frame,k}` 不再按 `1/k_photo` 缩放，
        # 跨帧散布立刻张开 ⇒ 这才是 ②③ 的牙。
        kscope = A.get("chain.a.p2.kscope_spread_min")
        snrs_bad, ws_bad = [], []
        for k in K_PHOTO_SET:
            vb = frame_variance_k_scope_misspecified(F_SRC_ADU, p, sky, k)
            sfb = sigma_F_frame(p, vb)
            snrs_bad.append(snr_frame(f_ref(k, f0), sfb))
            ws_bad.append(frame_weight(p, vb, k))
        spread_bad = float(np.std(snrs_bad) / np.mean(snrs_bad))
        w_spread_bad = float(np.std(ws_bad) / np.mean(ws_bad))
        # 闭式解析对拍（独立于 `frame_variance_k_scope_misspecified` 的实现路径）
        snrs_cf = [snr_k_misspecified_closed_form(p, sky, f0, k)
                   for k in K_PHOTO_SET]
        cf_rel = float(np.max(np.abs(np.array(snrs_cf) - np.array(snrs_bad)))
                       / np.max(np.abs(np.array(snrs_bad))))

        H.is_true(spread_bad >= float(kscope.value),
                  f"红臂：`k_photo` 只作用于源散粒项时跨帧 SNR 散布 {spread_bad:.4f} "
                  f"未达冻结下界 {float(kscope.value)} ⇒ 该判据对作用域错建模无判别力")
        H.is_true(w_spread_bad >= float(kscope.value),
                  f"红臂：同一错建模下 `W_k` 散布 {w_spread_bad:.4f} "
                  f"未达冻结下界 {float(kscope.value)} ⇒ 无判别力")
        H.less_equal(cf_rel, F64_RTOL,
                     "红臂 SNR 与闭式 `F0·√(ΣP²/(k²·base+S_i))` 不一致（闭式参照量）")

        ev.record("①两条代数写法的最大相对差", worst_two, F64_RTOL,
                  note="**A 类恒真对照**：同源恒等，不承担判别力")
        ev.record("②绿臂 跨帧 SNR 相对散布", spread, F64_RTOL,
                  note="**A 类夹具恒等**：`frame_variance_at_k` 把整个方差面除 k² ⇒ "
                       f"σ_F^{{frame,k}} ≡ σ_F^{{frame,1}}/k 逐位成立；"
                       f"k_photo 跨度 {max(K_PHOTO_SET) / min(K_PHOTO_SET):.2f}× 下的读数")
        ev.record("②绿臂 逐帧 W_k 相对散布", w_spread, F64_RTOL,
                  note="**A 类夹具恒等**：同上；参照量 1/σ_F^{sys,2}")
        ev.record("③红臂 跨帧 SNR 相对散布（作用域错建模）", spread_bad,
                  float(kscope.value),
                  note=f"**判别力读数**：超界 {spread_bad / float(kscope.value):.3g}×；"
                       f"极限 std(k)/mean(k) = "
                       f"{float(np.std(np.array(K_PHOTO_SET)) / np.mean(K_PHOTO_SET)):.4f}")
        ev.record("③红臂 逐帧 W_k 相对散布（作用域错建模）", w_spread_bad,
                  float(kscope.value),
                  note=f"**判别力读数**：超界 {w_spread_bad / float(kscope.value):.3g}×")
        ev.record("③红臂 SNR vs 闭式解析的最大相对差", cf_rel, F64_RTOL,
                  note="闭式 `SNR_k = F0·√(Σ_i P_i²/(k²·base + S_i))`，"
                       "`base = S_sky + DARK + (RN/g)²`、`S_i = F·P_i/g`")
        ev.record("③红臂逐帧 SNR（错建模）", float(snrs_bad[0]), None, "",
                  note="k = 0.8 档；同族读数 " +
                       ", ".join(f"{s:.4f}" for s in snrs_bad))
        ev.record("SNR（k_photo=1 档，正确建模）", snrs[K_PHOTO_SET.index(1.0)])

        # 第二个不同口径的参照量：蒙特卡洛实测通量散度
        rng = _kit.make_rng(777)
        est = np.array([simulate_fhat(rng, F_SRC_ADU, frames) / sum(ws)
                        for _ in range(R_REPLICATES)])
        sigma_hat = float(np.std(est, ddof=1))
        rel = abs(sigma_hat / sigma_pred - 1.0)
        c = A.get("chain.a.p2.information_rel")
        H.less_equal(rel, float(c.value),
                     f"实测通量散度 vs 闭式 √(1/Σ_k W_k)（冻结 {c.key} = {c.value!r}）")
        ev.record("σ̂(F̂)（蒙特卡洛实测）", sigma_hat, note=f"R = {R_REPLICATES}")
        ev.record("√(1/Σ_k W_k)（闭式）", sigma_pred, note="参照量取自权重之外")
        ev.record("相对偏差", rel, c.value)
        ev.record("Σ_k W_k", sum(ws), note="ADU^-2")


@H.test(
    "p2-b-sky-only-in-noise",
    intent="NOISE_SNR.md §3.3(:295) 逐字「在探测器未饱和、噪声估计由天光散粒主导的成立域内，"
           "**固定源通量下天光越亮信噪比越低并趋于零：分母随天光开方增长而分子不变**」。"
           "逐字判：①SNR 在 21 档天光上**严格递减**（精确档）；②天光受限极限 "
           "`SNR·√(S_sky·A_NEA) → S_src`；③分子不含天光由 ① 承担（若分子含天光，"
           "SNR 随天光上升 ⇒ ① 立刻红）。",
    inputs="源通量固定 `F_src = 1e4 ADU`；天光 `S_sky = 10^{linspace(-1,3,21)}` ADU/px；"
           "`RN = 25 e⁻`、`g = 1.5`、暗流 20 ADU/px；天光受限子域取"
           "`S_sky ≥ 100·max(dark + (RN/g)², F_src·P_max/g)`",
    expected="①21 档天光上 SNR 逐点严格递减，零违例；②天光受限档 "
             "`|SNR·√(S_sky·A_NEA)/F_src − 1| ≤ 1e-2`；③SNR 在最大天光档比最小档小 ≥ 30 倍",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:285,:295,:306) 逐字"
           "（逐像素方差式 / 「分子只有源」/ 「天光只经散粒噪声进入分母」）；"
           "`A_NEA = 1/Σ_i P_i²` 是 §3.4(:351) 逐字给的噪声等效面积；"
           "冻结容差 chain.a.p2.sky_asymptote_rel / sky_monotonic_margin",
    criteria=["P2-b"],
)
def p2_b_sky_only_in_noise():
    with H.evidence() as ev:
        p = gauss_profile(N_PX, FWHM_PX)
        a_nea = 1.0 / float(np.sum(p ** 2))
        sky = sky_sweep()
        var = [assemble_variance(F_SRC_ADU, p, float(s)) for s in sky]
        snr = np.array([snr_frame(F_SRC_ADU, sigma_F_frame(p, v)) for v in var])
        m = A.get("chain.a.p2.sky_monotonic_margin")
        nonmono = int(np.sum(np.diff(snr) >= 0.0))
        H.exact(nonmono, 0, "固定源通量下 SNR 必须随天光**严格递减**（零违例）")
        H.less_equal(float(snr[-1] / snr[0]), 1.0 / 30.0,
                     "天光递增 1e4 倍时 SNR 必须至少下降 30 倍（SNR ∝ 1/√S_sky）")
        ev.record("天光档数", float(sky.size), note="S_sky ∈ [1e3, 1e7] ADU/px")
        ev.record("单调性违例点数", float(nonmono), m.value,
                  note="门限 0 = 严格递减；天光进分子时该数会变成 20")
        ev.record("SNR 最小档 / 最大档", float(snr[0] / snr[-1]),
                  note="理论 ~ √(1e7/1e3) = 100")

        tolr = A.get("chain.a.p2.sky_asymptote_rel")
        thr = SKY_LIMITED_FACTOR * max(DARK_ADU + (READ_NOISE_E / GAIN_E_PER_ADU) ** 2,
                                       F_SRC_ADU * float(np.max(p)) / GAIN_E_PER_ADU)
        lim = sky >= thr
        prod = snr[lim] * np.sqrt(sky[lim] * a_nea)
        worst = float(np.max(np.abs(prod / F_SRC_ADU - 1.0)))
        H.less_equal(worst, float(tolr.value),
                     f"天光受限档的渐近恒等式（冻结 {tolr.key} = {tolr.value!r}）")
        ev.record("天光受限档门限 S_sky ≥ (ADU/px)", thr,
                  note="= 100 × max(dark+(RN/g)², F_src·P_max/g)")
        ev.record("天光受限档点数", float(np.sum(lim)))
        ev.record("max |SNR·√(S_sky·A_NEA)/F_src − 1|", worst, tolr.value,
                  note="A_NEA = " + f"{a_nea:.6f}" + " px")


@H.test(
    "p2-c-variance-three-term-assembly",
    intent="NOISE_SNR.md §3.3(:285,:288-293) 的逐像素方差组装："
           "`sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F * P_i / g`，"
           "且天空项语义**必须二选一并显式声明**：`shot_noise_only` 叠加 `(RN/g)²`、"
           "`empirical_total_rms` **不再叠加**。本条判被测的方差面与**定种子蒙特卡洛实测**"
           "逐像元一致（参照量取自采样实现之外），并证明两模式之差恰为 `(RN/g)²`。",
    inputs="400 像元 × 3 档（`S_sky = 1e3, 1e4, 1e7` ADU/px）× 2 种语义；"
           "`F_src = 1e4 ADU`、`RN = 25 e⁻`、`g = 1.5`、暗流 20 ADU/px；"
           "每档 64 次定种子采样（两点差分 ⇒ 1 自由度）",
    expected="两模式的解析方差逐像元给出；逐像元实测/解析之比的**中位**落在 ±20% 内"
             "（单自由度冻结档）；`empirical_total_rms` 的比值中心同样在 1 附近，"
             "两模式之差**逐位**等于 `(RN/g)²`",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:285,:290-293) 逐字"
           "（方差式与两种天空项语义）+ §2.2 的随机方差项表；"
           "统计口径：n 点样本方差的相对标准差 `√(2/(n−1))`（n = 64 ⇒ 0.178），"
           "400 像元的中位读数散布 `0.178·1.253/20 = 0.0112`；"
           "冻结档用骨架表 synth.mc.var_ratio_ci95 = 2.0e-1",
    criteria=["P2-c"],
)
def p2_c_variance_assembly():
    with H.evidence() as ev:
        p = gauss_profile(VAR_N_PX, FWHM_PX)
        rng = _kit.make_rng(4242)
        vt = tol.get("synth.mc.var_ratio_ci95")
        for sky in (1.0e3, 1.0e4, 1.0e7):
            for mode in ("shot_noise_only", "empirical_total_rms"):
                var = assemble_variance(F_SRC_ADU, p, float(sky), mode=mode)
                # 电子域入射率 λ_e = Var_ADU² · g²（ADU↔电子的标度），
                # 泊松的 Var[N] = λ_e ⇒ 抽样后除回 g² 才与解析 ADU² 同域。
                n_e = rng.poisson((var * GAIN_E_PER_ADU ** 2)[None, :],
                                  size=(VAR_N_DRAW, VAR_N_PX)).astype(np.float64)
                meas = np.var(n_e, axis=0, ddof=1) / GAIN_E_PER_ADU ** 2
                ratio = float(np.median(meas / var))
                H.less_equal(abs(ratio - 1.0), float(vt.value),
                             f"S_sky={sky:g} / {mode}: 实测方差/解析方差"
                             f"（冻结 {vt.key} = {vt.value!r}）")
                ev.record(f"S_sky={sky:g} {mode} 中位(实测/解析)", ratio, vt.value,
                          note=f"解析中位方差 = {float(np.median(var)):.6g} ADU²")
        v_shot = float(np.median(assemble_variance(F_SRC_ADU, p, 1.0e4)))
        v_emp = float(np.median(assemble_variance(F_SRC_ADU, p, 1.0e4,
                                                  mode="empirical_total_rms")))
        H.close(v_shot - v_emp, (READ_NOISE_E / GAIN_E_PER_ADU) ** 2,
                rtol=F64_RTOL, atol=tol.ulp((READ_NOISE_E / GAIN_E_PER_ADU) ** 2),
                scale=(READ_NOISE_E / GAIN_E_PER_ADU) ** 2,
                what="两模式之差必须在 f64 非归约档内等于 (RN/g)²（正本 §3.3:293 的双重计数量）")
        ev.record("(RN/g)²（ADU²）", (READ_NOISE_E / GAIN_E_PER_ADU) ** 2,
                  note="实测差 = " + f"{v_shot - v_emp!r}")


@H.test(
    "p2-d-m5-closed-form",
    intent="NOISE_SNR.md §3.3(:311,:314) 的 `m_5` 闭式 "
           "`m_5 = ZP_k - 2.5 * log10( 5 * sigma_F^{frame}(ref) )` 的**往返一致性**："
           "由 `m_5` 反解出的通量代回 SNR 必须回到 5（正本 :314 逐字「`m_5` 由信噪比"
           "定义与星等定义直接导出：令 `SNR = 5` 解出 `F` 再换算星等」）。"
           "跨 6 档 `k_photo` 逐点判（`ZP_k` 逐帧不同 ⇒ 不能只判一档）。",
    inputs="`m_ref = 18`、`ZP_syn = 26.4899`、`k_photo ∈ {0.8, 1.0, 1.35, 1.7, 2.1, 2.8}`、"
           "`S_sky = 1e4 ADU/px`、`RN = 25 e⁻`、`g = 1.5`",
    expected="逐档 `|SNR(m_5) − 5| / 5 ≤ 1e-12`；并给出 `ZP_k` 与 `σ_F^{frame}` 的逐档读数",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:264,:311,:314) 逐字"
           "（`ZP_k = ZP_syn - 2.5*log10(k_photo,k)`、`m_5` 式、系数 2.5 的三处同源要求）；"
           "冻结容差 chain.a.p2.m5_roundtrip_rel",
    criteria=["P2-d"],
)
def p2_d_m5_closed_form():
    with H.evidence() as ev:
        c = A.get("chain.a.p2.m5_roundtrip_rel")
        p = gauss_profile(N_PX, FWHM_PX)
        for k in K_PHOTO_SET:
            var = frame_variance_at_k(F_SRC_ADU, p, 1.0e4, k)
            sig = sigma_F_frame(p, var)
            back = snr_from_m5(m5(k, sig), k, sig)
            H.less_equal(abs(back - 5.0) / 5.0, float(c.value),
                         f"k_photo={k}: m_5 往返（冻结 {c.key} = {c.value!r}）")
            ev.record(f"k_photo={k} m_5 (mag)", m5(k, sig),
                      note=f"ZP_k = {zp_frame(k):.6f}；σ_F^{{frame}} = {sig:.6f} ADU")
            ev.record(f"k_photo={k} SNR(m_5)", back, 5.0)


@H.test(
    "p2-e-control-point-variance",
    intent="NOISE_SNR.md §3.5(:412-417) 的控制点方差闭式 `Var(median) = pi*sigma^2/(2N)`"
           "的**定种子蒙特卡洛**实测。⚠ 正本 PHASE2_SAMPLER.md:608 F3 逐字记"
           "「(a) 恒真、(b) 与 MC 实测比对」⇒ 只能走 MC 那支。判别力来自**与朴素替代 "
           "`1/N` 的分离**（朴素值是闭式的 0.6366 倍，距冻结半宽 4.5 倍）。",
    inputs="N = 64 个高斯样本 × R = 200 次定种子重复；统计量 = `mean_R(median²)/σ²`",
    expected="统计量与闭式 `π/(2N)` 的相对偏差 ≤ 2.0e-1（冻结半宽）；"
             "同时断言朴素替代 `1/N` 落在门限之外（判别力的自证）",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.5(:412,:415-417) 逐字"
           "（`Var(median) = 1/(4 N f(m)^2)` → 高斯边际 `= pi*sigma^2/(2N)`、"
           "`control_variance = k_corr*(pi/2)*sigma_bg^2/N_retained`、`k_corr = 1` ⇔ 忽略相关）；"
           "统计口径见骨架表 synth.mc.method（分位数法）与 synth.mc.replicates（R = 200）；"
           "冻结容差 chain.a.p2.control_median_var_rel",
    criteria=["P2-e"],
)
def p2_e_control_point_variance():
    with H.evidence() as ev:
        c = A.get("chain.a.p2.control_median_var_rel")
        n_retained = 64
        rng = _kit.make_rng(20260906)
        med = np.median(rng.normal(0.0, 1.0, size=(R_REPLICATES, n_retained)), axis=1)
        stat = float(np.mean(med ** 2))
        closed = math.pi / (2.0 * n_retained)
        naive = 1.0 / n_retained
        H.less_equal(abs(stat / closed - 1.0), float(c.value),
                     f"Var(median) 实测 vs 闭式 π/(2N)（冻结 {c.key} = {c.value!r}）")
        H.is_true(abs(naive / closed - 1.0) > float(c.value),
                  "朴素替代 1/N 必须落在冻结半宽之外（否则这条判据没有判别力）")
        ev.record("统计量 mean_R(median²)/σ²", stat)
        ev.record("闭式 π/(2N)", closed, c.value, note="N = 64")
        ev.record("统计量 / 闭式 − 1", stat / closed - 1.0, c.value,
                  note=f"冻结半宽 = Z95·√(2/R) = {Z95 * math.sqrt(2.0 / R_REPLICATES):.4f}")
        ev.record("朴素替代 1/N / 闭式 − 1", naive / closed - 1.0, c.value,
                  note=f"门限外 {abs(naive / closed - 1.0) / float(c.value):.2f} 倍 ⇒ 有判别力")
        ev.record("单次重复 median² 的 q975", float(np.quantile(med ** 2, 0.975)),
                  note="q025 = " + f"{float(np.quantile(med ** 2, 0.025)):.4e}"
                       "（1 自由度统计量的散布天然宽，这是正确的）")


@H.test(
    "p2-g-cross-frame-comparability",
    intent="NOISE_SNR.md §3.4(:361) 的跨帧可比性硬约束："
           "①天光受限臂内 `w = SNR²/F0²` 在同一 `m_ref` 档的参考电平扫掠下漂移 ≤ 2e-3"
           "（正本逐字「典型 `1e-4`–`1e-3` 量级，**仍是物理量而非数值零**」⇒ 判**上界**"
           "不判零）；②源主导臂的同一条读数**越出**该上界 ⇒ 证明两臂不可混比。",
    inputs="天光固定 4.4e6 ADU/px；参考电平扫掠 11 档："
           "天光受限臂 1e3–1e4 ADU（`F·P/g / S_sky ≤ 5e-4`）、源主导臂 1e6–1e7 ADU；"
           "`m_ref = 18`、`F0 = 10^{−0.4(m_ref−ZP_syn)}`",
    expected="①天光受限臂 `max|w(g)/w(0) − 1| ≤ 2e-3`（实测落在正本逐字的 1e-4–1e-3 档）；"
             "②源主导臂同一条读数越出 2e-3（否则「不可混比」这条判据恒绿）",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.4(:333,:361) 逐字"
           "（`w = SNR^2/F0^2`、跨帧可比性硬约束、漂移的典型量级与「是物理量而非数值零」、"
           "「在天光受限臂上该源项可忽略……在源主导臂上则随参考电平单调变化」）；"
           "冻结容差 chain.a.p2.sky_limited_drift_rel",
    criteria=["P2-g"],
)
def p2_g_cross_frame_comparability():
    with H.evidence() as ev:
        c = A.get("chain.a.p2.sky_limited_drift_rel")
        f0 = f0_anchor()
        p = gauss_profile(N_PX, FWHM_PX)

        # 漂移的定义（NOISE_SNR.md §3.4:361）：**参考电平被误判**时权重的相对变化。
        # `w = SNR²/F0² = Σ_i P_i²/σ_i²/k²`（:333 逐字）随 `σ_i² = B + F·P_i/g` 里的源散粒项
        # 变化；天光受限臂该变化被 `F/S_sky` 压低，源主导臂则单调变大。
        def w_arm(f_assumed: float) -> float:
            return frame_weight(p, assemble_variance(f_assumed, p, ARM_SKY_ADU), 1.0)

        f_true = 1.0e4
        grid = np.logspace(3.0, 5.0, ARM_STEPS)
        w_sky = np.array([w_arm(float(g)) for g in grid])
        grid_src = np.logspace(6.0, 8.0, ARM_STEPS)
        w_src = np.array([w_arm(float(g)) for g in grid_src])
        f_true_src = 1.0e7
        d_sky = float(np.max(np.abs(w_sky / w_arm(f_true) - 1.0)))
        d_src = float(np.max(np.abs(w_src / w_arm(f_true_src) - 1.0)))
        H.less_equal(d_sky, float(c.value),
                     f"天光受限臂的权重漂移（冻结 {c.key} = {c.value!r}）")
        _exceeds(d_src, float(c.value), "源主导臂的权重漂移",
                 "chain.a.p2.sky_limited_drift_rel")
        ev.record("天光受限臂 max|w/w₀ − 1|", d_sky, c.value,
                  note="正本 :361 逐字「典型 1e-4–1e-3 量级，仍是物理量而非数值零」")
        ev.record("源主导臂 max|w/w₀ − 1|", d_src, c.value,
                  note=f"超界 {d_src / float(c.value):.4g}× ⇒ 两臂不可混比")
        ev.record("天光受限臂 参考电平扫掠 (ADU)", float(grid[0]),
                  note=f"扫到 {grid[-1]:.3g}；真值 {f_true:.3g}；"
                       f"无量纲比 F·P_max/(g·S_sky) = "
                       f"{f_true * float(np.max(p)) / GAIN_E_PER_ADU / ARM_SKY_ADU:.3e}")
        ev.record("源主导臂 参考电平扫掠 (ADU)", float(grid_src[0]),
                  note=f"扫到 {grid_src[-1]:.3g}；真值 {f_true_src:.3g}；"
                       f"无量纲比 = "
                       f"{f_true_src * float(np.max(p)) / GAIN_E_PER_ADU / ARM_SKY_ADU:.3e}")


# ---------------------------------------------------------------------------
# §4 负例
# ---------------------------------------------------------------------------

@H.test(
    "p2-neg-sky-in-numerator",
    intent="负例：把天光**计入信噪比分子**（`SNR_bad = (S_src + N_px·S_sky)/σ_F`），"
           "这是 NOISE_SNR.md §3.3(:306) 逐字点名的「与信噪比的定义相反」的形态、也是 "
           "`04_SCIENCE_EVIDENCE_AND_EXPERIMENT.md` §2.2 逐字禁止的「天光水平当成分子」。"
           "断言独立判据 `chain.a.p2.sky_monotonic_margin`（严格递减，精确档）"
           "在缺陷侧给超界读数。",
    inputs="同一批 21 档天光；`F_src = 1e4 ADU` 固定；缺陷 = 分子加 `N_px·S_sky`",
    expected="正确实现：零单调性违例；缺陷实现：20 个相邻档全部违例（SNR **随天光上升**）",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:306) 逐字「**分子只有源**："
           "天光只经散粒噪声进入分母。把天光电平或观测电平计入分子，就得到一个"
           "**随天光上升**的量，与信噪比的定义相反」；上游 04 §2.2 逐字「天光对信噪比的影响"
           "通过散粒噪声体现」；冻结容差 chain.a.p2.sky_monotonic_margin（精确档 0.0）",
    criteria=["P2-b"],
    kind=H.NEGATIVE,
    inject="把天光计入信噪比分子：`SNR_bad = (S_src + N_px·S_sky)/σ_F`（天光进分子）",
    defect_id="P2-NEG-SKY-IN-NUMERATOR",
)
def p2_neg_sky_in_numerator():
    with H.evidence() as ev:
        p = gauss_profile(N_PX, FWHM_PX)
        sky = sky_sweep()
        var = [assemble_variance(F_SRC_ADU, p, float(s)) for s in sky]
        good = np.array([snr_frame(F_SRC_ADU, sigma_F_frame(p, v)) for v in var])
        bad = np.array([(F_SRC_ADU + N_PX * float(s)) / sigma_F_frame(p, v)
                        for s, v in zip(sky, var)])
        n_good = int(np.sum(np.diff(good) >= 0.0))
        n_bad = int(np.sum(np.diff(bad) >= 0.0))
        H.exact(n_good, 0, "未注入时单调性判据必须绿（零违例）")
        _exceeds(float(n_bad), 0.0, "注入「天光进分子」后的单调性违例点数",
                 "chain.a.p2.sky_monotonic_margin")
        ev.record("注入前单调性违例点数（正确，绿）", float(n_good), 0.0)
        ev.record("注入后单调性违例点数（缺陷，红）", float(n_bad), 0.0,
                  note=f"共 {sky.size - 1} 个相邻档，全部违例 ⇒ SNR 随天光上升")
        ev.record("注入前 SNR 最小档 / 最大档", float(good[0] / good[-1]),
                  note="固定源通量 ⇒ SNR 随天光**下降**")
        ev.record("注入后 SNR 最小档 / 最大档", float(bad[0] / bad[-1]),
                  note="> 1 即「随天光上升」，与信噪比定义相反")


#: 读噪主导子域的天光档（ADU/px）。⚠ **为什么不是 `p2-a` 用的 1e4**：
#: `(RN/g)²` 在解析方差里的占比随天光**下降**；在 1e4 ADU/px 档上它只占 2.8%，
#: 双重计数的 σ 相对高估被 P² 加权稀释到 1.1% —— 门限 1e-1 根本走不到。
#: 本负例取**读噪 + 暗流主导**的子域（S_sky = 1e2 ADU/px ⇒ 占比 ≈ 70%），
#: 这也正是双重计数危害最大的域（正本 :293「会高估通量不确定度」在该域最重）。
VAR_SKY_ADU_DOUBLE_COUNT = 1.0e2


@H.test(
    "p2-neg-variance-double-count",
    intent="负例：把「已含读噪的**经验总均方根**」**再叠加** `(RN/g)²`"
           "（NOISE_SNR.md §3.3(:291-293) 逐字点名的「双重计数，会高估通量不确定度」）。"
           "⚠ **两侧读数都来自定种子蒙特卡洛实测**：缺陷侧的被比较量是"
           "「MC 实测的经验总方差」与「该实测值再加一次 `(RN/g)²`」之比，"
           "**不是**由夹具字面量算出的解析量（对抗复核判定的缺陷 4：旧版 `over` 完全由 "
           "`S_sky`/`RN`/`g` 算出，函数里的 MC 只用来守绿臂 ⇒ 负例钉在夹具选值上、不钉在实现上）。"
           "判据 = 逐像素方差的相对高估越出冻结下界 `chain.a.p2.var_double_count_min`。",
    inputs="`F_src = 1e4 ADU`、**`S_sky = 1e2 ADU/px`（读噪 + 暗流主导子域，见上方常量说明）**、"
           "`RN = 25 e⁻`、`g = 1.5`、`DARK = 20 ADU/px`；400 像元 × 64 次定种子采样，"
           "采样在**电子域**做：`n_e = Poisson(λ_i·g²) + N(0, RN²)`、"
           "`λ_i = S_sky + DARK + F_src·P_i`；"
           "缺陷 = 把 MC 实测的 `Var(n_e)/g²`（已含读噪）再加 `(RN/g)²`",
    expected="①（绿臂）`median(实测经验总方差 / 解析总方差)` 落在 `±20%` 内"
             "（`synth.mc.var_ratio_ci95`）⇒ 实测确实代表「已含读噪」的经验总均方根；"
             "②（缺陷臂，`over`）`median(缺陷方差/实测方差 − 1)` 越出冻结下界 1e-1"
             "（读数 ≈ 0.71 ⇒ 余量 ≈ 7×）；"
             "③（支撑读数，只落盘）由同一份实测导出的 `σ_F` 相对高估与 `W` 相对亏损",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:285,:290-293) 逐字"
           "（`sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F * P_i / g`；`empirical_total_rms`"
           "「是经验总均方根、已含读噪噪声，此时**不再叠加** `(RN/g)^2`」；"
           "「把含读噪的经验总均方根填进散粒项、又在增益可用时叠加读噪项，是**双重计数**，"
           "会高估通量不确定度」）；统计口径：分位数法 `synth.mc.method`、"
           "`synth.mc.var_ratio_ci95`（绿臂）、`synth.mc.replicates`；"
           "冻结容差 chain.a.p2.var_double_count_min",
    criteria=["P2-c"],
    kind=H.NEGATIVE,
    inject="把「已含读噪的经验总均方根」当作散粒项，再在增益可用时**叠加一次** `(RN/g)²`"
           "（双重计数）",
    defect_id="P2-NEG-VAR-DOUBLE-COUNT",
)
def p2_neg_variance_double_count():
    with H.evidence() as ev:
        c = A.get("chain.a.p2.var_double_count_min")
        p = gauss_profile(VAR_N_PX, FWHM_PX)
        rn2_adu = (READ_NOISE_E / GAIN_E_PER_ADU) ** 2
        # 电子域采样：泊松（光子）+ 读噪高斯。**这条采样就是「经验总均方根」的真实来源**，
        # 它在物理上已经含读噪 ⇒ 后续任何再加一次 `(RN/g)²` 的实现都是双重计数。
        rng = _kit.make_rng(4242)
        lam_adu = (VAR_SKY_ADU_DOUBLE_COUNT + DARK_ADU
                   + F_SRC_ADU * p)                     # 每像元平均入射（ADU）
        n_e = (rng.poisson(lam_adu[None, :] * GAIN_E_PER_ADU ** 2,
                           size=(VAR_N_DRAW, VAR_N_PX))
               + rng.normal(0.0, READ_NOISE_E, size=(VAR_N_DRAW, VAR_N_PX)))
        var_emp = np.var(n_e, axis=0, ddof=1) / GAIN_E_PER_ADU ** 2   # **实测**经验总方差
        var_analytic_total = lam_adu + rn2_adu                        # 解析总方差（含读噪）

        # ---- 绿臂：实测确实等于「已含读噪」的经验总均方根 --------------------------
        r_ok = float(np.median(var_emp / var_analytic_total))
        H.less_equal(abs(r_ok - 1.0), float(tol.get("synth.mc.var_ratio_ci95").value),
                     "实测经验总方差 / 解析总方差 必须落在统计散布内"
                     "（否则「var_emp 已含读噪」这个前提不成立）")
        # ---- 缺陷臂：两侧读数都来自同一份**实测** ---------------------------------
        var_bad = var_emp + rn2_adu                   # 缺陷实现：再加一次 (RN/g)²
        over = float(np.median(var_bad / var_emp - 1.0))
        _exceeds(over, float(c.value), "双重计数造成的逐像素方差相对高估（实测基准）",
                 "chain.a.p2.var_double_count_min")
        # 支撑读数：由同一份实测导出的 σ_F 高估与 W 亏损（只落盘，不作门限）
        sf_ok = sigma_F_frame(p, var_emp)
        sf_bad = sigma_F_frame(p, var_bad)
        sigma_over = float(sf_bad / sf_ok - 1.0)
        w_ok = float(np.sum(p ** 2 / var_emp))
        w_bad = float(np.sum(p ** 2 / var_bad))
        w_drop = float(w_ok / w_bad - 1.0)
        share_meas = float(np.median(rn2_adu / var_emp))

        ev.record("①绿臂 中位(实测经验总方差 / 解析总方差)", r_ok,
                  tol.get("synth.mc.var_ratio_ci95").value,
                  note=f"S_sky = {VAR_SKY_ADU_DOUBLE_COUNT:.0e} ADU/px；"
                       f"{VAR_N_DRAW}×{VAR_N_PX} 定种子采样；"
                       "解析总方差含 `(RN/g)²`，实测值来自电子域 Poisson+高斯采样")
        ev.record("②缺陷臂 中位(缺陷方差/实测方差 − 1)", over, float(c.value),
                  note=f"**判别力读数**：超界 {over / float(c.value):.4g}×；"
                       "两侧（缺陷/实测）都来自同一份 MC 实测，"
                       "不再是由 `S_sky`/`RN`/`g` 字面量算出的解析量")
        ev.record("②支撑读数 (RN/g)² / 实测经验总方差（中位）", share_meas,
                  note=f"理论 = (RN/g)²/median(λ+(RN/g)²) = "
                       f"{rn2_adu / float(np.median(var_analytic_total)):.4f}；"
                       "两臂之差只来自实测散度的蒙特卡洛噪声")
        ev.record("③支撑读数 σ_F 相对高估（不由同一份实测导出）", sigma_over, None, "",
                  note="**不作门限**：`σ_F = (ΣP²/σ_i²)^{1/2}` 按 P² 加权，"
                       "中心像元的源项把读噪占比压低 ⇒ σ 层面的高估被稀释到门限以下。"
                       "正本 :293 的「高估通量不确定度」的直接后果在**方差面**上，"
                       "故门限挂在方差上")
        ev.record("③支撑读数 W = ΣP²/σ_i² 的相对亏损", w_drop, None, "",
                  note="同一份实测；`Var(F̂) = 1/Σ_k W_k` ⇒ W 亏损等效于通量方差高估")
        ev.record("λ（每像元平均入射）范围 [min, max]", float(lam_adu.min()),
                  note=f"max = {lam_adu.max():.4f} ADU/px；`(RN/g)²` = {rn2_adu:.4f} ADU²")



@H.test(
    "p2-neg-wrong-normalization",
    intent="负例：用**逐帧 `F_ref,k`** 而非公共锚 `F0` 归一（`w = SNR²/F_ref,k²`）。"
           "这是 NOISE_SNR.md §3.4(:336-341) 逐字点名的「唯一真实的失效面」："
           "「逐帧被该帧的测光标度**二次缩放**，`sum_k` 不再等于 `1/Var(F_hat)`」。"
           "断言正本指定的**点源信息量**判据在缺陷侧给超界读数。",
    inputs="`k_photo ∈ {0.8, 1.0, 1.35, 1.7, 2.1, 2.8}`（3.5 倍跨度），其余逐帧物理量相同",
    expected="公共锚归一：`Σ_k SNR_k²/F0² / Σ_k W_k = 1` 逐位；"
             "错归一：`(Σ_k SNR_k²/F_ref,k²)/Σ_k W_k = Σ_k W_k k_photo,k² / Σ_k W_k` "
             "越出冻结下界 10",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.4(:333,:336-341,:348,:359) 逐字"
           "（`w = SNR^2/F0^2 = 1/sigma_F^{sys,2}`；「归一化因子必须取公共锚 `F0`，"
           "不能取该帧的 `F_ref,k`」；「承载证据的判据是点源信息量」）；"
           "冻结容差 chain.a.p2.wrong_norm_min",
    criteria=["P2-f"],
    kind=H.NEGATIVE,
    inject="把归一化因子从公共锚 `F0` 换成逐帧 `F_ref,k`（`w = SNR²/F_ref,k²`）",
    defect_id="P2-NEG-WRONG-NORMALIZATION",
)
def p2_neg_wrong_normalization():
    with H.evidence() as ev:
        f0 = f0_anchor()
        p = gauss_profile(N_PX, FWHM_PX)
        sky = 1.0e4
        frames = [(p, frame_variance_at_k(F_SRC_ADU, p, sky, k), k) for k in K_PHOTO_SET]
        ws = [frame_weight(pp, vv, k) for pp, vv, k in frames]
        sum_w = sum(ws)
        good = sum(snr_frame(f_ref(k, f0), sigma_F_frame(pp, vv)) ** 2 / f0 ** 2
                   for pp, vv, k in frames)
        bad = sum(snr_frame(f_ref(k, f0), sigma_F_frame(pp, vv)) ** 2 / f_ref(k, f0) ** 2
                  for pp, vv, k in frames)
        good_ratio = good / sum_w
        bad_ratio = bad / sum_w
        H.close(good_ratio, 1.0, rtol=F64_RTOL, atol=tol.ulp(1.0), scale=1.0,
                what="公共锚归一下 `Σ_k SNR_k²/F0²` 必须等于 `Σ_k W_k`（负例的绿读数）")
        _exceeds(bad_ratio, float(A.get("chain.a.p2.wrong_norm_min").value),
                 "逐帧 F_ref 归一下的比值", "chain.a.p2.wrong_norm_min")
        ev.record("注入前 (Σ SNR²/F0²)/Σ_k W_k", good_ratio, 1.0)
        ev.record("注入后 (Σ SNR²/F_ref²)/Σ_k W_k", bad_ratio,
                  A.get("chain.a.p2.wrong_norm_min").value,
                  note=f"超界 {bad_ratio / float(A.get('chain.a.p2.wrong_norm_min').value):.4g}×；"
                       f"解析值 = Σ W_k k_photo,k² / Σ W_k = "
                       f"{sum(w * k ** 2 for w, k in zip(ws, K_PHOTO_SET)) / sum_w:.6f}")


@H.test(
    "p2-neg-m5-wrong-domain",
    intent="负例：`m_5` 里把 `σ_F^{frame}` 误取成统一测光坐标系的 `σ_F^{sys,k}`"
           "（NOISE_SNR.md §3.3(:273-274) 逐字把两者**分名**，:311 写的是 `σ_F^{frame}`）。"
           "断言独立判据 `chain.a.p2.m5_wrong_domain_min` 在缺陷侧给超界读数"
           "（判**最深的一档** `k_photo = 2.8`：偏差 `= |k_photo − 1|` 随 k 单调）。",
    inputs="`k_photo ∈ {0.8, 1.0, 1.35, 1.7, 2.1, 2.8}`；缺陷 = `σ_F` 漏乘回 `k_photo,k`",
    expected="正确写法：逐档 `|SNR(m_5) − 5|/5 ≤ 1e-12`；错域：`|SNR(m_5) − 5|/5 = "
             "`|k_photo − 1|`，最深档越出 1.5",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:270,:273-274,:311) 逐字"
           "（`sigma_F^{frame,k}`（ADU）与 `sigma_F^{sys,k} = k_photo,k * sigma_F^{frame,k}` "
           "**分名**；`m_5` 写的是 `sigma_F^{frame}(ref)`）；"
           "冻结容差 chain.a.p2.m5_wrong_domain_min",
    criteria=["P2-d"],
    kind=H.NEGATIVE,
    inject="`m_5` 的 `σ_F` 误取 `σ_F^{sys,k}`（未乘回 `k_photo,k`）⇒ 深度被系统性偏深",
    defect_id="P2-NEG-M5-WRONG-DOMAIN",
)
def p2_neg_m5_wrong_domain():
    with H.evidence() as ev:
        p = gauss_profile(N_PX, FWHM_PX)
        sky = 1.0e4
        devs = []
        for k in K_PHOTO_SET:
            sig = sigma_F_frame(p, frame_variance_at_k(F_SRC_ADU, p, sky, k))
            good = snr_from_m5(m5(k, sig), k, sig)
            bad = snr_from_m5(m5(k, sig * k), k, sig)
            H.less_equal(abs(good - 5.0) / 5.0,
                         float(A.get("chain.a.p2.m5_roundtrip_rel").value),
                         f"k_photo={k}: 未注入时 m_5 往返必须回到 5（负例的绿读数）")
            dev = abs(bad - 5.0) / 5.0
            devs.append(dev)
            ev.record(f"k_photo={k} 注入前 |SNR−5|/5", abs(good - 5.0) / 5.0,
                      note="正确写法")
            ev.record(f"k_photo={k} 注入后 |SNR−5|/5", dev,
                      note=f"解析预期 = |{k} − 1| = {abs(k - 1.0):.6f}")
        _exceeds(max(devs), float(A.get("chain.a.p2.m5_wrong_domain_min").value),
                 "最深一档（k_photo = 2.8）的错域偏差",
                 "chain.a.p2.m5_wrong_domain_min")


@H.test(
    "p2-neg-kphoto-as-forward-factor",
    intent="负例：把 `k_photo,k` 当作**前向亮度因子**（在帧面口径上又乘了一次 `k_photo`）。"
           "NOISE_SNR.md §3.3(:278) 逐字点名「任何把 `k_photo,k` 直接当作前向亮度因子的写法"
           "**方向相反**」。断言跨帧 SNR 散布判据（骨架表 "
           "`synth.p2.cross_frame_snr_spread_rel`）在缺陷侧给超界读数。",
    inputs="`k_photo ∈ {0.8, 1.0, 1.35, 1.7, 2.1, 2.8}`；每帧 ADU 读数与噪声同步缩放；"
           "缺陷 = `SNR_bad,k = F_ref,k·k_photo,k / σ_F^{frame,k}`",
    expected="正确：跨帧 SNR 相对散布 ≤ 1e-12（整除消去）；缺陷：`SNR_bad ∝ k_photo`，"
             "散布 `= std(k)/mean(k) ≈ 0.37`，越出冻结门限 0.1",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.3(:267,:273-278) 逐字"
           "（`F_ref,k = F0/k_photo,k`、`σ_F^{sys,k} = k_photo,k·σ_F^{frame,k}`、"
           "「整除消去」、「任何把 `k_photo,k` 直接当作前向亮度因子的写法方向相反」）；"
           "跨帧散布门限取骨架表 synth.p2.cross_frame_snr_spread_rel（量级冻结的占位）",
    criteria=["P2-a"],
    kind=H.NEGATIVE,
    inject="把 `k_photo,k` 当作前向亮度因子：`SNR_bad,k = F_ref,k·k_photo,k/σ_F^{frame,k}`",
    defect_id="P2-NEG-KPHOTO-AS-FORWARD-FACTOR",
)
def p2_neg_kphoto_as_forward_factor():
    with H.evidence() as ev:
        f0 = f0_anchor()
        p = gauss_profile(N_PX, FWHM_PX)
        sky = 1.0e4
        spread = float(tol.get("synth.p2.cross_frame_snr_spread_rel").value)
        good, bad = [], []
        for k in K_PHOTO_SET:
            sig = sigma_F_frame(p, frame_variance_at_k(F_SRC_ADU, p, sky, k))
            good.append(snr_frame(f_ref(k, f0), sig))
            bad.append(f_ref(k, f0) * k / sig)
        s_good = float(np.std(good) / np.mean(good))
        s_bad = float(np.std(bad) / np.mean(bad))
        H.less_equal(s_good, F64_RTOL, "未注入时跨帧 SNR 逐帧相同（负例的绿读数）")
        _exceeds(s_bad, spread, "错向前向因子后的跨帧 SNR 散布",
                 "synth.p2.cross_frame_snr_spread_rel")
        ev.record("注入前跨帧 SNR 相对散布", s_good, F64_RTOL,
                  note=f"k_photo 跨度 {max(K_PHOTO_SET) / min(K_PHOTO_SET):.2f}× 下的读数")
        ev.record("注入后跨帧 SNR 相对散布", s_bad, spread,
                  note=f"超界 {s_bad / spread:.4g}×；理论值 = std(k)/mean(k) = "
                       f"{float(np.std(np.array(K_PHOTO_SET)) / np.mean(K_PHOTO_SET)):.6f}")


@H.test(
    "p2-neg-mixed-arms",
    intent="负例：把**天光受限臂**与**源主导臂**混在同一 `m_ref` 档里求 "
           "`w = SNR²/F0²` 的散布。NOISE_SNR.md §3.4(:361) 逐字"
           "「跨帧比对必须限定在同一参考星等档 `m_ref` 内，**天光受限档与源主导档不可混比**」。"
           "断言跨帧散布门限在缺陷侧给超界读数。",
    inputs="天光固定 4.4e6 ADU/px；每臂 11 档："
           "天光受限臂参考电平 1e3–1e4 ADU、源主导臂 1e6–1e7 ADU",
    expected="单臂散布落在 `synth.p2.cross_frame_snr_spread_rel` 内（负例的绿读数）；"
             "混比散布越出该门限的 3 倍",
    source="正本条款 docs/science/noise_snr/NOISE_SNR.md §3.4(:361) 逐字（跨帧可比性的硬约束）；"
           "冻结容差 chain.a.p2.mixed_arm_min（引用骨架表 synth.p2.cross_frame_snr_spread_rel）",
    criteria=["P2-g"],
    kind=H.NEGATIVE,
    inject="把天光受限臂与源主导臂混在同一 `m_ref` 档里做跨帧统计",
    defect_id="P2-NEG-MIXED-ARMS",
)
def p2_neg_mixed_arms():
    with H.evidence() as ev:
        f0 = f0_anchor()
        p = gauss_profile(N_PX, FWHM_PX)
        spread = float(tol.get("synth.p2.cross_frame_snr_spread_rel").value)
        c = A.get("chain.a.p2.mixed_arm_min")

        def w_arm(f_assumed: float, sky: float) -> float:
            return frame_weight(p, assemble_variance(f_assumed, p, sky), 1.0)

        # 臂 A（天光受限）：sky = 1e8，参考电平在 ±1 dex 内扫。
        a = np.array([w_arm(float(g), ARM_SKY_ADU)
                      for g in np.logspace(3.0, 5.0, ARM_STEPS)])
        # 臂 B（源主导）：sky = 1e4、参考电平固定在 1e5 ⇒ 权重整体落在另一个**水平**上。
        b = np.array([w_arm(1.0e5, 1.0e4) for _ in range(ARM_STEPS)])
        single = [float(np.std(a) / np.mean(a)), float(np.std(b) / np.mean(b))]
        cat = np.concatenate([a, b])
        mixed = float(np.std(cat) / np.mean(cat))
        for i, v in enumerate(single):
            H.less_equal(v, spread, f"单臂 {i} 的权重散布必须过门限（负例的绿读数）")
        _exceeds(mixed, float(c.value), "混比后的权重散布", "chain.a.p2.mixed_arm_min")
        ev.record("天光受限臂权重散布", single[0], spread)
        ev.record("源主导臂权重散布", single[1], spread)
        ev.record("混比后权重散布（缺陷，红）", mixed, c.value,
                  note=f"超界 {mixed / float(c.value):.4g}×；两臂权重水平之比 = "
                       f"{float(np.mean(b) / np.mean(a)):.4g}"
                       "（同一 m_ref 档内两臂权重不在同一水平 ⇒ 混比无意义）")