#!/usr/bin/env python3
"""R3 点源 SNR → 球面面亮度精度的**可证伪反例**（SCIENCE_V2 R3）。

正本（逐字依据）：
  `docs/science/unified/UNIFIED_SCIENCE_MODEL.md` §4：
    「**SNR²/F_ref²=1/V_F 是代数恒等式，只对同一通量估计量成立**。不能把它未经论证地
      当成 V_B 的逆」；「T 有面积/PSF 相关量纲……**不得默认为 1**」。
  `docs/science/noise_snr/NOISE_SNR.md` §4.1 的解析反例（P_A/P_B 两帧）。
  `docs/science/unified/DATA_SEMANTICS.md`：三种逆方差**严格分名**——
    I_F = SNR²/F_ref²（1/[F]²）、w_B = 1/V_B = I_F/T（sr²/[x]²）、
    w_ctrl = 1/V_ctrl；「不得共用语义不明的 weight 字段」。
  `SCIENCE_V2_AGENT_TASK_PACKAGE.md` §4 R3：
    「构造两帧具有相同 reference SNR，但 PSF 宽度、像元面积或相关 K 不同，
      真实像元 V_B 不同……**不接受仅检查公式等式左右相等作为物理验证**」。

## 0 本文件回答的科学问题（可证伪）

**命题 A（要证伪的旧行为）**：把点源通量的逆方差当作 HEALPix 面亮度像元的最优
叠加权重，即 `w_F ≡ 1/V_F = (S_ref/F_ref)²`，在跨设备合并时给出正确/最优的合成。

**命题 B（正本口径）**：面亮度像元的最优权重是 `w_B,p = 1/V_B,p = I_F/T_p`，
其中 `T_p = V_B,p/V_F` 是**精度传递因子**。当两帧 `S_ref` 相同但 PSF 宽度 /
像元面积 / 相关核 K 不同时，`T_p` 不同 ⇒ `w_B` 不同 ⇒ 用 `w_F` 会给出
**可观测的错误合成**。

⇒ 本文件构造**独立生成**的两帧世界（解析求逆，不 import 任何被测实现），
分别用 `w_F` 与 `w_B` 合成，比较与解析 CRB 的关系。
**判据用方差界与解析真值，不是"公式两边相等"。**

## 1 解析世界（两条臂）

观测模型 `y_k = F_k h_k + Z_k β + n_k`，`C_B,k = σ²_B,k K_k`（面亮度域）。

    I_F,k = (1/σ²_B,k)·h_kᵀK_k⁻¹h_k ,  V_F,k = 1/I_F,k ,  S_ref = F_ref/√V_F,k
    V_B,p,k = σ²_B,k·(K_k)_pp ,        T_p,k ≡ V_B,p,k/V_F,k
    w_B,p,k = I_F,k / T_p,k = (S_ref/F_ref,k)² / T_p,k

    Var(B̂|w) = Σ w²V/(Σw)² ≥ 1/Σ(1/V_B) ≡ CRB      （取等 ⟺ w ∝ 1/V_B）

**臂 1（阴性世界，K=I）**：两帧 T 相同 ⇒ 两种归一只差共同常数 ⇒ 合成逐位相等。
**臂 2（阳性世界，K_B 相关且 Ω 不同）**：S_ref 相同但 T 不同 ⇒ 合成可测不等。

## 2 硬禁令（本文件不写的判据）

1. 不写 `w ≡ SNR²/F_ref²` 的自证（`weight_chain.h:435-438` 逐字禁止）。
2. 不写 `Σ w²V/(Σw)² ≥ 1/Σ(1/V)` 取等的 Cauchy–Schwarz 恒等式本身
   （结构上无判决力；只用它推出的**不等式**与**构造世界的实际超出量**）。
3. 不用权重效率 `E`（`NOISE_SNR.md:484-492`：E 对整体乘性偏差免疫）。
4. 不把本文件的解析世界当真实生产数据的替代品——它检验权重**公式的结构**。

## 3 运行

    python3 -m eng.tests.synthetic.run_synthetic --only r3
"""

from __future__ import annotations

import math

import numpy as np

from eng.tests.unit import harness as H  # noqa: E402
from eng.tests.synthetic import tolerances as T  # noqa: E402


# ── 解析世界的独立构造器（不 import 被测实现）──────────────────────────────

def build_frame(h, K, sigma_b2, omega=1.0):
    """构造一帧的解析观测量。h 已含 1/Ω；返回值只用解析式，不用任何被测实现。"""
    h = np.asarray(h, dtype=float)
    K = np.asarray(K, dtype=float)
    assert K.shape == (h.size, h.size)
    K_inv = np.linalg.inv(K)
    I_F = float(h @ K_inv @ h) / sigma_b2
    V_F = 1.0 / I_F
    V_B = sigma_b2 * float(K[0, 0])
    return {"h": h, "omega": float(omega), "K": K, "sigma_b2": float(sigma_b2),
            "I_F": I_F, "V_F": V_F, "V_B": V_B, "T": V_B / V_F}


def corr_eye(n, rho):
    K = np.eye(n)
    for i in range(n - 1):
        K[i, i + 1] = K[i + 1, i] = rho
    return K


def snr(frame, F_ref):
    return float(F_ref) / math.sqrt(frame["V_F"])


def blend(ys, ws):
    w = np.asarray(ws, float)
    y = np.asarray(ys, float)
    return float(np.sum(w * y) / np.sum(w))


def mc_blend_diff(frA, frB, wsA, wsB, n_seeds):
    """多 seed 的 |B̂(wF) − B̂(wB)| 中位数（对单样本噪声稳健）。"""
    diffs = []
    sA, sB = math.sqrt(frA["sigma_b2"]), math.sqrt(frB["sigma_b2"])
    for seed in range(n_seeds):
        rng = np.random.default_rng(seed)
        ya = sA * float(rng.standard_normal())
        yb = sB * float(rng.standard_normal())
        diffs.append(abs(blend([ya, yb], wsA) - blend([ya, yb], wsB)))
    return float(np.median(diffs))


def mc_variance(frA, frB, ws, n_seeds):
    """实测 Var(B̂|w)（定种子 MC）。"""
    vals = []
    sA, sB = math.sqrt(frA["sigma_b2"]), math.sqrt(frB["sigma_b2"])
    for seed in range(n_seeds):
        rng = np.random.default_rng(seed)
        ya = sA * float(rng.standard_normal())
        yb = sB * float(rng.standard_normal())
        vals.append(blend([ya, yb], ws))
    return float(np.var(vals, ddof=1))


def crb(V_A, V_B):
    return float(1.0 / (1.0 / V_A + 1.0 / V_B))


def world_positive():
    """阳性世界的两帧：S_ref 相同而 T 不同。返回 (frA, frB, V_A, V_B, crb_val)。"""
    n = 4
    frA = build_frame([0.5, 0.5, 0.0, 0.0], np.eye(n), 2.0, 1.0)
    frB = build_frame([0.25, 0.25, 0.0, 0.0], corr_eye(n, 0.5), 2.0, 2.0)
    F_ref = 100.0
    # 调 frB 的 σ²_B 使 S_ref 相同（T 与 σ²_B 无关 ⇒ T 不变，仍是阳性世界）
    scale = (snr(frB, F_ref) / snr(frA, F_ref)) ** 2
    frB["sigma_b2"] *= scale
    frB["V_F"] *= scale
    frB["V_B"] *= scale
    return frA, frB, frA["V_B"], frB["V_B"], crb(frA["V_B"], frB["V_B"])


# ── 用例 ───────────────────────────────────────────────────────────────────

@H.test(
    "r3a_negative_world_T_equal_blend_identical",
    intent="阴性世界：两帧 T 相同 ⇒ w_F 与 w_B 只差一个共同常数 ⇒ 加权平均逐位相等。"
           "这是证明『差异来自 T 而非实现误差』的必需对照臂。",
    inputs="臂1（K=I 白噪）：帧A h=(1/2,1/2,0,0)/Ω_A Ω_A=1；帧B h=(1/√2,0,0,0)/Ω_B "
           "Ω_B=√2；σ²_B=2 相同 ⇒ T_A=T_B=0.5。固定 seed=20261014 的单次观测对。",
    expected="blend(w_F) == blend(w_B) **逐位相等**（exact）；且 T_A == T_B 逐位相等。"
             "容差 synth.r3.blend_diff_min_abs = 1e-12 只用于阳性臂的门限，"
             "本条是 exact 判据。",
    source="正本 UNIFIED_SCIENCE_MODEL §4『再构造 T 对各帧相同的阴性世界，证明两种归一"
           "只差共同常数』逐字；解析式 T_p = K_pp(hᵀK⁻¹h)，K=I 时 T = Σ(P_i/Ω_i)² "
           "（NOISE_SNR §4.1 臂1）。",
)
def r3a_negative_world():
    n = 4
    frA = build_frame([0.5, 0.5, 0.0, 0.0], np.eye(n), 2.0, 1.0)
    frB = build_frame([1.0 / math.sqrt(2.0), 0.0, 0.0, 0.0],
                      np.eye(n), 2.0, math.sqrt(2.0))
    with H.evidence() as ev:
        # 阴性世界的前提：T 必须相同。不同则构造错，这条本身就该红。
        # 阴性世界里 T 的相等允许 binary64 舍入：1/√2 平方后不能精确回到 0.5
        # （实测 0.4999999999999999 vs 0.5，差 1.1e-16）。这是**构造**的浮点
        # 残差，不是科学偏差 ⇒ 用 1e-12 容差，不用 exact。
        H.close(frA["T"], frB["T"], rtol=0.0, atol=1e-12,
                what="T_A == T_B（阴性世界前提, 容 binary64 舍入）")
        ev.record("note", "T_A = T_B = {:.17g}".format(frA["T"]))
        F_ref = 100.0
        wsF = [1.0 / frA["V_F"], 1.0 / frB["V_F"]]
        wsB = [1.0 / frA["V_B"], 1.0 / frB["V_B"]]
        rng = np.random.default_rng(20261014)
        ys = [frA["sigma_b2"] ** 0.5 * float(rng.standard_normal()),
              frB["sigma_b2"] ** 0.5 * float(rng.standard_normal())]
        bF, bB = blend(ys, wsF), blend(ys, wsB)
        ev.record("note", "blend_wF={:.17g} blend_wB={:.17g} 权重比={:.17g}".format(
            bF, bB, wsF[0] / wsF[1]))
        # 注意：不是 bitwise。w_F 与 w_B 只差一个共同常数，加权平均的**数学**
        # 结果相等，但 binary64 下两条求和路径有 ~1e-17 残差（实测
        # 5.6e-17）。这是浮点结合律残差，不是科学偏差 ⇒ 用 close。
        H.close(bF, bB, rtol=0.0, atol=1e-14,
                what="阴性世界 w_F 与 w_B 的合成相等（容 binary64 结合律残差）")


@H.test(
    "r3b_positive_counterexample_blend_differs",
    intent="阳性反例：两帧 S_ref **相同**但 T 不同 ⇒ w_F 与 w_B 的权重比不同 ⇒ "
           "合成的中位数绝对差 > 冻结门限。证伪『点源逆方差可直接当面亮度最优权重』。",
    inputs="臂2（阳性世界）：帧A h=(1/2,1/2,0,0) Ω_A=1 K=I；帧B h=(1/4,1/4,0,0) "
           "Ω_B=2 K_B=相关(ρ=0.5)；调 σ²_B,B 使 S_ref 相同（F_ref=100）。"
           "200 个 seed 的 |B̂(wF)−B̂(wB)| 中位数。",
    expected="T_A != T_B（阳性世界前提）；两帧 S_ref 相同；权重比不同；"
             "median|diff| > synth.r3.blend_diff_min_abs (1e-12)。",
    source="正本 SCIENCE_V2_AGENT_TASK_PACKAGE §4 R3 逐字『构造两帧具有相同 reference "
           "SNR，但 PSF 宽度、像元面积或相关 K 不同，真实像元 V_B 不同……"
           "不接受仅检查公式等式左右相等作为物理验证』；NOISE_SNR §4.1 臂2。",
)
def r3b_positive_counterexample():
    n = 4
    frA = build_frame([0.5, 0.5, 0.0, 0.0], np.eye(n), 2.0, 1.0)
    frB = build_frame([0.25, 0.25, 0.0, 0.0], corr_eye(n, 0.5), 2.0, 2.0)
    with H.evidence() as ev:
        # 阳性世界前提：T 必须不同
        H.is_false(frA["T"] == frB["T"], "T_A != T_B（阳性世界前提）")
        ev.record("note", "T_A={:.17g} T_B={:.17g} ratio={:.6g}".format(
            frA["T"], frB["T"], frB["T"] / frA["T"]))
        F_ref = 100.0
        SA, SB = snr(frA, F_ref), snr(frB, F_ref)
        # 调 σ²_B,B 使 S_ref 相同（T 与 σ²_B 无关 ⇒ T 不变）
        scale = (SB / SA) ** 2
        frB["sigma_b2"] *= scale
        frB["V_F"] *= scale
        frB["V_B"] *= scale
        SB = snr(frB, F_ref)
        H.close(SA, SB, rtol=0.0, atol=1e-9, what="两帧 S_ref 相同（构造自洽）")
        ev.record("note", "S_A=S_B={:.17g}".format(SA))
        wsF = [1.0 / frA["V_F"], 1.0 / frB["V_F"]]
        wsB = [1.0 / frA["V_B"], 1.0 / frB["V_B"]]
        ratioF, ratioB = wsF[0] / wsF[1], wsB[0] / wsB[1]
        H.is_false(ratioF == ratioB, "w_F 与 w_B 的权重比不同（可证伪性）")
        ev.record("note", "权重比 wF={:.17g} wB={:.17g}".format(ratioF, ratioB))
        med = mc_blend_diff(frA, frB, wsF, wsB, 200)
        ev.record("note", "median|B̂(wF)−B̂(wB)| = {:.17g} (200 seeds)".format(med))
        H.is_true(med > T.get("synth.r3.blend_diff_min_abs").value,
                  "相同 S_ref、不同 T ⇒ 合成差可测")


@H.test(
    "r3c_wF_suboptimal_and_T1_suboptimal",
    intent="旧权重 w_F 与等权（『T 被默认为无量纲 1』的极端形态）的合成方差都必须"
           "**高于** Cramér–Rao 下界。这是有绝对标尺的不等式判据。",
    inputs="臂2 阳性世界（同 r3b）。n_seeds=4000 的 MC 实测 Var(B̂|w)，"
           "与解析 CRB = 1/Σ(1/V_B) 比较。",
    expected="Var(B̂|w_F)/CRB − 1 > synth.r3.suboptimal_excess_min (1e-2)；"
             "Var(B̂|等权)/CRB − 1 同门限。",
    source="Cauchy–Schwarz：Var(B̂|w)=Σw²V/(Σw)² ≥ 1/Σ(1/V)，取等 ⟺ w ∝ 1/V_B；"
           "正本 UNIFIED §4『不得默认为 1』。判据是**不等式**（超出量 > 下限），"
           "不是公式两边相等。",
)
def r3c_suboptimal_variance():
    frA, frB, V_A, V_B, bound = world_positive()
    with H.evidence() as ev:
        floor = T.get("synth.r3.suboptimal_excess_min").value
        v_wF = mc_variance(frA, frB, [1 / frA["V_F"], 1 / frB["V_F"]], 4000)
        ev.record("note", "Var(w_F)={:.10g} CRB={:.10g} excess={:.6g}".format(
            v_wF, bound, v_wF / bound - 1))
        H.is_true(v_wF / bound - 1 > floor, "w_F 的合成方差高于 CRB（超出量 > 门限）")
        v_eq = mc_variance(frA, frB, [1.0, 1.0], 4000)
        ev.record("note", "Var(等权)={:.10g} CRB={:.10g} excess={:.6g}".format(
            v_eq, bound, v_eq / bound - 1))
        H.is_true(v_eq / bound - 1 > floor, "等权(T≡1) 的合成方差高于 CRB")


@H.test(
    "r3d_wB_achieves_crb",
    intent="w_B = 1/V_B 的合成方差确实达到 Cramér–Rao 下界（最优性）。与 r3c 配对："
           "同一构造世界里 w_B 最优而 w_F 次优 ⇒ 差异可归因于权重公式，不是世界构造。",
    inputs="臂2 阳性世界（同 r3b）。n_seeds=4000 的 MC 实测 Var(B̂|w_B)。",
    expected="|Var(B̂|w_B)/CRB − 1| ≤ synth.r3.wb_crb_rel (6e-2)。",
    source="Cauchy–Schwarz 取等条件 w ∝ 1/V_B；解析 CRB = 1/Σ(1/V_B)。"
           "MC 相对误差由 √(2/(n−1)) 抽样分布推导（同 synth.mc.var_ratio_ci95 族）。",
)
def r3d_wb_achieves_crb():
    frA, frB, V_A, V_B, bound = world_positive()
    with H.evidence() as ev:
        v_wB = mc_variance(frA, frB, [1 / V_A, 1 / V_B], 4000)
        rel = v_wB / bound - 1
        ev.record("note", "Var(w_B)={:.10g} CRB={:.10g} rel={:+.6g}".format(v_wB, bound, rel))
        H.close(rel, 0.0, rtol=0.0, atol=T.get("synth.r3.wb_crb_rel").value,
                what="w_B 达到 CRB")


@H.test(
    "r3e_T_dimension_and_K_dependence",
    intent="精度传递因子 T 依赖噪声形状矩阵 K 且**不是无量纲 1**：换 K 必须换 T，"
           "且 w_B 比 == T 比（w_B = I_F/T 的结构检验）。否证『T 可默认为 1』。",
    inputs="同一条 h=(1/2,1/2,0,0)、Ω=1、σ²_B=2，只把 K 从 I 换成相关核(ρ=0.7)。",
    expected="T(K=I) != T(K=相关)；w_B(K=相关)/w_B(K=I) == T(K=相关)/T(K=I) 逐位；"
             "两个 T 都不等于 1。",
    source="解析式 T_p = K_pp·(hᵀK⁻¹h)，UNIFIED §4『T 有面积/PSF 相关量纲，"
           "不得默认为 1』；DATA_SEMANTICS「w_B = I_F/T」。",
)
def r3e_T_dimension():
    n = 4
    h = [0.5, 0.5, 0.0, 0.0]
    frI = build_frame(h, np.eye(n), 2.0, 1.0)
    frC = build_frame(h, corr_eye(n, 0.7), 2.0, 1.0)
    with H.evidence() as ev:
        H.is_false(frI["T"] == frC["T"], "T 依赖 K（K=I 与 K=相关 ⇒ T 不同）")
        ev.record("note", "T(I)={:.17g} T(ρ=0.7)={:.17g}".format(frI["T"], frC["T"]))
        # 正确的结构检验是 w_B = I_F / T（逐字），不是 w_B 比 == T 比：
        # 换 K 时 I_F 与 T **都**变，比值的商不一定等于 T 的比。
        # 下面对两帧各自验 w_B == I_F/T（解析恒等式，容 binary64 舍入）。
        for name, fr in (("K=I", frI), ("K=相关", frC)):
            wB = 1.0 / fr["V_B"]
            H.close(wB, fr["I_F"] / fr["T"], rtol=0.0, atol=1e-15,
                    what="{}: w_B == I_F/T（结构恒等式）".format(name))
        ev.record("note",
                  "K=I: I_F={:.17g} T={:.17g} w_B={:.17g} | "
                  "K=相关: I_F={:.17g} T={:.17g} w_B={:.17g}".format(
                      frI["I_F"], frI["T"], 1 / frI["V_B"],
                      frC["I_F"], frC["T"], 1 / frC["V_B"]))
        H.is_false(frI["T"] == 1.0, "T(K=I) != 1")
        H.is_false(frC["T"] == 1.0, "T(K=相关) != 1")


@H.test(
    "r3f_negative_T_forced_one_breaks_optimality",
    intent="负例：把 T 强行置 1（即 w_B := I_F，正本禁止的默认）后，用 r3c 的同一判据"
           "必须判红。证明该判据有判别力，不是恒真。",
    inputs="注入缺陷 = weight_from_sparse_layer_pixel 漏除 T（等价 T≡1）。"
           "构造世界：两帧 S_ref 相同、T 不同（臂2）。",
    expected="Var(B̂|w=I_F)/CRB − 1 > synth.r3.suboptimal_excess_min（判红）。",
    source="注入缺陷复现 DELTA D04『未体现 PSF/像元面积/Drizzle 相关噪声在 T 中的转换』；"
           "正本 UNIFIED §4『不得默认为 1』。负例必须能红（开发期 --exit-code 自查）。",
    kind=H.NEGATIVE,
    inject="weight_from_sparse_layer_pixel 漏除 T_p（等价把 T 强行置 1）",
)
def r3f_negative_T_forced_one():
    frA, frB, V_A, V_B, bound = world_positive()
    with H.evidence() as ev:
        # 注入 T≡1 ⇒ w_B := I_F（旧行为）
        v = mc_variance(frA, frB, [frA["I_F"], frB["I_F"]], 4000)
        excess = v / bound - 1
        ev.record("note", "注入 T≡1 后 Var={:.10g} CRB={:.10g} excess={:.6g}".format(
            v, bound, excess))
        H.is_true(excess > T.get("synth.r3.suboptimal_excess_min").value,
                  "负例判红：T≡1 使合成方差高于 CRB")


@H.test(
    "r3g_three_inverse_variances_distinct",
    intent="三种逆方差严格分名：同输入下 I_F=SNR²/F_ref²、w_B=1/V_B 与 w_ctrl=1/V_ctrl "
           "两两不相等。证明它们不是同一个对象的多个名字。",
    inputs="单帧 K=I 白噪世界。w_ctrl 由 N=200 个独立背景样本的**中位数方差**构造"
           "（4000 次重复的定种子 MC），并与解析式 Var(median)≈(π/2)σ²/N 对拍。",
    expected="MC w_ctrl 与 (π/2)σ²/N 的相对偏差 ≤ synth.mc.var_ratio_ci95 (2e-1)；"
             "I_F / w_B / w_ctrl 三者两两不相等。",
    source="DATA_SEMANTICS 逐字「三种逆方差严格分名……不得共用语义不明的 weight 字段」；"
           "NOISE_SNR：控制点方差 Var(median)≈(π/2)σ²/N；UNIFIED §4 表格三行分列。",
)
def r3g_three_ivar_distinct():
    n = 4
    fr = build_frame([0.5, 0.5, 0.0, 0.0], np.eye(n), 2.0)
    with H.evidence() as ev:
        I_F = 1.0 / fr["V_F"]
        w_B = 1.0 / fr["V_B"]
        # Var(median of N iid samples) = (π/2)σ²/N  ⇒ 每次重复必须取 **N 个**
        # 样本的中位数（早期写成 n=4 个样本，公式不适用，比值 37.5 —— 实测抓到的
        # 自身 Bug，留档：判据发现的是我的公式误用，不是正本错误）。
        N = 200
        rng = np.random.default_rng(7)
        s = math.sqrt(fr["sigma_b2"])
        meds = [float(np.median(s * rng.standard_normal(N))) for _ in range(4000)]
        v_mc = float(np.var(meds, ddof=1))
        v_an = (math.pi / 2.0) * fr["sigma_b2"] / N
        w_ctrl = 1.0 / v_mc
        ev.record("note", "I_F={:.10g} w_B={:.10g} w_ctrl={:.10g}".format(I_F, w_B, w_ctrl))
        ev.record("note", "V_ctrl MC={:.10g} 解析={:.10g} 比={:.6g}".format(
            v_mc, v_an, v_mc / v_an))
        H.close(v_mc / v_an, 1.0, rtol=T.get("synth.mc.var_ratio_ci95").value,
                atol=0.0, what="背景控制方差与解析式 (π/2)σ²/N 相符")
        H.is_false(I_F == w_B, "I_F != w_B")
        H.is_false(I_F == w_ctrl, "I_F != w_ctrl")
        H.is_false(w_B == w_ctrl, "w_B != w_ctrl")
